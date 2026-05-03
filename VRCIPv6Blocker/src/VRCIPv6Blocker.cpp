#include "VRCIPv6Blocker.h"
#include "YDKWinUtils.h"
#include "SubClassEditHandler.h"
#include "ProcessWaiter.h"
#include "UserProcessLauncher.h"
#include <CommDlg.h>
#include <Shlwapi.h>
#include <strsafe.h>
#include <pathcch.h>
#include <cerrno>
#include "win32except.h"

#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "Comdlg32.lib")
#pragma comment(lib, "pathcch.lib")

std::exception_ptr VRCIPv6BlockerApp::m_Exception = nullptr;

VRCIPv6BlockerApp::~VRCIPv6BlockerApp() {
    // コンストラクタはprivateだから下の方に書いてる
	::DeleteCriticalSection(&m_wCS);

	if (m_lpArgList != nullptr) {
		::LocalFree(m_lpArgList);
	}
	m_Logger->Log(L"アプリを終了します");
}

bool VRCIPv6BlockerApp::OnInitialize() {
	// アプリケーション初期化(コンストラクタとOnInitDialogの間の中途半端な位置)
	return true;
}

void VRCIPv6BlockerApp::OnShutdown() {
    // 設定の保存など
}

INT_PTR VRCIPv6BlockerApp::OnInitDialog(HWND hDlg) {
    // 基底クラスの処理
    ydk::DialogAppBase::OnInitDialog(hDlg);
	try {
		// まぁこのあたりに初期化処理を書く予定
		m_pEditPathHandler = std::make_unique<SubclassEditHandler>(::GetDlgItem(m_hWnd, IDC_EDIT_LINK));
		m_pEditPath = std::make_unique<ydk::SubclassView>(m_pEditPathHandler.get());
		m_TaskScheduler = std::make_unique<IPv6BlockScheduler>(m_Logger);
		WORD v1, v2, v3, v4;
		ydk::GetAppVersion(&v1, &v2, &v3, &v4);
		m_Version = (static_cast<unsigned __int64>(v1) << 48) |
			(static_cast<unsigned __int64>(v2) << 32) |
			(static_cast<unsigned __int64>(v3) << 16) |
			(static_cast<unsigned __int64>(v4) << 0);
		WCHAR szVer[64];
		::swprintf_s(szVer, L"Ver. %u.%u.%u(%u)", v1, v2, v3, v4);
		::SetDlgItemTextW(m_hWnd, IDC_STATIC_VERSION, szVer);

		if (!m_BlockList.LoadFromFile((m_ModulePath + BLOCK_LIST_FILE).c_str(), m_Logger)) {
			::MessageBoxW(m_hWnd, L"ブロックリストの読込に失敗しました\n詳細はログを確認してください", L"エラー", MB_ICONERROR | MB_OK);
		}

		try {
			m_Config.Load();
		}
		catch (const ydk::Win32Exception& ex) {
			m_Logger->LogError(L"設定の読込に失敗しました");
			m_Logger->LogError(ex.what_w());
			::MessageBoxW(m_hWnd, L"設定の読込に失敗しました\n詳細はログを確認してください", L"エラー", MB_ICONERROR | MB_OK);
			throw;
		}
		m_pRule = std::make_unique<RuleController>(
			(m_ModulePath + INPRG_FILE).c_str(),
			m_Logger, m_Config.GetConfig(),
			m_BlockList.GetBlockList()
		);
		ApplyConfigToDialog();
		::SetDlgItemTextW(m_hWnd, IDC_STATIC_STATUS, L"VRChatのプロセスを確認しています...");

		if (m_Config.GetConfig().uNonBlocking == BST_CHECKED) {
			WCHAR szCaption[64];
			::swprintf_s(szCaption, L"%s (NonBlock)", APP_NAME);
			::SetWindowTextW(m_hWnd, szCaption);
			m_Logger->LogWarning(L"IPv6をブロックしないアプリ名と相反する動作をします");
		}
		else {
			::SetWindowTextW(m_hWnd, APP_NAME);
		}

		m_VRCProcess = std::make_unique<VRCProcess>(m_Config.GetConfig().strVRCFile.c_str(), m_Logger);
		static VRCProcess::MonitorParams params{
			m_VRCProcess.get(),
			&m_Waiter,
			&m_bStopFlag,
			[hWnd = m_hWnd, pLogger = m_Logger](LPCWSTR lpText) {
				if (!::PostMessageW(hWnd, WM_SET_CTRLTEXT, IDC_STATIC_STATUS, reinterpret_cast<LPARAM>(lpText))) {
					pLogger->LogError(L"メッセージのポストに失敗 : WM_SET_CTRLTEXT");
				}
			},
			[hWnd = m_hWnd, pLogger = m_Logger](WPARAM wParam, LPARAM lParam) {
				if (!::PostMessageW(hWnd, WM_VRCEXIT, wParam, lParam)) {
					pLogger->LogError(L"メッセージのポストに失敗 : WM_VRCEXIT");
				}
			},
			m_wCS
		};
		m_Worker.emplace(VRCProcess::VRCMonitorThread, &params);
		if (m_pRule->IsRestore()) {
			if (m_pRule->IsComplete()) {
				::MessageBox(m_hWnd,
					L"アプリの異常終了を検出したため、設定を復元しました",
					L"報告",
					MB_ICONWARNING | MB_OK);
			}
			else {
				throw ydk::YDKException(
					(L"アプリの異常終了を検出したため、復元を試みましたが復元に失敗しました。\n"
						L"ファイアウォールやIPv6の設定を確認し、以下のファイルを削除してから再度起動してください\n"
						+ (m_ModulePath + INPRG_FILE)).c_str()
				);
			}
		}

		// 自動実行の場合は
		if (m_isAutoRun) AutoStart();
	}
	catch (...) {
		m_Exception = std::current_exception();
		::PostMessage(m_hWnd, WM_CLOSE, 0, 0L);
	}
    return TRUE;
}

