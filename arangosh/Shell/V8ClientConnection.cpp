////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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

#include <boost/algorithm/string.hpp>
#include <fuerte/connection.h>
#include <fuerte/jwt.h>
#include <fuerte/requests.h>
#include <v8.h>

#include "Basics/FileUtils.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "ApplicationFeatures/V8SecurityFeature.h"
#include "Import/ImportHelper.h"
#include "Rest/GeneralResponse.h"
#include "Rest/Version.h"
#include "Shell/ClientFeature.h"
#include "Shell/ConsoleFeature.h"
#include "SimpleHttpClient/GeneralClientConnection.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/SimpleHttpResult.h"
#include "Ssl/SslInterface.h"
#include "V8/v8-conv.h"
#include "V8/v8-buffer.h"
#include "V8/v8-utils.h"
#include "V8/v8-vpack.h"
#include "V8/v8-deadline.h"

#include <velocypack/Builder.h>
#include <velocypack/Parser.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

#include <iostream>

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::basics;
using namespace arangodb::httpclient;
using namespace arangodb::import;

namespace fu = arangodb::fuerte;

V8ClientConnection::V8ClientConnection(application_features::ApplicationServer& server,
                                       ClientFeature& client)
    : _server(server),
      _client(client),
      _requestTimeout(_client.requestTimeout()),
      _lastHttpReturnCode(0),
      _lastErrorMessage(""),
      _version("arango"),
      _mode("unknown mode"),
      _role("UNKNOWN"),
      _loop(1, "V8ClientConnection"),
      _vpackOptions(VPackOptions::Defaults),
      _forceJson(false),
      _setCustomError(false) {
  _vpackOptions.buildUnindexedObjects = true;
  _vpackOptions.buildUnindexedArrays = true;

  _builder.maxConnectRetries(3);
  _builder.connectRetryPause(std::chrono::milliseconds(100));
  _builder.connectTimeout(std::chrono::milliseconds(static_cast<int64_t>(1000.0 * _client.connectionTimeout())));
  _builder.onFailure([this](fu::Error err, std::string const& msg) {
    // care only about connection errors
    if (err == fu::Error::CouldNotConnect ||
        err == fu::Error::VstUnauthorized ||
        err == fu::Error::ProtocolError) {
      std::unique_lock<std::recursive_mutex> guard(_lock, std::try_to_lock);
      if (guard && !_setCustomError) {
        _lastHttpReturnCode = 503;
        _lastErrorMessage = msg;
      }
      _setCustomError = false;
    }
  });
}

V8ClientConnection::~V8ClientConnection() {
  _builder.onFailure(nullptr);  // reset callback
  shutdownConnection();
  _loop.stop();
}

std::shared_ptr<fu::Connection> V8ClientConnection::createConnection() {
  if (_client.endpoint() == "none") {
    setCustomError(400, "no endpoint specified");
    return nullptr;
  }
  auto newConnection = _builder.connect(_loop);
  fu::StringMap params{{"details", "true"}};
  auto req = fu::createRequest(fu::RestVerb::Get, "/_api/version", params);
  req->header.database = _databaseName;
  req->timeout(std::chrono::seconds(30));
  try {
    auto res = newConnection->sendRequest(std::move(req));

    if (!res) {
      setCustomError(500, "unable to create connection");
      return nullptr;
    }

    _lastHttpReturnCode = res->statusCode();

    std::shared_ptr<VPackBuilder> parsedBody;
    VPackSlice body;
    if (res->contentType() == fu::ContentType::VPack) {
      body = res->slice();
    } else if (res->contentType() == fu::ContentType::Json) {
      parsedBody =
          VPackParser::fromJson(reinterpret_cast<char const*>(res->payload().data()),
                                res->payload().size());
      body = parsedBody->slice();
    }
    if (_lastHttpReturnCode >= 400) {
      auto const& headers = res->messageHeader().meta();
      auto it = headers.find("http/1.1");
      if (it != headers.end()) {
        std::string errorMessage = (*it).second;
        if (body.isObject()) {
          std::string const msg =
            VelocyPackHelper::getStringValue(body, StaticStrings::ErrorMessage, "");
          if (!msg.empty()) {
            errorMessage = msg;
          }
        }
        setCustomError(_lastHttpReturnCode, errorMessage);
        return nullptr;
      }
    }

    if (!body.isObject()) {
      std::string msg("invalid response: '");
      msg += std::string(reinterpret_cast<char const*>(res->payload().data()),
                         res->payload().size());
      msg += "'";
      setCustomError(503, msg);
      return nullptr;
    }

    std::lock_guard<std::recursive_mutex> guard(_lock);
    _connection = newConnection;

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
        VPackSlice role = details.get("role");
        if (role.isString()) {
          _role = role.copyString();
        }
      }
      if (!body.hasKey("version")) {
        // if we don't get a version number in return, the server is
        // probably running in hardened mode
        return newConnection;
      }
      std::string const versionString =
          VelocyPackHelper::getStringValue(body, "version", "");
      std::pair<int, int> version = rest::Version::parseVersionString(versionString);
      if (version.first < 3) {
        // major version of server is too low
        //_client->disconnect();
        shutdownConnection();
        std::string msg("Server version number ('" + versionString +
                        "') is too low. Expecting 3.0 or higher");;
        setCustomError(500, msg);
        return newConnection;
      }
    }
    return _connection;
  } catch (fu::Error const& e) {  // connection error
    std::string msg(fu::to_string(e));
    setCustomError(503, msg);
    return nullptr;
  }
}

std::shared_ptr<fu::Connection> V8ClientConnection::acquireConnection() {
  std::lock_guard<std::recursive_mutex> guard(_lock);

  _lastErrorMessage = "";
  _lastHttpReturnCode = 0;

  if (!_connection ||
      (_connection->state() == fu::Connection::State::Closed)) {
    return createConnection();
  }
  return _connection;
}

void V8ClientConnection::setInterrupted(bool interrupted) {
  std::lock_guard<std::recursive_mutex> guard(_lock);
  if (interrupted && _connection != nullptr) {
    shutdownConnection();
  } else if (!interrupted && (_connection == nullptr ||
                              (_connection->state() == fu::Connection::State::Closed))) {
    createConnection();
  }
}

bool V8ClientConnection::isConnected() const {
  std::lock_guard<std::recursive_mutex> guard(_lock);
  if (_connection) {
    if (_connection->state() == fu::Connection::State::Connected) {
      return true;
    }
    // the client might have automatically closed the connection,
    // as long as there was no error we can reconnect
    return _lastHttpReturnCode < 400;
  }
  return false;
}

std::string V8ClientConnection::endpointSpecification() const {
  std::lock_guard<std::recursive_mutex> guard(_lock);
  if (_connection) {
    return _connection->endpoint();
  }
  return "";
}

application_features::ApplicationServer& V8ClientConnection::server() {
  return _server;
}

double V8ClientConnection::timeout() const { return _requestTimeout.count(); }

void V8ClientConnection::timeout(double value) {
  _requestTimeout = std::chrono::duration<double>(value);
}

void V8ClientConnection::connect() {
  std::lock_guard<std::recursive_mutex> guard(_lock);
  _forceJson = _client.forceJson();
  _requestTimeout = std::chrono::duration<double>(_client.requestTimeout());
  _databaseName = _client.databaseName();
  _builder.endpoint(_client.endpoint());
  // check jwtSecret first, as it is empty by default,
  // but username defaults to "root" in most configurations
  if (!_client.jwtSecret().empty()) {
    _builder.jwtToken(
        fu::jwt::generateInternalToken(_client.jwtSecret(), "arangosh"));
    _builder.authenticationType(fu::AuthenticationType::Jwt);
  } else if (!_client.username().empty()) {
    _builder.user(_client.username()).password(_client.password());
    _builder.authenticationType(fu::AuthenticationType::Basic);
  }
  createConnection();
}

