@echo off
rem Copies dgVoodoo2 and the avp2xr D3D11/OpenXR hook into the AVP2 folder.
rem Run as administrator. Also installs the settings: edit deploy\avp2xr.ini (no admin needed) and
rem run this again to apply a change.
setlocal
set GAME=C:\Program Files (x86)\Fox\Aliens vs. Predator 2
net session >nul 2>&1 || (echo Run this as administrator. & pause & exit /b 1)
for %%f in (DDraw.dll D3DImm.dll dgVoodoo.conf d3d11.dll openxr_loader.dll) do (
	copy /y "%~dp0deploy\%%f" "%GAME%\" >nul || (echo Failed to copy %%f & pause & exit /b 1)
	echo Installed %%f
)
rem The log now lives in %LOCALAPPDATA%\avp2xr; remove the old, never-updated copy here.
if exist "%GAME%\avp2xr.log" del "%GAME%\avp2xr.log"
rem Settings: deploy\avp2xr.ini is the one to edit; the game folder's previous copy is kept as .bak
if exist "%GAME%\avp2xr.ini" copy /y "%GAME%\avp2xr.ini" "%GAME%\avp2xr.ini.bak" >nul
copy /y "%~dp0deploy\avp2xr.ini" "%GAME%\" >nul || (echo Failed to copy avp2xr.ini & pause & exit /b 1)
echo Installed avp2xr.ini
rem The VR cshell.dll goes in its own vrrez folder in the game folder: the engine only loads game
rem DLLs from its -rez list (loose ones in the game folder are ignored), so the "AVP2VR" launchers
rem add that folder with -rez vrrez, and its cshell.dll overrides the retail one in AVP2DLL.REZ.
if not exist "%GAME%\vrrez" mkdir "%GAME%\vrrez"
copy /y "%~dp0deploy\vrrez\cshell.dll" "%GAME%\vrrez\" >nul || (echo Failed to copy vrrez\cshell.dll & pause & exit /b 1)
echo Installed vrrez\cshell.dll
rem Model fixes for VR (tools\normalref.py): copies of game models in the same place under vrrez,
rem which override the game's own
if exist "%~dp0deploy\vrrez\Models" (
	xcopy /s /i /y /q "%~dp0deploy\vrrez\Models" "%GAME%\vrrez\Models" >nul || (echo Failed to copy vrrez\Models & pause & exit /b 1)
	echo Installed vrrez\Models
)
pause
