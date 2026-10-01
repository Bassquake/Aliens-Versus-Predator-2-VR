@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul 2>&1
cd /d "%~dp0"
cl /nologo /EHsc unloadtest.cpp /Fe:..\bin\unloadtest.exe /Fo..\obj\ dxgi.lib user32.lib
