////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "AgencyMock.h"

#include "Agency/Store.h"
#include "Basics/ConditionLocker.h"
#include "Basics/NumberUtils.h"
#include "Basics/StringBuffer.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Rest/HttpResponse.h"

#include <velocypack/velocypack-aliases.h>

#include <utility>

namespace arangodb {
namespace consensus {

// FIXME TODO this implementation causes deadlock when unregistering a callback,
//            if there is still another callback registered; it's not obvious
//            how to fix this, as it seems the problem is that both "agents"
//            live on the same server and share an AgencyCallbackRegistry
//            instance; we could solve this if we could have two
//            ApplicationServers in the same instance, but too many things in
//            the feature stack are static still to make the changes right now
// FIXME TODO for some reason the implementation of this function is missing in the arangodb code
void Store::notifyObservers() const {
  if (!_server.hasFeature<arangodb::ClusterFeature>()) {
    return;
  }
  auto& clusterFeature = _server.getFeature<arangodb::ClusterFeature>();

  auto* callbackRegistry = clusterFeature.agencyCallbackRegistry();

  if (!callbackRegistry) {
    return;
  }

  std::vector<uint32_t> callbackIds;

  {
    MUTEX_LOCKER(storeLocker, _storeLock);

    for (auto& entry: _observerTable) {
      auto& key = entry.first;
      auto pos = key.rfind("/"); // observer id is after the last '/'

      if (std::string::npos == pos) {
        continue;
      }

      bool success;
      auto* idStr = &(key[pos + 1]);
      auto id = arangodb::NumberUtils::atoi<uint32_t>(
        idStr, idStr + std::strlen(idStr), success
      );

      if (success) {
        callbackIds.emplace_back(id);
      }
    }
  }

  for (auto& id: callbackIds) {
    try {
      callbackRegistry->getCallback(id)->refetchAndUpdate(true, true);  // force a check
    } catch(...) {
      // ignore
    }
  }
}

} // consensus
} // arangodb
#if 0
// -----------------------------------------------------------------------------
// --SECTION--                                 GeneralClientConnectionAgencyMock
// -----------------------------------------------------------------------------

void GeneralClientConnectionAgencyMock::handleRead(
    arangodb::basics::StringBuffer& buffer
) {
  auto const query = arangodb::velocypack::Parser::fromJson(_body);

  auto result = std::make_shared<arangodb::velocypack::Builder>();
  auto const success = _store->read(query, result);
  auto const code = std::find(success.begin(), success.end(), false) == success.end()
    ? arangodb::rest::ResponseCode::OK
    : arangodb::rest::ResponseCode::BAD;

  arangodb::HttpResponse resp(code, nullptr);

  std::string body;
  if (arangodb::rest::ResponseCode::OK == code && !result->isEmpty()) {
    body = result->slice().toString();
    resp.setContentType(arangodb::ContentType::VPACK);
    resp.headResponse(body.size());
  }

  resp.writeHeader(&buffer);

  if (!body.empty()) {
    buffer.appendText(body);
  }
}

void GeneralClientConnectionAgencyMock::handleWrite(
    arangodb::basics::StringBuffer& buffer
) {
  auto const query = arangodb::velocypack::Parser::fromJson(_body);
  auto const success = _store->applyTransactions(query);
  auto const code =
    std::find_if(success.begin(), success.end(),
                 [&](int i)->bool { return i != 0; }) == success.end() ?
    arangodb::rest::ResponseCode::OK : arangodb::rest::ResponseCode::PRECONDITION_FAILED;

  VPackBuilder bodyObj;
  bodyObj.openObject();
  bodyObj.add("results", VPackValue(VPackValueType::Array));
  bodyObj.close();
  bodyObj.close();
  auto body = bodyObj.slice().toString();

  arangodb::HttpResponse resp(code, nullptr);
  resp.setContentType(arangodb::ContentType::VPACK);
  resp.headResponse(body.size());

  resp.writeHeader(&buffer);
  buffer.appendText(body);
  _store->notifyObservers();
}

void GeneralClientConnectionAgencyMock::response(
    arangodb::basics::StringBuffer& buffer
) {
  if (_path.size() != 4) {
    // invalid message format
    throw std::exception();
  }

  auto const& op = action();

  if (op == "write") {
    handleWrite(buffer);
  } else if (op == "read") {
    handleRead(buffer);
  } else {
    // unsupported operation
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }

  if (_trace) {
    std::cerr << "Response(" << _url << " " << _path << " )" << std::endl;
    std::cerr << buffer.toString() << std::endl;
  }
}

