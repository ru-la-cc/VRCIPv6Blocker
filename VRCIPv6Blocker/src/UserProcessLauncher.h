#pragma once
#include <windows.h>
#include <string>

namespace ydk{
	// .url の解決モード
	enum class UrlResolveMode {
		CommandLine,
		DelegateToShell,
		Unresolvable
	};


	constexpr LPCWSTR WhiteListExt[] = {
		L".exe", L".lnk", L".url", L".bat", L".cmd", L".com", L".msi", L".scr"
	};
	// .lnk の解決
	bool GetExecutableFromLnk(
		const std::wstring& lnkPath,
		std::wstring& exe,
		std::wstring& args,
		std::wstring& workDir,
		int& showCmd);

	// .url の解決
	// 戻り値:
	//   CommandLine: exe, fullCmd, workDir, showCmd が有効 → CreateProcess* で直接起動可能
	//   DelegateToShell: ShellExecute に委譲すべき（既存の 'start' フォールバック等）
	//   Unresolvable: 解析不能（エラー扱い）
	UrlResolveMode GetExecutableFromUrlFile(
		const std::wstring& urlFile,
		std::wstring& url,
		std::wstring& exe,
		std::wstring& fullCmd,
		std::wstring& workDir,
		int& showCmd);

	bool IsWhiteListFile(LPCWSTR lpFileName);
	DWORD ShellExecuteWithLoginUser(LPCWSTR lpExePath, bool isComInitialize = false);
}
