////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

#include "ApplicationFeatures/ApplicationServer.h"
#include "Actions/RestActionHandler.h"
#include "Agency/AgencyFeature.h"
#include "Agency/RestAgencyHandler.h"
#include "Agency/RestAgencyPrivHandler.h"
#include "ApplicationFeatures/HttpEndpointProvider.h"
#include "Aql/RestAqlHandler.h"
#include "RestHandler/RestCrashHandler.h"
#include "SystemMonitor/AsyncRegistry/RestHandler.h"
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
#include "Metrics/CounterBuilder.h"
#include "Metrics/HistogramBuilder.h"
#include "Metrics/MetricsFeature.h"
#include "Network/NetworkFeature.h"
#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "Rest/HttpResponse.h"
#include "RestHandler/RestAdminClusterHandler.h"
#include "RestHandler/RestAdminDatabaseHandler.h"
#include "RestHandler/RestAdminDeploymentHandler.h"
#ifdef USE_V8
#include "RestHandler/RestAdminExecuteHandler.h"
#endif
#include "RestHandler/RestAdminLogHandler.h"
#ifdef USE_V8
#include "RestHandler/RestAdminRoutingHandler.h"
#endif
#include "RestHandler/RestAdminServerHandler.h"
#include "RestHandler/RestAdminStatisticsHandler.h"
#include "RestHandler/RestAnalyzerHandler.h"
#include "RestHandler/RestAqlFunctionsHandler.h"
#ifdef USE_V8
#include "RestHandler/RestAqlUserFunctionsHandler.h"
#endif
#include "RestHandler/RestAccessTokenHandler.h"
#include "RestHandler/RestAuthHandler.h"
#include "RestHandler/RestAuthReloadHandler.h"
#include "RestHandler/RestCompactHandler.h"
#include "RestHandler/RestCursorHandler.h"
#include "RestHandler/RestDatabaseHandler.h"
#include "RestHandler/RestDebugHandler.h"
#include "RestHandler/RestDocumentHandler.h"
#include "RestHandler/RestDocumentStateHandler.h"
#include "RestHandler/RestDumpHandler.h"
#include "RestHandler/RestEdgesHandler.h"
#include "RestHandler/RestEndpointHandler.h"
#include "RestHandler/RestEngineHandler.h"
#include "RestHandler/RestExplainHandler.h"
#include "RestHandler/RestGraphHandler.h"
#include "RestHandler/RestHandlerCreator.h"
#include "RestHandler/RestImportHandler.h"
#include "RestHandler/RestIndexHandler.h"
#include "RestHandler/RestKeyGeneratorsHandler.h"
#include "RestHandler/RestJobHandler.h"
#include "RestHandler/RestLicenseHandler.h"
#include "RestHandler/RestLogHandler.h"
#include "RestHandler/RestLogInternalHandler.h"
#include "RestHandler/RestMetricsHandler.h"
#include "RestHandler/RestOptionsDescriptionHandler.h"
#include "RestHandler/RestOptionsHandler.h"
#include "RestHandler/RestPublicOptionsHandler.h"
#include "RestHandler/RestQueryCacheHandler.h"
#include "RestHandler/RestQueryPlanCacheHandler.h"
#include "RestHandler/RestQueryHandler.h"
#include "RestHandler/RestShutdownHandler.h"
#include "RestHandler/RestSimpleHandler.h"
#include "RestHandler/RestSimpleQueryHandler.h"
#include "RestHandler/RestStatusHandler.h"
#include "RestHandler/RestSupervisionStateHandler.h"
#include "RestHandler/RestTelemetricsHandler.h"
#include "RestHandler/RestSupportInfoHandler.h"
#include "RestHandler/RestSystemReportHandler.h"
#include "RestHandler/RestTasksHandler.h"
#include "RestHandler/RestTestHandler.h"
#include "RestHandler/RestTimeHandler.h"
#include "RestHandler/RestTransactionHandler.h"
#include "RestHandler/RestTtlHandler.h"
#include "RestHandler/RestUploadHandler.h"
#include "RestHandler/RestUsageMetricsHandler.h"
#include "RestHandler/RestUsersHandler.h"
#include "RestHandler/RestVersionHandler.h"
#include "RestHandler/RestOpenApiHandler.h"
#include "RestHandler/RestViewHandler.h"
#include "RestHandler/RestWalAccessHandler.h"
#include "RestServer/EndpointFeature.h"
#include "Metrics/HistogramBuilder.h"
#include "Metrics/CounterBuilder.h"
#include "Metrics/GaugeBuilder.h"
#include "Metrics/MetricsFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/UpgradeFeature.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "SystemMonitor/AsyncRegistry/RestHandler.h"
#include "SystemMonitor/Activities/RestHandler.h"
#ifdef USE_V8
#include "V8Server/V8DealerFeature.h"
#endif

