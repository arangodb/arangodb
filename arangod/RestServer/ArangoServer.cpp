////////////////////////////////////////////////////////////////////////////////
/// @brief arango server
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2012 triAGENS GmbH, Cologne, Germany
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
/// @author Copyright 2011-2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "ArangoServer.h"

#include <v8.h>

#ifdef TRI_ENABLE_MRUBY
#include "mruby.h"
#include "mruby/compile.h"
#include "mruby/data.h"
#include "mruby/proc.h"
#include "mruby/variable.h"
#endif

#include "build.h"

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
#include "BasicsC/messages.h"
#include "BasicsC/strings.h"
#include "Dispatcher/ApplicationDispatcher.h"
#include "Dispatcher/Dispatcher.h"
#include "HttpServer/ApplicationEndpointServer.h"
#include "HttpServer/HttpHandlerFactory.h"
#include "HttpServer/RedirectHandler.h"

#include "Logger/Logger.h"
#include "Rest/InitialiseRest.h"
#include "Rest/OperationMode.h"
#include "RestHandler/RestBatchHandler.h"
#include "RestHandler/RestDocumentHandler.h"
#include "RestHandler/RestEdgeHandler.h"
#include "RestHandler/RestImportHandler.h"
#include "Scheduler/ApplicationScheduler.h"
#include "Statistics/statistics.h"

