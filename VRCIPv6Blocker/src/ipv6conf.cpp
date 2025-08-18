#include "ipv6conf.h"

#include <netioapi.h>
#include <iphlpapi.h>
#include <initguid.h>
#include <netcfgx.h>
#include <devguid.h>
#include <vector>
#include <cstring>

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "ole32.lib")

namespace ydk {
	bool AdapterKey::valid() const noexcept {
		static const GUID zero{};
		return ifIndex != 0 || std::memcmp(&ifGuid, &zero, sizeof(GUID)) != 0;
	}

	namespace {
		template<class T>
		class ComPtr {
			T* p_ = nullptr;
		public:
			ComPtr() = default;
			explicit ComPtr(T* p) : p_(p) {}
			ComPtr(const ComPtr&) = delete;
			ComPtr& operator=(const ComPtr&) = delete;
			ComPtr(ComPtr&& o) noexcept : p_(o.p_) { o.p_ = nullptr; }
			ComPtr& operator=(ComPtr&& o) noexcept {
				if (this != &o) { reset(); p_ = o.p_; o.p_ = nullptr; }
				return *this;
			}
			~ComPtr() { reset(); }

			T* get()  const noexcept { return p_; }
			T** put()        noexcept { reset(); return &p_; } // 受け渡し用
			T* detach()     noexcept { T* t = p_; p_ = nullptr; return t; }
			void reset(T* p = nullptr) noexcept { if (p_) p_->Release(); p_ = p; }
			T* operator->() const noexcept { return p_; }
			explicit operator bool() const noexcept { return p_ != nullptr; }
		};

		// IP 文字列パース（IPv4/IPv6）
		inline bool ParseIpLiteral(LPCWSTR ip, sockaddr_storage& ss, int& family) noexcept {
			std::memset(&ss, 0, sizeof(ss));
			family = AF_UNSPEC;
			if (!ip || !*ip) return false;

			sockaddr_in v4{}; v4.sin_family = AF_INET;
			if (InetPtonW(AF_INET, ip, &v4.sin_addr) == 1) {
				reinterpret_cast<sockaddr_in&>(ss) = v4;
				family = AF_INET;
				return true;
			}
			sockaddr_in6 v6{}; v6.sin6_family = AF_INET6;
			if (InetPtonW(AF_INET6, ip, &v6.sin6_addr) == 1) {
				reinterpret_cast<sockaddr_in6&>(ss) = v6;
				family = AF_INET6;
				return true;
			}
			return false;
		}

		// バインドパスが指定アダプタのものか判定
		inline bool BindingPathBelongsToAdapter(INetCfgBindingPath* path, const GUID& ifGuid) {
			if (!path) return false;

			ComPtr<IEnumNetCfgBindingInterface> e;
			if (FAILED(path->EnumBindingInterfaces(e.put())) || !e) return false;

			ULONG fetched = 0;
			ComPtr<INetCfgBindingInterface> bi;
			while (e->Next(1, bi.put(), &fetched) == S_OK && bi) {
				INetCfgComponent* lower = nullptr;
				if (SUCCEEDED(bi->GetLowerComponent(&lower)) && lower) {
					GUID inst{};
					HRESULT hr = lower->GetInstanceGuid(&inst);
					lower->Release();
					if (SUCCEEDED(hr) && IsEqualGUID(inst, ifGuid)) return true;
				}
				bi.reset();
			}
			return false;
		}

		// INetCfg セッション（書込みロック含む）
		// CoInitialize(Ex)済み前提、未初期化だとCoCreateInstanceがCO_E_NOTINITIALIZEDを返す・・・たぶん
		class NetCfgSession {
			ComPtr<INetCfg>     netcfg_;
			ComPtr<INetCfgLock> lock_;
			bool write_locked_ = false;
			static constexpr DWORD WRITE_LOCK_TIMEOUT_MS = 10000;
		public:
			HRESULT begin(bool writeLock, const wchar_t* who = L"ydk::ipv6conf") {
				INetCfg* raw = nullptr;
				HRESULT hr = CoCreateInstance(CLSID_CNetCfg, nullptr, CLSCTX_SERVER, IID_INetCfg, (void**)&raw);
				if (FAILED(hr)) return hr;
				netcfg_.reset(raw);

				if (writeLock) {
					INetCfgLock* lk = nullptr;
					hr = netcfg_.get()->QueryInterface(IID_INetCfgLock, (void**)&lk);
					if (FAILED(hr)) return hr;
					lock_.reset(lk);

					LPWSTR holder = nullptr;
					hr = lock_.get()->AcquireWriteLock(WRITE_LOCK_TIMEOUT_MS, who, &holder);
					if (holder) CoTaskMemFree(holder);
					if (FAILED(hr)) return hr;
					write_locked_ = true;
				}
				return netcfg_.get()->Initialize(nullptr);
			}
			HRESULT applyAndEnd() {
				if (!netcfg_) return E_UNEXPECTED;
				if (write_locked_) netcfg_.get()->Apply();
				netcfg_.get()->Uninitialize();
				if (write_locked_ && lock_) {
					lock_.get()->ReleaseWriteLock();
					write_locked_ = false;
				}
				lock_.reset();
				netcfg_.reset();
				return S_OK;
			}
			~NetCfgSession() { if (netcfg_) applyAndEnd(); }
			INetCfg* get() const noexcept { return netcfg_.get(); }
		};
	} // namespace

	// -----------------------------------------------------------------------------

