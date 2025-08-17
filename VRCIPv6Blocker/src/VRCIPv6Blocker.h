#pragma once
#include "DialogBase.h"
#include "FileLogger.h"
#include "ISubclass.h"
#include "ComInitializer.h"
#include <vector>
#include "../resource.h"

class VRCIPv6BlockerApp final : public ydk::DialogAppBase {
public:
	struct INI_SETTING {
		UINT uRunVRC;
		UINT uAutoShutdown;
		UINT uMinWindow;
		UINT uFirewallBlock;
		UINT uNonBlocking;
		std::wstring strExecutePath;
		std::wstring strVRCFile;
	};

	static inline constexpr UINT WM_VRCEXIT = WM_APP + 1;

	LPCWSTR APP_NAME = L"VRCIPv6Blocker";
	LPCWSTR BLOCK_LIST_FILE = L"blocklist.txt";
	LPCWSTR IK_RUNVRC = L"RunVRC";
	LPCWSTR IK_AUTOSHUTDOWN = L"AutoShutdown";
	LPCWSTR IK_MINWINDOW = L"MinWindow";
	LPCWSTR IK_FIREWALLBLOCK = L"FirewallBlock";
	LPCWSTR IK_EXECUTEPATH = L"Execute";
	LPCWSTR IK_NONBLOCKING = L"NonBlocking";
	LPCWSTR IK_VRCFILE = L"VRCFile";

	virtual ~VRCIPv6BlockerApp();
	inline constexpr ydk::IFileLogger<WCHAR>* Logger() const { return m_Logger; }
	const std::wstring& GetCurrentFile() const { return m_currentFile; }

	inline constexpr UINT GetMainDialogID() const override { return IDD_MAINDLG; }
	bool OnInitialize() override;
	void OnShutdown() override;

	INT_PTR OnInitDialog(HWND hDlg) override;
	INT_PTR OnCommand(HWND hDlg, WPARAM wParam, LPARAM lParam) override;
	INT_PTR OnClose(HWND hDlg) override;
	INT_PTR HandleMessage(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) override;

	static VRCIPv6BlockerApp* Instance();

private:
	LPCWSTR logFileName = L"VRCIPv6Blocker.log";
	LPCWSTR VRCFILENAME = L"VRChat.exe";
	const DWORD PROCESS_MONITOR_INTERVAL = 100UL;
	std::vector<std::string> m_BlockList;
	std::wstring m_ModulePath;
	std::wstring m_currentFile; // 何に使う想定だったか思い出せんけど一応残しておこう使わんなら消す
	ydk::IFileLogger<WCHAR>* m_Logger;
	ydk::ComInitializer m_comInitializer;
	int m_argc;
	LPWSTR* m_lpArgList;
	bool m_isAutoRun = false;
	std::unique_ptr<ydk::ISubclassHandler> m_pEditPathHandler;
	std::unique_ptr<ydk::ISubclassView> m_pEditPath;
	CRITICAL_SECTION m_tidCs;
	DWORD m_vrcProcessId = 0;
	CRITICAL_SECTION m_tCs;
	bool m_isStop = false;
	HANDLE m_hMonThread = nullptr;
	HANDLE m_hWaitThread = nullptr;

	// 設定関連
	INI_SETTING m_Setting;

	static unsigned __stdcall VRCMonitoringThread(void* param);
	static unsigned __stdcall ProcessExitNotifyThread(void* param);

	VRCIPv6BlockerApp();
	void LoadBlockList();
	void LoadSetting();
	void SaveSetting();
	void GetSetting();
	void SetSetting();
	void DumpSetting();
	void CheckDialogControl();
	[[nodiscard]] DWORD GetVRChatProcess();
	void VRCExecuter();
	void SetStopFlag(bool isStop);
	bool GetStopFlag();
	void SetVRCProcessId(DWORD dwProcessId);
	DWORD GetVRCProcessId();
};
