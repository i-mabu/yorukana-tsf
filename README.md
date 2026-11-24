# よるかな IME (TSF版) for Windows

> 📦 **このリポジトリは [i-mabu/yorukana-ime](https://github.com/i-mabu/yorukana-ime) に統合されました。** 今後の開発は yorukana-ime の `windows-tsf/` フォルダで行います。このリポジトリはアーカイブされます。

Android版「よるかな IME」の変換エンジンと辞書をそのままに、Windows の
**本物のシステムIME (TSF: Text Services Framework)** として実装した C++ プロジェクトです。
メモ帳でもブラウザでも、Windows の通常の入力欄でそのまま使える形式 (DLL) になります。

> ⚠ **このプロジェクトは開発機 (Linux) ではビルドできません。**
> あなたの Windows PC で Visual Studio 2022 を使ってビルドしてください。手順は **BUILD.md**。
> コードは TSF の公開仕様に基づくオリジナル実装で、**実機 (Windows) でのビルド・動作確認は未実施**です。

## 📋 設計原則（手動型）

yorukana は「自動で変換しない・自動で判定しない」手動型の IME です。変換の開始は Space、確定は Enter / 候補クリックなどの明示操作だけです。予測は表示されますが、確定には操作が必要です。詳しくは [MANUAL-INPUT-DESIGN.md](MANUAL-INPUT-DESIGN.md) を参照してください。

## 📋 動作仕様 (v1.2)

- **ローマ字入力のみ** (かな入力の12キーは未実装)
- 英字キーを打つと composition (未確定文字列) としてひらがなが表示される
- **Space**: 候補変換。押すたびに次の候補へ切替 (候補は composition 内に直接表示)
- **候補ウィンドウ**: 変換中は入力位置の下に候補一覧 (1ページ9件) が表示される
- **↑ / ↓**: 変換中に候補を移動 / **数字キー 1〜9**: 候補を直接選択して確定
- **予測タブ**: 変換中に **Tab** を押すと候補ウィンドウの [予測] タブへ切り替わり、
  現在の読みで始まる長い単語の候補から選べる (Tab 再押下 / Esc で [変換] タブへ戻る)。
  入力中 (変換前) も予測があればウィンドウに表示され、クリックで確定できる
- **マウスクリック**: 候補ウィンドウの候補やタブをクリックしても操作できる
- **学習機能**: 確定した候補を学習し、次回から同じ読みでは頻度の高い候補が先に来る
  (保存先: `%APPDATA%\YoruKana\learning.txt`。予測の並び順にも反映される)
- **Enter**: 確定 / **Esc**: キャンセル / **Backspace**: 1文字戻す (変換中は変換の取り消し)
- 確定時は選択範囲を末尾に畳んでから確定する (アプリ側で確定文字が選択状態に残らない)
- 入力中に別の入力欄へフォーカスを移すと、表示中の内容で確定してから切り替わる
- **Ctrl/Alt 併用キー** (Ctrl+C 等のショートカット) は IME が消費せずアプリへ素通しする
  (その際、入力中の composition は確定されずに残る)
- IME の有効/無効の切替は Windows 標準の **Win+Space** に委ねる
  (DLL は有効になっている間つねに日本語入力として振る舞う)

## ⚙️ 変換エンジン

Android版 (`~/workspace/yorukana-ime/`) の純ロジックを C++ に移植:

| C++                     | 移植元 (Kotlin)       | 内容                                                                         |
| ----------------------- | --------------------- | ---------------------------------------------------------------------------- |
| RomajiConverter.h/.cpp  | RomajiConverter.kt    | ローマ字→ひらがな (促音・撥音・拗音・外来音、未確定 pending 処理)            |
| KanaUtils.h/.cpp        | KanaUtils.kt          | ひらがな⇔カタカナ変換                                                        |
| Dictionary.h/.cpp       | Dictionary.kt         | 辞書＋動詞・形容詞の活用形生成                                               |
| DictionaryData.h        | DictionaryData.kt     | 辞書データ (機械生成。手で編集しない)                                        |
| ConversionEngine.h/.cpp | ConversionEngine.kt   | 候補生成 (辞書→カタカナ→ひらがな→ローマ字)、末尾助詞の簡易分離、前方一致予測 |
| Learning.h/.cpp         | (Android版の学習相当) | (読み, 候補) の選択頻度を数え、候補を頻度順に並び替える                      |

辞書の読み総数は **3,408** (Android版と一致を生成スクリプトで確認済み)。

`DictionaryData.h` の再生成:

```bat
python tools\generate_dictionary_data.py
```

(入力: Android版の DictionaryData.kt / 出力: YoruKana\DictionaryData.h。件数も表示されます)

## 📁 ファイル構成

```
yorukana-tsf/
├─ YoruKana.sln                  Visual Studio 2022 ソリューション
├─ BUILD.md                      ビルド〜登録〜動作確認の手順書
├─ README.md                     このファイル
├─ build-release.bat             MSBuild で Release をビルドする補助スクリプト (任意)
├─ install.bat                   DLL登録 (管理者として実行)
├─ uninstall.bat                 DLL登録解除 (管理者として実行)
├─ tools/
│  ├─ generate_dictionary_data.py  辞書データの機械変換スクリプト
│  └─ EngineTest.cpp               エンジンの動作テスト (開発機で g++ 実行・48項目確認用)
└─ YoruKana/
   ├─ YoruKana.vcxproj           DLLプロジェクト (Win32 / x64、Unicode、C++17)
   ├─ YoruKana.def               エクスポート定義 (DllGetClassObject 等)
   ├─ Guids.h                    GUID の集約場所 (下記一覧)
   ├─ Globals.h / DllMain.cpp    DllMain と COM 標準エクスポート
   ├─ ClassFactory.h/.cpp        IClassFactory 実装
   ├─ Register.h/.cpp            COM登録 + TSFプロファイル/カテゴリ登録
   ├─ TextService.h/.cpp         TIP 本体 (キー処理・composition 管理)
   ├─ EditSession.h/.cpp         ITfEditSession (編集はこの中でのみ行う)
   ├─ CandidateWindow.h/.cpp     候補ウィンドウ (自前描画ポップアップ)
   ├─ InitGuids.cpp              GUID 実体化専用の翻訳単位 (INITGUID)
   └─ (エンジン一式)              RomajiConverter / KanaUtils / Dictionary /
                                  DictionaryData.h / ConversionEngine / Learning
```

## 📝 GUID 一覧

| 名前                       | GUID                                     | 用途                                            |
| -------------------------- | ---------------------------------------- | ----------------------------------------------- |
| CLSID_TextService          | `{C92E270A-B54B-4B7B-BE78-EA19726076A7}` | テキストサービスの CLSID (COM登録・TSF登録の要) |
| GUID_Profile               | `{8E31A292-BAE5-48DC-B15D-938B785B9B71}` | 言語プロファイル (ja-JP, 0x0411)                |
| GUID_DisplayAttributeInput | `{09805E7C-7737-49C2-9A8D-DD22BC8FF2FC}` | 予約 (v1 未使用。将来の composition 表示属性用) |
| プロジェクト GUID          | `{D037F994-1323-4830-A18B-835DFFD0EEC9}` | .sln / .vcxproj 用                              |

TIP 名は「よるかな」、プロファイル説明は「よるかな (ローマ字入力)」、
カテゴリは `GUID_TFCAT_TIP_KEYBOARD` で登録します。

## ⚙️ 実装している TSF インターフェース

- `ITfTextInputProcessorEx` (ActivateEx / Deactivate)
- `ITfThreadMgrEventSink` (フォーカス変化で TextEditSink を付け替え)
- `ITfTextEditSink` (v1 では受け取るだけ)
- `ITfKeyEventSink` (OnTestKeyDown/Up で消費判定、OnKeyDown で編集セッション起動)
- `ITfCompositionSink` (OnCompositionTerminated で状態を片付け)

## 📋 既知の制限 (v1.2)

- 候補ウィンドウは自前描画ポップアップ (ITfCandidateListUIElement ではない)。
  入力位置が取得できないホストではキャレット位置に表示する
- ユーザー辞書は TSF版では未対応 (Android版のみ。候補は内蔵辞書＋学習だけから生成される)
- 入力中 (変換前) の予測はクリックでのみ選べる (キーで選びたい場合は Space で
  変換してから Tab で予測タブへ)
- かな入力 (12キー) なし、ローマ字のみ
- カーソル移動 (←→) 未対応: composing の途中編集はできない (末尾からの Backspace のみ)
- 半角/全角キーのような DLL 内モード切替なし (Win+Space で IME 自体を切り替える)
- Shift を押した英字も小文字として扱う (大文字入力はこの IME ではできない)
- 文節変換は簡易版 (全体変換＋末尾助詞分離のみ。Android版と同じ)
- composition の表示属性 (下線の色・太さ) はカスタマイズしていない
- 実機未検証: ビルドエラーや登録の不具合が残っている可能性がある (BUILD.md の確認ポイント参照)

## 📝 今後の課題

- 入力途中の予測候補の表示 (Predict の UI 連携)
- ITfCandidateListUIElement への移行、表示属性 (ITfDisplayAttributeProvider)
- かな入力、←→での文節/カーソル移動
- ARM64 構成の追加

変更履歴は [CHANGELOG.md](CHANGELOG.md) を参照してください。
