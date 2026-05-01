#pragma once
#include <windows.h>

namespace ydk {

	// INVALID_HANDLE_VALUE用
	class ScopedHandle final {
	public:
		ScopedHandle(HANDLE h) noexcept : m_handle(h) {}
		~ScopedHandle() {
			if (m_handle != INVALID_HANDLE_VALUE) ::CloseHandle(m_handle);
			m_handle = INVALID_HANDLE_VALUE;
		}
		ScopedHandle(const ScopedHandle&) = delete;
		ScopedHandle& operator=(const ScopedHandle&) = delete;
		ScopedHandle(ScopedHandle&& other) noexcept : m_handle(other.m_handle) {
			other.m_handle = INVALID_HANDLE_VALUE;
		}
		ScopedHandle& operator=(ScopedHandle&& other) noexcept {
			if (this != &other) {
				if (m_handle != INVALID_HANDLE_VALUE) ::CloseHandle(m_handle);
				m_handle = other.m_handle;
				other.m_handle = INVALID_HANDLE_VALUE;
			}
		}
		HANDLE Get() const noexcept { return m_handle; }
	private:
		HANDLE m_handle;
	};
}
