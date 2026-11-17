#pragma once

#include <string>

namespace YoruKana {

// ===== 1. かなユーティリティ (Android版 KanaUtils.java の移植) =====
// TSF版 v1 ではひらがな⇔カタカナ変換のみを使う (濁点トグル・12キー表は
// かな入力が未実装のため移植対象外。必要になったら Android版を参照)。
class KanaUtils {
public:
    static std::wstring ToKatakana(const std::wstring& s);
    static std::wstring ToHiragana(const std::wstring& s);
};

} // namespace YoruKana
