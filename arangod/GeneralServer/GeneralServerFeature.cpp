////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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

#include "GeneralServerFeature.h"

#include <stdexcept>

#include "Actions/RestActionHandler.h"
#include "Agency/AgencyFeature.h"
#include "Agency/RestAgencyHandler.h"
#include "Agency/RestAgencyPrivHandler.h"
#include "ApplicationFeatures/HttpEndpointProvider.h"
#include "Aql/RestAqlHandler.h"
#include "Basics/NumberOfCores.h"
#include "Basics/StringUtils.h"
#include "Basics/application-exit.h"
#include "Cluster/AgencyCallbackRegistry.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/MaintenanceRestHandler.h"
#include "Cluster/RestAgencyCallbacksHandler.h"
#include "Cluster/RestClusterHandler.h"
#include "FeaturePhases/AqlFeaturePhase.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "GeneralServer/GeneralServer.h"
#include "GeneralServer/RestHandlerFactory.h"
#include "GeneralServer/SslServerFeature.h"
#include "InternalRestHandler/InternalRestTraverserHandler.h"
#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "Rest/HttpResponse.h"
#include "RestHandler/RestAdminClusterHandler.h"
#include "RestHandler/RestAdminDatabaseHandler.h"
#include "RestHandler/RestAdminExecuteHandler.h"
#include "RestHandler/RestAdminLogHandler.h"
#include "RestHandler/RestAdminRoutingHandler.h"
#include "RestHandler/RestAdminServerHandler.h"
#include "RestHandler/RestAdminStatisticsHandler.h"
#include "RestHandler/RestAnalyzerHandler.h"
#include "RestHandler/RestAqlFunctionsHandler.h"
#include "RestHandler/RestAqlUserFunctionsHandler.h"
#include "RestHandler/RestAuthHandler.h"
#include "RestHandler/RestAuthReloadHandler.h"
#include "RestHandler/RestBatchHandler.h"
#include "RestHandler/RestCompactHandler.h"
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
#include "RestHandler/RestMetricsHandler.h"
#include "RestHandler/RestPleaseUpgradeHandler.h"
#include "RestHandler/RestPregelHandler.h"
#include "RestHandler/RestQueryCacheHandler.h"
#include "RestHandler/RestQueryHandler.h"
#include "RestHandler/RestRedirectHandler.h"
#include "RestHandler/RestRepairHandler.h"
#include "RestHandler/RestShutdownHandler.h"
#include "RestHandler/RestSimpleHandler.h"
#include "RestHandler/RestSimpleQueryHandler.h"
#include "RestHandler/RestSystemReportHandler.h"
#include "RestHandler/RestStatusHandler.h"
#include "RestHandler/RestSupervisionStateHandler.h"
#include "RestHandler/RestTasksHandler.h"
#include "RestHandler/RestTestHandler.h"
#include "RestHandler/RestTimeHandler.h"
#include "RestHandler/RestTransactionHandler.h"
#include "RestHandler/RestTtlHandler.h"
#include "RestHandler/RestUploadHandler.h"
#include "RestHandler/RestUsersHandler.h"
#include "RestHandler/RestVersionHandler.h"
#include "RestHandler/RestViewHandler.h"
#include "RestHandler/RestWalAccessHandler.h"
#include "RestServer/EndpointFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/UpgradeFeature.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "V8Server/V8DealerFeature.h"

#ifdef USE_ENTERPRISE
#include "Enterprise/RestHandler/RestHotBackupHandler.h"
#include "Enterprise/StorageEngine/HotBackupFeature.h"
#endif

using namespace arangodb::rest;
using namespace arangodb::options;

