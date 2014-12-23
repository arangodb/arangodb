////////////////////////////////////////////////////////////////////////////////
/// @brief batch request handler
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2012-2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "RestBatchHandler.h"

#include "Basics/StringUtils.h"
#include "Basics/logging.h"
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
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

RestBatchHandler::RestBatchHandler (HttpRequest* request)
  : RestVocbaseBaseHandler(request),
    _partContentType(HttpRequest::getPartContentType()) {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

RestBatchHandler::~RestBatchHandler () {
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   Handler methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a batch request
///
/// @RESTHEADER{POST /_api/batch,executes a batch request}
///
/// @RESTBODYPARAM{body,string,required}
/// The multipart batch request, consisting of the envelope and the individual
/// batch parts.
///
/// @RESTDESCRIPTION
/// Executes a batch request. A batch request can contain any number of
/// other requests that can be sent to ArangoDB in isolation. The benefit of
/// using batch requests is that batching requests requires less client/server
/// roundtrips than when sending isolated requests.
///
/// All parts of a batch request are executed serially on the server. The
/// server will return the results of all parts in a single response when all
/// parts are finished.
///
/// Technically, a batch request is a multipart HTTP request, with
/// content-type `multipart/form-data`. A batch request consists of an
/// envelope and the individual batch part actions. Batch part actions
/// are "regular" HTTP requests, including full header and an optional body.
/// Multiple batch parts are separated by a boundary identifier. The
/// boundary identifier is declared in the batch envelope. The MIME content-type
/// for each individual batch part must be `application/x-arango-batchpart`.
///
/// Please note that when constructing the individual batch parts, you must
/// use CRLF (`\r\n`) as the line terminator as in regular HTTP messages.
///
/// The response sent by the server will be an `HTTP 200` response, with an
/// optional error summary header `x-arango-errors`. This header contains the
/// number of batch part operations that failed with an HTTP error code of at
/// least 400. This header is only present in the response if the number of
/// errors is greater than zero.
///
/// The response sent by the server is a multipart response, too. It contains
/// the individual HTTP responses for all batch parts, including the full HTTP
/// result header (with status code and other potential headers) and an
/// optional result body. The individual batch parts in the result are
/// seperated using the same boundary value as specified in the request.
///
/// The order of batch parts in the response will be the same as in the
/// original client request. Client can additionally use the `Content-Id`
/// MIME header in a batch part to define an individual id for each batch part.
/// The server will return this id is the batch part responses, too.
///
/// @RESTRETURNCODES
///
/// @RESTRETURNCODE{200}
/// is returned if the batch was received successfully. HTTP 200 is returned
/// even if one or multiple batch part actions failed.
///
/// @RESTRETURNCODE{400}
/// is returned if the batch envelope is malformed or incorrectly formatted.
/// This code will also be returned if the content-type of the overall batch
/// request or the individual MIME parts is not as expected.
///
/// @RESTRETURNCODE{405}
/// is returned when an invalid HTTP method is used.
///
/// @EXAMPLES
///
/// Sending a batch request with five batch parts:
///
/// - GET /_api/version
///
/// - DELETE /_api/collection/products
///
/// - POST /_api/collection/products
///
/// - GET /_api/collection/products/figures
///
/// - DELETE /_api/collection/products
///
/// The boundary (`SomeBoundaryValue`) is passed to the server in the HTTP
/// `Content-Type` HTTP header.
///
/// @EXAMPLE_ARANGOSH_RUN{RestBatchMultipartHeader}
///     var parts = [
///       "Content-Type: application/x-arango-batchpart\r\nContent-Id: myId1\r\n\r\nGET /_api/version HTTP/1.1\r\n",
///       "Content-Type: application/x-arango-batchpart\r\nContent-Id: myId2\r\n\r\nDELETE /_api/collection/products HTTP/1.1\r\n",
///       "Content-Type: application/x-arango-batchpart\r\nContent-Id: someId\r\n\r\nPOST /_api/collection/products HTTP/1.1\r\n\r\n{ \"name\": \"products\" }\r\n",
///       "Content-Type: application/x-arango-batchpart\r\nContent-Id: nextId\r\n\r\nGET /_api/collection/products/figures HTTP/1.1\r\n",
///       "Content-Type: application/x-arango-batchpart\r\nContent-Id: otherId\r\n\r\nDELETE /_api/collection/products HTTP/1.1\r\n"
///     ];
///     var boundary = "SomeBoundaryValue";
///     var headers = { "Content-Type" : "multipart/form-data; boundary=" + boundary };
///     var body = "--" + boundary + "\r\n" +
///                parts.join("\r\n" + "--" + boundary + "\r\n") +
///                "--" + boundary + "--\r\n";
///
///     var response = logCurlRequestRaw('POST', '/_api/batch', body, headers);
///
///     assert(response.code === 200);
///
///     logRawResponse(response);
/// @END_EXAMPLE_ARANGOSH_RUN
///
/// Sending a batch request, setting the boundary implicitly (the server will
/// in this case try to find the boundary at the beginning of the request body).
///
/// @EXAMPLE_ARANGOSH_RUN{RestBatchImplicitBoundary}
///     var parts = [
///       "Content-Type: application/x-arango-batchpart\r\n\r\nDELETE /_api/collection/notexisting1 HTTP/1.1\r\n",
///       "Content-Type: application/x-arango-batchpart\r\n\r\nDELETE /_api/collection/notexisting2 HTTP/1.1\r\n"
///     ];
///     var boundary = "SomeBoundaryValue";
///     var body = "--" + boundary + "\r\n" +
///                parts.join("\r\n" + "--" + boundary + "\r\n") +
///                "--" + boundary + "--\r\n";
///
///     var response = logCurlRequestRaw('POST', '/_api/batch', body);
///
///     assert(response.code === 200);
///     assert(response.headers['x-arango-errors'] == 2);
///
///     logRawResponse(response);
/// @END_EXAMPLE_ARANGOSH_RUN
////////////////////////////////////////////////////////////////////////////////

Handler::status_t RestBatchHandler::execute() {
  // extract the request type
  const HttpRequest::HttpRequestType type = _request->requestType();

  if (type != HttpRequest::HTTP_REQUEST_POST && type != HttpRequest::HTTP_REQUEST_PUT) {
    generateError(HttpResponse::METHOD_NOT_ALLOWED, TRI_ERROR_HTTP_METHOD_NOT_ALLOWED);
    return status_t(Handler::HANDLER_DONE);
  }

  string boundary;

  // invalid content-type or boundary sent
  if (! getBoundary(&boundary)) {
    generateError(HttpResponse::BAD, TRI_ERROR_HTTP_BAD_PARAMETER, "invalid content-type or boundary received");
    return status_t(Handler::HANDLER_FAILED);
  }

  LOG_TRACE("boundary of multipart-message is '%s'", boundary.c_str());

  size_t errors = 0;

  // get authorization header. we will inject this into the subparts
  string authorization = _request->header("authorization");

  // create the response
  _response = createResponse(HttpResponse::OK);
  _response->setContentType(_request->header("content-type"));

  // setup some auxiliary structures to parse the multipart message
  MultipartMessage message(boundary.c_str(),
                           boundary.size(),
                           _request->body(),
                           _request->body() + _request->bodySize());

  SearchHelper helper;
  helper.message = &message;
  helper.searchStart = (char*) message.messageStart;

  // iterate over all parts of the multipart message
  while (true) {
    // get the next part from the multipart message
    if (! extractPart(&helper)) {
      // error
      generateError(HttpResponse::BAD, TRI_ERROR_HTTP_BAD_PARAMETER, "invalid multipart message received");
      LOG_WARNING("received a corrupted multipart message");

      return status_t(Handler::HANDLER_FAILED);
    }

    // split part into header & body
    const char* partStart = helper.foundStart;
    const char* partEnd = partStart + helper.foundLength;
    const size_t partLength = helper.foundLength;

    const char* headerStart = partStart;
    char* bodyStart = 0;
    size_t headerLength = 0;
    size_t bodyLength = 0;

    // assume Windows linebreak \r\n\r\n as delimiter
    char* p = strstr((char*) headerStart, "\r\n\r\n");

    if (p != 0 && p + 4 <= partEnd) {
      headerLength = p - partStart;
      bodyStart = p + 4;
      bodyLength = partEnd - bodyStart;
    }
    else {
      // test Unix linebreak
      p = strstr((char*) headerStart, "\n\n");

      if (p != 0 && p + 2 <= partEnd) {
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
    LOG_TRACE("part header is: %s", string(headerStart, headerLength).c_str());
    HttpRequest* request = new HttpRequest(_request->connectionInfo(), headerStart, headerLength, _request->compatibility(), false);

    if (request == 0) {
      generateError(HttpResponse::SERVER_ERROR, TRI_ERROR_OUT_OF_MEMORY);

      return status_t(Handler::HANDLER_FAILED);
    }

    // inject the request context from the framing (batch) request
    // the "false" means the context is not responsible for resource handling
    request->setRequestContext(_request->getRequestContext(), false);
    request->setDatabaseName(_request->databaseName());

    if (bodyLength > 0) {
      LOG_TRACE("part body is '%s'", string(bodyStart, bodyLength).c_str());
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

      return status_t(Handler::HANDLER_FAILED);
    }

    Handler::status_t status(Handler::HANDLER_FAILED);

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
    while (status.status == Handler::HANDLER_REQUEUE);


    if (status.status == Handler::HANDLER_FAILED) {
      // one of the handlers failed, we must exit now
      delete handler;
      generateError(HttpResponse::BAD, TRI_ERROR_INTERNAL, "executing a handler for batch part failed");

      return status_t(Handler::HANDLER_FAILED);
    }

    HttpResponse* partResponse = handler->getResponse();

    if (partResponse == nullptr) {
      delete handler;
      generateError(HttpResponse::BAD, TRI_ERROR_INTERNAL, "could not create a response for batch part request");

      return status_t(Handler::HANDLER_FAILED);
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
  return status_t(Handler::HANDLER_DONE);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extract the boundary from the body of a multipart message
////////////////////////////////////////////////////////////////////////////////

bool RestBatchHandler::getBoundaryBody (string* result) {
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

  const char* q = p;

  while (q < e &&
         *q &&
         *q != ' ' && *q != '\t' && *q != '\r' && *q != '\n') {
    ++q;
  }

  if ((q - p) < 5) {
    // 3 bytes is min length for boundary (without "--")
    return false;
  }

  string boundary(p, (q - p));

  *result = boundary;
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extract the boundary from the HTTP header of a multipart message
////////////////////////////////////////////////////////////////////////////////

bool RestBatchHandler::getBoundaryHeader (string* result) {
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

  *result = boundary;
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extract the boundary of a multipart message
////////////////////////////////////////////////////////////////////////////////

bool RestBatchHandler::getBoundary (string* result) {
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

bool RestBatchHandler::extractPart (SearchHelper* helper) {
  TRI_ASSERT(helper->searchStart != nullptr);

  // init the response
  helper->foundStart = nullptr;
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
    }
    else {
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
    char* colon = (char*) memchr(found, (int) ':', eol - found);

    if (nullptr == colon) {
      // invalid header, not containing ':'
      return false;
    }

    // set up the key
    string key(found, colon - found);
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
        string value(colon, eol - colon);
        StringUtils::trimInPlace(value);

        if (_partContentType == value) {
          hasTypeHeader = true;
        }
        else {
          LOG_WARNING("unexpected content-type '%s' for multipart-message. expected: '%s'",
                      value.c_str(),
                      _partContentType.c_str());
        }
      }
      else if ("content-id" == key) {
        helper->contentId = colon;
        helper->contentIdLength = eol - colon;
      }
      else {
        // ignore other headers
      }
    }

    found = eol + breakLength; // plus the \n
  }

  found += breakLength; // for 2nd \n

  if (! hasTypeHeader) {
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

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
