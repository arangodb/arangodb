////////////////////////////////////////////////////////////////////////////////
/// @brief v8 client connection
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

#include "V8ClientConnection.h"

#include <sstream>

#include "Basics/JsonHelper.h"
#include "Basics/StringUtils.h"
#include "Basics/json.h"
#include "Basics/tri-strings.h"
#include "Rest/HttpRequest.h"
#include "Rest/HttpResponse.h"
#include "SimpleHttpClient/GeneralClientConnection.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/SimpleHttpResult.h"
#include "V8/v8-conv.h"
#include "V8/v8-json.h"
#include "V8/v8-globals.h"

using namespace triagens::basics;
using namespace triagens::httpclient;
using namespace triagens::rest;
using namespace triagens::v8client;
using namespace std;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

V8ClientConnection::V8ClientConnection (Endpoint* endpoint,
                                        std::string databaseName,
                                        std::string const& username,
                                        std::string const& password,
                                        double requestTimeout,
                                        double connectTimeout,
                                        size_t numRetries,
                                        uint32_t sslProtocol,
                                        bool warn)
  : _connection(nullptr),
    _databaseName(databaseName),
    _lastHttpReturnCode(0),
    _lastErrorMessage(""),
    _client(nullptr),
    _httpResult(nullptr) {

  _connection = GeneralClientConnection::factory(endpoint, requestTimeout, connectTimeout, numRetries, sslProtocol);

  if (_connection == nullptr) {
    throw "out of memory";
  }

  _client = new SimpleHttpClient(_connection, requestTimeout, warn);

  if (_client == nullptr) {
    LOG_FATAL_AND_EXIT("out of memory");
  }

  _client->setLocationRewriter(this, &rewriteLocation);
  _client->setUserNamePassword("/", username, password);

  // connect to server and get version number
  map<string, string> headerFields;
  std::unique_ptr<SimpleHttpResult> result(_client->request(HttpRequest::HTTP_REQUEST_GET, "/_api/version?details=true", nullptr, 0, headerFields));

  if (result == nullptr || ! result->isComplete()) {
    // save error message
    _lastErrorMessage = _client->getErrorMessage();
    _lastHttpReturnCode = 500;
  }
  else {
    _lastHttpReturnCode = result->getHttpReturnCode();

    if (result->getHttpReturnCode() == HttpResponse::OK) {
      // default value
      _version = "arango";
      _mode = "unknown mode";

      // convert response body to json
      std::unique_ptr<TRI_json_t> json(TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, result->getBody().c_str()));

      if (json != nullptr) {
        // look up "server" value
        const string server = JsonHelper::getStringValue(json.get(), "server", "");

        // "server" value is a string and content is "arango"
        if (server == "arango") {
          // look up "version" value
          _version = JsonHelper::getStringValue(json.get(), "version", "");
          auto const* details = TRI_LookupObjectJson(json.get(), "details");

          if (TRI_IsObjectJson(details)) {
            auto const* mode = TRI_LookupObjectJson(details, "mode");

            if (TRI_IsStringJson(mode)) {
              _mode = std::string(mode->_value._string.data, mode->_value._string.length - 1); 
            }
          }
        }
      }
    }
    else {
      // initial request for /_api/version returned some non-HTTP 200 response.
      // now set up an error message
      _lastErrorMessage = _client->getErrorMessage();

      if (result->getHttpReturnCode() > 0) {
        _lastErrorMessage = StringUtils::itoa(result->getHttpReturnCode()) + ": " + result->getHttpReturnMessage();
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

V8ClientConnection::~V8ClientConnection () {
  delete _httpResult;
  delete _client;
  delete _connection;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief request location rewriter (injects database name)
////////////////////////////////////////////////////////////////////////////////

string V8ClientConnection::rewriteLocation (void* data,
                                            std::string const& location) {
  V8ClientConnection* c = static_cast<V8ClientConnection*>(data);

  TRI_ASSERT(c != nullptr);

  if (c->_databaseName.empty()) {
    // no database name provided
    return location;
  }

  if (location[0] == '/') {
    if (location.size() >= 5 && 
        location[1] == '_' &&
        location[2] == 'd' &&
        location[3] == 'b' &&
        location[4] == '/') {
      // location already contains /_db/
      return location;
    }

    return "/_db/" + c->_databaseName + location;
  }

  return "/_db/" + c->_databaseName + "/" + location;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns true if it is connected
////////////////////////////////////////////////////////////////////////////////

bool V8ClientConnection::isConnected () {
  return _connection->isConnected();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set the current operation to interrupted
////////////////////////////////////////////////////////////////////////////////

void V8ClientConnection::setInterrupted (bool value) {
  _connection->setInterrupted(value);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the current database name
////////////////////////////////////////////////////////////////////////////////

std::string const& V8ClientConnection::getDatabaseName () {
  return _databaseName;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set the current database name
////////////////////////////////////////////////////////////////////////////////

void V8ClientConnection::setDatabaseName (std::string const& databaseName) {
  _databaseName = databaseName;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the version and build number of the arango server
////////////////////////////////////////////////////////////////////////////////

std::string const& V8ClientConnection::getVersion () {
  return _version;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the server mode
////////////////////////////////////////////////////////////////////////////////

std::string const& V8ClientConnection::getMode () {
  return _mode;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the last http return code
////////////////////////////////////////////////////////////////////////////////

int V8ClientConnection::getLastHttpReturnCode () {
  return _lastHttpReturnCode;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the last error message
////////////////////////////////////////////////////////////////////////////////

std::string const& V8ClientConnection::getErrorMessage () {
  return _lastErrorMessage;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the simple http client
////////////////////////////////////////////////////////////////////////////////

triagens::httpclient::SimpleHttpClient* V8ClientConnection::getHttpClient() {
  return _client;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief do a "GET" request
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Value> V8ClientConnection::getData (v8::Isolate* isolate,
                                                   std::string const& location,
                                                   map<string, string> const& headerFields,
                                                   bool raw) {
  if (raw) {
    return requestDataRaw(isolate, HttpRequest::HTTP_REQUEST_GET, location, "", headerFields);
  }
  return requestData(isolate, HttpRequest::HTTP_REQUEST_GET, location, "", headerFields);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief do a "DELETE" request
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Value> V8ClientConnection::deleteData (v8::Isolate* isolate,
                                                      std::string const& location,
                                                      map<string, string> const& headerFields,
                                                      bool raw) {
  if (raw) {
    return requestDataRaw(isolate, HttpRequest::HTTP_REQUEST_DELETE, location, "", headerFields);
  }
  return requestData(isolate, HttpRequest::HTTP_REQUEST_DELETE, location, "", headerFields);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief do a "HEAD" request
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Value> V8ClientConnection::headData (v8::Isolate* isolate,
                                                    std::string const& location,
                                                    map<string, string> const& headerFields,
                                                    bool raw) {
  if (raw) {
    return requestDataRaw(isolate, HttpRequest::HTTP_REQUEST_HEAD, location, "", headerFields);
  }
  return requestData(isolate, HttpRequest::HTTP_REQUEST_HEAD, location, "", headerFields);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief do an "OPTIONS" request
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Value> V8ClientConnection::optionsData (v8::Isolate* isolate,
                                                       std::string const& location,
                                                       std::string const& body,
                                                       map<string, string> const& headerFields,
                                                       bool raw) {
  if (raw) {
    return requestDataRaw(isolate, HttpRequest::HTTP_REQUEST_OPTIONS, location, body, headerFields);
  }
  return requestData(isolate, HttpRequest::HTTP_REQUEST_OPTIONS, location, body, headerFields);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief do a "POST" request
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Value> V8ClientConnection::postData (v8::Isolate* isolate,
                                                    std::string const& location,
                                                    std::string const& body,
                                                    map<string, string> const& headerFields,
                                                    bool raw) {
  if (raw) {
    return requestDataRaw(isolate, HttpRequest::HTTP_REQUEST_POST, location, body, headerFields);
  }
  return requestData(isolate, HttpRequest::HTTP_REQUEST_POST, location, body, headerFields);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief do a "POST" request
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Value> V8ClientConnection::postData (v8::Isolate* isolate,
                                                    std::string const& location,
                                                    const char* body,
                                                    const size_t bodySize,
                                                    map<string, string> const& headerFields) {
  return requestData(isolate, HttpRequest::HTTP_REQUEST_POST, location, body, bodySize, headerFields);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief do a "PUT" request
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Value> V8ClientConnection::putData (v8::Isolate* isolate,
                                                   std::string const& location,
                                                   std::string const& body,
                                                   map<string, string> const& headerFields,
                                                   bool raw) {
  if (raw) {
    return requestDataRaw(isolate, HttpRequest::HTTP_REQUEST_PUT, location, body, headerFields);
  }
  return requestData(isolate, HttpRequest::HTTP_REQUEST_PUT, location, body, headerFields);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief do a "PATCH" request
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Value> V8ClientConnection::patchData (v8::Isolate* isolate,
                                                     std::string const& location,
                                                     std::string const& body,
                                                     map<string, string> const& headerFields,
                                                     bool raw) {
  if (raw) {
    return requestDataRaw(isolate, HttpRequest::HTTP_REQUEST_PATCH, location, body, headerFields);
  }
  return requestData(isolate, HttpRequest::HTTP_REQUEST_PATCH, location, body, headerFields);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a request
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Value> V8ClientConnection::requestData (v8::Isolate* isolate,
                                                       HttpRequest::HttpRequestType method,
                                                       string const& location,
                                                       const char* body,
                                                       const size_t bodySize,
                                                       map<string, string> const& headerFields) {

  _lastErrorMessage = "";
  _lastHttpReturnCode = 0;

  delete _httpResult;
  _httpResult = nullptr;

  _httpResult = _client->request(method, location, body, bodySize, headerFields);

  return handleResult(isolate);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a request
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Value> V8ClientConnection::requestData (v8::Isolate* isolate,
                                                       HttpRequest::HttpRequestType method,
                                                       string const& location,
                                                       string const& body,
                                                       map<string, string> const& headerFields) {
  _lastErrorMessage = "";
  _lastHttpReturnCode = 0;

  delete _httpResult;
  _httpResult = nullptr;

  if (body.empty()) {
    _httpResult = _client->request(method, location, nullptr, 0, headerFields);
  }
  else {
    _httpResult = _client->request(method, location, body.c_str(), body.length(), headerFields);
  }

  return handleResult(isolate);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief handles a result
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Value> V8ClientConnection::handleResult (v8::Isolate* isolate) {
  v8::EscapableHandleScope scope(isolate);

  if (_httpResult == nullptr) {
    return scope.Escape<v8::Value>(v8::Undefined(isolate));
  }

  if (! _httpResult->isComplete()) {
    // not complete
    _lastErrorMessage = _client->getErrorMessage();

    if (_lastErrorMessage.empty()) {
      _lastErrorMessage = "Unknown error";
    }

    _lastHttpReturnCode = HttpResponse::SERVER_ERROR;

    v8::Handle<v8::Object> result = v8::Object::New(isolate);
    result->ForceSet(TRI_V8_ASCII_STRING("error"), v8::Boolean::New(isolate, true));
    result->ForceSet(TRI_V8_ASCII_STRING("code"),  v8::Integer::New(isolate, HttpResponse::SERVER_ERROR));

    int errorNumber = 0;

    switch (_httpResult->getResultType()) {
      case (SimpleHttpResult::COULD_NOT_CONNECT) :
        errorNumber = TRI_SIMPLE_CLIENT_COULD_NOT_CONNECT;
        break;

      case (SimpleHttpResult::READ_ERROR) :
        errorNumber = TRI_SIMPLE_CLIENT_COULD_NOT_READ;
        break;

      case (SimpleHttpResult::WRITE_ERROR) :
        errorNumber = TRI_SIMPLE_CLIENT_COULD_NOT_WRITE;
        break;

      default:
        errorNumber = TRI_SIMPLE_CLIENT_UNKNOWN_ERROR;
        break;
    }

    result->ForceSet(TRI_V8_ASCII_STRING("errorNum"),     v8::Integer::New(isolate, errorNumber));
    result->ForceSet(TRI_V8_ASCII_STRING("errorMessage"), TRI_V8_STD_STRING(_lastErrorMessage));

    return scope.Escape<v8::Value>(result);
  }

  // complete

  _lastHttpReturnCode = _httpResult->getHttpReturnCode();

  // got a body
  StringBuffer& sb = _httpResult->getBody();

  if (sb.length() > 0) {
    isolate->GetCurrentContext()->Global();

    if (_httpResult->isJson()) {
      return scope.Escape<v8::Value>(TRI_FromJsonString(isolate, sb.c_str(), nullptr));
    }

    // return body as string
    return scope.Escape<v8::Value>(TRI_V8_STD_STRING(sb));
  }

  // no body

  v8::Handle<v8::Object> result = v8::Object::New(isolate);

  result->ForceSet(TRI_V8_ASCII_STRING("code"), v8::Integer::New(isolate, _lastHttpReturnCode));

  if (_lastHttpReturnCode >= 400) {
    string returnMessage(_httpResult->getHttpReturnMessage());

    result->ForceSet(TRI_V8_ASCII_STRING("error"),        v8::Boolean::New(isolate, true));
    result->ForceSet(TRI_V8_ASCII_STRING("errorNum"),     v8::Integer::New(isolate, _lastHttpReturnCode));
    result->ForceSet(TRI_V8_ASCII_STRING("errorMessage"), TRI_V8_STD_STRING(returnMessage));
  }
  else {
    result->ForceSet(TRI_V8_ASCII_STRING("error"),        v8::Boolean::New(isolate, false));
  }

  return scope.Escape<v8::Value>(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a request and returns raw response
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Value> V8ClientConnection::requestDataRaw (v8::Isolate* isolate,
                                                          HttpRequest::HttpRequestType method,
                                                          string const& location,
                                                          string const& body,
                                                          map<string, string> const& headerFields) {
  v8::EscapableHandleScope scope(isolate);

  _lastErrorMessage = "";
  _lastHttpReturnCode = 0;

  delete _httpResult;
  _httpResult = nullptr;

  if (body.empty()) {
    _httpResult = _client->request(method, location, nullptr, 0, headerFields);
  }
  else {
    _httpResult = _client->request(method, location, body.c_str(), body.length(), headerFields);
  }

  if (! _httpResult->isComplete()) {
    // not complete
    _lastErrorMessage = _client->getErrorMessage();

    if (_lastErrorMessage.empty()) {
      _lastErrorMessage = "Unknown error";
    }

    _lastHttpReturnCode = HttpResponse::SERVER_ERROR;

    v8::Handle<v8::Object> result = v8::Object::New(isolate);
    result->ForceSet(TRI_V8_ASCII_STRING("code"), v8::Integer::New(isolate, HttpResponse::SERVER_ERROR));

    int errorNumber = 0;

    switch (_httpResult->getResultType()) {
      case (SimpleHttpResult::COULD_NOT_CONNECT) :
        errorNumber = TRI_SIMPLE_CLIENT_COULD_NOT_CONNECT;
        break;

      case (SimpleHttpResult::READ_ERROR) :
        errorNumber = TRI_SIMPLE_CLIENT_COULD_NOT_READ;
        break;

      case (SimpleHttpResult::WRITE_ERROR) :
        errorNumber = TRI_SIMPLE_CLIENT_COULD_NOT_WRITE;
        break;

      default:
        errorNumber = TRI_SIMPLE_CLIENT_UNKNOWN_ERROR;
        break;
    }

    result->ForceSet(TRI_V8_ASCII_STRING("errorNum"),     v8::Integer::New(isolate, errorNumber));
    result->ForceSet(TRI_V8_ASCII_STRING("errorMessage"), TRI_V8_STD_STRING(_lastErrorMessage));

    return scope.Escape<v8::Value>(result);
  }

  // complete

  _lastHttpReturnCode = _httpResult->getHttpReturnCode();

  // create raw response
  v8::Handle<v8::Object> result = v8::Object::New(isolate);

  result->ForceSet(TRI_V8_ASCII_STRING("code"), v8::Integer::New(isolate, _lastHttpReturnCode));

  if (_lastHttpReturnCode >= 400) {
    string returnMessage(_httpResult->getHttpReturnMessage());

    result->ForceSet(TRI_V8_ASCII_STRING("error"),        v8::Boolean::New(isolate, true));
    result->ForceSet(TRI_V8_ASCII_STRING("errorNum"),     v8::Integer::New(isolate, _lastHttpReturnCode));
    result->ForceSet(TRI_V8_ASCII_STRING("errorMessage"), TRI_V8_STD_STRING(returnMessage));
  }
  else {
    result->ForceSet(TRI_V8_ASCII_STRING("error"), v8::Boolean::New(isolate, false));
  }

  // got a body, copy it into the result
  StringBuffer& sb = _httpResult->getBody();
  if (sb.length() > 0) {
    v8::Handle<v8::String> b = TRI_V8_STD_STRING(sb);

    result->ForceSet(TRI_V8_ASCII_STRING("body"), b);
  }

  // copy all headers
  v8::Handle<v8::Object> headers = v8::Object::New(isolate);
  auto const& hf = _httpResult->getHeaderFields();

  for (auto const& it : hf) {
    v8::Handle<v8::String> key = TRI_V8_STD_STRING(it.first);
    v8::Handle<v8::String> val = TRI_V8_STD_STRING(it.second);

    headers->ForceSet(key, val);
  }

  result->ForceSet(TRI_V8_ASCII_STRING("headers"), headers);

  // and returns
  return scope.Escape<v8::Value>(result);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
