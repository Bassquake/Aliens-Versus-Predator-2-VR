@echo off
rem Same as "AVP2VR_(SteamVR_runtime).bat", but uses the Meta/Oculus OpenXR runtime for this launch only.
rem Meta Quest Link 1.208's 32-bit runtime crashes every 32-bit OpenXR program in xrCreateSession (the
rem Khronos hello_xr sample too), so until Meta fixes that, use the SteamVR launcher with Link.
set XR_RUNTIME_JSON=C:\Program Files\Oculus\Support\oculus-runtime\oculus_openxr_32.json
cd /d "C:\Program Files (x86)\Fox\Aliens vs. Predator 2"
rem The display mode from avp2xr.ini [VR] GameResolution (e.g. 640x480x32), whatever the original
rem launcher (AVP2.exe) last saved: the command line overrides autoexec.cfg
set RES=
set RESARGS=
for /f "tokens=1,* delims==" %%a in ('findstr /b /i /c:"GameResolution=" "avp2xr.ini" 2^>nul') do set "RES=%%b"
if defined RES for /f "tokens=1-3 delims=x " %%w in ("%RES%x32") do set "RESARGS=+ScreenWidth %%w +ScreenHeight %%x +BitDepth %%y"
rem The launcher (AVP2.exe) writes avp2cmds.txt, the -rez list the engine loads cres.dll/cshell.dll from,
rem the first time its Play button is used; Windows may put it in VirtualStore (non-elevated launcher).
rem Without it the engine finds no resources ("Error copying file cres.dll"), so fall back to the
rem launcher's default list, keeping only the .rez files/folders that exist.
set "CMDARGS=-cmdfile avp2cmds.txt"
if exist "avp2cmds.txt" goto havecmds
if exist "%LOCALAPPDATA%\VirtualStore\Program Files (x86)\Fox\Aliens vs. Predator 2\avp2cmds.txt" goto havecmds
set REZ=
for %%r in (AVP2.rez sounds.rez Alien.rez Marine.rez Predator.rez Multi.rez AVP2dll.rez AVP2l.rez custom AVP2p.rez AVP2p2.rez AVP2P1.rez) do if exist "%%r" call set "REZ=%%REZ%% -rez %%r"
set CMDARGS=-windowtitle "Aliens vs. Predator 2"%REZ% +DisableMusic 0 +DisableSound 0 +DisableMovies 1 +EnableTripBuf 1 +DisableHardwareCursor 0
:havecmds
start "" lithtech.exe %CMDARGS% -rez vrrez %RESARGS%
