#include "vrcprocess.h"
#include "YDKWinUtils.h"
#include <tlhelp32.h>
#include "UserProcessLauncher.h"
#include "ScopedHandle.h"
#include "ComInitializer.h"
#include "ProcessWaiter.h"

bool VRCProcess::FindProcess() {
	ydk::ScopedHandle snapshot{ ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0) };
	if (!snapshot.IsValid()) {
		return false;
	}

	PROCESSENTRY32W pe32;
	pe32.dwSize = sizeof(PROCESSENTRY32W);

	m_ProcessID = 0;
	if (Process32FirstW(snapshot.Get(), &pe32)) {
		do {
			if (_wcsicmp(pe32.szExeFile, m_ExeFile.c_str()) == 0) {
				m_ProcessID = pe32.th32ProcessID;
				break;
			}
		} while (Process32NextW(snapshot.Get(), &pe32));
	}
	if (m_hProcess) {
		if (!::CloseHandle(m_hProcess)) {
			m_Logger->LogError((L"プロセスハンドルのCloseに失敗 : " +
				ydk::GetErrorMessage(::GetLastError())).c_str());
		}
		m_hProcess = nullptr; // Closeできないならどのみち無効だからNULLにしておく
	}
	if (m_ProcessID) {
		m_hProcess = ::OpenProcess(SYNCHRONIZE, FALSE, m_ProcessID);
		if (!m_hProcess) {
			m_ProcessID = 0;
			m_Logger->LogError((L"プロセスハンドルのOpenに失敗 : " +
				ydk::GetErrorMessage(::GetLastError())).c_str());
		}
	}
	return m_ProcessID != 0 && m_hProcess;
}

DWORD VRCProcess::GetProcessID() {
	ydk::CSLock lock(m_cs);
	if (m_hProcess && ::WaitForSingleObject(m_hProcess, 0) == WAIT_TIMEOUT) return m_ProcessID;
	FindProcess();
	return m_ProcessID;
}

int VRCProcess::UserExecute(LPCWSTR lpExePath) {
	ydk::CSLock lock(m_execute_cs);
	if (GetProcessID()) {
		m_Logger->LogWarning(L"既に起動してるので起動しないでほしい");
		return 0;
	}
	auto pid = ydk::ShellExecuteWithLoginUser(lpExePath, false, m_Logger);
	if (!pid) {
		m_Logger->LogError(L"起動できませんでした");
		return -1;
	}
	else {
		m_Logger->Log(L"起動しました");
		return 1;
	}
}

void VRCProcess::VRCMonitorThread(MonitorParams* params) {
	constexpr int SLEEP_CYCLES = 10;
	constexpr DWORD PROCESS_MONITOR_INTERVAL = 100UL;
	ydk::ComInitializer comInitializer;
	params->stopflag->store(false, std::memory_order_relaxed);
	bool isRunning = false;

	params->vrcp->m_Logger->Log(L"VRChatプロセス監視スレッドの開始");
	while (!params->stopflag->load(std::memory_order_relaxed)) {
		if (params->vrcp->GetProcessID()) {
			if (!isRunning) {
				isRunning = true;
				params->vrcp->m_Logger->Log(L"VRChatのプロセスを検出しました");
				params->settext(L"VRChat起動中");
			}
		}
		else {
			if (isRunning) {
				isRunning = false;
				params->settext(L"VRChat未起動");
			}
		}

		if (isRunning) {
			ydk::CSLock lock(params->cs);
			if (!params->waiter->has_value()) {
				params->waiter->emplace(VRCWaitThread, params->vrcp, params->vrcp->m_Logger, params->stopflag);
			}
		} else {
			Waiter(params);
		}
		for (int i = 0; i < SLEEP_CYCLES; ++i) { // プロセス監視するのは1秒おきくらいでいいと思ってる
			if (params->stopflag->load(std::memory_order_relaxed)) break;
			::Sleep(PROCESS_MONITOR_INTERVAL);
		}
	}

	Waiter(params);
	params->vrcp->m_Logger->Log(L"VRChatのプロセス監視スレッドを終了します");
}
void VRCProcess::Waiter(MonitorParams* params) {
	ydk::CSLock lock(params->cs);
	if (params->waiter->has_value()) {
		params->vrcp->m_Logger->Log(L"待機スレッドの完了を待っています...");
		params->waiter->value().join();
		params->waiter->reset();
		params->vrcexit(1, 0);
	}
}

void VRCProcess::VRCWaitThread(
	VRCProcess* vrcp,
	ydk::ILogger<WCHAR>* logger,
	std::atomic<bool>* stopflag
) {
	ydk::ComInitializer comInitializer; // 保険
	ydk::ProcessWaiter pw(vrcp->GetProcessID());

	logger->Log(L"VRChatの待機スレッドを開始します");
	while (pw.Wait(100UL) == WAIT_TIMEOUT) {
		if (stopflag->load(std::memory_order_relaxed)) break;
	}

	// ぶいちゃが終了後 ----------

	if (!pw.IsValid()) {
		logger->LogError(L"VRChatのプロセスハンドル開けてないんだが？");
	}
	logger->Log(L"VRChatの待機スレッドを終了します");
}
