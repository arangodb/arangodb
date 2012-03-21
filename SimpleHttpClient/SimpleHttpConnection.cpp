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

#include "SimpleHttpConnection.h"

#include <stdio.h>
#include <string>
#include <errno.h>

#include <netinet/in.h>
#include <netinet/tcp.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>

#include "Basics/StringUtils.h"

using namespace triagens::basics;
using namespace std;

namespace triagens {
  namespace httpclient {

    // -----------------------------------------------------------------------------
    // static
    // -----------------------------------------------------------------------------

    double SimpleHttpConnection::_minimalConnectionTimeout = 0.2;
    double SimpleHttpConnection::_minimalRequestTimeout = 0.2;

    // -----------------------------------------------------------------------------
    // constructors and destructors
    // -----------------------------------------------------------------------------

    SimpleHttpConnection::SimpleHttpConnection (string const& hostname, int port, double connTimeout)
    : _hostname (hostname), _port (port) {

      if (connTimeout > _minimalConnectionTimeout) {
        _connectionTimeout = connTimeout;
      }
      else {
        _connectionTimeout = _minimalConnectionTimeout;
      }

      _isConnected = false;
      _socket = -1;

      clearErrorMessage();

      _readBufferSize = 0;
    }

    SimpleHttpConnection::~SimpleHttpConnection () {
      if (_isConnected) {
        ::close(_socket);
      }
    }

    bool SimpleHttpConnection::connect () {
      if (_isConnected) {
        return true;
      }

      clearErrorMessage();
      return connectSocket();
    }

    bool SimpleHttpConnection::close () {
      _readBufferSize = 0;

      if (_isConnected) {
        ::close(_socket);
        _isConnected = false;
        return true;
      }

      return false;
    }

    bool SimpleHttpConnection::write (const string& str, double timeout) {
      if (!writeable(timeout)) {
        return false;
      }

#ifdef __APPLE__
      int status = ::send(_socket, str.c_str(), str.size(), 0);
#else
      int status = ::send(_socket, str.c_str(), str.size(), MSG_NOSIGNAL);
#endif

      if (status == -1) {
        setErrorMessage("::send failed with: " + string(strerror(errno)), errno);
        return false;
      }
      else {
        return true;
      }
    }

    bool SimpleHttpConnection::readLn (std::string& line, double timeout) {
      stringstream result;

      if (_readBufferSize > 0) {
        char* pos = (char*) memchr(_readBuffer, '\n', _readBufferSize);
        if (pos) {
          // line end found

          size_t len_line = pos - _readBuffer;
          line = string(_readBuffer, len_line);

          _readBufferSize = _readBufferSize - len_line - 1;
          memmove(_readBuffer, pos + 1, _readBufferSize);

          return true;
        }

        result.write(_readBuffer, _readBufferSize);
        _readBufferSize = 0;
      }

      bool ok = false;

      double start = now();
      double runtime = 0.0;
      
      while (runtime < timeout && readable(timeout - runtime)) {
        int len_read = ::read(_socket, _readBuffer, READBUFFER_SIZE - 1);

        if (len_read <= 0) {
          // error: stop reading
          break;
        }

        char* pos = (char*) memchr(_readBuffer, '\n', len_read);
        if (pos) {
          // line end found

          size_t len_line = pos - _readBuffer;
          result.write(_readBuffer, len_line);

          _readBufferSize = len_read - len_line - 1;
          memmove(_readBuffer, pos + 1, _readBufferSize);

          ok = true;
          break;
        }
        result.write(_readBuffer, len_read);
        runtime = now() - start;
      }

      line = result.str();
      return ok;
    }

    size_t SimpleHttpConnection::read (stringstream& stream, size_t contentLength, double timeout) {
      size_t toRead = contentLength;

      if (_readBufferSize > 0) {
        if (_readBufferSize == toRead) {
          stream.write(_readBuffer, _readBufferSize);
          _readBufferSize = 0;
          return contentLength;
        }
        if (_readBufferSize > toRead) {
          stream.write(_readBuffer, toRead);
          _readBufferSize = _readBufferSize - toRead;
          memmove(_readBuffer, _readBuffer + toRead, _readBufferSize);
          return contentLength;
        }

        toRead -= _readBufferSize;

        stream.write(_readBuffer, _readBufferSize);
        _readBufferSize = 0;
      }

      size_t len = 0;
      char* buffer = (char*) malloc(toRead + 1);

      double start = now();
      double runtime = 0.0;

      while (runtime < timeout && len < toRead && readable(timeout - runtime)) {
        int len_read = ::read(_socket, buffer + len, toRead - len);

        if (len_read <= 0) {
          // error: stop reading
          break;
        }
        len += len_read;
        runtime = now() - start;
      }
      stream.write(buffer, len);

      free(buffer);

      return (contentLength - toRead + len);
    }

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief returns true if the client is connected
    ////////////////////////////////////////////////////////////////////////////////

    bool SimpleHttpConnection::isConnected () {
      return _isConnected;
    }

