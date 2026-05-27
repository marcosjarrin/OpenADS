@echo off
REM DA-Web vendor download script
REM Downloads all client-side libraries so the app works without internet access.
REM Run once after cloning. Requires PowerShell 5.1+.

echo DA-Web vendor setup
echo ====================

powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$ErrorActionPreference='Stop'; ^
   $vendors = @( ^
     @{url='https://code.jquery.com/jquery-3.7.1.min.js'; dest='vendor\jquery\jquery.min.js'}, ^
     @{url='https://cdnjs.cloudflare.com/ajax/libs/jstree/3.3.16/jstree.min.js'; dest='vendor\jstree\jstree.min.js'}, ^
     @{url='https://cdnjs.cloudflare.com/ajax/libs/jstree/3.3.16/themes/default/style.min.css'; dest='vendor\jstree\themes\default\style.min.css'}, ^
     @{url='https://cdnjs.cloudflare.com/ajax/libs/jstree/3.3.16/themes/default/throbber.gif'; dest='vendor\jstree\themes\default\throbber.gif'}, ^
     @{url='https://cdnjs.cloudflare.com/ajax/libs/jstree/3.3.16/themes/default/32px.png'; dest='vendor\jstree\themes\default\32px.png'}, ^
     @{url='https://cdnjs.cloudflare.com/ajax/libs/jstree/3.3.16/themes/default/40px.png'; dest='vendor\jstree\themes\default\40px.png'}, ^
     @{url='https://unpkg.com/tabulator-tables@6.3.0/dist/js/tabulator.min.js'; dest='vendor\tabulator\js\tabulator.min.js'}, ^
     @{url='https://unpkg.com/tabulator-tables@6.3.0/dist/css/tabulator.min.css'; dest='vendor\tabulator\css\tabulator.min.css'}, ^
     @{url='https://unpkg.com/split.js@1.6.5/dist/split.min.js'; dest='vendor\split.js\split.min.js'} ^
   ); ^
   foreach ($v in $vendors) { ^
     $dir = Split-Path $v.dest; ^
     if (!(Test-Path $dir)) { New-Item -ItemType Directory -Force $dir | Out-Null } ^
     Write-Host ('Downloading ' + $v.dest + '...'); ^
     Invoke-WebRequest -Uri $v.url -OutFile $v.dest -UseBasicParsing ^
   }; ^
   Write-Host 'Done. All vendor files downloaded.'"

if %ERRORLEVEL% NEQ 0 (
  echo.
  echo ERROR: Download failed. Check internet connection and try again.
  exit /b 1
)

echo.
echo Setup complete. You can now run DA-Web offline.
