@echo off
setlocal
cd /d "%~dp0bin"
if errorlevel 1 exit /b 1
if exist "server.pending.exe" (
  powershell -NoProfile -Command "Move-Item -LiteralPath 'server.pending.exe' -Destination 'server.exe' -Force -ErrorAction Stop"
  if errorlevel 1 (
    echo Close the existing server window, then run server.bat again.
    pause
    exit /b 1
  )
)
"server.exe"
if errorlevel 1 pause
