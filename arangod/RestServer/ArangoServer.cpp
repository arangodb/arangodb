////////////////////////////////////////////////////////////////////////////////
/// @brief arango server
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2011-2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "ArangoServer.h"

#include <v8.h>

#include "Actions/RestActionHandler.h"
#include "Actions/actions.h"
#include "Admin/ApplicationAdminServer.h"
#include "Admin/RestHandlerCreator.h"
#include "Admin/RestShutdownHandler.h"
#include "Basics/FileUtils.h"
#include "Basics/Nonce.h"
#include "Basics/ProgramOptions.h"
#include "Basics/ProgramOptionsDescription.h"
#include "Basics/RandomGenerator.h"
#include "Basics/Utf8Helper.h"
#include "Basics/files.h"
#include "Basics/init.h"
#include "Basics/logging.h"
#include "Basics/messages.h"
#include "Basics/tri-strings.h"
#include "Cluster/HeartbeatThread.h"
#include "Dispatcher/ApplicationDispatcher.h"
#include "Dispatcher/Dispatcher.h"
#include "HttpServer/ApplicationEndpointServer.h"
#include "HttpServer/AsyncJobManager.h"
#include "Rest/InitialiseRest.h"
#include "Rest/OperationMode.h"
#include "Rest/Version.h"
#include "RestHandler/RestBatchHandler.h"
#include "RestHandler/RestDocumentHandler.h"
#include "RestHandler/RestEdgeHandler.h"
#include "RestHandler/RestImportHandler.h"
#include "RestHandler/RestPleaseUpgradeHandler.h"
#include "RestHandler/RestReplicationHandler.h"
#include "RestHandler/RestUploadHandler.h"
#include "RestServer/ConsoleThread.h"
#include "RestServer/VocbaseContext.h"
#include "Scheduler/ApplicationScheduler.h"
#include "Statistics/statistics.h"
#include "V8/V8LineEditor.h"
#include "V8/v8-conv.h"
#include "V8/v8-utils.h"
#include "V8Server/ApplicationV8.h"
#include "VocBase/auth.h"
#include "VocBase/server.h"
#include "Wal/LogfileManager.h"
#include "Cluster/ApplicationCluster.h"
#include "Cluster/RestShardHandler.h"
#include "Cluster/ClusterComm.h"
#include "Aql/RestAqlHandler.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::rest;
using namespace triagens::admin;
using namespace triagens::arango;

bool ALLOW_USE_DATABASE_IN_REST_ACTIONS;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief define "_api" and "_admin" handlers
////////////////////////////////////////////////////////////////////////////////

void ArangoServer::defineHandlers (HttpHandlerFactory* factory) {
  // First the "_api" handlers:
 
  // add "/version" handler
  _applicationAdminServer->addBasicHandlers(
      factory, "/_api",
      _applicationDispatcher->dispatcher(),
      _jobManager);

  // add a upgrade warning
  factory->addPrefixHandler("/_msg/please-upgrade",
                            RestHandlerCreator<RestPleaseUpgradeHandler>::createNoData);

  // add "/batch" handler
  factory->addPrefixHandler(RestVocbaseBaseHandler::BATCH_PATH,
                            RestHandlerCreator<RestBatchHandler>::createNoData);

  // add "/document" handler
  factory->addPrefixHandler(RestVocbaseBaseHandler::DOCUMENT_PATH,
                            RestHandlerCreator<RestDocumentHandler>::createNoData);

  // add "/edge" handler
  factory->addPrefixHandler(RestVocbaseBaseHandler::EDGE_PATH,
                            RestHandlerCreator<RestEdgeHandler>::createNoData);

  // add "/import" handler
  factory->addPrefixHandler(RestVocbaseBaseHandler::DOCUMENT_IMPORT_PATH,
                            RestHandlerCreator<RestImportHandler>::createNoData);

  // add "/replication" handler
  factory->addPrefixHandler(RestVocbaseBaseHandler::REPLICATION_PATH,
                            RestHandlerCreator<RestReplicationHandler>::createNoData);

  // add "/upload" handler
  factory->addPrefixHandler(RestVocbaseBaseHandler::UPLOAD_PATH,
                            RestHandlerCreator<RestUploadHandler>::createNoData);

  // add "/shard-comm" handler
  factory->addPrefixHandler("/_api/shard-comm",
                            RestHandlerCreator<RestShardHandler>::createData<Dispatcher*>,
                            _applicationDispatcher->dispatcher());

  // add "/aql" handler
  factory->addPrefixHandler("/_api/aql",
                            RestHandlerCreator<aql::RestAqlHandler>::createData<std::pair<ApplicationV8*, aql::QueryRegistry*>*>,
                            _pairForAql);

  // And now the "_admin" handlers

  // add "/_admin/version" handler
  _applicationAdminServer->addBasicHandlers(
      factory, "/_admin", 
      _applicationDispatcher->dispatcher(),
      _jobManager);

  // add "/_admin/shutdown" handler
  factory->addPrefixHandler("/_admin/shutdown",
                   RestHandlerCreator<RestShutdownHandler>::createData<void*>,
                   static_cast<void*>(_applicationServer));

  // add admin handlers
  _applicationAdminServer->addHandlers(factory, "/_admin");

}

