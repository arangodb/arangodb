////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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

#include <chrono>
#include <stdexcept>
#include <thread>

#include "ApplicationFeatures/ApplicationServer.h"
#include "Actions/RestActionHandler.h"
#include "Agency/AgencyFeature.h"
#include "Agency/RestAgencyHandler.h"
#include "Agency/RestAgencyPrivHandler.h"
#include "ApplicationFeatures/HttpEndpointProvider.h"
#include "Aql/RestAqlHandler.h"
#include "Basics/NumberOfCores.h"
#include "Basics/StringUtils.h"
#include "Basics/application-exit.h"
#include "Basics/debugging.h"
#include "Basics/FeatureFlags.h"
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
#include "RestHandler/RestLicenseHandler.h"
#include "RestHandler/RestLogHandler.h"
#include "RestHandler/RestLogInternalHandler.h"
#include "RestHandler/RestMetricsHandler.h"
#include "RestHandler/RestPregelHandler.h"
#include "RestHandler/RestQueryCacheHandler.h"
#include "RestHandler/RestQueryHandler.h"
#include "RestHandler/RestPrototypeStateHandler.h"
#include "RestHandler/RestReplicatedStateHandler.h"
#include "RestHandler/RestShutdownHandler.h"
#include "RestHandler/RestSimpleHandler.h"
#include "RestHandler/RestSimpleQueryHandler.h"
#include "RestHandler/RestStatusHandler.h"
#include "RestHandler/RestSupervisionStateHandler.h"
#include "RestHandler/RestSupportInfoHandler.h"
#include "RestHandler/RestSystemReportHandler.h"
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
#include "Metrics/HistogramBuilder.h"
#include "Metrics/CounterBuilder.h"
#include "Metrics/MetricsFeature.h"
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

constexpr uint64_t const maxIoThreads = 64;

struct RequestBodySizeScale {
  static metrics::LogScale<uint64_t> scale() { return {2, 64, 65536, 10}; }
};

DECLARE_HISTOGRAM(arangodb_request_body_size_http1, RequestBodySizeScale,
                  "Body size of HTTP/1.1 requests");
DECLARE_HISTOGRAM(arangodb_request_body_size_http2, RequestBodySizeScale,
                  "Body size of HTTP/2 requests");
DECLARE_HISTOGRAM(arangodb_request_body_size_vst, RequestBodySizeScale,
                  "Body size of VST requests");
DECLARE_COUNTER(arangodb_http2_connections_total,
                "Total number of HTTP/2 connections");
DECLARE_COUNTER(arangodb_vst_connections_total,
                "Total number of VST connections");

GeneralServerFeature::GeneralServerFeature(Server& server)
    : ArangodFeature{server, *this},
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
      _startedListening(false),
#endif
      _allowEarlyConnections(false),
      _allowMethodOverride(false),
      _proxyCheck(true),
      _returnQueueTimeHeader(true),
      _permanentRootRedirect(true),
      _redirectRootTo("/_admin/aardvark/index.html"),
      _supportInfoApiPolicy("admin"),
      _numIoThreads(0),
      _requestBodySizeHttp1(server.getFeature<metrics::MetricsFeature>().add(
          arangodb_request_body_size_http1{})),
      _requestBodySizeHttp2(server.getFeature<metrics::MetricsFeature>().add(
          arangodb_request_body_size_http2{})),
      _requestBodySizeVst(server.getFeature<metrics::MetricsFeature>().add(
          arangodb_request_body_size_vst{})),
      _http2Connections(server.getFeature<metrics::MetricsFeature>().add(
          arangodb_http2_connections_total{})),
      _vstConnections(server.getFeature<metrics::MetricsFeature>().add(
          arangodb_vst_connections_total{})) {
  static_assert(
      Server::isCreatedAfter<GeneralServerFeature, metrics::MetricsFeature>());

  setOptional(true);
  startsAfter<application_features::AqlFeaturePhase>();

  startsAfter<HttpEndpointProvider>();
  startsAfter<SslServerFeature>();
  startsAfter<SchedulerFeature>();
  startsAfter<UpgradeFeature>();

  _numIoThreads =
      (std::max)(static_cast<uint64_t>(1),
                 static_cast<uint64_t>(NumberOfCores::getValue() / 4));
  if (_numIoThreads > maxIoThreads) {
    _numIoThreads = maxIoThreads;
  }
}

