#include "rulectrl.h"
#include "defines.h"

RuleController::RuleController(LPCWSTR lpLockFileName)
	: m_LockFile(std::make_unique<ydk::LockFile>(lpLockFileName)) {
	if (m_LockFile->IsExist()) {
		// ロックファイルが存在したら、そこに復元するべき情報がある
		LOCK_INFO lockInfo = {};
		if (m_LockFile->GetLockInfo(reinterpret_cast<LPBYTE>(&lockInfo), sizeof(lockInfo))) {
			if (lockInfo.kind == LOCK_KIND::FW) {
				m_DefaultRuleExists = lockInfo.status == LOCK_STATUS::ON;
				m_LockFile->Cleanup();
			}
			else if (lockInfo.kind == LOCK_KIND::Adapter) {
				m_DefaultIPv6Enabled = lockInfo.status == LOCK_STATUS::ON;
				m_LockFile->Cleanup();
			}
		}
	}
}

bool RuleController::ApplyFirewallRules(const std::vector<std::wstring>& blockList, ydk::ILogger<WCHAR>* logger) {
	return false;
}

bool RuleController::DisableIPv6(ydk::ILogger<WCHAR>* logger) {
	return false;
}

bool RuleController::RestoreFirewallRule(ydk::ILogger<WCHAR>* logger) {
	return false;
}

bool RuleController::RestoreIPv6(ydk::ILogger<WCHAR>* logger) {
	return false;
}
