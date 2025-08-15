#include "ISubclass.h"
#include <unordered_map>
#include <memory>

namespace ydk {
	static std::unordered_map<HWND, std::pair<ISubclassHandler*, WNDPROC>> s_Subclass_map;

	LRESULT CALLBACK SubclassHandler::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
		auto it = s_Subclass_map.find(hWnd);
		if (it != s_Subclass_map.end()) {
			return it->second.first->HandleMessage(hWnd, msg, wParam, lParam);
		}

		return ::DefWindowProcW(hWnd, msg, wParam, lParam);
	}

	LRESULT SubclassHandler::CallOriginalWindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
		auto it = s_Subclass_map.find(hWnd);
		if (it != s_Subclass_map.end() && it->second.second != nullptr) {
			return ::CallWindowProcW(it->second.second, hWnd, msg, wParam, lParam);
		}

		return ::DefWindowProcW(hWnd, msg, wParam, lParam);
	}

	SubclassView::SubclassView(ISubclassHandler* handler)
		: m_pHandler(handler), m_originalWndProc(nullptr), m_isSubclassed(false) {
		if (m_pHandler != nullptr) {
			ChangeWindowProc();
		}
	}

	SubclassView::~SubclassView() {
		ResetWindowProc();
	}

	void SubclassView::ChangeWindowProc() {
		if (m_pHandler == nullptr || m_isSubclassed) {
			return;
		}

		HWND hWnd = m_pHandler->GetWindow();
		if (!::IsWindow(hWnd)) {
			return;
		}

		m_originalWndProc = reinterpret_cast<WNDPROC>(
			::GetWindowLongPtrW(hWnd, GWLP_WNDPROC)
			);

		if (m_originalWndProc == nullptr) {
			return;
		}

		s_Subclass_map[hWnd] = std::make_pair(m_pHandler, m_originalWndProc);

		::SetWindowLongPtrW(hWnd, GWLP_WNDPROC,
			reinterpret_cast<LONG_PTR>(SubclassHandler::WndProc));

		m_isSubclassed = true;
	}

	void SubclassView::ResetWindowProc() {
		if (!m_isSubclassed || m_pHandler == nullptr) {
			return;
		}

		HWND hWnd = m_pHandler->GetWindow();

		if (::IsWindow(hWnd) && m_originalWndProc != nullptr) {
			::SetWindowLongPtrW(hWnd, GWLP_WNDPROC,
				reinterpret_cast<LONG_PTR>(m_originalWndProc));
		}

		s_Subclass_map.erase(hWnd);
		m_isSubclassed = false;
	}
}
