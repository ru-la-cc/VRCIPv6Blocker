#include "FileLogger.h"
#include "YDKWinUtils.h"
#include <strsafe.h>
#include <iterator>
#include <cstring>

namespace ydk {
	FileLogger::FileLogger(LPCWSTR filePath, bool isAppend, bool isAutoFlush) noexcept {
		::InitializeCriticalSection(&m_criticalSection);
		m_isAppend = isAppend;
		m_isAutoFlush = isAutoFlush;
		m_hResultFileName = ::StringCchCopyW(m_filePath, std::size(m_filePath), filePath);
		Open();
	}

	FileLogger::~FileLogger() noexcept {
		if(m_hFile != INVALID_HANDLE_VALUE) Close();
		::DeleteCriticalSection(&m_criticalSection);
	}

	bool FileLogger::Log(LPCWSTR message) noexcept {
		bool result;
		CSLock lock(m_criticalSection);
		result = WriteLog(LogType::Info, message);
		return result;
	}

	bool FileLogger::LogWarning(LPCWSTR message) noexcept {
		bool result;
		CSLock lock(m_criticalSection);
		result = WriteLog(LogType::Warning, message);
		return result;
	}

	bool FileLogger::LogError(LPCWSTR message) noexcept {
		bool result;
		CSLock lock(m_criticalSection);
		result = WriteLog(LogType::Error, message);
		return result;
	}

	bool FileLogger::Open() noexcept {
		if (SUCCEEDED(m_hResultFileName)) {
			m_hFile = ::CreateFileW(m_filePath,
				GENERIC_WRITE,
				FILE_SHARE_READ,
				NULL,
				m_isAppend ? OPEN_ALWAYS : CREATE_ALWAYS,
				FILE_ATTRIBUTE_NORMAL,
				NULL);
			if (m_hFile != INVALID_HANDLE_VALUE) {
				if (m_isAppend) {
					LONG lHigh = 0;
					if (::SetFilePointer(m_hFile, 0, &lHigh, FILE_END) == INVALID_SET_FILE_POINTER) {
						::CloseHandle(m_hFile);
						m_hFile = INVALID_HANDLE_VALUE;
						SetError(::GetLastError());
						return false;
					}
				}
			}
			else {
				SetError(::GetLastError());
				return false;
			}
		}
		else {
			m_hFile = INVALID_HANDLE_VALUE;
			SetError(::GetLastError());
			return false;
		}
		return true;
	}

	bool FileLogger::Close() noexcept {
		bool result;
		CSLock lock(m_criticalSection);
		if (m_hFile != INVALID_HANDLE_VALUE) {
			if (::CloseHandle(m_hFile)) {
				m_hFile = INVALID_HANDLE_VALUE;
				result = true;
			}
			else {
				result = false;
				SetError(::GetLastError());
			}
		}
		else {
			result = false;
		}
		return result;
	}

	// private 
	bool FileLogger::WriteLog(LogType logType, LPCWSTR lpMessage) noexcept {

		if (m_hFile == INVALID_HANDLE_VALUE) return false;
		SYSTEMTIME st;
		::GetLocalTime(&st);
		char szTimeStampAndType[64];
		const char* LogTypes[] = { "INFO\t", "WARNING\t", "ERROR\t" };

		if (!SUCCEEDED(::StringCchPrintfA(szTimeStampAndType,
				std::size(szTimeStampAndType),
				"%04u-%02u-%02u %02u:%02u:%02u.%03u\t%s",
				st.wYear,
				st.wMonth,
				st.wDay,
				st.wHour,
				st.wMinute,
				st.wSecond,
				st.wMilliseconds,
				LogTypes[static_cast<size_t>(logType) >= std::size(LogTypes) ? 0 : static_cast<size_t>(logType)]))) {
			SetError(::GetLastError());
		}

		ToUtf8(lpMessage, m_szLogBuf, sizeof(m_szLogBuf));
		for (char* p = m_szLogBuf; *p; ++p) {
			if (*p == '\r' || *p == '\n' || *p == '\t') *p = ' ';
		}

		DWORD dwWrite;
		if (!::WriteFile(m_hFile, szTimeStampAndType,
				static_cast<DWORD>(std::strlen(szTimeStampAndType)), &dwWrite, nullptr)) {
			SetError(::GetLastError());
			return false;
		}
		if (!::WriteFile(m_hFile, m_szLogBuf,
				static_cast<DWORD>(std::strlen(m_szLogBuf)), &dwWrite, nullptr)) {
			SetError(::GetLastError());
			return false;
		}
		if (!::WriteFile(m_hFile, "\n", 1, &dwWrite, nullptr)) {
			SetError(::GetLastError());
			return false;
		}
		if (m_isAutoFlush) return Flush();
		return true;
	}

	char  FileLogger::m_szLogBuf[MAX_LOG_SIZE] = {};
	WCHAR  FileLogger::m_szLogBufW[MAX_LOG_SIZE] = {};

	bool FileLogger::Format(LogType logtype, const LPCWSTR fmtmsg, va_list args) noexcept {
		bool result = _vsnwprintf_s(m_szLogBufW, std::size(m_szLogBufW), _TRUNCATE, fmtmsg, args) != -1;
		return result && WriteLog(logtype, m_szLogBufW);
	}
}
