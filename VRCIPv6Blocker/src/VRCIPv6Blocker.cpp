#include "VRCIPv6Blocker.h"
#include "YDKWinUtils.h"
#include "SubClassEditHandler.h"
#include "ProcessWaiter.h"
#include "UserProcessLauncher.h"
#include <CommDlg.h>
#include <Shlwapi.h>
#include <strsafe.h>
#include <tlhelp32.h>
#include <process.h>

#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "Comdlg32.lib")

VRCIPv6BlockerApp::~VRCIPv6BlockerApp() {
    // デストラクタ（コンストラクタはprivate↓）
	::DeleteCriticalSection(&m_tidCs);
	::DeleteCriticalSection(&m_tCs);

	if (m_lpArgList != nullptr) {
		::LocalFree(m_lpArgList);
	}
	m_Logger->Log(L"アプリを終了します");
}

bool VRCIPv6BlockerApp::OnInitialize() {
	// アプリケーション初期化(コンストラクタとOnInitDialogの間の中途半端な立ち位置)

	return true;
}

void VRCIPv6BlockerApp::OnShutdown() {
    // 設定の保存など
}

INT_PTR VRCIPv6BlockerApp::OnInitDialog(HWND hDlg) {
    // 基底クラスの処理
    ydk::DialogAppBase::OnInitDialog(hDlg);
    // まぁこのあたりに初期化処理を書く予定
	m_pEditPathHandler = std::make_unique<SubclassEditHandler>(::GetDlgItem(m_hWnd, IDC_EDIT_LINK));
	m_pEditPath = std::make_unique<ydk::SubclassView>(m_pEditPathHandler.get());
	WORD v1, v2, v3, v4;
	ydk::GetAppVersion(&v1, &v2, &v3, &v4);
	WCHAR szVer[32];
	::swprintf_s(szVer, L"%u.%u.%u(%u)", v1, v2, v3, v4);
	::SetDlgItemTextW(m_hWnd, IDC_STATIC_VERSION, szVer);

	LoadBlockList();
	LoadSetting();
	SetSetting();

	SetVRCProcessId(GetVRChatProcess());

	if (GetVRCProcessId()) {
		m_Logger->LogWarning(L"VRChatが既に起動中");
		::SetDlgItemTextW(m_hWnd, IDC_STATIC_STATUS, L"VRChat起動中");
	}
	else {
		m_Logger->Log(L"VRChatは起動していません");
		::SetDlgItemTextW(m_hWnd, IDC_STATIC_STATUS, L"VRChatは起動していません");
	}

	if (m_Setting.uNonBlocking == BST_CHECKED) {
		WCHAR szCaption[64];
		::swprintf_s(szCaption, L"%s (NonBlock)", APP_NAME);
		::SetWindowTextW(m_hWnd, szCaption);
		m_Logger->LogWarning(L"IPv6をブロックしないアプリ名と相反する動作をします");
	}
	else {
		::SetWindowTextW(m_hWnd, APP_NAME);
	}

	m_hMonThread = reinterpret_cast<HANDLE>(::_beginthreadex(nullptr, 0, VRCMonitoringThread, this, 0, nullptr));
	if (m_hMonThread == nullptr) {
		auto err = ydk::GetErrorMessage(::GetLastError());
		m_Logger->LogError(L"監視用のワーカースレッドが起動できなかった...");
		m_Logger->LogError((L"GetLastError = " + err).c_str());
		::MessageBox(m_hWnd,
			L"VRChatの監視スレッドが起動できませんでした\nたぶん役に立たないので閉じてください",
			L"エラー",
			MB_ICONERROR | MB_OK);
	}
    return TRUE;
}

