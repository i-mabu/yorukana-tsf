#pragma once

#include <windows.h>

// ===== 1. 登録処理 (DllRegisterServer / DllUnregisterServer から呼ばれる) =====
// COM サーバ登録 (HKCR\CLSID) + TSF プロファイル登録 + カテゴリ登録をまとめて行う。
HRESULT RegisterTextService();
HRESULT UnregisterTextService();
