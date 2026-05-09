#pragma once
#include <windows.h>
#include <string>
#include <thread>
#include <optional>
#include <atomic>
#include <functional>
#include "ILogger.h"

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
		m_ExeFile(lpExeFile),
		m_hProcess(nullptr),
		m_ProcessID(0),
		m_Logger(logger) {
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
	inline void MakeParams(
		std::atomic<bool>* stopflag,
		std::function<void(LPCWSTR)> settext,
		std::function<void(WPARAM, LPARAM)> vrcexit,
		CRITICAL_SECTION& cs
	) {
		if (m_Params) return; // ワーカースレッドと待機スレッドはそれぞれ1個しか作らないからな
		m_Params = std::make_unique<MonitorParams>(
			this,
			&m_Waiter,
			stopflag,
			settext,
			vrcexit,
			cs
		);
	}
	inline void StartWorker() { m_Worker.emplace(VRCMonitorThread, m_Params.get()); }
	inline void ShutdownWorker() {
		if (m_Worker.has_value()) {
			m_Params->stopflag->store(true);
			m_Worker.value().join();
			m_Worker.reset();
		}
	}
	DWORD GetProcessID();
	int UserExecute(LPCWSTR lpExePath); // -1:エラー 0:起動している 1:起動した
	static void VRCMonitorThread(MonitorParams* params);
private:
	std::wstring m_ExeFile;
	HANDLE m_hProcess;
	DWORD m_ProcessID;
	ydk::ILogger<WCHAR>* m_Logger;
	std::unique_ptr<MonitorParams> m_Params;
	CRITICAL_SECTION m_cs;
	CRITICAL_SECTION m_execute_cs;
	std::optional<std::thread> m_Worker;
	std::optional<std::thread> m_Waiter;
	bool FindProcess();
	static void Waiter(MonitorParams* params);
	static void VRCWaitThread(
		VRCProcess* vrcp,
		ydk::ILogger<WCHAR>* logger,
		std::atomic<bool>* stopflag
	);
};

