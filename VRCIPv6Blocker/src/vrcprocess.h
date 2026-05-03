#pragma once
#include <windows.h>
#include <string>
#include <thread>
#include <optional>
#include <atomic>
#include <functional>
#include "ILogger.h"

struct CSLock {
	CRITICAL_SECTION& rcs;
	CSLock(CRITICAL_SECTION& cs) : rcs(cs) { ::EnterCriticalSection(&cs); }
	~CSLock() { ::LeaveCriticalSection(&rcs); }
	CSLock(const CSLock&) = delete;
	CSLock& operator=(const CSLock&) = delete;
	CSLock(CSLock&&) = delete;
	CSLock& operator=(CSLock&&) = delete;
};

class VRCProcess final {
public:
	struct MonitorParams {
		VRCProcess* vrcp;
		std::optional<std::thread>* waiter;
		std::atomic<bool>* stopflag;
		std::function<void(LPCWSTR)> settext;
		std::function<void(WPARAM, LPARAM)> vrcexit;
		CRITICAL_SECTION& cs;
	};
	VRCProcess() = delete;
	VRCProcess(LPCWSTR lpExeFile, ydk::ILogger<WCHAR>* logger) :
		m_ExeFile(lpExeFile), m_hProcess(nullptr), m_ProcessID(0), m_Logger(logger) {
		::InitializeCriticalSection(&m_cs);
		::InitializeCriticalSection(&m_execute_cs);
	}
	~VRCProcess() {
		::DeleteCriticalSection(&m_execute_cs);
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
	int UserExecute(LPCWSTR lpExePath); // -1:エラー 0:起動している 1:起動した
	static void VRCMonitorThread(MonitorParams* params);
private:
	std::wstring m_ExeFile;
	HANDLE m_hProcess;
	DWORD m_ProcessID;
	ydk::ILogger<WCHAR>* m_Logger;
	CRITICAL_SECTION m_cs;
	CRITICAL_SECTION m_execute_cs;
	bool FindProcess();
	static void VRCWaitThread(
		VRCProcess* vrcp,
		ydk::ILogger<WCHAR>* logger,
		std::function<void(WPARAM,LPARAM)> VRCExitCallback,
		std::atomic<bool>* stopflag
	);
};