INT_PTR VRCIPv6BlockerApp::OnCommand(HWND hDlg, WPARAM wParam, LPARAM lParam) {
	switch (HIWORD(wParam)) {
	case BN_CLICKED:
		switch (LOWORD(wParam)) {
		case IDC_CHECK_RUNVRC:
			::EnableWindow(::GetDlgItem(m_hWnd,
				IDC_BUTTON_RUNVRC),
				::IsDlgButtonChecked(m_hWnd, IDC_CHECK_RUNVRC) != BST_CHECKED);
			return TRUE;
		case IDC_CHECK_FIREWALL:
			auto isFirewall = ::IsDlgButtonChecked(m_hWnd, IDC_CHECK_FIREWALL) == BST_CHECKED;
			::EnableWindow(::GetDlgItem(m_hWnd, IDC_BUTTON_FIREWALL), isFirewall);
			::EnableWindow(::GetDlgItem(m_hWnd, IDC_BUTTON_IPV6), !isFirewall);
			return TRUE;
		}
	}

    switch (LOWORD(wParam)) {
	case IDC_BUTTON_RUNVRC:
		::Sleep(100);
		if (GetVRCProcessId()) {
			m_Logger->LogWarning(L"すでに起動中ですが");
		}
		else {
			VRCExecuter();
		}
		return TRUE;

	case IDC_BUTTON_REF:
	{
		std::wstring extensions;
		extensions.reserve(128);
		extensions += L"実行可能ファイル(";
		for (int i = 0; i < std::size(ydk::WhiteListExt); ++i) {
			if (i) extensions += L";";
			extensions += L"*";
			extensions += ydk::WhiteListExt[i];
		}
		extensions += L")";
		extensions.append(1, L'\0');
		for (int i = 0; i < std::size(ydk::WhiteListExt); ++i) {
			if (i) extensions += L";";
			extensions += L"*";
			extensions += ydk::WhiteListExt[i];
		}
		extensions.append(2, L'\0');

		WCHAR szFileName[MAX_PATH] = L"\0";
		if (ydk::OpenFileName(m_hWnd, szFileName, std::size(szFileName), L"起動用ショートカットかプログラム",
			OFN_FILEMUSTEXIST | OFN_HIDEREADONLY,
			extensions.c_str()
		)) {
			::SetDlgItemTextW(m_hWnd, IDC_EDIT_LINK, szFileName);
		}
	}
	return TRUE;

	case IDC_BUTTON_SAVE:
		GetSetting();
		SaveSetting();
		::MessageBoxW(m_hWnd, L"設定を保存しました", L"通知", MB_ICONINFORMATION | MB_OK);
		return TRUE;
    }
	return ydk::DialogAppBase::OnCommand(hDlg, wParam, lParam);
}

INT_PTR VRCIPv6BlockerApp::OnClose(HWND hDlg) {
	if (GetStopFlag()) {
		SetStopFlag(true);
		// 一応スレッド終了待ち
		::Sleep(PROCESS_MONITOR_INTERVAL);
	}
    return ydk::DialogAppBase::OnClose(hDlg);
}

INT_PTR VRCIPv6BlockerApp::HandleMessage(HWND hDlg, UINT message,
    WPARAM wParam, LPARAM lParam) {
    switch (message) {
        // ウインドウメッセージの処理をこの辺に書く予定
	case WM_VRCEXIT:
		if (wParam) {
			m_Logger->LogError(L"VRChatが変な終わり方しました");
		}
		else {
			WCHAR szLog[256];
			::swprintf_s(szLog, L"VRChatが終了しました(終了コード:%lu)", static_cast<DWORD>(lParam));
			m_Logger->Log(szLog);
		}
		if (m_hWaitThread != nullptr) {
			::CloseHandle(m_hWaitThread);
			m_hWaitThread = nullptr;
		}
		if (m_Setting.uAutoShutdown == BST_CHECKED) {
			m_Logger->Log(L"自動終了によりアプリの終了を開始します");
			::SendMessage(m_hWnd, WM_CLOSE, 0, 0);
		}
    return TRUE;
    }

    return ydk::DialogAppBase::HandleMessage(hDlg, message, wParam, lParam);
}

// private

