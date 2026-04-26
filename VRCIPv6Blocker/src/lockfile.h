#pragma once
#include <windows.h>
#include <string>

namespace ydk {
	class LockFile final {
	public:
		LockFile(LPCWSTR lpFilePath);
		~LockFile();
		LockFile(const LockFile&) = delete;
		LockFile& operator=(const LockFile&) = delete;
		LockFile(LockFile&&) = delete;
		LockFile& operator=(LockFile&&) = delete;
		bool IsLocked() const noexcept { return m_hFile != INVALID_HANDLE_VALUE; }
		bool IsExist() const noexcept;
		bool Lock() noexcept;
		bool Lock(LPCBYTE buffer, DWORD bufsize) noexcept;
		bool GetLockInfo(LPBYTE buffer, DWORD bufsize) const noexcept;
		bool Unlock() noexcept;
		[[nodiscard]] DWORD GetError() const noexcept { return m_dwError; }
	private:
		std::wstring m_filePath;
		HANDLE m_hFile;
		mutable DWORD m_dwError;
	};
}