INT_PTR VRCIPv6BlockerApp::OnCommand(HWND hDlg, WPARAM wParam, LPARAM lParam) {
	ApplyDialogToConfig();

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
		if (m_VRCProcess->GetProcessID()) {
			m_Logger->LogWarning(L"すでに起動中ですが");
		}
		else {
			::EnableWindow(::GetDlgItem(m_hWnd, IDC_BUTTON_SAVE), FALSE);
			VRCExecuter();
		}
		return TRUE;

	case IDC_BUTTON_FIREWALL:
		if (m_pRule->IsExistFirewallRule()) {
			if (m_pRule->DeleteFirewallRule()) {
				::MessageBox(m_hWnd, L"ファイアウォールのルールを削除しました", L"通知", MB_ICONINFORMATION | MB_OK);
			} else {
				::MessageBox(m_hWnd, L"ファイアウォールのルールを削除できません", L"エラー", MB_ICONERROR | MB_OK);
			}
		} else {
			if (m_pRule->ApplyFirewallRules()) {
				::MessageBox(m_hWnd, L"ファイアウォールのルールを登録しました", L"通知", MB_ICONINFORMATION | MB_OK);
			} else {
				::MessageBox(m_hWnd, L"ファイアウォールのルールを登録できません", L"エラー", MB_ICONERROR | MB_OK);
			}
		}
		CheckDialogControl();
		return TRUE;

	case IDC_BUTTON_IPV6:
		if (m_pRule->IsEnableIPv6()) {
			if (m_pRule->EnableIPv6(false)) {
				::MessageBox(m_hWnd, L"IPv6を無効化しました", L"通知", MB_ICONINFORMATION | MB_OK);
			} else {
				::MessageBox(m_hWnd, L"IPv6を無効化できませんでした", L"エラー", MB_ICONERROR | MB_OK);
			}
		} else {
			if (m_pRule->EnableIPv6(true)) {
				::MessageBox(m_hWnd, L"IPv6を有効化しました", L"通知", MB_ICONINFORMATION | MB_OK);
			} else {
				::MessageBox(m_hWnd, L"IPv6を有効化できませんでした", L"エラー", MB_ICONERROR | MB_OK);
			}
		}
		CheckDialogControl();
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

	case IDC_BUTTON_MAKELINK:
		OnClickMakeLinkButton();
		return TRUE;

	case IDC_BUTTON_DELTS:
		OnClickDeleteTask();
		return TRUE;

	case IDC_BUTTON_SAVE:
		ApplyDialogToConfig();
		try {
			auto& config = m_Config.GetConfig();
			if (m_Version > config.ullVersion) config.ullVersion = m_Version;
			m_Config.Save();
			::MessageBoxW(m_hWnd, L"設定を保存しました", L"通知", MB_ICONINFORMATION | MB_OK);
		}
		catch (const ydk::Win32Exception& ex) {
			m_Logger->LogError((std::wstring(L"設定の保存に失敗しました : ") + ex.what_w()).c_str());
			::MessageBoxW(m_hWnd, L"設定の保存でエラーが発生しました", L"ばかな...", MB_ICONERROR | MB_OK);
		}
		return TRUE;
    }
	return ydk::DialogAppBase::OnCommand(hDlg, wParam, lParam);
}

