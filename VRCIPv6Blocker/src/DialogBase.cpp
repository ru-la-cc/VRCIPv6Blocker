#include "DialogBase.h"
#include <CommCtrl.h>

#pragma comment(lib, "comctl32.lib")

namespace ydk {
    // std::unordered_map<HWND, DialogAppBase*> DialogAppBase::m_dialogMap;

    DialogAppBase::DialogAppBase()
        : m_hInstance(nullptr)
        , m_hWnd(nullptr)
        , m_nCmdShow(SW_SHOW)
        , m_applicationName(L"Dialog Application") {
    }

    DialogAppBase::~DialogAppBase() {
        Shutdown();
    }

    bool DialogAppBase::Initialize(HINSTANCE hInstance, int nCmdShow) {
        m_hInstance = hInstance;
        m_nCmdShow = nCmdShow;

        INITCOMMONCONTROLSEX icc = {
            sizeof(INITCOMMONCONTROLSEX),
            ICC_WIN95_CLASSES |
            ICC_DATE_CLASSES |
            ICC_USEREX_CLASSES |
            ICC_COOL_CLASSES |
            ICC_INTERNET_CLASSES |
            ICC_PAGESCROLLER_CLASS
        };

        if (!InitCommonControlsEx(&icc)) {
			return false;
        }

		// 派生クラスの初期化処理
        if (!OnInitialize()) {
            return false;
        }

        // メインダイアログ作成
        if (!CreateMainDialog()) {
            return false;
        }

        return true;
    }

    int DialogAppBase::Run() {
        if (!m_hWnd) {
            return -1;
        }

        ShowWindow(m_hWnd, m_nCmdShow);
        UpdateWindow(m_hWnd);

        return MessageLoop();
    }

    void DialogAppBase::Shutdown() {
        if (m_hWnd) {
            // m_dialogMap.erase(m_Wnd);

            if (IsWindow(m_hWnd)) {
                DestroyWindow(m_hWnd);
            }
            m_hWnd = nullptr;
        }

        OnShutdown();
    }

    bool DialogAppBase::CreateMainDialog() {
        UINT dialogID = GetMainDialogID();
        if (dialogID == 0) {
            return false;
        }

		// m_hWndはWM_INITDIALOGで入ってくるのだが...
        m_hWnd = CreateDialogParamW(
            m_hInstance,
            MAKEINTRESOURCE(dialogID),
            nullptr,
            DialogProcStatic,
            reinterpret_cast<LPARAM>(this)
        );

        if (!m_hWnd) {
            DWORD error = GetLastError();
            // エラー情報を出力（デバッグ用）
            wchar_t errorMsg[256];
            ::swprintf_s(errorMsg, L"CreateDialogParam failed. Error: %lu", error);
            OutputDebugString(errorMsg);
            return false;
        }

        // m_dialogMap[m_hWnd] = this;
        return true;
    }

    int DialogAppBase::MessageLoop() {
        MSG msg;
        BOOL bRet;

        while ((bRet = GetMessage(&msg, nullptr, 0, 0)) != 0) {
            if (bRet == -1) {
                // エラー処理
                return -1;
            }

            // PreTranslateMessageでカスタム処理
            if (PreTranslateMessage(&msg)) {
                continue;
            }

            // ダイアログメッセージの処理
            if (!IsDialogMessage(m_hWnd, &msg)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }

            // アイドル処理（メッセージキューが空の時）
            if (PeekMessage(&msg, nullptr, 0, 0, PM_NOREMOVE) == 0) {
                OnIdle();
            }
        }

        return static_cast<int>(msg.wParam);
    }

    INT_PTR CALLBACK DialogAppBase::DialogProcStatic(HWND hDlg, UINT message,
        WPARAM wParam, LPARAM lParam) {
        DialogAppBase* pThis = nullptr;

        if (message == WM_INITDIALOG) {
            // 初期化時はlParamからインスタンスを取得
            pThis = reinterpret_cast<DialogAppBase*>(lParam);
            if (pThis) {
                SetWindowLongPtr(hDlg, DWLP_USER, reinterpret_cast<LONG_PTR>(pThis));
                return pThis->OnInitDialog(hDlg);
            }
        }
        else {
            // 通常のメッセージ処理
            pThis = reinterpret_cast<DialogAppBase*>(GetWindowLongPtr(hDlg, DWLP_USER));
        }

        if (pThis) {
            switch (message) {
            case WM_COMMAND:
                return pThis->OnCommand(hDlg, wParam, lParam);
            case WM_CLOSE:
                return pThis->OnClose(hDlg);
            case WM_DESTROY:
                return pThis->OnDestroy(hDlg);
            default:
                return pThis->HandleMessage(hDlg, message, wParam, lParam);
            }
        }

        return FALSE;
    }