void V8ClientConnection::reconnect() {
  std::lock_guard<std::recursive_mutex> guard(_lock);

  _requestTimeout = std::chrono::duration<double>(_client.requestTimeout());
  _databaseName = _client.databaseName();
  _builder.endpoint(_client.endpoint());
  _forceJson = _client.forceJson();
  // check jwtSecret first, as it is empty by default,
  // but username defaults to "root" in most configurations
  if (!_client.jwtSecret().empty()) {
    _builder.jwtToken(
        fu::jwt::generateInternalToken(_client.jwtSecret(), "arangosh"));
    _builder.authenticationType(fu::AuthenticationType::Jwt);
  } else if (!_client.username().empty()) {
    _builder.user(_client.username()).password(_client.password());
    _builder.authenticationType(fu::AuthenticationType::Basic);
  }

  std::shared_ptr<fu::Connection> oldConnection;
  _connection.swap(oldConnection);
  if (oldConnection) {
    oldConnection->cancel();
  }
  oldConnection.reset();
  try {
    createConnection();
  } catch (...) {
    std::string errorMessage = "error in '" + _client.endpoint() + "'";
    throw errorMessage;
  }

  if (isConnected() && _lastHttpReturnCode == static_cast<int>(rest::ResponseCode::OK)) {
    LOG_TOPIC("2d416", INFO, arangodb::Logger::FIXME)
        << ClientFeature::buildConnectedMessage(endpointSpecification(), _version, _role, _mode, _databaseName, _client.username());
  } else {
    if (_client.getWarnConnect()) {
      LOG_TOPIC("9d7ea", ERR, arangodb::Logger::FIXME)
          << "Could not connect to endpoint '" << _client.endpoint()
          << "', username: '" << _client.username() << "' - Server message: " <<
        _lastErrorMessage;
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
    v8::Local<v8::Array> const props = v8Headers->GetPropertyNames(TRI_IGETC).FromMaybe(v8::Local<v8::Array>());
    auto context = TRI_IGETC;
    for (uint32_t i = 0; i < props->Length(); i++) {
      v8::Local<v8::Value> key = props->Get(context, i).FromMaybe(v8::Local<v8::Value>());
      myMap.emplace(TRI_ObjectToString(isolate, key),
                    TRI_ObjectToString(isolate, v8Headers->Get(context, key).FromMaybe(v8::Local<v8::Value>())));
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief weak reference callback for connections (call the destructor here)
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
/// @brief weak reference callback for connections (call the destructor here)
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_DestructorCallback(
    const v8::WeakCallbackInfo<v8::Persistent<v8::External>>& data) {
  auto persistent = data.GetParameter();
  auto myConnection = v8::Local<v8::External>::New(data.GetIsolate(), *persistent);
  auto v8connection = static_cast<V8ClientConnection*>(myConnection->Value());

  DestroyV8ClientConnection(v8connection);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief wrap V8ClientConnection in a v8::Object
////////////////////////////////////////////////////////////////////////////////

static v8::Local<v8::Value> WrapV8ClientConnection(v8::Isolate* isolate,
                                                   V8ClientConnection* v8connection) {
  v8::EscapableHandleScope scope(isolate);
  auto localConnectionTempl = v8::Local<v8::ObjectTemplate>::New(isolate, ConnectionTempl);
  v8::Local<v8::Object> result = localConnectionTempl->NewInstance(TRI_IGETC).FromMaybe(v8::Local<v8::Object>());

  auto myConnection = v8::External::New(isolate, v8connection);
  result->SetInternalField(SLOT_CLASS_TYPE, v8::Integer::New(isolate, WRAP_TYPE_CONNECTION));
  result->SetInternalField(SLOT_CLASS, myConnection);
  Connections[v8connection].Reset(isolate, myConnection);
  Connections[v8connection].SetWeak(&Connections[v8connection], ClientConnection_DestructorCallback,
                                    v8::WeakCallbackType::kFinalizer);
  return scope.Escape<v8::Value>(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection constructor
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_ConstructorCallback(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  v8::Local<v8::External> wrap = v8::Local<v8::External>::Cast(args.Data());
  ClientFeature* client = static_cast<ClientFeature*>(wrap->Value());

  auto v8connection = std::make_unique<V8ClientConnection>(client->server(), *client);
  v8connection->connect();

  if (v8connection->isConnected() &&
      v8connection->lastHttpReturnCode() == (int)rest::ResponseCode::OK) {
    LOG_TOPIC("9c8b4", INFO, arangodb::Logger::FIXME)
        << ClientFeature::buildConnectedMessage(v8connection->endpointSpecification(), v8connection->version(),
                                                v8connection->role(), v8connection->mode(), v8connection->databaseName(),
                                                v8connection->username());
  } else {
    std::string errorMessage =
        "Could not connect. Error message: " + v8connection->lastErrorMessage();

    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_SIMPLE_CLIENT_COULD_NOT_CONNECT,
                                   errorMessage);
  }

  TRI_V8_RETURN(WrapV8ClientConnection(isolate, v8connection.release()));
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "reconnect"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_reconnect(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);
  if (isExecutionDeadlineReached(isolate)) {
    return;
  }

  V8ClientConnection* v8connection =
      TRI_UnwrapClass<V8ClientConnection>(args.Holder(), WRAP_TYPE_CONNECTION, TRI_IGETC);

  v8::Local<v8::External> wrap = v8::Local<v8::External>::Cast(args.Data());
  ClientFeature* client = static_cast<ClientFeature*>(wrap->Value());

  if (v8connection == nullptr || client == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("reconnect() must be invoked on an arango connection object instance.");
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
      ConsoleFeature& console = v8connection->server().getFeature<ConsoleFeature>();

      if (console.isEnabled()) {
        password = console.readPassword("Please specify a password: ");
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
    warnConnect = TRI_ObjectToBoolean(isolate, args[4]);
  }

  V8SecurityFeature& v8security = v8connection->server().getFeature<V8SecurityFeature>();
  if (!v8security.isAllowedToConnectToEndpoint(isolate, endpoint, endpoint)) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_FORBIDDEN,
                                   std::string("not allowed to connect to this endpoint") +
                                   endpoint);
  }

  client->setEndpoint(endpoint);
  client->setDatabaseName(databaseName);
  client->setUsername(username);
  client->setPassword(password);
  client->setWarnConnect(warnConnect);

  try {
    v8connection->reconnect();
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

static void ClientConnection_connectedUser(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  v8::Local<v8::External> wrap = v8::Local<v8::External>::Cast(args.Data());
  ClientFeature* client = static_cast<ClientFeature*>(wrap->Value());

  if (client == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("connectedUser() must be invoked on an arango connection object instance.");
  }

  TRI_V8_RETURN(TRI_V8_STD_STRING(isolate, client->username()));
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "GET" helper
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpGetAny(v8::FunctionCallbackInfo<v8::Value> const& args,
                                        bool raw) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (isExecutionDeadlineReached(isolate)) {
    return;
  }
  // get the connection
  V8ClientConnection* v8connection =
      TRI_UnwrapClass<V8ClientConnection>(args.Holder(), WRAP_TYPE_CONNECTION, TRI_IGETC);

  if (v8connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("get() must be invoked on an arango connection object instance.");
  }

  // check params
  if (args.Length() < 1 || args.Length() > 2 || !args[0]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE("get(<url>[, <headers>])");
  }

  TRI_Utf8ValueNFC url(isolate, args[0]);
  // check header fields
  std::unordered_map<std::string, std::string> headerFields;

  if (args.Length() > 1) {
    ObjectToMap(isolate, headerFields, args[1]);
  }

  TRI_V8_RETURN(v8connection->getData(isolate, arangodb::velocypack::StringRef(*url, url.length()),
                                      headerFields, raw));
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "GET"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpGet(v8::FunctionCallbackInfo<v8::Value> const& args) {
  ClientConnection_httpGetAny(args, false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "GET_RAW"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpGetRaw(v8::FunctionCallbackInfo<v8::Value> const& args) {
  ClientConnection_httpGetAny(args, true);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "HEAD" helper
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpHeadAny(v8::FunctionCallbackInfo<v8::Value> const& args,
                                         bool raw) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);
  if (isExecutionDeadlineReached(isolate)) {
    return;
  }

  // get the connection
  V8ClientConnection* v8connection =
      TRI_UnwrapClass<V8ClientConnection>(args.Holder(), WRAP_TYPE_CONNECTION, TRI_IGETC);

  if (v8connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("head() must be invoked on an arango connection object instance.");
  }

  // check params
  if (args.Length() < 1 || args.Length() > 2 || !args[0]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE("head(<url>[, <headers>])");
  }

  TRI_Utf8ValueNFC url(isolate, args[0]);

  // check header fields
  std::unordered_map<std::string, std::string> headerFields;

  if (args.Length() > 1) {
    ObjectToMap(isolate, headerFields, args[1]);
  }

  TRI_V8_RETURN(v8connection->headData(isolate, arangodb::velocypack::StringRef(*url, url.length()),
                                       headerFields, raw));
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "HEAD"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpHead(v8::FunctionCallbackInfo<v8::Value> const& args) {
  ClientConnection_httpHeadAny(args, false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "HEAD_RAW"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpHeadRaw(v8::FunctionCallbackInfo<v8::Value> const& args) {
  ClientConnection_httpHeadAny(args, true);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "DELETE" helper
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpDeleteAny(v8::FunctionCallbackInfo<v8::Value> const& args,
                                           bool raw) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);
  if (isExecutionDeadlineReached(isolate)) {
    return;
  }

  // get the connection
  V8ClientConnection* v8connection =
      TRI_UnwrapClass<V8ClientConnection>(args.Holder(), WRAP_TYPE_CONNECTION, TRI_IGETC);

  if (v8connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("delete() must be invoked on an arango connection object instance.");
  }

  // check params
  if (args.Length() < 1 || args.Length() > 3 || !args[0]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE("delete(<url>[, <body>[, <headers>]])");
  }

  TRI_Utf8ValueNFC url(isolate, args[0]);

  std::unordered_map<std::string, std::string> headerFields;
  if (args.Length() == 1) {  // no body provided
    TRI_V8_RETURN(v8connection->deleteData(isolate, arangodb::velocypack::StringRef(*url, url.length()),
                                           v8::Undefined(isolate), headerFields, raw));
  }

  // check header fields
  if (args.Length() > 2) {
    ObjectToMap(isolate, headerFields, args[2]);
  }

  TRI_V8_RETURN(v8connection->deleteData(isolate, arangodb::velocypack::StringRef(*url, url.length()),
                                         args[1], headerFields, raw));
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "DELETE"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpDelete(v8::FunctionCallbackInfo<v8::Value> const& args) {
  ClientConnection_httpDeleteAny(args, false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "DELETE_RAW"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpDeleteRaw(v8::FunctionCallbackInfo<v8::Value> const& args) {
  ClientConnection_httpDeleteAny(args, true);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "OPTIONS" helper
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpOptionsAny(v8::FunctionCallbackInfo<v8::Value> const& args,
                                            bool raw) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);
  if (isExecutionDeadlineReached(isolate)) {
    return;
  }

  // get the connection
  V8ClientConnection* v8connection =
      TRI_UnwrapClass<V8ClientConnection>(args.Holder(), WRAP_TYPE_CONNECTION, TRI_IGETC);

  if (v8connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("options() must be invoked on an arango connection object instance.");
  }

  // check params
  if (args.Length() < 2 || args.Length() > 3 || !args[0]->IsString() ||
      args[1]->IsUndefined()) {
    TRI_V8_THROW_EXCEPTION_USAGE("options(<url>, <body>[, <headers>])");
  }

  TRI_Utf8ValueNFC url(isolate, args[0]);

  // check header fields
  std::unordered_map<std::string, std::string> headerFields;
  if (args.Length() > 2) {
    ObjectToMap(isolate, headerFields, args[2]);
  }

  TRI_V8_RETURN(v8connection->optionsData(isolate, arangodb::velocypack::StringRef(*url, url.length()),
                                          args[1], headerFields, raw));
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "OPTIONS"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpOptions(v8::FunctionCallbackInfo<v8::Value> const& args) {
  ClientConnection_httpOptionsAny(args, false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "OPTIONS_RAW"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpOptionsRaw(v8::FunctionCallbackInfo<v8::Value> const& args) {
  ClientConnection_httpOptionsAny(args, true);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "POST" helper
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpPostAny(v8::FunctionCallbackInfo<v8::Value> const& args,
                                         bool raw) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);
  if (isExecutionDeadlineReached(isolate)) {
    return;
  }

  // get the connection
  V8ClientConnection* v8connection =
      TRI_UnwrapClass<V8ClientConnection>(args.Holder(), WRAP_TYPE_CONNECTION, TRI_IGETC);

  if (v8connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("post() must be invoked on an arango connection object instance.");
  }

  // check params
  if (args.Length() < 2 || args.Length() > 3 || !args[0]->IsString() ||
      args[1]->IsUndefined()) {
    TRI_V8_THROW_EXCEPTION_USAGE("post(<url>, <body>[, <headers>])");
  }

  TRI_Utf8ValueNFC url(isolate, args[0]);
  // check header fields
  std::unordered_map<std::string, std::string> headerFields;
  if (args.Length() > 2) {
    ObjectToMap(isolate, headerFields, args[2]);
  }

  TRI_V8_RETURN(v8connection->postData(isolate, arangodb::velocypack::StringRef(*url, url.length()),
                                       args[1], headerFields, raw));
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "POST"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpPost(v8::FunctionCallbackInfo<v8::Value> const& args) {
  ClientConnection_httpPostAny(args, false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "POST_RAW"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpPostRaw(v8::FunctionCallbackInfo<v8::Value> const& args) {
  ClientConnection_httpPostAny(args, true);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "PUT" helper
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpPutAny(v8::FunctionCallbackInfo<v8::Value> const& args,
                                        bool raw) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);
  if (isExecutionDeadlineReached(isolate)) {
    return;
  }

  // get the connection
  V8ClientConnection* v8connection =
      TRI_UnwrapClass<V8ClientConnection>(args.Holder(), WRAP_TYPE_CONNECTION, TRI_IGETC);

  if (v8connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("put() must be invoked on an arango connection object instance.");
  }

  // check params
  if (args.Length() < 2 || args.Length() > 3 || !args[0]->IsString() ||
      args[1]->IsUndefined()) {
    TRI_V8_THROW_EXCEPTION_USAGE("put(<url>, <body>[, <headers>])");
  }

  TRI_Utf8ValueNFC url(isolate, args[0]);

  // check header fields
  std::unordered_map<std::string, std::string> headerFields;
  if (args.Length() > 2) {
    ObjectToMap(isolate, headerFields, args[2]);
  }

  TRI_V8_RETURN(v8connection->putData(isolate, arangodb::velocypack::StringRef(*url, url.length()),
                                      args[1], headerFields, raw));
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "PUT"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpPut(v8::FunctionCallbackInfo<v8::Value> const& args) {
  ClientConnection_httpPutAny(args, false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "PUT_RAW"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpPutRaw(v8::FunctionCallbackInfo<v8::Value> const& args) {
  ClientConnection_httpPutAny(args, true);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "PATCH" helper
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpPatchAny(v8::FunctionCallbackInfo<v8::Value> const& args,
                                          bool raw) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (isExecutionDeadlineReached(isolate)) {
    return;
  }
  // get the connection
  V8ClientConnection* v8connection =
      TRI_UnwrapClass<V8ClientConnection>(args.Holder(), WRAP_TYPE_CONNECTION, TRI_IGETC);

  if (v8connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("patch() must be invoked on an arango connection object instance.");
  }

  // check params
  if (args.Length() < 2 || args.Length() > 3 || !args[0]->IsString() ||
      args[1]->IsUndefined()) {
    TRI_V8_THROW_EXCEPTION_USAGE("patch(<url>, <body>[, <headers>])");
  }

  TRI_Utf8ValueNFC url(isolate, args[0]);
  // check header fields
  std::unordered_map<std::string, std::string> headerFields;
  if (args.Length() > 2) {
    ObjectToMap(isolate, headerFields, args[2]);
  }

  TRI_V8_RETURN(v8connection->patchData(isolate, arangodb::velocypack::StringRef(*url, url.length()),
                                        args[1], headerFields, raw));
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "PATCH"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpPatch(v8::FunctionCallbackInfo<v8::Value> const& args) {
  ClientConnection_httpPatchAny(args, false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "PATCH_RAW"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpPatchRaw(v8::FunctionCallbackInfo<v8::Value> const& args) {
  ClientConnection_httpPatchAny(args, true);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection send file helper
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpSendFile(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (isExecutionDeadlineReached(isolate)) {
    return;
  }

  // get the connection
  V8ClientConnection* v8connection =
      TRI_UnwrapClass<V8ClientConnection>(args.Holder(), WRAP_TYPE_CONNECTION, TRI_IGETC);

  if (v8connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("sendFile() must be invoked on an arango connection object instance.");
  }

  // check params
  if (args.Length() < 2 ||
      args.Length() > 3 ||
      !args[0]->IsString() ||
      args[1]->IsUndefined()) {
    TRI_V8_THROW_EXCEPTION_USAGE("sendFile(<url>, <file>[, <headers>])");
  }

  TRI_Utf8ValueNFC url(isolate, args[0]);

  std::string const infile = TRI_ObjectToString(isolate, args[1]);

  if (!FileUtils::exists(infile)) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_FILE_NOT_FOUND);
  }

  v8::TryCatch tryCatch(isolate);

  // check header fields
  std::unordered_map<std::string, std::string> headerFields;
    // check header fields
  if (args.Length() > 2) {
    ObjectToMap(isolate, headerFields, args[2]);
  }

  v8::Local<v8::Value> result =
      v8connection->postData(isolate, arangodb::velocypack::StringRef(*url, url.length()), args[1],
                             headerFields, false, /*isFile*/ true);

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

static void ClientConnection_getEndpoint(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate)
  v8::HandleScope scope(isolate);

  if (isExecutionDeadlineReached(isolate)) {
    return;
  }
  // get the connection
  V8ClientConnection* v8connection =
      TRI_UnwrapClass<V8ClientConnection>(args.Holder(), WRAP_TYPE_CONNECTION, TRI_IGETC);

  v8::Local<v8::External> wrap = v8::Local<v8::External>::Cast(args.Data());
  ClientFeature* client = static_cast<ClientFeature*>(wrap->Value());

  if (v8connection == nullptr || client == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("getEndpoint() must be invoked on an arango connection object instance.");
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

static void ClientConnection_importCsv(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::HandleScope scope(isolate);

  if (isExecutionDeadlineReached(isolate)) {
    return;
  }
  if (args.Length() < 2) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "importCsvFile(<filename>, <collection>[, <options>])");
  }

  // extract the filename
  v8::String::Utf8Value filename(isolate, args[0]);

  if (*filename == nullptr) {
    TRI_V8_THROW_TYPE_ERROR("<filename> must be a UTF-8 filename");
  }

  v8::String::Utf8Value collection(isolate, args[1]);

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
    v8::Local<v8::Object> options = TRI_ToObject(context, args[2]);
    // separator
    if (TRI_HasProperty(context, isolate, options, separatorKey)) {
      separator = TRI_ObjectToString(isolate, options->Get(context, separatorKey).FromMaybe(v8::Local<v8::Value>()));

      if (separator.length() < 1) {
        TRI_V8_THROW_EXCEPTION_PARAMETER(
            "<options>.separator must be at least one character");
      }
    }

    // quote
    if (TRI_HasProperty(context, isolate, options, quoteKey)) {
      quote = TRI_ObjectToString(isolate, options->Get(context, quoteKey).FromMaybe(v8::Local<v8::Value>()));

      if (quote.length() > 1) {
        TRI_V8_THROW_EXCEPTION_PARAMETER(
            "<options>.quote must be at most one character");
      }
    }
  }

  V8ClientConnection* v8connection =
      TRI_UnwrapClass<V8ClientConnection>(args.Holder(), WRAP_TYPE_CONNECTION, TRI_IGETC);

  v8::Local<v8::External> wrap = v8::Local<v8::External>::Cast(args.Data());
  ClientFeature* client = static_cast<ClientFeature*>(wrap->Value());

  SimpleHttpClientParams params(client->requestTimeout(), client->getWarn());
  ImportHelper ih(*client, v8connection->endpointSpecification(), params,
                  DefaultChunkSize, 1);

  ih.setQuote(quote);
  ih.setSeparator(separator.c_str());

  std::string fileName = TRI_ObjectToString(isolate, args[0]);
  std::string collectionName = TRI_ObjectToString(isolate, args[1]);

  if (ih.importDelimited(collectionName, fileName, ImportHelper::CSV)) {
    v8::Local<v8::Object> result = v8::Object::New(isolate);

    result->Set(context,
                TRI_V8_ASCII_STRING(isolate, "lines"),
                v8::Integer::New(isolate, (int32_t)ih.getReadLines())).FromMaybe(false);

    result->Set(context,
                TRI_V8_ASCII_STRING(isolate, "created"),
                v8::Integer::New(isolate, (int32_t)ih.getNumberCreated())).FromMaybe(false);

    result->Set(context,
                TRI_V8_ASCII_STRING(isolate, "errors"),
                v8::Integer::New(isolate, (int32_t)ih.getNumberErrors())).FromMaybe(false);

    result->Set(context,
                TRI_V8_ASCII_STRING(isolate, "updated"),
                v8::Integer::New(isolate, (int32_t)ih.getNumberUpdated())).FromMaybe(false);

    result->Set(context,
                TRI_V8_ASCII_STRING(isolate, "ignored"),
                v8::Integer::New(isolate, (int32_t)ih.getNumberIgnored())).FromMaybe(false);

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

static void ClientConnection_importJson(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (isExecutionDeadlineReached(isolate)) {
    return;
  }
  if (args.Length() < 2) {
    TRI_V8_THROW_EXCEPTION_USAGE("importJsonFile(<filename>, <collection>)");
  }

  // extract the filename
  v8::String::Utf8Value filename(isolate, args[0]);

  if (*filename == nullptr) {
    TRI_V8_THROW_TYPE_ERROR("<filename> must be a UTF-8 filename");
  }

  v8::String::Utf8Value collection(isolate, args[1]);

  if (*collection == nullptr) {
    TRI_V8_THROW_TYPE_ERROR("<collection> must be a UTF-8 filename");
  }

  V8ClientConnection* v8connection =
      TRI_UnwrapClass<V8ClientConnection>(args.Holder(), WRAP_TYPE_CONNECTION, TRI_IGETC);

  v8::Local<v8::External> wrap = v8::Local<v8::External>::Cast(args.Data());
  ClientFeature* client = static_cast<ClientFeature*>(wrap->Value());

  SimpleHttpClientParams params(client->requestTimeout(), client->getWarn());
  ImportHelper ih(*client, v8connection->endpointSpecification(), params,
                  DefaultChunkSize, 1);

  std::string fileName = TRI_ObjectToString(isolate, args[0]);
  std::string collectionName = TRI_ObjectToString(isolate, args[1]);
  auto context = TRI_IGETC;

  if (ih.importJson(collectionName, fileName, false)) {
    v8::Local<v8::Object> result = v8::Object::New(isolate);

    result->Set(context,
                TRI_V8_ASCII_STRING(isolate, "lines"),
                v8::Integer::New(isolate, (int32_t)ih.getReadLines())).FromMaybe(false);

    result->Set(context,
                TRI_V8_ASCII_STRING(isolate, "created"),
                v8::Integer::New(isolate, (int32_t)ih.getNumberCreated())).FromMaybe(false);

    result->Set(context,
                TRI_V8_ASCII_STRING(isolate, "errors"),
                v8::Integer::New(isolate, (int32_t)ih.getNumberErrors())).FromMaybe(false);

    result->Set(context,
                TRI_V8_ASCII_STRING(isolate, "updated"),
                v8::Integer::New(isolate, (int32_t)ih.getNumberUpdated())).FromMaybe(false);

    result->Set(context,
                TRI_V8_ASCII_STRING(isolate, "ignored"),
                v8::Integer::New(isolate, (int32_t)ih.getNumberIgnored())).FromMaybe(false);

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

static void ClientConnection_lastHttpReturnCode(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (isExecutionDeadlineReached(isolate)) {
    return;
  }

  // get the connection
  V8ClientConnection* v8connection =
      TRI_UnwrapClass<V8ClientConnection>(args.Holder(), WRAP_TYPE_CONNECTION, TRI_IGETC);

  if (v8connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("lastHttpReturnCode() must be invoked on an arango connection object instance.");
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

static void ClientConnection_lastErrorMessage(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (isExecutionDeadlineReached(isolate)) {
    return;
  }

  // get the connection
  V8ClientConnection* v8connection =
      TRI_UnwrapClass<V8ClientConnection>(args.Holder(), WRAP_TYPE_CONNECTION, TRI_IGETC);

  if (v8connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("lastErrorMessage() must be invoked on an arango connection object instance.");
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

static void ClientConnection_isConnected(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (isExecutionDeadlineReached(isolate)) {
    return;
  }

  // get the connection
  V8ClientConnection* v8connection =
      TRI_UnwrapClass<V8ClientConnection>(args.Holder(), WRAP_TYPE_CONNECTION, TRI_IGETC);

  if (v8connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("isConnected() must be invoked on an arango connection object instance.");
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
/// @brief ClientConnection method "forceJson"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_forceJson(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (isExecutionDeadlineReached(isolate)) {
    return;
  }

  // get the connection
  V8ClientConnection* v8connection =
      TRI_UnwrapClass<V8ClientConnection>(args.Holder(), WRAP_TYPE_CONNECTION, TRI_IGETC);

  if (v8connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("forceJson() must be invoked on an arango connection object instance.");
  }

  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("forceJson(bool)");
  }

  v8::Local<v8::External> wrap = v8::Local<v8::External>::Cast(args.Data());
  ClientFeature* client = static_cast<ClientFeature*>(wrap->Value());

  if (client == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("forceJson() unable to get client instance");
  }

  bool forceJson = TRI_ObjectToBoolean(isolate, args[0]);
  v8connection->setForceJson(forceJson);

  client->setForceJson(forceJson);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "timeout"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_timeout(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (isExecutionDeadlineReached(isolate)) {
    return;
  }

  // get the connection
  V8ClientConnection* v8connection =
      TRI_UnwrapClass<V8ClientConnection>(args.Holder(), WRAP_TYPE_CONNECTION, TRI_IGETC);

  if (v8connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("timeout() must be invoked on an arango connection object instance.");
  }

  if (args.Length() == 0) {
    TRI_V8_RETURN(v8::Number::New(isolate, v8connection->timeout()));
  } else {
    double value = TRI_ObjectToDouble(isolate, args[0]);
    v8connection->timeout(value);

    v8::Local<v8::External> wrap = v8::Local<v8::External>::Cast(args.Data());
    ClientFeature* client = static_cast<ClientFeature*>(wrap->Value());

    if (client == nullptr) {
      TRI_V8_THROW_EXCEPTION_INTERNAL("timeout() unable to get client instance");
    }

    client->requestTimeout(value);

    TRI_V8_RETURN_UNDEFINED();
  }

  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "toString"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_toString(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (isExecutionDeadlineReached(isolate)) {
    return;
  }

  // get the connection
  V8ClientConnection* v8connection =
      TRI_UnwrapClass<V8ClientConnection>(args.Holder(), WRAP_TYPE_CONNECTION, TRI_IGETC);

  if (v8connection == nullptr) {
    // when invoking ArangoConnection.toString() we end here, i.e. printObject does this.
    // be silent about this.
    isolate->ThrowException(v8::Object::New(isolate));
    return;
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

static void ClientConnection_getVersion(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (isExecutionDeadlineReached(isolate)) {
    return;
  }

  // get the connection
  V8ClientConnection* v8connection =
      TRI_UnwrapClass<V8ClientConnection>(args.Holder(), WRAP_TYPE_CONNECTION, TRI_IGETC);

  if (v8connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("getVersion() must be invoked on an arango connection object instance.");
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

static void ClientConnection_getMode(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (isExecutionDeadlineReached(isolate)) {
    return;
  }

  // get the connection
  V8ClientConnection* v8connection =
      TRI_UnwrapClass<V8ClientConnection>(args.Holder(), WRAP_TYPE_CONNECTION, TRI_IGETC);

  if (v8connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("getMode() must be invoked on an arango connection object instance.");
  }

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("getMode()");
  }

  TRI_V8_RETURN_STD_STRING(v8connection->mode());
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "getRole"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_getRole(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (isExecutionDeadlineReached(isolate)) {
    return;
  }

  // get the connection
  V8ClientConnection* v8connection =
      TRI_UnwrapClass<V8ClientConnection>(args.Holder(), WRAP_TYPE_CONNECTION, TRI_IGETC);

  if (v8connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("getRole() must be invoked on an arango connection object instance.");
  }

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("getRole()");
  }

  TRI_V8_RETURN_STD_STRING(v8connection->role());
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "getDatabaseName"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_getDatabaseName(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (isExecutionDeadlineReached(isolate)) {
    return;
  }

  // get the connection
  V8ClientConnection* v8connection =
      TRI_UnwrapClass<V8ClientConnection>(args.Holder(), WRAP_TYPE_CONNECTION, TRI_IGETC);

  if (v8connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("getDatabaseName() must be invoked on an arango connection object instance.");
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

static void ClientConnection_setDatabaseName(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (isExecutionDeadlineReached(isolate)) {
    return;
  }

  // get the connection
  V8ClientConnection* v8connection =
      TRI_UnwrapClass<V8ClientConnection>(args.Holder(), WRAP_TYPE_CONNECTION, TRI_IGETC);

  v8::Local<v8::External> wrap = v8::Local<v8::External>::Cast(args.Data());
  ClientFeature* client = static_cast<ClientFeature*>(wrap->Value());

  if (v8connection == nullptr || client == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("setDatabaseName() must be invoked on an arango connection object instance.");
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
    v8::Isolate* isolate, arangodb::velocypack::StringRef const& location,
    std::unordered_map<std::string, std::string> const& headerFields, bool raw) {
  if (raw) {
    return requestDataRaw(isolate, fu::RestVerb::Get, location,
                          v8::Undefined(isolate), headerFields);
  }
  return requestData(isolate, fu::RestVerb::Get, location,
                     v8::Undefined(isolate), headerFields);
}

v8::Local<v8::Value> V8ClientConnection::headData(
    v8::Isolate* isolate, arangodb::velocypack::StringRef const& location,
    std::unordered_map<std::string, std::string> const& headerFields, bool raw) {
  if (raw) {
    return requestDataRaw(isolate, fu::RestVerb::Head, location,
                          v8::Undefined(isolate), headerFields);
  }
  return requestData(isolate, fu::RestVerb::Head, location,
                     v8::Undefined(isolate), headerFields);
}

v8::Local<v8::Value> V8ClientConnection::deleteData(
    v8::Isolate* isolate, arangodb::velocypack::StringRef const& location, v8::Local<v8::Value> const& body,
    std::unordered_map<std::string, std::string> const& headerFields, bool raw) {
  if (raw) {
    return requestDataRaw(isolate, fu::RestVerb::Delete, location, body, headerFields);
  }
  return requestData(isolate, fu::RestVerb::Delete, location, body, headerFields);
}

v8::Local<v8::Value> V8ClientConnection::optionsData(
    v8::Isolate* isolate, arangodb::velocypack::StringRef const& location, v8::Local<v8::Value> const& body,
    std::unordered_map<std::string, std::string> const& headerFields, bool raw) {
  if (raw) {
    return requestDataRaw(isolate, fu::RestVerb::Options, location, body, headerFields);
  }
  return requestData(isolate, fu::RestVerb::Options, location, body, headerFields);
}

v8::Local<v8::Value> V8ClientConnection::postData(
    v8::Isolate* isolate, arangodb::velocypack::StringRef const& location, v8::Local<v8::Value> const& body,
    std::unordered_map<std::string, std::string> const& headerFields, bool raw, bool isFile) {
  if (raw) {
    return requestDataRaw(isolate, fu::RestVerb::Post, location, body, headerFields);
  }
  return requestData(isolate, fu::RestVerb::Post, location, body, headerFields, isFile);
}

v8::Local<v8::Value> V8ClientConnection::putData(
    v8::Isolate* isolate, arangodb::velocypack::StringRef const& location, v8::Local<v8::Value> const& body,
    std::unordered_map<std::string, std::string> const& headerFields, bool raw) {
  if (raw) {
    return requestDataRaw(isolate, fu::RestVerb::Put, location, body, headerFields);
  }
  return requestData(isolate, fu::RestVerb::Put, location, body, headerFields);
}

v8::Local<v8::Value> V8ClientConnection::patchData(
    v8::Isolate* isolate, arangodb::velocypack::StringRef const& location, v8::Local<v8::Value> const& body,
    std::unordered_map<std::string, std::string> const& headerFields, bool raw) {
  if (raw) {
    return requestDataRaw(isolate, fu::RestVerb::Patch, location, body, headerFields);
  }
  return requestData(isolate, fu::RestVerb::Patch, location, body, headerFields);
}

v8::Local<v8::Value> V8ClientConnection::requestData(
    v8::Isolate* isolate, fu::RestVerb method, arangodb::velocypack::StringRef const& location,
    v8::Local<v8::Value> const& body,
    std::unordered_map<std::string, std::string> const& headerFields, bool isFile) {

  bool retry = true;

again:
  auto req = std::make_unique<fu::Request>();
  req->header.restVerb = method;
  req->header.database = _databaseName;
  req->header.parseArangoPath(location.toString());
  for (auto& pair : headerFields) {
    if (boost::iequals(StaticStrings::ContentTypeHeader, pair.first)) {
      if (pair.second == StaticStrings::MimeTypeVPack) {
        req->header.contentType(fu::ContentType::VPack);
      } else if ((pair.second.length() >= StaticStrings::MimeTypeJsonNoEncoding.length()) &&
               (memcmp(pair.second.c_str(),
                       StaticStrings::MimeTypeJsonNoEncoding.c_str(),
                       StaticStrings::MimeTypeJsonNoEncoding.length()) == 0)) {
        // ignore encoding etc.
        req->header.contentType(fu::ContentType::Json);
      } else {
        req->header.contentType(fu::ContentType::Custom);
      }

    } else if (boost::iequals(StaticStrings::Accept, pair.first)) {
      if (pair.second == StaticStrings::MimeTypeVPack) {
        req->header.acceptType(fu::ContentType::VPack);
      } else if ((pair.second.length() >= StaticStrings::MimeTypeJsonNoEncoding.length()) &&
               (memcmp(pair.second.c_str(),
                       StaticStrings::MimeTypeJsonNoEncoding.c_str(),
                       StaticStrings::MimeTypeJsonNoEncoding.length()) == 0)) {
        // ignore encoding etc.
        req->header.acceptType(fu::ContentType::Json);
      } else {
        req->header.acceptType(fu::ContentType::Custom);
      }

      req->header.acceptType(fu::ContentType::Custom);
    }
    req->header.addMeta(pair.first, pair.second);
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
    req->header.contentType(fu::ContentType::Custom);
    req->addBinary(reinterpret_cast<uint8_t const*>(contents.data()), contents.length());
  } else if (body->IsString() || body->IsStringObject()) {  // assume JSON
    TRI_Utf8ValueNFC bodyString(isolate, body);
    req->addBinary(reinterpret_cast<uint8_t const*>(*bodyString), bodyString.length());
    if (req->header.contentType() == fu::ContentType::Unset) {
      req->header.contentType(fu::ContentType::Json);
    }
  } else if (body->IsObject() && V8Buffer::hasInstance(isolate, body)) {
    // supplied body is a Buffer object
    char const* data = V8Buffer::data(isolate, body.As<v8::Object>());
    size_t size = V8Buffer::length(isolate, body.As<v8::Object>());

    if (data == nullptr) {
      TRI_V8_SET_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "invalid <body> buffer value");
      return v8::Undefined(isolate);
    }
    req->addBinary(reinterpret_cast<uint8_t const*>(data), size);
  } else if (!body->IsNullOrUndefined()) {
    VPackBuffer<uint8_t> buffer;
    VPackBuilder builder(buffer, &_vpackOptions);
    TRI_V8ToVPack(isolate, builder, body, false);
    if (_forceJson) {
      auto resultJson = builder.slice().toJson();
      char const* resStr = resultJson.c_str();
      req->addBinary(reinterpret_cast<uint8_t const*>(resStr), resultJson.length());
      req->header.contentType(fu::ContentType::Json);
    } else {
      req->addVPack(std::move(buffer));
      req->header.contentType(fu::ContentType::VPack);
    }
  } else {
    // body is null or undefined
    if (req->header.contentType() == fu::ContentType::Unset) {
      req->header.contentType(fu::ContentType::Json);
    }
  }
  if (req->header.acceptType() == fu::ContentType::Unset) {
    if (_forceJson) {
      req->header.acceptType(fu::ContentType::Json);
    } else {
      req->header.acceptType(fu::ContentType::VPack);
    }
  }
  else if (_forceJson && (req->header.acceptType() == fu::ContentType::VPack)) {
    req->header.acceptType(fu::ContentType::Json);
  }
  req->timeout(
      correctTimeoutToExecutionDeadline(
        std::chrono::duration_cast<std::chrono::milliseconds>(_requestTimeout)));

  std::shared_ptr<fu::Connection> connection = acquireConnection();
  if (!connection || connection->state() == fu::Connection::State::Closed) {
    TRI_V8_SET_EXCEPTION_MESSAGE(TRI_ERROR_SIMPLE_CLIENT_COULD_NOT_CONNECT,
                                 "not connected");
    return v8::Undefined(isolate);
  }

  fu::Error rc = fu::Error::NoError;
  std::unique_ptr<fu::Response> response;
  try {
    response = connection->sendRequest(std::move(req));
  } catch (fu::Error const& ec) {
    rc = ec;
  }

  if (rc == fu::Error::ConnectionClosed && retry) {
    retry = false;
    goto again;
  }

  return handleResult(isolate, std::move(response), rc);
}

v8::Local<v8::Value> V8ClientConnection::requestDataRaw(
    v8::Isolate* isolate, fu::RestVerb method, arangodb::velocypack::StringRef const& location,
    v8::Local<v8::Value> const& body,
    std::unordered_map<std::string, std::string> const& headerFields) {

  bool retry = true;

again:
  auto req = std::make_unique<fu::Request>();
  req->header.restVerb = method;
  req->header.database = _databaseName;
  req->header.parseArangoPath(location.toString());
  for (auto& pair : headerFields) {
    if (boost::iequals(StaticStrings::ContentTypeHeader, pair.first)) {
      req->header.contentType(fu::ContentType::Custom);
    } else if (boost::iequals(StaticStrings::Accept, pair.first)) {
      req->header.acceptType(fu::ContentType::Custom);
    }
    req->header.addMeta(pair.first, pair.second);
  }
  if (body->IsString() || body->IsStringObject()) {  // assume JSON
    TRI_Utf8ValueNFC bodyString(isolate, body);
    req->addBinary(reinterpret_cast<uint8_t const*>(*bodyString), bodyString.length());
    if (req->header.contentType() == fu::ContentType::Unset) {
      req->header.contentType(fu::ContentType::Json);
    }
  } else if (body->IsObject() && V8Buffer::hasInstance(isolate, body)) {
      // supplied body is a Buffer object
      char const* data = V8Buffer::data(isolate, body.As<v8::Object>());
      size_t size = V8Buffer::length(isolate, body.As<v8::Object>());

      if (data == nullptr) {
        TRI_V8_SET_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                     "invalid <body> buffer value");
        return v8::Undefined(isolate);
      }
      req->addBinary(reinterpret_cast<uint8_t const*>(data), size);
  } else if (!body->IsNullOrUndefined()) {
    VPackBuffer<uint8_t> buffer;
    VPackBuilder builder(buffer);
    TRI_V8ToVPack(isolate, builder, body, false);
    req->addVPack(std::move(buffer));
    req->header.contentType(fu::ContentType::VPack);
  } else {
    // body is null or undefined
    if (req->header.contentType() == fu::ContentType::Unset) {
      req->header.contentType(fu::ContentType::Json);
    }
  }
  if (req->header.acceptType() == fu::ContentType::Unset) {
    req->header.acceptType(fu::ContentType::VPack);
  }
  if (_forceJson && (req->header.acceptType() == fu::ContentType::VPack)) {
    req->header.acceptType(fu::ContentType::Json);
  }

  req->timeout(
      correctTimeoutToExecutionDeadline(
        std::chrono::duration_cast<std::chrono::milliseconds>(_requestTimeout)));

  std::shared_ptr<fu::Connection> connection = acquireConnection();
  if (!connection || connection->state() == fu::Connection::State::Closed) {
    TRI_V8_SET_EXCEPTION_MESSAGE(TRI_ERROR_SIMPLE_CLIENT_COULD_NOT_CONNECT,
                                 "not connected");
    return v8::Undefined(isolate);
  }
  fu::Error rc = fu::Error::NoError;
  std::unique_ptr<fu::Response> response;
  try {
    response = connection->sendRequest(std::move(req));
  } catch (fu::Error const& e) {
    rc = e;
    _lastErrorMessage.assign(fu::to_string(e));
    _lastHttpReturnCode = 503;
  }

  if (rc == fu::Error::ConnectionClosed && retry) {
    retry = false;
    goto again;
  }

  auto context = TRI_IGETC;
  v8::Local<v8::Object> result = v8::Object::New(isolate);
  if (!response) {
    result->Set(context,
                TRI_V8_STD_STRING(isolate, StaticStrings::Error),
                v8::Boolean::New(isolate, true)).FromMaybe(false);
    result->Set(context,
                TRI_V8_STD_STRING(isolate, StaticStrings::ErrorNum),
                v8::Integer::New(isolate, _lastHttpReturnCode)).FromMaybe(false);
    result->Set(context,
                TRI_V8_STD_STRING(isolate, StaticStrings::ErrorMessage),
                TRI_V8_STD_STRING(isolate, _lastErrorMessage)).FromMaybe(false);

    return result;
  }

  // complete

  _lastHttpReturnCode = response->statusCode();

  // create raw response
  result->Set(context,
              TRI_V8_ASCII_STRING(isolate, "code"),
              v8::Integer::New(isolate, _lastHttpReturnCode)).FromMaybe(false);

  if (_lastHttpReturnCode >= 400) {
    std::string msg(GeneralResponse::responseString(
        static_cast<ResponseCode>(_lastHttpReturnCode)));

    result->Set(context,
                TRI_V8_STD_STRING(isolate, StaticStrings::Error),
                v8::Boolean::New(isolate, true)).FromMaybe(false);
    result->Set(context,
                TRI_V8_STD_STRING(isolate, StaticStrings::ErrorNum),
                v8::Integer::New(isolate, _lastHttpReturnCode)).FromMaybe(false);
    result->Set(context,
                TRI_V8_STD_STRING(isolate, StaticStrings::ErrorMessage),
                TRI_V8_STD_STRING(isolate, msg)).FromMaybe(false);
  } else {
    result->Set(context,
                TRI_V8_STD_STRING(isolate, StaticStrings::Error),
                v8::Boolean::New(isolate, false)).FromMaybe(false);
  }

  v8::Local<v8::Object> headers = v8::Object::New(isolate);
  auto responseBody = response->payload();
  if (response->contentType() != fuerte::ContentType::Custom) {
    if ((responseBody.size() > 0) && response->isContentTypeVPack()) {
      std::vector<VPackSlice> const& slices = response->slices();
      if (!slices.empty()) {
        result->Set(context,
                    TRI_V8_ASCII_STRING(isolate, "parsedBody"),
                    TRI_VPackToV8(isolate, slices[0])).FromMaybe(false);
      }
    }
    if (responseBody.size() > 0) {
      const char* bodyStr = reinterpret_cast<const char*>(responseBody.data());
      v8::Local<v8::String> b = TRI_V8_PAIR_STRING(isolate, bodyStr, responseBody.size());
      result->Set(context,
                  TRI_V8_ASCII_STRING(isolate, "body"), b).FromMaybe(false);
    }

    auto contentType = TRI_V8_STD_STRING(isolate, fu::to_string(response->contentType()));
    headers->Set(context,
                 TRI_V8_STD_STRING(isolate, StaticStrings::ContentTypeHeader), contentType).FromMaybe(false);
  } else {
    V8Buffer* buffer = V8Buffer::New
      (isolate,
       static_cast<char const*>(responseBody.data()),
       responseBody.size());
    auto bufObj = v8::Local<v8::Object>::New(isolate, buffer->_handle);
    result->Set(context,
                TRI_V8_ASCII_STRING(isolate, "body"),
                bufObj).FromMaybe(false);
  }

  for (auto const& it : response->header.meta()) {
    headers->Set(context,
                 TRI_V8_STD_STRING(isolate, it.first),
                 TRI_V8_STD_STRING(isolate, it.second)).FromMaybe(false);
  }

  if ((_builder.protocolType() == fu::ProtocolType::Vst) &&
      (method != fu::RestVerb::Head)) {
    // VST only adds a content-length header in case of head, since else its
    // part of the protocol.
    headers->Set(context,
                 TRI_V8_STD_STRING(isolate, StaticStrings::ContentLength),
                 TRI_V8_STD_STRING(isolate, std::to_string(responseBody.size()))).FromMaybe(false);

  }

  result->Set(context,
              TRI_V8_ASCII_STRING(isolate, "headers"), headers).FromMaybe(false);

  // and returns
  return result;
}

v8::Local<v8::Value> V8ClientConnection::handleResult(v8::Isolate* isolate,
                                                      std::unique_ptr<fu::Response> res,
                                                      fu::Error ec) {
  auto context = TRI_IGETC;
  // not complete
  if (!res) {
    _lastErrorMessage = fu::to_string(ec);
    _lastHttpReturnCode = static_cast<int>(rest::ResponseCode::SERVER_ERROR);

    v8::Local<v8::Object> result = v8::Object::New(isolate);
    result->Set(context,
                TRI_V8_STD_STRING(isolate, StaticStrings::Error),
                v8::Boolean::New(isolate, true)).FromMaybe(false);
    result->Set(context,
                TRI_V8_ASCII_STRING(isolate, "code"),
                v8::Integer::New(isolate, static_cast<int>(rest::ResponseCode::SERVER_ERROR))).FromMaybe(false);

    int errorNumber = 0;
    switch (ec) {
      case fu::Error::CouldNotConnect:
      case fu::Error::ConnectionClosed:
        errorNumber = TRI_ERROR_SIMPLE_CLIENT_COULD_NOT_CONNECT;
        break;

      case fu::Error::ReadError:
        errorNumber = TRI_ERROR_SIMPLE_CLIENT_COULD_NOT_READ;
        break;

      case fu::Error::WriteError:
        errorNumber = TRI_ERROR_SIMPLE_CLIENT_COULD_NOT_WRITE;
        break;

      default:
        errorNumber = TRI_ERROR_SIMPLE_CLIENT_UNKNOWN_ERROR;
        break;
    }

    result->Set(context,
                TRI_V8_STD_STRING(isolate, StaticStrings::ErrorNum),
                v8::Integer::New(isolate, errorNumber)).FromMaybe(false);
    result->Set(context,
                TRI_V8_STD_STRING(isolate, StaticStrings::ErrorMessage),
                TRI_V8_STD_STRING(isolate, _lastErrorMessage)).FromMaybe(false);

    return result;
  }

  TRI_ASSERT(res != nullptr);

  // complete
  _lastHttpReturnCode = res->statusCode();

  // got a body
  auto sb = res->payload();
  if (sb.size() > 0) {
    if (res->isContentTypeVPack()) {
      std::vector<VPackSlice> const& slices = res->slices();
      if (!slices.empty()) {
        return TRI_VPackToV8(isolate, slices[0]);
      }  // no body
    }

    char const* str = reinterpret_cast<char const*>(sb.data());

    if (res->isContentTypeJSON()) {
      v8::Local<v8::Value> ret;
      auto builder = std::make_shared<VPackBuilder>();
      VPackParser parser(builder);
      try {
        parser.parse(str, sb.size());
        ret = TRI_VPackToV8(isolate, builder->slice(), parser.options, nullptr);
      } catch (std::exception const& ex) {
        std::string err("Error parsing the server JSON reply: ");
        err += ex.what();
        TRI_CreateErrorObject(isolate, TRI_ERROR_HTTP_CORRUPTED_JSON, err, true);
      }
      return ret;
    }
    if (res->isContentTypeHtml() || res->isContentTypeText()) {
      // return body as string
      return TRI_V8_PAIR_STRING(isolate, str, sb.size());
    }

    V8Buffer* buffer;
    buffer = V8Buffer::New(isolate, reinterpret_cast<char const*>(sb.data()), sb.size());
    auto bufObj = v8::Local<v8::Object>::New(isolate, buffer->_handle);
    return bufObj;
  }

  // no body

  v8::Local<v8::Object> result = v8::Object::New(isolate);

  result->Set(context,
              TRI_V8_ASCII_STRING(isolate, "code"),
              v8::Integer::New(isolate, _lastHttpReturnCode)).FromMaybe(false);

  if (_lastHttpReturnCode >= 400) {
    std::string msg(GeneralResponse::responseString(
        static_cast<ResponseCode>(_lastHttpReturnCode)));

    result->Set(context,
                TRI_V8_STD_STRING(isolate, StaticStrings::Error),
                v8::Boolean::New(isolate, true)).FromMaybe(false);
    result->Set(context,
                TRI_V8_STD_STRING(isolate, StaticStrings::ErrorNum),
                v8::Integer::New(isolate, _lastHttpReturnCode)).FromMaybe(false);
    result->Set(context,
                TRI_V8_STD_STRING(isolate, StaticStrings::ErrorMessage),
                TRI_V8_STD_STRING(isolate, msg)).FromMaybe(false);
  } else {
    result->Set(context,
                TRI_V8_STD_STRING(isolate, StaticStrings::Error),
                v8::Boolean::New(isolate, false)).FromMaybe(false);
  }

  return result;
}

void V8ClientConnection::initServer(v8::Isolate* isolate, v8::Local<v8::Context> context) {
  v8::Local<v8::Value> v8client = v8::External::New(isolate, &_client);

  v8::Local<v8::FunctionTemplate> connection_templ = v8::FunctionTemplate::New(isolate);

  connection_templ->SetClassName(
      TRI_V8_ASCII_STRING(isolate, "ArangoConnection"));

  v8::Local<v8::ObjectTemplate> connection_proto = connection_templ->PrototypeTemplate();

  connection_proto->Set(isolate, "DELETE",
                        v8::FunctionTemplate::New(isolate, ClientConnection_httpDelete));

  connection_proto->Set(isolate, "DELETE_RAW",
                        v8::FunctionTemplate::New(isolate, ClientConnection_httpDeleteRaw));

  connection_proto->Set(isolate, "GET",
                        v8::FunctionTemplate::New(isolate, ClientConnection_httpGet));

  connection_proto->Set(isolate, "GET_RAW",
                        v8::FunctionTemplate::New(isolate, ClientConnection_httpGetRaw));

  connection_proto->Set(isolate, "HEAD",
                        v8::FunctionTemplate::New(isolate, ClientConnection_httpHead));

  connection_proto->Set(isolate, "HEAD_RAW",
                        v8::FunctionTemplate::New(isolate, ClientConnection_httpHeadRaw));

  connection_proto->Set(isolate, "OPTIONS",
                        v8::FunctionTemplate::New(isolate, ClientConnection_httpOptions));

  connection_proto->Set(isolate, "OPTIONS_RAW",
                        v8::FunctionTemplate::New(isolate, ClientConnection_httpOptionsRaw));

  connection_proto->Set(isolate, "PATCH",
                        v8::FunctionTemplate::New(isolate, ClientConnection_httpPatch));

  connection_proto->Set(isolate, "PATCH_RAW",
                        v8::FunctionTemplate::New(isolate, ClientConnection_httpPatchRaw));

  connection_proto->Set(isolate, "POST",
                        v8::FunctionTemplate::New(isolate, ClientConnection_httpPost));

  connection_proto->Set(isolate, "POST_RAW",
                        v8::FunctionTemplate::New(isolate, ClientConnection_httpPostRaw));

  connection_proto->Set(isolate, "PUT",
                        v8::FunctionTemplate::New(isolate, ClientConnection_httpPut));
  connection_proto->Set(isolate, "PUT_RAW",
                        v8::FunctionTemplate::New(isolate, ClientConnection_httpPutRaw));

  connection_proto->Set(isolate, "SEND_FILE",
                        v8::FunctionTemplate::New(isolate, ClientConnection_httpSendFile));

  connection_proto->Set(isolate, "getEndpoint",
                        v8::FunctionTemplate::New(isolate, ClientConnection_getEndpoint, v8client));

  connection_proto->Set(isolate, "lastHttpReturnCode",
                        v8::FunctionTemplate::New(isolate, ClientConnection_lastHttpReturnCode));

  connection_proto->Set(isolate, "lastErrorMessage",
                        v8::FunctionTemplate::New(isolate, ClientConnection_lastErrorMessage));

  connection_proto->Set(isolate, "isConnected",
                        v8::FunctionTemplate::New(isolate, ClientConnection_isConnected));

  connection_proto->Set(isolate, "forceJson",
                        v8::FunctionTemplate::New(isolate, ClientConnection_forceJson, v8client));

  connection_proto->Set(isolate, "reconnect",
                        v8::FunctionTemplate::New(isolate, ClientConnection_reconnect, v8client));

  connection_proto->Set(isolate, "connectedUser",
                        v8::FunctionTemplate::New(isolate, ClientConnection_connectedUser,
                                                  v8client));

  connection_proto->Set(isolate, "timeout",
                        v8::FunctionTemplate::New(isolate, ClientConnection_timeout, v8client));

  connection_proto->Set(isolate, "toString",
                        v8::FunctionTemplate::New(isolate, ClientConnection_toString, v8client));

  connection_proto->Set(isolate, "getVersion",
                        v8::FunctionTemplate::New(isolate, ClientConnection_getVersion));

  connection_proto->Set(isolate, "getMode",
                        v8::FunctionTemplate::New(isolate, ClientConnection_getMode));

  connection_proto->Set(isolate, "getRole",
                        v8::FunctionTemplate::New(isolate, ClientConnection_getRole));

  connection_proto->Set(isolate, "getDatabaseName",
                        v8::FunctionTemplate::New(isolate, ClientConnection_getDatabaseName));

  connection_proto->Set(isolate, "setDatabaseName",
                        v8::FunctionTemplate::New(isolate, ClientConnection_setDatabaseName,
                                                  v8client));

  connection_proto->Set(isolate, "importCsv",
                        v8::FunctionTemplate::New(isolate, ClientConnection_importCsv, v8client));

  connection_proto->Set(isolate, "importJson",
                        v8::FunctionTemplate::New(isolate, ClientConnection_importJson, v8client));

  connection_proto->SetCallAsFunctionHandler(ClientConnection_ConstructorCallback, v8client);

  v8::Local<v8::ObjectTemplate> connection_inst = connection_templ->InstanceTemplate();

  connection_inst->SetInternalFieldCount(2);

  TRI_AddGlobalVariableVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate, "ArangoConnection"),
                               connection_proto->NewInstance(TRI_IGETC).FromMaybe(v8::Local<v8::Object>()));

  ConnectionTempl.Reset(isolate, connection_inst);

  // add the client connection to the context:
  TRI_AddGlobalVariableVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate, "SYS_ARANGO"),
                               WrapV8ClientConnection(isolate, this));
}

void V8ClientConnection::shutdownConnection() {
  std::lock_guard<std::recursive_mutex> guard(_lock);
  if (_connection) {
    _connection->cancel();
  }
}
