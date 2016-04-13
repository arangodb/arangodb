////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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

#include "RestServerFeature.h"

#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/ServerFeature.h"

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::options;

RestServerFeature::RestServerFeature(application_features::ApplicationServer* server)
    : ApplicationFeature(server, "RestServer") {
  startsAfter("Server");
}

void RestServerFeature::start() {
  LOG_TOPIC(TRACE, Logger::STARTUP) << name() << "::start";

}

void RestServerFeature::stop() {
  LOG_TOPIC(TRACE, Logger::STARTUP) << name() << "::stop";

}


#if 0
      _jobManager(nullptr),

  rest::AsyncJobManager* jobManager() const { return _jobManager; }

      _handlerFactory(nullptr),
  rest::AsyncJobManager* _jobManager;


  defineHandlers();

  // disabled maintenance mode
  HttpHandlerFactory::setMaintenance(false);


  void buildServers();

  std::vector<rest::HttpServer*> _servers;



prepare:
  buildHandlerFactory();
  HttpHandlerFactory::setMaintenance(true);


start:
  buildServers();

  for (auto& server : _servers) {
    server->startListening();
  }

#warning TODO
#if 0
  _jobManager = new AsyncJobManager(ClusterCommRestCallback);
#endif



stop:

  for (auto& server : _servers) {
    server->stopListening();
  }

  for (auto& server : _servers) {
    server->stop();
  }

  for (auto& server : _servers) {
    delete server;
  }







void EndpointFeature::buildServers() {
  ServerFeature* server = dynamic_cast<ServerFeature*>(
      application_features::ApplicationServer::lookupFeature("Server"));

  TRI_ASSERT(server != nullptr);

  HttpHandlerFactory* handlerFactory = server->httpHandlerFactory();
  AsyncJobManager* jobManager = server->jobManager();

#warning TODO filter list
  HttpServer* httpServer;

  // unencrypted HTTP endpoints
  httpServer =
      new HttpServer(SchedulerFeature::SCHEDULER, DispatcherFeature::DISPATCHER,
                     handlerFactory, jobManager, _keepAliveTimeout);

  httpServer->setEndpointList(&_endpointList);
  _servers.push_back(httpServer);

  // ssl endpoints
  if (_endpointList.hasSsl()) {
    SslFeature* ssl = dynamic_cast<SslFeature*>(
        application_features::ApplicationServer::lookupFeature("Ssl"));

    // check the ssl context
    if (ssl == nullptr || ssl->sslContext() == nullptr) {
      LOG(FATAL) << "no ssl context is known, cannot create https server, "
                    "please use the '--ssl.keyfile' option";
      FATAL_ERROR_EXIT();
    }

    SSL_CTX* sslContext = ssl->sslContext();

    // https
    httpServer = new HttpsServer(SchedulerFeature::SCHEDULER,
                                 DispatcherFeature::DISPATCHER, handlerFactory,
                                 jobManager, _keepAliveTimeout, sslContext);

    httpServer->setEndpointList(&_endpointList);
    _servers.push_back(httpServer);
  }
}





  std::unique_ptr<rest::HttpHandlerFactory> _handlerFactory;

  void buildHandlerFactory();
  void defineHandlers();

  rest::HttpHandlerFactory* httpHandlerFactory() const {
    return _handlerFactory.get();
  }

static TRI_vocbase_t* LookupDatabaseFromRequest(HttpRequest* request,
                                                TRI_server_t* server) {
  // get database name from request
  std::string dbName = request->databaseName();

  if (dbName.empty()) {
    // if no databases was specified in the request, use system database name
    // as a fallback
    dbName = TRI_VOC_SYSTEM_DATABASE;
    request->setDatabaseName(dbName);
  }

  if (ServerState::instance()->isCoordinator()) {
    return TRI_UseCoordinatorDatabaseServer(server, dbName.c_str());
  }

  return TRI_UseDatabaseServer(server, dbName.c_str());
}

static bool SetRequestContext(HttpRequest* request, void* data) {
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
  request->setRequestContext(ctx, true);

  // the "true" means the request is the owner of the context
  return true;
}

void ServerFeature::buildHandlerFactory() {
  _handlerFactory.reset(new HttpHandlerFactory(
      _authenticationRealm, _defaultApiCompatibility, _allowMethodOverride,
      &SetRequestContext, nullptr));
}

