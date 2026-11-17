#pragma once

#include <string>

namespace YoruKana {

// ===== 1. ローマ字→ひらがな変換 (Android版 RomajiConverter.java の移植) =====
// composing 入力途中の未確定ローマ字も扱う: 変換済み部分 (kana) と
// まだ確定できない末尾ローマ字 (pending) を分けて返す。
class RomajiConverter {
public:
    struct Result {
        std::wstring kana;    // 変換済みのひらがな
        std::wstring pending; // 未確定の末尾ローマ字 (例: L"k", L"ky", L"n", L"sh")
    };

    // ローマ字文字列 (小文字 a-z と '-' と '\'' を想定) を左から変換する。
    static Result Convert(const std::wstring& input);

    // 完全変換 (確定時): pending の末尾 n は ん に、それ以外はローマ字のまま残す。
    static std::wstring ConvertFully(const std::wstring& input);
};

} // namespace YoruKana
