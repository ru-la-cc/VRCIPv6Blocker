#include "ISubClass.hpp"
#include <unordered_map>
#include <memory>

namespace ydkns {
	static std::unordered_map<HWND, std::pair<ISubClassHandler*, WNDPROC>> s_subclass_map;

	LRESULT CALLBACK SubClassHandler::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
		auto it = s_subclass_map.find(hWnd);
		if (it != s_subclass_map.end()) {
			return it->second.first->HandleMessage(hWnd, msg, wParam, lParam);
		}

		return ::DefWindowProcW(hWnd, msg, wParam, lParam);
	}

	LRESULT SubClassHandler::CallOriginalWindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
		auto it = s_subclass_map.find(hWnd);
		if (it != s_subclass_map.end() && it->second.second != nullptr) {
			return ::CallWindowProcW(it->second.second, hWnd, msg, wParam, lParam);
		}

		return ::DefWindowProcW(hWnd, msg, wParam, lParam);
	}

	SubClassView::SubClassView(ISubClassHandler* handler)
		: m_pHandler(handler), m_originalWndProc(nullptr), m_isSubclassed(false) {
		if (m_pHandler != nullptr) {
			ChangeWindowProc();
		}
	}

	SubClassView::~SubClassView() {
		ResetWindowProc();
	}

	void SubClassView::ChangeWindowProc() {
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

		s_subclass_map[hWnd] = std::make_pair(m_pHandler, m_originalWndProc);

		::SetWindowLongPtrW(hWnd, GWLP_WNDPROC,
			reinterpret_cast<LONG_PTR>(SubClassHandler::WndProc));

		m_isSubclassed = true;
	}

	void SubClassView::ResetWindowProc() {
		if (!m_isSubclassed || m_pHandler == nullptr) {
			return;
		}

		HWND hWnd = m_pHandler->GetWindow();

		if (::IsWindow(hWnd) && m_originalWndProc != nullptr) {
			::SetWindowLongPtrW(hWnd, GWLP_WNDPROC,
				reinterpret_cast<LONG_PTR>(m_originalWndProc));
		}

		s_subclass_map.erase(hWnd);
		m_isSubclassed = false;
	}
}
