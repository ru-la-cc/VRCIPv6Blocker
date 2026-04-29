#pragma once
#include <windows.h>
#include <string>
#include <stdexcept>
#include <concepts>
#include <type_traits>
#include "ILogger.h"

template<typename T>
concept ConfigType = std::integral<T> || std::floating_point<T> || std::same_as<T, LPCWSTR>;

class Config final {
public:
	struct INI_CONFIG {
		unsigned __int64 ullVersion = 0LLU;
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

	Config(LPCWSTR path = nullptr, ydk::ILogger<WCHAR>* logger = nullptr) : m_IniFile(path ? path : L""), m_Logger(logger) {}
	~Config() {}
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
	template<ConfigType T> void WriteKey(LPCWSTR key, T value, LPCWSTR format = L"%lld");
	void LoadKeyString(LPWSTR buf, DWORD dwSize, LPCWSTR key, std::wstring& value);
	void Log(LPCWSTR message) const { if (m_Logger) m_Logger->Log(message); }
	void LogWarning(LPCWSTR message) const { if (m_Logger) m_Logger->LogWarning(message); }
	void LogError(LPCWSTR message) const { if (m_Logger) m_Logger->LogError(message); }
};
