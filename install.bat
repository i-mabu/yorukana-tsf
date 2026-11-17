@echo off
setlocal EnableDelayedExpansion
rem ============================================================
rem  よるかな IME (TSF版) インストール (DLL登録)
rem  必ず「管理者として実行」してください。
rem  Release ビルド済みの YoruKana.dll を regsvr32 で登録します。
rem    x64 版  -> x64\Release\YoruKana.dll   (64bitアプリ用)
rem    Win32版 -> Release\YoruKana.dll       (32bitアプリ用)
rem  片方しか無ければある方だけ登録します。
rem ============================================================

rem --- 管理者権限チェック ---
net session >nul 2>&1
if !errorlevel! neq 0 (
    echo [エラー] 管理者権限が必要です。
    echo このファイルを右クリックして「管理者として実行」を選んでください。
    pause
    exit /b 1
)

set "ROOT=%~dp0"
set "DLL64=%ROOT%x64\Release\YoruKana.dll"
set "DLL32=%ROOT%Release\YoruKana.dll"
set "DONE=0"

if exist "%DLL64%" (
    call :RegisterDll "%SystemRoot%\System32\regsvr32.exe" "%DLL64%" x64版
) else (
    echo x64版 DLL が見つかりません ^(スキップ^): %DLL64%
)

if exist "%DLL32%" (
    call :RegisterDll "%SystemRoot%\SysWOW64\regsvr32.exe" "%DLL32%" Win32版
) else (
    echo Win32版 DLL が見つかりません ^(スキップ^): %DLL32%
)

if "%DONE%"=="0" (
    echo.
    echo [エラー] 登録できた DLL がありませんでした。
    echo 先に Visual Studio 2022 で Release ビルドしてください ^(BUILD.md 参照^)。
    pause
    exit /b 1
)

echo.
echo 登録が終わりました。
echo 設定アプリ ^> 時刻と言語 ^> 入力 ^> キーボードの詳細設定 などで
echo 日本語のキーボード一覧に「よるかな」が現れるか確認してください。
echo メモ帳を開き、Win+Space で「よるかな」に切り替えて入力してみてください。
pause
endlocal
exit /b 0

rem ============================================================
rem  :RegisterDll ^<regsvr32のパス^> ^<DLLのパス^> ^<表示名^>
rem  登録できたら DONE=1 を立てる。失敗はその場で警告する。
rem ============================================================
:RegisterDll
echo %~3を登録します: %~2
"%~1" "%~2"
if !errorlevel! neq 0 (
    echo [警告] %~3の登録でエラーが返りました。
) else (
    set "DONE=1"
)
goto :eof
