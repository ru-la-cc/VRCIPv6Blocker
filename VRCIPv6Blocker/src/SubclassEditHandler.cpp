#include "SubclassEditHandler.h"
#include "UserProcessLauncher.h"
#include <iterator>

LRESULT SubclassEditHandler::HandleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
	case WM_DROPFILES:
		HDROP hDrop = reinterpret_cast<HDROP>(wParam);
		HANDLE hFind;
		WIN32_FIND_DATA w32fd;
		TCHAR szFile[MAX_PATH];
		::DragQueryFileW(hDrop, 0, szFile, MAX_PATH);
		::DragFinish(hDrop);
		szFile[std::size(szFile) - 1] = L'\0'; // なんかバッファ足りん時に終端どうなるか明記ないから保険で
		hFind = ::FindFirstFile(szFile, &w32fd);
		if (hFind != INVALID_HANDLE_VALUE) {
			::FindClose(hFind);
			if (!(w32fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
				if (ydk::IsWhiteListFile(szFile)) {
					::SendMessage(hWnd, WM_SETTEXT, 0, reinterpret_cast<LPARAM>(szFile));
				}
			}
		}
		return 0;
	}
	return CallOriginalWindowProc(hWnd, msg, wParam, lParam);
};
