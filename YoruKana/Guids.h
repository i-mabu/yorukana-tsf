#pragma once

#include <windows.h>

// ===== 1. GUID 一覧 (よるかな TSF版で使う GUID はすべてここに集約する) =====
// CLSID_TextService        : {C92E270A-B54B-4B7B-BE78-EA19726076A7}
// GUID_Profile             : {8E31A292-BAE5-48DC-B15D-938B785B9B71}
// GUID_DisplayAttributeInput (予約: v1 では未使用。将来の composition 表示属性用)
//                          : {09805E7C-7737-49C2-9A8D-DD22BC8FF2FC}
//
// DEFINE_GUID は INITGUID が定義された翻訳単位 (InitGuids.cpp) でのみ実体化し、
// それ以外では extern 宣言になる。

// ----- 1.1 テキストサービスの CLSID -----
DEFINE_GUID(CLSID_TextService,
    0xc92e270a, 0xb54b, 0x4b7b, 0xbe, 0x78, 0xea, 0x19, 0x72, 0x60, 0x76, 0xa7);

// ----- 1.2 言語プロファイルの GUID -----
DEFINE_GUID(GUID_Profile,
    0x8e31a292, 0xbae5, 0x48dc, 0xb1, 0x5d, 0x93, 0x8b, 0x78, 0x5b, 0x9b, 0x71);

// ----- 1.3 表示属性の GUID (予約) -----
DEFINE_GUID(GUID_DisplayAttributeInput,
    0x09805e7c, 0x7737, 0x49c2, 0x9a, 0x8d, 0xdd, 0x22, 0xbc, 0x8f, 0xf2, 0xfc);

namespace YoruKana {

// ===== 2. 定数 =====
// TIP の表示名 (設定アプリのキーボード一覧などに表示される)
inline constexpr const wchar_t* kTipDescription = L"よるかな";
// プロファイルの説明文 (AddLanguageProfile に渡す)
inline constexpr const wchar_t* kProfileDescription = L"よるかな (ローマ字入力)";
// 対象言語: 日本語 (ja-JP)
inline constexpr LANGID kJapaneseLangId = 0x0411;

} // namespace YoruKana
