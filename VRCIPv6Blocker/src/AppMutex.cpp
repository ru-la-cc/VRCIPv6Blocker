#include "AppMutex.hpp"

namespace ydkns {
	AppMutex::AppMutex(LPCWSTR lpMutexName) : m_hMutex(nullptr), m_Error(0) {
		SECURITY_DESCRIPTOR sd;
		::ZeroMemory(&sd, sizeof(sd));
		if (::InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION)) {
			SECURITY_ATTRIBUTES sa;
			::ZeroMemory(&sa, sizeof(sa));
			sa.nLength = sizeof(sa);
			sa.lpSecurityDescriptor = &sd;
			sa.bInheritHandle = TRUE;
			m_hMutex = ::CreateMutexW(&sa, FALSE, lpMutexName);
			m_Error = ::GetLastError();
		}
	}

	AppMutex::~AppMutex() {
		if (m_hMutex != nullptr) {
			if (m_Error != ERROR_ALREADY_EXISTS) {
				::ReleaseMutex(m_hMutex);
			}
			::CloseHandle(m_hMutex);
			m_hMutex = nullptr;
		}
		m_Error = 0;
	}
}
