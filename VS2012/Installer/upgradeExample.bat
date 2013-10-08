@echo off
:: #################################################################
:: # script to start the arango server in windows
:: #################################################################
cls
break=off
setlocal enableextensions


SET ACTION_DIR=".\js\actions"
SET STARTUP_MOD=".\js\server\modules;.\js\common\modules;.\js\node"
SET STARTUP_DIR=".\js"
SET APP_PATH=".\js\apps"
SET PACKAGE_PATH=".\js\npm"


SET DATABASE_DIR=".\data"
SET HTTP_PORT=tcp://127.0.0.1:8529
SET LOG_LEVEL=info


:: ##################################################################
:: # Since we have set the database dir to be .\data, check that it
:: # really exists, if not create it - otherwise arangodb will exit!
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
  echo         If you are certain that no other arango database server instances are active,
  echo         you may attempt to manually remove the lock .\data\lock.
  echo ACTION: Session ends. 
  echo =======================================================================================
  goto END
)
goto START_ARANGO



:START_ARANGO
:: ##################################################################
:: # set the command line parameters
:: ##################################################################

SET CMD=
SET CMD=%CMD% --server.endpoint %HTTP_PORT%
SET CMD=%CMD% --database.directory %DATABASE_DIR%
SET CMD=%CMD% --server.disable-authentication true
SET CMD=%CMD% --log.level %LOG_LEVEL%
SET CMD=%CMD% --log.severity human 
SET CMD=%CMD% --javascript.action-directory %ACTION_DIR%
SET CMD=%CMD% --javascript.modules-path %STARTUP_MOD% 
SET CMD=%CMD% --javascript.app-path %APP_PATH% 
SET CMD=%CMD% --javascript.package-path %PACKAGE_PATH% 
SET CMD=%CMD% --javascript.startup-directory %STARTUP_DIR%
SET CMD=%CMD% --server.threads 4
SET CMD=%CMD% --scheduler.threads 4 
:: the default size is 32M change this if required - especially in the 32 bit version
:: SET CMD=%CMD% --database.maximal-journal-size 1048576
SET CMD=%CMD% --console
SET CMD=%CMD% --upgrade

SET CMD=%CMD% %*

echo starting arangod.exe in server mode   
echo with %CMD%

arangod.exe %CMD%
               

:END
