@echo off
rem Build + run the field-length probe (Pritpal mini_xbrowse /ads report).
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
set ACE=C:\OpenADS\build\release-x64\src\Release
set PATH=c:\harbour\bin\win\msvc64;%ACE%;%PATH%
cd /d "%~dp0"

echo [probe] hbmk2 build...
hbmk2 -comp=msvc64 -gtcgi -I"c:\harbour\contrib\rddads" -lrddads -L"%ACE%" -lace64 -llegacy_stdio_definitions -loldnames fieldlenprobe.prg
if errorlevel 1 goto :err

echo.
echo [probe] === DBFCDX baseline ===
.\fieldlenprobe.exe
echo.
echo [probe] === ADSCDX -^> OpenADS ===
.\fieldlenprobe.exe /ads
goto :eof

:err
echo BUILD FAILED with errorlevel %errorlevel%
exit /b %errorlevel%
