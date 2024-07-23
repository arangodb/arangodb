////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "RestBatchHandler.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringBuffer.h"
#include "Basics/StringUtils.h"
#include "Basics/debugging.h"
#include "GeneralServer/GeneralServer.h"
#include "GeneralServer/GeneralServerFeature.h"
#include "GeneralServer/RestHandlerFactory.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "Rest/HttpRequest.h"
#include "Rest/HttpResponse.h"
#include "Scheduler/SchedulerFeature.h"
#include "Utils/ExecContext.h"

#include <absl/strings/str_cat.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

bool arangodb::MultipartMessage::skipBoundaryStart(size_t& offset) const {
  size_t foundPos = message.find(boundary, offset);
  if (foundPos == offset) {
    foundPos += boundary.size();
    while (foundPos < message.size() && message[foundPos] == ' ') {
      ++foundPos;
    }
    if (foundPos < message.size() && message[foundPos] == '\r') {
      ++foundPos;
    }
    if (foundPos < message.size() && message[foundPos] == '\n') {
      offset = foundPos + 1;
      return true;
    }
  }
  return false;
}

bool arangodb::MultipartMessage::findBoundaryEnd(size_t& offset) const {
  size_t foundPos = message.find(boundary, offset);
  if (foundPos != std::string_view::npos) {
    offset = foundPos + boundary.size();
    return true;
  }
  return false;
}

bool arangodb::MultipartMessage::isAtEnd(size_t& offset) const {
  if (offset + 1 < message.size() && message[offset] == '-' &&
      message[offset + 1] == '-') {
    offset += 2;
    return true;
  }
  return false;
}

std::string arangodb::MultipartMessage::getHeader(size_t& offset) const {
  std::string header;

  while (true) {
    while (offset < message.size() && message[offset] == ' ') {
      ++offset;
    }
    if (offset >= message.size()) {
      break;
    }

    // find first linebreak
    size_t eol1 = message.find("\r\n", offset);
    size_t eol2 = message.find("\n", offset);

    if (eol1 == 0) {
      // line starts with Windows linebreak;
      offset += 2;
      break;
    }
    if (eol2 == 0) {
      // line starts with normal linebreak
      offset += 1;
      break;
    }

    size_t eol = [&]() {
      if (eol1 != std::string::npos ||
          (eol2 == std::string::npos || eol1 < eol2)) {
        return eol1;
      }
      return eol2;
    }();

    if (eol == std::string::npos) {
      break;
    }

    header = {message.data() + offset, eol - offset};
    offset = eol;
    TRI_ASSERT(offset < message.size());
    TRI_ASSERT(message[eol] == '\n' || message[eol] == '\r');
    if (message[offset] == '\r') {
      offset += 1;
    }
    if (message[offset] == '\n') {
      offset += 1;
    }
    break;
  }
  return header;
}

RestBatchHandler::RestBatchHandler(ArangodServer& server,
                                   GeneralRequest* request,
                                   GeneralResponse* response)
    : RestVocbaseBaseHandler(server, request, response), _errors(0) {}

RestBatchHandler::~RestBatchHandler() = default;

RestStatus RestBatchHandler::execute() {
  _response->setAllowCompression(rest::ResponseCompressionType::kNoCompression);

  // extract the request type
  auto const type = _request->requestType();

  if (type != rest::RequestType::POST && type != rest::RequestType::PUT) {
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                  TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
    return RestStatus::DONE;
  }

  // invalid content-type or boundary sent
  if (!getBoundary(_boundary)) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "invalid content-type or boundary received");
    return RestStatus::DONE;
  }

  LOG_TOPIC("b03fa", TRACE, arangodb::Logger::REPLICATION)
      << "boundary of multipart-message is '" << _boundary << "'";

  _errors = 0;

  // create the response
  resetResponse(rest::ResponseCode::OK);
  _response->setContentType(_request->header(StaticStrings::ContentTypeHeader));

  // http required here
  // setup some auxiliary structures to parse the multipart message
  _multipartMessage = {_boundary, _request->rawPayload()};

  _helper.message = _multipartMessage;
  _helper.searchStart = 0;

  // and wait for completion
  return executeNextHandler() ? RestStatus::WAITING : RestStatus::DONE;
}

