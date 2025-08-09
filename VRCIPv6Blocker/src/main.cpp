#include "VRCIPv6Blocker.hpp"

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);
	auto app = std::make_unique<VRCIPv6BlockerApp>();
	if (!app->Initialize(hInstance, nCmdShow)) {
		MessageBoxW(nullptr, L"アプリケーションの初期化に失敗", L"エラー", MB_ICONERROR | MB_OK);
		return 2;
	}
	return app->Run();
}
