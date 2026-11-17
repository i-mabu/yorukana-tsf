@echo off
setlocal
rem ============================================================
rem  よるかな IME (TSF版) アンインストール (DLL登録解除)
rem  必ず「管理者として実行」してください。
rem  先に Win+Space で別の IME (Microsoft IME 等) に切り替えてから
rem  実行すると確実です。
rem ============================================================

rem --- 管理者権限チェック ---
net session >nul 2>&1
if not %errorlevel%==0 (
    echo [エラー] 管理者権限が必要です。
    echo このファイルを右クリックして「管理者として実行」を選んでください。
    pause
    exit /b 1
)

set "ROOT=%~dp0"
set "DLL64=%ROOT%x64\Release\YoruKana.dll"
set "DLL32=%ROOT%Release\YoruKana.dll"

if exist "%DLL64%" (
    echo x64版の登録を解除します: %DLL64%
    "%SystemRoot%\System32\regsvr32.exe" /u "%DLL64%"
)
if exist "%DLL32%" (
    echo Win32版の登録を解除します: %DLL32%
    "%SystemRoot%\SysWOW64\regsvr32.exe" /u "%DLL32%"
)

echo.
echo 登録解除が終わりました。
echo DLL ファイル自体は残っています。不要ならこのフォルダごと削除してください。
pause
endlocal