void ServerFeature::defineHandlers() {
#warning TODO
#if 0
  // ...........................................................................
  // /_msg
  // ...........................................................................

  _handlerFactory->addPrefixHandler(
      "/_msg/please-upgrade",
      RestHandlerCreator<RestPleaseUpgradeHandler>::createNoData);

  // ...........................................................................
  // /_api
  // ...........................................................................

  _handlerFactory->addPrefixHandler(
      RestVocbaseBaseHandler::BATCH_PATH,
      RestHandlerCreator<RestBatchHandler>::createNoData);

  _handlerFactory->addPrefixHandler(
      RestVocbaseBaseHandler::CURSOR_PATH,
      RestHandlerCreator<RestCursorHandler>::createData<
          std::pair<ApplicationV8*, aql::QueryRegistry*>*>,
      _pairForAqlHandler);

  _handlerFactory->addPrefixHandler(
      RestVocbaseBaseHandler::DOCUMENT_PATH,
      RestHandlerCreator<RestDocumentHandler>::createNoData);

  _handlerFactory->addPrefixHandler(
      RestVocbaseBaseHandler::EDGE_PATH,
      RestHandlerCreator<RestEdgeHandler>::createNoData);

  _handlerFactory->addPrefixHandler(
      RestVocbaseBaseHandler::EDGES_PATH,
      RestHandlerCreator<RestEdgesHandler>::createNoData);

#warning TODO
#if 0
  _handlerFactory->addPrefixHandler(
      RestVocbaseBaseHandler::EXPORT_PATH,
      RestHandlerCreator<RestExportHandler>::createNoData);
#endif

  _handlerFactory->addPrefixHandler(
      RestVocbaseBaseHandler::IMPORT_PATH,
      RestHandlerCreator<RestImportHandler>::createNoData);

#warning TODO
#if 0
  _handlerFactory->addPrefixHandler(
      RestVocbaseBaseHandler::REPLICATION_PATH,
      RestHandlerCreator<RestReplicationHandler>::createNoData);
#endif

  _handlerFactory->addPrefixHandler(
      RestVocbaseBaseHandler::SIMPLE_QUERY_ALL_PATH,
      RestHandlerCreator<RestSimpleQueryHandler>::createData<
          std::pair<ApplicationV8*, aql::QueryRegistry*>*>,
      _pairForAqlHandler);

  _handlerFactory->addPrefixHandler(
      RestVocbaseBaseHandler::SIMPLE_LOOKUP_PATH,
      RestHandlerCreator<RestSimpleHandler>::createData<
          std::pair<ApplicationV8*, aql::QueryRegistry*>*>,
      _pairForAqlHandler);

  _handlerFactory->addPrefixHandler(
      RestVocbaseBaseHandler::SIMPLE_REMOVE_PATH,
      RestHandlerCreator<RestSimpleHandler>::createData<
          std::pair<ApplicationV8*, aql::QueryRegistry*>*>,
      _pairForAqlHandler);

  _handlerFactory->addPrefixHandler(
      RestVocbaseBaseHandler::UPLOAD_PATH,
      RestHandlerCreator<RestUploadHandler>::createNoData);

#warning TODO
#if 0
  _handlerFactory->addPrefixHandler(
      "/_api/shard-comm",
      RestHandlerCreator<RestShardHandler>::createNoData);
#endif

  _handlerFactory->addPrefixHandler(
      "/_api/aql", RestHandlerCreator<aql::RestAqlHandler>::createData<
                       std::pair<ApplicationV8*, aql::QueryRegistry*>*>,
      _pairForAqlHandler);

  _handlerFactory->addPrefixHandler(
      "/_api/query",
      RestHandlerCreator<RestQueryHandler>::createData<ApplicationV8*>,
      _applicationV8);

  _handlerFactory->addPrefixHandler(
      "/_api/query-cache",
      RestHandlerCreator<RestQueryCacheHandler>::createNoData);

  // And now some handlers which are registered in both /_api and /_admin
  _handlerFactory->addPrefixHandler(
      "/_api/job", RestHandlerCreator<arangodb::RestJobHandler>::createData<
                       std::pair<Dispatcher*, AsyncJobManager*>*>,
      _pairForJobHandler);

  _handlerFactory->addHandler(
      "/_api/version", RestHandlerCreator<RestVersionHandler>::createNoData,
      nullptr);

  // ...........................................................................
  // /_admin
  // ...........................................................................

  _handlerFactory->addPrefixHandler(
      "/_admin/job", RestHandlerCreator<arangodb::RestJobHandler>::createData<
                         std::pair<Dispatcher*, AsyncJobManager*>*>,
      _pairForJobHandler);

  _handlerFactory->addHandler(
      "/_admin/version", RestHandlerCreator<RestVersionHandler>::createNoData,
      nullptr);

  // further admin handlers
  _handlerFactory->addHandler(
      "/_admin/log",
      RestHandlerCreator<arangodb::RestAdminLogHandler>::createNoData, nullptr);

  _handlerFactory->addPrefixHandler(
      "/_admin/work-monitor",
      RestHandlerCreator<WorkMonitorHandler>::createNoData, nullptr);

// This handler is to activate SYS_DEBUG_FAILAT on DB servers
#ifdef ARANGODB_ENABLE_FAILURE_TESTS
  _handlerFactory->addPrefixHandler(
      "/_admin/debug", RestHandlerCreator<RestDebugHandler>::createNoData,
      nullptr);
#endif

#warning TODO
#if 0
  _handlerFactory->addPrefixHandler(
      "/_admin/shutdown",
      RestHandlerCreator<arangodb::RestShutdownHandler>::createData<void*>,
      static_cast<void*>(_applicationServer));
#endif

  // ...........................................................................
  // /_admin
  // ...........................................................................

  _handlerFactory->addPrefixHandler(
      "/", RestHandlerCreator<RestActionHandler>::createData<
               RestActionHandler::action_options_t*>,
      (void*)&httpOptions);
#endif
}

#endif
