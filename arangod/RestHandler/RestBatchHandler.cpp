////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "RestBatchHandler.h"

#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
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

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

RestBatchHandler::RestBatchHandler(application_features::ApplicationServer& server,
                                   GeneralRequest* request, GeneralResponse* response)
    : RestVocbaseBaseHandler(server, request, response), _errors(0) {}

RestBatchHandler::~RestBatchHandler() = default;

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_batch_processing
////////////////////////////////////////////////////////////////////////////////

RestStatus RestBatchHandler::execute() {
  switch (_response->transportType()) {
    case Endpoint::TransportType::HTTP: {
      return executeHttp();
    }
    case Endpoint::TransportType::VST:
    default: {
      return executeVst();
    }
  }
  // should never get here
  TRI_ASSERT(false);
  return RestStatus::DONE;
}

RestStatus RestBatchHandler::executeVst() {
  generateError(rest::ResponseCode::METHOD_NOT_ALLOWED, TRI_ERROR_NO_ERROR,
                "The RestBatchHandler is not supported for this protocol!");
  return RestStatus::DONE;
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
  if (_helper.contentId != nullptr) {
    httpResponse->body().appendText(
        "\r\nContent-Id: " + std::string(_helper.contentId, _helper.contentIdLength));
  }

  httpResponse->body().appendText(TRI_CHAR_LENGTH_PAIR("\r\n\r\n"));

  // remove some headers we don't need
  partResponse->setHeaderNC(StaticStrings::Server, "");

  // append the part response header
  partResponse->writeHeader(&httpResponse->body());

  // append the part response body
  httpResponse->body().appendText(partResponse->body());
  httpResponse->body().appendText(TRI_CHAR_LENGTH_PAIR("\r\n"));

  // we've read the last part
  if (!_helper.containsMore) {
    // complete the handler

    // append final boundary + "--"
    httpResponse->body().appendText(_boundary + "--");

    if (_errors > 0) {
      httpResponse->setHeaderNC(StaticStrings::Errors,
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
  std::string const& authorization = _request->header(StaticStrings::Authorization);

  // get the next part from the multipart message
  if (!extractPart(_helper)) {
    // error
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "invalid multipart message received");
    LOG_TOPIC("3204a", WARN, arangodb::Logger::REPLICATION)
        << "received a corrupted multipart message";
    return false;
  }

  // split part into header & body
  char const* partStart = _helper.foundStart;
  char const* partEnd = partStart + _helper.foundLength;
  size_t const partLength = _helper.foundLength;

  char const* headerStart = partStart;
  char const* bodyStart = nullptr;
  size_t headerLength = 0;
  size_t bodyLength = 0;

  // assume Windows linebreak \r\n\r\n as delimiter
  char const* p = strstr(headerStart, "\r\n\r\n");

  if (p != nullptr && p + 4 <= partEnd) {
    headerLength = p - partStart;
    bodyStart = p + 4;
    bodyLength = partEnd - bodyStart;
  } else {
    // test Unix linebreak
    p = strstr(headerStart, "\n\n");

    if (p != nullptr && p + 2 <= partEnd) {
      headerLength = p - partStart;
      bodyStart = p + 2;
      bodyLength = partEnd - bodyStart;
    } else {
      // no delimiter found, assume we have only a header
      headerLength = partLength;
    }
  }

  // set up request object for the part
  LOG_TOPIC("910e9", TRACE, arangodb::Logger::REPLICATION)
      << "part header is: " << std::string(headerStart, headerLength);

  auto request = std::make_unique<HttpRequest>(_request->connectionInfo(), /*messageId*/1,
                                               /*allowMethodOverride*/false);
  if (0 < headerLength) {
    auto buff = std::make_unique<char[]>(headerLength + 1);
    memcpy(buff.get(), headerStart, headerLength);
    (buff.get())[headerLength] = 0;
    request->parseHeader(buff.get(), headerLength);
  }

  // inject the request context from the framing (batch) request
  // the "false" means the context is not responsible for resource handling
  request->setRequestContext(_request->requestContext(), false);
  request->setDatabaseName(_request->databaseName());

  if (bodyLength > 0) {
    LOG_TOPIC("63afb", TRACE, arangodb::Logger::REPLICATION)
        << "part body is '" << std::string(bodyStart, bodyLength) << "'";
    request->body().clear();
    request->body().reserve(bodyLength+1);
    request->body().append(bodyStart, bodyLength);
    request->body().push_back('\0');
    request->body().resetTo(bodyLength); // ensure null terminated
  }

  if (!authorization.empty()) {
    // inject Authorization header of multipart message into part message
    request->setHeader(StaticStrings::Authorization.c_str(),
                       StaticStrings::Authorization.size(),
                       authorization.c_str(), authorization.size());
  }

  std::shared_ptr<RestHandler> handler;

  {
    auto response = std::make_unique<HttpResponse>(rest::ResponseCode::SERVER_ERROR, 1,
                                                   std::make_unique<StringBuffer>(false));
    auto& factory = server().getFeature<GeneralServerFeature>().handlerFactory();
    handler = factory.createHandler(server(), std::move(request), std::move(response));

    if (handler == nullptr) {
      generateError(rest::ResponseCode::BAD, TRI_ERROR_INTERNAL,
                    "could not create handler for batch part processing");

      return false;
    }
  }

  // assume a bad lane, so the request is definitely executed via the queues
  auto const lane = RequestLane::CLIENT_V8;


  // now schedule the real handler
  bool ok =
      SchedulerFeature::SCHEDULER->queue(lane, [this, self = shared_from_this(), handler]() {
        // start to work for this handler
        // ignore any errors here, will be handled later by inspecting the response
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
    generateError(rest::ResponseCode::SERVICE_UNAVAILABLE, TRI_ERROR_QUEUE_FULL);
    return false;
  }

  return true;
}

RestStatus RestBatchHandler::executeHttp() {
  TRI_ASSERT(_response->transportType() == Endpoint::TransportType::HTTP);

  // extract the request type
  auto const type = _request->requestType();

  if (type != rest::RequestType::POST && type != rest::RequestType::PUT) {
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED, TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
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
  VPackStringRef bodyStr = _request->rawPayload();

  // setup some auxiliary structures to parse the multipart message
  _multipartMessage =
      MultipartMessage{_boundary.data(), _boundary.size(), bodyStr.data(),
                       bodyStr.data() + bodyStr.size()};

  _helper.message = _multipartMessage;
  _helper.searchStart = _multipartMessage.messageStart;

  // and wait for completion
  return executeNextHandler() ? RestStatus::WAITING : RestStatus::DONE;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extract the boundary from the body of a multipart message
////////////////////////////////////////////////////////////////////////////////

bool RestBatchHandler::getBoundaryBody(std::string& result) {
  TRI_ASSERT(_response->transportType() == Endpoint::TransportType::HTTP);

  VPackStringRef bodyStr = _request->rawPayload();
  char const* p = bodyStr.data();
  char const* e = p + bodyStr.size();

  // skip whitespace
  while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') {
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

  if (parts.size() != 2 || parts[0] != StaticStrings::MultiPartContentType) {
    // content-type is not formatted as expected
    return false;
  }

  static size_t const boundaryLength = 9;  // strlen("boundary=");

  // trim 2nd part and lowercase it
  StringUtils::trimInPlace(parts[1]);
  std::string p = parts[1].substr(0, boundaryLength);
  StringUtils::tolowerInPlace(p);

  if (p != "boundary=") {
    return false;
  }

  std::string boundary = parts[1].substr(boundaryLength);

  if ((boundary.length() > 1) && (boundary[0] == '"') &&
      (boundary[boundary.length() - 1] == '"')) {
    StringUtils::trimInPlace(boundary, "\"");
  } else if ((boundary.length() > 1) && (boundary[0] == '\'') &&
             (boundary[boundary.length() - 1] == '\'')) {
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
  TRI_ASSERT(helper.searchStart != nullptr);

  // init the response
  helper.foundStart = nullptr;
  helper.foundLength = 0;
  helper.containsMore = false;
  helper.contentId = nullptr;
  helper.contentIdLength = 0;

  char const* searchEnd = helper.message.messageEnd;

  if (helper.searchStart >= searchEnd) {
    // we're at the end already
    return false;
  }

  // search for boundary
  char const* found = strstr(helper.searchStart, helper.message.boundary);

  if (found == nullptr) {
    // not contained. this is an error
    return false;
  }

  if (found != helper.searchStart) {
    // boundary not located at beginning. this is an error
    return false;
  }

  found += helper.message.boundaryLength;

  if (found + 1 >= searchEnd) {
    // we're outside the buffer. this is an error
    return false;
  }

  while (found < searchEnd && *found == ' ') {
    ++found;
  }

  if (found + 2 >= searchEnd) {
    // we're outside the buffer. this is an error
    return false;
  }

  if (*found == '\r') {
    ++found;
  }
  if (*found != '\n') {
    // no linebreak found
    return false;
  }
  ++found;

  bool hasTypeHeader = false;

  int breakLength = 1;

  while (found < searchEnd) {
    while (*found == ' ' && found < searchEnd) {
      ++found;
    }

    // try Windows linebreak first
    breakLength = 2;

    char const* eol = strstr(found, "\r\n");

    if (eol == nullptr) {
      breakLength = 1;
      eol = strchr(found, '\n');

      if (eol == found) {
        break;
      }
    } else {
      char const* eol2 = strchr(found, '\n');

      if (eol2 != nullptr && eol2 < eol) {
        breakLength = 1;
        eol = eol2;
      }
    }

    if (eol == nullptr || eol == found) {
      break;
    }

    // split key/value of header
    char const* colon = static_cast<char const*>(memchr(found, (int)':', eol - found));

    if (nullptr == colon) {
      // invalid header, not containing ':'
      return false;
    }

    // set up the key
    std::string key(found, colon - found);
    StringUtils::trimInPlace(key);

    if (key[0] == 'c' || key[0] == 'C') {
      // got an interesting key. now process it
      StringUtils::tolowerInPlace(key);

      // skip the colon itself
      ++colon;
      // skip any whitespace
      while (*colon == ' ') {
        ++colon;
      }

      if (key == StaticStrings::ContentTypeHeader) {
        // extract the value, too
        std::string value(colon, eol - colon);
        StringUtils::trimInPlace(value);

        if (value == StaticStrings::BatchContentType) {
          hasTypeHeader = true;
        } else {
          LOG_TOPIC("f7836", WARN, arangodb::Logger::REPLICATION)
              << "unexpected content-type '" << value << "' for multipart-message. expected: '"
              << StaticStrings::BatchContentType << "'";
        }
      } else if ("content-id" == key) {
        helper.contentId = colon;
        helper.contentIdLength = eol - colon;
      } else {
        // ignore other headers
      }
    }

    found = eol + breakLength;  // plus the \n
  }

  found += breakLength;  // for 2nd \n

  if (!hasTypeHeader) {
    // no Content-Type header. this is an error
    return false;
  }

  // we're at the start of the body part. set the return value
  helper.foundStart = found;

  // search for the end of the boundary
  found = strstr(helper.foundStart, helper.message.boundary);

  if (found == nullptr || found >= searchEnd) {
    // did not find the end. this is an error
    return false;
  }

  helper.foundLength = found - helper.foundStart;

  char const* p = found + helper.message.boundaryLength;

  if (p + 2 > searchEnd) {
    // end of boundary is outside the buffer
    return false;
  }

  if (*p != '-' || *(p + 1) != '-') {
    // we've not reached the end yet
    helper.containsMore = true;
    helper.searchStart = found;
  }

  return true;
}
