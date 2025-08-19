#include "VRCIPv6Blocker.h"
#include "AppMutex.h"

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	LPCWSTR mutex_name = L"Global\\" APP_GUID L"_" APP_NAME;

	ydk::AppMutex appMutex(mutex_name);
	if (appMutex.IsRunning()) return 2; // 多重起動は許さぬ

	VRCIPv6BlockerApp* app = VRCIPv6BlockerApp::Instance();
	if (!app->Initialize(hInstance, nCmdShow)) {
		MessageBoxW(nullptr, L"アプリケーションの初期化に失敗", L"エラー", MB_ICONERROR | MB_OK);
		return 2;
	}
	return app->Run();
}
