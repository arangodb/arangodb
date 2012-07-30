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

#ifndef TRIAGENS_HTTP_SERVER_HTTP_COMM_TASK_H
#define TRIAGENS_HTTP_SERVER_HTTP_COMM_TASK_H 1

#include "GeneralServer/GeneralCommTask.h"

#include "Basics/StringUtils.h"
#include "Basics/StringBuffer.h"

#include "Scheduler/ListenTask.h"
#include "HttpServer/HttpHandler.h"
#include "HttpServer/HttpHandlerFactory.h"
#include "Rest/HttpRequest.h"
#include "Rest/HttpResponse.h"
#include "Scheduler/Scheduler.h"

// -----------------------------------------------------------------------------
// --SECTION--                                              forward declarations
// -----------------------------------------------------------------------------

namespace triagens {
  namespace rest {

// -----------------------------------------------------------------------------
// --SECTION--                                                class HttpCommTask
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup HttpServer
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief task for http communication
////////////////////////////////////////////////////////////////////////////////

    template<typename S>
    class HttpCommTask : public GeneralCommTask<S, HttpHandlerFactory>,
                         public RequestStatisticsAgent {

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup HttpServer
/// @{
////////////////////////////////////////////////////////////////////////////////

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructors
////////////////////////////////////////////////////////////////////////////////

        HttpCommTask (S* server, 
                      socket_t fd, 
                      ConnectionInfo const& info) 
        : Task("HttpCommTask"),
          GeneralCommTask<S, HttpHandlerFactory>(server, fd, info) {
          ConnectionStatisticsAgentSetHttp(this);
          ConnectionStatisticsAgent::release();

          ConnectionStatisticsAgent::acquire();
          ConnectionStatisticsAgentSetStart(this);
          ConnectionStatisticsAgentSetHttp(this);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief destructors
////////////////////////////////////////////////////////////////////////////////

        virtual ~HttpCommTask () {
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

      protected:

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

        bool processRead () {
          if (this->_requestPending || this->_readBuffer->c_str() == 0) {
            return true;
          }

          bool handleRequest = false;

          if (! this->_readRequestBody) {
#ifdef TRI_ENABLE_FIGURES

            if (this->_readPosition == 0 && this->_readBuffer->c_str() != this->_readBuffer->end()) {
              RequestStatisticsAgent::acquire();
              RequestStatisticsAgentSetReadStart(this);
            }

#endif

            const char * ptr = this->_readBuffer->c_str() + this->_readPosition;
            const char * end = this->_readBuffer->end() - 3;

            for (;  ptr < end;  ptr++) {
              if (ptr[0] == '\r' && ptr[1] == '\n' && ptr[2] == '\r' && ptr[3] == '\n') {
                break;
              }
            }

            size_t headerLength = ptr - this->_readBuffer->c_str();

            if (headerLength > this->_maximalHeaderSize) {
              LOGGER_WARNING << "maximal header size is " << this->_maximalHeaderSize << ", request header size is "
                << headerLength;
              return false;
            }

            if (ptr < end) {
              this->_readPosition = ptr - this->_readBuffer->c_str() + 4;

              LOGGER_TRACE << "HTTP READ FOR " << static_cast<Task*>(this) << ":\n"
                << string(this->_readBuffer->c_str(), this->_readPosition);

              // check that we know, how to serve this request
              this->_request = this->_server->getHandlerFactory()->createRequest(this->_readBuffer->c_str(), this->_readPosition);

              if (this->_request == 0) {
                LOGGER_ERROR << "cannot generate request";
                return false;
              }

              // update the connection information, i. e. client and server addresses and ports
              this->_request->setConnectionInfo(this->_connectionInfo);

              LOGGER_TRACE << "server port = " << this->_connectionInfo.serverPort << ", client port = " << this->_connectionInfo.clientPort;

              // set body start to current position
              this->_bodyPosition = this->_readPosition;

              // and different methods
              switch (this->_request->requestType()) {
                case HttpRequest::HTTP_REQUEST_GET:
                case HttpRequest::HTTP_REQUEST_DELETE:
                case HttpRequest::HTTP_REQUEST_HEAD:
                  this->_bodyLength = this->_request->contentLength();

                  if (this->_bodyLength > 0) {
                    LOGGER_WARNING << "received http GET/DELETE/HEAD request with body length, this should not happen";
                    this->_readRequestBody = true;
                  }
                  else {
                    handleRequest = true;
                  }
                  break;

                case HttpRequest::HTTP_REQUEST_POST:
                case HttpRequest::HTTP_REQUEST_PUT:
                  this->_bodyLength = this->_request->contentLength();

                  if (this->_bodyLength > 0) {
                    this->_readRequestBody = true;
                  }
                  else {
                    handleRequest = true;
                  }
                  break;

                default:
                  LOGGER_WARNING << "got corrupted HTTP request '" << string(this->_readBuffer->c_str(), (this->_readPosition < 6 ? this->_readPosition : 6)) << "'";
                  return false;
              }

              // check for a 100-continue
              if (this->_readRequestBody) {
                bool found;
                string const& expect = this->_request->header("expect", found);

                if (found && triagens::basics::StringUtils::trim(expect) == "100-continue") {
                  LOGGER_TRACE << "received a 100-continue request";

                  triagens::basics::StringBuffer* buffer = new triagens::basics::StringBuffer(TRI_UNKNOWN_MEM_ZONE);
                  buffer->appendText("HTTP/1.1 100 (Continue)\r\n\r\n");

                  this->_writeBuffers.push_back(buffer);

#ifdef TRI_ENABLE_FIGURES

                  this->_writeBuffersStats.push_back(0);

#endif

                  this->fillWriteBuffer();
                }
              }
            }
            else {
              if (this->_readBuffer->c_str() < end) {
                this->_readPosition = end - this->_readBuffer->c_str();
              }
            }
          }

          // readRequestBody might have changed, so cannot use else
          if (this->_readRequestBody) {
            if (this->_bodyLength > this->_maximalBodySize) {
              LOGGER_WARNING << "maximal body size is " << this->_maximalBodySize << ", request body size is " << this->_bodyLength;
              return false;
            }

            if (this->_readBuffer->length() - this->_bodyPosition < this->_bodyLength) {
              return true;
            }

            // read "bodyLength" from read buffer and add this body to "httpRequest"
            this->_request->setBody(this->_readBuffer->c_str() + this->_bodyPosition, this->_bodyLength);

            LOGGER_TRACE << string(this->_readBuffer->c_str() + this->_bodyPosition, this->_bodyLength);

            // remove body from read buffer and reset read position
            this->_readRequestBody = false;
            handleRequest = true;
          }

          // we have to delete request in here or pass it to a handler, which will delete it
          if (handleRequest) {
            RequestStatisticsAgentSetReadEnd(this);
            RequestStatisticsAgentAddReceivedBytes(this, this->_bodyPosition + this->_bodyLength);

            this->_readBuffer->erase_front(this->_bodyPosition + this->_bodyLength);

            this->_requestPending = true;

            string connectionType = triagens::basics::StringUtils::tolower(this->_request->header("connection"));

            if (connectionType == "close") {
              // client has sent an explicit "Connection: Close" header. we should close the connection
              LOGGER_DEBUG << "connection close requested by client";
              this->_closeRequested = true;
            }
            else if (this->_request->isHttp10() && connectionType != "keep-alive") {
              // HTTP 1.0 request, and no "Connection: Keep-Alive" header sent
              // we should close the connection
              LOGGER_DEBUG << "no keep-alive, connection close requested by client";
              this->_closeRequested = true;
            }
            // we keep the connection open in all other cases (HTTP 1.1 or Keep-Alive header sent)

            this->_readPosition = 0;
            this->_bodyPosition = 0;
            this->_bodyLength = 0;

            bool auth = this->_server->getHandlerFactory()->authenticateRequest(this->_request);

            // authenticated
            if (auth) {
              HttpHandler* handler = this->_server->getHandlerFactory()->createHandler(this->_request);
              bool ok = false;

              if (handler == 0) {
                LOGGER_TRACE << "no handler is known, giving up";
                delete this->_request;
                this->_request = 0;

                HttpResponse response(HttpResponse::NOT_FOUND);
                this->handleResponse(&response);
              }
              else {
                this->RequestStatisticsAgent::transfer(handler);

                this->_request = 0;
                ok = this->_server->handleRequest(this, handler);

                if (! ok) {
                  HttpResponse response(HttpResponse::SERVER_ERROR);
                  this->handleResponse(&response);
                }
              }
            }

            // not authenticated
            else {
              string realm = "basic realm=\"" + this->_server->getHandlerFactory()->authenticationRealm(this->_request) + "\"";

              delete this->_request;
              this->_request = 0;

              HttpResponse response(HttpResponse::UNAUTHORIZED);
              response.setHeader("www-authenticate", strlen("www-authenticate"), realm.c_str());

              this->handleResponse(&response);
            }

            return processRead();
          }

          return true;
        }

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

        void addResponse (HttpResponse* response) {
          triagens::basics::StringBuffer * buffer;

          if (this->_closeRequested) {
            response->setHeader("connection", strlen("connection"), "Close");
          }
          else {
            // keep-alive is the default
            response->setHeader("connection", strlen("connection"), "Keep-Alive");
          }

          // save header
          buffer = new triagens::basics::StringBuffer(TRI_UNKNOWN_MEM_ZONE);
          response->writeHeader(buffer);
          buffer->appendText(response->body());

          this->_writeBuffers.push_back(buffer);

#ifdef TRI_ENABLE_FIGURES

          this->_writeBuffersStats.push_back(RequestStatisticsAgent::transfer());

#endif

          LOGGER_TRACE << "HTTP WRITE FOR " << static_cast<Task*>(this) << ":\n" << buffer->c_str();

          // clear body
          response->body().clear();

          // start output
          this->fillWriteBuffer();
        }
    };
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}\\)"
// End:
