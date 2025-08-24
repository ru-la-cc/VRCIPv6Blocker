#pragma once
#include "WinFirewall.h"
#include <atlbase.h>
#include <netfw.h>
#include <sstream>
#include <cwctype>
#include <algorithm>

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")

namespace ydk {
	constexpr size_t MAX_RULENAME_LEN = 128;

	inline void SetHr(HRESULT hr, HRESULT* out) noexcept { if (out) *out = hr; }

	inline bool IsValidRuleName(const std::wstring& name) {
		if (name.empty() || name.size() > MAX_RULENAME_LEN) return false;
		if (name.find(L'|') != std::wstring::npos) return false;
		if (std::iswspace(name.front()) || std::iswspace(name.back())) return false;
		if (std::any_of(name.begin(), name.end(),
			[](wchar_t ch) { return !!std::iswcntrl(ch); })) return false;
		return true;
	}

	inline std::wstring JoinComma(const std::vector<std::wstring>& addrs) {
		std::wostringstream oss;
		bool first = true;
		for (const auto& a : addrs) {
			if (a.empty()) continue;
			if (!first) oss << L",";
			oss << a;
			first = false;
		}
		return oss.str();
	}

	inline HRESULT GetPolicy2(INetFwPolicy2** pp) {
		if (!pp) return E_POINTER;
		CComPtr<INetFwPolicy2> p;
		HRESULT hr = p.CoCreateInstance(__uuidof(NetFwPolicy2));
		if (FAILED(hr)) return hr;
		*pp = p.Detach();
		return S_OK;
	}

	inline HRESULT GetRules(INetFwRules** pp) {
		if (!pp) return E_POINTER;
		CComPtr<INetFwPolicy2> policy;
		HRESULT hr = GetPolicy2(&policy);
		if (FAILED(hr)) return hr;
		CComPtr<INetFwRules> rules;
		hr = policy->get_Rules(&rules);
		if (FAILED(hr)) return hr;
		*pp = rules.Detach();
		return S_OK;
	}

