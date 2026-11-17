#pragma once

#include <string>
#include <utility>
#include <vector>

#include "Dictionary.h"
#include "Learning.h"

namespace YoruKana {

// ===== 1. 変換エンジン (Android版 ConversionEngine の移植) =====
// 候補生成順: ①辞書候補 ②カタカナ ③ひらがな原文 ④ローマ字原文。
// 末尾助詞の簡易分離変換と、前方一致の予測候補も提供する。
// 学習ストアが設定されている場合、辞書候補は学習スコア降順に並び替える
// (Android版の頻度学習と同じ考え方)。
class ConversionEngine {
public:
    ConversionEngine() = default;

    const Dictionary& GetDictionary() const { return _dictionary; }

    // 学習ストアを設定する (所有権は呼び出し側。未設定なら並び替えなし)。
    void SetLearningStore(const LearningStore* store) { _learning = store; }

    // ----- 1.1 候補生成 -----
    // reading: ひらがな読み / romajiRaw: ローマ字原文 (空でもよい)
    std::vector<std::wstring> Candidates(const std::wstring& reading,
                                         const std::wstring& romajiRaw) const;

    // ----- 1.2 予測候補 (前方一致) -----
    // prefix で始まる読みの代表候補 (各読みの先頭候補) を返す。
    std::vector<std::wstring> Predict(const std::wstring& prefix, size_t limit) const;

    // 予測候補を (読み, 候補) ペアで返す。
    std::vector<std::pair<std::wstring, std::wstring>> PredictEntries(
        const std::wstring& prefix, size_t limit) const;

private:
    // 分離変換が可能なら true (語幹 = reading の末尾1文字を除いた部分)
    bool CanSplitParticle(const std::wstring& reading) const;

    Dictionary _dictionary;
    const LearningStore* _learning = nullptr;
};

} // namespace YoruKana
