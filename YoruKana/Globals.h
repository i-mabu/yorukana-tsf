#pragma once

#include <windows.h>

// ===== 1. DLL グローバル =====
// 実体は DllMain.cpp に置く。
extern HINSTANCE g_hInstance; // この DLL のモジュールハンドル
extern LONG g_cRefDll;        // ロックカウント (COM オブジェクトの生存数)

// COM オブジェクトのコンストラクタ/デストラクタから呼ぶ。
void DllAddRef();
void DllReleaseRef();