    bool SimpleHttpConnection::readable (double timeout) {
      fd_set fdset;
      struct timeval tv;
      FD_ZERO(&fdset);
      FD_SET(_socket, &fdset);
      int so_error = -1;

      if (timeout > _minimalRequestTimeout) {
        tv.tv_sec = (uint64_t) timeout;
        tv.tv_usec = ((uint64_t) (timeout * 1000000.0)) % 1000000;
      }
      else {
        tv.tv_sec = (uint64_t) _minimalRequestTimeout;
        tv.tv_usec = ((uint64_t) (_minimalRequestTimeout * 1000000.0)) % 1000000;
      }

      if (select(_socket + 1, &fdset, NULL, NULL, &tv) == 1) {
        socklen_t len = sizeof so_error;

        getsockopt(_socket, SOL_SOCKET, SO_ERROR, &so_error, &len);

        if (so_error == 0) {
          return true;
        }

        setErrorMessage("socket not readable. getsockopt() failed with: " + string(strerror(errno)), errno);
      }
      else {
        setErrorMessage("socket not readable. select() failed with: " + string(strerror(errno)), errno);
      }

      return false;
    }

    bool SimpleHttpConnection::writeable (double timeout) {
      fd_set fdset;
      struct timeval tv;
      FD_ZERO(&fdset);
      FD_SET(_socket, &fdset);
      int so_error = -1;

      if (timeout > _minimalRequestTimeout) {
        tv.tv_sec = (uint64_t) timeout;
        tv.tv_usec = ((uint64_t) (timeout * 1000000.0)) % 1000000;
      }
      else {
        tv.tv_sec = (uint64_t) _minimalRequestTimeout;
        tv.tv_usec = ((uint64_t) (_minimalRequestTimeout * 1000000.0)) % 1000000;
      }

      if (select(_socket + 1, NULL, &fdset, NULL, &tv) == 1) {
        socklen_t len = sizeof so_error;

        getsockopt(_socket, SOL_SOCKET, SO_ERROR, &so_error, &len);

        if (so_error == 0) {
          return true;
        }

        setErrorMessage("socket not writable. getsockopt() failed with: " + string(strerror(errno)), errno);
      }
      else {
        setErrorMessage("socket not writable. select() failed with: " + string(strerror(errno)), errno);
      }

      return false;
    }

    // -----------------------------------------------------------------------------
    // private methods
    // -----------------------------------------------------------------------------

    bool SimpleHttpConnection::connectSocket () {
      _readBufferSize = 0;
      _isConnected = false;
      _socket = -1;

      struct addrinfo *result, *aip;
      struct addrinfo hints;
      int error;

      memset(&hints, 0, sizeof (struct addrinfo));
      hints.ai_family = AF_UNSPEC; // Allow IPv4 or IPv6
      hints.ai_flags = AI_NUMERICSERV | AI_ALL;
      hints.ai_socktype = SOCK_STREAM;

      string portString = StringUtils::itoa(_port);

      if (_hostname.empty()) {
        string localhost = "localhost";
        error = getaddrinfo(localhost.c_str(), portString.c_str(), &hints, &result);
      }
      else {
        error = getaddrinfo(_hostname.c_str(), portString.c_str(), &hints, &result);
      }

      if (error != 0) {
        setErrorMessage("socket not connected. getaddrinfo() failed with: " + string(strerror(errno)), errno);
        return false;
      }

      // Try all returned addresses until one works
      for (aip = result; aip != NULL; aip = aip->ai_next) {

        // try to connect the address info pointer
        if (connectSocket(aip)) {
          // is connected
          break;
        }

      }

      freeaddrinfo(result);

      return _isConnected;
    }

    bool SimpleHttpConnection::connectSocket (struct addrinfo *aip) {
      fd_set fdset;
      struct timeval tv;
      _isConnected = false;

      _socket = socket(aip->ai_family, aip->ai_socktype, aip->ai_protocol);
      if (_socket == -1) {
        return false;
      }

      if (!setNonBlocking(_socket)) {
        setErrorMessage("Socket not connected. Set non blocking failed with: " + string(strerror(errno)), errno);
        ::close(_socket);
        _socket = -1;
        return false;
      }

      if (!setCloseOnExec(_socket)) {
        setErrorMessage("Socket not connected. Set close on exec failed with: " + string(strerror(errno)), errno);
        ::close(_socket);
        _socket = -1;
        return false;
      }

      ::connect(_socket, (const struct sockaddr *) aip->ai_addr, aip->ai_addrlen);


      FD_ZERO(&fdset);
      FD_SET(_socket, &fdset);

      tv.tv_sec = (uint64_t) _connectionTimeout;
      tv.tv_usec = ((uint64_t) (_connectionTimeout * 1000000.0)) % 1000000;

      if (select(_socket + 1, NULL, &fdset, NULL, &tv) == 1) {
        int so_error;
        socklen_t len = sizeof so_error;

        getsockopt(_socket, SOL_SOCKET, SO_ERROR, &so_error, &len);

        if (so_error == 0) {
          // OK
          _isConnected = true;
        }
        else {
          setErrorMessage("Could not connect to server. (getsockopt() failed with: " + string(strerror(errno)) + ")", errno);
          ::close(_socket);
          _socket = -1;
        }
      }
      else {
        setErrorMessage("Could not connect to server. (select() failed with: " + string(strerror(errno)) + ")", errno);
      }

      return _isConnected;
    }

    bool SimpleHttpConnection::setNonBlocking (socket_t fd) {
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

    bool SimpleHttpConnection::setCloseOnExec (socket_t fd) {
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

    double SimpleHttpConnection::now () {
      struct timeval tv;
      gettimeofday(&tv, 0);

      double sec = tv.tv_sec; // seconds
      double usc = tv.tv_usec; // microseconds

      return sec + usc / 1000000.0;
    }
  }

}