INT_PTR VRCIPv6BlockerApp::OnClose(HWND hDlg) {
	if (m_Exception) {
		WaitWorker();
		return ydk::DialogAppBase::OnClose(hDlg);
	}
	if (m_isAutoRun){
		CSLock lock(m_wCS);
		if (m_Waiter.has_value() &&
			::WaitForSingleObject(m_Waiter.value().native_handle(), 0) == WAIT_TIMEOUT) {
			// VRChat起動中なのに閉じようとしたら一応警告を出す
			if (::MessageBoxW(m_hWnd,
				L"VRChatが起動中です！\n"
				L"このアプリを終了するとIPv6の設定が元に戻り、一時的に通信が切れる場合があります\n"
				L"それでもいいですか？",
				L"警告",
				MB_ICONWARNING | MB_YESNO | MB_DEFBUTTON2) != IDYES) return TRUE;
		}
	} else if (m_pRule->IsChange()) {
		if (::MessageBoxW(m_hWnd,
			L"\n"
			L"ファイアウォールまたはIPv6の設定が変更されています\n"
			L"そのまま終了してもいいですか？",
			L"確認！！",
			MB_ICONWARNING | MB_YESNO | MB_DEFBUTTON2) != IDYES) return TRUE;
	}
	WaitWorker();
	if (m_isAutoRun) AutoExit();
	else m_pRule->Cleanup();
    return ydk::DialogAppBase::OnClose(hDlg);
}

INT_PTR VRCIPv6BlockerApp::HandleMessage(HWND hDlg, UINT message,
    WPARAM wParam, LPARAM lParam) {
    switch (message) {
        // ウインドウメッセージの処理をこの辺に書く予定
	case WM_SHOWWINDOW:
		if (m_isAutoRun && wParam && m_Config.GetConfig().uMinWindow == BST_CHECKED) {
			m_Logger->Log(L"最小化します");
			::SendMessage(m_hWnd, WM_SYSCOMMAND, SC_MINIMIZE, 0);
		}
		return TRUE;
	case WM_VRCEXIT:
		if (wParam) {
			m_Logger->LogError(L"VRChatが変な終わり方しました？");
		} else {
			WCHAR szLog[256];
			::swprintf_s(szLog, L"VRChatが終了しました(終了コード:%lu)", static_cast<DWORD>(lParam));
			m_Logger->Log(szLog);
		}
		if (!m_isAutoRun) {
			::PostMessageW(m_hWnd, WM_ENABLE_CONTROL, static_cast<WPARAM>(TRUE), static_cast<LPARAM>(IDC_BUTTON_SAVE));
		} else if (m_Config.GetConfig().uAutoShutdown == BST_CHECKED) {
			m_Logger->Log(L"自動終了によりアプリの終了を開始します");
			::SendMessage(m_hWnd, WM_CLOSE, 0, 0);
		}
		return TRUE;
	case WM_SET_CTRLTEXT:
		::SetDlgItemTextW(m_hWnd, static_cast<int>(wParam), reinterpret_cast<LPCWSTR>(lParam));
		return TRUE;

	case WM_ERR_MESSAGE:
		::MessageBox(m_hWnd, reinterpret_cast<LPCWSTR>(lParam), L"エラー", MB_ICONERROR | MB_OK);
		return TRUE;

	case WM_WRITE_VRCFULLPATH:
		WriteExePath();
		return TRUE;
	case WM_ENABLE_CONTROL:
		::EnableWindow(::GetDlgItem(m_hWnd, static_cast<int>(lParam)), static_cast<BOOL>(wParam));
		return TRUE;
	}

    return ydk::DialogAppBase::HandleMessage(hDlg, message, wParam, lParam);
}

// private

VRCIPv6BlockerApp* VRCIPv6BlockerApp::Instance() {
    static VRCIPv6BlockerApp app = VRCIPv6BlockerApp();
    return &app;
}

