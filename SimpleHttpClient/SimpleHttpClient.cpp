////////////////////////////////////////////////////////////////////////////////
/// @brief simple http client
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
/// @author Dr. Frank Celler
/// @author Achim Brandt
/// @author Copyright 2009, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "SimpleHttpClient.h"

#include <stdio.h>
#include <string>
#include <errno.h>

#include <netinet/in.h>
#include <netinet/tcp.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>

#include "Basics/StringUtils.h"
#include "Logger/Logger.h"

#include "SimpleHttpResult.h"

using namespace triagens::basics;
using namespace std;

namespace triagens {
  namespace httpclient {

    // -----------------------------------------------------------------------------
    // constructors and destructors
    // -----------------------------------------------------------------------------

    SimpleHttpClient::SimpleHttpClient (string const& hostname,
            int port,
            double requestTimeout,
            double connectTimeout,
            size_t connectRetries) :
    _hostname (hostname),
    _port (port),
    _requestTimeout (requestTimeout),
    _connectTimeout (connectTimeout),
    _connectRetries (connectRetries) {

      _lastConnectTime = 0.0;
      _numConnectRetries = 0;
      _result = 0;
      _errorMessage = "";
      _written = 0;
      
      // _writeBuffer.clear();
      
      reset();

    }

    SimpleHttpClient::~SimpleHttpClient () {
      if (_isConnected) {
        ::close(_socket);
      }

    }

    // -----------------------------------------------------------------------------
    // public methods
    // -----------------------------------------------------------------------------

    SimpleHttpResult* SimpleHttpClient::request (int method,
            const string& location,
            const char* body,
            size_t bodyLength,
            const map<string, string>& headerFields) {
      
      _result = new SimpleHttpResult;
      _errorMessage = "";

      // set body to all connections
      setRequest(method, location, body, bodyLength, headerFields);

      double endTime = now() + _requestTimeout;
      double remainingTime = _requestTimeout;

      while (isWorking() && remainingTime > 0.0) {
        switch (_state) {
          case (IN_CONNECT):
            handleConnect();
            break;

          case (IN_WRITE):
            handleWrite(remainingTime);
            break;

          case (IN_READ_HEADER):
          case (IN_READ_BODY):
          case (IN_READ_CHUNKED_HEADER):
          case (IN_READ_CHUNKED_BODY):
            handleRead(remainingTime);
            break;
          default:
            break;
        }

        remainingTime = endTime - now();
      }
      
      if (isWorking() && _errorMessage=="" ) {
        LOGGER_ERROR << "Request timeout reached.";
        _errorMessage = "Request timeout reached.";
      }
      
      return getResult();
    }

    // -----------------------------------------------------------------------------
    // private methods
    // -----------------------------------------------------------------------------

    void SimpleHttpClient::handleConnect () {
      _isConnected = false;
      _socket = -1;

      if (_numConnectRetries < _connectRetries + 1) {
        _numConnectRetries++;
      }
      else {
        LOGGER_ERROR << "Could not connect to '" << _hostname << ":" << _port << "'! Connection is dead";
        _state = DEAD;
        return;
      }

      if (_hostname == "" || _port == 0) {
        _errorMessage = "Could not connect to '" + _hostname + ":" + StringUtils::itoa(_port) + "'";
        LOGGER_ERROR << "Could not connect to '" << _hostname << ":" << _port << "'! Connection is dead";
        _state = DEAD;
        return;
      }
    
      _lastConnectTime = now();

      struct addrinfo *result, *aip;
      struct addrinfo hints;
      int error;

      memset(&hints, 0, sizeof (struct addrinfo));
      hints.ai_family = AF_INET; // Allow IPv4 or IPv6
      hints.ai_flags = AI_NUMERICSERV | AI_ALL;
      hints.ai_socktype = SOCK_STREAM;

      string portString = StringUtils::itoa(_port);

      error = getaddrinfo(_hostname.c_str(), portString.c_str(), &hints, &result);

      if (error != 0) {
        LOGGER_ERROR << "Could not connect to '" << _hostname << ":" << _port << "'! Connection is dead";
        _errorMessage = "Could not connect to '" + _hostname + ":" + StringUtils::itoa(_port) +
                "'! getaddrinfo() failed with: " + string(gai_strerror(error));
        _state = DEAD;
        return;
      }

      // Try all returned addresses until one works
      for (aip = result; aip != NULL; aip = aip->ai_next) {

        // try to connect the address info pointer
        if (connectSocket(aip)) {
          // is connected
          LOGGER_TRACE << "Connected to '" << _hostname << ":" << _port << "'!";
          _isConnected = true;
          break;
        }

      }

      freeaddrinfo(result);

      if (_isConnected) {
        // can write now
        _state = IN_WRITE;
        _written = 0;
      }
    }

