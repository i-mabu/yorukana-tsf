#include "ClassFactory.h"

#include <new>

#include "Globals.h"
#include "Guids.h"
#include "TextService.h"

// ===== 1. 生成・破棄 =====
CClassFactory::CClassFactory() : _refCount(1) {
    DllAddRef();
}

CClassFactory::~CClassFactory() {
    DllReleaseRef();
}

// ===== 2. IUnknown =====
STDMETHODIMP CClassFactory::QueryInterface(REFIID riid, void** ppvObj) {
    if (ppvObj == nullptr) return E_INVALIDARG;
    *ppvObj = nullptr;
    if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IClassFactory)) {
        *ppvObj = static_cast<IClassFactory*>(this);
        AddRef();
        return S_OK;
    }
    return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) CClassFactory::AddRef() {
    return static_cast<ULONG>(InterlockedIncrement(&_refCount));
}

STDMETHODIMP_(ULONG) CClassFactory::Release() {
    LONG count = InterlockedDecrement(&_refCount);
    if (count == 0) {
        delete this;
        return 0;
    }
    return static_cast<ULONG>(count);
}

// ===== 3. IClassFactory =====
STDMETHODIMP CClassFactory::CreateInstance(IUnknown* pUnkOuter, REFIID riid, void** ppvObj) {
    if (ppvObj == nullptr) return E_INVALIDARG;
    *ppvObj = nullptr;
    if (pUnkOuter != nullptr) return CLASS_E_NOAGGREGATION;

    // COM 境界なので、生成中の例外 (辞書構築時の bad_alloc 等) は
    // 外へ漏らさず HRESULT に変換する。
    try {
        CTextService* pService = new (std::nothrow) CTextService();
        if (pService == nullptr) return E_OUTOFMEMORY;

        HRESULT hr = pService->QueryInterface(riid, ppvObj);
        pService->Release();
        return hr;
    } catch (const std::bad_alloc&) {
        return E_OUTOFMEMORY;
    } catch (...) {
        return E_FAIL;
    }
}

STDMETHODIMP CClassFactory::LockServer(BOOL fLock) {
    if (fLock) {
        DllAddRef();
    } else {
        DllReleaseRef();
    }
    return S_OK;
}
