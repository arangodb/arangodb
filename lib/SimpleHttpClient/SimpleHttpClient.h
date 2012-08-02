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

#include "Basics/StringBuffer.h"
#include "Logger/Logger.h"
#include "SimpleHttpClient/GeneralClientConnection.h"

namespace triagens {
  namespace httpclient {
      
    class SimpleHttpResult;
    
    ////////////////////////////////////////////////////////////////////////////////
    /// @brief simple http client
    ////////////////////////////////////////////////////////////////////////////////

    class SimpleHttpClient {

    private:
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
      ////////////////////////////////////////////////////////////////////////////////

      SimpleHttpClient (GeneralClientConnection*, 
                        double, 
                        bool);

      ////////////////////////////////////////////////////////////////////////////////
      /// @brief destructs a http client
      ////////////////////////////////////////////////////////////////////////////////

      virtual ~SimpleHttpClient ();

      ////////////////////////////////////////////////////////////////////////////////
      /// @brief make a http request
      ///
      /// The caller has to delete the result object
      ////////////////////////////////////////////////////////////////////////////////

      SimpleHttpResult* request (int method, 
                                 const string& location,
                                 const char* body, 
                                 size_t bodyLength,
                                 const map<string, string>& headerFields);

      
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
      /// @brief register and dump an error message
      ////////////////////////////////////////////////////////////////////////////////
      
      void setErrorMessage (const string& message) {
        _errorMessage = message;
       
        if (_warn) { 
          LOGGER_WARNING << _errorMessage;
        }
      }
      
      ////////////////////////////////////////////////////////////////////////////////
      /// @brief register an error message
      ////////////////////////////////////////////////////////////////////////////////

      void setErrorMessage (const string& message, int error) {
        if (error != 0) {
          _errorMessage = message + ": " + strerror(error);
        }
        else {
          setErrorMessage(message);
        }
      }
      
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
      
      ////////////////////////////////////////////////////////////////////////////////
      /// @brief returns true if the request is in progress
      ////////////////////////////////////////////////////////////////////////////////      

      bool isWorking () {
        return _state < FINISHED;
      }   
      
      ////////////////////////////////////////////////////////////////////////////////
      /// @brief initialise the connection
      ////////////////////////////////////////////////////////////////////////////////
    
      void handleConnect ();
      
      ////////////////////////////////////////////////////////////////////////////////
      /// @brief close connection
      ////////////////////////////////////////////////////////////////////////////////

      bool close ();

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
      /// @brief reset state
      ////////////////////////////////////////////////////////////////////////////////

      void reset ();

      ////////////////////////////////////////////////////////////////////////////////
      /// @brief get timestamp
      ///
      /// @return double          time in seconds
      ////////////////////////////////////////////////////////////////////////////////
      
      static double now ();                  
      
    private:
      ////////////////////////////////////////////////////////////////////////////////
      /// @brief connection used (TCP or SSL connection)
      ////////////////////////////////////////////////////////////////////////////////

      GeneralClientConnection* _connection;

      // read and write buffer
      triagens::basics::StringBuffer _writeBuffer;
      triagens::basics::StringBuffer _readBuffer;
      
      double _requestTimeout;

      bool _warn;
                  
      request_state _state;
      
      size_t _written;
      
      uint32_t _nextChunkedSize;

      SimpleHttpResult* _result;
      
      string _errorMessage;
      
      std::vector< std::pair<std::string, std::string> >_pathToBasicAuth;

    };
  }
}

#endif
