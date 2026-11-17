#include "ConversionEngine.h"

#include <algorithm>
#include <set>

#include "KanaUtils.h"

namespace YoruKana {

namespace {

// 末尾助詞として分離を試みる文字
const std::wstring kParticles = L"はがをにへとのかもねよ";

void AddUnique(std::vector<std::wstring>& out, std::set<std::wstring>& seen,
               const std::wstring& value) {
    if (seen.insert(value).second) out.push_back(value);
}

} // namespace

// ===== 1. 候補生成 =====
std::vector<std::wstring> ConversionEngine::Candidates(const std::wstring& reading,
                                                       const std::wstring& romajiRaw) const {
    std::vector<std::wstring> out;
    if (reading.empty()) return out;
    std::set<std::wstring> seen;

    std::vector<std::wstring> dict = _dictionary.Lookup(reading);
    if (dict.empty() && CanSplitParticle(reading)) {
        // 末尾助詞の簡易分離: 語幹候補 + 助詞 を提示
        std::wstring stem = reading.substr(0, reading.size() - 1);
        std::wstring particle = reading.substr(reading.size() - 1);
        for (const std::wstring& c : _dictionary.Lookup(stem)) {
            dict.push_back(c + particle);
        }
    }
    if (!dict.empty()) {
        // 学習があれば、辞書由来の候補だけを頻度順に並び替える
        // (カタカナ/ひらがな/ローマ字の定型候補は後方の定位置のまま)。
        if (_learning != nullptr) _learning->SortByScore(reading, dict);
        for (const std::wstring& c : dict) AddUnique(out, seen, c);
    }
    AddUnique(out, seen, KanaUtils::ToKatakana(reading)); // ②カタカナ
    AddUnique(out, seen, reading);                        // ③ひらがな原文
    if (!romajiRaw.empty()) {
        AddUnique(out, seen, romajiRaw);                  // ④ローマ字原文
    }
    return out;
}

bool ConversionEngine::CanSplitParticle(const std::wstring& reading) const {
    if (reading.size() < 3) return false; // 語幹2文字以上を要求
    wchar_t last = reading.back();
    if (kParticles.find(last) == std::wstring::npos) return false;
    return _dictionary.Has(reading.substr(0, reading.size() - 1));
}

// ===== 2. 予測候補 (前方一致) =====
// 並び順は Android版と同じく「学習頻度 → 読みの短さ」
// (学習ストア未設定時は頻度はすべて 0 なので読みの短さのみになる)。
std::vector<std::pair<std::wstring, std::wstring>> ConversionEngine::PredictEntries(
    const std::wstring& prefix, size_t limit) const {
    std::vector<std::pair<std::wstring, std::wstring>> out;
    if (prefix.empty() || limit == 0) return out;
    std::vector<std::wstring> readings = _dictionary.ReadingsWithPrefix(prefix);
    std::sort(readings.begin(), readings.end(),
              [this](const std::wstring& a, const std::wstring& b) {
                  if (_learning != nullptr) {
                      std::vector<std::wstring> ca = _dictionary.Lookup(a);
                      std::vector<std::wstring> cb = _dictionary.Lookup(b);
                      int sa = ca.empty() ? 0 : _learning->Score(a, ca[0]);
                      int sb = cb.empty() ? 0 : _learning->Score(b, cb[0]);
                      if (sa != sb) return sa > sb;
                  }
                  if (a.size() != b.size()) return a.size() < b.size();
                  return a < b;
              });
    std::set<std::wstring> seen;
    for (const std::wstring& r : readings) {
        std::vector<std::wstring> cands = _dictionary.Lookup(r);
        if (cands.empty()) continue;
        if (seen.insert(cands[0]).second) out.push_back({r, cands[0]});
        if (out.size() >= limit) break;
    }
    return out;
}

std::vector<std::wstring> ConversionEngine::Predict(const std::wstring& prefix,
                                                    size_t limit) const {
    std::vector<std::wstring> out;
    for (const auto& e : PredictEntries(prefix, limit)) out.push_back(e.second);
    return out;
}

} // namespace YoruKana