void RestBatchHandler::processSubHandlerResult(RestHandler const& handler) {
  HttpResponse* httpResponse = dynamic_cast<HttpResponse*>(_response.get());

  HttpResponse* partResponse = dynamic_cast<HttpResponse*>(handler.response());

  if (partResponse == nullptr) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_INTERNAL,
                  "could not create a response for batch part request");
    wakeupHandler();
    return;
  }

  rest::ResponseCode const code = partResponse->responseCode();

  // count everything above 400 as error
  if (int(code) >= 400) {
    ++_errors;
  }

  // append the boundary for this subpart
  httpResponse->body().appendText(_boundary + "\r\nContent-Type: ");
  httpResponse->body().appendText(StaticStrings::BatchContentType);

  // append content-id if it is present
  if (!_helper.contentId.empty()) {
    httpResponse->body().appendText(
        absl::StrCat("\r\nContent-Id: ", _helper.contentId));
  }

  httpResponse->body().appendText(std::string_view("\r\n\r\n"));

  // remove some headers we don't need
  partResponse->setHeaderNC(StaticStrings::Server, "");

  // append the part response header
  partResponse->writeHeader(&httpResponse->body());

  // append the part response body
  httpResponse->body().appendText(partResponse->body());
  httpResponse->body().appendText(std::string_view("\r\n"));

  // we've read the last part
  if (!_helper.containsMore) {
    // complete the handler

    // append final boundary + "--"
    httpResponse->body().appendText(_boundary + "--");

    if (_errors > 0) {
      httpResponse->setHeaderNC(
          StaticStrings::Errors,
          StringUtils::itoa(static_cast<uint64_t>(_errors)));
    }
    wakeupHandler();
  } else {
    if (!executeNextHandler()) {
      wakeupHandler();
    }
  }
}

