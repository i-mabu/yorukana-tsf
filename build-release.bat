@echo off
setlocal
rem ============================================================
rem  よるかな IME (TSF版) Release ビルド補助スクリプト (任意)
rem  Visual Studio 2022 の MSBuild を探して Release x64 / x86 を
rem  ビルドします。Visual Studio から直接ビルドしても構いません。
rem ============================================================

set "ROOT=%~dp0"
set "SLN=%ROOT%YoruKana.sln"
set "MSBUILD="

rem --- vswhere で MSBuild を探す ---
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%VSWHERE%" (
    for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe`) do set "MSBUILD=%%i"
)

rem --- 見つからなければ既知のパスを当たる ---
if not defined MSBUILD if exist "%ProgramFiles%\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" set "MSBUILD=%ProgramFiles%\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"
if not defined MSBUILD if exist "%ProgramFiles%\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe" set "MSBUILD=%ProgramFiles%\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe"
if not defined MSBUILD if exist "%ProgramFiles%\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe" set "MSBUILD=%ProgramFiles%\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe"
if not defined MSBUILD if exist "%ProgramFiles%\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe" set "MSBUILD=%ProgramFiles%\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe"

if not defined MSBUILD (
    echo [エラー] MSBuild が見つかりませんでした。
    echo Visual Studio 2022「C++ によるデスクトップ開発」ワークロードを
    echo インストールしてから再実行するか、Visual Studio で YoruKana.sln を
    echo 開いて手動でビルドしてください ^(BUILD.md 参照^)。
    pause
    exit /b 1
)

echo MSBuild: %MSBUILD%
echo.
echo === Release x64 をビルドします ===
"%MSBUILD%" "%SLN%" /m /p:Configuration=Release /p:Platform=x64
if not %errorlevel%==0 (
    echo [エラー] x64 ビルドが失敗しました。上のエラーを確認してください。
    pause
    exit /b 1
)

echo.
echo === Release x86 (Win32) をビルドします ===
"%MSBUILD%" "%SLN%" /m /p:Configuration=Release /p:Platform=x86
if not %errorlevel%==0 (
    echo [警告] x86 ビルドが失敗しました。x64 版だけでも 64bit アプリでは使えます。
    echo 32bit アプリでも使いたい場合はエラーを解消して再ビルドしてください。
)

echo.
echo ビルド完了。成果物:
echo   x64版 : %ROOT%x64\Release\YoruKana.dll
echo   Win32版: %ROOT%Release\YoruKana.dll
echo 次に install.bat を「管理者として実行」して登録してください。
pause
endlocal
