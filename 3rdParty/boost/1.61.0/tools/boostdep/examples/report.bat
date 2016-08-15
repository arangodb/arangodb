@REM This is an example cmd.exe batch script
@REM   that uses boostdep.exe to generate a
@REM   complete Boost dependency report.
@REM 
@REM It needs to be run from the Boost root.
@REM 
@REM Copyright 2014, 2015 Peter Dimov
@REM 
@REM Distributed under the Boost Software License, Version 1.0.
@REM See accompanying file LICENSE_1_0.txt or copy at
@REM http://www.boost.org/LICENSE_1_0.txt

SET BOOSTDEP=dist\bin\boostdep.exe

FOR /f %%i IN ('git rev-parse HEAD') DO @SET REV=%%i

FOR /f %%i IN ('git rev-parse --short HEAD') DO @SET SHREV=%%i

FOR /f %%i IN ('git rev-parse --abbrev-ref HEAD') DO @SET BRANCH=%%i

SET FOOTER=Generated on %DATE% %TIME% from revision %REV% on branch '%BRANCH%'

SET OUTDIR=..\report-%BRANCH%-%SHREV%

mkdir %OUTDIR%

%BOOSTDEP% --list-modules > %OUTDIR%\list-modules.txt

%BOOSTDEP% --footer "%FOOTER%" --html --module-overview > %OUTDIR%\module-overview.html
%BOOSTDEP% --footer "%FOOTER%" --html --module-levels > %OUTDIR%\module-levels.html
%BOOSTDEP% --footer "%FOOTER%" --html --module-weights > %OUTDIR%\module-weights.html

FOR /f %%i IN (%OUTDIR%\list-modules.txt) DO %BOOSTDEP% --title "Dependency Report for %%i" --footer "%FOOTER%" --html --primary %%i --secondary %%i --reverse %%i > %OUTDIR%\%%i.html
