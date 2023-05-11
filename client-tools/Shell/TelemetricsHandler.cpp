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
#include "ApplicationFeatures/CommunicationFeaturePhase.h"
#include "Basics/EncodingUtils.h"
#include "Basics/Exceptions.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Basics/voc-errors.h"
#include "Endpoint/Endpoint.h"
#include "Logger/LogMacros.h"
#include "Rest/Version.h"
#include "Shell/ClientFeature.h"
#include "SimpleHttpClient/GeneralClientConnection.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/SimpleHttpResult.h"
#include "Ssl/ssl-helper.h"
#include "Utils/ClientManager.h"
#include "V8/v8-utils.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

using namespace std::chrono;
using namespace std::chrono_literals;

namespace {
std::string const kTelemetricsGatheringUrl =
    "https://telemetrics.arangodb.com/v1/collect";
}

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
    return {TRI_ERROR_INTERNAL, _httpClient->getErrorMessage()};
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
  return {};
}

void TelemetricsHandler::fetchTelemetricsFromServer() {
  std::string const url = "/_admin/telemetrics";

  std::unordered_map<std::string, std::string> headers = {
      {StaticStrings::UserAgent, std::string("arangosh/") + ARANGODB_VERSION},
      {StaticStrings::AcceptEncoding, "gzip"}};

  uint32_t timeoutInSecs = 1;
  while (!_server.isStopping()) {
    try {
      ClientManager clientManager(
          _server.getFeature<HttpEndpointProvider, ClientFeature>(),
          Logger::FIXME);

      std::unique_lock lk(_mtx);
      _telemetricsFetchedInfo.clear();
      _httpClient = clientManager.getConnectedClient(true, false, false, 0);

      if (_httpClient != nullptr && _httpClient->isConnected()) {
        std::string role;
        Result result;
        std::tie(result, role) =
            clientManager.getArangoIsCluster(*(_httpClient.get()));
        if (result.fail()) {
          LOG_TOPIC("a3146", WARN, arangodb::Logger::FIXME)
              << "Error: could not detect ArangoDB instance type: "
              << result.errorMessage();
        } else if (role != "COORDINATOR" && role != "SINGLE") {
          _sendToEndpoint = false;
        }

        auto& clientParams = _httpClient->params();
        clientParams.setRequestTimeout(30);
        lk.unlock();
        // perform request outside of lock
        std::unique_ptr<httpclient::SimpleHttpResult> response(
            _httpClient->request(rest::RequestType::GET, url, nullptr, 0,
                                 headers));
        lk.lock();

        _telemetricsFetchResponse = checkHttpResponse(response);
        if (_telemetricsFetchResponse.ok() ||
            _telemetricsFetchResponse.is(TRI_ERROR_HTTP_FORBIDDEN) ||
            _telemetricsFetchResponse.is(TRI_ERROR_HTTP_ENHANCE_YOUR_CALM)) {
          _telemetricsFetchedInfo.add(response->getBodyVelocyPack()->slice());
          _httpClient.reset();
          break;
        }
      }
      _runCondition.wait_for(lk, std::chrono::seconds(timeoutInSecs),
                             [this] { return _server.isStopping(); });
      timeoutInSecs *= 2;
      timeoutInSecs = std::min<uint32_t>(100, timeoutInSecs);
    } catch (std::exception const& ex) {
      LOG_TOPIC("e1b5b", WARN, arangodb::Logger::FIXME)
          << "caught exception while fetching telemetrics from server: "
          << ex.what();
      throw;
    }
  }
}

