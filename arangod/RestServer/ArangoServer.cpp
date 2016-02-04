////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#include "ArangoServer.h"

#include <v8.h>
#include <iostream>

#include "Actions/RestActionHandler.h"
#include "Actions/actions.h"
#include "Aql/Query.h"
#include "Aql/QueryCache.h"
#include "Aql/RestAqlHandler.h"
#include "Basics/FileUtils.h"
#include "Basics/Nonce.h"
#include "Basics/ProgramOptions.h"
#include "Basics/ProgramOptionsDescription.h"
#include "Basics/RandomGenerator.h"
#include "Basics/Utf8Helper.h"
#include "Basics/files.h"
#include "Basics/init.h"
#include "Basics/Logger.h"
#include "Basics/messages.h"
#include "Basics/ThreadPool.h"
#include "Basics/tri-strings.h"
#include "Cluster/ApplicationCluster.h"
#include "Cluster/ClusterComm.h"
#include "Cluster/HeartbeatThread.h"
#include "Cluster/RestShardHandler.h"
#include "Dispatcher/ApplicationDispatcher.h"
#include "Dispatcher/Dispatcher.h"
#include "HttpServer/ApplicationEndpointServer.h"
#include "HttpServer/AsyncJobManager.h"
#include "HttpServer/HttpHandlerFactory.h"
#include "Rest/InitializeRest.h"
#include "Rest/OperationMode.h"
#include "Rest/Version.h"
#include "RestHandler/RestAdminLogHandler.h"
#include "RestHandler/RestBatchHandler.h"
#include "RestHandler/RestCursorHandler.h"
#include "RestHandler/RestDebugHandler.h"
#include "RestHandler/RestDocumentHandler.h"
#include "RestHandler/RestEdgeHandler.h"
#include "RestHandler/RestEdgesHandler.h"
#include "RestHandler/RestExportHandler.h"
#include "RestHandler/RestHandlerCreator.h"
#include "RestHandler/RestImportHandler.h"
#include "RestHandler/RestJobHandler.h"
#include "RestHandler/RestPleaseUpgradeHandler.h"
#include "RestHandler/RestQueryCacheHandler.h"
#include "RestHandler/RestQueryHandler.h"
#include "RestHandler/RestReplicationHandler.h"
#include "RestHandler/RestShutdownHandler.h"
#include "RestHandler/RestSimpleHandler.h"
#include "RestHandler/RestSimpleQueryHandler.h"
#include "RestHandler/RestUploadHandler.h"
#include "RestHandler/RestVersionHandler.h"
#include "RestHandler/WorkMonitorHandler.h"
#include "RestServer/ConsoleThread.h"
#include "RestServer/VocbaseContext.h"
#include "Scheduler/ApplicationScheduler.h"
#include "Statistics/statistics.h"
#include "V8/V8LineEditor.h"
#include "V8/v8-conv.h"
#include "V8/v8-utils.h"
#include "V8Server/ApplicationV8.h"
#include "VocBase/auth.h"
#include "VocBase/KeyGenerator.h"
#include "VocBase/server.h"
#include "Wal/LogfileManager.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

bool ALLOW_USE_DATABASE_IN_REST_ACTIONS;

bool IGNORE_DATAFILE_ERRORS;

////////////////////////////////////////////////////////////////////////////////
/// @brief converts list of size_t to string
////////////////////////////////////////////////////////////////////////////////

template <typename T>
static std::string ToString(std::vector<T> const& v) {
  std::string result = "";
  std::string sep = "[";

  for (auto const& e : v) {
    result += sep + std::to_string(e);
    sep = ",";
  }

  return result + "]";
}

////////////////////////////////////////////////////////////////////////////////
/// @brief define "_api" and "_admin" handlers
////////////////////////////////////////////////////////////////////////////////

void ArangoServer::defineHandlers(HttpHandlerFactory* factory) {
  // First the "_api" handlers:

  // add an upgrade warning
  factory->addPrefixHandler(
      "/_msg/please-upgrade",
      RestHandlerCreator<RestPleaseUpgradeHandler>::createNoData);

  // add "/batch" handler
  factory->addPrefixHandler(RestVocbaseBaseHandler::BATCH_PATH,
                            RestHandlerCreator<RestBatchHandler>::createNoData);

  // add "/cursor" handler
  factory->addPrefixHandler(
      RestVocbaseBaseHandler::CURSOR_PATH,
      RestHandlerCreator<RestCursorHandler>::createData<
          std::pair<ApplicationV8*, aql::QueryRegistry*>*>,
      _pairForAqlHandler);

  // add "/document" handler
  factory->addPrefixHandler(
      RestVocbaseBaseHandler::DOCUMENT_PATH,
      RestHandlerCreator<RestDocumentHandler>::createNoData);

  // add "/edge" handler
  factory->addPrefixHandler(RestVocbaseBaseHandler::EDGE_PATH,
                            RestHandlerCreator<RestEdgeHandler>::createNoData);

  // add "/edges" handler
  factory->addPrefixHandler(RestVocbaseBaseHandler::EDGES_PATH,
                            RestHandlerCreator<RestEdgesHandler>::createNoData);

  // add "/export" handler
  factory->addPrefixHandler(
      RestVocbaseBaseHandler::EXPORT_PATH,
      RestHandlerCreator<RestExportHandler>::createNoData);

  // add "/import" handler
  factory->addPrefixHandler(
      RestVocbaseBaseHandler::IMPORT_PATH,
      RestHandlerCreator<RestImportHandler>::createNoData);

  // add "/replication" handler
  factory->addPrefixHandler(
      RestVocbaseBaseHandler::REPLICATION_PATH,
      RestHandlerCreator<RestReplicationHandler>::createNoData);

  // add "/simple/all" handler
  factory->addPrefixHandler(
      RestVocbaseBaseHandler::SIMPLE_QUERY_ALL_PATH,
      RestHandlerCreator<RestSimpleQueryHandler>::createData<
          std::pair<ApplicationV8*, aql::QueryRegistry*>*>,
      _pairForAqlHandler);

  // add "/simple/lookup-by-key" handler
  factory->addPrefixHandler(
      RestVocbaseBaseHandler::SIMPLE_LOOKUP_PATH,
      RestHandlerCreator<RestSimpleHandler>::createData<
          std::pair<ApplicationV8*, aql::QueryRegistry*>*>,
      _pairForAqlHandler);

  // add "/simple/remove-by-key" handler
  factory->addPrefixHandler(
      RestVocbaseBaseHandler::SIMPLE_REMOVE_PATH,
      RestHandlerCreator<RestSimpleHandler>::createData<
          std::pair<ApplicationV8*, aql::QueryRegistry*>*>,
      _pairForAqlHandler);

  // add "/upload" handler
  factory->addPrefixHandler(
      RestVocbaseBaseHandler::UPLOAD_PATH,
      RestHandlerCreator<RestUploadHandler>::createNoData);

  // add "/shard-comm" handler
  factory->addPrefixHandler(
      "/_api/shard-comm",
      RestHandlerCreator<RestShardHandler>::createData<Dispatcher*>,
      _applicationDispatcher->dispatcher());

  // add "/aql" handler
  factory->addPrefixHandler(
      "/_api/aql", RestHandlerCreator<aql::RestAqlHandler>::createData<
                       std::pair<ApplicationV8*, aql::QueryRegistry*>*>,
      _pairForAqlHandler);

  factory->addPrefixHandler(
      "/_api/query",
      RestHandlerCreator<RestQueryHandler>::createData<ApplicationV8*>,
      _applicationV8);

  factory->addPrefixHandler(
      "/_api/query-cache",
      RestHandlerCreator<RestQueryCacheHandler>::createNoData);

  // And now some handlers which are registered in both /_api and /_admin
  factory->addPrefixHandler(
      "/_api/job",
      RestHandlerCreator<arangodb::RestJobHandler>::createData<
          std::pair<Dispatcher*, AsyncJobManager*>*>,
      _pairForJobHandler);

  factory->addHandler("/_api/version",
                      RestHandlerCreator<RestVersionHandler>::createNoData,
                      nullptr);

  // And now the _admin handlers
  factory->addPrefixHandler(
      "/_admin/job",
      RestHandlerCreator<arangodb::RestJobHandler>::createData<
          std::pair<Dispatcher*, AsyncJobManager*>*>,
      _pairForJobHandler);

  factory->addHandler("/_admin/version",
                      RestHandlerCreator<RestVersionHandler>::createNoData,
                      nullptr);

  // further admin handlers
  factory->addHandler(
      "/_admin/log",
      RestHandlerCreator<arangodb::RestAdminLogHandler>::createNoData,
      nullptr);

  factory->addHandler("/_admin/work-monitor",
                      RestHandlerCreator<WorkMonitorHandler>::createNoData,
                      nullptr);

// This handler is to activate SYS_DEBUG_FAILAT on DB servers
#ifdef TRI_ENABLE_FAILURE_TESTS
  factory->addPrefixHandler("/_admin/debug",
                            RestHandlerCreator<RestDebugHandler>::createNoData,
                            nullptr);
#endif

  factory->addPrefixHandler(
      "/_admin/shutdown",
      RestHandlerCreator<arangodb::RestShutdownHandler>::createData<
          void*>,
      static_cast<void*>(_applicationServer));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief determine the requested database from the request URL
/// when the database is present in the request and is still "alive", its
/// reference-counter is increased by one
////////////////////////////////////////////////////////////////////////////////

static TRI_vocbase_t* LookupDatabaseFromRequest(
    arangodb::rest::HttpRequest* request, TRI_server_t* server) {
  // get the request endpoint
  ConnectionInfo const& ci = request->connectionInfo();
  std::string const& endpoint = ci.endpoint;

  // get the databases mapped to the endpoint
  ApplicationEndpointServer* s = static_cast<ApplicationEndpointServer*>(
      server->_applicationEndpointServer);
  std::vector<std::string> const& databases = s->getEndpointMapping(endpoint);

  // get database name from request
  std::string dbName = request->databaseName();

  if (databases.empty()) {
    // no databases defined. this means all databases are accessible via the
    // endpoint

    if (dbName.empty()) {
      // if no databases was specified in the request, use system database name
      // as a fallback
      dbName = TRI_VOC_SYSTEM_DATABASE;
      request->setDatabaseName(dbName);
    }
  } else {
    // only some databases are allowed for this endpoint
    if (dbName.empty()) {
      // no specific database requested, so use first mapped database
      TRI_ASSERT(!databases.empty());

      dbName = databases[0];
      request->setDatabaseName(dbName);
    } else {
      bool found = false;

      for (size_t i = 0; i < databases.size(); ++i) {
        if (dbName == databases.at(i)) {
          request->setDatabaseName(dbName);
          found = true;
          break;
        }
      }

      // requested database not found
      if (!found) {
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

static bool SetRequestContext(arangodb::rest::HttpRequest* request,
                              void* data) {
  TRI_server_t* server = static_cast<TRI_server_t*>(data);
  TRI_vocbase_t* vocbase = LookupDatabaseFromRequest(request, server);

  // invalid database name specified, database not found etc.
  if (vocbase == nullptr) {
    return false;
  }

  // database needs upgrade
  if (vocbase->_state == (sig_atomic_t)TRI_VOCBASE_STATE_FAILED_VERSION) {
    request->setRequestPath("/_msg/please-upgrade");
    return false;
  }

  VocbaseContext* ctx = new arangodb::VocbaseContext(request, server, vocbase);

  if (ctx == nullptr) {
    // out of memory
    return false;
  }

  // the "true" means the request is the owner of the context
  request->setRequestContext(ctx, true);

  return true;
}

ArangoServer::ArangoServer(int argc, char** argv)
    : _argc(argc),
      _argv(argv),
      _tempPath(),
      _applicationScheduler(nullptr),
      _applicationDispatcher(nullptr),
      _applicationEndpointServer(nullptr),
      _applicationCluster(nullptr),
      _jobManager(nullptr),
      _applicationV8(nullptr),
      _authenticateSystemOnly(false),
      _disableAuthentication(false),
      _disableAuthenticationUnixSockets(false),
      _dispatcherThreads(8),
      _dispatcherQueueSize(16384),
      _v8Contexts(8),
      _indexThreads(2),
      _databasePath(),
      _queryCacheMode("off"),
      _queryCacheMaxResults(128),
      _defaultMaximalSize(TRI_JOURNAL_DEFAULT_MAXIMAL_SIZE),
      _defaultWaitForSync(false),
      _forceSyncProperties(true),
      _ignoreDatafileErrors(false),
      _disableReplicationApplier(false),
      _disableQueryTracking(false),
      _throwCollectionNotLoadedError(false),
      _foxxQueues(true),
      _foxxQueuesPollInterval(1.0),
      _server(nullptr),
      _queryRegistry(nullptr),
      _pairForAqlHandler(nullptr),
      _pairForJobHandler(nullptr),
      _indexPool(nullptr),
      _threadAffinity(0) {
  TRI_SetApplicationName("arangod");

#ifndef TRI_HAVE_THREAD_AFFINITY
  _threadAffinity = 0;
#endif

// set working directory and database directory
#ifdef _WIN32
  _workingDirectory = ".";
#else
  _workingDirectory = "/var/tmp";
#endif

  _defaultLanguage = Utf8Helper::DefaultUtf8Helper.getCollatorLanguage();

  TRI_InitServerGlobals();

  _server = new TRI_server_t;
}

ArangoServer::~ArangoServer() {
  delete _indexPool;
  delete _jobManager;
  delete _server;

  Nonce::destroy();
}

void ArangoServer::buildApplicationServer() {
  _applicationServer =
      new ApplicationServer("arangod", "[<options>] <database-directory>",
                            rest::Version::getDetailed());

  std::string conf = TRI_BinaryName(_argv[0]) + ".conf";

  _applicationServer->setSystemConfigFile(conf);

  // arangod allows defining a user-specific configuration file. arangosh and
  // the other binaries don't
  _applicationServer->setUserConfigFile(
      ".arango" + std::string(1, TRI_DIR_SEPARATOR_CHAR) + std::string(conf));

  // initialize the server's write ahead log
  wal::LogfileManager::initialize(&_databasePath, _server);

  // and add the feature to the application server
  _applicationServer->addFeature(wal::LogfileManager::instance());

  // .............................................................................
  // dispatcher
  // .............................................................................

  _applicationDispatcher = new ApplicationDispatcher();
  _applicationServer->addFeature(_applicationDispatcher);

  // .............................................................................
  // multi-threading scheduler
  // .............................................................................

  _applicationScheduler = new ApplicationScheduler(_applicationServer);

  _applicationScheduler->allowMultiScheduler(true);
  _applicationDispatcher->setApplicationScheduler(_applicationScheduler);

  _applicationServer->addFeature(_applicationScheduler);

  // ...........................................................................
  // create QueryRegistry
  // ...........................................................................

  _queryRegistry = new aql::QueryRegistry();
  _server->_queryRegistry = _queryRegistry;

  // .............................................................................
  // V8 engine
  // .............................................................................

  _applicationV8 = new ApplicationV8(
      _server, _queryRegistry, _applicationScheduler, _applicationDispatcher);

  _applicationServer->addFeature(_applicationV8);

  // .............................................................................
  // MRuby engine (this has been removed from arangod in version 2.2)
  // .............................................................................

  std::string ignoreOpt;
  std::map<std::string, ProgramOptionsDescription> additional;

  additional["Hidden Options"](
      "ruby.gc-interval", &ignoreOpt,
      "Ruby garbage collection interval (each x requests)")(
      "ruby.action-directory", &ignoreOpt, "path to the Ruby action directory")(
      "ruby.modules-path", &ignoreOpt,
      "one or more directories separated by (semi-) colons")(
      "ruby.startup-directory", &ignoreOpt,
      "path to the directory containing alternate Ruby startup scripts")(
      "server.disable-replication-logger", &ignoreOpt,
      "start with replication logger turned off")(
      "database.force-sync-shapes", &ignoreOpt,
      "force syncing of shape data to disk, will use waitForSync value of "
      "collection when turned off (deprecated)")(
      "database.remove-on-drop", &ignoreOpt,
      "wipe a collection from disk after dropping");

  // .............................................................................
  // define server options
  // .............................................................................

  // command-line only options
  additional["General Options:help-default"](
      "console",
      "do not start as server, start a JavaScript emergency console instead")(
      "upgrade", "perform a database upgrade")(
      "check-version", "checks the versions of the database and exit");

  // .............................................................................
  // set language of default collator
  // .............................................................................

  additional["Server Options:help-default"]("temp-path", &_tempPath,
                                            "temporary path")(
      "default-language", &_defaultLanguage, "ISO-639 language code");

  // other options
  additional["Hidden Options"]("no-upgrade", "skip a database upgrade")(
      "start-service", "used to start as windows service")(
      "no-server", "do not start the server, if console is requested")(
      "use-thread-affinity", &_threadAffinity,
      "try to set thread affinity (0=disable, 1=disjunct, 2=overlap, "
      "3=scheduler, 4=dispatcher)");

// .............................................................................
// daemon and supervisor mode
// .............................................................................

#ifndef _WIN32
  additional["General Options:help-admin"]("daemon", "run as daemon")(
      "pid-file", &_pidFile, "pid-file in daemon mode")(
      "supervisor", "starts a supervisor and runs as daemon")(
      "working-directory", &_workingDirectory,
      "working directory in daemon mode");
#endif

#ifdef __APPLE__
  additional["General Options:help-admin"]("voice",
                                           "enable voice based welcome");
#endif

  additional["Hidden Options"]("development-mode",
                               "start server in development mode");

  // .............................................................................
  // javascript options
  // .............................................................................

  additional["Javascript Options:help-admin"](
      "javascript.script", &_scriptFile,
      "do not start as server, run script instead")(
      "javascript.script-parameter", &_scriptParameters, "script parameter");

  additional["Hidden Options"](
      "javascript.unit-tests", &_unitTests,
      "do not start as server, run unit tests instead");

  // .............................................................................
  // database options
  // .............................................................................

  additional["Database Options:help-admin"](
      "database.directory", &_databasePath, "path to the database directory")(
      "database.maximal-journal-size", &_defaultMaximalSize,
      "default maximal journal size, can be overwritten when creating a "
      "collection")("database.wait-for-sync", &_defaultWaitForSync,
                    "default wait-for-sync behavior, can be overwritten when "
                    "creating a collection")(
      "database.force-sync-properties", &_forceSyncProperties,
      "force syncing of collection properties to disk, will use waitForSync "
      "value of collection when turned off")(
      "database.ignore-datafile-errors", &_ignoreDatafileErrors,
      "load collections even if datafiles may contain errors")(
      "database.disable-query-tracking", &_disableQueryTracking,
      "turn off AQL query tracking by default")(
      "database.query-cache-mode", &_queryCacheMode,
      "mode for the AQL query cache (on, off, demand)")(
      "database.query-cache-max-results", &_queryCacheMaxResults,
      "maximum number of results in query cache per database")(
      "database.index-threads", &_indexThreads,
      "threads to start for parallel background index creation")(
      "database.throw-collection-not-loaded-error",
      &_throwCollectionNotLoadedError,
      "throw an error when accessing a collection that is still loading");

  // .............................................................................
  // cluster options
  // .............................................................................

  _applicationCluster =
      new ApplicationCluster(_server, _applicationDispatcher, _applicationV8);
  _applicationServer->addFeature(_applicationCluster);

  // .............................................................................
  // server options
  // .............................................................................

  // .............................................................................
  // for this server we display our own options such as port to use
  // .............................................................................

  additional["Server Options:help-admin"](
      "server.authenticate-system-only", &_authenticateSystemOnly,
      "use HTTP authentication only for requests to /_api and /_admin")(
      "server.disable-authentication", &_disableAuthentication,
      "disable authentication for ALL client requests")
#ifdef TRI_HAVE_LINUX_SOCKETS
      ("server.disable-authentication-unix-sockets",
       &_disableAuthenticationUnixSockets,
       "disable authentication for requests via UNIX domain sockets")
#endif
          ("server.disable-replication-applier", &_disableReplicationApplier,
           "start with replication applier turned off")(
              "server.allow-use-database", &ALLOW_USE_DATABASE_IN_REST_ACTIONS,
              "allow change of database in REST actions, only needed for "
              "unittests")("server.threads", &_dispatcherThreads,
                           "number of threads for basic operations")(
              "server.additional-threads", &_additionalThreads,
              "number of threads in additional queues")(
              "server.hide-product-header", &HttpResponse::HideProductHeader,
              "do not expose \"Server: ArangoDB\" header in HTTP responses")(
              "server.foxx-queues", &_foxxQueues, "enable Foxx queues")(
              "server.foxx-queues-poll-interval", &_foxxQueuesPollInterval,
              "Foxx queue manager poll interval (in seconds)")(
              "server.session-timeout", &VocbaseContext::ServerSessionTtl,
              "timeout of web interface server sessions (in seconds)");

  bool disableStatistics = false;

  additional["Server Options:help-admin"]("server.disable-statistics",
                                          &disableStatistics,
                                          "turn off statistics gathering");

  additional["Javascript Options:help-admin"](
      "javascript.v8-contexts", &_v8Contexts,
      "number of V8 contexts that are created for executing JavaScript "
      "actions");

  additional["Server Options:help-admin"](
      "scheduler.maximal-queue-size", &_dispatcherQueueSize,
      "maximum size of queue for asynchronous operations");

  // .............................................................................
  // endpoint server
  // .............................................................................

  _jobManager = new AsyncJobManager(ClusterCommRestCallback);

  _applicationEndpointServer = new ApplicationEndpointServer(
      _applicationServer, _applicationScheduler, _applicationDispatcher,
      _jobManager, "arangodb", &SetRequestContext, (void*)_server);
  _applicationServer->addFeature(_applicationEndpointServer);

  // .............................................................................
  // parse the command line options - exit if there is a parse error
  // .............................................................................

  if (!_applicationServer->parse(_argc, _argv, additional)) {
    LOG(FATAL) << "cannot parse command line arguments"; FATAL_ERROR_EXIT();
  }

  // set the temp-path
  _tempPath = StringUtils::rTrim(_tempPath, TRI_DIR_SEPARATOR_STR);

  if (_applicationServer->programOptions().has("temp-path")) {
    TRI_SetUserTempPath((char*)_tempPath.c_str());
  }

  // must be used after drop privileges and be called to set it to avoid raise
  // conditions
  TRI_GetTempPath();

  IGNORE_DATAFILE_ERRORS = _ignoreDatafileErrors;

  // .............................................................................
  // set language name
  // .............................................................................

  std::string languageName;

  if (!Utf8Helper::DefaultUtf8Helper.setCollatorLanguage(_defaultLanguage)) {
    char const* ICU_env = getenv("ICU_DATA");
    LOG(FATAL) << "failed to initialize ICU; ICU_DATA='" << (ICU_env != nullptr ? ICU_env : "") << "'"; FATAL_ERROR_EXIT();
  }

  if (Utf8Helper::DefaultUtf8Helper.getCollatorCountry() != "") {
    languageName =
        std::string(Utf8Helper::DefaultUtf8Helper.getCollatorLanguage() + "_" +
                    Utf8Helper::DefaultUtf8Helper.getCollatorCountry());
  } else {
    languageName = Utf8Helper::DefaultUtf8Helper.getCollatorLanguage();
  }

  // ...........................................................................
  // init nonces
  // ...........................................................................

  uint32_t optionNonceHashSize = 0;

  if (optionNonceHashSize > 0) {
    LOG(DEBUG) << "setting nonce hash size to " << optionNonceHashSize;
    Nonce::create(optionNonceHashSize);
  }

  if (disableStatistics) {
    TRI_ENABLE_STATISTICS = false;
  }

  // validate journal size
  if (_defaultMaximalSize < TRI_JOURNAL_MINIMAL_SIZE) {
    LOG(FATAL) << "invalid value for '--database.maximal-journal-size'. expected at least " << TRI_JOURNAL_MINIMAL_SIZE; FATAL_ERROR_EXIT();
  }

  // validate queue size
  if (_dispatcherQueueSize <= 128) {
    LOG(FATAL) << "invalid value for `--server.maximal-queue-size'"; FATAL_ERROR_EXIT();
  }

  // .............................................................................
  // set directories and scripts
  // .............................................................................

  std::vector<std::string> arguments = _applicationServer->programArguments();

  if (1 < arguments.size()) {
    LOG(FATAL) << "expected at most one database directory, got " << arguments.size(); FATAL_ERROR_EXIT();
  } else if (1 == arguments.size()) {
    _databasePath = arguments[0];
  }

  if (_databasePath.empty()) {
    LOG(INFO) << "please use the '--database.directory' option";
    LOG(FATAL) << "no database path has been supplied, giving up"; FATAL_ERROR_EXIT();
  }

  runStartupChecks();

  // strip trailing separators
  _databasePath = StringUtils::rTrim(_databasePath, TRI_DIR_SEPARATOR_STR);

  _applicationEndpointServer->setBasePath(_databasePath);

  // disable certain options in unittest or script mode
  OperationMode::server_operation_mode_e mode =
      OperationMode::determineMode(_applicationServer->programOptions());

  if (mode == OperationMode::MODE_CONSOLE) {
    _applicationScheduler->disableControlCHandler();
  }

  if (mode == OperationMode::MODE_SCRIPT ||
      mode == OperationMode::MODE_UNITTESTS) {
    // testing disables authentication
    _disableAuthentication = true;
  }

  TRI_SetThrowCollectionNotLoadedVocBase(nullptr,
                                         _throwCollectionNotLoadedError);

  // set global query tracking flag
  arangodb::aql::Query::DisableQueryTracking(_disableQueryTracking);

  // configure the query cache
  {
    std::pair<std::string, size_t> cacheProperties{_queryCacheMode,
                                                   _queryCacheMaxResults};
    arangodb::aql::QueryCache::instance()->setProperties(cacheProperties);
  }

  // .............................................................................
  // now run arangod
  // .............................................................................

  // dump version details
  LOG(INFO) << "" << rest::Version::getVerboseVersionString().c_str();

  LOG(INFO) << "using default language '" << languageName.c_str() << "'";

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
      LOG(INFO) << "please use the '--pid-file' option";
      LOG(FATAL) << "no pid-file defined, but daemon or supervisor mode was requested"; FATAL_ERROR_EXIT();
    }

    OperationMode::server_operation_mode_e mode =
        OperationMode::determineMode(_applicationServer->programOptions());
    if (mode != OperationMode::MODE_SERVER) {
      LOG(FATAL) << "invalid mode. must not specify --console together with --daemon or --supervisor"; FATAL_ERROR_EXIT();
    }

    // make the pid filename absolute
    int err = 0;
    std::string currentDir = FileUtils::currentDirectory(&err);
    char* absoluteFile =
        TRI_GetAbsolutePath(_pidFile.c_str(), currentDir.c_str());

    if (absoluteFile != nullptr) {
      _pidFile = std::string(absoluteFile);
      TRI_Free(TRI_UNKNOWN_MEM_ZONE, absoluteFile);

      LOG(DEBUG) << "using absolute pid file '" << _pidFile.c_str() << "'";
    } else {
      LOG(FATAL) << "cannot determine current directory"; FATAL_ERROR_EXIT();
    }
  }

  if (_indexThreads > 0) {
    if (_indexThreads > 128) {
      // some arbitrary limit
      _indexThreads = 128;
    }
  }
}

int ArangoServer::startupServer() {
  TRI_InitializeStatistics();

  OperationMode::server_operation_mode_e mode =
      OperationMode::determineMode(_applicationServer->programOptions());
  bool startServer = true;

  if (_applicationServer->programOptions().has("no-server")) {
    startServer = false;
    TRI_ENABLE_STATISTICS = false;
    // --no-server disables all replication appliers
    _disableReplicationApplier = true;
  }

  // check version
  bool checkVersion = false;

  if (_applicationServer->programOptions().has("check-version")) {
    checkVersion = true;
    // --check-version disables all replication appliers
    _disableReplicationApplier = true;
    if (_applicationCluster != nullptr) {
      _applicationCluster->disable();
    }
  }

  // run upgrade script
  bool performUpgrade = false;

  if (_applicationServer->programOptions().has("upgrade")) {
    performUpgrade = true;
    // --upgrade disables all replication appliers
    _disableReplicationApplier = true;
    if (_applicationCluster != nullptr) {
      _applicationCluster->disable();
    }
  }

  // skip an upgrade even if VERSION is missing
  bool skipUpgrade = false;

  if (_applicationServer->programOptions().has("no-upgrade")) {
    skipUpgrade = true;
  }

  // special treatment for the write-ahead log
  // the log must exist before all other server operations can start
  LOG(TRACE) << "starting WAL logfile manager";

  if (!wal::LogfileManager::instance()->prepare() ||
      !wal::LogfileManager::instance()->start()) {
    // unable to initialize & start WAL logfile manager
    LOG(FATAL) << "unable to start WAL logfile manager"; FATAL_ERROR_EXIT();
  }

  // .............................................................................
  // prepare the various parts of the Arango server
  // .............................................................................

  KeyGenerator::Initialize();

  if (_dispatcherThreads < 1) {
    _dispatcherThreads = 1;
  }

  startupProgress();

  // open all databases
  bool const iterateMarkersOnOpen =
      !wal::LogfileManager::instance()->hasFoundLastTick();

  openDatabases(checkVersion, performUpgrade, iterateMarkersOnOpen);

  if (!checkVersion) {
    if (!wal::LogfileManager::instance()->open()) {
      LOG(FATAL) << "Unable to finish WAL recovery procedure"; FATAL_ERROR_EXIT();
    }
  }

  startupProgress();

  // fetch the system database
  TRI_vocbase_t* vocbase =
      TRI_UseDatabaseServer(_server, TRI_VOC_SYSTEM_DATABASE);

  if (vocbase == nullptr) {
    LOG(FATAL) << "No _system database found in database directory. Cannot start!"; FATAL_ERROR_EXIT();
  }

  TRI_ASSERT(vocbase != nullptr);

  startupProgress();

  // initialize V8
  if (!_applicationServer->programOptions().has("javascript.v8-contexts")) {
    // the option was added recently so it's not always set
    // the behavior in older ArangoDB was to create one V8 context per
    // dispatcher thread
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
  } else if (mode == OperationMode::MODE_UNITTESTS ||
             mode == OperationMode::MODE_SCRIPT) {
    if (_v8Contexts == 1) {
      // at least two to allow both the test-runner and the scheduler to use a
      // V8 instance
      _v8Contexts = 2;
    }
  }

  startupProgress();

  _applicationV8->setVocbase(vocbase);
  _applicationV8->setConcurrency(_v8Contexts);
  _applicationV8->defineDouble("DISPATCHER_THREADS", _dispatcherThreads);
  _applicationV8->defineDouble("V8_CONTEXTS", _v8Contexts);

  // .............................................................................
  // prepare everything
  // .............................................................................

  startupProgress();

  if (!startServer) {
    _applicationScheduler->disable();
    _applicationDispatcher->disable();
    _applicationEndpointServer->disable();
    _applicationV8->disableActions();
  }

  // prepare scheduler and dispatcher
  _applicationServer->prepare();

  startupProgress();

  auto const role = ServerState::instance()->getRole();

  // now we can create the queues
  if (startServer) {
    _applicationDispatcher->buildStandardQueue(_dispatcherThreads,
                                               (int)_dispatcherQueueSize);

    if (role == ServerState::ROLE_COORDINATOR ||
        role == ServerState::ROLE_PRIMARY ||
        role == ServerState::ROLE_SECONDARY) {
      _applicationDispatcher->buildAQLQueue(_dispatcherThreads,
                                            (int)_dispatcherQueueSize);
    }

    for (size_t i = 0; i < _additionalThreads.size(); ++i) {
      int n = _additionalThreads[i];

      if (n < 1) {
        n = 1;
      }

      _additionalThreads[i] = n;

      _applicationDispatcher->buildExtraQueue(i + 1, n,
                                              (int)_dispatcherQueueSize);
    }
  }

  startupProgress();

  // and finish prepare
  _applicationServer->prepare2();

  startupProgress();

  // run version check (will exit!)
  if (checkVersion) {
    _applicationV8->versionCheck();
  }

  startupProgress();

  _applicationV8->upgradeDatabase(skipUpgrade, performUpgrade);

  startupProgress();

  // setup the V8 actions
  if (startServer) {
    _applicationV8->prepareServer();
  }

  startupProgress();

  _pairForAqlHandler = new std::pair<ApplicationV8*, aql::QueryRegistry*>(
      _applicationV8, _queryRegistry);
  _pairForJobHandler = new std::pair<Dispatcher*, AsyncJobManager*>(
      _applicationDispatcher->dispatcher(), _jobManager);

  // ...........................................................................
  // create endpoints and handlers
  // ...........................................................................

  // we pass the options by reference, so keep them until shutdown
  RestActionHandler::action_options_t httpOptions;
  httpOptions._vocbase = vocbase;

  if (startServer) {
    // start with enabled maintenance mode
    HttpHandlerFactory::setMaintenance(true);

    // create the server
    _applicationEndpointServer->buildServers();

    HttpHandlerFactory* handlerFactory =
        _applicationEndpointServer->getHandlerFactory();

    defineHandlers(handlerFactory);

    // add action handler
    handlerFactory->addPrefixHandler(
        "/", RestHandlerCreator<RestActionHandler>::createData<
                 RestActionHandler::action_options_t*>,
        (void*)&httpOptions);
  }

  startupProgress();

  // .............................................................................
  // try to figure out the thread affinity
  // .............................................................................

  size_t n = TRI_numberProcessors();

  if (n > 2 && _threadAffinity > 0) {
    size_t ns = _applicationScheduler->numberOfThreads();
    size_t nd = _applicationDispatcher->numberOfThreads();

    if (ns != 0 && nd != 0) {
      LOG(INFO) << "the server has " << n << " (hyper) cores, using " << ns << " scheduler threads, " << nd << " dispatcher threads";
    } else {
      _threadAffinity = 0;
    }

    switch (_threadAffinity) {
      case 1:
        if (n < ns + nd) {
          ns = static_cast<size_t>(round(1.0 * n * ns / (ns + nd)));
          nd = static_cast<size_t>(round(1.0 * n * nd / (ns + nd)));

          if (ns < 1) {
            ns = 1;
          }
          if (nd < 1) {
            nd = 1;
          }

          while (n < ns + nd) {
            if (1 < ns) {
              ns -= 1;
            } else if (1 < nd) {
              nd -= 1;
            } else {
              ns = 1;
              nd = 1;
            }
          }
        }

        break;

      case 2:
        if (n < ns) {
          ns = n;
        }

        if (n < nd) {
          nd = n;
        }

        break;

      case 3:
        if (n < ns) {
          ns = n;
        }

        nd = 0;

        break;

      case 4:
        if (n < nd) {
          nd = n;
        }

        ns = 0;

        break;

      default:
        _threadAffinity = 0;
        break;
    }

    if (_threadAffinity > 0) {
      TRI_ASSERT(ns <= n);
      TRI_ASSERT(nd <= n);

      std::vector<size_t> ps;
      std::vector<size_t> pd;

      for (size_t i = 0; i < ns; ++i) {
        ps.push_back(i);
      }

      for (size_t i = 0; i < nd; ++i) {
        pd.push_back(n - i - 1);
      }

      if (0 < ns) {
        _applicationScheduler->setProcessorAffinity(ps);
      }

      if (0 < nd) {
        _applicationDispatcher->setProcessorAffinity(pd);
      }

      if (0 < ns) {
        LOG(INFO) << "scheduler cores: " << ToString(ps).c_str();
      }
      if (0 < nd) {
        LOG(INFO) << "dispatcher cores: " << ToString(pd).c_str();
      }
    } else {
      LOG(INFO) << "the server has " << n << " (hyper) cores";
    }
  }


  // active deadlock detection in case we're not running in cluster mode
  if (!arangodb::ServerState::instance()->isRunningInCluster()) {
    TRI_EnableDeadlockDetectionDatabasesServer(_server);
  }


  // .............................................................................
  // start the work monitor
  // .............................................................................

  InitializeWorkMonitor();

  // .............................................................................
  // start the main event loop
  // .............................................................................

  _applicationServer->start();

  // for a cluster coordinator, the users are loaded at a later stage;
  // the kickstarter will trigger a bootstrap process
  //
  if (role != ServerState::ROLE_COORDINATOR &&
      role != ServerState::ROLE_PRIMARY &&
      role != ServerState::ROLE_SECONDARY) {
    // if the authentication info could not be loaded, but authentication is
    // turned on,
    // then we refuse to start
    if (!vocbase->_authInfoLoaded && !_disableAuthentication) {
      LOG(FATAL) << "could not load required authentication information"; FATAL_ERROR_EXIT();
    }
  }

  if (_disableAuthentication) {
    LOG(INFO) << "Authentication is turned off";
  }

  LOG(INFO) << "ArangoDB (version " << TRI_VERSION_FULL << ") is ready for business. Have fun!";

  startupFinished();

  int res;

  if (mode == OperationMode::MODE_CONSOLE) {
    res = runConsole(vocbase);
  } else if (mode == OperationMode::MODE_UNITTESTS) {
    res = runUnitTests(vocbase);
  } else if (mode == OperationMode::MODE_SCRIPT) {
    res = runScript(vocbase);
  } else {
    res = runServer(vocbase);
  }

  shutDownBegins();

  // stop the replication appliers so all replication transactions can end
  TRI_StopReplicationAppliersServer(_server);

  _applicationServer->stop();

  _server->_queryRegistry = nullptr;

  delete _queryRegistry;
  _queryRegistry = nullptr;
  delete _pairForAqlHandler;
  _pairForAqlHandler = nullptr;
  delete _pairForJobHandler;
  _pairForJobHandler = nullptr;

  closeDatabases();

  if (mode == OperationMode::MODE_CONSOLE) {
    std::cout << std::endl
         << TRI_BYE_MESSAGE << std::endl;
  }

  TRI_ShutdownStatistics();
  ShutdownWorkMonitor();

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief run arbitrary checks at startup
////////////////////////////////////////////////////////////////////////////////

void ArangoServer::runStartupChecks() {
#ifdef __arm__
  // detect alignment settings for ARM
  {
    // To change the alignment trap behavior, simply echo a number into
    // /proc/cpu/alignment.  The number is made up from various bits:
    //
    // bit             behavior when set
    // ---             -----------------
    //
    // 0               A user process performing an unaligned memory access
    //                 will cause the kernel to print a message indicating
    //                 process name, pid, pc, instruction, address, and the
    //                 fault code.
    //
    // 1               The kernel will attempt to fix up the user process
    //                 performing the unaligned access.  This is of course
    //                 slow (think about the floating point emulator) and
    //                 not recommended for production use.
    //
    // 2               The kernel will send a SIGBUS signal to the user process
    //                 performing the unaligned access.
    bool alignmentDetected = false;

    std::string const filename("/proc/cpu/alignment");
    try {
      std::string const cpuAlignment =
          arangodb::basics::FileUtils::slurp(filename);
      auto start = cpuAlignment.find("User faults:");

      if (start != std::string::npos) {
        start += strlen("User faults:");
        size_t end = start;
        while (end < cpuAlignment.size()) {
          if (cpuAlignment[end] == ' ' || cpuAlignment[end] == '\t') {
            ++end;
          } else {
            break;
          }
        }
        while (end < cpuAlignment.size()) {
          ++end;
          if (cpuAlignment[end] < '0' || cpuAlignment[end] > '9') {
            break;
          }
        }

        int64_t alignment =
            std::stol(std::string(cpuAlignment.c_str() + start, end - start));
        if ((alignment & 2) == 0) {
          LOG(FATAL) << "possibly incompatible CPU alignment settings found in '" << filename.c_str() << "'. this may cause arangod to abort with SIGBUS. please set the value in '" << filename.c_str() << "' to 2"; FATAL_ERROR_EXIT();
        }

        alignmentDetected = true;
      }

    } catch (...) {
      // ignore that we cannot detect the alignment
      LOG(TRACE) << "unable to detect CPU alignment settings. could not process file '" << filename.c_str() << "'";
    }

    if (!alignmentDetected) {
      LOG(WARN) << "unable to detect CPU alignment settings. could not process file '" << filename.c_str() << "'. this may cause arangod to abort with SIGBUS. it may be necessary to set the value in '" << filename.c_str() << "' to 2";
    }
  }
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief wait for the heartbeat thread to run
/// before the server responds to requests, the heartbeat thread should have
/// run at least once
////////////////////////////////////////////////////////////////////////////////

void ArangoServer::waitForHeartbeat() {
  if (!ServerState::instance()->isCoordinator()) {
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

int ArangoServer::runServer(TRI_vocbase_t* vocbase) {
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

int ArangoServer::runConsole(TRI_vocbase_t* vocbase) {
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

  while (!console.done() && ++iterations < 30) {
    usleep(100000);  // spin while console is still needed
  }

  return EXIT_SUCCESS;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief runs unit tests
////////////////////////////////////////////////////////////////////////////////

int ArangoServer::runUnitTests(TRI_vocbase_t* vocbase) {
  ApplicationV8::V8Context* context =
      _applicationV8->enterContext(vocbase, true);

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

    for (size_t i = 0; i < _unitTests.size(); ++i) {
      sysTestFiles->Set((uint32_t)i, TRI_V8_STD_STRING(_unitTests[i]));
    }

    localContext->Global()->Set(TRI_V8_ASCII_STRING("SYS_UNIT_TESTS"),
                                sysTestFiles);
    localContext->Global()->Set(TRI_V8_ASCII_STRING("SYS_UNIT_TESTS_RESULT"),
                                v8::True(isolate));

    v8::Local<v8::String> name(TRI_V8_ASCII_STRING(TRI_V8_SHELL_COMMAND_NAME));

    // run tests
    auto input = TRI_V8_ASCII_STRING(
        "require(\"@arangodb/testrunner\").runCommandLineTests();");
    TRI_ExecuteJavaScriptString(isolate, localContext, input, name, true);

    if (tryCatch.HasCaught()) {
      if (tryCatch.CanContinue()) {
        std::cerr << TRI_StringifyV8Exception(isolate, &tryCatch);
      } else {
        // will stop, so need for v8g->_canceled = true;
        TRI_ASSERT(!ok);
      }
    } else {
      ok = TRI_ObjectToBoolean(localContext->Global()->Get(
          TRI_V8_ASCII_STRING("SYS_UNIT_TESTS_RESULT")));
    }

    localContext->Exit();
  }

  _applicationV8->exitContext(context);

  return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief runs a script
////////////////////////////////////////////////////////////////////////////////

int ArangoServer::runScript(TRI_vocbase_t* vocbase) {
  bool ok = false;
  ApplicationV8::V8Context* context =
      _applicationV8->enterContext(vocbase, true);
  auto isolate = context->isolate;

  {
    v8::HandleScope globalScope(isolate);

    auto localContext = v8::Local<v8::Context>::New(isolate, context->_context);
    localContext->Enter();
    {
      v8::TryCatch tryCatch;
      v8::Context::Scope contextScope(localContext);
      for (size_t i = 0; i < _scriptFile.size(); ++i) {
        bool r =
            TRI_ExecuteGlobalJavaScriptFile(isolate, _scriptFile[i].c_str());

        if (!r) {
          LOG(FATAL) << "cannot load script '" << _scriptFile[i].c_str() << "', giving up"; FATAL_ERROR_EXIT();
        }
      }

      // run the garbage collection for at most 30 seconds
      TRI_RunGarbageCollectionV8(isolate, 30.0);

      // parameter array
      v8::Handle<v8::Array> params = v8::Array::New(isolate);

      params->Set(0, TRI_V8_STD_STRING(_scriptFile[_scriptFile.size() - 1]));

      for (size_t i = 0; i < _scriptParameters.size(); ++i) {
        params->Set((uint32_t)(i + 1), TRI_V8_STD_STRING(_scriptParameters[i]));
      }

      // call main
      v8::Handle<v8::String> mainFuncName = TRI_V8_ASCII_STRING("main");
      v8::Handle<v8::Function> main = v8::Handle<v8::Function>::Cast(
          localContext->Global()->Get(mainFuncName));

      if (main.IsEmpty() || main->IsUndefined()) {
        LOG(FATAL) << "no main function defined, giving up"; FATAL_ERROR_EXIT();
      } else {
        v8::Handle<v8::Value> args[] = {params};

        try {
          v8::Handle<v8::Value> result = main->Call(main, 1, args);

          if (tryCatch.HasCaught()) {
            if (tryCatch.CanContinue()) {
              TRI_LogV8Exception(isolate, &tryCatch);
            } else {
              // will stop, so need for v8g->_canceled = true;
              TRI_ASSERT(!ok);
            }
          } else {
            ok = TRI_ObjectToDouble(result) == 0;
          }
        } catch (arangodb::basics::Exception const& ex) {
          LOG(ERR) << "caught exception " << TRI_errno_string(ex.code()) << ": " << ex.what();
          ok = false;
        } catch (std::bad_alloc const&) {
          LOG(ERR) << "caught exception " << TRI_errno_string(TRI_ERROR_OUT_OF_MEMORY);
          ok = false;
        } catch (...) {
          LOG(ERR) << "caught unknown exception";
          ok = false;
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

void ArangoServer::openDatabases(bool checkVersion, bool performUpgrade,
                                 bool iterateMarkersOnOpen) {
  TRI_vocbase_defaults_t defaults;

  // override with command-line options
  defaults.defaultMaximalSize = _defaultMaximalSize;
  defaults.defaultWaitForSync = _defaultWaitForSync;
  defaults.requireAuthentication = !_disableAuthentication;
  defaults.requireAuthenticationUnixSockets =
      !_disableAuthenticationUnixSockets;
  defaults.authenticateSystemOnly = _authenticateSystemOnly;
  defaults.forceSyncProperties = _forceSyncProperties;

  TRI_ASSERT(_server != nullptr);

  if (_indexThreads > 0) {
    _indexPool =
        new arangodb::basics::ThreadPool(_indexThreads, "IndexBuilder");
  }

  int res = TRI_InitServer(_server, _applicationEndpointServer, _indexPool,
                           _databasePath.c_str(),
                           _applicationV8->appPath().c_str(), &defaults,
                           _disableReplicationApplier, iterateMarkersOnOpen);

  if (res != TRI_ERROR_NO_ERROR) {
    LOG(FATAL) << "cannot create server instance: out of memory"; FATAL_ERROR_EXIT();
  }

  res = TRI_StartServer(_server, checkVersion, performUpgrade);

  if (res != TRI_ERROR_NO_ERROR) {
    if (checkVersion && res == TRI_ERROR_ARANGO_EMPTY_DATADIR) {
      TRI_EXIT_FUNCTION(EXIT_SUCCESS, nullptr);
    }

    LOG(FATAL) << "cannot start server: " << TRI_errno_string(res); FATAL_ERROR_EXIT();
  }

  LOG(TRACE) << "found system database";
}

////////////////////////////////////////////////////////////////////////////////
/// @brief closes all databases
////////////////////////////////////////////////////////////////////////////////

void ArangoServer::closeDatabases() {
  TRI_ASSERT(_server != nullptr);

  TRI_CleanupActions();

  // stop the replication appliers so all replication transactions can end
  TRI_StopReplicationAppliersServer(_server);

  // enforce logfile manager shutdown so we are sure no one else will
  // write to the logs
  wal::LogfileManager::instance()->stop();

  TRI_StopServer(_server);

  LOG(INFO) << "ArangoDB has been shut down";
}
