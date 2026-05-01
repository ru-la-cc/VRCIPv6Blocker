#pragma once
#include <windows.h>
#include <string>
#include "ILogger.h"

class VRCProcess final {
public:
	VRCProcess() = delete;
	VRCProcess(LPCWSTR lpExeFile, ydk::ILogger<WCHAR>* logger) :
		m_ExeFile(lpExeFile), m_hProcess(nullptr), m_ProcessID(0), m_Logger(logger) { }
	~VRCProcess() {
		if (m_hProcess) ::CloseHandle(m_hProcess);
		m_hProcess = nullptr;
		m_ProcessID = 0;
	}
	VRCProcess(const VRCProcess&) = delete;
	VRCProcess& operator=(const VRCProcess&) = delete;
	VRCProcess(VRCProcess&&) = delete;
	VRCProcess& operator=(VRCProcess&&) = delete;
	DWORD GetProcessID();
private:
	std::wstring m_ExeFile;
	HANDLE m_hProcess;
	DWORD m_ProcessID;
	ydk::ILogger<WCHAR>* m_Logger;
	bool FindProcess();
};
