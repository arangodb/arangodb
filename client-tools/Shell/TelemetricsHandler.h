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
#include <velocypack/Builder.h>

#include <condition_variable>
#include <mutex>
#include <string_view>
#include <thread>

namespace arangodb {

namespace httpclient {
class SimpleHttpClient;
class SimpleHttpResult;
}  // namespace httpclient

class TelemetricsHandler {
 public:
  TelemetricsHandler(ArangoshServer& server, bool sendToEndpoint);

  ~TelemetricsHandler();

  void runTelemetrics();

  void beginShutdown();

  void getTelemetricsInfo(VPackBuilder& builder);

  std::optional<VPackBuilder> sendTelemetricsToEndpoint(
      std::string const& reqUrl = kOriginalUrl);

 private:
  static constexpr char kOriginalUrl[] =
      "https://europe-west3-telemetrics-project.cloudfunctions.net/"
      "telemetrics-cf-3";

  Result checkHttpResponse(
      std::unique_ptr<httpclient::SimpleHttpResult> const& response);
  void fetchTelemetricsFromServer();

  void arrangeTelemetrics();

  ArangoshServer& _server;
  std::mutex _mtx;
  std::condition_variable _runCondition;
  std::condition_variable _runCondition2;
  std::thread _telemetricsThread;
  std::unique_ptr<httpclient::SimpleHttpClient> _httpClient;
  Result _telemetricsFetchResponse;
  VPackBuilder _telemetricsFetchedInfo;
  bool _sendToEndpoint;
};

}  // namespace arangodb
