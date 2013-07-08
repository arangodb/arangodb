Command-Line Options {#CommandLine}
===================================

@NAVIGATE_CommandLine
@EMBEDTOC{CommandLineTOC}

General Options {#CommandLineGeneralOptions}
============================================

@anchor CommandLineHelp
@copydetails triagens::rest::ApplicationServer::_options

@CLEARPAGE
@anchor CommandLineVersion
@copydetails triagens::rest::ApplicationServer::_version

@CLEARPAGE
@anchor CommandLineUpgrade
@copydetails triagens::arango::ApplicationV8::_performUpgrade

@CLEARPAGE
@anchor CommandLineConfiguration
@copydetails triagens::rest::ApplicationServer::_configFile

@CLEARPAGE
@anchor CommandLineDaemon
@CMDOPT{\--daemon}

Runs the server as a daemon (as a background process). This parameter can only
be set if the pid (process id) file is specified. That is, unless a value to the
parameter pid-file is given, then the server will report an error and exit.

@CLEARPAGE
@anchor CommandLineDefaultLanguage
@copydetails triagens::arango::ArangoServer::_defaultLanguage

@CLEARPAGE
@anchor CommandLineSupervisor
@CMDOPT{\--supervisor}

Executes the server in supervisor mode. In the event that the server
unexpectedly terminates due to an internal error, the supervisor will
automatically restart the server. Setting this flag automatically implies that
the server will run as a daemon. Note that, as with the daemon flag, this flag
requires that the pid-file parameter will set.

    unix> ./arangod --supervisor --pid-file /var/run/arangodb.pid /tmp/vocbase/
    2012-06-27T15:58:28Z [10133] INFO starting up in supervisor mode

As can be seen (e.g. by executing the ps command), this will start a supervisor
process and the actual database process:

    unix> ps fax | grep arangod
    10137 ?        Ssl    0:00 ./arangod --supervisor --pid-file /var/run/arangodb.pid /tmp/vocbase/
    10142 ?        Sl     0:00  \_ ./arangod --supervisor --pid-file /var/run/arangodb.pid /tmp/vocbase/

When the database process terminates unexpectedly, the supervisor process will
start up a new database process:

    > kill -SIGSEGV 10142

    > ps fax | grep arangod
    10137 ?        Ssl    0:00 ./arangod --supervisor --pid-file /var/run/arangodb.pid /tmp/vocbase/
    10168 ?        Sl     0:00  \_ ./arangod --supervisor --pid-file /var/run/arangodb.pid /tmp/vocbase/

@CLEARPAGE
@anchor CommandLineUid
@copydetails triagens::rest::ApplicationServer::_uid

@CLEARPAGE
@anchor CommandLineGid
@copydetails triagens::rest::ApplicationServer::_gid

@CLEARPAGE
@anchor CommandLinePidFile
@copydetails triagens::rest::AnyServer::_pidFile

@CLEARPAGE
@anchor CommandLineConsole
@CMDOPT{\--console}

Runs the server in an exclusive emergency console mode. When 
starting the server with this option, the server is started with
an interactive JavaScript emergency console, with all networking
and HTTP interfaces of the server disabled.

No requests can be made to the server in this mode, and the only
way to work with the server in this mode is by using the emergency
console. 
Note that the server cannot be started in this mode if it is 
already running in this or another mode. 

Command-Line Options for arangod {#CommandLineArangod}
======================================================

@anchor CommandLineArangoEndpoint
@copydetails triagens::rest::ApplicationEndpointServer::_endpoints

@CLEARPAGE
@anchor CommandLineArangoDisableAuthentication
@copydetails triagens::rest::ApplicationEndpointServer::_disableAuthentication

@CLEARPAGE
@anchor CommandLineArangoAuthenticateSystemOnly
@copydetails triagens::arango::ArangoServer::_authenticateSystemOnly

@CLEARPAGE
@anchor CommandLineArangoKeepAliveTimeout
@copydetails triagens::rest::ApplicationEndpointServer::_keepAliveTimeout

@CLEARPAGE
@anchor CommandLineArangoKeyFile
@copydetails triagens::rest::ApplicationEndpointServer::_httpsKeyfile

@CLEARPAGE
@anchor CommandLineArangoCaFile
@copydetails triagens::rest::ApplicationEndpointServer::_cafile

@CLEARPAGE
@anchor CommandLineArangoSslProtocol
@copydetails triagens::rest::ApplicationEndpointServer::_sslProtocol

@CLEARPAGE
@anchor CommandLineArangoSslCacheMode
@copydetails triagens::rest::ApplicationEndpointServer::_sslCache

@CLEARPAGE
@anchor CommandLineArangoSslOptions
@copydetails triagens::rest::ApplicationEndpointServer::_sslOptions

@CLEARPAGE
@anchor CommandLineArangoSslCipherList
@copydetails triagens::rest::ApplicationEndpointServer::_sslCipherList

@CLEARPAGE
@anchor CommandLineArangoDisableAdminInterface
@CMDOPT{\--disable-admin-interface @CA{value}}

If this option is specified and @CA{value} is `true`, then the HTML
administration interface at URL `http://server:port/` will be disabled and
cannot used by any user at all.

@CLEARPAGE
@anchor CommandLineArangoDisableStatistics
@CMDOPT{\--disable-statistics @CA{value}}

If this option is @CA{value} is `true`, then ArangoDB's statistics gathering
is turned off. Statistics gathering causes constant CPU activity so using this
option to turn it off might relieve heavy-loaded instances.
Note: this option is only available when ArangoDB has not been compiled with
the option `--disable-figures`.

@CLEARPAGE
@anchor CommandLineArangoDirectory
@copydetails triagens::arango::ArangoServer::_databasePath

@CLEARPAGE
@anchor CommandLineArangoMaximalJournalSize
@copydetails triagens::arango::ArangoServer::_defaultMaximalSize

@CLEARPAGE
@anchor CommandLineArangoWaitForSync
@copydetails triagens::arango::ArangoServer::_defaultWaitForSync

@CLEARPAGE
@anchor CommandLineArangoForceSyncProperties
@copydetails triagens::arango::ArangoServer::_forceSyncProperties

@CLEARPAGE
@anchor CommandLineArangoForceSyncShapes
@copydetails triagens::arango::ArangoServer::_forceSyncShapes

@CLEARPAGE
@anchor CommandLineArangoRemoveOnDrop
@copydetails triagens::arango::ArangoServer::_removeOnDrop

@CLEARPAGE
@anchor CommandLineArangoJsGcFrequency
@copydetails triagens::arango::ApplicationV8::_gcFrequency

@CLEARPAGE
@anchor CommandLineArangoJsGcInterval
@copydetails triagens::arango::ApplicationV8::_gcInterval

@CLEARPAGE
@anchor CommandLineArangoJsV8Options
@copydetails triagens::arango::ApplicationV8::_v8Options

@CLEARPAGE
Command-Line Options for Development {#CommandLineDevelopment}
==============================================================

@anchor CommandLineArangoDevelopmentMode
@copydetails triagens::arango::ArangoServer::_developmentMode

@CLEARPAGE
Command-Line Options for Communication {#CommandLineScheduler}
==============================================================

@anchor CommandLineSchedulerThreads
@copydetails triagens::rest::ApplicationScheduler::_nrSchedulerThreads

@CLEARPAGE
@anchor CommandLineSchedulerBackend
@copydetails triagens::rest::ApplicationScheduler::_backend

@CLEARPAGE
@anchor CommandLineSchedulerShowIoBackends
@CMDOPT{\--show-io-backends}

If this option is specified, then the server will list available backends and
exit. This option is useful only when used in conjunction with the option
scheduler.backend. An integer is returned (which is platform dependent) which
indicates available backends on your platform. See libev for further details and
for the meaning of the integer returned. This describes the allowed integers for
`scheduler.backend`, see @ref CommandLineScheduler "here" for details.

@CLEARPAGE
Command-Line Options for Logging {#CommandLineLogging}
======================================================

There are two different kinds of logs. Human-readable logs and machine-readable
logs. The human-readable logs are used to provide an administration with
information about the server. The machine-readable logs are used to provide
statistics about executed requests and timings about computation steps.

General Logging Options {#CommandLineLoggingGeneral}
----------------------------------------------------

@anchor CommandLineLoggingLogFile
@copydetails triagens::rest::ApplicationServer::_logFile

@CLEARPAGE
@anchor CommandLineLoggingLogRequestsFile
@copydetails triagens::rest::ApplicationServer::_logRequestsFile

@CLEARPAGE
@anchor CommandLineLoggingLogSeverity
@copydetails triagens::rest::ApplicationServer::_logSeverity

@CLEARPAGE
@anchor CommandLineLoggingLogSyslog
@copydetails triagens::rest::ApplicationServer::_logSyslog

@CLEARPAGE
Human Readable Logging {#CommandLineLoggingHuman}
-------------------------------------------------

@anchor CommandLineLoggingLogLevel
@copydetails triagens::rest::ApplicationServer::_logLevel

@CLEARPAGE
@anchor CommandLineLoggingLogLineNumber
@copydetails triagens::rest::ApplicationServer::_logLineNumber

@CLEARPAGE
@anchor CommandLineLoggingLogPrefix
@copydetails triagens::rest::ApplicationServer::_logPrefix

@CLEARPAGE
@anchor CommandLineLoggingLogThread
@copydetails triagens::rest::ApplicationServer::_logThreadId

@CLEARPAGE
@anchor CommandLineLoggingLogSourceFilter
@copydetails triagens::rest::ApplicationServer::_logSourceFilter

@CLEARPAGE
@anchor CommandLineLoggingLogContentFilter
@copydetails triagens::rest::ApplicationServer::_logContentFilter

@CLEARPAGE
Machine Readable Logging {#CommandLineLoggingMachine}
-----------------------------------------------------

@anchor CommandLineLoggingLogApplication
@copydetails triagens::rest::ApplicationServer::_logApplicationName

@CLEARPAGE
@anchor CommandLineLoggingLogFacility
@copydetails triagens::rest::ApplicationServer::_logFacility

@CLEARPAGE
@anchor CommandLineLoggingLogFormat
@copydetails triagens::rest::ApplicationServer::_logFormat

@CLEARPAGE
@anchor CommandLineLoggingLogHostName
@copydetails triagens::rest::ApplicationServer::_logHostName

@CLEARPAGE
Command-Line Options for Random Numbers {#CommandLineRandom}
============================================================

@anchor CommandLineRandomGenerator
@copydetails triagens::rest::ApplicationServer::_randomGenerator