// Sends telemetrics to the endpoint and tests the redirection when telemetrics
// is sent to an endpoint, though not exactly the same thing, because, for
// testing redirection, the endpoint is local, hence the same client is used and
// a new endpoint is not created.
velocypack::Builder TelemetricsHandler::sendTelemetricsToEndpoint(
    std::string const& reqUrl) {
  // in the first moment, the url we send the request to is reqUrl, then it can
  // change if there's redirection
  std::string url = reqUrl;
  velocypack::Builder responseBuilder;

  // compress request body (once and for all)
  basics::StringBuffer compressedBody;

  {
    std::string body = getFetchedInfo();
    auto res =
        encoding::gzipCompress(reinterpret_cast<uint8_t const*>(body.data()),
                               body.size(), compressedBody);
    if (res != TRI_ERROR_NO_ERROR) {
      // no need to retry if the body is never gonna be compressed
      // successfully
      return responseBuilder;
    }
  }

  // build request headers (once and for all)
  std::unordered_map<std::string, std::string> headers;
  headers.emplace(StaticStrings::ContentTypeHeader,
                  StaticStrings::MimeTypeJson);
  headers.emplace(StaticStrings::ContentLength,
                  std::to_string(compressedBody.size()));
  headers.emplace(StaticStrings::ContentEncoding, StaticStrings::EncodingGzip);
  headers.emplace("arangodb-request-type", "telemetrics");

  uint32_t timeoutInSecs = 10;
  // max number of new urls we accept to redirect the request to, otherwise, we
  // either try to send the request again to reqUrl or stop trying
  uint32_t maxRedirects = 5;
  uint32_t numRedirects = 0;

  while (!_server.isStopping()) {
    try {
      // potential side-effect: buildHttpClient can modify url
      auto [relativeUrl, httpClient] = buildHttpClient(url);

      // from here on we sporadically use the mutex to protect access to
      // _httpClient and _runCondition
      std::unique_lock lk(_mtx);

      if (httpClient != nullptr) {
        _httpClient = std::move(httpClient);

        // give up mutex temporarily, while we are making the request, as we
        // want the request to be cancelable from the outside
        lk.unlock();

        std::unique_ptr<httpclient::SimpleHttpResult> response;
        response.reset(_httpClient->request(rest::RequestType::POST,
                                            relativeUrl, compressedBody.data(),
                                            compressedBody.size(), headers));

        Result result = checkHttpResponse(response);

        if (result.ok()) {
          auto returnCode = response->getHttpReturnCode();
          if (returnCode == 200) {
            // we're not interested in the object if we're not testing
            // redirection
            if (reqUrl != ::kTelemetricsGatheringUrl) {
              responseBuilder.add(response->getBodyVelocyPack()->slice());
            }
            break;
          } else if (returnCode == 301 || returnCode == 302 ||
                     returnCode == 307) {
            bool foundUrl = false;
            if (numRedirects < maxRedirects) {
              url = response->getHeaderField(StaticStrings::Location, foundUrl);
            }
            if (foundUrl) {
              numRedirects++;
              continue;
            }

            // if hasn't found the new url from the redirection, there's no use
            // in trying to redirect again, so will retry to send the request to
            // the original url first
            url = reqUrl;
          }
        } else if (!result.is(TRI_ERROR_INTERNAL)) {
          // do not retry if 401, 403, 404, 420, etc.
          // note: if is an internal error, will retry to send the request to
          // the server
          break;
        }

        numRedirects = 0;
        timeoutInSecs *= 3;
        timeoutInSecs = std::min<uint32_t>(600, timeoutInSecs);

        TRI_ASSERT(!lk.owns_lock());
        lk.lock();
      }

      TRI_ASSERT(lk.owns_lock());
      _httpClient.reset();
      _runCondition.wait_for(lk, std::chrono::seconds(timeoutInSecs),
                             [this] { return _server.isStopping(); });
    } catch (std::exception const& ex) {
      LOG_TOPIC("09acf", WARN, arangodb::Logger::FIXME)
          << "caught exception while sending telemetrics: " << ex.what();
      throw;
    }
  }

  // note: may be empty
  return responseBuilder;
}

std::string TelemetricsHandler::getFetchedInfo() const {
  std::unique_lock lk(_mtx);
  return _telemetricsFetchedInfo.toJson();
}

std::pair<std::string, std::unique_ptr<httpclient::SimpleHttpClient>>
TelemetricsHandler::buildHttpClient(std::string& url) const {
  ClientFeature& cf = _server.getFeature<HttpEndpointProvider, ClientFeature>();

  if (url.starts_with('/')) {
    // if the url is local, just get the connected client, and the url is
    // already relative
    ClientManager clientManager(cf, Logger::FIXME);
    return std::make_pair(
        url, clientManager.getConnectedClient(true, false, false, 0));
  }

  // the url is not local, so we create a connection
  std::string lastEndpoint = basics::StringUtils::getEndpointFromUrl(url);
  std::vector<std::string> endpoints;
  // note: the following call may modify url in place!!
  auto [endpoint, relative, error] = getEndpoint(endpoints, url, lastEndpoint);

  std::unique_ptr<Endpoint> newEndpoint(Endpoint::clientFactory(endpoint));

  if (newEndpoint != nullptr) {
    constexpr uint64_t sslProtocol = TLS_V13;

    std::unique_ptr<httpclient::GeneralClientConnection> connection(
        httpclient::GeneralClientConnection::factory(
            cf.getCommFeaturePhase(), newEndpoint, 30, 60, 3, sslProtocol));

    if (connection != nullptr) {
      connection->setSocketNonBlocking(true);
      // note: the returned SimpleHttpClient instance takes over ownership
      // for the connection object
      return std::make_pair(
          std::move(relative),
          std::make_unique<httpclient::SimpleHttpClient>(
              connection, httpclient::SimpleHttpClientParams(30, false)));
    }
  }

  // no connection!
  return std::make_pair("", nullptr);
}

void TelemetricsHandler::getTelemetricsInfo(velocypack::Builder& builder) {
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
#ifndef ARANGODB_ENABLE_MAINTAINER_MODE
    _sendToEndpoint = true;
#endif
    fetchTelemetricsFromServer();

    std::unique_lock lk(_mtx);
    if (_telemetricsFetchResponse.ok() && !_telemetricsFetchedInfo.isEmpty()) {
      lk.unlock();
      if (_sendToEndpoint) {
        sendTelemetricsToEndpoint(::kTelemetricsGatheringUrl);
      }
    }
  } catch (...) {
    // no need to do anything special here. exceptions are already
    // handled in fetchTelemetricsFromServer and in sendTelemetricsToEndpoint.
    // here we can simply silence them
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
