#pragma once

#include <windows.h>

// ===== 1. クラスファクトリ =====
// CLSID_TextService に対する IClassFactory 実装。DllGetClassObject から返される。
class CClassFactory : public IClassFactory {
public:
    CClassFactory();
    ~CClassFactory();

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppvObj) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;

    // IClassFactory
    STDMETHODIMP CreateInstance(IUnknown* pUnkOuter, REFIID riid, void** ppvObj) override;
    STDMETHODIMP LockServer(BOOL fLock) override;

private:
    LONG _refCount;
};
