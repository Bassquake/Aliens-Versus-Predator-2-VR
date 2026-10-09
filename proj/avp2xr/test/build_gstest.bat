@echo off
rem Builds gstest.exe (see gstest.cpp) with the settings AVP2.props gives the game for the VC6 libraries
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul 2>&1
cd /d "%~dp0"
cl /nologo /EHsc /MT /Zc:wchar_t- /D_CRT_SECURE_NO_WARNINGS /I..\..\LT2\lithshared\incs /I..\..\LT2\lithshared\wonapi ^
   gstest.cpp ..\..\LT2\lithshared\vc6compat\vc6compat.cpp /Fe:..\bin\gstest.exe /Fo..\obj\ ^
   /link /SAFESEH:NO /NODEFAULTLIB:libcimt.lib /LIBPATH:..\..\LT2\lithshared\libs\release ^
   GameSpyClientMgr.lib WONAPI.lib legacy_stdio_definitions.lib wsock32.lib ws2_32.lib user32.lib advapi32.lib