#ifdef USE_ENTERPRISE
#include "Enterprise/RestHandler/RestHotBackupHandler.h"
#include "Enterprise/StorageEngine/HotBackupFeature.h"
#endif

#include <chrono>
#include <stdexcept>
#include <thread>

using namespace arangodb::rest;
using namespace arangodb::options;

namespace arangodb {

struct RequestBodySizeScale {
  static metrics::LogScale<uint64_t> scale() { return {2, 64, 65536, 10}; }
};

DECLARE_HISTOGRAM(arangodb_request_body_size_http1, RequestBodySizeScale,
                  "Body size of HTTP/1.1 requests");
DECLARE_HISTOGRAM(arangodb_request_body_size_http2, RequestBodySizeScale,
                  "Body size of HTTP/2 requests");
DECLARE_COUNTER(arangodb_http1_connections_total,
                "Total number of HTTP/1.1 connections");
DECLARE_COUNTER(arangodb_http2_connections_total,
                "Total number of HTTP/2 connections");
DECLARE_GAUGE(arangodb_requests_memory_usage, std::uint64_t,
              "Memory consumed by incoming requests");

GeneralServerFeature::GeneralServerFeature(
    application_features::ApplicationServer& server,
    metrics::MetricsFeature& metrics)
    : ApplicationFeature{server, *this},
      _currentRequestsSize(server.getFeature<metrics::MetricsFeature>().add(
          arangodb_requests_memory_usage{})),
      _requestBodySizeHttp1(metrics.add(arangodb_request_body_size_http1{})),
      _requestBodySizeHttp2(metrics.add(arangodb_request_body_size_http2{})),
      _http1Connections(metrics.add(arangodb_http1_connections_total{})),
      _http2Connections(metrics.add(arangodb_http2_connections_total{})) {
  setOptional(true);
  startsAfter<application_features::AqlFeaturePhase>();

  startsAfter<HttpEndpointProvider>();
  startsAfter<SslServerFeature>();
  startsAfter<SchedulerFeature>();
  startsAfter<UpgradeFeature>();
}

void GeneralServerFeature::collectOptions(
    std::shared_ptr<ProgramOptions> options) {
  options->addOldOption("server.allow-method-override",
                        "http.allow-method-override");
  options->addOldOption("server.hide-product-header",
                        "http.hide-product-header");
  options->addOldOption("server.keep-alive-timeout", "http.keep-alive-timeout");
  options->addOldOption("no-server", "server.rest-server");

  options
      ->addOption("--server.telemetrics-api",
                  "Whether to enable the telemetrics API.",
                  new options::BooleanParameter(&_options.enableTelemetrics),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::Uncommon,
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnCoordinator,
                      arangodb::options::Flags::OnDBServer,
                      arangodb::options::Flags::OnSingle))
      .setIntroducedIn(31100);

