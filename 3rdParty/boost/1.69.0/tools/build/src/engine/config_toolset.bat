@ECHO OFF

REM ~ Copyright 2002-2017 Rene Rivera.
REM ~ Distributed under the Boost Software License, Version 1.0.
REM ~ (See accompanying file LICENSE_1_0.txt or http://www.boost.org/LICENSE_1_0.txt)

goto Start


:Call_If_Exists
if EXIST %1 call %*
goto :eof


:Start
REM Setup the toolset command and options. This bit of code
REM needs to be flexible enough to handle both when
REM the toolset was guessed at and found, or when the toolset
REM was indicated in the command arguments.
REM NOTE: The strange multiple "if ?? == _toolset_" tests are that way
REM because in BAT variables are subsituted only once during a single
REM command. A complete "if ... else ..."
REM is a single command, even though it's in multiple lines here.
if NOT "_%BOOST_JAM_TOOLSET%_" == "_metrowerks_" goto Skip_METROWERKS
if NOT "_%CWFolder%_" == "__" (
    set "BOOST_JAM_TOOLSET_ROOT=%CWFolder%\"
    )
set "PATH=%BOOST_JAM_TOOLSET_ROOT%Other Metrowerks Tools\Command Line Tools;%PATH%"
set "BOOST_JAM_CC=mwcc -runtime ss -cwd include -DNT -lkernel32.lib -ladvapi32.lib -luser32.lib"
set "BOOST_JAM_OPT_JAM=-o bootstrap\jam0.exe"
set "BOOST_JAM_OPT_MKJAMBASE=-o bootstrap\mkjambase0.exe"
set "BOOST_JAM_OPT_YYACC=-o bootstrap\yyacc0.exe"
set "_known_=1"
:Skip_METROWERKS
if NOT "_%BOOST_JAM_TOOLSET%_" == "_msvc_" goto Skip_MSVC
if NOT "_%MSVCDir%_" == "__" (
    set "BOOST_JAM_TOOLSET_ROOT=%MSVCDir%\"
    )
call :Call_If_Exists "%BOOST_JAM_TOOLSET_ROOT%bin\VCVARS32.BAT"
if not "_%BOOST_JAM_TOOLSET_ROOT%_" == "__" (
    set "PATH=%BOOST_JAM_TOOLSET_ROOT%bin;%PATH%"
    )
set "BOOST_JAM_CC=cl /nologo /GZ /Zi /MLd /Fobootstrap/ /Fdbootstrap/ -DNT -DYYDEBUG kernel32.lib advapi32.lib user32.lib"
set "BOOST_JAM_OPT_JAM=/Febootstrap\jam0"
set "BOOST_JAM_OPT_MKJAMBASE=/Febootstrap\mkjambase0"
set "BOOST_JAM_OPT_YYACC=/Febootstrap\yyacc0"
set "_known_=1"
:Skip_MSVC
if NOT "_%BOOST_JAM_TOOLSET%_" == "_vc7_" goto Skip_VC7
if NOT "_%VS71COMNTOOLS%_" == "__" (
    set "BOOST_JAM_TOOLSET_ROOT=%VS71COMNTOOLS%..\..\VC7\"
    )
if "_%VCINSTALLDIR%_" == "__" call :Call_If_Exists "%BOOST_JAM_TOOLSET_ROOT%bin\VCVARS32.BAT"
if NOT "_%BOOST_JAM_TOOLSET_ROOT%_" == "__" (
    if "_%VCINSTALLDIR%_" == "__" (
        set "PATH=%BOOST_JAM_TOOLSET_ROOT%bin;%PATH%"
        ) )
set "BOOST_JAM_CC=cl /nologo /GZ /Zi /MLd /Fobootstrap/ /Fdbootstrap/ -DWINVER=0x0501 -D_WIN32_WINNT=0x0501 -DNT -DYYDEBUG kernel32.lib advapi32.lib user32.lib "
set "BOOST_JAM_OPT_JAM=/Febootstrap\jam0"
set "BOOST_JAM_OPT_MKJAMBASE=/Febootstrap\mkjambase0"
set "BOOST_JAM_OPT_YYACC=/Febootstrap\yyacc0"
set "_known_=1"
:Skip_VC7
if NOT "_%BOOST_JAM_TOOLSET%_" == "_vc8_" goto Skip_VC8
if NOT "_%VS80COMNTOOLS%_" == "__" (
    set "BOOST_JAM_TOOLSET_ROOT=%VS80COMNTOOLS%..\..\VC\"
    )
if "_%VCINSTALLDIR%_" == "__" call :Call_If_Exists "%BOOST_JAM_TOOLSET_ROOT%VCVARSALL.BAT" %BOOST_JAM_ARGS%
if NOT "_%BOOST_JAM_TOOLSET_ROOT%_" == "__" (
    if "_%VCINSTALLDIR%_" == "__" (
        set "PATH=%BOOST_JAM_TOOLSET_ROOT%bin;%PATH%"
        ) )
set "BOOST_JAM_CC=cl /nologo /RTC1 /Zi /MTd /Fobootstrap/ /Fdbootstrap/ -DWINVER=0x0501 -D_WIN32_WINNT=0x0501 -DNT -DYYDEBUG -wd4996 kernel32.lib advapi32.lib user32.lib"
set "BOOST_JAM_OPT_JAM=/Febootstrap\jam0"
set "BOOST_JAM_OPT_MKJAMBASE=/Febootstrap\mkjambase0"
set "BOOST_JAM_OPT_YYACC=/Febootstrap\yyacc0"
set "_known_=1"
:Skip_VC8
if NOT "_%BOOST_JAM_TOOLSET%_" == "_vc9_" goto Skip_VC9
if NOT "_%VS90COMNTOOLS%_" == "__" (
    set "BOOST_JAM_TOOLSET_ROOT=%VS90COMNTOOLS%..\..\VC\"
    )
if "_%VCINSTALLDIR%_" == "__" call :Call_If_Exists "%BOOST_JAM_TOOLSET_ROOT%VCVARSALL.BAT" %BOOST_JAM_ARGS%
if NOT "_%BOOST_JAM_TOOLSET_ROOT%_" == "__" (
    if "_%VCINSTALLDIR%_" == "__" (
        set "PATH=%BOOST_JAM_TOOLSET_ROOT%bin;%PATH%"
        ) )
set "BOOST_JAM_CC=cl /nologo /RTC1 /Zi /MTd /Fobootstrap/ /Fdbootstrap/ -DNT -DYYDEBUG -wd4996 kernel32.lib advapi32.lib user32.lib"
set "BOOST_JAM_OPT_JAM=/Febootstrap\jam0"
set "BOOST_JAM_OPT_MKJAMBASE=/Febootstrap\mkjambase0"
set "BOOST_JAM_OPT_YYACC=/Febootstrap\yyacc0"
set "_known_=1"
:Skip_VC9
if NOT "_%BOOST_JAM_TOOLSET%_" == "_vc10_" goto Skip_VC10
if NOT "_%VS100COMNTOOLS%_" == "__" (
    set "BOOST_JAM_TOOLSET_ROOT=%VS100COMNTOOLS%..\..\VC\"
    )
if "_%VCINSTALLDIR%_" == "__" call :Call_If_Exists "%BOOST_JAM_TOOLSET_ROOT%VCVARSALL.BAT" %BOOST_JAM_ARGS%
if NOT "_%BOOST_JAM_TOOLSET_ROOT%_" == "__" (
    if "_%VCINSTALLDIR%_" == "__" (
        set "PATH=%BOOST_JAM_TOOLSET_ROOT%bin;%PATH%"
        ) )
set "BOOST_JAM_CC=cl /nologo /RTC1 /Zi /MTd /Fobootstrap/ /Fdbootstrap/ -DNT -DYYDEBUG -wd4996 kernel32.lib advapi32.lib user32.lib"
set "BOOST_JAM_OPT_JAM=/Febootstrap\jam0"
set "BOOST_JAM_OPT_MKJAMBASE=/Febootstrap\mkjambase0"
set "BOOST_JAM_OPT_YYACC=/Febootstrap\yyacc0"
set "_known_=1"
:Skip_VC10
if NOT "_%BOOST_JAM_TOOLSET%_" == "_vc11_" goto Skip_VC11
if NOT "_%VS110COMNTOOLS%_" == "__" (
    set "BOOST_JAM_TOOLSET_ROOT=%VS110COMNTOOLS%..\..\VC\"
    )
if "_%VCINSTALLDIR%_" == "__" call :Call_If_Exists "%BOOST_JAM_TOOLSET_ROOT%VCVARSALL.BAT" %BOOST_JAM_ARGS%
if NOT "_%BOOST_JAM_TOOLSET_ROOT%_" == "__" (
    if "_%VCINSTALLDIR%_" == "__" (
        set "PATH=%BOOST_JAM_TOOLSET_ROOT%bin;%PATH%"
        ) )
set "BOOST_JAM_CC=cl /nologo /RTC1 /Zi /MTd /Fobootstrap/ /Fdbootstrap/ -DNT -DYYDEBUG -wd4996 kernel32.lib advapi32.lib user32.lib"
set "BOOST_JAM_OPT_JAM=/Febootstrap\jam0"
set "BOOST_JAM_OPT_MKJAMBASE=/Febootstrap\mkjambase0"
set "BOOST_JAM_OPT_YYACC=/Febootstrap\yyacc0"
set "_known_=1"
:Skip_VC11
if NOT "_%BOOST_JAM_TOOLSET%_" == "_vc12_" goto Skip_VC12
if NOT "_%VS120COMNTOOLS%_" == "__" (
    set "BOOST_JAM_TOOLSET_ROOT=%VS120COMNTOOLS%..\..\VC\"
    )

if "_%BOOST_JAM_ARCH%_" == "__" set BOOST_JAM_ARCH=x86
set BOOST_JAM_ARGS=%BOOST_JAM_ARGS% %BOOST_JAM_ARCH%

if "_%VCINSTALLDIR%_" == "__" call :Call_If_Exists "%BOOST_JAM_TOOLSET_ROOT%VCVARSALL.BAT" %BOOST_JAM_ARGS%
if NOT "_%BOOST_JAM_TOOLSET_ROOT%_" == "__" (
    if "_%VCINSTALLDIR%_" == "__" (
        set "PATH=%BOOST_JAM_TOOLSET_ROOT%bin;%PATH%"
        ) )
set "BOOST_JAM_CC=cl /nologo /RTC1 /Zi /MTd /Fobootstrap/ /Fdbootstrap/ -DNT -DYYDEBUG -wd4996 kernel32.lib advapi32.lib user32.lib"
set "BOOST_JAM_OPT_JAM=/Febootstrap\jam0"
set "BOOST_JAM_OPT_MKJAMBASE=/Febootstrap\mkjambase0"
set "BOOST_JAM_OPT_YYACC=/Febootstrap\yyacc0"
set "_known_=1"
:Skip_VC12
if NOT "_%BOOST_JAM_TOOLSET%_" == "_vc14_" goto Skip_VC14
if "_%BOOST_JAM_TOOLSET_ROOT%_" == "__" (
    if NOT "_%VS140COMNTOOLS%_" == "__" (
        set "BOOST_JAM_TOOLSET_ROOT=%VS140COMNTOOLS%..\..\VC\"
    ))

if "_%BOOST_JAM_ARCH%_" == "__" set BOOST_JAM_ARCH=x86
set BOOST_JAM_ARGS=%BOOST_JAM_ARGS% %BOOST_JAM_ARCH%

if "_%VCINSTALLDIR%_" == "__" call :Call_If_Exists "%BOOST_JAM_TOOLSET_ROOT%VCVARSALL.BAT" %BOOST_JAM_ARGS%
if NOT "_%BOOST_JAM_TOOLSET_ROOT%_" == "__" (
    if "_%VCINSTALLDIR%_" == "__" (
        set "PATH=%BOOST_JAM_TOOLSET_ROOT%bin;%PATH%"
        ) )
set "BOOST_JAM_CC=cl /nologo /RTC1 /Zi /MTd /Fobootstrap/ /Fdbootstrap/ -DNT -DYYDEBUG -wd4996 kernel32.lib advapi32.lib user32.lib"
set "BOOST_JAM_OPT_JAM=/Febootstrap\jam0"
set "BOOST_JAM_OPT_MKJAMBASE=/Febootstrap\mkjambase0"
set "BOOST_JAM_OPT_YYACC=/Febootstrap\yyacc0"
set "_known_=1"
:Skip_VC14
if NOT "_%BOOST_JAM_TOOLSET%_" == "_vc141_" goto Skip_VC141
call vswhere_usability_wrapper.cmd
REM Reset ERRORLEVEL since from now on it's all based on ENV vars
ver > nul 2> nul
if "_%BOOST_JAM_TOOLSET_ROOT%_" == "__" (
    if NOT "_%VS150COMNTOOLS%_" == "__" (
        set "BOOST_JAM_TOOLSET_ROOT=%VS150COMNTOOLS%..\..\VC\"
    ))

if "_%BOOST_JAM_ARCH%_" == "__" set BOOST_JAM_ARCH=x86
set BOOST_JAM_ARGS=%BOOST_JAM_ARGS% %BOOST_JAM_ARCH%

REM return to current directory as vsdevcmd_end.bat switches to %USERPROFILE%\Source if it exists.
pushd %CD%
if "_%VSINSTALLDIR%_" == "__" call :Call_If_Exists "%BOOST_JAM_TOOLSET_ROOT%Auxiliary\Build\vcvarsall.bat" %BOOST_JAM_ARGS%
popd
set "BOOST_JAM_CC=cl /nologo /RTC1 /Zi /MTd /Fobootstrap/ /Fdbootstrap/ -DNT -DYYDEBUG -wd4996 kernel32.lib advapi32.lib user32.lib"
set "BOOST_JAM_OPT_JAM=/Febootstrap\jam0"
set "BOOST_JAM_OPT_MKJAMBASE=/Febootstrap\mkjambase0"
set "BOOST_JAM_OPT_YYACC=/Febootstrap\yyacc0"
set "_known_=1"
:Skip_VC141
if NOT "_%BOOST_JAM_TOOLSET%_" == "_borland_" goto Skip_BORLAND
if "_%BOOST_JAM_TOOLSET_ROOT%_" == "__" (
    call :Test_Path bcc32.exe )
if "_%BOOST_JAM_TOOLSET_ROOT%_" == "__" (
    if not errorlevel 1 (
        set "BOOST_JAM_TOOLSET_ROOT=%FOUND_PATH%..\"
        ) )
if not "_%BOOST_JAM_TOOLSET_ROOT%_" == "__" (
    set "PATH=%BOOST_JAM_TOOLSET_ROOT%Bin;%PATH%"
    )
set "BOOST_JAM_CC=bcc32 -WC -w- -q -I%BOOST_JAM_TOOLSET_ROOT%Include -L%BOOST_JAM_TOOLSET_ROOT%Lib /DNT -nbootstrap"
set "BOOST_JAM_OPT_JAM=-ejam0"
set "BOOST_JAM_OPT_MKJAMBASE=-emkjambasejam0"
set "BOOST_JAM_OPT_YYACC=-eyyacc0"
set "_known_=1"
:Skip_BORLAND
if NOT "_%BOOST_JAM_TOOLSET%_" == "_como_" goto Skip_COMO
set "BOOST_JAM_CC=como -DNT"
set "BOOST_JAM_OPT_JAM=-o bootstrap\jam0.exe"
set "BOOST_JAM_OPT_MKJAMBASE=-o bootstrap\mkjambase0.exe"
set "BOOST_JAM_OPT_YYACC=-o bootstrap\yyacc0.exe"
set "_known_=1"
:Skip_COMO
if NOT "_%BOOST_JAM_TOOLSET%_" == "_gcc_" goto Skip_GCC
set "BOOST_JAM_CC=gcc -DNT"
set "BOOST_JAM_OPT_JAM=-o bootstrap\jam0.exe"
set "BOOST_JAM_OPT_MKJAMBASE=-o bootstrap\mkjambase0.exe"
set "BOOST_JAM_OPT_YYACC=-o bootstrap\yyacc0.exe"
set "_known_=1"
:Skip_GCC
if NOT "_%BOOST_JAM_TOOLSET%_" == "_gcc-nocygwin_" goto Skip_GCC_NOCYGWIN
set "BOOST_JAM_CC=gcc -DNT -mno-cygwin"
set "BOOST_JAM_OPT_JAM=-o bootstrap\jam0.exe"
set "BOOST_JAM_OPT_MKJAMBASE=-o bootstrap\mkjambase0.exe"
set "BOOST_JAM_OPT_YYACC=-o bootstrap\yyacc0.exe"
set "_known_=1"
:Skip_GCC_NOCYGWIN
if NOT "_%BOOST_JAM_TOOLSET%_" == "_intel-win32_" goto Skip_INTEL_WIN32
set "BOOST_JAM_CC=icl -DNT /nologo kernel32.lib advapi32.lib user32.lib"
set "BOOST_JAM_OPT_JAM=/Febootstrap\jam0"
set "BOOST_JAM_OPT_MKJAMBASE=/Febootstrap\mkjambase0"
set "BOOST_JAM_OPT_YYACC=/Febootstrap\yyacc0"
set "_known_=1"
:Skip_INTEL_WIN32
if NOT "_%BOOST_JAM_TOOLSET%_" == "_mingw_" goto Skip_MINGW
if not "_%BOOST_JAM_TOOLSET_ROOT%_" == "__" (
    set "PATH=%BOOST_JAM_TOOLSET_ROOT%bin;%PATH%"
    )
set "BOOST_JAM_CC=gcc -DNT"
set "BOOST_JAM_OPT_JAM=-o bootstrap\jam0.exe"
set "BOOST_JAM_OPT_MKJAMBASE=-o bootstrap\mkjambase0.exe"
set "BOOST_JAM_OPT_YYACC=-o bootstrap\yyacc0.exe"
set "_known_=1"
:Skip_MINGW
exit /b %errorlevel%
