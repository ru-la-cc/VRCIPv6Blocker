#pragma once
#include <windows.h>

namespace ydk{
	constexpr LPCWSTR WhiteListExt[] = {
		L".exe", L".lnk", L".url", L".bat", L".cmd", L".com", L".msi", L".scr"
	};
	bool IsWhiteListFile(LPCWSTR lpFileName);
	DWORD ShellExecuteWithLoginUser(LPCWSTR lpExePath, bool isComInitialize = false);
}