// ぶいちゃの起動状態を監視するワーカースレッド
unsigned __stdcall VRCIPv6BlockerApp::VRCMonitoringThread(void* param) {
	constexpr int SLEEP_CYCLES = 10;
	ydk::ComInitializer comInitializer; // com使ってないと思うけど一応保険として入れておこう
	auto app = reinterpret_cast<VRCIPv6BlockerApp*>(param);
	app->SetStopFlag(false);
	bool isRunning = false;
	while (!app->GetStopFlag()) {
		if (app->GetVRCProcessId()) {
			if (!isRunning) {
				isRunning = true;
				::SetDlgItemText(app->m_hWnd, IDC_STATIC_STATUS, L"VRChat起動中");
			}
		}
		else {
			if(isRunning) {
				isRunning = false;
				::SetDlgItemText(app->m_hWnd, IDC_STATIC_STATUS, L"VRChatは起動していません");
			}
		}

		app->SetVRCProcessId(app->GetVRChatProcess());
		if (app->m_hWaitThread == nullptr && app->GetVRCProcessId()) {
			app->m_hWaitThread = reinterpret_cast<HANDLE>(::_beginthreadex(nullptr, 0, ProcessExitNotifyThread, app, 0, nullptr));
			if (app->m_hWaitThread == nullptr) {
				auto err = ydk::GetErrorMessage(::GetLastError());
				app->m_Logger->LogError(L"待機用のワーカースレッドが起動できなかった...");
				app->m_Logger->LogError((L"GetLastError = " + err).c_str());
				::MessageBox(app->m_hWnd,
					L"VRChatの終了待機用スレッドが起動できませんでした\n自動では終了しないためアプリを閉じる場合は×で閉じてください",
					L"エラー",
					MB_ICONERROR | MB_OK);
			}
		}
		for (int i = 0; i < SLEEP_CYCLES; ++i) { // プロセス監視するのは1秒おきくらいでいいと思ってる
			if (app->GetStopFlag()) break;
			::Sleep(app->PROCESS_MONITOR_INTERVAL);
		}
	}
	::_endthreadex(0);
	return 0;
}

// ぶいちゃの終了を待つワーカースレッド
unsigned __stdcall VRCIPv6BlockerApp::ProcessExitNotifyThread(void* param) {
	ydk::ComInitializer comInitializer; // com使ってないと思うけど一応保険として入れておこう
	auto app = reinterpret_cast<VRCIPv6BlockerApp*>(param);

	ydk::ProcessWaiter pw(app->GetVRCProcessId());

	auto result = pw.Wait(); // 起動してなかったらすぐ制御返すはず
	if (!pw.IsValid()) {
		app->m_Logger->LogError(L"VRChatのプロセスハンドル開けてないんだが？");
	}
	WCHAR szResult[128];
	::swprintf_s(szResult, L"wait result = 0x%08X(%lu)", result, result);
	app->m_Logger->Log(szResult);
	app->SetVRCProcessId(0);
	DWORD dwExitCode;
	auto isSuccess = pw.ExitCode(dwExitCode);

	// メッセージ送るところ
	constexpr int MAX_RETRY = 10;
	constexpr DWORD MAX_SLEEP = 1000;
	int retry = 0;
	DWORD dwSleepMs = 50;
	while (!::PostMessageW(
			app->m_hWnd, WM_VRCEXIT,
			static_cast<WPARAM>(!isSuccess), // WPARAMはExitCode成功時に0を設定させたい
			static_cast<LPARAM>(dwExitCode))) // WPARAMが0でなければこの値は未定義、つまり何なのか知らん
	{
		DWORD dwError = ::GetLastError();
		if (dwError == ERROR_INVALID_WINDOW_HANDLE || dwError == ERROR_ACCESS_DENIED) {
			app->m_Logger->LogError(ydk::GetErrorMessage(dwError).c_str());
		}
		if (++retry > MAX_RETRY) break;
		::Sleep(dwSleepMs);
		dwSleepMs *= 2;
		if (dwSleepMs > MAX_SLEEP) dwSleepMs = MAX_SLEEP;
	}
	::_endthreadex(0);
	return 0;
}

VRCIPv6BlockerApp* VRCIPv6BlockerApp::Instance() {
    static VRCIPv6BlockerApp app = VRCIPv6BlockerApp();
    return &app;
}

