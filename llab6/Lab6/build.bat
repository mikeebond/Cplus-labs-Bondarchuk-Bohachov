@echo off
rem Запускати з "x64 Native Tools Command Prompt for VS"
cl /nologo /utf-8 /DUNICODE /D_UNICODE server.c user32.lib /Feserver.exe
cl /nologo /utf-8 /DUNICODE /D_UNICODE /DCLIENT_NO=1 client.c user32.lib gdi32.lib /Feclient1.exe
cl /nologo /utf-8 /DUNICODE /D_UNICODE /DCLIENT_NO=2 client.c user32.lib gdi32.lib /Feclient2.exe
del *.obj 2>nul