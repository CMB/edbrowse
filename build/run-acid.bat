@setlocal
@REM set TMPFIL=jsrt
@REM set TMPFIL=jsrt.html
@set TMPFIL=..\src\acid3
@if NOT EXIST %TMPFIL% goto NOFIL
@if NOT EXIST run-exe.bat goto NOBAT
@echo.
@echo On load ?????
@echo set debug, 'db5'... 'db9'
@echo redirect debugging output to a file 'db^>tempeb.txt' 
@echo add cmd 'b'
@echo.
@echo Use 'qt' to exit...
@echo.

call run-exe %TMPFIL%

@goto END

:NOFIL
@echo Error: Can NOT locate %TMPFIL%! *** FIX ME ***
@goto END

:NOBAT
@echo Error: Can NOT locate run-exe.bat! *** FIX ME ***
@goto END


:END

