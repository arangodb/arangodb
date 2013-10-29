////////////////////////////////////////////////////////////////////////////////
/// @brief arango server
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "ArangoServer.h"

#include <v8.h>

#include "BasicsC/common.h"

#ifdef TRI_ENABLE_MRUBY
#include "mruby.h"
#include "mruby/compile.h"
#include "mruby/data.h"
#include "mruby/proc.h"
#include "mruby/variable.h"
#endif

#include "Actions/RestActionHandler.h"
#include "Actions/actions.h"
#include "Admin/ApplicationAdminServer.h"
#include "Admin/RestHandlerCreator.h"
#include "Basics/FileUtils.h"
#include "Basics/Nonce.h"
#include "Basics/ProgramOptions.h"
#include "Basics/ProgramOptionsDescription.h"
#include "Basics/RandomGenerator.h"
#include "Basics/Utf8Helper.h"
#include "BasicsC/files.h"
#include "BasicsC/init.h"
#include "BasicsC/logging.h"
#include "BasicsC/messages.h"
#include "BasicsC/tri-strings.h"
#include "Dispatcher/ApplicationDispatcher.h"
#include "Dispatcher/Dispatcher.h"
#include "HttpServer/ApplicationEndpointServer.h"
#include "HttpServer/AsyncJobManager.h"
#include "HttpServer/HttpHandlerFactory.h"
#include "Rest/InitialiseRest.h"
#include "Rest/OperationMode.h"
#include "Rest/Version.h"
#include "RestHandler/RestBatchHandler.h"
#include "RestHandler/RestDocumentHandler.h"
#include "RestHandler/RestEdgeHandler.h"
#include "RestHandler/RestImportHandler.h"
#include "RestHandler/RestReplicationHandler.h"
#include "RestHandler/RestUploadHandler.h"
#include "RestServer/VocbaseContext.h"
#include "Scheduler/ApplicationScheduler.h"
#include "Statistics/statistics.h"

#include "V8/V8LineEditor.h"
#include "V8/v8-conv.h"
#include "V8/v8-utils.h"
#include "V8Server/ApplicationV8.h"
#include "VocBase/auth.h"
#include "VocBase/server.h"

#ifdef TRI_ENABLE_MRUBY
#include "MRServer/ApplicationMR.h"
#include "MRServer/mr-actions.h"
#include "MRuby/MRLineEditor.h"
#include "MRuby/MRLoader.h"
#endif

using namespace std;
using namespace triagens::basics;
using namespace triagens::rest;
using namespace triagens::admin;
using namespace triagens::arango;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief define "_api" handlers
////////////////////////////////////////////////////////////////////////////////

static void DefineApiHandlers (HttpHandlerFactory* factory,
                               ApplicationAdminServer* admin,
                               AsyncJobManager* jobManager) {

  // add "/version" handler
  admin->addBasicHandlers(factory, "/_api", (void*) jobManager);
  
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
}

////////////////////////////////////////////////////////////////////////////////
/// @brief define "admin" handlers
////////////////////////////////////////////////////////////////////////////////

static void DefineAdminHandlers (HttpHandlerFactory* factory,
                                 ApplicationAdminServer* admin,
                                 AsyncJobManager* jobManager) {

  // add "/version" handler
  admin->addBasicHandlers(factory, "/_admin", (void*) jobManager);

  // add admin handlers
  admin->addHandlers(factory, "/_admin");
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

      if (! found) {
        // requested database not found
        return 0;
      }
    }
  }
  

  TRI_vocbase_t* vocbase = TRI_UseDatabaseServer(server, dbName.c_str()); 
  
  if (vocbase == 0) {
    // database not found
    return 0;
  } 

  return vocbase;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add the context to a request
////////////////////////////////////////////////////////////////////////////////

