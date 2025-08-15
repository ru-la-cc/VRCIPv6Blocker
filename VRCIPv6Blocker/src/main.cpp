#include "VRCIPv6Blocker.h"
#include "AppMutex.h"

#define MUTEX_NAME (L"Global\\{31952356-61C8-42F9-9D19-AC73E9AF5ED5}_VRCIPv6Blocker")

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	ydkns::AppMutex appMutex(MUTEX_NAME);
	if (appMutex.IsRunning()) return 2; // 多重起動は許さぬ

	VRCIPv6BlockerApp* app = VRCIPv6BlockerApp::Instance();
	if (!app->Initialize(hInstance, nCmdShow)) {
		MessageBoxW(nullptr, L"アプリケーションの初期化に失敗", L"エラー", MB_ICONERROR | MB_OK);
		return 2;
	}
	return app->Run();
}
