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

#include <v8.h>
#include <iostream>
#include <fuerte/connection.h>
#include <fuerte/jwt.h>
#include <fuerte/requests.h>

#include "Basics/FileUtils.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Import/ImportHelper.h"
#include "Rest/HttpResponse.h"
#include "Rest/Version.h"
#include "Shell/ClientFeature.h"
#include "Shell/ConsoleFeature.h"
#include "SimpleHttpClient/GeneralClientConnection.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/SimpleHttpResult.h"
#include "Ssl/SslInterface.h"
#include "V8/v8-conv.h"
#include "V8/v8-json.h"
#include "V8/v8-utils.h"
#include "V8/v8-vpack.h"

#include <iostream>

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::basics;
using namespace arangodb::httpclient;
using namespace arangodb::import;

V8ClientConnection::V8ClientConnection()
    : _lastHttpReturnCode(0),
      _lastErrorMessage(""),
      _version("arango"),
      _mode("unknown mode"),
      _loop(1),
      _vpackOptions(VPackOptions::Defaults) {
  _vpackOptions.buildUnindexedObjects = true;
  _vpackOptions.buildUnindexedArrays = true;
  _builder.onFailure([this](int error, std::string const& msg) {
    std::unique_lock<std::mutex> guard(_lock, std::try_to_lock);
    if (guard) {
      _lastHttpReturnCode = 503;
      _lastErrorMessage = msg;
    }
  });
}

V8ClientConnection::~V8ClientConnection() {
  _builder.onFailure(nullptr); // reset callback
  shutdownConnection();
}

void V8ClientConnection::createConnection() {
  auto newConnection = _builder.connect(_loop);
  fuerte::StringMap params{{"details","true"}};
  auto req = fuerte::createRequest(fuerte::RestVerb::Get, "/_api/version", params);
  req->header.database = _databaseName;
  req->timeout(std::chrono::seconds(30));
  try {
    auto res = newConnection->sendRequest(std::move(req));

    _lastHttpReturnCode = res->statusCode();
    if (_lastHttpReturnCode >= 400) {
      auto const& headers = res->messageHeader().meta;
      auto it = headers.find("http/1.1");
      if (it != headers.end()) {
        _lastErrorMessage = (*it).second;
      }
    }
    
    if (_lastHttpReturnCode == 200) {
      _connection = std::move(newConnection);
      
      std::shared_ptr<VPackBuilder> parsedBody;
      VPackSlice body;
      if (res->contentType() == fuerte::ContentType::VPack) {
        body = res->slices()[0];
      } else {
        parsedBody = VPackParser::fromJson(reinterpret_cast<char const*>(res->payload().data()),
                                           res->payload().size());
        body = parsedBody->slice();
      }
      
      std::string const server =
      VelocyPackHelper::getStringValue(body, "server", "");
      
      // "server" value is a string and content is "arango"
      if (server == "arango") {
        // look up "version" value
        _version = VelocyPackHelper::getStringValue(body, "version", "");
        VPackSlice const details = body.get("details");
        if (details.isObject()) {
          VPackSlice const mode = details.get("mode");
          if (mode.isString()) {
            _mode = mode.copyString();
          }
        }
        std::string const versionString =
        VelocyPackHelper::getStringValue(body, "version", "");
        std::pair<int, int> version =
        rest::Version::parseVersionString(versionString);
        if (version.first < 3) {
          // major version of server is too low
          //_client->disconnect();
          shutdownConnection();
          _lastErrorMessage = "Server version number ('" + versionString +
          "') is too low. Expecting 3.0 or higher";
          return;
        }
      }
    }
  } catch (fuerte::ErrorCondition const& e) { // connection error
    _lastErrorMessage = fuerte::to_string(e);
    _lastHttpReturnCode = 503;
  }
}

void V8ClientConnection::setInterrupted(bool interrupted) {
  std::lock_guard<std::mutex> guard(_lock);
  if (interrupted && _connection.get() != nullptr) {
    _connection->cancel();
    _connection.reset();
  } else if (!interrupted && _connection.get() == nullptr) {
    createConnection();
  }
}

bool V8ClientConnection::isConnected() {
  if (_connection) {
    return _connection->state() == fuerte::Connection::State::Connected;
  }
  return false;
}

std::string V8ClientConnection::endpointSpecification() const {
  if (_connection) {
    return _connection->endpoint();
  }
  return "";
}

void V8ClientConnection::connect(ClientFeature* client) {
  TRI_ASSERT(client);
  std::lock_guard<std::mutex> guard(_lock);

  _requestTimeout = std::chrono::duration<double>(client->requestTimeout());
  _databaseName = client->databaseName();
  _builder.endpoint(client->endpoint());
  // check jwtSecret first, as it is empty by default,
  // but username defaults to "root" in most configurations
  if (!client->jwtSecret().empty()) {
    _builder.jwtToken(fuerte::jwt::generateInternalToken(client->jwtSecret(), "arangosh"));
    _builder.authenticationType(fuerte::AuthenticationType::Jwt);
  } else if (!client->username().empty()) {
    _builder.user(client->username()).password(client->password());
    _builder.authenticationType(fuerte::AuthenticationType::Basic);
  }
  createConnection();
}

