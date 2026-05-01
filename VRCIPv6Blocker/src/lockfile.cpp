#include "lockfile.h"
#include <filesystem>

namespace ydk {
	LockFile::LockFile(LPCWSTR lpFilePath) :
			m_filePath(lpFilePath), m_hFile(INVALID_HANDLE_VALUE), m_dwError(0) { }

	LockFile::~LockFile() {
		if (m_hFile != INVALID_HANDLE_VALUE) {
			::CloseHandle(m_hFile);
			m_hFile = INVALID_HANDLE_VALUE;
		}
	}

	bool LockFile::IsExist() const noexcept {
		try {
			return std::filesystem::exists(m_filePath.c_str());
		} catch (...) {
			return false;
		}
	}

	bool LockFile::Lock() noexcept {
		if (IsLocked()) return false;
		m_hFile = ::CreateFileW(m_filePath.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (m_hFile == INVALID_HANDLE_VALUE) {
			m_dwError = ::GetLastError();
			return false;
		}
		m_dwError = ERROR_SUCCESS;
		return true;
	}

	bool LockFile::Lock(LPCBYTE buffer, DWORD bufsize) noexcept {
		if (!Lock()) return false;
		if (buffer && bufsize > 0) {
			DWORD bytesWritten;
			if (!::WriteFile(m_hFile, buffer, bufsize, &bytesWritten, nullptr) || bytesWritten != bufsize) {
				m_dwError = ::GetLastError();
				::CloseHandle(m_hFile);
				m_hFile = INVALID_HANDLE_VALUE;
				::DeleteFileW(m_filePath.c_str());
				return false;
			}
			if (!::FlushFileBuffers(m_hFile)) {
				m_dwError = ::GetLastError();
				::CloseHandle(m_hFile);
				m_hFile = INVALID_HANDLE_VALUE;
				::DeleteFileW(m_filePath.c_str());
				return false;
			}
		}
		m_dwError = ERROR_SUCCESS;
		return true;
	}

	bool LockFile::Reacquire() noexcept {
		if (IsLocked()) return false;
		m_hFile = ::CreateFileW(m_filePath.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (m_hFile == INVALID_HANDLE_VALUE) {
			m_dwError = ::GetLastError();
			return false;
		}
		m_dwError = ERROR_SUCCESS;
		return true;
	}

	bool LockFile::GetLockInfo(LPBYTE buffer, DWORD bufsize) const noexcept {
		DWORD readBytes;
		if (IsLocked()) {
			if (::SetFilePointer(m_hFile, 0, nullptr, FILE_BEGIN) == INVALID_SET_FILE_POINTER) {
				m_dwError = ::GetLastError();
				return false;
			}
		}
		if (!::ReadFile(m_hFile, buffer, bufsize, &readBytes, nullptr) || readBytes != bufsize) {
			m_dwError = ::GetLastError();
			return false;
		}
		m_dwError = ERROR_SUCCESS;
		return true;
	}

	bool LockFile::Unlock() noexcept {
		if (IsLocked() && !::CloseHandle(m_hFile)) {
			m_dwError = ::GetLastError();
			return false;
		}
		m_hFile = INVALID_HANDLE_VALUE;
		m_dwError = ERROR_SUCCESS;
		return true;
	}

	bool LockFile::Cleanup() noexcept {
		Unlock();
		if (!::DeleteFileW(m_filePath.c_str())) {
			m_dwError = ::GetLastError();
			return false;
		}
		m_dwError = ERROR_SUCCESS;
		return true;
	}
}
