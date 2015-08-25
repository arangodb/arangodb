:: ========================================================================================================
:: ==== <CONFIGURATION>
:: ========================================================================================================

:: x86_amd64 or x86
set ARCHITECTURE=%1

::x64 or ia32
set PLATFORM=%2

:: x64 or 32
set MSPLATFORM=%3

:: 64 or 32
set SUFFIX=%4

:: Set the version of Visual Studio. This will just add a suffix to the string
:: of your directories to avoid mixing them up.
SET VS_VERSION=vs%5

:: Set this to the directory that contains vcvarsall.bat file of the
:: VC Visual Studio version you want to use for building ICU.
SET VISUAL_STUDIO_VC=%6

:: Set this to the version of V8 you are building
SET V8_VERSION=%7

:: ========================================================================================================
:: ==== <DISTCLEAN>
:: ========================================================================================================

cd %V8_VERSION%

del /f /q "build\all.sln"
del /f /q "build\all.vcxproj"
del /f /q "build\all.sdf"
del /f /q "build\all.suo"

cd build

rmdir /S /Q Debug
rmdir /S /Q Debug%SUFFIX%

rmdir /S /Q Release
rmdir /S /Q Release%SUFFIX%

exit
