// EngineTest.cpp
// 変換エンジン (RomajiConverter / KanaUtils / Dictionary / ConversionEngine) の
// 動作テスト。開発機で g++ -std=c++17 -I YoruKana tools/EngineTest.cpp
// YoruKana/RomajiConverter.cpp YoruKana/KanaUtils.cpp YoruKana/Dictionary.cpp
// YoruKana/ConversionEngine.cpp -o EngineTest のようにビルドして確認した。
// (辞書読み数 3502 と主要変換の一致をチェックする。Windows では実行不要)
// エンジン移植の動作テスト (Linux g++ / wchar_t=UTF-32 でもロジックは同一に動く)
#include <cstdio>
#include <string>
#include <vector>

#include "ConversionEngine.h"
#include "Dictionary.h"
#include "KanaUtils.h"
#include "Learning.h"
#include "RomajiConverter.h"

static int g_checks = 0;
static int g_failures = 0;

// wstring を UTF-8 にして表示する (デバッグ用)
static std::string U8(const std::wstring& ws) {
    std::string out;
    for (wchar_t wc : ws) {
        char32_t c = static_cast<char32_t>(wc);
        if (c < 0x80) {
            out += static_cast<char>(c);
        } else if (c < 0x800) {
            out += static_cast<char>(0xC0 | (c >> 6));
            out += static_cast<char>(0x80 | (c & 0x3F));
        } else if (c < 0x10000) {
            out += static_cast<char>(0xE0 | (c >> 12));
            out += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (c & 0x3F));
        } else {
            out += static_cast<char>(0xF0 | (c >> 18));
            out += static_cast<char>(0x80 | ((c >> 12) & 0x3F));
            out += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (c & 0x3F));
        }
    }
    return out;
}

static void ExpectEq(const wchar_t* /*name*/, const std::wstring& got, const std::wstring& want) {
    g_checks++;
    if (got != want) {
        g_failures++;
        printf("FAIL: got=%s want=%s\n", U8(got).c_str(), U8(want).c_str());
    }
}

static void ExpectTrue(const char* name, bool cond) {
    g_checks++;
    if (!cond) {
        g_failures++;
        printf("FAIL %s\n", name);
    }
}

static bool Contains(const std::vector<std::wstring>& v, const std::wstring& s) {
    for (const auto& e : v)
        if (e == s) return true;
    return false;
}

