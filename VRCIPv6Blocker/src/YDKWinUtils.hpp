#pragma once
#include <windows.h>
#include <string>

namespace ydkns {
	std::wstring GetModuleDir(HMODULE hModule = nullptr);
}