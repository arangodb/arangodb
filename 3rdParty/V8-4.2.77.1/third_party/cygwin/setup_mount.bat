@echo off

REM reg.exe comes with XP and after, for Win2K can get it in the Resource Kit

set MOUNT_KEY=HKCU\Software\Cygnus Solutions\Cygwin\mounts v0

reg add "%MOUNT_KEY%" /f
reg add "%MOUNT_KEY%" /v "cygdrive flags" /t REG_DWORD /d 34 /f 
reg add "%MOUNT_KEY%" /v "cygdrive prefix" /t REG_SZ /d "/cygdrive" /f

reg add "%MOUNT_KEY%\/" /f
reg add "%MOUNT_KEY%\/" /v native /t REG_SZ /d %~dp0 /f 
reg add "%MOUNT_KEY%\/" /v "flags" /t REG_DWORD /d 2 /f 

reg add "%MOUNT_KEY%\/bin" /f
reg add "%MOUNT_KEY%\/bin" /v native /t REG_SZ /d %~dp0bin /f
reg add "%MOUNT_KEY%\/bin" /v "flags" /t REG_DWORD /d 2 /f

reg add "%MOUNT_KEY%\/usr/bin" /f
reg add "%MOUNT_KEY%\/usr/bin" /v native /t REG_SZ /d %~dp0bin /f
reg add "%MOUNT_KEY%\/usr/bin" /v "flags" /t REG_DWORD /d 2 /f

reg add "%MOUNT_KEY%\/usr/lib" /f
reg add "%MOUNT_KEY%\/usr/lib" /v native /t REG_SZ /d %~dp0lib /f
reg add "%MOUNT_KEY%\/usr/lib" /v "flags" /t REG_DWORD /d 2 /f
