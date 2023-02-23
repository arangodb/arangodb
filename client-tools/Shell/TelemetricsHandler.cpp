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

#include "TelemetricsHandler.h"
#include "Basics/StaticStrings.h"
#include <velocypack/Slice.h>
#include <chrono>

#include "Logger/LogMacros.h"

namespace arangodb {

Result TelemetricsHandler::checkHttpResponse(
    std::unique_ptr<httpclient::SimpleHttpResult> const& response) {
  using basics::StringUtils::concatT;
  using basics::StringUtils::itoa;
  if (response == nullptr || !response->isComplete()) {
    return {TRI_ERROR_INTERNAL, "got invalid response from server: " +
                                    this->_httpClient->getErrorMessage()};
  }
  if (response->wasHttpError()) {
    auto errorNum = TRI_ERROR_INTERNAL;
    std::string errorMsg = response->getHttpReturnMessage();
    std::shared_ptr<velocypack::Builder> bodyBuilder;
    // Handle case of no body:
    try {
      bodyBuilder = response->getBodyVelocyPack();
      velocypack::Slice error = bodyBuilder->slice();
      if (!error.isNone() && error.hasKey(StaticStrings::ErrorMessage)) {
        errorNum = ErrorCode{
            error.get(StaticStrings::ErrorNum).getNumericValue<int>()};
        errorMsg = error.get(StaticStrings::ErrorMessage).copyString();
        LOG_DEVEL << "error " << errorNum << " " << errorMsg;
      }
    } catch (...) {
    }
    return {errorNum,
            concatT("got invalid response from server: HTTP ",
                    itoa(response->getHttpReturnCode()), ": ", errorMsg)};
  }
  return {TRI_ERROR_NO_ERROR};
}

void TelemetricsHandler::sendTelemetricsRequest() {
  TRI_ASSERT(_httpClient != nullptr);
  std::string const url = "/_admin/server-info";
  Result result = {TRI_ERROR_NO_ERROR};
  do {
    std::unique_ptr<httpclient::SimpleHttpResult> response(
        _httpClient->request(rest::RequestType::GET, url, nullptr, 0));
    result = this->checkHttpResponse(response);
    std::this_thread::sleep_for(std::chrono::seconds(1));
  } while (_runCondition && result.errorNumber() == TRI_ERROR_INTERNAL);
  _telemetricsResponse = std::move(result);
}

void TelemetricsHandler::printTelemetrics() {
  LOG_TOPIC("dcd06", WARN, Logger::STATISTICS) << _telemetricsResponse.toJson();
}

void TelemetricsHandler::arrangeTelemetrics() {
  this->sendTelemetricsRequest();
  if (_printTelemetrics) {
    this->_printTelemetrics();
  }
}

void TelemetricsHandler::runTelemetrics() {
  _telemetricsThread =
      std::thread(&TelemetricsHandler::arrangeTelemetrics, this);
}

}  // namespace arangodb
