#include "TextService.h"

#include <new>

#include "Globals.h"
#include "Guids.h"
#include "RomajiConverter.h"

namespace {

// Ctrl / Alt のどちらかが押されているか。
// 押されている間のキーはアプリ側のショートカット (Ctrl+C 等) として
// 素通しするために判定する。
bool IsCtrlOrAltDown() {
    return (GetKeyState(VK_CONTROL) < 0) || (GetKeyState(VK_MENU) < 0);
}

// 学習ファイルのパス (%APPDATA%\YoruKana\learning.txt)。取得できなければ空。
std::wstring LearningFilePath() {
    wchar_t buf[MAX_PATH] = {};
    DWORD len = GetEnvironmentVariableW(L"APPDATA", buf, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) return L"";
    return std::wstring(buf, len) + L"\\YoruKana\\learning.txt";
}

std::string WideToUtf8(const std::wstring& s) {
    if (s.empty()) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()),
                                  nullptr, 0, nullptr, nullptr);
    if (len <= 0) return "";
    std::string out(static_cast<size_t>(len), '\0');
    WideCharToMultiByte(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), out.data(),
                        len, nullptr, nullptr);
    return out;
}

std::wstring Utf8ToWide(const std::string& s) {
    if (s.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()),
                                  nullptr, 0);
    if (len <= 0) return L"";
    std::wstring out(static_cast<size_t>(len), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), out.data(),
                        len);
    return out;
}

} // namespace

// ===== 1. 生成・破棄 =====
CTextService::CTextService() : _refCount(1) {
    DllAddRef();
    _engine.SetLearningStore(&_learning);
}

CTextService::~CTextService() {
    if (_pComposition != nullptr) {
        _pComposition->Release();
        _pComposition = nullptr;
    }
    if (_pCompositionContext != nullptr) {
        _pCompositionContext->Release();
        _pCompositionContext = nullptr;
    }
    if (_pFocusedContext != nullptr) {
        _pFocusedContext->Release();
        _pFocusedContext = nullptr;
    }
    if (_pThreadMgr != nullptr) {
        _pThreadMgr->Release();
        _pThreadMgr = nullptr;
    }
    DllReleaseRef();
}

// ===== 2. IUnknown =====
STDMETHODIMP CTextService::QueryInterface(REFIID riid, void** ppvObj) {
    if (ppvObj == nullptr) return E_INVALIDARG;
    *ppvObj = nullptr;

    if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfTextInputProcessor) ||
        IsEqualIID(riid, IID_ITfTextInputProcessorEx)) {
        *ppvObj = static_cast<ITfTextInputProcessorEx*>(this);
    } else if (IsEqualIID(riid, IID_ITfThreadMgrEventSink)) {
        *ppvObj = static_cast<ITfThreadMgrEventSink*>(this);
    } else if (IsEqualIID(riid, IID_ITfTextEditSink)) {
        *ppvObj = static_cast<ITfTextEditSink*>(this);
    } else if (IsEqualIID(riid, IID_ITfKeyEventSink)) {
        *ppvObj = static_cast<ITfKeyEventSink*>(this);
    } else if (IsEqualIID(riid, IID_ITfCompositionSink)) {
        *ppvObj = static_cast<ITfCompositionSink*>(this);
    } else {
        return E_NOINTERFACE;
    }
    AddRef();
    return S_OK;
}

STDMETHODIMP_(ULONG) CTextService::AddRef() {
    return static_cast<ULONG>(InterlockedIncrement(&_refCount));
}

STDMETHODIMP_(ULONG) CTextService::Release() {
    LONG count = InterlockedDecrement(&_refCount);
    if (count == 0) {
        delete this;
        return 0;
    }
    return static_cast<ULONG>(count);
}

// ===== 3. 有効化 / 無効化 =====
STDMETHODIMP CTextService::Activate(ITfThreadMgr* pThreadMgr, TfClientId tfClientId) {
    return ActivateEx(pThreadMgr, tfClientId, 0);
}

STDMETHODIMP CTextService::ActivateEx(ITfThreadMgr* pThreadMgr, TfClientId tfClientId,
                                      DWORD /*dwFlags*/) {
    if (pThreadMgr == nullptr) return E_INVALIDARG;
    _pThreadMgr = pThreadMgr;
    _pThreadMgr->AddRef();
    _clientId = tfClientId;

    HRESULT hr = InitThreadMgrEventSink();
    if (FAILED(hr)) goto Fail;
    hr = InitKeyEventSink();
    if (FAILED(hr)) goto Fail;

    // 候補ウィンドウと学習データを用意する (失敗しても入力自体は続行する)。
    if (_candidateWindow.Create(g_hInstance)) {
        _candidateWindow.OnItemClicked = [this](size_t index) { OnCandidateClicked(index); };
        _candidateWindow.OnTabClicked = [this](int tab) { OnTabClicked(tab); };
    }
    LoadLearning();
    return S_OK;

Fail:
    Deactivate();
    return hr;
}