namespace arangodb {

static uint64_t const _maxIoThreads = 64;

rest::RestHandlerFactory* GeneralServerFeature::HANDLER_FACTORY = nullptr;
rest::AsyncJobManager* GeneralServerFeature::JOB_MANAGER = nullptr;
GeneralServerFeature* GeneralServerFeature::GENERAL_SERVER = nullptr;

GeneralServerFeature::GeneralServerFeature(application_features::ApplicationServer& server)
    : ApplicationFeature(server, "GeneralServer"),
      _allowMethodOverride(false),
      _proxyCheck(true),
      _numIoThreads(0) {
  setOptional(true);
  startsAfter<application_features::AqlFeaturePhase>();

  startsAfter<HttpEndpointProvider>();
  startsAfter<SslServerFeature>();
  startsAfter<UpgradeFeature>();

  _numIoThreads = (std::max)(static_cast<uint64_t>(1),
                             static_cast<uint64_t>(NumberOfCores::getValue() / 4));
  if (_numIoThreads > _maxIoThreads) {
    _numIoThreads = _maxIoThreads;
  }
}

void GeneralServerFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addSection("server", "Server features");

  options->addOldOption("server.allow-method-override",
                        "http.allow-method-override");
  options->addOldOption("server.hide-product-header",
                        "http.hide-product-header");
  options->addOldOption("server.keep-alive-timeout", "http.keep-alive-timeout");
  options->addOldOption("server.default-api-compatibility", "");
  options->addOldOption("no-server", "server.rest-server");

  options->addOption("--server.io-threads",
                     "Number of threads used to handle IO",
                     new UInt64Parameter(&_numIoThreads),
                     arangodb::options::makeDefaultFlags(arangodb::options::Flags::Dynamic));

  options->addSection("http", "HttpServer features");

  options->addOption("--http.allow-method-override",
                     "allow HTTP method override using special headers",
                     new BooleanParameter(&_allowMethodOverride),
                     arangodb::options::makeDefaultFlags(arangodb::options::Flags::Hidden));

  options->addOption("--http.keep-alive-timeout",
                     "keep-alive timeout in seconds",
                     new DoubleParameter(&_keepAliveTimeout));

  options->addOption(
      "--http.hide-product-header",
      "do not expose \"Server: ArangoDB\" header in HTTP responses",
      new BooleanParameter(&HttpResponse::HIDE_PRODUCT_HEADER));

  options->addOption("--http.trusted-origin",
                     "trusted origin URLs for CORS requests with credentials",
                     new VectorParameter<StringParameter>(&_accessControlAllowOrigins));

  options->addSection("frontend", "Frontend options");

  options->addOption("--frontend.proxy-request-check",
                     "enable proxy request checking",
                     new BooleanParameter(&_proxyCheck),
                     arangodb::options::makeFlags(
                     arangodb::options::Flags::DefaultNoComponents,
                     arangodb::options::Flags::OnCoordinator,
                     arangodb::options::Flags::OnSingle));