    void SimpleHttpClient::handleWrite (double timeout) {
      struct timeval tv;
      fd_set fdset;

      tv.tv_sec = (uint64_t) timeout;
      tv.tv_usec = ((uint64_t) (timeout * 1000000.0)) % 1000000;

      FD_ZERO(&fdset);
      FD_SET(_socket, &fdset);

      if (select(_socket + 1, NULL, &fdset, NULL, &tv) > 0) {
        write();
      }
    }

    void SimpleHttpClient::handleRead (double timeout) {
      struct timeval tv;
      fd_set fdset;

      tv.tv_sec = (uint64_t) timeout;
      tv.tv_usec = ((uint64_t) (timeout * 1000000.0)) % 1000000;

      FD_ZERO(&fdset);
      FD_SET(_socket, &fdset);

      if (select(_socket + 1, &fdset, NULL, NULL, &tv) > 0) {
        read();
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

    void SimpleHttpClient::setRequest (int method,
            const string& location,
            const char* body,
            size_t bodyLength,
            const map<string, string>& headerFields) {

      if (_state == DEAD) {
        _numConnectRetries = 0;
        _lastConnectTime = 0.0;
      }

      ///////////////////// fill the write buffer //////////////////////////////      
      _writeBuffer.clear();

      switch (method) {
        case GET:
          _writeBuffer.appendText("GET ");
          break;
        case POST:
          _writeBuffer.appendText("POST ");
          break;
        case PUT:
          _writeBuffer.appendText("PUT ");
          break;
        case DELETE:
          _writeBuffer.appendText("DELETE ");
          break;
        case HEAD:
          _writeBuffer.appendText("HEAD ");
          break;
        default:
          _writeBuffer.appendText("POST ");
          break;
      }

      string l = location;
      if (location.length() == 0 || location[0] != '/') {
        l = "/" + location;
      }

      _writeBuffer.appendText(l);
      _writeBuffer.appendText(" HTTP/1.1\r\n");

      _writeBuffer.appendText("Host: ");
      _writeBuffer.appendText(_hostname);
      _writeBuffer.appendText(":");
      _writeBuffer.appendInteger(_port);
      _writeBuffer.appendText("\r\n");
      _writeBuffer.appendText("Connection: Keep-Alive\r\n");
      _writeBuffer.appendText("User-Agent: VOC-Client/1.0\r\n");

      //requestBuffer << "Accept: application/json\r\n";      
      //if (bodyLength > 0) {
      //  requestBuffer << "Content-Type: application/json; charset=utf-8\r\n";
      //}

      // do basic authorization
      if (_pathToBasicAuth.size() > 0) {
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

      LOGGER_TRACE << "Request: " << _writeBuffer.c_str();

      //////////////////////////////////////////////////////////////////////////


      if (_state != FINISHED) {
        // close connection to reset all read and write buffers
        close();
      }

      if (_isConnected) {
        // we are connected start with writing
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

    bool SimpleHttpClient::close () {
      if (_isConnected) {
        ::close(_socket);
        _isConnected = false;
      }

      reset();

      return true;
    }

    bool SimpleHttpClient::write () {
      if (!checkSocket()) {
        return false;
      }
      
      //printf("write():\n%s\n", (_writeBuffer.c_str() + _written));

#ifdef __APPLE__
      int status = ::send(_socket, _writeBuffer.c_str() + _written, _writeBuffer.length() - _written, 0);
#else
      int status = ::send(_socket, _writeBuffer.c_str() + _written, _writeBuffer.length() - _written, MSG_NOSIGNAL);
#endif

      if (status == -1) {
        _errorMessage = "::send() failed with: " + string(strerror(errno));
        //LOGGER_ERROR << "::send() failed with " << strerror(errno);
        
        close();
        
        return false;
      }
      
      _written += status;
      
      if (_written == _writeBuffer.length())  {
        _state = IN_READ_HEADER;        
      }
      return true;
    }

    bool SimpleHttpClient::read () {
      if (!checkSocket()) {
        return false;
      }

     do {
        char buffer[READBUFFER_SIZE];

        int len_read = ::read(_socket, buffer, READBUFFER_SIZE - 1);

        if (len_read <= 0) {
          // error: stop reading
          break;
        }

        _readBuffer.appendText(buffer, len_read);
      }
      while(readable());

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
      return true;
    }

    bool SimpleHttpClient::readHeader () {
      char* pos = (char*) memchr(_readBuffer.c_str(), '\n', _readBuffer.length());

      while (pos) {
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
            
            if (_result->getContentLength() > 5000000) {
              _errorMessage = "Content length > 5000000 bytes found!";
              LOGGER_ERROR << "Content length > 5000000 bytes found! Closing connection.";
              
              // reset connection 
              close();
          
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

        string trimed = StringUtils::trim(line);

        if (trimed == "\r" || trimed == "") {
          // ignore empty lines
          pos = (char*) memchr(_readBuffer.c_str(), '\n', _readBuffer.length());
          continue;
        }

        uint32_t contentLength;
        sscanf(trimed.c_str(), "%x", &contentLength);

        if (contentLength == 0) {
          // OK: last content length found
          //printf("Last length found\n");

          _result->setResultType(SimpleHttpResult::COMPLETE);
          
          _state = FINISHED;
          
          return true;
        }

        if (contentLength > 5000000) {
          // failed: too many bytes
          
          _errorMessage = "Content length > 5000000 bytes found!";
          LOGGER_ERROR << "Content length > 5000000 bytes found! Closing connection.";
 
          // reset connection 
          close();
          
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

    bool SimpleHttpClient::readable () {
      fd_set fdset;
      FD_ZERO(&fdset);
      FD_SET(_socket, &fdset);

      struct timeval tv;
      tv.tv_sec = 0;
      tv.tv_usec = 0;

      if (select(_socket + 1, &fdset, NULL, NULL, &tv) == 1) {        
        return checkSocket();
      }
      
      return false;
    }

    bool SimpleHttpClient::checkSocket () {
      int so_error = -1;
      socklen_t len = sizeof so_error;

      getsockopt(_socket, SOL_SOCKET, SO_ERROR, &so_error, &len);

      if (so_error == 0) {
        return true;
      }

      _errorMessage = "getsockopt() failed with: " + string(strerror(errno));
      LOGGER_ERROR << "getsockopt() failed with: " << strerror(errno) << ". Closing connection.";

      //close and reset conection
      close();

      _state = IN_CONNECT;

      return false;
    }

    bool SimpleHttpClient::connectSocket (struct addrinfo *aip) {

      // create socket and connect socket here
      
      _socket = socket(aip->ai_family, aip->ai_socktype, aip->ai_protocol);

      // check socket and set the socket not blocking and close on exit 
      
      if (_socket == -1) {
        //setErrorMessage("Socket not connected. socket() faild with: " + string(strerror(errno)), errno);
        return false;
      }

      if (!setNonBlocking(_socket)) {
        //setErrorMessage("Socket not connected. Set non blocking failed with: " + string(strerror(errno)), errno);
        ::close(_socket);
        _socket = -1;
        return false;
      }

      if (!setCloseOnExec(_socket)) {
        //setErrorMessage("Socket not connected. Set close on exec failed with: " + string(strerror(errno)), errno);
        ::close(_socket);
        _socket = -1;
        return false;
      }

      ::connect(_socket, (const struct sockaddr *) aip->ai_addr, aip->ai_addrlen);

      struct timeval tv;
      fd_set fdset;

      tv.tv_sec = (uint64_t) _connectTimeout;
      tv.tv_usec = ((uint64_t) (_connectTimeout * 1000000.0)) % 1000000;

      FD_ZERO(&fdset);
      FD_SET(_socket, &fdset);

      if (select(_socket + 1, NULL, &fdset, NULL, &tv) > 0) {

        if (checkSocket()) {
          return true;
        }

        return false;
      }

      // connect timeout reached
      _errorMessage = "Could not conect to server in " + StringUtils::ftoa(_connectTimeout) + " seconds.";
      LOGGER_WARNING << "Could not conect to server in " << _connectTimeout << " seconds.";

      ::close(_socket);
      
      return false;
    }

    bool SimpleHttpClient::setNonBlocking (socket_t fd) {
#ifdef TRI_HAVE_WIN32_NON_BLOCKING
      DWORD ul = 1;

      return ioctlsocket(fd, FIONBIO, &ul) == SOCKET_ERROR ? false : true;
#else
      long flags = fcntl(fd, F_GETFL, 0);

      if (flags < 0) {
        return false;
      }

      flags = fcntl(fd, F_SETFL, flags | O_NONBLOCK);

      if (flags < 0) {
        return false;
      }

      return true;
#endif
    }

    bool SimpleHttpClient::setCloseOnExec (socket_t fd) {
#ifdef TRI_HAVE_WIN32_CLOSE_ON_EXEC
#else
      long flags = fcntl(fd, F_GETFD, 0);

      if (flags < 0) {
        return false;
      }

      flags = fcntl(fd, F_SETFD, flags | FD_CLOEXEC);

      if (flags < 0) {
        return false;
      }
#endif

      return true;
    }

    void SimpleHttpClient::reset () {
      _state = IN_CONNECT;

      _isConnected = false;
      _socket = -1;

      if (_result) {
        _result->clear();
      }

      _readBuffer.clear();
    }

    double SimpleHttpClient::now () {
      struct timeval tv;
      gettimeofday(&tv, 0);

      double sec = tv.tv_sec; // seconds
      double usc = tv.tv_usec; // microseconds

      return sec + usc / 1000000.0;
    }

  }

}
