#include <windows.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <shlwapi.h>
#include <shellapi.h>
#include <wtsapi32.h>
#include <userenv.h>
#include <tlhelp32.h>
#include <cwctype>
#include <array>
#include <string>
#include <algorithm>
#include <memory>

#include "UserProcessLauncher.h"
#include "ComInitializer.h"

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "userenv.lib")
#pragma comment(lib, "wtsapi32.lib")

namespace ydk {

	struct HandleCloser { void operator()(HANDLE h) const noexcept { if (h && h != INVALID_HANDLE_VALUE) CloseHandle(h); } };
	using unique_handle = std::unique_ptr<std::remove_pointer<HANDLE>::type, HandleCloser>;

	struct EnvBlockDeleter { void operator()(void* p) const noexcept { if (p) DestroyEnvironmentBlock(p); } };
	using unique_env = std::unique_ptr<void, EnvBlockDeleter>;

	bool IsWhiteListExt(LPCWSTR lpExt) {
		for (const auto& ext : WhiteListExt) {
			if (::_wcsicmp(lpExt, ext) == 0) return true;
		}
		return false;
	}

	inline void SetErrorOrDefault(DWORD defErr) { DWORD e = GetLastError(); SetLastError(e ? e : defErr); }

	// ---- privilege helper ----
	static bool EnablePrivilege(LPCWSTR name) {
		HANDLE hTok = nullptr;
		if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hTok)) return false;
		unique_handle tok(hTok);
		LUID luid{};
		if (!LookupPrivilegeValueW(nullptr, name, &luid)) return false;
		TOKEN_PRIVILEGES tp{}; tp.PrivilegeCount = 1; tp.Privileges[0].Luid = luid; tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
		AdjustTokenPrivileges(tok.get(), FALSE, &tp, sizeof(tp), nullptr, nullptr);
		return GetLastError() == ERROR_SUCCESS;
	}

	// ---- string utils ----
	inline std::wstring tolower_copy(std::wstring s) { std::transform(s.begin(), s.end(), s.begin(), [](wchar_t c) { return (wchar_t)std::towlower(c); }); return s; }
	inline std::wstring ext_of(const std::wstring& path) { LPCWSTR e = PathFindExtensionW(path.c_str()); return e ? tolower_copy(e) : L""; }
	inline std::wstring dirname_of(const std::wstring& path) {
		std::wstring d = path; std::replace(d.begin(), d.end(), L'/', L'\\');
		size_t pos = d.find_last_of(L'\\'); if (pos != std::wstring::npos) d.resize(pos); else d.clear(); return d;
	}
	inline std::wstring TrimSurroundingQuotes(const std::wstring& s) { return (s.size() >= 2 && s.front() == L'"' && s.back() == L'"') ? s.substr(1, s.size() - 2) : s; }
	inline std::wstring QuotePathOnly(const std::wstring& s) {
		if (s.empty()) return L"\"\"";
		if (s.find_first_of(L" \t\"") == std::wstring::npos) return s;
		std::wstring q; q.reserve(s.size() + 2); q.push_back(L'"'); q.append(s); q.push_back(L'"'); return q;
	}

	// ---- Windows 規約の引数クォート（CreateProcess/CRT 準拠）----
	inline std::wstring QuoteArgWin(const std::wstring& arg) {
		if (arg.empty()) return L"\"\"";
		bool needQuote = arg.find_first_of(L" \t\"") != std::wstring::npos;
		if (!needQuote) return arg;
		std::wstring out; out.reserve(arg.size() + 2);
		out.push_back(L'"'); size_t bs = 0;
		for (wchar_t ch : arg) {
			if (ch == L'\\') { ++bs; }
			else if (ch == L'"') { out.append(bs * 2, L'\\'); bs = 0; out.append(L"\\\""); }
			else { if (bs) { out.append(bs, L'\\'); bs = 0; } out.push_back(ch); }
		}
		if (bs) out.append(bs * 2, L'\\');
		out.push_back(L'"');
		return out;
	}
	inline std::wstring BuildCommandLine(const std::wstring& exePath, const std::wstring& rawArgs) {
		std::wstring cmd; cmd.reserve(exePath.length() + rawArgs.length() + 16);
		cmd += QuotePathOnly(exePath);
		if (!rawArgs.empty()) {
			std::wstring dummy = L"x " + rawArgs;
			int argc = 0; LPWSTR* argv = CommandLineToArgvW(dummy.c_str(), &argc);
			if (argv && argc > 1) {
				cmd += L" ";
				for (int i = 1; i < argc; ++i) { if (i > 1) cmd += L" "; cmd += QuoteArgWin(argv[i]); }
				LocalFree(argv);
			}
			else { if (argv) LocalFree(argv); cmd += L" "; cmd += rawArgs; }
		}
		return cmd;
	}

	// ---- user token acquisition (ShellWindow → WTS → explorer列挙) ----
	inline unique_handle duplicate_user_primary_token_from_explorer() {
		unique_handle snap(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
		if (!snap || snap.get() == INVALID_HANDLE_VALUE) return {};
		PROCESSENTRY32W pe{ sizeof(pe) }; if (!Process32FirstW(snap.get(), &pe)) return {};
		DWORD activeSession = WTSGetActiveConsoleSessionId();
		do {
			if (_wcsicmp(pe.szExeFile, L"explorer.exe") == 0) {
				DWORD sid = 0; if (ProcessIdToSessionId(pe.th32ProcessID, &sid) && sid == activeSession) {
					unique_handle proc(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe.th32ProcessID));
					if (!proc) continue;
					HANDLE hTok = nullptr;
					if (OpenProcessToken(proc.get(), TOKEN_DUPLICATE | TOKEN_QUERY, &hTok)) {
						unique_handle tok(hTok); HANDLE hPrimary = nullptr;
						if (DuplicateTokenEx(tok.get(),
							TOKEN_ASSIGN_PRIMARY | TOKEN_DUPLICATE | TOKEN_QUERY | TOKEN_ADJUST_DEFAULT | TOKEN_ADJUST_SESSIONID,
							nullptr, SecurityImpersonation, TokenPrimary, &hPrimary)) {
							return unique_handle(hPrimary);
						}
					}
				}
			}
		} while (Process32NextW(snap.get(), &pe));
		return {};
	}
	inline unique_handle primary_token_of_shell_user() {
		if (HWND shell = GetShellWindow()) {
			DWORD pid = 0; GetWindowThreadProcessId(shell, &pid);
			if (pid) {
				unique_handle proc(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid));
				if (proc) {
					HANDLE hTok = nullptr;
					if (OpenProcessToken(proc.get(), TOKEN_DUPLICATE | TOKEN_QUERY, &hTok)) {
						unique_handle tok(hTok); HANDLE hPrimary = nullptr;
						if (DuplicateTokenEx(tok.get(),
							TOKEN_ASSIGN_PRIMARY | TOKEN_DUPLICATE | TOKEN_QUERY | TOKEN_ADJUST_DEFAULT | TOKEN_ADJUST_SESSIONID,
							nullptr, SecurityImpersonation, TokenPrimary, &hPrimary)) {
							return unique_handle(hPrimary);
						}
					}
				}
			}
		}
		DWORD sid = WTSGetActiveConsoleSessionId();
		HANDLE hUser = nullptr;
		if (WTSQueryUserToken(sid, &hUser)) {
			unique_handle imp(hUser); HANDLE hPrimary = nullptr;
			if (DuplicateTokenEx(imp.get(),
				TOKEN_ASSIGN_PRIMARY | TOKEN_DUPLICATE | TOKEN_QUERY | TOKEN_ADJUST_DEFAULT | TOKEN_ADJUST_SESSIONID,
				nullptr, SecurityImpersonation, TokenPrimary, &hPrimary)) {
				return unique_handle(hPrimary);
			}
		}
		return duplicate_user_primary_token_from_explorer();
	}

	// ---- .lnk resolve（ShowCmd 取得） ----
	inline bool resolve_lnk(const std::wstring& lnkPath,
		std::wstring& outTarget,
		std::wstring& outArgs,
		std::wstring& outWorkDir,
		int& outShowCmd)
	{
		IShellLinkW* psl = nullptr;
		HRESULT hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&psl));
		if (FAILED(hr)) { SetLastError(ERROR_FUNCTION_FAILED); return false; }
		std::unique_ptr<IShellLinkW, void(*)(IShellLinkW*)> sl(psl, [](IShellLinkW* p) { if (p) p->Release(); });

		IPersistFile* ppf = nullptr; hr = sl->QueryInterface(IID_PPV_ARGS(&ppf));
		if (FAILED(hr)) { SetLastError(ERROR_FUNCTION_FAILED); return false; }
		std::unique_ptr<IPersistFile, void(*)(IPersistFile*)> pf(ppf, [](IPersistFile* p) { if (p) p->Release(); });

		hr = pf->Load(lnkPath.c_str(), STGM_READ);
		if (FAILED(hr)) { SetLastError(ERROR_BAD_PATHNAME); return false; }

		sl->Resolve(nullptr, SLR_NO_UI);

		wchar_t pathBuf[MAX_PATH] = {};
		wchar_t argBuf[4096] = {};
		wchar_t dirBuf[MAX_PATH] = {};
		WIN32_FIND_DATAW wfd{};

		if (SUCCEEDED(sl->GetPath(pathBuf, MAX_PATH, &wfd, SLGP_UNCPRIORITY))) outTarget.assign(pathBuf);
		sl->GetArguments(argBuf, (int)std::size(argBuf));
		sl->GetWorkingDirectory(dirBuf, MAX_PATH);

		int show = SW_SHOWNORMAL; sl->GetShowCmd(&show); outShowCmd = show;

		outArgs.assign(argBuf); outWorkDir.assign(dirBuf);
		if (outTarget.empty()) { SetLastError(ERROR_BAD_PATHNAME); return false; }
		if (outWorkDir.empty()) outWorkDir = dirname_of(outTarget);
		return true;
	}

	// ---- .url read ----
	inline bool read_url_from_urlfile(const std::wstring& urlFile, std::wstring& outUrl) {
		wchar_t buf[8192] = {};
		DWORD n = GetPrivateProfileStringW(L"InternetShortcut", L"URL", L"", buf, (DWORD)std::size(buf), urlFile.c_str());
		if (n == 0) { SetLastError(ERROR_BAD_PATHNAME); return false; }
		outUrl.assign(buf, n);
		return !outUrl.empty();
	}
	inline std::wstring associated_exe_for_scheme(std::wstring_view url) {
		size_t pos = url.find(L':');
		if (pos == std::wstring_view::npos || pos == 0) return L"";
		std::wstring scheme(url.substr(0, pos));
		wchar_t exePath[MAX_PATH] = {}; DWORD cch = MAX_PATH;
		if (SUCCEEDED(AssocQueryStringW(ASSOCF_NONE, ASSOCSTR_EXECUTABLE, scheme.c_str(), L"open", exePath, &cch)))
			return exePath;
		return L"";
	}

	// ---- ShellExecuteEx をユーザー偽装で呼ぶ共通関数 ----
	inline bool ShellExecuteAsUser(HANDLE userPrimaryToken,
		LPCWSTR file, LPCWSTR parameters,
		LPCWSTR workDir, int nShow, DWORD& outPid)
	{
		HANDLE hImpRaw = nullptr;
		if (!DuplicateTokenEx(userPrimaryToken, TOKEN_IMPERSONATE | TOKEN_QUERY, nullptr,
			SecurityImpersonation, TokenImpersonation, &hImpRaw)) {
			SetErrorOrDefault(ERROR_ACCESS_DENIED); return false;
		}
		unique_handle hImpersonation(hImpRaw);
		if (!ImpersonateLoggedOnUser(hImpersonation.get())) {
			SetErrorOrDefault(ERROR_ACCESS_DENIED); return false;
		}

		SHELLEXECUTEINFOW sei{}; sei.cbSize = sizeof(sei);
		sei.fMask = SEE_MASK_NOCLOSEPROCESS;
		sei.lpVerb = L"open";
		sei.lpFile = file;
		sei.lpParameters = parameters;
		sei.lpDirectory = workDir && *workDir ? workDir : nullptr;
		sei.nShow = nShow;

		bool ok = !!ShellExecuteExW(&sei);
		DWORD pid = 0;
		if (ok && sei.hProcess) { pid = GetProcessId(sei.hProcess); CloseHandle(sei.hProcess); }
		else if (!ok) { SetErrorOrDefault(ERROR_FILE_NOT_FOUND); }

		RevertToSelf();
		if (ok) { outPid = pid; return true; }
		return false;
	}

	// ---- 入力検証 ----
	inline bool ValidateInputPath(LPCWSTR lpExePath) {
		if (!lpExePath || !*lpExePath) { SetLastError(ERROR_INVALID_PARAMETER); return false; }

		// 1) 引用符：0個 or 偶数個かつ先頭/末尾にのみ存在
		const size_t rawLen = wcslen(lpExePath);
		size_t quoteCount = 0; for (const wchar_t* p = lpExePath; *p; ++p) if (*p == L'"') ++quoteCount;
		if (quoteCount != 0 && (quoteCount % 2 != 0 || lpExePath[0] != L'"' || lpExePath[rawLen - 1] != L'"')) {
			SetLastError(ERROR_INVALID_NAME); return false;
		}

		// 実際に扱うパス（外側の引用符を除去）
		std::wstring tmp = TrimSurroundingQuotes(lpExePath);
		const size_t len = tmp.length();
		const bool isExtended = (len >= 4 && wcsncmp(tmp.c_str(), L"\\\\?\\", 4) == 0);

		// 2) パス長
		if ((!isExtended && len >= MAX_PATH) || (isExtended && len >= 32767)) {
			SetLastError(ERROR_FILENAME_EXCED_RANGE); return false;
		}

		// 3) 危険/制御文字
		constexpr wchar_t dangerousChars[] = L"<>|*?";  // 末尾はヌル
		for (wchar_t ch : tmp) {
			if (ch < 0x20) { SetLastError(ERROR_INVALID_NAME); return false; }
			if (std::find(std::begin(dangerousChars), std::end(dangerousChars) - 1, ch) != (std::end(dangerousChars) - 1)) {
				SetLastError(ERROR_INVALID_NAME); return false;
			}
		}

		// 4) 拡張子ホワイトリスト(exe,lnk,urlだけでいいと思うんやが...)
		const std::wstring ext = ext_of(tmp);
		const bool ok = IsWhiteListExt(ext.c_str());
			//(ext == L".exe" || ext == L".lnk" || ext == L".url" ||
			//	ext == L".bat" || ext == L".cmd" || ext == L".com" ||
			//	ext == L".msi" || ext == L".scr");
		if (!ok) { SetLastError(ERROR_NOT_SUPPORTED); return false; }

		// 5) ファイルの存在確認（※ここは「入力ファイルそのもの」：.lnk/.url 自体の存在もチェック）
		DWORD attrs = GetFileAttributesW(tmp.c_str());
		if (attrs == INVALID_FILE_ATTRIBUTES || (attrs & FILE_ATTRIBUTE_DIRECTORY)) {
			SetLastError(ERROR_FILE_NOT_FOUND); return false;
		}
		return true;
	}
	// --- 環境変数展開 ---
	inline std::wstring ExpandEnvVars(const std::wstring& s) {
		DWORD need = ExpandEnvironmentStringsW(s.c_str(), nullptr, 0);
		if (need == 0) return s;
		std::wstring out(need, L'\0');
		DWORD n = ExpandEnvironmentStringsW(s.c_str(), out.data(), need);
		if (n == 0) return s;
		if (!out.empty() && out.back() == L'\0') out.pop_back();
		return out;
	}

	// --- コマンドテンプレートのプレースホルダ置換（%1, %L, %l, %u, %U, %* を扱う）---
	inline std::wstring ReplacePlaceholders(const std::wstring& tmpl, const std::wstring& urlQuoted) {
		std::wstring out; out.reserve(tmpl.size() + urlQuoted.size() + 8);
		for (size_t i = 0; i < tmpl.size(); ) {
			if (tmpl[i] == L'%' && i + 1 < tmpl.size()) {
				wchar_t t = tmpl[i + 1];
				if (t == L'1' || t == L'L' || t == L'l' || t == L'u' || t == L'U') {
					out += urlQuoted; i += 2; continue;
				}
				else if (t == L'*') {
					out += urlQuoted; i += 2; continue;
				}
			}
			out.push_back(tmpl[i++]);
		}
		// どの置換も行われなかった場合は、末尾に URL を付けて互換を確保
		if (out.find(urlQuoted) == std::wstring::npos) {
			out += L' ';
			out += urlQuoted;
		}
		return out;
	}

	// --- CommandLine 文字列から先頭実行ファイルパスを抽出 ---
	inline bool ExtractExeFromCommand(const std::wstring& cmd, std::wstring& outExe) {
		int argc = 0;
		LPWSTR* argv = CommandLineToArgvW(cmd.c_str(), &argc);
		if (!argv || argc < 1) { if (argv) LocalFree(argv); return false; }
		outExe.assign(argv[0]);
		LocalFree(argv);
		return !outExe.empty();
	}

	// --- URL スキーム → 実行 EXE と完全コマンドラインに解決 ---
	inline bool ResolveProtocolCommand(const std::wstring& url,
		std::wstring& outExe,
		std::wstring& outCmdLine,
		std::wstring& outWorkDir)
	{
		// スキームを切り出し
		size_t pos = url.find(L':');
		if (pos == std::wstring::npos || pos == 0) { SetLastError(ERROR_INVALID_PARAMETER); return false; }
		std::wstring scheme = tolower_copy(url.substr(0, pos));

		// open コマンドテンプレート取得（例: "C:\Path\App.exe" --flag "%1"）
		wchar_t buf[4096] = {};
		DWORD cch = (DWORD)std::size(buf);
		HRESULT hr = AssocQueryStringW(ASSOCF_NONE, ASSOCSTR_COMMAND, scheme.c_str(), L"open", buf, &cch);
		if (FAILED(hr) || !buf[0]) {
			SetLastError(ERROR_NOT_SUPPORTED); // 登録なし or AppX 専用など
			return false;
		}

		std::wstring tmpl = ExpandEnvVars(buf);
		// URL を Windows 規約でクォート
		std::wstring urlQ = QuoteArgWin(url);

		// プレースホルダ置換
		std::wstring fullCmd = ReplacePlaceholders(tmpl, urlQ);

		// 先頭 EXE 抽出（rundll32.exe 等も含む）
		std::wstring exe;
		if (!ExtractExeFromCommand(fullCmd, exe)) {
			SetLastError(ERROR_NOT_SUPPORTED);
			return false;
		}

		// 作業ディレクトリは EXE のあるフォルダに
		outWorkDir = dirname_of(exe);
		outExe = exe;
		outCmdLine = fullCmd;
		return true;
	}

	// --- UWP/解決不可プロトコル用: cmd.exe /c start "" URL をユーザートークンで起動 ---
	inline bool StartUrlViaCmdFallback(HANDLE userPrimaryToken, const std::wstring& url, int nShow, DWORD& outPid) {
		wchar_t sysdir[MAX_PATH] = {};
		if (!GetSystemDirectoryW(sysdir, MAX_PATH)) { SetErrorOrDefault(ERROR_PATH_NOT_FOUND); return false; }
		std::wstring cmdExe = std::wstring(sysdir) + L"\\cmd.exe";

		std::wstring cmdLine = L"\""; cmdLine += cmdExe; cmdLine += L"\" /c start \"\" ";
		cmdLine += QuoteArgWin(url);

		// CreateProcessWithTokenW でユーザー権限として起動
		STARTUPINFOW si{}; si.cb = sizeof(si);
		si.dwFlags |= STARTF_USESHOWWINDOW; si.wShowWindow = (WORD)nShow;
		PROCESS_INFORMATION pi{};
		std::wstring mutableCmd = cmdLine;

		BOOL ok = CreateProcessWithTokenW(userPrimaryToken, LOGON_WITH_PROFILE,
			nullptr, mutableCmd.data(),
			0, nullptr, nullptr, &si, &pi);
		if (!ok) {
			SetErrorOrDefault(ERROR_ACCESS_DENIED);
			return false;
		}
		outPid = pi.dwProcessId;
		if (pi.hThread) CloseHandle(pi.hThread);
		if (pi.hProcess) CloseHandle(pi.hProcess);
		return true;
	}


	// まぁこいつも汎用的に・・・か？
	bool IsWhiteListFile(LPCWSTR lpFileName) {
		if (lpFileName == nullptr) return false;
		LPCWSTR lpChar = lpFileName + std::wcslen(lpFileName);
		for (; lpChar > lpFileName; lpChar = ::CharPrevW(lpFileName, lpChar)) {
			if(*lpChar == L'.') break;
		}
		for (const auto& ext : WhiteListExt) {
			if (::_wcsicmp(lpChar, ext) == 0) return true;
		}
		return false;
	}


	// ---- 本体・・・実際に起動する時に呼ぶのはこいつだけ ----
	// 戻り値: 起動したプロセスの PID（取得不能/既存委譲時は 0）。失敗時 0。
	DWORD ShellExecuteWithLoginUser(LPCWSTR lpExePath, bool isComInitialize)
	{
		// 入力検証（.lnk / .url 自体の存在もここでチェック）
		if (!ValidateInputPath(lpExePath)) return 0;

		// COM: 関数冒頭で一度だけ
		auto com = isComInitialize ? std::make_unique<ComInitializer>() : nullptr;

		// 必要特権の明示有効化
		EnablePrivilege(SE_INCREASE_QUOTA_NAME);
		EnablePrivilege(SE_ASSIGNPRIMARYTOKEN_NAME);
		EnablePrivilege(SE_IMPERSONATE_NAME);

		// 正規化（外側の引用符を除去）と拡張子
		std::wstring src = TrimSurroundingQuotes(lpExePath);
		std::wstring ext = ext_of(src);

		// ログインユーザーのプライマリトークン
		auto hUserPrimary = primary_token_of_shell_user();
		if (!hUserPrimary) { SetLastError(ERROR_NO_TOKEN); return 0; }

		// 解析結果
		std::wstring targetPath, args, workDir;
		std::wstring url;
		int showCmd = SW_SHOWNORMAL;

		if (ext == L".lnk") {
			// .lnk → 実体・引数・作業Dir・ShowCmd を解決
			if (!resolve_lnk(src, targetPath, args, workDir, showCmd)) return 0;
			ext = ext_of(targetPath);
			// 実体の存在確認（壊れたショートカット対策）※Validate は .lnk 自体の存在を見ただけ
			DWORD attrs = GetFileAttributesW(targetPath.c_str());
			if (attrs == INVALID_FILE_ATTRIBUTES || (attrs & FILE_ATTRIBUTE_DIRECTORY)) { SetLastError(ERROR_FILE_NOT_FOUND); return 0; }
		}
		else if (ext == L".url") {
			// if (!read_url_from_urlfile(src, url)) return 0;
			// std::wstring exe = associated_exe_for_scheme(url);
			// workDir = exe.empty() ? L"" : dirname_of(exe);
			// showCmd = SW_SHOWNORMAL（ハンドラ任せ）
			if (!read_url_from_urlfile(src, url)) return 0;
			// --- ここから差し替え：スキーム解決 → 直接起動 ---
			std::wstring exe, fullCmd, workDir;
			if (ResolveProtocolCommand(url, exe, fullCmd, workDir)) {
				// 解析できた → CreateProcessWithTokenW でユーザー権限として起動
				void* envBlockRaw = nullptr;
				CreateEnvironmentBlock(&envBlockRaw, hUserPrimary.get(), FALSE);
				unique_env envBlock(envBlockRaw);

				STARTUPINFOW si{}; si.cb = sizeof(si);
				si.lpDesktop = const_cast<LPWSTR>(L"winsta0\\default");
				si.dwFlags |= STARTF_USESHOWWINDOW;
				si.wShowWindow = (WORD)showCmd;
				PROCESS_INFORMATION pi{};
				std::wstring mutableCmd = fullCmd; // 可変バッファ
				BOOL ok = CreateProcessWithTokenW(
					hUserPrimary.get(), LOGON_WITH_PROFILE,
					nullptr, mutableCmd.data(),
					CREATE_UNICODE_ENVIRONMENT,
					envBlock.get(),
					workDir.empty() ? nullptr : workDir.c_str(),
					&si, &pi);

				if (!ok) {
					// 失敗時は CreateProcessAsUserW へフォールバック
					ok = CreateProcessAsUserW(
						hUserPrimary.get(),
						nullptr, mutableCmd.data(),
						nullptr, nullptr, FALSE,
						CREATE_UNICODE_ENVIRONMENT,
						envBlock.get(),
						workDir.empty() ? nullptr : workDir.c_str(),
						&si, &pi);
				}
				if (!ok) { SetErrorOrDefault(ERROR_ACCESS_DENIED); return 0; }

				DWORD pid = pi.dwProcessId;
				if (pi.hThread) CloseHandle(pi.hThread);
				if (pi.hProcess) CloseHandle(pi.hProcess);
				return pid;
			}
			else {
				// 解決できない（AppX 等）→ ユーザー権限で cmd.exe 経由の 'start' フォールバック
				DWORD pid = 0;
				if (!StartUrlViaCmdFallback(hUserPrimary.get(), url, showCmd, pid)) return 0;
				return pid;
			}
		}
		else {
			targetPath = src;
			workDir = dirname_of(targetPath);
		}

		// --- ShellExecute 経由が望ましい種類（ダブルクリック踏襲） ---
		auto needShellExec = [](const std::wstring& e)->bool {
			return (e == L".bat" || e == L".cmd" || e == L".msi" || e == L".url"); // .url は別分岐だが便宜で含む
			};

		// --- URL（既定ハンドラへ委譲） ---
		if (!url.empty()) {
			DWORD pid = 0;
			if (!ShellExecuteAsUser(hUserPrimary.get(), url.c_str(), nullptr,
				workDir.empty() ? nullptr : workDir.c_str(),
				showCmd, pid)) {
				return 0;
			}
			return pid; // 既存インスタンス委譲時は 0 のままになり得る
		}

		// --- .bat/.cmd/.msi は ShellExecuteEx（関連付け） ---
		if (needShellExec(ext)) {
			DWORD pid = 0;
			if (!ShellExecuteAsUser(hUserPrimary.get(),
				targetPath.c_str(),
				args.empty() ? nullptr : args.c_str(),
				workDir.empty() ? nullptr : workDir.c_str(),
				showCmd, pid)) {
				return 0;
			}
			return pid;
		}

		// --- .exe/.com/.scr は CreateProcessWithTokenW で直接起動 ---
		void* envBlockRaw = nullptr;
		if (!CreateEnvironmentBlock(&envBlockRaw, hUserPrimary.get(), FALSE)) envBlockRaw = nullptr;
		unique_env envBlock(envBlockRaw);

		STARTUPINFOW si{}; si.cb = sizeof(si);
		si.lpDesktop = const_cast<LPWSTR>(L"winsta0\\default");
		si.dwFlags |= STARTF_USESHOWWINDOW; si.wShowWindow = (WORD)showCmd;

		PROCESS_INFORMATION pi{};
		const std::wstring cmdLine = BuildCommandLine(targetPath, args);
		std::wstring cmdMutable = cmdLine; // 可変バッファ

		DWORD logonFlags = LOGON_WITH_PROFILE;
		DWORD creationFlags = CREATE_UNICODE_ENVIRONMENT;

		BOOL ok = CreateProcessWithTokenW(
			hUserPrimary.get(), logonFlags,
			targetPath.c_str(),
			cmdMutable.data(),
			creationFlags,
			envBlock.get(),
			workDir.empty() ? nullptr : workDir.c_str(),
			&si, &pi);

		if (!ok) {
			ok = CreateProcessAsUserW(
				hUserPrimary.get(),
				targetPath.c_str(),
				cmdMutable.data(),
				nullptr, nullptr, FALSE,
				creationFlags,
				envBlock.get(),
				workDir.empty() ? nullptr : workDir.c_str(),
				&si, &pi);
		}
		if (!ok) { SetErrorOrDefault(ERROR_ACCESS_DENIED); return 0; }

		DWORD pid = pi.dwProcessId;
		if (pi.hThread) CloseHandle(pi.hThread);
		if (pi.hProcess) CloseHandle(pi.hProcess);
		return pid;
	}

} // namespace detail
