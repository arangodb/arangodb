////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Aalekh Nigam
////////////////////////////////////////////////////////////////////////////////

#include "VelocyCommTask.h"

#include "Basics/MutexLocker.h"
#include "Basics/StringBuffer.h"
#include "Basics/logging.h"
#include "HttpServer/ArangoTask.h" 
#include "VelocyServer/VelocyCommTask.h"
#include "HttpServer/GeneralHandlerFactory.h"
// #include "VelocyServer/VelocyHandlerFactory.h"
#include "HttpServer/GeneralServer.h"
#include "Scheduler/Scheduler.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::velocypack;
using namespace arangodb::rest;

// ////////////////////////////////////////////////////////////////////////////////
// /// @brief static initializers
// ////////////////////////////////////////////////////////////////////////////////

// size_t const VelocyCommTask::MaximalHeaderSize = 1 * 1024 * 1024;       //   1 MB
// size_t const VelocyCommTask::MaximalBodySize = 512 * 1024 * 1024;       // 512 MB
// size_t const VelocyCommTask::MaximalPipelineSize = 1024 * 1024 * 1024;  //   1 GB

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new task
////////////////////////////////////////////////////////////////////////////////

VelocyCommTask::VelocyCommTask(GeneralServer* server, TRI_socket_t socket,
                           ConnectionInfo const& info, double keepAliveTimeout)
    : Task("VelocyCommTask"), 
      ArangoTask(server, socket, info, keepAliveTimeout, "HttpCommTask", 
                            GeneralRequest::VSTREAM_UNKNOWN, GeneralRequest::VSTREAM_REQUEST_ILLEGAL),
      _isFirstChunk()
    {
  // connectionStatisticsAgentSetHttp(); @TODO: find STAT / TRI_stat_t structure and add velocystream support to it
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reads data from the socket
////////////////////////////////////////////////////////////////////////////////

bool VelocyCommTask::processRead() {
  if (_requestPending && _readBuffer->c_str() == nullptr) {
    return false;
  }
  bool handleRequest = false;

  // still trying to read the header fields
	  if (!_readRequestBody) {
	  	// starting a new request
	    if (_newRequest) {
	      // acquire a new statistics entry for the request
	      RequestStatisticsAgent::acquire();

	      _newRequest = false;
	      _httpVersion = GeneralRequest::VSTREAM_UNKNOWN;
	      _requestType = GeneralRequest::VSTREAM_REQUEST_ILLEGAL;
	      _fullUrl = "";
	      _denyCredentials = false;
	      _acceptDeflate = false;

	      _sinceCompactification++;
	    }

      requestStatisticsAgentSetReadStart();

	    if (_isFirstChunk && _readBuffer->length() > MaximalHeaderSize) {
	      LOG_WARNING("maximal header size is %d, request header size is %d",
	                  (int)MaximalHeaderSize, (int)_readBuffer->length());

	      // header is too large
	      GeneralResponse response(GeneralResponse::VstreamResponseCode::VSTREAM_REQUEST_HEADER_FIELDS_TOO_LARGE,
	                            getCompatibility());

	      resetState(true);
	      handleResponse(&response);

	      return false;
	    }

	if (_readBuffer->c_str() == nullptr) {

	  // @TODO: Create a new handler in HandlerFactory for VelocyStream
    /// insert _request here
      // // check that we know, how to serve this request and update the connection
      // // information, i. e. client and server addresses and ports and create a
      // // request context for that request
      // _request = _server->handlerFactory()->createRequest(
      //     _connectionInfo, _readBuffer->c_str() + _startPosition,
      //     _readPosition - _startPosition);

      if (_request == nullptr) {
        LOG_ERROR("cannot generate request");

        // internal server error
        GeneralResponse response(GeneralResponse::VstreamResponseCode::VSTREAM_SERVER_ERROR, getCompatibility());

        // we need to close the connection, because there is no way we
        // know how to remove the body and then continue
        resetState(true);
        handleResponse(&response);

        return false;
      }

      _request->setClientTaskId(_taskId);

      // check VSTREAM protocol version
      _httpVersion = _request->protocolVersion();

      // Currently we have aonly Vstream version 1.0 available
      if (_httpVersion != GeneralRequest::VSTREAM_1_0) {
        GeneralResponse response(GeneralResponse::VSTREAM_VERSION_NOT_SUPPORTED,
                              getCompatibility());

        // we need to close the connection, because there is no way we
        // know what to remove and then continue
        resetState(true);
        handleResponse(&response);

        return false;
      }

      // check max URL length
      _fullUrl = _request->fullUrl();

      if (_fullUrl.size() > 16384) {
        GeneralResponse response(GeneralResponse::VstreamResponseCode::VSTREAM_REQUEST_URI_TOO_LONG,
                              getCompatibility());

        // we need to close the connection, because there is no way we
        // know what to remove and then continue
        resetState(true);
        handleResponse(&response);

        return false;
      }

      // update the connection information, i. e. client and server addresses
      // and ports
      _request->setProtocol(_server->protocol());

      LOG_TRACE("server port %d, client port %d",
                (int)_connectionInfo.serverPort,
                (int)_connectionInfo.clientPort);

      // keep track of the original value of the "origin" request header (if
      // any)
      // we need this value to handle CORS requests
      _origin = _request->header("origin");

      if (!_origin.empty()) {
        // check for Access-Control-Allow-Credentials header
        bool found;
        std::string const& allowCredentials =
            _request->header("access-control-allow-credentials", found);

        if (found) {
          _denyCredentials = !StringUtils::boolean(allowCredentials);
        }
      }

      // store the original request's type. we need it later when responding
      // (original request object gets deleted before responding)
      _requestType = _request->requestType();

      requestStatisticsAgentSetRequestType(_requestType);

      // handle different VSTREAM methods
      switch (_requestType) {
        case GeneralRequest::VSTREAM_REQUEST_GET:
        case GeneralRequest::VSTREAM_REQUEST_DELETE:
        case GeneralRequest::VSTREAM_REQUEST_HEAD:
        case GeneralRequest::VSTREAM_REQUEST_OPTIONS:
        case GeneralRequest::VSTREAM_REQUEST_POST:
        case GeneralRequest::VSTREAM_REQUEST_PUT:
        case GeneralRequest::VSTREAM_REQUEST_PATCH: 
        case GeneralRequest::VSTREAM_REQUEST_CRED:
        case GeneralRequest::VSTREAM_REQUEST_REGISTER:
        case GeneralRequest::VSTREAM_REQUEST_STATUS:{
          // technically, sending a body for an VSTREAM DELETE request is not
          // forbidden, but it is not explicitly supported
          bool const expectContentLength =
              (_requestType == GeneralRequest::VSTREAM_REQUEST_POST ||
               _requestType == GeneralRequest::VSTREAM_REQUEST_PUT ||
               _requestType == GeneralRequest::VSTREAM_REQUEST_PATCH ||
               _requestType == GeneralRequest::VSTREAM_REQUEST_OPTIONS ||
               _requestType == GeneralRequest::VSTREAM_REQUEST_DELETE ||
               _requestType == GeneralRequest::VSTREAM_REQUEST_CRED||
               _requestType == GeneralRequest::VSTREAM_REQUEST_REGISTER||
               _requestType == GeneralRequest::VSTREAM_REQUEST_STATUS);

        if(!_isFirstChunk){ 
          if (_readBuffer->length() == 0) {
            handleRequest = true;
          }
        }  
          break;
        }

        default: {

          LOG_WARNING( "got corrupted VELOCYSTREAM request ");

          // bad request, method not allowed
          GeneralResponse response(GeneralResponse::VstreamResponseCode::VSTREAM_METHOD_NOT_ALLOWED,
                                getCompatibility());

          // we need to close the connection, because there is no way we
          // know what to remove and then continue
          resetState(true);

          // force a socket close, response will be ignored!
          TRI_CLOSE_SOCKET(_commSocket);
          TRI_invalidatesocket(&_commSocket);

          // might delete this
          handleResponse(&response);

          return false;
        }
      }

      // .............................................................................
      // check if server is active
      // .............................................................................

      Scheduler const* scheduler =
          _server->scheduler(); 

      if (scheduler != nullptr && !scheduler->isActive()) {
        LOG_TRACE("cannot serve request - server is inactive");

        GeneralResponse response(GeneralResponse::VSTREAM_SERVICE_UNAVAILABLE,
                              getCompatibility());

        resetState(true);
        handleResponse(&response);

        return false;
      }

      // @TODO: handle write buffer here for vpack
      // if (_readRequestBody) {  
      //   bool found;
      //   std::string const& expect = _request->header("expect", found);

      //   if (found && StringUtils::trim(expect) == "100-continue") {
      //     LOG_TRACE("received a 100-continue request");

      //     auto buffer = std::make_unique<StringBuffer>(TRI_UNKNOWN_MEM_ZONE);
      //     buffer->appendText(
      //         TRI_CHAR_LENGTH_PAIR("HTTP/1.1 100 (Continue)\r\n\r\n"));

      //     _writeBuffers.push_back(buffer.get());
      //     buffer.release();

      //     _writeBuffersStats.push_back(nullptr);

      //     fillWriteBuffer();
      //   }
      // }

  	} 
  } 

  // readRequestBody might have changed, so cannot use else
  if (!_isFirstChunk) {

    // read "bodyLength" from read buffer and add this body to "GeneralRequest"
    _request->setBody(_readBuffer->c_str(), _readBuffer->length());

    // LOG_TRACE("%s", std::to_string(_readBuffer->length()));

    // remove body from read buffer and reset read position
    _readRequestBody = false;
    handleRequest = true;
  }

  if (!handleRequest) {
    return false;
  }
  if(!_isFirstChunk){
  	requestStatisticsAgentSetReadEnd();
  	requestStatisticsAgentAddReceivedBytes(_readBuffer->length());
  }
  bool const isOptionsRequest =
      (_requestType == GeneralRequest::VSTREAM_REQUEST_OPTIONS);
  resetState(false);

  // .............................................................................
  // keep-alive handling
  // .............................................................................

  std::string connectionType =
      StringUtils::tolower(_request->header("connection"));

  if (connectionType == "close") {
    LOG_DEBUG("connection close requested by client");
    _closeRequested = true;
  } else if (connectionType != "keep-alive") {
    LOG_DEBUG("no keep-alive, connection close requested by client");
    _closeRequested = true;
  } else if (_keepAliveTimeout <= 0.0) {
    LOG_DEBUG("keep-alive disabled by admin");
    _closeRequested = true;
  }

  // .............................................................................
  // authenticate
  // .............................................................................

  auto const compatibility = _request->compatibility();

  GeneralResponse::VstreamResponseCode authResult = GeneralResponse::VSTREAM_OK;

      // _server->handlerFactory()->authenticateRequestVstream(_request); //@TODO: Create authentication handler for velocypack

  if (authResult == GeneralResponse::VSTREAM_OK || isOptionsRequest) {
    if (isOptionsRequest) {
      processCorsOptions(compatibility);
    } else {
      processRequest(compatibility);
    }
  }

    // not found
  else if (authResult == GeneralResponse::VstreamResponseCode::VSTREAM_NOT_FOUND) {
    GeneralResponse response(authResult, compatibility);

    // arangodb::velocypack::Builder b;                                                                                                         
  	response.bodyVpack().add(arangodb::velocypack::Value(arangodb::velocypack::ValueType::Object));                                                                                   
  	response.bodyVpack().add("error", arangodb::velocypack::Value("true"));                                                                              
  	response.bodyVpack().add("errorMessage", arangodb::velocypack::Value(TRI_errno_string(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND)));                                                                                  
  	response.bodyVpack().add("code:", arangodb::velocypack::Value(std::to_string((int)authResult)));                                                 
  	response.bodyVpack().add("errorNum", Value(std::to_string(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND)));
  	response.bodyVpack().close();

    clearRequest();
    handleResponse(&response); // Create a response for builder objects
  }

  // forbidden
  else if (authResult == GeneralResponse::VstreamResponseCode::VSTREAM_FORBIDDEN) {
    GeneralResponse response(authResult, compatibility);
    // arangodb::velocypack::Builder b;                                                                                                         
  	response.bodyVpack().add(arangodb::velocypack::Value(arangodb::velocypack::ValueType::Object));                                                                                   
  	response.bodyVpack().add("error", Value("true"));                                                                              
  	response.bodyVpack().add("errorMessage", Value("change password"));                                                                                  
  	response.bodyVpack().add("code:", Value(authResult));                                                 
  	response.bodyVpack().add("errorNum", Value(std::to_string(TRI_ERROR_USER_CHANGE_PASSWORD)));
  	response.bodyVpack().close();

    clearRequest();
    handleResponse(&response
      );
  } else{
  	GeneralResponse response(GeneralResponse::VstreamResponseCode::VSTREAM_UNAUTHORIZED, compatibility);
    std::string const realm = "basic realm=\"" + _server->handlerFactory()->authenticationRealm(_request) + "\"";

    if (sendWwwAuthenticateHeader()) {
      response.setHeader(TRI_CHAR_LENGTH_PAIR("www-authenticate"),
                         realm.c_str());
    }

    clearRequest();
    handleResponse(&response);
  }
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sends more chunked data
////////////////////////////////////////////////////////////////////////////////

// void VelocyCommTask::sendChunk(arangodb::velocypack::Builder buffer) {
//   if (_isChunked) {
//     TRI_ASSERT(buffer != nullptr);

//     _writeBuffers.push_back(buffer);
//     _writeBuffersStats.push_back(nullptr);

//     fillWriteBuffer();
//   } else {
//     delete buffer;
//   }
// }

////////////////////////////////////////////////////////////////////////////////
/// @brief reads data from the socket
////////////////////////////////////////////////////////////////////////////////

void VelocyCommTask::addResponse(GeneralResponse* response) {

  if (!_origin.empty()) {

    LOG_TRACE("handling CORS response");

    response->setHeader(TRI_CHAR_LENGTH_PAIR("access-control-expose-headers"),
                        "etag, content-encoding, content-length, location, "
                        "server, x-arango-errors, x-arango-async-id");

    // send back original value of "Origin" header
    response->setHeader(TRI_CHAR_LENGTH_PAIR("access-control-allow-origin"),
                        _origin);

    // send back "Access-Control-Allow-Credentials" header
    response->setHeader(
        TRI_CHAR_LENGTH_PAIR("access-control-allow-credentials"),
        (_denyCredentials ? "false" : "true"));
  }

  response->setHeader(TRI_CHAR_LENGTH_PAIR("connection"),
                      (_closeRequested ? "Close" : "Keep-Alive"));

  size_t const responseBodyLength = response->bodySize();

  if (_requestType == GeneralRequest::VSTREAM_REQUEST_HEAD) {

    response->headResponseVpack(responseBodyLength);
  }

  // reserve a buffer with some spare capacity
  auto buffer = std::make_unique<basics::StringBuffer*>(TRI_UNKNOWN_MEM_ZONE,
                                               responseBodyLength + 128);

  // write header
  response->writeHeader(buffer.get());

  // @TODO : Need to revaluate this, behavior is not same as Http should be VelocyPack instead
  // // write body
  // if (_requestType != GeneralRequest::VSTREAM_REQUEST_HEAD) {
  //   if (_isChunked) {
  //     if (0 != responseBodyLength) {
  //       buffer->appendHex(response->body().length());
  //       buffer->appendText(response->body());
  //     }
  //   } else {
  //     buffer->appendText(response->body());
  //   }
  // }

  _writeBuffers.push_back(buffer.get());
  auto b = buffer.release();

  // LOG_TRACE("VSTREAM WRITE FOR %p: %s", (void*)this, b->c_str());

  // clear body
  response->bodyVpack().clear();

  double const totalTime = RequestStatisticsAgent::elapsedSinceReadStart();

  _writeBuffersStats.push_back(RequestStatisticsAgent::transfer());

  // disable the following statement to prevent excessive logging of incoming
  // requests
  LOG_USAGE(
      ",\"velocystream-request\",\"%s\",\"%s\",\"%s\",%d,%llu,%llu,\"%s\",%.6f",
      _connectionInfo.clientAddress.c_str(),
      GeneralRequest::translateMethod(_requestType).c_str(),
      GeneralRequest::translateVersion(_httpVersion).c_str(),
      (int)response->responseCode(), (unsigned long long)_originalBodyLength,
      (unsigned long long)responseBodyLength, _fullUrl.c_str(), totalTime);

  // start output
  fillWriteBuffer();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief handles CORS options
////////////////////////////////////////////////////////////////////////////////

void VelocyCommTask::processCorsOptions(uint32_t compatibility) {
  std::string const allowedMethods = "DELETE, GET, HEAD, PATCH, POST, PUT, REGISTER";

  GeneralResponse response(GeneralResponse::VSTREAM_OK, compatibility);

  response.setHeader(TRI_CHAR_LENGTH_PAIR("allow"), allowedMethods);

  if (!_origin.empty()) {
    LOG_TRACE("got CORS preflight request");
    std::string const allowHeaders =
        StringUtils::trim(_request->header("access-control-request-headers"));

    // send back which VSTREAM methods are allowed for the resource
    // we'll allow all
    response.setHeader(TRI_CHAR_LENGTH_PAIR("access-control-allow-methods"),
                       allowedMethods);

    if (!allowHeaders.empty()) {
      response.setHeader(TRI_CHAR_LENGTH_PAIR("access-control-allow-headers"),
                         allowHeaders);
      LOG_TRACE("client requested validation of the following headers: %s",
                allowHeaders.c_str());
    }
    response.setHeader(TRI_CHAR_LENGTH_PAIR("access-control-max-age"), "1800");
  }

  clearRequest();
  handleResponse(&response);
}

////////////
// @TODO: Works on checkContentLength
////////////


///////////////////////////////////////////////////////////////////////////////
/// @brief processes a request
////////////////////////////////////////////////////////////////////////////////

void VelocyCommTask::processRequest(uint32_t compatibility) {

  bool found;
  std::string const& acceptEncoding =
      _request->header("accept-encoding", found);

  if (found) {
    if (acceptEncoding.find("deflate") != std::string::npos) {
      _acceptDeflate = true;
    }
  }

  std::string const& asyncExecution = _request->header("x-arango-async", found);

  WorkItem::uptr<GeneralHandler> handler(
      _server->handlerFactory()->createHandler(_request));

  if (handler == nullptr) {
    LOG_TRACE("no handler is known, giving up");

    GeneralResponse response(GeneralResponse::VSTREAM_NOT_FOUND, compatibility);

    clearRequest();
    handleResponse(&response);

    return;
  }

  handler->setTaskId(_taskId, _loop);


  _request = nullptr;
  RequestStatisticsAgent::transfer(handler.get());


  bool ok = false;

  if (found && (asyncExecution == "true" || asyncExecution == "store")) {
    requestStatisticsAgentSetAsync();
    uint64_t jobId = 0;

    if (asyncExecution == "store") {

      ok = _server->handleRequestAsync(handler, &jobId);
    } else {

      ok = _server->handleRequestAsync(handler, nullptr);
    }

    if (ok) {
      GeneralResponse response(GeneralResponse::VSTREAM_ACCEPTED, compatibility);

      if (jobId > 0) {

        response.setHeader(TRI_CHAR_LENGTH_PAIR("x-arango-async-id"),
                           StringUtils::itoa(jobId));
      }

      handleResponse(&response);

      return;
    }
  }

  else {
    ok = _server->handleRequest(this, handler);
  }

  if (!ok) {
    GeneralResponse response(GeneralResponse::VSTREAM_SERVER_ERROR, compatibility);
    handleResponse(&response);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get request compatibility
////////////////////////////////////////////////////////////////////////////////

int32_t VelocyCommTask::getCompatibility() const {
  if (_request != nullptr) {
    return _request->compatibility();
  }

  return GeneralRequest::MinCompatibility;
}