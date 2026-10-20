@echo off
:: Starts the console program or the web dashboard. (build.bat must have been run once.)
cd /d "%~dp0"

if not exist smarthome.exe goto notbuilt
if not exist server.exe goto notbuilt

:menu
echo.
echo  1  Console program      (smarthome.exe)
echo  2  Web dashboard        (server.exe + browser)
echo  0  Exit
set /p pick=Choose: 
if "%pick%"=="1" smarthome.exe
if "%pick%"=="2" (
  start "" "http://127.0.0.1:8080"
  echo Server running. Keep this window open. Press Ctrl+C to stop.
  server.exe
)
if "%pick%"=="0" exit /b 0
goto menu

:notbuilt
echo smarthome.exe or server.exe not found. Run build.bat first.
pause
