////////////////////////////////////////////////////////////////////////////////
/// @brief simple http client
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

#include "SimpleHttpClient.h"
#include "Basics/JsonHelper.h"
#include "Basics/logging.h"
#include "Basics/StringUtils.h"
#include "GeneralClientConnection.h"
#include "SimpleHttpClient/SimpleHttpResult.h"

using namespace triagens::basics;
using namespace triagens::rest;
using namespace std;

namespace triagens {
  namespace httpclient {

////////////////////////////////////////////////////////////////////////////////
/// @brief empty map, used for headers
////////////////////////////////////////////////////////////////////////////////

    std::map<std::string, std::string> const SimpleHttpClient::NoHeaders;

// -----------------------------------------------------------------------------
// constructors and destructors
// -----------------------------------------------------------------------------

    SimpleHttpClient::SimpleHttpClient (GeneralClientConnection* connection,
                                        double requestTimeout,
                                        bool warn) 
      : _connection(connection),
        _writeBuffer(TRI_UNKNOWN_MEM_ZONE),
        _readBuffer(TRI_UNKNOWN_MEM_ZONE),
        _readBufferOffset(0),
        _requestTimeout(requestTimeout),
        _state(IN_CONNECT),
        _written(0),
        _errorMessage(""),
        _locationRewriter({nullptr, nullptr}),
        _nextChunkedSize(0),
        _result(nullptr),
        _maxPacketSize(128 * 1024 * 1024),
        _maxRetries(3),
        _retryWaitTime(1 * 1000 * 1000),
        _retryMessage(),
        _keepConnectionOnDestruction(false),
        _warn(warn),
        _keepAlive(true),
        _exposeArangoDB(true),
        _supportDeflate(true) {

      TRI_ASSERT(connection != nullptr);

      if (_connection->isConnected()) {
        _state = FINISHED;
      }
    }

    SimpleHttpClient::~SimpleHttpClient () {
      // connection may have been invalidated by other objects
      if (_connection != nullptr) {
        if (! _keepConnectionOnDestruction || ! _connection->isConnected()) {
          _connection->disconnect();
        }
      }
    }

// -----------------------------------------------------------------------------
// public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief close connection
////////////////////////////////////////////////////////////////////////////////

    void SimpleHttpClient::close () {
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

    SimpleHttpResult* SimpleHttpClient::retryRequest (rest::HttpRequest::HttpRequestType method,
                                                      std::string const& location,
                                                      char const* body,
                                                      size_t bodyLength) {
      return retryRequest(method, location, body, bodyLength, NoHeaders);
    }

////////////////////////////////////////////////////////////////////////////////
/// @brief send out a request, creating a new HttpResult object
/// this version does not allow specifying custom headers
/// if the request fails because of connection problems, the request will be
/// retried until it either succeeds (at least no connection problem) or there
/// have been _maxRetries retries
////////////////////////////////////////////////////////////////////////////////

    SimpleHttpResult* SimpleHttpClient::retryRequest (rest::HttpRequest::HttpRequestType method,
                                                      std::string const& location,
                                                      char const* body,
                                                      size_t bodyLength,
                                                      std::map<std::string, std::string> const& headers) {
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

        if (tries++ >= _maxRetries) {
          break;
        }
        
        if (! _retryMessage.empty()) {
          LOG_WARNING("%s - retries left: %d", _retryMessage.c_str(), (int) (_maxRetries - tries));
        }

#ifdef _WIN32
        usleep((unsigned long) _retryWaitTime);
#else
        usleep((useconds_t) _retryWaitTime);
#endif
      }

      return result;
    }

////////////////////////////////////////////////////////////////////////////////
/// @brief send out a request, creating a new HttpResult object
/// this version does not allow specifying custom headers
////////////////////////////////////////////////////////////////////////////////

    SimpleHttpResult* SimpleHttpClient::request (rest::HttpRequest::HttpRequestType method,
                                                 std::string const& location,
                                                 char const* body,
                                                 size_t bodyLength) {
      return doRequest(method, location, body, bodyLength, NoHeaders);
    }

////////////////////////////////////////////////////////////////////////////////
/// @brief send out a request, creating a new HttpResult object
/// this version allows specifying custom headers
////////////////////////////////////////////////////////////////////////////////

