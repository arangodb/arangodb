///////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
////////////////////////////////////////////////////////////////////////////////

#include "V8ClientConnectionBindings.h"

#include "ClientConnection.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/FileUtils.h"
#include "Basics/EncodingUtils.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/system-functions.h"
#include "Import/ImportHelper.h"
#include "Logger/LogMacros.h"
#include "Rest/GeneralResponse.h"
#include "Rest/Version.h"
#include "Shell/ClientFeature.h"
#include "fuerte/types.h"
#ifdef ARANGODB_ENABLE_FAILURE_TESTS
#include "Shell/RequestFuzzer.h"
#endif
#include "Shell/ShellConsoleFeature.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "Ssl/SslInterface.h"
#include "Ssl/ssl-helper.h"
#include "Utilities/NameValidator.h"
#include "V8/V8SecurityFeature.h"
#include "V8/v8-buffer.h"
#include "V8/v8-conv.h"
#include "V8/v8-deadline.h"
#include "V8/v8-utils.h"
#include "V8/v8-vpack.h"
#ifdef USE_ENTERPRISE
#include "Enterprise/Encryption/EncryptionFeature.h"
#endif

#include <absl/strings/str_cat.h>
#include <fuerte/connection.h>
#include <fuerte/requests.h>
#include <fuerte/helper.h>
#include "Ssl/jwt.h"
#include <v8.h>
#include <velocypack/Builder.h>
#include <velocypack/Parser.h>
#include <velocypack/Slice.h>
#include <stdexcept>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/hash2/md5.hpp>

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::basics;
using namespace arangodb::httpclient;
using namespace arangodb::import;

namespace fu = arangodb::fuerte;

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
    v8::Local<v8::Array> const props =
        v8Headers->GetPropertyNames(TRI_IGETC).FromMaybe(
            v8::Local<v8::Array>());
    auto context = TRI_IGETC;
    for (uint32_t i = 0; i < props->Length(); i++) {
      v8::Local<v8::Value> key =
          props->Get(context, i).FromMaybe(v8::Local<v8::Value>());
      myMap.emplace(
          TRI_ObjectToString(isolate, key),
          TRI_ObjectToString(
              isolate,
              v8Headers->Get(context, key).FromMaybe(v8::Local<v8::Value>())));
    }
  }
}

namespace {
bool canParseResponse(fu::Response const& response) {
  return (response.isContentTypeVPack() || response.isContentTypeJSON()) &&
         (response.contentEncoding() == fuerte::ContentEncoding::Identity ||
          response.contentEncoding() == fuerte::ContentEncoding::Gzip ||
          response.contentEncoding() == fuerte::ContentEncoding::Deflate) &&
         response.payloadSize() > 0;
}

v8::Local<v8::Value> parseReplyBodyToV8(fu::Response& response,
                                        v8::Isolate* isolate) {
  if (response.contentType() != fu::ContentType::VPack &&
      response.contentType() != fu::ContentType::Json) {
    return v8::Undefined(isolate);
  }

  if (response.contentType() == fu::ContentType::VPack) {
    auto const& slices = response.slices();
    return TRI_VPackToV8(isolate, slices[0]);
  }
  TRI_ASSERT(response.contentType() == fu::ContentType::Json);
  auto responseBody = response.payload();
  try {
    auto parsedBody = VPackParser::fromJson(
        reinterpret_cast<char const*>(responseBody.data()),
        responseBody.size());
    return TRI_VPackToV8(isolate, parsedBody->slice());
  } catch (std::exception const& ex) {
    std::string err =
        absl::StrCat("Error parsing the server JSON reply: ", ex.what());
    TRI_CreateErrorObject(isolate, TRI_ERROR_HTTP_CORRUPTED_JSON, err, true);
  }
  return v8::Undefined(isolate);
}

v8::Local<v8::Value> translateResultBodyToV8(fu::Response& response,
                                             v8::Isolate* isolate) {
  auto responseBody = response.payload();
  if (response.contentEncoding() == fuerte::ContentEncoding::Identity &&
      (response.isContentTypeJSON() || response.isContentTypeText() ||
       response.isContentTypeHtml())) {
    char const* bodyStr = reinterpret_cast<char const*>(responseBody.data());
    return TRI_V8_PAIR_STRING(isolate, bodyStr, responseBody.size());
  }
  V8Buffer* buffer =
      V8Buffer::New(isolate, static_cast<char const*>(responseBody.data()),
                    responseBody.size());
  return v8::Local<v8::Object>::New(isolate, buffer->_handle);
}

void setResultMessage(v8::Isolate* isolate, v8::Local<v8::Context> context,
                      bool isError, unsigned lastHttpReturnCode,
                      std::string const& message,
                      v8::Local<v8::Object> result) {
  result
      ->Set(context, TRI_V8_STD_STRING(isolate, StaticStrings::Error),
            v8::Boolean::New(isolate, true))
      .FromMaybe(isError);
  result
      ->Set(context, TRI_V8_STD_STRING(isolate, StaticStrings::ErrorNum),
            v8::Integer::New(isolate, lastHttpReturnCode))
      .FromMaybe(false);
  result
      ->Set(context, TRI_V8_STD_STRING(isolate, StaticStrings::ErrorMessage),
            TRI_V8_STD_STRING(isolate, message))
      .FromMaybe(false);
}

void setResultMessage(v8::Isolate* isolate, v8::Local<v8::Context> context,
                      bool isError, unsigned lastHttpReturnCode,
                      v8::Local<v8::Object> result) {
  // create raw response
  result
      ->Set(context, TRI_V8_ASCII_STD_STRING(isolate, StaticStrings::Code),
            v8::Integer::New(isolate, lastHttpReturnCode))
      .FromMaybe(false);

  if (lastHttpReturnCode >= 400) {
    std::string msg(GeneralResponse::responseString(
        static_cast<ResponseCode>(lastHttpReturnCode)));
    setResultMessage(isolate, context, isError, lastHttpReturnCode, msg,
                     result);
  } else {
    result
        ->Set(context, TRI_V8_STD_STRING(isolate, StaticStrings::Error),
              v8::Boolean::New(isolate, false))
        .FromMaybe(false);
  }
}

velocypack::Options const& requestBodyOptions() {
  static velocypack::Options const options = [] {
    velocypack::Options options = velocypack::Options::Defaults;
    options.buildUnindexedObjects = true;
    options.buildUnindexedArrays = true;
    return options;
  }();
  return options;
}

v8::Local<v8::Value> resultToV8(v8::Isolate* isolate,
                                ClientConnection const& connection,
                                ClientConnection::RequestOutcome& outcome) {
  auto context = TRI_IGETC;
  v8::Local<v8::Object> result = v8::Object::New(isolate);
  if (!outcome.response) {
    setResultMessage(isolate, context, true,
                     static_cast<int>(outcome.error.errorNumber()),
                     std::string(outcome.error.errorMessage()), result);
    result
        ->Set(context, TRI_V8_ASCII_STD_STRING(isolate, StaticStrings::Code),
              v8::Integer::New(isolate, connection.lastHttpReturnCode()))
        .FromMaybe(false);
    return result;
  }

  fu::Response& response = *outcome.response;
  if (canParseResponse(response)) {
    return parseReplyBodyToV8(response, isolate);
  }
  if (response.payloadSize() > 0) {
    return translateResultBodyToV8(response, isolate);
  }
  setResultMessage(isolate, context, false, response.statusCode(), result);
  return result;
}

v8::Local<v8::Value> rawResultToV8(v8::Isolate* isolate,
                                   ClientConnection::RequestOutcome& outcome) {
  auto context = TRI_IGETC;
  v8::Local<v8::Object> result = v8::Object::New(isolate);
  if (!outcome.response) {
    setResultMessage(
        isolate, context, true,
        static_cast<unsigned>(rest::ResponseCode::SERVICE_UNAVAILABLE),
        std::string(outcome.error.errorMessage()), result);
    return result;
  }

  fu::Response& response = *outcome.response;
  setResultMessage(isolate, context, false, response.statusCode(), result);

  v8::Local<v8::Object> headers = v8::Object::New(isolate);
  result->Set(context, TRI_V8_ASCII_STRING(isolate, "headers"), headers)
      .FromMaybe(false);

  if (canParseResponse(response)) {
    result
        ->Set(context, TRI_V8_STD_STRING(isolate, StaticStrings::ParsedBody),
              parseReplyBodyToV8(response, isolate))
        .FromMaybe(false);
  }
  if (response.payloadSize() > 0) {
    result
        ->Set(context, TRI_V8_STD_STRING(isolate, StaticStrings::Body),
              translateResultBodyToV8(response, isolate))
        .FromMaybe(false);
  }

  if (response.contentType() != fuerte::ContentType::Custom) {
    auto contentType =
        TRI_V8_STD_STRING(isolate, fu::to_string(response.contentType()));
    headers
        ->Set(context,
              TRI_V8_STD_STRING(isolate, StaticStrings::ContentTypeHeader),
              contentType)
        .FromMaybe(false);
  }
  for (auto const& it : response.header.meta()) {
    headers
        ->Set(context, TRI_V8_STD_STRING(isolate, it.first),
              TRI_V8_STD_STRING(isolate, it.second))
        .FromMaybe(false);
  }

  return result;
}

// isFile: body is the name of the file to send
v8::Local<v8::Value> requestData(
    v8::Isolate* isolate, ClientConnection& connection, fu::RestVerb verb,
    std::string_view location, v8::Local<v8::Value> const& body,
    std::unordered_map<std::string, std::string> const& headerFields, bool raw,
    bool isFile = false) {
  // keep what the request body refers to until the request is sent
  std::optional<TRI_Utf8ValueNFC> text;
  VPackBuilder value(&requestBodyOptions());

  request_coding::RequestBody requestBody = request_coding::NoBody{};
  if (isFile && !raw) {
    requestBody = request_coding::FileBody{TRI_ObjectToString(isolate, body)};
  } else if (body->IsString() || body->IsStringObject()) {
    text.emplace(isolate, body);
    requestBody =
        request_coding::JsonBody{std::string_view(**text, text->length())};
  } else if (body->IsObject() && V8Buffer::hasInstance(isolate, body)) {
    char const* data = V8Buffer::data(isolate, body.As<v8::Object>());
    size_t size = V8Buffer::length(isolate, body.As<v8::Object>());
    if (data == nullptr) {
      TRI_V8_SET_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "invalid <body> buffer value");
      return v8::Undefined(isolate);
    }
    requestBody = request_coding::BinaryBody{std::string_view(data, size)};
  } else if (!body->IsNullOrUndefined()) {
    TRI_V8ToVPack(isolate, value, body, false);
    requestBody = request_coding::ValueBody{value.slice()};
  }

