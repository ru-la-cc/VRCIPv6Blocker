#include <strsafe.h>
#include "blocklist.h"
#include "YDKWinUtils.h"

bool BlockList::LoadFromFile(LPCWSTR lpFileName, ydk::ILogger<WCHAR>* logger) {
	m_BlockList.clear();
	logger->Log(L"IPv6ブロックリストの読込...");
	auto pf = ydk::OpenReadFile(lpFileName); // なければ空ファイルを作るのは意図通り
	if (!pf) {
		logger->LogError((std::wstring(lpFileName) + L"を開けません").c_str());
		return false;
	}

	char buf[256];
	bool isSkip, isRead;
	char* ps = buf;
	while (std::fgets(buf, sizeof(buf), pf) != nullptr) {
		isSkip = isRead = false;
		for (char* p = buf; *p; ++p) {
			if (*p == ' ' || *p == '\t') continue;
			if (*p == '#') {
				if (isRead) {
					*p = '\0';
					break;
				}
				isSkip = true;
				break;
			}
			if (!isRead &&
				(std::isxdigit(*p) ||
					*p == ':' ||
					*p == '/' ||
					*p == ',' ||
					*p == '-' ||
					*p == '.')) {
				isRead = true;
				ps = p;
				continue;
			}
			if (*p == '\r' || *p == '\n') {
				*p = '\0';
				break;
			}
		}
		if (isSkip) continue;
		char* pt;
		for (pt = ps + std::strlen(ps) - 1; pt > ps; --pt) {
			if (*pt == ' ' || *pt == '\t') continue;
			break;
		}
		*(pt + 1) = '\0';
		if (*ps) {
			WCHAR szRule[256];
			ydk::ToUtf16(ps, szRule, std::size(szRule));
			m_BlockList.push_back(szRule);
		}
	}
	bool result = false;
	if (std::ferror(pf)) {
		logger->LogError(L"ブロックリストの読込中にエラーが発生しました");
	} else {
		WCHAR szLog[256];
		::StringCchPrintfW(szLog, std::size(szLog), L"ブロックリスト有効件数 : %llu", m_BlockList.size());
		logger->Log(szLog);
		result = true;
	}
	std::fclose(pf);
	return result;
}
