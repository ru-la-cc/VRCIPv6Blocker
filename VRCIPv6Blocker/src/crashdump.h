#pragma once
#include <windows.h>
#include <DbgHelp.h>
#include <strsafe.h>
#include <iterator>
#include <cwchar>
#include "ScopedHandle.h"

#pragma comment(lib, "DbgHelp.lib")

namespace ydk {
	// クラッシュしたスレッドで呼ばれるから不安定ではあるので気休めだと思ってほしい
	class CrashDump final {
	public:
		CrashDump() = delete;
		CrashDump(const CrashDump&) = delete;
		CrashDump& operator=(const CrashDump&) = delete;
		CrashDump(CrashDump&&) = delete;
		CrashDump& operator=(CrashDump&&) = delete;
		static void Install() noexcept {
			DWORD result = ::GetModuleFileNameW(nullptr, m_szDumpPath, std::size(m_szDumpPath));
			if (result == 0 || result >= std::size(m_szDumpPath)) {
				m_szDumpPath[0] = L'\0'; // パス長すぎたらカレントディレクトリになるからね
			}
			for (
				LPCWSTR lpChar = m_szDumpPath + std::wcslen(m_szDumpPath);
				lpChar > m_szDumpPath;
				lpChar = ::CharPrevW(m_szDumpPath, lpChar)
				) {
				if (*lpChar == L'\\') {
					*::CharNextW(lpChar) = L'\0';
					break;
				}
			}
			m_DefFilter = ::SetUnhandledExceptionFilter(UnhandledExceptionFilter);
		}
		static void UnInstall() noexcept { ::SetUnhandledExceptionFilter(m_DefFilter); }
	private:
		static constexpr WCHAR PATH_PREFIX[] = L"\\\\?\\";
		static constexpr size_t m_PrefixLen = std::size(PATH_PREFIX) - 1;
		inline static WCHAR m_szDumpPath[MAX_PATH] = {};
		inline static WCHAR m_szFileName[64] = {};
		inline static WCHAR m_szDumpFileName[MAX_PATH * 2] = {}; // 深い場所に置くのも限度をわきまえてほしい
		inline static LPTOP_LEVEL_EXCEPTION_FILTER m_DefFilter = nullptr;
		static LONG WINAPI UnhandledExceptionFilter(EXCEPTION_POINTERS* pEx) {
			SYSTEMTIME st{};
			::GetLocalTime(&st);
			::StringCchPrintfW(
				m_szFileName,
				std::size(m_szFileName),
				L"crash_%04u%02u%02u_%02u%02u%02u.%03u_%lu-%lu.dmp",
				st.wYear,
				st.wMonth,
				st.wDay,
				st.wHour,
				st.wMinute,
				st.wSecond,
				st.wMilliseconds,
				::GetCurrentProcessId(),
				::GetCurrentThreadId()
			);
			// UNCパスで実行するような変な使い方は想定してませんからね
			::StringCchPrintfW(
				m_szDumpFileName,
				std::size(m_szDumpFileName),
				L"%s%s%s",
				(std::wcslen(m_szFileName) + std::wcslen(m_szDumpPath)) >= MAX_PATH && std::wcsncmp(m_szDumpPath, PATH_PREFIX, m_PrefixLen) != 0 ? PATH_PREFIX : L"",
				m_szDumpPath,
				m_szFileName
			);
			ScopedHandle handle{ ::CreateFile(m_szDumpFileName, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, nullptr) };
			if (handle.IsValid()) {
				MINIDUMP_EXCEPTION_INFORMATION info{};
				info.ThreadId = GetCurrentThreadId();
				info.ExceptionPointers = pEx;
				info.ClientPointers = FALSE;

				::MiniDumpWriteDump(
					::GetCurrentProcess(),
					::GetCurrentProcessId(),
					handle.Get(),
					MiniDumpNormal,
					&info,
					nullptr,
					nullptr
				);
			}
			return EXCEPTION_EXECUTE_HANDLER;
		}
	};
}
