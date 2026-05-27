@echo off
echo DA-Web vendor setup
echo ====================
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0setup.ps1"
if %ERRORLEVEL% NEQ 0 (
  echo.
  echo ERROR: Download failed. Check internet connection and try again.
  exit /b 1
)
echo.
echo Setup complete. You can now run DA-Web offline.
