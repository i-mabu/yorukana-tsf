#pragma once

#include <windows.h>
#include <msctf.h>

class CTextService;

// ===== 1. キーアクション =====
// キーイベントシンクが判定した「何をすべきか」を編集セッションへ渡すための構造体。
enum class KeyActionType {
    InputChar,       // ローマ字1文字を追記する (ch が対象文字)
    Convert,         // Space: 候補変換 / 次候補へ
    SelectNext,      // ↓: 次の候補を選ぶ (変換中のみ)
    SelectPrev,      // ↑: 前の候補を選ぶ (変換中のみ)
    ToggleTab,       // Tab: 候補ウィンドウの [変換] / [予測] タブを切り替える
    Commit,          // Enter: 現在の表示で確定する
    CommitCandidate, // 数字キー / 候補ウィンドウのクリック: candidateIndex 番目の候補で確定する
    CommitPrediction, // 予測タブ / 入力中予測のクリック: candidateIndex 番目の予測で確定する
    Cancel,          // Esc: composition を破棄する
    Backspace,       // BS: composing を1文字戻す / 変換中なら変換を取り消す
};

struct KeyAction {
    KeyActionType type = KeyActionType::InputChar;
    wchar_t ch = 0;         // InputChar のときの文字 ('a'-'z' または '-')
    int candidateIndex = 0; // CommitCandidate のときの候補番号 (0始まり)
};

// ===== 2. 編集セッション =====
// TSF のドキュメント編集は ITfEditSession::DoEditSession の中でのみ許可される。
// キー処理はすべてこのセッション経由で CTextService::HandleEditSession を呼ぶ。
class CEditSession : public ITfEditSession {
public:
    CEditSession(CTextService* pService, ITfContext* pContext, const KeyAction& action);
    ~CEditSession();

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppvObj) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;

    // ITfEditSession
    STDMETHODIMP DoEditSession(TfEditCookie ec) override;

private:
    LONG _refCount;
    CTextService* _pService; // AddRef して保持する
    ITfContext* _pContext;   // AddRef して保持する
    KeyAction _action;
};