VRCIPv6BlockerApp::VRCIPv6BlockerApp()
    : ydk::DialogAppBase() {
    // コンストラクタ
    m_ModulePath = ydk::GetModuleDir();
	static auto logger = ydk::FileLogger((m_ModulePath + logFileName).c_str());
    m_Logger = &logger;
	m_Logger->Log(L"アプリを起動します");

	m_lpArgList = CommandLineToArgvW(GetCommandLineW(), &m_argc);
	if (m_lpArgList == nullptr) {
		m_argc = 0;
		m_Logger->LogError(L"コマンドライン引数の読込に失敗しました");
	}
	else {
		constexpr LPCWSTR ARG_AUTORUN = L"-autorun";
		for (int i = 0; i < m_argc; ++i) {
			m_isAutoRun = std::wcscmp(m_lpArgList[i], ARG_AUTORUN) == 0;
		}
	}
	if (m_isAutoRun) m_Logger->Log(L"VRChatを自動実行 ... できたらします");

	::InitializeCriticalSection(&m_tCs);
	::InitializeCriticalSection(&m_tidCs);
}

// ブロックリストの読込
void VRCIPv6BlockerApp::LoadBlockList() {
	m_Logger->Log(L"ブロックリストの読込...");
	m_BlockList.clear();
	std::wstring blocklistPath(m_ModulePath);
	blocklistPath += VRCIPv6BlockerApp::BLOCK_LIST_FILE;
	auto pf = ydk::OpenReadFile(blocklistPath.c_str());
	if (pf == nullptr) {
		m_Logger->LogError((std::wstring(blocklistPath) + L"を開けません").c_str());
		::MessageBoxW(m_hWnd, L"ブロックリストが開けませんでした", L"エラー", MB_ICONERROR | MB_OK);
		return;
	}

	char buf[256];
	bool isSkip, isRead;
	char* ps = buf;
	while (std::fgets(buf, sizeof(buf), pf) != nullptr) {
		isSkip = isRead = false;
		for (char* p = buf; *p; ++p) {
			if (*p == ' ' || *p == '\t') continue;
			if (*p == '#') {
				if (isRead) {
					*p = '\0';
					break;
				}
				isSkip = true;
				break;
			}
			if (!isRead &&
				(std::isxdigit(*p) ||
				*p == ':' ||
				*p == '/' ||
				*p == ',' ||
				*p == '-' ||
				*p == '.')) {
				isRead = true;
				ps = p;
				continue;
			}
			if (*p == '\r' || *p == '\n') {
				*p = '\0';
				break;
			}
		}
		if (isSkip) continue;
		char* pt;
		for (pt = ps + std::strlen(ps) - 1; pt > ps; --pt) {
			if (*pt == ' ' || *pt == '\t') continue;
			break;
		}
		*(pt + 1) = '\0';
		if(*ps) m_BlockList.push_back(ps);
	}
	if (std::ferror(pf)) {
		m_Logger->LogError(L"ブロックリストの読込中にエラーが発生しました");
		::MessageBoxW(m_hWnd, L"ブロックリストの読込中にエラーが発生しました", L"エラー", MB_ICONERROR | MB_OK);
	}
	else {
		WCHAR szLog[256];
		::StringCchPrintfW(szLog, std::size(szLog), L"ブロックリスト有効件数 : %llu", m_BlockList.size());
		m_Logger->Log(szLog);
	}

	//for (std::vector<std::string>::iterator it = m_BlockList.begin(); it != m_BlockList.end(); ++it) {
	//	::MessageBoxA(m_hWnd, it->c_str(), "vector", MB_OK);
	//}

	std::fclose(pf);
}

// 設定情報の読込
void VRCIPv6BlockerApp::LoadSetting() {
	std::wstring iniPath(m_ModulePath);
	iniPath += APP_NAME;
	iniPath += L".ini";
	m_Setting.uRunVRC = ::GetPrivateProfileIntW(APP_NAME, IK_RUNVRC, BST_CHECKED, iniPath.c_str());
	m_Setting.uAutoShutdown = ::GetPrivateProfileIntW(APP_NAME, IK_AUTOSHUTDOWN, BST_CHECKED, iniPath.c_str());
	m_Setting.uMinWindow = ::GetPrivateProfileIntW(APP_NAME, IK_MINWINDOW, BST_UNCHECKED, iniPath.c_str());
	m_Setting.uFirewallBlock = ::GetPrivateProfileIntW(APP_NAME, IK_FIREWALLBLOCK, BST_CHECKED, iniPath.c_str());
	m_Setting.uNonBlocking = ::GetPrivateProfileIntW(APP_NAME, IK_NONBLOCKING, BST_UNCHECKED, iniPath.c_str());
	WCHAR szPath[MAX_PATH];
	::GetPrivateProfileStringW(APP_NAME, IK_EXECUTEPATH, L"", szPath, std::size(szPath), iniPath.c_str());
	m_Setting.strExecutePath = szPath;
	::GetPrivateProfileStringW(APP_NAME, IK_VRCFILE, VRCFILENAME, szPath, std::size(szPath), iniPath.c_str());
	m_Setting.strVRCFile = szPath;
	m_Logger->Log(L"設定を読込みました");
	DumpSetting();
}

