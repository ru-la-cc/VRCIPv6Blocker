#include "rulectrl.h"
#include "WinFirewall.h"
#include <strsafe.h>
#include "defines.h"
#include "win32except.h"

RuleController::RuleController(LPCWSTR lpLockFileName, ydk::ILogger<WCHAR>* logger, const Config::INI_CONFIG& config)
	: m_LockFile(std::make_unique<ydk::LockFile>(lpLockFileName)), m_Logger(logger), m_conf(config) {
	if (m_LockFile->IsExist()) {
		// ロックファイルが存在したら、そこに復元するべき情報がある
		m_Logger->LogWarning(L"アプリの異常終了を検出したため設定の復元を行います");
		if (m_LockFile->Reacquire() && m_LockFile->GetLockInfo(reinterpret_cast<LPBYTE>(&m_LockInfo), sizeof(m_LockInfo))) {
			if (m_LockInfo.kind == LOCK_KIND::FW) {
				m_DefaultRuleExists = m_LockInfo.status == LOCK_DEFAULT::ON;
				m_IsRestore = RestoreFirewallRule();
				m_IsComplete = m_IsRestore;
			}
			else if (m_LockInfo.kind == LOCK_KIND::Adapter) {
				m_DefaultIPv6Enabled = m_LockInfo.status == LOCK_DEFAULT::ON;
				m_IsRestore = RestoreIPv6();
				m_IsComplete = m_IsRestore;
			}
		}
		else {
			m_Logger->LogError(L"ロック情報が取得できない");
		}
		IsExistsFirewallRule();
		IsIPv6Enabled();
	} else {
		// アプリ起動時点の状態をデフォルトとしておく
		// アプリ起動中にFWルール登録やIPv6の設定を変えると起動時点の状態に戻る可能性があるがそれは仕方ない
		m_DefaultRuleExists = IsExistsFirewallRule();
		m_DefaultIPv6Enabled = IsIPv6Enabled();
		m_IsComplete = true;
	}
}

bool RuleController::ApplyFirewallRules(const std::vector<std::wstring>& blockList) {
	if (blockList.empty()) {
		m_Logger->LogError(L"有効なブロック対象アドレスがないため設定は行いません");
		return false;
	}
	m_LockInfo.kind = LOCK_KIND::FW;
	m_LockInfo.status = m_DefaultRuleExists ? LOCK_DEFAULT::ON : LOCK_DEFAULT::OFF;
	DWORD dwAttr = ::GetFileAttributesW(m_conf.strVRCFullPath.c_str());
	if (!ydk::RegisterFirewallRule(
		REGISTER_NAME,
		blockList,
		nullptr,
		L"VRChat IPv6 Block Rule",
		(m_conf.uOnlyVRC == BST_UNCHECKED || dwAttr == INVALID_FILE_ATTRIBUTES || (dwAttr & FILE_ATTRIBUTE_DIRECTORY)) ?
		nullptr : m_conf.strVRCFullPath.c_str())
		) {
		m_Logger->LogError(L"Firewallのルール登録に失敗");
		return false;
	}
	else {
		m_LockFile->Lock(reinterpret_cast<LPCBYTE>(&m_LockInfo), sizeof(m_LockInfo));
		m_IsExistFirewallRule = true;
		m_Logger->Log(L"Firewallにルールを登録しました");
		return true;
	}
}

bool RuleController::DisableIPv6() {
	if (!m_DefaultIPv6Enabled) {
		m_Logger->LogWarning(L"アプリケーション起動時のIPv6はもともと無効です");
	}
	m_LockInfo.kind = LOCK_KIND::Adapter;
	m_LockInfo.status = m_DefaultIPv6Enabled ? LOCK_DEFAULT::ON : LOCK_DEFAULT::OFF;

	if (EnableIPv6(false)) {
		m_LockFile->Lock(reinterpret_cast<LPCBYTE>(&m_LockInfo), sizeof(m_LockInfo));
		m_Logger->Log(L"IPv6を無効化しました");
		return true;
	} else {
		m_Logger->LogError(L"IPv6の無効化に失敗しました");
	}
	return false;
}

