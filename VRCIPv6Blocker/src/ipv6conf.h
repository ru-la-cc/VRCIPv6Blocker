#pragma once

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <string>

// CoInitialize済み前提

namespace ydk {
	// アダプタ識別キー
	struct AdapterKey {
		GUID         ifGuid{};            // NDIS IfGuid
		ULONG        ifIndex = 0;         // IF インデックス
		std::wstring friendly;            // FriendlyName（任意）
		bool valid() const noexcept;
	};

	// 宛先IP文字列（IPv4/IPv6）に基づき、インターネット到達に使うアダプタを決定
	// destIpStrがnullptrまたはパース失敗時は8.8.8.8にフォールバック
	HRESULT ResolveInternetAdapterFromString(LPCWSTR destIpStr, AdapterKey& out);

	// 既定宛先（8.8.8.8）で自動判定
	HRESULT ResolveInternetAdapter(AdapterKey& out);

	// IPv6のチェック状態（true=有効）エラー時はfalseを返し、*phResultにHRESULTを格納
	// phResult==nullptrの場合はエラー情報を格納しない
	bool IsIPv6Enable(const AdapterKey& key, HRESULT* phResult);

	// IPv6 の有効/無効切替（アダプタ単位）
	// pKeyがnullptrまたは無効ならdestIpStrを使って対象アダプタを自動判定（nullptr/パース失敗は8.8.8.8）
	// 戻り値:S_OK=成功 / S_FALSE=対象パスなし / その他=HRESULT エラー
	HRESULT SetIPv6Enable(bool enable,
		const AdapterKey* pKey /*nullable*/,
		LPCWSTR destIpStr /*nullable*/ = nullptr);
}