    SimpleHttpResult* SimpleHttpClient::request (rest::HttpRequest::HttpRequestType method,
                                                 std::string const& location,
                                                 char const* body,
                                                 size_t bodyLength,
                                                 std::map<std::string, std::string> const& headers) {
      return doRequest(method, location, body, bodyLength, headers);
    }

////////////////////////////////////////////////////////////////////////////////
/// @brief send out a request, worker function
////////////////////////////////////////////////////////////////////////////////

    SimpleHttpResult* SimpleHttpClient::doRequest (rest::HttpRequest::HttpRequestType method,
                                                   std::string const& location,
                                                   char const* body,
                                                   size_t bodyLength,
                                                   std::map<std::string, std::string> const& headers) {
      
      // ensure connection has not yet been invalidated
      TRI_ASSERT(_connection != nullptr);

      // ensure that result is empty
      TRI_ASSERT(_result == nullptr);

      // create a new result
      _result = new SimpleHttpResult;

      // reset error message
      _errorMessage = "";

      // set body
      setRequest(method, rewriteLocation(location), body, bodyLength, headers);

      // ensure state
      TRI_ASSERT(_state == IN_CONNECT || _state == IN_WRITE);

      // respect timeout
      double endTime = TRI_microtime() + _requestTimeout;
      double remainingTime = _requestTimeout;

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

            bool res = _connection->handleWrite(
              remainingTime, 
              static_cast<void const*>(_writeBuffer.c_str() + _written),
              _writeBuffer.length() - _written,
              &bytesWritten);

            if (! res) {
              setErrorMessage("Error writing to '" +
                              _connection->getEndpoint()->getSpecification() +
                              "' '" +
                              _connection->getErrorDetails() +
                              "'");
              this->close(); // this sets _state to IN_CONNECT for a retry
            }
            else {
              _written += bytesWritten;

              if (_written == _writeBuffer.length())  {
                _state = IN_READ_HEADER;
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

            bool res = _connection->handleRead(remainingTime,
                                               _readBuffer,
                                               connectionClosed);


            // If there was an error, then we are doomed:
            if (! res) {
              setErrorMessage("Error reading from: '" +
                              _connection->getEndpoint()->getSpecification() +
                              "' '" +
                              _connection->getErrorDetails() +
                              "'");

              if (_connection->isInterrupted()) {
                this->close();
                delete _result;
                _result = nullptr;
                setErrorMessage("Command locally aborted");
                return nullptr;
              }
              this->close(); // this sets the state to IN_CONNECT for a retry
              break;
            }

            if (connectionClosed) {
              // write might have succeeded even if the server has closed 
              // the connection, this will then show up here with us being
              // in state IN_READ_HEADER but nothing read.
              if (_state == IN_READ_HEADER && 0 == _readBuffer.length()) {
                this->close(); // sets _state to IN_CONNECT again for a retry
                continue;
              }

              else if (_state == IN_READ_BODY && ! _result->hasContentLength()) {
                // If we are reading the body and no content length was
                // found in the header, then we must read until no more
                // progress is made (but without an error), this then means
                // that the server has closed the connection and we must
                // process the body one more time:
                _result->setContentLength(_readBuffer.length() - _readBufferOffset);
                processBody();

                if (_state != FINISHED) {
                  // If the body was not fully found we give up:
                  this->close(); // this sets the state IN_CONNECT to retry
                }

                break;
              }

              else {
                // In all other cases of closed connection, we are doomed:
                this->close(); // this sets the state to IN_CONNECT retry
                break;
              }
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

        remainingTime = endTime - TRI_microtime();
      }

      if (_state < FINISHED && _errorMessage.empty()) {
        setErrorMessage("Request timeout reached");
      }

      // set result type in getResult()
      SimpleHttpResult* result = getResult();

      _result = nullptr;

      return result;
    }

// -----------------------------------------------------------------------------
    // private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief initialize the connection
////////////////////////////////////////////////////////////////////////////////

      void SimpleHttpClient::handleConnect () {
        // ensure connection has not yet been invalidated
        TRI_ASSERT(_connection != nullptr);

        if (! _connection->connect()) {
          setErrorMessage("Could not connect to '" +
                          _connection->getEndpoint()->getSpecification() +
                          "' '" +
                          _connection->getErrorDetails() +
                          "'");
          _state = DEAD;
        }
        else {
          // can write now
          _state = IN_WRITE;
          _written = 0;
        }
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief clearReadBuffer, clears the read buffer as well as the result
////////////////////////////////////////////////////////////////////////////////

    void SimpleHttpClient::clearReadBuffer () {
      _readBuffer.clear();
      _readBufferOffset = 0;

      if (_result) {
        _result->clear();
      }
    }

////////////////////////////////////////////////////////////////////////////////
/// @brief sets username and password
////////////////////////////////////////////////////////////////////////////////

    void SimpleHttpClient::setUserNamePassword (const string& prefix,
                                                const string& username,
                                                const string& password) {

      string value = triagens::basics::StringUtils::encodeBase64(username + ":" + password);

      _pathToBasicAuth.push_back(make_pair(prefix, value));
    }

////////////////////////////////////////////////////////////////////////////////
/// @brief return the result
////////////////////////////////////////////////////////////////////////////////

    SimpleHttpResult* SimpleHttpClient::getResult () {
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

      if (haveErrorMessage() && (_result->getHttpReturnMessage().length() == 0)) {
        _result->setHttpReturnMessage(_errorMessage);
      }

      return _result;
    }

////////////////////////////////////////////////////////////////////////////////
/// @brief prepare a request
////////////////////////////////////////////////////////////////////////////////

    void SimpleHttpClient::setRequest (rest::HttpRequest::HttpRequestType method,
                                       std::string const& location,
                                       char const* body,
                                       size_t bodyLength,
                                       std::map<std::string, std::string> const& headers) {
      // clear read-buffer (no pipeling!)
      _readBufferOffset = 0;
      _readBuffer.reset();

      // set HTTP method
      _method = method;

      // now fill the write buffer
      _writeBuffer.clear();

      // append method
      HttpRequest::appendMethod(method, &_writeBuffer);

      // append location
      string l(location);
      if (location.empty() || location[0] != '/') {
        l = "/" + location;
      }

      _writeBuffer.appendText(l);

      // append protocol
      _writeBuffer.appendText(TRI_CHAR_LENGTH_PAIR(" HTTP/1.1\r\n"));

      // append hostname
      string&& hostname = _connection->getEndpoint()->getHost();

      _writeBuffer.appendText(TRI_CHAR_LENGTH_PAIR("Host: "));
      _writeBuffer.appendText(hostname);
      _writeBuffer.appendText(TRI_CHAR_LENGTH_PAIR("\r\n"));

      if (_keepAlive) {
        _writeBuffer.appendText(TRI_CHAR_LENGTH_PAIR("Connection: Keep-Alive\r\n"));
      }
      else {
        _writeBuffer.appendText(TRI_CHAR_LENGTH_PAIR("Connection: Close\r\n"));
      }

      if (_exposeArangoDB) {
        _writeBuffer.appendText(TRI_CHAR_LENGTH_PAIR("User-Agent: ArangoDB\r\n"));
      }

      // do not automatically advertise deflate support
      if (_supportDeflate) {
        _writeBuffer.appendText(TRI_CHAR_LENGTH_PAIR("Accept-Encoding: deflate\r\n"));
      }

      // do basic authorization
      if (! _pathToBasicAuth.empty()) {
        string foundPrefix;
        string foundValue;
        std::vector<std::pair<std::string, std::string>>::iterator i = _pathToBasicAuth.begin();

        for (; i != _pathToBasicAuth.end(); ++i) {
          string& f = i->first;

          if (l.find(f) == 0) {
            // f is prefix of l
            if (f.length() > foundPrefix.length()) {
              foundPrefix = f;
              foundValue = i->second;
            }
          }
        }

        if (! foundValue.empty()) {
          _writeBuffer.appendText(TRI_CHAR_LENGTH_PAIR("Authorization: Basic "));
          _writeBuffer.appendText(foundValue);
          _writeBuffer.appendText(TRI_CHAR_LENGTH_PAIR("\r\n"));
        }
      }

      for (auto const& header : headers) {
        _writeBuffer.appendText(header.first);
        _writeBuffer.appendText(TRI_CHAR_LENGTH_PAIR(": "));
        _writeBuffer.appendText(header.second);
        _writeBuffer.appendText(TRI_CHAR_LENGTH_PAIR("\r\n"));
      }

      if (method != HttpRequest::HTTP_REQUEST_GET) {
        _writeBuffer.appendText(TRI_CHAR_LENGTH_PAIR("Content-Length: "));
        _writeBuffer.appendInteger(bodyLength);
        _writeBuffer.appendText(TRI_CHAR_LENGTH_PAIR("\r\n\r\n"));
      }
      else {
        _writeBuffer.appendText(TRI_CHAR_LENGTH_PAIR("\r\n"));
      }

      if (body != nullptr) {
        _writeBuffer.appendText(body, bodyLength);
      }

      LOG_TRACE("Request: %s", _writeBuffer.c_str());

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

    void SimpleHttpClient::processHeader () {
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
        if (*ptr == '\r' || *ptr == '\0') {
          size_t len = pos - ptr;
          _readBufferOffset += len + 1;
          ptr += len + 1;
          remain -= len + 1;

          if (*pos == '\r') {
            // adjust offset if line ended with \r\n
            ++_readBufferOffset;
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
          else if (! _result->hasContentLength()) {
            _state = IN_READ_BODY;
            processBody();
            return;
          }

          // no body
          else if (_result->hasContentLength() && _result->getContentLength() == 0) {
            _result->setResultType(SimpleHttpResult::COMPLETE);
            _state = FINISHED;

            if (! _keepAlive) {
              _connection->disconnect();
            }
            return;
          }

          // found content-length header in response
          else if (_result->hasContentLength() && _result->getContentLength() > 0) {
            if (_result->getContentLength() > _maxPacketSize) {
              setErrorMessage("Content-Length > max packet size found", true);

              // reset connection
              this->close();
              _state = DEAD;

              return;
            }

            _state = IN_READ_BODY;
            processBody();
            return;
          }
          else {
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
        
          ptr += len + 1;
          _readBufferOffset += len + 1;
          remain -= (len + 1);
          
          TRI_ASSERT(_readBufferOffset <= _readBuffer.length());
          TRI_ASSERT(ptr == _readBuffer.c_str() + _readBufferOffset);
          TRI_ASSERT(remain == _readBuffer.length() - _readBufferOffset);
          pos = static_cast<char const*>(memchr(ptr, '\n', remain));

          if (pos == nullptr) {
            _readBufferOffset++;
            ptr++;
            remain--;
          } 
        }
      }
    }


    void SimpleHttpClient::processBody () {
      // HEAD requests may be responded to without a body...
      if (_method == HttpRequest::HTTP_REQUEST_HEAD) {
        _result->setResultType(SimpleHttpResult::COMPLETE);
        _state = FINISHED;

        if (! _keepAlive) {
          _connection->disconnect();
        }

        return;
      }

      // we need to wait for a close, if content length is unknown
      if (! _result->hasContentLength()) {
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
      }

      _readBufferOffset += _result->getContentLength();
      _result->setResultType(SimpleHttpResult::COMPLETE);
      _state = FINISHED;

      if (! _keepAlive) {
        _connection->disconnect();
      }
    }


    void SimpleHttpClient::processChunkedHeader () {
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
      string line(_readBuffer.c_str() + _readBufferOffset, len);
      StringUtils::trimInPlace(line);

      _readBufferOffset += (len + 1);
          
      // adjust offset if line ended with \r\n
      if (*pos == '\r') {
        ++_readBufferOffset;
        ++len;
      }

      // empty lines are an error
      if (line[0] == '\r' || line.empty()) {
        setErrorMessage("found invalid content-length", true);
        // reset connection
        this->close();
        _state = DEAD;

        return;
      }

      uint32_t contentLength;

      try {
        contentLength = static_cast<uint32_t>(std::stol(line, nullptr, 16)); // C++11
      }
      catch (...) {
        setErrorMessage("found invalid content-length", true);
        // reset connection
        this->close();
        _state = DEAD;

        return;
      }

      // failed: too many bytes
      if (contentLength > _maxPacketSize) {

        setErrorMessage("Content-Length > max packet size found!", true);
        // reset connection
        this->close();
        _state = DEAD;

        return;
      }

      _state = IN_READ_CHUNKED_BODY;
      _nextChunkedSize = contentLength;

      processChunkedBody();
    }


    void SimpleHttpClient::processChunkedBody () {

      // HEAD requests may be responded to without a body...
      if (_method == HttpRequest::HTTP_REQUEST_HEAD) {
        _result->setResultType(SimpleHttpResult::COMPLETE);
        _state = FINISHED;

        if (! _keepAlive) {
          _connection->disconnect();
        }

        return;
      }

      if (_readBuffer.length() - _readBufferOffset >= _nextChunkedSize + 2) {

        // last chunk length was 0, therefore we are finished
        if (_nextChunkedSize == 0) {
          _result->setResultType(SimpleHttpResult::COMPLETE);

          _state = FINISHED;

          if (! _keepAlive) {
            _connection->disconnect();
          }

          return;
        }
      
        if (_result->isDeflated()) {
          _readBuffer.inflate(_result->getBody(), 16384, _readBufferOffset);
        }
        else {
          _result->getBody().appendText(_readBuffer.c_str() + _readBufferOffset,
                                        (size_t) _nextChunkedSize);
        }

        _readBufferOffset += (size_t) _nextChunkedSize + 2;
        _state = IN_READ_CHUNKED_HEADER;
        processChunkedHeader();
      }
    }

////////////////////////////////////////////////////////////////////////////////
/// @brief extract an error message from a response
////////////////////////////////////////////////////////////////////////////////

    std::string SimpleHttpClient::getHttpErrorMessage (SimpleHttpResult* result) {
      triagens::basics::StringBuffer const& body = result->getBody();
      std::string details;

      std::unique_ptr<TRI_json_t> json(triagens::basics::JsonHelper::fromString(body.c_str(), body.length()));

      if (json != nullptr) {
        std::string const errorMessage = triagens::basics::JsonHelper::getStringValue(json.get(), "errorMessage", "");
        int errorNum = triagens::basics::JsonHelper::getNumericValue<int>(json.get(), "errorNum", 0);

        if (errorMessage != "" && errorNum > 0) {
          details = ": ArangoError " + StringUtils::itoa(errorNum) + ": " + errorMessage;
        }
      }

      return "got error from server: HTTP " +
            triagens::basics::StringUtils::itoa(result->getHttpReturnCode()) +
            " (" + result->getHttpReturnMessage() + ")" +
            details;
    }

////////////////////////////////////////////////////////////////////////////////
/// @brief fetch the version from the server
////////////////////////////////////////////////////////////////////////////////

    std::string SimpleHttpClient::getServerVersion () {
      std::unique_ptr<SimpleHttpResult> response(request(HttpRequest::HTTP_REQUEST_GET,
                                                          "/_api/version",
                                                          nullptr,
                                                          0));

      if (response == nullptr || ! response->isComplete()) {
        return "";
      }

      std::string version;

      if (response->getHttpReturnCode() == HttpResponse::OK) {
        // default value
        version = "arango";

        // convert response body to json
        std::unique_ptr<TRI_json_t> json(TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, response->getBody().c_str()));

        if (json != nullptr) {
          // look up "server" value
          std::string const server = triagens::basics::JsonHelper::getStringValue(json.get(), "server", "");

          // "server" value is a string and content is "arango"
          if (server == "arango") {
            // look up "version" value
            version = triagens::basics::JsonHelper::getStringValue(json.get(), "version", "");
          }
        }
      }
      else {
        if (response->wasHttpError()) {
          setErrorMessage(getHttpErrorMessage(response.get()), false);
        }
        _connection->disconnect();
      }

      return version;
    }

  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
