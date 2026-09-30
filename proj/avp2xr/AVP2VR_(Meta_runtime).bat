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
start "" lithtech.exe -cmdfile avp2cmds.txt -rez vrrez %RESARGS%
