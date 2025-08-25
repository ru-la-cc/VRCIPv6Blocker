#pragma once
#include <windows.h>
#include <string>
#include <vector>

// CoInitializeやってるの前提だからこれ
namespace ydk {
	bool ExistsFirewallRule(LPCWSTR ruleName, HRESULT* hResult = nullptr);
	bool RegisterFirewallRule(
			LPCWSTR ruleName,
			const std::vector<std::wstring>& remoteAddresses,
			HRESULT* hResult = nullptr,
			LPCWSTR lpDescription = L"",
			LPCWSTR appExePath = nullptr
	);
	bool RemoveFirewallRule(LPCWSTR ruleName, HRESULT* hResult = nullptr);
}
