////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Julia Puget
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/Result.h"
#include "Shell/arangosh.h"

#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>

namespace arangodb {

namespace httpclient {
class SimpleHttpClient;
class SimpleHttpResult;
}  // namespace httpclient

namespace velocypack {
class Builder;
}

class TelemetricsHandler {
 public:
  TelemetricsHandler(ArangoshServer& server, bool sendToEndpoint);

  ~TelemetricsHandler();

  void runTelemetrics();

  void beginShutdown();

  void getTelemetricsInfo(velocypack::Builder& builder);

  velocypack::Builder sendTelemetricsToEndpoint(std::string const& reqUrl);

 private:
  Result checkHttpResponse(
      std::unique_ptr<httpclient::SimpleHttpResult> const& response) const;

  void fetchTelemetricsFromServer();

  void arrangeTelemetrics();

  std::pair<std::string, std::unique_ptr<httpclient::SimpleHttpClient>>
  buildHttpClient(std::string& url) const;

  std::string getFetchedInfo() const;

  ArangoshServer& _server;

  std::mutex mutable _mtx;
  // the following members are all protected by _mtx
  std::condition_variable _runCondition;
  std::unique_ptr<httpclient::SimpleHttpClient> _httpClient;
  Result _telemetricsFetchResponse;
  velocypack::Builder _telemetricsFetchedInfo;

  std::thread _telemetricsThread;
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  bool const _sendToEndpoint;
#endif
};

}  // namespace arangodb
