#include "FileLogger.hpp"
#include <strsafe.h>
#include <iterator>

namespace ydkns {
	FileLogger::FileLogger(LPCWSTR filePath, bool isAppend, bool isAutoFlush) {
		::InitializeCriticalSection(&m_criticalSection);
		::InitializeCriticalSection(&m_setterCritical);
		m_isAppend = isAppend;
		m_isAutoFlush = isAutoFlush;
		m_hResultFileName = ::StringCchCopyW(m_filePath, std::size(m_filePath), filePath);
		Open();
	}

	FileLogger::~FileLogger() {
		if(m_hFile != INVALID_HANDLE_VALUE) Close();
		::DeleteCriticalSection(&m_setterCritical);
		::DeleteCriticalSection(&m_criticalSection);
	}

	bool FileLogger::Log(LPCWSTR message){
		bool result;
		::EnterCriticalSection(&m_criticalSection);
		result = WriteLog(LogType::Info, message);
		::LeaveCriticalSection(&m_criticalSection);
		return result;
	}

	bool FileLogger::LogWarning(LPCWSTR message) {
		bool result;
		::EnterCriticalSection(&m_criticalSection);
		result = WriteLog(LogType::Warning, message);
		::LeaveCriticalSection(&m_criticalSection);
		return result;
	}

	bool FileLogger::LogError(LPCWSTR message) {
		bool result;
		::EnterCriticalSection(&m_criticalSection);
		result = WriteLog(LogType::Error, message);
		::LeaveCriticalSection(&m_criticalSection);
		return result;
	}

	bool FileLogger::Open() {
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

	bool FileLogger::Close() {
		bool result;
		::EnterCriticalSection(&m_criticalSection);
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
		::LeaveCriticalSection(&m_criticalSection);
		return result;
	}

	// private 
	bool FileLogger::WriteLog(LogType logType, LPCWSTR lpMessage) noexcept {

		if (m_hFile == INVALID_HANDLE_VALUE) return false;
		SYSTEMTIME st;
		::GetLocalTime(&st);
		WCHAR szTimeStampAndType[64];
		LPCWSTR LogTypes[] = { L"INFO\t", L"WARNING\t", L"ERROR\t" };

		if (!SUCCEEDED(::StringCchPrintfW(szTimeStampAndType,
				std::size(szTimeStampAndType),
				L"%04u-%02u-%02u %02u:%02u:%02u.%03u\t%s",
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
		

		DWORD dwWrite;
		if (!::WriteFile(m_hFile, szTimeStampAndType, ::wcslen(szTimeStampAndType) * sizeof(WCHAR), &dwWrite, nullptr)) {
			SetError(::GetLastError());
			return false;
		}
		if (!::WriteFile(m_hFile, lpMessage, ::wcslen(lpMessage) * sizeof(WCHAR), &dwWrite, nullptr)) {
			SetError(::GetLastError());
			return false;
		}
		if (!::WriteFile(m_hFile, L"\n", 1 * sizeof(WCHAR), &dwWrite, nullptr)) {
			SetError(::GetLastError());
			return false;
		}		if (m_isAutoFlush) return Flush();
		return true;
	}

}