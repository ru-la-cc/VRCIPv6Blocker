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
			std::system_error(errorCode, std::system_category(), To_CP932(message)),
			m_Message(message ? message : GetErrorMessage(errorCode)) {}
		~Win32Exception() {}
		LPCWSTR what_w() const noexcept { return m_Message.c_str(); }
	private:
		std::wstring m_Message;
	};

	// この例外を実装しておこう
	// 続行に耐えられぬ時投げるがいい
	class YDKException final : public std::runtime_error {
	public:
		YDKException(LPCWSTR message = nullptr) :
			std::runtime_error(To_CP932(message)),
			m_Message(message ? message : L"The app crashed") {
		}
		~YDKException() {}
		LPCWSTR what_w() const noexcept { return m_Message.c_str(); }
	private:
		std::wstring m_Message;
	};
}
