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

#include <optional>

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
    std::unique_ptr<httpclient::SimpleHttpResult> const& response) const {
  using basics::StringUtils::concatT;
  using basics::StringUtils::itoa;
  if (response == nullptr || !response->isComplete()) {
    return {TRI_ERROR_INTERNAL, this->_httpClient->getErrorMessage()};
  }
  if (response->wasHttpError()) {
    auto errorNum = TRI_ERROR_INTERNAL;
    std::string errorMsg = response->getHttpReturnMessage();
    // Handle case of no body:
    try {
      auto bodyBuilder = response->getBodyVelocyPack();
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
    _telemetricsFetchedInfo.clear();
    _httpClient = clientManager.getConnectedClient(true, false, false, 0);
    if (_httpClient != nullptr && _httpClient->isConnected()) {
      auto& clientParams = _httpClient->params();
      clientParams.setRequestTimeout(30);
      lk.unlock();
      std::unique_ptr<httpclient::SimpleHttpResult> response(
          _httpClient->request(rest::RequestType::GET, url, nullptr, 0));
      lk.lock();

      result = this->checkHttpResponse(response);
      _telemetricsFetchResponse = result;
      if (auto errorNum = result.errorNumber();
          errorNum == TRI_ERROR_NO_ERROR ||
          errorNum == TRI_ERROR_HTTP_FORBIDDEN) {
        _telemetricsFetchedInfo.add(response->getBodyVelocyPack()->slice());
        break;
      }
    }
    _runCondition.wait_for(lk, std::chrono::seconds(timeoutInSecs),
                           [this] { return _server.isStopping(); });
    timeoutInSecs *= 2;
    timeoutInSecs = std::min<uint32_t>(100, timeoutInSecs);
  }
}

// Sends telemetrics to the endpoint and tests the redirection when telemetrics
// is sent to an endpoint, though not exactly the same thing, because here the
// endpoint is local, hence the same client is used and a new endpoint is not
// created.
VPackBuilder TelemetricsHandler::sendTelemetricsToEndpoint(
    std::string const& reqUrl) {
  VPackBuilder responseBuilder;

  // in the first moment, the url we send the request to is reqUrl, then it can
  // change if there's redirection
  std::string url = reqUrl;

  Result result = {TRI_ERROR_NO_ERROR};
  uint32_t timeoutInSecs = 10;
  uint64_t sslProtocol = TLS_V13;
  // max number of new urls we accept to redirect the request to, otherwise, we
  // either try to send the request again to reqUrl or stop trying
  uint32_t maxRedirects = 5;
  uint32_t numRedirects = 0;

  while (!_server.isStopping()) {
    bool isLocalUrl = url.starts_with('/');
    std::unique_lock lk(_mtx);
    std::string const body = _telemetricsFetchedInfo.toJson();
    lk.unlock();
    std::unordered_map<std::string, std::string> headers;
    headers.emplace(StaticStrings::ContentTypeHeader,
                    StaticStrings::MimeTypeJson);
    std::string relativeUrl;
    basics::StringBuffer compressedBody;
    auto res =
        encoding::gzipCompress(reinterpret_cast<uint8_t const*>(body.data()),
                               body.size(), compressedBody);
    if (res != TRI_ERROR_NO_ERROR) {
      // no need to retry if the body is never gonna be compressed successfully
      break;
    }
    headers.emplace(StaticStrings::ContentLength,
                    std::to_string(compressedBody.size()));
    headers.emplace(StaticStrings::ContentEncoding,
                    StaticStrings::EncodingGzip);
    headers.emplace("arangodb-request-type", "telemetrics");
    if (isLocalUrl) {
      // if the url is local, just get the connected client, and the url is
      // already relative
      relativeUrl = url;
      ClientManager clientManager(
          _server.getFeature<HttpEndpointProvider, ClientFeature>(),
          Logger::FIXME);
      lk.lock();
      _httpClient = clientManager.getConnectedClient(true, false, false, 0);
    } else {
      // the url is not local, so we create a connection
      std::string lastEndpoint = getEndpointFromUrl(url);
      std::vector<std::string> endpoints;
      auto [endpoint, relative, error] =
          getEndpoint(nullptr, endpoints, url, lastEndpoint);
      relativeUrl = std::move(relative);
      ClientFeature& client =
          _server.getFeature<HttpEndpointProvider, ClientFeature>();
      std::unique_ptr<Endpoint> newEndpoint(Endpoint::clientFactory(endpoint));

      lk.lock();

      if (newEndpoint.get() != nullptr) {
        std::unique_ptr<httpclient::GeneralClientConnection> connection(
            httpclient::GeneralClientConnection::factory(
                client.getCommFeaturePhase(), newEndpoint, 30, 60, 3,
                sslProtocol));

        if (connection != nullptr) {
          httpclient::SimpleHttpClientParams const params(30, false);
          _httpClient = std::make_unique<httpclient::SimpleHttpClient>(
              connection, params);
        }
      }
    }
    if (_httpClient != nullptr) {
      lk.unlock();
      std::unique_ptr<httpclient::SimpleHttpResult> response;
      response.reset(_httpClient->request(rest::RequestType::POST, relativeUrl,
                                          compressedBody.data(),
                                          compressedBody.size(), headers));
      lk.lock();
      result = this->checkHttpResponse(response);
      if (result.ok()) {
        auto returnCode = response->getHttpReturnCode();
        if (returnCode == 200) {
          responseBuilder.add(response->getBodyVelocyPack()->slice());
          break;
        } else if (returnCode == 301 || returnCode == 302 ||
                   returnCode == 307) {
          bool foundUrl = false;
          if (numRedirects < maxRedirects) {
            url = response->getHeaderField(StaticStrings::Location, foundUrl);
          }
          if (foundUrl) {
            numRedirects++;
          } else {
            // if hasn't found the new url from the redirection, there's no use
            // in trying to redirect again, so will retry to send the request to
            // the original url first
            url = reqUrl;
            numRedirects = 0;
          }
        }
      } else if (result.errorNumber() == TRI_ERROR_INTERNAL) {
        // if is an internal error, will retry to send the request to the server
        numRedirects = 0;
      } else {
        // do not retry if 401, 403, 404, etc.
        break;
      }
      if (numRedirects == 0) {
        timeoutInSecs *= 3;
        timeoutInSecs = std::min<uint32_t>(600, timeoutInSecs);
        _runCondition.wait_for(lk, std::chrono::seconds(timeoutInSecs),
                               [this] { return _server.isStopping(); });
      }
      _httpClient.reset(nullptr);
    }
  }
  return responseBuilder;
}

void TelemetricsHandler::getTelemetricsInfo(VPackBuilder& builder) {
  // as this is for printing the telemetrics result in the tests, we send the
  // telemetrics object if the request returned ok and the error if not to
  // parse the error in the tests
  std::unique_lock lk(_mtx);

  if (_telemetricsFetchResponse.ok() && !_telemetricsFetchedInfo.isEmpty()) {
    builder.add(_telemetricsFetchedInfo.slice());
  } else if (_telemetricsFetchResponse.fail()) {
    builder.openObject();
    builder.add(StaticStrings::ErrorNum,
                VPackValue(_telemetricsFetchResponse.errorNumber()));
    builder.add(StaticStrings::ErrorMessage,
                VPackValue(_telemetricsFetchResponse.errorMessage()));
    builder.close();
  }
}

void TelemetricsHandler::arrangeTelemetrics() {
  try {
    this->fetchTelemetricsFromServer();

    std::unique_lock lk(_mtx);
    if (_telemetricsFetchResponse.ok() && !_telemetricsFetchedInfo.isEmpty()) {
      lk.unlock();
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
        << "Exception on handling telemetrics: " << err.what();
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
