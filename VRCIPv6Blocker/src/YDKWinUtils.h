#pragma once
#include <windows.h>
#include <string>
#include <cstdio>
#include <shlobj.h>

namespace ydk {
	std::wstring GetModuleDir(HMODULE hModule = nullptr);
	int GetToUtf8Size(LPCWSTR lpUtf16);
	int GetToUtf16Size(LPCSTR lpUtf8);
	int ToUtf8(LPCWSTR lpUtf16, LPSTR lpUtf8, int buflen);
	int ToUtf16(LPCSTR lpUtf8, LPWSTR lpUtf16, int buflen);
	[[nodiscard]] FILE* OpenReadFile(LPCWSTR lpFileName, bool isCreate = true);
	[[nodiscard]] FILE* OpenWriteFile(LPCWSTR lpFileName);
	[[nodiscard]] std::wstring GetErrorMessage(DWORD dwError);
	void GetAppVersion(PWORD pV1, PWORD pV2, PWORD pV3, PWORD pV4);
	BOOL OpenFileName(HWND hWnd, LPWSTR lpFileName, DWORD dwLen, LPCWSTR lpTitle = nullptr,
		DWORD dwFlg = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY,
		LPCWSTR lpFilter = L"すべてのファイル(*.*)\0*.*\0\0",
		LPCWSTR lpInitialDir = nullptr, LPCWSTR lpDefExt = nullptr);
	BOOL SaveFileName(HWND hWnd, LPWSTR lpFileName, DWORD dwLen, LPCWSTR lpTitle = nullptr,
		DWORD dwFlg = OFN_OVERWRITEPROMPT,
		LPCWSTR lpFilter = L"すべてのファイル(*.*)\0*.*\0\0",
		LPCWSTR lpInitialDir = nullptr, LPCWSTR lpDefExt = nullptr);
	bool SelectFolder(HWND hWnd, LPWSTR lpDir, int nLen,
		LPCWSTR lpTitle = L"フォルダの参照", KNOWNFOLDERID initialFolder = GUID_NULL);
	bool CreateShortcut(LPCWSTR lpShortcutName, LPCWSTR lpLinkPath, LPCWSTR lpWorkDir = nullptr,
		LPCWSTR lpIconFile = nullptr, int nIconIndex = 0, LPCWSTR lpArguments = nullptr,
		LPCWSTR lpDescription = nullptr, int nShow = SW_SHOWNORMAL, WORD wHotkey = 0);
	bool GetKnownFolderPath(std::wstring& path, KNOWNFOLDERID folderId = FOLDERID_Desktop);
}
