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

#include "ApplicationFeatures/HttpEndpointProvider.h"
#include "ApplicationFeatures/V8PlatformFeature.h"
#include "ApplicationFeatures/CommunicationFeaturePhase.h"
#include "Basics/Exceptions.h"
#include "Basics/StaticStrings.h"
#include "Basics/voc-errors.h"
#include "Endpoint/Endpoint.h"
#include "Shell/ClientFeature.h"
#include "SimpleHttpClient/GeneralClientConnection.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/SimpleHttpResult.h"
#include "Ssl/SslInterface.h"
#include "Ssl/ssl-helper.h"
#include "Utils/ClientManager.h"
#include "Rest/GeneralRequest.h"

#include <velocypack/Slice.h>

#include "Logger/LogMacros.h"

#include "V8/v8-buffer.h"
#include "V8/v8-conv.h"
#include "V8/v8-globals.h"
#include "V8/v8-vpack.h"
#include "V8/v8-deadline.h"

namespace arangodb {
TelemetricsHandler::TelemetricsHandler(ArangoshServer& server)
    : _server(server), _printTelemetrics(false) {}

TelemetricsHandler::~TelemetricsHandler() {
  if (_telemetricsThread.joinable()) {
    _telemetricsThread.join();
  }
}

Result TelemetricsHandler::checkHttpResponse(
    std::unique_ptr<httpclient::SimpleHttpResult> const& response) {
  using basics::StringUtils::concatT;
  using basics::StringUtils::itoa;
  // this shouldn't happen
  if (response == nullptr || !response->isComplete()) {
    LOG_DEVEL << "response " << (response == nullptr) << " "
              << response->isComplete();
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
      }
    } catch (...) {
    }
    return {errorNum,
            concatT("got invalid response from server: HTTP ",
                    itoa(response->getHttpReturnCode()), ": ", errorMsg)};
  }
  _telemetricsResult.clear();
  _telemetricsResult.openObject();
  _telemetricsResult.add("telemetrics", response->getBodyVelocyPack()->slice());
  _telemetricsResult.close();

  return {TRI_ERROR_NO_ERROR};
}

void TelemetricsHandler::fetchTelemetricsFromServer() {
  std::string const url = "/_admin/server-info";
  Result result = {TRI_ERROR_NO_ERROR};
  uint32_t timeoutInSecs = 1;
  while (!_server.isStopping()) {
    ClientManager clientManager(
        _server.getFeature<HttpEndpointProvider, ClientFeature>(),
        Logger::FIXME);
    std::unique_lock lk(_mtx);

    _httpClient = clientManager.getConnectedClient(true, false, false, 0);
    if (_httpClient != nullptr && _httpClient->isConnected()) {
      auto& clientParams = _httpClient->params();
      clientParams.setRequestTimeout(30);
      lk.unlock();
      std::unique_ptr<httpclient::SimpleHttpResult> response(
          _httpClient->request(rest::RequestType::GET, url, nullptr, 0));
      lk.lock();
      result = this->checkHttpResponse(response);
      if (result.ok() || result.is(TRI_ERROR_HTTP_FORBIDDEN)) {
        _telemetricsResponse = std::move(result);
        break;
      }
    }
    _runCondition.wait_for(lk, std::chrono::seconds(timeoutInSecs),
                           [this] { return _server.isStopping(); });
    timeoutInSecs *= 2;
    timeoutInSecs = std::min<uint32_t>(100, timeoutInSecs);
  }
}

