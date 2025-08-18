#include "taskman.h"
#include <taskschd.h>
#include <combaseapi.h> // SysAllocString, SysFreeString
#include <shlwapi.h>    // PathFileExistsW 等（必要なら）
#pragma comment(lib, "taskschd.lib")
#pragma comment(lib, "shlwapi.lib")

#include "ydkcomptr.h"

namespace ydk {

	struct BStr {
		BSTR b = nullptr;
		BStr() = default;
		explicit BStr(const wchar_t* s) { if (s) b = ::SysAllocString(s); }
		~BStr() { if (b) ::SysFreeString(b); }
		operator BSTR() const noexcept { return b; }
		BStr(const BStr&) = delete;
		BStr& operator=(const BStr&) = delete;
	};

	static inline VARIANT VEmpty() noexcept {
		VARIANT v; ::VariantInit(&v); return v;
	}

	// ITaskService 接続
	static HRESULT ConnectService(ComPtr<ITaskService>& svc) noexcept {
		HRESULT hr = ::CoCreateInstance(CLSID_TaskScheduler, nullptr, CLSCTX_INPROC_SERVER,
			IID_PPV_ARGS(svc.put()));
		if (FAILED(hr)) return hr;
		VARIANT vEmpty = VEmpty();
		return svc->Connect(vEmpty, vEmpty, vEmpty, vEmpty); // 現在ユーザー
	}

	// ルートフォルダ取得
	static HRESULT GetRootFolder(ITaskService* svc, ComPtr<ITaskFolder>& folder) noexcept {
		if (!svc) return E_POINTER;
		BStr root(L"\\");
		return svc->GetFolder(root, folder.put());
	}

	// アクション（実行）作成
	static HRESULT AddExecAction(ITaskDefinition* def,
		const wchar_t* exePath,
		const wchar_t* arguments,
		const wchar_t* workDir) noexcept {
		if (!def || !exePath || !*exePath) return E_INVALIDARG;

		ComPtr<IActionCollection> actions;
		HRESULT hr = def->get_Actions(actions.put());
		if (FAILED(hr)) return hr;

		ComPtr<IAction> action;
		hr = actions->Create(TASK_ACTION_EXEC, action.put());
		if (FAILED(hr)) return hr;

		ComPtr<IExecAction> exec;
		hr = action->QueryInterface(IID_PPV_ARGS(exec.put()));
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

	// 設定適用
	static HRESULT ApplySettings(ITaskDefinition* def) noexcept {
		if (!def) return E_POINTER;

		ComPtr<IPrincipal> principal;
		HRESULT hr = def->get_Principal(principal.put());
		if (FAILED(hr)) return hr;

		hr = principal->put_LogonType(TASK_LOGON_INTERACTIVE_TOKEN);
		if (FAILED(hr)) return hr;

		// 「最上位の特権で実行する」
		hr = principal->put_RunLevel(TASK_RUNLEVEL_HIGHEST);
		if (FAILED(hr)) return hr;

		ComPtr<ITaskSettings> settings;
		hr = def->get_Settings(settings.put());
		if (FAILED(hr)) return hr;

		hr = settings->put_AllowDemandStart(VARIANT_TRUE);                  if (FAILED(hr)) return hr;
		hr = settings->put_RunOnlyIfIdle(VARIANT_FALSE);                    if (FAILED(hr)) return hr;
		hr = settings->put_DisallowStartIfOnBatteries(VARIANT_FALSE);       if (FAILED(hr)) return hr;
		hr = settings->put_StopIfGoingOnBatteries(VARIANT_FALSE);           if (FAILED(hr)) return hr;
		hr = settings->put_MultipleInstances(TASK_INSTANCES_IGNORE_NEW);    if (FAILED(hr)) return hr;
		hr = settings->put_StartWhenAvailable(VARIANT_FALSE);               if (FAILED(hr)) return hr;
		hr = settings->put_AllowHardTerminate(VARIANT_TRUE);                if (FAILED(hr)) return hr;
		hr = settings->put_ExecutionTimeLimit(BStr(L"PT0S"));               if (FAILED(hr)) return hr;
		hr = settings->put_RestartCount(0);                                 if (FAILED(hr)) return hr;
		hr = settings->put_RestartInterval(BStr(L"PT0S"));                  if (FAILED(hr)) return hr;
		hr = settings->put_Enabled(VARIANT_TRUE);                           if (FAILED(hr)) return hr;
		hr = settings->put_WakeToRun(VARIANT_FALSE);                        if (FAILED(hr)) return hr;
		hr = settings->put_Priority(5);                                     if (FAILED(hr)) return hr;

		return S_OK;
	}

	// トリガ無し（オンデマンド）
	HRESULT RegisterTaskScheduler(const wchar_t* taskName,
		const wchar_t* exePath,
		const wchar_t* arguments,
		const wchar_t* workDir) noexcept
	{
		if (!taskName || !*taskName || !exePath || !*exePath)
			return E_INVALIDARG;

		if (!::PathFileExistsW(exePath)) {
			return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
		}

		ComPtr<ITaskService> svc;
		HRESULT hr = ConnectService(svc);
		if (FAILED(hr)) return hr;

		ComPtr<ITaskFolder> root;
		hr = GetRootFolder(svc.get(), root);
		if (FAILED(hr)) return hr;

		// 定義を作成
		ComPtr<ITaskDefinition> def;
		{
			ComPtr<IRegistrationInfo> _;
			hr = svc->NewTask(0, def.put());
			if (FAILED(hr)) return hr;
		}

		// 設定適用
		hr = ApplySettings(def.get());
		if (FAILED(hr)) return hr;

		// アクション
		hr = AddExecAction(def.get(), exePath, arguments, workDir);
		if (FAILED(hr)) return hr;

		// 登録/更新
		BStr name(taskName);
		VARIANT vEmpty = VEmpty();
		ComPtr<IRegisteredTask> registered;

		hr = root->RegisterTaskDefinition(
			name, def.get(),
			TASK_CREATE_OR_UPDATE,
			vEmpty,                   // User
			vEmpty,                   // Password
			TASK_LOGON_INTERACTIVE_TOKEN,
			vEmpty,                   // SDDL
			registered.put()
		);
		return hr;
	}

	bool IsExistSchedule(const wchar_t* taskName) noexcept {
		if (!taskName || !*taskName) return false;

		ComPtr<ITaskService> svc;
		if (FAILED(ConnectService(svc))) return false;

		ComPtr<ITaskFolder> root;
		if (FAILED(GetRootFolder(svc.get(), root))) return false;

		ComPtr<IRegisteredTask> task;
		BStr name(taskName);
		HRESULT hr = root->GetTask(name, task.put());
		if (SUCCEEDED(hr) && task) return true;

		return false;
	}

	HRESULT RemoveTaskScheduler(const wchar_t* taskName) noexcept {
		if (!taskName || !*taskName) return E_INVALIDARG;

		ComPtr<ITaskService> svc;
		HRESULT hr = ConnectService(svc);
		if (FAILED(hr)) return hr;

		ComPtr<ITaskFolder> root;
		hr = GetRootFolder(svc.get(), root);
		if (FAILED(hr)) return hr;

		BStr name(taskName);
		hr = root->DeleteTask(name, 0);

		if (hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)) return S_OK;
		return hr;
	}
}
