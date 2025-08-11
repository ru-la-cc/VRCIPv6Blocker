#pragma once
#include <windows.h>
namespace ydkns {
	class AppMutex final {
		HANDLE m_hMutex;
		DWORD m_Error;
	public:
		AppMutex(LPCWSTR lpMutexName);
		~AppMutex();

		// これがtrueなら起動させない敵なことをするなど
		bool IsRefusal() { return (m_hMutex == nullptr || m_Error == ERROR_ALREADY_EXISTS); }
	};
}
