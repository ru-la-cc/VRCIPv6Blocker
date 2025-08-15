#pragma once
#include <windows.h>

namespace ydkns {
	class ComInitializer final {
	public:
		ComInitializer(DWORD coInit = COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE) {
			m_hResult = ::CoInitializeEx(nullptr, coInit);
		}

		~ComInitializer() {
			if (SUCCEEDED(m_hResult)) {
				::CoUninitialize();
			}
		}

		ComInitializer(const ComInitializer&) = delete;
		ComInitializer& operator=(const ComInitializer&) = delete;
		ComInitializer(ComInitializer&&) = delete;
		ComInitializer& operator=(ComInitializer&&) = delete;

		inline bool IsInitialized() const { return SUCCEEDED(m_hResult); }
	private:
		HRESULT m_hResult;
	};
}