void GeneralServerFeature::collectOptions(
    std::shared_ptr<ProgramOptions> options) {
  options->addOldOption("server.allow-method-override",
                        "http.allow-method-override");
  options->addOldOption("server.hide-product-header",
                        "http.hide-product-header");
  options->addOldOption("server.keep-alive-timeout", "http.keep-alive-timeout");
  options->addOldOption("no-server", "server.rest-server");

  options->addOption(
      "--server.io-threads", "number of threads used to handle IO",
      new UInt64Parameter(&_numIoThreads),
      arangodb::options::makeDefaultFlags(arangodb::options::Flags::Dynamic));

  options
      ->addOption("--server.support-info-api",
                  "policy for exposing support info API",
                  new DiscreteValuesParameter<StringParameter>(
                      &_supportInfoApiPolicy,
                      std::unordered_set<std::string>{"disabled", "jwt",
                                                      "admin", "public"}))
      .setIntroducedIn(30900);

  options->addSection("http", "HTTP server features");

  options
      ->addOption("--http.allow-method-override",
                  "allow HTTP method override using special headers",
                  new BooleanParameter(&_allowMethodOverride),
                  arangodb::options::makeDefaultFlags(
                      arangodb::options::Flags::Uncommon))
      .setDeprecatedIn(30800);

  options->addOption("--http.keep-alive-timeout",
                     "keep-alive timeout in seconds",
                     new DoubleParameter(&_keepAliveTimeout));

  options
      ->addOption("--http.hide-product-header",
                  "do not expose \"Server: ArangoDB\" header in HTTP responses",
                  new BooleanParameter(&HttpResponse::HIDE_PRODUCT_HEADER))
      .setDeprecatedIn(30800);

  options->addOption(
      "--http.trusted-origin",
      "trusted origin URLs for CORS requests with credentials",
      new VectorParameter<StringParameter>(&_accessControlAllowOrigins));

  options
      ->addOption("--http.redirect-root-to", "redirect of root URL",
                  new StringParameter(&_redirectRootTo))
      .setIntroducedIn(30712);

  options
      ->addOption(
          "--http.permanently-redirect-root",
          "if true, use a permanent redirect. If false, use a temporary",
          new BooleanParameter(&_permanentRootRedirect))
      .setIntroducedIn(30712);

  options
      ->addOption("--http.return-queue-time-header",
                  "if true, return the 'x-arango-queue-time-seconds' header in "
                  "responses",
                  new BooleanParameter(&_returnQueueTimeHeader))
      .setIntroducedIn(30900);

  options
      ->addOption("--server.early-connections",
                  "allow requests to limited APIs early during server startup",
                  new BooleanParameter(&_allowEarlyConnections))
      .setIntroducedIn(31000);

  options->addOldOption("frontend.proxy-request-check",
                        "web-interface.proxy-request-check");

  options->addOption("--web-interface.proxy-request-check",
                     "enable proxy request checking",
                     new BooleanParameter(&_proxyCheck),
                     arangodb::options::makeFlags(
                         arangodb::options::Flags::DefaultNoComponents,
                         arangodb::options::Flags::OnCoordinator,
                         arangodb::options::Flags::OnSingle));

  options->addOldOption("frontend.trusted-proxy",
                        "web-interface.trusted-proxy");

  options->addOption("--web-interface.trusted-proxy",
                     "list of proxies to trust (may be IP or network). Make "
                     "sure --web-interface.proxy-request-check is enabled",
                     new VectorParameter<StringParameter>(&_trustedProxies),
                     arangodb::options::makeFlags(
                         arangodb::options::Flags::DefaultNoComponents,
                         arangodb::options::Flags::OnCoordinator,
                         arangodb::options::Flags::OnSingle));

#ifdef ARANGODB_ENABLE_FAILURE_TESTS
  options->addOption(
      "--server.failure-point",
      "failure point to set during server startup (requires compilation with "
      "failure points support)",
      new VectorParameter<StringParameter>(&_failurePoints),
      arangodb::options::makeFlags(arangodb::options::Flags::Default,
                                   arangodb::options::Flags::Uncommon));
#endif
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
      } else if (!it.empty() && it.back() == '/') {
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
    LOG_TOPIC("1ade3", WARN, Logger::FIXME) << "Need at least one io-context";
    _numIoThreads = 1;
  } else if (_numIoThreads > maxIoThreads) {
    LOG_TOPIC("80dcf", WARN, Logger::FIXME)
        << "io-contexts are limited to " << maxIoThreads;
    _numIoThreads = maxIoThreads;
  }