bool RestBatchHandler::executeNextHandler() {
  // get authorization header. we will inject this into the subparts
  std::string const& authorization =
      _request->header(StaticStrings::Authorization);

  // get the next part from the multipart message
  if (!extractPart(_helper)) {
    // error
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "invalid multipart message received");
    LOG_TOPIC("3204a", WARN, arangodb::Logger::REPLICATION)
        << "received a corrupted multipart message";
    return false;
  }

  auto [header, body] = [](std::string_view value) {
    // assume Windows linebreak \r\n\r\n as delimiter
    size_t pos = value.find("\r\n\r\n");
    if (pos != std::string_view::npos) {
      return std::make_pair(value.substr(0, pos), value.substr(pos + 4));
    }
    // test Unix linebreak
    pos = value.find("\n\n");
    if (pos != std::string_view::npos) {
      return std::make_pair(value.substr(0, pos), value.substr(pos + 2));
    }
    // assume we only have a header
    return std::make_pair(value, std::string_view());
  }(_helper.found);

  // set up request object for the part
  LOG_TOPIC("910e9", TRACE, arangodb::Logger::REPLICATION)
      << "part header is: " << header;

  auto request = std::make_unique<HttpRequest>(_request->connectionInfo(),
                                               /*messageId*/ 1);
  if (!header.empty()) {
    auto buff = std::make_unique<char[]>(header.size() + 1);
    memcpy(buff.get(), header.data(), header.size());
    (buff.get())[header.size()] = 0;
    request->parseHeader(buff.get(), header.size());
  }

  // inject the request context from the framing (batch) request
  request->setRequestContext(_request->requestContext());
  request->setDatabaseName(_request->databaseName());

  if (!body.empty()) {
    LOG_TOPIC("63afb", TRACE, arangodb::Logger::REPLICATION)
        << "part body is '" << body << "'";
    request->clearBody();
    request->appendBody(body.data(), body.size());
    request->appendNullTerminator();
  }

  if (!authorization.empty()) {
    // inject Authorization header of multipart message into part message
    request->setHeader(StaticStrings::Authorization, authorization);
  }

  std::shared_ptr<RestHandler> handler;

  {
    // batch responses do not support response compression
    auto response =
        std::make_unique<HttpResponse>(rest::ResponseCode::SERVER_ERROR, 1,
                                       std::make_unique<StringBuffer>(false),
                                       ResponseCompressionType::kNoCompression);
    auto factory = server().getFeature<GeneralServerFeature>().handlerFactory();
    handler = factory->createHandler(server(), std::move(request),
                                     std::move(response));

    if (handler == nullptr) {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_INTERNAL,
                    "could not create handler for batch part processing");

      return false;
    }

    handler->setIsAsyncRequest();
  }

  // assume a bad lane, so the request is definitely executed via the queues
  auto const lane = RequestLane::CLIENT_V8;

  // now schedule the real handler
  bool ok = SchedulerFeature::SCHEDULER->tryBoundedQueue(
      lane, [this, self = shared_from_this(), handler]() {
        // start to work for this handler
        // ignore any errors here, will be handled later by inspecting the
        // response
        try {
          ExecContextScope scope(nullptr);  // workaround because of assertions
          handler->runHandler([this, self](RestHandler* handler) {
            processSubHandlerResult(*handler);
          });
        } catch (...) {
          processSubHandlerResult(*handler);
        }
      });

  if (!ok) {
    generateError(rest::ResponseCode::SERVICE_UNAVAILABLE,
                  TRI_ERROR_QUEUE_FULL);
    return false;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extract the boundary from the body of a multipart message
////////////////////////////////////////////////////////////////////////////////

bool RestBatchHandler::getBoundaryBody(std::string& result) {
  std::string_view bodyStr = _request->rawPayload();
  char const* p = bodyStr.data();
  char const* e = p + bodyStr.size();

  // skip whitespace
  while (p < e && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) {
    ++p;
  }

  if (p + 10 > e) {
    return false;
  }

  if (p[0] != '-' || p[1] != '-') {
    // boundary must start with "--"
    return false;
  }

  char const* q = p;

  while (q < e && *q && *q != ' ' && *q != '\t' && *q != '\r' && *q != '\n') {
    ++q;
  }

  if ((q - p) < 5) {
    // 3 bytes is min length for boundary (without "--")
    return false;
  }

  result = std::string(p, (q - p));
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extract the boundary from the HTTP header of a multipart message
////////////////////////////////////////////////////////////////////////////////

bool RestBatchHandler::getBoundaryHeader(std::string& result) {
  // extract content type
  std::string const contentType =
      StringUtils::trim(_request->header(StaticStrings::ContentTypeHeader));

  // content type is expect to contain a boundary like this:
  // "Content-Type: multipart/form-data; boundary=<boundary goes here>"
  std::vector<std::string> parts = StringUtils::split(contentType, ';');

  if (parts.size() != 2) {
    // content-type is not formatted as expected
    return false;
  }
  std::string& key = parts[0];
  StringUtils::tolowerInPlace(key);
  if (key != StaticStrings::MultiPartContentType) {
    return false;
  }

  // trim 2nd part and lowercase it
  std::string boundary = parts[1];
  StringUtils::trimInPlace(boundary);

  parts = StringUtils::split(boundary, '=');
  if (parts.size() < 2) {
    return false;
  }
  StringUtils::tolowerInPlace(parts[0]);

  constexpr std::string_view kBoundary{"boundary"};

  if (!parts[0].starts_with(kBoundary)) {
    return false;
  }

  boundary = parts[1];

  if (boundary.starts_with('"') && boundary.ends_with('"')) {
    StringUtils::trimInPlace(boundary, "\"");
  } else if (boundary.starts_with('\'') && boundary.ends_with('\'')) {
    StringUtils::trimInPlace(boundary, "'");
  }

  if (boundary.size() < 3) {
    // 3 bytes is min length for boundary (without "--")
    return false;
  }

  result = "--" + boundary;
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extract the boundary of a multipart message
////////////////////////////////////////////////////////////////////////////////

bool RestBatchHandler::getBoundary(std::string& result) {
  TRI_ASSERT(_request);

  // try peeking at header first
  if (getBoundaryHeader(result)) {
    return true;
  }

  // boundary not found in header, now peek in body
  return getBoundaryBody(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extract the next part from a multipart message
////////////////////////////////////////////////////////////////////////////////

bool RestBatchHandler::extractPart(SearchHelper& helper) {
  // init the response
  helper.found = {};
  helper.containsMore = false;
  helper.contentId = "";

  if (helper.searchStart >= helper.message.message.size()) {
    // we're at the end already
    return false;
  }

  // this call can modify helper.searchStart
  if (!helper.message.skipBoundaryStart(helper.searchStart)) {
    return false;
  }

  bool hasTypeHeader = false;
  while (true) {
    // this call can modify helper.searchStart
    std::string header = helper.message.getHeader(helper.searchStart);
    if (header.empty()) {
      break;
    }
    auto parts = StringUtils::split(header, ':');
    if (parts.size() != 2) {
      // invalid header, not containing ':'
      return false;
    }

    // set up the key
    std::string& key = parts[0];
    StringUtils::trimInPlace(key);
    StringUtils::tolowerInPlace(key);

    if (key == StaticStrings::ContentTypeHeader) {
      // got an interesting key. now process it
      // extract the value, too
      std::string& value = parts[1];
      StringUtils::trimInPlace(value);

      if (value == StaticStrings::BatchContentType) {
        hasTypeHeader = true;
      } else {
        LOG_TOPIC("f7836", WARN, arangodb::Logger::REPLICATION)
            << "unexpected content-type '" << value
            << "' for multipart-message. expected: '"
            << StaticStrings::BatchContentType << "'";
      }
    } else if ("content-id" == key) {
      std::string& value = parts[1];
      StringUtils::trimInPlace(value);
      helper.contentId = value;
    }
  }

  if (!hasTypeHeader) {
    // no Content-Type header. this is an error
    return false;
  }

  size_t offset = helper.searchStart;

  if (!helper.message.findBoundaryEnd(helper.searchStart)) {
    // did not find the end. this is an error
    return false;
  }

  helper.found = {helper.message.message.data() + offset,
                  helper.searchStart - offset - helper.message.boundary.size()};

  helper.containsMore = !helper.message.isAtEnd(helper.searchStart);
  if (helper.containsMore) {
    helper.searchStart -= helper.message.boundary.size();
  }
  return true;
}
