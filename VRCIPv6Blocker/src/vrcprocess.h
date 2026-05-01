#pragma once
#include <windows.h>
#include <string>
#include "ILogger.h"

struct CSLock {
	CRITICAL_SECTION& rcs;
	CSLock(CRITICAL_SECTION& cs) : rcs(cs) { ::EnterCriticalSection(&cs); }
	~CSLock() { ::LeaveCriticalSection(&rcs); }
};

class VRCProcess final {
public:
	VRCProcess() = delete;
	VRCProcess(LPCWSTR lpExeFile, ydk::ILogger<WCHAR>* logger) :
		m_ExeFile(lpExeFile), m_hProcess(nullptr), m_ProcessID(0), m_Logger(logger) {
		::InitializeCriticalSection(&m_cs);
	}
	~VRCProcess() {
		::DeleteCriticalSection(&m_cs);
		if (m_hProcess) ::CloseHandle(m_hProcess);
		m_hProcess = nullptr;
		m_ProcessID = 0;
	}
	VRCProcess(const VRCProcess&) = delete;
	VRCProcess& operator=(const VRCProcess&) = delete;
	VRCProcess(VRCProcess&&) = delete;
	VRCProcess& operator=(VRCProcess&&) = delete;
	DWORD GetProcessID();
	int UserExecute(LPCWSTR lpExePath); // スレッドセーフではない。 戻り値 -> -1:エラー 0:起動している 1:起動した
private:
	std::wstring m_ExeFile;
	HANDLE m_hProcess;
	DWORD m_ProcessID;
	ydk::ILogger<WCHAR>* m_Logger;
	CRITICAL_SECTION m_cs;
	bool FindProcess();
};
