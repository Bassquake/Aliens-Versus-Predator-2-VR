@echo off
rem Removes everything Install_avp2vr.bat added to the AVP2 folder. Run as administrator.
setlocal
set GAME=C:\Program Files (x86)\Fox\Aliens vs. Predator 2
net session >nul 2>&1 || (echo Run this as administrator. & pause & exit /b 1)
for %%f in (DDraw.dll D3DImm.dll dgVoodoo.conf d3d11.dll openxr_loader.dll xrres.exe "AVP2VR_(SteamVR_runtime).bat" avp2xr.ini avp2xr.ini.bak avp2xr.log) do if exist "%GAME%\%%~f" del "%GAME%\%%~f" && echo Removed %%~f
for %%f in (cshell.dll object.lto cres.dll sres.dll) do if exist "%GAME%\vrrez\%%f" del "%GAME%\vrrez\%%f" && echo Removed vrrez\%%f
if exist "%GAME%\vrrez\Models" rmdir /s /q "%GAME%\vrrez\Models" && echo Removed vrrez\Models
if exist "%GAME%\vrrez" rmdir "%GAME%\vrrez" 2>nul
rem The copy of dgVoodoo.conf with the render resolution, which xrres.exe (run by the launchers) writes
set "VSCONF=%LOCALAPPDATA%\VirtualStore\Program Files (x86)\Fox\Aliens vs. Predator 2\dgVoodoo.conf"
if exist "%VSCONF%" del "%VSCONF%" && echo Removed the VirtualStore copy of dgVoodoo.conf
pause
rem Last, the copy of this script that Install_avp2vr.bat put in the game folder: (goto) ends the
rem batch first, so the copy can delete itself when that's the one being run
if exist "%GAME%\Uninstall_avp2vr.bat" (goto) 2>nul & del "%GAME%\Uninstall_avp2vr.bat"
