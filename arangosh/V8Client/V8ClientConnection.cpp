////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#include "V8ClientConnection.h"

#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Rest/GeneralRequest.h"
#include "Rest/GeneralResponse.h"
#include "SimpleHttpClient/GeneralClientConnection.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/SimpleHttpResult.h"
#include "V8/v8-json.h"
#include "V8/v8-globals.h"

using namespace arangodb::basics;
using namespace arangodb::httpclient;
using namespace arangodb::rest;
using namespace arangodb::v8client;
using namespace std;

V8ClientConnection::V8ClientConnection(
    Endpoint* endpoint, std::string databaseName, std::string const& username,
    std::string const& password, double requestTimeout, double connectTimeout,
    size_t numRetries, uint32_t sslProtocol, bool warn)
    : _connection(nullptr),
      _databaseName(databaseName),
      _lastHttpReturnCode(0),
      _lastErrorMessage(""),
      _client(nullptr),
      _httpResult(nullptr) {
  _connection = GeneralClientConnection::factory(
      endpoint, requestTimeout, connectTimeout, numRetries, sslProtocol);

  if (_connection == nullptr) {
    throw "out of memory";
  }

  _client = new SimpleHttpClient(_connection, requestTimeout, warn);

  if (_client == nullptr) {
    LOG(FATAL) << "out of memory"; FATAL_ERROR_EXIT();
  }

  _client->setLocationRewriter(this, &rewriteLocation);
  _client->setUserNamePassword("/", username, password);

  // connect to server and get version number
  std::map<std::string, std::string> headerFields;
  std::unique_ptr<SimpleHttpResult> result(
      _client->request(GeneralRequest::HTTP_REQUEST_GET,
                       "/_api/version?details=true", nullptr, 0, headerFields));

  if (result == nullptr || !result->isComplete()) {
    // save error message
    _lastErrorMessage = _client->getErrorMessage();
    _lastHttpReturnCode = 500;
  } else {
    _lastHttpReturnCode = result->getHttpReturnCode();

    if (result->getHttpReturnCode() == GeneralResponse::OK) {
      // default value
      _version = "arango";
      _mode = "unknown mode";

      try {
        std::shared_ptr<VPackBuilder> parsedBody = result->getBodyVelocyPack();
        VPackSlice const body = parsedBody->slice();
        std::string const server =
            arangodb::basics::VelocyPackHelper::getStringValue(body, "server",
                                                               "");

        // "server" value is a string and content is "arango"
        if (server == "arango") {
          // look up "version" value
          _version = arangodb::basics::VelocyPackHelper::getStringValue(
              body, "version", "");
          VPackSlice const details = body.get("details");
          if (details.isObject()) {
            VPackSlice const mode = details.get("mode");
            if (mode.isString()) {
              _mode = mode.copyString();
            }
          }
        }
      } catch (...) {
        // Ignore all parse errors
      }
    } else {
      // initial request for /_api/version returned some non-HTTP 200 response.
      // now set up an error message
      _lastErrorMessage = _client->getErrorMessage();

      if (result->getHttpReturnCode() > 0) {
        _lastErrorMessage = StringUtils::itoa(result->getHttpReturnCode()) +
                            ": " + result->getHttpReturnMessage();
      }
    }
  }
}

