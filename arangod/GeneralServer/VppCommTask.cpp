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
///     vpp://www.apache.org/licenses/LICENSE-2.0
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
////////////////////////////////////////////////////////////////////////////////

#include "VppCommTask.h"

#include "Basics/HybridLogicalClock.h"
#include "GeneralServer/GeneralServer.h"
#include "GeneralServer/GeneralServerFeature.h"
#include "GeneralServer/RestHandler.h"
#include "GeneralServer/RestHandlerFactory.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"
#include "VocBase/ticks.h"

#include <velocypack/Validator.h>
#include <velocypack/velocypack-aliases.h>

#include <stdexcept>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

namespace {
std::size_t findAndValidateVPacks(char const* vpHeaderStart,
                                  char const* chunkEnd) {
  VPackValidator validator;
  // check for slice start to the end of Chunk
  // isSubPart allows the slice to be shorter than the checked buffer.
  validator.validate(vpHeaderStart, std::distance(vpHeaderStart, chunkEnd),
                     /*isSubPart =*/true);

  // check if there is payload and locate the start
  VPackSlice vpHeader(vpHeaderStart);
  auto vpHeaderLen = vpHeader.byteSize();
  auto vpPayloadStart = vpHeaderStart + vpHeaderLen;
  if (vpPayloadStart == chunkEnd) {
    return 0;  // no payload available
  }

  // validate Payload VelocyPack
  validator.validate(vpPayloadStart, std::distance(vpPayloadStart, chunkEnd),
                     /*isSubPart =*/false);
  return std::distance(vpHeaderStart, vpPayloadStart);
}
}

void VppCommTask::addResponse(VppResponse* response, bool isError) {
  /*
  _requestPending = false;
  _isChunked = false;

  if (isError) {
    resetState(true);
  }

  // CORS response handling
  if (!_origin.empty()) {
    // the request contained an Origin header. We have to send back the
    // access-control-allow-origin header now
    LOG(TRACE) << "handling CORS response";

    response->setHeaderNC(StaticStrings::AccessControlExposeHeaders,
                          StaticStrings::ExposedCorsHeaders);

    // send back original value of "Origin" header
    response->setHeaderNC(StaticStrings::AccessControlAllowOrigin, _origin);

    // send back "Access-Control-Allow-Credentials" header
    response->setHeaderNC(StaticStrings::AccessControlAllowCredentials,
                          (_denyCredentials ? "false" : "true"));
  }
  // CORS request handling EOF

  // set "connection" header
  // keep-alive is the default
  response->setConnectionType(_closeRequested
                                  ? VppResponse::CONNECTION_CLOSE
                                  : VppResponse::CONNECTION_KEEP_ALIVE);

  size_t const responseBodyLength = response->bodySize();

  if (_requestType == GeneralRequest::RequestType::HEAD) {
    // clear body if this is an vpp HEAD request
    // HEAD must not return a body
    response->headResponse(responseBodyLength);
  }

  // reserve a buffer with some spare capacity
  auto buffer = std::make_unique<StringBuffer>(TRI_UNKNOWN_MEM_ZONE,
                                               responseBodyLength + 128, false);

  // write header
  response->writeHeader(buffer.get());

  // write body
  if (_requestType != GeneralRequest::RequestType::HEAD) {
    if (_isChunked) {
      if (0 != responseBodyLength) {
        buffer->appendHex(response->body().length());
        buffer->appendText(TRI_CHAR_LENGTH_PAIR("\r\n"));
        buffer->appendText(response->body());
        buffer->appendText(TRI_CHAR_LENGTH_PAIR("\r\n"));
      }
    } else {
      buffer->appendText(response->body());
    }
  }

  buffer->ensureNullTerminated();

  _writeBuffers.push_back(buffer.get());
  auto b = buffer.release();

  if (!b->empty()) {
    LOG_TOPIC(TRACE, Logger::REQUESTS)
        << "\"vpp-request-response\",\"" << (void*)this << "\",\""
        << StringUtils::escapeUnicode(std::string(b->c_str(), b->length()))
        << "\"";
  }

  // clear body
  response->body().clear();

  double const totalTime = RequestStatisticsAgent::elapsedSinceReadStart();

  _writeBuffersStats.push_back(RequestStatisticsAgent::steal());

  LOG_TOPIC(INFO, Logger::REQUESTS)
      << "\"vpp-request-end\",\"" << (void*)this << "\",\""
      << _connectionInfo.clientAddress << "\",\""
      << VppRequest::translateMethod(_requestType) << "\",\""
      << VppRequest::translateVersion(_protocolVersion) << "\","
      << static_cast<int>(response->responseCode()) << ","
      << _originalBodyLength << "," << responseBodyLength << ",\"" << _fullUrl
      << "\"," << Logger::FIXED(totalTime, 6);

  // start output
  fillWriteBuffer();
  */
}

