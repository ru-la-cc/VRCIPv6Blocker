#pragma once
#include "rulectrl.h"
#include "ipv6conf.h"
#include "DialogBase.h"
#include "FileLogger.h"
#include "ISubclass.h"
#include "ComInitializer.h"
#include "lockfile.h"
#include "config.h"
#include "blocklist.h"
#include "tasksc.h"
#include <vector>
#include <optional>
#include <thread>
#include <atomic>
#include "vrcprocess.h"
#include "defines.h"
#include "../resource.h"

// このクラスのインスタンスはstaticなのでライフタイムとか気にしたら負け
class VRCIPv6BlockerApp final : public ydk::DialogAppBase {
public:
	~VRCIPv6BlockerApp();
	static VRCIPv6BlockerApp* Instance();
private:
	std::wstring m_ModulePath;
	std::wstring m_IniFile;
	Config m_Config;
	BlockList m_BlockList;
	std::unique_ptr<RuleController> m_pRule;
	std::unique_ptr<IPv6BlockScheduler> m_TaskScheduler;
	std::unique_ptr<VRCProcess> m_VRCProcess;
	ydk::ComInitializer m_comInitializer;
	CRITICAL_SECTION m_wCS;
	//CRITICAL_SECTION m_tidCs;
	std::unique_ptr<ydk::ISubclassHandler> m_pEditPathHandler;
	std::unique_ptr<ydk::ISubclassView> m_pEditPath;
	std::optional<std::thread> m_Worker;
	std::optional<std::thread> m_Waiter;
	std::atomic<bool> m_bStopFlag;

	ydk::IFileLogger<WCHAR>* m_Logger;
	LPWSTR* m_lpArgList;
	unsigned __int64 m_Version;

	HANDLE m_hMonThread = nullptr;
	HANDLE m_hWaitThread = nullptr;

	const DWORD PROCESS_MONITOR_INTERVAL = 100UL;
	DWORD m_vrcProcessId = 0;
	static constexpr UINT WM_VRCEXIT = WM_APP + 1;
	static constexpr UINT WM_SET_CTRLTEXT = WM_APP + 2;
	static constexpr UINT WM_ERR_MESSAGE = WM_APP + 3;
	static constexpr UINT WM_WRITE_VRCFULLPATH = WM_APP + 4;
	static constexpr UINT WM_ENABLE_CONTROL = WM_APP + 5;
	int m_argc;
	bool m_isAutoRun = false;

	VRCIPv6BlockerApp();

	inline constexpr ydk::IFileLogger<WCHAR>* Logger() const { return m_Logger; }
	inline constexpr UINT GetMainDialogID() const override { return IDD_MAINDLG; }
	bool OnInitialize() override;
	void OnShutdown() override;
	INT_PTR OnInitDialog(HWND hDlg) override;
	INT_PTR OnCommand(HWND hDlg, WPARAM wParam, LPARAM lParam) override;
	INT_PTR OnClose(HWND hDlg) override;
	INT_PTR HandleMessage(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) override;

	void ApplyDialogToConfig();
	void ApplyConfigToDialog();
	void CheckDialogControl();
	void VRCExecuter();
	std::wstring SerializeGuid(const GUID& guid);
	bool DeserializeGuid(LPCWSTR lpStr, GUID& guid);
	void WriteGuid(LPCWSTR lpGuid);
	void AutoStart();
	void AutoExit();
	void CreateShortcut();
	void OnClickMakeLinkButton();
	void OnClickDeleteTask();

	void WriteExePath();
	// ---------------------------- 以下、現時点では使用していない
	std::wstring GetLinkPath(LPCWSTR lpLinkFile);
	bool GetExeFilePath(LPCWSTR lpLaunchPath, std::wstring& exePath);
};
