@echo off
rem Starts AVP2 in VR with SteamVR's OpenXR runtime, whichever runtime is the system's active one.
rem Runs lithtech.exe directly with the launcher's saved settings (avp2cmds.txt) plus an extra
rem resource folder, vrrez (installed into the game folder by Install_avp2vr.bat), whose cshell.dll (the VR
rem build) overrides the retail one in AVP2DLL.REZ.
rem Steam (running this as a non-Steam VR game) sets XR_RUNTIME_JSON to the 64-bit SteamVR runtime,
rem which the 32-bit game can't load, and with Meta Quest Link as the active runtime the registered
rem 32-bit one is Meta's, which crashes 32-bit games (Link 1.208). So point XR_RUNTIME_JSON at
rem SteamVR's 32-bit runtime: SteamVR's folder is in openvrpaths.vrpath, else under Steam's own.
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
rem The -rez list the engine loads cres.dll/cshell.dll from is in avp2cmds.txt, which the launcher (AVP2.exe)
rem writes the first time its Play button is used, into VirtualStore when it isn't elevated (lithtech.exe
rem reads that copy first, cmd doesn't see it, so look there explicitly). Its line is passed on the command
rem line without Master Server Patch 2.4's avp2p5.rez: that holds the patch's cshell.dll/object.lto, and its
rem object.lto (game code for local servers) doesn't match the 1.0.9.6 source the VR cshell is built from.
rem Without avp2cmds.txt the engine finds no resources ("Error copying file cres.dll"), so fall back to the
rem launcher's default list, keeping only the .rez files/folders that exist.
set "CMDFILE=%LOCALAPPDATA%\VirtualStore\Program Files (x86)\Fox\Aliens vs. Predator 2\avp2cmds.txt"
if not exist "%CMDFILE%" set "CMDFILE=avp2cmds.txt"
set CMDARGS=
if exist "%CMDFILE%" for /f "usebackq delims=" %%l in ("%CMDFILE%") do set "CMDARGS=%%l"
if defined CMDARGS goto havecmds
set REZ=
for %%r in (AVP2.rez sounds.rez Alien.rez Marine.rez Predator.rez Multi.rez AVP2dll.rez AVP2l.rez custom AVP2p.rez AVP2p2.rez AVP2P1.rez) do if exist "%%r" call set "REZ=%%REZ%% -rez %%r"
set CMDARGS=-windowtitle "Aliens vs. Predator 2"%REZ% +DisableMusic 0 +DisableSound 0 +DisableMovies 1 +EnableTripBuf 1 +DisableHardwareCursor 0
:havecmds
set "CMDARGS=%CMDARGS:-rez avp2p5.rez=%"
start "" lithtech.exe %CMDARGS% -rez vrrez %RESARGS%
