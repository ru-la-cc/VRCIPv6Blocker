#include "VRCIPv6Blocker.hpp"
#include "../resource.h"
#include <string>
#include <memory>
#include <CommDlg.h>  // ファイルダイアログ
#include <Shlwapi.h>  // パス操作

#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "Comdlg32.lib")

VRCIPv6BlockerApp::VRCIPv6BlockerApp()
    : ydkns::DialogAppBase() {
    // コンストラクタ
}

VRCIPv6BlockerApp::~VRCIPv6BlockerApp() {
    // デストラクタ
}

UINT VRCIPv6BlockerApp::GetMainDialogID() const {
    return IDD_MAINDLG;
}

bool VRCIPv6BlockerApp::OnInitialize() {
    // アプリケーション初期化
    // 設定ファイルの読み込みなど
	return true;
}

void VRCIPv6BlockerApp::OnShutdown() {
    // 設定の保存など
}

INT_PTR VRCIPv6BlockerApp::OnInitDialog(HWND hDlg) {
    // 基底クラスの処理
    ydkns::DialogAppBase::OnInitDialog(hDlg);

    // まぁこのあたりに初期化処理を書く予定

    return TRUE;
}

INT_PTR VRCIPv6BlockerApp::OnCommand(HWND hDlg, WPARAM wParam, LPARAM lParam) {
    switch (LOWORD(wParam)) {
    // IDM_xxxの処理を書くよてい
    default:
        return ydkns::DialogAppBase::OnCommand(hDlg, wParam, lParam);
    }
}

INT_PTR VRCIPv6BlockerApp::OnClose(HWND hDlg) {
    return ydkns::DialogAppBase::OnClose(hDlg);
}

INT_PTR VRCIPv6BlockerApp::HandleMessage(HWND hDlg, UINT message,
    WPARAM wParam, LPARAM lParam) {
    switch (message) {
        // ウインドウメッセージの処理をこの辺に書く予定
    return TRUE;
    }

    return ydkns::DialogAppBase::HandleMessage(hDlg, message, wParam, lParam);
}
