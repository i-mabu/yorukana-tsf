#include "Register.h"

#include <msctf.h>
#include <objbase.h>

#include <cstring>

#include <string>

#include "Globals.h"
#include "Guids.h"

namespace {

// ===== 1. 内部ヘルパ =====

// GUID を "{...}" 形式の文字列にする。
bool GuidToString(REFGUID guid, std::wstring& out) {
    LPOLESTR psz = nullptr;
    if (FAILED(StringFromCLSID(guid, &psz)) || psz == nullptr) return false;
    out = psz;
    CoTaskMemFree(psz);
    return true;
}

// この DLL のフルパスを取得する。
// MAX_PATH 固定はせず、入り切らない場合はバッファを拡張して再試行する。
bool GetModulePath(std::wstring& out) {
    std::wstring buf(256, L'\0');
    for (;;) {
        DWORD len = GetModuleFileNameW(g_hInstance, buf.data(),
                                       static_cast<DWORD>(buf.size()));
        if (len == 0) return false;
        if (len < buf.size()) {
            buf.resize(len);
            out = buf;
            return true;
        }
        // len == buf.size() のときは切り捨てられた可能性があるため拡張する。
        if (buf.size() >= 32768) return false;
        buf.resize(buf.size() * 2, L'\0');
    }
}

// HKCR 配下にキーを作り、既定値 (文字列) を設定する。
bool SetRegistryString(HKEY root, const std::wstring& subKey, const wchar_t* valueName,
                       const std::wstring& value) {
    HKEY hKey = nullptr;
    LONG rc = RegCreateKeyExW(root, subKey.c_str(), 0, nullptr, REG_OPTION_NON_VOLATILE,
                              KEY_WRITE, nullptr, &hKey, nullptr);
    if (rc != ERROR_SUCCESS) return false;
    rc = RegSetValueExW(hKey, valueName, 0, REG_SZ,
                        reinterpret_cast<const BYTE*>(value.c_str()),
                        static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(hKey);
    return rc == ERROR_SUCCESS;
}

// ===== 2. COM サーバ登録 (HKCR\CLSID\{CLSID}\InprocServer32) =====
HRESULT RegisterComServer() {
    std::wstring clsidStr, modulePath;
    if (!GuidToString(CLSID_TextService, clsidStr)) return E_FAIL;
    if (!GetModulePath(modulePath)) return E_FAIL;

    std::wstring clsidKey = L"CLSID\\" + clsidStr;
    if (!SetRegistryString(HKEY_CLASSES_ROOT, clsidKey, nullptr,
                           YoruKana::kTipDescription)) {
        return E_FAIL;
    }
    std::wstring inprocKey = clsidKey + L"\\InprocServer32";
    if (!SetRegistryString(HKEY_CLASSES_ROOT, inprocKey, nullptr, modulePath)) {
        return E_FAIL;
    }
    if (!SetRegistryString(HKEY_CLASSES_ROOT, inprocKey, L"ThreadingModel", L"Apartment")) {
        return E_FAIL;
    }
    return S_OK;
}

HRESULT UnregisterComServer() {
    std::wstring clsidStr;
    if (!GuidToString(CLSID_TextService, clsidStr)) return E_FAIL;
    std::wstring clsidKey = L"CLSID\\" + clsidStr;
    LONG rc = RegDeleteTreeW(HKEY_CLASSES_ROOT, clsidKey.c_str());
    // 既に削除済み (キーなし) は失敗扱いにしない。
    if (rc != ERROR_SUCCESS && rc != ERROR_FILE_NOT_FOUND) {
        return HRESULT_FROM_WIN32(rc);
    }
    return S_OK;
}

// ===== 3. TSF プロファイル登録 (ITfInputProcessorProfiles) =====
HRESULT RegisterProfile() {
    ITfInputProcessorProfiles* pProfiles = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr,
                                  CLSCTX_INPROC_SERVER, IID_ITfInputProcessorProfiles,
                                  reinterpret_cast<void**>(&pProfiles));
    if (FAILED(hr) || pProfiles == nullptr) return FAILED(hr) ? hr : E_FAIL;

    hr = pProfiles->Register(CLSID_TextService);
    if (SUCCEEDED(hr)) {
        const wchar_t* desc = YoruKana::kProfileDescription;
        hr = pProfiles->AddLanguageProfile(
            CLSID_TextService, YoruKana::kJapaneseLangId, GUID_Profile, desc,
            static_cast<ULONG>(wcslen(desc)), nullptr, 0, 0);
    }
    if (SUCCEEDED(hr)) {
        // 既定で有効化しておく (ユーザーが Win+Space で切り替えられる状態にする)。
        // 有効化の失敗も結果に反映する (握り潰さない)。
        hr = pProfiles->EnableLanguageProfile(CLSID_TextService, YoruKana::kJapaneseLangId,
                                              GUID_Profile, TRUE);
    }
    pProfiles->Release();
    return hr;
}

HRESULT UnregisterProfile() {
    ITfInputProcessorProfiles* pProfiles = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr,
                                  CLSCTX_INPROC_SERVER, IID_ITfInputProcessorProfiles,
                                  reinterpret_cast<void**>(&pProfiles));
    if (FAILED(hr) || pProfiles == nullptr) return FAILED(hr) ? hr : E_FAIL;
    // 各段の失敗は最初のものを記録しつつ、後片付けは最後まで試みる。
    HRESULT hrResult = S_OK;
    hr = pProfiles->EnableLanguageProfile(CLSID_TextService, YoruKana::kJapaneseLangId,
                                          GUID_Profile, FALSE);
    if (FAILED(hr)) hrResult = hr;
    hr = pProfiles->RemoveLanguageProfile(CLSID_TextService, YoruKana::kJapaneseLangId,
                                          GUID_Profile);
    if (FAILED(hr) && SUCCEEDED(hrResult)) hrResult = hr;
    hr = pProfiles->Unregister(CLSID_TextService);
    if (FAILED(hr) && SUCCEEDED(hrResult)) hrResult = hr;
    pProfiles->Release();
    return hrResult;
}

