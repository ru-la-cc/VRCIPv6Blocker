#pragma once
#include <windows.h>
namespace ydkns {
	class AppMutex final {
		HANDLE m_hMutex;
		DWORD m_Error;
	public:
		AppMutex(LPCWSTR lpMutexName) noexcept : m_hMutex(nullptr), m_Error(0) {
			m_hMutex = ::CreateMutexW(nullptr, FALSE, lpMutexName);
			m_Error = ::GetLastError();
		}

		~AppMutex() noexcept {
			if (m_hMutex != nullptr) {
				::CloseHandle(m_hMutex);
				m_hMutex = nullptr;
			}
			m_Error = 0;
		}

		// これがtrueなら起動させない敵なことをするなど
		inline bool IsRunning() const noexcept{ return (m_hMutex == nullptr || m_Error == ERROR_ALREADY_EXISTS); }
	};
}
