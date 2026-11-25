@echo off
setlocal DisableDelayedExpansion
title Vista Watch Together setup
set "VISTA_EXTENSION_DIR=%~dp0extension"

if not exist "%VISTA_EXTENSION_DIR%\manifest.json" (
  echo Extract the whole ZIP to a folder, then run this script from that folder.
  pause
  exit /b 1
)

echo.
echo Vista Watch Together for Chrome
echo.
echo 1. In Chrome, press Ctrl+L, type chrome://extensions and press Enter.
echo 2. Turn on Developer mode and click Load unpacked.
echo    In the folder picker, paste the following path
echo    into the address bar, press Enter, then click Select Folder.
echo    "%VISTA_EXTENSION_DIR%"
echo 3. From the puzzle-piece menu, open Vista Watch Together.
echo    Click Save and allow access to https://watch.notice.photos.
echo 4. Open or refresh https://web.stremio.com and join with your room code.
echo.
echo Keep this extracted folder in place so Chrome can load the extension.
echo.
powershell.exe -NoProfile -Command "Set-Clipboard -Value $env:VISTA_EXTENSION_DIR" >nul 2>&1
if not errorlevel 1 echo The extension folder path was copied to your clipboard.

start "" chrome.exe
if errorlevel 1 echo Open Chrome manually to start step 1.
echo.
pause
