#pragma once
#include <windows.h>
#include <string>
#include <stdexcept>
#include <concepts>
#include <type_traits>
#include "ILogger.h"

template<typename T>
concept ConfigType = std::integral<T> || std::floating_point<T>;

class Config final {
public:
	struct INI_CONFIG {
		unsigned __int64 ullVersion;
		UINT uRunVRC;
		UINT uAutoShutdown;
		UINT uMinWindow;
		UINT uFirewallBlock;
		UINT uNonBlocking;
		//UINT uRevert;
		UINT uOnlyVRC; // VRChat.exeをfirewallに登録するかどうかのやつだった気がする(現時点では使ってないのでそのうち消すかもしれない)
		std::wstring strExecutePath;
		std::wstring strVRCFile;
		std::wstring strDestIp;
		//std::wstring strNIC;
		std::wstring strVRCFullPath;
	};

	Config(LPCWSTR path = nullptr, ydk::ILogger<WCHAR>* logger = nullptr) : m_IniFile(path ? path : L""), m_Logger(logger) {}
	~Config() = default;
	Config(const Config&) = delete;
	Config& operator=(const Config&) = delete;
	Config(Config&&) = delete;
	Config& operator=(Config&&) = delete;
	void SetFilePath(LPCWSTR path) { m_IniFile = path; }
	void SetLogger(ydk::ILogger<WCHAR>* logger) { m_Logger = logger; }
	void Save();
	void Load();
	void DebugLog() const;
	INI_CONFIG& GetConfig() { return m_IniConfig; }
private:
	std::wstring m_IniFile;
	ydk::ILogger<WCHAR>* m_Logger;
	INI_CONFIG m_IniConfig = {};
	void WriteKey(LPCWSTR key, LPCWSTR value);
	template<ConfigType T> void WriteKey(LPCWSTR key, T value, LPCWSTR format);
	void LoadKeyString(LPWSTR buf, DWORD dwSize, LPCWSTR key, std::wstring& value, LPCWSTR lpDefault = L"");
	void Log(LPCWSTR message) const { if (m_Logger) m_Logger->Log(message); }
	void LogWarning(LPCWSTR message) const { if (m_Logger) m_Logger->LogWarning(message); }
	void LogError(LPCWSTR message) const { if (m_Logger) m_Logger->LogError(message); }
};
