#include "KanaUtils.h"

namespace YoruKana {

// ===== 1. ひらがな ⇔ カタカナ =====
// ぁ(0x3041)〜ゖ(0x3096) と ァ(0x30A1)〜ヶ(0x30F6) は 0x60 差で対応する。
// ※ Windows の wchar_t は16bit (UTF-16) のため、かなはすべて BMP 内で
//    1文字 = 1 wchar_t として扱える。

std::wstring KanaUtils::ToKatakana(const std::wstring& s) {
    std::wstring out = s;
    for (size_t i = 0; i < out.size(); i++) {
        wchar_t c = out[i];
        if (c >= L'ぁ' && c <= L'ゖ') out[i] = static_cast<wchar_t>(c + 0x60);
    }
    return out;
}

std::wstring KanaUtils::ToHiragana(const std::wstring& s) {
    std::wstring out = s;
    for (size_t i = 0; i < out.size(); i++) {
        wchar_t c = out[i];
        if (c >= L'ァ' && c <= L'ヶ') out[i] = static_cast<wchar_t>(c - 0x60);
    }
    return out;
}

} // namespace YoruKana
