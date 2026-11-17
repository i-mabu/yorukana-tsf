#include "RomajiConverter.h"

#include <map>

namespace YoruKana {

// ===== 1. 変換テーブル =====
namespace {

const std::map<std::wstring, std::wstring>& Table() {
    static const std::map<std::wstring, std::wstring> table = {
        // 基本 (五段)
        {L"a", L"あ"}, {L"i", L"い"}, {L"u", L"う"}, {L"e", L"え"}, {L"o", L"お"},
        {L"ka", L"か"}, {L"ki", L"き"}, {L"ku", L"く"}, {L"ke", L"け"}, {L"ko", L"こ"},
        {L"ga", L"が"}, {L"gi", L"ぎ"}, {L"gu", L"ぐ"}, {L"ge", L"げ"}, {L"go", L"ご"},
        {L"sa", L"さ"}, {L"shi", L"し"}, {L"si", L"し"}, {L"su", L"す"}, {L"se", L"せ"}, {L"so", L"そ"},
        {L"za", L"ざ"}, {L"ji", L"じ"}, {L"zi", L"じ"}, {L"zu", L"ず"}, {L"ze", L"ぜ"}, {L"zo", L"ぞ"},
        {L"ta", L"た"}, {L"chi", L"ち"}, {L"tsu", L"つ"}, {L"tu", L"つ"}, {L"te", L"て"}, {L"to", L"と"},
        {L"da", L"だ"}, {L"du", L"づ"}, {L"de", L"で"}, {L"do", L"ど"},
        {L"na", L"な"}, {L"ni", L"に"}, {L"nu", L"ぬ"}, {L"ne", L"ね"}, {L"no", L"の"},
        {L"ha", L"は"}, {L"hi", L"ひ"}, {L"fu", L"ふ"}, {L"hu", L"ふ"}, {L"he", L"へ"}, {L"ho", L"ほ"},
        {L"ba", L"ば"}, {L"bi", L"び"}, {L"bu", L"ぶ"}, {L"be", L"べ"}, {L"bo", L"ぼ"},
        {L"pa", L"ぱ"}, {L"pi", L"ぴ"}, {L"pu", L"ぷ"}, {L"pe", L"ぺ"}, {L"po", L"ぽ"},
        {L"ma", L"ま"}, {L"mi", L"み"}, {L"mu", L"む"}, {L"me", L"め"}, {L"mo", L"も"},
        {L"ya", L"や"}, {L"yu", L"ゆ"}, {L"yo", L"よ"},
        {L"ra", L"ら"}, {L"ri", L"り"}, {L"ru", L"る"}, {L"re", L"れ"}, {L"ro", L"ろ"},
        {L"wa", L"わ"}, {L"wo", L"を"},
        // 拗音
        {L"kya", L"きゃ"}, {L"kyu", L"きゅ"}, {L"kyo", L"きょ"},
        {L"gya", L"ぎゃ"}, {L"gyu", L"ぎゅ"}, {L"gyo", L"ぎょ"},
        {L"sha", L"しゃ"}, {L"shu", L"しゅ"}, {L"sho", L"しょ"},
        {L"sya", L"しゃ"}, {L"syu", L"しゅ"}, {L"syo", L"しょ"},
        {L"ja", L"じゃ"}, {L"ju", L"じゅ"}, {L"jo", L"じょ"},
        {L"jya", L"じゃ"}, {L"jyu", L"じゅ"}, {L"jyo", L"じょ"},
        {L"zya", L"じゃ"}, {L"zyu", L"じゅ"}, {L"zyo", L"じょ"},
        {L"cha", L"ちゃ"}, {L"chu", L"ちゅ"}, {L"cho", L"ちょ"},
        {L"tya", L"ちゃ"}, {L"tyu", L"ちゅ"}, {L"tyo", L"ちょ"},
        {L"cya", L"ちゃ"}, {L"cyu", L"ちゅ"}, {L"cyo", L"ちょ"},
        {L"nya", L"にゃ"}, {L"nyu", L"にゅ"}, {L"nyo", L"にょ"},
        {L"hya", L"ひゃ"}, {L"hyu", L"ひゅ"}, {L"hyo", L"ひょ"},
        {L"bya", L"びゃ"}, {L"byu", L"びゅ"}, {L"byo", L"びょ"},
        {L"pya", L"ぴゃ"}, {L"pyu", L"ぴゅ"}, {L"pyo", L"ぴょ"},
        {L"mya", L"みゃ"}, {L"myu", L"みゅ"}, {L"myo", L"みょ"},
        {L"rya", L"りゃ"}, {L"ryu", L"りゅ"}, {L"ryo", L"りょ"},
        {L"dya", L"ぢゃ"}, {L"dyu", L"ぢゅ"}, {L"dyo", L"ぢょ"},
        // 外来音 (Android版準拠: ti/di は てぃ/でぃ)
        {L"fa", L"ふぁ"}, {L"fi", L"ふぃ"}, {L"fe", L"ふぇ"}, {L"fo", L"ふぉ"}, {L"fyu", L"ふゅ"},
        {L"ti", L"てぃ"}, {L"di", L"でぃ"},
        {L"wi", L"うぃ"}, {L"we", L"うぇ"},
        {L"va", L"ゔぁ"}, {L"vi", L"ゔぃ"}, {L"vu", L"ゔ"}, {L"ve", L"ゔぇ"}, {L"vo", L"ゔぉ"},
        {L"she", L"しぇ"}, {L"je", L"じぇ"}, {L"che", L"ちぇ"},
        {L"tsa", L"つぁ"}, {L"tsi", L"つぃ"}, {L"tse", L"つぇ"}, {L"tso", L"つぉ"},
        {L"kwa", L"くぁ"}, {L"gwa", L"ぐぁ"},
        // 小書き (x / l 系)
        {L"xa", L"ぁ"}, {L"xi", L"ぃ"}, {L"xu", L"ぅ"}, {L"xe", L"ぇ"}, {L"xo", L"ぉ"},
        {L"la", L"ぁ"}, {L"li", L"ぃ"}, {L"lu", L"ぅ"}, {L"le", L"ぇ"}, {L"lo", L"ぉ"},
        {L"xya", L"ゃ"}, {L"xyu", L"ゅ"}, {L"xyo", L"ょ"},
        {L"lya", L"ゃ"}, {L"lyu", L"ゅ"}, {L"lyo", L"ょ"},
        {L"xtu", L"っ"}, {L"ltu", L"っ"}, {L"xtsu", L"っ"}, {L"ltsu", L"っ"},
        {L"xwa", L"ゎ"}, {L"lwa", L"ゎ"},
        {L"xka", L"ヵ"}, {L"xke", L"ヶ"},
    };
    return table;
}

bool IsVowel(wchar_t c) {
    return c == L'a' || c == L'i' || c == L'u' || c == L'e' || c == L'o';
}

bool IsConsonant(wchar_t c) {
    return c >= L'a' && c <= L'z' && !IsVowel(c);
}

// rest が何らかの変換キーの接頭辞なら「まだ続きがあり得る」= pending
bool IsPossiblePrefix(const std::wstring& rest) {
    if (rest.empty()) return false;
    for (const auto& kv : Table()) {
        if (kv.first.size() >= rest.size() && kv.first.compare(0, rest.size(), rest) == 0) {
            return true;
        }
    }
    return false;
}

} // namespace

// ===== 2. 変換本体 =====
// ローマ字文字列を左から変換し、確定できない末尾を pending として返す。
RomajiConverter::Result RomajiConverter::Convert(const std::wstring& input) {
    Result result;
    const size_t n = input.size();
    size_t i = 0;
    while (i < n) {
        wchar_t c = input[i];
        if (c == L'-') { result.kana += L'ー'; i++; continue; }
        if (c < L'a' || c > L'z') {
            if (c == L'\'') { i++; continue; } // 単独の ' は読み飛ばす (n' は n 側で処理)
            result.kana += c; i++; continue;
        }
        // 促音: "tch" (matcha 等)
        if (c == L't' && input.compare(i, 3, L"tch") == 0) { result.kana += L'っ'; i++; continue; }
        // 促音: 子音の重ね (n を除く)
        if (i + 1 < n && input[i + 1] == c && IsConsonant(c) && c != L'n') {
            result.kana += L'っ'; i++; continue;
        }
        // 撥音 n の処理
        if (c == L'n') {
            if (i + 1 >= n) { result.pending = L"n"; i = n; break; }
            wchar_t nx = input[i + 1];
            if (nx == L'\'') { result.kana += L'ん'; i += 2; continue; }
            if (nx == L'n') {
                if (i + 2 < n && (IsVowel(input[i + 2]) || input[i + 2] == L'y')) {
                    result.kana += L'ん'; i += 1; continue; // ん + な行 (nna → んな)
                }
                result.kana += L'ん'; i += 2; continue;
            }
            if (!IsVowel(nx) && nx != L'y') { result.kana += L'ん'; i += 1; continue; }
            // 母音 / y が続く場合はテーブル (na / nya 等) に委ねる
        }
        // テーブル最長一致 (最長キーは "xtsu" / "ltsu" の4文字)
        const std::wstring* matched = nullptr;
        size_t mlen = 0;
        size_t maxLen = (n - i < 4) ? (n - i) : 4;
        for (size_t len = maxLen; len >= 1; len--) {
            auto it = Table().find(input.substr(i, len));
            if (it != Table().end()) { matched = &it->second; mlen = len; break; }
            if (len == 1) break; // size_t のアンダーフロー防止
        }
        if (matched != nullptr) { result.kana += *matched; i += mlen; continue; }
        // 変換できない位置: 残りがキーの接頭辞なら pending、それ以外は文字を素通し
        std::wstring rest = input.substr(i);
        if (IsPossiblePrefix(rest)) { result.pending = rest; break; }
        result.kana += c; i++;
    }
    return result;
}

// ===== 3. 完全変換 (確定時) =====
// pending の末尾 n は ん に、それ以外の pending はローマ字のまま残す。
std::wstring RomajiConverter::ConvertFully(const std::wstring& input) {
    Result r = Convert(input);
    if (r.pending.empty()) return r.kana;
    if (r.pending == L"n") return r.kana + L"ん";
    return r.kana + r.pending;
}

} // namespace YoruKana
