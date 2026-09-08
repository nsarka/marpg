@echo off
setlocal
cd /d "%~dp0bin"
if errorlevel 1 exit /b 1
"client.exe"
if errorlevel 1 pause
