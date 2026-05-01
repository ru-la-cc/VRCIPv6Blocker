#include "vrcprocess.h"
#include "YDKWinUtils.h"
#include <tlhelp32.h>

bool VRCProcess::FindProcess() {
	struct SnapshotHandle final {
		HANDLE handle;
		SnapshotHandle() : handle(::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0)) {}
		~SnapshotHandle() { if (handle != INVALID_HANDLE_VALUE) ::CloseHandle(handle); }
	} snapshot;
	if (snapshot.handle == INVALID_HANDLE_VALUE) {
		return false;
	}

	PROCESSENTRY32W pe32;
	pe32.dwSize = sizeof(PROCESSENTRY32W);

	m_ProcessID = 0;
	if (Process32FirstW(snapshot.handle, &pe32)) {
		do {
			if (_wcsicmp(pe32.szExeFile, m_ExeFile.c_str()) == 0) {
				m_ProcessID = pe32.th32ProcessID;
				break;
			}
		} while (Process32NextW(snapshot.handle, &pe32));
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
	if (m_hProcess && ::WaitForSingleObject(m_hProcess, 0) == WAIT_TIMEOUT) return m_ProcessID;
	FindProcess();
	return m_ProcessID;
}
