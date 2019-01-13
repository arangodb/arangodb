@ECHO OFF

SETLOCAL

REM Copyright (C) 2009 Vladimir Prus
REM
REM Distributed under the Boost Software License, Version 1.0.
REM (See accompanying file LICENSE_1_0.txt or http://www.boost.org/LICENSE_1_0.txt)

ECHO Building Boost.Build engine
if exist ".\tools\build\src\engine\bin.ntx86\b2.exe" del tools\build\src\engine\bin.ntx86\b2.exe
if exist ".\tools\build\src\engine\bin.ntx86\bjam.exe" del tools\build\src\engine\bin.ntx86\bjam.exe
if exist ".\tools\build\src\engine\bin.ntx86_64\b2.exe" del tools\build\src\engine\bin.ntx86_64\b2.exe
if exist ".\tools\build\src\engine\bin.ntx86_64\bjam.exe" del tools\build\src\engine\bin.ntx86_64\bjam.exe
pushd tools\build\src\engine

call .\build.bat %* > ..\..\..\..\bootstrap.log
@ECHO OFF

popd

if exist ".\tools\build\src\engine\bin.ntx86\bjam.exe" (
   copy .\tools\build\src\engine\bin.ntx86\b2.exe . > nul
   copy .\tools\build\src\engine\bin.ntx86\bjam.exe . > nul
   goto :bjam_built)

if exist ".\tools\build\src\engine\bin.ntx86_64\bjam.exe" (
   copy .\tools\build\src\engine\bin.ntx86_64\b2.exe . > nul
   copy .\tools\build\src\engine\bin.ntx86_64\bjam.exe . > nul
   goto :bjam_built)

goto :bjam_failure

:bjam_built

REM Ideally, we should obtain the toolset that build.bat has
REM guessed. However, it uses setlocal at the start and does not
REM export BOOST_JAM_TOOLSET, and I don't know how to do that
REM properly. Default to msvc if not specified.

SET TOOLSET=msvc

IF "%1"=="gcc" SET TOOLSET=gcc

IF "%1"=="vc71" SET TOOLSET=msvc : 7.1
IF "%1"=="vc8" SET TOOLSET=msvc : 8.0
IF "%1"=="vc9" SET TOOLSET=msvc : 9.0
IF "%1"=="vc10" SET TOOLSET=msvc : 10.0
IF "%1"=="vc11" SET TOOLSET=msvc : 11.0
IF "%1"=="vc12" SET TOOLSET=msvc : 12.0
IF "%1"=="vc14" SET TOOLSET=msvc : 14.0
IF "%1"=="vc141" SET TOOLSET=msvc : 14.1

ECHO import option ; > project-config.jam
ECHO. >> project-config.jam
ECHO using %TOOLSET% ; >> project-config.jam
ECHO. >> project-config.jam
ECHO option.set keep-going : false ; >> project-config.jam
ECHO. >> project-config.jam

ECHO.
ECHO Bootstrapping is done. To build, run:
ECHO.
ECHO     .\b2
ECHO.    
ECHO To adjust configuration, edit 'project-config.jam'.
ECHO Further information:
ECHO.
ECHO     - Command line help:
ECHO     .\b2 --help
ECHO.     
ECHO     - Getting started guide: 
ECHO     http://boost.org/more/getting_started/windows.html
ECHO.     
ECHO     - Boost.Build documentation:
ECHO     http://www.boost.org/build/doc/html/index.html
ECHO.

goto :end

:bjam_failure

ECHO.
ECHO Failed to build Boost.Build engine.
ECHO Please consult bootstrap.log for further diagnostics.
ECHO.

REM Set an error code to allow `bootstrap && b2`
cmd /c exit /b 1 > nul

:end