	inline bool IsNotFound(HRESULT hr) noexcept {
		return hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) ||
			hr == HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
	}

	// ---- 公開 API（bool 戻り / HRESULT* は任意で受け取り） -------------------------
	// 1) ルール存在確認（存在= true / 無しまたは失敗= false）
	//    *hResult = S_OK(存在) / S_FALSE(非存在)
	bool ExistsFirewallRule(LPCWSTR ruleName, HRESULT* hResult) {
		if (!ruleName) { SetHr(E_POINTER, hResult); return false; }
		if (!IsValidRuleName(std::wstring(ruleName))) {
			SetHr(HRESULT_FROM_WIN32(ERROR_INVALID_NAME), hResult);
			return false;
		}
		CComPtr<INetFwRules> rules;
		HRESULT hr = GetRules(&rules);
		if (FAILED(hr)) { SetHr(hr, hResult); return false; }

		CComPtr<INetFwRule> rule;
		hr = rules->Item(CComBSTR(ruleName), &rule);
		if (SUCCEEDED(hr) && rule) {
			SetHr(S_OK, hResult);
			return true; // 存在
		}

		// 見つからない（正常系の一種）
		if (IsNotFound(hr)) {
			SetHr(S_FALSE, hResult);
			return false;  // 非存在
		}

		// それ以外の失敗
		SetHr(hr, hResult);
		return false; // 取得失敗も false
	}

	// 2) 登録（同名があれば更新）。成功= true / 失敗= false
	//    *hResult = S_OK（追加/更新どちらも）/ 失敗コード
	bool RegisterFirewallRule(
		LPCWSTR ruleName,
		const std::vector<std::wstring>& remoteAddresses,
		HRESULT* hResult,
		LPCWSTR lpDescription,
		LPCWSTR appExePath // 追加: 対象 exe のフルパス（null なら全プロセス）
	) {
		if (!IsValidRuleName(std::wstring(ruleName))) { SetHr(HRESULT_FROM_WIN32(ERROR_INVALID_NAME), hResult); return false; }
		if (remoteAddresses.empty()) { SetHr(E_INVALIDARG, hResult); return false; }

		// 絶対パスである保険的な検証とか
		std::wstring exe;
		if (appExePath && *appExePath) {
			wchar_t buf[MAX_PATH];
			DWORD n = ::GetFullPathNameW(appExePath, (DWORD)std::size(buf), buf, nullptr);
			if (n == 0 || n >= std::size(buf) || GetFileAttributesW(buf) == INVALID_FILE_ATTRIBUTES) {
				SetHr(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND), hResult);
				return false;
			}
			exe.assign(buf, n);
		}

		const std::wstring joined = JoinComma(remoteAddresses);
		if (joined.empty()) { SetHr(E_INVALIDARG, hResult); return false; }

		CComPtr<INetFwRules> rules;
		HRESULT hr = GetRules(&rules);
		if (FAILED(hr)) { SetHr(hr, hResult); return false; }

		CComPtr<INetFwRule> rule;
		hr = rules->Item(CComBSTR(ruleName), &rule);
		if (SUCCEEDED(hr) && rule) {
			// 既存更新
			if (FAILED(rule->put_Direction(NET_FW_RULE_DIR_OUT)) ||
				FAILED(rule->put_Action(NET_FW_ACTION_BLOCK)) ||
				FAILED(rule->put_Protocol(NET_FW_IP_PROTOCOL_ANY)) ||
				FAILED(rule->put_Enabled(VARIANT_TRUE)) ||
				FAILED(rule->put_Profiles(NET_FW_PROFILE2_ALL)) ||
				FAILED(rule->put_RemoteAddresses(CComBSTR(joined.c_str())))) {
				SetHr(E_FAIL, hResult); return false;
			}
			// 対象のexe指定されたら...
			// if (appExePath && *appExePath) {
			if (!exe.empty()) { // こっちの判定が自然か？
				if (FAILED(rule->put_ApplicationName(CComBSTR(exe.c_str())))) {
					SetHr(E_FAIL, hResult); return false;
				}
			}
			else {
				rule->put_ApplicationName(nullptr);
			}
			SetHr(S_OK, hResult); return true;
		}
		if (FAILED(hr) && !IsNotFound(hr)) { SetHr(hr, hResult); return false; }

		// 新規作成
		rule.Release();
		hr = rule.CoCreateInstance(__uuidof(NetFwRule));
		if (FAILED(hr)) { SetHr(hr, hResult); return false; }

		if (FAILED(rule->put_Name(CComBSTR(ruleName))) ||
			FAILED(rule->put_Description(CComBSTR(lpDescription))) ||
			FAILED(rule->put_Direction(NET_FW_RULE_DIR_OUT)) ||
			FAILED(rule->put_Action(NET_FW_ACTION_BLOCK)) ||
			FAILED(rule->put_Protocol(NET_FW_IP_PROTOCOL_ANY)) ||
			FAILED(rule->put_Enabled(VARIANT_TRUE)) ||
			FAILED(rule->put_Profiles(NET_FW_PROFILE2_ALL)) ||
			FAILED(rule->put_RemoteAddresses(CComBSTR(joined.c_str())))) {
			SetHr(E_FAIL, hResult); return false;
		}
		if (appExePath && *appExePath) {
			if (FAILED(rule->put_ApplicationName(CComBSTR(exe.c_str())))) {
				SetHr(E_FAIL, hResult); return false;
			}
		}

		hr = rules->Add(rule);
		if (FAILED(hr)) { SetHr(hr, hResult); return false; }
		SetHr(S_OK, hResult); return true;
		// 新規
		rule.Release();
		hr = rule.CoCreateInstance(__uuidof(NetFwRule));
		if (FAILED(hr)) { SetHr(hr, hResult); return false; }

		if (FAILED(rule->put_Name(CComBSTR(ruleName))) ||
			FAILED(rule->put_Description(CComBSTR(lpDescription))) ||
			FAILED(rule->put_Direction(NET_FW_RULE_DIR_OUT)) ||
			FAILED(rule->put_Action(NET_FW_ACTION_BLOCK)) ||
			FAILED(rule->put_Protocol(NET_FW_IP_PROTOCOL_ANY)) ||
			FAILED(rule->put_Enabled(VARIANT_TRUE)) ||
			FAILED(rule->put_Profiles(NET_FW_PROFILE2_ALL)) ||
			FAILED(rule->put_RemoteAddresses(CComBSTR(joined.c_str())))) {
			SetHr(E_FAIL, hResult);
			return false;
		}

		hr = rules->Add(rule);
		if (FAILED(hr)) { SetHr(hr, hResult); return false; }

		SetHr(S_OK, hResult);
		return true;
	}

	// 3) 削除。見つからなくても呼び出し成功なら true、*hResult=S_FALSE で区別
	//    *hResult = S_OK(削除済) / S_FALSE(見つからず未削除) / 失敗コード
	bool RemoveFirewallRule(LPCWSTR ruleName, HRESULT* hResult) {
		if (!IsValidRuleName(std::wstring(ruleName))) { SetHr(HRESULT_FROM_WIN32(ERROR_INVALID_NAME), hResult); return false; }

		CComPtr<INetFwRules> rules;
		HRESULT hr = GetRules(&rules);
		if (FAILED(hr)) { SetHr(hr, hResult); return false; }

		hr = rules->Remove(CComBSTR(ruleName));
		if (SUCCEEDED(hr)) { SetHr(S_OK, hResult); return true; }

		if (hr == HRESULT_FROM_WIN32(ERROR_NOT_FOUND)) {
			SetHr(S_FALSE, hResult); // 非存在は “機能的成功（No-Op）”
			return true;
		}

		SetHr(hr, hResult);
		return false;
	}
}
