@ECHO OFF

REM ~ Copyright 2002-2007 Rene Rivera.
REM ~ Distributed under the Boost Software License, Version 1.0.
REM ~ (See accompanying file LICENSE_1_0.txt or http://www.boost.org/LICENSE_1_0.txt)

setlocal
goto Start


:Set_Error
color 00
goto :eof


:Clear_Error
ver >nul
goto :eof


:Error_Print
REM Output an error message and set the errorlevel to indicate failure.
setlocal
ECHO ###
ECHO ### %1
ECHO ###
ECHO ### You can specify the toolset as the argument, i.e.:
ECHO ###     .\build.bat msvc
ECHO ###
ECHO ### Toolsets supported by this script are: borland, como, gcc,
ECHO ###     gcc-nocygwin, intel-win32, metrowerks, mingw, msvc, vc7, vc8,
ECHO ###     vc9, vc10, vc11, vc12, vc14, vc141
ECHO ###
ECHO ### If you have Visual Studio 2017 installed you will need either update
ECHO ### the Visual Studio 2017 installer or run from VS 2017 Command Prompt
ECHO ### as we where unable to detect your toolset installation.
ECHO ###
call :Set_Error
endlocal
goto :eof


:Test_Option
REM Tests whether the given string is in the form of an option: "--*"
call :Clear_Error
setlocal
set test=%1
if not defined test (
    call :Set_Error
    goto Test_Option_End
)
set test=###%test%###
set test=%test:"###=%
set test=%test:###"=%
set test=%test:###=%
if not "-" == "%test:~1,1%" call :Set_Error
:Test_Option_End
endlocal
goto :eof


:Test_Empty
REM Tests whether the given string is not empty
call :Clear_Error
setlocal
set test=%1
if not defined test (
    call :Clear_Error
    goto Test_Empty_End
)
set test=###%test%###
set test=%test:"###=%
set test=%test:###"=%
set test=%test:###=%
if not "" == "%test%" call :Set_Error
:Test_Empty_End
endlocal
goto :eof


:Guess_Toolset
REM Try and guess the toolset to bootstrap the build with...
REM Sets BOOST_JAM_TOOLSET to the first found toolset.
REM May also set BOOST_JAM_TOOLSET_ROOT to the
REM location of the found toolset.

call :Clear_Error
call :Test_Empty "%ProgramFiles%"
if not errorlevel 1 set "ProgramFiles=C:\Program Files"

REM Visual studio is by default installed to %ProgramFiles% on 32-bit machines and
REM %ProgramFiles(x86)% on 64-bit machines. Making a common variable for both.
call :Clear_Error
call :Test_Empty "%ProgramFiles(x86)%"
if errorlevel 1 (
    set "VS_ProgramFiles=%ProgramFiles(x86)%"
) else (
    set "VS_ProgramFiles=%ProgramFiles%"
)

call guess_toolset.bat
if errorlevel 1 (
    call :Error_Print "Could not find a suitable toolset."
    goto :eof)


:Guess_Yacc
call guess_toolset.bat yacc
if errorlevel 1 (
    call :Error_Print "Could not find Yacc to build the Jam grammar.")
goto :eof


:Start
set BOOST_JAM_TOOLSET=
set BOOST_JAM_ARGS=

REM If no arguments guess the toolset;
REM or if first argument is an option guess the toolset;
REM otherwise the argument is the toolset to use.
call :Clear_Error
call :Test_Empty %1
if not errorlevel 1 (
    call :Guess_Toolset
    if not errorlevel 1 ( goto Setup_Toolset ) else ( goto Finish )
)

call :Clear_Error
call :Test_Option %1
if not errorlevel 1 (
    call :Guess_Toolset
    if not errorlevel 1 ( goto Setup_Toolset ) else ( goto Finish )
)

call :Clear_Error
set BOOST_JAM_TOOLSET=%1
shift
goto Setup_Toolset


:Setup_Toolset
REM Setup the toolset command and options. This bit of code
REM needs to be flexible enough to handle both when
REM the toolset was guessed at and found, or when the toolset
REM was indicated in the command arguments.
REM NOTE: The strange multiple "if ?? == _toolset_" tests are that way
REM because in BAT variables are subsituted only once during a single
REM command. A complete "if ... else ..."
REM is a single command, even though it's in multiple lines here.
:Setup_Args
call :Clear_Error
call :Test_Empty %1
if not errorlevel 1 goto Config_Toolset
call :Clear_Error
call :Test_Option %1
if errorlevel 1 (
    set BOOST_JAM_ARGS=%BOOST_JAM_ARGS% %1
    shift
    goto Setup_Args
)
:Config_Toolset
call config_toolset.bat 
if "_%_known_%_" == "__" (
    call :Error_Print "Unknown toolset: %BOOST_JAM_TOOLSET%"
)
if errorlevel 1 goto Finish

echo ###
echo ### Using '%BOOST_JAM_TOOLSET%' toolset.
echo ###

set YYACC_SOURCES=yyacc.c
set MKJAMBASE_SOURCES=mkjambase.c
set BJAM_SOURCES=
set BJAM_SOURCES=%BJAM_SOURCES% command.c compile.c constants.c debug.c
set BJAM_SOURCES=%BJAM_SOURCES% execcmd.c execnt.c filent.c frames.c function.c
set BJAM_SOURCES=%BJAM_SOURCES% glob.c hash.c hdrmacro.c headers.c jam.c
set BJAM_SOURCES=%BJAM_SOURCES% jambase.c jamgram.c lists.c make.c make1.c
set BJAM_SOURCES=%BJAM_SOURCES% object.c option.c output.c parse.c pathnt.c
set BJAM_SOURCES=%BJAM_SOURCES% pathsys.c regexp.c rules.c scan.c search.c
set BJAM_SOURCES=%BJAM_SOURCES% subst.c timestamp.c variable.c modules.c
set BJAM_SOURCES=%BJAM_SOURCES% strings.c filesys.c builtins.c md5.c class.c
set BJAM_SOURCES=%BJAM_SOURCES% cwd.c w32_getreg.c native.c modules/set.c
set BJAM_SOURCES=%BJAM_SOURCES% modules/path.c modules/regex.c
set BJAM_SOURCES=%BJAM_SOURCES% modules/property-set.c modules/sequence.c
set BJAM_SOURCES=%BJAM_SOURCES% modules/order.c

set BJAM_UPDATE=
:Check_Update
call :Test_Empty %1
if not errorlevel 1 goto Check_Update_End
call :Clear_Error
setlocal
set test=%1
set test=###%test%###
set test=%test:"###=%
set test=%test:###"=%
set test=%test:###=%
if "%test%" == "--update" goto Found_Update
endlocal & set BOOST_JAM_TOOLSET=%BOOST_JAM_TOOLSET%
shift
if not "_%BJAM_UPDATE%_" == "_update_" goto Check_Update
:Found_Update
endlocal & set BOOST_JAM_TOOLSET=%BOOST_JAM_TOOLSET%
set BJAM_UPDATE=update
:Check_Update_End
if "_%BJAM_UPDATE%_" == "_update_" (
    if not exist ".\bootstrap\jam0.exe" (
        set BJAM_UPDATE=
    )
)

@echo ON
@if "_%BJAM_UPDATE%_" == "_update_" goto Skip_Bootstrap
if exist bootstrap rd /S /Q bootstrap
md bootstrap
@if not exist jamgram.y goto Bootstrap_GrammarPrep
@if not exist jamgramtab.h goto Bootstrap_GrammarPrep
@goto Skip_GrammarPrep
:Bootstrap_GrammarPrep
%BOOST_JAM_CC% %BOOST_JAM_OPT_YYACC% %YYACC_SOURCES%
@if not exist ".\bootstrap\yyacc0.exe" goto Skip_GrammarPrep
.\bootstrap\yyacc0 jamgram.y jamgramtab.h jamgram.yy
:Skip_GrammarPrep
@if not exist jamgram.c goto Bootstrap_GrammarBuild
@if not exist jamgram.h goto Bootstrap_GrammarBuild
@goto Skip_GrammarBuild
:Bootstrap_GrammarBuild
@echo OFF
if "_%YACC%_" == "__" (
    call :Guess_Yacc
)
if errorlevel 1 goto Finish
@echo ON
%YACC% jamgram.y
@if errorlevel 1 goto Finish
del /f jamgram.c
rename y.tab.c jamgram.c
del /f jamgram.h
rename y.tab.h jamgram.h
:Skip_GrammarBuild
@echo ON
@if exist jambase.c goto Skip_Jambase
%BOOST_JAM_CC% %BOOST_JAM_OPT_MKJAMBASE% %MKJAMBASE_SOURCES%
@if not exist ".\bootstrap\mkjambase0.exe" goto Skip_Jambase
.\bootstrap\mkjambase0 jambase.c Jambase
:Skip_Jambase
%BOOST_JAM_CC% %BOOST_JAM_OPT_JAM% %BJAM_SOURCES%
:Skip_Bootstrap
@if not exist ".\bootstrap\jam0.exe" goto Skip_Jam
@set args=%*
@echo OFF
:Set_Args
setlocal
call :Test_Empty %args%
if not errorlevel 1 goto Set_Args_End
set test=###%args:~0,2%###
set test=%test:"###=%
set test=%test:###"=%
set test=%test:###=%
set test1=%test:~0,1%
set test2=%test:~1,1%
REM We can't just search for dash because it might be a part of
REM toolset name, like in `intel-win32'.
if "-" == "%test1%" goto Set_Args_End
if "-" == "%test2%" if not " " == "%test1" if not """" == "%test1" goto Set_Args_bite_Two
goto Set_Args_Bite_One
endlocal
:Set_Args_Bite_Two
set args=%args:~1%
:Set_Args_Bite_One
set args=%args:~1%
goto Set_Args
:Set_Args_End
@echo ON
@if "_%BJAM_UPDATE%_" == "_update_" goto Skip_Clean
.\bootstrap\jam0 -f build.jam --toolset=%BOOST_JAM_TOOLSET% "--toolset-root=%BOOST_JAM_TOOLSET_ROOT% " %args% clean
:Skip_Clean
.\bootstrap\jam0 -f build.jam --toolset=%BOOST_JAM_TOOLSET% "--toolset-root=%BOOST_JAM_TOOLSET_ROOT% " %args%
:Skip_Jam

:Finish
exit /b %ERRORLEVEL%
