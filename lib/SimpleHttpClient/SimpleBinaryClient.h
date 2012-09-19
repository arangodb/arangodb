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

#ifndef TRIAGENS_SIMPLE_HTTP_CLIENT_SIMPLE_BINARY_CLIENT_H
#define TRIAGENS_SIMPLE_HTTP_CLIENT_SIMPLE_BINARY_CLIENT_H 1

#include <Basics/Common.h>

#include "Basics/StringBuffer.h"
#include "Logger/Logger.h"
#include "SimpleHttpClient/SimpleClient.h"
#include "ProtocolBuffers/arangodb.pb.h"

namespace triagens {
  namespace httpclient {
      
    class SimpleHttpResult;
    class GeneralClientConnection;
    
    ////////////////////////////////////////////////////////////////////////////////
    /// @brief simple binary client
    ////////////////////////////////////////////////////////////////////////////////

    class SimpleBinaryClient : public SimpleClient {

    private:
      SimpleBinaryClient (SimpleBinaryClient const&);
      SimpleBinaryClient& operator= (SimpleBinaryClient const&);

    public:

      ////////////////////////////////////////////////////////////////////////////////
      /// @brief constructs a new binary client
      ////////////////////////////////////////////////////////////////////////////////

      SimpleBinaryClient (GeneralClientConnection*, 
                          double, 
                          bool);

      ////////////////////////////////////////////////////////////////////////////////
      /// @brief destructs a binary client
      ////////////////////////////////////////////////////////////////////////////////

      virtual ~SimpleBinaryClient ();
      
      ////////////////////////////////////////////////////////////////////////////////
      /// @brief make a http request
      /// the caller has to delete the result object
      ////////////////////////////////////////////////////////////////////////////////

      virtual SimpleHttpResult* request (int method, 
                                         const string& location,
                                         const char* body, 
                                         size_t bodyLength,
                                         const map<string, string>& headerFields);
      
      ////////////////////////////////////////////////////////////////////////////////
      /// @brief set user name and password
      /// not needed for binary client
      ////////////////////////////////////////////////////////////////////////////////
    
      virtual void setUserNamePassword (const string& prefix,
                                        const string& username,
                                        const string& password) {
      }

      ////////////////////////////////////////////////////////////////////////////////
      /// @brief reset state
      ////////////////////////////////////////////////////////////////////////////////

      virtual void reset ();

    private:
      
      ////////////////////////////////////////////////////////////////////////////////
      /// @brief get the result
      /// the caller has to delete the result object
      ///
      /// @return SimpleHttpResult          the request result
      ////////////////////////////////////////////////////////////////////////////////

      SimpleHttpResult* getResult ();
      
      ////////////////////////////////////////////////////////////////////////////////
      /// @brief set the request
      ////////////////////////////////////////////////////////////////////////////////
      
      void setRequest (int method,
                       const string& location,
                       const char* body, 
                       size_t bodyLength,
                       const map<string, string>& headerFields);
      
    private:

      SimpleHttpResult* _result;

    };
  }
}

#endif
