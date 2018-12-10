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

#include "GeneralServerFeature.h"

#include <stdexcept>

#include "Actions/RestActionHandler.h"
#include "Agency/AgencyFeature.h"
#include "Agency/RestAgencyHandler.h"
#include "Agency/RestAgencyPrivHandler.h"
#include "Aql/RestAqlHandler.h"
#include "Basics/StringUtils.h"
#include "Cluster/AgencyCallbackRegistry.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/MaintenanceRestHandler.h"
#include "Cluster/RestAgencyCallbacksHandler.h"
#include "Cluster/RestClusterHandler.h"
#include "Cluster/TraverserEngineRegistry.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "GeneralServer/GeneralServer.h"
#include "GeneralServer/RestHandlerFactory.h"
#include "Graph/Graph.h"
#include "InternalRestHandler/InternalRestTraverserHandler.h"
#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "RestHandler/RestAdminDatabaseHandler.h"
#include "RestHandler/RestAdminLogHandler.h"
#include "RestHandler/RestAdminRoutingHandler.h"
#include "RestHandler/RestAdminServerHandler.h"
#include "RestHandler/RestAdminStatisticsHandler.h"
#include "RestHandler/RestAqlFunctionsHandler.h"
#include "RestHandler/RestAqlUserFunctionsHandler.h"
#include "RestHandler/RestAuthHandler.h"
#include "RestHandler/RestBatchHandler.h"
#include "RestHandler/RestCollectionHandler.h"
#include "RestHandler/RestControlPregelHandler.h"
#include "RestHandler/RestCursorHandler.h"
#include "RestHandler/RestDatabaseHandler.h"
#include "RestHandler/RestDebugHandler.h"
#include "RestHandler/RestDocumentHandler.h"
#include "RestHandler/RestEdgesHandler.h"
#include "RestHandler/RestEndpointHandler.h"
#include "RestHandler/RestEngineHandler.h"
#include "RestHandler/RestExplainHandler.h"
#include "RestHandler/RestGraphHandler.h"
#include "RestHandler/RestHandlerCreator.h"
#include "RestHandler/RestImportHandler.h"
#include "RestHandler/RestIndexHandler.h"
#include "RestHandler/RestJobHandler.h"
#include "RestHandler/RestPleaseUpgradeHandler.h"
#include "RestHandler/RestPregelHandler.h"
#include "RestHandler/RestQueryCacheHandler.h"
#include "RestHandler/RestQueryHandler.h"
#include "RestHandler/RestRepairHandler.h"
#include "RestHandler/RestShutdownHandler.h"
#include "RestHandler/RestSimpleHandler.h"
#include "RestHandler/RestSimpleQueryHandler.h"
#include "RestHandler/RestStatusHandler.h"
#include "RestHandler/RestTasksHandler.h"
#include "RestHandler/RestTestHandler.h"
#include "RestHandler/RestTransactionHandler.h"
#include "RestHandler/RestUploadHandler.h"
#include "RestHandler/RestUsersHandler.h"
#include "RestHandler/RestVersionHandler.h"
#include "RestHandler/RestViewHandler.h"
#include "RestHandler/RestWalAccessHandler.h"
#include "RestServer/EndpointFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/ServerFeature.h"
#include "RestServer/TraverserEngineRegistryFeature.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"
#include "Ssl/SslServerFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"

using namespace arangodb::rest;
using namespace arangodb::options;