VRCIPv6BlockerApp::VRCIPv6BlockerApp()
    : ydk::DialogAppBase() {
    // コンストラクタ
	m_Version = 0;
    m_ModulePath = ydk::GetModuleDir();
	m_Config.SetFilePath((ydk::GetModuleDir() + APP_NAME + L".ini").c_str());
	m_IniFile = m_ModulePath;
	m_IniFile += APP_NAME;
	m_IniFile += L".ini";
	static auto logger = ydk::FileLogger((m_ModulePath + LOGFILENAME).c_str());
    m_Logger = &logger;
	m_Config.SetLogger(m_Logger);
	m_Logger->Log(L"アプリを起動します");

	m_lpArgList = CommandLineToArgvW(GetCommandLineW(), &m_argc);
	if (m_lpArgList == nullptr) {
		m_argc = 0;
		m_Logger->LogError(L"コマンドライン引数の読込に失敗しました");
	}
	else {
		m_isAutoRun = false;
		for (int i = 0; i < m_argc; ++i) {
			m_isAutoRun |= std::wcscmp(m_lpArgList[i], ARG_AUTORUN) == 0;
		}
	}
	if (m_isAutoRun) m_Logger->Log(L"オートモードで起動しました");

	::InitializeCriticalSection(&m_wCS);
}

void VRCIPv6BlockerApp::ApplyDialogToConfig() {
	auto& config = m_Config.GetConfig();
	config.uRunVRC = ::IsDlgButtonChecked(m_hWnd, IDC_CHECK_RUNVRC);
	config.uAutoShutdown = ::IsDlgButtonChecked(m_hWnd, IDC_CHECK_AUTOEXIT);
	config.uMinWindow = ::IsDlgButtonChecked(m_hWnd, IDC_CHECK_MINWINDOW);
	config.uFirewallBlock = ::IsDlgButtonChecked(m_hWnd, IDC_CHECK_FIREWALL);
	WCHAR szBuf[MAX_PATH];
	::GetDlgItemTextW(m_hWnd, IDC_EDIT_LINK, szBuf, std::size(szBuf));
	config.strExecutePath = szBuf;
}

void VRCIPv6BlockerApp::ApplyConfigToDialog() {
	auto& config = m_Config.GetConfig();
	::CheckDlgButton(m_hWnd, IDC_CHECK_RUNVRC, config.uRunVRC);
	::CheckDlgButton(m_hWnd, IDC_CHECK_AUTOEXIT, config.uAutoShutdown);
	::CheckDlgButton(m_hWnd, IDC_CHECK_MINWINDOW, config.uMinWindow);
	::CheckDlgButton(m_hWnd, IDC_CHECK_FIREWALL, config.uFirewallBlock);
	::SetDlgItemTextW(m_hWnd, IDC_EDIT_LINK, config.strExecutePath.c_str());

	CheckDialogControl();
}

void VRCIPv6BlockerApp::CheckDialogControl() {
	auto& config = m_Config.GetConfig();
	if (m_isAutoRun && config.uRunVRC == BST_CHECKED && config.strExecutePath.length() > 0) {
		::EnableWindow(::GetDlgItem(m_hWnd, IDC_BUTTON_RUNVRC), FALSE);
	}
	else {
		::EnableWindow(::GetDlgItem(m_hWnd, IDC_BUTTON_RUNVRC), TRUE);
	}
	if (m_pRule->IsExistFirewallRule()) {
		::SetDlgItemText(m_hWnd, IDC_BUTTON_FIREWALL, L"FWブロック解除");
	}
	else {
		::SetDlgItemText(m_hWnd, IDC_BUTTON_FIREWALL, L"FWブロック登録");
	}
	if (m_pRule->IsEnableIPv6()) {
		::SetDlgItemText(m_hWnd, IDC_BUTTON_IPV6, L"IPv6無効化");
	}
	else {
		::SetDlgItemText(m_hWnd, IDC_BUTTON_IPV6, L"IPv6有効化");
	}
	::EnableWindow(::GetDlgItem(m_hWnd, IDC_BUTTON_FIREWALL), !m_isAutoRun && config.uFirewallBlock == BST_CHECKED);
	::EnableWindow(::GetDlgItem(m_hWnd, IDC_BUTTON_IPV6), !m_isAutoRun && config.uFirewallBlock != BST_CHECKED);

	::EnableWindow(::GetDlgItem(m_hWnd, IDC_BUTTON_REF), !m_isAutoRun);
	::EnableWindow(::GetDlgItem(m_hWnd, IDC_BUTTON_DELTS), !m_isAutoRun);
	::EnableWindow(::GetDlgItem(m_hWnd, IDC_BUTTON_MAKELINK), !m_isAutoRun);
	::EnableWindow(::GetDlgItem(m_hWnd, IDC_BUTTON_SAVE), !m_isAutoRun);

	::EnableWindow(::GetDlgItem(m_hWnd, IDC_BUTTON_DELTS), !m_isAutoRun && m_TaskScheduler->IsExists());
}

