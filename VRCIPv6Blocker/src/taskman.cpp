#include "taskman.h"
#include <taskschd.h>
#include <combaseapi.h> // SysAllocString, SysFreeString
#include <shlwapi.h>    // PathFileExistsW 等（必要なら）
#pragma comment(lib, "taskschd.lib")
#pragma comment(lib, "shlwapi.lib")

namespace ydk {

	// そのうちこいつは外出しして共通化する
	template <class T>
	struct ComPtr {
		T* p = nullptr;
		ComPtr() = default;
		ComPtr(const ComPtr&) = delete;
		ComPtr& operator=(const ComPtr&) = delete;
		~ComPtr() { if (p) p->Release(); }
		T** operator&() noexcept { return &p; }
		T* operator->() const noexcept { return p; }
		operator bool() const noexcept { return p != nullptr; }
	};

	struct BStr {
		BSTR b = nullptr;
		BStr() = default;
		explicit BStr(const wchar_t* s) { if (s) b = ::SysAllocString(s); }
		~BStr() { if (b) ::SysFreeString(b); }
		operator BSTR() const noexcept { return b; }
		// コピー不可・ムーブ不可（単純化）
		BStr(const BStr&) = delete;
		BStr& operator=(const BStr&) = delete;
	};

	static inline VARIANT VEmpty() noexcept {
		VARIANT v; ::VariantInit(&v); return v;
	}

	static HRESULT ConnectService(ComPtr<ITaskService>& svc) noexcept {
		HRESULT hr = ::CoCreateInstance(CLSID_TaskScheduler, nullptr, CLSCTX_INPROC_SERVER,
			IID_PPV_ARGS(&svc.p));
		if (FAILED(hr)) return hr;
		VARIANT vEmpty = VEmpty();
		return svc->Connect(vEmpty, vEmpty, vEmpty, vEmpty); // 現在ユーザー
	}

	// ルートフォルダ取得
	static HRESULT GetRootFolder(ITaskService* svc, ComPtr<ITaskFolder>& folder) noexcept {
		if (!svc) return E_POINTER;
		BStr root(L"\\");
		return svc->GetFolder(root, &folder.p);
	}

	// アクション（実行）作成
	static HRESULT AddExecAction(ITaskDefinition* def,
		const wchar_t* exePath,
		const wchar_t* arguments,
		const wchar_t* workDir) noexcept {
		if (!def || !exePath || !*exePath) return E_INVALIDARG;

		ComPtr<IActionCollection> actions;
		HRESULT hr = def->get_Actions(&actions.p);
		if (FAILED(hr)) return hr;

		ComPtr<IAction> action;
		hr = actions->Create(TASK_ACTION_EXEC, &action.p);
		if (FAILED(hr)) return hr;

		ComPtr<IExecAction> exec;
		hr = action->QueryInterface(IID_PPV_ARGS(&exec.p));
		if (FAILED(hr)) return hr;

		{
			BStr p(exePath);
			hr = exec->put_Path(p);
			if (FAILED(hr)) return hr;
		}
		if (arguments && *arguments) {
			BStr a(arguments);
			hr = exec->put_Arguments(a);
			if (FAILED(hr)) return hr;
		}
		if (workDir && *workDir) {
			BStr w(workDir);
			hr = exec->put_WorkingDirectory(w);
			if (FAILED(hr)) return hr;
		}
		return S_OK;
	}