namespace arangodb {

static uint64_t const _maxIoThreads = 64;

rest::RestHandlerFactory* GeneralServerFeature::HANDLER_FACTORY = nullptr;
rest::AsyncJobManager* GeneralServerFeature::JOB_MANAGER = nullptr;
GeneralServerFeature* GeneralServerFeature::GENERAL_SERVER = nullptr;

GeneralServerFeature::GeneralServerFeature(
    application_features::ApplicationServer& server
)
    : ApplicationFeature(server, "GeneralServer"),
      _allowMethodOverride(false),
      _proxyCheck(true),
      _numIoThreads(0) {
  setOptional(true);
  startsAfter("AQLPhase");

  startsAfter("Endpoint");
  startsAfter("Upgrade");
  startsAfter("SslServer");

  _numIoThreads = (std::max)(static_cast<uint64_t>(1),
    static_cast<uint64_t>(TRI_numberProcessors() / 4));
  if (_numIoThreads > _maxIoThreads) {
    _numIoThreads = _maxIoThreads;
  }
}

void GeneralServerFeature::collectOptions(
    std::shared_ptr<ProgramOptions> options) {
  options->addSection("server", "Server features");

  options->addOldOption("server.allow-method-override",
                        "http.allow-method-override");
  options->addOldOption("server.hide-product-header",
                        "http.hide-product-header");
  options->addOldOption("server.keep-alive-timeout", "http.keep-alive-timeout");
  options->addOldOption("server.default-api-compatibility", "");
  options->addOldOption("no-server", "server.rest-server");

  options->addOption(
      "--server.io-threads",
      "Number of threads used to handle IO",
      new UInt64Parameter(&_numIoThreads),
      arangodb::options::makeFlags(arangodb::options::Flags::Dynamic));

  options->addSection("http", "HttpServer features");

  options->addOption("--http.allow-method-override",
                     "allow HTTP method override using special headers",
                     new BooleanParameter(&_allowMethodOverride),
                     arangodb::options::makeFlags(arangodb::options::Flags::Hidden));

  options->addOption("--http.keep-alive-timeout",
                     "keep-alive timeout in seconds",
                     new DoubleParameter(&_keepAliveTimeout));

  options->addOption(
      "--http.hide-product-header",
      "do not expose \"Server: ArangoDB\" header in HTTP responses",
      new BooleanParameter(&HttpResponse::HIDE_PRODUCT_HEADER));

  options->addOption(
      "--http.trusted-origin",
      "trusted origin URLs for CORS requests with credentials",
      new VectorParameter<StringParameter>(&_accessControlAllowOrigins));

  options->addSection("frontend", "Frontend options");

  options->addOption("--frontend.proxy-request-check",
                     "enable proxy request checking",
                     new BooleanParameter(&_proxyCheck));

  options->addOption("--frontend.trusted-proxy",
                     "list of proxies to trust (may be IP or network). Make "
                     "sure --frontend.proxy-request-check is enabled",
                     new VectorParameter<StringParameter>(&_trustedProxies));
}

void GeneralServerFeature::validateOptions(std::shared_ptr<ProgramOptions>) {
  if (!_accessControlAllowOrigins.empty()) {
    // trim trailing slash from all members
    for (auto& it : _accessControlAllowOrigins) {
      if (it == "*" || it == "all") {
        // special members "*" or "all" means all origins are allowed
        _accessControlAllowOrigins.clear();
        _accessControlAllowOrigins.push_back("*");
        break;
      } else if (it == "none") {
        // "none" means no origins are allowed
        _accessControlAllowOrigins.clear();
        break;
      } else if (it[it.size() - 1] == '/') {
        // strip trailing slash
        it = it.substr(0, it.size() - 1);
      }
    }

    // remove empty members
    _accessControlAllowOrigins.erase(
        std::remove_if(_accessControlAllowOrigins.begin(),
                       _accessControlAllowOrigins.end(),
                       [](std::string const& value) {
                         return basics::StringUtils::trim(value).empty();
                       }),
        _accessControlAllowOrigins.end());
  }

  // we need at least one io thread and context
  if (_numIoThreads == 0) {
    LOG_TOPIC(WARN, Logger::FIXME)
      << "Need at least one io-context thread.";
    _numIoThreads = 1;
  } else if (_numIoThreads > _maxIoThreads) {
    LOG_TOPIC(WARN, Logger::FIXME)
      << "IO-contexts are limited to " << _maxIoThreads;
      _numIoThreads = _maxIoThreads;
  }
}

void GeneralServerFeature::prepare() {
  ServerState::instance()->setServerMode(ServerState::Mode::MAINTENANCE);
  GENERAL_SERVER = this;
}

void GeneralServerFeature::start() {
  _jobManager.reset(new AsyncJobManager);

  JOB_MANAGER = _jobManager.get();

  _handlerFactory.reset(new RestHandlerFactory());

  HANDLER_FACTORY = _handlerFactory.get();

  defineHandlers();
  buildServers();

  for (auto& server : _servers) {
    server->startListening();
  }
}

void GeneralServerFeature::stop() {
  for (auto& server : _servers) {
    server->stopListening();
  }

  _jobManager->deleteJobs();
}

void GeneralServerFeature::unprepare() {
  for (auto& server : _servers) {
    delete server;
  }

  _jobManager.reset();

  GENERAL_SERVER = nullptr;
  JOB_MANAGER = nullptr;
  HANDLER_FACTORY = nullptr;
}

void GeneralServerFeature::buildServers() {
  TRI_ASSERT(_jobManager != nullptr);

  EndpointFeature* endpoint =
      application_features::ApplicationServer::getFeature<EndpointFeature>(
          "Endpoint");
  auto const& endpointList = endpoint->endpointList();

  // check if endpointList contains ssl featured server
  if (endpointList.hasSsl()) {
    SslServerFeature* ssl =
        application_features::ApplicationServer::getFeature<SslServerFeature>(
            "SslServer");

    if (ssl == nullptr) {
      LOG_TOPIC(FATAL, arangodb::Logger::FIXME)
          << "no ssl context is known, cannot create https server, "
             "please enable SSL";
      FATAL_ERROR_EXIT();
    }

    ssl->SSL->verifySslOptions();
  }

  GeneralServer* server = new GeneralServer(_numIoThreads);

  server->setEndpointList(&endpointList);
  _servers.push_back(server);
}

void GeneralServerFeature::defineHandlers() {
  TRI_ASSERT(_jobManager != nullptr);

  AgencyFeature* agency =
      application_features::ApplicationServer::getFeature<AgencyFeature>(
          "Agency");
  TRI_ASSERT(agency != nullptr);

  ClusterFeature* cluster =
      application_features::ApplicationServer::getFeature<ClusterFeature>(
          "Cluster");
  TRI_ASSERT(cluster != nullptr);

  AuthenticationFeature* authentication =
      application_features::ApplicationServer::getFeature<
          AuthenticationFeature>("Authentication");
  TRI_ASSERT(authentication != nullptr);

  auto queryRegistry = QueryRegistryFeature::registry();
  auto traverserEngineRegistry =  TraverserEngineRegistryFeature::registry();
  if (_combinedRegistries == nullptr) {
    _combinedRegistries = std::make_unique<std::pair<aql::QueryRegistry*, traverser::TraverserEngineRegistry*>> (queryRegistry, traverserEngineRegistry);
  } else {
    TRI_ASSERT(false);
  }

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
      RestVocbaseBaseHandler::CONTROL_PREGEL_PATH,
      RestHandlerCreator<RestControlPregelHandler>::createNoData);

