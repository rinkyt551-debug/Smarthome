@echo off
:: Compiles both programs. Run this once, and again after you edit a .cpp file.
cd /d "%~dp0"

echo Building smarthome.exe (console program)...
g++ -std=c++11 -Wall -o smarthome.exe smarthome.cpp
if errorlevel 1 goto failed

echo Building server.exe (web server)...
g++ -std=c++11 -Wall -o server.exe server.cpp -lws2_32
if errorlevel 1 goto failed

echo.
echo Build OK. Now run run.bat
pause
exit /b 0

:failed
echo.
echo Build failed. Read the compiler errors above.
pause
exit /b 1
