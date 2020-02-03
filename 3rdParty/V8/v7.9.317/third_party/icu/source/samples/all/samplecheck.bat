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

echo Testing ICU samples in %ICU_ICUDIR%  arch=%ICU_ARCH% type=%ICU_DBRL%
set PATH=%ICU_BINDIR%;%PATH%
pushd %ICU_BINDIR%

set SAMPLE_BREAK=%ICU_SAMPLESDIR%\break\%ICU_ARCH%\%ICU_DBRL%\break.exe
set SAMPLE_CAL=%ICU_SAMPLESDIR%\cal\%ICU_ARCH%\%ICU_DBRL%\cal.exe
set SAMPLE_CASE=%ICU_SAMPLESDIR%\case\%ICU_ARCH%\%ICU_DBRL%\case.exe
set SAMPLE_CITER=%ICU_SAMPLESDIR%\citer\%ICU_ARCH%\%ICU_DBRL%\citer.exe
set SAMPLE_COLL=%ICU_SAMPLESDIR%\coll\%ICU_ARCH%\%ICU_DBRL%\coll.exe
set SAMPLE_CSDET=%ICU_SAMPLESDIR%\csdet\%ICU_ARCH%\%ICU_DBRL%\csdet.exe
set SAMPLE_DATE=%ICU_SAMPLESDIR%\date\%ICU_ARCH%\%ICU_DBRL%\date.exe
set SAMPLE_DATEFMT=%ICU_SAMPLESDIR%\datefmt\%ICU_ARCH%\%ICU_DBRL%\datefmt.exe
set SAMPLE_DTITVFMT=%ICU_SAMPLESDIR%\dtitvfmtsample\%ICU_ARCH%\%ICU_DBRL%\dtitvfmtsample.exe
set SAMPLE_DTPTNG=%ICU_SAMPLESDIR%\dtptngsample\%ICU_ARCH%\%ICU_DBRL%\dtptngsample.exe
set SAMPLE_MSGFMT=%ICU_SAMPLESDIR%\msgfmt\%ICU_ARCH%\%ICU_DBRL%\msgfmt.exe
set SAMPLE_NUMFMT=%ICU_SAMPLESDIR%\numfmt\%ICU_ARCH%\%ICU_DBRL%\numfmt.exe
set SAMPLE_PLURFMTSAMPLE=%ICU_SAMPLESDIR%\plurfmtsample\%ICU_ARCH%\%ICU_DBRL%\plurfmtsample.exe
set SAMPLE_PROPS=%ICU_SAMPLESDIR%\props\%ICU_ARCH%\%ICU_DBRL%\props.exe
set SAMPLE_STRSRCH=%ICU_SAMPLESDIR%\strsrch\%ICU_ARCH%\%ICU_DBRL%\strsrch.exe
set SAMPLE_TRANSLIT=%ICU_SAMPLESDIR%\translit\%ICU_ARCH%\%ICU_DBRL%\translit.exe
set SAMPLE_UCITER8=%ICU_SAMPLESDIR%\uciter8\%ICU_ARCH%\%ICU_DBRL%\uciter8.exe
set SAMPLE_UCNV=%ICU_SAMPLESDIR%\ucnv\%ICU_ARCH%\%ICU_DBRL%\ucnv.exe
REM udata needs changes to the vcxproj to change the output locations for writer/reader.
rem set SAMPLE_UDATA_WRITER=%ICU_SAMPLESDIR%\udata\%ICU_ARCH%\%ICU_DBRL%\writer.exe
rem set SAMPLE_UDATA_READER=%ICU_SAMPLESDIR%\udata\%ICU_ARCH%\%ICU_DBRL%\reader.exe
set SAMPLE_UFORTUNE=%ICU_SAMPLESDIR%\ufortune\%ICU_ARCH%\%ICU_DBRL%\ufortune.exe
set SAMPLE_UGREP=%ICU_SAMPLESDIR%\ugrep\%ICU_ARCH%\%ICU_DBRL%\ugrep.exe
REM There is also the 'resources' project in VS.
set SAMPLE_URESB=%ICU_SAMPLESDIR%\uresb\%ICU_ARCH%\%ICU_DBRL%\uresb.exe
set SAMPLE_USTRING=%ICU_SAMPLESDIR%\ustring\%ICU_ARCH%\%ICU_DBRL%\ustring.exe


@set THT=break
@echo.
@echo ==== %THT% =========================================================================
%SAMPLE_BREAK%
if ERRORLEVEL 1 goto :SampleError

