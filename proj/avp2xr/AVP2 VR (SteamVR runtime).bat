@echo off
rem Starts AVP2 in VR with the system's active OpenXR runtime (SteamVR).
rem Runs lithtech.exe directly with the launcher's saved settings (avp2cmds.txt) plus an extra
rem resource folder, vrrez (installed into the game folder by install.bat), whose cshell.dll (the VR
rem build) overrides the retail one in AVP2DLL.REZ.
rem Steam (running this as a non-Steam VR game) sets XR_RUNTIME_JSON to the 64-bit SteamVR runtime,
rem which the 32-bit game can't load; cleared, the registered 32-bit SteamVR runtime is used.
set XR_RUNTIME_JSON=
cd /d "C:\Program Files (x86)\Fox\Aliens vs. Predator 2"
rem The display mode from avp2xr.ini [VR] GameResolution (e.g. 640x480x32), whatever the original
rem launcher (AVP2.exe) last saved: the command line overrides autoexec.cfg
set RES=
set RESARGS=
for /f "tokens=1,* delims==" %%a in ('findstr /b /i /c:"GameResolution=" "avp2xr.ini" 2^>nul') do set "RES=%%b"
if defined RES for /f "tokens=1-3 delims=x " %%w in ("%RES%x32") do set "RESARGS=+ScreenWidth %%w +ScreenHeight %%x +BitDepth %%y"
start "" lithtech.exe -cmdfile avp2cmds.txt -rez vrrez %RESARGS%