#ifdef ARANGODB_ENABLE_FAILURE_TESTS
  for (auto const& it : _failurePoints) {
    TRI_AddFailurePointDebugging(it);
  }
#endif
}

void GeneralServerFeature::prepare() {
  ServerState::instance()->setServerMode(ServerState::Mode::STARTUP);

  if (ServerState::instance()->isDBServer() &&
      !server().options()->processingResult().touched(
          "http.hide-product-header")) {
    // if we are a DB server, client applications will not talk to us
    // directly, so we can turn off the Server signature header.
    HttpResponse::HIDE_PRODUCT_HEADER = true;
  }

  _jobManager = std::make_unique<AsyncJobManager>();

  // create an initial, very stripped-down RestHandlerFactory.
  // this initial factory only knows a few selected RestHandlers.
  // we will later create another RestHandlerFactory that knows
  // all routes.
  auto hf = std::make_shared<RestHandlerFactory>();
  defineInitialHandlers(*hf);
  // make handler-factory read-only
  hf->seal();

  std::atomic_store(&_handlerFactory, std::move(hf));

  buildServers();

  if (_allowEarlyConnections) {
    // open HTTP interface early if this is requested.
    startListening();
  }

#ifdef ARANGODB_ENABLE_FAILURE_TESTS
  TRI_IF_FAILURE("startListeningEarly") {
    while (TRI_ShouldFailDebugging("startListeningEarly")) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  }
#endif
}

void GeneralServerFeature::start() {
  TRI_ASSERT(ServerState::instance()->mode() == ServerState::Mode::STARTUP);

  // create the full RestHandlerFactory that knows all the routes.
  // this will replace the previous, stripped-down RestHandlerFactory
  // instance.
  auto hf = std::make_shared<RestHandlerFactory>();

  defineInitialHandlers(*hf);
  defineRemainingHandlers(*hf);
  hf->seal();

  std::atomic_store(&_handlerFactory, std::move(hf));

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  TRI_ASSERT(!_allowEarlyConnections || _startedListening);
#endif
  if (!_allowEarlyConnections) {
    // if HTTP interface is not open yet, open it now
    startListening();
  }
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  TRI_ASSERT(_startedListening);
#endif

  ServerState::instance()->setServerMode(ServerState::Mode::MAINTENANCE);
}

