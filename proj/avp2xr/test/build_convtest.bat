@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul 2>&1
cd /d "%~dp0"
cl /nologo /EHsc /I..\..\LT2\sdk\inc convtest.cpp ..\..\LT2\sdk\inc\ltquatbase.cpp /Fe:..\bin\convtest.exe /Fo..\obj\