  _handlerFactory->addPrefixHandler(
      RestVocbaseBaseHandler::CURSOR_PATH,
      RestHandlerCreator<RestCursorHandler>::createData<aql::QueryRegistry*>,
      queryRegistry);

  _handlerFactory->addPrefixHandler(
      RestVocbaseBaseHandler::DATABASE_PATH,
      RestHandlerCreator<RestDatabaseHandler>::createNoData);

  _handlerFactory->addPrefixHandler(
      RestVocbaseBaseHandler::DOCUMENT_PATH,
      RestHandlerCreator<RestDocumentHandler>::createNoData);

  _handlerFactory->addPrefixHandler(
      RestVocbaseBaseHandler::EDGES_PATH,
      RestHandlerCreator<RestEdgesHandler>::createNoData);

  _handlerFactory->addPrefixHandler(
      RestVocbaseBaseHandler::GHARIAL_PATH,
      RestHandlerCreator<RestGraphHandler>::createNoData);

  _handlerFactory->addPrefixHandler(
      RestVocbaseBaseHandler::ENDPOINT_PATH,
      RestHandlerCreator<RestEndpointHandler>::createNoData);

  _handlerFactory->addPrefixHandler(
      RestVocbaseBaseHandler::IMPORT_PATH,
      RestHandlerCreator<RestImportHandler>::createNoData);

  _handlerFactory->addPrefixHandler(
      RestVocbaseBaseHandler::INDEX_PATH,
      RestHandlerCreator<RestIndexHandler>::createNoData);

  _handlerFactory->addPrefixHandler(
      RestVocbaseBaseHandler::SIMPLE_QUERY_ALL_PATH,
      RestHandlerCreator<RestSimpleQueryHandler>::createData<
          aql::QueryRegistry*>, queryRegistry);

