@ECHO OFF

REM ~ Copyright 2002-2017 Rene Rivera.
REM ~ Distributed under the Boost Software License, Version 1.0.
REM ~ (See accompanying file LICENSE_1_0.txt or http://www.boost.org/LICENSE_1_0.txt)

if "_%1_" == "_yacc_" goto Guess_Yacc
goto Guess


:Clear_Error
ver >nul
goto :eof


:Test_Path
REM Tests for the given executable file presence in the directories in the PATH
REM environment variable. Additionally sets FOUND_PATH to the path of the
REM found file.
call :Clear_Error
setlocal
set test=%~$PATH:1
endlocal
if not errorlevel 1 set FOUND_PATH=%~dp$PATH:1
goto :eof


:Guess
REM Check the variable first. This can be set manually by the user (by running the tools command prompt).
call :Clear_Error
call vswhere_usability_wrapper.cmd
if NOT "_%VS150COMNTOOLS%_" == "__" (
    set "BOOST_JAM_TOOLSET=vc141"
    set "BOOST_JAM_TOOLSET_ROOT=%VS150COMNTOOLS%..\..\VC\"
    goto :eof)

:skip_vswhere
call :Clear_Error
if EXIST "%VS_ProgramFiles%\Microsoft Visual Studio\2017\Enterprise\VC\Auxiliary\Build\vcvarsall.bat"  (
    set "BOOST_JAM_TOOLSET=vc141"
    set "BOOST_JAM_TOOLSET_ROOT=%VS_ProgramFiles%\Microsoft Visual Studio\2017\Enterprise\VC\"
    goto :eof)
if EXIST "%VS_ProgramFiles%\Microsoft Visual Studio\2017\Professional\VC\Auxiliary\Build\vcvarsall.bat"  (
    set "BOOST_JAM_TOOLSET=vc141"
    set "BOOST_JAM_TOOLSET_ROOT=%VS_ProgramFiles%\Microsoft Visual Studio\2017\Professional\VC\"
    goto :eof)
if EXIST "%VS_ProgramFiles%\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvarsall.bat"  (
    set "BOOST_JAM_TOOLSET=vc141"
    set "BOOST_JAM_TOOLSET_ROOT=%VS_ProgramFiles%\Microsoft Visual Studio\2017\Community\VC\"
    goto :eof)
if NOT "_%VS140COMNTOOLS%_" == "__" (
    set "BOOST_JAM_TOOLSET=vc14"
    set "BOOST_JAM_TOOLSET_ROOT=%VS140COMNTOOLS%..\..\VC\"
    goto :eof)
if EXIST "%VS_ProgramFiles%\Microsoft Visual Studio 14.0\VC\VCVARSALL.BAT" (
    set "BOOST_JAM_TOOLSET=vc14"
    set "BOOST_JAM_TOOLSET_ROOT=%VS_ProgramFiles%\Microsoft Visual Studio 14.0\VC\"
    goto :eof)
if NOT "_%VS120COMNTOOLS%_" == "__" (
    set "BOOST_JAM_TOOLSET=vc12"
    set "BOOST_JAM_TOOLSET_ROOT=%VS120COMNTOOLS%..\..\VC\"
    goto :eof)
if EXIST "%VS_ProgramFiles%\Microsoft Visual Studio 12.0\VC\VCVARSALL.BAT" (
    set "BOOST_JAM_TOOLSET=vc12"
    set "BOOST_JAM_TOOLSET_ROOT=%VS_ProgramFiles%\Microsoft Visual Studio 12.0\VC\"
    goto :eof)
if NOT "_%VS110COMNTOOLS%_" == "__" (
    set "BOOST_JAM_TOOLSET=vc11"
    set "BOOST_JAM_TOOLSET_ROOT=%VS110COMNTOOLS%..\..\VC\"
    goto :eof)
if EXIST "%VS_ProgramFiles%\Microsoft Visual Studio 11.0\VC\VCVARSALL.BAT" (
    set "BOOST_JAM_TOOLSET=vc11"
    set "BOOST_JAM_TOOLSET_ROOT=%VS_ProgramFiles%\Microsoft Visual Studio 11.0\VC\"
    goto :eof)
if NOT "_%VS100COMNTOOLS%_" == "__" (
    set "BOOST_JAM_TOOLSET=vc10"
    set "BOOST_JAM_TOOLSET_ROOT=%VS100COMNTOOLS%..\..\VC\"
    goto :eof)
if EXIST "%VS_ProgramFiles%\Microsoft Visual Studio 10.0\VC\VCVARSALL.BAT" (
    set "BOOST_JAM_TOOLSET=vc10"
    set "BOOST_JAM_TOOLSET_ROOT=%VS_ProgramFiles%\Microsoft Visual Studio 10.0\VC\"
    goto :eof)
if NOT "_%VS90COMNTOOLS%_" == "__" (
    set "BOOST_JAM_TOOLSET=vc9"
    set "BOOST_JAM_TOOLSET_ROOT=%VS90COMNTOOLS%..\..\VC\"
    goto :eof)
if EXIST "%VS_ProgramFiles%\Microsoft Visual Studio 9.0\VC\VCVARSALL.BAT" (
    set "BOOST_JAM_TOOLSET=vc9"
    set "BOOST_JAM_TOOLSET_ROOT=%VS_ProgramFiles%\Microsoft Visual Studio 9.0\VC\"
    goto :eof)
if NOT "_%VS80COMNTOOLS%_" == "__" (
    set "BOOST_JAM_TOOLSET=vc8"
    set "BOOST_JAM_TOOLSET_ROOT=%VS80COMNTOOLS%..\..\VC\"
    goto :eof)
if EXIST "%VS_ProgramFiles%\Microsoft Visual Studio 8\VC\VCVARSALL.BAT" (
    set "BOOST_JAM_TOOLSET=vc8"
    set "BOOST_JAM_TOOLSET_ROOT=%VS_ProgramFiles%\Microsoft Visual Studio 8\VC\"
    goto :eof)
if NOT "_%VS71COMNTOOLS%_" == "__" (
    set "BOOST_JAM_TOOLSET=vc7"
    set "BOOST_JAM_TOOLSET_ROOT=%VS71COMNTOOLS%\..\..\VC7\"
    goto :eof)
if NOT "_%VCINSTALLDIR%_" == "__" (
    REM %VCINSTALLDIR% is also set for VC9 (and probably VC8)
    set "BOOST_JAM_TOOLSET=vc7"
    set "BOOST_JAM_TOOLSET_ROOT=%VCINSTALLDIR%\VC7\"
    goto :eof)
if EXIST "%VS_ProgramFiles%\Microsoft Visual Studio .NET 2003\VC7\bin\VCVARS32.BAT" (
    set "BOOST_JAM_TOOLSET=vc7"
    set "BOOST_JAM_TOOLSET_ROOT=%VS_ProgramFiles%\Microsoft Visual Studio .NET 2003\VC7\"
    goto :eof)
if EXIST "%VS_ProgramFiles%\Microsoft Visual Studio .NET\VC7\bin\VCVARS32.BAT" (
    set "BOOST_JAM_TOOLSET=vc7"
    set "BOOST_JAM_TOOLSET_ROOT=%VS_ProgramFiles%\Microsoft Visual Studio .NET\VC7\"
    goto :eof)
if NOT "_%MSVCDir%_" == "__" (
    set "BOOST_JAM_TOOLSET=msvc"
    set "BOOST_JAM_TOOLSET_ROOT=%MSVCDir%\"
    goto :eof)
if EXIST "%VS_ProgramFiles%\Microsoft Visual Studio\VC98\bin\VCVARS32.BAT" (
    set "BOOST_JAM_TOOLSET=msvc"
    set "BOOST_JAM_TOOLSET_ROOT=%VS_ProgramFiles%\Microsoft Visual Studio\VC98\"
    goto :eof)
if EXIST "%VS_ProgramFiles%\Microsoft Visual C++\VC98\bin\VCVARS32.BAT" (
    set "BOOST_JAM_TOOLSET=msvc"
    set "BOOST_JAM_TOOLSET_ROOT=%VS_ProgramFiles%\Microsoft Visual C++\VC98\"
    goto :eof)
call :Test_Path cl.exe
if not errorlevel 1 (
    set "BOOST_JAM_TOOLSET=msvc"
    set "BOOST_JAM_TOOLSET_ROOT=%FOUND_PATH%..\"
    goto :eof)
call :Test_Path vcvars32.bat
if not errorlevel 1 (
    set "BOOST_JAM_TOOLSET=msvc"
    call "%FOUND_PATH%VCVARS32.BAT"
    set "BOOST_JAM_TOOLSET_ROOT=%MSVCDir%\"
    goto :eof)
if EXIST "C:\Borland\BCC55\Bin\bcc32.exe" (
    set "BOOST_JAM_TOOLSET=borland"
    set "BOOST_JAM_TOOLSET_ROOT=C:\Borland\BCC55\"
    goto :eof)
call :Test_Path bcc32.exe
if not errorlevel 1 (
    set "BOOST_JAM_TOOLSET=borland"
    set "BOOST_JAM_TOOLSET_ROOT=%FOUND_PATH%..\"
    goto :eof)
call :Test_Path icl.exe
if not errorlevel 1 (
    set "BOOST_JAM_TOOLSET=intel-win32"
    set "BOOST_JAM_TOOLSET_ROOT=%FOUND_PATH%..\"
    goto :eof)
if EXIST "C:\MinGW\bin\gcc.exe" (
    set "BOOST_JAM_TOOLSET=mingw"
    set "BOOST_JAM_TOOLSET_ROOT=C:\MinGW\"
    goto :eof)
if NOT "_%CWFolder%_" == "__" (
    set "BOOST_JAM_TOOLSET=metrowerks"
    set "BOOST_JAM_TOOLSET_ROOT=%CWFolder%\"
    goto :eof)
call :Test_Path mwcc.exe
if not errorlevel 1 (
    set "BOOST_JAM_TOOLSET=metrowerks"
    set "BOOST_JAM_TOOLSET_ROOT=%FOUND_PATH%..\..\"
    goto :eof)
REM Could not find a suitable toolset
exit /b 1


:Guess_Yacc
REM Tries to find bison or yacc in common places so we can build the grammar.
call :Test_Path yacc.exe
if not errorlevel 1 (
    set "YACC=yacc -d"
    goto :eof)
call :Test_Path bison.exe
if not errorlevel 1 (
    set "YACC=bison -d --yacc"
    goto :eof)
if EXIST "C:\Program Files\GnuWin32\bin\bison.exe" (
    set "YACC=C:\Program Files\GnuWin32\bin\bison.exe" -d --yacc
    goto :eof)
exit /b 1
