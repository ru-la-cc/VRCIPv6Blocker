#pragma once
#include <windows.h>
#include <string>
#include <vector>

// CoInitializeやってるの前提だからこれ
namespace ydk {
	bool ExistsFirewallRule(const std::wstring& ruleName, bool* pExists, HRESULT* hResult = nullptr);
	bool RegisterFirewallRule(
			const std::wstring& ruleName,
			const std::vector<std::wstring>& remoteAddresses,
			HRESULT* hResult = nullptr,
			LPCWSTR lpDescription = L""
	);
	bool RemoveFirewallRule(const std::wstring& ruleName, HRESULT* hResult = nullptr);
}
