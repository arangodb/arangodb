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
#include "Basics/EncodingUtils.h"
#include "Basics/Exceptions.h"
#include "Basics/StaticStrings.h"
#include "Basics/voc-errors.h"
#include "Endpoint/Endpoint.h"
#include "Logger/LogMacros.h"
#include "Shell/ClientFeature.h"
#include "SimpleHttpClient/GeneralClientConnection.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/SimpleHttpResult.h"
#include "Ssl/ssl-helper.h"
#include "Utils/ClientManager.h"
#include "Rest/GeneralRequest.h"
#include "V8/v8-deadline.h"
#include "V8/v8-utils.h"

#include <velocypack/Slice.h>

using namespace std::chrono;
using namespace std::chrono_literals;

namespace {
static std::string getEndpointFromUrl(std::string const& url) {
  char const* p = url.c_str();
  char const* e = p + url.size();
  size_t slashes = 0;

  while (p < e) {
    if (*p == '?') {
      // http(s)://example.com?foo=bar
      return url.substr(0, p - url.c_str());
    } else if (*p == '/') {
      if (++slashes == 3) {
        return url.substr(0, p - url.c_str());
      }
    }
    ++p;
  }

  return url;
}
}  // namespace

namespace arangodb {
TelemetricsHandler::TelemetricsHandler(ArangoshServer& server,
                                       bool sendToEndpoint)
    : _server(server), _sendToEndpoint(sendToEndpoint) {}

TelemetricsHandler::~TelemetricsHandler() {
  if (_telemetricsThread.joinable()) {
    _telemetricsThread.join();
  }
}

Result TelemetricsHandler::checkHttpResponse(
    std::unique_ptr<httpclient::SimpleHttpResult> const& response,
    bool storeResult) {
  using basics::StringUtils::concatT;
  using basics::StringUtils::itoa;
  if (response == nullptr || !response->isComplete()) {
    return {TRI_ERROR_INTERNAL, this->_httpClient->getErrorMessage()};
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
  if (storeResult) {
    _telemetricsResult.clear();
    _telemetricsResult.add(response->getBodyVelocyPack()->slice());
  }

  return {TRI_ERROR_NO_ERROR};
}

void TelemetricsHandler::fetchTelemetricsFromServer() {
  std::string const url = "/_admin/telemetrics";
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

      result = this->checkHttpResponse(response, true);
      if (auto errorNum = result.errorNumber();
          errorNum == TRI_ERROR_NO_ERROR ||
          errorNum == TRI_ERROR_HTTP_FORBIDDEN) {
        _telemetricsResponse = std::move(result);
        break;
      } else {
        _telemetricsResponse = std::move(result);
      }
    }
    _runCondition.wait_for(lk, std::chrono::seconds(timeoutInSecs),
                           [this] { return _server.isStopping(); });
    timeoutInSecs *= 2;
    timeoutInSecs = std::min<uint32_t>(100, timeoutInSecs);
  }
}

// Tests the redirection when telemetrics is sent to an endpoint, though not
// exactly the same thing, because here the endpoint is local, hence the same
// client is used and a new endpoint is not created.
void TelemetricsHandler::sendTelemetricsToEndpointTestRedirect(
    VPackBuilder& builder) {
  std::string const originalUrl = "/test-redirect/redirect";
  std::string url = originalUrl;
  Result result = {TRI_ERROR_NO_ERROR};
  uint32_t timeoutInSecs = 10;
  uint32_t maxRedirects = 5;

  uint32_t numRedirects = 0;

  while (!_server.isStopping()) {
    ClientManager clientManager(
        _server.getFeature<HttpEndpointProvider, ClientFeature>(),
        Logger::FIXME);
    std::unique_lock lk(_mtx);

    _httpClient = clientManager.getConnectedClient(true, false, false, 0);
    if (_httpClient != nullptr && _httpClient->isConnected()) {
      std::string const body = _telemetricsResult.toJson();
      auto& clientParams = _httpClient->params();
      clientParams.setRequestTimeout(30);
      lk.unlock();

      std::unordered_map<std::string, std::string> headers;
      headers.emplace(StaticStrings::ContentTypeHeader,
                      StaticStrings::MimeTypeJson);
      headers.emplace(StaticStrings::ContentLength,
                      std::to_string(body.size()));

      std::unique_ptr<httpclient::SimpleHttpResult> response(
          _httpClient->request(rest::RequestType::POST, url, body.data(),
                               body.size(), headers));
      lk.lock();
      result = this->checkHttpResponse(response, false);
      if (result.ok()) {
        auto returnCode = response->getHttpReturnCode();
        if (returnCode == 200) {
          builder.add(response->getBodyVelocyPack()->slice());
          break;
        } else if (returnCode == 301 || returnCode == 302 ||
                   returnCode == 307) {
          if (numRedirects < maxRedirects) {
            bool found;
            url = response->getHeaderField(StaticStrings::Location, found);
            if (found) {
              numRedirects++;
            } else {
              url = originalUrl;
              numRedirects = 0;
            }
          } else {
            url = originalUrl;
            numRedirects = 0;
          }
        }
      } else {
        builder.openObject();
        builder.add("errorNum", VPackValue(_telemetricsResponse.errorNumber()));
        builder.add("errorMessage",
                    VPackValue(_telemetricsResponse.errorMessage()));
        builder.close();
        numRedirects = 0;
      }
    }

    if (numRedirects == 0) {
      timeoutInSecs *= 3;
      timeoutInSecs = std::min<uint32_t>(600, timeoutInSecs);
      _runCondition.wait_for(lk, std::chrono::seconds(timeoutInSecs),
                             [this] { return _server.isStopping(); });
    }
  }
}

void TelemetricsHandler::sendTelemetricsToEndpoint() {
  std::string const originalUrl =
      "https://europe-west3-telemetrics-project.cloudfunctions.net/"
      "telemetrics-cf-3";

  std::string url = originalUrl;

  Result result = {TRI_ERROR_NO_ERROR};
  uint32_t timeoutInSecs = 10;
  uint64_t sslProtocol = TLS_V12;
  uint32_t maxRedirects = 5;

  std::string const body = _telemetricsResult.toJson();

  httpclient::SimpleHttpClientParams const params(30, false);
  uint32_t numRedirects = 0;

  while (!_server.isStopping()) {
    std::string lastEndpoint = getEndpointFromUrl(url);
    std::vector<std::string> endpoints;
    auto [endpoint, relative, error] =
        getEndpoint(nullptr, endpoints, url, lastEndpoint);

    basics::StringBuffer compressedBody;

    auto res =
        encoding::gzipCompress(reinterpret_cast<uint8_t const*>(body.data()),
                               body.size(), compressedBody);
    if (res != TRI_ERROR_NO_ERROR) {
      break;
    }
    std::unordered_map<std::string, std::string> headers;
    headers.emplace(StaticStrings::ContentTypeHeader,
                    StaticStrings::MimeTypeJson);
    headers.emplace(StaticStrings::ContentLength,
                    std::to_string(compressedBody.size()));
    headers.emplace(StaticStrings::ContentEncoding,
                    StaticStrings::EncodingGzip);

    ClientFeature& client =
        _server.getFeature<HttpEndpointProvider, ClientFeature>();
    std::unique_ptr<Endpoint> newEndpoint(Endpoint::clientFactory(endpoint));

    std::unique_lock lk(_mtx);

    TRI_ASSERT(newEndpoint.get() != nullptr);
    std::unique_ptr<httpclient::GeneralClientConnection> connection(
        httpclient::GeneralClientConnection::factory(
            client.getCommFeaturePhase(), newEndpoint, 30, 60, 3, sslProtocol));

    TRI_ASSERT(connection != nullptr);

    if (_httpClient != nullptr) {
      _httpClient.reset(nullptr);
    }
    _httpClient = std::make_unique<httpclient::SimpleHttpClient>(
        connection.get(), params);
    TRI_ASSERT(_httpClient != nullptr);

    lk.unlock();

    std::unique_ptr<httpclient::SimpleHttpResult> response(_httpClient->request(
        rest::RequestType::POST, relative, compressedBody.data(),
        compressedBody.size(), headers));
    lk.lock();
    result = this->checkHttpResponse(response, false);
    if (result.ok()) {
      auto returnCode = response->getHttpReturnCode();
      if (returnCode == 200) {
        break;
      } else if (returnCode == 301 || returnCode == 302 || returnCode == 307) {
        if (numRedirects < maxRedirects) {
          bool found;
          url = response->getHeaderField(StaticStrings::Location, found);
          if (found) {
            numRedirects++;
          } else {
            url = originalUrl;
            numRedirects = 0;
          }
        } else {
          url = originalUrl;
          numRedirects = 0;
        }
      }
    } else {
      numRedirects = 0;
    }
    if (numRedirects == 0) {
      timeoutInSecs *= 3;
      timeoutInSecs = std::min<uint32_t>(600, timeoutInSecs);
      _runCondition.wait_for(lk, std::chrono::seconds(timeoutInSecs),
                             [this] { return _server.isStopping(); });
    }
    _httpClient.reset(nullptr);
    connection.reset(nullptr);
  }
}

void TelemetricsHandler::getTelemetricsInfo(VPackBuilder& builder) {
  // as this is for printing the telemetrics result in the tests, we send the
  // telemetrics object if the request returned ok and the error if not to
  // parse the error in the tests
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
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
      if (_sendToEndpoint) {
        this->sendTelemetricsToEndpoint();
      }
#else
      this->sendTelemetricsToEndpoint();
#endif
    }
  } catch (std::exception const& err) {
    LOG_TOPIC("e1b5b", WARN, arangodb::Logger::FIXME)
        << "Exception on handling telemetrics " << err.what();
  }
}

void TelemetricsHandler::runTelemetrics() {
  _telemetricsThread =
      std::thread(&TelemetricsHandler::arrangeTelemetrics, this);
}

void TelemetricsHandler::beginShutdown() {
  std::unique_lock lk(_mtx);
  if (_httpClient != nullptr) {
    _httpClient->setAborted(true);
  }
  _runCondition.notify_one();
}

}  // namespace arangodb
