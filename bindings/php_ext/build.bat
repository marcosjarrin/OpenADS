@echo off
setlocal

echo Setting up VS2022 x64 build environment...
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1

echo Building php_openads extension (ZTS x64)...
nmake /f Makefile.win %*
set BUILD_ERR=%ERRORLEVEL%

if %BUILD_ERR% NEQ 0 goto :end

echo Patching PE linker version to 14.28 (matching PHP 8.0.1 VS16 build)...
powershell -NoProfile -ExecutionPolicy Bypass -File "F:\php_advantage\patch_linker_ver.ps1" ^
    "F:\OpenADS\bindings\php_ext\bin\php_openads.dll"

:end
endlocal
exit /b %BUILD_ERR%
