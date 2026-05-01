#include "VRCIPv6Blocker.h"
#include "AppMutex.h"
#include "win32except.h"

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	LPCWSTR mutex_name = L"Global\\" APP_GUID L"_" APP_NAME;

	ydk::AppMutex appMutex(mutex_name);
	if (appMutex.IsRunning()) {
		::MessageBoxW(nullptr, L"複数起動することはできません", L"エラー", MB_ICONERROR | MB_OK);
		return 2;
	}
	VRCIPv6BlockerApp* app = VRCIPv6BlockerApp::Instance();
	if (!app->Initialize(hInstance, nCmdShow)) {
		::MessageBoxW(nullptr, L"アプリケーションの初期化に失敗", L"エラー", MB_ICONERROR | MB_OK);
		return 2;
	}
	try {
		return app->Run();
	}
	catch (const ydk::Win32Exception& ex) {
		::MessageBoxW(nullptr,
			(std::wstring(L"WinAPI Error : ") + ex.what_w()).c_str(),
			L"Win32API Error",
			MB_ICONERROR | MB_OK);
	}
	catch (const ydk::YDKException& ex) {
		::MessageBoxW(nullptr,
			ex.what_w(),
			L"Runtime Error",
			MB_ICONERROR | MB_OK);
	}
	catch (const std::exception& ex) {
		::MessageBoxA(nullptr,
			ex.what(),
			"C++ exception",
			MB_ICONERROR | MB_OK);
	}
	catch (...) {
		::MessageBoxW(nullptr,
			L"もはや何が起きたかわからない",
			L"謎の例外",
			MB_ICONERROR | MB_OK);
	}
	return 2;
}
