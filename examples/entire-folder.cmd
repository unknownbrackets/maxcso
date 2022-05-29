@echo off

rem Put a full path here starting with drive letter (leave blank and it will ask.)
set inpath=
set outpath=
rem Put any extra args (like --fast or --block=32768) here.
set args=
rem This is where maxcso (usually don't need to change.)
set maxcso=%~dp0\maxcso.exe

if "%inpath%" == "" (
	set /p inpath="Input path (ISOs): "
)
if not exist "%inpath%" (
	echo Please create and populate this folder first: %inpath%
	exit /b 1
)

if "%outpath%" == "" (
	set /p outpath="Output path (CSOs): "
)
if not exist "%outpath%" (
	echo Please create this folder first: %outpath%
	exit /b 1
)

if not exist "%maxcso%" (
	rem In case it was kept in "examples"...
	set maxcso=%~dp0\..\maxcso.exe
)
if not exist "%maxcso%" (
	echo Could not find maxcso.exe
	exit /b 1
)

for /r "%inpath%" %%f in (*.iso) do (
	"%maxcso%" %args% -o "%outpath%/%%~nf.cso" "%%f"
)
pause