void GeneralServerFeature::initiateSoftShutdown() {
  if (_jobManager != nullptr) {
    _jobManager->initiateSoftShutdown();
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
}

double GeneralServerFeature::keepAliveTimeout() const noexcept {
  return _keepAliveTimeout;
}

bool GeneralServerFeature::proxyCheck() const noexcept { return _proxyCheck; }

bool GeneralServerFeature::returnQueueTimeHeader() const noexcept {
  return _returnQueueTimeHeader;
}

std::vector<std::string> GeneralServerFeature::trustedProxies() const {
  return _trustedProxies;
}

bool GeneralServerFeature::allowMethodOverride() const noexcept {
  return _allowMethodOverride;
}

std::vector<std::string> const&
GeneralServerFeature::accessControlAllowOrigins() const {
  return _accessControlAllowOrigins;
}

Result GeneralServerFeature::reloadTLS() {  // reload TLS data from disk
  Result res;
  for (auto& up : _servers) {
    Result res2 = up->reloadTLS();
    if (res2.fail()) {
      res = res2;  // yes, we only report the last error if there is one
    }
  }
  return res;
}

bool GeneralServerFeature::permanentRootRedirect() const noexcept {
  return _permanentRootRedirect;
}

std::string GeneralServerFeature::redirectRootTo() const {
  return _redirectRootTo;
}

std::string const& GeneralServerFeature::supportInfoApiPolicy() const noexcept {
  return _supportInfoApiPolicy;
}

std::shared_ptr<rest::RestHandlerFactory> GeneralServerFeature::handlerFactory()
    const {
  return std::atomic_load_explicit(&_handlerFactory, std::memory_order_relaxed);
}

rest::AsyncJobManager& GeneralServerFeature::jobManager() {
  return *_jobManager;
}

void GeneralServerFeature::buildServers() {
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
    ssl.verifySslOptions();
  }

  _servers.emplace_back(std::make_unique<GeneralServer>(
      *this, _numIoThreads, _allowEarlyConnections));
}

void GeneralServerFeature::startListening() {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  TRI_ASSERT(!_startedListening);
#endif

  EndpointFeature& endpoint =
      server().getFeature<HttpEndpointProvider, EndpointFeature>();
  auto& endpointList = endpoint.endpointList();

  for (auto& server : _servers) {
    server->startListening(endpointList);
  }

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  _startedListening = true;
#endif
}

void GeneralServerFeature::defineInitialHandlers(rest::RestHandlerFactory& f) {
  // these handlers will be available early during the server start.
  // if you want to add more handlers here, please make sure that they
  // run on the CLIENT_FAST request lane. otherwise the incoming requests
  // will still be rejected during startup, even though they are registered
  // here.
  f.addHandler("/_api/version",
               RestHandlerCreator<RestVersionHandler>::createNoData);
  f.addHandler("/_admin/version",
               RestHandlerCreator<RestVersionHandler>::createNoData);
  f.addHandler("/_admin/status",
               RestHandlerCreator<RestStatusHandler>::createNoData);
#ifdef ARANGODB_ENABLE_FAILURE_TESTS
  // This handler can be used to control failure points
  f.addPrefixHandler(
      "/_admin/debug",
      RestHandlerCreator<arangodb::RestDebugHandler>::createNoData);
#endif
}

