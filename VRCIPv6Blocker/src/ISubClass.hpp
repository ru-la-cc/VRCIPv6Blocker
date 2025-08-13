#pragma once
#include <windows.h>

namespace ydkns {
	struct ISubClassHandler {
		virtual ~ISubClassHandler() = default;
		virtual HWND GetWindow() const = 0;
		virtual LRESULT HandleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) = 0;
	};

	struct ISubClassView {
		virtual ~ISubClassView() = default;
		virtual void ChangeWindowProc() = 0;
		virtual void ResetWindowProc() = 0;
	};

	class SubClassHandler : public ISubClassHandler {
	public:
		SubClassHandler(HWND hWnd) : m_hWnd(hWnd) {}
		virtual ~SubClassHandler() override = default;

		[[noexcept]] constexpr HWND GetWindow() const override { return m_hWnd; }

		LRESULT HandleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) override {
			return CallOriginalWindowProc(hWnd, msg, wParam, lParam);
		}

		static LRESULT CallOriginalWindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

	private:
		HWND m_hWnd;
		static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
		friend class SubClassView;
	};

	class SubClassView : public ISubClassView {
	public:
		SubClassView(ISubClassHandler* handler);
		virtual ~SubClassView() override;

		void ChangeWindowProc() override;
		void ResetWindowProc() override;

	private:
		ISubClassHandler* m_pHandler;
		WNDPROC m_originalWndProc; // 元のウィンドウプロシージャ
		bool m_isSubclassed;
	};
}
