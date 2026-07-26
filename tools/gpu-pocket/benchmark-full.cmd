@echo off
setlocal
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0test-machine.ps1" -Mode full
echo.
echo Result files are in "%~dp0results".
pause
exit /b %ERRORLEVEL%
