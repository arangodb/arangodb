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

#include "BasicsC/hashes.h"

#include "Basics/StringUtils.h"
#include "Basics/StringBuffer.h"

#include "Scheduler/ListenTask.h"
#include "HttpServer/HttpHandler.h"
#include "HttpServer/HttpHandlerFactory.h"
#include "Rest/HttpRequest.h"
#include "Rest/HttpRequestPlain.h"
#include "Rest/HttpResponse.h"
#include "Scheduler/Scheduler.h"
#include "BinaryServer/BinaryMessage.h"

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
                        ConnectionInfo const& info,
                        double keepAliveTimeout) 
        : Task("BinaryCommTask"),
          GeneralCommTask<S, HttpHandlerFactory>(server, fd, info, keepAliveTimeout) {
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

            if (end - ptr >= (ptrdiff_t) BinaryMessage::getHeaderLength()) {
              this->_readPosition = BinaryMessage::getHeaderLength();

              LOGGER_TRACE << "BINARY READ FOR " << static_cast<Task*>(this);

              // create a fake HTTP request
              triagens::basics::StringBuffer fakeRequest(TRI_UNKNOWN_MEM_ZONE);
              fakeRequest.appendText("POST /_api/batch HTTP/1.1\r\nContent-Type: ");
              fakeRequest.appendText(BinaryMessage::getContentType());
              fakeRequest.appendText("\r\nContent-Length: ");
              fakeRequest.appendInteger(this->_bodyLength);
              fakeRequest.appendText("\r\nConnection: Close\r\n\r\n");

              this->_request = new HttpRequestPlain(fakeRequest.begin(), fakeRequest.length());
              
              if (this->_request == 0) {
                LOGGER_ERROR << "cannot generate request";
                return false;
              }

              // update the connection information, i. e. client and server addresses and ports
              this->_request->setConnectionInfo(this->_connectionInfo);

              LOGGER_TRACE << "server port = " << this->_connectionInfo.serverPort << ", client port = " << this->_connectionInfo.clientPort;
              
              // set body start to current position
              this->_bodyPosition = this->_readPosition;
                  
              this->_bodyLength = (size_t) BinaryMessage::decodeLength((const uint8_t*) (this->_readBuffer->c_str() + 4));
              this->_readRequestBody = true;
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
      
//            std::cout << "body length: " << this->_bodyLength << ", hash: " << TRI_FnvHashPointer(this->_readBuffer->c_str() + this->_bodyPosition, this->_bodyLength) << "\n";

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

            if (this->_keepAliveTimeout <= 0.0) {
              this->_closeRequested = true;
            }
            else {
              this->_closeRequested = false;
            }

            this->_readPosition = 0;
            this->_bodyPosition = 0;
            this->_bodyLength = 0;

            HttpHandler* handler = this->_server->getHandlerFactory()->createHandler(this->_request);
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
              bool ok = this->_server->handleRequest(this, handler);

              if (! ok) {
                HttpResponse response(HttpResponse::SERVER_ERROR);
                this->handleResponse(&response);
              }
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
          buffer->appendText("\0\0\0\0\0\0\0\0", 8);
          uint8_t* outPointer = (uint8_t*) buffer->c_str();

          BinaryMessage::writeHeader((uint32_t) response->body().length(), outPointer);
          
          buffer->appendText(response->body());

          // LOGGER_TRACE << "binary body length: " << response->body().length() << ", hash: " << TRI_FnvHashPointer(response->body().c_str(), response->body().length());

          this->_writeBuffers.push_back(buffer);

#ifdef TRI_ENABLE_FIGURES

          this->_writeBuffersStats.push_back(RequestStatisticsAgent::transfer());

#endif

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
