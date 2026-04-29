#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include "ILogger.h"

class BlockList final {
public:
	BlockList() = default;
	~BlockList() = default;
	BlockList(const BlockList&) = delete;
	BlockList& operator=(const BlockList&) = delete;
	BlockList(BlockList&&) = delete;
	BlockList& operator=(BlockList&&) = delete;
	bool LoadFromFile(LPCWSTR lpFileName, ydk::ILogger<WCHAR>* logger);
	const std::vector<std::wstring>& GetBlockList() const { return m_BlockList; }
private:
	std::vector<std::wstring> m_BlockList;
};
