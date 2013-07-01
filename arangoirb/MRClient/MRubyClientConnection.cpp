////////////////////////////////////////////////////////////////////////////////
/// @brief mruby client connection
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
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "MRubyClientConnection.h"

#include <sstream>

#include "Basics/StringUtils.h"
#include "BasicsC/json.h"
#include "BasicsC/tri-strings.h"
#include "Rest/HttpRequest.h"
#include "Rest/HttpResponse.h"
#include "SimpleHttpClient/GeneralClientConnection.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/SimpleHttpResult.h"

extern "C" {
#include "mruby/array.h"
#include "mruby/hash.h"
}

using namespace triagens::basics;
using namespace triagens::httpclient;
using namespace triagens::mrclient;
using namespace triagens::rest;
using namespace std;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8ClientConnection
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

MRubyClientConnection::MRubyClientConnection (mrb_state* mrb,
                                              Endpoint* endpoint,
                                              const string& username,
                                              const string& password,
                                              double requestTimeout,
                                              double connectionTimeout,
                                              size_t numRetries,
                                              bool warn)
  : _mrb(mrb),
    _connection(0),
    _lastHttpReturnCode(0),
    _lastErrorMessage(""),
    _client(0),
    _httpResult(0) {

  _connection = GeneralClientConnection::factory(endpoint, connectionTimeout, requestTimeout, numRetries);
  if (_connection == 0) {
    throw "out of memory";
  }

  _client = new SimpleHttpClient(_connection, requestTimeout, warn);
  _client->setUserNamePassword("/", username, password);

  // connect to server and get version number
  map<string, string> headerFields;
  SimpleHttpResult* result = _client->request(HttpRequest::HTTP_REQUEST_GET, "/_api/version", 0, 0, headerFields);

  if (!result->isComplete()) {
    // save error message
    _lastErrorMessage = _client->getErrorMessage();
    _lastHttpReturnCode = 500;
  }
  else {
    _lastHttpReturnCode = result->getHttpReturnCode();

    if (result->getHttpReturnCode() == HttpResponse::OK) {

      // default value
      _version = "arango";

      // convert response body to json
      TRI_json_t* json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, result->getBody().str().c_str());

      if (json) {

        // look up "server" value (this returns a pointer, not a copy)
        TRI_json_t* server = TRI_LookupArrayJson(json, "server");

        if (server) {

          // "server" value is a string and content is "arango"
          if (server->_type == TRI_JSON_STRING && TRI_EqualString(server->_value._string.data, "arango")) {

            // look up "version" value (this returns a pointer, not a copy)
            TRI_json_t* vs = TRI_LookupArrayJson(json, "version");

            if (vs) {

              // "version" value is a string
              if (vs->_type == TRI_JSON_STRING) {
                _version = string(vs->_value._string.data, vs->_value._string.length);
              }
            }
          }

          // must not free server and vs, they are contained in the "json" variable and freed below
        }

        TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
      }
    }
  }

  delete result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