void VRCIPv6BlockerApp::VRCExecuter() {
	switch (m_VRCProcess->UserExecute(m_Config.GetConfig().strExecutePath.c_str())) {
	case 0:
		break;
	case 1:
		::SetDlgItemTextW(m_hWnd, IDC_STATIC_STATUS, L"VRChatの起動待ち...");
		break;
	default:
		::MessageBoxW(m_hWnd, L"起動できませんでした", L"エラー", MB_ICONERROR | MB_OK);
		break;
	}
}

void VRCIPv6BlockerApp::AutoStart() {
	auto& config = m_Config.GetConfig();
	if (config.uNonBlocking == BST_UNCHECKED) {
		m_pRule->ApplyBlock(config.uFirewallBlock == BST_CHECKED);
	}

	// まぁVRChatは起動しますけどね
	if (m_VRCProcess->GetProcessID()) {
		m_Logger->LogWarning(L"VRChatはすでに起動中です");
	}
	else {
		::Sleep(500); // なんか設定反映にラグがあったら嫌だから念のため500msほど待ってみる（不毛？）
		if(config.uRunVRC == BST_CHECKED) VRCExecuter();
	}
}

void VRCIPv6BlockerApp::AutoExit() {
	auto& config = m_Config.GetConfig();
	if (config.uNonBlocking == BST_UNCHECKED) {
		m_pRule->Restore(config.uFirewallBlock == BST_CHECKED);
	}
}

void VRCIPv6BlockerApp::WaitWorker() {
	if (m_Worker.has_value()) {
		m_bStopFlag.store(true);
		m_Worker.value().join();
		m_Worker.reset();
	}
}

void VRCIPv6BlockerApp::CreateShortcut() {
	WCHAR szFileName[MAX_PATH];
	::StringCchCopyW(szFileName, std::size(szFileName), DEF_SHORTCUT_NAME);
	if (ydk::SaveFileName(m_hWnd, szFileName, std::size(szFileName), L"ショートカットの保存先",
		OFN_OVERWRITEPROMPT,
		L"ショートカット(*.lnk)\0 * .lnk\0\0"
	)) {
		auto arg = std::wstring(L"/run /tn \"");
		arg += REGISTER_NAME;
		arg += L"\"";
		WCHAR szModuleFile[MAX_PATH] = {};
		::GetModuleFileNameW(m_hInstance, szModuleFile, std::size(szModuleFile));
		szModuleFile[std::size(szModuleFile) - 1] = L'\0';
		if (!ydk::CreateShortcut(szFileName, L"schtasks", L"C:\\WINDOWS\\system32\\", szModuleFile, -IDI_APPICON, arg.c_str())) {
			m_Logger->LogError(L"ショートカットの作成に失敗しました");
		}
		else {
			m_Logger->LogError(L"ショートカットを作成しました");
			::MessageBoxW(m_hWnd,
				L"ショートカットを作成しました\n"
				L"このアプリを終了して作成したショートカットから起動してください\n"
				L"後で設定を変更する場合はこのアプリを直接起動してください\n"
				L"※もしアプリのフォルダを変更する場合は作り直してください",
				L"通知",
				MB_ICONINFORMATION | MB_OK);
		}
	}
}

