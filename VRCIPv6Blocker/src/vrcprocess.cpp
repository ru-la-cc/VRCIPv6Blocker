#include "vrcprocess.h"
#include "YDKWinUtils.h"
#include <tlhelp32.h>
#include "UserProcessLauncher.h"
#include "ScopedHandle.h"

bool VRCProcess::FindProcess() {
	//struct SnapshotHandle final {
	//	HANDLE handle;
	//	SnapshotHandle() : handle(::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0)) {}
	//	~SnapshotHandle() { if (handle != INVALID_HANDLE_VALUE) ::CloseHandle(handle); }
	//} snapshot;
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
	CSLock lock(m_cs);
	if (m_hProcess && ::WaitForSingleObject(m_hProcess, 0) == WAIT_TIMEOUT) return m_ProcessID;
	FindProcess();
	return m_ProcessID;
}

int VRCProcess::UserExecute(LPCWSTR lpExePath) {
	if (GetProcessID()) {
		m_Logger->LogError(L"既に起動してるので起動しないでほしい");
		return 0;
	}
	auto pid = ydk::ShellExecuteWithLoginUser(lpExePath);
	if (!pid) {
		m_Logger->LogError(L"起動できませんでした");
		return -1;
	}
	else {
		WCHAR szMsg[256];
		::swprintf_s(szMsg, L"プロセスID(%lu)で起動しました(ランチャーの可能性もあるからこのPIDは信用できん)", pid);
		m_Logger->Log(szMsg);
		return 1;
	}
}