MRubyClientConnection::~MRubyClientConnection () {
  if (_httpResult) {
    delete _httpResult;
  }

  if (_client) {
    delete _client;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup MRubyClientConnection
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief returns true if it is connected
////////////////////////////////////////////////////////////////////////////////

bool MRubyClientConnection::isConnected () {
  return _connection->isConnected();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the version and build number of the arango server
////////////////////////////////////////////////////////////////////////////////

const string& MRubyClientConnection::getVersion () {
  return _version;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the last http return code
////////////////////////////////////////////////////////////////////////////////

int MRubyClientConnection::getLastHttpReturnCode () {
  return _lastHttpReturnCode;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the last error message
////////////////////////////////////////////////////////////////////////////////

const std::string& MRubyClientConnection::getErrorMessage () {
  return _lastErrorMessage;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the simple http client
////////////////////////////////////////////////////////////////////////////////

triagens::httpclient::SimpleHttpClient* MRubyClientConnection::getHttpClient() {
  return _client;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief do a "GET" request
////////////////////////////////////////////////////////////////////////////////

mrb_value MRubyClientConnection::getData (std::string const& location,
                                          map<string, string> const& headerFields) {
  return requestData(HttpRequest::HTTP_REQUEST_GET, location, "", headerFields);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief do a "DELETE" request
////////////////////////////////////////////////////////////////////////////////

mrb_value MRubyClientConnection::deleteData (std::string const& location,
                                             map<string, string> const& headerFields) {
  return requestData(HttpRequest::HTTP_REQUEST_DELETE, location, "", headerFields);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief do a "HEAD" request
////////////////////////////////////////////////////////////////////////////////

mrb_value MRubyClientConnection::headData (std::string const& location,
                                           map<string, string> const& headerFields) {
  return requestData(HttpRequest::HTTP_REQUEST_HEAD, location, "", headerFields);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief do a "POST" request
////////////////////////////////////////////////////////////////////////////////

mrb_value MRubyClientConnection::postData (std::string const& location,
                                           std::string const& body,
                                           map<string, string> const& headerFields) {
  return requestData(HttpRequest::HTTP_REQUEST_POST, location, body, headerFields);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief do a "PUT" request
////////////////////////////////////////////////////////////////////////////////

mrb_value MRubyClientConnection::putData (std::string const& location,
                                          std::string const& body,
                                          map<string, string> const& headerFields) {
  return requestData(HttpRequest::HTTP_REQUEST_PUT, location, body, headerFields);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup MRubyClientConnection
/// @{
////////////////////////////////////////////////////////////////////////////////

mrb_value MRubyClientConnection::requestData (HttpRequest::HttpRequestType method,
                                              string const& location,
                                              string const& body,
                                              map<string, string> const& headerFields) {
  MR_state_t* mrs;

  mrs = (MR_state_t*) _mrb->ud;

  _lastErrorMessage = "";
  _lastHttpReturnCode = 0;

  if (_httpResult) {
    delete _httpResult;
  }

  if (body.empty()) {
    _httpResult = _client->request(method, location, 0, 0, headerFields);
  }
  else {
    _httpResult = _client->request(method, location, body.c_str(), body.length(), headerFields);
  }

  if (!_httpResult->isComplete()) {
    // not complete
    _lastErrorMessage = _client->getErrorMessage();

    if (_lastErrorMessage.empty()) {
      _lastErrorMessage = "Unknown error";
    }

    _lastHttpReturnCode = HttpResponse::SERVER_ERROR;

    mrb_value result = mrb_hash_new_capa(_mrb, 2);

    mrb_hash_set(_mrb, result, mrs->_errorSym, mrb_true_value());
    mrb_hash_set(_mrb, result, mrs->_codeSym, mrb_fixnum_value(HttpResponse::SERVER_ERROR));

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

    mrb_hash_set(_mrb, result, mrs->_errorNumSym, mrb_fixnum_value(errorNumber));
    mrb_hash_set(_mrb,
                 result,
                 mrs->_errorMessageSym,
                 mrb_str_new(_mrb, _lastErrorMessage.c_str(), _lastErrorMessage.length()));

    return result;
  }
  else {
    // complete
    _lastHttpReturnCode = _httpResult->getHttpReturnCode();

    // got a body
    if (_httpResult->getBody().str().length() > 0) {
      string contentType = _httpResult->getContentType(true);

      if (contentType == "application/json") {
        TRI_json_t* js = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, _httpResult->getBody().str().c_str());

        if (js != NULL) {
          // return v8 object
          mrb_value result = MR_ObjectJson(_mrb, js);
          TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, js);

          return result;
        }
      }

      // return body as string
      mrb_value result = mrb_str_new(_mrb,
                                     _httpResult->getBody().str().c_str(),
                                     _httpResult->getBody().str().length());

      return result;
    }
    else {
      // no body
      // this should not happen
      mrb_value result;

      mrb_hash_set(_mrb, result, mrs->_errorSym, mrb_false_value());
      mrb_hash_set(_mrb, result, mrs->_codeSym, mrb_fixnum_value(_httpResult->getHttpReturnCode()));

      return result;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
