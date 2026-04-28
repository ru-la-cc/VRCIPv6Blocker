#pragma once
#include <windows.h>
#include <string>
#include <stdexcept>

class Config final {
public:
	struct INI_CONFIG {
		unsigned __int64 ullVersion;
		UINT uRunVRC;
		UINT uAutoShutdown;
		UINT uMinWindow;
		UINT uFirewallBlock;
		UINT uNonBlocking;
		UINT uRevert;
		UINT uOnlyVRC;
		std::wstring strExecutePath;
		std::wstring strVRCFile;
		std::wstring strDestIp;
		std::wstring strNIC;
		std::wstring strVRCFullPath;
	};

	Config() {}
	Config(LPCWSTR path) : m_IniFile(path) {}
	~Config() {}
	void SetFilePath(LPCWSTR path) { m_IniFile = path; }
private:
	std::wstring m_IniFile;
	INI_CONFIG m_IniConfig;
	void Save();
	void Load();
};
