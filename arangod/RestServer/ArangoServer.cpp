ArangoServer::ArangoServer(int argc, char** argv)
    : _mode(ServerMode::MODE_STANDALONE),
#warning TODO
      // _applicationServer(nullptr),
      _argc(argc),
      _argv(argv),
      _tempPath(),
      _applicationEndpointServer(nullptr),
      _applicationCluster(nullptr),
      _agencyCallbackRegistry(nullptr),
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
  
  // Loading ageny's persistent state
  if(_applicationAgency->agent() != nullptr) {
    _applicationAgency->agent()->load();
  }

  int res;

#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief wait for the heartbeat thread to run
/// before the server responds to requests, the heartbeat thread should have
/// run at least once
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief runs the server
////////////////////////////////////////////////////////////////////////////////

int ArangoServer::runServer(TRI_vocbase_t* vocbase) {
#warning TODO
#if 0
  waitForHeartbeat();

  // just wait until we are signalled
  _applicationServer->wait();

  return EXIT_SUCCESS;
#endif
}

