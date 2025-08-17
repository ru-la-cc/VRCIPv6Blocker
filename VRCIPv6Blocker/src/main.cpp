#include "VRCIPv6Blocker.h"
#include "AppMutex.h"

#define APP_GUID L"{31952356-61C8-42F9-9D19-AC73E9AF5ED5}"

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	std::wstring mutex_name(L"Global\\"  L"{31952356-61C8-42F9-9D19-AC73E9AF5ED5}");
	mutex_name += L"_";
	mutex_name += VRCIPv6BlockerApp::APP_NAME;

	ydk::AppMutex appMutex(mutex_name.c_str());
	if (appMutex.IsRunning()) return 2; // 多重起動は許さぬ

	VRCIPv6BlockerApp* app = VRCIPv6BlockerApp::Instance();
	if (!app->Initialize(hInstance, nCmdShow)) {
		MessageBoxW(nullptr, L"アプリケーションの初期化に失敗", L"エラー", MB_ICONERROR | MB_OK);
		return 2;
	}
	return app->Run();
}
