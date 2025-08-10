#pragma once
#include "DialogBase.hpp"
#include "FileLogger.hpp"

class VRCIPv6BlockerApp final : public ydkns::DialogAppBase {
public:
	virtual ~VRCIPv6BlockerApp();
	inline ydkns::IFileLogger<WCHAR>* Logger() { return m_Logger; }
	const std::wstring& GetCurrentFile() const { return m_currentFile; }

	virtual UINT GetMainDialogID() const override;
	virtual bool OnInitialize() override;
	virtual void OnShutdown() override;

	virtual INT_PTR OnInitDialog(HWND hDlg) override;
	virtual INT_PTR OnCommand(HWND hDlg, WPARAM wParam, LPARAM lParam) override;
	virtual INT_PTR OnClose(HWND hDlg) override;
	virtual INT_PTR HandleMessage(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) override;

	static VRCIPv6BlockerApp* Instance();

private:
	LPCWSTR logFileName = L"VRCIPv6Blocker.log";
	std::wstring m_currentFile; // 何に使う想定だったか思い出せんけど一応残しておこう
	ydkns::IFileLogger<WCHAR>* m_Logger;

	VRCIPv6BlockerApp(); // シングルトンでええやろこれ
};
