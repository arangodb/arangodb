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
            double connectionTimeout) : _client(0) {
      
      _connected = false;
      _lastHttpReturnCode = 0;
      _lastErrorMessage = "";
      _client = new SimpleHttpClient(hostname, port, requestTimeout, retries, connectionTimeout);

      // connect to server and get version number
      map<string, string> headerFields;
      SimpleHttpResult* result = _client->request(SimpleHttpClient::GET, "/version", 0, 0, headerFields);

      if (!result->isComplete()) {
        // save error message
        _lastErrorMessage = _client->getErrorMessage();
        _lastHttpReturnCode = 500;
      }
      else {
        _lastHttpReturnCode = result->getHttpReturnCode();
        if (result->getHttpReturnCode() == SimpleHttpClient::HTTP_STATUS_OK) {

          triagens::basics::VariantArray* json = result->getBodyAsVariantArray();
          if (json) {
            triagens::basics::VariantString* vs = json->lookupString("server");
            if (vs && vs->getValue() == "avocado") {
              // connected to avocado server
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
      if (_client) {
        delete _client;
      }
    }

    v8::Handle<v8::Value> V8ClientConnection::getData (std::string const& location, map<string, string> const& headerFields) {
      return sendData(SimpleHttpClient::GET, location, "", headerFields);
    }
    
    v8::Handle<v8::Value> V8ClientConnection::deleteData (std::string const& location, map<string, string> const& headerFields) {
      return sendData(SimpleHttpClient::DELETE, location, "", headerFields);
    }
    
    v8::Handle<v8::Value> V8ClientConnection::headData (std::string const& location, map<string, string> const& headerFields) {
      return sendData(SimpleHttpClient::HEAD, location, "", headerFields);
    }    

    v8::Handle<v8::Value> V8ClientConnection::postData (std::string const& location, std::string const& body, map<string, string> const& headerFields) {
      return sendData(SimpleHttpClient::POST, location, body, headerFields);
    }
    
    v8::Handle<v8::Value> V8ClientConnection::putData (std::string const& location, std::string const& body, map<string, string> const& headerFields) {
      return sendData(SimpleHttpClient::PUT, location, body, headerFields);
    }

    const std::string& V8ClientConnection::getHostname () {
      return _client->getHostname();
    }

    int V8ClientConnection::getPort () {
      return _client->getPort();
    }

    v8::Handle<v8::Value> V8ClientConnection::sendData (int method, string const& location, string const& body, map<string, string> const& headerFields) {
      _lastErrorMessage = "";
      _lastHttpReturnCode = 0;
      
      SimpleHttpResult* result = 0;
      
      if (body.empty()) {
        result = _client->request(method, location, 0, 0, headerFields);
      }
      else {
        result = _client->request(method, location, body.c_str(), body.length(), headerFields);
      }
      
      v8::Handle<v8::String> v8string;
      
      if (!result->isComplete()) {
        // save error message
        _lastErrorMessage = _client->getErrorMessage();
        
        stringstream message;
        message << "{";
        message << "\"error\":true,";
        message << "\"code\":" << SimpleHttpClient::HTTP_STATUS_SERVER_ERROR << ",";
        message << "\"errorNum\":" << result->getResultType() << ",";
        
        message << "\"errorMessage\":\"";
        switch (result->getResultType()) {
          case (SimpleHttpResult::COULD_NOT_CONNECT) :
            message << "Could not connect to server";
            break;
          case (SimpleHttpResult::READ_ERROR) :
            message << "Error while reading data from server";
            break;
          case (SimpleHttpResult::WRITE_ERROR) :
            message << "Error while reading data to server";
            break;
          default :
            message << "Unknown communication error";
            break;
        }
        message << "\"}";
        
        v8string =  v8::String::New(message.str().c_str());        
        _lastHttpReturnCode = SimpleHttpClient::HTTP_STATUS_SERVER_ERROR;
      }
      else {
        if (result->getBody().str().length() == 0) {
          // handle empty result
          result->getBody() << "{\"error\":true,";
          result->getBody() << "\"code\":" << result->getHttpReturnCode() << ",";
          result->getBody() << "\"errorNum\":" << result->getHttpReturnCode() << ",";        
          result->getBody() << "\"errorMessage\":\"No result.\"}";          
        }
        
        v8string =  v8::String::New(result->getBody().str().c_str());
        _lastHttpReturnCode = result->getHttpReturnCode();
      }
      
      delete result;
      
      // body should be json 
      return v8string;
    }
    
  }
}
