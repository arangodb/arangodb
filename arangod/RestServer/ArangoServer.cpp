bool IGNORE_DATAFILE_ERRORS;

ArangoServer::ArangoServer(int argc, char** argv)
    : _mode(ServerMode::MODE_STANDALONE),
#warning TODO
      // _applicationServer(nullptr),
      _argc(argc),
      _argv(argv),
      _tempPath(),
      _applicationEndpointServer(nullptr),
      _applicationCluster(nullptr),
      _applicationAgency(nullptr),
      _jobManager(nullptr),
      _applicationV8(nullptr),
      _authenticateSystemOnly(false),
      _disableAuthentication(false),
      _disableAuthenticationUnixSockets(false),
      _v8Contexts(8),
      _indexThreads(2),
      _databasePath(),
      _disableReplicationApplier(false),
      _disableQueryTracking(false),
      _foxxQueues(true),
      _foxxQueuesPollInterval(1.0),
      _server(nullptr),
      _queryRegistry(nullptr),
      _pairForAqlHandler(nullptr),
      _pairForJobHandler(nullptr),
      _indexPool(nullptr),
      _threadAffinity(0) {
#ifndef TRI_HAVE_THREAD_AFFINITY
  _threadAffinity = 0;
#endif
}

ArangoServer::~ArangoServer() {
  delete _jobManager;
  delete _server;
}


void ArangoServer::buildApplicationServer() {
#warning TODO
#if 0

  _applicationCluster =
      new ApplicationCluster(_server);
  _applicationServer->addFeature(_applicationCluster);

  // .............................................................................
  // agency options
  // .............................................................................

  _applicationAgency = new ApplicationAgency();
  _applicationServer->addFeature(_applicationAgency);

#endif
}

int ArangoServer::startupServer() {
#warning TODO
#if 0

  // .............................................................................
  // prepare everything
  // .............................................................................

  if (!startServer) {
    _applicationEndpointServer->disable();
  }

  // prepare scheduler and dispatcher
  _applicationServer->prepare();

  auto const role = ServerState::instance()->getRole();

  // and finish prepare
  _applicationServer->prepare2();

  // ...........................................................................
  // create endpoints and handlers
  // ...........................................................................

  // we pass the options by reference, so keep them until shutdown

  // active deadlock detection in case we're not running in cluster mode
  if (!arangodb::ServerState::instance()->isRunningInCluster()) {
    TRI_EnableDeadlockDetectionDatabasesServer(_server);
  }

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
      LOG(FATAL) << "could not load required authentication information";
      FATAL_ERROR_EXIT();
    }
  }

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


  _applicationServer->stop();


  return res;
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
#warning TODO
#if 0
  waitForHeartbeat();

  // Loading ageny's persistent state
  if(_applicationAgency->agent()!=nullptr)
    _applicationAgency->agent()->load();

  // just wait until we are signalled
  _applicationServer->wait();

  return EXIT_SUCCESS;
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the JavaScript emergency console
////////////////////////////////////////////////////////////////////////////////

int ArangoServer::runConsole(TRI_vocbase_t* vocbase) {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief runs unit tests
////////////////////////////////////////////////////////////////////////////////

int ArangoServer::runUnitTests(TRI_vocbase_t* vocbase) {
#warning TODO
#if 0
  ApplicationV8::V8Context* context =
      _applicationV8->enterContext(vocbase, true);

  auto isolate = context->isolate;

  bool ok = false;
  {
    v8::HandleScope scope(isolate);
    v8::TryCatch tryCatch;

    auto localContext = v8::Local<v8::Context>::New(isolate, context->_context);
    localContext->Enter();
    {
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

      v8::Local<v8::String> name(
          TRI_V8_ASCII_STRING(TRI_V8_SHELL_COMMAND_NAME));

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
    }
    localContext->Exit();
  }

  _applicationV8->exitContext(context);

  return ok ? EXIT_SUCCESS : EXIT_FAILURE;
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief runs a script
////////////////////////////////////////////////////////////////////////////////

int ArangoServer::runScript(TRI_vocbase_t* vocbase) {
#warning TODO
#if 0
  bool ok = false;
  ApplicationV8::V8Context* context =
      _applicationV8->enterContext(vocbase, true);
  auto isolate = context->isolate;

  {
    v8::HandleScope globalScope(isolate);

    auto localContext = v8::Local<v8::Context>::New(isolate, context->_context);
    localContext->Enter();
    {
      v8::Context::Scope contextScope(localContext);
      for (size_t i = 0; i < _scriptFile.size(); ++i) {
        bool r = TRI_ExecuteGlobalJavaScriptFile(isolate,
                                                 _scriptFile[i].c_str(), true);

        if (!r) {
          LOG(FATAL) << "cannot load script '" << _scriptFile[i]
                     << "', giving up";
          FATAL_ERROR_EXIT();
        }
      }

      v8::TryCatch tryCatch;
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
        LOG(FATAL) << "no main function defined, giving up";
        FATAL_ERROR_EXIT();
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
          LOG(ERR) << "caught exception " << TRI_errno_string(ex.code()) << ": "
                   << ex.what();
          ok = false;
        } catch (std::bad_alloc const&) {
          LOG(ERR) << "caught exception "
                   << TRI_errno_string(TRI_ERROR_OUT_OF_MEMORY);
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
#endif
}