	HRESULT ResolveInternetAdapterFromString(LPCWSTR destIpStr, AdapterKey& out) {
		sockaddr_storage ss{}; int fam = AF_UNSPEC;
		if (!ParseIpLiteral(destIpStr, ss, fam)) {
			// フォールバック: 8.8.8.8
			sockaddr_in v4{}; v4.sin_family = AF_INET;
			v4.sin_addr.s_addr = htonl((8u << 24) | (8u << 16) | (8u << 8) | 8u);
			reinterpret_cast<sockaddr_in&>(ss) = v4;
			fam = AF_INET;
		}

		NET_LUID luid{}; ULONG ifIndex = 0;
		if (fam == AF_INET) {
			DWORD err = GetBestInterfaceEx(reinterpret_cast<SOCKADDR*>(&ss), &ifIndex);
			if (err != NO_ERROR) return HRESULT_FROM_WIN32(err);
			out.ifIndex = ifIndex;
			if (ConvertInterfaceIndexToLuid(ifIndex, &luid) != NO_ERROR) return E_FAIL;
		}
		else { // AF_INET6
			MIB_IPFORWARD_ROW2 bestRoute{};
			SOCKADDR_INET dst{};     // 入力: 目的地
			dst.si_family = AF_INET6;
			dst.Ipv6 = *reinterpret_cast<const sockaddr_in6*>(&ss);

			SOCKADDR_INET bestSrc{};

			DWORD err = GetBestRoute2(
				nullptr,            // InterfaceLuid (省略可)
				0,                  // InterfaceIndex (0=自動)
				nullptr,            // SourceAddress (省略可)
				&dst,               // DestinationAddress (const SOCKADDR_INET*)
				0,                  // AddressSortOptions / BestRouteFlags
				&bestRoute,         // out
				&bestSrc            // out
			);
			if (err != NO_ERROR) return HRESULT_FROM_WIN32(err);

			out.ifIndex = bestRoute.InterfaceIndex;
			luid = bestRoute.InterfaceLuid;
		}
		if (ConvertInterfaceLuidToGuid(&luid, &out.ifGuid) != NO_ERROR) return E_FAIL;

		// FriendlyName（任意）
		ULONG sz = 0;
		GetAdaptersAddresses(AF_UNSPEC, 0, nullptr, nullptr, &sz);
		if (sz) {
			std::vector<unsigned char> buf(sz);
			auto paa = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buf.data());
			if (GetAdaptersAddresses(AF_UNSPEC, 0, nullptr, paa, &sz) == NO_ERROR) {
				for (auto p = paa; p; p = p->Next) {
					if (p->Luid.Value == luid.Value) {
						if (p->FriendlyName) out.friendly = p->FriendlyName;
						break;
					}
				}
			}
		}
		return S_OK;
	}

	HRESULT ResolveInternetAdapter(AdapterKey& out) {
		return ResolveInternetAdapterFromString(nullptr, out);
	}

	bool IsIPv6Enable(const AdapterKey& key, HRESULT* phResult /* nullable */) {
		if (phResult) *phResult = E_UNEXPECTED;
		if (!key.valid()) { if (phResult) *phResult = E_INVALIDARG; return false; }

		NetCfgSession ses;
		HRESULT hr = ses.begin(/* writeLock= */ false);
		if (FAILED(hr)) { if (phResult) *phResult = hr; return false; }

		INetCfg* nc = ses.get();
		INetCfgComponent* ipv6 = nullptr;
		hr = nc->FindComponent(L"ms_tcpip6", &ipv6);
		if (FAILED(hr) || !ipv6) {
			ses.applyAndEnd();
			if (phResult) *phResult = FAILED(hr) ? hr : HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
			if (ipv6) ipv6->Release();
			return false;
		}

		INetCfgComponentBindings* binds = nullptr;
		hr = ipv6->QueryInterface(IID_INetCfgComponentBindings, (void**)&binds);
		ipv6->Release();
		if (FAILED(hr) || !binds) { ses.applyAndEnd(); if (phResult) *phResult = hr; if (binds) binds->Release(); return false; }

		IEnumNetCfgBindingPath* paths = nullptr;
		hr = binds->EnumBindingPaths(EBP_BELOW, &paths);
		binds->Release();
		if (FAILED(hr) || !paths) { ses.applyAndEnd(); if (phResult) *phResult = hr; if (paths) paths->Release(); return false; }

		bool enabled = false;
		bool found = false;
		ULONG n = 0;
		INetCfgBindingPath* path = nullptr;
#pragma warning(push)
#pragma warning(disable:6387) // DA☆MA☆RE
		while (paths->Next(1, &path, &n) == S_OK && path) {
#pragma warning(pop)
			if (BindingPathBelongsToAdapter(path, key.ifGuid)) {
				found = true;
				const HRESULT h = path->IsEnabled();
				if (h == S_OK) enabled = true;
				else if (h == S_FALSE) enabled = false;
				else {
					if (phResult) *phResult = h;
					enabled = false;
				}
				path->Release();
				break;
			}
			path->Release();
			path = nullptr;
		}
		paths->Release();
		ses.applyAndEnd();

		if (phResult) *phResult = S_OK;
		return found ? enabled : false;
	}

	HRESULT SetIPv6Enable(bool enable,
		const AdapterKey* pKey,
		LPCWSTR destIpStr) {
		AdapterKey key{};
		if (!pKey || !pKey->valid()) {
			HRESULT hr = ResolveInternetAdapterFromString(destIpStr, key);
			if (FAILED(hr)) return hr;
		}
		else {
			key = *pKey;
		}

		NetCfgSession ses;
		HRESULT hr = ses.begin(/* writeLock= */ true, L"ydk::SetIPv6Enable");
		if (FAILED(hr)) return hr;

		INetCfg* nc = ses.get();
		INetCfgComponent* ipv6 = nullptr;
		hr = nc->FindComponent(L"ms_tcpip6", &ipv6);
		if (FAILED(hr) || !ipv6) {
			ses.applyAndEnd();
			if (ipv6) ipv6->Release();
			return FAILED(hr) ? hr : HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
		}

		INetCfgComponentBindings* binds = nullptr;
		hr = ipv6->QueryInterface(IID_INetCfgComponentBindings, (void**)&binds);
		ipv6->Release();
		if (FAILED(hr) || !binds) { ses.applyAndEnd(); if (binds) binds->Release(); return hr; }

		IEnumNetCfgBindingPath* paths = nullptr;
		hr = binds->EnumBindingPaths(EBP_BELOW, &paths);
		binds->Release();
		if (FAILED(hr) || !paths) { ses.applyAndEnd(); if (paths) paths->Release(); return hr; }

		bool touched = false;
		ULONG n = 0;
		INetCfgBindingPath* path = nullptr;
#pragma warning(push)
#pragma warning(disable:6387) // ええやんか別に...
		while (paths->Next(1, &path, &n) == S_OK && path) {
#pragma warning(pop)
			if (BindingPathBelongsToAdapter(path, key.ifGuid)) {
				HRESULT hr2 = path->Enable(enable ? TRUE : FALSE);
				path->Release();
				if (FAILED(hr2)) { paths->Release(); ses.applyAndEnd(); return hr2; }
				touched = true;
				break;
			}
			path->Release();
			path = nullptr;
		}

		paths->Release();
		ses.applyAndEnd();
		return touched ? S_OK : S_FALSE;
	}
}
