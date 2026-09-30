@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul 2>&1
cd /d "%~dp0"
cl /nologo /EHsc ddpattern.cpp /Fe:..\dgtest2\ddpattern.exe /Fo..\obj\ ddraw.lib dxguid.lib user32.lib gdi32.lib
