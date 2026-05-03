#include "VRCIPv6Blocker.h"
#include "AppMutex.h"
#include "crashdump.h"
#include "win32except.h"

int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_  HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow) {
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

	ydk::CrashDump::Install();
	int result = 0;
	try {
		result = app->Run();
		if (app->m_Exception) {
			std::rethrow_exception(app->m_Exception);
		}
	}
	catch (const ydk::Win32Exception& ex) {
		::MessageBoxW(nullptr,
			(std::wstring(L"WinAPI Error : ") + ex.what_w()).c_str(),
			L"Win32API Error",
			MB_ICONERROR | MB_OK);
		result = 2;
	}
	catch (const ydk::YDKException& ex) {
		::MessageBoxW(nullptr,
			ex.what_w(),
			L"Runtime Error",
			MB_ICONERROR | MB_OK);
		result = 2;
	}
	catch (const std::exception& ex) {
		::MessageBoxA(nullptr,
			ex.what(),
			"C++ exception",
			MB_ICONERROR | MB_OK);
		result = 2;
	}
	catch (...) {
		::MessageBoxW(nullptr,
			L"もはや何が起きたかわからない",
			L"謎の例外",
			MB_ICONERROR | MB_OK);
		result = 2;
	}
	ydk::CrashDump::UnInstall();
	return result;
}
