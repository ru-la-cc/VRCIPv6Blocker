#pragma once
#include <windows.h>
#include "ILogger.hpp"

namespace ydkns {
	class FileLogger final : public IFileLogger<WCHAR> {
	public:
		FileLogger(LPCWSTR filePath, bool isAppend = false, bool isAutoFlush = true);
		~FileLogger();
		bool Log(LPCWSTR message) override;
		bool LogWarning(LPCWSTR message) override;
		bool LogError(LPCWSTR message) override;

		bool Open() override;
		bool Close() override;
		inline bool Flush() override {
			if (m_isAutoFlush && !::FlushFileBuffers(m_hFile)) {
				SetError(::GetLastError());
				return false;
			}
			return true;
		}

		inline void SetError(DWORD error, bool isForce = false) {
			::EnterCriticalSection(&m_setterCritical); // 神経質すぎるかこれ
			if (!m_dwError || isForce) m_dwError = error;
			::LeaveCriticalSection(&m_setterCritical);
		}

		[[nodiscard]] constexpr DWORD GetError() const { return m_dwError; }
	private:
		bool m_isAppend, m_isAutoFlush;
		WCHAR m_filePath[MAX_PATH];
		HRESULT m_hResultFileName;
		HANDLE m_hFile;
		DWORD m_dwError = 0;
		CRITICAL_SECTION m_criticalSection, m_setterCritical;

		bool WriteLog(LogType logType, LPCWSTR lpMessage) noexcept;
	};
}