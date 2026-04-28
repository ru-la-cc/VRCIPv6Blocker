#include "win32except.h"

namespace ydk {
	std::string Win32Exception::To_Multibyte(LPCWSTR message) {
		if (message == nullptr) return std::string();
		int size = ydk::GetToUtf8Size(message);
		if (size <= 0) return std::string();
		std::string result(size, '\0');
		ydk::ToUtf8(message, result.data(), size);
		return result;
	}
}
