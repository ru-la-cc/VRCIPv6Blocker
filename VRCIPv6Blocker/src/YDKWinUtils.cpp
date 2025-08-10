#include "YDKWinUtils.hpp"
#include <iterator>

namespace ydkns {
	std::wstring GetModuleDir(HMODULE hModule) {
		WCHAR szPath[MAX_PATH];
		if (::GetModuleFileNameW(hModule, szPath, std::size(szPath))) szPath[std::size(szPath) - 1] = L'\0';
		else szPath[0] = L'\0';
		for (LPCWSTR lpChar = szPath + ::lstrlenW(szPath); lpChar > szPath; lpChar = ::CharPrev(szPath, lpChar)) {
			if (*lpChar == L'\\') {
				*::CharNext(lpChar) = L'\0';
				break;
			}
		}
		return std::wstring(szPath);
	}
}