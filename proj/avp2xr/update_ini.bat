@echo off
rem Copies only avp2xr.ini into the AVP2 folder (run as administrator). Works while the game is running,
rem unlike Install_avp2vr.bat (the DLLs are in use then): e.g. to try [LeftHand] settings, which are read at each
rem change of weapon.
setlocal
set GAME=C:\Program Files (x86)\Fox\Aliens vs. Predator 2
net session >nul 2>&1 || (echo Run this as administrator. & pause & exit /b 1)
copy /y "%~dp0deploy\avp2xr.ini" "%GAME%\" >nul || (echo Failed to copy avp2xr.ini & pause & exit /b 1)
echo Installed avp2xr.ini
pause