void VRCIPv6BlockerApp::OnClickMakeLinkButton() {
	if (m_TaskScheduler->IsExists()) {
		// 本来はここに来ることはないはず
		if (::MessageBoxW(
			m_hWnd,
			L"既に同名のタスクがあります。\n上書きしていいですか？",
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

	if (!m_TaskScheduler->CreateSchedule(szPath, ARG_AUTORUN, m_ModulePath.c_str())) {
		::MessageBoxW(m_hWnd, L"タスクスケジューラの登録でエラーが発生しました", L"エラー", MB_ICONERROR | MB_OK);
		return;
	}
	CreateShortcut();
	CheckDialogControl();
}

void VRCIPv6BlockerApp::OnClickDeleteTask() {
	if (::MessageBoxW(
		m_hWnd,
		L"タスクを削除すると作成したショートカットも無効になりますがいいですか？\n"
		L"再度登録するにはショートカットを作り直してください",
		L"確認",
		MB_ICONQUESTION | MB_YESNO
	) != IDYES) {
		return;
	}
	if (!m_TaskScheduler->DeleteSchedule()) {
		::MessageBoxW(m_hWnd, L"タスクスケジューラの削除に失敗しました", L"エラー", MB_ICONERROR | MB_OK);
	} else {
		::MessageBoxW(m_hWnd,
			L"タスクスケジューラから削除しました\n"
			L"現在のショートカットは無効になりますので再度登録する場合はショートカットから作り直してください",
			L"通知",
			MB_ICONINFORMATION | MB_OK);
	}
	CheckDialogControl();
}

void VRCIPv6BlockerApp::WriteExePath() {
	::WritePrivateProfileStringW(APP_NAME, IK_VRCFULLPATH, m_Config.GetConfig().strVRCFullPath.c_str(), m_IniFile.c_str());
}

// ---------------------------- 以下、現時点では使用していない

std::wstring VRCIPv6BlockerApp::GetLinkPath(LPCWSTR lpLinkFile) {
	std::wstring url, exe, fullCmd, workDir;
	int showCmd;
	LPCWSTR lpExt = ::PathFindExtensionW(lpLinkFile);
	if (!::_wcsicmp(lpExt, L".url")) {
		m_Logger->Log((std::wstring(L"url resolve -> ") + lpLinkFile).c_str());
		if (ydk::GetExecutableFromUrlFile(
				lpLinkFile, url, exe, fullCmd, workDir, showCmd)
			== ydk::UrlResolveMode::CommandLine
			) {
			m_Logger->Log((L"urlの指すパス : " + exe).c_str());
		}
		else {
			m_Logger->LogError(L"urlの解決に失敗");
		}
		return L"";
	}
	else if (!::_wcsicmp(lpExt, L".lnk")) {
		m_Logger->Log((std::wstring(L"lnk resolve -> ") + lpLinkFile).c_str());
		if (ydk::GetExecutableFromLnk(lpLinkFile, exe, fullCmd, workDir, showCmd)) {
			m_Logger->Log((L"lnkの指すパス : " + exe).c_str());
		}
		else {
			m_Logger->LogError(L"urlの解決に失敗");
			exe.clear();
		}
		return exe;
	}
	return lpLinkFile;
}

bool VRCIPv6BlockerApp::GetExeFilePath(LPCWSTR lpLaunchPath, std::wstring& exePath) {
	WCHAR szPath[MAX_PATH] = {};
	DWORD dwAttr = ::GetFileAttributesW(lpLaunchPath);
	if (dwAttr == INVALID_FILE_ATTRIBUTES) {
		m_Logger->LogError(lpLaunchPath != nullptr && *lpLaunchPath ? lpLaunchPath : L"<null>");
		m_Logger->LogError(L"ファイルパスがおかしいと思う");
		return false;
	}

	::wcscpy_s(szPath, lpLaunchPath);
	if (dwAttr & FILE_ATTRIBUTE_DIRECTORY) {
		m_Logger->LogWarning(L"渡されたパスはフォルダです");
	}
	else {
		HRESULT hr = ::PathCchRemoveFileSpec(szPath, std::size(szPath));
		if (FAILED(hr)) {
			m_Logger->LogError(L"パスを切り出せません");
			return false;
		}
	}
	LPCWSTR lastChar = nullptr;
	for (LPWSTR p = szPath; *p; ++p) {
		if (*p == L'/') *p = L'\\'; // ねんのため
		lastChar = p;
	}
	exePath = szPath;
	if (lastChar != nullptr && *lastChar != L'\\') exePath += L'\\';
	exePath += m_Config.GetConfig().strVRCFile;
	m_Logger->Log((L"ブロック対象のプログラムを特定 : " + exePath).c_str());
	return true;
}

