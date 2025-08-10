#ifndef __INC_YDK_ILOGGER_HPP__
#define __INC_YDK_ILOGGER_HPP__
#include <concepts>
#include <type_traits>

// インターフェース的な何か
namespace ydkns {
	enum class LogType : int {
		Info = 0,
		Warning,
		Error,
	};

	template <typename T>
	concept CharType =
		std::same_as<T, char> ||
		std::same_as<T, char16_t> ||
		std::same_as<T, char32_t> ||
		std::same_as<T, wchar_t>;

	template <typename TChar>
	requires CharType<TChar>
	class ILogger {
	public:
		virtual ~ILogger() = default;
		virtual bool Log(const TChar* message) = 0;
		virtual bool LogWarning(const TChar* message) = 0;
		virtual bool LogError(const TChar* message) = 0;
	protected:
		ILogger() = default;
	};

	template <typename TChar>
	requires CharType<TChar>
	class IFileLogger : public ILogger<TChar> {
	public:
		virtual ~IFileLogger() = default;
		virtual bool Open() = 0;
		virtual bool Close() = 0;
		virtual bool Flush() = 0;
	protected:
		IFileLogger() = default;
	};
}
#endif
