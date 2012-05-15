////////////////////////////////////////////////////////////////////////////////
/// @brief v8 client connection
///
/// @file
///
/// DISCLAIMER
///
/// Copyright by triAGENS GmbH - All rights reserved.
///
/// The Programs (which include both the software and documentation)
/// contain proprietary information of triAGENS GmbH; they are
/// provided under a license agreement containing restrictions on use and
/// disclosure and are also protected by copyright, patent and other
/// intellectual and industrial property laws. Reverse engineering,
/// disassembly or decompilation of the Programs, except to the extent
/// required to obtain interoperability with other independently created
/// software or as specified by law, is prohibited.
///
/// The Programs are not intended for use in any nuclear, aviation, mass
/// transit, medical, or other inherently dangerous applications. It shall
/// be the licensee's responsibility to take all appropriate fail-safe,
/// backup, redundancy, and other measures to ensure the safe use of such
/// applications if the Programs are used for such purposes, and triAGENS
/// GmbH disclaims liability for any damages caused by such use of
/// the Programs.
///
/// This software is the confidential and proprietary information of
/// triAGENS GmbH. You shall not disclose such confidential and
/// proprietary information and shall use it only in accordance with the
/// terms of the license agreement you entered into with triAGENS GmbH.
///
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Achim Brandt
/// @author Copyright 2008-2011, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "V8ClientConnection.h"

#include <sstream>

#include "Basics/StringUtils.h"

#include "Variant/VariantArray.h"
#include "Variant/VariantString.h"

#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/SimpleHttpResult.h"

#include "json.h"
#include "V8/v8-conv.h"

using namespace triagens::basics;
using namespace triagens::httpclient;
using namespace std;

namespace triagens {
  namespace v8client {

    ////////////////////////////////////////////////////////////////////////////////
    /// constructor and destructor
    ////////////////////////////////////////////////////////////////////////////////

    V8ClientConnection::V8ClientConnection (const std::string& hostname,
            int port,
            double requestTimeout,
            size_t retries,
            double connectionTimeout) : _client(0), _httpResult(0) {
      
      _connected = false;
      _lastHttpReturnCode = 0;
      _lastErrorMessage = "";
      _client = new SimpleHttpClient(hostname, port, requestTimeout, retries, connectionTimeout);

      // connect to server and get version number
      map<string, string> headerFields;
      SimpleHttpResult* result = _client->request(SimpleHttpClient::GET, "/_admin/version", 0, 0, headerFields);

      if (!result->isComplete()) {
        // save error message
        _lastErrorMessage = _client->getErrorMessage();
        _lastHttpReturnCode = 500;
      }
      else {
        _lastHttpReturnCode = result->getHttpReturnCode();
        if (result->getHttpReturnCode() == SimpleHttpResult::HTTP_STATUS_OK) {

          triagens::basics::VariantArray* json = result->getBodyAsVariantArray();
          if (json) {
            triagens::basics::VariantString* vs = json->lookupString("server");
            if (vs && vs->getValue() == "arango") {
              // connected to arango server
              _connected = true;
              vs = json->lookupString("version");
              if (vs) {
                _version = vs->getValue();
              }
            }
            delete json;
          }
        }        
      }
    
      delete result; 
    }

    V8ClientConnection::~V8ClientConnection () {
      if (_httpResult) {
        delete _httpResult;
      }

      if (_client) {
        delete _client;
      }
    }

    v8::Handle<v8::Value> V8ClientConnection::getData (std::string const& location, map<string, string> const& headerFields) {
      return requestData(SimpleHttpClient::GET, location, "", headerFields);
    }
    
    v8::Handle<v8::Value> V8ClientConnection::deleteData (std::string const& location, map<string, string> const& headerFields) {
      return requestData(SimpleHttpClient::DELETE, location, "", headerFields);
    }
    
    v8::Handle<v8::Value> V8ClientConnection::headData (std::string const& location, map<string, string> const& headerFields) {
      return requestData(SimpleHttpClient::HEAD, location, "", headerFields);
    }    

    v8::Handle<v8::Value> V8ClientConnection::postData (std::string const& location, std::string const& body, map<string, string> const& headerFields) {
      return requestData(SimpleHttpClient::POST, location, body, headerFields);
    }
    
    v8::Handle<v8::Value> V8ClientConnection::putData (std::string const& location, std::string const& body, map<string, string> const& headerFields) {
      return requestData(SimpleHttpClient::PUT, location, body, headerFields);
    }

    const std::string& V8ClientConnection::getHostname () {
      return _client->getHostname();
    }

    int V8ClientConnection::getPort () {
      return _client->getPort();
    }
    
    v8::Handle<v8::Value> V8ClientConnection::requestData (int method, string const& location, string const& body, map<string, string> const& headerFields) {
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
        
        if (_lastErrorMessage == "") {
          _lastErrorMessage = "Unknown error";
        }
        
        _lastHttpReturnCode = SimpleHttpResult::HTTP_STATUS_SERVER_ERROR;
        
        v8::Handle<v8::Object> result = v8::Object::New();
        result->Set(v8::String::New("error"), v8::Boolean::New(true));        
        result->Set(v8::String::New("code"), v8::Integer::New(SimpleHttpResult::HTTP_STATUS_SERVER_ERROR));
                
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
        }        
        
        result->Set(v8::String::New("errorNum"), v8::Integer::New(errorNumber));
        result->Set(v8::String::New("errorMessage"), v8::String::New(_lastErrorMessage.c_str(), _lastErrorMessage.length()));        
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
              v8::Handle<v8::Value> result = TRI_ObjectJson(js);
              TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, js);
              return result;
            }
          }

          // return body as string
          v8::Handle<v8::String> result = v8::String::New(_httpResult->getBody().str().c_str(), _httpResult->getBody().str().length());
          return result;
        }
        else {
          // no body 
          // this should not happen
          v8::Handle<v8::Object> result;        
          result->Set(v8::String::New("error"), v8::Boolean::New(false));
          result->Set(v8::String::New("code"), v8::Integer::New(_httpResult->getHttpReturnCode()));
          return result;
        }        
      }      
    }
    
  }
}