  options->addOption("--frontend.trusted-proxy",
                     "list of proxies to trust (may be IP or network). Make "
                     "sure --frontend.proxy-request-check is enabled",
                     new VectorParameter<StringParameter>(&_trustedProxies),
                     arangodb::options::makeFlags(
                     arangodb::options::Flags::DefaultNoComponents,
                     arangodb::options::Flags::OnCoordinator,
                     arangodb::options::Flags::OnSingle));
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
    LOG_TOPIC("1ade3", WARN, Logger::FIXME) << "Need at least one io-context thread.";
    _numIoThreads = 1;
  } else if (_numIoThreads > _maxIoThreads) {
    LOG_TOPIC("80dcf", WARN, Logger::FIXME) << "IO-contexts are limited to " << _maxIoThreads;
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

void GeneralServerFeature::beginShutdown() {
  for (auto& server : _servers) {
    server->stopListening();
  }
}

void GeneralServerFeature::stop() {
  _jobManager->deleteJobs();
  for (auto& server : _servers) {
    server->stopConnections();
  }
}

void GeneralServerFeature::unprepare() {
  for (auto& server : _servers) {
    server->stopWorking();
  }
  _servers.clear();
  _jobManager.reset();

  GENERAL_SERVER = nullptr;
  JOB_MANAGER = nullptr;
  HANDLER_FACTORY = nullptr;
}

void GeneralServerFeature::buildServers() {
  TRI_ASSERT(_jobManager != nullptr);

  EndpointFeature& endpoint =
      server().getFeature<HttpEndpointProvider, EndpointFeature>();
  auto const& endpointList = endpoint.endpointList();

  // check if endpointList contains ssl featured server
  if (endpointList.hasSsl()) {
    if (!server().hasFeature<SslServerFeature>()) {
      LOG_TOPIC("8df10", FATAL, arangodb::Logger::FIXME)
          << "no ssl context is known, cannot create https server, "
             "please enable SSL";
      FATAL_ERROR_EXIT();
    }
    SslServerFeature& ssl = server().getFeature<SslServerFeature>();
    ssl.SSL->verifySslOptions();
  }

  auto server = std::make_unique<GeneralServer>(*this, _numIoThreads);
  server->setEndpointList(&endpointList);
  _servers.push_back(std::move(server));
}

void GeneralServerFeature::defineHandlers() {
  TRI_ASSERT(_jobManager != nullptr);

  AgencyFeature& agency = server().getFeature<AgencyFeature>();
  ClusterFeature& cluster = server().getFeature<ClusterFeature>();
  AuthenticationFeature& authentication = server().getFeature<AuthenticationFeature>();
#ifdef USE_ENTERPRISE
  HotBackupFeature& backup = server().getFeature<HotBackupFeature>();
#endif

  // ...........................................................................
  // /_msg
  // ...........................................................................

  _handlerFactory->addPrefixHandler("/_msg/please-upgrade",
                                    RestHandlerCreator<RestPleaseUpgradeHandler>::createNoData);

  // ...........................................................................
  // /_api
  // ...........................................................................

  _handlerFactory->addPrefixHandler( // add handler
    RestVocbaseBaseHandler::ANALYZER_PATH, // base URL
    RestHandlerCreator<iresearch::RestAnalyzerHandler>::createNoData // handler
  );

  _handlerFactory->addPrefixHandler(RestVocbaseBaseHandler::BATCH_PATH,
                                    RestHandlerCreator<RestBatchHandler>::createNoData);

  _handlerFactory->addPrefixHandler(RestVocbaseBaseHandler::CONTROL_PREGEL_PATH,
                                    RestHandlerCreator<RestControlPregelHandler>::createNoData);

  auto queryRegistry = QueryRegistryFeature::registry();
  _handlerFactory->addPrefixHandler(RestVocbaseBaseHandler::CURSOR_PATH,
                                    RestHandlerCreator<RestCursorHandler>::createData<aql::QueryRegistry*>,
                                    queryRegistry);

  _handlerFactory->addPrefixHandler(RestVocbaseBaseHandler::DATABASE_PATH,
                                    RestHandlerCreator<RestDatabaseHandler>::createNoData);

  _handlerFactory->addPrefixHandler(RestVocbaseBaseHandler::DOCUMENT_PATH,
                                    RestHandlerCreator<RestDocumentHandler>::createNoData);

  _handlerFactory->addPrefixHandler(RestVocbaseBaseHandler::EDGES_PATH,
                                    RestHandlerCreator<RestEdgesHandler>::createNoData);

  _handlerFactory->addPrefixHandler(RestVocbaseBaseHandler::GHARIAL_PATH,
                                    RestHandlerCreator<RestGraphHandler>::createNoData);

  _handlerFactory->addPrefixHandler(RestVocbaseBaseHandler::ENDPOINT_PATH,
                                    RestHandlerCreator<RestEndpointHandler>::createNoData);

  _handlerFactory->addPrefixHandler(RestVocbaseBaseHandler::IMPORT_PATH,
                                    RestHandlerCreator<RestImportHandler>::createNoData);

  _handlerFactory->addPrefixHandler(RestVocbaseBaseHandler::INDEX_PATH,
                                    RestHandlerCreator<RestIndexHandler>::createNoData);

  _handlerFactory->addPrefixHandler(
      RestVocbaseBaseHandler::SIMPLE_QUERY_ALL_PATH,
      RestHandlerCreator<RestSimpleQueryHandler>::createData<aql::QueryRegistry*>,
      queryRegistry);

  _handlerFactory->addPrefixHandler(
      RestVocbaseBaseHandler::SIMPLE_QUERY_ALL_KEYS_PATH,
      RestHandlerCreator<RestSimpleQueryHandler>::createData<aql::QueryRegistry*>,
      queryRegistry);

  _handlerFactory->addPrefixHandler(
      RestVocbaseBaseHandler::SIMPLE_QUERY_BY_EXAMPLE,
      RestHandlerCreator<RestSimpleQueryHandler>::createData<aql::QueryRegistry*>,
      queryRegistry);

  _handlerFactory->addPrefixHandler(RestVocbaseBaseHandler::SIMPLE_LOOKUP_PATH,
                                    RestHandlerCreator<RestSimpleHandler>::createData<aql::QueryRegistry*>,
                                    queryRegistry);

  _handlerFactory->addPrefixHandler(RestVocbaseBaseHandler::SIMPLE_REMOVE_PATH,
                                    RestHandlerCreator<RestSimpleHandler>::createData<aql::QueryRegistry*>,
                                    queryRegistry);

  if (server().isEnabled<V8DealerFeature>()) {
    // the tasks feature depends on V8. only enable it if JavaScript is enabled
    _handlerFactory->addPrefixHandler(RestVocbaseBaseHandler::TASKS_PATH,
                                      RestHandlerCreator<RestTasksHandler>::createNoData);
  }

  _handlerFactory->addPrefixHandler(RestVocbaseBaseHandler::UPLOAD_PATH,
                                    RestHandlerCreator<RestUploadHandler>::createNoData);

  _handlerFactory->addPrefixHandler(RestVocbaseBaseHandler::USERS_PATH,
                                    RestHandlerCreator<RestUsersHandler>::createNoData);

  _handlerFactory->addPrefixHandler(RestVocbaseBaseHandler::VIEW_PATH,
                                    RestHandlerCreator<RestViewHandler>::createNoData);

  // This is the only handler were we need to inject
  // more than one data object. So we created the combinedRegistries
  // for it.
  _handlerFactory->addPrefixHandler(
      "/_api/aql",
      RestHandlerCreator<aql::RestAqlHandler>::createData<aql::QueryRegistry*>, queryRegistry);

  _handlerFactory->addPrefixHandler("/_api/aql-builtin",
                                    RestHandlerCreator<RestAqlFunctionsHandler>::createNoData);

  if (server().isEnabled<V8DealerFeature>()) {
    // the AQL UDfs feature depends on V8. only enable it if JavaScript is enabled
    _handlerFactory->addPrefixHandler("/_api/aqlfunction",
                                      RestHandlerCreator<RestAqlUserFunctionsHandler>::createNoData);
  }

  _handlerFactory->addPrefixHandler("/_api/explain",
                                    RestHandlerCreator<RestExplainHandler>::createNoData);

  _handlerFactory->addPrefixHandler("/_api/query",
                                    RestHandlerCreator<RestQueryHandler>::createNoData);

  _handlerFactory->addPrefixHandler("/_api/query-cache",
                                    RestHandlerCreator<RestQueryCacheHandler>::createNoData);

  _handlerFactory->addPrefixHandler("/_api/pregel",
                                    RestHandlerCreator<RestPregelHandler>::createNoData);

  _handlerFactory->addPrefixHandler("/_api/wal",
                                    RestHandlerCreator<RestWalAccessHandler>::createNoData);

  if (agency.isEnabled()) {
    _handlerFactory->addPrefixHandler(RestVocbaseBaseHandler::AGENCY_PATH,
                                      RestHandlerCreator<RestAgencyHandler>::createData<consensus::Agent*>,
                                      agency.agent());

    _handlerFactory->addPrefixHandler(
        RestVocbaseBaseHandler::AGENCY_PRIV_PATH,
        RestHandlerCreator<RestAgencyPrivHandler>::createData<consensus::Agent*>,
        agency.agent());
  }

  
  if (cluster.isEnabled()) {
    // add "/agency-callbacks" handler
    _handlerFactory->addPrefixHandler(
        cluster.agencyCallbacksPath(),
        RestHandlerCreator<RestAgencyCallbacksHandler>::createData<AgencyCallbackRegistry*>,
        cluster.agencyCallbackRegistry());
    // add "_api/cluster" handler
    _handlerFactory->addPrefixHandler(cluster.clusterRestPath(),
                                      RestHandlerCreator<RestClusterHandler>::createNoData);
  }
  _handlerFactory->addPrefixHandler(
      RestVocbaseBaseHandler::INTERNAL_TRAVERSER_PATH,
      RestHandlerCreator<InternalRestTraverserHandler>::createData<aql::QueryRegistry*>,
       queryRegistry);

  // And now some handlers which are registered in both /_api and /_admin
  _handlerFactory->addHandler("/_admin/actions",
                              RestHandlerCreator<MaintenanceRestHandler>::createNoData);
    
  _handlerFactory->addHandler("/_admin/auth/reload",
                              RestHandlerCreator<RestAuthReloadHandler>::createNoData);

  if (V8DealerFeature::DEALER && V8DealerFeature::DEALER->allowAdminExecute()) {
    // the /_admin/execute API depends on V8. only enable it if JavaScript is enabled
    _handlerFactory->addHandler("/_admin/execute",
                                RestHandlerCreator<RestAdminExecuteHandler>::createNoData);
  }

  _handlerFactory->addHandler("/_admin/time",
                              RestHandlerCreator<RestTimeHandler>::createNoData);
  
  _handlerFactory->addHandler("/_admin/compact",
                              RestHandlerCreator<RestCompactHandler>::createNoData);

  _handlerFactory->addPrefixHandler("/_api/job",
                                    RestHandlerCreator<arangodb::RestJobHandler>::createData<AsyncJobManager*>,
                                    _jobManager.get());

  _handlerFactory->addPrefixHandler("/_api/engine",
                                    RestHandlerCreator<RestEngineHandler>::createNoData);

  _handlerFactory->addHandler("/_api/version",
                              RestHandlerCreator<RestVersionHandler>::createNoData);

  _handlerFactory->addPrefixHandler("/_api/transaction",
                                    RestHandlerCreator<RestTransactionHandler>::createNoData);

  _handlerFactory->addPrefixHandler("/_api/ttl",
                                    RestHandlerCreator<RestTtlHandler>::createNoData);

  // ...........................................................................
  // /_admin
  // ...........................................................................

  _handlerFactory->addPrefixHandler("/_admin/cluster",
                                    RestHandlerCreator<arangodb::RestAdminClusterHandler>::createNoData);

  _handlerFactory->addHandler("/_admin/status",
                              RestHandlerCreator<RestStatusHandler>::createNoData);

  _handlerFactory->addHandler("/_admin/system-report",
                              RestHandlerCreator<RestSystemReportHandler>::createNoData);

  _handlerFactory->addPrefixHandler("/_admin/job",
                                    RestHandlerCreator<arangodb::RestJobHandler>::createData<AsyncJobManager*>,
                                    _jobManager.get());

  _handlerFactory->addHandler("/_admin/version",
                              RestHandlerCreator<RestVersionHandler>::createNoData);

  // further admin handlers
  _handlerFactory->addPrefixHandler("/_admin/database/target-version",
                                    RestHandlerCreator<arangodb::RestAdminDatabaseHandler>::createNoData);

  _handlerFactory->addPrefixHandler("/_admin/log",
                                    RestHandlerCreator<arangodb::RestAdminLogHandler>::createNoData);

  if (server().isEnabled<V8DealerFeature>()) {
    // the routing feature depends on V8. only enable it if JavaScript is enabled
    _handlerFactory->addPrefixHandler("/_admin/routing",
                                      RestHandlerCreator<arangodb::RestAdminRoutingHandler>::createNoData);
  }

  _handlerFactory->addHandler("/_admin/supervisionState",
                              RestHandlerCreator<arangodb::RestSupervisionStateHandler>::createNoData);

#ifdef ARANGODB_ENABLE_FAILURE_TESTS
  // This handler is to activate SYS_DEBUG_FAILAT on DB servers
  _handlerFactory->addPrefixHandler("/_admin/debug",
                                    RestHandlerCreator<arangodb::RestDebugHandler>::createNoData);
#endif

  _handlerFactory->addPrefixHandler("/_admin/shutdown",
                                    RestHandlerCreator<arangodb::RestShutdownHandler>::createNoData);

  if (authentication.isActive()) {
    _handlerFactory->addPrefixHandler("/_open/auth",
                                      RestHandlerCreator<arangodb::RestAuthHandler>::createNoData);
  }

  _handlerFactory->addPrefixHandler("/_admin/server",
                                    RestHandlerCreator<arangodb::RestAdminServerHandler>::createNoData);

  _handlerFactory->addHandler("/_admin/statistics",
                              RestHandlerCreator<arangodb::RestAdminStatisticsHandler>::createNoData);

  _handlerFactory->addHandler("/_admin/metrics",
                              RestHandlerCreator<arangodb::RestMetricsHandler>::createNoData);

  _handlerFactory->addHandler("/_admin/statistics-description",
                              RestHandlerCreator<arangodb::RestAdminStatisticsHandler>::createNoData);

  if (cluster.isEnabled()) {
    _handlerFactory->addPrefixHandler("/_admin/repair",
                                      RestHandlerCreator<arangodb::RestRepairHandler>::createNoData);
  }

#ifdef USE_ENTERPRISE
  if (backup.isAPIEnabled()) {
    _handlerFactory->addPrefixHandler("/_admin/backup",
                                    RestHandlerCreator<arangodb::RestHotBackupHandler>::createNoData);
  }
#endif


  // ...........................................................................
  // test handler
  // ...........................................................................
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  _handlerFactory->addPrefixHandler("/_api/test",
                                    RestHandlerCreator<RestTestHandler>::createNoData);
#endif

  // ...........................................................................
  // actions defined in v8
  // ...........................................................................

  _handlerFactory->addPrefixHandler("/", RestHandlerCreator<RestActionHandler>::createNoData);

  // ...........................................................................
  // redirects
  // ...........................................................................

  // UGLY HACK INCOMING!

#define ADD_REDIRECT(from, to) do{_handlerFactory->addPrefixHandler(from, \
                                    RestHandlerCreator<RestRedirectHandler>::createData<const char*>, \
                                    (void*) to);}while(0)

  ADD_REDIRECT("/_admin/clusterNodeVersion", "/_admin/cluster/nodeVersion");
  ADD_REDIRECT("/_admin/clusterNodeEngine", "/_admin/cluster/nodeEngine");
  ADD_REDIRECT("/_admin/clusterNodeStats", "/_admin/cluster/nodeStatistics");
  ADD_REDIRECT("/_admin/clusterStatistics", "/_admin/cluster/statistics");


  // engine specific handlers
  StorageEngine& engine = server().getFeature<EngineSelectorFeature>().engine();
  engine.addRestHandlers(*_handlerFactory);
}

}  // namespace arangodb