	// 設定適用（ご指定のチェックON/OFFとインスタンス規則）
	static HRESULT ApplySettings(ITaskDefinition* def) noexcept {
		if (!def) return E_POINTER;

		ComPtr<IPrincipal> principal;
		HRESULT hr = def->get_Principal(&principal.p);
		if (FAILED(hr)) return hr;

		// 現在ユーザー トークンで実行（ユーザーのログオン時・オンデマンド）
		hr = principal->put_LogonType(TASK_LOGON_INTERACTIVE_TOKEN);
		if (FAILED(hr)) return hr;

		// 必要に応じて実行レベル（通常権限）。管理者昇格が必要なら TASK_RUNLEVEL_HIGHEST に変更。
		// hr = principal->put_RunLevel(TASK_RUNLEVEL_LUA);
		hr = principal->put_RunLevel(TASK_RUNLEVEL_HIGHEST); // 特権でな
		if (FAILED(hr)) return hr;

		// 設定
		ComPtr<ITaskSettings> settings;
		hr = def->get_Settings(&settings.p);
		if (FAILED(hr)) return hr;

		// 「タスクを要求時に実行する」ON
		hr = settings->put_AllowDemandStart(VARIANT_TRUE);
		if (FAILED(hr)) return hr;

		// 「次の間アイドル状態の場合のみ～」OFF
		hr = settings->put_RunOnlyIfIdle(VARIANT_FALSE);
		if (FAILED(hr)) return hr;

		// AC電源のみ実行 OFF
		hr = settings->put_DisallowStartIfOnBatteries(VARIANT_FALSE);
		if (FAILED(hr)) return hr;
		hr = settings->put_StopIfGoingOnBatteries(VARIANT_FALSE);
		if (FAILED(hr)) return hr;

		// 同時実行ポリシー: 新しいインスタンスを開始しない
		hr = settings->put_MultipleInstances(TASK_INSTANCES_IGNORE_NEW);
		if (FAILED(hr)) return hr;

		// そのほか基本OFF寄せ
		hr = settings->put_StartWhenAvailable(VARIANT_FALSE);          if (FAILED(hr)) return hr;
		hr = settings->put_AllowHardTerminate(VARIANT_TRUE);           if (FAILED(hr)) return hr; // 停止は許可
		hr = settings->put_ExecutionTimeLimit(BStr(L"PT0S"));          if (FAILED(hr)) return hr; // 実行時間制限なし
		hr = settings->put_RestartCount(0);                            if (FAILED(hr)) return hr; // 再起動しない
		hr = settings->put_RestartInterval(BStr(L"PT0S"));             if (FAILED(hr)) return hr; // 無効化
		hr = settings->put_Enabled(VARIANT_TRUE);                      if (FAILED(hr)) return hr; // タスク有効
		hr = settings->put_WakeToRun(VARIANT_FALSE);                   if (FAILED(hr)) return hr;
		hr = settings->put_Priority(5);                                if (FAILED(hr)) return hr; // 通常優先度

		return S_OK;
	}

	// トリガは作らずオンデマンドのみ。
	// 参考: トリガが必要になったら ITriggerCollection->Create(...) を追加

	HRESULT RegisterTaskScheduler(const wchar_t* taskName,
		const wchar_t* exePath,
		const wchar_t* arguments,
		const wchar_t* workDir) noexcept
	{
		if (!taskName || !*taskName || !exePath || !*exePath)
			return E_INVALIDARG;

		// 実行ファイルの有無は任意で確認
		// 無効パスでも登録自体は可能だが、事故を防ぐため
		if (!::PathFileExistsW(exePath)) {
			// 存在しなくても登録したい要件があれば、このチェックを外す
			return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
		}

		ComPtr<ITaskService> svc;
		HRESULT hr = ConnectService(svc);
		if (FAILED(hr)) return hr;

		ComPtr<ITaskFolder> root;
		hr = GetRootFolder(svc.p, root);
		if (FAILED(hr)) return hr;

		// 定義を作成
		ComPtr<ITaskDefinition> def;
		{
			ComPtr<IRegistrationInfo> _;
			hr = svc->NewTask(0, &def.p);
			if (FAILED(hr)) return hr;
		}

		// 設定適用
		hr = ApplySettings(def.p);
		if (FAILED(hr)) return hr;

		// アクション
		hr = AddExecAction(def.p, exePath, arguments, workDir);
		if (FAILED(hr)) return hr;

		// 登録/更新
		BStr name(taskName);
		VARIANT vEmpty = VEmpty();
		ComPtr<IRegisteredTask> registered;

		// 現在ユーザー（パスワード不要）、インタラクティブトークン
		hr = root->RegisterTaskDefinition(
			name, def.p,
			TASK_CREATE_OR_UPDATE,
			vEmpty,               // User
			vEmpty,               // Password
			TASK_LOGON_INTERACTIVE_TOKEN,
			vEmpty,               // SDDL
			&registered.p
		);
		return hr;
	}

	bool IsExistSchedule(const wchar_t* taskName) noexcept {
		if (!taskName || !*taskName) return false;

		ComPtr<ITaskService> svc;
		if (FAILED(ConnectService(svc))) return false;

		ComPtr<ITaskFolder> root;
		if (FAILED(GetRootFolder(svc.p, root))) return false;

		ComPtr<IRegisteredTask> task;
		BStr name(taskName);
		HRESULT hr = root->GetTask(name, &task.p);
		if (SUCCEEDED(hr) && task) return true;

		// 取得失敗の多くは「存在しない」(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))。
		return false;
	}

	HRESULT RemoveTaskScheduler(const wchar_t* taskName) noexcept {
		if (!taskName || !*taskName) return E_INVALIDARG;

		ComPtr<ITaskService> svc;
		HRESULT hr = ConnectService(svc);
		if (FAILED(hr)) return hr;

		ComPtr<ITaskFolder> root;
		hr = GetRootFolder(svc.p, root);
		if (FAILED(hr)) return hr;

		BStr name(taskName);
		hr = root->DeleteTask(name, 0);

		// 存在しない場合は成功扱いにする
		if (hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)) return S_OK;
		return hr;
	}

}

