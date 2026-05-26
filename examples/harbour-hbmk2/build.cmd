@echo off
rem build.cmd — convenience wrapper around hbmk2 openads_demo.hbp.
rem Sets OPENADS_LIB, puts ace64.dll on PATH for the run, invokes hbmk2.
rem
rem Usage:
rem   build.cmd                        :: defaults to ..\..\build\default\src\Release
rem   build.cmd C:\path\to\openads-release
rem
rem Prereqs: Harbour 3.2 at %HB_INSTALL% (default c:\harbour), with
rem contrib/rddads built for msvc64; an MSVC x64 environment on PATH
rem (run from a Developer Command Prompt or after vcvars64.bat).

setlocal
if "%HB_INSTALL%"=="" set "HB_INSTALL=c:\harbour"
set "OPENADS_LIB=%~1"
if "%OPENADS_LIB%"=="" set "OPENADS_LIB=%~dp0..\..\build\default\src\Release"

set "PATH=%HB_INSTALL%\bin\win\msvc64;%OPENADS_LIB%;%PATH%"

echo [hbmk2] building openads_demo.exe (OPENADS_LIB=%OPENADS_LIB%) ...
hbmk2 openads_demo.hbp || goto :err

echo [hbmk2] copying OpenADS ace64.dll next to the exe ...
copy /y "%OPENADS_LIB%\ace64.dll" . >nul 2>&1

echo [hbmk2] done. Run:  openads_demo.exe
endlocal & exit /b 0

:err
echo [hbmk2] BUILD FAILED (errorlevel %errorlevel%)
endlocal & exit /b 1
