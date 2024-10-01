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

#pragma once

#include "Basics/Result.h"
#include "GeneralServer/AsyncJobManager.h"
#include "GeneralServer/GeneralServer.h"
#include "GeneralServer/RestHandlerFactory.h"
#include "Metrics/Counter.h"
#include "Metrics/LogScale.h"
#include "Metrics/Histogram.h"
#include "Metrics/Gauge.h"
#include "RestServer/arangod.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace arangodb {
class RestServerThread;

class GeneralServerFeature final : public ArangodFeature {
 public:
  static constexpr std::string_view name() noexcept { return "GeneralServer"; }

  explicit GeneralServerFeature(Server& server,
                                metrics::MetricsFeature& metrics);

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void prepare() override final;
  void start() override final;
  void initiateSoftShutdown() override final;
  void beginShutdown() override final;
  void stop() override final;
  void unprepare() override final;

  double keepAliveTimeout() const noexcept;
  bool handleContentEncodingForUnauthenticatedRequests() const noexcept;
  bool proxyCheck() const noexcept;
  bool returnQueueTimeHeader() const noexcept;
  std::vector<std::string> trustedProxies() const;
  std::vector<std::string> const& accessControlAllowOrigins() const;
  Result reloadTLS();
  bool permanentRootRedirect() const noexcept;
  std::string redirectRootTo() const;
  std::string const& supportInfoApiPolicy() const noexcept;
  std::string const& optionsApiPolicy() const noexcept;
  uint64_t compressResponseThreshold() const noexcept;

  std::shared_ptr<rest::RestHandlerFactory> handlerFactory() const;
  rest::AsyncJobManager& jobManager();

  void countHttp1Request(uint64_t bodySize) noexcept {
    _requestBodySizeHttp1.count(bodySize);
  }

  void countHttp2Request(uint64_t bodySize) noexcept {
    _requestBodySizeHttp2.count(bodySize);
  }

  void countHttp1Connection() { _http1Connections.count(); }

  void countHttp2Connection() { _http2Connections.count(); }

  bool isTelemetricsEnabled() const noexcept { return _enableTelemetrics; }
  uint64_t telemetricsMaxRequestsPerInterval() const noexcept {
    return _telemetricsMaxRequestsPerInterval;
  }

  metrics::Gauge<std::uint64_t>& _currentRequestsSize;

 private:
  // build HTTP server(s)
  void buildServers();
  // open REST interface for listening
  void startListening();
  // define initial (minimal) REST handlers
  void defineInitialHandlers(rest::RestHandlerFactory& f);
  // define remaining REST handlers
  void defineRemainingHandlers(rest::RestHandlerFactory& f);

  double _keepAliveTimeout = 300.0;
  uint64_t _telemetricsMaxRequestsPerInterval;
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  bool _startedListening;
#endif
  bool _allowEarlyConnections;
  bool _handleContentEncodingForUnauthenticatedRequests;
  bool _enableTelemetrics;
  bool _proxyCheck;
  bool _returnQueueTimeHeader;
  bool _permanentRootRedirect;
  uint64_t _compressResponseThreshold;
  std::vector<std::string> _trustedProxies;
  std::vector<std::string> _accessControlAllowOrigins;
  std::string _redirectRootTo;
  std::string _supportInfoApiPolicy;
  std::string _optionsApiPolicy;
  std::shared_ptr<rest::RestHandlerFactory> _handlerFactory;
  std::unique_ptr<rest::AsyncJobManager> _jobManager;
  std::vector<std::unique_ptr<rest::GeneralServer>> _servers;
  uint64_t _numIoThreads;

#ifdef ARANGODB_ENABLE_FAILURE_TESTS
  std::vector<std::string> _failurePoints;
#endif

  // Some metrics about requests and connections
  metrics::Histogram<metrics::LogScale<uint64_t>>& _requestBodySizeHttp1;
  metrics::Histogram<metrics::LogScale<uint64_t>>& _requestBodySizeHttp2;
  metrics::Counter& _http1Connections;
  metrics::Counter& _http2Connections;
};

}  // namespace arangodb
