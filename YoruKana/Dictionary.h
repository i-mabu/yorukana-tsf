#pragma once

#include <map>
#include <string>
#include <vector>

namespace YoruKana {

// ===== 1. 内蔵辞書 (Android版 Dictionary.kt の移植) =====
// DictionaryData の明示エントリに加え、動詞・形容詞の活用形を生成して登録する。
// 読み → 候補リスト のマップとして保持する。
class Dictionary {
public:
    Dictionary();

    // 読みに対する候補 (無ければ空)。単漢字フォールバックも含む。
    bool Has(const std::wstring& reading) const;
    std::vector<std::wstring> Lookup(const std::wstring& reading) const;

    // 前方一致する読みの一覧 (予測変換用)
    std::vector<std::wstring> ReadingsWithPrefix(const std::wstring& prefix) const;

    // 登録されている読みの総数 (辞書語数)。Android版と同じ 3408 になる。
    size_t Size() const { return _map.size(); }

private:
    void Add(const std::wstring& reading, const std::wstring& candidate);
    void Build();
    void GenGodan(const std::wstring& reading, const std::wstring& kanji);
    void GenIchidan(const std::wstring& reading, const std::wstring& kanji);
    void GenIAdj(const std::wstring& reading, const std::wstring& kanji);
    void GenNaAdj(const std::wstring& reading, const std::wstring& kanji);

    // std::map はキーの順序が安定する (前方一致列挙の順序が Java版 LinkedHashMap と
    // 異なるが、予測候補は頻度→長さで再ソートするため実用上の差はない)。
    std::map<std::wstring, std::vector<std::wstring>> _map;
    std::map<std::wstring, std::vector<std::wstring>> _singleKanji;
};

} // namespace YoruKana
