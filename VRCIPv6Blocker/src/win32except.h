#pragma once
#include <windows.h>
#include <string>
#include <system_error>
#include "YDKWinUtils.h"

namespace ydk {
	// WinAPIのエラーは全部こいつにまとめてぶん投げてくれる
	class Win32Exception final : public std::system_error {
	public:
		Win32Exception(DWORD errorCode, LPCWSTR message = L"Win32Exception") :
			m_Message(message),std::system_error(errorCode, std::system_category(), To_Multibyte(message)) {}
		~Win32Exception() {}
	private:
		std::wstring m_Message;
		std::string To_Multibyte(LPCWSTR message);
	};
}
