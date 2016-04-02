  _applicationV8 = new ApplicationV8(
      _server, _queryRegistry);

  _applicationServer->addFeature(_applicationV8);

  _applicationV8->setVocbase(vocbase);
  _applicationV8->setConcurrency(_v8Contexts);
  _applicationV8->defineDouble("DISPATCHER_THREADS", _dispatcherThreads);
  _applicationV8->defineDouble("V8_CONTEXTS", _v8Contexts);

  if (!startServer) {
    _applicationV8->disableActions();
  }

  _applicationV8->upgradeDatabase(skipUpgrade, performUpgrade);

  // setup the V8 actions
  if (startServer) {
    _applicationV8->prepareServer();
  }

