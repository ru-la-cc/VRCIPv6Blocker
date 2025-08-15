#pragma once
#include "ISubclass.h"

class SubclassEditHandler : public ydk::SubclassHandler {
public:
	SubclassEditHandler(HWND hWnd) : SubclassHandler(hWnd) { }
	LRESULT HandleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) override;
};
