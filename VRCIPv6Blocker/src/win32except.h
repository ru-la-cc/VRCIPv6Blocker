#pragma once
#include <windows.h>
#include <string>
#include <system_error>
#include "YDKWinUtils.h"

namespace ydk {
	// めんどいのでWinAPIのエラーは全部こいつにまとめてぶん投げてくれる
	class Win32Exception final : public std::system_error {
	public:
		Win32Exception(DWORD errorCode, LPCWSTR message = nullptr) :
			std::system_error(errorCode, std::system_category(), To_Multibyte(message)),
			m_Message(message ? message : GetErrorMessage(errorCode)) {}
		~Win32Exception() {}
	private:
		std::wstring m_Message;
		static std::string To_Multibyte(LPCWSTR message);
	};
}
