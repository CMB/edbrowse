@setlocal
@set TMP3RD=..\..\3rdParty.x64\bin
@set TMPEXE=release\edbrowse.exe
@if NOT EXIST %TMP3RD%\nul goto NO3RD
@if NOT EXIST %TMPEXE% goto NOEXE

@set TMPCMD=
:RPT
@if "%~1x" == "x" goto GOTCMD
@set TMPCMD=%TMPCMD% %1
@shift
@goto RPT
:GOTCMD

@set PATH=%TMP3RD%;%PATH%

%TMPEXE% %TMPCMD%

@goto END

:NO3RD
@echo Error: Can NOT locate %TMP3RD%! *** FIX ME ***
@echo This is the install folder for the 3rdParty DLLS
@goto END

:NOEXE
@echo Erro: No EXE %TMPEXE%! Has it been built? *** FIX ME ***
@goto END

:END

@rem eof