void GeneralClientConnectionAgencyMock::request(char const* data, size_t length) {
  static const std::string bodyDelimiter("\r\n\r\n");

  std::string const request(data, length);

  if (_trace) {
    std::cerr << "Request()" << std::endl;
    std::cerr << request << std::endl;
  }

  auto pos = request.find("\r\n");

  if (pos == std::string::npos) {
    return; // use full string as key
  }

  // <HTTP-method> <path> HTTP/1.1
  auto const requestLineParts = arangodb::basics::StringUtils::split(
    std::string(data, pos), ' '
  );

  if (3 != requestLineParts.size()
      || requestLineParts[0] != "POST" // agency works with POST requests only
      || requestLineParts[2] != "HTTP/1.1") {
    // invalid message format
    throw std::exception();
  }

  pos = request.find(bodyDelimiter, pos);

  if (pos == std::string::npos) {
    return; // use full string as key
  }

  _url = requestLineParts[1];
  _path = arangodb::basics::StringUtils::split(_url, '/');
  _body.assign(data + pos + bodyDelimiter.size());
}
#endif

using namespace arangodb;
using namespace arangodb::network;

struct AsyncAgencyStorePoolConnection final : public fuerte::Connection {
  AsyncAgencyStorePoolConnection(AsyncAgencyStorePoolMock* mock, std::string endpoint)
      : fuerte::Connection(fuerte::detail::ConnectionConfiguration()),
        _mock(mock),
        _endpoint(std::move(endpoint)) {}

  std::size_t requestsLeft() const override { return 1; }
  State state() const override { return fuerte::Connection::State::Connected; }

  void cancel() override {}
  void startConnection() override {}

  AsyncAgencyStorePoolMock* _mock;
  std::string _endpoint;

  auto handleRead(VPackSlice body) -> std::unique_ptr<fuerte::Response> {
    auto* store = _mock->_store;

    fuerte::ResponseHeader header;

    auto bodyBuilder = std::make_shared<VPackBuilder>(body);
    VPackBuffer<uint8_t> responseBuffer;
    {
      auto result = std::make_shared<arangodb::velocypack::Builder>(responseBuffer);
      auto const success = store->read(bodyBuilder, result);
      auto const code =
          std::find(success.begin(), success.end(), false) == success.end()
              ? fuerte::StatusOK
              : fuerte::StatusBadRequest;

      header.contentType(fuerte::ContentType::VPack);
      header.responseCode = code;
    }

    auto response = std::make_unique<fuerte::Response>(std::move(header));
    response->setPayload(std::move(responseBuffer), 0);

    return response;
  }

  auto handleWrite(VPackSlice body) -> std::unique_ptr<fuerte::Response> {
    auto* store = _mock->_store;
    auto bodyBuilder = std::make_shared<VPackBuilder>(body);

    auto const success = store->applyTransactions(bodyBuilder);
    auto const code =
        std::find_if(success.begin(), success.end(),
                     [&](int i) -> bool { return i != 0; }) == success.end()
            ? fuerte::StatusOK
            : fuerte::StatusPreconditionFailed;

    VPackBuffer<uint8_t> responseBody;
    {
      VPackBuilder bodyObj(responseBody);
      bodyObj.openObject();
      bodyObj.add("results", VPackValue(VPackValueType::Array));
      bodyObj.close();
      bodyObj.close();
    }

    fuerte::ResponseHeader header;
    header.contentType(fuerte::ContentType::VPack);
    header.responseCode = code;

    auto response = std::make_unique<fuerte::Response>(std::move(header));
    response->setPayload(std::move(responseBody), 0);

    store->notifyObservers();

    return response;
  }

  fuerte::MessageID sendRequest(std::unique_ptr<fuerte::Request> req,
                                fuerte::RequestCallback cb) override {

    std::unique_ptr<fuerte::Response> resp;

    if (req->header.restVerb == fuerte::RestVerb::Post) {
      if (req->header.path.find("write") != std::string::npos) {
        resp = handleWrite(req->slice());
      } else if(req->header.path.find("read") != std::string::npos) {
        resp = handleRead(req->slice());
      } else {
        throw std::logic_error("invalid operation");
      }
    } else {
      throw std::logic_error("only post requests for agency");
    }

    cb(fuerte::Error::NoError, std::move(req), std::move(resp));
    return 0;
  }
};

ConnectionPtr AsyncAgencyStorePoolMock::createConnection(fuerte::ConnectionBuilder& builder) {
  return std::make_shared<AsyncAgencyStorePoolConnection>(this, builder.normalizedEndpoint());
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
