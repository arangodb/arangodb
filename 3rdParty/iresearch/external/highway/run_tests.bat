@echo off
REM Switch directory of this batch file
cd %~dp0

if not exist build mkdir build

cd build
cmake .. -G Ninja || goto error
ninja || goto error
ctest -j || goto error

cd ..
echo Success
goto end

:error
echo Failure
exit /b 1

:end
