@echo off
rem Removes everything install.bat added to the AVP2 folder. Run as administrator.
setlocal
set GAME=C:\Program Files (x86)\Fox\Aliens vs. Predator 2
net session >nul 2>&1 || (echo Run this as administrator. & pause & exit /b 1)
for %%f in (DDraw.dll D3DImm.dll dgVoodoo.conf d3d11.dll openxr_loader.dll avp2xr.ini avp2xr.ini.bak avp2xr.log) do if exist "%GAME%\%%f" del "%GAME%\%%f" && echo Removed %%f
if exist "%GAME%\vrrez\cshell.dll" del "%GAME%\vrrez\cshell.dll" && echo Removed vrrez\cshell.dll
if exist "%GAME%\vrrez\Models" rmdir /s /q "%GAME%\vrrez\Models" && echo Removed vrrez\Models
if exist "%GAME%\vrrez" rmdir "%GAME%\vrrez" 2>nul
pause