  _handlerFactory->addPrefixHandler(
      RestVocbaseBaseHandler::SIMPLE_QUERY_ALL_KEYS_PATH,
      RestHandlerCreator<RestSimpleQueryHandler>::createData<
          aql::QueryRegistry*>, queryRegistry);

  _handlerFactory->addPrefixHandler(
      RestVocbaseBaseHandler::SIMPLE_QUERY_BY_EXAMPLE,
      RestHandlerCreator<RestSimpleQueryHandler>::createData<
      aql::QueryRegistry*>, queryRegistry);

  _handlerFactory->addPrefixHandler(
      RestVocbaseBaseHandler::SIMPLE_LOOKUP_PATH,
      RestHandlerCreator<RestSimpleHandler>::createData<aql::QueryRegistry*>,
      queryRegistry);

  _handlerFactory->addPrefixHandler(
      RestVocbaseBaseHandler::SIMPLE_REMOVE_PATH,
      RestHandlerCreator<RestSimpleHandler>::createData<aql::QueryRegistry*>,
      queryRegistry);

  _handlerFactory->addPrefixHandler(
      RestVocbaseBaseHandler::TASKS_PATH,
      RestHandlerCreator<RestTasksHandler>::createNoData);

  _handlerFactory->addPrefixHandler(
      RestVocbaseBaseHandler::UPLOAD_PATH,
      RestHandlerCreator<RestUploadHandler>::createNoData);

  _handlerFactory->addPrefixHandler(
    RestVocbaseBaseHandler::USERS_PATH,
    RestHandlerCreator<RestUsersHandler>::createNoData);

  _handlerFactory->addPrefixHandler(
      RestVocbaseBaseHandler::VIEW_PATH,
      RestHandlerCreator<RestViewHandler>::createNoData);

  // This is the only handler were we need to inject
  // more than one data object. So we created the combinedRegistries
  // for it.
  _handlerFactory->addPrefixHandler(
      "/_api/aql",
      RestHandlerCreator<aql::RestAqlHandler>::createData<
          std::pair<aql::QueryRegistry*, traverser::TraverserEngineRegistry*>*>,
          _combinedRegistries.get());

  _handlerFactory->addPrefixHandler(
      "/_api/aql-builtin",
      RestHandlerCreator<RestAqlFunctionsHandler>::createNoData);

  if (server()->isEnabled("V8Dealer")) {
    _handlerFactory->addPrefixHandler(
        "/_api/aqlfunction",
        RestHandlerCreator<RestAqlUserFunctionsHandler>::createNoData);
  }

  _handlerFactory->addPrefixHandler(
      "/_api/explain", RestHandlerCreator<RestExplainHandler>::createNoData);

  _handlerFactory->addPrefixHandler(
      "/_api/query", RestHandlerCreator<RestQueryHandler>::createNoData);

  _handlerFactory->addPrefixHandler(
      "/_api/query-cache",
      RestHandlerCreator<RestQueryCacheHandler>::createNoData);

  _handlerFactory->addPrefixHandler(
      "/_api/pregel", RestHandlerCreator<RestPregelHandler>::createNoData);

  _handlerFactory->addPrefixHandler(
      "/_api/wal", RestHandlerCreator<RestWalAccessHandler>::createNoData);

  if (agency->isEnabled()) {
    _handlerFactory->addPrefixHandler(
        RestVocbaseBaseHandler::AGENCY_PATH,
        RestHandlerCreator<RestAgencyHandler>::createData<consensus::Agent*>,
        agency->agent());

    _handlerFactory->addPrefixHandler(
        RestVocbaseBaseHandler::AGENCY_PRIV_PATH,
        RestHandlerCreator<RestAgencyPrivHandler>::createData<
            consensus::Agent*>,
        agency->agent());
  }

  if (cluster->isEnabled()) {
    // add "/agency-callbacks" handler
    _handlerFactory->addPrefixHandler(
        cluster->agencyCallbacksPath(),
        RestHandlerCreator<RestAgencyCallbacksHandler>::createData<
            AgencyCallbackRegistry*>,
        cluster->agencyCallbackRegistry());
    // add "_api/cluster" handler
    _handlerFactory->addPrefixHandler(cluster->clusterRestPath(),
                                      RestHandlerCreator<RestClusterHandler>::createNoData);
  }
  _handlerFactory->addPrefixHandler(
      RestVocbaseBaseHandler::INTERNAL_TRAVERSER_PATH,
      RestHandlerCreator<InternalRestTraverserHandler>::createData<
          traverser::TraverserEngineRegistry*>,
      traverserEngineRegistry);

