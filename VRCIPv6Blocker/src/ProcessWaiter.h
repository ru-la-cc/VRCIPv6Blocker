#pragma once
#include <windows.h>

namespace ydk {
	class ProcessWaiter final {
	public:
		ProcessWaiter(DWORD dwProcessId) noexcept {
			m_hProcess = ::OpenProcess(
				SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION,
				FALSE,
				dwProcessId
			);
			if (m_hProcess != nullptr) {
				DWORD dwLen = MAX_PATH;
				m_exePath = new WCHAR[dwLen];
				if (!::QueryFullProcessImageNameW(m_hProcess, 0, m_exePath, &dwLen)) {
					m_exePath[0] = L'\0';
				}
			}
		}
		~ProcessWaiter() noexcept {
			if (m_hProcess != nullptr) {
				::CloseHandle(m_hProcess);
				m_hProcess = nullptr;
			}
			delete[] m_exePath;
			m_exePath = nullptr;
		}
		[[nodiscard]] inline bool IsValid() const noexcept { return m_hProcess != nullptr; }
		DWORD inline Wait(DWORD timeout = INFINITE) const noexcept {
			if (m_hProcess == nullptr) return WAIT_FAILED;
			return ::WaitForSingleObject(m_hProcess, timeout);
		}
		[[nodiscard]] inline bool ExitCode(DWORD& exitCode) const noexcept {
			if (m_hProcess == nullptr) return false;
			return ::GetExitCodeProcess(m_hProcess, &exitCode);
		}
		inline LPCWSTR GetExePath() const { return m_exePath == nullptr ? L"" : m_exePath; }
		ProcessWaiter(const ProcessWaiter&) = delete;
		ProcessWaiter& operator=(const ProcessWaiter&) = delete;
		ProcessWaiter(ProcessWaiter&&) = delete;
		ProcessWaiter& operator=(ProcessWaiter&&) = delete;
	private:
		HANDLE m_hProcess = nullptr;
		LPWSTR m_exePath = nullptr;
	};
}
