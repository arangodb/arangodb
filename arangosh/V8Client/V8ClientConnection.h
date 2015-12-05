////////////////////////////////////////////////////////////////////////////////
/// @brief V8 client connection
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Achim Brandt
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2008-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_V8CLIENT_V8CLIENT_CONNECTION_H
#define ARANGODB_V8CLIENT_V8CLIENT_CONNECTION_H 1

#include "Basics/Common.h"
#include "Rest/HttpRequest.h"

#include <v8.h>

// -----------------------------------------------------------------------------
// --SECTION--                                              forward declarations
// -----------------------------------------------------------------------------

namespace triagens {
  namespace httpclient {
    class GeneralClientConnection;
    class SimpleHttpClient;
    class SimpleHttpResult;
  }

  namespace rest {
    class Endpoint;
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                          class V8ClientConnection
// -----------------------------------------------------------------------------

namespace triagens {
  namespace v8client {

////////////////////////////////////////////////////////////////////////////////
/// @brief class for http requests
////////////////////////////////////////////////////////////////////////////////

    class V8ClientConnection {
      private:
        V8ClientConnection (V8ClientConnection const&);
        V8ClientConnection& operator= (V8ClientConnection const&);

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

        V8ClientConnection (triagens::rest::Endpoint*,
                            std::string,
                            std::string const&,
                            std::string const&,
                            double,
                            double,
                            size_t,
                            uint32_t,
                            bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

        ~V8ClientConnection ();

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief request location rewriter (injects database name)
////////////////////////////////////////////////////////////////////////////////

        static std::string rewriteLocation (void*, std::string const&);

////////////////////////////////////////////////////////////////////////////////
/// @brief returns true if it is connected
////////////////////////////////////////////////////////////////////////////////

        bool isConnected ();

////////////////////////////////////////////////////////////////////////////////
/// @brief set the current operation to interrupted
////////////////////////////////////////////////////////////////////////////////

        void setInterrupted (bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the current database name
////////////////////////////////////////////////////////////////////////////////

        std::string const& getDatabaseName ();

////////////////////////////////////////////////////////////////////////////////
/// @brief set the current database name
////////////////////////////////////////////////////////////////////////////////

        void setDatabaseName (std::string const&);

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the version and build number of the arango server
////////////////////////////////////////////////////////////////////////////////

        std::string const& getVersion ();

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the server mode
////////////////////////////////////////////////////////////////////////////////

        std::string const& getMode ();

////////////////////////////////////////////////////////////////////////////////
/// @brief get the last http return code
///
/// @return int          the code
////////////////////////////////////////////////////////////////////////////////

        int getLastHttpReturnCode ();

////////////////////////////////////////////////////////////////////////////////
/// @brief get the last error message
///
/// @return string          the error message
////////////////////////////////////////////////////////////////////////////////

        std::string const& getErrorMessage ();

////////////////////////////////////////////////////////////////////////////////
/// @brief get the endpoint string
///
/// @return string
////////////////////////////////////////////////////////////////////////////////

        std::string getEndpointSpecification ();

////////////////////////////////////////////////////////////////////////////////
/// @brief get the simple http client
///
/// @return triagens::httpclient::SimpleHttpClient*    then client connection
////////////////////////////////////////////////////////////////////////////////

        triagens::httpclient::SimpleHttpClient* getHttpClient();

////////////////////////////////////////////////////////////////////////////////
/// @brief do a "GET" request
///
/// @param string location                     the request location
/// @param map<string, string> headerFields    additional header fields
/// @param raw                                 return the raw response
///
/// @return v8::Value                          a V8 JavaScript object
////////////////////////////////////////////////////////////////////////////////

        v8::Handle<v8::Value> getData (v8::Isolate* isolate,
                                       std::string const& location,
                                       std::map<std::string, std::string> const& headerFields,
                                       bool raw);

////////////////////////////////////////////////////////////////////////////////
/// @brief do a "DELETE" request
///
/// @param string location                     the request location
/// @param map<string, string> headerFields    additional header fields
/// @param raw                                 return the raw response
///
/// @return v8::Value                          a V8 JavaScript object
////////////////////////////////////////////////////////////////////////////////

        v8::Handle<v8::Value> deleteData (v8::Isolate* isolate,
                                          std::string const& location,
                                          std::map<std::string, std::string> const& headerFields,
                                          bool raw);

////////////////////////////////////////////////////////////////////////////////
/// @brief do a "HEAD" request
///
/// @param string location                     the request location
/// @param map<string, string> headerFields    additional header fields
/// @param raw                                 return the raw response
///
/// @return v8::Value                          a V8 JavaScript object
////////////////////////////////////////////////////////////////////////////////

        v8::Handle<v8::Value> headData (v8::Isolate* isolate,
                                        std::string const& location,
                                        std::map<std::string, std::string> const& headerFields,
                                        bool raw);

////////////////////////////////////////////////////////////////////////////////
/// @brief do an "OPTIONS" request
///
/// @param string location                     the request location
/// @param string body                         the request body
/// @param map<string, string> headerFields    additional header fields
/// @param raw                                 return the raw response
///
/// @return v8::Value                          a V8 JavaScript object
////////////////////////////////////////////////////////////////////////////////

        v8::Handle<v8::Value> optionsData (v8::Isolate* isolate,
                                           std::string const& location,
                                           std::string const& body,
                                           std::map<std::string, std::string> const& headerFields,
                                           bool raw);

////////////////////////////////////////////////////////////////////////////////
/// @brief do a "POST" request
///
/// @param string location                     the request location
/// @param string body                         the request body
/// @param map<string, string> headerFields    additional header fields
/// @param raw                                 return the raw response
///
/// @return v8::Value                          a V8 JavaScript object
////////////////////////////////////////////////////////////////////////////////

        v8::Handle<v8::Value> postData (v8::Isolate* isolate,
                                        std::string const& location,
                                        std::string const& body,
                                        std::map<std::string, std::string> const& headerFields,
                                        bool raw);

        v8::Handle<v8::Value> postData (v8::Isolate* isolate,
                                        std::string const& location,
                                        const char* body,
                                        const size_t bodySize,
                                        std::map<std::string, std::string> const& headerFields);

////////////////////////////////////////////////////////////////////////////////
/// @brief do a "PUT" request
///
/// @param string location                     the request location
/// @param string body                         the request body
/// @param map<string, string> headerFields    additional header fields
/// @param raw                                 return the raw response
///
/// @return v8::Value                          a V8 JavaScript object
////////////////////////////////////////////////////////////////////////////////

        v8::Handle<v8::Value> putData (v8::Isolate* isolate,
                                       std::string const& location,
                                       std::string const& body,
                                       std::map<std::string, std::string> const& headerFields,
                                       bool raw);

////////////////////////////////////////////////////////////////////////////////
/// @brief do a "PATCH" request
///
/// @param string location                     the request location
/// @param string body                         the request body
/// @param map<string, string> headerFields    additional header fields
/// @param raw                                 return the raw response
///
/// @return v8::Value                          a V8 JavaScript object
////////////////////////////////////////////////////////////////////////////////

        v8::Handle<v8::Value> patchData (v8::Isolate* isolate,
                                         std::string const& location,
                                         std::string const& body,
                                         std::map<std::string, std::string> const& headerFields,
                                         bool raw);

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

    private:

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a request
////////////////////////////////////////////////////////////////////////////////

        v8::Handle<v8::Value> requestData (v8::Isolate* isolate,
                                           rest::HttpRequest::HttpRequestType method,
                                           std::string const& location,
                                           const char* body,
                                           const size_t bodySize,
                                           std::map<std::string, std::string> const& headerFields);

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a request
////////////////////////////////////////////////////////////////////////////////

      v8::Handle<v8::Value> requestData (v8::Isolate* isolate,
                                         rest::HttpRequest::HttpRequestType method,
                                         std::string const& location,
                                         std::string const& body,
                                         std::map<std::string, std::string> const& headerFields);

////////////////////////////////////////////////////////////////////////////////
/// @brief handles a result
////////////////////////////////////////////////////////////////////////////////

      v8::Handle<v8::Value> handleResult (v8::Isolate* isolate);

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a request and returns raw response
////////////////////////////////////////////////////////////////////////////////

      v8::Handle<v8::Value> requestDataRaw (v8::Isolate* isolate,
                                            rest::HttpRequest::HttpRequestType method,
                                            std::string const& location,
                                            std::string const& body,
                                            std::map<std::string, std::string> const& headerFields);

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

    private:

////////////////////////////////////////////////////////////////////////////////
/// @brief connection
////////////////////////////////////////////////////////////////////////////////

      triagens::httpclient::GeneralClientConnection* _connection;

////////////////////////////////////////////////////////////////////////////////
/// @brief database name
////////////////////////////////////////////////////////////////////////////////

      std::string _databaseName;

////////////////////////////////////////////////////////////////////////////////
/// @brief server version
////////////////////////////////////////////////////////////////////////////////

      std::string _version;

////////////////////////////////////////////////////////////////////////////////
/// @brief mode
////////////////////////////////////////////////////////////////////////////////
      
      std::string _mode;

////////////////////////////////////////////////////////////////////////////////
/// @brief last http return code
////////////////////////////////////////////////////////////////////////////////

      int _lastHttpReturnCode;

////////////////////////////////////////////////////////////////////////////////
/// @brief last error message
////////////////////////////////////////////////////////////////////////////////

      std::string _lastErrorMessage;

////////////////////////////////////////////////////////////////////////////////
/// @brief underlying client
////////////////////////////////////////////////////////////////////////////////

      triagens::httpclient::SimpleHttpClient* _client;

////////////////////////////////////////////////////////////////////////////////
/// @brief last result
////////////////////////////////////////////////////////////////////////////////

      triagens::httpclient::SimpleHttpResult* _httpResult;
    };
  }
}

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
