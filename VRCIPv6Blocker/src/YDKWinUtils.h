#pragma once
#include <windows.h>
#include <string>
#include <cstdio>

namespace ydk {
	std::wstring GetModuleDir(HMODULE hModule = nullptr);
	int GetToUtf8Size(LPCWSTR lpUtf16);
	int GetToUtf16Size(LPCSTR lpUtf8);
	int ToUtf8(LPCWSTR lpUtf16, LPSTR lpUtf8, int buflen);
	int ToUtf16(LPCSTR lpUtf8, LPWSTR lpUtf16, int buflen);
	[[nodiscard]] FILE* OpenReadFile(LPCWSTR lpFileName);
	[[nodiscard]] FILE* OpenWriteFile(LPCWSTR lpFileName);
	[[nodiscard]] std::wstring GetErrorMessage(DWORD dwError);
}
