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

#ifndef TRIAGENS_SIMPLE_HTTP_CLIENT_SIMPLE_HTTP_CLIENT_H
#define TRIAGENS_SIMPLE_HTTP_CLIENT_SIMPLE_HTTP_CLIENT_H 1

#include <Basics/Common.h>

#include <netdb.h>

#include "Basics/StringBuffer.h"
#include "Rest/Endpoint.h"

namespace triagens {
  namespace httpclient {
      
      class SimpleHttpResult;
    
    ////////////////////////////////////////////////////////////////////////////////
    /// @brief simple http client
    ////////////////////////////////////////////////////////////////////////////////

    class SimpleHttpClient {
    private:
      enum { READBUFFER_SIZE = 8192 };
      
      SimpleHttpClient (SimpleHttpClient const&);
      SimpleHttpClient& operator= (SimpleHttpClient const&);

    public:

      ////////////////////////////////////////////////////////////////////////////////
      /// @brief supported request methods
      ////////////////////////////////////////////////////////////////////////////////

      enum http_method {
        GET, POST, PUT, DELETE, HEAD
      };

      ////////////////////////////////////////////////////////////////////////////////
      /// @brief state of the connection
      ////////////////////////////////////////////////////////////////////////////////

      enum request_state {
        IN_CONNECT,
        IN_WRITE,
        IN_READ_HEADER,
        IN_READ_BODY,
        IN_READ_CHUNKED_HEADER,
        IN_READ_CHUNKED_BODY,
        FINISHED,
        DEAD
      };      
      
      ////////////////////////////////////////////////////////////////////////////////
      /// @brief constructs a new http client
      ///
      /// @param endpoint               endpoint to connect to 
      /// @param requestTimeout         timeout in seconds for the request
      /// @param connectTimeout         timeout in seconds for the tcp connect 
      /// @param connectRetries         maximum number of request retries
      /// @param warn                   enable warnings
      ///
      ////////////////////////////////////////////////////////////////////////////////

      SimpleHttpClient (triagens::rest::Endpoint* endpoint, 
                        double requestTimeout,
                        double connectTimeout, 
                        size_t connectRetries,
                        bool warn);

      ////////////////////////////////////////////////////////////////////////////////
      /// @brief destructs a http client
      ////////////////////////////////////////////////////////////////////////////////

      virtual ~SimpleHttpClient ();

      ////////////////////////////////////////////////////////////////////////////////
      /// @brief make a http request
      ///        The caller has to delete the result object
      ///
      /// @param method                         http method
      /// @param location                       request location
      /// @param body                           request body
      /// @param bodyLength                     body length
      /// @param headerFields                   header fields
      ///
      /// @return SimpleHttpResult              the request result
      ////////////////////////////////////////////////////////////////////////////////

      SimpleHttpResult* request (int method, 
                                 const string& location,
                                 const char* body, 
                                 size_t bodyLength,
                                 const map<string, string>& headerFields);

      ////////////////////////////////////////////////////////////////////////////////
      /// @brief returns true if the socket is connected
      ////////////////////////////////////////////////////////////////////////////////      

      bool isConnected () {
          return _isConnected;
      } 
      
      ////////////////////////////////////////////////////////////////////////////////
      /// @brief returns the endpoint
      ////////////////////////////////////////////////////////////////////////////////      

      const string getEndpointSpecification () {
        if (_endpoint == 0) {
          return "-";
        }

        return _endpoint->getSpecification();
      }
      
      ////////////////////////////////////////////////////////////////////////////////
      /// @brief returns the port of the remote server
      ////////////////////////////////////////////////////////////////////////////////      
     
      const string& getErrorMessage () {
          return _errorMessage;
      }

      ////////////////////////////////////////////////////////////////////////////////
      /// @brief sets username and password 
      ///
      /// @param prefix                         prefix for sending username and password
      /// @param username                       username
      /// @param password                       password
      ///
      /// @return void
      ////////////////////////////////////////////////////////////////////////////////
      
      void setUserNamePassword (const string& prefix, 
                                const string& username, 
                                const string& password);

    private:
      
      ////////////////////////////////////////////////////////////////////////////////
      /// @brief get the Result
      ///        The caller has to delete the result object
      ///
      /// @return SimpleHttpResult          the request result
      ////////////////////////////////////////////////////////////////////////////////

      SimpleHttpResult* getResult ();
      
