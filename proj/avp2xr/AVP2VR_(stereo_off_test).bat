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
rem Render resolution: xrres.exe sets dgVoodoo's forced resolution (both eyes side by side) to the runtime's
rem recommended per-eye size (SteamVR's render resolution) times avp2xr.ini RenderScale, in the VirtualStore
rem copy of dgVoodoo.conf the game reads. If it can't (headset asleep), the last resolution stays.
if exist xrres.exe xrres.exe
rem The display mode from avp2xr.ini [VR] GameResolution (e.g. 640x480x32), whatever the original
rem launcher (AVP2.exe) last saved: the command line overrides autoexec.cfg
set RES=
set RESARGS=
for /f "tokens=1,* delims==" %%a in ('findstr /b /i /c:"GameResolution=" "avp2xr.ini" 2^>nul') do set "RES=%%b"
if defined RES for /f "tokens=1-3 delims=x " %%w in ("%RES%x32") do set "RESARGS=+ScreenWidth %%w +ScreenHeight %%x +BitDepth %%y"
rem The -rez list the engine loads cres.dll/cshell.dll from is in avp2cmds.txt, which the launcher (AVP2.exe)
rem writes the first time its Play button is used, into VirtualStore when it isn't elevated (lithtech.exe
rem reads that copy first, cmd doesn't see it, so look there explicitly). Master Server Patch 2.4's
rem avp2p5.rez in it is fine: its maps and art are used, and vrrez (last, so it wins) replaces its
rem cshell.dll, object.lto, cres.dll and sres.dll with ones built from the 1.0.9.6 source.
rem Without avp2cmds.txt the engine finds no resources ("Error copying file cres.dll"), so fall back to the
rem launcher's default list, keeping only the .rez files/folders that exist.
set "CMDFILE=%LOCALAPPDATA%\VirtualStore\Program Files (x86)\Fox\Aliens vs. Predator 2\avp2cmds.txt"
if not exist "%CMDFILE%" set "CMDFILE=avp2cmds.txt"
set CMDARGS=
if exist "%CMDFILE%" for /f "usebackq delims=" %%l in ("%CMDFILE%") do set "CMDARGS=%%l"
if defined CMDARGS goto havecmds
set REZ=
for %%r in (AVP2.rez sounds.rez Alien.rez Marine.rez Predator.rez Multi.rez AVP2dll.rez AVP2l.rez custom AVP2p.rez AVP2p2.rez AVP2P1.rez AVP2P5.rez) do if exist "%%r" call set "REZ=%%REZ%% -rez %%r"
set CMDARGS=-windowtitle "Aliens vs. Predator 2"%REZ% +DisableMusic 0 +DisableSound 0 +DisableMovies 1 +EnableTripBuf 1 +DisableHardwareCursor 0
:havecmds
start "" lithtech.exe %CMDARGS% -rez vrrez %RESARGS% +VREnable 0
