#pragma once
#include <windows.h>

namespace ydk {

	// タスクを登録（存在すれば上書き）。オンデマンド実行、各種条件/設定は要件通りに適用。
	// taskName  : タスク名（例: L"MyAppTask"）
	// exePath   : 実行ファイルの絶対パス
	// arguments : 省略可
	// workDir   : 省略可（作業ディレクトリ）
	// 戻り値    : 成功時 S_OK
	HRESULT RegisterTaskScheduler(const wchar_t* taskName,
		const wchar_t* exePath,
		const wchar_t* arguments = nullptr,
		const wchar_t* workDir = nullptr) noexcept;

	// タスクの存在確認（ルートフォルダ直下にあるか）
	bool IsExistSchedule(const wchar_t* taskName) noexcept;

	// タスクの削除（存在しなくても成功扱い）
	HRESULT RemoveTaskScheduler(const wchar_t* taskName) noexcept;

} // namespace taskman