      ////////////////////////////////////////////////////////////////////////////////
      /// @brief set the request
      ///
      /// @param method                         request method
      /// @param location                       request uri
      /// @param body                           request body
      /// @param bodyLength                     size of body
      /// @param headerFields                   list of header fields
      ////////////////////////////////////////////////////////////////////////////////
      
      void setRequest (int method,
                       const string& location,
                       const char* body, 
                       size_t bodyLength,
                       const map<string, string>& headerFields);
      
      void handleConnect ();
      void handleWrite (double timeout);
      void handleRead (double timeout);
      
      
      ////////////////////////////////////////////////////////////////////////////////
      /// @brief returns true if the request is in progress
      ////////////////////////////////////////////////////////////////////////////////      

      bool isWorking () {
        return  _state < FINISHED;
      }   
      
      ////////////////////////////////////////////////////////////////////////////////
      /// @brief returns true if the socket is connected
      ////////////////////////////////////////////////////////////////////////////////

      bool checkConnect ();

      ////////////////////////////////////////////////////////////////////////////////
      /// @brief close connection
      ////////////////////////////////////////////////////////////////////////////////

      bool close ();

      ////////////////////////////////////////////////////////////////////////////////
      /// @brief write data to socket
      ////////////////////////////////////////////////////////////////////////////////

      bool write ();

      ////////////////////////////////////////////////////////////////////////////////
      /// @brief read th answer
      ////////////////////////////////////////////////////////////////////////////////

      bool read ();
      
      ////////////////////////////////////////////////////////////////////////////////
      /// @brief read the http header
      ////////////////////////////////////////////////////////////////////////////////
      
      bool readHeader ();

      ////////////////////////////////////////////////////////////////////////////////
      /// @brief read the http body by content length
      ////////////////////////////////////////////////////////////////////////////////
      
      bool readBody ();

      ////////////////////////////////////////////////////////////////////////////////
      /// @brief read the chunk size
      ////////////////////////////////////////////////////////////////////////////////
      
      bool readChunkedHeader ();

      ////////////////////////////////////////////////////////////////////////////////
      /// @brief read the net chunk
      ////////////////////////////////////////////////////////////////////////////////
      
      bool readChunkedBody ();

      ////////////////////////////////////////////////////////////////////////////////
      /// @brief connect the socket
      ////////////////////////////////////////////////////////////////////////////////

      bool connectSocket ();

      ////////////////////////////////////////////////////////////////////////////////
      /// @brief returns true if the socket is readable
      ////////////////////////////////////////////////////////////////////////////////

      bool readable ();
      
      ////////////////////////////////////////////////////////////////////////////////
      /// @brief sets non-blocking mode for a socket
      ////////////////////////////////////////////////////////////////////////////////

      bool setNonBlocking (socket_t fd);

      ////////////////////////////////////////////////////////////////////////////////
      /// @brief sets close-on-exit for a socket
      ////////////////////////////////////////////////////////////////////////////////

      bool setCloseOnExec (socket_t fd);

      ////////////////////////////////////////////////////////////////////////////////
      /// @brief reset connection
      ////////////////////////////////////////////////////////////////////////////////

      void reset ();

      ////////////////////////////////////////////////////////////////////////////////
      /// @brief ckeck socket
      ////////////////////////////////////////////////////////////////////////////////

      bool checkSocket ();
      
      ////////////////////////////////////////////////////////////////////////////////
      /// @brief get timestamp
      ///
      /// @return double          time in seconds
      ////////////////////////////////////////////////////////////////////////////////
      
      double now ();                  
      
    private:

      ////////////////////////////////////////////////////////////////////////////////
      /// @brief the endpoint to connect to
      ////////////////////////////////////////////////////////////////////////////////

      triagens::rest::Endpoint* _endpoint;

      double _requestTimeout;
      double _connectTimeout;
      size_t _connectRetries;
      bool _warn;

      socket_t _socket;         

      // read and write buffer
      triagens::basics::StringBuffer _writeBuffer;
      triagens::basics::StringBuffer _readBuffer;
                  
      request_state _state;
      
      size_t _written;
      
      uint32_t _nextChunkedSize;

      bool _isConnected;
            
      SimpleHttpResult* _result;
      
      double _lastConnectTime;
      size_t _numConnectRetries;
      
      string _errorMessage;
      
      std::vector< std::pair<std::string, std::string> >_pathToBasicAuth;

    };
  }
}

#endif
