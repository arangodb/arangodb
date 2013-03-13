////////////////////////////////////////////////////////////////////////////////
/// @brief simple http client
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

#include "SimpleHttpClient.h"

#include <stdio.h>
#include <string>
#include <errno.h>

#include "Basics/StringUtils.h"
#include "Logger/Logger.h"

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

    SimpleHttpClient::SimpleHttpClient (GeneralClientConnection* connection, double requestTimeout, bool warn) :
      SimpleClient(connection, requestTimeout, warn), _result(0), _maxPacketSize(128 * 1024 * 1024) {
    }

    SimpleHttpClient::~SimpleHttpClient () {
    }

// -----------------------------------------------------------------------------
    // public methods
// -----------------------------------------------------------------------------

    SimpleHttpResult* SimpleHttpClient::request (rest::HttpRequest::HttpRequestType method,
            const string& location,
            const char* body,
            size_t bodyLength,
            const map<string, string>& headerFields) {

      assert(_result == 0);

      _result = new SimpleHttpResult;
      _errorMessage = "";

      // set body to all connections
      setRequest(method, location, body, bodyLength, headerFields);

      double endTime = now() + _requestTimeout;
      double remainingTime = _requestTimeout;

      while (isWorking() && remainingTime > 0.0) {
        switch (_state) {
          case (IN_CONNECT): {
            handleConnect();
            break;
          }

          case (IN_WRITE): {
            size_t bytesWritten = 0;

            TRI_set_errno(TRI_ERROR_NO_ERROR);
            if (! _connection->handleWrite(remainingTime, (void*) (_writeBuffer.c_str() + _written), _writeBuffer.length() - _written, &bytesWritten)) {
              setErrorMessage(TRI_last_error(), false);
              this->close();
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
            if (_connection->handleRead(remainingTime, _readBuffer)) {
              switch (_state) {
                case (IN_READ_HEADER):
                  readHeader();
                  break;
                case (IN_READ_BODY):
                  readBody();
                  break;
                case (IN_READ_CHUNKED_HEADER):
                  readChunkedHeader();
                  break;
                case (IN_READ_CHUNKED_BODY):
                  readChunkedBody();
                  break;
                default:
                  break;
              }
            }
            else {
              setErrorMessage(TRI_last_error(), false);
              this->close();
            }
            break;
          }
          default:
            break;
        }

        remainingTime = endTime - now();
      }

      if (isWorking() && _errorMessage == "" ) {
        setErrorMessage("Request timeout reached");
      }

      // set result type in getResult()
      SimpleHttpResult* result = getResult();

      _result = 0;

      return result;
    }

// -----------------------------------------------------------------------------
    // private methods
// -----------------------------------------------------------------------------

    void SimpleHttpClient::reset () {
      SimpleClient::reset();
      if (_result) {
        _result->clear();
      }
    }

////////////////////////////////////////////////////////////////////////////////
/// @brief sets username and password
////////////////////////////////////////////////////////////////////////////////

    void SimpleHttpClient::setUserNamePassword (
            const string& prefix,
            const string& username,
            const string& password) {

      string value = triagens::basics::StringUtils::encodeBase64(username + ":" + password);

      _pathToBasicAuth.push_back(make_pair(prefix, value));
    }

    SimpleHttpResult* SimpleHttpClient::getResult () {
      switch (_state) {
        case (IN_CONNECT):
          _result->setResultType(SimpleHttpResult::COULD_NOT_CONNECT);
          break;

        case (IN_WRITE):
          _result->setResultType(SimpleHttpResult::WRITE_ERROR);
          break;

        case (IN_READ_HEADER):
        case (IN_READ_BODY):
        case (IN_READ_CHUNKED_HEADER):
        case (IN_READ_CHUNKED_BODY):
          _result->setResultType(SimpleHttpResult::READ_ERROR);
          break;

        case (FINISHED):
          _result->setResultType(SimpleHttpResult::COMPLETE);
          break;

        default :
          _result->setResultType(SimpleHttpResult::COULD_NOT_CONNECT);
      }

      return _result;
    }

    void SimpleHttpClient::setRequest (rest::HttpRequest::HttpRequestType method,
            const string& location,
            const char* body,
            size_t bodyLength,
            const map<string, string>& headerFields) {

      if (_state == DEAD) {
        _connection->resetNumConnectRetries();
      }

///////////////////// fill the write buffer //////////////////////////////
      _writeBuffer.clear();

      HttpRequest::appendMethod(method, &_writeBuffer);

      string l = location;
      if (location.length() == 0 || location[0] != '/') {
        l = "/" + location;
      }

      _writeBuffer.appendText(l);
      _writeBuffer.appendText(" HTTP/1.1\r\n");

      _writeBuffer.appendText("Host: ");
      _writeBuffer.appendText(_connection->getEndpoint()->getHostString());
      _writeBuffer.appendText("\r\n");
      _writeBuffer.appendText("Connection: Keep-Alive\r\n");
      _writeBuffer.appendText("User-Agent: VOC-Client/1.0\r\n");

      // do basic authorization
      if (! _pathToBasicAuth.empty()) {
        string foundPrefix;
        string foundValue;
        std::vector< std::pair<std::string, std::string> >::iterator i = _pathToBasicAuth.begin();
        for (; i != _pathToBasicAuth.end(); ++i) {
          string f = i->first;
          if (l.find(f) == 0) {
            // f is prefix of l
            if (f.length() > foundPrefix.length()) {
              foundPrefix = f;
              foundValue = i->second;
            }
          }
        }
        if (foundValue.length() > 0) {
          _writeBuffer.appendText("Authorization: Basic ");
          _writeBuffer.appendText(foundValue);
          _writeBuffer.appendText("\r\n");
        }
      }

      for (map<string, string>::const_iterator i = headerFields.begin(); i != headerFields.end(); ++i) {
        // TODO: check Header name and value
        _writeBuffer.appendText(i->first);
        _writeBuffer.appendText(": ");
        _writeBuffer.appendText(i->second);
        _writeBuffer.appendText("\r\n");
      }

      _writeBuffer.appendText("Content-Length: ");
      _writeBuffer.appendInteger(bodyLength);
      _writeBuffer.appendText("\r\n\r\n");
      _writeBuffer.appendText(body, bodyLength);

      LOGGER_TRACE("Request: " << _writeBuffer.c_str());

////////////////////////////////////////////////////////////////////////////////


      if (_state != FINISHED) {
        // close connection to reset all read and write buffers
        this->close();
      }

      if (_connection->isConnected()) {
        // we are connected, start with writing
        _state = IN_WRITE;
        _written = 0;
      }
      else {
        // connect to server
        _state = IN_CONNECT;
      }
    }


// -----------------------------------------------------------------------------
    // private methods
// -----------------------------------------------------------------------------

    bool SimpleHttpClient::readHeader () {
      char* pos = (char*) memchr(_readBuffer.c_str(), '\n', _readBuffer.length());

      while (pos) {
        // size_t is unsigned, should never get < 0
        assert(pos >= _readBuffer.c_str());

        size_t len = pos - _readBuffer.c_str();
        string line(_readBuffer.c_str(), len);
        _readBuffer.erase_front(len + 1);

        //printf("found header line %s\n", line.c_str());

        if (line == "\r" || line == "") {
          // end of header found
          if (_result->isChunked()) {
            _state = IN_READ_CHUNKED_HEADER;
            return readChunkedHeader();
          }
          else if (_result->getContentLength()) {

            if (_result->getContentLength() > _maxPacketSize) {
              setErrorMessage("Content-Length > max packet size found", true);

              // reset connection
              this->close();
              _state = DEAD;

              return false;
            }

            _state = IN_READ_BODY;
            return readBody();
          }
          else {
            _result->setResultType(SimpleHttpResult::COMPLETE);
            _state = FINISHED;
          }
          break;
        }
        else {
          _result->addHeaderField(line);
        }

        pos = (char*) memchr(_readBuffer.c_str(), '\n', _readBuffer.length());
      }
      return true;
    }

    bool SimpleHttpClient::readBody () {
      if (_readBuffer.length() >= _result->getContentLength()) {
        _result->getBody().write(_readBuffer.c_str(), _result->getContentLength());
        _readBuffer.erase_front(_result->getContentLength());
        _result->setResultType(SimpleHttpResult::COMPLETE);
        _state = FINISHED;
      }

      return true;
    }

    bool SimpleHttpClient::readChunkedHeader () {
      char* pos = (char*) memchr(_readBuffer.c_str(), '\n', _readBuffer.length());

      while (pos) {
        // got a line
        size_t len = pos - _readBuffer.c_str();
        string line(_readBuffer.c_str(), len);
        _readBuffer.erase_front(len + 1);

        string trimmed = StringUtils::trim(line);

        if (trimmed == "\r" || trimmed == "") {
          // ignore empty lines
          pos = (char*) memchr(_readBuffer.c_str(), '\n', _readBuffer.length());
          continue;
        }

        uint32_t contentLength;
        sscanf(trimmed.c_str(), "%x", &contentLength);

        if (contentLength == 0) {
          // OK: last content length found

          _result->setResultType(SimpleHttpResult::COMPLETE);

          _state = FINISHED;

          return true;
        }

        if (contentLength > _maxPacketSize) {
          // failed: too many bytes

          setErrorMessage("Content-Length > max packet size found!", true);
          // reset connection
          this->close();
          _state = DEAD;

          return false;
        }

        _state = IN_READ_CHUNKED_BODY;
        _nextChunkedSize = contentLength;

        return readChunkedBody();
      }

      return true;
    }

    bool SimpleHttpClient::readChunkedBody () {

      if (_readBuffer.length() >= _nextChunkedSize) {

        _result->getBody().write(_readBuffer.c_str(), (size_t) _nextChunkedSize);
        _readBuffer.erase_front((size_t) _nextChunkedSize);
        _state = IN_READ_CHUNKED_HEADER;
        return readChunkedHeader();
      }

      return true;
    }

  }

}
