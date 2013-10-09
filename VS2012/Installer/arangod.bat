@echo off
:: #################################################################
:: # script to start the ArangoDB server in Windows
:: #################################################################
cls
break=off
setlocal enableextensions

:: ##################################################################
:: # Since we have set the database dir to be .\data, check that it
:: # really exists, if not create it - otherwise ArangoDB will exit!
:: ##################################################################
if EXIST ".\data" goto CHECK_LOCK
mkdir data
if NOT EXIST ".\data" (
  echo Error when attempting to create data directory ... exiting
  goto END
)  

:CHECK_LOCK
:: ##################################################################
:: # Check for the pid lock file and delete it.                    
:: # This indicates abnormal termination. 
:: ##################################################################

if EXIST ".\data\LOCK" goto DEL_LOCK

:DEL_LOCK
echo removing lock file
del /F .\data\LOCK > NUL
if EXIST ".\data\LOCK" (
  echo =======================================================================================
  echo ERROR: There appears to be a lock file which is in use. This is generally caused
  echo         by starting a second server instance before the first instance has terminated.
  echo         If you are certain that no other ArangoDB server instances are active,
  echo         you may attempt to manually remove the lock .\data\lock.
  echo ACTION: Session ends. 
  echo =======================================================================================
  goto END
)
goto START_ARANGO

:START_ARANGO
arangod.exe -c arangod.conf %*

:END
