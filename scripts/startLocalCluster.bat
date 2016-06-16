@ECHO OFF

SET NRAGENTS=%1
SET /a mod="%NRAGENTS% %% 2"
if %mod% == 0 (
  CALL ECHO Number of agents must be odd! Exiting!
  CALL EXIT /B
) else (
  CALL ECHO Number of agents: %NRAGENTS%
)
CALL ECHO Number of agents: %NRAGENTS%

SET NRDBSERVERS=%2
CALL ECHO Number of db servers: %NRDBSERVERS%

SET NRCOORDINATORS=%3
CALL ECHO Number of oordinators: %NRCOORDINATORS%

rd /s /q cluster
md cluster


SET MINP=2.0
SET MAXP=10.0
SET SFRE=2.5
SET COMP=1000
SET NATH=16
SET AID=0
SET BASE=4000
SET ENDPOINTS=
CALL ECHO Starting %NRAGENTS% db servers
for /l %%I in (1, 1, %NRAGENTS%) do (
  SET /a PORT=%%I+%BASE%
  SET /a AID=%%I-1
  CALL SET CMD=arangod -c none --agency.id %%AID%% --agency.compaction-step-size %COMP% --agency.election-timeout-min %MINP% --agency.election-timeout-max %MAXP% --agency.size %NRAGENTS% --agency.supervision true --agency.supervision-frequency %SFRE% --agency.wait-for-sync false --database.directory cluster/data%%PORT%% --javascript.app-path ./js/apps --javascript.startup-directory ./js --javascript.v8-contexts 1 --log.file cluster/%%PORT%%.log --server.authentication false --server.endpoint tcp://127.0.0.1:%%PORT%% --server.statistics false --server.threads %NATH% --log.force-direct true
  CALL SET "ENDPOINTS=%%ENDPOINTS%%--agency.endpoint tcp://127.0.0.1:%%PORT%% "
  IF %%I == %NRAGENTS% (
    CALL SET "ENDPOINTS=%%ENDPOINTS%% --agency.notify true"
    CALL SET "CMD=%%CMD%% %%ENDPOINTS%%"
  )
  START /B CMD /c %%CMD%% ^> cluster/%%PORT%%.stdout
)

timeout /T 10 /NOBREAK

SET BASE=8628
SET ROLE=PRIMARY
CALL ECHO Starting %NRDBSERVERS% db servers
for /l %%I in (1, 1, %NRDBSERVERS%) do (
  SET /a PORT=%%I+%BASE%
  START /B CMD /c arangod -c none --database.directory cluster/data%%PORT%% --cluster.agency-endpoint tcp://127.0.0.1:4001 --cluster.my-address tcp://127.0.0.1:%%PORT%% --server.endpoint tcp://127.0.0.1:%%PORT%% --cluster.my-local-info %TYPE%:127.0.0.1:%%PORT%% --cluster.my-role %ROLE% --log.file cluster/%%PORT%%.log --log.level info --server.statistics true --server.threads 5 --javascript.startup-directory ./js --server.authentication false --javascript.app-path ./js/apps --log.force-direct true ^> cluster/%%PORT%%.stdout
)

SET BASE=8529
SET ROLE=COORDINATOR
CALL ECHO Starting %NRCOORDINATORS% coordinators
for /l %%I in (1, 1, %NRCOORDINATORS%) do (
  SET /a PORT=%%I+%BASE%
  START /B CMD /c arangod -c none --database.directory cluster/data%%PORT%% --cluster.agency-endpoint tcp://127.0.0.1:4001 --cluster.my-address tcp://127.0.0.1:%%PORT%% --server.endpoint tcp://127.0.0.1:%%PORT%% --cluster.my-local-info %TYPE%:127.0.0.1:%%PORT%% --cluster.my-role %ROLE% --log.file cluster/%%PORT%%.log --log.level info --server.statistics true --server.threads 5 --javascript.startup-directory ./js --server.authentication false --javascript.app-path ./js/apps --log.force-direct true ^> cluster/%%PORT%%.stdout
)
:end