// 設定情報の書込
void VRCIPv6BlockerApp::SaveSetting() {
	std::wstring iniPath(m_ModulePath);
	iniPath += APP_NAME;
	iniPath += L".ini";
	WCHAR szFormat[64];
	::StringCchPrintfW(szFormat, std::size(szFormat), L"%u", m_Setting.uRunVRC);
	::WritePrivateProfileStringW(APP_NAME, IK_RUNVRC, szFormat, iniPath.c_str());
	::StringCchPrintfW(szFormat, std::size(szFormat), L"%u", m_Setting.uAutoShutdown);
	::WritePrivateProfileStringW(APP_NAME, IK_AUTOSHUTDOWN, szFormat, iniPath.c_str());
	::StringCchPrintfW(szFormat, std::size(szFormat), L"%u", m_Setting.uMinWindow);
	::WritePrivateProfileStringW(APP_NAME, IK_MINWINDOW, szFormat, iniPath.c_str());
	::StringCchPrintfW(szFormat, std::size(szFormat), L"%u", m_Setting.uFirewallBlock);
	::WritePrivateProfileStringW(APP_NAME, IK_FIREWALLBLOCK, szFormat, iniPath.c_str());
	::StringCchPrintfW(szFormat, std::size(szFormat), L"%u", m_Setting.uNonBlocking);
	::WritePrivateProfileStringW(APP_NAME, IK_NONBLOCKING, szFormat, iniPath.c_str());

	::WritePrivateProfileStringW(APP_NAME, IK_EXECUTEPATH, m_Setting.strExecutePath.c_str(), iniPath.c_str());
	::WritePrivateProfileStringW(APP_NAME, IK_VRCFILE, m_Setting.strVRCFile.c_str(), iniPath.c_str());
	m_Logger->Log(L"設定を書込みました");
	DumpSetting();
}

void VRCIPv6BlockerApp::GetSetting() {
	m_Setting.uRunVRC = ::IsDlgButtonChecked(m_hWnd, IDC_CHECK_RUNVRC);
	m_Setting.uAutoShutdown = ::IsDlgButtonChecked(m_hWnd, IDC_CHECK_AUTOEXIT);
	m_Setting.uMinWindow = ::IsDlgButtonChecked(m_hWnd, IDC_CHECK_MINWINDOW);
	m_Setting.uFirewallBlock = ::IsDlgButtonChecked(m_hWnd, IDC_CHECK_FIREWALL);
	WCHAR szPath[MAX_PATH];
	::GetDlgItemTextW(m_hWnd, IDC_EDIT_LINK, szPath, std::size(szPath));
	m_Setting.strExecutePath = szPath;
}

void VRCIPv6BlockerApp::SetSetting() {
	::CheckDlgButton(m_hWnd, IDC_CHECK_RUNVRC, m_Setting.uRunVRC);
	::CheckDlgButton(m_hWnd, IDC_CHECK_AUTOEXIT, m_Setting.uAutoShutdown);
	::CheckDlgButton(m_hWnd, IDC_CHECK_MINWINDOW, m_Setting.uMinWindow);
	::CheckDlgButton(m_hWnd, IDC_CHECK_FIREWALL, m_Setting.uFirewallBlock);
	::SetDlgItemTextW(m_hWnd, IDC_EDIT_LINK, m_Setting.strExecutePath.c_str());

	CheckDialogControl();
}

