#include "VRCIPv6Blocker.h"
#include "YDKWinUtils.h"
#include "SubClassEditHandler.h"
#include <CommDlg.h>
#include <Shlwapi.h>
#include <strsafe.h>

#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "Comdlg32.lib")

VRCIPv6BlockerApp::~VRCIPv6BlockerApp() {
    // デストラクタ（シングルトンだからコンストラクタはprivate↓）
	if (m_lpArgList != nullptr) {
		::LocalFree(m_lpArgList);
	}
	m_Logger->Log(L"アプリを終了します");
}

bool VRCIPv6BlockerApp::OnInitialize() {
    // アプリケーション初期化

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

	LoadBlockList();
	LoadSetting();
	SetSetting();

	if (m_Setting.uNonBlocking == BST_CHECKED) {
		WCHAR szCaption[64];
		::swprintf_s(szCaption, L"%s (NonBlock)", APP_NAME);
		::SetWindowTextW(m_hWnd, szCaption);
		m_Logger->LogWarning(L"IPv6をブロックしない名前と相反する動作をします");
	}
	else {
		::SetWindowTextW(m_hWnd, APP_NAME);
	}

    return TRUE;
}

INT_PTR VRCIPv6BlockerApp::OnCommand(HWND hDlg, WPARAM wParam, LPARAM lParam) {
	switch (HIWORD(wParam)) {
	case BN_CLICKED:
		m_Logger->Log(L"BN_CLICKED");
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
	case IDC_BUTTON_SAVE:
		GetSetting();
		SaveSetting();
		::MessageBoxW(m_hWnd, L"設定を保存しました", L"通知", MB_ICONINFORMATION | MB_OK);
		return TRUE;
    }
	return ydk::DialogAppBase::OnCommand(hDlg, wParam, lParam);
}

INT_PTR VRCIPv6BlockerApp::OnClose(HWND hDlg) {
    return ydk::DialogAppBase::OnClose(hDlg);
}

INT_PTR VRCIPv6BlockerApp::HandleMessage(HWND hDlg, UINT message,
    WPARAM wParam, LPARAM lParam) {
    switch (message) {
        // ウインドウメッセージの処理をこの辺に書く予定
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
