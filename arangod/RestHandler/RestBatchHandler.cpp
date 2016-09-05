////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
#include "Logger/Logger.h"
#include "Rest/HttpRequest.h"

#include <iostream>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

RestBatchHandler::RestBatchHandler(GeneralRequest* request,
                                   GeneralResponse* response)
    : RestVocbaseBaseHandler(request, response) {}

RestBatchHandler::~RestBatchHandler() {}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_batch_processing
////////////////////////////////////////////////////////////////////////////////

RestHandler::status RestBatchHandler::execute() {
  switch (_response->transportType()) {
    case Endpoint::TransportType::HTTP: {
      return executeHttp();
    }
    case Endpoint::TransportType::VPP: {
      return executeVpp();
    }
  }
  // should never get here
  TRI_ASSERT(false);
  return RestHandler::status::FAILED;
}

RestHandler::status RestBatchHandler::executeVpp() {
  generateError(rest::ResponseCode::METHOD_NOT_ALLOWED, TRI_ERROR_NO_ERROR,
                "The RestBatchHandler is not supported for this protocol!");
  return status::DONE;
}

RestHandler::status RestBatchHandler::executeHttp() {
  HttpResponse* httpResponse = dynamic_cast<HttpResponse*>(_response.get());

  if (httpResponse == nullptr) {
    std::cout << "please fix this for vpack" << std::endl;
    THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
  }

  HttpRequest const* httpRequest =
      dynamic_cast<HttpRequest const*>(_request.get());

  if (httpRequest == nullptr) {
    std::cout << "please fix this for vpack" << std::endl;
    THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
  }

  // extract the request type
  auto const type = _request->requestType();

  if (type != rest::RequestType::POST && type != rest::RequestType::PUT) {
    generateError(rest::ResponseCode::METHOD_NOT_ALLOWED,
                  TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
    return status::DONE;
  }

  std::string boundary;

  // invalid content-type or boundary sent
  if (!getBoundary(&boundary)) {
    generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "invalid content-type or boundary received");
    return status::FAILED;
  }

  LOG(TRACE) << "boundary of multipart-message is '" << boundary << "'";

  size_t errors = 0;

  // get authorization header. we will inject this into the subparts
  std::string const& authorization =
      _request->header(StaticStrings::Authorization);

  // create the response
  resetResponse(rest::ResponseCode::OK);
  _response->setContentType(_request->header(StaticStrings::ContentTypeHeader));

  // http required here
  std::string const& bodyStr = httpRequest->body();
  // setup some auxiliary structures to parse the multipart message
  MultipartMessage message(boundary.c_str(), boundary.size(), bodyStr.c_str(),
                           bodyStr.c_str() + bodyStr.size());

  SearchHelper helper;
  helper.message = &message;
  helper.searchStart = message.messageStart;

  // iterate over all parts of the multipart message
  while (true) {
    // get the next part from the multipart message
    if (!extractPart(&helper)) {
      // error
      generateError(rest::ResponseCode::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                    "invalid multipart message received");
      LOG(WARN) << "received a corrupted multipart message";

      return status::FAILED;
    }

    // split part into header & body
    char const* partStart = helper.foundStart;
    char const* partEnd = partStart + helper.foundLength;
    size_t const partLength = helper.foundLength;

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
    LOG(TRACE) << "part header is: " << std::string(headerStart, headerLength);

    std::unique_ptr<HttpRequest> request(new HttpRequest(
        _request->connectionInfo(), headerStart, headerLength, false));

    // we do not have a client task id here
    request->setClientTaskId(0);

    // inject the request context from the framing (batch) request
    // the "false" means the context is not responsible for resource handling
    request->setRequestContext(_request->requestContext(), false);
    request->setDatabaseName(_request->databaseName());

    if (bodyLength > 0) {
      LOG(TRACE) << "part body is '" << std::string(bodyStart, bodyLength)
                 << "'";
      request->setBody(bodyStart, bodyLength);
    }

    if (!authorization.empty()) {
      // inject Authorization header of multipart message into part message
      request->setHeader(StaticStrings::Authorization.c_str(),
                         StaticStrings::Authorization.size(),
                         authorization.c_str(), authorization.size());
    }

    RestHandler* handler = nullptr;

    {
      std::unique_ptr<HttpResponse> response(
          new HttpResponse(rest::ResponseCode::SERVER_ERROR));

      handler = GeneralServerFeature::HANDLER_FACTORY->createHandler(
          std::move(request), std::move(response));

      if (handler == nullptr) {
        generateError(rest::ResponseCode::BAD, TRI_ERROR_INTERNAL,
                      "could not create handler for batch part processing");

        return status::FAILED;
      }
    }

    // start to work for this handler
    {
      HandlerWorkStack work(handler);
      RestHandler::status result = work.handler()->executeFull();

      if (result == status::FAILED) {
        generateError(rest::ResponseCode::BAD, TRI_ERROR_INTERNAL,
                      "executing a handler for batch part failed");

        return status::FAILED;
      }

      HttpResponse* partResponse =
          dynamic_cast<HttpResponse*>(handler->response());

      if (partResponse == nullptr) {
        generateError(rest::ResponseCode::BAD, TRI_ERROR_INTERNAL,
                      "could not create a response for batch part request");

        return status::FAILED;
      }

      rest::ResponseCode const code = partResponse->responseCode();

      // count everything above 400 as error
      if (int(code) >= 400) {
        ++errors;
      }

      // append the boundary for this subpart
      httpResponse->body().appendText(boundary + "\r\nContent-Type: ");
      httpResponse->body().appendText(StaticStrings::BatchContentType);

      // append content-id if it is present
      if (helper.contentId != 0) {
        httpResponse->body().appendText(
            "\r\nContent-Id: " +
            std::string(helper.contentId, helper.contentIdLength));
      }

      httpResponse->body().appendText(TRI_CHAR_LENGTH_PAIR("\r\n\r\n"));

      // remove some headers we don't need
      partResponse->setConnectionType(rest::ConnectionType::CONNECTION_NONE);
      partResponse->setHeaderNC(StaticStrings::Server, "");

      // append the part response header
      partResponse->writeHeader(&httpResponse->body());

      // append the part response body
      httpResponse->body().appendText(partResponse->body());
      httpResponse->body().appendText(TRI_CHAR_LENGTH_PAIR("\r\n"));
    }

    // we've read the last part
    if (!helper.containsMore) {
      break;
    }
  }

  // append final boundary + "--"
  httpResponse->body().appendText(boundary + "--");

  if (errors > 0) {
    httpResponse->setHeaderNC(StaticStrings::Errors, StringUtils::itoa(errors));
  }

  // success
  return status::DONE;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extract the boundary from the body of a multipart message
////////////////////////////////////////////////////////////////////////////////

bool RestBatchHandler::getBoundaryBody(std::string* result) {
  HttpRequest const* req = dynamic_cast<HttpRequest const*>(_request.get());

  if (req == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
  }

  std::string const& bodyStr = req->body();
  char const* p = bodyStr.c_str();
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

  std::string boundary(p, (q - p));

  *result = boundary;
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extract the boundary from the HTTP header of a multipart message
////////////////////////////////////////////////////////////////////////////////

bool RestBatchHandler::getBoundaryHeader(std::string* result) {
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
  StringUtils::tolowerInPlace(&p);

  if (p != "boundary=") {
    return false;
  }

  std::string boundary = "--" + parts[1].substr(boundaryLength);

  if (boundary.size() < 5) {
    // 3 bytes is min length for boundary (without "--")
    return false;
  }

  *result = boundary;
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extract the boundary of a multipart message
////////////////////////////////////////////////////////////////////////////////

bool RestBatchHandler::getBoundary(std::string* result) {
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

bool RestBatchHandler::extractPart(SearchHelper* helper) {
  TRI_ASSERT(helper->searchStart != nullptr);

  // init the response
  helper->foundStart = nullptr;
  helper->foundLength = 0;
  helper->containsMore = false;
  helper->contentId = 0;
  helper->contentIdLength = 0;

  char const* searchEnd = helper->message->messageEnd;

  if (helper->searchStart >= searchEnd) {
    // we're at the end already
    return false;
  }

  // search for boundary
  char const* found = strstr(helper->searchStart, helper->message->boundary);

  if (found == nullptr) {
    // not contained. this is an error
    return false;
  }

  if (found != helper->searchStart) {
    // boundary not located at beginning. this is an error
    return false;
  }

  found += helper->message->boundaryLength;

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
    char const* colon =
        static_cast<char const*>(memchr(found, (int)':', eol - found));

    if (nullptr == colon) {
      // invalid header, not containing ':'
      return false;
    }

    // set up the key
    std::string key(found, colon - found);
    StringUtils::trimInPlace(key);

    if (key[0] == 'c' || key[0] == 'C') {
      // got an interesting key. now process it
      StringUtils::tolowerInPlace(&key);

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
          LOG(WARN) << "unexpected content-type '" << value
                    << "' for multipart-message. expected: '"
                    << StaticStrings::BatchContentType << "'";
        }
      } else if ("content-id" == key) {
        helper->contentId = colon;
        helper->contentIdLength = eol - colon;
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
  helper->foundStart = found;

  // search for the end of the boundary
  found = strstr(helper->foundStart, helper->message->boundary);

  if (found == nullptr || found >= searchEnd) {
    // did not find the end. this is an error
    return false;
  }

  helper->foundLength = found - helper->foundStart;

  char const* p = found + helper->message->boundaryLength;

  if (p + 2 > searchEnd) {
    // end of boundary is outside the buffer
    return false;
  }

  if (*p != '-' || *(p + 1) != '-') {
    // we've not reached the end yet
    helper->containsMore = true;
    helper->searchStart = found;
  }

  return true;
}