void VRCIPv6BlockerApp::DumpSetting() {
	WCHAR szLog[384];
	::StringCchPrintfW(szLog,
		std::size(szLog),
		L"DumpSetting : uRunVRC(%u), uAutoShutdown(%u), uMinWindow(%u), uFirewallBlock(%u), uNonBlocking(%u)",
		m_Setting.uRunVRC,
		m_Setting.uAutoShutdown,
		m_Setting.uMinWindow,
		m_Setting.uFirewallBlock,
		m_Setting.uNonBlocking,
		m_Setting.strExecutePath.c_str());
	m_Logger->Log(szLog);

	::StringCchPrintfW(szLog,
		std::size(szLog),
		L"DumpSetting : strExecutePath=%s",
		m_Setting.strExecutePath.c_str());
	m_Logger->Log(szLog);

	::StringCchPrintfW(szLog,
		std::size(szLog),
		L"DumpSetting : strVRCFile=%s",
		m_Setting.strVRCFile.c_str());
	m_Logger->Log(szLog);
}

void VRCIPv6BlockerApp::CheckDialogControl() {
	if (m_Setting.uRunVRC == BST_CHECKED && m_Setting.strExecutePath.length() > 0) {
		::EnableWindow(::GetDlgItem(m_hWnd, IDC_BUTTON_RUNVRC), FALSE);
	}
	else {
		::EnableWindow(::GetDlgItem(m_hWnd, IDC_BUTTON_RUNVRC), TRUE);
	}
	::EnableWindow(::GetDlgItem(m_hWnd, IDC_BUTTON_FIREWALL), m_Setting.uFirewallBlock == BST_CHECKED);
	::EnableWindow(::GetDlgItem(m_hWnd, IDC_BUTTON_IPV6), m_Setting.uFirewallBlock != BST_CHECKED);
}

DWORD VRCIPv6BlockerApp::GetVRChatProcess() {
	HANDLE hSnapshot = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (hSnapshot == INVALID_HANDLE_VALUE) {
		return 0;
	}

	PROCESSENTRY32W pe32;
	pe32.dwSize = sizeof(PROCESSENTRY32W);

	DWORD processId = 0;
	if (Process32FirstW(hSnapshot, &pe32)) {
		do {
			if (_wcsicmp(pe32.szExeFile,
					m_Setting.strVRCFile.length() > 0 ?
					m_Setting.strVRCFile.c_str() :
					VRCFILENAME) == 0) {
				processId = pe32.th32ProcessID;
				break;
			}
		} while (Process32NextW(hSnapshot, &pe32));
	}

	CloseHandle(hSnapshot);
	return processId;
}

void VRCIPv6BlockerApp::VRCExecuter() {
	if (GetVRCProcessId()) {
		m_Logger->LogError(L"既に起動してるので起動しないでほしい");
		return;
	}
	auto pid = ydk::ShellExecuteWithLoginUser(m_Setting.strExecutePath.c_str());
	if (!pid) {
		m_Logger->LogError(L"起動できませんでした");
		::MessageBoxW(m_hWnd, L"起動できませんでした", L"エラー", MB_ICONERROR | MB_OK);
	}
	else {
		WCHAR szMsg[256];
		::swprintf_s(szMsg, L"プロセスID(%lu)で起動しました(ランチャーの可能性もあるからこのPIDは信用できん)", pid);
		m_Logger->Log(szMsg);
		::SetDlgItemTextW(m_hWnd, IDC_STATIC_STATUS, L"VRChatの起動待ち...");
	}
}
void VRCIPv6BlockerApp::SetStopFlag(bool isStop)
{
	::EnterCriticalSection(&m_tidCs);
	m_isStop = isStop;
	::LeaveCriticalSection(&m_tidCs);
}

bool VRCIPv6BlockerApp::GetStopFlag()
{
	::EnterCriticalSection(&m_tidCs);
	bool isStop = m_isStop;
	::LeaveCriticalSection(&m_tidCs);
	return isStop;
}

void VRCIPv6BlockerApp::SetVRCProcessId(DWORD dwProcessId) {
	::EnterCriticalSection(&m_tCs);
	m_vrcProcessId = dwProcessId;
	::LeaveCriticalSection(&m_tCs);
}

DWORD VRCIPv6BlockerApp::GetVRCProcessId() {
	::EnterCriticalSection(&m_tCs);
	DWORD result = m_vrcProcessId;
	::LeaveCriticalSection(&m_tCs);
	return result;
}
