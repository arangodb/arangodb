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

#ifndef TRIAGENS_SIMPLE_HTTP_CLIENT_SIMPLE_HTTP_CLIENT_H
#define TRIAGENS_SIMPLE_HTTP_CLIENT_SIMPLE_HTTP_CLIENT_H 1

#include "Basics/Common.h"

#include "Basics/StringBuffer.h"
#include "Logger/Logger.h"
#include "Rest/HttpRequest.h"
#include "SimpleHttpClient/SimpleClient.h"

namespace triagens {
  namespace httpclient {

    class SimpleHttpResult;
    class GeneralClientConnection;

////////////////////////////////////////////////////////////////////////////////
/// @brief simple http client
////////////////////////////////////////////////////////////////////////////////

    class SimpleHttpClient : public SimpleClient {

    private:
      SimpleHttpClient (SimpleHttpClient const&);
      SimpleHttpClient& operator= (SimpleHttpClient const&);

    public:

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
/// the caller has to delete the result object
////////////////////////////////////////////////////////////////////////////////

      virtual SimpleHttpResult* request (rest::HttpRequest::HttpRequestType method,
                                         const string& location,
                                         const char* body,
                                         size_t bodyLength,
                                         const map<string, string>& headerFields);

////////////////////////////////////////////////////////////////////////////////
/// @brief sets username and password
///
/// @param prefix                         prefix for sending username and password
/// @param username                       username
/// @param password                       password
////////////////////////////////////////////////////////////////////////////////

      virtual void setUserNamePassword (const string& prefix,
                                        const string& username,
                                        const string& password);

////////////////////////////////////////////////////////////////////////////////
/// @brief allows rewriting locations
////////////////////////////////////////////////////////////////////////////////

      void setLocationRewriter (void* data, 
                                std::string (*func)(void*, const std::string&)) {
        _locationRewriter.data = data;
        _locationRewriter.func = func;
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief reset state
////////////////////////////////////////////////////////////////////////////////

      virtual void reset ();

    private:

////////////////////////////////////////////////////////////////////////////////
/// @brief rewrite a location URL
////////////////////////////////////////////////////////////////////////////////

      string rewriteLocation (const string& location) {
        if (_locationRewriter.func != 0) {
          return _locationRewriter.func(_locationRewriter.data, location);
        }

        return location;
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief get the result
/// the caller has to delete the result object
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

      void setRequest (rest::HttpRequest::HttpRequestType method,
                       const string& location,
                       const char* body,
                       size_t bodyLength,
                       const map<string, string>& headerFields);

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

    private:

////////////////////////////////////////////////////////////////////////////////
/// @brief struct for rewriting location URLs
////////////////////////////////////////////////////////////////////////////////

      struct {
        void* data;
        std::string (*func)(void*, const std::string&);
      }
      _locationRewriter;
    
      uint32_t _nextChunkedSize;

      SimpleHttpResult* _result;

      std::vector<std::pair<std::string, std::string> >_pathToBasicAuth;

      const size_t _maxPacketSize;
      
      rest::HttpRequest::HttpRequestType _method;

    };
  }
}

#endif
