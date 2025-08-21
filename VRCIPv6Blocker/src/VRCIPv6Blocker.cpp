// winapiはincludeの順序気にしないといけないの大杉なんだよ...
#include "ipv6conf.h"
#include "WinFirewall.h"
#include "taskman.h"
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
	WCHAR szVer[64];
	::swprintf_s(szVer, L"Ver. %u.%u.%u(%u)", v1, v2, v3, v4);
	::SetDlgItemTextW(m_hWnd, IDC_STATIC_VERSION, szVer);

	LoadBlockList();
	LoadSetting();

	CheckIPv6Setting();
	m_isFirewallBlocked = IsFirewallRegistered();
	SetSetting();
	::EnableWindow(::GetDlgItem(m_hWnd, IDC_BUTTON_DELTS), !ydk::IsExistSchedule(REGISTER_NAME));

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

	// 自動実行の場合は
	if(m_isAutoRun) AutoStart();
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

	case IDC_BUTTON_FIREWALL:
		ChangeFireWall();
		return TRUE;

	case IDC_BUTTON_IPV6:
		ChangeIPv6();
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
	if(m_isAutoRun) AutoExit();
    return ydk::DialogAppBase::OnClose(hDlg);
}

INT_PTR VRCIPv6BlockerApp::HandleMessage(HWND hDlg, UINT message,
    WPARAM wParam, LPARAM lParam) {
    switch (message) {
        // ウインドウメッセージの処理をこの辺に書く予定
	case WM_SHOWWINDOW:
		if (m_isAutoRun && wParam && m_Setting.uMinWindow == BST_CHECKED) {
			m_Logger->Log(L"最小化します");
			::SendMessage(m_hWnd, WM_SYSCOMMAND, SC_MINIMIZE, 0);
		}
		return TRUE;
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
		if (m_isAutoRun && m_Setting.uAutoShutdown == BST_CHECKED) {
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
	m_IniFile = m_ModulePath;
	m_IniFile += APP_NAME;
	m_IniFile += L".ini";
	static auto logger = ydk::FileLogger((m_ModulePath + logFileName).c_str());
    m_Logger = &logger;
	m_Logger->Log(L"アプリを起動します");

	m_lpArgList = CommandLineToArgvW(GetCommandLineW(), &m_argc);
	if (m_lpArgList == nullptr) {
		m_argc = 0;
		m_Logger->LogError(L"コマンドライン引数の読込に失敗しました");
	}
	else {
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
		if (*ps) {
			WCHAR szRule[256];
			ydk::ToUtf16(ps, szRule, std::size(szRule));
			m_BlockList.push_back(szRule);
		}
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

	// for (std::vector<std::wstring>::iterator it = m_BlockList.begin(); it != m_BlockList.end(); ++it) {
	// 	m_Logger->Log(it->c_str());
	// }
	std::fclose(pf);
}

// 設定情報の読込
void VRCIPv6BlockerApp::LoadSetting() {
	m_Setting.uRunVRC = ::GetPrivateProfileIntW(APP_NAME, IK_RUNVRC, BST_CHECKED, m_IniFile.c_str());
	m_Setting.uAutoShutdown = ::GetPrivateProfileIntW(APP_NAME, IK_AUTOSHUTDOWN, BST_CHECKED, m_IniFile.c_str());
	m_Setting.uMinWindow = ::GetPrivateProfileIntW(APP_NAME, IK_MINWINDOW, BST_UNCHECKED, m_IniFile.c_str());
	m_Setting.uFirewallBlock = ::GetPrivateProfileIntW(APP_NAME, IK_FIREWALLBLOCK, BST_CHECKED, m_IniFile.c_str());
	m_Setting.uNonBlocking = ::GetPrivateProfileIntW(APP_NAME, IK_NONBLOCKING, BST_UNCHECKED, m_IniFile.c_str());
	m_Setting.uRevert = ::GetPrivateProfileIntW(APP_NAME, IK_REVERT, BST_UNCHECKED, m_IniFile.c_str());
	WCHAR szBuf[MAX_PATH];
	::GetPrivateProfileStringW(APP_NAME, IK_EXECUTEPATH, L"", szBuf, std::size(szBuf), m_IniFile.c_str());
	m_Setting.strExecutePath = szBuf;
	::GetPrivateProfileStringW(APP_NAME, IK_VRCFILE, VRCFILENAME, szBuf, std::size(szBuf), m_IniFile.c_str());
	m_Setting.strVRCFile = szBuf;
	::GetPrivateProfileStringW(APP_NAME, IK_DESTIP, L"8.8.8.8", szBuf, std::size(szBuf), m_IniFile.c_str());
	m_Setting.strDestIp = szBuf;
	::GetPrivateProfileStringW(APP_NAME, IK_NIC, L"", szBuf, std::size(szBuf), m_IniFile.c_str());
	m_Setting.strNIC = szBuf;
	m_Logger->Log(L"設定を読込みました");
	DumpSetting();
}

// 設定情報の書込
void VRCIPv6BlockerApp::SaveSetting() {
	WCHAR szFormat[64];
	::StringCchPrintfW(szFormat, std::size(szFormat), L"%u", m_Setting.uRunVRC);
	::WritePrivateProfileStringW(APP_NAME, IK_RUNVRC, szFormat, m_IniFile.c_str());
	::StringCchPrintfW(szFormat, std::size(szFormat), L"%u", m_Setting.uAutoShutdown);
	::WritePrivateProfileStringW(APP_NAME, IK_AUTOSHUTDOWN, szFormat, m_IniFile.c_str());
	::StringCchPrintfW(szFormat, std::size(szFormat), L"%u", m_Setting.uMinWindow);
	::WritePrivateProfileStringW(APP_NAME, IK_MINWINDOW, szFormat, m_IniFile.c_str());
	::StringCchPrintfW(szFormat, std::size(szFormat), L"%u", m_Setting.uFirewallBlock);
	::WritePrivateProfileStringW(APP_NAME, IK_FIREWALLBLOCK, szFormat, m_IniFile.c_str());
	::StringCchPrintfW(szFormat, std::size(szFormat), L"%u", m_Setting.uNonBlocking);
	::WritePrivateProfileStringW(APP_NAME, IK_NONBLOCKING, szFormat, m_IniFile.c_str());
	::StringCchPrintfW(szFormat, std::size(szFormat), L"%u", m_Setting.uRevert);
	::WritePrivateProfileStringW(APP_NAME, IK_REVERT, szFormat, m_IniFile.c_str());

	::WritePrivateProfileStringW(APP_NAME, IK_EXECUTEPATH, m_Setting.strExecutePath.c_str(), m_IniFile.c_str());
	::WritePrivateProfileStringW(APP_NAME, IK_VRCFILE, m_Setting.strVRCFile.c_str(), m_IniFile.c_str());
	::WritePrivateProfileStringW(APP_NAME, IK_DESTIP, m_Setting.strDestIp.c_str(), m_IniFile.c_str());
	::WritePrivateProfileStringW(APP_NAME, IK_NIC, m_Setting.strNIC.c_str(), m_IniFile.c_str());
	m_Logger->Log(L"設定を書込みました");
	DumpSetting();
}

void VRCIPv6BlockerApp::GetSetting() {
	m_Setting.uRunVRC = ::IsDlgButtonChecked(m_hWnd, IDC_CHECK_RUNVRC);
	m_Setting.uAutoShutdown = ::IsDlgButtonChecked(m_hWnd, IDC_CHECK_AUTOEXIT);
	m_Setting.uMinWindow = ::IsDlgButtonChecked(m_hWnd, IDC_CHECK_MINWINDOW);
	m_Setting.uFirewallBlock = ::IsDlgButtonChecked(m_hWnd, IDC_CHECK_FIREWALL);
	WCHAR szBuf[MAX_PATH];
	::GetDlgItemTextW(m_hWnd, IDC_EDIT_LINK, szBuf, std::size(szBuf));
	m_Setting.strExecutePath = szBuf;
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
		L"DumpSetting : uRunVRC(%u), uAutoShutdown(%u), uMinWindow(%u), uFirewallBlock(%u), uNonBlocking(%u), uRevert(%u)",
		m_Setting.uRunVRC,
		m_Setting.uAutoShutdown,
		m_Setting.uMinWindow,
		m_Setting.uFirewallBlock,
		m_Setting.uNonBlocking,
		m_Setting.uRevert);
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

	::StringCchPrintfW(szLog,
		std::size(szLog),
		L"DumpSetting : strDestIp=%s",
		m_Setting.strDestIp.c_str());
	m_Logger->Log(szLog);

	::StringCchPrintfW(szLog,
		std::size(szLog),
		L"DumpSetting : strNIC=%s",
		m_Setting.strNIC.c_str());
	m_Logger->Log(szLog);
}

void VRCIPv6BlockerApp::CheckDialogControl() {
	if (m_isAutoRun && m_Setting.uRunVRC == BST_CHECKED && m_Setting.strExecutePath.length() > 0) {
		::EnableWindow(::GetDlgItem(m_hWnd, IDC_BUTTON_RUNVRC), FALSE);
	}
	else {
		::EnableWindow(::GetDlgItem(m_hWnd, IDC_BUTTON_RUNVRC), TRUE);
	}
	if (m_isFirewallBlocked) {
		::SetDlgItemText(m_hWnd, IDC_BUTTON_FIREWALL, L"FWブロック解除");
	}
	else {
		::SetDlgItemText(m_hWnd, IDC_BUTTON_FIREWALL, L"FWブロック登録");
	}
	if (m_isIPv6Enabled) {
		::SetDlgItemText(m_hWnd, IDC_BUTTON_IPV6, L"IPv6無効化");
	}
	else {
		::SetDlgItemText(m_hWnd, IDC_BUTTON_IPV6, L"IPv6有効化");
	}
	::EnableWindow(::GetDlgItem(m_hWnd, IDC_BUTTON_FIREWALL), !m_isAutoRun && m_Setting.uFirewallBlock == BST_CHECKED);
	::EnableWindow(::GetDlgItem(m_hWnd, IDC_BUTTON_IPV6), !m_isAutoRun && m_Setting.uFirewallBlock != BST_CHECKED);

	::EnableWindow(::GetDlgItem(m_hWnd, IDC_BUTTON_REF), !m_isAutoRun);
	::EnableWindow(::GetDlgItem(m_hWnd, IDC_BUTTON_DELTS), !m_isAutoRun);
	::EnableWindow(::GetDlgItem(m_hWnd, IDC_BUTTON_MAKELINK), !m_isAutoRun);
	::EnableWindow(::GetDlgItem(m_hWnd, IDC_BUTTON_SAVE), !m_isAutoRun);
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

bool VRCIPv6BlockerApp::IsFirewallRegistered() {
	HRESULT hr;
	if (ydk::ExistsFirewallRule(REGISTER_NAME, &hr)) {
		m_Logger->LogWarning(L"同一のルール名あり！登録する場合ルールは上書きされます");
		return true;
	}
	else if (FAILED(hr)) {
		m_Logger->LogError(L"ルールのチェックに失敗");
		WCHAR szErr[256];
		::swprintf_s(szErr, std::size(szErr), L"hr = %u", hr);
		m_Logger->LogError(szErr);
	}
	return false;
}

void VRCIPv6BlockerApp::SetFirewall() {
	if (m_BlockList.size() == 0) {
		m_Logger->LogError(L"有効なFirewallのルールが存在しないため設定は行いません");
		return;
	}

	m_isFirewallBlocked = IsFirewallRegistered();

	if(!ydk::RegisterFirewallRule(REGISTER_NAME, m_BlockList, nullptr, L"VRChat IPv6 Block Rule")){
		m_Logger->LogError(L"Firewallのルール登録に失敗");
	} else {
		m_Logger->Log(L"Firewallにルールを登録しました");
		m_isFirewallBlocked = true;
	}
	GetSetting();
	CheckDialogControl();
}

void VRCIPv6BlockerApp::RemoveFirewall() {
	HRESULT hr;
	if (ydk::RemoveFirewallRule(REGISTER_NAME, &hr)) {
		if (hr == S_OK) {
			m_Logger->Log(L"Firewallの対象ルールを削除しました");
			m_isFirewallBlocked = false;
		}
		else if (hr == S_FALSE) {
			m_Logger->LogWarning(L"Firewallの対象ルールがありません");
			m_isFirewallBlocked = false;
		}
		else {
			m_Logger->LogWarning(L"ここは来ないはずだが...");
		}
	}
	else {
		DWORD err = ::GetLastError();
		m_Logger->LogError(L"ルールの削除に失敗");
	}
	GetSetting();
	CheckDialogControl();
}

bool VRCIPv6BlockerApp::SetIPv6(bool isEnable) {
	HRESULT hr = ydk::SetIPv6Enable(isEnable, &m_adapterKey, m_Setting.strDestIp.c_str());
	if (hr == S_OK) {
		m_Logger->Log((std::wstring(L"IPv6を") + (isEnable ? L"有効化しました" : L"無効化しました")).c_str());
		m_isIPv6Enabled = isEnable;
	}
	else if (hr == S_FALSE) {
		m_Logger->LogWarning(L"対象のネットワークアダプタが見つかりませんでした");
	}
	else {
		m_Logger->LogError(L"ネットワークアダプタの設定に失敗しました");
	}
	GetSetting();
	CheckDialogControl();
	return hr == S_OK;
}

std::wstring VRCIPv6BlockerApp::SerializeGuid(const GUID& guid) {
	wchar_t buf[64] = {};
	int len = StringFromGUID2(guid, buf, _countof(buf));
	return (len > 0) ? std::wstring(buf) : L"";
}

bool VRCIPv6BlockerApp::DeserializeGuid(LPCWSTR lpStr, GUID& guid) {
	HRESULT hr = CLSIDFromString(lpStr, &guid);
	return SUCCEEDED(hr);
}

void VRCIPv6BlockerApp::WriteGuid(LPCWSTR lpGuid) {
	m_Setting.strNIC = lpGuid;
	m_Setting.uRevert = lpGuid == nullptr ? BST_UNCHECKED : BST_CHECKED;
	WCHAR szChk[32];
	::swprintf_s(szChk, L"%u", m_Setting.uRevert);
	::WritePrivateProfileStringW(APP_NAME, IK_REVERT, szChk, m_IniFile.c_str());
	::WritePrivateProfileStringW(APP_NAME, IK_NIC, lpGuid, m_IniFile.c_str());
}

void VRCIPv6BlockerApp::CheckIPv6Setting() {
	HRESULT hr = ydk::ResolveInternetAdapterFromString(m_Setting.strDestIp.c_str(), m_adapterKey);
	if (SUCCEEDED(hr)) {
		m_Logger->Log(L"ネットワークアダプタを特定しました");
		if (ydk::IsIPv6Enable(m_adapterKey, &hr)) {
			m_Setting.uRevert = BST_CHECKED;
			m_isIPv6Enabled = true;
		}
		else {
			if (FAILED(hr)) {
				m_Logger->LogError(L"IPv6の設定状況の取得に失敗しました");
				::MessageBoxW(m_hWnd, L"IPv6の設定状況の取得に失敗しました", L"エラー", MB_ICONERROR | MB_OK);
			} else {
				m_isIPv6Enabled = false;
			}
			m_Setting.uRevert = BST_UNCHECKED;
		}
		WriteGuid(SerializeGuid(m_adapterKey.ifGuid).c_str());
	}
	else {
		m_Logger->LogError(L"ネットワークアダプタの情報を取得できませんでした");
		::MessageBoxW(m_hWnd, L"ネットワークアダプタを取得できません", L"エラー", MB_ICONERROR | MB_OK);
	}
}

void VRCIPv6BlockerApp::ChangeFireWall() {
	if (m_isFirewallBlocked) {
		RemoveFirewall();
		if (m_isFirewallBlocked) {
			::MessageBoxW(m_hWnd, L"ルールは削除されませんでした", L"警告", MB_ICONWARNING | MB_OK);
		}
		else if(!m_isAutoRun) {
			::MessageBoxW(m_hWnd, L"ルールを削除しました", L"通知", MB_ICONINFORMATION | MB_OK);
		}
	}
	else {
		SetFirewall();
		if (m_isFirewallBlocked) {
			if(!m_isAutoRun) ::MessageBoxW(m_hWnd, L"ルールを登録しました", L"通知", MB_ICONINFORMATION | MB_OK);
		}
		else {
			::MessageBoxW(m_hWnd, L"ルールが登録できませんでした", L"エラー", MB_ICONERROR | MB_OK);
		}
	}
}

void VRCIPv6BlockerApp::ChangeIPv6() {
	if (SetIPv6(!m_isIPv6Enabled)) {
		if (m_isIPv6Enabled) {
			if (!m_isAutoRun) ::MessageBoxW(m_hWnd, L"IPv6を有効化しました", L"通知", MB_ICONINFORMATION | MB_OK);
		}
		else {
			if (!m_isAutoRun) ::MessageBoxW(m_hWnd, L"IPv6を無効化しました", L"通知", MB_ICONINFORMATION | MB_OK);
		}
	}
	else {
		::MessageBoxW(m_hWnd, L"IPv6の設定変更ができませんでした", L"エラー", MB_ICONERROR | MB_OK);
	}
}

void VRCIPv6BlockerApp::AutoStart() {
	if (m_Setting.uFirewallBlock == BST_CHECKED) {
		// ファイアウォールにブロックを追加する場合
		if (!m_isFirewallBlocked) {
			ChangeFireWall();
		}
		else {
			m_Logger->LogWarning(L"ファイアウォール登録済みのためスキップします");
		}
	}
	else {
		// IPv6無効化の場合
		if (m_isIPv6Enabled) {
			ChangeIPv6();
		}
		else {
			m_Logger->LogWarning(L"IPv6は無効のためスキップします");
		}
	}

	// まぁVRChatは起動しますけどね
	if (GetVRCProcessId()) {
		m_Logger->LogWarning(L"VRChatはすでに起動中です");
	}
	else {
		::Sleep(500); // なんか設定反映にラグがあったら嫌だから念のため500msほど待ってみる（不毛？）
		VRCExecuter();
	}
}

void VRCIPv6BlockerApp::AutoExit() {
	if (m_Setting.uFirewallBlock == BST_CHECKED) {
		// ファイアウォールのブロックを解除する場合
		if (m_isFirewallBlocked) {
			ChangeFireWall();
		}
		else {
			m_Logger->LogWarning(L"ファイアウォールに登録されていません");
		}
	}
	else {
		// IPv6有効化の場合（もともと無効だったら無効のまま）
		if (!m_isIPv6Enabled) {
			if(m_Setting.uRevert == BST_CHECKED) ChangeIPv6();
			else m_Logger->Log(L"IPv6はもともと無効かエラーだったためスキップします");
		}
		else {
			m_Logger->LogWarning(L"IPv6は有効のためスキップします");
		}
	}
}

void VRCIPv6BlockerApp::CreateScheduledTaskWithShortcut() {
	bool isExist = ydk::IsExistSchedule(REGISTER_NAME);
	if (isExist) {
		if (::MessageBoxW(
				m_hWnd,
				L"既に同名のタスクがあります。\n更新していいですか？",
				L"確認",
				MB_ICONQUESTION | MB_YESNO
			) != IDYES) {
			return;
		}
		m_Logger->LogWarning(L"現在のタスクスケジューラの設定を上書きします");
	}

	WCHAR szPath[MAX_PATH];
	::GetModuleFileNameW(m_hInstance, szPath, std::size(szPath));
	szPath[std::size(szPath) - 1] = L'\0'; // ねんのため

	HRESULT hr = ydk::RegisterTaskScheduler(REGISTER_NAME, szPath, ARG_AUTORUN, m_ModulePath.c_str());
	if (FAILED(hr)) {
		// ログ等
		m_Logger->LogError(L"タスクスケジューラの登録でエラーが発生しました");
	}
	else {
		::EnableWindow(::GetDlgItem(m_hWnd, IDC_BUTTON_DELTS), !ydk::IsExistSchedule(REGISTER_NAME));
	}

	// ショートカット作るぞ
}
