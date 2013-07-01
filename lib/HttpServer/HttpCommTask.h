////////////////////////////////////////////////////////////////////////////////
/// @brief task for http communication
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
/// @author Dr. Frank Celler
/// @author Achim Brandt
/// @author Copyright 2009-2013, triAGENS GmbH, Cologne, Germany
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
                      TRI_socket_t socket,
                      ConnectionInfo const& info,
                      double keepAliveTimeout)
        : Task("HttpCommTask"),
          GeneralCommTask<S, HttpHandlerFactory>(server, socket, info, keepAliveTimeout),
          _requestType(HttpRequest::HTTP_REQUEST_ILLEGAL),
          _origin(),
          _denyCredentials(false) {
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
// --SECTION--                                                 protected methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup HttpServer
/// @{
////////////////////////////////////////////////////////////////////////////////

      protected:

////////////////////////////////////////////////////////////////////////////////
/// @brief reset the internal state
/// this method can be called to clean up when the request handling aborts
/// prematurely
////////////////////////////////////////////////////////////////////////////////

        void resetState () {
          if (this->_request != 0) {
            delete this->_request;
            this->_request       = 0;
          }

          this->_readPosition    = 0;
          this->_bodyPosition    = 0;
          this->_bodyLength      = 0;
          this->_readRequestBody = false;
          this->_requestPending  = false;
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
              LOGGER_WARNING("maximal header size is " << this->_maximalHeaderSize << ", request header size is " << headerLength);
              // header is too large
              HttpResponse response(HttpResponse::REQUEST_HEADER_FIELDS_TOO_LARGE);
              this->handleResponse(&response);

              return true;
            }

            if (ptr < end) {
              this->_readPosition = ptr - this->_readBuffer->c_str() + 4;

              LOGGER_TRACE("HTTP READ FOR " << static_cast<Task*>(this) << ":\n"
                           << string(this->_readBuffer->c_str(), this->_readPosition));

              // check that we know, how to serve this request
              // and update the connection information, i. e. client and server addresses and ports
              // and create a request context for that request
              this->_request = this->_server->getHandlerFactory()->createRequest(this->_connectionInfo, this->_readBuffer->c_str(), this->_readPosition);

              if (this->_request == 0) {
                LOGGER_ERROR("cannot generate request");
                // internal server error
                HttpResponse response(HttpResponse::SERVER_ERROR);
                this->handleResponse(&response);
                this->resetState();

                return true;
              }

              // check HTTP protocol
              if (! this->_request->isHttp10() && ! this->_request->isHttp11()) {
                HttpResponse response(HttpResponse::HTTP_VERSION_NOT_SUPPORTED);
                this->handleResponse(&response);
                this->resetState();

                return true;
              }
              
              // check max URL length
              if (this->_request->fullUrl().length() > 16384) {
                HttpResponse response(HttpResponse::REQUEST_URI_TOO_LONG);
                this->handleResponse(&response);
                this->resetState();

                return true;
              }

              // update the connection information, i. e. client and server addresses and ports
              //this->_request->setConnectionInfo(this->_connectionInfo);
              this->_request->setProtocol(S::protocol());


              LOGGER_TRACE("server port = " << this->_connectionInfo.serverPort << ", client port = " << this->_connectionInfo.clientPort);

              // set body start to current position
              this->_bodyPosition = this->_readPosition;


              // keep track of the original value of the "origin" request header (if any)
              // we need this value to handle CORS requests
              this->_origin = this->_request->header("origin");
              if (this->_origin.size() > 0) {
                // check for Access-Control-Allow-Credentials header
                bool found;
                string const& allowCredentials = this->_request->header("access-control-allow-credentials", found);
                if (found) {
                  this->_denyCredentials = ! triagens::basics::StringUtils::boolean(allowCredentials);
                }
              }

              // store the original request's type. we need it later when responding
              // (original request object gets deleted before responding)
              this->_requestType = this->_request->requestType();


              // handle different HTTP methods
              switch (this->_requestType) {
                case HttpRequest::HTTP_REQUEST_GET:
                case HttpRequest::HTTP_REQUEST_DELETE:
                case HttpRequest::HTTP_REQUEST_HEAD:
                case HttpRequest::HTTP_REQUEST_OPTIONS:
                case HttpRequest::HTTP_REQUEST_POST:
                case HttpRequest::HTTP_REQUEST_PUT:
                case HttpRequest::HTTP_REQUEST_PATCH: {
                  // technically, sending a body for an HTTP DELETE request is not forbidden, but it is not explicitly supported
                  const bool expectContentLength = (this->_requestType == HttpRequest::HTTP_REQUEST_POST ||
                                                    this->_requestType == HttpRequest::HTTP_REQUEST_PUT ||
                                                    this->_requestType == HttpRequest::HTTP_REQUEST_PATCH ||
                                                    this->_requestType == HttpRequest::HTTP_REQUEST_OPTIONS ||
                                                    this->_requestType == HttpRequest::HTTP_REQUEST_DELETE);

                  if (! checkContentLength(expectContentLength)) {
                    return true;
                  }

                  if (this->_bodyLength == 0) {
                    handleRequest = true;
                  }
                  break;
                }

                default: {
                  LOGGER_WARNING("got corrupted HTTP request '" << string(this->_readBuffer->c_str(), (this->_readPosition < 6 ? this->_readPosition : 6)) << "'");
                  // bad request, method not allowed
                  HttpResponse response(HttpResponse::METHOD_NOT_ALLOWED);
                  this->handleResponse(&response);
                  this->resetState();

                  return true;
                }
              }
          
           
              // enable the following statement for excessive logging of incoming requests
              // LOGGER_INFO(this->_connectionInfo.serverAddress << "," << this->_connectionInfo.serverPort << "," << 
              //             this->_connectionInfo.clientAddress << "," << 
              //             HttpRequest::translateMethod(this->_request->requestType()) << "," <<
              //             HttpRequest::translateVersion(this->_request->httpVersion()) << "," << 
              //             this->_bodyLength << "," <<
              //             this->_request->fullUrl());
            
              // .............................................................................
              // check if server is active
              // .............................................................................

              Scheduler const* scheduler = this->_server->getScheduler();

              if (scheduler != 0 && ! scheduler->isActive()) {
                // server is inactive and will intentionally respond with HTTP 503
                LOGGER_TRACE("cannot serve request - server is inactive");
                HttpResponse response(HttpResponse::SERVICE_UNAVAILABLE);
                this->handleResponse(&response);
                this->resetState();

                return true;
              }


              // check for a 100-continue
              if (this->_readRequestBody) {
                bool found;
                string const& expect = this->_request->header("expect", found);

                if (found && triagens::basics::StringUtils::trim(expect) == "100-continue") {
                  LOGGER_TRACE("received a 100-continue request");

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
              LOGGER_WARNING("maximal body size is " << this->_maximalBodySize << ", request body size is " << this->_bodyLength);
              // request entity too large
              HttpResponse response(HttpResponse::REQUEST_ENTITY_TOO_LARGE);
              this->handleResponse(&response);
              this->resetState();

              return true;
            }

            if (this->_readBuffer->length() - this->_bodyPosition < this->_bodyLength) {
              // still more data to be read

              SocketTask* socketTask = dynamic_cast<SocketTask*>(this);
              if (socketTask) {
                // set read request time-out
                LOGGER_TRACE("waiting for rest of body to be received. request timeout set to 60 s");
                socketTask->setKeepAliveTimeout(60.0);
              }

              // let client send more
              return true;
            }

            // read "bodyLength" from read buffer and add this body to "httpRequest"
            this->_request->setBody(this->_readBuffer->c_str() + this->_bodyPosition, this->_bodyLength);

            LOGGER_TRACE(string(this->_readBuffer->c_str() + this->_bodyPosition, this->_bodyLength));

            // remove body from read buffer and reset read position
            this->_readRequestBody = false;
            handleRequest = true;
          }

          // we have to delete request in here or pass it to a handler, which will delete it
          if (handleRequest) {
            RequestStatisticsAgentSetReadEnd(this);
            RequestStatisticsAgentAddReceivedBytes(this, this->_bodyPosition + this->_bodyLength);

            this->_readBuffer->erase_front(this->_bodyPosition + this->_bodyLength);

            if (this->_readBuffer->length() > 0) {
              // we removed the front of the read buffer, but it still contains data.
              // this means that the content-length header of the request must have been wrong
              // (value in content-length header smaller than actual body size)

              // check if there is invalid stuff left in the readbuffer
              // whitespace is allowed
              const char* p = this->_readBuffer->begin();
              const char* e = this->_readBuffer->end();
              while (p < e) {
                const char c = *(p++);
                if (c != '\n' && c != '\r' && c != ' ' && c != '\t' && c != '\0') {
                  LOGGER_WARNING("read buffer is not empty. probably got a wrong Content-Length header?");

                  HttpResponse response(HttpResponse::BAD);
                  this->handleResponse(&response);
                  this->resetState();

                  return true;
                }
              }
            }

            this->_requestPending = true;

            // .............................................................................
            // keep-alive handling
            // .............................................................................

            string connectionType = triagens::basics::StringUtils::tolower(this->_request->header("connection"));

            if (connectionType == "close") {
              // client has sent an explicit "Connection: Close" header. we should close the connection
              LOGGER_DEBUG("connection close requested by client");
              this->_closeRequested = true;
            }
            else if (this->_request->isHttp10() && connectionType != "keep-alive") {
              // HTTP 1.0 request, and no "Connection: Keep-Alive" header sent
              // we should close the connection
              LOGGER_DEBUG("no keep-alive, connection close requested by client");
              this->_closeRequested = true;
            }
            else if (this->_keepAliveTimeout <= 0.0) {
              // if keepAliveTimeout was set to 0.0, we'll close even keep-alive connections immediately
              LOGGER_DEBUG("keep-alive disabled by admin");
              this->_closeRequested = true;
            }
            // we keep the connection open in all other cases (HTTP 1.1 or Keep-Alive header sent)

            this->_readPosition = 0;
            this->_bodyPosition = 0;
            this->_bodyLength = 0;

            // .............................................................................
            // authenticate
            // .............................................................................

            bool auth = this->_server->getHandlerFactory()->authenticateRequest(this->_request);

            // authenticated
            // or an HTTP OPTIONS request. OPTIONS requests currently go unauthenticated
            if (auth || this->_requestType == HttpRequest::HTTP_REQUEST_OPTIONS) {

              // handle HTTP OPTIONS requests directly
              if (this->_requestType == HttpRequest::HTTP_REQUEST_OPTIONS) {
                const string allowedMethods = "DELETE, GET, HEAD, PATCH, POST, PUT";

                HttpResponse response(HttpResponse::OK);

                response.setHeader("allow", strlen("allow"), allowedMethods);

                if (this->_origin.size() > 0) {
                  LOGGER_TRACE("got CORS preflight request");
                  const string allowHeaders = triagens::basics::StringUtils::trim(this->_request->header("access-control-request-headers"));

                  // send back which HTTP methods are allowed for the resource
                  // we'll allow all
                  response.setHeader("access-control-allow-methods", strlen("access-control-allow-methods"), allowedMethods);

                  if (allowHeaders.size() > 0) {
                    // allow all extra headers the client requested
                    // we don't verify them here. the worst that can happen is that the client
                    // sends some broken headers and then later cannot access the data on the
                    // server. that's a client problem.
                    response.setHeader("access-control-allow-headers", strlen("access-control-allow-headers"), allowHeaders);
                    LOGGER_TRACE("client requested validation of the following headers " << allowHeaders);
                  }
                  // set caching time (hard-coded to 1 day)
                  response.setHeader("access-control-max-age", strlen("access-control-max-age"), "1800");
                }
                // End of CORS handling

                this->handleResponse(&response);

                this->resetState();

                // we're done
                return true;
              }
              // end HTTP OPTIONS handling


              HttpHandler* handler = this->_server->getHandlerFactory()->createHandler(this->_request);
              bool ok = false;

              if (handler == 0) {
                LOGGER_TRACE("no handler is known, giving up");
                HttpResponse response(HttpResponse::NOT_FOUND);
                this->handleResponse(&response);

                this->resetState();
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

          // CORS response handling
          if (this->_origin.size() > 0) {
            // the request contained an Origin header. We have to send back the
            // access-control-allow-origin header now
            LOGGER_TRACE("handling CORS response");

            // send back original value of "Origin" header
            response->setHeader("access-control-allow-origin", strlen("access-control-allow-origin"), this->_origin);

            // send back "Access-Control-Allow-Credentials" header
            if (this->_denyCredentials) {
              response->setHeader("access-control-allow-credentials", "false");
            }
            else {
              response->setHeader("access-control-allow-credentials", "true");
            }
          }
          // CORS request handling EOF


          if (this->_closeRequested) {
            response->setHeader("connection", strlen("connection"), "Close");
          }
          else {
            // keep-alive is the default
            response->setHeader("connection", strlen("connection"), "Keep-Alive");
          }

          if (this->_requestType == HttpRequest::HTTP_REQUEST_HEAD) {
            // clear body if this is an HTTP HEAD request
            // HEAD must not return a body
            response->headResponse(response->bodySize());
          }

          // reserve some outbuffer size
          const size_t len = response->bodySize() + 128;
          triagens::basics::StringBuffer* buffer = new triagens::basics::StringBuffer(TRI_UNKNOWN_MEM_ZONE, len);
          // write header
          response->writeHeader(buffer);

          // write body
          buffer->appendText(response->body());

          this->_writeBuffers.push_back(buffer);

#ifdef TRI_ENABLE_FIGURES

          this->_writeBuffersStats.push_back(RequestStatisticsAgent::transfer());

#endif

          LOGGER_TRACE("HTTP WRITE FOR " << static_cast<Task*>(this) << ":\n" << buffer->c_str());

          // clear body
          response->body().clear();

          // start output
          this->fillWriteBuffer();
        }

////////////////////////////////////////////////////////////////////////////////
/// check the content-length header of a request and fail it is broken
////////////////////////////////////////////////////////////////////////////////

        bool checkContentLength (const bool expectContentLength) {
          const int64_t bodyLength = this->_request->contentLength();

          if (bodyLength < 0) {
            // bad request, body length is < 0. this is a client error
            HttpResponse response(HttpResponse::LENGTH_REQUIRED);
            this->handleResponse(&response);

            return false;
          }

          if (! expectContentLength && bodyLength > 0) {
            // content-length header was sent but the request method does not support that
            // we'll warn but read the body anyway
            LOGGER_WARNING("received HTTP GET/HEAD request with content-length, this should not happen");
          }

          if ((size_t) bodyLength > this->_maximalBodySize) {
            // request entity too large
            LOGGER_WARNING("maximal body size is " << this->_maximalBodySize << ", request body size is " << bodyLength);
            HttpResponse response(HttpResponse::REQUEST_ENTITY_TOO_LARGE);
            this->handleResponse(&response);

            return false;
          }

          // set instance variable to content-length value
          this->_bodyLength = (size_t) bodyLength;
          if (this->_bodyLength > 0) {
            // we'll read the body
            this->_readRequestBody = true;
          }

          // everything's fine
          return true;
        }


////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup HttpServer
/// @{
////////////////////////////////////////////////////////////////////////////////

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief type of request (GET, POST, ...)
////////////////////////////////////////////////////////////////////////////////

        HttpRequest::HttpRequestType _requestType;

////////////////////////////////////////////////////////////////////////////////
/// @brief value of the HTTP origin header the client sent (if any).
/// this is only used for CORS
////////////////////////////////////////////////////////////////////////////////

        std::string _origin;

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not to allow credentialed requests
/// this is only used for CORS
////////////////////////////////////////////////////////////////////////////////

        bool _denyCredentials;

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
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
