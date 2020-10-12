////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include <stddef.h>
#include <chrono>
#include <cstdint>
#include <exception>
#include <thread>
#include <utility>

#include <boost/algorithm/string/predicate.hpp>

#include <velocypack/Builder.h>
#include <velocypack/Parser.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

#include "SimpleHttpClient.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "ApplicationFeatures/CommunicationFeaturePhase.h"
#include "Basics/ScopeGuard.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Basics/system-compiler.h"
#include "Basics/system-functions.h"
#include "Endpoint/Endpoint.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "Rest/GeneralResponse.h"
#include "SimpleHttpClient/GeneralClientConnection.h"
#include "SimpleHttpClient/SimpleHttpResult.h"

using namespace arangodb;
using namespace arangodb::basics;

namespace arangodb {
namespace httpclient {

/// @brief empty map, used for headers
std::unordered_map<std::string, std::string> const SimpleHttpClient::NO_HEADERS{};

/// @brief default value for max packet size
size_t SimpleHttpClientParams::MaxPacketSize = 512 * 1024 * 1024;

SimpleHttpClient::SimpleHttpClient(GeneralClientConnection* connection,
                                   SimpleHttpClientParams const& params)
    : _connection(connection),
      _deleteConnectionOnDestruction(false),
      _params(params),
      _writeBuffer(false),
      _readBuffer(false),
      _readBufferOffset(0),
      _state(IN_CONNECT),
      _written(0),
      _errorMessage(""),
      _nextChunkedSize(0),
      _method(rest::RequestType::GET),
      _result(nullptr),
      _aborted(false) {
  TRI_ASSERT(connection != nullptr);

  if (_connection->isConnected()) {
    _state = FINISHED;
  }
}

SimpleHttpClient::SimpleHttpClient(std::unique_ptr<GeneralClientConnection>& connection,
                                   SimpleHttpClientParams const& params)
    : SimpleHttpClient(connection.get(), params) {
  _deleteConnectionOnDestruction = true;
  connection.release();
}

SimpleHttpClient::~SimpleHttpClient() {
  // connection may have been invalidated by other objects
  if (_connection != nullptr) {
    if (!_params._keepConnectionOnDestruction || !_connection->isConnected()) {
      _connection->disconnect();
    }

    if (_deleteConnectionOnDestruction) {
      delete _connection;
    }
  }
}

// -----------------------------------------------------------------------------
// public methods
// -----------------------------------------------------------------------------

void SimpleHttpClient::setAborted(bool value) noexcept {
  _aborted.store(value, std::memory_order_release);
  setInterrupted(value);
}

void SimpleHttpClient::setInterrupted(bool value) {
  if (_connection != nullptr) {
    _connection->setInterrupted(value);
  }
}

bool SimpleHttpClient::isConnected() { return _connection->isConnected(); }

void SimpleHttpClient::disconnect() { _connection->disconnect(); }

std::string SimpleHttpClient::getEndpointSpecification() const {
  return _connection == nullptr ? "unknown" : _connection->getEndpointSpecification();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief close connection
////////////////////////////////////////////////////////////////////////////////

void SimpleHttpClient::close() {
  // ensure connection has not yet been invalidated
  TRI_ASSERT(_connection != nullptr);

  _connection->disconnect();
  _state = IN_CONNECT;

  clearReadBuffer();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief send out a request, creating a new HttpResult object
/// this version does not allow specifying custom headers
/// if the request fails because of connection problems, the request will be
/// retried until it either succeeds (at least no connection problem) or there
/// have been _maxRetries retries
////////////////////////////////////////////////////////////////////////////////

SimpleHttpResult* SimpleHttpClient::retryRequest(rest::RequestType method,
                                                 std::string const& location,
                                                 char const* body, size_t bodyLength) {
  return retryRequest(method, location, body, bodyLength, NO_HEADERS);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief send out a request, creating a new HttpResult object
/// this version does not allow specifying custom headers
/// if the request fails because of connection problems, the request will be
/// retried until it either succeeds (at least no connection problem) or there
/// have been _maxRetries retries
////////////////////////////////////////////////////////////////////////////////

SimpleHttpResult* SimpleHttpClient::retryRequest(
    rest::RequestType method, std::string const& location, char const* body,
    size_t bodyLength, std::unordered_map<std::string, std::string> const& headers) {
  SimpleHttpResult* result = nullptr;
  size_t tries = 0;

  while (true) {
    TRI_ASSERT(result == nullptr);

    result = doRequest(method, location, body, bodyLength, headers);

    if (result != nullptr && result->isComplete()) {
      break;
    }

    delete result;
    result = nullptr;

    if (tries++ >= _params._maxRetries) {
      LOG_TOPIC("de0be", WARN, arangodb::Logger::HTTPCLIENT)
          << "" << _params._retryMessage << " - no retries left";
      break;
    }

    auto& server = _connection->server();
    if (server.isStopping()) {
      // abort this client, will also lead to exiting this loop next
      setAborted(true);
    }

    if (isAborted()) {
      break;
    }

    if (!_params._retryMessage.empty() && (_params._maxRetries - tries) > 0) {
      LOG_TOPIC("2b48f", WARN, arangodb::Logger::HTTPCLIENT)
          << "" << _params._retryMessage
          << " - retries left: " << (_params._maxRetries - tries);
    }

    // 1 microsecond == 10^-6 seconds
    std::this_thread::sleep_for(std::chrono::microseconds(_params._retryWaitTime));
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief send out a request, creating a new HttpResult object
/// this version does not allow specifying custom headers
////////////////////////////////////////////////////////////////////////////////

SimpleHttpResult* SimpleHttpClient::request(rest::RequestType method,
                                            std::string const& location,
                                            char const* body, size_t bodyLength) {
  return doRequest(method, location, body, bodyLength, NO_HEADERS);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief send out a request, creating a new HttpResult object
/// this version allows specifying custom headers
////////////////////////////////////////////////////////////////////////////////

SimpleHttpResult* SimpleHttpClient::request(
    rest::RequestType method, std::string const& location, char const* body,
    size_t bodyLength, std::unordered_map<std::string, std::string> const& headers) {
  return doRequest(method, location, body, bodyLength, headers);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief send out a request, worker function
////////////////////////////////////////////////////////////////////////////////

SimpleHttpResult* SimpleHttpClient::doRequest(
    rest::RequestType method, std::string const& location, char const* body,
    size_t bodyLength, std::unordered_map<std::string, std::string> const& headers) {
  // ensure connection has not yet been invalidated
  TRI_ASSERT(_connection != nullptr);
  if (isAborted()) {
    return nullptr;
  }

  // ensure that result is empty
  TRI_ASSERT(_result == nullptr);

  // create a new result
  _result = new SimpleHttpResult();
  auto resultGuard = scopeGuard([this] {
    delete _result;
    _result = nullptr;
  });

  // reset error message
  _errorMessage = "";

  auto& server = _connection->server();
  auto& comm = server.getFeature<application_features::CommunicationFeaturePhase>();

  // set body
  setRequest(method, rewriteLocation(location), body, bodyLength, headers);

  // ensure state
  TRI_ASSERT(_state == IN_CONNECT || _state == IN_WRITE);

  // respect timeout
  double endTime = TRI_microtime() + _params._requestTimeout;
  double remainingTime = _params._requestTimeout;

  bool haveSentRequest = false;

  while (_state < FINISHED && remainingTime > 0.0) {
    // Note that this loop can either be left by timeout or because
    // a connect did not work (which sets the _state to DEAD). In all
    // other error conditions we call close() which resets the state
    // to IN_CONNECT and tries a reconnect. This is important because
    // it is always possible that we are called with a connection that
    // has already been closed by the other side. This leads to the
    // strange effect that the write (if it is small enough) proceeds
    // but the following read runs into an error. In that case we try
    // to reconnect one and then give up if this does not work.
    switch (_state) {
      case (IN_CONNECT): {
        handleConnect();
        // If this goes wrong, _state is set to DEAD
        break;
      }

      case (IN_WRITE): {
        size_t bytesWritten = 0;

        TRI_ASSERT(_writeBuffer.length() >= _written);
        TRI_set_errno(TRI_ERROR_NO_ERROR);

        bool res =
            _connection->handleWrite(remainingTime,
                                     static_cast<void const*>(_writeBuffer.c_str() + _written),
                                     _writeBuffer.length() - _written, &bytesWritten);

        if (!res) {
          setErrorMessage("Error writing to '" + _connection->getEndpoint()->specification() +
                          "' '" + _connection->getErrorDetails() + "'");
          this->close();  // this sets _state to IN_CONNECT for a retry
        } else {
          _written += bytesWritten;

          if (_written == _writeBuffer.length()) {
            _state = IN_READ_HEADER;
            haveSentRequest = true;
          }
        }

        break;
      }

      case (IN_READ_HEADER):
      case (IN_READ_BODY):
      case (IN_READ_CHUNKED_HEADER):
      case (IN_READ_CHUNKED_BODY): {
        TRI_set_errno(TRI_ERROR_NO_ERROR);

        // we need to notice if the other side has closed the connection:
        bool connectionClosed;

        bool res = _connection->handleRead(remainingTime, _readBuffer, connectionClosed);

        // If there was an error, then we are doomed:
        if (!res) {
          setErrorMessage("Error reading from: '" +
                          _connection->getEndpoint()->specification() + "' '" +
                          _connection->getErrorDetails() + "'");

          if (_connection->isInterrupted()) {
            this->close();
            setErrorMessage("Command locally aborted");
            return nullptr;
          }
          this->close();  // this sets the state to IN_CONNECT for a retry
          LOG_TOPIC("e5154", DEBUG, arangodb::Logger::HTTPCLIENT) << _errorMessage;

          std::this_thread::sleep_for(std::chrono::milliseconds(5));
          break;
        }

        // no error

        if (connectionClosed) {
          // write might have succeeded even if the server has closed
          // the connection, this will then show up here with us being
          // in state IN_READ_HEADER but nothing read.
          if (_state == IN_READ_HEADER && 0 == _readBuffer.length()) {
            this->close();
            _state = DEAD;
            setErrorMessage("Connection closed by remote");
            break;
          }

          if (_state == IN_READ_HEADER) {
            processHeader();
          }

          if (_state == IN_READ_BODY) {
            if (!_result->hasContentLength()) {
              // If we are reading the body and no content length was
              // found in the header, then we must read until no more
              // progress is made (but without an error), this then means
              // that the server has closed the connection and we must
              // process the body one more time:
              _result->setContentLength(_readBuffer.length() - _readBufferOffset);
            }
            processBody();
          }

          if (_state != FINISHED) {
            // If the body was not fully found we give up:
            this->close();
            _state = DEAD;
            setErrorMessage("Got unexpected response from remote");
          }

          break;
        }

        // the connection is still alive:
        switch (_state) {
          case (IN_READ_HEADER):
            processHeader();
            break;

          case (IN_READ_BODY):
            processBody();
            break;

          case (IN_READ_CHUNKED_HEADER):
            processChunkedHeader();
            break;

          case (IN_READ_CHUNKED_BODY):
            processChunkedBody();
            break;

          default:
            break;
        }

        break;
      }

      default:
        break;
    }

    if (!comm.getCommAllowed()) {
      setErrorMessage("Command locally aborted");
      return nullptr;
    }

    remainingTime = endTime - TRI_microtime();
    if (isAborted()) {
      setErrorMessage("Client request aborted");
      break;
    }
  }

  if (_state < FINISHED && _errorMessage.empty()) {
    setErrorMessage("Request timeout reached");
    _result->setHttpReturnCode(TRI_ERROR_HTTP_GATEWAY_TIMEOUT);
  }

  // set result type in getResult()
  SimpleHttpResult* result = getResult(haveSentRequest);
  _result = nullptr;
  resultGuard.cancel();  // doesn't matter but do it anyway

  return result;
}

// -----------------------------------------------------------------------------
// private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief initialize the connection
////////////////////////////////////////////////////////////////////////////////

void SimpleHttpClient::handleConnect() {
  // ensure connection has not yet been invalidated
  TRI_ASSERT(_connection != nullptr);

  if (!_connection->connect()) {
    setErrorMessage("Could not connect to '" + _connection->getEndpoint()->specification() +
                    "' '" + _connection->getErrorDetails() + "'");
    _state = DEAD;
  } else {
    // can write now
    _state = IN_WRITE;
    _written = 0;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief clearReadBuffer, clears the read buffer as well as the result
////////////////////////////////////////////////////////////////////////////////

void SimpleHttpClient::clearReadBuffer() {
  _readBuffer.clear();
  _readBufferOffset = 0;

  if (_result) {
    _result->clear();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the result
////////////////////////////////////////////////////////////////////////////////

SimpleHttpResult* SimpleHttpClient::getResult(bool haveSentRequest) {
  _result->setHaveSentRequestFully(haveSentRequest);
  switch (_state) {
    case IN_WRITE:
      _result->setResultType(SimpleHttpResult::WRITE_ERROR);
      break;

    case IN_READ_HEADER:
    case IN_READ_BODY:
    case IN_READ_CHUNKED_HEADER:
    case IN_READ_CHUNKED_BODY:
      _result->setResultType(SimpleHttpResult::READ_ERROR);
      break;

    case FINISHED:
      _result->setResultType(SimpleHttpResult::COMPLETE);
      break;

    case IN_CONNECT:
    default: {
      _result->setResultType(SimpleHttpResult::COULD_NOT_CONNECT);
      if (!haveErrorMessage()) {
        setErrorMessage("Could not connect");
      }
      break;
    }
  }

  if (haveErrorMessage() && _result->getHttpReturnMessage().empty()) {
    _result->setHttpReturnMessage(_errorMessage);
  }

  return _result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief prepare a request
////////////////////////////////////////////////////////////////////////////////

void SimpleHttpClient::setRequest(rest::RequestType method, std::string const& location,
                                  char const* body, size_t bodyLength,
                                  std::unordered_map<std::string, std::string> const& headers) {
  // clear read-buffer (no pipelining!)
  _readBufferOffset = 0;
  _readBuffer.reset();

  // set HTTP method
  _method = method;

  // now fill the write buffer
  _writeBuffer.clear();

  // append method
  GeneralRequest::appendMethod(method, &_writeBuffer);

  // append location
  std::string const* l = &location;

  std::string appended;
  if (location.empty() || location[0] != '/') {
    appended.reserve(1 + location.size());
    appended = '/' + location;
    l = &appended;
  }

  _writeBuffer.appendText(*l);

  // append protocol
  _writeBuffer.appendText(TRI_CHAR_LENGTH_PAIR(" HTTP/1.1\r\n"));

  // append hostname
  std::string hostname = _connection->getEndpoint()->host();

  LOG_TOPIC("908b8", DEBUG, Logger::HTTPCLIENT)
      << "request to " << hostname << ": "
      << GeneralRequest::translateMethod(method) << ' ' << *l;

  _writeBuffer.appendText(TRI_CHAR_LENGTH_PAIR("Host: "));
  _writeBuffer.appendText(hostname);
  _writeBuffer.appendText(TRI_CHAR_LENGTH_PAIR("\r\n"));

  if (_params._keepAlive) {
    _writeBuffer.appendText(TRI_CHAR_LENGTH_PAIR("Connection: Keep-Alive\r\n"));
  } else {
    _writeBuffer.appendText(TRI_CHAR_LENGTH_PAIR("Connection: Close\r\n"));
  }

  if (_params._exposeArangoDB) {
    _writeBuffer.appendText(TRI_CHAR_LENGTH_PAIR("User-Agent: ArangoDB\r\n"));
  }

  // do not automatically advertise deflate support
  if (_params._supportDeflate) {
    _writeBuffer.appendText(
        TRI_CHAR_LENGTH_PAIR("Accept-Encoding: deflate\r\n"));
  }

  // do basic authorization
  std::vector<std::pair<size_t, size_t>> exclusions;
  size_t pos = 0;
  if (!_params._jwt.empty()) {
    _writeBuffer.appendText(TRI_CHAR_LENGTH_PAIR("Authorization: bearer "));
    pos = _writeBuffer.size();
    _writeBuffer.appendText(_params._jwt);
    exclusions.emplace_back(pos, _writeBuffer.size());
    _writeBuffer.appendText(TRI_CHAR_LENGTH_PAIR("\r\n"));
  } else if (!_params._basicAuth.empty()) {
    _writeBuffer.appendText(TRI_CHAR_LENGTH_PAIR("Authorization: Basic "));
    pos = _writeBuffer.size();
    _writeBuffer.appendText(_params._basicAuth);
    exclusions.emplace_back(pos, _writeBuffer.size());
    _writeBuffer.appendText(TRI_CHAR_LENGTH_PAIR("\r\n"));
  }

  for (auto const& header : headers) {
    if (boost::iequals(StaticStrings::ContentLength, header.first)) {
      continue; // skip content-length header
    }
    _writeBuffer.appendText(header.first);
    _writeBuffer.appendText(TRI_CHAR_LENGTH_PAIR(": "));
    if (boost::iequals(StaticStrings::Authorization, header.first)) {
      pos = _writeBuffer.size();
      _writeBuffer.appendText(header.second);
      exclusions.emplace_back(pos, _writeBuffer.size());
    } else {
      _writeBuffer.appendText(header.second);
    }
    _writeBuffer.appendText(TRI_CHAR_LENGTH_PAIR("\r\n"));
  }

  if (method != rest::RequestType::GET) {
    _writeBuffer.appendText(TRI_CHAR_LENGTH_PAIR("Content-Length: "));
    _writeBuffer.appendInteger(static_cast<uint64_t>(bodyLength));
    _writeBuffer.appendText(TRI_CHAR_LENGTH_PAIR("\r\n\r\n"));
  } else {
    _writeBuffer.appendText(TRI_CHAR_LENGTH_PAIR("\r\n"));
  }

  if (body != nullptr) {
    _writeBuffer.appendText(body, bodyLength);
  }

  _writeBuffer.ensureNullTerminated();

  if (exclusions.empty()) {
    LOG_TOPIC("12c4c", TRACE, arangodb::Logger::HTTPCLIENT)
        << "request: " << _writeBuffer;
  } else {
    pos = 0;
    for (size_t i = 0; i < exclusions.size(); ++i) {
      LOG_TOPIC("12c4b", TRACE, arangodb::Logger::HTTPCLIENT)
          << "request: "
          << std::string_view(_writeBuffer.data() + pos, exclusions[i].first - pos)
          << "SENSITIVE_DETAILS_HIDDEN";
      pos = exclusions[i].second;
    }
    LOG_TOPIC("12c4e", TRACE, arangodb::Logger::HTTPCLIENT)
        << "request: " << std::string_view(_writeBuffer.data() + pos, _writeBuffer.size() - pos);
  }

  if (_state == DEAD) {
    _connection->resetNumConnectRetries();
  }

  // close connection to reset all read and write buffers
  if (_state != FINISHED) {
    this->close();
  }

  // we are connected, start with writing
  if (_connection->isConnected()) {
    _state = IN_WRITE;
    _written = 0;
  }

  // connect to server
  else {
    _state = IN_CONNECT;
  }

  TRI_ASSERT(_state == IN_CONNECT || _state == IN_WRITE);
}

// -----------------------------------------------------------------------------
// private methods
// -----------------------------------------------------------------------------

void SimpleHttpClient::processHeader() {
  TRI_ASSERT(_readBufferOffset <= _readBuffer.length());
  size_t remain = _readBuffer.length() - _readBufferOffset;
  char const* ptr = _readBuffer.c_str() + _readBufferOffset;
  char const* pos = static_cast<char const*>(memchr(ptr, '\n', remain));

  // We enforce the following invariants:
  //   ptr = _readBuffer.c_str() + _readBufferOffset
  //   _readBuffer.length() >= _readBufferOffset
  //   remain = _readBuffer.length() - _readBufferOffset
  while (pos) {
    TRI_ASSERT(_readBufferOffset <= _readBuffer.length());
    TRI_ASSERT(ptr == _readBuffer.c_str() + _readBufferOffset);
    TRI_ASSERT(remain == _readBuffer.length() - _readBufferOffset);

    if (pos > ptr && *(pos - 1) == '\r') {
      // adjust eol position
      --pos;
    }

    // end of header found
    if (*ptr == '\r' || *ptr == '\n' || *ptr == '\0') {
      size_t len = pos - ptr;
      _readBufferOffset += len + 1;
      TRI_ASSERT(_readBufferOffset <= _readBuffer.length());

      ptr += len + 1;
      remain -= len + 1;

      if (*pos == '\r') {
        // adjust offset if line ended with \r\n
        ++_readBufferOffset;
        TRI_ASSERT(_readBufferOffset <= _readBuffer.length());

        ptr++;
        remain--;
      }

      // handle chunks
      if (_result->isChunked()) {
        _state = IN_READ_CHUNKED_HEADER;
        processChunkedHeader();
        return;
      }

      // no content-length header in response
      else if (!_result->hasContentLength()) {
        _state = IN_READ_BODY;
        processBody();
        return;
      }

      // no body
      else if (_result->hasContentLength() && _result->getContentLength() == 0) {
        _result->setResultType(SimpleHttpResult::COMPLETE);
        _state = FINISHED;

        if (!_params._keepAlive) {
          _connection->disconnect();
        }
        return;
      }

      // found content-length header in response
      else if (_result->hasContentLength() && _result->getContentLength() > 0) {
        if (_result->getContentLength() > _params._maxPacketSize) {
          std::string errorMessage(
              "ignoring HTTP response with 'Content-Length' bigger than max "
              "packet size (");
          errorMessage += std::to_string(_result->getContentLength()) + " > " +
                          std::to_string(_params._maxPacketSize) + ")";
          setErrorMessage(errorMessage, true);

          // reset connection
          this->close();
          _state = DEAD;

          return;
        }

        _state = IN_READ_BODY;
        processBody();
        return;
      } else {
        TRI_ASSERT(false);
      }

      break;
    }

    // we have found more header fields
    else {
      size_t len = pos - ptr;
      _result->addHeaderField(ptr, len);

      if (*pos == '\r') {
        // adjust length if line ended with \r\n
        // (header was already added so no harm is done)
        ++len;
      }

      // account for \n
      ptr += len + 1;
      _readBufferOffset += len + 1;
      TRI_ASSERT(_readBufferOffset <= _readBuffer.length());

      remain -= (len + 1);

      TRI_ASSERT(_readBufferOffset <= _readBuffer.length());
      TRI_ASSERT(ptr == _readBuffer.c_str() + _readBufferOffset);
      TRI_ASSERT(remain == _readBuffer.length() - _readBufferOffset);
      pos = static_cast<char const*>(memchr(ptr, '\n', remain));
    }
  }
}

void SimpleHttpClient::processBody() {
  // HEAD requests may be responded to without a body...
  if (_method == rest::RequestType::HEAD) {
    _result->setResultType(SimpleHttpResult::COMPLETE);
    _state = FINISHED;

    if (!_params._keepAlive) {
      _connection->disconnect();
    }

    return;
  }

  // we need to wait for a close, if content length is unknown
  if (!_result->hasContentLength()) {
    return;
  }

  // we need to wait for more data
  if (_readBuffer.length() - _readBufferOffset < _result->getContentLength()) {
    return;
  }

  // body is compressed using deflate. inflate it
  if (_result->isDeflated()) {
    _readBuffer.inflate(_result->getBody(), 16384, _readBufferOffset);
  }

  // body is not compressed
  else {
    // Note that if we are here, then
    // _result->getContentLength() <= _readBuffer.length()-_readBufferOffset
    _result->getBody().appendText(_readBuffer.c_str() + _readBufferOffset,
                                  _result->getContentLength());
    _result->getBody().ensureNullTerminated();
  }

  _readBufferOffset += _result->getContentLength();
  TRI_ASSERT(_readBufferOffset <= _readBuffer.length());

  _result->setResultType(SimpleHttpResult::COMPLETE);
  _state = FINISHED;

  if (!_params._keepAlive) {
    _connection->disconnect();
  }
}

void SimpleHttpClient::processChunkedHeader() {
  size_t remain = _readBuffer.length() - _readBufferOffset;
  char const* ptr = _readBuffer.c_str() + _readBufferOffset;
  char const* pos = static_cast<char const*>(memchr(ptr, '\n', remain));

  // not yet finished, newline is missing
  if (pos == nullptr) {
    return;
  }

  // adjust eol position
  if (pos > ptr && *(pos - 1) == '\r') {
    --pos;
  }

  size_t len = pos - (_readBuffer.c_str() + _readBufferOffset);
  std::string line(_readBuffer.c_str() + _readBufferOffset, len);
  StringUtils::trimInPlace(line);

  _readBufferOffset += (len + 1);

  // adjust offset if line ended with \r\n
  if (*pos == '\r') {
    ++_readBufferOffset;
    TRI_ASSERT(_readBufferOffset <= _readBuffer.length());
  }

  // empty lines are an error
  if (line[0] == '\r' || line.empty()) {
    setErrorMessage("found invalid Content-Length", true);
    // reset connection
    this->close();
    _state = DEAD;

    return;
  }

  uint32_t contentLength;

  try {
    contentLength = static_cast<uint32_t>(std::stol(line, nullptr, 16));
  } catch (...) {
    setErrorMessage("found invalid Content-Length", true);
    // reset connection
    this->close();
    _state = DEAD;

    return;
  }

  // failed: too many bytes
  if (contentLength > _params._maxPacketSize) {
    std::string errorMessage(
        "ignoring HTTP response with 'Content-Length' bigger than max packet "
        "size (");
    errorMessage += std::to_string(contentLength) + " > " +
                    std::to_string(_params._maxPacketSize) + ")";
    setErrorMessage(errorMessage, true);
    // reset connection
    this->close();
    _state = DEAD;

    return;
  }

  _state = IN_READ_CHUNKED_BODY;
  _nextChunkedSize = contentLength;

  processChunkedBody();
}

void SimpleHttpClient::processChunkedBody() {
  // HEAD requests may be responded to without a body...
  if (_method == rest::RequestType::HEAD) {
    _result->setResultType(SimpleHttpResult::COMPLETE);
    _state = FINISHED;

    if (!_params._keepAlive) {
      _connection->disconnect();
    }

    return;
  }

  if (_readBuffer.length() - _readBufferOffset >= _nextChunkedSize + 2) {
    // last chunk length was 0, therefore we are finished
    if (_nextChunkedSize == 0) {
      _result->setResultType(SimpleHttpResult::COMPLETE);

      _state = FINISHED;

      if (!_params._keepAlive) {
        _connection->disconnect();
      }

      return;
    }

    if (_result->isDeflated()) {
      _readBuffer.inflate(_result->getBody(), 16384, _readBufferOffset);
      _result->getBody().ensureNullTerminated();
    } else {
      _result->getBody().appendText(_readBuffer.c_str() + _readBufferOffset,
                                    (size_t)_nextChunkedSize);
      _result->getBody().ensureNullTerminated();
    }

    _readBufferOffset += (size_t)_nextChunkedSize + 2;

    _state = IN_READ_CHUNKED_HEADER;
    processChunkedHeader();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extract an error message from a response
////////////////////////////////////////////////////////////////////////////////

std::string SimpleHttpClient::getHttpErrorMessage(SimpleHttpResult const* result,
                                                  int* errorCode) {
  if (errorCode != nullptr) {
    *errorCode = TRI_ERROR_NO_ERROR;
  }

  arangodb::basics::StringBuffer const& body = result->getBody();
  std::string details;

  try {
    std::shared_ptr<VPackBuilder> builder =
        VPackParser::fromJson(body.c_str(), body.length());

    VPackSlice slice = builder->slice();
    if (slice.isObject()) {
      VPackSlice msg = slice.get(StaticStrings::ErrorMessage);
      int errorNum = slice.get(StaticStrings::ErrorNum).getNumericValue<int>();

      if (msg.isString() && msg.getStringLength() > 0 && errorNum > 0) {
        if (errorCode != nullptr) {
          *errorCode = errorNum;
        }
        details = ": ArangoError " + std::to_string(errorNum) + ": " + msg.copyString();
      }
    }
  } catch (...) {
    // don't rethrow here. we'll respond with an error message anyway
  }

  return "got error from server: HTTP " + std::to_string(result->getHttpReturnCode()) +
         " (" + result->getHttpReturnMessage() + ")" + details;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief fetch the version from the server
////////////////////////////////////////////////////////////////////////////////

std::string SimpleHttpClient::getServerVersion(int* errorCode) {
  if (errorCode != nullptr) {
    *errorCode = TRI_ERROR_INTERNAL;
  }

  std::unique_ptr<SimpleHttpResult> response(
      request(rest::RequestType::GET, "/_api/version", nullptr, 0));

  if (response == nullptr || !response->isComplete()) {
    return "";
  }

  if (response->getHttpReturnCode() == static_cast<int>(rest::ResponseCode::OK)) {
    // default value
    std::string version = "arango";

    arangodb::basics::StringBuffer const& body = response->getBody();
    try {
      std::shared_ptr<VPackBuilder> builder =
          VPackParser::fromJson(body.c_str(), body.length());

      VPackSlice slice = builder->slice();
      if (slice.isObject()) {
        VPackSlice server = slice.get("server");
        if (server.isString() && server.copyString() == "arango") {
          // "server" value is a string and its content is "arango"
          VPackSlice v = slice.get("version");
          if (v.isString()) {
            version = v.copyString();
          }
        }
      }

      if (errorCode != nullptr) {
        *errorCode = TRI_ERROR_NO_ERROR;
      }
      return version;
    } catch (std::exception const& ex) {
      setErrorMessage(ex.what(), false);
      return "";
    } catch (...) {
      setErrorMessage("Unable to parse server response", false);
      return "";
    }
  }

  if (response->wasHttpError()) {
    std::string msg = getHttpErrorMessage(response.get(), errorCode);
    setErrorMessage(msg, false);
  }
  _connection->disconnect();

  return "";
}
}  // namespace httpclient
}  // namespace arangodb
