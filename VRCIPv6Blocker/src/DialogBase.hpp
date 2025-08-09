#pragma once

#include <Windows.h>
#include <memory>
#include <string>
#include <unordered_map>

namespace ydkns {
	// 汎用ダイアログアプリ基底クラス
	class DialogAppBase {
	protected:
		HINSTANCE m_hInstance;
		HWND m_hMainDialog;
		int m_nCmdShow;
		std::wstring m_applicationName;

		static std::unordered_map<HWND, DialogAppBase*> m_dialogMap;

	public:
		DialogAppBase();
		virtual ~DialogAppBase();

		bool Initialize(HINSTANCE hInstance, int nCmdShow);
		int Run();
		void Shutdown();

		HWND GetMainDialog() const { return m_hMainDialog; }
		HINSTANCE GetInstance() const { return m_hInstance; }

	protected:
		// 派生クラスでオーバーライドする仮想関数
		virtual UINT GetMainDialogID() const = 0;  // ダイアログリソースID取得
		virtual bool OnInitialize() { return true; }  // 初期化時の追加処理
		virtual void OnShutdown() {}  // 終了時の追加処理

		// ダイアログプロシージャ
		virtual INT_PTR OnInitDialog(HWND hDlg);
		virtual INT_PTR OnCommand(HWND hDlg, WPARAM wParam, LPARAM lParam);
		virtual INT_PTR OnClose(HWND hDlg);
		virtual INT_PTR OnDestroy(HWND hDlg);
		virtual INT_PTR HandleMessage(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);

		// メッセージループのカスタマイズ用
		virtual bool PreTranslateMessage(MSG* pMsg) { return false; }
		virtual void OnIdle() {}  // アイドル処理

	private:
		// スタティックダイアログプロシージャ
		static INT_PTR CALLBACK DialogProcStatic(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);

		// メインダイアログ作成
		bool CreateMainDialog();

		// カスタムメッセージループ
		int MessageLoop();
	};

	// モーダルダイアログ基底クラス
	class ModalDialogBase {
	protected:
		HWND m_hWnd;
		HWND m_hParent;

	public:
		ModalDialogBase(HWND hParent = nullptr);
		virtual ~ModalDialogBase();

		// ダイアログ表示
		INT_PTR ShowDialog(HINSTANCE hInstance, UINT dialogID);

	protected:
		virtual INT_PTR OnInitDialog(HWND hDlg);
		virtual INT_PTR OnCommand(HWND hDlg, WPARAM wParam, LPARAM lParam);
		virtual INT_PTR HandleMessage(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);

	private:
		static INT_PTR CALLBACK DialogProcStatic(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
	};
}
