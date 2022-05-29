@echo off

rem This is where maxcso (usually don't need to change.)
set maxcso=%~dp0\maxcso.exe

if not exist "%maxcso%" (
	rem In case it was kept in "examples"...
	set maxcso=%~dp0\..\maxcso.exe
)
if not exist "%maxcso%" (
	echo Could not find maxcso.exe
	exit /b 1
)

"%maxcso%" --block=32768 %*
pause
