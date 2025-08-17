#include "YDKWinUtils.h"
#include <winver.h>
#include <iterator>
#include <fcntl.h>
#include <io.h>
#include <memory>
#include <filesystem>
#include <atlbase.h>
#include <wrl/client.h>

#pragma comment(lib, "version.lib")

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

	void GetAppVersion(PWORD pV1, PWORD pV2, PWORD pV3, PWORD pV4) {
		DWORD dwTemp;
		UINT uSize;
		VS_FIXEDFILEINFO vfi;
		WCHAR szFile[MAX_PATH];
		LPVOID lpvBuf;
		auto hModule = ::GetModuleHandleW(nullptr);
		::GetModuleFileNameW(hModule, szFile, std::size(szFile));
		szFile[std::size(szFile) - 1] = L'\0'; // なんかVista以降は必ずつくらしいが...

		uSize = ::GetFileVersionInfoSizeW(szFile, &dwTemp);
		std::unique_ptr<BYTE[]> lpBuf;
		if (uSize) {
			lpBuf = std::make_unique<BYTE[]>(uSize);
		}
		else {
			if (pV1 != nullptr) *pV1 = 0;
			if (pV2 != nullptr) *pV2 = 0;
			if (pV3 != nullptr) *pV3 = 0;
			if (pV4 != nullptr) *pV4 = 0;
			return;
		}

		if (::GetFileVersionInfoW(szFile, 0 , uSize, lpBuf.get()) &&
			::VerQueryValueW(lpBuf.get(), L"\\", &lpvBuf, &uSize)
			) {
			CopyMemory(&vfi, lpvBuf, sizeof(vfi));
			if (pV1 != nullptr) *pV1 = static_cast<WORD>(vfi.dwFileVersionMS >> 16);
			if (pV2 != nullptr) *pV2 = static_cast<WORD>(vfi.dwFileVersionMS & 0x0000FFFF);
			if (pV3 != nullptr) *pV3 = static_cast<WORD>(vfi.dwFileVersionLS >> 16);
			if (pV4 != nullptr) *pV4 = static_cast<WORD>(vfi.dwFileVersionLS & 0x0000FFFF);
		}
		else {
			if (pV1 != nullptr) *pV1 = 0;
			if (pV2 != nullptr) *pV2 = 0;
			if (pV3 != nullptr) *pV3 = 0;
			if (pV4 != nullptr) *pV4 = 0;
		}
	}

	BOOL OpenFileName(HWND hWnd, LPWSTR lpFileName, DWORD dwLen, LPCWSTR lpTitle,
		DWORD dwFlg, LPCWSTR lpFilter, LPCWSTR lpInitialDir, LPCWSTR lpDefExt) {
		if (dwLen == 0 || lpFileName == nullptr) return FALSE;
		OPENFILENAMEW ofn = {};
		ofn.lStructSize = sizeof(ofn);
		ofn.hwndOwner = hWnd;
		ofn.lpstrFilter = lpFilter;
		// ofn.lpstrFilter = TEXT("CSVファイル(*.csv)\0*.csv\0テキストファイル(*.txt)\0*.txt\0すべてのファイル(*.*)\0*.*\0\0");
		ofn.nFilterIndex = 1;							// フィルターの初期位置
		ofn.lpstrFile = lpFileName;						// ファイル名用文字列バッファ
		ofn.nMaxFile = dwLen;							// 文字列バッファのサイズ
		ofn.lpstrInitialDir = lpInitialDir;
		ofn.lpstrTitle = lpTitle;						// タイトル
		ofn.Flags = dwFlg;
		ofn.lpstrDefExt = lpDefExt;

		if (::GetOpenFileNameW(&ofn)) {
			lpFileName[dwLen - 1] = L'\0';
			return TRUE;
		}
		return FALSE;
	}

	BOOL SaveFileName(HWND hWnd, LPWSTR lpFileName, DWORD dwLen, LPCWSTR lpTitle,
		DWORD dwFlg, LPCWSTR lpFilter, LPCWSTR lpInitialDir, LPCWSTR lpDefExt) {
		if (dwLen == 0 || lpFileName == nullptr) return FALSE;
		OPENFILENAMEW ofn = {};
		ofn.lStructSize = sizeof(ofn);
		ofn.hwndOwner = hWnd;
		ofn.lpstrFilter = lpFilter;
		// ofn.lpstrFilter = TEXT("CSVファイル(*.csv)\0*.csv\0テキストファイル(*.txt)\0*.txt\0すべてのファイル(*.*)\0*.*\0\0");
		ofn.nFilterIndex = 1;							// フィルターの初期位置
		ofn.lpstrFile = lpFileName;						// ファイル名用文字列バッファ
		ofn.nMaxFile = dwLen;							// 文字列バッファのサイズ
		ofn.lpstrInitialDir = lpInitialDir;
		ofn.lpstrTitle = lpTitle;						// タイトル
		ofn.Flags = dwFlg;
		ofn.lpstrDefExt = lpDefExt;

		if (::GetSaveFileNameW(&ofn)) {
			lpFileName[dwLen - 1] = L'\0';
			return TRUE;
		}
		return FALSE;
	}

	bool SelectFolder(HWND hWnd, LPWSTR lpDir, int nLen, LPCWSTR lpTitle, KNOWNFOLDERID initialFolder) {
		if (lpDir == nullptr || nLen == 0) return false;
		lpDir[0] = L'\0';

		Microsoft::WRL::ComPtr<IFileOpenDialog> pfd;
		HRESULT hr = ::CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pfd));
		if (FAILED(hr)) return false;

		DWORD opts = 0;
		if (SUCCEEDED(pfd->GetOptions(&opts))) {
			opts |= FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_NOCHANGEDIR;
			pfd->SetOptions(opts);
		}

		if (lpTitle) pfd->SetTitle(lpTitle);

		if (initialFolder != GUID_NULL) {
			Microsoft::WRL::ComPtr<IShellItem> init;
			if (SUCCEEDED(::SHGetKnownFolderItem(initialFolder, KF_FLAG_DEFAULT, nullptr, IID_PPV_ARGS(&init)))) {
				pfd->SetDefaultFolder(init.Get());
				pfd->SetFolder(init.Get()); // 必要に応じて片方でもOK
			}
		}

		hr = pfd->Show(hWnd);
		if (hr == HRESULT_FROM_WIN32(ERROR_CANCELLED)) return false;
		if (FAILED(hr)) return false;

		Microsoft::WRL::ComPtr<IShellItem> result;
		hr = pfd->GetResult(&result);
		if (FAILED(hr)) return false;

		PWSTR pszPath = nullptr;
		hr = result->GetDisplayName(SIGDN_FILESYSPATH, &pszPath);
		if (FAILED(hr)) return false;

		const size_t need = wcslen(pszPath) + 1;
		if (need > nLen) {
			::CoTaskMemFree(pszPath);
			::SetLastError(ERROR_INSUFFICIENT_BUFFER); // うーん...
			return false;
		}
		wmemcpy(lpDir, pszPath, need);
		::CoTaskMemFree(pszPath);
		return true;
	}

	bool CreateShortcut(LPCWSTR lpShortcutName, LPCWSTR lpLinkPath, LPCWSTR lpWorkDir = nullptr,
		LPCWSTR lpIconFile = nullptr, int nIconIndex = 0, LPCWSTR lpArguments = nullptr,
		LPCWSTR lpDescription = nullptr, int nShow = SW_SHOWNORMAL, WORD wHotkey = 0) {

		if (!lpShortcutName || !*lpShortcutName || !lpLinkPath || !*lpLinkPath) {
			SetLastError(ERROR_INVALID_PARAMETER);
			return false;
		}

		std::filesystem::path out(lpShortcutName);
		if (!out.has_extension() || _wcsicmp(out.extension().c_str(), L".lnk")) {
			out.replace_extension(L".lnk");
		}

		std::error_code fec;
		if (!out.parent_path().empty()) {
			std::filesystem::create_directories(out.parent_path(), fec);
		}

		CComPtr<IShellLinkW> pLink;
		CComPtr<IPersistFile> pFile;

		HRESULT hr = pLink.CoCreateInstance(CLSID_ShellLink);
		if (FAILED(hr)) return false;

		hr = pLink->SetPath(lpLinkPath);
		if (SUCCEEDED(hr)) {
			if (lpWorkDir && *lpWorkDir) {
				hr = pLink->SetWorkingDirectory(lpWorkDir);
			}
			else {
				std::filesystem::path tgt(lpLinkPath);
				if (tgt.has_parent_path()) {
					pLink->SetWorkingDirectory(tgt.parent_path().c_str());
				}
			}
		}
		if (SUCCEEDED(hr) && lpIconFile && *lpIconFile) hr = pLink->SetIconLocation(lpIconFile, nIconIndex);
		if (SUCCEEDED(hr) && lpArguments && *lpArguments) hr = pLink->SetArguments(lpArguments);
		if (SUCCEEDED(hr) && lpDescription && *lpDescription) hr = pLink->SetDescription(lpDescription);
		if (SUCCEEDED(hr)) hr = pLink->SetShowCmd(nShow);
		if (SUCCEEDED(hr) && wHotkey != 0) hr = pLink->SetHotkey(wHotkey); // 下位:VK_xxx, 上位:HOTKEYF_xxx

		if (SUCCEEDED(hr)) {
			hr = pLink->QueryInterface(&pFile);
			if (SUCCEEDED(hr)) {
				hr = pFile->Save(out.c_str(), TRUE); // 上書き
				if (SUCCEEDED(hr)) {
					pFile->SaveCompleted(out.c_str());
				}
			}
		}

		return SUCCEEDED(hr);
	}

	bool GetKnownFolderPath(std::wstring& path, KNOWNFOLDERID folderId) {
		PWSTR pPath;

		HRESULT hr = ::SHGetKnownFolderPath(
			folderId,
			0,
			nullptr,
			&pPath
		);

		if (SUCCEEDED(hr)) {
			path = pPath;
			::CoTaskMemFree(pPath);
			return true;
		}
		if (pPath != nullptr) ::CoTaskMemFree(pPath); // ここにくる状況になるパターンがそもそもあるのか謎だが
		path.clear();

		return false;
	}
}
