@echo off
rem Build the smoke test against Harbour's msvc64 rddads.lib +
rem OpenADS-built ace64.lib import library + ace64.dll on PATH.
rem
rem Usage: build.cmd <openads_build_dir>
rem   <openads_build_dir> defaults to ..\..\..\build\default\src\Release.
rem
rem Run from a Developer Command Prompt (or after vcvars64.bat) so
rem hbmk2 can find cl.exe / link.exe.

setlocal
set HBROOT=c:\harbour
set OPENADS_LIB=%~1
if "%OPENADS_LIB%"=="" set OPENADS_LIB=%~dp0..\..\..\build\default\src\Release

set PATH=%HBROOT%\bin\win\msvc64;%OPENADS_LIB%;%PATH%

echo [smoke] hbmk2 build...
hbmk2 -comp=msvc64 -i"%HBROOT%\contrib\rddads" -lrddads -L"%OPENADS_LIB%" -lace64 -info smoke.prg
endlocal
