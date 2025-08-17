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

	// ---- 公開 API（bool 戻り / HRESULT* は任意で受け取り） -------------------------

	// 1) ルール存在確認（成功= true / 失敗= false）
	//    *pExists = true/false に実在可否を返す
	//    *hResult = S_OK(存在) / S_FALSE(非存在) / 失敗コード
	bool ExistsFirewallRule(const std::wstring& ruleName, bool* pExists, HRESULT* hResult) {
		if (!pExists) { SetHr(E_POINTER, hResult); return false; }
		*pExists = false;

		if (!IsValidRuleName(ruleName)) { SetHr(HRESULT_FROM_WIN32(ERROR_INVALID_NAME), hResult); return false; }

		CComPtr<INetFwRules> rules;
		HRESULT hr = GetRules(&rules);
		if (FAILED(hr)) { SetHr(hr, hResult); return false; }

		CComPtr<INetFwRule> rule;
		hr = rules->Item(CComBSTR(ruleName.c_str()), &rule);
		if (SUCCEEDED(hr) && rule) {
			*pExists = true;
			SetHr(S_OK, hResult);
			return true;
		}
		if (hr == HRESULT_FROM_WIN32(ERROR_NOT_FOUND) || hr == 0x80070490L) {
			*pExists = false;
			SetHr(S_FALSE, hResult);
			return true;
		}
		SetHr(hr, hResult);
		return false;
	}

	// 2) 登録（同名があれば更新）。成功= true / 失敗= false
	//    *hResult = S_OK（追加/更新どちらも）/ 失敗コード
	bool RegisterFirewallRule(
			const std::wstring& ruleName,
			const std::vector<std::wstring>& remoteAddresses,
			HRESULT* hResult,
			LPCWSTR lpDescription
		) {
		if (!IsValidRuleName(ruleName)) { SetHr(HRESULT_FROM_WIN32(ERROR_INVALID_NAME), hResult); return false; }
		if (remoteAddresses.empty()) { SetHr(E_INVALIDARG, hResult); return false; }

		const std::wstring joined = JoinComma(remoteAddresses);
		if (joined.empty()) { SetHr(E_INVALIDARG, hResult); return false; }

		CComPtr<INetFwRules> rules;
		HRESULT hr = GetRules(&rules);
		if (FAILED(hr)) { SetHr(hr, hResult); return false; }

		// 既存検索
		CComPtr<INetFwRule> rule;
		hr = rules->Item(CComBSTR(ruleName.c_str()), &rule);
		if (SUCCEEDED(hr) && rule) {
			// 更新
			if (FAILED(rule->put_Direction(NET_FW_RULE_DIR_OUT)) ||
				FAILED(rule->put_Action(NET_FW_ACTION_BLOCK)) ||
				FAILED(rule->put_Protocol(NET_FW_IP_PROTOCOL_ANY)) ||
				FAILED(rule->put_Enabled(VARIANT_TRUE)) ||
				FAILED(rule->put_Profiles(NET_FW_PROFILE2_ALL)) ||
				FAILED(rule->put_RemoteAddresses(CComBSTR(joined.c_str())))) {
				SetHr(E_FAIL, hResult);
				return false;
			}
			SetHr(S_OK, hResult);
			return true;
		}
		if (hr != HRESULT_FROM_WIN32(ERROR_NOT_FOUND) && hr != 0x80070490L && FAILED(hr)) {
			SetHr(hr, hResult);
			return false;
		}

		// 新規
		rule.Release();
		hr = rule.CoCreateInstance(__uuidof(NetFwRule));
		if (FAILED(hr)) { SetHr(hr, hResult); return false; }

		if (FAILED(rule->put_Name(CComBSTR(ruleName.c_str()))) ||
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
	bool RemoveFirewallRule(const std::wstring& ruleName, HRESULT* hResult) {
		if (!IsValidRuleName(ruleName)) { SetHr(HRESULT_FROM_WIN32(ERROR_INVALID_NAME), hResult); return false; }

		CComPtr<INetFwRules> rules;
		HRESULT hr = GetRules(&rules);
		if (FAILED(hr)) { SetHr(hr, hResult); return false; }

		hr = rules->Remove(CComBSTR(ruleName.c_str()));
		if (SUCCEEDED(hr)) { SetHr(S_OK, hResult); return true; }

		if (hr == HRESULT_FROM_WIN32(ERROR_NOT_FOUND) || hr == 0x80070490L) {
			SetHr(S_FALSE, hResult); // 非存在は “機能的成功（No-Op）”
			return true;
		}

		SetHr(hr, hResult);
		return false;
	}
}
