#pragma once

#include <windows.h>
#include <msctf.h>

#include <string>
#include <vector>

#include <utility>

#include "CandidateWindow.h"
#include "ConversionEngine.h"
#include "EditSession.h"
#include "Learning.h"

// ===== 1. テキストサービス本体 (TIP) =====
// 実装インターフェース:
//   ITfTextInputProcessorEx ... 有効化/無効化 (ActivateEx / Deactivate)
//   ITfThreadMgrEventSink ..... フォーカス変化で ITfTextEditSink を付け替える
//   ITfTextEditSink ........... ドキュメント編集の通知 (v1 では受け取るだけ)
//   ITfKeyEventSink ........... キー入力の横取りと振り分け
//   ITfCompositionSink ........ composition 終了通知で状態を片付ける
class CTextService : public ITfTextInputProcessorEx,
                     public ITfThreadMgrEventSink,
                     public ITfTextEditSink,
                     public ITfKeyEventSink,
                     public ITfCompositionSink {
public:
    CTextService();
    ~CTextService();

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppvObj) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;

    // ITfTextInputProcessor
    STDMETHODIMP Activate(ITfThreadMgr* pThreadMgr, TfClientId tfClientId) override;
    STDMETHODIMP Deactivate() override;

    // ITfTextInputProcessorEx
    STDMETHODIMP ActivateEx(ITfThreadMgr* pThreadMgr, TfClientId tfClientId,
                            DWORD dwFlags) override;

    // ITfThreadMgrEventSink
    STDMETHODIMP OnInitDocumentMgr(ITfDocumentMgr* pDocMgr) override;
    STDMETHODIMP OnUninitDocumentMgr(ITfDocumentMgr* pDocMgr) override;
    STDMETHODIMP OnSetFocus(ITfDocumentMgr* pDocMgrFocus,
                            ITfDocumentMgr* pDocMgrPrevFocus) override;
    STDMETHODIMP OnPushContext(ITfContext* pContext) override;
    STDMETHODIMP OnPopContext(ITfContext* pContext) override;

    // ITfTextEditSink
    STDMETHODIMP OnEndEdit(ITfContext* pContext, TfEditCookie ecReadOnly,
                           ITfEditRecord* pEditRecord) override;

    // ITfKeyEventSink
    STDMETHODIMP OnSetFocus(BOOL fForeground) override;
    STDMETHODIMP OnTestKeyDown(ITfContext* pContext, WPARAM wParam, LPARAM lParam,
                               BOOL* pfEaten) override;
    STDMETHODIMP OnTestKeyUp(ITfContext* pContext, WPARAM wParam, LPARAM lParam,
                             BOOL* pfEaten) override;
    STDMETHODIMP OnKeyDown(ITfContext* pContext, WPARAM wParam, LPARAM lParam,
                           BOOL* pfEaten) override;
    STDMETHODIMP OnKeyUp(ITfContext* pContext, WPARAM wParam, LPARAM lParam,
                         BOOL* pfEaten) override;
    STDMETHODIMP OnPreservedKey(ITfContext* pContext, REFGUID rguid, BOOL* pfEaten) override;

    // ITfCompositionSink
    STDMETHODIMP OnCompositionTerminated(TfEditCookie ecWrite,
                                         ITfComposition* pComposition) override;

    // 編集セッションからの実処理入口 (CEditSession が呼ぶ)
    HRESULT HandleEditSession(TfEditCookie ec, ITfContext* pContext, const KeyAction& action);

