////////////////////////////////////////////////////////////////////////////////
/// @brief simple binary client
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2011 triagens GmbH, Cologne, Germany
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
/// @author Copyright 2012, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "SimpleBinaryClient.h"

#include <stdio.h>
#include <string>
#include <errno.h>

#include "Basics/StringUtils.h"
#include "Logger/Logger.h"

#include "GeneralClientConnection.h"
#include "SimpleHttpClient/SimpleHttpResult.h"
#include "BinaryServer/BinaryMessage.h"

using namespace triagens::basics;
using namespace triagens::rest;
using namespace std;

namespace triagens {
  namespace httpclient {

    // -----------------------------------------------------------------------------
    // constructors and destructors
    // -----------------------------------------------------------------------------

    SimpleBinaryClient::SimpleBinaryClient (GeneralClientConnection* connection, double requestTimeout, bool warn) :
      SimpleClient(connection, requestTimeout, warn), _result(0) {
    }

    SimpleBinaryClient::~SimpleBinaryClient () {
    }

    // -----------------------------------------------------------------------------
    // public methods
    // -----------------------------------------------------------------------------

    SimpleHttpResult* SimpleBinaryClient::request (int method,
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

            if (! _connection->handleWrite(remainingTime, (void*) (_writeBuffer.c_str() + _written), _writeBuffer.length() - _written, &bytesWritten)) {
              setErrorMessage("::send() failed", errno);
              close();
            }
            else {
              _written += bytesWritten;
              if (_written == _writeBuffer.length())  {
                _state = IN_READ_HEADER;        
              }
            }
            break;
          }

          case (IN_READ_HEADER): {
            if (_connection->handleRead(remainingTime, _readBuffer)) {
              switch (_state) {
                case (IN_READ_HEADER):
                 // _result->setData(_readBuffer.c_str(), _readBuffer.length());
                 //  _result->getBody().write(_readBuffer.c_str(), _result->getContentLength());
                  _result->setResultType(SimpleHttpResult::COMPLETE);
                  _state = FINISHED;
                  break;
                default:
                  break;
              }
            }
            else {
              setErrorMessage("gesockopt() failed", errno);
              close();
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
    
    void SimpleBinaryClient::reset () {
      SimpleClient::reset();
      if (_result) {
        _result->clear();
      }
    }

    SimpleHttpResult* SimpleBinaryClient::getResult () {
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

    void SimpleBinaryClient::setRequest (int method,
            const string& location,
            const char* body,
            size_t bodyLength,
            const map<string, string>& headerFields) {

      if (_state == DEAD) {
        _connection->resetNumConnectRetries();
      }
          
      ///////////////////// fill the write buffer //////////////////////////////      
      _writeBuffer.clear();

      // write signature
      const char* signature = BinaryMessage::getSignature();
      for (int i = 0; i < 4; ++i) {
        _writeBuffer.appendChar(signature[i]);
      }

      // write length
      uint8_t length[4];
      BinaryMessage::encodeLength(bodyLength, &length[0]);
      for (int i = 0; i < 4; ++i) {
        _writeBuffer.appendChar((char) length[i]);
      }

      _writeBuffer.appendText(body, bodyLength);
      //////////////////////////////////////////////////////////////////////////

      if (_state != FINISHED) {
        // close connection to reset all read and write buffers
        close();
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

  }

}
