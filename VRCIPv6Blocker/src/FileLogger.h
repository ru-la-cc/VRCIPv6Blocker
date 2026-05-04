#pragma once
#include <windows.h>
#include "ILogger.h"
#include "YDKWinUtils.h"

namespace ydk {
	class FileLogger final : public IFileLogger<WCHAR> {
	public:
		FileLogger(LPCWSTR filePath, bool isAppend = false, bool isAutoFlush = true) noexcept;
		~FileLogger() noexcept;
		bool Log(LPCWSTR message) noexcept override;
		bool LogWarning(LPCWSTR message)  noexcept override;
		bool LogError(LPCWSTR message) noexcept override;

		bool Open() noexcept override;
		bool Close() noexcept override;
		bool Flush() noexcept override {
			if (m_isAutoFlush && !::FlushFileBuffers(m_hFile)) {
				SetError(::GetLastError());
				return false;
			}
			return true;
		}

		void SetError(DWORD error, bool isForce = false) noexcept {
			CSLock lock(m_criticalSection);
			if (!m_dwError || isForce) m_dwError = error;
		}

		[[nodiscard]] inline constexpr DWORD GetError() const noexcept { return m_dwError; }
	protected:
		bool Format(LogType logtype, LPCWSTR fmtmsg, va_list args) noexcept override;
	private:
		static constexpr int MAX_LOG_SIZE = 2048;
		static char m_szLogBuf[MAX_LOG_SIZE];
		static WCHAR m_szLogBufW[MAX_LOG_SIZE];
		bool m_isAppend, m_isAutoFlush;
		WCHAR m_filePath[MAX_PATH];
		HRESULT m_hResultFileName;
		HANDLE m_hFile;
		DWORD m_dwError = 0;
		CRITICAL_SECTION m_criticalSection;

		bool WriteLog(LogType logType, LPCWSTR lpMessage) noexcept;
	};
}