private:
    // ----- 初期化ヘルパ -----
    HRESULT InitThreadMgrEventSink();
    void UninitThreadMgrEventSink();
    HRESULT InitKeyEventSink();
    void UninitKeyEventSink();
    HRESULT InitTextEditSink(ITfContext* pContext);
    void UninitTextEditSink();

    // ----- キー判定 -----
    // このキーを IME が消費するかどうか (composing 状態に依存する)。
    bool IsKeyEaten(WPARAM wParam) const;
    // 消費するキーなら KeyAction に変換する。変換できなければ false。
    bool KeyToAction(WPARAM wParam, KeyAction& action) const;

    // ----- composition 操作 (すべて編集セッション内で呼ぶこと) -----
    HRESULT StartComposition(TfEditCookie ec, ITfContext* pContext);
    HRESULT SetCompositionText(TfEditCookie ec, ITfContext* pContext,
                               const std::wstring& text);
    void EndComposition(TfEditCookie ec);
    void ClearComposingState();

    HRESULT DoInputChar(TfEditCookie ec, ITfContext* pContext, wchar_t ch);
    HRESULT DoConvert(TfEditCookie ec, ITfContext* pContext);
    HRESULT DoSelectDelta(TfEditCookie ec, ITfContext* pContext, int delta);
    HRESULT DoToggleTab(TfEditCookie ec, ITfContext* pContext);
    HRESULT DoCommit(TfEditCookie ec, ITfContext* pContext, const std::wstring& finalText);
    HRESULT DoCommitPrediction(TfEditCookie ec, ITfContext* pContext, size_t index);
    HRESULT DoCancel(TfEditCookie ec, ITfContext* pContext);
    HRESULT DoBackspace(TfEditCookie ec, ITfContext* pContext);
    // 確定前に選択範囲を composition 末尾へ畳む (確定文字が選択状態で残る
    // アプリがあるため。EndComposition の前に呼ぶこと)。
    void CollapseSelectionToCompositionEnd(TfEditCookie ec, ITfContext* pContext);

    // ----- 候補ウィンドウ / 学習 -----
    // 現在の状態 (変換タブ / 予測タブ / 入力中予測) に合わせて候補ウィンドウを
    // 表示し直す。表示すべきものが無ければ隠す。
    void RefreshCandidateWindow(TfEditCookie ec, ITfContext* pContext);
    // 候補ウィンドウのクリックを受けたときの処理 (編集セッションを要求する)。
    void OnCandidateClicked(size_t index);
    void OnTabClicked(int tab);
    // 学習データの読み書き (%APPDATA%\YoruKana\learning.txt、UTF-8)。
    void LoadLearning();
    void SaveLearning();
    // 未確定表示文字列 (変換済みかな + 未確定ローマ字) を作る。
    std::wstring CurrentDisplayText() const;
    // 確定用文字列 (pending の n は ん になる完全変換)。
    std::wstring CurrentReading() const;

    LONG _refCount;

    ITfThreadMgr* _pThreadMgr = nullptr;
    TfClientId _clientId = 0;
    DWORD _threadMgrEventSinkCookie = TF_INVALID_COOKIE;
    DWORD _textEditSinkCookie = TF_INVALID_COOKIE;
    ITfContext* _pFocusedContext = nullptr; // TextEditSink を advise 中のコンテキスト

    ITfComposition* _pComposition = nullptr;
    ITfContext* _pCompositionContext = nullptr; // composition を所有するコンテキスト
    WPARAM _keyDownEatenVk = 0; // OnKeyDown で消費した仮想キーコード (OnKeyUp の消費判定用)
    std::wstring _romaji; // 未確定のローマ字原文
    std::wstring _currentReading; // 変換開始時点の読み (学習のキーに使う)
    std::vector<std::wstring> _candidates;
    size_t _candidateIndex = 0;
    bool _converting = false;

    // 候補ウィンドウのタブ状態と予測候補 (読み, 候補) の一覧。
    enum class ActiveTab { Convert, Predict };
    ActiveTab _activeTab = ActiveTab::Convert;
    std::vector<std::pair<std::wstring, std::wstring>> _predictions;
    size_t _predictionIndex = 0;
    bool _showingPredictions = false; // 入力中 (未変換) に予測を表示しているか

    YoruKana::ConversionEngine _engine;
    YoruKana::LearningStore _learning;
    CCandidateWindow _candidateWindow;
};
