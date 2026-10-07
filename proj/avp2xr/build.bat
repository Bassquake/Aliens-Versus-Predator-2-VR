@echo off
rem Builds the 32-bit avp2xr proxy d3d11.dll into bin\.
setlocal
set VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat
call "%VCVARS%" >nul || exit /b 1
cd /d "%~dp0"
if not exist bin mkdir bin
if not exist obj mkdir obj
rem Must not link d3d11.lib or dxgi.lib: this DLL *is* d3d11.dll.
cl /nologo /O2 /MT /W3 /EHsc /Zi /I..\thirdparty\openxr_loader_windows\include /Foobj\ /Fdobj\ ^
   avp2xr.cpp /LD /Fe:bin\d3d11.dll /link /DEF:d3d11.def /MAP:obj\d3d11.map /DEBUG /OPT:REF /OPT:ICF user32.lib || exit /b 1
copy /y ..\thirdparty\openxr_loader_windows\Win32\bin\openxr_loader.dll bin\ >nul
echo Built bin\d3d11.dll
rem The launchers' render resolution helper (see xrres.cpp)
cl /nologo /O2 /MT /W3 /EHsc /I..\thirdparty\openxr_loader_windows\include /Foobj\ /Fdobj\ ^
   xrres.cpp /Fe:bin\xrres.exe /link /MANIFEST:EMBED /OPT:REF /OPT:ICF || exit /b 1
echo Built bin\xrres.exe
