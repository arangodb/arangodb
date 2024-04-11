@echo off
REM Copyright (C) 2016 and later: Unicode, Inc. and others.
REM License & terms of use: http://www.unicode.org/copyright.html
REM  ********************************************************************

REM Don't add additional global environment variables, keep the variables local to this script.
rem setlocal

set ICU_ARCH=%1
set ICU_DBRL=%2

if "%1" == "" (
 echo Usage: %0 "x86 or x64"  "Debug or Release"
 exit /b 1
)

if "%2" == "" (
 echo Usage: %0 %1 "Debug or Release"
 exit /b 1
)

set ICU_ICUDIR="%~dp0"\..\..\..
set ICU_SAMPLESDIR=%ICU_ICUDIR%\source\samples

if "%ICU_ARCH%" == "x64" (
 set ICU_BINDIR=%~dp0..\..\..\bin64
) else (
 set ICU_BINDIR=%~dp0..\..\..\bin
)

if not exist "%ICU_BINDIR%" (
 echo Error '%ICU_BINDIR%' does not exist!
 echo Have you built all of ICU yet ?
 goto :eof
)

REM Change the codepage to UTF-8 in order to better handle non-ASCII characters from the samples.
echo Setting codepage to UTF-8
chcp 65001

echo Testing ICU samples in %ICU_ICUDIR%  arch=%ICU_ARCH% type=%ICU_DBRL%
set PATH=%ICU_BINDIR%;%PATH%
pushd %ICU_BINDIR%

set SAMPLE_COLL=%ICU_SAMPLESDIR%\coll\%ICU_ARCH%\%ICU_DBRL%\coll.exe
set SAMPLE_STRSRCH=%ICU_SAMPLESDIR%\strsrch\%ICU_ARCH%\%ICU_DBRL%\strsrch.exe
set SAMPLE_TRANSLIT=%ICU_SAMPLESDIR%\translit\%ICU_ARCH%\%ICU_DBRL%\translit.exe
set SAMPLE_UGREP=%ICU_SAMPLESDIR%\ugrep\%ICU_ARCH%\%ICU_DBRL%\ugrep.exe


@set THT=coll
@echo.
@echo ==== %THT% =========================================================================
%SAMPLE_COLL%
if ERRORLEVEL 1 goto :SampleError

@set THT=strsrch
@echo.
@echo ==== %THT% =========================================================================
%SAMPLE_STRSRCH%
if ERRORLEVEL 1 goto :SampleError

@set THT=translit
@echo.
@echo ==== %THT% =========================================================================
%SAMPLE_TRANSLIT%
if ERRORLEVEL 1 goto :SampleError

@set THT=ugrep
@echo.
@echo ==== %THT% =========================================================================
echo Looking for "ICU" in '%ICU_SAMPLESDIR%\ugrep\readme.txt' with ugrep.exe 
%SAMPLE_UGREP% ICU %ICU_SAMPLESDIR%\ugrep\readme.txt
if ERRORLEVEL 1 goto :SampleError



rem All done
goto :QuitWithNoError

:SampleError
 echo.
 echo ERROR: Sample program %THT% did not exit cleanly. Stopping execution.
 echo.
 goto :QuitWithError

:QuitWithNoError
 echo.
 popd
 exit /b 0

:QuitWithError
 echo.
 popd
 rem Exit with non-zero error code.
 exit /b 1
