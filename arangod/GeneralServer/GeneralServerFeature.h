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

#pragma once

#include "ApplicationFeatures/ApplicationFeature.h"
#include "GeneralServer/AsyncJobManager.h"
#include "GeneralServer/GeneralServer.h"
#include "GeneralServer/RestHandlerFactory.h"
#include "Metrics/Counter.h"
#include "Metrics/LogScale.h"
#include "Metrics/Histogram.h"

namespace arangodb {
class RestServerThread;

class GeneralServerFeature final
    : public application_features::ApplicationFeature {
 public:
  explicit GeneralServerFeature(
      application_features::ApplicationServer& server);

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void prepare() override final;
  void start() override final;
  void initiateSoftShutdown() override final;
  void beginShutdown() override final;
  void stop() override final;
  void unprepare() override final;

  double keepAliveTimeout() const noexcept;
  bool proxyCheck() const noexcept;
  bool returnQueueTimeHeader() const noexcept;
  std::vector<std::string> trustedProxies() const;
  bool allowMethodOverride() const noexcept;
  std::vector<std::string> const& accessControlAllowOrigins() const;
  Result reloadTLS();
  bool permanentRootRedirect() const noexcept;
  std::string redirectRootTo() const;
  std::string const& supportInfoApiPolicy() const noexcept;

  rest::RestHandlerFactory& handlerFactory();
  rest::AsyncJobManager& jobManager();

  void countHttp1Request(uint64_t bodySize) {
    _requestBodySizeHttp1.count(bodySize);
  }

  void countHttp2Request(uint64_t bodySize) {
    _requestBodySizeHttp2.count(bodySize);
  }

  void countVstRequest(uint64_t bodySize) {
    _requestBodySizeVst.count(bodySize);
  }

  void countHttp2Connection() { _http2Connections.count(); }

  void countVstConnection() { _vstConnections.count(); }

 private:
  void buildServers();
  void defineHandlers();

 private:
  double _keepAliveTimeout = 300.0;
  bool _allowMethodOverride;
  bool _proxyCheck;
  bool _returnQueueTimeHeader;
  bool _permanentRootRedirect;
  std::vector<std::string> _trustedProxies;
  std::vector<std::string> _accessControlAllowOrigins;
  std::string _redirectRootTo;
  std::string _supportInfoApiPolicy;
  std::unique_ptr<rest::RestHandlerFactory> _handlerFactory;
  std::unique_ptr<rest::AsyncJobManager> _jobManager;
  std::vector<std::unique_ptr<rest::GeneralServer>> _servers;
  uint64_t _numIoThreads;

  // Some metrics about
  metrics::Histogram<metrics::LogScale<uint64_t>>& _requestBodySizeHttp1;
  metrics::Histogram<metrics::LogScale<uint64_t>>& _requestBodySizeHttp2;
  metrics::Histogram<metrics::LogScale<uint64_t>>& _requestBodySizeVst;
  metrics::Counter& _http2Connections;
  metrics::Counter& _vstConnections;
};

}  // namespace arangodb
