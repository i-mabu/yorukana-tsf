#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
DictionaryData.kt -> DictionaryData.h 機械変換スクリプト

使い方:
    python3 tools/generate_dictionary_data.py \
        [入力 DictionaryData.kt のパス] [出力 DictionaryData.h のパス]

省略時のパス:
    入力: ~/workspace/yorukana-ime/app/src/main/java/jp/yorukana/ime/DictionaryData.kt
    出力: ~/workspace/yorukana-tsf/YoruKana/DictionaryData.h

やること:
  1. Kotlin ソースから ENTRIES / GODAN / ICHIDAN / I_ADJ / NA_ADJ / SINGLE_KANJI
     の6つの arrayOf(...) リテラルを抽出する。
  2. C++ ヘッダ (UTF-8 / L"..." ワイド文字列の std::vector<std::wstring>) として書き出す。
  3. Dictionary.kt と同じ活用生成ロジックを Python で再現し、
     登録される「読み」の総数を数えて表示する (Android 版の公称値 3,502 と照合するため)。
"""
import os
import re
import sys

DEFAULT_IN = os.path.expanduser(
    "~/workspace/yorukana-ime/app/src/main/java/jp/yorukana/ime/DictionaryData.kt")
DEFAULT_OUT = os.path.expanduser(
    "~/workspace/yorukana-tsf/YoruKana/DictionaryData.h")

ARRAYS = ["ENTRIES", "GODAN", "ICHIDAN", "I_ADJ", "NA_ADJ", "SINGLE_KANJI"]


def extract_arrays(kt_src: str):
    """各 arrayOf(...) の中の文字列リテラルだけを順序どおり取り出す。"""
    result = {}
    for name in ARRAYS:
        m = re.search(name + r":\s*Array<String>\s*=\s*arrayOf\((.*?)\n\s*\)", kt_src, re.S)
        if not m:
            raise SystemExit(f"配列 {name} が見つかりません")
        body = m.group(1)
        # コメント行を除去してから文字列リテラルを拾う
        body = re.sub(r"//[^\n]*", "", body)
        result[name] = re.findall(r'"((?:[^"\\]|\\.)*)"', body)
    return result


def cpp_escape(s: str) -> str:
    return s.replace("\\", "\\\\").replace('"', '\\"')


# ===== Dictionary.kt の活用生成ロジックの再現 (読み数カウント用) =====
GODAN_ROWS = {
    "く": ["き", "か", "け", "こ", "いた", "いて"],
    "ぐ": ["ぎ", "が", "げ", "ご", "いだ", "いで"],
    "す": ["し", "さ", "せ", "そ", "した", "して"],
    "つ": ["ち", "た", "て", "と", "った", "って"],
    "る": ["り", "ら", "れ", "ろ", "った", "って"],
    "う": ["い", "わ", "え", "お", "った", "って"],
    "ぬ": ["に", "な", "ね", "の", "んだ", "んで"],
    "ぶ": ["び", "ば", "べ", "ぼ", "んだ", "んで"],
    "む": ["み", "ま", "め", "も", "んだ", "んで"],
}


def count_readings(data) -> int:
    readings = set()

    def add(reading):
        if reading:
            readings.add(reading)

    for e in data["ENTRIES"]:
        if "=" in e:
            add(e.split("=", 1)[0])
    for v in data["GODAN"]:
        if "|" not in v:
            continue
        reading, kanji = v.split("|", 1)
        if len(reading) < 2 or len(kanji) < 2:
            continue
        row = GODAN_ROWS.get(reading[-1])
        if row is None:
            continue
        rs = reading[:-1]
        add(reading)
        add(rs + row[0] + "ます")
        add(rs + row[0] + "ました")
        add(rs + row[0] + "たい")
        add(rs + row[0] + "た")
        if reading == "いく":
            add("いった"); add("いって")
        else:
            add(rs + row[4]); add(rs + row[5])
        add(rs + row[1] + "ない")
        add(rs + row[2] + "る")
        add(rs + row[3] + "う")
    for v in data["ICHIDAN"]:
        if "|" not in v:
            continue
        reading, _kanji = v.split("|", 1)
        if len(reading) < 2 or not reading.endswith("る"):
            continue
        rs = reading[:-1]
        for suffix in ["", "ます", "ました", "た", "て", "ない", "たい", "られる", "よう"]:
            add(rs + suffix if suffix else reading)
    for v in data["I_ADJ"]:
        if "|" not in v:
            continue
        reading, _kanji = v.split("|", 1)
        if len(reading) < 2 or not reading.endswith("い"):
            continue
        rs = reading[:-1]
        add(reading)
        for suffix in ["くない", "かった", "くて", "く", "さ"]:
            add(rs + suffix)
    for v in data["NA_ADJ"]:
        if "|" not in v:
            continue
        reading, _kanji = v.split("|", 1)
        add(reading)
        for suffix in ["な", "に", "だ", "です", "だった"]:
            add(reading + suffix)
    return len(readings)


def main():
    in_path = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_IN
    out_path = sys.argv[2] if len(sys.argv) > 2 else DEFAULT_OUT
    with open(in_path, encoding="utf-8") as f:
        kt_src = f.read()
    data = extract_arrays(kt_src)

    lines = []
    lines.append("// DictionaryData.h")
    lines.append("// このファイルは tools/generate_dictionary_data.py により")
    lines.append("// Android版 DictionaryData.kt から機械生成された。手で編集しないこと。")
    lines.append("// 再生成: python3 tools/generate_dictionary_data.py")
    lines.append("#pragma once")
    lines.append("")
    lines.append("#include <string>")
    lines.append("#include <vector>")
    lines.append("")
    lines.append("namespace YoruKana {")
    lines.append("")
    var_names = {
        "ENTRIES": "kEntries",
        "GODAN": "kGodan",
        "ICHIDAN": "kIchidan",
        "I_ADJ": "kIAdj",
        "NA_ADJ": "kNaAdj",
        "SINGLE_KANJI": "kSingleKanji",
    }
    for name in ARRAYS:
        lines.append(f"// {name}: {len(data[name])} 件")
        lines.append(f"inline const std::vector<std::wstring>& {var_names[name]}() {{")
        lines.append("    static const std::vector<std::wstring> data = {")
        for item in data[name]:
            lines.append(f'        L"{cpp_escape(item)}",')
        lines.append("    };")
        lines.append("    return data;")
        lines.append("}")
        lines.append("")
    lines.append("} // namespace YoruKana")
    lines.append("")

    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    with open(out_path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines))

    total_items = sum(len(data[n]) for n in ARRAYS)
    print(f"生成: {out_path}")
    for n in ARRAYS:
        print(f"  {n}: {len(data[n])} 件")
    print(f"  データ項目合計: {total_items} 件")
    print(f"  活用生成後の読み総数 (Dictionary.kt ロジック再現): {count_readings(data)}")


if __name__ == "__main__":
    main()