@set THT=cal
@echo.
@echo ==== %THT% =========================================================================
%SAMPLE_CAL%
if ERRORLEVEL 1 goto :SampleError

@set THT=case
@echo.
@echo ==== %THT% =========================================================================
%SAMPLE_CASE%
if ERRORLEVEL 1 goto :SampleError

@set THT=citer
@echo.
@echo ==== %THT% =========================================================================
%SAMPLE_CITER%
if ERRORLEVEL 1 goto :SampleError

@set THT=coll
@echo.
@echo ==== %THT% =========================================================================
%SAMPLE_COLL%
if ERRORLEVEL 1 goto :SampleError

@set THT=csdet
@echo.
@echo ==== %THT% =========================================================================
%SAMPLE_CSDET% %ICU_SAMPLESDIR%\csdet\readme.txt
if ERRORLEVEL 1 goto :SampleError

@set THT=date
@echo.
@echo ==== %THT% =========================================================================
%SAMPLE_DATE%
if ERRORLEVEL 1 goto :SampleError

@set THT=datefmt
@echo.
@echo ==== %THT% =========================================================================
%SAMPLE_DATEFMT%
if ERRORLEVEL 1 goto :SampleError

@set THT=dtitvfmtsample
@echo.
@echo ==== %THT% =========================================================================
%SAMPLE_DTITVFMT%
if ERRORLEVEL 1 goto :SampleError

@set THT=dtptngsample
@echo.
@echo ==== %THT% =========================================================================
pushd %ICU_SAMPLESDIR%\dtptngsample\%ICU_ARCH%\%ICU_DBRL%
%SAMPLE_DTPTNG%
popd
if ERRORLEVEL 1 goto :SampleError

@set THT=msgfmt
@echo.
@echo ==== %THT% =========================================================================
%SAMPLE_MSGFMT% arg1 arg2
if ERRORLEVEL 1 goto :SampleError

@set THT=numfmt
@echo.
@echo ==== %THT% =========================================================================
%SAMPLE_NUMFMT%
if ERRORLEVEL 1 goto :SampleError

@set THT=plurfmtsample
@echo.
@echo ==== %THT% =========================================================================
%SAMPLE_PLURFMTSAMPLE%
if ERRORLEVEL 1 goto :SampleError

@set THT=props
@echo.
@echo ==== %THT% =========================================================================
%SAMPLE_PROPS%
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

@set THT=citer8
@echo.
@echo ==== %THT% =========================================================================
%SAMPLE_UCITER8%
if ERRORLEVEL 1 goto :SampleError

@set THT=ucnv
@echo.
@echo ==== %THT% =========================================================================
pushd %ICU_SAMPLESDIR%\ucnv
%SAMPLE_UCNV%
popd
if ERRORLEVEL 1 goto :SampleError

REM TODO: udata needs changes to the vcxproj file to fix the output locations for writer/reader.
@set THT=udata
@echo.
@echo ==== %THT% =========================================================================
pushd %ICU_SAMPLESDIR%\udata\%ICU_ARCH%\%ICU_DBRL%
@echo TODO: udata needs changes to the vcxproj file to fix the output locations for writer/reader.
@echo Skipping %THT%
rem %SAMPLE_UDATA_WRITER%
rem %SAMPLE_UDATA_READER%
popd
if ERRORLEVEL 1 goto :SampleError

@set THT=ufortune
@echo.
@echo ==== %THT% =========================================================================
if "%ICU_ARCH%" == "x64" (
    @echo The ufortune sample currently only runs on x86.
    @echo Skipping %THT%.
) else (
    %SAMPLE_UFORTUNE%
    if ERRORLEVEL 1 goto :SampleError
)

@set THT=ugrep
@echo.
@echo ==== %THT% =========================================================================
echo Looking for "ICU" in '%ICU_SAMPLESDIR%\ugrep\readme.txt' with ugrep.exe 
%SAMPLE_UGREP% ICU %ICU_SAMPLESDIR%\ugrep\readme.txt
if ERRORLEVEL 1 goto :SampleError

@set THT=uresb
@echo.
@echo ==== %THT% =========================================================================
pushd %ICU_SAMPLESDIR%\uresb
%SAMPLE_URESB% en
%SAMPLE_URESB% root
%SAMPLE_URESB% sr
popd
if ERRORLEVEL 1 goto :SampleError

@set THT=ustring
@echo.
@echo ==== %THT% =========================================================================
%SAMPLE_USTRING%
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