bool RuleController::RestoreFirewallRule() {
	if (m_DefaultRuleExists) {
		m_Logger->LogWarning(L"Firewallのルールはもともと登録されていたため削除は行いません");
		m_LockFile->Cleanup();
		return true;
	}
	if (DeleteFirewallRule()) {
		m_LockFile->Cleanup();
		return true;
	}
	return false;
}

bool RuleController::RestoreIPv6() {
	m_LockInfo.kind = LOCK_KIND::Adapter;
	m_LockInfo.status = m_DefaultIPv6Enabled ? LOCK_DEFAULT::ON : LOCK_DEFAULT::OFF;
	if (EnableIPv6(m_DefaultIPv6Enabled)) {
		m_Logger->Log(L"IPv6の設定を復元しました");
		m_LockFile->Cleanup();
		return true;
	}
	else {
		m_Logger->LogError(L"IPv6の設定の復元に失敗しました");
	}
	return false;
}

bool RuleController::DeleteFirewallRule() {
	HRESULT hr;
	if (ydk::RemoveFirewallRule(REGISTER_NAME, &hr)) {
		if (hr == S_OK) {
			m_Logger->Log(L"Firewallの対象ルールを削除しました");
		}
		else if (hr == S_FALSE) {
			m_Logger->LogWarning(L"削除しようとしていたFirewallの対象ルールはありませんので削除したことにしておきます");
		}
		else {
			m_Logger->LogError(L"ここには来ないはずだが...");
			return false;
		}
		m_IsExistFirewallRule = false;
		return true;
	}
	else {
		m_Logger->LogError(L"ルールの削除に失敗しました");
	}
	return false;
}

bool RuleController::EnableIPv6(bool isEnable) {
	HRESULT hr = ydk::SetIPv6Enable(isEnable, &m_AdapterKey, m_conf.strDestIp.c_str());
	if (hr == S_OK) {
		m_IsEnableIPv6 = isEnable;
		if(isEnable) m_Logger->Log(L"IPv6を有効化しました");
		else m_Logger->Log(L"IPv6を無効化しました");
		return true;
	}
	else {
		m_Logger->LogError(L"IPv6の設定に失敗しました");
	}
	return false;
}


bool RuleController::IsExistsFirewallRule() const {
	HRESULT hr;
	if (ydk::ExistsFirewallRule(REGISTER_NAME, &hr)) {
		m_Logger->LogWarning(L"同一のルール名あり！登録する場合ルールは上書きされます");
		m_IsExistFirewallRule = true;
		return true; // 登録ある
	}
	if (FAILED(hr)) {
		WCHAR szErr[256];
		::StringCchPrintfW(szErr, std::size(szErr), L"ルールのチェックに失敗しました : HRESULT=%ld(0x%08x)", hr, hr);
		m_Logger->LogError(szErr);
		throw ydk::YDKException(L"ファイアウォールの情報を取得できないため処理を中断します");
	}
	m_IsExistFirewallRule = false;
	return false; // 登録なし
}

bool RuleController::IsIPv6Enabled() const {
	HRESULT hr;
	if (!m_AdapterKey.valid()) {
		hr = ydk::ResolveInternetAdapterFromString(m_conf.strDestIp.c_str(), m_AdapterKey);
		if (FAILED(hr)) {
			WCHAR szErr[256];
			::StringCchPrintfW(szErr, std::size(szErr), L"ネットワークアダプタの特定に失敗しました : HRESULT=%ld(0x%08x)", hr, hr);
			m_Logger->LogError(szErr);
			throw ydk::YDKException(L"ネットワークアダプタを取得できないため処理を中断します");
		}
	}
	if (ydk::IsIPv6Enable(m_AdapterKey, &hr)) {
		m_IsEnableIPv6 = true;
		return true;
	}
	if (FAILED(hr)) {
		WCHAR szErr[256];
		::StringCchPrintfW(szErr, std::size(szErr), L"IPv6の設定状況の取得に失敗しました : HRESULT=%ld(0x%08x)", hr, hr);
		m_Logger->LogError(szErr);
		throw ydk::YDKException(L"IPv6の設定状況を取得できないため処理を中断します");
	}
	m_IsEnableIPv6 = false;
	return false;
}
