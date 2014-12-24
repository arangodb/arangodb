:: ========================================================================================================
:: ==== <CONFIGURATION>
:: ========================================================================================================

:: Set the version of Visual Studio. This will just add a suffix to the string
:: of your directories to avoid mixing them up.
SET VS_VERSION=vs2013

:: Set this to the directory that contains vcvarsall.bat file of the
:: VC Visual Studio version you want to use for building ICU.
SET VISUAL_STUDIO_VC="C:\Program Files (x86)\Microsoft Visual Studio 12.0\VC"

:: Set this to the version of ICU you are building
SET V8_VERSION=3.29.59

:: x86_amd64 or x86
set ARCHITECTURE=%1

::x64 or ia32
set PLATFORM=%2

:: x64 or Win32
set MSPLATFORM=%3

:: 64 or 32
set SUFFIX=%4

:: ========================================================================================================
:: ==== <BUILD> 
:: ========================================================================================================

call %VISUAL_STUDIO_VC%\vcvarsall.bat %ARCHITECTURE%

set CMD=-G msvs_version=2013
set CMD=%CMD% -Dtarget_arch=%PLATFORM%
set CMD=%CMD% -Dcomponent=static_library
set CMD=%CMD% -Dmode=release
set CMD=%CMD% -Dlibrary=static_library
set CMD=%CMD% -Dv8_use_snapshot=false

echo %CMD%

cd V8-%V8_VERSION%

third_party\python_26\python build\gyp_v8 %CMD%

cd build

:: DEBUG

rmdir /S /Q Debug
rmdir /S /Q Debug%SUFFIX%

msbuild All.sln /t:v8 /p:Configuration=Debug /p:Platform=%MSPLATFORM%
msbuild All.sln /t:v8-libbase /p:Configuration=Debug /p:Platform=%MSPLATFORM%
msbuild All.sln /t:v8-libplatform /p:Configuration=Debug /p:Platform=%MSPLATFORM%

cd ..\third_party\icu
msbuild icu.sln /t:icudata /p:Configuration=Debug /p:Platform=%MSPLATFORM%
msbuild icu.sln /t:icui18n /p:Configuration=Debug /p:Platform=%MSPLATFORM%
msbuild icu.sln /t:icuuc /p:Configuration=Debug /p:Platform=%MSPLATFORM%
cd ..\..\build

ren Debug Debug%SUFFIX%

:: RELEASE

rmdir /S /Q Release
rmdir /S /Q Release%SUFFIX%

msbuild All.sln /t:v8 /p:Configuration=Release /p:Platform=%MSPLATFORM%
msbuild All.sln /t:v8-libbase /p:Configuration=Release /p:Platform=%MSPLATFORM%
msbuild All.sln /t:v8-libplatform /p:Configuration=Release /p:Platform=%MSPLATFORM%

cd ..\third_party\icu
msbuild icu.sln /t:icudata /p:Configuration=Release /p:Platform=%MSPLATFORM%
msbuild icu.sln /t:icui18n /p:Configuration=Release /p:Platform=%MSPLATFORM%
msbuild icu.sln /t:icuuc /p:Configuration=Release /p:Platform=%MSPLATFORM%
cd ..\..\build

ren Release Release%SUFFIX%

cd ..
cd ..

exit