  // And now some handlers which are registered in both /_api and /_admin
  _handlerFactory->addHandler(
      "/_admin/actions", RestHandlerCreator<MaintenanceRestHandler>::createNoData);

  _handlerFactory->addPrefixHandler(
      "/_api/job", RestHandlerCreator<arangodb::RestJobHandler>::createData<
                       AsyncJobManager*>,
      _jobManager.get());

  _handlerFactory->addPrefixHandler(
      "/_api/engine", RestHandlerCreator<RestEngineHandler>::createNoData);

  _handlerFactory->addHandler(
      "/_api/version", RestHandlerCreator<RestVersionHandler>::createNoData);

  _handlerFactory->addHandler(
    "/_api/transaction", RestHandlerCreator<RestTransactionHandler>::createNoData);

  // ...........................................................................
  // /_admin
  // ...........................................................................

  _handlerFactory->addHandler(
      "/_admin/status", RestHandlerCreator<RestStatusHandler>::createNoData);

  _handlerFactory->addPrefixHandler(
      "/_admin/job", RestHandlerCreator<arangodb::RestJobHandler>::createData<
                         AsyncJobManager*>,
      _jobManager.get());

  _handlerFactory->addHandler(
      "/_admin/version", RestHandlerCreator<RestVersionHandler>::createNoData);

  // further admin handlers
  _handlerFactory->addPrefixHandler(
      "/_admin/database/target-version",
      RestHandlerCreator<arangodb::RestAdminDatabaseHandler>::createNoData);

  _handlerFactory->addPrefixHandler(
      "/_admin/log",
      RestHandlerCreator<arangodb::RestAdminLogHandler>::createNoData);

  if (server()->isEnabled("V8Dealer")) {
    _handlerFactory->addPrefixHandler(
        "/_admin/routing",
        RestHandlerCreator<arangodb::RestAdminRoutingHandler>::createNoData);
  }

#ifdef ARANGODB_ENABLE_FAILURE_TESTS
  // This handler is to activate SYS_DEBUG_FAILAT on DB servers
  _handlerFactory->addPrefixHandler(
      "/_admin/debug", RestHandlerCreator<arangodb::RestDebugHandler>::createNoData);
#endif

  _handlerFactory->addPrefixHandler(
      "/_admin/shutdown",
      RestHandlerCreator<arangodb::RestShutdownHandler>::createNoData);

  if (authentication->isActive()) {
    _handlerFactory->addPrefixHandler(
        "/_open/auth",
        RestHandlerCreator<arangodb::RestAuthHandler>::createNoData);
  }

  _handlerFactory->addPrefixHandler(
    "/_admin/server",
    RestHandlerCreator<arangodb::RestAdminServerHandler>::createNoData);

  _handlerFactory->addHandler(
    "/_admin/statistics",
    RestHandlerCreator<arangodb::RestAdminStatisticsHandler>::createNoData);

  _handlerFactory->addHandler(
    "/_admin/statistics-description",
    RestHandlerCreator<arangodb::RestAdminStatisticsHandler>::createNoData);

  if (cluster->isEnabled()) {
    _handlerFactory->addPrefixHandler(
      "/_admin/repair",
      RestHandlerCreator<arangodb::RestRepairHandler>
      ::createNoData
    );
  }

  // ...........................................................................
  // test handler
  // ...........................................................................
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  _handlerFactory->addPrefixHandler(
    "/_api/test",
    RestHandlerCreator<RestTestHandler>::createNoData);
#endif

  // ...........................................................................
  // actions defined in v8
  // ...........................................................................

  _handlerFactory->addPrefixHandler(
     "/", RestHandlerCreator<RestActionHandler>::createNoData);

  // engine specific handlers
  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  TRI_ASSERT(engine != nullptr);  // Engine not loaded. Startup broken
  engine->addRestHandlers(*_handlerFactory);
}

} // arangodb
