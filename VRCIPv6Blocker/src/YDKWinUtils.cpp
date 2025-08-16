#include "YDKWinUtils.h"
#include <iterator>
#include <fcntl.h>
#include <io.h>

namespace ydk {
	std::wstring GetModuleDir(HMODULE hModule) {
		WCHAR szPath[MAX_PATH];
		if (::GetModuleFileNameW(hModule, szPath, std::size(szPath))) szPath[std::size(szPath) - 1] = L'\0';
		else szPath[0] = L'\0';
		for (LPCWSTR lpChar = szPath + ::lstrlenW(szPath); lpChar > szPath; lpChar = ::CharPrevW(szPath, lpChar)) {
			if (*lpChar == L'\\') {
				*::CharNextW(lpChar) = L'\0';
				break;
			}
		}
		return std::wstring(szPath);
	}

	int GetToUtf8Size(LPCWSTR lpUtf16) {
		return ::WideCharToMultiByte(CP_UTF8, 0, lpUtf16, -1, nullptr, 0, nullptr, nullptr);
	}

	int GetToUtf16Size(LPCSTR lpUtf8) {
		return ::MultiByteToWideChar(CP_UTF8, 0, lpUtf8, -1, nullptr, 0);
	}

	int ToUtf8(LPCWSTR lpUtf16, LPSTR lpUtf8, int buflen) {
		if (lpUtf8 != nullptr && buflen < 1) {
			lpUtf8[0] = '\0';
			return 0;
		}

		int result = ::WideCharToMultiByte(CP_UTF8, 0, lpUtf16, -1, lpUtf8, buflen, nullptr, nullptr);
		if(lpUtf8 != nullptr) lpUtf8[buflen - 1] = '\0';
		return result;
	}

	int ToUtf16(LPCSTR lpUtf8, LPWSTR lpUtf16, int buflen) {
		if (lpUtf16 != nullptr && buflen < 1) {
			lpUtf16[0] = L'\0';
			return 0;
		}
		int result = ::MultiByteToWideChar(CP_UTF8, 0, lpUtf8, -1, lpUtf16, buflen);
		if(lpUtf16 != nullptr) lpUtf16[buflen - 1] = L'\0';
		return result;
	}

	FILE* OpenReadFile(LPCWSTR lpFileName) {
		HANDLE hFile;
		int fd;
		FILE* pfile;

		hFile = ::CreateFileW(lpFileName,
							GENERIC_READ,
							FILE_SHARE_READ,
							nullptr,
							OPEN_EXISTING,
							FILE_ATTRIBUTE_NORMAL,
							nullptr);
		if (hFile == INVALID_HANDLE_VALUE) return nullptr;
		fd = _open_osfhandle((intptr_t)hFile, _O_RDONLY);
		if (fd < 0) {
			::CloseHandle(hFile);
			return nullptr;
		}
		if ((pfile = _fdopen(fd, "rb")) == nullptr) {
			_close(fd);
			return nullptr;
		}
		return pfile;
	}

	FILE* OpenWriteFile(LPCWSTR lpFileName) {
		HANDLE hFile;
		int fd;
		FILE* pfile;

		hFile = ::CreateFileW(lpFileName,
							GENERIC_WRITE,
							0,
							nullptr,
							CREATE_ALWAYS,
							FILE_ATTRIBUTE_NORMAL,
							nullptr);
		if (hFile == INVALID_HANDLE_VALUE) return nullptr;
		fd = _open_osfhandle((intptr_t)hFile, 0);
		if (fd < 0) {
			::CloseHandle(hFile);
			return nullptr;
		}
		if ((pfile = _fdopen(fd, "wb")) == nullptr) {
			_close(fd);
			return nullptr;
		}

		return pfile;
	}

	std::wstring GetErrorMessage(DWORD dwError) {
		WCHAR szMessage[1024];
		std::wstring resultMessage;

		if (::FormatMessageW(
				FORMAT_MESSAGE_FROM_SYSTEM |
				FORMAT_MESSAGE_IGNORE_INSERTS,
				nullptr,
				dwError,
				MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
				szMessage,
				std::size(szMessage),
				nullptr)) {
			szMessage[std::size(szMessage) - 1] = L'\0'; // バッファオーバーしても\0つくかわからんし失敗として戻るのかわからんから保険的に...
		}
		else {
			// しかしエラーメッセージ取りに行く処理でエラーになるとか滑稽だな
			::swprintf_s(szMessage, L"FormatMessage Error = %lu", ::GetLastError());
		}
		
		return { szMessage };
	}
}
