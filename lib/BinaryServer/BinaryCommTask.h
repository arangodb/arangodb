////////////////////////////////////////////////////////////////////////////////
/// @brief task for binary communication
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
/// @author Jan Steemann
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_HTTP_SERVER_BINARY_COMM_TASK_H
#define TRIAGENS_HTTP_SERVER_BINARY_COMM_TASK_H 1

#include "GeneralServer/GeneralCommTask.h"

#include "Basics/StringUtils.h"
#include "Basics/StringBuffer.h"

#include "Scheduler/ListenTask.h"
#include "HttpServer/HttpHandler.h"
#include "HttpServer/HttpHandlerFactory.h"
#include "Rest/HttpRequest.h"
#include "Rest/HttpRequestPlain.h"
#include "Rest/HttpResponse.h"
#include "Scheduler/Scheduler.h"

// -----------------------------------------------------------------------------
// --SECTION--                                              forward declarations
// -----------------------------------------------------------------------------

namespace triagens {
  namespace rest {

// -----------------------------------------------------------------------------
// --SECTION--                                              class BinaryCommTask
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup HttpServer
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief task for binary communication
////////////////////////////////////////////////////////////////////////////////

    template<typename S>
    class BinaryCommTask : public GeneralCommTask<S, HttpHandlerFactory>,
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

        BinaryCommTask (S* server, 
                        socket_t fd, 
                        ConnectionInfo const& info) 
        : Task("BinaryCommTask"),
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

        virtual ~BinaryCommTask () {
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
            const char * end = this->_readBuffer->end();
            size_t length = 0;

            if (end - ptr >= 8) {
              if ((ptr[0] & 0xFF) != 0xB0 ||
                  (ptr[1] & 0xFF) != 0x00 ||
                  (ptr[2] & 0xFF) != 0xB1 ||
                  (ptr[3] & 0xFF) != 0x35) {

                HttpResponse response(HttpResponse::BAD);
                this->handleResponse(&response);
                this->_requestPending = true;
                return false;
              }

              length = (size_t) ptr[7] + 
                       (size_t) (ptr[6] << 8) + 
                       (size_t) (ptr[5] << 16) +
                       (size_t) (ptr[4] << 24);

              this->_readRequestBody = true;
            }
            
            if (length > 128 * 1024 * 1024) {
              LOGGER_WARNING << "maximum size is " << 128 * 1024 * 1024 << ", message size is " << length;
              return false;
            }

            if (ptr < end) {
              this->_readPosition = 8;

              LOGGER_TRACE << "HTTP READ FOR " << static_cast<Task*>(this) << ":\n"
                << string(this->_readBuffer->c_str(), this->_readPosition);

              // create a fake HTTP request
              triagens::basics::StringBuffer fakeRequest(TRI_UNKNOWN_MEM_ZONE);
              fakeRequest.appendText("POST /_api/batch HTTP/1.1\r\nContent-Type: application/x-protobuf\r\nContent-Length: ");
              fakeRequest.appendInteger(length);
              fakeRequest.appendText("\r\nConnection: Close\r\n\r\n");
              fakeRequest.appendText(this->_readBuffer->c_str() + 8, length);

              // LOGGER_INFO << fakeRequest.c_str() << "\n\n";

              this->_request = new HttpRequestPlain(fakeRequest.begin(), fakeRequest.length());

              // update the connection information, i. e. client and server addresses and ports
              this->_request->setConnectionInfo(this->_connectionInfo);

              LOGGER_TRACE << "server port = " << this->_connectionInfo.serverPort << ", client port = " << this->_connectionInfo.clientPort;
              
              this->_bodyPosition = this->_readPosition;
              this->_bodyLength = length - this->_readPosition;
            }
            else {
              if (this->_readBuffer->c_str() < end) {
                this->_readPosition = end - this->_readBuffer->c_str();
              }
            }
          }
          
          // readRequestBody might have changed, so cannot use else
          if (this->_readRequestBody) {
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

            this->_requestPending = true;
            this->_readPosition = 0;
            this->_bodyPosition = 0;
            this->_bodyLength = 0;

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

          return true;
        }

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

        void addResponse (HttpResponse* response) {
          triagens::basics::StringBuffer * buffer;

          response->setHeader("connection", strlen("connection"), "Close");

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
