:: This script must not rely on any external tools or PATH values.
@echo OFF

:: Let advanced users checkout the tools in just one P4 enlistment
if "%SETUP_ENV_PYTHON_26%"=="done" goto :EOF
set  SETUP_ENV_PYTHON_26=done

set PATH=%~dp0;%PATH%
