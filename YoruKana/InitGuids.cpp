// ===== 1. GUID 実体化専用の翻訳単位 =====
// INITGUID を定義した状態で msctf.h と Guids.h を include することで、
// TSF のシステム GUID (GUID_TFCAT_TIP_KEYBOARD, CLSID_TF_InputProcessorProfiles 等) と
// よるかな独自 GUID の実体をこのファイルにだけ生成する。
// 他の .cpp では INITGUID を定義してはいけない。
#define INITGUID
#include <windows.h>
#include <msctf.h>
#include "Guids.h"
#undef INITGUID
