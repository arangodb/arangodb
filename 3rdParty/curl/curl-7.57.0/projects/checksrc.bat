@echo off
rem ***************************************************************************
rem *                                  _   _ ____  _
rem *  Project                     ___| | | |  _ \| |
rem *                             / __| | | | |_) | |
rem *                            | (__| |_| |  _ <| |___
rem *                             \___|\___/|_| \_\_____|
rem *
rem * Copyright (C) 2014 - 2016, Steve Holme, <steve_holme@hotmail.com>.
rem *
rem * This software is licensed as described in the file COPYING, which
rem * you should have received as part of this distribution. The terms
rem * are also available at https://curl.haxx.se/docs/copyright.html.
rem *
rem * You may opt to use, copy, modify, merge, publish, distribute and/or sell
rem * copies of the Software, and permit persons to whom the Software is
rem * furnished to do so, under the terms of the COPYING file.
rem *
rem * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
rem * KIND, either express or implied.
rem *
rem ***************************************************************************

:begin
  rem Check we are running on a Windows NT derived OS
  if not "%OS%" == "Windows_NT" goto nodos

  rem Set our variables
  setlocal
  set CHECK_LIB=TRUE
  set CHECK_SRC=TRUE
  set CHECK_TESTS=TRUE
  set CHECK_EXAMPLES=TRUE

:parseArgs
  if "%~1" == "" goto prerequisites

  if /i "%~1" == "-?" (
    goto syntax
  ) else if /i "%~1" == "-h" (
    goto syntax
  ) else if /i "%~1" == "-help" (
    goto syntax
  ) else if /i "%~1" == "lib" (
    set CHECK_LIB=TRUE
    set CHECK_SRC=FALSE
    set CHECK_TESTS=FALSE
    set CHECK_EXAMPLES=FALSE
  ) else if /i "%~1" == "src" (
    set CHECK_LIB=FALSE
    set CHECK_SRC=TRUE
    set CHECK_TESTS=FALSE
    set CHECK_EXAMPLES=FALSE
  ) else if /i "%~1" == "tests" (
    set CHECK_LIB=FALSE
    set CHECK_SRC=FALSE
    set CHECK_TESTS=TRUE
    set CHECK_EXAMPLES=FALSE
  ) else if /i "%~1" == "examples" (
    set CHECK_LIB=FALSE
    set CHECK_SRC=FALSE
    set CHECK_TESTS=FALSE
    set CHECK_EXAMPLES=TRUE
  ) else (
    if not defined SRC_DIR (
      set SRC_DIR=%~1%
    ) else (
      goto unknown
    )
  )

  shift & goto parseArgs

:prerequisites
  rem Check we have Perl in our path
  echo %PATH% | findstr /I /C:"\Perl" 1>nul
  if errorlevel 1 (
    rem It isn't so check we have it installed and set the path if it is
    if exist "%SystemDrive%\Perl" (
      set "PATH=%SystemDrive%\Perl\bin;%PATH%"
    ) else (
      if exist "%SystemDrive%\Perl64" (
        set "PATH=%SystemDrive%\Perl64\bin;%PATH%"
      ) else (
        goto noperl
      )
    )
  )

:configure
  if "%SRC_DIR%" == "" set SRC_DIR=..
  if not exist "%SRC_DIR%" goto nosrc

:start
  if "%CHECK_SRC%" == "TRUE" (
    rem Check the src directory
    if exist %SRC_DIR%\src (
      for /f "delims=" %%i in ('dir "%SRC_DIR%\src\*.c.*" /b 2^>NUL') do @perl "%SRC_DIR%\lib\checksrc.pl" "-D%SRC_DIR%\src" -Wtool_hugehelp.c "%%i"
      for /f "delims=" %%i in ('dir "%SRC_DIR%\src\*.h.*" /b 2^>NUL') do @perl "%SRC_DIR%\lib\checksrc.pl" "-D%SRC_DIR%\src" "%%i"
    )
  )

  if "%CHECK_LIB%" == "TRUE" (
    rem Check the lib directory
    if exist %SRC_DIR%\lib (
      for /f "delims=" %%i in ('dir "%SRC_DIR%\lib\*.c.*" /b 2^>NUL') do @perl "%SRC_DIR%\lib\checksrc.pl" "-D%SRC_DIR%\lib" "%%i"
      for /f "delims=" %%i in ('dir "%SRC_DIR%\lib\*.h.*" /b 2^>NUL') do @perl "%SRC_DIR%\lib\checksrc.pl" "-D%SRC_DIR%\lib" -Wcurl_config.h.cmake -Wcurl_config.h.in -Wcurl_config.h "%%i"
    )

    rem Check the lib\vauth directory
    if exist %SRC_DIR%\lib\vauth (
      for /f "delims=" %%i in ('dir "%SRC_DIR%\lib\vauth\*.c.*" /b 2^>NUL') do @perl "%SRC_DIR%\lib\checksrc.pl" "-D%SRC_DIR%\lib\vauth" "%%i"
      for /f "delims=" %%i in ('dir "%SRC_DIR%\lib\vauth\*.h.*" /b 2^>NUL') do @perl "%SRC_DIR%\lib\checksrc.pl" "-D%SRC_DIR%\lib\vauth" "%%i"
    )

    rem Check the lib\vtls directory
    if exist %SRC_DIR%\lib\vtls (
      for /f "delims=" %%i in ('dir "%SRC_DIR%\lib\vtls\*.c.*" /b 2^>NUL') do @perl "%SRC_DIR%\lib\checksrc.pl" "-D%SRC_DIR%\lib\vtls" "%%i"
      for /f "delims=" %%i in ('dir "%SRC_DIR%\lib\vtls\*.h.*" /b 2^>NUL') do @perl "%SRC_DIR%\lib\checksrc.pl" "-D%SRC_DIR%\lib\vtls" "%%i"
    )
  )

  if "%CHECK_TESTS%" == "TRUE" (
    rem Check the tests\libtest directory
    if exist %SRC_DIR%\tests\libtest (
      for /f "delims=" %%i in ('dir "%SRC_DIR%\tests\libtest\*.c.*" /b 2^>NUL') do @perl "%SRC_DIR%\lib\checksrc.pl" "-D%SRC_DIR%\tests\libtest" "%%i"
      for /f "delims=" %%i in ('dir "%SRC_DIR%\tests\libtest\*.h.*" /b 2^>NUL') do @perl "%SRC_DIR%\lib\checksrc.pl" "-D%SRC_DIR%\tests\libtest" "%%i"
    )

    rem Check the tests\unit directory
    if exist %SRC_DIR%\tests\unit (
      for /f "delims=" %%i in ('dir "%SRC_DIR%\tests\unit\*.c.*" /b 2^>NUL') do @perl "%SRC_DIR%\lib\checksrc.pl" "-D%SRC_DIR%\tests\unit" "%%i"
      for /f "delims=" %%i in ('dir "%SRC_DIR%\tests\unit\*.h.*" /b 2^>NUL') do @perl "%SRC_DIR%\lib\checksrc.pl" "-D%SRC_DIR%\tests\unit" "%%i"
    )

    rem Check the tests\server directory
    if exist %SRC_DIR%\tests\server (
      for /f "delims=" %%i in ('dir "%SRC_DIR%\tests\server\*.c.*" /b 2^>NUL') do @perl "%SRC_DIR%\lib\checksrc.pl" "-D%SRC_DIR%\tests\server" "%%i"
      for /f "delims=" %%i in ('dir "%SRC_DIR%\tests\server\*.h.*" /b 2^>NUL') do @perl "%SRC_DIR%\lib\checksrc.pl" "-D%SRC_DIR%\tests\server" "%%i"
    )
  )

  if "%CHECK_EXAMPLES%" == "TRUE" (
    rem Check the docs\examples directory
    if exist %SRC_DIR%\docs\examples (
      for /f "delims=" %%i in ('dir "%SRC_DIR%\docs\examples\*.c.*" /b 2^>NUL') do @perl "%SRC_DIR%\lib\checksrc.pl" "-D%SRC_DIR%\docs\examples" "%%i"
    )
  )

  goto success

:syntax
  rem Display the help
  echo.
  echo Usage: checksrc [what] [directory]
  echo.
  echo What to scan:
  echo.
  echo lib       - Scan the libcurl source
  echo src       - Scan the command-line tool source
  echo tests     - Scan the library tests and unit tests
  echo examples  - Scan the examples
  echo.
  echo directory - Specifies the curl source directory
  goto success

:unknown
  echo.
  echo Error: Unknown argument '%1'
  goto error

:nodos
  echo.
  echo Error: Only a Windows NT based Operating System is supported
  goto error

:noperl
  echo.
  echo Error: Perl is not installed
  goto error

:nosrc
  echo.
  echo Error: "%SRC_DIR%" does not exist
  goto error

:error
  if "%OS%" == "Windows_NT" endlocal
  exit /B 1

:success
  endlocal
  exit /B 0
