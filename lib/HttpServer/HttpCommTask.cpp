////////////////////////////////////////////////////////////////////////////////
/// @brief task for http communication
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2011 triagens GmbH, Cologne, Germany
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
/// @author Copyright 2009-2011, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "HttpCommTask.h"

#include <Basics/StringUtils.h>
#include <Rest/HttpRequest.h>
#include <Rest/HttpResponse.h>

#include "Scheduler/Scheduler.h"
#include "GeneralServer/GeneralFigures.h"
#include "HttpServer/HttpHandlerFactory.h"
#include "HttpServer/HttpHandler.h"
#include "HttpServer/HttpServerImpl.h"

using namespace triagens::basics;
using namespace triagens::rest::GeneralFigures;

namespace triagens {
  namespace rest {

    // -----------------------------------------------------------------------------
    // constructors and destructors
    // -----------------------------------------------------------------------------

    HttpCommTask::HttpCommTask (HttpServerImpl* server, socket_t fd, ConnectionInfo const& info)
      : Task("HttpCommTask"),
        GeneralCommTask<HttpServerImpl, HttpHandlerFactory>(server, fd, info), _handler(0) {
      incCounter<GeneralServerStatistics::httpAccessor>();
    }



    HttpCommTask::~HttpCommTask () {
      decCounter<GeneralServerStatistics::httpAccessor>();
      destroyHandler();      
    }

    // -----------------------------------------------------------------------------
    // GeneralCommTask methods
    // -----------------------------------------------------------------------------

    bool HttpCommTask::processRead () {
      if (requestPending || readBuffer->c_str() == 0) {
        return true;
      }

      bool handleRequest = false;

      if (! readRequestBody) {
        const char * ptr = readBuffer->c_str() + readPosition;
        const char * end = readBuffer->end() - 3;

        for (;  ptr < end;  ptr++) {
          if (ptr[0] == '\r' && ptr[1] == '\n' && ptr[2] == '\r' && ptr[3] == '\n') {
            break;
          }
        }

        size_t headerLength = ptr - readBuffer->c_str();

        if (headerLength > maximalHeaderSize) {
          LOGGER_WARNING << "maximal header size is " << maximalHeaderSize << ", request header size is "
                         << headerLength;
          return false;
        }

        if (ptr < end) {
          readPosition = ptr - readBuffer->c_str() + 4;

          LOGGER_TRACE << "HTTP READ FOR " << static_cast<Task*>(this) << ":\n"
                       << string(readBuffer->c_str(), readPosition);

          // check that we know, how to serve this request
          request = server->createRequest(readBuffer->c_str(), readPosition);

          if (request == 0) {
            LOGGER_ERROR << "cannot generate request";
            return false;
          }

          // update the connection information, i. e. client and server addresses and ports
          request->setConnectionInfo(connectionInfo);

          LOGGER_TRACE << "server port = " << connectionInfo.serverPort << ", client port = " << connectionInfo.clientPort;

          // set body start to current position
          bodyPosition = readPosition;

          // and different methods
          switch (request->requestType()) {
            case HttpRequest::HTTP_REQUEST_GET:
            case HttpRequest::HTTP_REQUEST_DELETE:
            case HttpRequest::HTTP_REQUEST_HEAD:
              bodyLength = request->contentLength();

              if (bodyLength > 0) {
                LOGGER_WARNING << "received http GET/DELETE/HEAD request with body length, this should not happen";
                readRequestBody = true;
              }
              else {
                handleRequest = true;
              }
              break;

            case HttpRequest::HTTP_REQUEST_POST:
            case HttpRequest::HTTP_REQUEST_PUT:
              bodyLength = request->contentLength();

              if (bodyLength > 0) {
                readRequestBody = true;
              }
              else {
                handleRequest = true;
              }
              break;

            default:
              LOGGER_WARNING << "got corrupted HTTP request '" << string(readBuffer->c_str(), (readPosition < 6 ? readPosition : 6)) << "'";
              return false;
          }

          // check for a 100-continue
          if (readRequestBody) {
            bool found;
            string const& expect = request->header("expect", found);

            if (found && StringUtils::trim(expect) == "100-continue") {
              LOGGER_TRACE << "received a 100-continue request";

              StringBuffer* buffer = new StringBuffer(TRI_UNKNOWN_MEM_ZONE);
              buffer->appendText("HTTP/1.1 100 (Continue)\r\n\r\n");

              writeBuffers.push_back(buffer);
              fillWriteBuffer();
            }
          }
        }
        else {
          if (readBuffer->c_str() < end) {
            readPosition = end - readBuffer->c_str();
          }
        }
      }

      // readRequestBody might have changed, so cannot use else
      if (readRequestBody) {
        if (bodyLength > maximalBodySize) {
          LOGGER_WARNING << "maximal body size is " << maximalBodySize << ", request body size is " << bodyLength;
          return false;
        }

        if (readBuffer->length() - bodyPosition < bodyLength) {
          return true;
        }

        // read "bodyLength" from read buffer and add this body to "httpRequest"
        request->setBody(readBuffer->c_str() + bodyPosition, bodyLength);

        LOGGER_TRACE << string(readBuffer->c_str() + bodyPosition, bodyLength);

        // remove body from read buffer and reset read position
        readRequestBody = false;
        handleRequest = true;
      }

      // we have to delete request in here or pass it to a handler, which will delete it
      if (handleRequest) {
        readBuffer->erase_front(bodyPosition + bodyLength);

        requestPending = true;

        string connectionType = StringUtils::tolower(StringUtils::trim(request->header("connection")));

        if (connectionType == "close") {
          LOGGER_DEBUG << "connection close requested by client";
          closeRequested = true;
        }
        else if (server->getCloseWithoutKeepAlive() && connectionType != "keep-alive") {
          LOGGER_DEBUG << "no keep-alive, connection close requested by client";
          closeRequested = true;
        }

        readPosition = 0;
        bodyPosition = 0;
        bodyLength = 0;

         _handler = server->createHandler(request);
        bool ok = false;

        if (_handler == 0) {
          LOGGER_TRACE << "no handler is known, giving up";
          delete request;
          request = 0;

          HttpResponse response(HttpResponse::NOT_FOUND);
          handleResponse(&response);
        }
        else {
          // let the handler know the comm task
          _handler->setTask(this);

          request = 0;
          ok = server->handleRequest(this, _handler);

          if (! ok) {
            HttpResponse response(HttpResponse::SERVER_ERROR);
            handleResponse(&response);
          }
        }

        return processRead();
      }

      return true;
    }


    void HttpCommTask::addResponse (HttpResponse* response) {
      StringBuffer * buffer;

      // save header
      buffer = new StringBuffer(TRI_UNKNOWN_MEM_ZONE);
      response->writeHeader(buffer);
      buffer->appendText(response->body());

      writeBuffers.push_back(buffer);

      LOGGER_TRACE << "HTTP WRITE FOR " << static_cast<Task*>(this) << ":\n" << buffer->c_str();

      // clear body
      response->body().clear();

      // start output
      fillWriteBuffer();
    }
    
    ////////////////////////////////////////////////////////////////////////////////
    /// @brief destroy the handler if any present
    ////////////////////////////////////////////////////////////////////////////////
    
    void HttpCommTask::destroyHandler () {
      if (_handler) {
        _handler->setTask(0);
        server->destroyHandler(_handler);
        _handler = 0;
      }
    }

  }
}