static bool SetRequestContext (triagens::rest::HttpRequest* request, 
                               void* data) {

  TRI_server_t* server   = (TRI_server_t*) data;
  TRI_vocbase_t* vocbase = LookupDatabaseFromRequest(request, server);

  if (vocbase == 0) {
    // invalid database name specified, database not found etc.
    return false;
  }

  VocbaseContext* ctx = new triagens::arango::VocbaseContext(request, server, vocbase);

  if (ctx == 0) {
    // out of memory
    return false;
  }
  
  // the "true" means the request is the owner of the context
  request->setRequestContext(ctx, true);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                class ArangoServer
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

ArangoServer::ArangoServer (int argc, char** argv)
  : _argc(argc),
    _argv(argv),
    _tempPath(),
    _applicationScheduler(0),
    _applicationDispatcher(0),
    _applicationEndpointServer(0),
    _applicationAdminServer(0),
    _jobManager(0),
#ifdef TRI_ENABLE_MRUBY
    _applicationMR(0),
#endif
    _applicationV8(0),
    _authenticateSystemOnly(false),
    _disableAuthentication(false),
    _dispatcherThreads(8),
    _dispatcherQueueSize(8192),
    _databasePath(),
    _defaultMaximalSize(TRI_JOURNAL_DEFAULT_MAXIMAL_SIZE),
    _defaultWaitForSync(false),
    _developmentMode(false),
    _forceSyncProperties(true),
    _forceSyncShapes(true),
    _disableReplicationLogger(false),
    _disableReplicationApplier(false),
    _removeOnCompacted(true),
    _removeOnDrop(true),
    _server(0) {

  char* p = TRI_GetTempPath();
  // copy the string
  _tempPath = string(p);
  TRI_FreeString(TRI_CORE_MEM_ZONE, p);

  // set working directory and database directory
  _workingDirectory = "/var/tmp";

  _defaultLanguage = Utf8Helper::DefaultUtf8Helper.getCollatorLanguage();

 
  TRI_InitialiseServer();

  _server = TRI_CreateServer();

  if (_server == 0) {
    LOG_FATAL_AND_EXIT("could not create server instance");
  }
}
  
////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

ArangoServer::~ArangoServer () {
  if (_jobManager != 0) {
    delete _jobManager;
  }

  if (_server != 0) {
    TRI_FreeServer(_server);
  }
  
  TRI_ShutdownServer();

  Nonce::destroy();
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 AnyServer methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void ArangoServer::buildApplicationServer () {
  map<string, ProgramOptionsDescription> additional;

  _applicationServer = new ApplicationServer("arangod", "[<options>] <database-directory>", rest::Version::getDetailed());
  if (_applicationServer == 0) {
    LOG_FATAL_AND_EXIT("out of memory");
  }

  _applicationServer->setSystemConfigFile("arangod.conf");

  // arangod allows defining a user-specific configuration file. arangosh and the other binaries don't
  _applicationServer->setUserConfigFile(string(".arango") + string(1, TRI_DIR_SEPARATOR_CHAR) + string("arangod.conf") );


  // .............................................................................
  // multi-threading scheduler
  // .............................................................................

  _applicationScheduler = new ApplicationScheduler(_applicationServer);
  if (_applicationScheduler == 0) {
    LOG_FATAL_AND_EXIT("out of memory");
  }
  _applicationScheduler->allowMultiScheduler(true);

  _applicationServer->addFeature(_applicationScheduler);

  // .............................................................................
  // dispatcher
  // .............................................................................

  _applicationDispatcher = new ApplicationDispatcher(_applicationScheduler);
  if (_applicationDispatcher == 0) {
    LOG_FATAL_AND_EXIT("out of memory");
  }
  _applicationServer->addFeature(_applicationDispatcher);

  // .............................................................................
  // V8 engine
  // .............................................................................

  _applicationV8 = new ApplicationV8(_server);
  if (_applicationV8 == 0) {
    LOG_FATAL_AND_EXIT("out of memory");
  }
  _applicationServer->addFeature(_applicationV8);

  // .............................................................................
  // MRuby engine
  // .............................................................................

#ifdef TRI_ENABLE_MRUBY

  _applicationMR = new ApplicationMR(_server);
  if (_applicationMR == 0) {
    LOG_FATAL_AND_EXIT("out of memory");
  }
  _applicationServer->addFeature(_applicationMR);

#else

  string ignoreOpt;

  additional[ApplicationServer::OPTIONS_HIDDEN]
    ("ruby.gc-interval", &ignoreOpt, "Ruby garbage collection interval (each x requests)")
    ("ruby.action-directory", &ignoreOpt, "path to the Ruby action directory")
    ("ruby.modules-path", &ignoreOpt, "one or more directories separated by (semi-) colons")
    ("ruby.startup-directory", &ignoreOpt, "path to the directory containing alternate Ruby startup scripts")
  ;

#endif
  
  // .............................................................................
  // and start a simple admin server
  // .............................................................................

  _applicationAdminServer = new ApplicationAdminServer();
  if (_applicationAdminServer == 0) {
    LOG_FATAL_AND_EXIT("out of memory");
  }

  _applicationServer->addFeature(_applicationAdminServer);
  _applicationAdminServer->allowLogViewer();

  // .............................................................................
  // define server options
  // .............................................................................

  // .............................................................................
  // daemon and supervisor mode
  // .............................................................................


  additional[ApplicationServer::OPTIONS_CMDLINE]
    ("console", "do not start as server, start a JavaScript emergency console instead")
    ("temp-path", &_tempPath, "temporary path")
    ("upgrade", "perform a database upgrade")
  ;

  additional[ApplicationServer::OPTIONS_HIDDEN]
    ("no-upgrade", "skip a database upgrade")
  ;

#ifdef TRI_ENABLE_MRUBY
  additional[ApplicationServer::OPTIONS_CMDLINE]
    ("ruby-console", "do not start as server, start a Ruby emergency console instead")
  ;
#endif

  additional[ApplicationServer::OPTIONS_CMDLINE + ":help-extended"]
    ("daemon", "run as daemon")
    ("pid-file", &_pidFile, "pid-file in daemon mode")
    ("supervisor", "starts a supervisor and runs as daemon")
    ("working-directory", &_workingDirectory, "working directory in daemon mode")
    ("default-language", &_defaultLanguage, "ISO-639 language code")
  ;

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
    ("jslint", &_jslint, "do not start as server, run js lint instead")
    ("javascript.unit-tests", &_unitTests, "do not start as server, run unit tests instead")
  ;
  
  // .............................................................................
  // database options
  // .............................................................................

  additional["DIRECTORY Options:help-admin"]
    ("database.directory", &_databasePath, "path to the database directory")
  ;

  additional["DATABASE Options:help-admin"]
    ("database.remove-on-drop", &_removeOnDrop, "wipe a collection from disk after dropping")
    ("database.maximal-journal-size", &_defaultMaximalSize, "default maximal journal size, can be overwritten when creating a collection")
    ("database.wait-for-sync", &_defaultWaitForSync, "default wait-for-sync behavior, can be overwritten when creating a collection")
    ("database.force-sync-shapes", &_forceSyncShapes, "force syncing of shape data to disk, will use waitForSync value of collection when turned off")
    ("database.force-sync-properties", &_forceSyncProperties, "force syncing of collection properties to disk, will use waitForSync value of collection when turned off")
  ;

  additional["DATABASE Options:help-devel"]
    ("database.remove-on-compacted", &_removeOnCompacted, "wipe a datafile from disk after compaction")
  ;

  // .............................................................................
  // server options
  // .............................................................................

  // .............................................................................
  // for this server we display our own options such as port to use
  // .............................................................................

  additional[ApplicationServer::OPTIONS_SERVER + ":help-admin"]
    ("server.authenticate-system-only", &_authenticateSystemOnly, "use HTTP authentication only for requests to /_api and /_admin")
    ("server.disable-authentication", &_disableAuthentication, "disable authentication for ALL client requests")
    ("server.disable-replication-logger", &_disableReplicationLogger, "start with replication logger turned off")
    ("server.disable-replication-applier", &_disableReplicationApplier, "start with replication applier turned off")
  ;


  bool disableStatistics = false;

#if TRI_ENABLE_FIGURES
  additional[ApplicationServer::OPTIONS_SERVER + ":help-admin"]
    ("server.disable-statistics", &disableStatistics, "turn off statistics gathering")
  ;
#endif

  additional["THREAD Options:help-admin"]
    ("server.threads", &_dispatcherThreads, "number of threads for basic operations")
  ;
  
  additional["Server Options:help-extended"]
    ("scheduler.maximal-queue-size", &_dispatcherQueueSize, "maximum size of queue for asynchronous operations")
  ;

  // .............................................................................
  // endpoint server
  // .............................................................................

  _jobManager = new AsyncJobManager(&TRI_NewTickServer);

  _applicationEndpointServer = new ApplicationEndpointServer(_applicationServer,
                                                             _applicationScheduler,
                                                             _applicationDispatcher,
                                                             _jobManager,
                                                             "arangodb",
                                                             &SetRequestContext,
                                                             (void*) _server);
  if (_applicationEndpointServer == 0) {
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

  // configure v8
  if (_applicationServer->programOptions().has("development-mode")) {
    _developmentMode = true;
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

  // .............................................................................
  // now run arangod
  // .............................................................................
  
  // dump version details
  LOG_INFO("%s", rest::Version::getVerboseVersionString().c_str());

  LOG_INFO("using default language '%s'", languageName.c_str());

  OperationMode::server_operation_mode_e mode = OperationMode::determineMode(_applicationServer->programOptions());

  if (mode == OperationMode::MODE_CONSOLE ||
      mode == OperationMode::MODE_UNITTESTS ||
      mode == OperationMode::MODE_JSLINT ||
      mode == OperationMode::MODE_SCRIPT) {
    int res = executeConsole(mode);

    TRI_EXIT_FUNCTION(res, NULL);
  }

#ifdef TRI_ENABLE_MRUBY
  else if (mode == OperationMode::MODE_RUBY_CONSOLE) {
    int res = executeRubyConsole();

    TRI_EXIT_FUNCTION(res, NULL);
  }
#endif

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
  v8::HandleScope scope;
  
  // .............................................................................
  // prepare the various parts of the Arango server
  // .............................................................................

  if (_dispatcherThreads < 1) {
    _dispatcherThreads = 1;
  }
  

  // open all databases
  openDatabases();

  // fetch the system database
  TRI_vocbase_t* vocbase = TRI_UseDatabaseServer(_server, TRI_VOC_SYSTEM_DATABASE);
  assert(vocbase != 0);


  _applicationV8->setVocbase(vocbase);
  _applicationV8->setConcurrency(_dispatcherThreads);

  if (_applicationServer->programOptions().has("upgrade")) {
    _applicationV8->performUpgrade();
  }

  // skip an upgrade even if VERSION is missing
  if (_applicationServer->programOptions().has("no-upgrade")) {
    _applicationV8->skipUpgrade();
  }

#if TRI_ENABLE_MRUBY
  _applicationMR->setVocbase(vocbase);
  _applicationMR->setConcurrency(_dispatcherThreads);
#endif

  _applicationServer->prepare();


  // .............................................................................
  // create the dispatcher
  // .............................................................................

  _applicationDispatcher->buildStandardQueue(_dispatcherThreads, (int) _dispatcherQueueSize);

  _applicationServer->prepare2();
  

  // we pass the options by reference, so keep them until shutdown
  RestActionHandler::action_options_t httpOptions;
  httpOptions._vocbase = vocbase;
  httpOptions._queue = "STANDARD";

  // create the handlers
  httpOptions._contexts.insert("user");
  httpOptions._contexts.insert("api");
  httpOptions._contexts.insert("admin");


  // create the server
  _applicationEndpointServer->buildServers();

  HttpHandlerFactory* handlerFactory = _applicationEndpointServer->getHandlerFactory();

  DefineApiHandlers(handlerFactory, _applicationAdminServer, _jobManager);
  DefineAdminHandlers(handlerFactory, _applicationAdminServer, _jobManager);

  // add action handler
  handlerFactory->addPrefixHandler(
    "/",
    RestHandlerCreator<RestActionHandler>::createData<RestActionHandler::action_options_t*>,
    (void*) &httpOptions);

  // .............................................................................
  // start the main event loop
  // .............................................................................

  _applicationServer->start();

  // load authentication
  TRI_LoadAuthInfoVocBase(vocbase);

  // if the authentication info could not be loaded, but authentication is turned on,
  // then we refuse to start
  if (! vocbase->_authInfoLoaded && ! _disableAuthentication) {
    LOG_FATAL_AND_EXIT("could not load required authentication information");
  }

  if (_disableAuthentication) {
    LOG_INFO("Authentication is turned off");
  }

  LOG_INFO("ArangoDB (version " TRI_VERSION_FULL ") is ready for business. Have fun!");

  _applicationServer->wait();

  // .............................................................................
  // and cleanup
  // .............................................................................

  _applicationServer->stop();

  closeDatabases();

  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the JavaScript emergency console
////////////////////////////////////////////////////////////////////////////////

int ArangoServer::executeConsole (OperationMode::server_operation_mode_e mode) {
  // open all databases
  openDatabases();

  // fetch the system database
  TRI_vocbase_t* vocbase = TRI_UseDatabaseServer(_server, TRI_VOC_SYSTEM_DATABASE);

  if (vocbase == 0) {
    LOG_FATAL_AND_EXIT("cannot start server");
  }

  // load authentication
  TRI_LoadAuthInfoVocBase(vocbase);

  // set-up V8 context
  _applicationV8->setVocbase(vocbase);
  _applicationV8->setConcurrency(1);
  _applicationV8->disableActions();

  if (_applicationServer->programOptions().has("upgrade")) {
    _applicationV8->performUpgrade();
  }

  // skip an upgrade even if VERSION is missing
  if (_applicationServer->programOptions().has("no-upgrade")) {
    _applicationV8->skipUpgrade();
  }

  bool ok = _applicationV8->prepare();

  if (! ok) {
    LOG_FATAL_AND_EXIT("cannot initialize V8 enigne");
  }

  _applicationV8->start();

  // enter V8 context
  ApplicationV8::V8Context* context = _applicationV8->enterContext(vocbase, true, true);

  // .............................................................................
  // execute everything with a global scope
  // .............................................................................

  {
    v8::HandleScope globalScope;

    // run the shell
    if (mode != OperationMode::MODE_SCRIPT) {
      cout << "ArangoDB JavaScript emergency console (" << rest::Version::getVerboseVersionString() << ")" << endl;
    }
    else {
      LOG_INFO("%s", rest::Version::getVerboseVersionString().c_str());
    }

    v8::Local<v8::String> name(v8::String::New("(arango)"));
    v8::Context::Scope contextScope(context->_context);

    ok = true;

    switch (mode) {

      // .............................................................................
      // run all unit tests
      // .............................................................................

      case OperationMode::MODE_UNITTESTS: {
        v8::HandleScope scope;
        v8::TryCatch tryCatch;

        // set-up unit tests array
        v8::Handle<v8::Array> sysTestFiles = v8::Array::New();

        for (size_t i = 0;  i < _unitTests.size();  ++i) {
          sysTestFiles->Set((uint32_t) i, v8::String::New(_unitTests[i].c_str()));
        }

        context->_context->Global()->Set(v8::String::New("SYS_UNIT_TESTS"), sysTestFiles);
        context->_context->Global()->Set(v8::String::New("SYS_UNIT_TESTS_RESULT"), v8::True());

        // run tests
        char const* input = "require(\"jsunity\").runCommandLineTests();";
        TRI_ExecuteJavaScriptString(context->_context, v8::String::New(input), name, true);

        if (tryCatch.HasCaught()) {
          cout << TRI_StringifyV8Exception(&tryCatch);
          ok = false;
        }
        else {
          ok = TRI_ObjectToBoolean(context->_context->Global()->Get(v8::String::New("SYS_UNIT_TESTS_RESULT")));
        }

        break;
      }

      // .............................................................................
      // run jslint
      // .............................................................................

      case OperationMode::MODE_JSLINT: {
        v8::HandleScope scope;
        v8::TryCatch tryCatch;

        // set-up tests files array
        v8::Handle<v8::Array> sysTestFiles = v8::Array::New();

        for (size_t i = 0;  i < _jslint.size();  ++i) {
          sysTestFiles->Set((uint32_t) i, v8::String::New(_jslint[i].c_str()));
        }

        context->_context->Global()->Set(v8::String::New("SYS_UNIT_TESTS"), sysTestFiles);
        context->_context->Global()->Set(v8::String::New("SYS_UNIT_TESTS_RESULT"), v8::True());

        char const* input = "require(\"jslint\").runCommandLineTests({ });";
        TRI_ExecuteJavaScriptString(context->_context, v8::String::New(input), name, true);

        if (tryCatch.HasCaught()) {
          cout << TRI_StringifyV8Exception(&tryCatch);
          ok = false;
        }
        else {
          ok = TRI_ObjectToBoolean(context->_context->Global()->Get(v8::String::New("SYS_UNIT_TESTS_RESULT")));
        }

        break;
      }

      // .............................................................................
      // run script
      // .............................................................................

      case OperationMode::MODE_SCRIPT: {
        v8::TryCatch tryCatch;

        for (size_t i = 0;  i < _scriptFile.size();  ++i) {
          bool r = TRI_ExecuteGlobalJavaScriptFile(_scriptFile[i].c_str());

          if (! r) {
            LOG_FATAL_AND_EXIT("cannot load script '%s', giving up", _scriptFile[i].c_str());
          }
        }

        v8::V8::LowMemoryNotification();
        while(! v8::V8::IdleNotification()) {
        }

        if (ok) {

          // parameter array
          v8::Handle<v8::Array> params = v8::Array::New();

          params->Set(0, v8::String::New(_scriptFile[_scriptFile.size() - 1].c_str()));

          for (size_t i = 0;  i < _scriptParameters.size();  ++i) {
            params->Set((uint32_t) (i + 1), v8::String::New(_scriptParameters[i].c_str()));
          }

          // call main
          v8::Handle<v8::String> mainFuncName = v8::String::New("main");
          v8::Handle<v8::Function> main = v8::Handle<v8::Function>::Cast(context->_context->Global()->Get(mainFuncName));

          if (main.IsEmpty() || main->IsUndefined()) {
            LOG_FATAL_AND_EXIT("no main function defined, giving up");
          }
          else {
            v8::Handle<v8::Value> args[] = { params };

            v8::Handle<v8::Value> result = main->Call(main, 1, args);

            if (tryCatch.HasCaught()) {
              TRI_LogV8Exception(&tryCatch);
              ok = false;
            }
            else {
              ok = TRI_ObjectToDouble(result) == 0;
            }
          }
        }

        break;
      }

      // .............................................................................
      // run console
      // .............................................................................

      case OperationMode::MODE_CONSOLE: {
        V8LineEditor console(context->_context, ".arangod.history");

        console.open(true);

        while (true) {
          v8::V8::LowMemoryNotification();
          while(! v8::V8::IdleNotification()) {
          }

          char* input = console.prompt("arangod> ");

          if (input == 0) {
            printf("<ctrl-D>\n%s\n", TRI_BYE_MESSAGE);
            break;
          }

          if (*input == '\0') {
            TRI_FreeString(TRI_UNKNOWN_MEM_ZONE, input);
            continue;
          }

          console.addHistory(input);

          v8::HandleScope scope;
          v8::TryCatch tryCatch;

          TRI_ExecuteJavaScriptString(context->_context, v8::String::New(input), name, true);
          TRI_FreeString(TRI_UNKNOWN_MEM_ZONE, input);

          if (tryCatch.HasCaught()) {
            cout << TRI_StringifyV8Exception(&tryCatch);
          }
        }

        break;
      }

      default: {
        assert(false);
      }
    }
  }

  // .............................................................................
  // and return from the context and isolate
  // .............................................................................

  _applicationV8->exitContext(context);

  _applicationV8->close();
  _applicationV8->stop();


  closeDatabases();
  Random::shutdown();

  if (! ok) {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the MRuby emergency shell
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_MRUBY

struct RClass* ArangoDatabaseClass;
struct RClass* ArangoCollectionClass;

mrb_value MR_ArangoDatabase_Initialize (mrb_state* mrb, mrb_value exc) {
  printf("initializer of ArangoDatabase called\n");
  return exc;
}

static void MR_ArangoDatabase_Free (mrb_state* mrb, void* p) {
  printf("free of ArangoDatabase called\n");
}

static const struct mrb_data_type MR_ArangoDatabase_Type = {
  "ArangoDatabase", MR_ArangoDatabase_Free
};

static void MR_ArangoCollection_Free (mrb_state* mrb, void* p) {
  printf("free of ArangoCollection called\n");
}

static const struct mrb_data_type MR_ArangoCollection_Type = {
  "ArangoDatabase", MR_ArangoCollection_Free
};

mrb_value MR_ArangoDatabase_Collection (mrb_state* mrb, mrb_value self) {
  char* name;
  TRI_vocbase_t* vocbase;
  TRI_vocbase_col_t* collection;
  struct RData* rdata;

  // check "class.c" to see how to specify the arguments
  mrb_get_args(mrb, "s", &name);

  if (name == 0) {
    return self;
  }

  // check

  printf("using collection '%s'\n", name);

  // looking at "mruby.h" I assume that is the way to unwrap the pointer
  rdata = (struct RData*) mrb_object(self);
  vocbase = (TRI_vocbase_t*) rdata->data;
  collection = TRI_LookupCollectionByNameVocBase(vocbase, name);

  if (collection == NULL) {
    printf("unknown collection (TODO raise error)\n");
    return self;
  }

  return mrb_obj_value(Data_Wrap_Struct(mrb, ArangoCollectionClass, &MR_ArangoCollection_Type, (void*) collection));
}

  // setup the classes
#if 0
  struct RClass* ArangoDatabaseClass = mrb_define_class(mrb, "ArangoDatabase", mrb->object_class);
  struct RClass* ArangoCollectionClass = mrb_define_class(mrb, "ArangoCollection", mrb->object_class);

  // add an initializer (for TESTING only)
  mrb_define_method(mrb, ArangoDatabaseClass, "initialize", MR_ArangoDatabase_Initialize, ARGS_ANY());

  // add a method to extract the collection
  mrb_define_method(mrb, ArangoDatabaseClass, "_collection", MR_ArangoDatabase_Collection, ARGS_ANY());

  // create the database variable
  mrb_value db = mrb_obj_value(Data_Wrap_Struct(mrb, ArangoDatabaseClass, &MR_ArangoDatabase_Type, (void*) _vocbase));

  mrb_gv_set(mrb, mrb_intern(mrb, "$db"), db);

  // read-eval-print loop
  mrb_define_const(mrb, "$db", db);
#endif


int ArangoServer::executeRubyConsole () {
  // open all databases
  openDatabases();

  // fetch the system database
  TRI_vocbase_t* vocbase = TRI_UseDatabaseServer(_server, TRI_VOC_SYSTEM_DATABASE);
  assert(vocbase != 0);

  // load authentication
  TRI_LoadAuthInfoVocBase(vocbase);

  // set-up MRuby context
  _applicationMR->setVocbase(vocbase);
  _applicationMR->setConcurrency(1);
  _applicationMR->disableActions();

  bool ok = _applicationMR->prepare();

  if (! ok) {
    LOG_FATAL_AND_EXIT("cannot initialize MRuby enigne");
  }

  _applicationMR->start();

  // enter MR context
  ApplicationMR::MRContext* context = _applicationMR->enterContext();

  // create a line editor
  cout << "ArangoDB MRuby emergency console (" << rest::Version::getVerboseVersionString() << ")" << endl;

  MRLineEditor console(context->_mrb, ".arangod-ruby.history");

  console.open(false);

  while (true) {
    char* input = console.prompt("arangod (ruby)> ");

    if (input == 0) {
      printf("<ctrl-D>\n" TRI_BYE_MESSAGE "\n");
      break;
    }

    if (*input == '\0') {
      TRI_FreeString(TRI_UNKNOWN_MEM_ZONE, input);
      continue;
    }

    console.addHistory(input);

    struct mrb_parser_state* p = mrb_parse_string(context->_mrb, input, NULL);
    TRI_FreeString(TRI_UNKNOWN_MEM_ZONE, input);

    if (p == 0 || p->tree == 0 || 0 < p->nerr) {
      LOG_ERROR("failed to compile input");
      continue;
    }

    int n = mrb_generate_code(context->_mrb, p);

    if (n < 0) {
      LOG_ERROR("failed to execute Ruby bytecode");
      continue;
    }

    mrb_value result = mrb_run(context->_mrb,
                               mrb_proc_new(context->_mrb, context->_mrb->irep[n]),
                               mrb_top_self(context->_mrb));

    if (context->_mrb->exc != 0) {
      LOG_ERROR("caught Ruby exception");
      mrb_p(context->_mrb, mrb_obj_value(context->_mrb->exc));
      context->_mrb->exc = 0;
    }
    else if (! mrb_nil_p(result)) {
      mrb_p(context->_mrb, result);
    }
  }

  // close the console
  console.close();

  // close the database
  closeDatabases();
  Random::shutdown();

  return EXIT_SUCCESS;
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief opens the database
////////////////////////////////////////////////////////////////////////////////

void ArangoServer::openDatabases () {
  TRI_vocbase_defaults_t defaults;  

  // override with command-line options 
  defaults.defaultMaximalSize            = _defaultMaximalSize;
  defaults.removeOnDrop                  = _removeOnDrop;
  defaults.removeOnCompacted             = _removeOnCompacted;
  defaults.defaultWaitForSync            = _defaultWaitForSync;
  defaults.forceSyncShapes               = _forceSyncShapes;
  defaults.forceSyncProperties           = _forceSyncProperties;
  defaults.requireAuthentication         = ! _disableAuthentication;
  defaults.authenticateSystemOnly        = _authenticateSystemOnly;
  
  assert(_server != 0); 
  
  int res = TRI_InitServer(_server, 
                           _applicationEndpointServer,
                           _databasePath.c_str(),
                           _applicationV8->appPath().c_str(),
                           _applicationV8->devAppPath().c_str(), 
                           &defaults, 
                           _disableReplicationLogger, 
                           _disableReplicationApplier);
  
  if (res != TRI_ERROR_NO_ERROR) {
    LOG_FATAL_AND_EXIT("cannot create server instance: out of memory");
  }

  const bool isUpgrade = _applicationServer->programOptions().has("upgrade");
  res = TRI_StartServer(_server, isUpgrade);

  if (res != TRI_ERROR_NO_ERROR) {
    LOG_FATAL_AND_EXIT("cannot start server: %s", TRI_errno_string(res));
  }
  
  LOG_TRACE("found system database");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief closes all databases
////////////////////////////////////////////////////////////////////////////////

void ArangoServer::closeDatabases () {
  assert(_server != 0);

  TRI_CleanupActions();

  TRI_StopServer(_server);


  LOG_INFO("ArangoDB has been shut down");
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
