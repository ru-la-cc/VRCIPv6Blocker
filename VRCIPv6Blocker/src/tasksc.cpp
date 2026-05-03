#include "tasksc.h"
#include <strsafe.h>
#include <string>
#include "YDKWinUtils.h"
#include "win32except.h"
#include "../resource.h"

bool IPv6BlockScheduler::CreateSchedule(LPCWSTR lpExePath, LPCWSTR lpArgs, LPCWSTR lpWorkDir) noexcept {
	HRESULT hr = ydk::RegisterTaskScheduler(REGISTER_NAME, lpExePath, lpArgs, lpWorkDir);
	if (FAILED(hr)) {
		WCHAR szBuf[128];
		::StringCchPrintfW(szBuf, std::size(szBuf), L"タスクスケジューラの登録でエラーが発生しました HRESULT=%lu(0x%08x)", hr, hr);
		m_Logger->LogError(szBuf);
		return false;
	}
	m_Logger->Log((std::wstring(L"タスクスケジューラに登録しました : ") + REGISTER_NAME).c_str());
	return true;
}

bool IPv6BlockScheduler::DeleteSchedule() noexcept {
	HRESULT hr = ydk::RemoveTaskScheduler(REGISTER_NAME);
	if (FAILED(hr)) {
		WCHAR szBuf[128];
		::StringCchPrintfW(szBuf, std::size(szBuf), L"タスクスケジューラの削除に失敗しました HRESULT=%lu(0x%08x)", hr, hr);
		m_Logger->LogError(szBuf);
		return false;
	}
	m_Logger->Log((std::wstring(L"タスクスケジューラから削除しました : ") + REGISTER_NAME).c_str());
	return true;
}

