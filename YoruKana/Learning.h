#pragma once

#include <map>
#include <string>
#include <utility>
#include <vector>

namespace YoruKana {

// ===== 1. 学習ストア =====
// (読み, 候補) ごとの選択回数を数え、候補の並び替えに使う。
// Android版の学習機能 (頻度学習) の TSF 版。候補生成時にスコア降順で
// 安定ソートすることで、よく選ぶ候補が先頭に来るようにする。
//
// 永続化は Serialize / Deserialize のテキスト形式 (1行 = 読み \t 候補 \t 回数)。
// ファイル入出力と UTF-8 変換は呼び出し側 (TextService) が担うため、
// このクラス自体はプラットフォーム非依存で単体テストできる。
class LearningStore {
public:
    // 候補が選択・確定されたことを記録する。
    void Record(const std::wstring& reading, const std::wstring& candidate);

    // 記録済みの選択回数 (未記録は 0)。
    int Score(const std::wstring& reading, const std::wstring& candidate) const;

    // 候補リストを学習スコア降順に安定ソートする (同点は元の順序を維持)。
    void SortByScore(const std::wstring& reading,
                     std::vector<std::wstring>& candidates) const;

    // 永続化形式への書き出し / 読み込み。不正な行は読み飛ばす。
    std::wstring Serialize() const;
    void Deserialize(const std::wstring& text);

    size_t EntryCount() const { return _scores.size(); }

private:
    std::map<std::pair<std::wstring, std::wstring>, int> _scores;
};

} // namespace YoruKana