void GeneralServerFeature::defineRemainingHandlers(
    rest::RestHandlerFactory& f) {
  TRI_ASSERT(_jobManager != nullptr);

  AgencyFeature& agency = server().getFeature<AgencyFeature>();
  ClusterFeature& cluster = server().getFeature<ClusterFeature>();
  AuthenticationFeature& authentication =
      server().getFeature<AuthenticationFeature>();

  // ...........................................................................
  // /_api
  // ...........................................................................

  f.addPrefixHandler(                         // add handler
      RestVocbaseBaseHandler::ANALYZER_PATH,  // base URL
      RestHandlerCreator<
          iresearch::RestAnalyzerHandler>::createNoData  // handler
  );

  f.addPrefixHandler(RestVocbaseBaseHandler::BATCH_PATH,
                     RestHandlerCreator<RestBatchHandler>::createNoData);

  f.addPrefixHandler(
      RestVocbaseBaseHandler::CONTROL_PREGEL_PATH,
      RestHandlerCreator<RestControlPregelHandler>::createNoData);

  auto queryRegistry = QueryRegistryFeature::registry();
  f.addPrefixHandler(
      RestVocbaseBaseHandler::CURSOR_PATH,
      RestHandlerCreator<RestCursorHandler>::createData<aql::QueryRegistry*>,
      queryRegistry);

  f.addPrefixHandler(RestVocbaseBaseHandler::DATABASE_PATH,
                     RestHandlerCreator<RestDatabaseHandler>::createNoData);

  f.addPrefixHandler(RestVocbaseBaseHandler::DOCUMENT_PATH,
                     RestHandlerCreator<RestDocumentHandler>::createNoData);

  f.addPrefixHandler(RestVocbaseBaseHandler::EDGES_PATH,
                     RestHandlerCreator<RestEdgesHandler>::createNoData);

  f.addPrefixHandler(RestVocbaseBaseHandler::GHARIAL_PATH,
                     RestHandlerCreator<RestGraphHandler>::createNoData);

  f.addPrefixHandler(RestVocbaseBaseHandler::ENDPOINT_PATH,
                     RestHandlerCreator<RestEndpointHandler>::createNoData);

  f.addPrefixHandler(RestVocbaseBaseHandler::IMPORT_PATH,
                     RestHandlerCreator<RestImportHandler>::createNoData);

  f.addPrefixHandler(RestVocbaseBaseHandler::INDEX_PATH,
                     RestHandlerCreator<RestIndexHandler>::createNoData);

  f.addPrefixHandler(RestVocbaseBaseHandler::SIMPLE_QUERY_ALL_PATH,
                     RestHandlerCreator<RestSimpleQueryHandler>::createData<
                         aql::QueryRegistry*>,
                     queryRegistry);

  f.addPrefixHandler(RestVocbaseBaseHandler::SIMPLE_QUERY_ALL_KEYS_PATH,
                     RestHandlerCreator<RestSimpleQueryHandler>::createData<
                         aql::QueryRegistry*>,
                     queryRegistry);

  f.addPrefixHandler(RestVocbaseBaseHandler::SIMPLE_QUERY_BY_EXAMPLE,
                     RestHandlerCreator<RestSimpleQueryHandler>::createData<
                         aql::QueryRegistry*>,
                     queryRegistry);

  f.addPrefixHandler(
      RestVocbaseBaseHandler::SIMPLE_LOOKUP_PATH,
      RestHandlerCreator<RestSimpleHandler>::createData<aql::QueryRegistry*>,
      queryRegistry);

  f.addPrefixHandler(
      RestVocbaseBaseHandler::SIMPLE_REMOVE_PATH,
      RestHandlerCreator<RestSimpleHandler>::createData<aql::QueryRegistry*>,
      queryRegistry);

  if (server().isEnabled<V8DealerFeature>()) {
    // the tasks feature depends on V8. only enable it if JavaScript is enabled
    f.addPrefixHandler(RestVocbaseBaseHandler::TASKS_PATH,
                       RestHandlerCreator<RestTasksHandler>::createNoData);
  }

  f.addPrefixHandler(RestVocbaseBaseHandler::UPLOAD_PATH,
                     RestHandlerCreator<RestUploadHandler>::createNoData);

  f.addPrefixHandler(RestVocbaseBaseHandler::USERS_PATH,
                     RestHandlerCreator<RestUsersHandler>::createNoData);

  f.addPrefixHandler(RestVocbaseBaseHandler::VIEW_PATH,
                     RestHandlerCreator<RestViewHandler>::createNoData);

  if (::arangodb::replication2::EnableReplication2 && cluster.isEnabled()) {
    f.addPrefixHandler(std::string{StaticStrings::ApiLogExternal},
                       RestHandlerCreator<RestLogHandler>::createNoData);
    f.addPrefixHandler(
        std::string{StaticStrings::ApiLogInternal},
        RestHandlerCreator<RestLogInternalHandler>::createNoData);
    f.addPrefixHandler(
        std::string{StaticStrings::ApiReplicatedStateExternal},
        RestHandlerCreator<RestReplicatedStateHandler>::createNoData);
    f.addPrefixHandler(
        "/_api/prototype-state",
        RestHandlerCreator<RestPrototypeStateHandler>::createNoData);
  }

  // This is the only handler were we need to inject
  // more than one data object. So we created the combinedRegistries
  // for it.
  f.addPrefixHandler(
      "/_api/aql",
      RestHandlerCreator<aql::RestAqlHandler>::createData<aql::QueryRegistry*>,
      queryRegistry);

  f.addPrefixHandler("/_api/aql-builtin",
                     RestHandlerCreator<RestAqlFunctionsHandler>::createNoData);

  if (server().isEnabled<V8DealerFeature>()) {
    // the AQL UDfs feature depends on V8. only enable it if JavaScript is
    // enabled
    f.addPrefixHandler(
        "/_api/aqlfunction",
        RestHandlerCreator<RestAqlUserFunctionsHandler>::createNoData);
  }

  f.addPrefixHandler("/_api/explain",
                     RestHandlerCreator<RestExplainHandler>::createNoData);

  f.addPrefixHandler("/_api/query",
                     RestHandlerCreator<RestQueryHandler>::createNoData);

  f.addPrefixHandler("/_api/query-cache",
                     RestHandlerCreator<RestQueryCacheHandler>::createNoData);

  f.addPrefixHandler("/_api/pregel",
                     RestHandlerCreator<RestPregelHandler>::createNoData);

  f.addPrefixHandler("/_api/wal",
                     RestHandlerCreator<RestWalAccessHandler>::createNoData);

  if (agency.isEnabled()) {
    f.addPrefixHandler(
        RestVocbaseBaseHandler::AGENCY_PATH,
        RestHandlerCreator<RestAgencyHandler>::createData<consensus::Agent*>,
        agency.agent());

    f.addPrefixHandler(RestVocbaseBaseHandler::AGENCY_PRIV_PATH,
                       RestHandlerCreator<RestAgencyPrivHandler>::createData<
                           consensus::Agent*>,
                       agency.agent());
  }

  if (cluster.isEnabled()) {
    // add "/agency-callbacks" handler
    f.addPrefixHandler(
        cluster.agencyCallbacksPath(),
        RestHandlerCreator<RestAgencyCallbacksHandler>::createData<
            AgencyCallbackRegistry*>,
        cluster.agencyCallbackRegistry());
    // add "_api/cluster" handler
    f.addPrefixHandler(cluster.clusterRestPath(),
                       RestHandlerCreator<RestClusterHandler>::createNoData);
  }
  f.addPrefixHandler(
      RestVocbaseBaseHandler::INTERNAL_TRAVERSER_PATH,
      RestHandlerCreator<InternalRestTraverserHandler>::createData<
          aql::QueryRegistry*>,
      queryRegistry);

  // And now some handlers which are registered in both /_api and /_admin
  f.addHandler("/_admin/actions",
               RestHandlerCreator<MaintenanceRestHandler>::createNoData);

  f.addHandler("/_admin/auth/reload",
               RestHandlerCreator<RestAuthReloadHandler>::createNoData);

  if (server().hasFeature<V8DealerFeature>() &&
      server().getFeature<V8DealerFeature>().allowAdminExecute()) {
    // the /_admin/execute API depends on V8. only enable it if JavaScript is
    // enabled
    f.addHandler("/_admin/execute",
                 RestHandlerCreator<RestAdminExecuteHandler>::createNoData);
  }

  f.addHandler("/_admin/time",
               RestHandlerCreator<RestTimeHandler>::createNoData);

  f.addHandler("/_admin/compact",
               RestHandlerCreator<RestCompactHandler>::createNoData);

  f.addPrefixHandler("/_api/job",
                     RestHandlerCreator<arangodb::RestJobHandler>::createData<
                         AsyncJobManager*>,
                     _jobManager.get());

  f.addPrefixHandler("/_api/engine",
                     RestHandlerCreator<RestEngineHandler>::createNoData);

  f.addPrefixHandler("/_api/transaction",
                     RestHandlerCreator<RestTransactionHandler>::createNoData);

  f.addPrefixHandler("/_api/ttl",
                     RestHandlerCreator<RestTtlHandler>::createNoData);

  // ...........................................................................
  // /_admin
  // ...........................................................................

  f.addPrefixHandler(
      "/_admin/cluster",
      RestHandlerCreator<arangodb::RestAdminClusterHandler>::createNoData);

  if (_supportInfoApiPolicy != "disabled") {
    f.addHandler("/_admin/support-info",
                 RestHandlerCreator<RestSupportInfoHandler>::createNoData);
  }

  f.addHandler("/_admin/system-report",
               RestHandlerCreator<RestSystemReportHandler>::createNoData);

  f.addPrefixHandler("/_admin/job",
                     RestHandlerCreator<arangodb::RestJobHandler>::createData<
                         AsyncJobManager*>,
                     _jobManager.get());

  // further admin handlers
  f.addPrefixHandler(
      "/_admin/database/target-version",
      RestHandlerCreator<arangodb::RestAdminDatabaseHandler>::createNoData);

  f.addPrefixHandler(
      "/_admin/log",
      RestHandlerCreator<arangodb::RestAdminLogHandler>::createNoData);

  if (server().isEnabled<V8DealerFeature>()) {
    // the routing feature depends on V8. only enable it if JavaScript is
    // enabled
    f.addPrefixHandler(
        "/_admin/routing",
        RestHandlerCreator<arangodb::RestAdminRoutingHandler>::createNoData);
  }

  f.addHandler(
      "/_admin/supervisionState",
      RestHandlerCreator<arangodb::RestSupervisionStateHandler>::createNoData);

  f.addPrefixHandler(
      "/_admin/shutdown",
      RestHandlerCreator<arangodb::RestShutdownHandler>::createNoData);

  if (authentication.isActive()) {
    f.addPrefixHandler(
        "/_open/auth",
        RestHandlerCreator<arangodb::RestAuthHandler>::createNoData);
  }

  f.addPrefixHandler(
      "/_admin/server",
      RestHandlerCreator<arangodb::RestAdminServerHandler>::createNoData);

  f.addHandler(
      "/_admin/statistics",
      RestHandlerCreator<arangodb::RestAdminStatisticsHandler>::createNoData);

  f.addPrefixHandler(
      "/_admin/metrics",
      RestHandlerCreator<arangodb::RestMetricsHandler>::createNoData);

  f.addHandler(
      "/_admin/statistics-description",
      RestHandlerCreator<arangodb::RestAdminStatisticsHandler>::createNoData);

  f.addPrefixHandler(
      "/_admin/license",
      RestHandlerCreator<arangodb::RestLicenseHandler>::createNoData);

#ifdef USE_ENTERPRISE
  HotBackupFeature& backup = server().getFeature<HotBackupFeature>();
  if (backup.isAPIEnabled()) {
    f.addPrefixHandler(
        "/_admin/backup",
        RestHandlerCreator<arangodb::RestHotBackupHandler>::createNoData);
  }
#endif

  // ...........................................................................
  // test handler
  // ...........................................................................
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  f.addPrefixHandler("/_api/test",
                     RestHandlerCreator<RestTestHandler>::createNoData);
#endif

  // ...........................................................................
  // actions defined in v8
  // ...........................................................................

  f.addPrefixHandler("/", RestHandlerCreator<RestActionHandler>::createNoData);

  // engine specific handlers
  StorageEngine& engine = server().getFeature<EngineSelectorFeature>().engine();
  engine.addRestHandlers(f);
}

}  // namespace arangodb
