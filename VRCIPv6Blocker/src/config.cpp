#include <strsafe.h>
#include "config.h"
#include "defines.h"
#include "win32except.h"

void Config::Save() {
#ifdef _DEBUG
	DebugLog();
#endif
	WriteKey(App::IK_VERSION, m_IniConfig.ullVersion, L"%llu");
	WriteKey(App::IK_RUNVRC, m_IniConfig.uRunVRC, L"%u");
	WriteKey(App::IK_AUTOSHUTDOWN, m_IniConfig.uAutoShutdown, L"%u");
	WriteKey(App::IK_MINWINDOW, m_IniConfig.uMinWindow, L"%u");
	WriteKey(App::IK_FIREWALLBLOCK, m_IniConfig.uFirewallBlock, L"%u");
	WriteKey(App::IK_NONBLOCKING, m_IniConfig.uNonBlocking, L"%u");
	WriteKey(App::IK_REVERT, m_IniConfig.uRevert, L"%u");
	WriteKey(App::IK_ONLYVRC, m_IniConfig.uOnlyVRC, L"%u");
	WriteKey(App::IK_EXECUTEPATH, m_IniConfig.strExecutePath.c_str());
	WriteKey(App::IK_VRCFILE, m_IniConfig.strVRCFile.c_str());
	WriteKey(App::IK_DESTIP, m_IniConfig.strDestIp.c_str());
	WriteKey(App::IK_NIC, m_IniConfig.strNIC.c_str());
	WriteKey(App::IK_VRCFULLPATH, m_IniConfig.strVRCFullPath.c_str());
}

void Config::Load() {
	m_IniConfig.uRunVRC = ::GetPrivateProfileIntW(APP_NAME, App::IK_RUNVRC, BST_CHECKED, m_IniFile.c_str());
	m_IniConfig.uAutoShutdown = ::GetPrivateProfileIntW(APP_NAME, App::IK_AUTOSHUTDOWN, BST_CHECKED, m_IniFile.c_str());
	m_IniConfig.uMinWindow = ::GetPrivateProfileIntW(APP_NAME, App::IK_MINWINDOW, BST_UNCHECKED, m_IniFile.c_str());
	m_IniConfig.uFirewallBlock = ::GetPrivateProfileIntW(APP_NAME, App::IK_FIREWALLBLOCK, BST_CHECKED, m_IniFile.c_str());
	m_IniConfig.uNonBlocking = ::GetPrivateProfileIntW(APP_NAME, App::IK_NONBLOCKING, BST_UNCHECKED, m_IniFile.c_str());
	m_IniConfig.uRevert = ::GetPrivateProfileIntW(APP_NAME, App::IK_REVERT, BST_UNCHECKED, m_IniFile.c_str());
	m_IniConfig.uOnlyVRC = ::GetPrivateProfileIntW(APP_NAME, App::IK_ONLYVRC, BST_UNCHECKED, m_IniFile.c_str());

	WCHAR szBuf[MAX_PATH];
	::GetPrivateProfileStringW(APP_NAME, App::IK_VERSION, L"0", szBuf, std::size(szBuf), m_IniFile.c_str());
	LPWSTR lpEnd;
	m_IniConfig.ullVersion = std::wcstoull(szBuf, &lpEnd, 10);
	if (*lpEnd || errno == ERANGE || !m_IniConfig.ullVersion) m_IniConfig.ullVersion = 0;

	LoadKeyString(szBuf, std::size(szBuf), App::IK_EXECUTEPATH, m_IniConfig.strExecutePath);
	LoadKeyString(szBuf, std::size(szBuf), App::IK_VRCFILE, m_IniConfig.strVRCFile);
	LoadKeyString(szBuf, std::size(szBuf), App::IK_DESTIP, m_IniConfig.strDestIp);
	LoadKeyString(szBuf, std::size(szBuf), App::IK_NIC, m_IniConfig.strNIC);
	LoadKeyString(szBuf, std::size(szBuf), App::IK_VRCFULLPATH, m_IniConfig.strVRCFullPath);
#ifdef _DEBUG
	DebugLog();
#endif
}

void Config::DebugLog() const {
	// デバッグ用なんでエラーハンドリングはしない
	WCHAR szLog[384];
	::StringCchPrintfW(szLog,
		std::size(szLog),
		L"IniConfig : Ver(%llu), uRunVRC(%u), uAutoShutdown(%u), uMinWindow(%u), uFirewallBlock(%u), uNonBlocking(%u), uRevert(%u), uOnlyVRC(%u)",
		m_IniConfig.ullVersion,
		m_IniConfig.uRunVRC,
		m_IniConfig.uAutoShutdown,
		m_IniConfig.uMinWindow,
		m_IniConfig.uFirewallBlock,
		m_IniConfig.uNonBlocking,
		m_IniConfig.uRevert,
		m_IniConfig.uOnlyVRC);
	Log(szLog);

	::StringCchPrintfW(szLog,
		std::size(szLog),
		L"IniConfig : strExecutePath=%s",
		m_IniConfig.strExecutePath.c_str());
	Log(szLog);

	::StringCchPrintfW(szLog,
		std::size(szLog),
		L"IniConfig : strVRCFile=%s",
		m_IniConfig.strVRCFile.c_str());
	Log(szLog);

	::StringCchPrintfW(szLog,
		std::size(szLog),
		L"IniConfig : strDestIp=%s",
		m_IniConfig.strDestIp.c_str());
	Log(szLog);

	::StringCchPrintfW(szLog,
		std::size(szLog),
		L"IniConfig : strNIC=%s",
		m_IniConfig.strNIC.c_str());

	::StringCchPrintfW(szLog,
		std::size(szLog),
		L"IniConfig : strVRCFullPath=%s",
		m_IniConfig.strVRCFullPath.c_str());
	Log(szLog);
}

template<ConfigType T> void Config::WriteKey(LPCWSTR key, T value, LPCWSTR format) {
	WCHAR szBuf[64];
	LPCWSTR writeValue = szBuf;
	DWORD err;
	if constexpr (std::same_as<T, LPCWSTR>) {
		writeValue = value;
	}
	else {
		::StringCchPrintfW(szBuf, std::size(szBuf), format, value);
	}
	if (!::WritePrivateProfileStringW(APP_NAME, key, writeValue, m_IniFile.c_str()) &&
		(err = ::GetLastError()) != ERROR_SUCCESS) {
		LogError((std::wstring(L"設定の書き込みに失敗: key=") + key).c_str());
		throw ydk::Win32Exception(err);
	}
}

void Config::LoadKeyString(LPWSTR buf, DWORD dwSize, LPCWSTR key, std::wstring& value) {
	::GetPrivateProfileStringW(APP_NAME, key, L"", buf, dwSize, m_IniFile.c_str());
	DWORD err = ::GetLastError();
	if (err != ERROR_SUCCESS) {
		LogError((std::wstring(L"設定の読み込みに失敗: key=") + key).c_str());
		throw ydk::Win32Exception(err);
	}
	value = buf;
}
