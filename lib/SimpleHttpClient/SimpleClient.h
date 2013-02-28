////////////////////////////////////////////////////////////////////////////////
/// @brief simple client
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

#ifndef TRIAGENS_SIMPLE_HTTP_CLIENT_SIMPLE_CLIENT_H
#define TRIAGENS_SIMPLE_HTTP_CLIENT_SIMPLE_CLIENT_H 1

#include <Basics/Common.h>

#include "Basics/StringBuffer.h"
#include "Logger/Logger.h"
#include "Rest/HttpRequest.h"

namespace triagens {
  namespace httpclient {
    
    class GeneralClientConnection;
    class SimpleHttpResult;
      
    ////////////////////////////////////////////////////////////////////////////////
    /// @brief simple client
    ////////////////////////////////////////////////////////////////////////////////

    class SimpleClient {

    private:
      SimpleClient (SimpleClient const&);
      SimpleClient& operator= (SimpleClient const&);

    public:

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
      /// @brief constructs a new client
      ////////////////////////////////////////////////////////////////////////////////

      SimpleClient (GeneralClientConnection*, 
                    double, 
                    bool);

      ////////////////////////////////////////////////////////////////////////////////
      /// @brief destructs a client
      ////////////////////////////////////////////////////////////////////////////////

      virtual ~SimpleClient ();

      ////////////////////////////////////////////////////////////////////////////////
      /// @brief returns the port of the remote server
      ////////////////////////////////////////////////////////////////////////////////      
     
      const string& getErrorMessage () {
          return _errorMessage;
      }
      
      ////////////////////////////////////////////////////////////////////////////////
      /// @brief set user name and password
      ////////////////////////////////////////////////////////////////////////////////
    
      virtual void setUserNamePassword (const string& prefix,
                                        const string& username,
                                        const string& password) = 0;

      ////////////////////////////////////////////////////////////////////////////////
      /// @brief make a http request
      /// the caller has to delete the result object
      ////////////////////////////////////////////////////////////////////////////////

      virtual SimpleHttpResult* request (rest::HttpRequest::HttpRequestType method, 
                                         const string& location,
                                         const char* body, 
                                         size_t bodyLength,
                                         const map<string, string>& headerFields) = 0;

    protected:
      
      ////////////////////////////////////////////////////////////////////////////////
      /// @brief register and dump an error message
      ////////////////////////////////////////////////////////////////////////////////
      
      void setErrorMessage (const string& message, bool forceWarn = false) {
        _errorMessage = message;
       
        if (_warn || forceWarn) { 
          LOGGER_WARNING(_errorMessage);
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
      /// @brief returns true if the request is in progress
      ////////////////////////////////////////////////////////////////////////////////      

      bool isWorking () const {
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
      /// @brief reset state
      ////////////////////////////////////////////////////////////////////////////////

      virtual void reset ();

      ////////////////////////////////////////////////////////////////////////////////
      /// @brief get timestamp
      ///
      /// @return double          time in seconds
      ////////////////////////////////////////////////////////////////////////////////
      
      static double now ();                  
      
    protected:

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
      
      string _errorMessage;
      
    };
  }
}

#endif
