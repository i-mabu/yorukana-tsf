#include "Learning.h"

#include <algorithm>

namespace YoruKana {

namespace {

// 回数として受け付ける上限 (壊れたファイルで桁あふれしないための防御)。
constexpr int kMaxScore = 1000000;

// 10進文字列を int にする。不正なら -1。
int ParseCount(const std::wstring& s) {
    if (s.empty() || s.size() > 7) return -1;
    int value = 0;
    for (wchar_t c : s) {
        if (c < L'0' || c > L'9') return -1;
        value = value * 10 + (c - L'0');
    }
    return value;
}

} // namespace

// ===== 1. 記録と参照 =====
void LearningStore::Record(const std::wstring& reading, const std::wstring& candidate) {
    if (reading.empty() || candidate.empty()) return;
    int& score = _scores[{reading, candidate}];
    if (score < kMaxScore) score++;
}

int LearningStore::Score(const std::wstring& reading, const std::wstring& candidate) const {
    auto it = _scores.find({reading, candidate});
    return (it != _scores.end()) ? it->second : 0;
}

void LearningStore::SortByScore(const std::wstring& reading,
                                std::vector<std::wstring>& candidates) const {
    if (_scores.empty() || candidates.size() < 2) return;
    std::stable_sort(candidates.begin(), candidates.end(),
                     [&](const std::wstring& a, const std::wstring& b) {
                         return Score(reading, a) > Score(reading, b);
                     });
}

// ===== 2. 永続化形式 =====
std::wstring LearningStore::Serialize() const {
    std::wstring out;
    for (const auto& kv : _scores) {
        out += kv.first.first;
        out += L'\t';
        out += kv.first.second;
        out += L'\t';
        out += std::to_wstring(kv.second);
        out += L'\n';
    }
    return out;
}

void LearningStore::Deserialize(const std::wstring& text) {
    size_t pos = 0;
    while (pos <= text.size()) {
        size_t eol = text.find(L'\n', pos);
        std::wstring line = (eol == std::wstring::npos)
                                ? text.substr(pos)
                                : text.substr(pos, eol - pos);
        pos = (eol == std::wstring::npos) ? text.size() + 1 : eol + 1;
        if (!line.empty() && line.back() == L'\r') line.pop_back();
        if (line.empty()) continue;

        size_t t1 = line.find(L'\t');
        if (t1 == std::wstring::npos) continue;
        size_t t2 = line.find(L'\t', t1 + 1);
        if (t2 == std::wstring::npos) continue;
        std::wstring reading = line.substr(0, t1);
        std::wstring candidate = line.substr(t1 + 1, t2 - t1 - 1);
        int count = ParseCount(line.substr(t2 + 1));
        if (reading.empty() || candidate.empty() || count <= 0) continue;
        if (count > kMaxScore) count = kMaxScore;
        _scores[{reading, candidate}] = count;
    }
}

} // namespace YoruKana
