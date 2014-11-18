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

#include "Basics/StringUtils.h"
#include "Basics/logging.h"

#include "GeneralClientConnection.h"
#include "SimpleHttpResult.h"

using namespace triagens::basics;
using namespace triagens::rest;
using namespace std;

namespace triagens {
  namespace httpclient {

// -----------------------------------------------------------------------------
// constructors and destructors
// -----------------------------------------------------------------------------

    SimpleHttpClient::SimpleHttpClient (GeneralClientConnection* connection,
                                        double requestTimeout,
                                        bool warn) :
      _connection(connection),
      _keepConnectionOnDestruction(false),
      _writeBuffer(TRI_UNKNOWN_MEM_ZONE),
      _readBuffer(TRI_UNKNOWN_MEM_ZONE),
      _readBufferOffset(0),
      _requestTimeout(requestTimeout),
      _warn(warn),
      _state(IN_CONNECT),
      _written(0),
      _errorMessage(""),
      _locationRewriter({nullptr, nullptr}),
      _nextChunkedSize(0),
      _result(nullptr),
      _maxPacketSize(128 * 1024 * 1024),
      _keepAlive(true) {

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
////////////////////////////////////////////////////////////////////////////////

    SimpleHttpResult* SimpleHttpClient::request (
      rest::HttpRequest::HttpRequestType method,
      std::string const& location,
      char const* body,
      size_t bodyLength,
      std::map<std::string, std::string> const& headerFields) {
      
      // ensure connection has not yet been invalidated
      TRI_ASSERT(_connection != nullptr);

      // ensure that result is empty
      TRI_ASSERT(_result == nullptr);

      // create a new result
      _result = new SimpleHttpResult;

      // reset error message
      _errorMessage = "";

      // set body
      setRequest(method, rewriteLocation(location), body, bodyLength, headerFields);

      // ensure state
      TRI_ASSERT(_state == IN_CONNECT || _state == IN_WRITE);

      // respect timeout
      double endTime = now() + _requestTimeout;
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

            TRI_set_errno(TRI_ERROR_NO_ERROR);

            bool res = _connection->handleWrite(
              remainingTime, 
              (void*) (_writeBuffer.c_str() + _written),
              _writeBuffer.length() - _written,
              &bytesWritten);

            if (! res) {
              if (TRI_errno() != TRI_ERROR_NO_ERROR) {
                setErrorMessage(TRI_last_error(), false);
              }

              this->close();   // this sets _state to IN_CONNECT for a retry
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

            // we need to read a at least one byte to make progress
            bool progress;
            bool res = _connection->handleRead(remainingTime, _readBuffer, progress);

            // If there was an error, then we are doomed:
            if (! res) {
              this->close();   // this sets the state to IN_CONNECT for a retry
              break;
            }

            if (! progress) {
              // write might have succeeded even if the server has closed 
              // the connection, this will then show up here with us being
              // in state IN_READ_HEADER but nothing read.
              if (_state == IN_READ_HEADER && 0 == _readBuffer.length()) {
                this->close();   // sets _state to IN_CONNECT again for a retry
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
                  this->close();   // this sets the state to retry
                }

                break;
              }

              else {
                // In all other cases of no progress, we are doomed:
                this->close();   // this sets the state to retry
                break;
              }
            }

            // we made progress, we process whatever we are in progress to do
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

        remainingTime = endTime - now();
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
/// @brief initialise the connection
////////////////////////////////////////////////////////////////////////////////

      void SimpleHttpClient::handleConnect () {
        // ensure connection has not yet been invalidated
        TRI_ASSERT(_connection != nullptr);

        if (! _connection->connect()) {
          setErrorMessage("Could not connect to '" +  _connection->getEndpoint()->getSpecification() + "'", errno);
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
          setErrorMessage("Could not connect");
          break;
        }
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
                                       std::map<std::string, std::string> const& headerFields) {
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
      static char const* ProtocolHeader = " HTTP/1.1\r\n";
      _writeBuffer.appendText(ProtocolHeader, strlen(ProtocolHeader));

      // append hostname
      string&& hostname = _connection->getEndpoint()->getHost();

      static char const* HostHeader = "Host: ";
      _writeBuffer.appendText(HostHeader, strlen(HostHeader));
      _writeBuffer.appendText(hostname);
      _writeBuffer.appendText("\r\n", 2);

      if (_keepAlive) {
        static char const* ConnectionKeepAliveHeader = "Connection: Keep-Alive\r\n";
        _writeBuffer.appendText(ConnectionKeepAliveHeader, strlen(ConnectionKeepAliveHeader));
      }
      else {
        static char const* ConnectionCloseHeader = "Connection: Close\r\n";
        _writeBuffer.appendText(ConnectionCloseHeader, strlen(ConnectionCloseHeader));
      }
      _writeBuffer.appendText("User-Agent: ArangoDB\r\n");
      _writeBuffer.appendText("Accept-Encoding: deflate\r\n");

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
          _writeBuffer.appendText("Authorization: Basic ");
          _writeBuffer.appendText(foundValue);
          _writeBuffer.appendText("\r\n", 2);
        }
      }

      for (auto const& header : headerFields) {
        // TODO: check Header name and value
        _writeBuffer.appendText(header.first);
        _writeBuffer.appendText(": ", strlen(": "));
        _writeBuffer.appendText(header.second);
        _writeBuffer.appendText("\r\n", 2);
      }

      if (method != HttpRequest::HTTP_REQUEST_GET) {
        static char const* ContentLengthHeader = "Content-Length: ";
        _writeBuffer.appendText(ContentLengthHeader, strlen(ContentLengthHeader));
        _writeBuffer.appendInteger(bodyLength);
        _writeBuffer.appendText("\r\n\r\n", 4);
      }
      else {
        _writeBuffer.appendText("\r\n", 2);
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
      size_t remain = _readBuffer.length() - _readBufferOffset;
      char const* ptr = _readBuffer.c_str() + _readBufferOffset;
      char const* pos = (char*) memchr(ptr, '\n', remain);

      while (pos) {
        if (pos > ptr && *(pos - 1) == '\r') {
          // adjust eol position
          --pos;
        }

        // end of header found
        if (*ptr == '\r' || *ptr == '\0') {
          size_t len = pos - (_readBuffer.c_str() + _readBufferOffset);
          _readBufferOffset += (len + 1);

          if (*pos == '\r') {
            // adjust offset if line ended with \r\n
            ++_readBufferOffset;
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
          
          TRI_ASSERT(remain >= (len + 1));
          remain -= (len + 1);

          pos = (char*) memchr(ptr, '\n', remain);

          if (pos == nullptr) {
            _readBufferOffset = ptr - _readBuffer.c_str() + 1;
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
        size_t len = _result->getContentLength();

        // prevent reading across the string-buffer end
        if (len > _readBuffer.length() - _readBufferOffset) {
          len = _readBuffer.length() - _readBufferOffset;
        }

        _result->getBody().appendText(_readBuffer.c_str() + _readBufferOffset, len);
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
      char* pos = (char*) memchr(ptr, '\n', remain);

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

        _result->getBody().appendText(_readBuffer.c_str() + _readBufferOffset,
                                      (size_t) _nextChunkedSize);

        _readBufferOffset += (size_t) _nextChunkedSize + 2;
        _state = IN_READ_CHUNKED_HEADER;
        processChunkedHeader();
      }
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
