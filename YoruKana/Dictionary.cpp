#include "Dictionary.h"

#include <algorithm>
#include <map>

#include "DictionaryData.h"

namespace YoruKana {

namespace {

// 文字列を区切り文字で分割する (Java の String.split の単純版。空要素は含めない)。
std::vector<std::wstring> Split(const std::wstring& s, wchar_t delim) {
    std::vector<std::wstring> out;
    size_t start = 0;
    while (start <= s.size()) {
        size_t pos = s.find(delim, start);
        if (pos == std::wstring::npos) {
            out.push_back(s.substr(start));
            break;
        }
        out.push_back(s.substr(start, pos - start));
        start = pos + 1;
    }
    return out;
}

// 五段動詞の活用行: 語尾 → {い段, あ段, え段, お段, た形語尾, て形語尾}
const std::map<wchar_t, std::vector<std::wstring>>& GodanRows() {
    static const std::map<wchar_t, std::vector<std::wstring>> rows = {
        {L'く', {L"き", L"か", L"け", L"こ", L"いた", L"いて"}},
        {L'ぐ', {L"ぎ", L"が", L"げ", L"ご", L"いだ", L"いで"}},
        {L'す', {L"し", L"さ", L"せ", L"そ", L"した", L"して"}},
        {L'つ', {L"ち", L"た", L"て", L"と", L"った", L"って"}},
        {L'る', {L"り", L"ら", L"れ", L"ろ", L"った", L"って"}},
        {L'う', {L"い", L"わ", L"え", L"お", L"った", L"って"}},
        {L'ぬ', {L"に", L"な", L"ね", L"の", L"んだ", L"んで"}},
        {L'ぶ', {L"び", L"ば", L"べ", L"ぼ", L"んだ", L"んで"}},
        {L'む', {L"み", L"ま", L"め", L"も", L"んだ", L"んで"}},
    };
    return rows;
}

bool EndsWith(const std::wstring& s, const std::wstring& suffix) {
    return s.size() >= suffix.size() &&
           s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

} // namespace

Dictionary::Dictionary() {
    Build();
}

// ===== 1. 構築 =====
void Dictionary::Add(const std::wstring& reading, const std::wstring& candidate) {
    if (reading.empty() || candidate.empty()) return;
    std::vector<std::wstring>& list = _map[reading];
    if (std::find(list.begin(), list.end(), candidate) == list.end()) {
        list.push_back(candidate);
    }
}

void Dictionary::Build() {
    for (const std::wstring& e : kEntries()) {
        size_t eq = e.find(L'=');
        if (eq == std::wstring::npos) continue;
        std::wstring reading = e.substr(0, eq);
        for (const std::wstring& cand : Split(e.substr(eq + 1), L',')) Add(reading, cand);
    }
    for (const std::wstring& v : kGodan()) {
        size_t bar = v.find(L'|');
        if (bar == std::wstring::npos) continue;
        GenGodan(v.substr(0, bar), v.substr(bar + 1));
    }
    for (const std::wstring& v : kIchidan()) {
        size_t bar = v.find(L'|');
        if (bar == std::wstring::npos) continue;
        GenIchidan(v.substr(0, bar), v.substr(bar + 1));
    }
    for (const std::wstring& a : kIAdj()) {
        size_t bar = a.find(L'|');
        if (bar == std::wstring::npos) continue;
        GenIAdj(a.substr(0, bar), a.substr(bar + 1));
    }
    for (const std::wstring& a : kNaAdj()) {
        size_t bar = a.find(L'|');
        if (bar == std::wstring::npos) continue;
        GenNaAdj(a.substr(0, bar), a.substr(bar + 1));
    }
    for (const std::wstring& e : kSingleKanji()) {
        size_t eq = e.find(L'=');
        if (eq == std::wstring::npos) continue;
        _singleKanji[e.substr(0, eq)] = Split(e.substr(eq + 1), L',');
    }
}

void Dictionary::GenGodan(const std::wstring& reading, const std::wstring& kanji) {
    if (reading.size() < 2 || kanji.size() < 2) return;
    auto it = GodanRows().find(reading.back());
    if (it == GodanRows().end()) return;
    const std::vector<std::wstring>& row = it->second;
    std::wstring rs = reading.substr(0, reading.size() - 1); // 読みの語幹
    std::wstring ks = kanji.substr(0, kanji.size() - 1);     // 漢字形の語幹
    Add(reading, kanji);                                     // 辞書形
    Add(rs + row[0] + L"ます", ks + row[0] + L"ます");       // ます形
    Add(rs + row[0] + L"ました", ks + row[0] + L"ました");   // ました
    Add(rs + row[0] + L"たい", ks + row[0] + L"たい");       // たい
    Add(rs + row[0] + L"た", ks + row[0] + L"た");           // 連用 (た の前段)
    std::wstring taR, teR, taK, teK;
    if (reading == L"いく") { // 行く は例外 (いった/いって)
        taR = L"いった"; teR = L"いって"; taK = L"行った"; teK = L"行って";
    } else {
        taR = rs + row[4]; teR = rs + row[5]; taK = ks + row[4]; teK = ks + row[5];
    }
    Add(taR, taK);                                           // た形
    Add(teR, teK);                                           // て形
    Add(rs + row[1] + L"ない", ks + row[1] + L"ない");       // ない形
    Add(rs + row[2] + L"る", ks + row[2] + L"る");           // 可能形
    Add(rs + row[3] + L"う", ks + row[3] + L"う");           // 意向形
}

void Dictionary::GenIchidan(const std::wstring& reading, const std::wstring& kanji) {
    if (reading.size() < 2 || !EndsWith(reading, L"る")) return;
    std::wstring rs = reading.substr(0, reading.size() - 1);
    std::wstring ks = kanji.substr(0, kanji.size() - 1);
    Add(reading, kanji);                                     // 辞書形
    Add(rs + L"ます", ks + L"ます");
    Add(rs + L"ました", ks + L"ました");
    Add(rs + L"た", ks + L"た");                             // た形
    Add(rs + L"て", ks + L"て");                             // て形
    Add(rs + L"ない", ks + L"ない");                         // ない形
    Add(rs + L"たい", ks + L"たい");                         // たい
    Add(rs + L"られる", ks + L"られる");                     // 可能/受身
    Add(rs + L"よう", ks + L"よう");                         // 意向形
}

void Dictionary::GenIAdj(const std::wstring& reading, const std::wstring& kanji) {
    if (reading.size() < 2 || !EndsWith(reading, L"い")) return;
    std::wstring rs = reading.substr(0, reading.size() - 1);
    std::wstring ks = kanji.substr(0, kanji.size() - 1);
    Add(reading, kanji);                                     // 終止形
    Add(rs + L"くない", ks + L"くない");                     // 否定
    Add(rs + L"かった", ks + L"かった");                     // 過去
    Add(rs + L"くて", ks + L"くて");                         // て形
    Add(rs + L"く", ks + L"く");                             // 連用
    Add(rs + L"さ", ks + L"さ");                             // 名詞化 (高さ等)
}

void Dictionary::GenNaAdj(const std::wstring& reading, const std::wstring& kanji) {
    Add(reading, kanji);                                     // 語幹
    Add(reading + L"な", kanji + L"な");                     // 連体
    Add(reading + L"に", kanji + L"に");                     // 連用
    Add(reading + L"だ", kanji + L"だ");                     // 終止
    Add(reading + L"です", kanji + L"です");                 // 丁寧
    Add(reading + L"だった", kanji + L"だった");             // 過去
}

// ===== 2. 検索 =====
bool Dictionary::Has(const std::wstring& reading) const {
    return _map.find(reading) != _map.end();
}

std::vector<std::wstring> Dictionary::Lookup(const std::wstring& reading) const {
    std::vector<std::wstring> out;
    auto it = _map.find(reading);
    if (it != _map.end()) out = it->second;
    if (out.empty() && reading.size() == 1) {
        auto sk = _singleKanji.find(reading);
        if (sk != _singleKanji.end()) out = sk->second;
    }
    return out;
}

std::vector<std::wstring> Dictionary::ReadingsWithPrefix(const std::wstring& prefix) const {
    std::vector<std::wstring> out;
    if (prefix.empty()) return out;
    for (const auto& kv : _map) {
        if (kv.first.size() > prefix.size() &&
            kv.first.compare(0, prefix.size(), prefix) == 0) {
            out.push_back(kv.first);
        }
    }
    return out;
}

} // namespace YoruKana