#include "V8/V8LineEditor.h"
#include "V8/v8-conv.h"
#include "V8/v8-utils.h"
#include "V8Server/ApplicationV8.h"
#include "VocBase/auth.h"

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
                               TRI_vocbase_t* vocbase) {

  // add "/version" handler
  admin->addBasicHandlers(factory, "/_api");

  // add "/document" handler
  factory->addPrefixHandler(RestVocbaseBaseHandler::DOCUMENT_PATH,
                            RestHandlerCreator<RestDocumentHandler>::createData<TRI_vocbase_t*>,
                            vocbase);

  // add "/edge" handler
  factory->addPrefixHandler(RestVocbaseBaseHandler::EDGE_PATH,
                            RestHandlerCreator<RestEdgeHandler>::createData<TRI_vocbase_t*>,
                            vocbase);

  // add import handler
  factory->addPrefixHandler(RestVocbaseBaseHandler::DOCUMENT_IMPORT_PATH,
                            RestHandlerCreator<RestImportHandler>::createData<TRI_vocbase_t*>,
                            vocbase);
  
  // add batch handler
  factory->addPrefixHandler(RestVocbaseBaseHandler::BATCH_PATH,
                            RestHandlerCreator<RestBatchHandler>::createData<TRI_vocbase_t*>,
                            vocbase);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief define "admin" handlers
////////////////////////////////////////////////////////////////////////////////

static void DefineAdminHandlers (HttpHandlerFactory* factory,
                                 ApplicationAdminServer* admin,
                                 TRI_vocbase_t* vocbase) {

  // add "/version" handler
  admin->addBasicHandlers(factory, "/_admin");

  // add admin handlers
  admin->addHandlers(factory, "/_admin");
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
    _binaryPath(),
    _applicationScheduler(0),
    _applicationDispatcher(0),
    _applicationEndpointServer(0),
    _applicationAdminServer(0),
    _dispatcherThreads(8),
    _databasePath(),
    _removeOnDrop(true),
    _removeOnCompacted(true),
    _defaultMaximalSize(TRI_JOURNAL_DEFAULT_MAXIMAL_SIZE),
    _defaultWaitForSync(false),
    _forceSyncShapes(true),
    _vocbase(0) {

  // locate path to binary
  char* p;

  p = TRI_LocateBinaryPath(argv[0]);
  _binaryPath = p;

  TRI_FreeString(TRI_CORE_MEM_ZONE, p);

  // set working directory and database directory
  _workingDirectory = "/var/tmp";

  _defaultLanguage = Utf8Helper::DefaultUtf8Helper.getCollatorLanguage();

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

  _applicationServer = new ApplicationServer("arangod", "[<options>] <database-directory>", TRIAGENS_VERSION);
  _applicationServer->setSystemConfigFile("arangod.conf");

  // arangod allows defining a user-specific configuration file. arangosh and the other binaries don't
  _applicationServer->setUserConfigFile(string(".arango") + string(1, TRI_DIR_SEPARATOR_CHAR) + string("arangod.conf") );

  
  // .............................................................................
  // multi-threading scheduler 
  // .............................................................................

  _applicationScheduler = new ApplicationScheduler(_applicationServer);
  _applicationScheduler->allowMultiScheduler(true);
  
  _applicationServer->addFeature(_applicationScheduler);

  // .............................................................................
  // dispatcher
  // .............................................................................

  _applicationDispatcher = new ApplicationDispatcher(_applicationScheduler);
  _applicationServer->addFeature(_applicationDispatcher);

  // .............................................................................
  // V8 engine
  // .............................................................................

  _applicationV8 = new ApplicationV8(_binaryPath);
  _applicationServer->addFeature(_applicationV8);

  // .............................................................................
  // MRuby engine
  // .............................................................................

#ifdef TRI_ENABLE_MRUBY

  _applicationMR = new ApplicationMR(_binaryPath);
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
  _applicationServer->addFeature(_applicationAdminServer);

  _applicationAdminServer->allowLogViewer();
  _applicationAdminServer->allowVersion("arango", TRIAGENS_VERSION);
  _applicationAdminServer->allowAdminDirectory(); // might be changed later

  // .............................................................................
  // define server options
  // .............................................................................

  // .............................................................................
  // daemon and supervisor mode
  // .............................................................................


  additional[ApplicationServer::OPTIONS_CMDLINE]
    ("console", "do not start as server, start a JavaScript emergency console instead")
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
  
  // .............................................................................
  // javascript options
  // .............................................................................

  additional["JAVASCRIPT Options:help-admin"]
    ("javascript.script", &_scriptFile, "do not start as server, run script instead")
    ("javascript.script-parameter", &_scriptParameters, "script parameter")
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
  ;

  additional["DATABASE Options:help-devel"]
    ("database.remove-on-compacted", &_removeOnCompacted, "wipe a datafile from disk after compaction")
  ;
   
  additional["JAVASCRIPT Options:help-devel"]
    ("jslint", &_jslint, "do not start as server, run js lint instead")
    ("javascript.unit-tests", &_unitTests, "do not start as server, run unit tests instead")
  ;

  // .............................................................................
  // server options
  // .............................................................................
  
  // .............................................................................
  // for this server we display our own options such as port to use
  // .............................................................................

  bool disableAdminInterface = false;

  additional[ApplicationServer::OPTIONS_SERVER + ":help-admin"]
    ("server.disable-admin-interface", &disableAdminInterface, "turn off the HTML admin interface")
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
  
  // .............................................................................
  // endpoint server
  // .............................................................................


  _applicationEndpointServer = new ApplicationEndpointServer(_applicationServer,
                                                             _applicationScheduler,
                                                             _applicationDispatcher,
                                                             "arangodb",
                                                             TRI_CheckAuthenticationAuthInfo);
  _applicationServer->addFeature(_applicationEndpointServer);

  // .............................................................................
  // parse the command line options - exit if there is a parse error
  // .............................................................................

  if (! _applicationServer->parse(_argc, _argv, additional)) {
    CLEANUP_LOGGING_AND_EXIT_ON_FATAL_ERROR();
  }
  
  // .............................................................................
  // set language of default collator
  // .............................................................................

  UVersionInfo icuVersion;
  char icuVersionString[U_MAX_VERSION_STRING_LENGTH];
  u_getVersion(icuVersion);
  u_versionToString(icuVersion, icuVersionString);  
  LOGGER_INFO("using ICU " << icuVersionString);        
  
  Utf8Helper::DefaultUtf8Helper.setCollatorLanguage(_defaultLanguage);
  if (Utf8Helper::DefaultUtf8Helper.getCollatorCountry() != "") {
    LOGGER_INFO("using default language '" << Utf8Helper::DefaultUtf8Helper.getCollatorLanguage() << "_" << Utf8Helper::DefaultUtf8Helper.getCollatorCountry() << "'");    
  }
  else {
    LOGGER_INFO("using default language '" << Utf8Helper::DefaultUtf8Helper.getCollatorLanguage() << "'" );        
  }

  // .............................................................................
  // init nonces
  // .............................................................................
  
  uint32_t optionNonceHashSize = 0; // TODO: add an server option
  if (optionNonceHashSize > 0) {
    LOGGER_INFO("setting nonce hash size to '" << optionNonceHashSize << "'" );        
    Nonce::create(optionNonceHashSize);
  }
  
  // .............................................................................
  // disable access to the HTML admin interface
  // .............................................................................

  if (disableAdminInterface) {
    _applicationAdminServer->allowAdminDirectory(false);
  }

  if (disableStatistics) {
    TRI_ENABLE_STATISTICS =  false;
  }

  if (_defaultMaximalSize < TRI_JOURNAL_MINIMAL_SIZE) {
    // validate journal size
    LOGGER_FATAL_AND_EXIT("invalid journal size. expected at least " << TRI_JOURNAL_MINIMAL_SIZE);
  }

  // .............................................................................
  // set directories and scripts
  // .............................................................................

  vector<string> arguments = _applicationServer->programArguments();

  if (1 < arguments.size()) {
    LOGGER_FATAL_AND_EXIT("expected at most one database directory, got " << arguments.size());
  }
  else if (1 == arguments.size()) {
    _databasePath = arguments[0];
  }

  if (_databasePath.empty()) {
    LOGGER_INFO("please use the '--database.directory' option");
    LOGGER_FATAL_AND_EXIT("no database path has been supplied, giving up");
  }

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
      LOGGER_INFO("please use the '--pid-file' option");
      LOGGER_FATAL_AND_EXIT("no pid-file defined, but daemon or supervisor mode was requested");
    }

    // make the pid filename absolute
    int err = 0;
    string currentDir = FileUtils::currentDirectory(&err);
    char* absoluteFile = TRI_GetAbsolutePath(_pidFile.c_str(), currentDir.c_str());

    if (absoluteFile != 0) {
      _pidFile = string(absoluteFile);
      TRI_Free(TRI_UNKNOWN_MEM_ZONE, absoluteFile);
 
      LOGGER_DEBUG("using absolute pid file '" << _pidFile << "'");
    }
    else {
      LOGGER_FATAL_AND_EXIT("cannot determine current directory");
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

int ArangoServer::startupServer () {
  v8::HandleScope handle_scope;

  // .............................................................................
  // open the database
  // .............................................................................

  openDatabase();

  
  // .............................................................................
  // prepare the various parts of the Arango server
  // .............................................................................

  if (_dispatcherThreads < 1) {
    _dispatcherThreads = 1;
  }

  _applicationV8->setVocbase(_vocbase);
  _applicationV8->setConcurrency(_dispatcherThreads);

  if (_applicationServer->programOptions().has("upgrade")) {
    _applicationV8->performUpgrade();
  }

  // skip an upgrade even if VERSION is missing
  if (_applicationServer->programOptions().has("no-upgrade")) {
    _applicationV8->skipUpgrade();
  }

#if TRI_ENABLE_MRUBY
  _applicationMR->setVocbase(_vocbase);
  _applicationMR->setConcurrency(_dispatcherThreads);
#endif

  _applicationServer->prepare();

  
  // .............................................................................
  // create the dispatcher
  // .............................................................................

  _applicationDispatcher->buildStandardQueue(_dispatcherThreads);


  _applicationServer->prepare2();
  
    
  // we pass the options by reference, so keep them until shutdown
  RestActionHandler::action_options_t httpOptions;
  httpOptions._vocbase = _vocbase;
  httpOptions._queue = "STANDARD";

  // create the handlers
  httpOptions._contexts.insert("user");
  httpOptions._contexts.insert("api");
  httpOptions._contexts.insert("admin");


  // create the server
  _applicationEndpointServer->buildServers();
    
  HttpHandlerFactory* handlerFactory = _applicationEndpointServer->getHandlerFactory();

  DefineApiHandlers(handlerFactory, _applicationAdminServer, _vocbase);
  DefineAdminHandlers(handlerFactory, _applicationAdminServer, _vocbase);

  // add action handler
  handlerFactory->addPrefixHandler("/",
                                   RestHandlerCreator<RestActionHandler>::createData<RestActionHandler::action_options_t*>,
                                   (void*) &httpOptions);
  

  // .............................................................................
  // start the main event loop
  // .............................................................................

  _applicationServer->start();

  LOGGER_INFO("ArangoDB (version " << TRIAGENS_VERSION << ") is ready for business");
  LOGGER_INFO("Have Fun!");

  _applicationServer->wait();
  
  // .............................................................................
  // and cleanup
  // .............................................................................

  _applicationServer->stop();

  closeDatabase();

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
  bool ok;

  // open the database
  openDatabase();

  // set-up V8 context
  _applicationV8->setVocbase(_vocbase);
  _applicationV8->setConcurrency(1);

  if (_applicationServer->programOptions().has("upgrade")) {
    _applicationV8->performUpgrade();
  }

  // skip an upgrade even if VERSION is missing
  if (_applicationServer->programOptions().has("no-upgrade")) {
    _applicationV8->skipUpgrade();
  }

  _applicationV8->disableActions();

  ok = _applicationV8->prepare();

  if (! ok) {
    LOGGER_FATAL_AND_EXIT("cannot initialize V8 enigne");
  }

  _applicationV8->start();

  // enter V8 context
  ApplicationV8::V8Context* context = _applicationV8->enterContext();

  // .............................................................................
  // execute everything with a global scope
  // .............................................................................

  {
    v8::HandleScope globalScope;

    // run the shell
    if (mode != OperationMode::MODE_SCRIPT) {
      printf("ArangoDB JavaScript emergency console [V8 version %s, DB version %s]\n", v8::V8::GetVersion(), TRIAGENS_VERSION);
    }
    else {
      LOGGER_INFO("V8 version " << v8::V8::GetVersion() << ", DB version " << TRIAGENS_VERSION);
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
            LOGGER_FATAL_AND_EXIT("cannot load script '" << _scriptFile[i] << ", giving up");
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
            LOGGER_FATAL_AND_EXIT("no main function defined, giving up");
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
        V8LineEditor console(context->_context, ".arangod");

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

  
  closeDatabase();
  Random::shutdown();

  if (!ok) {
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
  bool ok;

  // open the database
  openDatabase();

  // set-up MRuby context
  _applicationMR->setVocbase(_vocbase);
  _applicationMR->setConcurrency(1);
  _applicationMR->disableActions();

  ok = _applicationMR->prepare();

  if (! ok) {
    LOGGER_FATAL_AND_EXIT("cannot initialize MRuby enigne");
  }

  _applicationMR->start();

  // enter MR context
  ApplicationMR::MRContext* context = _applicationMR->enterContext();

  // create a line editor
  printf("ArangoDB MRuby emergency console [DB version %s]\n", TRIAGENS_VERSION);

  MRLineEditor console(context->_mrb, ".arangod");

  console.open(false);

  while (true) {
    char* input = console.prompt("arangod> ");

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
      LOGGER_ERROR("failed to compile input");
      continue;
    }

    int n = mrb_generate_code(context->_mrb, p);

    if (n < 0) {
      LOGGER_ERROR("failed to execute Ruby bytecode");
      continue;
    }

    mrb_value result = mrb_run(context->_mrb,
                               mrb_proc_new(context->_mrb, context->_mrb->irep[n]),
                               mrb_top_self(context->_mrb));

    if (context->_mrb->exc != 0) {
      LOGGER_ERROR("caught Ruby exception");
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
  closeDatabase();

  Random::shutdown();

  return EXIT_SUCCESS;
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief opens the database
////////////////////////////////////////////////////////////////////////////////

void ArangoServer::openDatabase () {
  TRI_InitialiseVocBase();

  _vocbase = TRI_OpenVocBase(_databasePath.c_str());

  if (! _vocbase) {
    LOGGER_INFO("please use the '--database.directory' option");
    LOGGER_FATAL_AND_EXIT("cannot open database '" << _databasePath << "'");
  }

  _vocbase->_removeOnDrop = _removeOnDrop;
  _vocbase->_removeOnCompacted = _removeOnCompacted;
  _vocbase->_defaultMaximalSize = _defaultMaximalSize;
  _vocbase->_defaultWaitForSync = _defaultWaitForSync;
  _vocbase->_forceSyncShapes = _forceSyncShapes;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief closes the database
////////////////////////////////////////////////////////////////////////////////

void ArangoServer::closeDatabase () {
  TRI_CleanupActions();
  TRI_DestroyVocBase(_vocbase);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, _vocbase);
  _vocbase = 0;
  TRI_ShutdownVocBase();
  
  Nonce::destroy();
  
  LOGGER_INFO("ArangoDB has been shut down");
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}\\)"
// End:
