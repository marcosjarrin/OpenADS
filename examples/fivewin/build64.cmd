@echo off
rem build64.cmd — build xbrowse_ads.prg with FiveWin's bcc64 toolchain,
rem linking Harbour's rddads contrib + OpenADS' ace64 import lib.
rem
rem Modelled on FWH's samples\build64.bat, with two extra link entries:
rem   %HBDIR%\lib\win\bcc64\librddads.a    (the ADSCDX/ADSNTX RDD)
rem   %HBDIR%\contrib\rddads\libace64.a    (import lib for ace64.dll)
rem
rem Run the produced xbrowse_ads.exe with OpenADS' ace64.dll first on
rem PATH (or copied next to the exe) — NOT a SAP-shipped one.
rem
rem Prereqs: FWH at %FWDIR% (or c:\fwteam), Harbour at %HBDIR%
rem (or c:\harbour), Embarcadero bcc64 at %BCDIR% (or c:\bcc7764).

setlocal
if "%FWDIR%"=="" set "FWDIR=c:\fwteam"
if "%HBDIR%"=="" set "HBDIR=c:\harbour"
if "%BCDIR%"=="" set "BCDIR=c:\bcc7764"
set "OPENADS_DLL=%~1"
if "%OPENADS_DLL%"=="" set "OPENADS_DLL=%~dp0..\..\build\default\src\Release"

set "PRG=xbrowse_ads"
set "HDIRL=%HBDIR%\lib\win\bcc64"

echo [fwh] compiling %PRG%.prg ...
"%HBDIR%\bin\win\bcc64\harbour" %PRG% /n /i"%FWDIR%\include";"%HBDIR%\include" /w /p /d__64__ || goto :err

echo [fwh] bcc64 %PRG%.c ...
"%BCDIR%\bin\bcc64" -c -w -I"%HBDIR%\include" -I"%BCDIR%\include\windows\sdk" -I"%BCDIR%\include\windows\crtl" -o%PRG%.obj %PRG%.c || goto :err

echo [fwh] linking ...
> b64.bc echo "%BCDIR%\lib\c0w64.o" +
>> b64.bc echo %PRG%.obj, +
>> b64.bc echo %PRG%.exe, +
>> b64.bc echo %PRG%.map, +
>> b64.bc echo "%FWDIR%\lib\Five64.a" "%FWDIR%\lib\FiveC64.a" +
>> b64.bc echo "%HDIRL%\hbwin.a" "%HDIRL%\gtgui.a" "%HDIRL%\hbrtl.a" "%HDIRL%\hbvm.a" +
>> b64.bc echo "%HDIRL%\hblang.a" "%HDIRL%\hbmacro.a" "%HDIRL%\hbrdd.a" +
>> b64.bc echo "%HDIRL%\rddntx.a" "%HDIRL%\rddcdx.a" "%HDIRL%\rddfpt.a" "%HDIRL%\hbsix.a" +
>> b64.bc echo "%HDIRL%\librddads.a" "%HBDIR%\contrib\rddads\libace64.a" +
>> b64.bc echo "%HDIRL%\hbdebug.a" "%HDIRL%\hbcommon.a" "%HDIRL%\hbpp.a" +
>> b64.bc echo "%HDIRL%\hbcpage.a" "%HDIRL%\hbcplr.a" "%HDIRL%\hbct.a" "%HDIRL%\hbpcre.a" +
>> b64.bc echo "%HDIRL%\hbzlib.a" +
>> b64.bc echo "%BCDIR%\lib\cw64.a" "%BCDIR%\lib\cw64mti.a" "%BCDIR%\lib\import64.a" "%BCDIR%\lib\oldnames.a" +
>> b64.bc echo ,, +
"%BCDIR%\bin\ilink64" -Gn -aa -Tpe -x @b64.bc || goto :err

echo [fwh] copying OpenADS ace64.dll next to the exe ...
copy /y "%OPENADS_DLL%\ace64.dll" . >nul 2>&1

echo [fwh] done: %PRG%.exe   (run: %PRG%.exe /auto  for a self-closing smoke run)
endlocal & exit /b 0

:err
echo [fwh] BUILD FAILED (errorlevel %errorlevel%)
endlocal & exit /b 1
