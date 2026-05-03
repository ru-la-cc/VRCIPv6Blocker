#include <strsafe.h>
#include "config.h"
#include "defines.h"
#include "win32except.h"

void Config::Save() {
#ifdef _DEBUG
	DebugLog();
#endif
	WriteKey(IK_VERSION, m_IniConfig.ullVersion, L"%llu");
	WriteKey(IK_RUNVRC, m_IniConfig.uRunVRC, L"%u");
	WriteKey(IK_AUTOSHUTDOWN, m_IniConfig.uAutoShutdown, L"%u");
	WriteKey(IK_MINWINDOW, m_IniConfig.uMinWindow, L"%u");
	WriteKey(IK_FIREWALLBLOCK, m_IniConfig.uFirewallBlock, L"%u");
	WriteKey(IK_NONBLOCKING, m_IniConfig.uNonBlocking, L"%u");
	WriteKey(IK_ONLYVRC, m_IniConfig.uOnlyVRC, L"%u");
	WriteKey(IK_EXECUTEPATH, m_IniConfig.strExecutePath.c_str());
	WriteKey(IK_VRCFILE, m_IniConfig.strVRCFile.c_str());
	WriteKey(IK_DESTIP, m_IniConfig.strDestIp.c_str());
	WriteKey(IK_VRCFULLPATH, m_IniConfig.strVRCFullPath.c_str());
}

void Config::Load() {
	m_IniConfig.uRunVRC = ::GetPrivateProfileIntW(APP_NAME, IK_RUNVRC, BST_CHECKED, m_IniFile.c_str());
	m_IniConfig.uAutoShutdown = ::GetPrivateProfileIntW(APP_NAME, IK_AUTOSHUTDOWN, BST_CHECKED, m_IniFile.c_str());
	m_IniConfig.uMinWindow = ::GetPrivateProfileIntW(APP_NAME, IK_MINWINDOW, BST_UNCHECKED, m_IniFile.c_str());
	m_IniConfig.uFirewallBlock = ::GetPrivateProfileIntW(APP_NAME, IK_FIREWALLBLOCK, BST_CHECKED, m_IniFile.c_str());
	m_IniConfig.uNonBlocking = ::GetPrivateProfileIntW(APP_NAME, IK_NONBLOCKING, BST_UNCHECKED, m_IniFile.c_str());
	m_IniConfig.uOnlyVRC = ::GetPrivateProfileIntW(APP_NAME, IK_ONLYVRC, BST_UNCHECKED, m_IniFile.c_str());

	WCHAR szBuf[MAX_PATH];
	::GetPrivateProfileStringW(APP_NAME, IK_VERSION, L"0", szBuf, std::size(szBuf), m_IniFile.c_str());
	LPWSTR lpEnd;
	errno = 0;
	m_IniConfig.ullVersion = std::wcstoull(szBuf, &lpEnd, 10);
	if (*lpEnd || errno == ERANGE || !m_IniConfig.ullVersion) m_IniConfig.ullVersion = 0;

	LoadKeyString(szBuf, std::size(szBuf), IK_EXECUTEPATH, m_IniConfig.strExecutePath);
	LoadKeyString(szBuf, std::size(szBuf), IK_VRCFILE, m_IniConfig.strVRCFile, VRCFILENAME);
	LoadKeyString(szBuf, std::size(szBuf), IK_DESTIP, m_IniConfig.strDestIp, DEF_GATEWAY_HINT_IP);
	LoadKeyString(szBuf, std::size(szBuf), IK_VRCFULLPATH, m_IniConfig.strVRCFullPath);
#ifdef _DEBUG
	DebugLog();
#endif
}

void Config::DebugLog() const {
	// デバッグ用なんでエラーハンドリングはしない
	WCHAR szLog[384];
	::StringCchPrintfW(szLog,
		std::size(szLog),
		L"IniConfig : Ver(%llu), uRunVRC(%u), uAutoShutdown(%u), uMinWindow(%u), uFirewallBlock(%u), uNonBlocking(%u), uOnlyVRC(%u)",
		m_IniConfig.ullVersion,
		m_IniConfig.uRunVRC,
		m_IniConfig.uAutoShutdown,
		m_IniConfig.uMinWindow,
		m_IniConfig.uFirewallBlock,
		m_IniConfig.uNonBlocking,
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
		L"IniConfig : strVRCFullPath=%s",
		m_IniConfig.strVRCFullPath.c_str());
	Log(szLog);
}

void Config::WriteKey(LPCWSTR key, LPCWSTR value) {
	DWORD err;
	if (!::WritePrivateProfileStringW(APP_NAME, key, value, m_IniFile.c_str()) &&
		(err = ::GetLastError()) != ERROR_SUCCESS) {
		LogError((std::wstring(L"設定の書き込みに失敗: key=") + key).c_str());
		throw ydk::Win32Exception(err);
	}
}

template<ConfigType T>
void Config::WriteKey(LPCWSTR key, T value, LPCWSTR format) {
	WCHAR szBuf[64];
	::StringCchPrintfW(szBuf, std::size(szBuf), format, value);
	WriteKey(key, szBuf);
}

void Config::LoadKeyString(LPWSTR buf, DWORD dwSize, LPCWSTR key, std::wstring& value, LPCWSTR lpDefault) {
	::GetPrivateProfileStringW(APP_NAME, key, lpDefault, buf, dwSize, m_IniFile.c_str());
	value = buf;
}
