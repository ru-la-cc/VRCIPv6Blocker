#pragma once
#include "taskman.h"
#include "ILogger.h"
#include "defines.h"

class IPv6BlockScheduler final {
public:
	IPv6BlockScheduler() = delete;
	IPv6BlockScheduler(ydk::ILogger<WCHAR>* logger) : m_Logger(logger) {}
	~IPv6BlockScheduler() = default;
	IPv6BlockScheduler(const IPv6BlockScheduler&) = delete;
	IPv6BlockScheduler& operator=(const IPv6BlockScheduler&) = delete;
	IPv6BlockScheduler(IPv6BlockScheduler&&) = delete;
	IPv6BlockScheduler& operator=(IPv6BlockScheduler&&) = delete;
	bool IsExists() const noexcept { return ydk::IsExistSchedule(REGISTER_NAME); }
	bool CreateSchedule(LPCWSTR lpExePath, LPCWSTR lpArgs, LPCWSTR lpWorkDir) noexcept;
	bool DeleteSchedule() noexcept;
private:
	ydk::ILogger<WCHAR>* m_Logger;
};