VppCommTask::ChunkHeader VppCommTask::readChunkHeader() {
  VppCommTask::ChunkHeader header;

  auto cursor = _readBuffer->begin();

  std::memcpy(&header._chunkLength, cursor, sizeof(header._chunkLength));
  cursor += sizeof(header._chunkLength);

  uint32_t chunkX;
  std::memcpy(&chunkX, cursor, sizeof(chunkX));
  cursor += sizeof(chunkX);

  header._isFirst = chunkX & 0x1;
  header._chunk = chunkX >> 1;

  std::memcpy(&header._messageId, cursor, sizeof(header._messageId));
  cursor += sizeof(header._messageId);

  // extract total len of message
  if (header._isFirst && header._chunk == 1) {
    std::memcpy(&header._messageLength, cursor, sizeof(header._messageLength));
    cursor += sizeof(header._messageLength);
  } else {
    header._messageLength = 0;  // not needed
  }

  header._headerLength = std::distance(_readBuffer->begin(), cursor);

  return header;
}

bool VppCommTask::isChunkComplete() {
  auto start = _readBuffer->begin();
  std::size_t length = std::distance(start, _readBuffer->end());
  auto& prv = _processReadVariables;

  if (!prv._currentChunkLength && length < sizeof(uint32_t)) {
    return false;
  }
  if (!prv._currentChunkLength) {
    // read chunk length
    std::memcpy(&prv._currentChunkLength, start, sizeof(uint32_t));
  }
  if (length < prv._currentChunkLength) {
    // chunk not complete
    return false;
  }

  return true;
}