  options
      ->addOption("--server.telemetrics-api-max-requests",
                  "The maximum number of requests from arangosh that the "
                  "telemetrics API responds to without rate-limiting.",
                  new options::UInt64Parameter(
                      &_options.telemetricsMaxRequestsPerInterval),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::Uncommon,
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnCoordinator,
                      arangodb::options::Flags::OnSingle))
      .setIntroducedIn(31100)
      .setLongDescription(R"(This option limits requests from the arangosh to
the telemetrics API, but not any other requests to the API.

Requests to the telemetrics API are counted for every 2 hour interval, and then
reset. This means after a period of at most 2 hours, the telemetrics API
becomes usable again.

The purpose of this option is to keep a deployment from being overwhelmed by
too many telemetrics requests issued by arangosh instances that are used for
batch processing.)");

  options->addOption(
      "--server.io-threads", "The number of threads used to handle I/O.",
      new UInt64Parameter(&_options.numIoThreads, /*base*/ 1, /*minValue*/ 1),
      arangodb::options::makeDefaultFlags(arangodb::options::Flags::Dynamic));

  options
      ->addOption("--server.support-info-api",
                  "The policy for exposing the support info and also the "
                  "telemetrics API.",
                  new DiscreteValuesParameter<StringParameter>(
                      &_options.supportInfoApiPolicy,
                      std::unordered_set<std::string>{"disabled", "jwt",
                                                      "admin", "public"}))
      .setIntroducedIn(30900);

  options
      ->addOption("--server.options-api",
                  "The policy for exposing the options API.",
                  new DiscreteValuesParameter<StringParameter>(
                      &_options.optionsApiPolicy,
                      std::unordered_set<std::string>{"disabled", "jwt",
                                                      "admin", "public"}))
      .setIntroducedIn(31200);

  options->addSection("http", "HTTP server features");

  // option was deprecated in 3.8 and removed in 3.12.
  options->addObsoleteOption(
      "--http.allow-method-override",
      "Allow HTTP method override using special headers.", true);

  options
      ->addOption("--http.keep-alive-timeout",
                  "The keep-alive timeout for HTTP connections (in seconds).",
                  new DoubleParameter(&_options.keepAliveTimeout))
      .setLongDescription(R"(Idle keep-alive connections are closed by the
server automatically when the timeout is reached. A keep-alive-timeout value of
`0` disables the keep-alive feature entirely.)");

  // option was deprecated in 3.8 and removed in 3.12.
  options->addObsoleteOption(
      "--http.hide-product-header",
      "Whether to omit the `Server: ArangoDB` header in HTTP responses.", true);

  options->addOption(
      "--http.trusted-origin",
      "The trusted origin URLs for CORS requests with credentials.",
      new VectorParameter<StringParameter>(
          &_options.accessControlAllowOrigins));

  options->addOption("--http.redirect-root-to", "Redirect of the root URL.",
                     new StringParameter(&_options.redirectRootTo));

  options->addOption("--http.permanently-redirect-root",
                     "Whether to use a permanent or temporary redirect.",
                     new BooleanParameter(&_options.permanentRootRedirect));

  options
      ->addOption("--http.return-queue-time-header",
                  "Whether to return the `x-arango-queue-time-seconds` header "
                  "in all responses.",
                  new BooleanParameter(&_options.returnQueueTimeHeader))
      .setIntroducedIn(30900)
      .setLongDescription(R"(The value contained in this header indicates the
current queueing/dequeuing time for requests in the scheduler (in seconds).
Client applications and drivers can use this value to control the server load
and also react on overload.)");

  options
      ->addOption("--http.compress-response-threshold",
                  "The HTTP response body size from which on responses are "
                  "transparently compressed in case the client asks for it.",
                  new UInt64Parameter(&_options.compressResponseThreshold))
      .setIntroducedIn(31200)
      .setLongDescription(
          R"(Automatically compress outgoing HTTP responses with the
deflate or gzip compression format, in case the client request advertises
support for this. Compression will only happen for HTTP/1.1 and HTTP/2
connections, if the size of the uncompressed response body exceeds
the threshold value controlled by this startup option,
and if the response body size after compression is less than the original
response body size.
Using the value 0 disables the automatic response compression.")");

  options
      ->addOption("--server.early-connections",
                  "Allow requests to a limited set of APIs early during the "
                  "server startup.",
                  new BooleanParameter(&_options.allowEarlyConnections))
      .setIntroducedIn(31000);

  options->addOldOption("frontend.proxy-request-check",
                        "web-interface.proxy-request-check");

  options->addOption("--web-interface.proxy-request-check",
                     "Enable proxy request checking.",
                     new BooleanParameter(&_options.proxyCheck),
                     arangodb::options::makeFlags(
                         arangodb::options::Flags::DefaultNoComponents,
                         arangodb::options::Flags::OnCoordinator,
                         arangodb::options::Flags::OnSingle));

  options->addOldOption("frontend.trusted-proxy",
                        "web-interface.trusted-proxy");

  options->addOption(
      "--web-interface.trusted-proxy",
      "The list of proxies to trust (can be IP or network). Make "
      "sure `--web-interface.proxy-request-check` is enabled.",
      new VectorParameter<StringParameter>(&_options.trustedProxies),
      arangodb::options::makeFlags(
          arangodb::options::Flags::DefaultNoComponents,
          arangodb::options::Flags::OnCoordinator,
          arangodb::options::Flags::OnSingle));

#ifdef ARANGODB_ENABLE_FAILURE_TESTS
  options->addOption(
      "--server.failure-point",
      "The failure point to set during server startup (requires compilation "
      "with failure points support).",
      new VectorParameter<StringParameter>(&_options.failurePoints),
      arangodb::options::makeFlags(arangodb::options::Flags::Default,
                                   arangodb::options::Flags::Uncommon));
#endif

  options
      ->addOption(
          "--http.handle-content-encoding-for-unauthenticated-requests",
          "Handle Content-Encoding headers for unauthenticated requests.",
          new BooleanParameter(
              &_options.handleContentEncodingForUnauthenticatedRequests))
      .setIntroducedIn(31200)
      .setLongDescription(
          R"(If the option is set to `true`, the server will automatically
uncompress incoming HTTP requests with Content-Encodings gzip and deflate
even if the request is not authenticated.)");
}

void GeneralServerFeature::validateOptions(std::shared_ptr<ProgramOptions>) {
  if (!_options.accessControlAllowOrigins.empty()) {
    // trim trailing slash from all members
    for (auto& it : _options.accessControlAllowOrigins) {
      if (it == "*" || it == "all") {
        // special members "*" or "all" means all origins are allowed
        _options.accessControlAllowOrigins.clear();
        _options.accessControlAllowOrigins.push_back("*");
        break;
      } else if (it == "none") {
        // "none" means no origins are allowed
        _options.accessControlAllowOrigins.clear();
        break;
      } else if (it.ends_with('/')) {
        // strip trailing slash
        it = it.substr(0, it.size() - 1);
      }
    }

    // remove empty members
    _options.accessControlAllowOrigins.erase(
        std::remove_if(_options.accessControlAllowOrigins.begin(),
                       _options.accessControlAllowOrigins.end(),
                       [](std::string const& value) {
                         return basics::StringUtils::trim(value).empty();
                       }),
        _options.accessControlAllowOrigins.end());
  }

#ifdef ARANGODB_ENABLE_FAILURE_TESTS
  for (auto const& it : _options.failurePoints) {
    TRI_AddFailurePointDebugging(it);
  }
#endif
}

void GeneralServerFeature::prepare() {
  ServerState::instance()->setServerMode(ServerState::Mode::STARTUP);

  if (ServerState::instance()->isAgent()) {
    // telemetrics automatically and always turned off on agents
    _options.enableTelemetrics = false;
  }

  _jobManager = std::make_unique<AsyncJobManager>();

  // create an initial, very stripped-down RestHandlerFactory.
  // this initial factory only knows a few selected RestHandlers.
  // we will later create another RestHandlerFactory that knows
  // all routes.
  auto hf = std::make_shared<RestHandlerFactory>(ApiVersion::maxApiVersion());
  defineInitialHandlers(*hf);
  // make handler-factory read-only
  hf->seal();

  std::atomic_store(&_handlerFactory, std::move(hf));

  buildServers();

  if (_options.allowEarlyConnections) {
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
  auto hf = std::make_shared<RestHandlerFactory>(ApiVersion::maxApiVersion());

  defineInitialHandlers(*hf);
  defineRemainingHandlers(*hf);
  hf->seal();

  std::atomic_store(&_handlerFactory, std::move(hf));
  TRI_ASSERT(!_options.allowEarlyConnections || _options.startedListening);
  if (!_options.allowEarlyConnections) {
    // if HTTP interface is not open yet, open it now
    startListening();
  }
  TRI_ASSERT(_options.startedListening);

  ServerState::setServerMode(ServerState::Mode::MAINTENANCE);
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
  return _options.keepAliveTimeout;
}

bool GeneralServerFeature::handleContentEncodingForUnauthenticatedRequests()
    const noexcept {
  return _options.handleContentEncodingForUnauthenticatedRequests;
}

bool GeneralServerFeature::proxyCheck() const noexcept {
  return _options.proxyCheck;
}

bool GeneralServerFeature::returnQueueTimeHeader() const noexcept {
  return _options.returnQueueTimeHeader;
}

std::vector<std::string> GeneralServerFeature::trustedProxies() const {
  return _options.trustedProxies;
}

std::vector<std::string> const&
GeneralServerFeature::accessControlAllowOrigins() const {
  return _options.accessControlAllowOrigins;
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
  return _options.permanentRootRedirect;
}

std::string GeneralServerFeature::redirectRootTo() const {
  return _options.redirectRootTo;
}

std::string const& GeneralServerFeature::supportInfoApiPolicy() const noexcept {
  return _options.supportInfoApiPolicy;
}

std::string const& GeneralServerFeature::optionsApiPolicy() const noexcept {
  return _options.optionsApiPolicy;
}

uint64_t GeneralServerFeature::compressResponseThreshold() const noexcept {
  return _options.compressResponseThreshold;
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
      *this, _options.numIoThreads, _options.allowEarlyConnections));
}

void GeneralServerFeature::startListening() {
  TRI_ASSERT(!_options.startedListening);

  EndpointFeature& endpoint =
      server().getFeature<HttpEndpointProvider, EndpointFeature>();
  auto& endpointList = endpoint.endpointList();

  for (auto& server : _servers) {
    server->startListening(endpointList);
  }
  _options.startedListening = true;
}

void GeneralServerFeature::defineInitialHandlers(rest::RestHandlerFactory& f) {
  // these handlers will be available early during the server start.
  // if you want to add more handlers here, please make sure that they
  // run on the CLIENT_FAST request lane. otherwise the incoming requests
  // will still be rejected during startup, even though they are registered
  // here.
  f.addHandler("/_api/version",
               RestHandlerCreator<RestVersionHandler>::createNoData, {0, 1});
  f.addHandler("/_admin/version",
               RestHandlerCreator<RestVersionHandler>::createNoData, {0, 1});
  f.addHandler("/openapi.json",
               RestHandlerCreator<RestOpenApiHandler>::createNoData, {0, 1});
  f.addHandler("/_admin/status",
               RestHandlerCreator<RestStatusHandler>::createNoData, {0, 1});
#ifdef ARANGODB_ENABLE_FAILURE_TESTS
  // This handler can be used to control failure points
  f.addPrefixHandler(
      "/_admin/debug",
      RestHandlerCreator<arangodb::RestDebugHandler>::createNoData, {0, 1});
#endif
}

