////////////////////////////////////////////////////////////////////////////////
/// @brief batch request handler
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "RestBatchHandler.h"

#include "Basics/StringUtils.h"
#include "Logger/Logger.h"
#include "HttpServer/HttpServer.h"
#include "Rest/HttpRequest.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::rest;
using namespace triagens::arango;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

RestBatchHandler::RestBatchHandler (HttpRequest* request, TRI_vocbase_t* vocbase)
  : RestVocbaseBaseHandler(request, vocbase),
    _partContentType(HttpRequest::getPartContentType()) {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

RestBatchHandler::~RestBatchHandler () {
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   Handler methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool RestBatchHandler::isDirect () {
  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

string const& RestBatchHandler::queue () const {
  static string const client = "STANDARD";

  return client;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

Handler::status_e RestBatchHandler::execute() {
  // extract the request type
  const HttpRequest::HttpRequestType type = _request->requestType();

  if (type != HttpRequest::HTTP_REQUEST_POST && type != HttpRequest::HTTP_REQUEST_PUT) {
    generateError(HttpResponse::METHOD_NOT_ALLOWED, TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);

    return Handler::HANDLER_DONE;
  }

  string boundary;
  if (! getBoundary(&boundary)) {
    // invalid content-type or boundary sent
    generateError(HttpResponse::BAD, TRI_ERROR_HTTP_BAD_PARAMETER, "invalid content-type or boundary received");

    return Handler::HANDLER_FAILED;
  }

  size_t errors = 0;

  // get authorization header. we will inject this into the subparts
  string authorization = _request->header("authorization");

  // create the response
  _response = createResponse(HttpResponse::OK);
  _response->setContentType(_request->header("content-type"));

  // setup some auxiliary structures to parse the multipart message
  MultipartMessage message(boundary.c_str(), boundary.size(), _request->body(), _request->body() + _request->bodySize());

  SearchHelper helper;
  helper.message = &message;
  helper.searchStart = (char*) message.messageStart;

  // iterate over all parts of the multipart message
  while (true) {
    // get the next part from the multipart message
    if (! extractPart(&helper)) {
      // error
      generateError(HttpResponse::BAD, TRI_ERROR_HTTP_BAD_PARAMETER, "invalid multipart message received");
      LOGGER_WARNING("received a corrupted multipart message");

      return Handler::HANDLER_FAILED;
    }

    // split part into header & body
    const char* partStart = helper.foundStart;
    const char* partEnd = partStart + helper.foundLength;
    const size_t partLength = helper.foundLength;

    const char* headerStart = partStart;
    char* bodyStart = NULL;
    size_t headerLength = 0;
    size_t bodyLength = 0;

    // assume Windows linebreak \r\n\r\n as delimiter
    char* p = strstr((char*) headerStart, "\r\n\r\n");
    if (p != NULL) {
      headerLength = p - partStart;
      bodyStart = p + 4;
      bodyLength = partEnd - bodyStart;
    }
    else {
      // test Unix linebreak
      p = strstr((char*) headerStart, "\n\n");
      if (p != NULL) {
        headerLength = p - partStart;
        bodyStart = p + 2;
        bodyLength = partEnd - bodyStart;
      }
      else {
        // no delimiter found, assume we have only a header
        headerLength = partLength;
      }
    }

    // set up request object for the part
    LOGGER_TRACE("part header is " << string(headerStart, headerLength));
    HttpRequest* request = new HttpRequest(headerStart, headerLength);

    if (bodyLength > 0) {
      LOGGER_TRACE("part body is " << string(bodyStart, bodyLength));
      request->setBody(bodyStart, bodyLength);
    }

    if (authorization.size()) {
      // inject Authorization header of multipart message into part message
      request->setHeader("authorization", 13, authorization.c_str());
    }


    HttpHandler* handler = _server->createHandler(request);
    if (! handler) {
      delete request;

      generateError(HttpResponse::BAD, TRI_ERROR_INTERNAL, "could not create handler for batch part processing");

      return Handler::HANDLER_FAILED;
    }

    Handler::status_e status = Handler::HANDLER_FAILED;
    do {
      try {
        status = handler->execute();
      }
      catch (triagens::basics::TriagensError const& ex) {
        handler->handleError(ex);
      }
      catch (std::exception const& ex) {
        triagens::basics::InternalError err(ex, __FILE__, __LINE__);

        handler->handleError(err);
      }
      catch (...) {
        triagens::basics::InternalError err("executeDirectHandler", __FILE__, __LINE__);
        handler->handleError(err);
      }
    }
    while (status == Handler::HANDLER_REQUEUE);


    if (status == Handler::HANDLER_FAILED) {
      // one of the handlers failed, we must exit now
      delete handler;
      generateError(HttpResponse::BAD, TRI_ERROR_INTERNAL, "executing a handler for batch part failed");

      return Handler::HANDLER_FAILED;
    }

    HttpResponse* partResponse = handler->getResponse();
    if (partResponse == 0) {
      delete handler;
      generateError(HttpResponse::BAD, TRI_ERROR_INTERNAL, "could not create a response for batch part request");

      return Handler::HANDLER_FAILED;
    }

    const HttpResponse::HttpResponseCode code = partResponse->responseCode();
    if (code >= 400) {
      // error
      ++errors;
    }

    // append the boundary for this subpart
    _response->body().appendText(boundary + "\r\nContent-Type: ");
    _response->body().appendText(_partContentType);

    if (helper.contentId != 0) {
      // append content-id
      _response->body().appendText("\r\nContent-Id: " + string(helper.contentId, helper.contentIdLength));
    }

    _response->body().appendText("\r\n\r\n", 4);

    // remove some headers we don't need
    partResponse->setHeader("connection", 10, "");
    partResponse->setHeader("server", 6, "");

    // append the part response header
    partResponse->writeHeader(&_response->body());
    // append the part response body
    _response->body().appendText(partResponse->body());
    _response->body().appendText("\r\n", 2);

    delete handler;


    if (! helper.containsMore) {
      // we've read the last part
      break;
    }
  } // next part

  // append final boundary + "--"
  _response->body().appendText(boundary + "--");

  if (errors > 0) {
    _response->setHeader(HttpResponse::getBatchErrorHeader(), StringUtils::itoa(errors));
  }

  // success
  return Handler::HANDLER_DONE;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extract the boundary of a multipart message
////////////////////////////////////////////////////////////////////////////////

bool RestBatchHandler::getBoundary (string* result) {
  assert(_request);

  // extract content type
  string contentType = StringUtils::trim(_request->header("content-type"));

  // content type is expect to contain a boundary like this:
  // "Content-Type: multipart/form-data; boundary=<boundary goes here>"
  vector<string> parts = StringUtils::split(contentType, ';');
  if (parts.size() != 2 || parts[0] != HttpRequest::getMultipartContentType().c_str()) {
    // content-type is not formatted as expected
    return false;
  }

  static const size_t boundaryLength = 9; // strlen("boundary=");

  // trim 2nd part and lowercase it
  StringUtils::trimInPlace(parts[1]);
  string p = parts[1].substr(0, boundaryLength);
  StringUtils::tolowerInPlace(&p);

  if (p != "boundary=") {
    return false;
  }

  string boundary = "--" + parts[1].substr(boundaryLength);
  if (boundary.size() < 5) {
    // 3 bytes is min length for boundary (without "--")
    return false;
  }

  LOGGER_TRACE("boundary of multipart-message is " << boundary);

  *result = boundary;
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extract the next part from a multipart message
////////////////////////////////////////////////////////////////////////////////

bool RestBatchHandler::extractPart (SearchHelper* helper) {
  assert(helper->searchStart != NULL);

  // init the response
  helper->foundStart = NULL;
  helper->foundLength = 0;
  helper->containsMore = false;
  helper->contentId = 0;
  helper->contentIdLength = 0;

  const char* searchEnd = helper->message->messageEnd;

  if (helper->searchStart >= searchEnd) {
    // we're at the end already
    return false;
  }

  // search for boundary
  char* found = strstr(helper->searchStart, helper->message->boundary);
  if (found == NULL) {
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

  while (found < searchEnd) {
    char* eol = strstr(found, "\r\n");
    if (0 == eol || eol == found) {
      break;
    }

    // split key/value of header
    char* colon = (char*) memchr(found, (int) ':', eol - found);

    if (0 == colon) {
      // invalid header, not containing ':'
      return false;
    }

    // set up key/value pair
    string key(found, colon - found);
    StringUtils::trimInPlace(key);
    StringUtils::tolowerInPlace(&key);

    // skip the colon itself
    ++colon;
    // skip any whitespace
    while (*colon == ' ') {
      ++colon;
    }

    string value(colon, eol - colon);
    StringUtils::trimInPlace(value);

    if ("content-type" == key) {
      if (_partContentType == value) {
        hasTypeHeader = true;
      }
    }
    else if ("content-id" == key) {
      helper->contentId = colon;
      helper->contentIdLength = eol - colon;
    }
    else {
      // ignore other headers
    }

    found = eol + 2;
  }

  found += 2; // for 2nd \r\n

  if (!hasTypeHeader) {
    // no Content-Type header. this is an error
    return false;
  }

  // we're at the start of the body part. set the return value
  helper->foundStart = found;

  // search for the end of the boundary
  found = strstr(helper->foundStart, helper->message->boundary);
  if (found == NULL || found >= searchEnd) {
    // did not find the end. this is an error
    return false;
  }

  helper->foundLength = found - helper->foundStart;

  char* p = found + helper->message->boundaryLength;
  if (p + 2 >= searchEnd) {
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

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
