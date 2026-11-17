#include <windows.h>
#include <objbase.h>

#include <new>

#include "ClassFactory.h"
#include "Globals.h"
#include "Guids.h"
#include "Register.h"

// ===== 1. DLL グローバル実体 =====
HINSTANCE g_hInstance = nullptr;
LONG g_cRefDll = 0;

void DllAddRef() {
    InterlockedIncrement(&g_cRefDll);
}

void DllReleaseRef() {
    InterlockedDecrement(&g_cRefDll);
}

// ===== 2. DllMain =====
BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID /*lpReserved*/) {
    if (dwReason == DLL_PROCESS_ATTACH) {
        g_hInstance = hInstance;
        DisableThreadLibraryCalls(hInstance);
    }
    return TRUE;
}

// ===== 3. COM 標準エクスポート (YoruKana.def で公開) =====
STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, void** ppvObj) {
    if (ppvObj == nullptr) return E_INVALIDARG;
    *ppvObj = nullptr;
    if (!IsEqualCLSID(rclsid, CLSID_TextService)) return CLASS_E_CLASSNOTAVAILABLE;

    CClassFactory* pFactory = new (std::nothrow) CClassFactory();
    if (pFactory == nullptr) return E_OUTOFMEMORY;
    HRESULT hr = pFactory->QueryInterface(riid, ppvObj);
    pFactory->Release();
    return hr;
}

STDAPI DllCanUnloadNow() {
    return (g_cRefDll == 0) ? S_OK : S_FALSE;
}

STDAPI DllRegisterServer() {
    return RegisterTextService();
}

STDAPI DllUnregisterServer() {
    return UnregisterTextService();
}
