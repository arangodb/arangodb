///////////////////////////////////////////
/////////////////////////////////////
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

#ifndef TRIAGENS_SIMPLE_HTTP_CLIENT_SIMPLE_HTTP_CONNECTION_H
#define TRIAGENS_SIMPLE_HTTP_CLIENT_SIMPLE_HTTP_CONNECTION_H 1

#include <Basics/Common.h>

#include <netdb.h>

namespace triagens {
  namespace httpclient {


    ////////////////////////////////////////////////////////////////////////////////
    /// @brief simple http client
    ////////////////////////////////////////////////////////////////////////////////

    class SimpleHttpConnection {
    private:
      enum { READBUFFER_SIZE = 8096 };
      
      SimpleHttpConnection (SimpleHttpConnection const&);
      SimpleHttpConnection& operator= (SimpleHttpConnection const&);

    public:

      ////////////////////////////////////////////////////////////////////////////////
      /// @brief constructs a new http client
      ///
      /// @param string hostname            server hostname
      /// @param int port                   server port
      /// @param double connTimeout         timeout in seconds for tcp connect 
      ///
      ////////////////////////////////////////////////////////////////////////////////

      SimpleHttpConnection (string const& hostname, int port, double connTimeout);

      ////////////////////////////////////////////////////////////////////////////////
      /// @brief destructs a http client
      ////////////////////////////////////////////////////////////////////////////////

      virtual ~SimpleHttpConnection ();

      ////////////////////////////////////////////////////////////////////////////////
      /// @brief connects to server
      ////////////////////////////////////////////////////////////////////////////////

      bool connect ();

      ////////////////////////////////////////////////////////////////////////////////
      /// @brief close connection
      ////////////////////////////////////////////////////////////////////////////////

      bool close ();

      ////////////////////////////////////////////////////////////////////////////////
      /// @brief writes a string
      ///
      /// @param string str     the string       
      /// @param double timeout      timeout in seconds for read
      ///
      /// @return bool          returns true on success
      ////////////////////////////////////////////////////////////////////////////////

      bool write (std::string const& str, double timeout);

      ////////////////////////////////////////////////////////////////////////////////
      /// @brief reads one line 
      ///
      /// @param string line         the result string
      /// @param double timeout      timeout in seconds for read
      ///
      /// @return bool          returns true on success
      ////////////////////////////////////////////////////////////////////////////////

      bool readLn (std::string& line, double timeout);

      ////////////////////////////////////////////////////////////////////////////////
      /// @brief reads "contenLength" bytes
      ///
      /// @param stringstream stream    the bytes are written to that stream
      /// @param size_t contenLength    number of bytes to read
      /// @param double timeout         timeout in seconds for read
      ///
      /// @return bool          returns true on success
      ////////////////////////////////////////////////////////////////////////////////

      size_t read (std::stringstream& stream, size_t contenLength, double timeout);

      ////////////////////////////////////////////////////////////////////////////////
      /// @brief returns true if the client is connected
      ///
      /// @return bool          true if the client is connected
      ////////////////////////////////////////////////////////////////////////////////
      
      bool isConnected ();
      
      ////////////////////////////////////////////////////////////////////////////////
      /// @brief get the hostname 
      ///
      /// @return string          the server name
      ////////////////////////////////////////////////////////////////////////////////

      const std::string& getHostname () {
        return _hostname;
      }
      
      ////////////////////////////////////////////////////////////////////////////////
      /// @brief get the port
      ///
      /// @return string          the server port
      ////////////////////////////////////////////////////////////////////////////////
      
      int getPort () {
        return _port;
      }

      ////////////////////////////////////////////////////////////////////////////////
      /// @brief get last errno
      ///
      /// @return int          errno
      ////////////////////////////////////////////////////////////////////////////////
      
      int getErrno () {
        return _errno;
      }

      ////////////////////////////////////////////////////////////////////////////////
      /// @brief get error message
      ///
      /// @return string          the last error message
      ////////////////////////////////////////////////////////////////////////////////
      
      const std::string& getErrorMessage () {
        return _errorMessage;
      }
      
      ////////////////////////////////////////////////////////////////////////////////
      /// @brief get error message
      ///
      /// @return string          the last error message
      ////////////////////////////////////////////////////////////////////////////////
      
      double now ();
                  
    private:

      ////////////////////////////////////////////////////////////////////////////////
      /// @brief connect the socket
      ////////////////////////////////////////////////////////////////////////////////

      bool connectSocket ();
      bool connectSocket (struct addrinfo *aip);

      ////////////////////////////////////////////////////////////////////////////////
      /// @brief returns true if the socket is readable
      ////////////////////////////////////////////////////////////////////////////////

      bool readable (double timeout);
      
      ////////////////////////////////////////////////////////////////////////////////
      /// @brief returns true if the socket is readable
      ////////////////////////////////////////////////////////////////////////////////

      bool writeable (double timeout);
      
      ////////////////////////////////////////////////////////////////////////////////
      /// @brief sets non-blocking mode for a socket
      ////////////////////////////////////////////////////////////////////////////////

      bool setNonBlocking (socket_t fd);

      ////////////////////////////////////////////////////////////////////////////////
      /// @brief sets close-on-exit for a socket
      ////////////////////////////////////////////////////////////////////////////////

      bool setCloseOnExec (socket_t fd);
      
      void clearErrorMessage () {
        _errno = 0;
        _errorMessage = "";
      }
      
      void setErrorMessage (std::string const& message, int no = 0) {
        _errno = no;
        _errorMessage = message;
      }

    private:
      string _hostname;
      int _port;

      bool _isConnected;

      socket_t _socket;

      uint64_t _connectionTimeout;
      
      static double _minimalConnectionTimeout;
      static double _minimalRequestTimeout;
      
      int _errno;
      std::string _errorMessage;
      
      char _readBuffer[READBUFFER_SIZE];
      size_t _readBufferSize;      
    };
  }
}

#endif