V8ClientConnection::~V8ClientConnection() {
  delete _httpResult;
  delete _client;
  delete _connection;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief request location rewriter (injects database name)
////////////////////////////////////////////////////////////////////////////////

std::string V8ClientConnection::rewriteLocation(void* data,
                                                std::string const& location) {
  V8ClientConnection* c = static_cast<V8ClientConnection*>(data);

  TRI_ASSERT(c != nullptr);

  if (c->_databaseName.empty()) {
    // no database name provided
    return location;
  }

  if (location[0] == '/') {
    if (location.size() >= 5 && location[1] == '_' && location[2] == 'd' &&
        location[3] == 'b' && location[4] == '/') {
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

bool V8ClientConnection::isConnected() { return _connection->isConnected(); }

////////////////////////////////////////////////////////////////////////////////
/// @brief set the current operation to interrupted
////////////////////////////////////////////////////////////////////////////////

void V8ClientConnection::setInterrupted(bool value) {
  _connection->setInterrupted(value);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the current database name
////////////////////////////////////////////////////////////////////////////////

std::string const& V8ClientConnection::getDatabaseName() {
  return _databaseName;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set the current database name
////////////////////////////////////////////////////////////////////////////////

void V8ClientConnection::setDatabaseName(std::string const& databaseName) {
  _databaseName = databaseName;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the version and build number of the arango server
////////////////////////////////////////////////////////////////////////////////

std::string const& V8ClientConnection::getVersion() { return _version; }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the server mode
////////////////////////////////////////////////////////////////////////////////

std::string const& V8ClientConnection::getMode() { return _mode; }

////////////////////////////////////////////////////////////////////////////////
/// @brief get the last http return code
////////////////////////////////////////////////////////////////////////////////

int V8ClientConnection::getLastHttpReturnCode() { return _lastHttpReturnCode; }

////////////////////////////////////////////////////////////////////////////////
/// @brief get the last error message
////////////////////////////////////////////////////////////////////////////////

std::string const& V8ClientConnection::getErrorMessage() {
  return _lastErrorMessage;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the simple http client
////////////////////////////////////////////////////////////////////////////////

arangodb::httpclient::SimpleHttpClient* V8ClientConnection::getHttpClient() {
  return _client;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief do a "GET" request
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Value> V8ClientConnection::getData(
    v8::Isolate* isolate, std::string const& location,
    std::map<std::string, std::string> const& headerFields, bool raw) {
  if (raw) {
    return requestDataRaw(isolate, GeneralRequest::HTTP_REQUEST_GET, location, "",
                          headerFields);
  }
  return requestData(isolate, GeneralRequest::HTTP_REQUEST_GET, location, "",
                     headerFields);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief do a "DELETE" request
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Value> V8ClientConnection::deleteData(
    v8::Isolate* isolate, std::string const& location,
    std::map<std::string, std::string> const& headerFields, bool raw) {
  if (raw) {
    return requestDataRaw(isolate, GeneralRequest::HTTP_REQUEST_DELETE, location,
                          "", headerFields);
  }
  return requestData(isolate, GeneralRequest::HTTP_REQUEST_DELETE, location, "",
                     headerFields);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief do a "HEAD" request
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Value> V8ClientConnection::headData(
    v8::Isolate* isolate, std::string const& location,
    std::map<std::string, std::string> const& headerFields, bool raw) {
  if (raw) {
    return requestDataRaw(isolate, GeneralRequest::HTTP_REQUEST_HEAD, location, "",
                          headerFields);
  }
  return requestData(isolate, GeneralRequest::HTTP_REQUEST_HEAD, location, "",
                     headerFields);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief do an "OPTIONS" request
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Value> V8ClientConnection::optionsData(
    v8::Isolate* isolate, std::string const& location, std::string const& body,
    std::map<std::string, std::string> const& headerFields, bool raw) {
  if (raw) {
    return requestDataRaw(isolate, GeneralRequest::HTTP_REQUEST_OPTIONS, location,
                          body, headerFields);
  }
  return requestData(isolate, GeneralRequest::HTTP_REQUEST_OPTIONS, location, body,
                     headerFields);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief do a "POST" request
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Value> V8ClientConnection::postData(
    v8::Isolate* isolate, std::string const& location, std::string const& body,
    std::map<std::string, std::string> const& headerFields, bool raw) {
  if (raw) {
    return requestDataRaw(isolate, GeneralRequest::HTTP_REQUEST_POST, location,
                          body, headerFields);
  }
  return requestData(isolate, GeneralRequest::HTTP_REQUEST_POST, location, body,
                     headerFields);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief do a "POST" request
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Value> V8ClientConnection::postData(
    v8::Isolate* isolate, std::string const& location, char const* body,
    size_t bodySize, std::map<std::string, std::string> const& headerFields) {
  return requestData(isolate, GeneralRequest::HTTP_REQUEST_POST, location, body,
                     bodySize, headerFields);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief do a "PUT" request
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Value> V8ClientConnection::putData(
    v8::Isolate* isolate, std::string const& location, std::string const& body,
    std::map<std::string, std::string> const& headerFields, bool raw) {
  if (raw) {
    return requestDataRaw(isolate, GeneralRequest::HTTP_REQUEST_PUT, location,
                          body, headerFields);
  }
  return requestData(isolate, GeneralRequest::HTTP_REQUEST_PUT, location, body,
                     headerFields);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief do a "PATCH" request
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Value> V8ClientConnection::patchData(
    v8::Isolate* isolate, std::string const& location, std::string const& body,
    std::map<std::string, std::string> const& headerFields, bool raw) {
  if (raw) {
    return requestDataRaw(isolate, GeneralRequest::HTTP_REQUEST_PATCH, location,
                          body, headerFields);
  }
  return requestData(isolate, GeneralRequest::HTTP_REQUEST_PATCH, location, body,
                     headerFields);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a request
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Value> V8ClientConnection::requestData(
    v8::Isolate* isolate, GeneralRequest::RequestType method,
    std::string const& location, char const* body, size_t bodySize,
    std::map<std::string, std::string> const& headerFields) {
  _lastErrorMessage = "";
  _lastHttpReturnCode = 0;

  delete _httpResult;
  _httpResult = nullptr;

  _httpResult =
      _client->request(method, location, body, bodySize, headerFields);

  return handleResult(isolate);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a request
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Value> V8ClientConnection::requestData(
    v8::Isolate* isolate, GeneralRequest::RequestType method,
    std::string const& location, std::string const& body,
    std::map<std::string, std::string> const& headerFields) {
  _lastErrorMessage = "";
  _lastHttpReturnCode = 0;

  delete _httpResult;
  _httpResult = nullptr;

  if (body.empty()) {
    _httpResult = _client->request(method, location, nullptr, 0, headerFields);
  } else {
    _httpResult = _client->request(method, location, body.c_str(),
                                   body.length(), headerFields);
  }

  return handleResult(isolate);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief handles a result
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Value> V8ClientConnection::handleResult(v8::Isolate* isolate) {
  v8::EscapableHandleScope scope(isolate);

  if (_httpResult == nullptr) {
    return scope.Escape<v8::Value>(v8::Undefined(isolate));
  }

  if (!_httpResult->isComplete()) {
    // not complete
    _lastErrorMessage = _client->getErrorMessage();

    if (_lastErrorMessage.empty()) {
      _lastErrorMessage = "Unknown error";
    }

    _lastHttpReturnCode = GeneralResponse::SERVER_ERROR;

    v8::Handle<v8::Object> result = v8::Object::New(isolate);
    result->ForceSet(TRI_V8_ASCII_STRING("error"),
                     v8::Boolean::New(isolate, true));
    result->ForceSet(TRI_V8_ASCII_STRING("code"),
                     v8::Integer::New(isolate, GeneralResponse::SERVER_ERROR));

    int errorNumber = 0;

    switch (_httpResult->getResultType()) {
      case (SimpleHttpResult::COULD_NOT_CONNECT):
        errorNumber = TRI_SIMPLE_CLIENT_COULD_NOT_CONNECT;
        break;

      case (SimpleHttpResult::READ_ERROR):
        errorNumber = TRI_SIMPLE_CLIENT_COULD_NOT_READ;
        break;

      case (SimpleHttpResult::WRITE_ERROR):
        errorNumber = TRI_SIMPLE_CLIENT_COULD_NOT_WRITE;
        break;

      default:
        errorNumber = TRI_SIMPLE_CLIENT_UNKNOWN_ERROR;
        break;
    }

    result->ForceSet(TRI_V8_ASCII_STRING("errorNum"),
                     v8::Integer::New(isolate, errorNumber));
    result->ForceSet(TRI_V8_ASCII_STRING("errorMessage"),
                     TRI_V8_STD_STRING(_lastErrorMessage));

    return scope.Escape<v8::Value>(result);
  }

  // complete

  _lastHttpReturnCode = _httpResult->getHttpReturnCode();

  // got a body
  StringBuffer& sb = _httpResult->getBody();

  if (sb.length() > 0) {
    isolate->GetCurrentContext()->Global();

    if (_httpResult->isJson()) {
      return scope.Escape<v8::Value>(
          TRI_FromJsonString(isolate, sb.c_str(), nullptr));
    }

    // return body as string
    return scope.Escape<v8::Value>(TRI_V8_STD_STRING(sb));
  }

  // no body

  v8::Handle<v8::Object> result = v8::Object::New(isolate);

  result->ForceSet(TRI_V8_ASCII_STRING("code"),
                   v8::Integer::New(isolate, _lastHttpReturnCode));

  if (_lastHttpReturnCode >= 400) {
    std::string returnMessage(_httpResult->getHttpReturnMessage());

    result->ForceSet(TRI_V8_ASCII_STRING("error"),
                     v8::Boolean::New(isolate, true));
    result->ForceSet(TRI_V8_ASCII_STRING("errorNum"),
                     v8::Integer::New(isolate, _lastHttpReturnCode));
    result->ForceSet(TRI_V8_ASCII_STRING("errorMessage"),
                     TRI_V8_STD_STRING(returnMessage));
  } else {
    result->ForceSet(TRI_V8_ASCII_STRING("error"),
                     v8::Boolean::New(isolate, false));
  }

  return scope.Escape<v8::Value>(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a request and returns raw response
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Value> V8ClientConnection::requestDataRaw(
    v8::Isolate* isolate, GeneralRequest::RequestType method,
    std::string const& location, std::string const& body,
    std::map<std::string, std::string> const& headerFields) {
  v8::EscapableHandleScope scope(isolate);

  _lastErrorMessage = "";
  _lastHttpReturnCode = 0;

  delete _httpResult;
  _httpResult = nullptr;

  if (body.empty()) {
    _httpResult = _client->request(method, location, nullptr, 0, headerFields);
  } else {
    _httpResult = _client->request(method, location, body.c_str(),
                                   body.length(), headerFields);
  }

  if (!_httpResult->isComplete()) {
    // not complete
    _lastErrorMessage = _client->getErrorMessage();

    if (_lastErrorMessage.empty()) {
      _lastErrorMessage = "Unknown error";
    }

    _lastHttpReturnCode = GeneralResponse::SERVER_ERROR;

    v8::Handle<v8::Object> result = v8::Object::New(isolate);
    result->ForceSet(TRI_V8_ASCII_STRING("code"),
                     v8::Integer::New(isolate, GeneralResponse::SERVER_ERROR));

    int errorNumber = 0;

    switch (_httpResult->getResultType()) {
      case (SimpleHttpResult::COULD_NOT_CONNECT):
        errorNumber = TRI_SIMPLE_CLIENT_COULD_NOT_CONNECT;
        break;

      case (SimpleHttpResult::READ_ERROR):
        errorNumber = TRI_SIMPLE_CLIENT_COULD_NOT_READ;
        break;

      case (SimpleHttpResult::WRITE_ERROR):
        errorNumber = TRI_SIMPLE_CLIENT_COULD_NOT_WRITE;
        break;

      default:
        errorNumber = TRI_SIMPLE_CLIENT_UNKNOWN_ERROR;
        break;
    }

    result->ForceSet(TRI_V8_ASCII_STRING("errorNum"),
                     v8::Integer::New(isolate, errorNumber));
    result->ForceSet(TRI_V8_ASCII_STRING("errorMessage"),
                     TRI_V8_STD_STRING(_lastErrorMessage));

    return scope.Escape<v8::Value>(result);
  }

  // complete

  _lastHttpReturnCode = _httpResult->getHttpReturnCode();

  // create raw response
  v8::Handle<v8::Object> result = v8::Object::New(isolate);

  result->ForceSet(TRI_V8_ASCII_STRING("code"),
                   v8::Integer::New(isolate, _lastHttpReturnCode));

  if (_lastHttpReturnCode >= 400) {
    std::string returnMessage(_httpResult->getHttpReturnMessage());

    result->ForceSet(TRI_V8_ASCII_STRING("error"),
                     v8::Boolean::New(isolate, true));
    result->ForceSet(TRI_V8_ASCII_STRING("errorNum"),
                     v8::Integer::New(isolate, _lastHttpReturnCode));
    result->ForceSet(TRI_V8_ASCII_STRING("errorMessage"),
                     TRI_V8_STD_STRING(returnMessage));
  } else {
    result->ForceSet(TRI_V8_ASCII_STRING("error"),
                     v8::Boolean::New(isolate, false));
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
