#pragma once
#include <windows.h>
#include <string>
#include <vector>

// CoInitializeやってるの前提だからこれ
namespace ydk {
	enum class FWSetterResult : int {
		Ok = 0, // エラーなし
		Error_Create,
		Error_Name,
		Error_Description,
		Error_Direction,
		Error_Action,
		Error_Protocol,
		Error_Enabled,
		Error_Profiles,
		Error_RemoteAddresses,
		Error_Path,
		Error_GetRules,
		Error_AppName,
		Error_NotFound,
		Error_AddRule,
		Error_Unknown
	};

	constexpr LPCWSTR FWSetterResultString(FWSetterResult result) {
		switch (result) {
			case FWSetterResult::Ok:                    return L"Ok";
			case FWSetterResult::Error_Create:          return L"Error_Create";
			case FWSetterResult::Error_Name:            return L"Error_Name";
			case FWSetterResult::Error_Description:     return L"Error_Description";
			case FWSetterResult::Error_Direction:       return L"Error_Direction";
			case FWSetterResult::Error_Action:          return L"Error_Action";
			case FWSetterResult::Error_Protocol:        return L"Error_Protocol";
			case FWSetterResult::Error_Enabled:         return L"Error_Enabled";
			case FWSetterResult::Error_Profiles:        return L"Error_Profiles";
			case FWSetterResult::Error_RemoteAddresses: return L"Error_RemoteAddresses";
			case FWSetterResult::Error_Path:            return L"Error_Path";
			case FWSetterResult::Error_GetRules:        return L"Error_GetRules";
			case FWSetterResult::Error_AppName:         return L"Error_AppName";
			case FWSetterResult::Error_NotFound:        return L"Error_NotFound";
			case FWSetterResult::Error_AddRule:         return L"Error_AddRule";
			case FWSetterResult::Error_Unknown:         return L"Error_Unknown";
			default:                                    return L"";
		}
	}

	bool ExistsFirewallRule(LPCWSTR ruleName, HRESULT* hResult = nullptr);
	FWSetterResult RegisterFirewallRule(
			LPCWSTR ruleName,
			const std::vector<std::wstring>& remoteAddresses,
			HRESULT* hResult = nullptr,
			LPCWSTR lpDescription = L"",
			LPCWSTR appExePath = nullptr
	);
	bool RemoveFirewallRule(LPCWSTR ruleName, HRESULT* hResult = nullptr);
}
