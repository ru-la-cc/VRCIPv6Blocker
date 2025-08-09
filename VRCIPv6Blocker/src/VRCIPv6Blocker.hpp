#pragma once
#include "DialogBase.hpp"

class VRCIPv6BlockerApp : public ydkns::DialogAppBase {
private:
	std::wstring m_currentFile;

public:
	VRCIPv6BlockerApp();
	virtual ~VRCIPv6BlockerApp();
	const std::wstring& GetCurrentFile() const { return m_currentFile; }

protected:
	virtual UINT GetMainDialogID() const override;
	virtual bool OnInitialize() override;
	virtual void OnShutdown() override;

	virtual INT_PTR OnInitDialog(HWND hDlg) override;
	virtual INT_PTR OnCommand(HWND hDlg, WPARAM wParam, LPARAM lParam) override;
	virtual INT_PTR OnClose(HWND hDlg) override;
	virtual INT_PTR HandleMessage(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) override;
};
