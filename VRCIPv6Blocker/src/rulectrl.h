#pragma once
#include <string>
#include <vector>
#include <memory>
#include "ILogger.h"
#include "lockfile.h"

class RuleController final {
public:
	enum class LOCK_KIND : WORD {
		FW = 1,
		Adapter = 2
	};
	enum class LOCK_STATUS : WORD {
		OFF = 0,
		ON = 1
	};
	struct LOCK_INFO {
		LOCK_KIND kind;
		LOCK_STATUS status;
		ULONG ifIndex;
		GUID adapterGuid;
	};
	RuleController() = delete;
	RuleController(LPCWSTR lpLockFileName);
	~RuleController() = default;
	RuleController(const RuleController&) = delete;
	RuleController& operator=(const RuleController&) = delete;
	RuleController(RuleController&&) = delete;
	RuleController& operator=(RuleController&&) = delete;
	bool ApplyFirewallRules(const std::vector<std::wstring>& blockList, ydk::ILogger<WCHAR>* logger);
	bool DisableIPv6(ydk::ILogger<WCHAR>* logger);
	bool RestoreFirewallRule(ydk::ILogger<WCHAR>* logger);
	bool RestoreIPv6(ydk::ILogger<WCHAR>* logger);
private:
	bool m_DefaultRuleExists = false;
	bool m_DefaultIPv6Enabled = true;
	std::unique_ptr<ydk::LockFile> m_LockFile;
	bool CheckLock() const { return m_LockFile->IsExist(); }
};
