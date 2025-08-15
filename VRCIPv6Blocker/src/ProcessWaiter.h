#pragma once
#include <windows.h>

namespace ydk {
	class ProcessWaiter final {
	public:
		ProcessWaiter(DWORD dwProcessId);
		ProcessWaiter(const ProcessWaiter&) = delete;
		ProcessWaiter& operator=(const ProcessWaiter&) = delete;
		ProcessWaiter(ProcessWaiter&&) = delete;
		ProcessWaiter& operator=(ProcessWaiter&&) = delete;
	private:
		HANDLE m_hProcess;
	};
}
