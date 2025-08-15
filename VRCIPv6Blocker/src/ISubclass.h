#pragma once
#include <windows.h>

namespace ydkns {
	struct ISubclassHandler {
		virtual ~ISubclassHandler() = default;
		virtual HWND GetWindow() const = 0;
		virtual LRESULT HandleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) = 0;
	};

	struct ISubclassView {
		virtual ~ISubclassView() = default;
		virtual void ChangeWindowProc() = 0;
		virtual void ResetWindowProc() = 0;
	};

	class SubclassHandler : public ISubclassHandler {
	public:
		SubclassHandler(HWND hWnd) : m_hWnd(hWnd) {}
		virtual ~SubclassHandler() override = default;

		[[noexcept]] constexpr HWND GetWindow() const override { return m_hWnd; }

		LRESULT HandleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) override {
			return CallOriginalWindowProc(hWnd, msg, wParam, lParam);
		}

		static LRESULT CallOriginalWindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

	private:
		HWND m_hWnd;
		static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
		friend class SubclassView;
	};

	class SubclassView : public ISubclassView {
	public:
		SubclassView(ISubclassHandler* handler);
		virtual ~SubclassView() override;

		void ChangeWindowProc() override;
		void ResetWindowProc() override;

	private:
		ISubclassHandler* m_pHandler;
		WNDPROC m_originalWndProc; // 元のウィンドウプロシージャ
		bool m_isSubclassed;
	};
}
