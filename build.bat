@echo off
chcp 65001 >nul
rem ============================================================
rem  よるかな IME (TSF版) — ワンコマンドビルド (統一入口)
rem  使い方: build.bat を実行するだけ
rem    Release x64 / x86 をビルドし、成果物 DLL のパスを表示します
rem  必要なもの: Visual Studio 2022
rem    (ワークロード「C++ によるデスクトップ開発」+ Windows SDK)
rem  実体は build-release.bat です (MSBuild の探索とエラー処理はそちら)。
rem ============================================================
call "%~dp0build-release.bat" %*
exit /b %errorlevel%
