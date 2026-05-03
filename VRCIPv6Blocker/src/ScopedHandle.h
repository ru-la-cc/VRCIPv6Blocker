#pragma once
#include <windows.h>

namespace ydk {
	struct InvalidHandleTraits {
		using HandleType = HANDLE;
		static HandleType Invalid() noexcept { return INVALID_HANDLE_VALUE; }
		static void Close(HandleType h) noexcept { ::CloseHandle(h); }
	};

	struct NullHandleTraits {
		using HandleType = HANDLE;
		static HandleType Invalid() noexcept { return nullptr; }
		static void Close(HandleType h) noexcept { ::CloseHandle(h); }
	};

	template<typename Traits>
	class GenericScopedHandle final {
	public:
		GenericScopedHandle() noexcept : m_handle(Traits::Invalid()) {}
		explicit GenericScopedHandle(Traits::HandleType h) noexcept : m_handle(h) {}
		~GenericScopedHandle() noexcept { Close(); }
		GenericScopedHandle(const GenericScopedHandle&) = delete;
		GenericScopedHandle& operator=(const GenericScopedHandle&) = delete;
		GenericScopedHandle(GenericScopedHandle&& other) noexcept : m_handle(other.m_handle) {
			other.m_handle = Traits::Invalid();
		}
		GenericScopedHandle& operator=(GenericScopedHandle&& other) noexcept {
			if (this != &other) {
				Close();
				m_handle = other.m_handle;
				other.m_handle = Traits::Invalid();
			}
			return *this;
		}
		[[nodiscard]] bool IsValid() const noexcept { return m_handle != Traits::Invalid(); }
		Traits::HandleType Get() const noexcept { return m_handle; }
		void Close() noexcept {
			if (m_handle != Traits::Invalid()) Traits::Close(m_handle);
			m_handle = Traits::Invalid();
		}
	private:
		Traits::HandleType m_handle;
	};

	using ScopedHandle = GenericScopedHandle<InvalidHandleTraits>;
	using ScopedNullHandle = GenericScopedHandle<NullHandleTraits>;
}
