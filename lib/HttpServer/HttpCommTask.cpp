////////////////////////////////////////////////////////////////////////////////
/// @brief task for http communication
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2012 triAGENS GmbH, Cologne, Germany
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
/// @author Dr. Frank Celler
/// @author Achim Brandt
/// @author Copyright 2009-2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "HttpCommTask.h"

#include "Basics/StringUtils.h"
#include "HttpServer/HttpHandler.h"
#include "HttpServer/HttpHandlerFactory.h"
#include "HttpServer/HttpServer.h"
#include "Rest/HttpRequest.h"
#include "Rest/HttpResponse.h"
#include "Scheduler/Scheduler.h"

using namespace triagens::basics;
using namespace triagens::rest;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup HttpServer
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief constructors
////////////////////////////////////////////////////////////////////////////////

HttpCommTask::HttpCommTask (HttpServer* server, socket_t fd, ConnectionInfo const& info)
  : Task("HttpCommTask"),
    GeneralCommTask<HttpServer, HttpHandlerFactory>(server, fd, info) {
  ConnectionStatisticsAgentSetHttp(this);
  ConnectionStatisticsAgent::release();

  ConnectionStatisticsAgent::acquire();
  ConnectionStatisticsAgentSetStart(this);
  ConnectionStatisticsAgentSetHttp(this);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructors
////////////////////////////////////////////////////////////////////////////////

HttpCommTask::~HttpCommTask () {
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                           GeneralCommTask methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup HttpServer
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool HttpCommTask::processRead () {
  if (_requestPending || _readBuffer->c_str() == 0) {
    return true;
  }

  bool handleRequest = false;

  if (! _readRequestBody) {
#ifdef TRI_ENABLE_FIGURES

    if (_readPosition == 0 && _readBuffer->c_str() != _readBuffer->end()) {
      RequestStatisticsAgent::acquire();
      RequestStatisticsAgentSetReadStart(this);
    }

#endif

    const char * ptr = _readBuffer->c_str() + _readPosition;
    const char * end = _readBuffer->end() - 3;

    for (;  ptr < end;  ptr++) {
      if (ptr[0] == '\r' && ptr[1] == '\n' && ptr[2] == '\r' && ptr[3] == '\n') {
        break;
      }
    }

    size_t headerLength = ptr - _readBuffer->c_str();

    if (headerLength > _maximalHeaderSize) {
      LOGGER_WARNING << "maximal header size is " << _maximalHeaderSize << ", request header size is "
                     << headerLength;
      return false;
    }

    if (ptr < end) {
      _readPosition = ptr - _readBuffer->c_str() + 4;

      LOGGER_TRACE << "HTTP READ FOR " << static_cast<Task*>(this) << ":\n"
                   << string(_readBuffer->c_str(), _readPosition);

      // check that we know, how to serve this request
      _request = _server->createRequest(_readBuffer->c_str(), _readPosition);

      if (_request == 0) {
        LOGGER_ERROR << "cannot generate request";
        return false;
      }

      // update the connection information, i. e. client and server addresses and ports
      _request->setConnectionInfo(_connectionInfo);

      LOGGER_TRACE << "server port = " << _connectionInfo.serverPort << ", client port = " << _connectionInfo.clientPort;

      // set body start to current position
      _bodyPosition = _readPosition;

      // and different methods
      switch (_request->requestType()) {
        case HttpRequest::HTTP_REQUEST_GET:
        case HttpRequest::HTTP_REQUEST_DELETE:
        case HttpRequest::HTTP_REQUEST_HEAD:
          _bodyLength = _request->contentLength();

          if (_bodyLength > 0) {
            LOGGER_WARNING << "received http GET/DELETE/HEAD request with body length, this should not happen";
            _readRequestBody = true;
          }
          else {
            handleRequest = true;
          }
          break;

        case HttpRequest::HTTP_REQUEST_POST:
        case HttpRequest::HTTP_REQUEST_PUT:
          _bodyLength = _request->contentLength();

          if (_bodyLength > 0) {
            _readRequestBody = true;
          }
          else {
            handleRequest = true;
          }
          break;

        default:
          LOGGER_WARNING << "got corrupted HTTP request '" << string(_readBuffer->c_str(), (_readPosition < 6 ? _readPosition : 6)) << "'";
          return false;
      }

      // check for a 100-continue
      if (_readRequestBody) {
        bool found;
        string const& expect = _request->header("expect", found);

        if (found && StringUtils::trim(expect) == "100-continue") {
          LOGGER_TRACE << "received a 100-continue request";

          StringBuffer* buffer = new StringBuffer(TRI_UNKNOWN_MEM_ZONE);
          buffer->appendText("HTTP/1.1 100 (Continue)\r\n\r\n");

          _writeBuffers.push_back(buffer);

#ifdef TRI_ENABLE_FIGURES

          _writeBuffersStats.push_back(0);

#endif

          fillWriteBuffer();
        }
      }
    }
    else {
      if (_readBuffer->c_str() < end) {
        _readPosition = end - _readBuffer->c_str();
      }
    }
  }

  // readRequestBody might have changed, so cannot use else
  if (_readRequestBody) {
    if (_bodyLength > _maximalBodySize) {
      LOGGER_WARNING << "maximal body size is " << _maximalBodySize << ", request body size is " << _bodyLength;
      return false;
    }

    if (_readBuffer->length() - _bodyPosition < _bodyLength) {
      return true;
    }

    // read "bodyLength" from read buffer and add this body to "httpRequest"
    _request->setBody(_readBuffer->c_str() + _bodyPosition, _bodyLength);

    LOGGER_TRACE << string(_readBuffer->c_str() + _bodyPosition, _bodyLength);

    // remove body from read buffer and reset read position
    _readRequestBody = false;
    handleRequest = true;
  }

  // we have to delete request in here or pass it to a handler, which will delete it
  if (handleRequest) {
    RequestStatisticsAgentSetReadEnd(this);
    RequestStatisticsAgentAddReceivedBytes(this, _bodyPosition + _bodyLength);

    _readBuffer->erase_front(_bodyPosition + _bodyLength);

    _requestPending = true;

    string connectionType = StringUtils::tolower(StringUtils::trim(_request->header("connection")));

    if (connectionType == "close") {
      LOGGER_DEBUG << "connection close requested by client";
      _closeRequested = true;
    }
    else if (_server->getCloseWithoutKeepAlive() && connectionType != "keep-alive") {
      LOGGER_DEBUG << "no keep-alive, connection close requested by client";
      _closeRequested = true;
    }

    _readPosition = 0;
    _bodyPosition = 0;
    _bodyLength = 0;

    bool auth = _server->authenticateRequest(_request);

    // authenticated
    if (auth) {
      HttpHandler* handler = _server->createHandler(_request);
      bool ok = false;

      if (handler == 0) {
        LOGGER_TRACE << "no handler is known, giving up";
        delete _request;
        _request = 0;

        HttpResponse response(HttpResponse::NOT_FOUND);
        handleResponse(&response);
      }
      else {
        this->RequestStatisticsAgent::transfer(handler);

        _request = 0;
        ok = _server->handleRequest(this, handler);

        if (! ok) {
          HttpResponse response(HttpResponse::SERVER_ERROR);
          handleResponse(&response);
        }
      }
    }

    // not authenticated
    else {
      string realm = "basic realm=\"" + _server->authenticationRealm(_request) + "\"";

      delete _request;
      _request = 0;

      HttpResponse response(HttpResponse::UNAUTHORIZED);
      response.setHeader("www-authenticate", realm.c_str());

      handleResponse(&response);
    }

    return processRead();
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void HttpCommTask::addResponse (HttpResponse* response) {
  StringBuffer * buffer;

  // save header
  buffer = new StringBuffer(TRI_UNKNOWN_MEM_ZONE);
  response->writeHeader(buffer);
  buffer->appendText(response->body());

  _writeBuffers.push_back(buffer);

#ifdef TRI_ENABLE_FIGURES

  _writeBuffersStats.push_back(RequestStatisticsAgent::transfer());

#endif

  LOGGER_TRACE << "HTTP WRITE FOR " << static_cast<Task*>(this) << ":\n" << buffer->c_str();

  // clear body
  response->body().clear();

  // start output
  fillWriteBuffer();
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}\\)"
// End:
