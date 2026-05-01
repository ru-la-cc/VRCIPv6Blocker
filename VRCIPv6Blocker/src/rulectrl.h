#pragma once
#include "ipv6conf.h"
#include <string>
#include <vector>
#include <memory>
#include "ILogger.h"
#include "lockfile.h"
#include "config.h"

class RuleController final {
public:
	enum class LOCK_KIND : WORD {
		FW = 1,
		Adapter = 2
	};
	enum class LOCK_DEFAULT : WORD {
		OFF = 0,
		ON = 1
	};
	struct LOCK_INFO {
		LOCK_KIND kind;
		LOCK_DEFAULT status;
		ULONG ifIndex;
		GUID adapterGuid;
	};
	RuleController() = delete;
	RuleController(LPCWSTR lpLockFileName, ydk::ILogger<WCHAR>* logger, const Config::INI_CONFIG& config);
	~RuleController() { m_LockFile->Unlock(); }
	RuleController(const RuleController&) = delete;
	RuleController& operator=(const RuleController&) = delete;
	RuleController(RuleController&&) = delete;
	RuleController& operator=(RuleController&&) = delete;
	bool ApplyFirewallRules(const std::vector<std::wstring>& blockList);
	bool DisableIPv6();
	bool RestoreFirewallRule();
	bool RestoreIPv6();
	bool DeleteFirewallRule();
	bool EnableIPv6(bool isEnable);
	bool IsRestore() const noexcept { return m_IsRestore; } // 復元が実行されたら true
	bool IsComplete() const noexcept { return m_IsComplete; } // 復元が不要か復元が成功してたら true
	bool IsExistFirewallRule() const noexcept { return m_IsExistFirewallRule; } // Firewallのルールが存在しているか
	bool IsEnableIPv6() const noexcept { return m_IsEnableIPv6; } // IPv6が有効か
	bool IsChange() const noexcept {
		return (m_DefaultRuleExists != m_IsExistFirewallRule ||
			m_DefaultIPv6Enabled != m_IsEnableIPv6);
	}
	bool Cleanup() noexcept { return m_LockFile->Cleanup(); } // ロックファイルを削除するだけ（設定をそのままにしておく場合とか）
private:
	const Config::INI_CONFIG& m_conf;
	LOCK_INFO m_LockInfo = {};
	mutable ydk::AdapterKey m_AdapterKey = {};
	ydk::ILogger<WCHAR>* m_Logger;
	std::unique_ptr<ydk::LockFile> m_LockFile;
	bool IsExistsFirewallRule() const;
	bool IsIPv6Enabled() const;
	bool m_DefaultRuleExists = false;
	bool m_DefaultIPv6Enabled = true;
	bool m_IsRestore = false;
	bool m_IsComplete = false;
	mutable bool m_IsExistFirewallRule = false;
	mutable bool m_IsEnableIPv6 = false;
};
