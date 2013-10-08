@echo off
:: #################################################################
:: # script to start the foxx manager in windows
:: #################################################################
cls
break=off
setlocal enableextensions

SET STARTUP_MOD=".\js\client\modules;.\js\common\modules;.\js\node"
SET STARTUP_DIR=".\js"
SET PACKAGE_PATH=".\js\npm"

SET HTTP_PORT=tcp://127.0.0.1:8529
SET LOG_LEVEL=info


:START_ARANGO
:: ##################################################################
:: # set the command line parameters
:: # If you wish to start the shell without a connection to an
:: # server, please change the the command
:: # SET CMD=%CMD% --server.endpoint %HTTP_PORT% 
:: # to
:: # SET CMD=%CMD% --server.endpoint none
:: ##################################################################

SET CMD=
SET CMD=%CMD% --server.endpoint %HTTP_PORT%
SET CMD=%CMD% --server.disable-authentication true
SET CMD=%CMD% --log.level %LOG_LEVEL%
SET CMD=%CMD% --javascript.startup-directory %STARTUP_DIR%
SET CMD=%CMD% --javascript.modules-path %STARTUP_MOD% 
SET CMD=%CMD% --javascript.package-path %PACKAGE_PATH% 
SET CMD=%CMD% --javascript.execute-string "require(\"org/arangodb/foxx/manager\").run(ARGUMENTS);"

SET CMD=%CMD% %*

echo starting arangosh.exe    
echo with %CMD%

arangosh.exe %CMD%
