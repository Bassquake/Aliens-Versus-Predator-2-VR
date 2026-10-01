@echo off
rem Builds butetest.exe against the rewritten ButeMgr the same way cshell does (no MFC, mfcstub CString).
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul 2>&1
cd /d "%~dp0"
set LT2=..\..\..\LT2
cl /nologo /EHsc /MT /DWIN32 /DNO_PRAGMA_LIBS /D_CRT_SECURE_NO_WARNINGS /D_SILENCE_STDEXT_HASH_DEPRECATION_WARNINGS ^
   /I%LT2%\lithshared\butemgr /I%LT2%\lithshared\incs /I%LT2%\sdk\inc /I%LT2%\sdk\inc\compat ^
   butetest.cpp %LT2%\lithshared\butemgr\butemgr.cpp %LT2%\lithshared\vc6compat\vc6compat.cpp /Fe:butetest.exe ^
   /link /NODEFAULTLIB:libcimt.lib %LT2%\lithshared\libs\release\MFCStub.lib %LT2%\lithshared\libs\release\StdLith.lib legacy_stdio_definitions.lib