void TelemetricsHandler::sendTelemetricsToEndpoint() {
  std::string const url =
      "ssl://europe-west3-telemetrics-project.cloudfunctions.net:443";
  std::string const relative = "/telemetrics-cf-3";
  Result result = {TRI_ERROR_NO_ERROR};
  uint32_t timeoutInSecs = 1;
  // uint32_t maxRedirects = 5;
  uint64_t sslProtocol = TLS_V12;
  LOG_DEVEL << "before while" << _telemetricsResult.isClosed();
  std::string const body =
      _telemetricsResult.slice().get("telemetrics").toJson();
  LOG_DEVEL << "body is " << body;
  httpclient::SimpleHttpClientParams const params(30, false);

  while (!_server.isStopping()) {
    LOG_DEVEL << "INSIDE WHILE";
    ClientFeature& client =
        _server.getFeature<HttpEndpointProvider, ClientFeature>();
    std::unique_ptr<Endpoint> newEndpoint(Endpoint::clientFactory(url));

    std::unique_lock lk(_mtx);

    /*
    v8::Isolate* isolate =
    _server.getFeature<V8PlatformFeature>().createIsolate();

    TRI_GET_GLOBALS();

    httpclient::GeneralClientConnection::factory(v8g->_comm,
                                                 newEndpoint, 30, 30,
                                                 */
    TRI_ASSERT(newEndpoint.get() != nullptr);
    //     TRI_GET_GLOBALS();
    std::unique_ptr<httpclient::GeneralClientConnection> connection(
        httpclient::GeneralClientConnection::factory(
            client.getCommFeaturePhase(), newEndpoint, 30, 30, 3, sslProtocol));

    LOG_DEVEL << "created connection";

    TRI_ASSERT(connection != nullptr);

    if (_httpClient != nullptr) {
      LOG_DEVEL << "will reset";
      _httpClient.reset(nullptr);
    }
    _httpClient = std::make_unique<httpclient::SimpleHttpClient>(
        connection.get(), params);
    TRI_ASSERT(_httpClient != nullptr);
    LOG_DEVEL << "new endpoint " << connection->getEndpointSpecification();
    LOG_DEVEL << _httpClient->params().getRequestTimeout();
    LOG_DEVEL << _httpClient->getEndpointSpecification();
    LOG_DEVEL << "INSIDE IF";

    lk.unlock();

    LOG_DEVEL << "after unlock";
    /*
        clientParams.setLocationRewriter(static_cast<void*>(&client), []
    (void* data, std::string const& location) -> std::string { return
    {location};});
                                                                     */

    std::unordered_map<std::string, std::string> headers;
    headers.emplace(StaticStrings::ContentTypeHeader,
                    StaticStrings::MimeTypeJson);

    LOG_DEVEL << "will send request to "
              << _httpClient->getEndpointSpecification();
    auto httpResult = _httpClient->request(rest::RequestType::POST, relative,
                                           body.c_str(), body.size(), headers);
    //  nullptr, 0, headers);
    LOG_DEVEL << "response body " << httpResult->getBody();

    std::unique_ptr<httpclient::SimpleHttpResult> response(httpResult);
    lk.lock();
    if (!response->wasHttpError()) {
      LOG_DEVEL << "was not http error";
      break;
    } else {
      LOG_DEVEL << "was http error response "
                << response->getHttpReturnMessage();
    }

    /*
    try {
      LOG_DEVEL << "response " << httpResult->isComplete() << " " <<
    httpResult->getHttpReturnMessage() << " " << httpResult->getHttpReturnCode()
    << " " << httpResult->getBody();
      std::unique_ptr<httpclient::SimpleHttpResult> response(httpResult);

      lk.lock();
      result = this->checkHttpResponse(response);

      LOG_DEVEL << "result " << result.errorMessage();
      if (!response->wasHttpError()) {
        break;
      }
    } catch (std::exception const& err) {
      LOG_DEVEL << "CATCH";
      LOG_DEVEL << err.what();
    }
    */

    _runCondition.wait_for(lk, std::chrono::seconds(timeoutInSecs),
                           [this] { return _server.isStopping(); });
    timeoutInSecs *= 2;
    timeoutInSecs = std::min<uint32_t>(100, timeoutInSecs);
    LOG_DEVEL << "increased timeout to " << timeoutInSecs;
    connection.reset(nullptr);
  }
}

void TelemetricsHandler::getTelemetricsInfo(VPackBuilder& builder) {
  // as this is for printing the telemetrics result in the tests, we send the
  // telemetrics object if the request returned ok and the error if not to parse
  // the error in the tests
  std::unique_lock lk(_mtx);
  if (_telemetricsResponse.ok()) {
    builder.add(_telemetricsResult.slice());
  } else {
    builder.openObject();
    builder.add("errorNum", VPackValue(_telemetricsResponse.errorNumber()));
    builder.add("errorMessage",
                VPackValue(_telemetricsResponse.errorMessage()));
    builder.close();
  }
}

void TelemetricsHandler::arrangeTelemetrics() {
  try {
    this->fetchTelemetricsFromServer();

    if (_telemetricsResponse.ok()) {
      this->sendTelemetricsToEndpoint();
    }

  } catch (std::exception const& err) {
    LOG_TOPIC("e1b5b", WARN, arangodb::Logger::FIXME)
        << "Exception on fetching telemetrics " << err.what();
  }
}

void TelemetricsHandler::runTelemetrics() {
  _telemetricsThread =
      std::thread(&TelemetricsHandler::arrangeTelemetrics, this);
}

void TelemetricsHandler::beginShutdown() {
  LOG_DEVEL << "begin shutdown";
  std::unique_lock lk(_mtx);
  if (_httpClient != nullptr) {
    _httpClient->setAborted(true);
  }
  _runCondition.notify_one();
  LOG_DEVEL << "notified";
}

}  // namespace arangodb