  auto const timeout = correctTimeoutToExecutionDeadline(
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::duration<double>(connection.timeout())));
  auto outcome = connection.sendRequest(verb, location, headerFields,
                                        requestBody, timeout);
  if (outcome.fail()) {
    TRI_CreateErrorObject(isolate, outcome.errorNumber(),
                          outcome.errorMessage(), true);
    return v8::Undefined(isolate);
  }
  return raw ? rawResultToV8(isolate, outcome.get())
             : resultToV8(isolate, connection, outcome.get());
}

v8::Local<v8::Value> handleTableToV8(
    v8::Isolate* isolate,
    std::vector<ClientConnection::HandleInfo> const& handles) {
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Object> table = v8::Object::New(isolate);

  for (auto const& info : handles) {
    v8::Local<v8::Object> entry = v8::Object::New(isolate);
    auto const setString = [&](char const* key, std::string const& value) {
      entry
          ->Set(context, TRI_V8_ASCII_STRING(isolate, key),
                TRI_V8_STD_STRING(isolate, value))
          .FromMaybe(false);
    };
    auto const setBool = [&](char const* key, bool value) {
      entry
          ->Set(context, TRI_V8_ASCII_STRING(isolate, key),
                value ? v8::True(isolate) : v8::False(isolate))
          .FromMaybe(false);
    };

    setBool("active", info.active);
    setBool("connected", info.peer.has_value() && info.peer->connected);
    setString("endpoint", info.peer ? info.peer->endpoint : "N/A");
    setString("localPort", info.peer ? info.peer->localEndpoint : "N/A");
    setString("username", info.username);
    setString("password", info.password);
    setString("jwtToken", info.jwtToken);

    table->Set(context, TRI_V8_STRING(isolate, info.handle), entry)
        .FromMaybe(false);
  }
  return table;
}
}  // namespace

////////////////////////////////////////////////////////////////////////////////
/// @brief weak reference callback for connections (call the destructor here)
////////////////////////////////////////////////////////////////////////////////