// reads data from the socket
bool VppCommTask::processRead() {
  auto chunkBegin = _readBuffer->begin();
  if (chunkBegin == nullptr || !isChunkComplete()) {
    return true;  // no data or incomplete
  }

  auto chunkHeader = readChunkHeader();
  auto chunkEnd = chunkBegin + chunkHeader._chunkLength;
  auto vpackBegin = chunkBegin + chunkHeader._headerLength;
  bool do_execute = false;

  // CASE 1: message is in one chunk
  if (chunkHeader._isFirst && chunkHeader._chunk == 1) {
    std::size_t payloadOffset = findAndValidateVPacks(vpackBegin, chunkEnd);
    VPackMessage message;
    message._buffer.append(vpackBegin, std::distance(vpackBegin, chunkEnd));
    message._header = VPackSlice(message._buffer.data());
    if (payloadOffset) {
      message._payload = VPackSlice(message._buffer.data() + payloadOffset);
    }
    do_execute = true;
  }

  // CASE 2:  message is in multiple chunks
  auto incompleteMessageItr = _incompleteMessages.find(chunkHeader._messageId);

  // CASE 2a: chunk starts new message
  if (chunkHeader._isFirst) {  // first chunk of multi chunk message
    if (incompleteMessageItr != _incompleteMessages.end()) {
      throw std::logic_error(
          "Message should be first but is already in the Map of incomplete "
          "messages");
    }

    IncompleteVPackMessage message(chunkHeader._messageLength,
                                   chunkHeader._chunk /*number of chunks*/);
    message._buffer.append(vpackBegin, std::distance(vpackBegin, chunkEnd));
    auto insertPair = _incompleteMessages.insert(
        std::make_pair(chunkHeader._messageId, std::move(message)));
    if (!insertPair.second) {
      throw std::logic_error("insert failed");
    }

    // CASE 2b: chunk continues a message
  } else {  // followup chunk of some mesage
    if (incompleteMessageItr == _incompleteMessages.end()) {
      throw std::logic_error("found message without previous part");
    }
    auto& im = incompleteMessageItr->second;  // incomplete Message
    im._currentChunk++;
    assert(im._currentChunk == chunkHeader._chunk);
    im._buffer.append(vpackBegin, std::distance(vpackBegin, chunkEnd));

    // MESSAGE COMPLETE
    if (im._currentChunk == im._numberOfChunks) {
      std::size_t payloadOffset = findAndValidateVPacks(
          reinterpret_cast<char const*>(im._buffer.data()),
          reinterpret_cast<const char*>(im._buffer.data() +
                                        im._buffer.byteSize()));
      VPackMessage message;
      message._buffer = std::move(im._buffer);
      message._header = VPackSlice(message._buffer.data());
      if (payloadOffset) {
        message._payload = VPackSlice(message._buffer.data() + payloadOffset);
      }
      do_execute = true;
    }
  }
  (void)do_execute;
  // clean buffer up to length of chunk
  return true;

  /*
      if (RequestStatisticsAgent::_statistics != nullptr) {
        RequestStatisticsAgent::_statistics->_id = (void*)this;
      }
  #endif

      _newRequest = false;
      _startPosition = _readPosition;
      _protocolVersion = GeneralRequest::ProtocolVersion::UNKNOWN;
      _requestType = GeneralRequest::RequestType::ILLEGAL;
      _fullUrl = "";
      _denyCredentials = true;
      _acceptDeflate = false;

      _sinceCompactification++;
    }

    char const* end = etr - 3;

    // read buffer contents are way to small. we can exit here directly
    if (ptr >= end) {
      return false;
    }

    // request started
    requestStatisticsAgentSetReadStart();

    // check for the end of the request
    for (; ptr < end; ptr++) {
      if (ptr[0] == '\r' && ptr[1] == '\n' && ptr[2] == '\r' &&
          ptr[3] == '\n') {
        break;
      }
    }

    // check if header is too large
    size_t headerLength = ptr - (_readBuffer->c_str() + _startPosition);

    if (headerLength > MaximalHeaderSize) {
      LOG(WARN) << "maximal header size is " << MaximalHeaderSize
                << ", request header size is " << headerLength;

      // header is too large
      handleSimpleError(
          GeneralResponse::ResponseCode::REQUEST_HEADER_FIELDS_TOO_LARGE);
      return false;
    }

    // header is complete
    if (ptr < end) {
      _readPosition = ptr - _readBuffer->c_str() + 4;

      LOG(TRACE) << "vpp READ FOR " << (void*)this << ": "
                 << std::string(_readBuffer->c_str() + _startPosition,
                                _readPosition - _startPosition);

      // check that we know, how to serve this request and update the
  connection
      // information, i. e. client and server addresses and ports and create a
      // request context for that request
      _request = new VppRequest(
          _connectionInfo, _readBuffer->c_str() + _startPosition,
          _readPosition - _startPosition, _allowMethodOverride);

      GeneralServerFeature::HANDLER_FACTORY->setRequestContext(_request);
      _request->setClientTaskId(_taskId);

      // check vpp protocol version
      _protocolVersion = _request->protocolVersion();

      if (_protocolVersion != GeneralRequest::ProtocolVersion::vpp_1_0 &&
          _protocolVersion != GeneralRequest::ProtocolVersion::vpp_1_1) {
        handleSimpleError(
            GeneralResponse::ResponseCode::vpp_VERSION_NOT_SUPPORTED);
        return false;
      }

      // check max URL length
      _fullUrl = _request->fullUrl();

      if (_fullUrl.size() > 16384) {
        handleSimpleError(GeneralResponse::ResponseCode::REQUEST_URI_TOO_LONG);
        return false;
      }

      // update the connection information, i. e. client and server addresses
      // and ports
      _request->setProtocol(_protocol);

      LOG(TRACE) << "server port " << _connectionInfo.serverPort
                 << ", client port " << _connectionInfo.clientPort;

      // set body start to current position
      _bodyPosition = _readPosition;
      _bodyLength = 0;

      // keep track of the original value of the "origin" request header (if
      // any), we need this value to handle CORS requests
      _origin = _request->header(StaticStrings::Origin);

      if (!_origin.empty()) {
        // check for Access-Control-Allow-Credentials header
        bool found;
        std::string const& allowCredentials = _request->header(
            StaticStrings::AccessControlAllowCredentials, found);

        if (found) {
          // default is to allow nothing
          _denyCredentials = true;

          // if the request asks to allow credentials, we'll check against the
          // configured whitelist of origins
          std::vector<std::string> const& accessControlAllowOrigins =
              GeneralServerFeature::accessControlAllowOrigins();

          if (StringUtils::boolean(allowCredentials) &&
              !accessControlAllowOrigins.empty()) {
            if (accessControlAllowOrigins[0] == "*") {
              // special case: allow everything
              _denyCredentials = false;
            } else if (!_origin.empty()) {
              // copy origin string
              if (_origin[_origin.size() - 1] == '/') {
                // strip trailing slash
                auto result = std::find(accessControlAllowOrigins.begin(),
                                        accessControlAllowOrigins.end(),
                                        _origin.substr(0, _origin.size() -
  1));
                _denyCredentials = (result ==
  accessControlAllowOrigins.end());
              } else {
                auto result =
                    std::find(accessControlAllowOrigins.begin(),
                              accessControlAllowOrigins.end(), _origin);
                _denyCredentials = (result ==
  accessControlAllowOrigins.end());
              }
            } else {
              TRI_ASSERT(_denyCredentials);
            }
          }
        }
      }

      // store the original request's type. we need it later when responding
      // (original request object gets deleted before responding)
      _requestType = _request->requestType();

      requestStatisticsAgentSetRequestType(_requestType);

      // handle different vpp methods
      switch (_requestType) {
        case GeneralRequest::RequestType::GET:
        case GeneralRequest::RequestType::DELETE_REQ:
        case GeneralRequest::RequestType::HEAD:
        case GeneralRequest::RequestType::OPTIONS:
        case GeneralRequest::RequestType::POST:
        case GeneralRequest::RequestType::PUT:
        case GeneralRequest::RequestType::PATCH: {
          // technically, sending a body for an vpp DELETE request is not
          // forbidden, but it is not explicitly supported
          bool const expectContentLength =
              (_requestType == GeneralRequest::RequestType::POST ||
               _requestType == GeneralRequest::RequestType::PUT ||
               _requestType == GeneralRequest::RequestType::PATCH ||
               _requestType == GeneralRequest::RequestType::OPTIONS ||
               _requestType == GeneralRequest::RequestType::DELETE_REQ);

          if (!checkContentLength(expectContentLength)) {
            return false;
          }

          if (_bodyLength == 0) {
            handleRequest = true;
          }

          break;
        }

        default: {
          size_t l = _readPosition - _startPosition;

          if (6 < l) {
            l = 6;
          }

          LOG(WARN) << "got corrupted vpp request '"
                    << std::string(_readBuffer->c_str() + _startPosition, l)
                    << "'";

          // force a socket close, response will be ignored!
          TRI_CLOSE_SOCKET(_commSocket);
          TRI_invalidatesocket(&_commSocket);

          // bad request, method not allowed
          handleSimpleError(GeneralResponse::ResponseCode::METHOD_NOT_ALLOWED);
          return false;
        }
      }

      //
  .............................................................................
      // check if server is active
      //
  .............................................................................

      Scheduler const* scheduler = SchedulerFeature::SCHEDULER;

      if (scheduler != nullptr && !scheduler->isActive()) {
        // server is inactive and will intentionally respond with vpp 503
        LOG(TRACE) << "cannot serve request - server is inactive";

        handleSimpleError(GeneralResponse::ResponseCode::SERVICE_UNAVAILABLE);
        return false;
      }

      // check for a 100-continue
      if (_readRequestBody) {
        bool found;
        std::string const& expect =
            _request->header(StaticStrings::Expect, found);

        if (found && StringUtils::trim(expect) == "100-continue") {
          LOG(TRACE) << "received a 100-continue request";

          auto buffer = std::make_unique<StringBuffer>(TRI_UNKNOWN_MEM_ZONE);
          buffer->appendText(
              TRI_CHAR_LENGTH_PAIR("vpp/1.1 100 (Continue)\r\n\r\n"));
          buffer->ensureNullTerminated();

          _writeBuffers.push_back(buffer.get());
          buffer.release();

          _writeBuffersStats.push_back(nullptr);

          fillWriteBuffer();
        }
      }
    } else {
      size_t l = (_readBuffer->end() - _readBuffer->c_str());

      if (_startPosition + 4 <= l) {
        _readPosition = l - 4;
      }
    }
  }

  // readRequestBody might have changed, so cannot use else
  if (_readRequestBody) {
    if (_readBuffer->length() - _bodyPosition < _bodyLength) {
      setKeepAliveTimeout(_keepAliveTimeout);

      // let client send more
      return false;
    }

    // read "bodyLength" from read buffer and add this body to "vppRequest"
    requestAsVpp()->setBody(_readBuffer->c_str() + _bodyPosition,
  _bodyLength);

    LOG(TRACE) << "" << std::string(_readBuffer->c_str() + _bodyPosition,
                                    _bodyLength);

    // remove body from read buffer and reset read position
    _readRequestBody = false;
    handleRequest = true;
  }

  //
  .............................................................................
  // request complete
  //
  // we have to delete request in here or pass it to a handler, which will
  // delete
  // it
  //
  .............................................................................

  if (!handleRequest) {
    return false;
  }

  requestStatisticsAgentSetReadEnd();
  requestStatisticsAgentAddReceivedBytes(_bodyPosition - _startPosition +
                                         _bodyLength);

  bool const isOptionsRequest =
      (_requestType == GeneralRequest::RequestType::OPTIONS);
  resetState(false);

  //
  .............................................................................
  // keep-alive handling
  //
  .............................................................................

  std::string connectionType =
      StringUtils::tolower(_request->header(StaticStrings::Connection));

  if (connectionType == "close") {
    // client has sent an explicit "Connection: Close" header. we should close
    // the connection
    LOG(DEBUG) << "connection close requested by client";
    _closeRequested = true;
  } else if (requestAsVpp()->isVpp10() && connectionType != "keep-alive") {
    // vpp 1.0 request, and no "Connection: Keep-Alive" header sent
    // we should close the connection
    LOG(DEBUG) << "no keep-alive, connection close requested by client";
    _closeRequested = true;
  } else if (_keepAliveTimeout <= 0.0) {
    // if keepAliveTimeout was set to 0.0, we'll close even keep-alive
    // connections immediately
    LOG(DEBUG) << "keep-alive disabled by admin";
    _closeRequested = true;
  }

  // we keep the connection open in all other cases (vpp 1.1 or Keep-Alive
  // header sent)

  //
  .............................................................................
  // authenticate
  //
  .............................................................................

  GeneralResponse::ResponseCode authResult = authenticateRequest();

  // authenticated or an OPTIONS request. OPTIONS requests currently go
  // unauthenticated
  if (authResult == GeneralResponse::ResponseCode::OK || isOptionsRequest) {
    // handle vpp OPTIONS requests directly
    if (isOptionsRequest) {
      processCorsOptions();
    } else {
      processRequest();
    }
  }
  // not found
  else if (authResult == GeneralResponse::ResponseCode::NOT_FOUND) {
    handleSimpleError(authResult, TRI_ERROR_ARANGO_DATABASE_NOT_FOUND,
                      TRI_errno_string(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND));
  }
  // forbidden
  else if (authResult == GeneralResponse::ResponseCode::FORBIDDEN) {
    handleSimpleError(authResult, TRI_ERROR_USER_CHANGE_PASSWORD,
                      "change password");
  }
  // not authenticated
  else {
    VppResponse response(GeneralResponse::ResponseCode::UNAUTHORIZED);
    std::string realm = "Bearer token_type=\"JWT\", realm=\"ArangoDB\"";

    response.setHeaderNC(StaticStrings::WwwAuthenticate, std::move(realm));

    clearRequest();
    processResponse(&response);
  }

  */
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief processes a request
////////////////////////////////////////////////////////////////////////////////

void VppCommTask::processRequest() {
  /*
  // check for deflate
  bool found;

  auto vppRequest = requestAsVpp();
  std::string const& acceptEncoding =
      vppRequest->header(StaticStrings::AcceptEncoding, found);

  if (found) {
    if (acceptEncoding.find("deflate") != std::string::npos) {
      _acceptDeflate = true;
    }
  }

  if (vppRequest != nullptr) {
    LOG_TOPIC(DEBUG, Logger::REQUESTS)
        << "\"vpp-request-begin\",\"" << (void*)this << "\",\""
        << _connectionInfo.clientAddress << "\",\""
        << VppRequest::translateMethod(_requestType) << "\",\""
        << VppRequest::translateVersion(_protocolVersion) << "\"," << _fullUrl
        << "\"";

    std::string const& body = vppRequest->body();

    if (!body.empty()) {
      LOG_TOPIC(DEBUG, Logger::REQUESTS)
          << "\"vpp-request-body\",\"" << (void*)this << "\",\""
          << (StringUtils::escapeUnicode(body)) << "\"";
    }
  }

  // check for an HLC time stamp
  std::string const& timeStamp =
      _request->header(StaticStrings::HLCHeader, found);
  if (found) {
    uint64_t timeStampInt =
        arangodb::basics::HybridLogicalClock::decodeTimeStampWithCheck(
            timeStamp);
    if (timeStampInt != 0) {
      TRI_HybridLogicalClock(timeStampInt);
    }
  }

  // create a handler and execute
  executeRequest(_request,
                 new
  VppResponse(GeneralResponse::ResponseCode::SERVER_ERROR));
                */
}

void VppCommTask::completedWriteBuffer() {
  //  _writeBuffer = nullptr;
  //  _writeLength = 0;
  //
  //  if (_writeBufferStatistics != nullptr) {
  //    _writeBufferStatistics->_writeEnd = TRI_StatisticsTime();
  //    TRI_ReleaseRequestStatistics(_writeBufferStatistics);
  //    _writeBufferStatistics = nullptr;
  //  }
  //
  //  fillWriteBuffer();
  //
  //  if (!_clientClosed && _closeRequested && !hasWriteBuffer() &&
  //      _writeBuffers.empty() && !_isChunked) {
  //    _clientClosed = true;
  //  }
}

void VppCommTask::resetState(bool close) {
  /*
  if (close) {
    clearRequest();

    _requestPending = false;
    _isChunked = false;
    _closeRequested = true;

    _readPosition = 0;
    _bodyPosition = 0;
    _bodyLength = 0;
  } else {
    _requestPending = true;

    bool compact = false;

    if (_sinceCompactification > RunCompactEvery) {
      compact = true;
    } else if (_readBuffer->length() > MaximalPipelineSize) {
      compact = true;
    }

    if (compact) {
      _readBuffer->erase_front(_bodyPosition + _bodyLength);

      _sinceCompactification = 0;
      _readPosition = 0;
    } else {
      _readPosition = _bodyPosition + _bodyLength;

      if (_readPosition == _readBuffer->length()) {
        _sinceCompactification = 0;
        _readPosition = 0;
        _readBuffer->reset();
      }
    }

    _bodyPosition = 0;
    _bodyLength = 0;
  }

  _newRequest = true;
  _readRequestBody = false;
  */
}

// GeneralResponse::ResponseCode VppCommTask::authenticateRequest() {
//   auto context = (_request == nullptr) ? nullptr :
//   _request->requestContext();
//
//   if (context == nullptr && _request != nullptr) {
//     bool res =
//         GeneralServerFeature::HANDLER_FACTORY->setRequestContext(_request);
//
//     if (!res) {
//       return GeneralResponse::ResponseCode::NOT_FOUND;
//     }
//
//     context = _request->requestContext();
//   }
//
//   if (context == nullptr) {
//     return GeneralResponse::ResponseCode::SERVER_ERROR;
//   }
//
//   return context->authenticate();
// }

// convert internal GeneralRequest to VppRequest
VppRequest* VppCommTask::requestAsVpp() {
  VppRequest* request = dynamic_cast<VppRequest*>(_request);
  if (request == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
  }
  return request;
};
