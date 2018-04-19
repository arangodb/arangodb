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
#include "lib/Rest/HttpResponse.h"

#include <velocypack/velocypack-aliases.h>

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

  arangodb::HttpResponse resp(code);

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
  auto const code = std::find(success.begin(), success.end(), false) == success.end()
    ? arangodb::rest::ResponseCode::OK
    : arangodb::rest::ResponseCode::BAD;

  VPackBuilder bodyObj;
  bodyObj.openObject();
  bodyObj.add("results", VPackValue(VPackValueType::Array));
  bodyObj.close();
  bodyObj.close();
  auto body = bodyObj.slice().toString();

  arangodb::HttpResponse resp(code);
  resp.setContentType(arangodb::ContentType::VPACK);
  resp.headResponse(body.size());

  resp.writeHeader(&buffer);
  buffer.appendText(body);
}

void GeneralClientConnectionAgencyMock::getValue(
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
}

void GeneralClientConnectionAgencyMock::setKey(char const* data, size_t length) {
  static const std::string bodyDelimiter("\r\n\r\n");

  std::string request(data, length);

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

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
