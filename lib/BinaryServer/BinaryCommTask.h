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
          GeneralCommTask<S, HttpHandlerFactory>(server, fd, info, keepAliveTimeout),
          _binaryMessage(0) {
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
          if (_binaryMessage != 0) {
            delete _binaryMessage;
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

      private:

        BinaryMessage* _binaryMessage;

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

          assert(this->_readPosition == 0);

#ifdef TRI_ENABLE_FIGURES

          if (this->_readBuffer->c_str() != this->_readBuffer->end()) {
            RequestStatisticsAgent::acquire();
            RequestStatisticsAgentSetReadStart(this);
          }

#endif
        
          assert(_binaryMessage == 0);

          _binaryMessage = BinaryMessage::createFromRequest(this->_readBuffer->c_str(), this->_readBuffer->end());

          if (_binaryMessage == 0) {
            HttpResponse response(HttpResponse::BAD);
            this->handleResponse(&response);
            this->_requestPending = true;
            return false;
          }

          this->_readPosition = BinaryMessage::getHeaderLength();
          this->_bodyPosition = this->_readPosition;
          this->_bodyLength = _binaryMessage->bodyLength();
          

          LOGGER_TRACE << "BINARY READ FOR " << static_cast<Task*>(this) << ": BODY SIZE: " << this->_bodyLength;

          // create a fake HTTP request
          triagens::basics::StringBuffer fakeRequest(TRI_UNKNOWN_MEM_ZONE);
          fakeRequest.appendText("POST /_api/batch HTTP/1.1\r\nContent-Type: ");
          fakeRequest.appendText(BinaryMessage::getContentType());
          fakeRequest.appendText("\r\nContent-Length: ");
          fakeRequest.appendInteger(this->_bodyLength);
          fakeRequest.appendText("\r\nConnection: Close\r\n\r\n");

          this->_request = new HttpRequestPlain(fakeRequest.begin(), fakeRequest.length());

          // pass the body pointer to the request, not copying it
          this->_request->setBodyReference(_binaryMessage->body(), this->_bodyLength);

          // update the connection information, i. e. client and server addresses and ports
          this->_request->setConnectionInfo(this->_connectionInfo);

          LOGGER_TRACE << "server port = " << this->_connectionInfo.serverPort << ", client port = " << this->_connectionInfo.clientPort;
              
          // we have to delete request in here or pass it to a handler, which will delete it
          RequestStatisticsAgentSetReadEnd(this);
          RequestStatisticsAgentAddReceivedBytes(this, this->_bodyLength);

          this->_requestPending = true;
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