// ===== 4. カテゴリ登録 (キーボード TIP として分類する) =====
HRESULT RegisterCategory() {
    ITfCategoryMgr* pCategoryMgr = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_TF_CategoryMgr, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_ITfCategoryMgr,
                                  reinterpret_cast<void**>(&pCategoryMgr));
    if (FAILED(hr) || pCategoryMgr == nullptr) return FAILED(hr) ? hr : E_FAIL;
    hr = pCategoryMgr->RegisterCategory(CLSID_TextService, GUID_TFCAT_TIP_KEYBOARD,
                                        CLSID_TextService);
    pCategoryMgr->Release();
    return hr;
}

HRESULT UnregisterCategory() {
    ITfCategoryMgr* pCategoryMgr = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_TF_CategoryMgr, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_ITfCategoryMgr,
                                  reinterpret_cast<void**>(&pCategoryMgr));
    if (FAILED(hr) || pCategoryMgr == nullptr) return FAILED(hr) ? hr : E_FAIL;
    hr = pCategoryMgr->UnregisterCategory(CLSID_TextService, GUID_TFCAT_TIP_KEYBOARD,
                                          CLSID_TextService);
    pCategoryMgr->Release();
    return hr;
}

} // namespace

// ===== 5. 全体登録 =====
HRESULT RegisterTextService() {
    HRESULT hr = RegisterComServer();
    if (FAILED(hr)) return hr;

    hr = RegisterProfile();
    if (FAILED(hr)) {
        UnregisterComServer();
        return hr;
    }

    hr = RegisterCategory();
    if (FAILED(hr)) {
        UnregisterProfile();
        UnregisterComServer();
        return hr;
    }
    return S_OK;
}

HRESULT UnregisterTextService() {
    // 全段を試み、最初の失敗を呼び出し側へ伝播させる。
    HRESULT hrResult = S_OK;
    HRESULT hr = UnregisterCategory();
    if (FAILED(hr)) hrResult = hr;
    hr = UnregisterProfile();
    if (FAILED(hr) && SUCCEEDED(hrResult)) hrResult = hr;
    hr = UnregisterComServer();
    if (FAILED(hr) && SUCCEEDED(hrResult)) hrResult = hr;
    return hrResult;
}
