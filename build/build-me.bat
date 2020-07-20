@setlocal

@set DOTINST=0
@set DOINSTALL=0

@set TMPRT=..
@set TMPVER=1
@set TMPPRJ=edbrowse
@set TMPSRC=%TMPRT%
@set TMPBGN=%TIME%
@set TMPINS=..\..\3rdParty.x64
@set TMPCM=%TMPSRC%\CMakeLists.txt
@set DOPAUSE=pause

@call chkmsvc %TMPPRJ% 

@if EXIST build-cmake.bat (
@call build-cmake
)

@if NOT EXIST %TMPCM% goto NOCM

@set TMPLOG=bldlog-1.txt

@set TMPOPTS=-DCMAKE_INSTALL_PREFIX=%TMPINS%
@REM 20151031 - Add OSBC support in WIN32 build
@set TMPOPTS=%TMPOPTS% -DBUILD_EDBR_ODBC:BOOL=ON

:RPT
@if "%~1x" == "x" goto GOTCMD
@set TMPOPTS=%TMPOPTS% %1
@shift
@goto RPT
:GOTCMD

@echo Build %DATE% %TIME% > %TMPLOG%
@echo Build source %TMPSRC%... all output to build log %TMPLOG%
@echo Build source %TMPSRC%... all output to build log %TMPLOG% >> %TMPLOG%

cmake %TMPSRC% %TMPOPTS% >> %TMPLOG% 2>&1
@if ERRORLEVEL 1 goto ERR1

cmake --build . --config Debug >> %TMPLOG% 2>&1
@if ERRORLEVEL 1 goto ERR2

cmake --build . --config Release >> %TMPLOG% 2>&1
@if ERRORLEVEL 1 goto ERR3
:DONEREL

@fa4 "***" %TMPLOG%
@call elapsed %TMPBGN%
@echo Appears a successful build... see %TMPLOG%

@if "%DOINSTALL%x" == "0x" (
@echo Skipping install for now...
@goto END
)
@echo Continue with install? Only Ctrl+c aborts...
@%DOPAUSE%

cmake --build . --config Debug  --target INSTALL >> %TMPLOG% 2>&1
@if ERRORLEVEL 1 goto ERR4

cmake --build . --config Release  --target INSTALL >> %TMPLOG% 2>&1
@if ERRORLEVEL 1 goto ERR5

@echo.
@fa4 " -- " %TMPLOG%
@echo.
@call elapsed %TMPBGN%
@echo All done... see %TMPLOG%

@goto END

:NOCM
@echo Error: Can NOT locate %TMPCM%
@goto ISERR

:ERR1
@echo cmake configuration or generations ERROR
@goto ISERR

:ERR2
@echo ERROR: Cmake build Debug FAILED!
@goto ISERR

:ERR3
@fa4 "mt.exe : general error c101008d:" %TMPLOG% >nul
@if ERRORLEVEL 1 goto ERR33
:ERR34
@echo ERROR: Cmake build Release FAILED!
@goto ISERR
:ERR33
@echo Try again due to this STUPID STUPID STUPID error
@echo Try again due to this STUPID STUPID STUPID error >>%TMPLOG%
cmake --build . --config Release >> %TMPLOG% 2>&1
@if ERRORLEVEL 1 goto ERR34
@goto DONEREL

:ERR4
@echo ERROR: Cmake install Debug FAILED!
@goto ISERR

:ERR5
@echo ERROR: Cmake install Release FAILED!
@goto ISERR

:ISERR
@echo See %TMPLOG% for details...
@endlocal
@exit /b 1

:END
@endlocal
@exit /b 0

@REM eof
