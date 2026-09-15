@echo off
setlocal
pushd "%~dp0"
chcp 65001 >nul
title AM5 Native Memory Lab
echo Kernel self-test only
echo Keep other benchmarks and games closed.
echo.
"%~dp0AM5MemoryLab.exe" --self-test
set "rc=%errorlevel%"
echo.
echo Exit code: %rc%
echo Results are saved under the results folder.
pause
popd
exit /b %rc%
