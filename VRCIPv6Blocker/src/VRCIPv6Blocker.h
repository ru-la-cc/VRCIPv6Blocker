#pragma once
#include "ipv6conf.h"
#include "DialogBase.h"
#include "FileLogger.h"
#include "ISubclass.h"
#include "ComInitializer.h"
#include <vector>
#include "../resource.h"

#define APP_GUID L"{31952356-61C8-42F9-9D19-AC73E9AF5ED5}"
#define APP_NAME L"VRCIPv6Blocker"

class VRCIPv6BlockerApp final : public ydk::DialogAppBase {
public:
	struct INI_SETTING {
		UINT uRunVRC;
		UINT uAutoShutdown;
		UINT uMinWindow;
		UINT uFirewallBlock;
		UINT uNonBlocking;
		UINT uRevert;
		UINT uOnlyVRC;
		std::wstring strExecutePath;
		std::wstring strVRCFile;
		std::wstring strDestIp;
		std::wstring strNIC;
		std::wstring strVRCFullPath;
	};

	static inline constexpr UINT WM_VRCEXIT = WM_APP + 1;
	static inline constexpr UINT WM_SET_CTRLTEXT = WM_APP + 2;
	static inline constexpr UINT WM_ERR_MESSAGE = WM_APP + 3;
	static inline constexpr UINT WM_WRITE_VRCFULLPATH = WM_APP + 4;
	static inline constexpr UINT WM_ENABLE_CONTROL = WM_APP + 5;

	static constexpr LPCWSTR REGISTER_NAME = APP_GUID L"_" APP_NAME;
	LPCWSTR BLOCK_LIST_FILE = L"blocklist.txt";
	LPCWSTR IK_RUNVRC = L"RunVRC";
	LPCWSTR IK_AUTOSHUTDOWN = L"AutoShutdown";
	LPCWSTR IK_MINWINDOW = L"MinWindow";
	LPCWSTR IK_FIREWALLBLOCK = L"FirewallBlock";
	LPCWSTR IK_EXECUTEPATH = L"Execute";
	LPCWSTR IK_NONBLOCKING = L"NonBlocking";
	LPCWSTR IK_REVERT = L"Revert";
	LPCWSTR IK_ONLYVRC = L"OnlyVRC";
	LPCWSTR IK_VRCFILE = L"VRCFile";
	LPCWSTR IK_DESTIP = L"DestIp";
	LPCWSTR IK_NIC = L"NIC";
	LPCWSTR IK_VRCFULLPATH = L"VRCFullPath";
	LPCWSTR ARG_AUTORUN = L"-autorun";

	virtual ~VRCIPv6BlockerApp();
	inline constexpr ydk::IFileLogger<WCHAR>* Logger() const { return m_Logger; }

	inline constexpr UINT GetMainDialogID() const override { return IDD_MAINDLG; }
	bool OnInitialize() override;
	void OnShutdown() override;

	INT_PTR OnInitDialog(HWND hDlg) override;
	INT_PTR OnCommand(HWND hDlg, WPARAM wParam, LPARAM lParam) override;
	INT_PTR OnClose(HWND hDlg) override;
	INT_PTR HandleMessage(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) override;

	static VRCIPv6BlockerApp* Instance();

private:
	std::vector<std::wstring> m_BlockList;
	std::wstring m_ModulePath;
	std::wstring m_IniFile;
	INI_SETTING m_Setting = {};
	ydk::ComInitializer m_comInitializer;
	CRITICAL_SECTION m_tCs;
	CRITICAL_SECTION m_tidCs;
	std::unique_ptr<ydk::ISubclassHandler> m_pEditPathHandler;
	std::unique_ptr<ydk::ISubclassView> m_pEditPath;
	ydk::AdapterKey m_adapterKey = {};
	ydk::IFileLogger<WCHAR>* m_Logger;
	LPWSTR* m_lpArgList;
	LPCWSTR logFileName = L"VRCIPv6Blocker.log";
	LPCWSTR VRCFILENAME = L"VRChat.exe";
	HANDLE m_hMonThread = nullptr;
	HANDLE m_hWaitThread = nullptr;
	const DWORD PROCESS_MONITOR_INTERVAL = 100UL;
	DWORD m_vrcProcessId = 0;
	int m_argc;
	bool m_isAutoRun = false;
	bool m_isStop = false;
	bool m_isFirewallBlocked = false;
	bool m_isIPv6Enabled = true;
	bool m_isTaskExist = false;

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
	bool IsFirewallRegistered();
	void SetFirewall();
	void RemoveFirewall();
	bool SetIPv6(bool isEnable);
	std::wstring SerializeGuid(const GUID& guid);
	bool DeserializeGuid(LPCWSTR lpStr, GUID& guid);
	void WriteGuid(LPCWSTR lpGuid);
	void CheckIPv6Setting();
	void ChangeFireWall();
	void ChangeIPv6();
	void AutoStart();
	void AutoExit();
	bool CreateShortcut();
	void CreateScheduledTaskWithShortcut();
	void DeleteTask();

	void WriteExePath();
	// ---------------------------- 以下、使ってないけど残しておく
	std::wstring GetLinkPath(LPCWSTR lpLinkFile);
	bool GetExeFilePath(LPCWSTR lpLaunchPath, std::wstring& exePath);
};