    INT_PTR DialogAppBase::OnInitDialog(HWND hDlg) {
        // ウィンドウアイコン設定（必要に応じて）
		m_hWnd = hDlg;
        HICON hIcon = LoadIcon(m_hInstance, IDI_APPLICATION);
        if (hIcon) {
            SendMessage(hDlg, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(hIcon));
            SendMessage(hDlg, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(hIcon));
        }

        return TRUE;
    }

    INT_PTR DialogAppBase::OnCommand(HWND hDlg, WPARAM wParam, LPARAM lParam) {
        switch (LOWORD(wParam)) {
        case IDOK:
        case IDCANCEL:
            PostMessage(hDlg, WM_CLOSE, 0, 0);
            return TRUE;
        }
        return FALSE;
    }

    INT_PTR DialogAppBase::OnClose(HWND hDlg) {
        DestroyWindow(hDlg);
        return TRUE;
    }

    INT_PTR DialogAppBase::OnDestroy(HWND hDlg) {
        PostQuitMessage(0);
        return TRUE;
    }

    INT_PTR DialogAppBase::HandleMessage(HWND hDlg, UINT message,
        WPARAM wParam, LPARAM lParam) {
        return FALSE;
    }

    ///////////////////////////// ModalDialogBase実装 ///////////////////////////////

    ModalDialogBase::ModalDialogBase(HWND hParent)
        : m_hWnd(nullptr)
        , m_hParent(hParent) {
    }

    ModalDialogBase::~ModalDialogBase() {
    }

    INT_PTR ModalDialogBase::ShowDialog(HINSTANCE hInstance, UINT dialogID) {
        return DialogBoxParam(
            hInstance,
            MAKEINTRESOURCE(dialogID),
            m_hParent,
            DialogProcStatic,
            reinterpret_cast<LPARAM>(this)
        );
    }

    INT_PTR CALLBACK ModalDialogBase::DialogProcStatic(HWND hDlg, UINT message,
        WPARAM wParam, LPARAM lParam) {
        ModalDialogBase* pThis = nullptr;

        if (message == WM_INITDIALOG) {
            pThis = reinterpret_cast<ModalDialogBase*>(lParam);
            if (pThis) {
                pThis->m_hWnd = hDlg;
                SetWindowLongPtr(hDlg, DWLP_USER, reinterpret_cast<LONG_PTR>(pThis));
                return pThis->OnInitDialog(hDlg);
            }
        }
        else {
            pThis = reinterpret_cast<ModalDialogBase*>(GetWindowLongPtr(hDlg, DWLP_USER));
        }

        if (pThis) {
            switch (message) {
            case WM_COMMAND:
                return pThis->OnCommand(hDlg, wParam, lParam);
            default:
                return pThis->HandleMessage(hDlg, message, wParam, lParam);
            }
        }

        return FALSE;
    }

    INT_PTR ModalDialogBase::OnInitDialog(HWND hDlg) {
        // ダイアログを親ウィンドウの中央に配置・・・これってダイアログリソース側で設定できんかったっけ？
        if (m_hParent && IsWindow(m_hParent)) {
            RECT rcParent, rcDlg;
            GetWindowRect(m_hParent, &rcParent);
            GetWindowRect(hDlg, &rcDlg);

            int width = rcDlg.right - rcDlg.left;
            int height = rcDlg.bottom - rcDlg.top;
            int x = rcParent.left + (rcParent.right - rcParent.left - width) / 2;
            int y = rcParent.top + (rcParent.bottom - rcParent.top - height) / 2;

            SetWindowPos(hDlg, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
        }

        return TRUE;
    }

    INT_PTR ModalDialogBase::OnCommand(HWND hDlg, WPARAM wParam, LPARAM lParam) {
        switch (LOWORD(wParam)) {
        case IDOK:
            EndDialog(hDlg, IDOK);
            return TRUE;
        case IDCANCEL:
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
        }
        return FALSE;
    }

    INT_PTR ModalDialogBase::HandleMessage(HWND hDlg, UINT message,
        WPARAM wParam, LPARAM lParam) {
        return FALSE;
    }
}