STDMETHODIMP CTextService::Deactivate() {
    UninitKeyEventSink();
    UninitTextEditSink();
    UninitThreadMgrEventSink();

    _candidateWindow.DestroyWindowHandle();

    if (_pComposition != nullptr) {
        // Deactivate 中は編集セッションを持てないため、EndComposition は呼べず
        // 参照を放すだけにする (composition 自体は TSF 側で終了する)。
        _pComposition->Release();
        _pComposition = nullptr;
    }
    if (_pCompositionContext != nullptr) {
        _pCompositionContext->Release();
        _pCompositionContext = nullptr;
    }
    ClearComposingState();
    _keyDownEatenVk = 0;

    if (_pThreadMgr != nullptr) {
        _pThreadMgr->Release();
        _pThreadMgr = nullptr;
    }
    _clientId = 0;
    return S_OK;
}

// ----- 3.1 シンク初期化 -----
HRESULT CTextService::InitThreadMgrEventSink() {
    ITfSource* pSource = nullptr;
    HRESULT hr = _pThreadMgr->QueryInterface(IID_ITfSource, reinterpret_cast<void**>(&pSource));
    if (FAILED(hr) || pSource == nullptr) return FAILED(hr) ? hr : E_FAIL;
    hr = pSource->AdviseSink(IID_ITfThreadMgrEventSink,
                             static_cast<ITfThreadMgrEventSink*>(this),
                             &_threadMgrEventSinkCookie);
    pSource->Release();
    return hr;
}

void CTextService::UninitThreadMgrEventSink() {
    if (_pThreadMgr == nullptr || _threadMgrEventSinkCookie == TF_INVALID_COOKIE) return;
    ITfSource* pSource = nullptr;
    if (SUCCEEDED(_pThreadMgr->QueryInterface(IID_ITfSource,
                                              reinterpret_cast<void**>(&pSource))) &&
        pSource != nullptr) {
        pSource->UnadviseSink(_threadMgrEventSinkCookie);
        pSource->Release();
    }
    _threadMgrEventSinkCookie = TF_INVALID_COOKIE;
}

HRESULT CTextService::InitKeyEventSink() {
    ITfKeystrokeMgr* pKeystrokeMgr = nullptr;
    HRESULT hr = _pThreadMgr->QueryInterface(IID_ITfKeystrokeMgr,
                                             reinterpret_cast<void**>(&pKeystrokeMgr));
    if (FAILED(hr) || pKeystrokeMgr == nullptr) return FAILED(hr) ? hr : E_FAIL;
    // fForeground = TRUE: フォアグラウンドの TIP としてキーを受け取る。
    hr = pKeystrokeMgr->AdviseKeyEventSink(_clientId, static_cast<ITfKeyEventSink*>(this),
                                           TRUE);
    pKeystrokeMgr->Release();
    return hr;
}

void CTextService::UninitKeyEventSink() {
    if (_pThreadMgr == nullptr) return;
    ITfKeystrokeMgr* pKeystrokeMgr = nullptr;
    if (SUCCEEDED(_pThreadMgr->QueryInterface(IID_ITfKeystrokeMgr,
                                              reinterpret_cast<void**>(&pKeystrokeMgr))) &&
        pKeystrokeMgr != nullptr) {
        pKeystrokeMgr->UnadviseKeyEventSink(_clientId);
        pKeystrokeMgr->Release();
    }
}

HRESULT CTextService::InitTextEditSink(ITfContext* pContext) {
    if (pContext == nullptr) return E_INVALIDARG;
    ITfSource* pSource = nullptr;
    HRESULT hr = pContext->QueryInterface(IID_ITfSource, reinterpret_cast<void**>(&pSource));
    if (FAILED(hr) || pSource == nullptr) return FAILED(hr) ? hr : E_FAIL;
    hr = pSource->AdviseSink(IID_ITfTextEditSink, static_cast<ITfTextEditSink*>(this),
                             &_textEditSinkCookie);
    pSource->Release();
    if (SUCCEEDED(hr)) {
        _pFocusedContext = pContext;
        _pFocusedContext->AddRef();
    }
    return hr;
}

void CTextService::UninitTextEditSink() {
    if (_pFocusedContext == nullptr) return;
    if (_textEditSinkCookie != TF_INVALID_COOKIE) {
        ITfSource* pSource = nullptr;
        if (SUCCEEDED(_pFocusedContext->QueryInterface(
                IID_ITfSource, reinterpret_cast<void**>(&pSource))) &&
            pSource != nullptr) {
            pSource->UnadviseSink(_textEditSinkCookie);
            pSource->Release();
        }
        _textEditSinkCookie = TF_INVALID_COOKIE;
    }
    _pFocusedContext->Release();
    _pFocusedContext = nullptr;
}

// ===== 4. ITfThreadMgrEventSink =====
STDMETHODIMP CTextService::OnInitDocumentMgr(ITfDocumentMgr* /*pDocMgr*/) {
    return S_OK;
}