void GeneralServerFeature::defineRemainingHandlers(
    rest::RestHandlerFactory& f) {
  TRI_ASSERT(_jobManager != nullptr);

  AgencyFeature& agency = server().getFeature<AgencyFeature>();
  ClusterFeature& cluster = server().getFeature<ClusterFeature>();

  // ...........................................................................
  // /_api
  // ...........................................................................

  f.addPrefixHandler(                         // add handler
      RestVocbaseBaseHandler::ANALYZER_PATH,  // base URL
      RestHandlerCreator<
          iresearch::RestAnalyzerHandler>::createNoData,  // handler
      {0, 1});

  auto queryRegistry = QueryRegistryFeature::registry();
  f.addPrefixHandler(
      RestVocbaseBaseHandler::CURSOR_PATH,
      RestHandlerCreator<RestCursorHandler>::createData<aql::QueryRegistry*>,
      {0, 1}, queryRegistry);

  f.addPrefixHandler(RestVocbaseBaseHandler::DATABASE_PATH,
                     RestHandlerCreator<RestDatabaseHandler>::createNoData,
                     {0, 1});

  f.addPrefixHandler(RestVocbaseBaseHandler::DOCUMENT_PATH,
                     RestHandlerCreator<RestDocumentHandler>::createNoData,
                     {0, 1});

  f.addPrefixHandler(RestVocbaseBaseHandler::EDGES_PATH,
                     RestHandlerCreator<RestEdgesHandler>::createNoData,
                     {0, 1});

  f.addPrefixHandler(RestVocbaseBaseHandler::GHARIAL_PATH,
                     RestHandlerCreator<RestGraphHandler>::createNoData,
                     {0, 1});

  f.addPrefixHandler(RestVocbaseBaseHandler::ENDPOINT_PATH,
                     RestHandlerCreator<RestEndpointHandler>::createNoData,
                     {0, 1});

  f.addPrefixHandler(RestVocbaseBaseHandler::IMPORT_PATH,
                     RestHandlerCreator<RestImportHandler>::createNoData,
                     {0, 1});

  f.addPrefixHandler(RestVocbaseBaseHandler::INDEX_PATH,
                     RestHandlerCreator<RestIndexHandler>::createNoData,
                     {0, 1});

  f.addPrefixHandler(RestVocbaseBaseHandler::SIMPLE_QUERY_ALL_PATH,
                     RestHandlerCreator<RestSimpleQueryHandler>::createData<
                         aql::QueryRegistry*>,
                     {0}, queryRegistry);

  f.addPrefixHandler(RestVocbaseBaseHandler::SIMPLE_QUERY_ALL_KEYS_PATH,
                     RestHandlerCreator<RestSimpleQueryHandler>::createData<
                         aql::QueryRegistry*>,
                     {0}, queryRegistry);

  f.addPrefixHandler(RestVocbaseBaseHandler::SIMPLE_QUERY_BY_EXAMPLE,
                     RestHandlerCreator<RestSimpleQueryHandler>::createData<
                         aql::QueryRegistry*>,
                     {0}, queryRegistry);

  f.addPrefixHandler(
      RestVocbaseBaseHandler::SIMPLE_LOOKUP_PATH,
      RestHandlerCreator<RestSimpleHandler>::createData<aql::QueryRegistry*>,
      {0}, queryRegistry);

  f.addPrefixHandler(
      RestVocbaseBaseHandler::SIMPLE_REMOVE_PATH,
      RestHandlerCreator<RestSimpleHandler>::createData<aql::QueryRegistry*>,
      {0}, queryRegistry);

#ifdef USE_V8
  if (server().isEnabled<V8DealerFeature>()) {
    // the tasks feature depends on V8. only enable it if JavaScript is enabled
    f.addPrefixHandler(RestVocbaseBaseHandler::TASKS_PATH,
                       RestHandlerCreator<RestTasksHandler>::createNoData, {0});
  }
#endif

  f.addPrefixHandler(RestVocbaseBaseHandler::UPLOAD_PATH,
                     RestHandlerCreator<RestUploadHandler>::createNoData,
                     {0, 1});

  f.addPrefixHandler(RestVocbaseBaseHandler::USERS_PATH,
                     RestHandlerCreator<RestUsersHandler>::createNoData,
                     {0, 1});

  f.addPrefixHandler(RestVocbaseBaseHandler::ACCESS_TOKEN_PATH,
                     RestHandlerCreator<RestAccessTokenHandler>::createNoData,
                     {0, 1});

  f.addPrefixHandler(RestVocbaseBaseHandler::VIEW_PATH,
                     RestHandlerCreator<RestViewHandler>::createNoData, {0, 1});

  if (::arangodb::replication2::EnableReplication2 && cluster.isEnabled()) {
    f.addPrefixHandler(std::string{StaticStrings::ApiLogExternal},
                       RestHandlerCreator<RestLogHandler>::createNoData,
                       {0, 1});
    f.addPrefixHandler(std::string{StaticStrings::ApiLogInternal},
                       RestHandlerCreator<RestLogInternalHandler>::createNoData,
                       {0, 1});
    f.addPrefixHandler(
        std::string{StaticStrings::ApiDocumentStateExternal},
        RestHandlerCreator<RestDocumentStateHandler>::createNoData, {0, 1});
  }

  // This is the only handler were we need to inject
  // more than one data object. So we created the combinedRegistries
  // for it.
  f.addPrefixHandler(
      "/_api/aql",
      RestHandlerCreator<aql::RestAqlHandler>::createData<aql::QueryRegistry*>,
      {0, 1}, queryRegistry);

  f.addPrefixHandler("/_api/aql-builtin",
                     RestHandlerCreator<RestAqlFunctionsHandler>::createNoData,
                     {0, 1});

#ifdef USE_V8
  if (server().isEnabled<V8DealerFeature>()) {
    // the AQL UDfs feature depends on V8. only enable it if JavaScript is
    // enabled
    f.addPrefixHandler(
        "/_api/aqlfunction",
        RestHandlerCreator<RestAqlUserFunctionsHandler>::createNoData, {0});
  }
#endif

  f.addPrefixHandler(
      "/_api/dump", RestHandlerCreator<arangodb::RestDumpHandler>::createNoData,
      {0, 1});

  f.addPrefixHandler("/_api/explain",
                     RestHandlerCreator<RestExplainHandler>::createNoData,
                     {0, 1});

  f.addPrefixHandler("/_api/key-generators",
                     RestHandlerCreator<RestKeyGeneratorsHandler>::createNoData,
                     {0, 1});

  f.addPrefixHandler("/_api/query",
                     RestHandlerCreator<RestQueryHandler>::createNoData,
                     {0, 1});

  f.addPrefixHandler("/_api/query-cache",
                     RestHandlerCreator<RestQueryCacheHandler>::createNoData,
                     {0, 1});

  f.addPrefixHandler(
      "/_api/query-plan-cache",
      RestHandlerCreator<RestQueryPlanCacheHandler>::createNoData, {0, 1});

  f.addPrefixHandler("/_api/wal",
                     RestHandlerCreator<RestWalAccessHandler>::createNoData,
                     {0, 1});

  if (agency.isEnabled()) {
    f.addPrefixHandler(
        RestVocbaseBaseHandler::AGENCY_PATH,
        RestHandlerCreator<RestAgencyHandler>::createData<consensus::Agent*>,
        {0, 1}, agency.agent());

    f.addPrefixHandler(RestVocbaseBaseHandler::AGENCY_PRIV_PATH,
                       RestHandlerCreator<RestAgencyPrivHandler>::createData<
                           consensus::Agent*>,
                       {0, 1}, agency.agent());
  }

  if (cluster.isEnabled()) {
    // add "/agency-callbacks" handler
    f.addPrefixHandler(
        cluster.agencyCallbacksPath(),
        RestHandlerCreator<RestAgencyCallbacksHandler>::createData<
            AgencyCallbackRegistry*>,
        {0, 1}, cluster.agencyCallbackRegistry());
    // add "_api/cluster" handler
    f.addPrefixHandler(cluster.clusterRestPath(),
                       RestHandlerCreator<RestClusterHandler>::createNoData,
                       {0, 1});
  }
  f.addPrefixHandler(
      RestVocbaseBaseHandler::INTERNAL_TRAVERSER_PATH,
      RestHandlerCreator<InternalRestTraverserHandler>::createData<
          aql::QueryRegistry*>,
      {0, 1}, queryRegistry);

  // And now some handlers which are registered in both /_api and /_admin
  f.addHandler("/_admin/actions",
               RestHandlerCreator<MaintenanceRestHandler>::createNoData,
               {0, 1});

  f.addHandler("/_admin/auth/reload",
               RestHandlerCreator<RestAuthReloadHandler>::createNoData, {0, 1});

#ifdef USE_V8
  if (server().hasFeature<V8DealerFeature>() &&
      server().getFeature<V8DealerFeature>().allowAdminExecute()) {
    // the /_admin/execute API depends on V8. only enable it if JavaScript is
    // enabled
    f.addHandler("/_admin/execute",
                 RestHandlerCreator<RestAdminExecuteHandler>::createNoData,
                 {0});
  }
#endif

  f.addHandler("/_admin/time",
               RestHandlerCreator<RestTimeHandler>::createNoData, {0, 1});

  f.addHandler("/_admin/compact",
               RestHandlerCreator<RestCompactHandler>::createNoData, {0, 1});

  f.addPrefixHandler("/_api/job",
                     RestHandlerCreator<arangodb::RestJobHandler>::createData<
                         AsyncJobManager*>,
                     {0, 1}, _jobManager.get());

  f.addPrefixHandler("/_api/engine",
                     RestHandlerCreator<RestEngineHandler>::createNoData,
                     {0, 1});

  f.addPrefixHandler("/_api/transaction",
                     RestHandlerCreator<RestTransactionHandler>::createNoData,
                     {0, 1});

  f.addPrefixHandler("/_api/ttl",
                     RestHandlerCreator<RestTtlHandler>::createNoData, {0, 1});

  // ...........................................................................
  // /_admin
  // ...........................................................................

  f.addPrefixHandler(
      "/_admin/async-registry",
      RestHandlerCreator<arangodb::async_registry::RestHandler>::createNoData,
      {0, 1});

  f.addPrefixHandler(
      "/_admin/activities",
      RestHandlerCreator<arangodb::activities::RestHandler>::createNoData,
      {ApiVersion::experimentalApiVersion});

  f.addPrefixHandler(
      "/_admin/cluster",
      RestHandlerCreator<arangodb::RestAdminClusterHandler>::createNoData,
      {0, 1});

  f.addPrefixHandler(
      "/_admin/crashes",
      RestHandlerCreator<
          arangodb::crash_handler::RestCrashHandler>::createNoData,
      {0, 1});

  f.addPrefixHandler(
      "/_admin/deployment",
      RestHandlerCreator<arangodb::RestAdminDeploymentHandler>::createNoData,
      {0, 1});

  if (_options.supportInfoApiPolicy != "disabled") {
    f.addHandler("/_admin/support-info",
                 RestHandlerCreator<RestSupportInfoHandler>::createNoData,
                 {0, 1});

    f.addHandler("/_admin/telemetrics",
                 RestHandlerCreator<RestTelemetricsHandler>::createNoData,
                 {0, 1});
  }

  if (_options.optionsApiPolicy != "disabled") {
    f.addHandler("/_admin/options",
                 RestHandlerCreator<RestOptionsHandler>::createNoData, {0, 1});
    f.addHandler(
        "/_admin/options-description",
        RestHandlerCreator<RestOptionsDescriptionHandler>::createNoData,
        {0, 1});
  }

  // Note that this is intentionally visible even if `optionsApiPolicy`
  // is set to 'disabled', since we need the public options API for the
  // platform UI to be always on.
  f.addHandler("/_admin/options-public",
               RestHandlerCreator<RestPublicOptionsHandler>::createNoData,
               {0, 1});

  f.addHandler("/_admin/system-report",
               RestHandlerCreator<RestSystemReportHandler>::createNoData,
               {0, 1});

  f.addPrefixHandler("/_admin/job",
                     RestHandlerCreator<arangodb::RestJobHandler>::createData<
                         AsyncJobManager*>,
                     {0, 1}, _jobManager.get());

  // further admin handlers
  f.addPrefixHandler(
      "/_admin/database/target-version",
      RestHandlerCreator<arangodb::RestAdminDatabaseHandler>::createNoData,
      {0});

  f.addPrefixHandler(
      "/_admin/log",
      RestHandlerCreator<arangodb::RestAdminLogHandler>::createNoData, {0, 1});

#ifdef USE_V8
  if (server().isEnabled<V8DealerFeature>()) {
    // the routing feature depends on V8. only enable it if JavaScript is
    // enabled
    f.addPrefixHandler(
        "/_admin/routing",
        RestHandlerCreator<arangodb::RestAdminRoutingHandler>::createNoData,
        {0});
  }
#endif

  f.addHandler(
      "/_admin/supervisionState",
      RestHandlerCreator<arangodb::RestSupervisionStateHandler>::createNoData,
      {0, 1});

  f.addPrefixHandler(
      "/_admin/shutdown",
      RestHandlerCreator<arangodb::RestShutdownHandler>::createNoData, {0, 1});

  f.addPrefixHandler(
      "/_open/auth",
      RestHandlerCreator<arangodb::RestAuthHandler>::createNoData, {0, 1});

  f.addPrefixHandler(
      "/_admin/server",
      RestHandlerCreator<arangodb::RestAdminServerHandler>::createNoData,
      {0, 1});

  f.addHandler(
      "/_admin/statistics",
      RestHandlerCreator<arangodb::RestAdminStatisticsHandler>::createNoData,
      {0});

  f.addPrefixHandler(
      "/_admin/metrics",
      RestHandlerCreator<arangodb::RestMetricsHandler>::createNoData, {0, 1});

  f.addPrefixHandler(
      "/_admin/usage-metrics",
      RestHandlerCreator<arangodb::RestUsageMetricsHandler>::createNoData,
      {0, 1});

  f.addHandler(
      "/_admin/statistics-description",
      RestHandlerCreator<arangodb::RestAdminStatisticsHandler>::createNoData,
      {0});

  f.addPrefixHandler(
      "/_admin/license",
      RestHandlerCreator<arangodb::RestLicenseHandler>::createNoData, {0, 1});

#ifdef USE_ENTERPRISE
  HotBackupFeature& backup = server().getFeature<HotBackupFeature>();
  if (backup.isAPIEnabled()) {
    f.addPrefixHandler(
        "/_admin/backup",
        RestHandlerCreator<arangodb::RestHotBackupHandler>::createNoData,
        {0, 1});
  }
#endif

  // ...........................................................................
  // test handler
  // ...........................................................................
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  f.addPrefixHandler("/_api/test",
                     RestHandlerCreator<RestTestHandler>::createNoData, {0, 1});
#endif

  // ...........................................................................
  // actions defined in v8
  // ...........................................................................

  // Note that the catchall handler must be added for every API version
  // (including the currently unused experimental one), or else requests to
  // unknown endpoints (Foxx) might run into a crash.
  f.addPrefixHandler("/", RestHandlerCreator<RestActionHandler>::createNoData,
                     {0, 1, ApiVersion::experimentalApiVersion});

  // engine specific handlers
  StorageEngine& engine = server().getFeature<EngineSelectorFeature>().engine();
  engine.addRestHandlers(f);
}

}  // namespace arangodb
