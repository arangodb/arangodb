////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "PreparedResponseConnectionPool.h"

#include <fuerte/connection.h>

#include "ApplicationFeatures/ApplicationServer.h"
#include "Cluster/AgencyCache.h"
#include "IResearch/AgencyMock.h"
#include "IResearch/RestHandlerMock.h"
#include "RestServer/MetricsFeature.h"

using namespace arangodb;
using namespace arangodb::tests;
using namespace arangodb::network;

namespace {

bool IsRestVerbEqual(fuerte::RestVerb const& fuerteVerb,
                     arangodb::rest::RequestType const& arangoVerb) {
  auto adb = std::string(requestToString(arangoVerb));
  auto fue = arangodb::fuerte::from_string(adb);
  return fue == fuerteVerb;
}

std::vector<std::string> SplitPath(std::string const& path) {
  std::vector<std::string> result;
  size_t start;
  size_t end = 0;

  while ((start = path.find_first_not_of('/', end)) != std::string::npos) {
    end = path.find('/', start);
    result.emplace_back(path.substr(start, end - start));
  }
  return result;
}

class FakeConnection final : public fuerte::Connection {
 public:
  FakeConnection(std::shared_ptr<std::vector<PreparedRequestResponse>> responses)
      : fuerte::Connection(fuerte::detail::ConnectionConfiguration()),
        _responses(responses) {}

  std::size_t requestsLeft() const override { return 1; }
  State state() const override { return fuerte::Connection::State::Connected; }

  void sendRequest(std::unique_ptr<fuerte::Request> req, fuerte::RequestCallback cb) override {
    if (_responses) {
      for (auto const& r : *_responses) {
        if (r == *req) {
          cb(fuerte::Error::NoError, std::move(req), r.generateResponse());
          return;
        }
      }
    }

    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }

  void cancel() override {}

 private:
  std::shared_ptr<std::vector<PreparedRequestResponse>> _responses;
};

bool IsAgencyEndpoint(fuerte::ConnectionBuilder const& builder) {
  return builder.port() == "4000";
}
}  // namespace

PreparedRequestResponse::PreparedRequestResponse(TRI_vocbase_t& vocbase)
    : _vocbase{vocbase},
      _dbName(vocbase.name()),
      _type(arangodb::rest::RequestType::GET),
      _suffixes{},
      _payload{nullptr},
      _response{nullptr} {}

void PreparedRequestResponse::setRequestType(arangodb::rest::RequestType type) {
  _type = type;
}

void PreparedRequestResponse::addSuffix(std::string suffix) {
  _suffixes.emplace_back(suffix);
  _fullSuffixes.emplace_back(std::move(suffix));
}

void PreparedRequestResponse::addRestSuffix(std::string suffix) {
  _fullSuffixes.emplace_back(std::move(suffix));
}

void PreparedRequestResponse::addBody(VPackSlice slice) {
  VPackBuilder builder(slice);
  _payload = builder.steal();
}

std::unique_ptr<GeneralRequestMock> PreparedRequestResponse::generateRequest() const {
  auto fakeRequest = std::make_unique<GeneralRequestMock>(_vocbase);  // <-- set body here in fakeRequest

  fakeRequest->setRequestType(_type);
  for (auto const& s : _suffixes) {
    fakeRequest->addSuffix(s);
  }
  if (_payload) {
    fakeRequest->setData(VPackSlice{_payload->data()});
  }
  return fakeRequest;
}

void PreparedRequestResponse::rememberResponse(std::unique_ptr<GeneralResponse> response) {
  _response = std::move(response);
}

bool PreparedRequestResponse::operator==(fuerte::Request const& other) const {
  auto const& header = other.header;
  auto const& db = header.database;
  if (db != _dbName) {
    return false;
  }
  auto method = header.restVerb;
  if (!IsRestVerbEqual(method, _type)) {
    return false;
  }
  auto const& path = header.path;
  auto suffixes = SplitPath(path);
  suffixes.erase(suffixes.begin());
  if (_fullSuffixes.size() != suffixes.size()) {
    return false;
  }
  for (size_t i = 0; i < _fullSuffixes.size(); ++i) {
    if (suffixes[i] != _fullSuffixes[i]) {
      return false;
    }
  }

  auto reqBody = static_cast<const unsigned char*>(other.payload().data());
  VPackSlice reqBodySlice{reqBody};
  VPackSlice myBody{_payload->data()};
  return basics::VelocyPackHelper::equal(reqBodySlice, myBody, false);
}

std::unique_ptr<fuerte::Response> PreparedRequestResponse::generateResponse() const {
  // TODO what if response is empty;

  fuerte::ResponseHeader header{};
  auto code = _response->responseCode(); // TODO: NOT moved / set in responseMock
  (void)code;
  auto& payloadBuilder = static_cast<GeneralResponseMock*>(_response.get())->_payload;
  // TODO use code
  header.responseCode = arangodb::fuerte::StatusOK;

  auto resp = std::make_unique<fuerte::Response>(header);
  velocypack::Buffer<uint8_t> cloned{*payloadBuilder.buffer()};
  resp->setPayload(std::move(cloned), 0);
  return resp;
}

PreparedResponseConnectionPool::PreparedResponseConnectionPool(
    arangodb::AgencyCache& agencyCache, ConnectionPool::Config const& config)
    : ConnectionPool(config), _cache(agencyCache) {}

void PreparedResponseConnectionPool::addPreparedResponses(
    std::pair<std::string, std::string> const& endpoint,
    std::vector<PreparedRequestResponse> responses) {
  _responses.emplace(endpoint.first + ":" + endpoint.second,
                     std::make_shared<std::vector<PreparedRequestResponse>>(
                         std::move(responses)));
}

std::shared_ptr<fuerte::Connection> PreparedResponseConnectionPool::createConnection(
    fuerte::ConnectionBuilder& builder) {
  if (IsAgencyEndpoint(builder)) {
    return std::make_shared<AsyncAgencyStorePoolConnection>(_cache, builder.normalizedEndpoint());
  }
  auto search = builder.host() + ":" + builder.port();
  return std::make_shared<::FakeConnection>(_responses[search]);
}