STDMETHODIMP CTextService::OnUninitDocumentMgr(ITfDocumentMgr* /*pDocMgr*/) {
    return S_OK;
}

STDMETHODIMP CTextService::OnSetFocus(ITfDocumentMgr* pDocMgrFocus,
                                      ITfDocumentMgr* /*pDocMgrPrevFocus*/) {
    ITfContext* pNewTop = nullptr;
    if (pDocMgrFocus != nullptr) {
        pDocMgrFocus->GetTop(&pNewTop);
    }

    // 入力中に別の欄へフォーカスが移る場合、旧コンテキストの composition を
    // そのまま放置すると次の入力が旧 composition を書き換えてしまう。
    // 移行先が composition の所有コンテキストでなければ、旧コンテキスト上で
    // 同期セッションを張って現在表示中の内容で確定してから切り替える。
    if (_pComposition != nullptr && _pCompositionContext != nullptr &&
        pNewTop != _pCompositionContext) {
        KeyAction action;
        action.type = KeyActionType::Commit;
        CEditSession* pSession =
            new (std::nothrow) CEditSession(this, _pCompositionContext, action);
        HRESULT hrSession = E_FAIL;
        HRESULT hr = (pSession != nullptr)
                         ? _pCompositionContext->RequestEditSession(
                               _clientId, pSession, TF_ES_SYNC | TF_ES_READWRITE, &hrSession)
                         : E_OUTOFMEMORY;
        if (pSession != nullptr) pSession->Release();
        if (FAILED(hr) || FAILED(hrSession)) {
            // 確定できなかった場合は参照を放して状態だけ片付ける
            // (composition 自体は TSF 側で終了する)。
            _pComposition->Release();
            _pComposition = nullptr;
            _pCompositionContext->Release();
            _pCompositionContext = nullptr;
            ClearComposingState();
            _candidateWindow.Hide();
        }
    }

    // フォーカスのあるドキュメントの先頭コンテキストに TextEditSink を付ける。
    UninitTextEditSink();
    if (pNewTop != nullptr) {
        InitTextEditSink(pNewTop);
        pNewTop->Release();
    }
    return S_OK;
}

STDMETHODIMP CTextService::OnPushContext(ITfContext* /*pContext*/) {
    return S_OK;
}

STDMETHODIMP CTextService::OnPopContext(ITfContext* /*pContext*/) {
    return S_OK;
}

// ===== 5. ITfTextEditSink =====
// v1 ではドキュメント側の変更を追跡しない (候補は composition 内で完結する)。
STDMETHODIMP CTextService::OnEndEdit(ITfContext* /*pContext*/, TfEditCookie /*ecReadOnly*/,
                                     ITfEditRecord* /*pEditRecord*/) {
    return S_OK;
}

// ===== 6. ITfKeyEventSink =====
STDMETHODIMP CTextService::OnSetFocus(BOOL /*fForeground*/) {
    return S_OK;
}

bool CTextService::IsKeyEaten(WPARAM wParam) const {
    // 英字と '-' は composition の開始・継続に使うため常に消費する。
    if (wParam >= 'A' && wParam <= 'Z') return true;
    if (wParam == VK_OEM_MINUS) return true;

    bool composing = (_pComposition != nullptr) || !_romaji.empty();
    if (!composing) return false;

    if (wParam == VK_SPACE || wParam == VK_RETURN || wParam == VK_ESCAPE ||
        wParam == VK_BACK) {
        return true;
    }
    // 変換中は数字キー 1-9 を候補の直接選択として消費する。
    if (_converting) {
        if (wParam >= '1' && wParam <= '9') return true;
        if (wParam >= VK_NUMPAD1 && wParam <= VK_NUMPAD9) return true;
        // ↑↓ / Tab は候補の移動に使う。
        if (wParam == VK_DOWN || wParam == VK_UP || wParam == VK_TAB) return true;
    }
    return false;
}

bool CTextService::KeyToAction(WPARAM wParam, KeyAction& action) const {
    if (wParam >= 'A' && wParam <= 'Z') {
        action.type = KeyActionType::InputChar;
        action.ch = static_cast<wchar_t>(L'a' + (wParam - 'A'));
        return true;
    }
    if (wParam == VK_OEM_MINUS) {
        action.type = KeyActionType::InputChar;
        action.ch = L'-';
        return true;
    }
    switch (wParam) {
        case VK_SPACE:
            action.type = KeyActionType::Convert;
            return true;
        case VK_DOWN:
            action.type = KeyActionType::SelectNext;
            return true;
        case VK_UP:
            action.type = KeyActionType::SelectPrev;
            return true;
        case VK_TAB:
            action.type = KeyActionType::ToggleTab;
            return true;
        case VK_RETURN:
            action.type = KeyActionType::Commit;
            return true;
        case VK_ESCAPE:
            action.type = KeyActionType::Cancel;
            return true;
        case VK_BACK:
            action.type = KeyActionType::Backspace;
            return true;
        default:
            break;
    }
    if (wParam >= '1' && wParam <= '9') {
        action.type = KeyActionType::CommitCandidate;
        action.candidateIndex = static_cast<int>(wParam - '1');
        return true;
    }
    if (wParam >= VK_NUMPAD1 && wParam <= VK_NUMPAD9) {
        action.type = KeyActionType::CommitCandidate;
        action.candidateIndex = static_cast<int>(wParam - VK_NUMPAD1);
        return true;
    }
    return false;
}