int main() {
    using namespace YoruKana;

    // ローマ字変換
    ExpectEq(L"t1", RomajiConverter::Convert(L"kanji").kana, L"かんじ");
    ExpectEq(L"t2", RomajiConverter::Convert(L"gakkou").kana, L"がっこう");
    ExpectEq(L"t3", RomajiConverter::Convert(L"konnichiha").kana, L"こんにちは");
    ExpectEq(L"t4", RomajiConverter::Convert(L"k").pending, L"k");
    ExpectEq(L"t5", RomajiConverter::Convert(L"shinbun").kana, L"しんぶ");
    ExpectEq(L"t5b", RomajiConverter::ConvertFully(L"shinbun"), L"しんぶん");
    ExpectEq(L"t6", RomajiConverter::ConvertFully(L"kan"), L"かん");
    ExpectEq(L"t7", RomajiConverter::Convert(L"matcha").kana, L"まっちゃ");
    ExpectEq(L"t8", RomajiConverter::Convert(L"fa").kana, L"ふぁ");
    ExpectEq(L"t9", RomajiConverter::Convert(L"ti").kana, L"てぃ");
    ExpectEq(L"t10", RomajiConverter::Convert(L"kyou").kana, L"きょう");
    ExpectEq(L"t11", RomajiConverter::Convert(L"xtsu").kana, L"っ");
    ExpectEq(L"t12", RomajiConverter::ConvertFully(L"ltsu"), L"っ");

    // かな変換
    ExpectEq(L"k1", KanaUtils::ToKatakana(L"かんじ"), L"カンジ");
    ExpectEq(L"k2", KanaUtils::ToHiragana(L"カンジ"), L"かんじ");

    // 辞書
    Dictionary dict;
    printf("Dictionary size: %zu\n", dict.Size());
    ExpectTrue("dict size == 3502", dict.Size() == 3502);
    ExpectTrue("lookup kanji", Contains(dict.Lookup(L"かんじ"), L"漢字"));
    ExpectTrue("godan masu", Contains(dict.Lookup(L"かきます"), L"書きます"));
    ExpectTrue("godan ta", Contains(dict.Lookup(L"およいだ"), L"泳いだ"));
    ExpectTrue("iku exception", Contains(dict.Lookup(L"いった"), L"行った"));
    ExpectTrue("ichidan", Contains(dict.Lookup(L"たべます"), L"食べます"));
    ExpectTrue("iadj past", Contains(dict.Lookup(L"たかかった"), L"高かった"));
    ExpectTrue("naadj", Contains(dict.Lookup(L"しずかな"), L"静かな"));
    ExpectTrue("single kanji", Contains(dict.Lookup(L"き"), L"木"));
    // v1.2.1 辞書修正の回帰確認 (Android版と同一修正)
    ExpectTrue("heru godan ta", Contains(dict.Lookup(L"へった"), L"減った"));
    ExpectTrue("heru godan nai", Contains(dict.Lookup(L"へらない"), L"減らない"));
    ExpectTrue("heru not ichidan", !Contains(dict.Lookup(L"へた"), L"減た"));
    ExpectTrue("kimochi yoku nai", Contains(dict.Lookup(L"きもちよくない"), L"気持ちよくない"));
    ExpectTrue("kimochi not ikunai", !Contains(dict.Lookup(L"きもちいくない"), L"気持ちいくない"));

    // 変換エンジン
    ConversionEngine engine;
    auto cands = engine.Candidates(L"かんじ", L"kanji");
    ExpectTrue("cand first is kanji", !cands.empty() && cands[0] == L"漢字");
    ExpectTrue("cand has katakana", Contains(cands, L"カンジ"));
    ExpectTrue("cand has hiragana", Contains(cands, L"かんじ"));
    ExpectTrue("cand has romaji", Contains(cands, L"kanji"));
    auto split = engine.Candidates(L"がっこうは", L"gakkouha");
    ExpectTrue("particle split", Contains(split, L"学校は"));
    auto pred = engine.Predict(L"かん", 5);
    ExpectTrue("predict nonempty", !pred.empty());

    // 学習ストア
    LearningStore store;
    ExpectTrue("learn initial score 0", store.Score(L"かんじ", L"感じ") == 0);
    store.Record(L"かんじ", L"感じ");
    store.Record(L"かんじ", L"感じ");
    store.Record(L"かんじ", L"漢字");
    ExpectTrue("learn score kanji-feel", store.Score(L"かんじ", L"感じ") == 2);
    ExpectTrue("learn score kanji", store.Score(L"かんじ", L"漢字") == 1);
    std::vector<std::wstring> sorted = {L"漢字", L"感じ", L"カンジ"};
    store.SortByScore(L"かんじ", sorted);
    ExpectTrue("learn sort first", sorted[0] == L"感じ");
    ExpectTrue("learn sort second", sorted[1] == L"漢字");
    ExpectTrue("learn sort unscored last", sorted[2] == L"カンジ");
    // 永続化ラウンドトリップと不正行の読み飛ばし
    LearningStore restored;
    restored.Deserialize(store.Serialize() + L"壊れた行\n\t\t\nかんじ\t\t3\n");
    ExpectTrue("learn roundtrip", restored.Score(L"かんじ", L"感じ") == 2 &&
                                  restored.Score(L"かんじ", L"漢字") == 1);
    ExpectTrue("learn malformed skipped", restored.EntryCount() == 2);
    // エンジン連携: 学習後は頻度の高い候補が先頭に来る
    ConversionEngine learnedEngine;
    learnedEngine.SetLearningStore(&store);
    auto lcands = learnedEngine.Candidates(L"かんじ", L"kanji");
    ExpectTrue("learn engine first", !lcands.empty() && lcands[0] == L"感じ");
    ExpectTrue("learn engine katakana kept", Contains(lcands, L"カンジ"));

    // 予測候補 (予測タブ用): 接頭辞より長い読みと候補のペアが返る
    auto entries = engine.PredictEntries(L"かん", 5);
    ExpectTrue("predict entries nonempty", !entries.empty());
    bool allLonger = true;
    for (const auto& e : entries) {
        if (e.first.size() <= 2 || e.first.compare(0, 2, L"かん") != 0 || e.second.empty()) {
            allLonger = false;
        }
    }
    ExpectTrue("predict entries shape", allLonger);
    // 学習した予測は先頭に来る (代表候補は辞書形の「考える」)
    LearningStore predStore;
    for (int i = 0; i < 5; i++) predStore.Record(L"かんがえる", L"考える");
    ConversionEngine predEngine;
    predEngine.SetLearningStore(&predStore);
    auto pentries = predEngine.PredictEntries(L"かん", 10);
    ExpectTrue("predict learned first",
               !pentries.empty() && pentries[0].first == L"かんがえる" &&
                   pentries[0].second == L"考える");

    printf("checks=%d failures=%d\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