void V8ClientConnection::reconnect(ClientFeature* client) {
  std::lock_guard<std::mutex> guard(_lock);
  
  _requestTimeout = std::chrono::duration<double>(client->requestTimeout());
  _databaseName = client->databaseName();
  _builder.endpoint(client->endpoint());
  if (!client->username().empty()) {
    _builder.user(client->username()).password(client->password());
    _builder.authenticationType(fuerte::AuthenticationType::Basic);
  } else if (!client->jwtSecret().empty()) {
    _builder.jwtToken(fuerte::jwt::generateInternalToken(client->jwtSecret(), "arangosh"));
    _builder.authenticationType(fuerte::AuthenticationType::Jwt);
  }
  
  auto oldConnection = std::move(_connection);
  if (oldConnection) {
    oldConnection->cancel();
  }
  try {
    createConnection();
  } catch (...) {
    std::string errorMessage = "error in '" + client->endpoint() + "'";
    throw errorMessage;
  }

  if (isConnected() &&
      _lastHttpReturnCode == static_cast<int>(rest::ResponseCode::OK)) {
    LOG_TOPIC(INFO, arangodb::Logger::FIXME)
        << "Connected to ArangoDB "
        << "'" << endpointSpecification() << "', "
        << "version " << _version << " [" << _mode << "], "
        << "database '" << _databaseName << "', "
        << "username: '" << client->username() << "'";
  } else {
    if (client->getWarnConnect()) {
      LOG_TOPIC(ERR, arangodb::Logger::FIXME)
          << "Could not connect to endpoint '" << client->endpoint()
          << "', username: '" << client->username() << "'";
    }

    std::string errorMsg = "could not connect";

    if (!_lastErrorMessage.empty()) {
      errorMsg = _lastErrorMessage;
    }

    throw errorMsg;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief enum for wrapped V8 objects
////////////////////////////////////////////////////////////////////////////////

enum WRAP_CLASS_TYPES { WRAP_TYPE_CONNECTION = 1 };

////////////////////////////////////////////////////////////////////////////////
/// @brief map of connection objects
////////////////////////////////////////////////////////////////////////////////

static std::unordered_map<void*, v8::Persistent<v8::External>> Connections;

////////////////////////////////////////////////////////////////////////////////
/// @brief object template for the initial connection
////////////////////////////////////////////////////////////////////////////////

static v8::Persistent<v8::ObjectTemplate> ConnectionTempl;

////////////////////////////////////////////////////////////////////////////////
/// @brief copies v8::Object to std::unordered_map<std::string, std::string>
////////////////////////////////////////////////////////////////////////////////

static void ObjectToMap(v8::Isolate* isolate,
                        std::unordered_map<std::string, std::string>& myMap,
                        v8::Local<v8::Value> val) {
  v8::Local<v8::Object> v8Headers = val.As<v8::Object>();

  if (v8Headers->IsObject()) {
    v8::Local<v8::Array> const props = v8Headers->GetPropertyNames();

    for (uint32_t i = 0; i < props->Length(); i++) {
      v8::Local<v8::Value> key = props->Get(i);
      myMap.emplace(TRI_ObjectToString(isolate, key),
                    TRI_ObjectToString(isolate, v8Headers->Get(key)));
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief weak reference callback for queries (call the destructor here)
////////////////////////////////////////////////////////////////////////////////

static void DestroyV8ClientConnection(V8ClientConnection* v8connection) {
  TRI_ASSERT(v8connection != nullptr);

  auto it = Connections.find(v8connection);

  if (it != Connections.end()) {
    (*it).second.Reset();
    Connections.erase(it);
  }

  delete v8connection;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief weak reference callback for queries (call the destructor here)
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_DestructorCallback(
    const v8::WeakCallbackInfo<v8::Persistent<v8::External>>& data) {
  auto persistent = data.GetParameter();
  auto myConnection =
      v8::Local<v8::External>::New(data.GetIsolate(), *persistent);
  auto v8connection = static_cast<V8ClientConnection*>(myConnection->Value());

  DestroyV8ClientConnection(v8connection);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief wrap V8ClientConnection in a v8::Object
////////////////////////////////////////////////////////////////////////////////

static v8::Local<v8::Value> WrapV8ClientConnection(
    v8::Isolate* isolate, V8ClientConnection* v8connection) {
  v8::EscapableHandleScope scope(isolate);
  auto localConnectionTempl =
      v8::Local<v8::ObjectTemplate>::New(isolate, ConnectionTempl);
  v8::Local<v8::Object> result = localConnectionTempl->NewInstance();

  auto myConnection = v8::External::New(isolate, v8connection);
  result->SetInternalField(SLOT_CLASS_TYPE,
                           v8::Integer::New(isolate, WRAP_TYPE_CONNECTION));
  result->SetInternalField(SLOT_CLASS, myConnection);
  Connections[v8connection].Reset(isolate, myConnection);
  Connections[v8connection].SetWeak(&Connections[v8connection],
                                    ClientConnection_DestructorCallback,
                                    v8::WeakCallbackType::kFinalizer);
  return scope.Escape<v8::Value>(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection constructor
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_ConstructorCallback(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  v8::Local<v8::External> wrap = v8::Local<v8::External>::Cast(args.Data());
  ClientFeature* client = static_cast<ClientFeature*>(wrap->Value());

  auto v8connection = std::make_unique<V8ClientConnection>();
  v8connection->connect(client);

  if (v8connection->isConnected() &&
      v8connection->lastHttpReturnCode() == (int)rest::ResponseCode::OK) {
    LOG_TOPIC(INFO, arangodb::Logger::FIXME)
        << "Connected to ArangoDB "
        << "'" << v8connection->endpointSpecification() << "', "
        << "version " << v8connection->version() << " [" << v8connection->mode()
        << "], "
        << "database '" << v8connection->databaseName() << "', "
        << "username: '" << v8connection->username() << "'";

  } else {
    std::string errorMessage =
        "Could not connect. Error message: " + v8connection->lastErrorMessage();

    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_SIMPLE_CLIENT_COULD_NOT_CONNECT,
                                   errorMessage.c_str());
  }

  TRI_V8_RETURN(WrapV8ClientConnection(isolate, v8connection.release()));
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "reconnect"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_reconnect(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  V8ClientConnection* v8connection =
      TRI_UnwrapClass<V8ClientConnection>(args.Holder(), WRAP_TYPE_CONNECTION);

  v8::Local<v8::External> wrap = v8::Local<v8::External>::Cast(args.Data());
  ClientFeature* client = static_cast<ClientFeature*>(wrap->Value());

  if (v8connection == nullptr || client == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("connection class corrupted");
  }

  if (args.Length() < 2) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "reconnect(<endpoint>, <database>, [, <username>, <password>])");
  }

  std::string const endpoint = TRI_ObjectToString(isolate, args[0]);
  std::string databaseName = TRI_ObjectToString(isolate, args[1]);

  std::string username;

  if (args.Length() < 3) {
    username = client->username();
  } else {
    username = TRI_ObjectToString(isolate, args[2]);
  }

  std::string password;

  if (args.Length() < 4) {
    if (client->jwtSecret().empty()) {
      ConsoleFeature* console =
        ApplicationServer::getFeature<ConsoleFeature>("Console");
      
      if (console->isEnabled()) {
        password = console->readPassword("Please specify a password: ");
      } else {
        std::cout << "Please specify a password: " << std::flush;
        password = ConsoleFeature::readPassword();
        std::cout << std::endl << std::flush;
      }
    }
  } else {
    password = TRI_ObjectToString(isolate, args[3]);
  }
  
  bool warnConnect = true;
  if (args.Length() > 4) {
    warnConnect = TRI_ObjectToBoolean(args[4]);
  }

  client->setEndpoint(endpoint);
  client->setDatabaseName(databaseName);
  client->setUsername(username);
  client->setPassword(password);
  client->setWarnConnect(warnConnect);

  try {
    v8connection->reconnect(client);
  } catch (std::string const& errorMessage) {
    TRI_V8_THROW_EXCEPTION_PARAMETER(errorMessage.c_str());
  } catch (...) {
    std::string errorMessage = "error in '" + endpoint + "'";
    TRI_V8_THROW_EXCEPTION_PARAMETER(errorMessage.c_str());
  }

  TRI_ExecuteJavaScriptString(
      isolate, isolate->GetCurrentContext(),
      TRI_V8_STRING(isolate, "require('internal').db._flushCache();"),
      TRI_V8_ASCII_STRING(isolate, "reload db object"), false);

  TRI_V8_RETURN_TRUE();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "connectedUser"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_connectedUser(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  v8::Local<v8::External> wrap = v8::Local<v8::External>::Cast(args.Data());
  ClientFeature* client = static_cast<ClientFeature*>(wrap->Value());
  if (client == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("connection class corrupted");
  }

  TRI_V8_RETURN(TRI_V8_STD_STRING(isolate, client->username()));
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "GET" helper
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpGetAny(
    v8::FunctionCallbackInfo<v8::Value> const& args, bool raw) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  // get the connection
  V8ClientConnection* v8connection =
      TRI_UnwrapClass<V8ClientConnection>(args.Holder(), WRAP_TYPE_CONNECTION);

  if (v8connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("connection class corrupted");
  }

  // check params
  if (args.Length() < 1 || args.Length() > 2 || !args[0]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE("get(<url>[, <headers>])");
  }

  TRI_Utf8ValueNFC url(args[0]);
  // check header fields
  std::unordered_map<std::string, std::string> headerFields;

  if (args.Length() > 1) {
    ObjectToMap(isolate, headerFields, args[1]);
  }

  TRI_V8_RETURN(v8connection->getData(isolate, StringRef(*url, url.length()), headerFields, raw));
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "GET"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpGet(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  ClientConnection_httpGetAny(args, false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "GET_RAW"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpGetRaw(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  ClientConnection_httpGetAny(args, true);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "HEAD" helper
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpHeadAny(
    v8::FunctionCallbackInfo<v8::Value> const& args, bool raw) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  // get the connection
  V8ClientConnection* v8connection =
      TRI_UnwrapClass<V8ClientConnection>(args.Holder(), WRAP_TYPE_CONNECTION);

  if (v8connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("connection class corrupted");
  }

  // check params
  if (args.Length() < 1 || args.Length() > 2 || !args[0]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE("head(<url>[, <headers>])");
  }

  TRI_Utf8ValueNFC url(args[0]);

  // check header fields
  std::unordered_map<std::string, std::string> headerFields;

  if (args.Length() > 1) {
    ObjectToMap(isolate, headerFields, args[1]);
  }

  TRI_V8_RETURN(v8connection->headData(isolate, StringRef(*url, url.length()), headerFields, raw));
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "HEAD"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpHead(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  ClientConnection_httpHeadAny(args, false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "HEAD_RAW"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpHeadRaw(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  ClientConnection_httpHeadAny(args, true);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "DELETE" helper
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpDeleteAny(
    v8::FunctionCallbackInfo<v8::Value> const& args, bool raw) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  // get the connection
  V8ClientConnection* v8connection =
      TRI_UnwrapClass<V8ClientConnection>(args.Holder(), WRAP_TYPE_CONNECTION);

  if (v8connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("connection class corrupted");
  }

  // check params
  if (args.Length() < 1 || args.Length() > 3 || !args[0]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE("delete(<url>[, <body>[, <headers>]])");
  }

  TRI_Utf8ValueNFC url(args[0]);
  
  std::unordered_map<std::string, std::string> headerFields;
  if (args.Length() == 1) { // no body provided
    TRI_V8_RETURN(v8connection->deleteData(isolate, StringRef(*url, url.length()),
                                           v8::Undefined(isolate), headerFields, raw));
  }

  // check header fields
  if (args.Length() > 2) {
    ObjectToMap(isolate, headerFields, args[2]);
  }

  TRI_V8_RETURN(v8connection->deleteData(isolate, StringRef(*url, url.length()), args[1], headerFields, raw));
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "DELETE"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpDelete(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  ClientConnection_httpDeleteAny(args, false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "DELETE_RAW"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpDeleteRaw(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  ClientConnection_httpDeleteAny(args, true);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "OPTIONS" helper
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpOptionsAny(
    v8::FunctionCallbackInfo<v8::Value> const& args, bool raw) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  // get the connection
  V8ClientConnection* v8connection =
      TRI_UnwrapClass<V8ClientConnection>(args.Holder(), WRAP_TYPE_CONNECTION);

  if (v8connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("connection class corrupted");
  }

  // check params
  if (args.Length() < 2 || args.Length() > 3 || !args[0]->IsString() ||
      args[1]->IsUndefined()) {
    TRI_V8_THROW_EXCEPTION_USAGE("options(<url>, <body>[, <headers>])");
  }

  TRI_Utf8ValueNFC url(args[0]);

  // check header fields
  std::unordered_map<std::string, std::string> headerFields;
  if (args.Length() > 2) {
    ObjectToMap(isolate, headerFields, args[2]);
  }

  TRI_V8_RETURN(
      v8connection->optionsData(isolate, StringRef(*url, url.length()), args[1], headerFields, raw));
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "OPTIONS"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpOptions(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  ClientConnection_httpOptionsAny(args, false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "OPTIONS_RAW"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpOptionsRaw(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  ClientConnection_httpOptionsAny(args, true);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "POST" helper
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpPostAny(
    v8::FunctionCallbackInfo<v8::Value> const& args, bool raw) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  // get the connection
  V8ClientConnection* v8connection =
      TRI_UnwrapClass<V8ClientConnection>(args.Holder(), WRAP_TYPE_CONNECTION);

  if (v8connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("connection class corrupted");
  }

  // check params
  if (args.Length() < 2 || args.Length() > 3 || !args[0]->IsString() ||
      args[1]->IsUndefined()) {
    TRI_V8_THROW_EXCEPTION_USAGE("post(<url>, <body>[, <headers>])");
  }

  TRI_Utf8ValueNFC url(args[0]);
  // check header fields
  std::unordered_map<std::string, std::string> headerFields;
  if (args.Length() > 2) {
    ObjectToMap(isolate, headerFields, args[2]);
  }

  TRI_V8_RETURN(
      v8connection->postData(isolate, StringRef(*url, url.length()), args[1], headerFields, raw));
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "POST"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpPost(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  ClientConnection_httpPostAny(args, false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "POST_RAW"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpPostRaw(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  ClientConnection_httpPostAny(args, true);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "PUT" helper
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpPutAny(
    v8::FunctionCallbackInfo<v8::Value> const& args, bool raw) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  // get the connection
  V8ClientConnection* v8connection =
      TRI_UnwrapClass<V8ClientConnection>(args.Holder(), WRAP_TYPE_CONNECTION);

  if (v8connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("connection class corrupted");
  }

  // check params
  if (args.Length() < 2 || args.Length() > 3 || !args[0]->IsString() ||
      args[1]->IsUndefined()) {
    TRI_V8_THROW_EXCEPTION_USAGE("put(<url>, <body>[, <headers>])");
  }

  TRI_Utf8ValueNFC url(args[0]);

  // check header fields
  std::unordered_map<std::string, std::string> headerFields;
  if (args.Length() > 2) {
    ObjectToMap(isolate, headerFields, args[2]);
  }

  TRI_V8_RETURN(v8connection->putData(isolate, StringRef(*url, url.length()), args[1], headerFields, raw));
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "PUT"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpPut(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  ClientConnection_httpPutAny(args, false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "PUT_RAW"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpPutRaw(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  ClientConnection_httpPutAny(args, true);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "PATCH" helper
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpPatchAny(
    v8::FunctionCallbackInfo<v8::Value> const& args, bool raw) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  // get the connection
  V8ClientConnection* v8connection =
      TRI_UnwrapClass<V8ClientConnection>(args.Holder(), WRAP_TYPE_CONNECTION);

  if (v8connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("connection class corrupted");
  }

  // check params
  if (args.Length() < 2 || args.Length() > 3 || !args[0]->IsString() ||
      args[1]->IsUndefined()) {
    TRI_V8_THROW_EXCEPTION_USAGE("patch(<url>, <body>[, <headers>])");
  }

  TRI_Utf8ValueNFC url(args[0]);
  // check header fields
  std::unordered_map<std::string, std::string> headerFields;
  if (args.Length() > 2) {
    ObjectToMap(isolate, headerFields, args[2]);
  }

  TRI_V8_RETURN(
      v8connection->patchData(isolate, StringRef(*url, url.length()), args[1], headerFields, raw));
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "PATCH"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpPatch(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  ClientConnection_httpPatchAny(args, false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "PATCH_RAW"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpPatchRaw(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  ClientConnection_httpPatchAny(args, true);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection send file helper
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpSendFile(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  // get the connection
  V8ClientConnection* v8connection =
      TRI_UnwrapClass<V8ClientConnection>(args.Holder(), WRAP_TYPE_CONNECTION);

  if (v8connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("connection class corrupted");
  }

  // check params
  if (args.Length() != 2 || !args[0]->IsString() || args[1]->IsUndefined()) {
    TRI_V8_THROW_EXCEPTION_USAGE("sendFile(<url>, <file>)");
  }

  TRI_Utf8ValueNFC url(args[0]);
  
  std::string const infile = TRI_ObjectToString(isolate, args[1]);
  
  if (!FileUtils::exists(infile)) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_FILE_NOT_FOUND);
  }

  v8::TryCatch tryCatch;

  // check header fields
  std::unordered_map<std::string, std::string> headerFields;
  v8::Local<v8::Value> result =
      v8connection->postData(isolate, StringRef(*url, url.length()), args[1], headerFields,
                             false, /*isFile*/ true);

  if (tryCatch.HasCaught()) {
    isolate->ThrowException(tryCatch.Exception());
    return;
  }

  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "getEndpoint"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_getEndpoint(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate)
  v8::HandleScope scope(isolate);

  // get the connection
  V8ClientConnection* v8connection =
      TRI_UnwrapClass<V8ClientConnection>(args.Holder(), WRAP_TYPE_CONNECTION);

  v8::Local<v8::External> wrap = v8::Local<v8::External>::Cast(args.Data());
  ClientFeature* client = static_cast<ClientFeature*>(wrap->Value());

  if (v8connection == nullptr || client == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("connection class corrupted");
  }

  // check params
  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("getEndpoint()");
  }

  TRI_V8_RETURN_STD_STRING(client->endpoint());
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief imports a CSV file
////////////////////////////////////////////////////////////////////////////////

static uint64_t DefaultChunkSize = 1024 * 1024 * 4;

static void ClientConnection_importCsv(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() < 2) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "importCsvFile(<filename>, <collection>[, <options>])");
  }

  // extract the filename
  v8::String::Utf8Value filename(args[0]);

  if (*filename == nullptr) {
    TRI_V8_THROW_TYPE_ERROR("<filename> must be a UTF-8 filename");
  }

  v8::String::Utf8Value collection(args[1]);

  if (*collection == nullptr) {
    TRI_V8_THROW_TYPE_ERROR("<collection> must be a UTF-8 filename");
  }

  // extract the options
  v8::Local<v8::String> separatorKey =
      TRI_V8_ASCII_STRING(isolate, "separator");
  v8::Local<v8::String> quoteKey = TRI_V8_ASCII_STRING(isolate, "quote");

  std::string separator = ",";
  std::string quote = "\"";

  if (3 <= args.Length()) {
    v8::Local<v8::Object> options = args[2]->ToObject();
    // separator
    if (options->Has(separatorKey)) {
      separator = TRI_ObjectToString(isolate, options->Get(separatorKey));

      if (separator.length() < 1) {
        TRI_V8_THROW_EXCEPTION_PARAMETER(
            "<options>.separator must be at least one character");
      }
    }

    // quote
    if (options->Has(quoteKey)) {
      quote = TRI_ObjectToString(isolate, options->Get(quoteKey));

      if (quote.length() > 1) {
        TRI_V8_THROW_EXCEPTION_PARAMETER(
            "<options>.quote must be at most one character");
      }
    }
  }

  V8ClientConnection* v8connection =
      TRI_UnwrapClass<V8ClientConnection>(args.Holder(), WRAP_TYPE_CONNECTION);

  v8::Local<v8::External> wrap = v8::Local<v8::External>::Cast(args.Data());
  ClientFeature* client = static_cast<ClientFeature*>(wrap->Value());
  SimpleHttpClientParams params(client->requestTimeout(), client->getWarn());
  ImportHelper ih(client, v8connection->endpointSpecification(), params,
                  DefaultChunkSize, 1);

  ih.setQuote(quote);
  ih.setSeparator(separator.c_str());

  std::string fileName = TRI_ObjectToString(isolate, args[0]);
  std::string collectionName = TRI_ObjectToString(isolate, args[1]);

  if (ih.importDelimited(collectionName, fileName, ImportHelper::CSV)) {
    v8::Local<v8::Object> result = v8::Object::New(isolate);

    result->Set(TRI_V8_ASCII_STRING(isolate, "lines"),
                v8::Integer::New(isolate, (int32_t)ih.getReadLines()));

    result->Set(TRI_V8_ASCII_STRING(isolate, "created"),
                v8::Integer::New(isolate, (int32_t)ih.getNumberCreated()));

    result->Set(TRI_V8_ASCII_STRING(isolate, "errors"),
                v8::Integer::New(isolate, (int32_t)ih.getNumberErrors()));

    result->Set(TRI_V8_ASCII_STRING(isolate, "updated"),
                v8::Integer::New(isolate, (int32_t)ih.getNumberUpdated()));

    result->Set(TRI_V8_ASCII_STRING(isolate, "ignored"),
                v8::Integer::New(isolate, (int32_t)ih.getNumberIgnored()));

    TRI_V8_RETURN(result);
  }

  std::string error = "error messages:";
  for (std::string const& msg : ih.getErrorMessages()) {
    error.append(msg + ";\t");
  }

  TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_FAILED, error.c_str());
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief imports a JSON file
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_importJson(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() < 2) {
    TRI_V8_THROW_EXCEPTION_USAGE("importJsonFile(<filename>, <collection>)");
  }

  // extract the filename
  v8::String::Utf8Value filename(args[0]);

  if (*filename == nullptr) {
    TRI_V8_THROW_TYPE_ERROR("<filename> must be a UTF-8 filename");
  }

  v8::String::Utf8Value collection(args[1]);

  if (*collection == nullptr) {
    TRI_V8_THROW_TYPE_ERROR("<collection> must be a UTF-8 filename");
  }

  V8ClientConnection* v8connection =
      TRI_UnwrapClass<V8ClientConnection>(args.Holder(), WRAP_TYPE_CONNECTION);

  v8::Local<v8::External> wrap = v8::Local<v8::External>::Cast(args.Data());
  ClientFeature* client = static_cast<ClientFeature*>(wrap->Value());

  SimpleHttpClientParams params(client->requestTimeout(), client->getWarn());
  ImportHelper ih(client, v8connection->endpointSpecification(), params,
                  DefaultChunkSize, 1);

  std::string fileName = TRI_ObjectToString(isolate, args[0]);
  std::string collectionName = TRI_ObjectToString(isolate, args[1]);

  if (ih.importJson(collectionName, fileName, false)) {
    v8::Local<v8::Object> result = v8::Object::New(isolate);

    result->Set(TRI_V8_ASCII_STRING(isolate, "lines"),
                v8::Integer::New(isolate, (int32_t)ih.getReadLines()));

    result->Set(TRI_V8_ASCII_STRING(isolate, "created"),
                v8::Integer::New(isolate, (int32_t)ih.getNumberCreated()));

    result->Set(TRI_V8_ASCII_STRING(isolate, "errors"),
                v8::Integer::New(isolate, (int32_t)ih.getNumberErrors()));

    result->Set(TRI_V8_ASCII_STRING(isolate, "updated"),
                v8::Integer::New(isolate, (int32_t)ih.getNumberUpdated()));

    result->Set(TRI_V8_ASCII_STRING(isolate, "ignored"),
                v8::Integer::New(isolate, (int32_t)ih.getNumberIgnored()));

    TRI_V8_RETURN(result);
  }

  std::string error = "error messages:";
  for (std::string const& msg : ih.getErrorMessages()) {
    error.append(msg + ";\t");
  }

  TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_FAILED, error.c_str());
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "lastError"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_lastHttpReturnCode(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  // get the connection
  V8ClientConnection* v8connection =
      TRI_UnwrapClass<V8ClientConnection>(args.Holder(), WRAP_TYPE_CONNECTION);

  if (v8connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("connection class corrupted");
  }

  // check params
  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("lastHttpReturnCode()");
  }

  TRI_V8_RETURN(v8::Integer::New(isolate, v8connection->lastHttpReturnCode()));
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "lastErrorMessage"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_lastErrorMessage(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  // get the connection
  V8ClientConnection* v8connection =
      TRI_UnwrapClass<V8ClientConnection>(args.Holder(), WRAP_TYPE_CONNECTION);

  if (v8connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("connection class corrupted");
  }

  // check params
  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("lastErrorMessage()");
  }

  TRI_V8_RETURN_STD_STRING(v8connection->lastErrorMessage());
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "isConnected"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_isConnected(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  // get the connection
  V8ClientConnection* v8connection =
      TRI_UnwrapClass<V8ClientConnection>(args.Holder(), WRAP_TYPE_CONNECTION);

  if (v8connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("connection class corrupted");
  }

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("isConnected()");
  }

  if (v8connection->isConnected()) {
    TRI_V8_RETURN_TRUE();
  }
  TRI_V8_RETURN_FALSE();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "isConnected"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_toString(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  // get the connection
  V8ClientConnection* v8connection =
      TRI_UnwrapClass<V8ClientConnection>(args.Holder(), WRAP_TYPE_CONNECTION);

  if (v8connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("connection class corrupted");
  }

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("toString()");
  }

  std::string result =
      "[object ArangoConnection:" + v8connection->endpointSpecification();

  if (v8connection->isConnected()) {
    result += "," + v8connection->version() + ",connected]";
  } else {
    result += ",unconnected]";
  }

  TRI_V8_RETURN_STD_STRING(result);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "getVersion"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_getVersion(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  // get the connection
  V8ClientConnection* v8connection =
      TRI_UnwrapClass<V8ClientConnection>(args.Holder(), WRAP_TYPE_CONNECTION);

  if (v8connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("connection class corrupted");
  }

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("getVersion()");
  }

  TRI_V8_RETURN_STD_STRING(v8connection->version());
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "getMode"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_getMode(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  // get the connection
  V8ClientConnection* v8connection =
      TRI_UnwrapClass<V8ClientConnection>(args.Holder(), WRAP_TYPE_CONNECTION);

  if (v8connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("connection class corrupted");
  }

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("getMode()");
  }

  TRI_V8_RETURN_STD_STRING(v8connection->mode());
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "getDatabaseName"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_getDatabaseName(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  // get the connection
  V8ClientConnection* v8connection =
      TRI_UnwrapClass<V8ClientConnection>(args.Holder(), WRAP_TYPE_CONNECTION);

  if (v8connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("connection class corrupted");
  }

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("getDatabaseName()");
  }

  TRI_V8_RETURN_STD_STRING(v8connection->databaseName());
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "setDatabaseName"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_setDatabaseName(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  // get the connection
  V8ClientConnection* v8connection =
      TRI_UnwrapClass<V8ClientConnection>(args.Holder(), WRAP_TYPE_CONNECTION);

  v8::Local<v8::External> wrap = v8::Local<v8::External>::Cast(args.Data());
  ClientFeature* client = static_cast<ClientFeature*>(wrap->Value());

  if (v8connection == nullptr || client == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("connection class corrupted");
  }

  if (args.Length() != 1 || !args[0]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE("setDatabaseName(<name>)");
  }

  std::string const dbName = TRI_ObjectToString(isolate, args[0]);
  v8connection->setDatabaseName(dbName);
  client->setDatabaseName(dbName);

  TRI_V8_RETURN_TRUE();
  TRI_V8_TRY_CATCH_END
}

v8::Local<v8::Value> V8ClientConnection::getData(
    v8::Isolate* isolate, StringRef const& location,
    std::unordered_map<std::string, std::string> const& headerFields, bool raw) {
  if (raw) {
    return requestDataRaw(isolate, fuerte::RestVerb::Get, location, v8::Undefined(isolate), headerFields);
  }
  return requestData(isolate, fuerte::RestVerb::Get, location, v8::Undefined(isolate), headerFields);
}

v8::Local<v8::Value> V8ClientConnection::headData(
    v8::Isolate* isolate, StringRef const& location,
    std::unordered_map<std::string, std::string> const& headerFields, bool raw) {
  if (raw) {
    return requestDataRaw(isolate, fuerte::RestVerb::Head, location, v8::Undefined(isolate), headerFields);
  }
  return requestData(isolate, fuerte::RestVerb::Head, location, v8::Undefined(isolate), headerFields);
}

v8::Local<v8::Value> V8ClientConnection::deleteData(
    v8::Isolate* isolate, StringRef const& location, v8::Local<v8::Value> const& body,
    std::unordered_map<std::string, std::string> const& headerFields, bool raw) {
  if (raw) {
    return requestDataRaw(isolate, fuerte::RestVerb::Delete, location, body, headerFields);
  }
  return requestData(isolate, fuerte::RestVerb::Delete, location, body, headerFields);
}

v8::Local<v8::Value> V8ClientConnection::optionsData(
    v8::Isolate* isolate, StringRef const& location, v8::Local<v8::Value> const& body,
    std::unordered_map<std::string, std::string> const& headerFields, bool raw) {
  if (raw) {
    return requestDataRaw(isolate, fuerte::RestVerb::Options, location, body, headerFields);
  }
  return requestData(isolate, fuerte::RestVerb::Options, location, body, headerFields);
}

v8::Local<v8::Value> V8ClientConnection::postData(
    v8::Isolate* isolate, StringRef const& location, v8::Local<v8::Value> const& body,
    std::unordered_map<std::string, std::string> const& headerFields, bool raw, bool isFile) {
  if (raw) {
    return requestDataRaw(isolate, fuerte::RestVerb::Post, location, body, headerFields);
  }
  return requestData(isolate, fuerte::RestVerb::Post, location, body, headerFields, isFile);
}

v8::Local<v8::Value> V8ClientConnection::putData(
    v8::Isolate* isolate, StringRef const& location, v8::Local<v8::Value> const& body,
    std::unordered_map<std::string, std::string> const& headerFields, bool raw) {
  if (raw) {
    return requestDataRaw(isolate, fuerte::RestVerb::Put, location, body, headerFields);
  }
  return requestData(isolate, fuerte::RestVerb::Put, location, body, headerFields);
}

v8::Local<v8::Value> V8ClientConnection::patchData(
    v8::Isolate* isolate, StringRef const& location, v8::Local<v8::Value> const& body,
    std::unordered_map<std::string, std::string> const& headerFields, bool raw) {
  if (raw) {
    return requestDataRaw(isolate, fuerte::RestVerb::Patch, location, body, headerFields);
  }
  return requestData(isolate, fuerte::RestVerb::Patch, location, body, headerFields);
}

v8::Local<v8::Value> V8ClientConnection::requestData(
    v8::Isolate* isolate, fuerte::RestVerb method,
    StringRef const& location, v8::Local<v8::Value> const& body,
    std::unordered_map<std::string, std::string> const& headerFields,
    bool isFile) {
  _lastErrorMessage = "";
  _lastHttpReturnCode = 0;
  if (!_connection) {
    TRI_V8_SET_EXCEPTION_MESSAGE(TRI_SIMPLE_CLIENT_COULD_NOT_CONNECT,
                                 "not connected");
    return v8::Undefined(isolate); 
  }
  
  auto req = std::make_unique<fuerte::Request>();
  req->header.restVerb = method;
  req->header.database = _databaseName;
  req->header.parseArangoPath(location.toString());
  for (auto& pair : headerFields) {
    req->header.meta.emplace(std::move(pair));
  }
  if (isFile) {
    std::string const infile = TRI_ObjectToString(isolate, body);
    TRI_ASSERT(FileUtils::exists(infile));
    std::string contents;
    try {
      contents = FileUtils::slurp(infile);
    } catch (...) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_errno(), "could not read file");
    }
    req->addBinary(reinterpret_cast<uint8_t const*>(contents.data()), contents.length());
  } else if (body->IsString()) { // assume JSON
    TRI_Utf8ValueNFC bodyString(body);
    req->addBinary(reinterpret_cast<uint8_t const*>(*bodyString), bodyString.length());
    if (req->header.contentType() == fuerte::ContentType::Unset) {
      req->header.contentType(fuerte::ContentType::Json);
    }
  } else if (!body->IsNullOrUndefined()) {
    VPackBuffer<uint8_t> buffer;
    VPackBuilder builder(buffer, &_vpackOptions);
    int res = TRI_V8ToVPack(isolate, builder, body, false);
    if (res != TRI_ERROR_NO_ERROR) {
      LOG_TOPIC(ERR, Logger::V8) << "error converting request body " << TRI_errno_string(res);
      return v8::Null(isolate);
    }
    req->addVPack(std::move(buffer));
    req->header.contentType(fuerte::ContentType::VPack);
  } else {
    // body is null or undefined
    if (req->header.contentType() == fuerte::ContentType::Unset) {
      req->header.contentType(fuerte::ContentType::Json);
    }
  }
  if (req->header.acceptType() == fuerte::ContentType::Unset) {
    req->header.acceptType(fuerte::ContentType::VPack);
  }
  req->timeout(std::chrono::duration_cast<std::chrono::milliseconds>(_requestTimeout));
  
  std::unique_ptr<fuerte::Response> response;
  try {
    response = _connection->sendRequest(std::move(req));
  } catch (fuerte::ErrorCondition const& ec) {
    return handleResult(isolate, nullptr, ec);
  }
  
  return handleResult(isolate, std::move(response), fuerte::ErrorCondition::NoError);
}

v8::Local<v8::Value> V8ClientConnection::requestDataRaw(
    v8::Isolate* isolate, fuerte::RestVerb method,
    StringRef const& location, v8::Local<v8::Value> const& body,
    std::unordered_map<std::string, std::string> const& headerFields) {

  _lastErrorMessage = "";
  _lastHttpReturnCode = 0;
  if (!_connection) {
    TRI_V8_SET_EXCEPTION_MESSAGE(TRI_SIMPLE_CLIENT_COULD_NOT_CONNECT,
                                 "not connected");
    return v8::Undefined(isolate); 
  }

  auto req = std::make_unique<fuerte::Request>();
  req->header.restVerb = method;
  req->header.database = _databaseName;
  req->header.parseArangoPath(location.toString());
  for (auto& pair : headerFields) {
    req->header.meta.emplace(std::move(pair));
  }
  if (body->IsString()) { // assume JSON
    TRI_Utf8ValueNFC bodyString(body);
    req->addBinary(reinterpret_cast<uint8_t const*>(*bodyString), bodyString.length());
    if (req->header.contentType() == fuerte::ContentType::Unset) {
      req->header.contentType(fuerte::ContentType::Json);
    }
  } else if (!body->IsNullOrUndefined()) {
    VPackBuffer<uint8_t> buffer;
    VPackBuilder builder(buffer);
    int res = TRI_V8ToVPack(isolate, builder, body, false);
    if (res != TRI_ERROR_NO_ERROR) {
      LOG_TOPIC(ERR, Logger::V8) << "error converting request body " << TRI_errno_string(res);
      return v8::Null(isolate);
    }
    req->addVPack(std::move(buffer));
    req->header.contentType(fuerte::ContentType::VPack);
  } else {
    // body is null or undefined
    if (req->header.contentType() == fuerte::ContentType::Unset) {
      req->header.contentType(fuerte::ContentType::Json);
    }
  }
  if (req->header.acceptType() == fuerte::ContentType::Unset) {
    req->header.acceptType(fuerte::ContentType::VPack);
  }
  req->timeout(std::chrono::duration_cast<std::chrono::milliseconds>(_requestTimeout));
  
  std::unique_ptr<fuerte::Response> response;
  try {
    response = _connection->sendRequest(std::move(req));
  } catch (fuerte::ErrorCondition const& e) {
    _lastErrorMessage.assign(fuerte::to_string(e));
    _lastHttpReturnCode = 503;
  }
  
  v8::Local<v8::Object> result = v8::Object::New(isolate);
  if (!response) {
    result->ForceSet(TRI_V8_STD_STRING(isolate, StaticStrings::Error),
                     v8::Boolean::New(isolate, true));
    result->ForceSet(TRI_V8_STD_STRING(isolate, StaticStrings::ErrorNum),
                     v8::Integer::New(isolate, _lastHttpReturnCode));
    result->ForceSet(TRI_V8_STD_STRING(isolate, StaticStrings::ErrorMessage),
                     TRI_V8_STD_STRING(isolate, _lastErrorMessage));

    return result;
  }

  // complete

  _lastHttpReturnCode = response->statusCode();

  // create raw response
  result->ForceSet(TRI_V8_ASCII_STRING(isolate, "code"),
                   v8::Integer::New(isolate, _lastHttpReturnCode));

  if (_lastHttpReturnCode >= 400) {
    std::string msg(GeneralResponse::responseString(static_cast<ResponseCode>(_lastHttpReturnCode)));

    result->ForceSet(TRI_V8_STD_STRING(isolate, StaticStrings::Error),
                     v8::Boolean::New(isolate, true));
    result->ForceSet(TRI_V8_STD_STRING(isolate, StaticStrings::ErrorNum),
                     v8::Integer::New(isolate, _lastHttpReturnCode));
    result->ForceSet(TRI_V8_STD_STRING(isolate, StaticStrings::ErrorMessage),
                     TRI_V8_STD_STRING(isolate, msg));
  } else {
    result->ForceSet(TRI_V8_STD_STRING(isolate, StaticStrings::Error),
                     v8::Boolean::New(isolate, false));
  }

  // got a body, copy it into the result
  auto sb = response->payload();
  if (sb.size() > 0) {
    const char* str = reinterpret_cast<const char*>(sb.data());
    v8::Local<v8::String> b = TRI_V8_PAIR_STRING(isolate, str, sb.size());
    result->ForceSet(TRI_V8_ASCII_STRING(isolate, "body"), b);
  }

  // copy all headers
  v8::Local<v8::Object> headers = v8::Object::New(isolate);
  for (auto const& it : response->header.meta) {
    v8::Local<v8::String> key = TRI_V8_STD_STRING(isolate, it.first);
    v8::Local<v8::String> val = TRI_V8_STD_STRING(isolate, it.second);

    headers->ForceSet(key, val);
  }

  result->ForceSet(TRI_V8_ASCII_STRING(isolate, "headers"), headers);

  // and returns
  return result;
}

v8::Local<v8::Value> V8ClientConnection::handleResult(v8::Isolate* isolate,
                                                       std::unique_ptr<fuerte::Response> res,
                                                       fuerte::ErrorCondition ec) {
  // not complete
  if (!res) {
    _lastErrorMessage = fuerte::to_string(ec);
    _lastHttpReturnCode = static_cast<int>(rest::ResponseCode::SERVER_ERROR);

    v8::Local<v8::Object> result = v8::Object::New(isolate);
    result->ForceSet(TRI_V8_STD_STRING(isolate, StaticStrings::Error),
                     v8::Boolean::New(isolate, true));
    result->ForceSet(
        TRI_V8_ASCII_STRING(isolate, "code"),
        v8::Integer::New(isolate,
                         static_cast<int>(rest::ResponseCode::SERVER_ERROR)));

    int errorNumber = 0;
    switch (ec) {
      case fuerte::ErrorCondition::CouldNotConnect:
      case fuerte::ErrorCondition::ConnectionClosed:
        errorNumber = TRI_SIMPLE_CLIENT_COULD_NOT_CONNECT;
        break;

      case fuerte::ErrorCondition::ReadError:
        errorNumber = TRI_SIMPLE_CLIENT_COULD_NOT_READ;
        break;

      case fuerte::ErrorCondition::WriteError:
        errorNumber = TRI_SIMPLE_CLIENT_COULD_NOT_WRITE;
        break;

      default:
        errorNumber = TRI_SIMPLE_CLIENT_UNKNOWN_ERROR;
        break;
    }

    result->ForceSet(TRI_V8_STD_STRING(isolate, StaticStrings::ErrorNum),
                     v8::Integer::New(isolate, errorNumber));
    result->ForceSet(TRI_V8_STD_STRING(isolate, StaticStrings::ErrorMessage),
                     TRI_V8_STD_STRING(isolate, _lastErrorMessage));

    return result;
  }

  // complete
  _lastHttpReturnCode = res->statusCode();

  // got a body
  auto sb = res->payload();
  if (sb.size() > 0) {
    if (res->isContentTypeVPack()) {
      std::vector<VPackSlice> const& slices = res->slices();
      if (!slices.empty()) {
        return TRI_VPackToV8(isolate, slices[0]);
      } // no body
    } 
      
    char const* str = reinterpret_cast<char const*>(sb.data());
    
    if (res->isContentTypeJSON()) {
      return TRI_FromJsonString(isolate, str, sb.size(), nullptr);
    } 
    // return body as string
    return TRI_V8_PAIR_STRING(isolate, str, sb.size());
  }

  // no body

  v8::Local<v8::Object> result = v8::Object::New(isolate);

  result->ForceSet(TRI_V8_ASCII_STRING(isolate, "code"),
                   v8::Integer::New(isolate, _lastHttpReturnCode));

  if (_lastHttpReturnCode >= 400) {
    std::string msg(GeneralResponse::responseString(static_cast<ResponseCode>(_lastHttpReturnCode)));

    result->ForceSet(TRI_V8_STD_STRING(isolate, StaticStrings::Error),
                     v8::Boolean::New(isolate, true));
    result->ForceSet(TRI_V8_STD_STRING(isolate, StaticStrings::ErrorNum),
                     v8::Integer::New(isolate, _lastHttpReturnCode));
    result->ForceSet(TRI_V8_STD_STRING(isolate, StaticStrings::ErrorMessage),
                     TRI_V8_STD_STRING(isolate, msg));
  } else {
    result->ForceSet(TRI_V8_STD_STRING(isolate, StaticStrings::Error),
                     v8::Boolean::New(isolate, false));
  }

  return result;
}

void V8ClientConnection::initServer(v8::Isolate* isolate,
                                    v8::Local<v8::Context> context,
                                    ClientFeature* client) {
  v8::Local<v8::Value> v8client = v8::External::New(isolate, client);

  v8::Local<v8::FunctionTemplate> connection_templ =
      v8::FunctionTemplate::New(isolate);

  connection_templ->SetClassName(
      TRI_V8_ASCII_STRING(isolate, "ArangoConnection"));

  v8::Local<v8::ObjectTemplate> connection_proto =
      connection_templ->PrototypeTemplate();

  connection_proto->Set(
      isolate, "DELETE",
      v8::FunctionTemplate::New(isolate, ClientConnection_httpDelete));

  connection_proto->Set(
      isolate, "DELETE_RAW",
      v8::FunctionTemplate::New(isolate, ClientConnection_httpDeleteRaw));

  connection_proto->Set(
      isolate, "GET",
      v8::FunctionTemplate::New(isolate, ClientConnection_httpGet));

  connection_proto->Set(
      isolate, "GET_RAW",
      v8::FunctionTemplate::New(isolate, ClientConnection_httpGetRaw));

  connection_proto->Set(
      isolate, "HEAD",
      v8::FunctionTemplate::New(isolate, ClientConnection_httpHead));

  connection_proto->Set(
      isolate, "HEAD_RAW",
      v8::FunctionTemplate::New(isolate, ClientConnection_httpHeadRaw));

  connection_proto->Set(
      isolate, "OPTIONS",
      v8::FunctionTemplate::New(isolate, ClientConnection_httpOptions));

  connection_proto->Set(
      isolate, "OPTIONS_RAW",
      v8::FunctionTemplate::New(isolate, ClientConnection_httpOptionsRaw));

  connection_proto->Set(
      isolate, "PATCH",
      v8::FunctionTemplate::New(isolate, ClientConnection_httpPatch));

  connection_proto->Set(
      isolate, "PATCH_RAW",
      v8::FunctionTemplate::New(isolate, ClientConnection_httpPatchRaw));

  connection_proto->Set(
      isolate, "POST",
      v8::FunctionTemplate::New(isolate, ClientConnection_httpPost));

  connection_proto->Set(
      isolate, "POST_RAW",
      v8::FunctionTemplate::New(isolate, ClientConnection_httpPostRaw));

  connection_proto->Set(
      isolate, "PUT",
      v8::FunctionTemplate::New(isolate, ClientConnection_httpPut));
  connection_proto->Set(
      isolate, "PUT_RAW",
      v8::FunctionTemplate::New(isolate, ClientConnection_httpPutRaw));

  connection_proto->Set(
      isolate, "SEND_FILE",
      v8::FunctionTemplate::New(isolate, ClientConnection_httpSendFile));

  connection_proto->Set(isolate, "getEndpoint",
                        v8::FunctionTemplate::New(
                            isolate, ClientConnection_getEndpoint, v8client));

  connection_proto->Set(
      isolate, "lastHttpReturnCode",
      v8::FunctionTemplate::New(isolate, ClientConnection_lastHttpReturnCode));

  connection_proto->Set(
      isolate, "lastErrorMessage",
      v8::FunctionTemplate::New(isolate, ClientConnection_lastErrorMessage));

  connection_proto->Set(
      isolate, "isConnected",
      v8::FunctionTemplate::New(isolate, ClientConnection_isConnected));

  connection_proto->Set(
      isolate, "reconnect",
      v8::FunctionTemplate::New(isolate, ClientConnection_reconnect, v8client));

  connection_proto->Set(isolate, "connectedUser",
                        v8::FunctionTemplate::New(
                            isolate, ClientConnection_connectedUser, v8client));

  connection_proto->Set(
      isolate, "toString",
      v8::FunctionTemplate::New(isolate, ClientConnection_toString));

  connection_proto->Set(
      isolate, "getVersion",
      v8::FunctionTemplate::New(isolate, ClientConnection_getVersion));

  connection_proto->Set(
      isolate, "getMode",
      v8::FunctionTemplate::New(isolate, ClientConnection_getMode));

  connection_proto->Set(
      isolate, "getDatabaseName",
      v8::FunctionTemplate::New(isolate, ClientConnection_getDatabaseName));

  connection_proto->Set(
      isolate, "setDatabaseName",
      v8::FunctionTemplate::New(isolate, ClientConnection_setDatabaseName,
                                v8client));

  connection_proto->Set(
      isolate, "importCsv",
      v8::FunctionTemplate::New(isolate, ClientConnection_importCsv, v8client));

  connection_proto->Set(isolate, "importJson",
                        v8::FunctionTemplate::New(
                            isolate, ClientConnection_importJson, v8client));

  connection_proto->SetCallAsFunctionHandler(
      ClientConnection_ConstructorCallback, v8client);

  v8::Local<v8::ObjectTemplate> connection_inst =
      connection_templ->InstanceTemplate();

  connection_inst->SetInternalFieldCount(2);

  TRI_AddGlobalVariableVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate, "ArangoConnection"),
                               connection_proto->NewInstance());

  ConnectionTempl.Reset(isolate, connection_inst);

  // add the client connection to the context:
  TRI_AddGlobalVariableVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate, "SYS_ARANGO"),
                               WrapV8ClientConnection(isolate, this));
}

void V8ClientConnection::shutdownConnection() {
  if (_connection) {
    _connection->cancel();
    _connection.reset();
  }
}
