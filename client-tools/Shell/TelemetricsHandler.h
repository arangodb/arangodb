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

#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/SimpleHttpResult.h"
#include "Logger/LogMacros.h"

#include <thread>

namespace arangodb {

class TelemetricsHandler {
 public:
  TelemetricsHandler()
      : _printTelemetrics(false), _runCondition(true), _httpClient(nullptr) {}
  ~TelemetricsHandler() {
    if (_telemetricsThread.joinable()) {
      _telemetricsThread.join();
    }
  }

  void runTelemetrics();

  void disableRunCondition() { _runCondition = false; }
  void setHttpClient(
      std::unique_ptr<httpclient::SimpleHttpClient>& httpClient) {
    _httpClient = std::move(httpClient);
  }

  void setPrintTelemetrics(bool printTemeletrics) {
    _printTelemetrics = printTemeletrics;
  }

 private:
  arangodb::Result checkHttpResponse(
      std::unique_ptr<httpclient::SimpleHttpResult> const& response);
  void sendTelemetricsRequest();
  void SendTelemetricsToEndpoint();
  void arrangeTelemetrics();
  void printTelemetrics();

  bool _printTelemetrics;
  std::atomic<bool> _runCondition;
  std::unique_ptr<httpclient::SimpleHttpClient> _httpClient;
  std::thread _telemetricsThread;
  Result _telemetricsResponse;
  Result _response;
  VPackBuilder _telemetricsResult;
};

}  // namespace arangodb
