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

#include "Basics/StringUtils.h"
#include "Basics/Logger.h"
#include "HttpServer/HttpHandlerFactory.h"
#include "HttpServer/HttpServer.h"
#include "Rest/HttpRequest.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

RestBatchHandler::RestBatchHandler(HttpRequest* request)
    : RestVocbaseBaseHandler(request) {}

RestBatchHandler::~RestBatchHandler() {}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_batch_processing
////////////////////////////////////////////////////////////////////////////////

HttpHandler::status_t RestBatchHandler::execute() {
  // extract the request type
  const HttpRequest::HttpRequestType type = _request->requestType();

  if (type != HttpRequest::HTTP_REQUEST_POST &&
      type != HttpRequest::HTTP_REQUEST_PUT) {
    generateError(HttpResponse::METHOD_NOT_ALLOWED,
                  TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
    return status_t(HttpHandler::HANDLER_DONE);
  }

  std::string boundary;

  // invalid content-type or boundary sent
  if (!getBoundary(&boundary)) {
    generateError(HttpResponse::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                  "invalid content-type or boundary received");
    return status_t(HttpHandler::HANDLER_FAILED);
  }

  LOG(TRACE) << "boundary of multipart-message is '" << boundary.c_str() << "'";

  size_t errors = 0;

  // get authorization header. we will inject this into the subparts
  std::string authorization = _request->header("authorization");

  // create the response
  createResponse(HttpResponse::OK);
  _response->setContentType(_request->header("content-type"));

  // setup some auxiliary structures to parse the multipart message
  MultipartMessage message(boundary.c_str(), boundary.size(), _request->body(),
                           _request->body() + _request->bodySize());

  SearchHelper helper;
  helper.message = &message;
  helper.searchStart = (char*)message.messageStart;

  // iterate over all parts of the multipart message
  while (true) {
    // get the next part from the multipart message
    if (!extractPart(&helper)) {
      // error
      generateError(HttpResponse::BAD, TRI_ERROR_HTTP_BAD_PARAMETER,
                    "invalid multipart message received");
      LOG(WARN) << "received a corrupted multipart message";

      return status_t(HttpHandler::HANDLER_FAILED);
    }

    // split part into header & body
    char const* partStart = helper.foundStart;
    char const* partEnd = partStart + helper.foundLength;
    size_t const partLength = helper.foundLength;

    char const* headerStart = partStart;
    char* bodyStart = nullptr;
    size_t headerLength = 0;
    size_t bodyLength = 0;

    // assume Windows linebreak \r\n\r\n as delimiter
    char* p = strstr((char*)headerStart, "\r\n\r\n");

    if (p != nullptr && p + 4 <= partEnd) {
      headerLength = p - partStart;
      bodyStart = p + 4;
      bodyLength = partEnd - bodyStart;
    } else {
      // test Unix linebreak
      p = strstr((char*)headerStart, "\n\n");

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
    LOG(TRACE) << "part header is: " << std::string(headerStart, headerLength).c_str();
    HttpRequest* request =
        new HttpRequest(_request->connectionInfo(), headerStart, headerLength,
                        _request->compatibility(), false);

    if (request == nullptr) {
      generateError(HttpResponse::SERVER_ERROR, TRI_ERROR_OUT_OF_MEMORY);

      return status_t(HttpHandler::HANDLER_FAILED);
    }

    // we do not have a client task id here
    request->setClientTaskId(0);

    // inject the request context from the framing (batch) request
    // the "false" means the context is not responsible for resource handling
    request->setRequestContext(_request->getRequestContext(), false);
    request->setDatabaseName(_request->databaseName());

    if (bodyLength > 0) {
      LOG(TRACE) << "part body is '" << std::string(bodyStart, bodyLength).c_str() << "'";
      request->setBody(bodyStart, bodyLength);
    }

    if (authorization.size()) {
      // inject Authorization header of multipart message into part message
      request->setHeader("authorization", 13, authorization.c_str());
    }

    HttpHandler* handler = _server->createHandler(request);

    if (!handler) {
      delete request;

      generateError(HttpResponse::BAD, TRI_ERROR_INTERNAL,
                    "could not create handler for batch part processing");

      return status_t(HttpHandler::HANDLER_FAILED);
    }

    // start to work for this handler
    {
      HandlerWorkStack work(handler);
      HttpHandler::status_t status = handler->executeFull();

      if (status._status == HttpHandler::HANDLER_FAILED) {
        generateError(HttpResponse::BAD, TRI_ERROR_INTERNAL,
                      "executing a handler for batch part failed");

        return status_t(HttpHandler::HANDLER_FAILED);
      }

      HttpResponse* partResponse = handler->getResponse();

      if (partResponse == nullptr) {
        generateError(HttpResponse::BAD, TRI_ERROR_INTERNAL,
                      "could not create a response for batch part request");

        return status_t(HttpHandler::HANDLER_FAILED);
      }

      const HttpResponse::HttpResponseCode code = partResponse->responseCode();

      // count everything above 400 as error
      if (code >= 400) {
        ++errors;
      }

      // append the boundary for this subpart
      _response->body().appendText(boundary + "\r\nContent-Type: ");
      _response->body().appendText(
          arangodb::rest::HttpRequest::BatchContentType);

      // append content-id if it is present
      if (helper.contentId != 0) {
        _response->body().appendText(
            "\r\nContent-Id: " +
            std::string(helper.contentId, helper.contentIdLength));
      }

      _response->body().appendText(TRI_CHAR_LENGTH_PAIR("\r\n\r\n"));

      // remove some headers we don't need
      partResponse->setHeader(TRI_CHAR_LENGTH_PAIR("connection"), "");
      partResponse->setHeader(TRI_CHAR_LENGTH_PAIR("server"), "");

      // append the part response header
      partResponse->writeHeader(&_response->body());

      // append the part response body
      _response->body().appendText(partResponse->body());
      _response->body().appendText(TRI_CHAR_LENGTH_PAIR("\r\n"));
    }

    // we've read the last part
    if (!helper.containsMore) {
      break;
    }
  }

  // append final boundary + "--"
  _response->body().appendText(boundary + "--");

  if (errors > 0) {
    _response->setHeader(HttpResponse::BatchErrorHeader,
                         StringUtils::itoa(errors));
  }

  // success
  return status_t(HttpHandler::HANDLER_DONE);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extract the boundary from the body of a multipart message
////////////////////////////////////////////////////////////////////////////////

bool RestBatchHandler::getBoundaryBody(std::string* result) {
  char const* p = _request->body();
  char const* e = p + _request->bodySize();

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
  std::string contentType = StringUtils::trim(_request->header("content-type"));

  // content type is expect to contain a boundary like this:
  // "Content-Type: multipart/form-data; boundary=<boundary goes here>"
  std::vector<std::string> parts = StringUtils::split(contentType, ';');

  if (parts.size() != 2 || parts[0] != HttpRequest::MultiPartContentType) {
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
  char* found = strstr(helper->searchStart, helper->message->boundary);

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

    char* eol = strstr(found, "\r\n");

    if (eol == nullptr) {
      breakLength = 1;
      eol = strchr(found, '\n');

      if (eol == found) {
        break;
      }
    } else {
      char* eol2 = strchr(found, '\n');

      if (eol2 != nullptr && eol2 < eol) {
        breakLength = 1;
        eol = eol2;
      }
    }

    if (eol == nullptr || eol == found) {
      break;
    }

    // split key/value of header
    char* colon = (char*)memchr(found, (int)':', eol - found);

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

      if ("content-type" == key) {
        // extract the value, too
        std::string value(colon, eol - colon);
        StringUtils::trimInPlace(value);

        if (arangodb::rest::HttpRequest::BatchContentType == value) {
          hasTypeHeader = true;
        } else {
          LOG(WARN) << "unexpected content-type '" << value.c_str() << "' for multipart-message. expected: '" << arangodb::rest::HttpRequest::BatchContentType.c_str() << "'";
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

  char* p = found + helper->message->boundaryLength;

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
