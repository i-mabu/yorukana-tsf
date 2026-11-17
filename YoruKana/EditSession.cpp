#include "EditSession.h"

#include <new>

#include "TextService.h"

// ===== 1. 生成・破棄 =====
CEditSession::CEditSession(CTextService* pService, ITfContext* pContext,
                           const KeyAction& action)
    : _refCount(1), _pService(pService), _pContext(pContext), _action(action) {
    if (_pService != nullptr) _pService->AddRef();
    if (_pContext != nullptr) _pContext->AddRef();
}

CEditSession::~CEditSession() {
    if (_pContext != nullptr) _pContext->Release();
    if (_pService != nullptr) _pService->Release();
}

// ===== 2. IUnknown =====
STDMETHODIMP CEditSession::QueryInterface(REFIID riid, void** ppvObj) {
    if (ppvObj == nullptr) return E_INVALIDARG;
    *ppvObj = nullptr;
    if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfEditSession)) {
        *ppvObj = static_cast<ITfEditSession*>(this);
        AddRef();
        return S_OK;
    }
    return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) CEditSession::AddRef() {
    return static_cast<ULONG>(InterlockedIncrement(&_refCount));
}

STDMETHODIMP_(ULONG) CEditSession::Release() {
    LONG count = InterlockedDecrement(&_refCount);
    if (count == 0) {
        delete this;
        return 0;
    }
    return static_cast<ULONG>(count);
}

// ===== 3. ITfEditSession =====
STDMETHODIMP CEditSession::DoEditSession(TfEditCookie ec) {
    if (_pService == nullptr || _pContext == nullptr) return E_FAIL;
    return _pService->HandleEditSession(ec, _pContext, _action);
}
