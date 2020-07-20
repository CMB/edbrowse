@setlocal
@REM set TMPFIL=jsrt
@REM set TMPFIL=jsrt.html
@set TMPFIL=..\src\jsrt
@if NOT EXIST %TMPFIL% goto NOFIL
@if NOT EXIST run-exe.bat goto NOBAT
@echo.
@echo On load 23493
@echo After load, add cmd 'b'
@echo Should see
@echo doc loader attached
@echo body loading
@echo form questionnaire loading
@echo 630, was 621
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

