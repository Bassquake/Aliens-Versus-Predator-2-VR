@echo off
rem Diagnostic: the VR cshell with stereo rendering switched off (VREnable 0). The game shows on the
rem flat virtual screen like AVP2.exe, but runs the VR cshell from the game folder's vrrez.
rem SteamVR's 32-bit OpenXR runtime, found as in "AVP2VR_(SteamVR_runtime).bat".
set XR_RUNTIME_JSON=
set "SVR="
for /f "usebackq delims=" %%p in (`powershell -NoProfile -Command "try { (Get-Content -Raw -LiteralPath (Join-Path $env:LOCALAPPDATA 'openvr\openvrpaths.vrpath') | ConvertFrom-Json).runtime | Where-Object { Test-Path -LiteralPath (Join-Path $_ 'steamxr_win32.json') } | Select-Object -First 1 } catch {}" 2^>nul`) do set "SVR=%%p"
if not defined SVR for /f "tokens=2,*" %%a in ('reg query "HKLM\SOFTWARE\WOW6432Node\Valve\Steam" /v InstallPath 2^>nul ^| findstr /i "InstallPath"') do if exist "%%b\steamapps\common\SteamVR\steamxr_win32.json" set "SVR=%%b\steamapps\common\SteamVR"
if defined SVR set "XR_RUNTIME_JSON=%SVR%\steamxr_win32.json"
if not defined SVR echo SteamVR was not found; using the system's active OpenXR runtime.
cd /d "C:\Program Files (x86)\Fox\Aliens vs. Predator 2"
rem The display mode from avp2xr.ini [VR] GameResolution (e.g. 640x480x32), whatever the original
rem launcher (AVP2.exe) last saved: the command line overrides autoexec.cfg
set RES=
set RESARGS=
for /f "tokens=1,* delims==" %%a in ('findstr /b /i /c:"GameResolution=" "avp2xr.ini" 2^>nul') do set "RES=%%b"
if defined RES for /f "tokens=1-3 delims=x " %%w in ("%RES%x32") do set "RESARGS=+ScreenWidth %%w +ScreenHeight %%x +BitDepth %%y"
start "" lithtech.exe -cmdfile avp2cmds.txt -rez vrrez %RESARGS% +VREnable 0
