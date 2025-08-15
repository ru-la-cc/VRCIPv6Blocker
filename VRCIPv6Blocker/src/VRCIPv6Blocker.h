#pragma once
#include "DialogBase.h"
#include "FileLogger.h"
#include "ISubclass.h"
#include "ComInitializer.h"
#include <vector>
#include "../resource.h"

class VRCIPv6BlockerApp final : public ydkns::DialogAppBase {
public:
	struct INI_SETTING {
		UINT uRunVRC;
		UINT uAutoShutdown;
		UINT uMinWindow;
		UINT uFirewallBlock;
		UINT uNonBlocking;
		std::wstring strExecutePath;
	};

	LPCWSTR APP_NAME = L"VRCIPv6Blocker";
	LPCWSTR BLOCK_LIST_FILE = L"blocklist.txt";
	LPCWSTR IK_RUNVRC = L"RunVRC";
	LPCWSTR IK_AUTOSHUTDOWN = L"AutoShutdown";
	LPCWSTR IK_MINWINDOW = L"MinWindow";
	LPCWSTR IK_FIREWALLBLOCK = L"FirewallBlock";
	LPCWSTR IK_EXECUTEPATH = L"Execute";
	LPCWSTR IK_NONBLOCKING = L"NonBlocking";

	virtual ~VRCIPv6BlockerApp();
	inline ydkns::IFileLogger<WCHAR>* Logger() { return m_Logger; }
	const std::wstring& GetCurrentFile() const { return m_currentFile; }

	constexpr UINT GetMainDialogID() const override { return IDD_MAINDLG; }
	bool OnInitialize() override;
	void OnShutdown() override;

	INT_PTR OnInitDialog(HWND hDlg) override;
	INT_PTR OnCommand(HWND hDlg, WPARAM wParam, LPARAM lParam) override;
	INT_PTR OnClose(HWND hDlg) override;
	INT_PTR HandleMessage(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) override;

	static VRCIPv6BlockerApp* Instance();

private:
	LPCWSTR logFileName = L"VRCIPv6Blocker.log";
	std::vector<std::string> m_BlockList;
	std::wstring m_ModulePath;
	std::wstring m_currentFile; // 何に使う想定だったか思い出せんけど一応残しておこう使わんなら消す
	ydkns::IFileLogger<WCHAR>* m_Logger;
	ydkns::ComInitializer m_comInitializer;
	int m_argc;
	LPWSTR* m_lpArgList;
	bool m_isAutoRun = false;
	std::unique_ptr<ydkns::ISubclassHandler> m_pEditPathHandler;
	std::unique_ptr<ydkns::ISubclassView> m_pEditPath;

	// 設定関連
	INI_SETTING m_Setting;

	VRCIPv6BlockerApp();
	void LoadBlockList();
	void LoadSetting();
	void SaveSetting();
	void GetSetting();
	void SetSetting();
	void DumpSetting();
	void CheckDialogControl();
};