static void DestroyV8ClientConnection(ClientConnection* v8connection) {
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
  auto myConnection =
      v8::Local<v8::External>::New(data.GetIsolate(), *persistent);
  auto v8connection = static_cast<ClientConnection*>(myConnection->Value());

  DestroyV8ClientConnection(v8connection);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief wrap ClientConnection in a v8::Object
////////////////////////////////////////////////////////////////////////////////

static v8::Local<v8::Value> WrapV8ClientConnection(
    v8::Isolate* isolate, ClientConnection* v8connection) {
  v8::EscapableHandleScope scope(isolate);
  auto localConnectionTempl =
      v8::Local<v8::ObjectTemplate>::New(isolate, ConnectionTempl);
  v8::Local<v8::Object> result =
      localConnectionTempl->NewInstance(TRI_IGETC).FromMaybe(
          v8::Local<v8::Object>());

  auto myConnection = v8::External::New(isolate, v8connection);
  result->SetInternalField(SLOT_CLASS_TYPE,
                           v8::Integer::New(isolate, WRAP_TYPE_CONNECTION));
  result->SetInternalField(SLOT_CLASS, myConnection);
  Connections[v8connection].Reset(isolate, myConnection);
  Connections[v8connection].SetWeak(&Connections[v8connection],
                                    ClientConnection_DestructorCallback,
                                    v8::WeakCallbackType::kParameter);
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

  TRI_GET_GLOBALS();

  auto v8connection =
      std::make_unique<ClientConnection>(v8g->server(), *client);
  auto res = v8connection->connect();
  if (res.ok() && v8connection->isConnected() &&
      v8connection->lastHttpReturnCode() == (int)rest::ResponseCode::OK) {
    LOG_TOPIC("9c8b4", INFO, arangodb::Logger::FIXME)
        << ClientFeature::buildConnectedMessage(
               v8connection->endpointSpecification(), v8connection->version(),
               v8connection->role(), v8connection->mode(),
               v8connection->databaseName(), v8connection->username());
  } else {
    if (!res.ok()) {
      TRI_V8_THROW_EXCEPTION_MESSAGE(res.errorNumber(), res.errorMessage());
    } else {
      std::string errorMessage = "Could not connect. Error message: " +
                                 v8connection->lastErrorMessage();

      TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_SIMPLE_CLIENT_COULD_NOT_CONNECT,
                                     errorMessage);
    }
  }

  TRI_V8_RETURN(WrapV8ClientConnection(isolate, v8connection.release()));
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "protocol"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_protocol(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  ClientConnection* v8connection = TRI_UnwrapClass<ClientConnection>(
      args.Holder(), WRAP_TYPE_CONNECTION, TRI_IGETC);

  v8::Local<v8::External> wrap = v8::Local<v8::External>::Cast(args.Data());
  ClientFeature* client = static_cast<ClientFeature*>(wrap->Value());

  if (v8connection == nullptr || client == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL(
        "protocol() must be invoked on an arango connection object instance.");
  }

  TRI_V8_RETURN(TRI_V8_STD_STRING(isolate, v8connection->protocol()));

  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "reconnect"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_reconnect(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);
  if (isExecutionDeadlineReached(isolate)) {
    return;
  }

  ClientConnection* v8connection = TRI_UnwrapClass<ClientConnection>(
      args.Holder(), WRAP_TYPE_CONNECTION, TRI_IGETC);

  v8::Local<v8::External> wrap = v8::Local<v8::External>::Cast(args.Data());
  ClientFeature* client = static_cast<ClientFeature*>(wrap->Value());

  if (v8connection == nullptr || client == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL(
        "reconnect() must be invoked on an arango connection object instance.");
  }

  if (args.Length() < 2) {
    // Note that there are two additional parameters, which aren't advertised,
    // namely `warnConnect` and `jwtSecret`.
    TRI_V8_THROW_EXCEPTION_USAGE(
        "reconnect(<endpoint>, <database> [, <username>, <password> ])");
  }

  std::string const endpoint = TRI_ObjectToString(isolate, args[0]);
  std::string databaseName = TRI_ObjectToString(isolate, args[1]);

  if (auto res = DatabaseNameValidator::validateName(true, true, databaseName);
      res.fail()) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  std::string username;

  if (args.Length() < 3) {
    username = client->username();
  } else {
    username = TRI_ObjectToString(isolate, args[2]);
  }

  std::string password;

  if (args.Length() < 4) {
    if (client->jwtSecret().empty()) {
      ShellConsoleFeature& console =
          v8connection->server().getFeature<ShellConsoleFeature>();

      if (console.isEnabled()) {
        password = console.readPassword("Please specify a password: ");
      } else {
        std::cout << "Please specify a password: " << std::flush;
        password = ShellConsoleFeature::readPassword();
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

  V8SecurityFeature& v8security =
      v8connection->server().getFeature<V8SecurityFeature>();
  if (!v8security.isAllowedToConnectToUrl(isolate, endpoint)) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(
        TRI_ERROR_FORBIDDEN,
        absl::StrCat("not allowed to connect to this endpoint: ", endpoint));
  }

  if (args.Length() > 5 && !args[5]->IsUndefined()) {
    // only use JWT from parameters when specified
    client->setJwtSecret(TRI_ObjectToString(isolate, args[5]));
  } else if (args.Length() >= 4) {
    // password specified, but no JWT
    client->setJwtSecret("");
  }

  client->setEndpoint(endpoint);
  client->setDatabaseName(databaseName);
  client->setUsername(username);
  client->setPassword(password);
  client->setWarnConnect(warnConnect);

  try {
    auto res = v8connection->reconnect();
    if (!res.ok()) {
      TRI_V8_THROW_EXCEPTION_MESSAGE(res.errorNumber(), res.errorMessage());
    }
  } catch (std::exception const& ex) {
    TRI_V8_THROW_EXCEPTION_PARAMETER(ex.what());
  } catch (...) {
    std::string errorMessage = absl::StrCat("error in '", endpoint, "'");
    TRI_V8_THROW_EXCEPTION_PARAMETER(errorMessage);
  }

  TRI_ExecuteJavaScriptString(isolate, "require('internal').db._flushCache();",
                              "reload db object", false);

  TRI_V8_RETURN_TRUE();
  TRI_V8_TRY_CATCH_END
}

static void ClientConnection_setJwtSecret(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (isExecutionDeadlineReached(isolate)) {
    return;
  }

  // get the connection
  ClientConnection* v8connection = TRI_UnwrapClass<ClientConnection>(
      args.Holder(), WRAP_TYPE_CONNECTION, TRI_IGETC);

  v8::Local<v8::External> wrap = v8::Local<v8::External>::Cast(args.Data());
  ClientFeature* client = static_cast<ClientFeature*>(wrap->Value());

  if (v8connection == nullptr || client == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL(
        "setJwtSecret() must be invoked on an arango connection object "
        "instance.");
  }

  if (args.Length() != 1 || !args[0]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE("setJwtSecret(<value>)");
  }

  std::string const value = TRI_ObjectToString(isolate, args[0]);
  client->setJwtSecret(value);

  TRI_V8_RETURN_TRUE();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "getHandle"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_getHandle(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  ClientConnection* v8connection = TRI_UnwrapClass<ClientConnection>(
      args.Holder(), WRAP_TYPE_CONNECTION, TRI_IGETC);

  if (v8connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL(
        "getHandle() must be invoked on an arango connection object "
        "instance.");
  }

  TRI_V8_RETURN_STD_STRING(v8connection->getHandle());
  TRI_V8_TRY_CATCH_END
}

static void ClientConnection_getHandleTable(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  ClientConnection* v8connection = TRI_UnwrapClass<ClientConnection>(
      args.Holder(), WRAP_TYPE_CONNECTION, TRI_IGETC);

  if (v8connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL(
        "getHandleTable() must be invoked on an arango connection object "
        "instance.");
  }

  TRI_V8_RETURN(
      handleTableToV8(isolate, v8connection->connectionHandleTable()));
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "connectHandle"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_connectHandle(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  ClientConnection* v8connection = TRI_UnwrapClass<ClientConnection>(
      args.Holder(), WRAP_TYPE_CONNECTION, TRI_IGETC);

  if (v8connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL(
        "connectHandle() must be invoked on an arango connection object "
        "instance.");
  }
  // check params
  if (args.Length() != 1 || !args[0]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE("connectHandle(<handleString>)");
  }

  auto handle = TRI_ObjectToString(isolate, args[0]);
  switch (v8connection->connectHandle(handle)) {
    case ClientConnection::HandleSwitch::kUnknownHandle:
      TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_SIMPLE_CLIENT_COULD_NOT_CONNECT,
                                     "Handle not found in the connection list");
    case ClientConnection::HandleSwitch::kSwitched:
      // the db object caches what it read via the previous handle, which may
      // have been another database, server or user. it is not refilled here,
      // as switching handles must not send requests
      TRI_ExecuteJavaScriptString(isolate,
                                  "require('internal').db._forgetCache();",
                                  "forget db object cache", false);
      break;
    case ClientConnection::HandleSwitch::kAlreadyActive:
      break;
  }
  TRI_V8_RETURN_TRUE();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "disconnectHandle"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_disconnectHandle(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  ClientConnection* v8connection = TRI_UnwrapClass<ClientConnection>(
      args.Holder(), WRAP_TYPE_CONNECTION, TRI_IGETC);

  if (v8connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL(
        "disconnectHandle() must be invoked on an arango connection object "
        "instance.");
  }
  // check params
  if (args.Length() != 1 || !args[0]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE("disconnectHandle(<handleString>)");
  }

  auto handle = TRI_ObjectToString(isolate, args[0]);
  if (v8connection->disconnectHandle(handle)) {
    TRI_V8_RETURN_TRUE();
  }
  TRI_V8_RETURN_FALSE();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "flushConnectionCache"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_flushConnectionCache(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  ClientConnection* v8connection = TRI_UnwrapClass<ClientConnection>(
      args.Holder(), WRAP_TYPE_CONNECTION, TRI_IGETC);

  if (v8connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL(
        "flushConnectionCache() must be invoked on an arango connection object "
        "instance.");
  }
  // check params
  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("flushConnectionCache()");
  }

  v8connection->flushConnectionCache();
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

  ClientConnection* v8connection = TRI_UnwrapClass<ClientConnection>(
      args.Holder(), WRAP_TYPE_CONNECTION, TRI_IGETC);

  if (v8connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL(
        "connectedUser() must be invoked on an arango connection object "
        "instance.");
  }

  // the user of the active connection, which connectHandle() may have
  // switched to
  TRI_V8_RETURN(TRI_V8_STD_STRING(isolate, v8connection->username()));
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "GET" helper
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpGetAny(
    v8::FunctionCallbackInfo<v8::Value> const& args, bool raw) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (isExecutionDeadlineReached(isolate)) {
    return;
  }
  // get the connection
  ClientConnection* v8connection = TRI_UnwrapClass<ClientConnection>(
      args.Holder(), WRAP_TYPE_CONNECTION, TRI_IGETC);

  if (v8connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL(
        "get() must be invoked on an arango connection object instance.");
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

  TRI_V8_RETURN(requestData(isolate, *v8connection, fu::RestVerb::Get,
                            std::string_view(*url, url.length()),
                            v8::Undefined(isolate), headerFields, raw));
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
  if (isExecutionDeadlineReached(isolate)) {
    return;
  }

  // get the connection
  ClientConnection* v8connection = TRI_UnwrapClass<ClientConnection>(
      args.Holder(), WRAP_TYPE_CONNECTION, TRI_IGETC);

  if (v8connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL(
        "head() must be invoked on an arango connection object instance.");
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

  TRI_V8_RETURN(requestData(isolate, *v8connection, fu::RestVerb::Head,
                            std::string_view(*url, url.length()),
                            v8::Undefined(isolate), headerFields, raw));
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
  if (isExecutionDeadlineReached(isolate)) {
    return;
  }

  // get the connection
  ClientConnection* v8connection = TRI_UnwrapClass<ClientConnection>(
      args.Holder(), WRAP_TYPE_CONNECTION, TRI_IGETC);

  if (v8connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL(
        "delete() must be invoked on an arango connection object instance.");
  }

  // check params
  if (args.Length() < 1 || args.Length() > 3 || !args[0]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE("delete(<url>[, <body>[, <headers>]])");
  }

  TRI_Utf8ValueNFC url(isolate, args[0]);

  std::unordered_map<std::string, std::string> headerFields;
  if (args.Length() == 1) {  // no body provided
    TRI_V8_RETURN(requestData(isolate, *v8connection, fu::RestVerb::Delete,
                              std::string_view(*url, url.length()),
                              v8::Undefined(isolate), headerFields, raw));
  }

  // check header fields
  if (args.Length() > 2) {
    ObjectToMap(isolate, headerFields, args[2]);
  }

  TRI_V8_RETURN(requestData(isolate, *v8connection, fu::RestVerb::Delete,
                            std::string_view(*url, url.length()), args[1],
                            headerFields, raw));
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
  if (isExecutionDeadlineReached(isolate)) {
    return;
  }

  // get the connection
  ClientConnection* v8connection = TRI_UnwrapClass<ClientConnection>(
      args.Holder(), WRAP_TYPE_CONNECTION, TRI_IGETC);

  if (v8connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL(
        "options() must be invoked on an arango connection object instance.");
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

  TRI_V8_RETURN(requestData(isolate, *v8connection, fu::RestVerb::Options,
                            std::string_view(*url, url.length()), args[1],
                            headerFields, raw));
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
  if (isExecutionDeadlineReached(isolate)) {
    return;
  }

  // get the connection
  ClientConnection* v8connection = TRI_UnwrapClass<ClientConnection>(
      args.Holder(), WRAP_TYPE_CONNECTION, TRI_IGETC);

  if (v8connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL(
        "post() must be invoked on an arango connection object instance.");
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

  TRI_V8_RETURN(requestData(isolate, *v8connection, fu::RestVerb::Post,
                            std::string_view(*url, url.length()), args[1],
                            headerFields, raw));
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

#ifdef ARANGODB_ENABLE_FAILURE_TESTS
////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "fuzzRequests"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpFuzzRequests(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);
  if (isExecutionDeadlineReached(isolate)) {
    return;
  }

  auto context = TRI_IGETC;
  // get the connection
  ClientConnection* v8connection = TRI_UnwrapClass<ClientConnection>(
      args.Holder(), WRAP_TYPE_CONNECTION, context);

  if (v8connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL(
        "fuzzRequests() must be invoked on an arango connection object "
        "instance.");
  }

  if (args.Length() < 4 || args.Length() > 5) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "fuzzRequests(<numRequests>, <numIterations>, <wordListForKeys>, "
        "<wordListForRoute> [, <seed>])");
  }

  // arg0 = number of requests, arg1 = number of iterations, arg2 = seed for
  // rand
  uint64_t numReqs = TRI_ObjectToUInt64(isolate, args[0], true);
  uint64_t numIts = TRI_ObjectToUInt64(isolate, args[1], true);

  if (numIts > 256) {
    TRI_V8_THROW_EXCEPTION_USAGE("<numIterations> is expected to be <= 256");
  }

  std::vector<std::string> wordListForKeys;

  v8::Handle<v8::Value> a = args[2];
  if (a->IsArray()) {
    v8::Handle<v8::Array> arr = v8::Handle<v8::Array>::Cast(a);

    uint32_t const n = arr->Length();

    wordListForKeys.reserve(n - 1);
    for (uint32_t i = 0; i < n; ++i) {
      TRI_Utf8ValueNFC keyStr(
          isolate, arr->Get(context, i).FromMaybe(v8::Handle<v8::Value>()));

      if (*keyStr == nullptr) {
        wordListForKeys.push_back("");
      } else {
        wordListForKeys.push_back(*keyStr);
      }
    }
  } else {
    TRI_V8_THROW_TYPE_ERROR("<wordListForKeys> must be an array of strings");
  }

  std::vector<std::string> wordListForRoute;

  a = args[3];
  if (a->IsArray()) {
    v8::Handle<v8::Array> arr = v8::Handle<v8::Array>::Cast(a);

    uint32_t const n = arr->Length();
    wordListForRoute.reserve(n - 1);
    for (uint32_t i = 0; i < n; ++i) {
      TRI_Utf8ValueNFC routeStr(
          isolate, arr->Get(context, i).FromMaybe(v8::Handle<v8::Value>()));

      if (*routeStr == nullptr) {
        wordListForRoute.push_back("");
      } else {
        wordListForRoute.push_back(*routeStr);
      }
    }
  } else {
    TRI_V8_THROW_TYPE_ERROR("<wordListForRoute> must be an array of strings");
  }

  std::optional<uint32_t> seed;
  if (args.Length() > 4) {
    if (!args[4]->IsUint32()) {
      TRI_V8_THROW_EXCEPTION_USAGE("<seed> must be an unsigned int.");
    }
    seed = static_cast<uint32_t>(TRI_ObjectToUInt64(isolate, args[4], false));
  }

  fuzzer::RequestFuzzer fuzzer(static_cast<uint32_t>(numIts), wordListForKeys,
                               wordListForRoute, seed);
  if (!seed.has_value()) {
    // log the random seed value for later reproducibility.
    // log level must be warning here because log levels < WARN are suppressed
    // during testing.
    LOG_TOPIC("39e50", WARN, arangodb::Logger::HTTPCLIENT)
        << "fuzzer producing " << numReqs << " requests(s) with " << numIts
        << " iteration(s) each, using seed " << fuzzer.getSeed()
        << " from: " << v8connection->getLocalEndpoint();
  }
  std::unordered_map<uint32_t, uint32_t> fuzzReturnCodesCount;

  // by creating a new connection here we make sure that we always use a new
  // connection when starting the fuzzing. that way the fuzzing results for the
  // same input seed value should be fully deterministic.
  v8connection->forceNewConnection();

  for (uint64_t i = 0; i < numReqs; ++i) {
    uint32_t returnCode = v8connection->sendFuzzRequest(fuzzer);
    if (returnCode == ClientConnection::kFuzzNotConnected) {
      TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_SIMPLE_CLIENT_COULD_NOT_CONNECT,
                                     "connection lost during fuzzing tests");
    }
    fuzzReturnCodesCount[returnCode]++;
  }

  VPackBuilder builder;
  builder.openObject();
  builder.add("seed", velocypack::Value(fuzzer.getSeed()));
  builder.add("totalRequests", velocypack::Value(numReqs));

  if (auto it = fuzzReturnCodesCount.find(
          ClientConnection::kFuzzClosedConnectionCode);
      it != fuzzReturnCodesCount.end()) {
    builder.add("connectionClosed", velocypack::Value(it->second));
  }

  if (auto it =
          fuzzReturnCodesCount.find(ClientConnection::kFuzzNoResponseCode);
      it != fuzzReturnCodesCount.end()) {
    builder.add("noResponse", velocypack::Value(it->second));
  }

  if (auto it = fuzzReturnCodesCount.find(ClientConnection::kFuzzNotConnected);
      it != fuzzReturnCodesCount.end()) {
    builder.add("notConnected", velocypack::Value(it->second));
  }

  builder.add(velocypack::Value("returnCodes"));
  builder.openObject();
  for (auto const& [returnCode, count] : fuzzReturnCodesCount) {
    if (returnCode != ClientConnection::kFuzzClosedConnectionCode &&
        returnCode != ClientConnection::kFuzzNoResponseCode &&
        returnCode != ClientConnection::kFuzzNotConnected) {
      builder.add(std::to_string(returnCode), velocypack::Value(count));
    }
  }
  builder.close();
  builder.close();

  TRI_V8_RETURN(TRI_VPackToV8(isolate, builder.slice()));

  TRI_V8_TRY_CATCH_END
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "PUT" helper
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_httpPutAny(
    v8::FunctionCallbackInfo<v8::Value> const& args, bool raw) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);
  if (isExecutionDeadlineReached(isolate)) {
    return;
  }

  // get the connection
  ClientConnection* v8connection = TRI_UnwrapClass<ClientConnection>(
      args.Holder(), WRAP_TYPE_CONNECTION, TRI_IGETC);

  if (v8connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL(
        "put() must be invoked on an arango connection object instance.");
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

  TRI_V8_RETURN(requestData(isolate, *v8connection, fu::RestVerb::Put,
                            std::string_view(*url, url.length()), args[1],
                            headerFields, raw));
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

  if (isExecutionDeadlineReached(isolate)) {
    return;
  }
  // get the connection
  ClientConnection* v8connection = TRI_UnwrapClass<ClientConnection>(
      args.Holder(), WRAP_TYPE_CONNECTION, TRI_IGETC);

  if (v8connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL(
        "patch() must be invoked on an arango connection object instance.");
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

  TRI_V8_RETURN(requestData(isolate, *v8connection, fu::RestVerb::Patch,
                            std::string_view(*url, url.length()), args[1],
                            headerFields, raw));
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

  if (isExecutionDeadlineReached(isolate)) {
    return;
  }

  // get the connection
  ClientConnection* v8connection = TRI_UnwrapClass<ClientConnection>(
      args.Holder(), WRAP_TYPE_CONNECTION, TRI_IGETC);

  if (v8connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL(
        "sendFile() must be invoked on an arango connection object instance.");
  }

  // check params
  if (args.Length() < 2 || args.Length() > 3 || !args[0]->IsString() ||
      args[1]->IsUndefined()) {
    TRI_V8_THROW_EXCEPTION_USAGE("sendFile(<url>, <file>[, <headers>])");
  }

  TRI_Utf8ValueNFC url(isolate, args[0]);

  std::string const infile = TRI_ObjectToString(isolate, args[1]);

  std::error_code existsEc;
  if (!std::filesystem::exists(infile, existsEc)) {
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
      requestData(isolate, *v8connection, fu::RestVerb::Post,
                  std::string_view(*url, url.length()), args[1], headerFields,
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

  if (isExecutionDeadlineReached(isolate)) {
    return;
  }
  // get the connection
  ClientConnection* v8connection = TRI_UnwrapClass<ClientConnection>(
      args.Holder(), WRAP_TYPE_CONNECTION, TRI_IGETC);

  v8::Local<v8::External> wrap = v8::Local<v8::External>::Cast(args.Data());
  ClientFeature* client = static_cast<ClientFeature*>(wrap->Value());

  if (v8connection == nullptr || client == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL(
        "getEndpoint() must be invoked on an arango connection object "
        "instance.");
  }

  // check params
  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("getEndpoint()");
  }

  // the endpoint of the active connection, which connectHandle() may have
  // switched to
  TRI_V8_RETURN_STD_STRING(v8connection->endpoint());
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief imports a CSV file
////////////////////////////////////////////////////////////////////////////////

static uint64_t DefaultChunkSize = 1024 * 1024 * 4;

static void ClientConnection_importCsv(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
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
      separator =
          TRI_ObjectToString(isolate, options->Get(context, separatorKey)
                                          .FromMaybe(v8::Local<v8::Value>()));

      if (separator.length() < 1) {
        TRI_V8_THROW_EXCEPTION_PARAMETER(
            "<options>.separator must be at least one character");
      }
    }

    // quote
    if (TRI_HasProperty(context, isolate, options, quoteKey)) {
      quote = TRI_ObjectToString(
          isolate,
          options->Get(context, quoteKey).FromMaybe(v8::Local<v8::Value>()));

      if (quote.length() > 1) {
        TRI_V8_THROW_EXCEPTION_PARAMETER(
            "<options>.quote must be at most one character");
      }
    }
  }

  ClientConnection* v8connection = TRI_UnwrapClass<ClientConnection>(
      args.Holder(), WRAP_TYPE_CONNECTION, TRI_IGETC);

  application_features::ApplicationServer& server = v8connection->server();
  EncryptionFeature* encryption{};
#ifdef USE_ENTERPRISE
  TRI_ASSERT(server.hasFeature<EncryptionFeature>());
  encryption = &server.getFeature<EncryptionFeature>();
#endif

  v8::Local<v8::External> wrap = v8::Local<v8::External>::Cast(args.Data());
  auto* client = static_cast<ClientFeature*>(wrap->Value());

  SimpleHttpClientParams params(client->requestTimeout(), client->getWarn());
  params.setCompressRequestThreshold(
      client->compressTransfer() ? client->compressRequestThreshold() : 0);

  ImportHelper ih(encryption, *client, v8connection->endpointSpecification(),
                  params, DefaultChunkSize, /*threadCount*/ 1,
                  /*maxErrors*/ UINT64_MAX);

  ih.setQuote(quote);
  ih.setSeparator(separator);

  std::string fileName = TRI_ObjectToString(isolate, args[0]);
  std::string collectionName = TRI_ObjectToString(isolate, args[1]);

  if (ih.importDelimited(collectionName, fileName, "", ImportHelper::CSV)) {
    v8::Local<v8::Object> result = v8::Object::New(isolate);

    result
        ->Set(context, TRI_V8_ASCII_STRING(isolate, "lines"),
              v8::Integer::New(isolate, (int32_t)ih.getReadLines()))
        .FromMaybe(false);

    result
        ->Set(context, TRI_V8_ASCII_STRING(isolate, "created"),
              v8::Integer::New(isolate, (int32_t)ih.getNumberCreated()))
        .FromMaybe(false);

    result
        ->Set(context, TRI_V8_ASCII_STRING(isolate, "errors"),
              v8::Integer::New(isolate, (int32_t)ih.getNumberErrors()))
        .FromMaybe(false);

    result
        ->Set(context, TRI_V8_ASCII_STRING(isolate, "updated"),
              v8::Integer::New(isolate, (int32_t)ih.getNumberUpdated()))
        .FromMaybe(false);

    result
        ->Set(context, TRI_V8_ASCII_STRING(isolate, "ignored"),
              v8::Integer::New(isolate, (int32_t)ih.getNumberIgnored()))
        .FromMaybe(false);

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

  ClientConnection* v8connection = TRI_UnwrapClass<ClientConnection>(
      args.Holder(), WRAP_TYPE_CONNECTION, TRI_IGETC);

  application_features::ApplicationServer& server = v8connection->server();

  EncryptionFeature* encryption{};
#ifdef USE_ENTERPRISE
  TRI_ASSERT(server.hasFeature<EncryptionFeature>());
  encryption = &server.getFeature<EncryptionFeature>();
#endif

  v8::Local<v8::External> wrap = v8::Local<v8::External>::Cast(args.Data());
  ClientFeature* client = static_cast<ClientFeature*>(wrap->Value());

  SimpleHttpClientParams params(client->requestTimeout(), client->getWarn());
  params.setCompressRequestThreshold(
      client->compressTransfer() ? client->compressRequestThreshold() : 0);

  ImportHelper ih(encryption, *client, v8connection->endpointSpecification(),
                  params, DefaultChunkSize, /*threadCount*/ 1,
                  /*maxErrors*/ UINT64_MAX);

  std::string fileName = TRI_ObjectToString(isolate, args[0]);
  std::string collectionName = TRI_ObjectToString(isolate, args[1]);
  auto context = TRI_IGETC;

  if (ih.importJson(collectionName, fileName, false)) {
    v8::Local<v8::Object> result = v8::Object::New(isolate);

    result
        ->Set(context, TRI_V8_ASCII_STRING(isolate, "lines"),
              v8::Integer::New(isolate, (int32_t)ih.getReadLines()))
        .FromMaybe(false);

    result
        ->Set(context, TRI_V8_ASCII_STRING(isolate, "created"),
              v8::Integer::New(isolate, (int32_t)ih.getNumberCreated()))
        .FromMaybe(false);

    result
        ->Set(context, TRI_V8_ASCII_STRING(isolate, "errors"),
              v8::Integer::New(isolate, (int32_t)ih.getNumberErrors()))
        .FromMaybe(false);

    result
        ->Set(context, TRI_V8_ASCII_STRING(isolate, "updated"),
              v8::Integer::New(isolate, (int32_t)ih.getNumberUpdated()))
        .FromMaybe(false);

    result
        ->Set(context, TRI_V8_ASCII_STRING(isolate, "ignored"),
              v8::Integer::New(isolate, (int32_t)ih.getNumberIgnored()))
        .FromMaybe(false);

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

  if (isExecutionDeadlineReached(isolate)) {
    return;
  }

  // get the connection
  ClientConnection* v8connection = TRI_UnwrapClass<ClientConnection>(
      args.Holder(), WRAP_TYPE_CONNECTION, TRI_IGETC);

  if (v8connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL(
        "lastHttpReturnCode() must be invoked on an arango connection object "
        "instance.");
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

  if (isExecutionDeadlineReached(isolate)) {
    return;
  }

  // get the connection
  ClientConnection* v8connection = TRI_UnwrapClass<ClientConnection>(
      args.Holder(), WRAP_TYPE_CONNECTION, TRI_IGETC);

  if (v8connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL(
        "lastErrorMessage() must be invoked on an arango connection object "
        "instance.");
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

  if (isExecutionDeadlineReached(isolate)) {
    return;
  }

  // get the connection
  ClientConnection* v8connection = TRI_UnwrapClass<ClientConnection>(
      args.Holder(), WRAP_TYPE_CONNECTION, TRI_IGETC);

  if (v8connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL(
        "isConnected() must be invoked on an arango connection object "
        "instance.");
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

static void ClientConnection_forceJson(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (isExecutionDeadlineReached(isolate)) {
    return;
  }

  // get the connection
  ClientConnection* v8connection = TRI_UnwrapClass<ClientConnection>(
      args.Holder(), WRAP_TYPE_CONNECTION, TRI_IGETC);

  if (v8connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL(
        "forceJson() must be invoked on an arango connection object instance.");
  }

  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("forceJson(bool)");
  }

  v8::Local<v8::External> wrap = v8::Local<v8::External>::Cast(args.Data());
  ClientFeature* client = static_cast<ClientFeature*>(wrap->Value());

  if (client == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL(
        "forceJson() unable to get client instance");
  }

  bool forceJson = TRI_ObjectToBoolean(isolate, args[0]);
  v8connection->setForceJson(forceJson);

  client->setForceJson(forceJson);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "timeout"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_timeout(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (isExecutionDeadlineReached(isolate)) {
    return;
  }

  // get the connection
  ClientConnection* v8connection = TRI_UnwrapClass<ClientConnection>(
      args.Holder(), WRAP_TYPE_CONNECTION, TRI_IGETC);

  if (v8connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL(
        "timeout() must be invoked on an arango connection object instance.");
  }

  if (args.Length() == 0) {
    TRI_V8_RETURN(v8::Number::New(isolate, v8connection->timeout()));
  } else {
    double value = TRI_ObjectToDouble(isolate, args[0]);
    v8connection->timeout(value);

    v8::Local<v8::External> wrap = v8::Local<v8::External>::Cast(args.Data());
    ClientFeature* client = static_cast<ClientFeature*>(wrap->Value());

    if (client == nullptr) {
      TRI_V8_THROW_EXCEPTION_INTERNAL(
          "timeout() unable to get client instance");
    }

    client->requestTimeout(value);

    TRI_V8_RETURN_UNDEFINED();
  }

  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "compressTransfer"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_compressTransfer(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  v8::Local<v8::External> wrap = v8::Local<v8::External>::Cast(args.Data());
  ClientFeature* client = static_cast<ClientFeature*>(wrap->Value());

  if (client == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL(
        "compressTransfer() unable to get client instance");
  }

  if (args.Length() == 0) {
    TRI_V8_RETURN(v8::Boolean::New(isolate, client->compressTransfer()));
  } else {
    bool value = TRI_ObjectToBoolean(isolate, args[0]);
    client->setCompressTransfer(value);

    TRI_V8_RETURN_UNDEFINED();
  }

  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "jwtRenewalThreshold"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_jwtRenewalThreshold(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  v8::Local<v8::External> wrap = v8::Local<v8::External>::Cast(args.Data());
  ClientFeature* client = static_cast<ClientFeature*>(wrap->Value());

  if (client == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL(
        "jwtRenewalThreshold() unable to get client instance");
  }

  if (args.Length() == 0) {
    // Get current value
    TRI_V8_RETURN(v8::Number::New(isolate, client->jwtRenewalThreshold()));
  } else {
    // Set new value
    double value = TRI_ObjectToDouble(isolate, args[0]);
    client->setJwtRenewalThreshold(value);

    TRI_V8_RETURN_UNDEFINED();
  }

  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "toString"
////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_toString(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (isExecutionDeadlineReached(isolate)) {
    return;
  }

  // get the connection
  ClientConnection* v8connection = TRI_UnwrapClass<ClientConnection>(
      args.Holder(), WRAP_TYPE_CONNECTION, TRI_IGETC);

  if (v8connection == nullptr) {
    // when invoking ArangoConnection.toString() we end here, i.e. printObject
    // does this. be silent about this.
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

static void ClientConnection_getVersion(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (isExecutionDeadlineReached(isolate)) {
    return;
  }

  // get the connection
  ClientConnection* v8connection = TRI_UnwrapClass<ClientConnection>(
      args.Holder(), WRAP_TYPE_CONNECTION, TRI_IGETC);

  if (v8connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL(
        "getVersion() must be invoked on an arango connection object "
        "instance.");
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

  if (isExecutionDeadlineReached(isolate)) {
    return;
  }

  // get the connection
  ClientConnection* v8connection = TRI_UnwrapClass<ClientConnection>(
      args.Holder(), WRAP_TYPE_CONNECTION, TRI_IGETC);

  if (v8connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL(
        "getMode() must be invoked on an arango connection object instance.");
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

static void ClientConnection_getRole(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (isExecutionDeadlineReached(isolate)) {
    return;
  }

  // get the connection
  ClientConnection* v8connection = TRI_UnwrapClass<ClientConnection>(
      args.Holder(), WRAP_TYPE_CONNECTION, TRI_IGETC);

  if (v8connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL(
        "getRole() must be invoked on an arango connection object instance.");
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

static void ClientConnection_getDatabaseName(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (isExecutionDeadlineReached(isolate)) {
    return;
  }

  // get the connection
  ClientConnection* v8connection = TRI_UnwrapClass<ClientConnection>(
      args.Holder(), WRAP_TYPE_CONNECTION, TRI_IGETC);

  if (v8connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL(
        "getDatabaseName() must be invoked on an arango connection object "
        "instance.");
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

  if (isExecutionDeadlineReached(isolate)) {
    return;
  }

  // get the connection
  ClientConnection* v8connection = TRI_UnwrapClass<ClientConnection>(
      args.Holder(), WRAP_TYPE_CONNECTION, TRI_IGETC);

  v8::Local<v8::External> wrap = v8::Local<v8::External>::Cast(args.Data());
  ClientFeature* client = static_cast<ClientFeature*>(wrap->Value());

  if (v8connection == nullptr || client == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL(
        "setDatabaseName() must be invoked on an arango connection object "
        "instance.");
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

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
////////////////////////////////////////////////////////////////////////////////////////
/// @brief ClientConnection method "reconnectWithNewPassword" for test
/// environment only
////////////////////////////////////////////////////////////////////////////////////////

static void ClientConnection_reconnectWithNewPassword(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (isExecutionDeadlineReached(isolate)) {
    return;
  }

  // get the connection
  ClientConnection* v8connection = TRI_UnwrapClass<ClientConnection>(
      args.Holder(), WRAP_TYPE_CONNECTION, TRI_IGETC);

  if (v8connection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL(
        "reconnectWithNewPassword() must be invoked on an arango connection "
        "object "
        "instance.");
  }

  if (args.Length() != 1 || !args[0]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE("reconnectWithNewPassword(<password>)");
  }

  std::string const password = TRI_ObjectToString(isolate, args[0]);
  v8connection->reconnectWithNewPassword(password);

  TRI_V8_RETURN_TRUE();
  TRI_V8_TRY_CATCH_END
}
#endif

static void JS_getAddrInfo(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("getAddrInfo(<hostname>)");
  }

  std::string hostNameStr = TRI_ObjectToString(isolate, args[0]);

  boost::asio::io_context io_ctx;
  boost::asio::ip::tcp::resolver resolver(io_ctx);
  boost::system::error_code ec;

  auto results = resolver.resolve(hostNameStr, "", ec);

  if (ec) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, ec.message().c_str());
  }

  auto context = TRI_IGETC;
  v8::Local<v8::Object> array = v8::Array::New(isolate);
  uint32_t i = 0;

  for (const auto& entry : results) {
    std::string addr_str = entry.endpoint().address().to_string();
    v8::Local<v8::String> oneAddr =
        TRI_V8_ASCII_STRING(isolate, addr_str.c_str());
    array->Set(context, i, oneAddr).FromMaybe(false);
    i++;
  }

  TRI_V8_RETURN(array);
  TRI_V8_TRY_CATCH_END
}

void arangodb::TRI_InitV8ClientConnection(v8::Isolate* isolate,
                                          v8::Local<v8::Context> context,
                                          ClientConnection& connection,
                                          ClientFeature& client) {
  v8::Local<v8::Value> v8client = v8::External::New(isolate, &client);

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

#ifdef ARANGODB_ENABLE_FAILURE_TESTS
  connection_proto->Set(
      isolate, "fuzzRequests",
      v8::FunctionTemplate::New(isolate, ClientConnection_httpFuzzRequests));
#endif

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
      isolate, "forceJson",
      v8::FunctionTemplate::New(isolate, ClientConnection_forceJson, v8client));

  connection_proto->Set(
      isolate, "reconnect",
      v8::FunctionTemplate::New(isolate, ClientConnection_reconnect, v8client));

  connection_proto->Set(
      isolate, "getConnectionHandle",
      v8::FunctionTemplate::New(isolate, ClientConnection_getHandle, v8client));

  connection_proto->Set(
      isolate, "getConnectionHandleTable",
      v8::FunctionTemplate::New(isolate, ClientConnection_getHandleTable,
                                v8client));

  connection_proto->Set(isolate, "connectHandle",
                        v8::FunctionTemplate::New(
                            isolate, ClientConnection_connectHandle, v8client));

  connection_proto->Set(
      isolate, "disconnectHandle",
      v8::FunctionTemplate::New(isolate, ClientConnection_disconnectHandle,
                                v8client));

  connection_proto->Set(
      isolate, "flushConnectionCache",
      v8::FunctionTemplate::New(isolate, ClientConnection_flushConnectionCache,
                                v8client));

  connection_proto->Set(isolate, "connectedUser",
                        v8::FunctionTemplate::New(
                            isolate, ClientConnection_connectedUser, v8client));

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  connection_proto->Set(
      isolate, "reconnectWithNewPassword",
      v8::FunctionTemplate::New(
          isolate, ClientConnection_reconnectWithNewPassword, v8client));
#endif

  connection_proto->Set(
      isolate, "protocol",
      v8::FunctionTemplate::New(isolate, ClientConnection_protocol, v8client));

  connection_proto->Set(
      isolate, "timeout",
      v8::FunctionTemplate::New(isolate, ClientConnection_timeout, v8client));

  connection_proto->Set(
      isolate, "compressTransfer",
      v8::FunctionTemplate::New(isolate, ClientConnection_compressTransfer,
                                v8client));

  connection_proto->Set(
      isolate, "jwtRenewalThreshold",
      v8::FunctionTemplate::New(isolate, ClientConnection_jwtRenewalThreshold,
                                v8client));

  connection_proto->Set(
      isolate, "toString",
      v8::FunctionTemplate::New(isolate, ClientConnection_toString, v8client));

  connection_proto->Set(
      isolate, "getVersion",
      v8::FunctionTemplate::New(isolate, ClientConnection_getVersion));

  connection_proto->Set(
      isolate, "getMode",
      v8::FunctionTemplate::New(isolate, ClientConnection_getMode));

  connection_proto->Set(
      isolate, "getRole",
      v8::FunctionTemplate::New(isolate, ClientConnection_getRole));

  connection_proto->Set(
      isolate, "getDatabaseName",
      v8::FunctionTemplate::New(isolate, ClientConnection_getDatabaseName));

  connection_proto->Set(
      isolate, "setDatabaseName",
      v8::FunctionTemplate::New(isolate, ClientConnection_setDatabaseName,
                                v8client));

  connection_proto->Set(isolate, "setJwtSecret",
                        v8::FunctionTemplate::New(
                            isolate, ClientConnection_setJwtSecret, v8client));

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

  TRI_AddGlobalVariableVocbase(
      isolate, TRI_V8_ASCII_STRING(isolate, "ArangoConnection"),
      connection_proto->NewInstance(TRI_IGETC).FromMaybe(
          v8::Local<v8::Object>()));

  ConnectionTempl.Reset(isolate, connection_inst);

  TRI_AddGlobalFunctionVocbase(
      isolate, TRI_V8_ASCII_STRING(isolate, "getAddrInfo"), JS_getAddrInfo);

  // add the client connection to the context:
  TRI_AddGlobalVariableVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate, "SYS_ARANGO"),
                               WrapV8ClientConnection(isolate, &connection));
}