STDMETHODIMP CTextService::OnTestKeyDown(ITfContext* /*pContext*/, WPARAM wParam,
                                         LPARAM /*lParam*/, BOOL* pfEaten) {
    if (pfEaten == nullptr) return E_INVALIDARG;
    // Ctrl/Alt 併用キーはアプリ側のショートカットとして素通しする。
    // この場合、入力中の composition は確定せずそのまま残る (BUILD.md 参照)。
    if (IsCtrlOrAltDown()) {
        *pfEaten = FALSE;
        return S_OK;
    }
    *pfEaten = IsKeyEaten(wParam) ? TRUE : FALSE;
    return S_OK;
}

STDMETHODIMP CTextService::OnTestKeyUp(ITfContext* /*pContext*/, WPARAM wParam,
                                       LPARAM /*lParam*/, BOOL* pfEaten) {
    if (pfEaten == nullptr) return E_INVALIDARG;
    // KeyDown で消費したキーの KeyUp だけを消費対象にする
    // (記録のクリアは OnKeyUp で行う)。
    *pfEaten = (_keyDownEatenVk != 0 && _keyDownEatenVk == wParam) ? TRUE : FALSE;
    return S_OK;
}

STDMETHODIMP CTextService::OnKeyDown(ITfContext* pContext, WPARAM wParam, LPARAM /*lParam*/,
                                     BOOL* pfEaten) {
    if (pfEaten == nullptr) return E_INVALIDARG;
    *pfEaten = FALSE;
    if (pContext == nullptr) return S_OK;
    // Ctrl/Alt 併用キーは素通しする (通常は OnTestKeyDown の段階で弾かれるが、
    // 直接呼ばれた場合への防御としてここでも判定する)。
    // composition は確定せず、そのまま残る。
    if (IsCtrlOrAltDown()) return S_OK;
    if (!IsKeyEaten(wParam)) return S_OK;

    KeyAction action;
    if (!KeyToAction(wParam, action)) return S_OK;

    CEditSession* pSession = new (std::nothrow) CEditSession(this, pContext, action);
    if (pSession == nullptr) return E_OUTOFMEMORY;

    HRESULT hrSession = S_OK;
    HRESULT hr = pContext->RequestEditSession(_clientId, pSession,
                                              TF_ES_SYNC | TF_ES_READWRITE, &hrSession);
    pSession->Release();
    if (SUCCEEDED(hr) && SUCCEEDED(hrSession)) {
        *pfEaten = TRUE;
        _keyDownEatenVk = wParam; // 対応する KeyUp も消費するため記録する
    }
    return S_OK;
}

STDMETHODIMP CTextService::OnKeyUp(ITfContext* /*pContext*/, WPARAM wParam,
                                   LPARAM /*lParam*/, BOOL* pfEaten) {
    if (pfEaten == nullptr) return E_INVALIDARG;
    // OnKeyDown で実際に消費したキーの KeyUp のみ消費し、記録をクリアする。
    // (KeyUp 時点で composing 状態が変わっていても判定がぶれないようにするため。
    //  キーダウンを消費したキーのアップをアプリ側へ漏らさない目的もある)
    *pfEaten = (_keyDownEatenVk != 0 && _keyDownEatenVk == wParam) ? TRUE : FALSE;
    if (*pfEaten) _keyDownEatenVk = 0;
    return S_OK;
}

STDMETHODIMP CTextService::OnPreservedKey(ITfContext* /*pContext*/, REFGUID /*rguid*/,
                                          BOOL* pfEaten) {
    if (pfEaten == nullptr) return E_INVALIDARG;
    *pfEaten = FALSE;
    return S_OK;
}

// ===== 7. ITfCompositionSink =====
STDMETHODIMP CTextService::OnCompositionTerminated(TfEditCookie /*ecWrite*/,
                                                   ITfComposition* pComposition) {
    // 自分の composition が終了した場合だけ状態を片付ける。
    // (無関係な composition の終了通知で composing 状態まで消さない)
    if (pComposition != nullptr && pComposition == _pComposition) {
        _pComposition->Release();
        _pComposition = nullptr;
        if (_pCompositionContext != nullptr) {
            _pCompositionContext->Release();
            _pCompositionContext = nullptr;
        }
        ClearComposingState();
        _candidateWindow.Hide();
    }
    return S_OK;
}

