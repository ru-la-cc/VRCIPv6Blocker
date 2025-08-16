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
		}
		~ProcessWaiter() noexcept {
			if (m_hProcess != nullptr) {
				::CloseHandle(m_hProcess);
				m_hProcess = nullptr;
			}
		}
		bool IsValid() const noexcept { return m_hProcess != nullptr; }
		DWORD Wait(DWORD timeout = INFINITE) const noexcept {
			if (m_hProcess == nullptr) return WAIT_FAILED;
			return ::WaitForSingleObject(m_hProcess, timeout);
		}
		bool ExitCode(DWORD& exitCode) const noexcept {
			if (m_hProcess == nullptr) return false;
			return ::GetExitCodeProcess(m_hProcess, &exitCode);
		}
		ProcessWaiter(const ProcessWaiter&) = delete;
		ProcessWaiter& operator=(const ProcessWaiter&) = delete;
		ProcessWaiter(ProcessWaiter&&) = delete;
		ProcessWaiter& operator=(ProcessWaiter&&) = delete;
	private:
		HANDLE m_hProcess = nullptr;
	};
}