////////////////////////////////////////////////////////////////////////////////
/// @brief determine the requested database from the request URL
/// when the database is present in the request and is still "alive", its
/// reference-counter is increased by one
////////////////////////////////////////////////////////////////////////////////

static TRI_vocbase_t* LookupDatabaseFromRequest (triagens::rest::HttpRequest* request,
                                                 TRI_server_t* server) {
  // get the request endpoint
  ConnectionInfo ci = request->connectionInfo();
  const string& endpoint = ci.endpoint;

  // get the databases mapped to the endpoint
  ApplicationEndpointServer* s = static_cast<ApplicationEndpointServer*>(server->_applicationEndpointServer);
  const vector<string> databases = s->getEndpointMapping(endpoint);

  // get database name from request
  string dbName = request->databaseName();

  if (databases.empty()) {
    // no databases defined. this means all databases are accessible via the endpoint

    if (dbName.empty()) {
      // if no databases was specified in the request, use system database name as a fallback
      dbName = TRI_VOC_SYSTEM_DATABASE;
      request->setDatabaseName(dbName);
    }
  }
  else {

    // only some databases are allowed for this endpoint
    if (dbName.empty()) {
      // no specific database requested, so use first mapped database
      dbName = databases.at(0);
      request->setDatabaseName(dbName);
    }
    else {
      bool found = false;

      for (size_t i = 0; i < databases.size(); ++i) {
        if (dbName == databases.at(i)) {
          found = true;
          break;
        }
      }

      // requested database not found
      if (! found) {
        return nullptr;
      }
    }
  }

  if (ServerState::instance()->isCoordinator()) {
    return TRI_UseCoordinatorDatabaseServer(server, dbName.c_str());
  }

  return TRI_UseDatabaseServer(server, dbName.c_str());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add the context to a request
////////////////////////////////////////////////////////////////////////////////

static bool SetRequestContext (triagens::rest::HttpRequest* request,
                               void* data) {

  TRI_server_t* server   = static_cast<TRI_server_t*>(data);
  TRI_vocbase_t* vocbase = LookupDatabaseFromRequest(request, server);

  // invalid database name specified, database not found etc.
  if (vocbase == nullptr) {
    return false;
  }

  // database needs upgrade
  if (vocbase->_state == (sig_atomic_t) TRI_VOCBASE_STATE_FAILED_VERSION) {
    request->setRequestPath("/_msg/please-upgrade");
    return false;
  }

  VocbaseContext* ctx = new triagens::arango::VocbaseContext(request, server, vocbase);

  if (ctx == nullptr) {
    // out of memory
    return false;
  }

  // the "true" means the request is the owner of the context
  request->setRequestContext(ctx, true);

  return true;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                class ArangoServer
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

ArangoServer::ArangoServer (int argc, char** argv)
  : _argc(argc),
    _argv(argv),
    _tempPath(),
    _applicationScheduler(nullptr),
    _applicationDispatcher(nullptr),
    _applicationEndpointServer(nullptr),
    _applicationAdminServer(nullptr),
    _applicationCluster(nullptr),
    _jobManager(nullptr),
    _applicationV8(nullptr),
    _authenticateSystemOnly(false),
    _disableAuthentication(false),
    _disableAuthenticationUnixSockets(false),
    _dispatcherThreads(8),
    _dispatcherQueueSize(8192),
    _v8Contexts(8),
    _databasePath(),
    _defaultMaximalSize(TRI_JOURNAL_DEFAULT_MAXIMAL_SIZE),
    _defaultWaitForSync(false),
    _forceSyncProperties(true),
    _disableReplicationApplier(false),
    _developmentMode(false),
    _server(nullptr),
    _queryRegistry(nullptr),
    _pairForAql(nullptr) {

  TRI_SetApplicationName("arangod");

  char* p = TRI_GetTempPath();
  // copy the string
  _tempPath = string(p);
  TRI_FreeString(TRI_CORE_MEM_ZONE, p);

  // set working directory and database directory
#ifdef _WIN32
  _workingDirectory = ".";
#else
  _workingDirectory = "/var/tmp";
#endif

  _defaultLanguage = Utf8Helper::DefaultUtf8Helper.getCollatorLanguage();


  TRI_InitServerGlobals();

  _server = TRI_CreateServer();

  if (_server == nullptr) {
    LOG_FATAL_AND_EXIT("could not create server instance");
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

ArangoServer::~ArangoServer () {
  if (_jobManager != nullptr) {
    delete _jobManager;
  }

  if (_server != nullptr) {
    TRI_FreeServer(_server);
  }

  TRI_FreeServerGlobals();

  Nonce::destroy();
}

// -----------------------------------------------------------------------------
// --SECTION--                                                 AnyServer methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void ArangoServer::buildApplicationServer () {
  map<string, ProgramOptionsDescription> additional;

  _applicationServer = new ApplicationServer("arangod", "[<options>] <database-directory>", rest::Version::getDetailed());

  if (_applicationServer == nullptr) {
    LOG_FATAL_AND_EXIT("out of memory");
  }

  char* p = TRI_BinaryName(_argv[0]);
  string conf = p;
  TRI_FreeString(TRI_CORE_MEM_ZONE, p);
  conf += ".conf";

  _applicationServer->setSystemConfigFile(conf);

  // arangod allows defining a user-specific configuration file. arangosh and the other binaries don't
  _applicationServer->setUserConfigFile(".arango" + string(1, TRI_DIR_SEPARATOR_CHAR) + string(conf));


  // initialise the server's write ahead log
  wal::LogfileManager::initialise(&_databasePath, _server);
  // and add the feature to the application server
  _applicationServer->addFeature(wal::LogfileManager::instance());

  // .............................................................................
  // dispatcher
  // .............................................................................

  _applicationDispatcher = new ApplicationDispatcher();

  if (_applicationDispatcher == nullptr) {
    LOG_FATAL_AND_EXIT("out of memory");
  }

  _applicationServer->addFeature(_applicationDispatcher);

  // .............................................................................
  // multi-threading scheduler
  // .............................................................................

  _applicationScheduler = new ApplicationScheduler(_applicationServer);

  if (_applicationScheduler == nullptr) {
    LOG_FATAL_AND_EXIT("out of memory");
  }

  _applicationScheduler->allowMultiScheduler(true);
  _applicationDispatcher->setApplicationScheduler(_applicationScheduler);

  _applicationServer->addFeature(_applicationScheduler);
  
  // ...........................................................................
  // create QueryRegistry
  // ...........................................................................

  _queryRegistry = new aql::QueryRegistry();
  _server->_queryRegistry = static_cast<void*>(_queryRegistry);

  // .............................................................................
  // V8 engine
  // .............................................................................

  _applicationV8 = new ApplicationV8(_server,
                                     _queryRegistry,
                                     _applicationScheduler,
                                     _applicationDispatcher);

  if (_applicationV8 == nullptr) {
    LOG_FATAL_AND_EXIT("out of memory");
  }

  _applicationServer->addFeature(_applicationV8);

  // .............................................................................
  // MRuby engine (this has been removed from arangod in version 2.2)
  // .............................................................................

  string ignoreOpt;

  additional[ApplicationServer::OPTIONS_HIDDEN]
    ("ruby.gc-interval", &ignoreOpt, "Ruby garbage collection interval (each x requests)")
    ("ruby.action-directory", &ignoreOpt, "path to the Ruby action directory")
    ("ruby.modules-path", &ignoreOpt, "one or more directories separated by (semi-) colons")
    ("ruby.startup-directory", &ignoreOpt, "path to the directory containing alternate Ruby startup scripts")
    ("server.disable-replication-logger", &ignoreOpt, "start with replication logger turned off")
    ("database.force-sync-shapes", &ignoreOpt, "force syncing of shape data to disk, will use waitForSync value of collection when turned off (deprecated)")
    ("database.remove-on-drop", &ignoreOpt, "wipe a collection from disk after dropping")
  ;

  // .............................................................................
  // and start a simple admin server
  // .............................................................................

  _applicationAdminServer = new ApplicationAdminServer();
  if (_applicationAdminServer == nullptr) {
    LOG_FATAL_AND_EXIT("out of memory");
  }

  _applicationServer->addFeature(_applicationAdminServer);
  _applicationAdminServer->allowLogViewer();

  // .............................................................................
  // define server options
  // .............................................................................

  // command-line only options
  additional[ApplicationServer::OPTIONS_CMDLINE]
    ("console", "do not start as server, start a JavaScript emergency console instead")
    ("upgrade", "perform a database upgrade")
    ("check-version", "checks the versions of the database and exit")
  ;

  // other options
  additional[ApplicationServer::OPTIONS_SERVER]
    ("temp-path", &_tempPath, "temporary path")
    ("default-language", &_defaultLanguage, "ISO-639 language code")
  ;

  additional[ApplicationServer::OPTIONS_HIDDEN]
    ("no-upgrade", "skip a database upgrade")
    ("start-service", "used to start as windows service")
    ("no-server", "do not start the server, if console is requested")
  ;

  // .............................................................................
  // daemon and supervisor mode
  // .............................................................................

#ifndef _WIN32

  additional[ApplicationServer::OPTIONS_CMDLINE + ":help-extended"]
    ("daemon", "run as daemon")
    ("pid-file", &_pidFile, "pid-file in daemon mode")
    ("supervisor", "starts a supervisor and runs as daemon")
    ("working-directory", &_workingDirectory, "working directory in daemon mode")
  ;

#endif

#ifdef __APPLE__
  additional[ApplicationServer::OPTIONS_CMDLINE + ":help-extended"]
    ("voice", "enable voice based welcome")
  ;
#endif

  additional[ApplicationServer::OPTIONS_HIDDEN]
    ("development-mode", "start server in development mode")
  ;

  // .............................................................................
  // javascript options
  // .............................................................................

  additional["JAVASCRIPT Options:help-admin"]
    ("javascript.script", &_scriptFile, "do not start as server, run script instead")
    ("javascript.script-parameter", &_scriptParameters, "script parameter")
  ;

  additional["JAVASCRIPT Options:help-devel"]
    ("javascript.unit-tests", &_unitTests, "do not start as server, run unit tests instead")
  ;

  // .............................................................................
  // database options
  // .............................................................................

  additional["DIRECTORY Options:help-admin"]
    ("database.directory", &_databasePath, "path to the database directory")
  ;

  additional["DATABASE Options:help-admin"]
    ("database.maximal-journal-size", &_defaultMaximalSize, "default maximal journal size, can be overwritten when creating a collection")
    ("database.wait-for-sync", &_defaultWaitForSync, "default wait-for-sync behavior, can be overwritten when creating a collection")
    ("database.force-sync-properties", &_forceSyncProperties, "force syncing of collection properties to disk, will use waitForSync value of collection when turned off")
  ;

  // .............................................................................
  // cluster options
  // .............................................................................

  _applicationCluster = new ApplicationCluster(_server, _applicationDispatcher, _applicationV8);

  if (_applicationCluster == nullptr) {
    LOG_FATAL_AND_EXIT("out of memory");
  }

  _applicationServer->addFeature(_applicationCluster);

  // .............................................................................
  // server options
  // .............................................................................

  // .............................................................................
  // for this server we display our own options such as port to use
  // .............................................................................

  additional[ApplicationServer::OPTIONS_SERVER + ":help-admin"]
    ("server.authenticate-system-only", &_authenticateSystemOnly, "use HTTP authentication only for requests to /_api and /_admin")
    ("server.disable-authentication", &_disableAuthentication, "disable authentication for ALL client requests")
#ifdef TRI_HAVE_LINUX_SOCKETS
    ("server.disable-authentication-unix-sockets", &_disableAuthenticationUnixSockets, "disable authentication for requests via UNIX domain sockets")
#endif
    ("server.disable-replication-applier", &_disableReplicationApplier, "start with replication applier turned off")
    ("server.allow-use-database", &ALLOW_USE_DATABASE_IN_REST_ACTIONS, "allow change of database in REST actions, only needed for unittests")
  ;

  bool disableStatistics = false;

#if TRI_ENABLE_FIGURES
  additional[ApplicationServer::OPTIONS_SERVER + ":help-admin"]
    ("server.disable-statistics", &disableStatistics, "turn off statistics gathering")
  ;
#endif

  additional["THREAD Options:help-admin"]
    ("server.threads", &_dispatcherThreads, "number of threads for basic operations")
    ("javascript.v8-contexts", &_v8Contexts, "number of V8 contexts that are created for executing JavaScript actions")
  ;

  additional["Server Options:help-extended"]
    ("scheduler.maximal-queue-size", &_dispatcherQueueSize, "maximum size of queue for asynchronous operations")
  ;

  // .............................................................................
  // endpoint server
  // .............................................................................

  _jobManager = new AsyncJobManager(&TRI_NewTickServer,
                                    ClusterCommRestCallback);

  _applicationEndpointServer = new ApplicationEndpointServer(_applicationServer,
                                                             _applicationScheduler,
                                                             _applicationDispatcher,
                                                             _jobManager,
                                                             "arangodb",
                                                             &SetRequestContext,
                                                             (void*) _server);
  if (_applicationEndpointServer == nullptr) {
    LOG_FATAL_AND_EXIT("out of memory");
  }

  _applicationServer->addFeature(_applicationEndpointServer);

  // .............................................................................
  // parse the command line options - exit if there is a parse error
  // .............................................................................

  if (! _applicationServer->parse(_argc, _argv, additional)) {
    LOG_FATAL_AND_EXIT("cannot parse command line arguments");
  }

  // set the temp-path
  _tempPath = StringUtils::rTrim(_tempPath, TRI_DIR_SEPARATOR_STR);

  if (_applicationServer->programOptions().has("temp-path")) {
    TRI_SetUserTempPath((char*) _tempPath.c_str());
  }

  // configure v8 w/ development-mode
  if (_applicationServer->programOptions().has("development-mode")) {
    _applicationV8->enableDevelopmentMode();
  }

  // .............................................................................
  // set language of default collator
  // .............................................................................

  string languageName;

  Utf8Helper::DefaultUtf8Helper.setCollatorLanguage(_defaultLanguage);

  if (Utf8Helper::DefaultUtf8Helper.getCollatorCountry() != "") {
    languageName = string(Utf8Helper::DefaultUtf8Helper.getCollatorLanguage() + "_" + Utf8Helper::DefaultUtf8Helper.getCollatorCountry());
  }
  else {
    languageName = Utf8Helper::DefaultUtf8Helper.getCollatorLanguage();
  }

  // .............................................................................
  // init nonces
  // .............................................................................

  uint32_t optionNonceHashSize = 0; // TODO: add a server option

  if (optionNonceHashSize > 0) {
    LOG_DEBUG("setting nonce hash size to %d", (int) optionNonceHashSize);
    Nonce::create(optionNonceHashSize);
  }

  if (disableStatistics) {
    TRI_ENABLE_STATISTICS = false;
  }

  // validate journal size
  if (_defaultMaximalSize < TRI_JOURNAL_MINIMAL_SIZE) {
    LOG_FATAL_AND_EXIT("invalid value for '--database.maximal-journal-size'. expected at least %d", (int) TRI_JOURNAL_MINIMAL_SIZE);
  }

  // validate queue size
  if (_dispatcherQueueSize <= 128) {
    LOG_FATAL_AND_EXIT("invalid value for `--server.maximal-queue-size'");
  }

  // .............................................................................
  // set directories and scripts
  // .............................................................................

  vector<string> arguments = _applicationServer->programArguments();

  if (1 < arguments.size()) {
    LOG_FATAL_AND_EXIT("expected at most one database directory, got %d", (int) arguments.size());
  }
  else if (1 == arguments.size()) {
    _databasePath = arguments[0];
  }

  if (_databasePath.empty()) {
    LOG_INFO("please use the '--database.directory' option");
    LOG_FATAL_AND_EXIT("no database path has been supplied, giving up");
  }

  // strip trailing separators
  _databasePath = StringUtils::rTrim(_databasePath, TRI_DIR_SEPARATOR_STR);

  _applicationEndpointServer->setBasePath(_databasePath);


  // disable certain options in unittest or script mode
  OperationMode::server_operation_mode_e mode = OperationMode::determineMode(_applicationServer->programOptions());

  if (mode == OperationMode::MODE_SCRIPT || mode == OperationMode::MODE_UNITTESTS) {
    // testing disables authentication
    _disableAuthentication = true;
  }

  // .............................................................................
  // now run arangod
  // .............................................................................

  // dump version details
  LOG_INFO("%s", rest::Version::getVerboseVersionString().c_str());

  LOG_INFO("using default language '%s'", languageName.c_str());

  // if we got here, then we are in server mode

  // .............................................................................
  // sanity checks
  // .............................................................................

  if (_applicationServer->programOptions().has("daemon")) {
    _daemonMode = true;
  }

  if (_applicationServer->programOptions().has("supervisor")) {
    _supervisorMode = true;
  }

  if (_daemonMode || _supervisorMode) {
    if (_pidFile.empty()) {
      LOG_INFO("please use the '--pid-file' option");
      LOG_FATAL_AND_EXIT("no pid-file defined, but daemon or supervisor mode was requested");
    }

    // make the pid filename absolute
    int err = 0;
    string currentDir = FileUtils::currentDirectory(&err);
    char* absoluteFile = TRI_GetAbsolutePath(_pidFile.c_str(), currentDir.c_str());

    if (absoluteFile != 0) {
      _pidFile = string(absoluteFile);
      TRI_Free(TRI_UNKNOWN_MEM_ZONE, absoluteFile);

      LOG_DEBUG("using absolute pid file '%s'", _pidFile.c_str());
    }
    else {
      LOG_FATAL_AND_EXIT("cannot determine current directory");
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

int ArangoServer::startupServer () {
  OperationMode::server_operation_mode_e mode = OperationMode::determineMode(_applicationServer->programOptions());
  bool startServer = true;

  if (_applicationServer->programOptions().has("no-server")) {
    startServer = false;
  }

  // check version
  bool checkVersion = false;

  if (_applicationServer->programOptions().has("check-version")) {
    checkVersion = true;
  }

  // run upgrade script
  bool performUpgrade = false;

  if (_applicationServer->programOptions().has("upgrade")) {
    performUpgrade = true;
  }

  // skip an upgrade even if VERSION is missing
  bool skipUpgrade = false;

  if (_applicationServer->programOptions().has("no-upgrade")) {
    skipUpgrade = true;
  }

  // special treatment for the write-ahead log
  // the log must exist before all other server operations can start
  LOG_TRACE("starting WAL logfile manager");

  if (! wal::LogfileManager::instance()->prepare() ||
      ! wal::LogfileManager::instance()->start()) {
    // unable to initialise & start WAL logfile manager
    LOG_FATAL_AND_EXIT("unable to start WAL logfile manager");
  }

  // .............................................................................
  // prepare the various parts of the Arango server
  // .............................................................................

  if (_dispatcherThreads < 1) {
    _dispatcherThreads = 1;
  }

  // open all databases
  bool const iterateMarkersOnOpen = ! wal::LogfileManager::instance()->hasFoundLastTick();

  openDatabases(checkVersion, performUpgrade, iterateMarkersOnOpen);

  if (! checkVersion) {
    if (! wal::LogfileManager::instance()->open()) {
      LOG_FATAL_AND_EXIT("Unable to finish WAL recovery procedure");
    }
  }

  // fetch the system database
  TRI_vocbase_t* vocbase = TRI_UseDatabaseServer(_server, TRI_VOC_SYSTEM_DATABASE);

  if (vocbase == nullptr) {
    LOG_FATAL_AND_EXIT("No _system database found in database directory. Cannot start!");
  }

  TRI_ASSERT(vocbase != nullptr);


  // initialise V8
  if (! _applicationServer->programOptions().has("javascript.v8-contexts")) {
    // the option was added recently so it's not always set
    // the behavior in older ArangoDB was to create one V8 context per dispatcher thread
    _v8Contexts = _dispatcherThreads;
  }

  if (_v8Contexts < 1) {
    _v8Contexts = 1;
  }

  if (mode == OperationMode::MODE_CONSOLE) {
    // one V8 instance is taken by the console
    if (startServer) {
      ++_v8Contexts;
    }
  }
  else if (mode == OperationMode::MODE_UNITTESTS || mode == OperationMode::MODE_SCRIPT) {
    if (_v8Contexts == 1) {
      // at least two to allow both the test-runner and the scheduler to use a V8 instance
      _v8Contexts = 2;
    }
  }

  _applicationV8->setVocbase(vocbase);
  _applicationV8->setConcurrency(_v8Contexts);
  _applicationV8->defineDouble("DISPATCHER_THREADS", _dispatcherThreads);
  _applicationV8->defineDouble("V8_CONTEXTS", _v8Contexts);

  // .............................................................................
  // prepare everything
  // .............................................................................

  if (! startServer) {
    _applicationScheduler->disable();
    _applicationDispatcher->disable();
    _applicationEndpointServer->disable();
    _applicationV8->disableActions();
  }

  // prepare scheduler and dispatcher
  _applicationServer->prepare();

  // now we can create the queues
  if (startServer) {
    _applicationDispatcher->buildStandardQueue(_dispatcherThreads, (int) _dispatcherQueueSize);
  }

  // and finish prepare
  _applicationServer->prepare2();

  // run version check (will exit!)
  if (checkVersion) {
    _applicationV8->versionCheck();
  }

  _applicationV8->upgradeDatabase(skipUpgrade, performUpgrade);

  // setup the V8 actions
  if (startServer) {
    _applicationV8->prepareServer();
  }


  _pairForAql = new std::pair<ApplicationV8*, aql::QueryRegistry*>;
  _pairForAql->first = _applicationV8;
  _pairForAql->second = _queryRegistry;

  // ...........................................................................
  // create endpoints and handlers
  // ...........................................................................

  // we pass the options by reference, so keep them until shutdown
  RestActionHandler::action_options_t httpOptions;
  httpOptions._vocbase = vocbase;
  httpOptions._queue = "STANDARD";

  if (startServer) {
    // start with enabled maintenance mode
    HttpHandlerFactory::setMaintenance(true);

    // create the server
    _applicationEndpointServer->buildServers();

    HttpHandlerFactory* handlerFactory = _applicationEndpointServer->getHandlerFactory();

    defineHandlers(handlerFactory);

    // add action handler
    handlerFactory->addPrefixHandler(
      "/",
      RestHandlerCreator<RestActionHandler>::createData<RestActionHandler::action_options_t*>,
      (void*) &httpOptions);
  }

  // .............................................................................
  // start the main event loop
  // .............................................................................

  _applicationServer->start();

  // for a cluster coordinator, the users are loaded at a later stage;
  // the kickstarter will trigger a bootstrap process
  const auto role = ServerState::instance()->getRole();

  if (role != ServerState::ROLE_COORDINATOR && role != ServerState::ROLE_PRIMARY && role != ServerState::ROLE_SECONDARY) {

    // if the authentication info could not be loaded, but authentication is turned on,
    // then we refuse to start
    if (! vocbase->_authInfoLoaded && ! _disableAuthentication) {
      LOG_FATAL_AND_EXIT("could not load required authentication information");
    }
  }

  if (_disableAuthentication) {
    LOG_INFO("Authentication is turned off");
  }

  LOG_INFO("ArangoDB (version " TRI_VERSION_FULL ") is ready for business. Have fun!");

  int res;

  if (mode == OperationMode::MODE_CONSOLE) {
    res = runConsole(vocbase);
  }
  else if (mode == OperationMode::MODE_UNITTESTS) {
    res = runUnitTests(vocbase);
  }
  else if (mode == OperationMode::MODE_SCRIPT) {
    res = runScript(vocbase);
  }
  else {
    res = runServer(vocbase);
  }

  _applicationServer->stop();

  _server->_queryRegistry = nullptr;

  delete _queryRegistry;
  _queryRegistry = nullptr;
  delete _pairForAql;
  _pairForAql = nullptr;

  closeDatabases();

  if (mode == OperationMode::MODE_CONSOLE) {
    cout << endl << TRI_BYE_MESSAGE << endl;
  }

  return res;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief wait for the heartbeat thread to run
/// before the server responds to requests, the heartbeat thread should have
/// run at least once
////////////////////////////////////////////////////////////////////////////////

void ArangoServer::waitForHeartbeat () {
  if (! ServerState::instance()->isCoordinator()) {
    // waiting for the heartbeart thread is necessary on coordinator only
    return;
  }

  while (true) {
    if (HeartbeatThread::hasRunOnce()) {
      break;
    }
    usleep(100 * 1000);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief runs the server
////////////////////////////////////////////////////////////////////////////////

int ArangoServer::runServer (TRI_vocbase_t* vocbase) {
  // disabled maintenance mode
  waitForHeartbeat();
  HttpHandlerFactory::setMaintenance(false);

  // just wait until we are signalled
  _applicationServer->wait();

  return EXIT_SUCCESS;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the JavaScript emergency console
////////////////////////////////////////////////////////////////////////////////

int ArangoServer::runConsole (TRI_vocbase_t* vocbase) {
  ConsoleThread console(_applicationServer, _applicationV8, vocbase);
  console.start();

#ifdef __APPLE__
  if (_applicationServer->programOptions().has("voice")) {
    system("say -v zarvox 'welcome to ArangoDB' &");
  }
#endif

  // disabled maintenance mode
  waitForHeartbeat();
  HttpHandlerFactory::setMaintenance(false);

  // just wait until we are signalled
  _applicationServer->wait();

#ifdef __APPLE__
  if (_applicationServer->programOptions().has("voice")) {
    system("say -v zarvox 'good-bye' &");
  }
#endif

  // .............................................................................
  // and cleanup
  // .............................................................................

  console.userAbort();
  console.stop();

  int iterations = 0;

  while (! console.done() && ++iterations < 30) {
    usleep(100000); // spin while console is still needed
  }

  return EXIT_SUCCESS;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief runs unit tests
////////////////////////////////////////////////////////////////////////////////

int ArangoServer::runUnitTests (TRI_vocbase_t* vocbase) {

  ApplicationV8::V8Context* context = _applicationV8->enterContext("STANDARD", vocbase, true, true);

  auto isolate = context->isolate;

  bool ok = false;
  {
    v8::TryCatch tryCatch;
    v8::HandleScope scope(isolate);
  
    auto localContext = v8::Local<v8::Context>::New(isolate, context->_context);
    localContext->Enter();

    v8::Context::Scope contextScope(localContext);
    // set-up unit tests array
    v8::Handle<v8::Array> sysTestFiles = v8::Array::New(isolate);

    for (size_t i = 0;  i < _unitTests.size();  ++i) {
      sysTestFiles->Set((uint32_t) i, TRI_V8_STD_STRING(_unitTests[i]));
    }

    localContext->Global()->Set(TRI_V8_ASCII_STRING("SYS_UNIT_TESTS"), sysTestFiles);
    localContext->Global()->Set(TRI_V8_ASCII_STRING("SYS_UNIT_TESTS_RESULT"), v8::True(isolate));

    v8::Local<v8::String> name(TRI_V8_ASCII_STRING("(arango)"));

    // run tests
    auto input = TRI_V8_ASCII_STRING("require(\"org/arangodb/testrunner\").runCommandLineTests();");
    TRI_ExecuteJavaScriptString(isolate, localContext, input, name, true);

    if (tryCatch.HasCaught()) {
      if (tryCatch.CanContinue()) {
        std::cerr << TRI_StringifyV8Exception(isolate, &tryCatch);
      }
      else {
        // will stop, so need for v8g->_canceled = true;
        TRI_ASSERT(! ok);
      }
    }
    else {
      ok = TRI_ObjectToBoolean(localContext->Global()->Get(TRI_V8_ASCII_STRING("SYS_UNIT_TESTS_RESULT")));
    }

    localContext->Exit();
  }

  _applicationV8->exitContext(context);

  return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief runs a script
////////////////////////////////////////////////////////////////////////////////

int ArangoServer::runScript (TRI_vocbase_t* vocbase) {
  bool ok = false;
  ApplicationV8::V8Context* context = _applicationV8->enterContext("STANDARD", vocbase, true, true);
  auto isolate = context->isolate;

  {
    v8::HandleScope globalScope(isolate);

    auto localContext = v8::Local<v8::Context>::New(isolate, context->_context);
    localContext->Enter();
    {
      v8::TryCatch tryCatch;
      v8::Context::Scope contextScope(localContext);
      for (size_t i = 0;  i < _scriptFile.size();  ++i) {
        bool r = TRI_ExecuteGlobalJavaScriptFile(isolate, _scriptFile[i].c_str());

        if (! r) {
          LOG_FATAL_AND_EXIT("cannot load script '%s', giving up", _scriptFile[i].c_str());
        }
      }

      isolate->LowMemoryNotification();
      while (! isolate->IdleNotification(1000)) {
      }

      // parameter array
      v8::Handle<v8::Array> params = v8::Array::New(isolate);

      params->Set(0, TRI_V8_STD_STRING(_scriptFile[_scriptFile.size() - 1]));

      for (size_t i = 0;  i < _scriptParameters.size();  ++i) {
        params->Set((uint32_t) (i + 1), TRI_V8_STD_STRING(_scriptParameters[i]));
      }

      // call main
      v8::Handle<v8::String> mainFuncName = TRI_V8_ASCII_STRING("main");
      v8::Handle<v8::Function> main = v8::Handle<v8::Function>::Cast(localContext->Global()->Get(mainFuncName));

      if (main.IsEmpty() || main->IsUndefined()) {
        LOG_FATAL_AND_EXIT("no main function defined, giving up");
      }
      else {
        v8::Handle<v8::Value> args[] = { params };
        v8::Handle<v8::Value> result = main->Call(main, 1, args);

        if (tryCatch.HasCaught()) {
          if (tryCatch.CanContinue()) {
            TRI_LogV8Exception(isolate, &tryCatch);
          }
          else {
            // will stop, so need for v8g->_canceled = true;
            TRI_ASSERT(! ok);
          }
        }
        else {
          ok = TRI_ObjectToDouble(result) == 0;
        }
      }
    }

    localContext->Exit();
  }

  _applicationV8->exitContext(context);
  return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief opens all databases
////////////////////////////////////////////////////////////////////////////////

void ArangoServer::openDatabases (bool checkVersion,
                                  bool performUpgrade,
                                  bool iterateMarkersOnOpen) {
  TRI_vocbase_defaults_t defaults;

  // override with command-line options
  defaults.defaultMaximalSize               = _defaultMaximalSize;
  defaults.defaultWaitForSync               = _defaultWaitForSync;
  defaults.requireAuthentication            = ! _disableAuthentication;
  defaults.requireAuthenticationUnixSockets = ! _disableAuthenticationUnixSockets;
  defaults.authenticateSystemOnly           = _authenticateSystemOnly;
  defaults.forceSyncProperties              = _forceSyncProperties;

  TRI_ASSERT(_server != nullptr);

  int res = TRI_InitServer(_server,
                           _applicationEndpointServer,
                           _databasePath.c_str(),
                           _applicationV8->appPath().c_str(),
                           _applicationV8->devAppPath().c_str(),
                           &defaults,
                           _disableReplicationApplier,
                           iterateMarkersOnOpen);

  if (res != TRI_ERROR_NO_ERROR) {
    LOG_FATAL_AND_EXIT("cannot create server instance: out of memory");
  }

  res = TRI_StartServer(_server, checkVersion, performUpgrade);

  if (res != TRI_ERROR_NO_ERROR) {
    if (checkVersion && res == TRI_ERROR_ARANGO_EMPTY_DATADIR) {
      TRI_EXIT_FUNCTION(EXIT_SUCCESS, nullptr);
    }

    LOG_FATAL_AND_EXIT("cannot start server: %s", TRI_errno_string(res));
  }

  LOG_TRACE("found system database");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief closes all databases
////////////////////////////////////////////////////////////////////////////////

void ArangoServer::closeDatabases () {
  TRI_ASSERT(_server != nullptr);

  TRI_CleanupActions();
  
  // stop the replication appliers so all replication transactions can end
  TRI_StopReplicationAppliersServer(_server);

  // enforce logfile manager shutdown so we are sure no one else will
  // write to the logs
  wal::LogfileManager::instance()->stop();

  TRI_StopServer(_server);

  LOG_INFO("ArangoDB has been shut down");
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
