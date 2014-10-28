////////////////////////////////////////////////////////////////////////////////
/// @brief task for http communication
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
/// @author Dr. Frank Celler
/// @author Achim Brandt
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2009-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_HTTP_SERVER_HTTP_COMM_TASK_H
#define ARANGODB_HTTP_SERVER_HTTP_COMM_TASK_H 1

#include "Basics/Common.h"

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
/// @brief task for http communication
////////////////////////////////////////////////////////////////////////////////

    template<typename S>
    class HttpCommTask : public GeneralCommTask<S, HttpHandlerFactory>,
                         public RequestStatisticsAgent {

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

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
          _httpVersion(HttpRequest::HTTP_UNKNOWN),
          _requestType(HttpRequest::HTTP_REQUEST_ILLEGAL),
          _origin(),
          _denyCredentials(false),
          _acceptDeflate(false),
          _newRequest(true),
          _startPosition(0),
          _sinceCompactification(0) {
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

// -----------------------------------------------------------------------------
// --SECTION--                                                 protected methods
// -----------------------------------------------------------------------------

      protected:

////////////////////////////////////////////////////////////////////////////////
/// @brief handles CORS options
////////////////////////////////////////////////////////////////////////////////

        void processCorsOptions (uint32_t compatibility) {
          const std::string allowedMethods = "DELETE, GET, HEAD, PATCH, POST, PUT";

          HttpResponse response(HttpResponse::OK, compatibility);

          response.setHeader("allow", strlen("allow"), allowedMethods);

          if (! this->_origin.empty()) {
            LOG_TRACE("got CORS preflight request");
            const std::string allowHeaders = triagens::basics::StringUtils::trim(this->_request->header("access-control-request-headers"));

            // send back which HTTP methods are allowed for the resource
            // we'll allow all
            response.setHeader("access-control-allow-methods", strlen("access-control-allow-methods"), allowedMethods);

            if (! allowHeaders.empty()) {
              // allow all extra headers the client requested
              // we don't verify them here. the worst that can happen is that the client
              // sends some broken headers and then later cannot access the data on the
              // server. that's a client problem.
              response.setHeader("access-control-allow-headers", strlen("access-control-allow-headers"), allowHeaders);
              LOG_TRACE("client requested validation of the following headers: %s", allowHeaders.c_str());
            }

            // set caching time (hard-coded value)
            response.setHeader("access-control-max-age", strlen("access-control-max-age"), "1800");
          }

          this->clearRequest();
          this->handleResponse(&response);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief handles request
////////////////////////////////////////////////////////////////////////////////

        void processRequest (uint32_t compatibility) {
          HttpHandler* handler = this->_server->getHandlerFactory()->createHandler(this->_request);
          bool ok = false;

          if (handler == nullptr) {
            LOG_TRACE("no handler is known, giving up");

            HttpResponse response(HttpResponse::NOT_FOUND, compatibility);

            this->clearRequest();
            this->handleResponse(&response);

            return;
          }

          bool found;
          std::string const& acceptEncoding = this->_request->header("accept-encoding", found);

          if (found) {
            if (acceptEncoding.find("deflate") != std::string::npos) {
              _acceptDeflate = true;
            }
          }

          // check for an async request
          std::string const& asyncExecution = this->_request->header("x-arango-async", found);

          // clear request object
          this->_request = nullptr;
          this->RequestStatisticsAgent::transfer(handler);

          // async execution
          if (found && (asyncExecution == "true" || asyncExecution == "store")) {

#ifdef TRI_ENABLE_FIGURES
            RequestStatisticsAgentSetAsync(this);
#endif

            uint64_t jobId = 0;

            if (asyncExecution == "store") {
              // persist the responses
              ok = this->_server->handleRequestAsync(handler, &jobId);
            }
            else {
              // don't persist the responses
              ok = this->_server->handleRequestAsync(handler, 0);
            }

            if (ok) {
              HttpResponse response(HttpResponse::ACCEPTED, compatibility);

              if (jobId > 0) {
                // return the job id we just created
                response.setHeader("x-arango-async-id",
                                   strlen("x-arango-async-id"),
                                   triagens::basics::StringUtils::itoa(jobId));
              }

              this->handleResponse(&response);

              return;
            }
          }

          // synchronous request
          else {
            ok = this->_server->handleRequest(this, handler);
          }

          if (! ok) {
            HttpResponse response(HttpResponse::SERVER_ERROR, compatibility);
            this->handleResponse(&response);
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief clears the request object
////////////////////////////////////////////////////////////////////////////////

        void clearRequest () {
          if (this->_request != nullptr) {
            delete this->_request;
            this->_request = nullptr;
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief reset the internal state
///
/// this method can be called to clean up when the request handling aborts
/// prematurely
////////////////////////////////////////////////////////////////////////////////

        void resetState (bool close) {
          const size_t COMPACT_EVERY = 500;

          if (close) {
            this->clearRequest();

            this->_requestPending = false;
            this->_closeRequested = true;

            this->_readPosition    = 0;
            this->_bodyPosition    = 0;
            this->_bodyLength      = 0;
          }
          else {
            this->_requestPending = true;

            bool compact = false;

            if (this->_sinceCompactification > COMPACT_EVERY) {
              compact = true;
            }
            else if (this->_readBuffer->length() > this->_maximalPipelineSize) {
              compact = true;
            }

            if (compact) {
              this->_readBuffer->erase_front(this->_bodyPosition + this->_bodyLength);

              this->_sinceCompactification = 0;
              this->_readPosition = 0;
            }
            else {
              this->_readPosition = this->_bodyPosition + this->_bodyLength;
            }

            this->_bodyPosition    = 0;
            this->_bodyLength      = 0;
          }

          this->_newRequest      = true;
          this->_readRequestBody = false;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief decide whether or not we should send back a www-authenticate header
////////////////////////////////////////////////////////////////////////////////

        bool sendWwwAuthenticateHeader () const {
          bool found;
          std::string const value = this->_request->header("x-omit-www-authenticate", found);

          return ! found;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief get request compatibility
////////////////////////////////////////////////////////////////////////////////

        int32_t getCompatibility () const {
          if (this->_request != nullptr) {
            return this->_request->compatibility();
          }

          return HttpRequest::MinCompatibility;
        }

// -----------------------------------------------------------------------------
// --SECTION--                                           GeneralCommTask methods
// -----------------------------------------------------------------------------

      protected:

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

        bool processRead () {
          if (this->_requestPending || this->_readBuffer->c_str() == nullptr) {
            return true;
          }

          bool handleRequest = false;

          // still trying to read the header fields
          if (! this->_readRequestBody) {

            // starting a new request
            if (this->_newRequest) {
#ifdef TRI_ENABLE_FIGURES
              RequestStatisticsAgent::acquire();
              RequestStatisticsAgentSetReadStart(this);
#endif

              this->_newRequest      = false;
              this->_startPosition   = this->_readPosition;
              this->_httpVersion     = HttpRequest::HTTP_UNKNOWN;
              this->_requestType     = HttpRequest::HTTP_REQUEST_ILLEGAL;
              this->_fullUrl         = "";
              this->_denyCredentials = false;
              this->_acceptDeflate   = false;

              this->_sinceCompactification++;
            }

            const char * ptr = this->_readBuffer->c_str() + this->_readPosition;
            const char * end = this->_readBuffer->end() - 3;

            for (;  ptr < end;  ptr++) {
              if (ptr[0] == '\r' && ptr[1] == '\n' && ptr[2] == '\r' && ptr[3] == '\n') {
                break;
              }
            }


            // check if header is too large
            size_t headerLength = ptr - (this->_readBuffer->c_str() + this->_startPosition);

            if (headerLength > this->_maximalHeaderSize) {
              LOG_WARNING("maximal header size is %d, request header size is %d",
                          (int) this->_maximalHeaderSize,
                          (int) headerLength);

              // header is too large
              HttpResponse response(HttpResponse::REQUEST_HEADER_FIELDS_TOO_LARGE, getCompatibility());

              // we need to close the connection, because there is no way we 
              // know what to remove and then continue
              this->resetState(true);
              this->handleResponse(&response);

              return true;
            }

            // header is complete
            if (ptr < end) {
              this->_readPosition = ptr - this->_readBuffer->c_str() + 4;

              LOG_TRACE("HTTP READ FOR %p: %s", (void*) this,
                        std::string(this->_readBuffer->c_str() + this->_startPosition,
                                    this->_readPosition - this->_startPosition).c_str());

              // check that we know, how to serve this request
              // and update the connection information, i. e. client and server addresses and ports
              // and create a request context for that request
              this->_request = this->_server->getHandlerFactory()->createRequest(
                this->_connectionInfo,
                this->_readBuffer->c_str() + this->_startPosition,
                this->_readPosition - this->_startPosition);

              if (this->_request == nullptr) {
                LOG_ERROR("cannot generate request");

                // internal server error
                HttpResponse response(HttpResponse::SERVER_ERROR, getCompatibility());

                // we need to close the connection, because there is no way we 
                // know how to remove the body and then continue
                this->resetState(true);
                this->handleResponse(&response);

                return true;
              }

              TRI_ASSERT(this->_request != nullptr);

              // check HTTP protocol version
              _httpVersion = this->_request->httpVersion();

              if (_httpVersion != HttpRequest::HTTP_1_0 &&
                  _httpVersion != HttpRequest::HTTP_1_1) {

                HttpResponse response(HttpResponse::HTTP_VERSION_NOT_SUPPORTED, getCompatibility());

                // we need to close the connection, because there is no way we 
                // know what to remove and then continue
                this->resetState(true);
                this->handleResponse(&response);

                return true;
              }

              // check max URL length
              _fullUrl = this->_request->fullUrl();

              if (_fullUrl.size() > 16384) {
                HttpResponse response(HttpResponse::REQUEST_URI_TOO_LONG, getCompatibility());

                // we need to close the connection, because there is no way we 
                // know what to remove and then continue
                this->resetState(true);
                this->handleResponse(&response);

                return true;
              }

              // update the connection information, i. e. client and server addresses and ports
              this->_request->setProtocol(S::protocol());

              LOG_TRACE("server port %d, client port %d",
                        (int) this->_connectionInfo.serverPort,
                        (int) this->_connectionInfo.clientPort);

              // set body start to current position
              this->_bodyPosition = this->_readPosition;
              this->_bodyLength = 0;

              // keep track of the original value of the "origin" request header (if any)
              // we need this value to handle CORS requests
              this->_origin = this->_request->header("origin");

              if (! this->_origin.empty()) {

                // check for Access-Control-Allow-Credentials header
                bool found;
                std::string const& allowCredentials = this->_request->header("access-control-allow-credentials", found);

                if (found) {
                  this->_denyCredentials = ! triagens::basics::StringUtils::boolean(allowCredentials);
                }
              }

              // store the original request's type. we need it later when responding
              // (original request object gets deleted before responding)
              this->_requestType = this->_request->requestType();

#ifdef TRI_ENABLE_FIGURES
              RequestStatisticsAgentSetRequestType(this, this->_requestType);
#endif

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
                  const bool expectContentLength = (this->_requestType == HttpRequest::HTTP_REQUEST_POST
                                                 || this->_requestType == HttpRequest::HTTP_REQUEST_PUT
                                                 || this->_requestType == HttpRequest::HTTP_REQUEST_PATCH
                                                 || this->_requestType == HttpRequest::HTTP_REQUEST_OPTIONS
                                                 || this->_requestType == HttpRequest::HTTP_REQUEST_DELETE);

                  if (! checkContentLength(expectContentLength)) {
                    return true;
                  }

                  if (this->_bodyLength == 0) {
                    handleRequest = true;
                  }

                  break;
                }

                default: {
                  size_t l = this->_readPosition - this->_startPosition;

                  if (6 < l) {
                    l = 6;
                  }

                  LOG_WARNING("got corrupted HTTP request '%s'",
                              std::string(this->_readBuffer->c_str() + this->_startPosition, l).c_str());

                  // bad request, method not allowed
                  HttpResponse response(HttpResponse::METHOD_NOT_ALLOWED, getCompatibility());

                  // we need to close the connection, because there is no way we 
                  // know what to remove and then continue
                  this->resetState(true);
                  this->handleResponse(&response);

                  // force a socket close, response will be ignored!
                  TRI_CLOSE_SOCKET(this->_commSocket);

                  return true;
                }
              }

              // .............................................................................
              // check if server is active
              // .............................................................................

              Scheduler const* scheduler = this->_server->getScheduler();

              if (scheduler != nullptr && ! scheduler->isActive()) {
                // server is inactive and will intentionally respond with HTTP 503
                LOG_TRACE("cannot serve request - server is inactive");

                HttpResponse response(HttpResponse::SERVICE_UNAVAILABLE, getCompatibility());

                // we need to close the connection, because there is no way we 
                // know what to remove and then continue
                this->resetState(true);
                this->handleResponse(&response);

                return true;
              }

              // check for a 100-continue
              if (this->_readRequestBody) {
                bool found;
                std::string const& expect = this->_request->header("expect", found);

                if (found && triagens::basics::StringUtils::trim(expect) == "100-continue") {
                  LOG_TRACE("received a 100-continue request");

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
              size_t l = (this->_readBuffer->end() - this->_readBuffer->c_str());

              if (this->_startPosition + 4 <= l) {
                this->_readPosition = l - 4;
              }
            }
          }

          // readRequestBody might have changed, so cannot use else
          if (this->_readRequestBody) {
            if (this->_readBuffer->length() - this->_bodyPosition < this->_bodyLength) {
              // still more data to be read
              SocketTask* socketTask = dynamic_cast<SocketTask*>(this);

              if (socketTask) {
                // set read request time-out
                LOG_TRACE("waiting for rest of body to be received. request timeout set to 60 s");

                socketTask->setKeepAliveTimeout(60.0);
              }

              // let client send more
              return true;
            }

            // read "bodyLength" from read buffer and add this body to "httpRequest"
            this->_request->setBody(this->_readBuffer->c_str() + this->_bodyPosition, this->_bodyLength);

            LOG_TRACE("%s", std::string(this->_readBuffer->c_str() + this->_bodyPosition, this->_bodyLength).c_str());

            // remove body from read buffer and reset read position
            this->_readRequestBody = false;
            handleRequest = true;
          }

          // .............................................................................
          // request complete
          //
          // we have to delete request in here or pass it to a handler, which will delete
          // it
          // .............................................................................

          if (! handleRequest) {
            return true;
          }

          RequestStatisticsAgentSetReadEnd(this);
          RequestStatisticsAgentAddReceivedBytes(this, this->_bodyPosition - this->_startPosition + this->_bodyLength);

          bool isOptions = (this->_requestType == HttpRequest::HTTP_REQUEST_OPTIONS);
          this->resetState(false);

          // .............................................................................
          // keep-alive handling
          // .............................................................................

          std::string connectionType = triagens::basics::StringUtils::tolower(this->_request->header("connection"));

          if (connectionType == "close") {
            // client has sent an explicit "Connection: Close" header. we should close the connection
            LOG_DEBUG("connection close requested by client");
            this->_closeRequested = true;
          }
          else if (this->_request->isHttp10() && connectionType != "keep-alive") {
            // HTTP 1.0 request, and no "Connection: Keep-Alive" header sent
            // we should close the connection
            LOG_DEBUG("no keep-alive, connection close requested by client");
            this->_closeRequested = true;
          }
          else if (this->_keepAliveTimeout <= 0.0) {
            // if keepAliveTimeout was set to 0.0, we'll close even keep-alive connections immediately
            LOG_DEBUG("keep-alive disabled by admin");
            this->_closeRequested = true;
          }

          // we keep the connection open in all other cases (HTTP 1.1 or Keep-Alive header sent)

          // .............................................................................
          // authenticate
          // .............................................................................

          auto const compatibility = this->_request->compatibility();

          HttpResponse::HttpResponseCode authResult = this->_server->getHandlerFactory()->authenticateRequest(this->_request);

          // authenticated or an OPTIONS request. OPTIONS requests currently go unauthenticated
          if (authResult == HttpResponse::OK || isOptions) {

            // handle HTTP OPTIONS requests directly
            if (isOptions) {
              this->processCorsOptions(compatibility);
            }
            else {
              this->processRequest(compatibility);
            }
          }

          // not found
          else if (authResult == HttpResponse::NOT_FOUND) {
            HttpResponse response(authResult, compatibility);
            response.setContentType("application/json; charset=utf-8");

            response.body().appendText("{\"error\":true,\"errorMessage\":\"")
                           .appendText(TRI_errno_string(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND))
                           .appendText("\",\"code\":")
                           .appendInteger((int) authResult)
                           .appendText(",\"errorNum\":")
                           .appendInteger(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND)
                           .appendText("}");

            this->clearRequest();
            this->handleResponse(&response);
          }

          // forbidden
          else if (authResult == HttpResponse::FORBIDDEN) {
            HttpResponse response(authResult, compatibility);
            response.setContentType("application/json; charset=utf-8");

            response.body().appendText("{\"error\":true,\"errorMessage\":\"change password\",\"code\":")
                           .appendInteger((int) authResult)
                           .appendText(",\"errorNum\":")
                           .appendInteger(TRI_ERROR_USER_CHANGE_PASSWORD)
                           .appendText("}");

            this->clearRequest();
            this->handleResponse(&response);
          }

          // not authenticated
          else {
            HttpResponse response(HttpResponse::UNAUTHORIZED, compatibility);
            const std::string realm = "basic realm=\"" + this->_server->getHandlerFactory()->authenticationRealm(this->_request) + "\"";

            if (sendWwwAuthenticateHeader()) {
              response.setHeader("www-authenticate", strlen("www-authenticate"), realm.c_str());
            }

            this->clearRequest();
            this->handleResponse(&response);
          }

          return true;
        }

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

        void addResponse (HttpResponse* response) {

          // CORS response handling
          if (! this->_origin.empty()) {
            // the request contained an Origin header. We have to send back the
            // access-control-allow-origin header now
            LOG_TRACE("handling CORS response");

            response->setHeader("access-control-expose-headers", strlen("access-control-expose-headers"), "etag, content-encoding, content-length, location, server, x-arango-errors, x-arango-async-id");
            // TODO: check whether anyone actually needs these headers in the browser: x-arango-replication-checkmore, x-arango-replication-lastincluded, x-arango-replication-lasttick, x-arango-replication-active");

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

          // set "connection" header
          if (this->_closeRequested) {
            response->setHeader("connection", strlen("connection"), "Close");
          }
          else {
            // keep-alive is the default
            response->setHeader("connection", strlen("connection"), "Keep-Alive");
          }

          size_t responseBodyLength = response->bodySize();

          if (this->_requestType == HttpRequest::HTTP_REQUEST_HEAD) {
            // clear body if this is an HTTP HEAD request
            // HEAD must not return a body
            response->headResponse(responseBodyLength);
          }
          // else {
          //    // to enable automatic deflating of responses, active this.
          //   // deflate takes a lot of CPU time so it should only be enabled for
          //   // dedicated purposes and not generally
          //   if (responseBodyLength > 16384  && _acceptDeflate) {
          //     response->deflate();
          //     responseBodyLength = response->bodySize();
          //   }
          // }

          // reserve some outbuffer size
          triagens::basics::StringBuffer* buffer = new triagens::basics::StringBuffer(TRI_UNKNOWN_MEM_ZONE, responseBodyLength + 128);

          // write header
          response->writeHeader(buffer);

          // write body
          if (this->_requestType != HttpRequest::HTTP_REQUEST_HEAD) {
            buffer->appendText(response->body());
          }

          this->_writeBuffers.push_back(buffer);

#ifdef TRI_ENABLE_FIGURES
          this->_writeBuffersStats.push_back(RequestStatisticsAgent::transfer());
#endif

          LOG_TRACE("HTTP WRITE FOR %p: %s", (void*) this, buffer->c_str());

          // disable the following statement to prevent excessive logging of incoming requests
          LOG_USAGE(",\"http-request\",\"%s\",\"%s\",\"%s\",%d,%llu,%llu,\"%s\"",
                    this->_connectionInfo.clientAddress.c_str(),
                    HttpRequest::translateMethod(this->_requestType).c_str(),
                    HttpRequest::translateVersion(this->_httpVersion).c_str(),
                    (int) response->responseCode(),
                    (unsigned long long) this->_bodyLength,
                    (unsigned long long) responseBodyLength,
                    this->_fullUrl.c_str());

          // clear body
          response->body().clear();

          // start output
          this->fillWriteBuffer();

          // and process any remaining input
          processRead();
        }

////////////////////////////////////////////////////////////////////////////////
/// check the content-length header of a request and fail it is broken
////////////////////////////////////////////////////////////////////////////////

        bool checkContentLength (const bool expectContentLength) {
          const int64_t bodyLength = this->_request->contentLength();

          if (bodyLength < 0) {

            // bad request, body length is < 0. this is a client error
            HttpResponse response(HttpResponse::LENGTH_REQUIRED, getCompatibility());

            this->resetState(true);
            this->handleResponse(&response);

            return false;
          }

          if (! expectContentLength && bodyLength > 0) {
            // content-length header was sent but the request method does not support that
            // we'll warn but read the body anyway
            LOG_WARNING("received HTTP GET/HEAD request with content-length, this should not happen");
          }

          if ((size_t) bodyLength > this->_maximalBodySize) {
            LOG_WARNING("maximal body size is %d, request body size is %d", (int) this->_maximalBodySize, (int) bodyLength);

            // request entity too large
            HttpResponse response(HttpResponse::REQUEST_ENTITY_TOO_LARGE, getCompatibility());

            this->resetState(true);
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

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief http version number used
////////////////////////////////////////////////////////////////////////////////

        HttpRequest::HttpVersion _httpVersion;

////////////////////////////////////////////////////////////////////////////////
/// @brief type of request (GET, POST, ...)
////////////////////////////////////////////////////////////////////////////////

        HttpRequest::HttpRequestType _requestType;

////////////////////////////////////////////////////////////////////////////////
/// @brief value of requested URL
////////////////////////////////////////////////////////////////////////////////

        std::string _fullUrl;

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

////////////////////////////////////////////////////////////////////////////////
/// whether the client accepts deflate algorithm
////////////////////////////////////////////////////////////////////////////////

        bool _acceptDeflate;

////////////////////////////////////////////////////////////////////////////////
/// @brief new request started
////////////////////////////////////////////////////////////////////////////////

        bool _newRequest;

////////////////////////////////////////////////////////////////////////////////
/// @brief start position of current request
////////////////////////////////////////////////////////////////////////////////

        size_t _startPosition;

////////////////////////////////////////////////////////////////////////////////
/// @brief number of requests since last compactification
////////////////////////////////////////////////////////////////////////////////

        size_t _sinceCompactification;
    };
  }
}

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