// ===== 8. 編集セッション実処理 =====
HRESULT CTextService::HandleEditSession(TfEditCookie ec, ITfContext* pContext,
                                        const KeyAction& action) {
    // COM 境界 (ITfEditSession::DoEditSession の先) なので、例外は外へ漏らさず
    // HRESULT に変換する (bad_alloc は E_OUTOFMEMORY、それ以外は E_FAIL)。
    try {
        switch (action.type) {
            case KeyActionType::InputChar:
                return DoInputChar(ec, pContext, action.ch);
            case KeyActionType::Convert:
                // 予測タブでの Space は選択中の予測で確定する。
                if (_converting && _activeTab == ActiveTab::Predict) {
                    return DoCommitPrediction(ec, pContext, _predictionIndex);
                }
                return DoConvert(ec, pContext);
            case KeyActionType::SelectNext:
                return DoSelectDelta(ec, pContext, 1);
            case KeyActionType::SelectPrev:
                return DoSelectDelta(ec, pContext, -1);
            case KeyActionType::ToggleTab:
                return DoToggleTab(ec, pContext);
            case KeyActionType::Commit:
                if (_converting && _activeTab == ActiveTab::Predict) {
                    return DoCommitPrediction(ec, pContext, _predictionIndex);
                }
                return DoCommit(ec, pContext,
                                _converting && !_candidates.empty()
                                    ? _candidates[_candidateIndex]
                                    : CurrentReading());
            case KeyActionType::CommitCandidate:
                if (_converting && _activeTab == ActiveTab::Predict) {
                    return DoCommitPrediction(
                        ec, pContext, static_cast<size_t>(action.candidateIndex));
                }
                if (_converting && action.candidateIndex >= 0 &&
                    static_cast<size_t>(action.candidateIndex) < _candidates.size()) {
                    return DoCommit(ec, pContext,
                                    _candidates[static_cast<size_t>(action.candidateIndex)]);
                }
                return S_OK;
            case KeyActionType::CommitPrediction:
                return DoCommitPrediction(ec, pContext,
                                          static_cast<size_t>(action.candidateIndex));
            case KeyActionType::Cancel:
                // 予測タブでの Esc は変換タブへ戻るだけにする。
                if (_converting && _activeTab == ActiveTab::Predict) {
                    return DoToggleTab(ec, pContext);
                }
                return DoCancel(ec, pContext);
            case KeyActionType::Backspace:
                return DoBackspace(ec, pContext);
        }
    } catch (const std::bad_alloc&) {
        return E_OUTOFMEMORY;
    } catch (...) {
        return E_FAIL;
    }
    return S_OK;
}

std::wstring CTextService::CurrentDisplayText() const {
    YoruKana::RomajiConverter::Result r = YoruKana::RomajiConverter::Convert(_romaji);
    return r.kana + r.pending;
}

std::wstring CTextService::CurrentReading() const {
    return YoruKana::RomajiConverter::ConvertFully(_romaji);
}

HRESULT CTextService::DoInputChar(TfEditCookie ec, ITfContext* pContext, wchar_t ch) {
    if (_pComposition == nullptr) {
        HRESULT hr = StartComposition(ec, pContext);
        if (FAILED(hr)) return hr;
    }
    _romaji += ch;
    _converting = false;
    _candidates.clear();
    _candidateIndex = 0;
    _currentReading.clear();
    _activeTab = ActiveTab::Convert;
    HRESULT hr = SetCompositionText(ec, pContext, CurrentDisplayText());
    if (SUCCEEDED(hr)) {
        // 入力中は予測候補があればウィンドウに出す (クリックで確定できる)。
        _predictions = _engine.PredictEntries(CurrentReading(), 9);
        _predictionIndex = 0;
        _showingPredictions = !_predictions.empty();
        RefreshCandidateWindow(ec, pContext);
    }
    return hr;
}

HRESULT CTextService::DoConvert(TfEditCookie ec, ITfContext* pContext) {
    if (_pComposition == nullptr) return S_OK;
    if (!_converting) {
        _currentReading = CurrentReading();
        _candidates = _engine.Candidates(_currentReading, _romaji);
        if (_candidates.empty()) return S_OK;
        _candidateIndex = 0;
        _converting = true;
        _activeTab = ActiveTab::Convert;
        _showingPredictions = false;
    } else {
        _candidateIndex = (_candidateIndex + 1) % _candidates.size();
    }
    HRESULT hr = SetCompositionText(ec, pContext, _candidates[_candidateIndex]);
    if (SUCCEEDED(hr)) RefreshCandidateWindow(ec, pContext);
    return hr;
}

HRESULT CTextService::DoSelectDelta(TfEditCookie ec, ITfContext* pContext, int delta) {
    if (_pComposition == nullptr || !_converting) return S_OK;
    if (_activeTab == ActiveTab::Predict) {
        if (_predictions.empty()) return S_OK;
        long size = static_cast<long>(_predictions.size());
        long next = (static_cast<long>(_predictionIndex) + delta) % size;
        if (next < 0) next += size;
        _predictionIndex = static_cast<size_t>(next);
        HRESULT hr = SetCompositionText(ec, pContext, _predictions[_predictionIndex].second);
        if (SUCCEEDED(hr)) RefreshCandidateWindow(ec, pContext);
        return hr;
    }
    if (_candidates.empty()) return S_OK;
    long size = static_cast<long>(_candidates.size());
    long next = (static_cast<long>(_candidateIndex) + delta) % size;
    if (next < 0) next += size;
    _candidateIndex = static_cast<size_t>(next);
    HRESULT hr = SetCompositionText(ec, pContext, _candidates[_candidateIndex]);
    if (SUCCEEDED(hr)) RefreshCandidateWindow(ec, pContext);
    return hr;
}

HRESULT CTextService::DoToggleTab(TfEditCookie ec, ITfContext* pContext) {
    if (_pComposition == nullptr || !_converting) return S_OK;
    if (_activeTab == ActiveTab::Convert) {
        // 予測タブへ: 現在の読みで始まる長い読みの候補を予測として出す。
        std::vector<std::pair<std::wstring, std::wstring>> predictions =
            _engine.PredictEntries(_currentReading, 27);
        if (predictions.empty()) return S_OK; // 予測が無ければ切り替えない
        _predictions = predictions;
        _predictionIndex = 0;
        _activeTab = ActiveTab::Predict;
        HRESULT hr = SetCompositionText(ec, pContext, _predictions[0].second);
        if (SUCCEEDED(hr)) RefreshCandidateWindow(ec, pContext);
        return hr;
    }
    // 変換タブへ戻る: 表示を選択中の変換候補へ戻す。
    _activeTab = ActiveTab::Convert;
    HRESULT hr = _candidates.empty()
                     ? S_OK
                     : SetCompositionText(ec, pContext, _candidates[_candidateIndex]);
    if (SUCCEEDED(hr)) RefreshCandidateWindow(ec, pContext);
    return hr;
}

HRESULT CTextService::DoCommit(TfEditCookie ec, ITfContext* pContext,
                               const std::wstring& finalText) {
    if (_pComposition == nullptr) return S_OK;
    HRESULT hr = SetCompositionText(ec, pContext, finalText);
    if (FAILED(hr)) return hr;
    CollapseSelectionToCompositionEnd(ec, pContext);
    // 変換候補から選んだ確定は学習に記録する (同じ読みで次回は先に来る)。
    if (_converting && !_currentReading.empty()) {
        for (const std::wstring& c : _candidates) {
            if (c == finalText) {
                _learning.Record(_currentReading, finalText);
                SaveLearning();
                break;
            }
        }
    }
    EndComposition(ec);
    ClearComposingState();
    _candidateWindow.Hide();
    return S_OK;
}

HRESULT CTextService::DoCommitPrediction(TfEditCookie ec, ITfContext* pContext,
                                         size_t index) {
    if (_pComposition == nullptr || index >= _predictions.size()) return S_OK;
    const std::wstring reading = _predictions[index].first;
    const std::wstring finalText = _predictions[index].second;
    HRESULT hr = SetCompositionText(ec, pContext, finalText);
    if (FAILED(hr)) return hr;
    CollapseSelectionToCompositionEnd(ec, pContext);
    _learning.Record(reading, finalText);
    SaveLearning();
    EndComposition(ec);
    ClearComposingState();
    _candidateWindow.Hide();
    return S_OK;
}

HRESULT CTextService::DoCancel(TfEditCookie ec, ITfContext* pContext) {
    if (_pComposition == nullptr) return S_OK;
    HRESULT hr = SetCompositionText(ec, pContext, L"");
    if (FAILED(hr)) return hr;
    CollapseSelectionToCompositionEnd(ec, pContext);
    EndComposition(ec);
    ClearComposingState();
    _candidateWindow.Hide();
    return S_OK;
}

HRESULT CTextService::DoBackspace(TfEditCookie ec, ITfContext* pContext) {
    if (_pComposition == nullptr) return S_OK;
    if (_converting) {
        // 変換中ならまず変換だけを取り消し、ひらがな表示に戻す。
        _converting = false;
        _candidates.clear();
        _candidateIndex = 0;
        _currentReading.clear();
        _activeTab = ActiveTab::Convert;
        _predictions = _engine.PredictEntries(CurrentReading(), 9);
        _predictionIndex = 0;
        _showingPredictions = !_predictions.empty();
        HRESULT hr = SetCompositionText(ec, pContext, CurrentDisplayText());
        if (SUCCEEDED(hr)) RefreshCandidateWindow(ec, pContext);
        return hr;
    }
    if (!_romaji.empty()) _romaji.pop_back();
    if (_romaji.empty()) {
        HRESULT hr = SetCompositionText(ec, pContext, L"");
        if (FAILED(hr)) return hr;
        EndComposition(ec);
        ClearComposingState();
        _candidateWindow.Hide();
        return S_OK;
    }
    HRESULT hr = SetCompositionText(ec, pContext, CurrentDisplayText());
    if (SUCCEEDED(hr)) {
        _predictions = _engine.PredictEntries(CurrentReading(), 9);
        _predictionIndex = 0;
        _showingPredictions = !_predictions.empty();
        RefreshCandidateWindow(ec, pContext);
    }
    return hr;
}

void CTextService::CollapseSelectionToCompositionEnd(TfEditCookie ec,
                                                     ITfContext* pContext) {
    if (_pComposition == nullptr) return;
    ITfRange* pRange = nullptr;
    HRESULT hr = _pComposition->GetRange(&pRange);
    if (FAILED(hr) || pRange == nullptr) return;
    ITfRange* pCollapsed = nullptr;
    if (SUCCEEDED(pRange->Clone(&pCollapsed)) && pCollapsed != nullptr) {
        // 範囲選択のまま composition を終えると、確定した文字列がアプリ側で
        // 選択状態のまま残る (メモ帳等)。末尾に畳んだ選択へ置き換える。
        pCollapsed->Collapse(ec, TF_ANCHOR_END);
        TF_SELECTION selection = {};
        selection.range = pCollapsed;
        selection.style.ase = TF_AE_END;
        selection.style.fInterimChar = FALSE;
        pContext->SetSelection(ec, 1, &selection);
        pCollapsed->Release();
    }
    pRange->Release();
}

// ===== 9. composition 操作 =====
HRESULT CTextService::StartComposition(TfEditCookie ec, ITfContext* pContext) {
    ITfContextComposition* pContextComposition = nullptr;
    HRESULT hr = pContext->QueryInterface(IID_ITfContextComposition,
                                          reinterpret_cast<void**>(&pContextComposition));
    if (FAILED(hr) || pContextComposition == nullptr) return FAILED(hr) ? hr : E_FAIL;

    TF_SELECTION selection = {};
    ULONG fetched = 0;
    hr = pContext->GetSelection(ec, TF_DEFAULT_SELECTION, 1, &selection, &fetched);
    if (FAILED(hr) || fetched == 0 || selection.range == nullptr) {
        pContextComposition->Release();
        return FAILED(hr) ? hr : E_FAIL;
    }

    ITfComposition* pComposition = nullptr;
    hr = pContextComposition->StartComposition(ec, selection.range,
                                               static_cast<ITfCompositionSink*>(this),
                                               &pComposition);
    selection.range->Release();
    pContextComposition->Release();
    if (FAILED(hr) || pComposition == nullptr) return FAILED(hr) ? hr : E_FAIL;

    _pComposition = pComposition; // StartComposition の出力参照をそのまま保持する
    _pCompositionContext = pContext;
    _pCompositionContext->AddRef();
    return S_OK;
}

HRESULT CTextService::SetCompositionText(TfEditCookie ec, ITfContext* pContext,
                                         const std::wstring& text) {
    if (_pComposition == nullptr) return E_FAIL;
    ITfRange* pRange = nullptr;
    HRESULT hr = _pComposition->GetRange(&pRange);
    if (FAILED(hr) || pRange == nullptr) return FAILED(hr) ? hr : E_FAIL;

    hr = pRange->SetText(ec, 0, text.c_str(), static_cast<LONG>(text.size()));
    if (SUCCEEDED(hr)) {
        // 選択 (カーソル) を composition の末尾に置く。
        TF_SELECTION selection = {};
        selection.range = pRange;
        selection.style.ase = TF_AE_END;
        selection.style.fInterimChar = FALSE;
        pContext->SetSelection(ec, 1, &selection);
    }
    pRange->Release();
    return hr;
}

void CTextService::EndComposition(TfEditCookie ec) {
    ITfComposition* pComposition = _pComposition;
    _pComposition = nullptr; // 先に外しておくと OnCompositionTerminated が二重解放しない
    if (pComposition != nullptr) {
        pComposition->EndComposition(ec);
        pComposition->Release();
    }
    if (_pCompositionContext != nullptr) {
        _pCompositionContext->Release();
        _pCompositionContext = nullptr;
    }
}

void CTextService::ClearComposingState() {
    _romaji.clear();
    _currentReading.clear();
    _candidates.clear();
    _candidateIndex = 0;
    _converting = false;
    _activeTab = ActiveTab::Convert;
    _predictions.clear();
    _predictionIndex = 0;
    _showingPredictions = false;
}

// ===== 10. 候補ウィンドウ / 学習 =====
void CTextService::RefreshCandidateWindow(TfEditCookie ec, ITfContext* pContext) {
    if (_pComposition == nullptr) {
        _candidateWindow.Hide();
        return;
    }

    std::vector<std::wstring> items;
    size_t selected = CCandidateWindow::kNoSelection;
    bool showTabs = false;
    int activeTab = 0;
    const wchar_t* headerLabel = nullptr;
    if (_converting) {
        showTabs = true;
        if (_activeTab == ActiveTab::Convert) {
            items = _candidates;
            selected = _candidateIndex;
            activeTab = 0;
        } else {
            for (const auto& p : _predictions) items.push_back(p.second);
            selected = _predictionIndex;
            activeTab = 1;
        }
    } else if (_showingPredictions && !_predictions.empty()) {
        for (const auto& p : _predictions) items.push_back(p.second);
        headerLabel = L"予測";
    } else {
        _candidateWindow.Hide();
        return;
    }
    if (items.empty()) {
        _candidateWindow.Hide();
        return;
    }

    ITfRange* pRange = nullptr;
    HRESULT hr = _pComposition->GetRange(&pRange);
    if (FAILED(hr) || pRange == nullptr) return;

    RECT rect = {};
    bool got = false;
    ITfContextView* pView = nullptr;
    if (SUCCEEDED(pContext->GetActiveView(&pView)) && pView != nullptr) {
        BOOL clipped = FALSE;
        if (SUCCEEDED(pView->GetTextExt(ec, pRange, &rect, &clipped))) got = true;
        pView->Release();
    }
    pRange->Release();

    if (!got) {
        // 画面位置が取れないホスト (コンソール等) ではキャレット位置で代用する。
        POINT pt = {};
        if (!GetCaretPos(&pt)) return;
        HWND hwndForeground = GetForegroundWindow();
        if (hwndForeground != nullptr) ClientToScreen(hwndForeground, &pt);
        rect.left = pt.x;
        rect.right = pt.x;
        rect.top = pt.y;
        rect.bottom = pt.y + 20;
    }
    _candidateWindow.Show(items, selected, rect, showTabs, activeTab, headerLabel);
}

void CTextService::OnCandidateClicked(size_t index) {
    if (_pCompositionContext == nullptr) return;
    KeyAction action;
    if (_converting) {
        action.type = (_activeTab == ActiveTab::Predict)
                          ? KeyActionType::CommitPrediction
                          : KeyActionType::CommitCandidate;
    } else if (_showingPredictions) {
        action.type = KeyActionType::CommitPrediction;
    } else {
        return;
    }
    action.candidateIndex = static_cast<int>(index);
    CEditSession* pSession =
        new (std::nothrow) CEditSession(this, _pCompositionContext, action);
    if (pSession == nullptr) return;
    HRESULT hrSession = S_OK;
    _pCompositionContext->RequestEditSession(_clientId, pSession, TF_ES_READWRITE,
                                             &hrSession);
    pSession->Release();
}

void CTextService::OnTabClicked(int tab) {
    if (!_converting || _pCompositionContext == nullptr) return;
    if ((tab == 0 && _activeTab == ActiveTab::Convert) ||
        (tab == 1 && _activeTab == ActiveTab::Predict)) {
        return;
    }
    KeyAction action;
    action.type = KeyActionType::ToggleTab;
    CEditSession* pSession =
        new (std::nothrow) CEditSession(this, _pCompositionContext, action);
    if (pSession == nullptr) return;
    HRESULT hrSession = S_OK;
    _pCompositionContext->RequestEditSession(_clientId, pSession, TF_ES_READWRITE,
                                             &hrSession);
    pSession->Release();
}

void CTextService::LoadLearning() {
    std::wstring path = LearningFilePath();
    if (path.empty()) return;
    HANDLE hFile = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return;

    LARGE_INTEGER size = {};
    bool ok = GetFileSizeEx(hFile, &size) && size.QuadPart <= 4 * 1024 * 1024;
    std::string bytes;
    if (ok && size.QuadPart > 0) {
        bytes.resize(static_cast<size_t>(size.QuadPart));
        DWORD read = 0;
        ok = ReadFile(hFile, bytes.data(), static_cast<DWORD>(bytes.size()), &read,
                      nullptr) != FALSE;
        bytes.resize(read);
    }
    CloseHandle(hFile);
    if (!ok) return;

    // UTF-8 BOM があれば取り除く。
    if (bytes.size() >= 3 && static_cast<unsigned char>(bytes[0]) == 0xEF &&
        static_cast<unsigned char>(bytes[1]) == 0xBB &&
        static_cast<unsigned char>(bytes[2]) == 0xBF) {
        bytes.erase(0, 3);
    }
    _learning.Deserialize(Utf8ToWide(bytes));
}

void CTextService::SaveLearning() {
    std::wstring path = LearningFilePath();
    if (path.empty()) return;
    // 保存先フォルダ (%APPDATA%\YoruKana) を用意する。
    size_t sep = path.find_last_of(L'\\');
    if (sep != std::wstring::npos) {
        CreateDirectoryW(path.substr(0, sep).c_str(), nullptr);
    }
    std::string utf8 = "\xEF\xBB\xBF" + WideToUtf8(_learning.Serialize());
    HANDLE hFile = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                               FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return;
    DWORD written = 0;
    WriteFile(hFile, utf8.data(), static_cast<DWORD>(utf8.size()), &written, nullptr);
    CloseHandle(hFile);
}
