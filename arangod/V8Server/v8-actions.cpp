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
////////////////////////////////////////////////////////////////////////////////

#include "v8-actions.h"
#include "Actions/ActionFeature.h"
#include "Actions/actions.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "ApplicationFeatures/V8SecurityFeature.h"
#include "Basics/MutexLocker.h"
#include "Basics/ReadLocker.h"
#include "Basics/StringUtils.h"
#include "Basics/WriteLocker.h"
#include "Basics/conversions.h"
#include "Basics/files.h"
#include "Basics/tri-strings.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "Futures/Utilities.h"
#include "GeneralServer/GeneralServer.h"
#include "GeneralServer/ServerSecurityFeature.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Network/Methods.h"
#include "Network/NetworkFeature.h"
#include "Network/Utils.h"
#include "Rest/GeneralRequest.h"
#include "Rest/HttpRequest.h"
#include "Rest/HttpResponse.h"
#include "StorageEngine/HotBackup.h"
#include "Utils/ExecContext.h"
#include "V8/JavaScriptSecurityContext.h"
#include "V8/v8-buffer.h"
#include "V8/v8-conv.h"
#include "V8/v8-utils.h"
#include "V8/v8-vpack.h"
#include "V8Server/FoxxQueuesFeature.h"
#include "V8Server/V8Context.h"
#include "V8Server/V8DealerFeature.h"
#include "V8Server/v8-vocbase.h"
#include "VocBase/ticks.h"
#include "VocBase/vocbase.h"

#include <velocypack/Buffer.h>
#include <velocypack/Builder.h>
#include <velocypack/Parser.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

static TRI_action_result_t ExecuteActionVocbase(TRI_vocbase_t*, v8::Isolate*,
                                                TRI_action_t const*,
                                                v8::Handle<v8::Function> callback,
                                                GeneralRequest*, GeneralResponse*);

////////////////////////////////////////////////////////////////////////////////
/// @brief action description for V8
////////////////////////////////////////////////////////////////////////////////

class v8_action_t final : public TRI_action_t {
 public:
  v8_action_t() : TRI_action_t(), _callbacks(), _callbacksLock() {}

  void visit(void* data) override {
    v8::Isolate* isolate = static_cast<v8::Isolate*>(data);

    WRITE_LOCKER(writeLocker, _callbacksLock);

    auto it = _callbacks.find(isolate);

    if (it != _callbacks.end()) {
      (*it).second.Reset();  // dispose persistent
      _callbacks.erase(it);  // remove entry from map
    }
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief creates callback for a context
  //////////////////////////////////////////////////////////////////////////////

  void createCallback(v8::Isolate* isolate, v8::Handle<v8::Function> callback) {
    WRITE_LOCKER(writeLocker, _callbacksLock);

    auto it = _callbacks.find(isolate);

    if (it == _callbacks.end()) {
      _callbacks[isolate].Reset(isolate, callback);
    } else {
      LOG_TOPIC("982a6", ERR, Logger::V8) << "cannot recreate callback for '" << _url << "'";
    }
  }

  TRI_action_result_t execute(TRI_vocbase_t* vocbase, GeneralRequest* request,
                              GeneralResponse* response, Mutex* dataLock,
                              void** data) override {
    TRI_action_result_t result;

    // allow use database execution in rest calls?
    bool allowUseDatabase = _allowUseDatabase || ActionFeature::ACTION->allowUseDatabase();

    // get a V8 context
    V8ContextGuard guard(vocbase, _isSystem ?
        JavaScriptSecurityContext::createInternalContext() :
        JavaScriptSecurityContext::createRestActionContext(allowUseDatabase));

    // locate the callback
    READ_LOCKER(readLocker, _callbacksLock);

    {
      auto it = _callbacks.find(guard.isolate());

      if (it == _callbacks.end()) {
        LOG_TOPIC("94556", WARN, arangodb::Logger::FIXME)
            << "no callback function for JavaScript action '" << _url << "'";

        result.isValid = true;
        response->setResponseCode(rest::ResponseCode::NOT_FOUND);

        return result;
      }

      // and execute it
      {
        // cppcheck-suppress redundantPointerOp
        MUTEX_LOCKER(mutexLocker, *dataLock);

        if (*data != nullptr) {
          result.canceled = true;
          return result;
        }

        *data = (void*)guard.isolate();
      }
      v8::HandleScope scope(guard.isolate());
      auto localFunction = v8::Local<v8::Function>::New(guard.isolate(), it->second);

      // we can release the lock here already as no other threads will
      // work in our isolate at this time
      readLocker.unlock();

      try {
        result = ExecuteActionVocbase(vocbase, guard.isolate(), this,
                                      localFunction, request, response);
      } catch (...) {
        result.isValid = false;
      }

      {
        // cppcheck-suppress redundantPointerOp
        MUTEX_LOCKER(mutexLocker, *dataLock);
        *data = nullptr;
      }
    }

    return result;
  }

  void cancel(Mutex* dataLock, void** data) override {
    {
      // cppcheck-suppress redundantPointerOp
      MUTEX_LOCKER(mutexLocker, *dataLock);

      // either we have not yet reached the execute above or we are already done
      if (*data == nullptr) {
        *data = (void*)1;  // mark as canceled
      }

      // data is set, cancel the execution
      else {
        if (!((v8::Isolate*)*data)->IsExecutionTerminating()) {
          ((v8::Isolate*)*data)->TerminateExecution();
        }
      }
    }
  }

 private:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief callback dictionary
  //////////////////////////////////////////////////////////////////////////////

  std::unordered_map<v8::Isolate*, v8::Persistent<v8::Function>> _callbacks;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief lock for the callback dictionary
  //////////////////////////////////////////////////////////////////////////////

  ReadWriteLock _callbacksLock;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief parses the action options
////////////////////////////////////////////////////////////////////////////////

static void ParseActionOptions(v8::Isolate* isolate, TRI_v8_global_t* v8g,
                               TRI_action_t* action, v8::Handle<v8::Object> options) {
  auto context = TRI_IGETC;
  TRI_GET_GLOBAL_STRING(PrefixKey);
  // check the "prefix" field
  if (TRI_HasProperty(context, isolate, options, PrefixKey)) {
    action->_isPrefix = TRI_ObjectToBoolean(isolate, options->Get(context, PrefixKey).FromMaybe(v8::Local<v8::Value>()));
  } else {
    action->_isPrefix = false;
  }

  // check the "allowUseDatabase" field
  TRI_GET_GLOBAL_STRING(AllowUseDatabaseKey);
  if (TRI_HasProperty(context, isolate, options, AllowUseDatabaseKey)) {
    action->_allowUseDatabase =
      TRI_ObjectToBoolean(isolate, options->Get(context, AllowUseDatabaseKey).FromMaybe(v8::Local<v8::Value>()));
  } else {
    action->_allowUseDatabase = false;
  }

  TRI_GET_GLOBAL_STRING(IsSystemKey);
  if (TRI_HasProperty(context, isolate, options, IsSystemKey)) {
    action->_isSystem =
      TRI_ObjectToBoolean(isolate, options->Get(context, IsSystemKey).FromMaybe(v8::Local<v8::Value>()));
  } else {
    action->_isSystem = false;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add cookie
////////////////////////////////////////////////////////////////////////////////

static void AddCookie(v8::Isolate* isolate, TRI_v8_global_t const* v8g,
                      HttpResponse* response, v8::Handle<v8::Object> data) {
  auto context = TRI_IGETC;
  std::string name;
  std::string value;
  int lifeTimeSeconds = 0;
  std::string path = "/";
  std::string domain = "";
  bool secure = false;
  bool httpOnly = false;

  TRI_GET_GLOBAL_STRING(NameKey);
  if (TRI_HasProperty(context, isolate, data, NameKey)) {
    v8::Handle<v8::Value> v = data->Get(context, NameKey).FromMaybe(v8::Handle<v8::Value>());
    name = TRI_ObjectToString(isolate, v);
  } else {
    // something is wrong here
    return;
  }
  TRI_GET_GLOBAL_STRING(ValueKey);
  if (TRI_HasProperty(context, isolate, data, ValueKey)) {
    v8::Handle<v8::Value> v = data->Get(context, ValueKey).FromMaybe(v8::Local<v8::Value>());
    value = TRI_ObjectToString(isolate, v);
  } else {
    // something is wrong here
    return;
  }
  TRI_GET_GLOBAL_STRING(LifeTimeKey);
  if (TRI_HasProperty(context, isolate, data, LifeTimeKey)) {
    v8::Handle<v8::Value> v = data->Get(context, LifeTimeKey).FromMaybe(v8::Local<v8::Value>());
    lifeTimeSeconds = (int)TRI_ObjectToInt64(isolate, v);
  }
  TRI_GET_GLOBAL_STRING(PathKey);
  if (TRI_HasProperty(context, isolate, data, PathKey) && !data->Get(context, PathKey).FromMaybe(v8::Local<v8::Value>())->IsUndefined()) {
    v8::Handle<v8::Value> v = data->Get(context, PathKey).FromMaybe(v8::Handle<v8::Value>());
    path = TRI_ObjectToString(isolate, v);
  }
  TRI_GET_GLOBAL_STRING(DomainKey);
  if (TRI_HasProperty(context, isolate, data, DomainKey) && !data->Get(context, DomainKey).FromMaybe(v8::Local<v8::Value>())->IsUndefined()) {
    v8::Handle<v8::Value> v = data->Get(context, DomainKey).FromMaybe(v8::Local<v8::Value>());
    domain = TRI_ObjectToString(isolate, v);
  }
  TRI_GET_GLOBAL_STRING(SecureKey);
  if (TRI_HasProperty(context, isolate, data, SecureKey)) {
    v8::Handle<v8::Value> v = data->Get(context, SecureKey).FromMaybe(v8::Local<v8::Value>());
    secure = TRI_ObjectToBoolean(isolate, v);
  }
  TRI_GET_GLOBAL_STRING(HttpOnlyKey);
  if (TRI_HasProperty(context, isolate, data, HttpOnlyKey)) {
    v8::Handle<v8::Value> v = data->Get(context, HttpOnlyKey).FromMaybe(v8::Local<v8::Value>());
    httpOnly = TRI_ObjectToBoolean(isolate, v);
  }

  response->setCookie(name, value, lifeTimeSeconds, path, domain, secure, httpOnly);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convert a C++ HttpRequest to a V8 request object
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Object> TRI_RequestCppToV8(v8::Isolate* isolate,
                                          TRI_v8_global_t const* v8g,
                                          arangodb::GeneralRequest* request,
                                          TRI_action_t const* action) {
  // setup the request
  v8::Handle<v8::Object> req = v8::Object::New(isolate);
  auto context = TRI_IGETC;
  // Example:
  //      {
  //        path : "/full/path/suffix1/suffix2",
  //
  //        prefix : "/full/path",
  //
  //        "suffix" : [
  //          "suffix1",
  //          "suffix2"
  //        ],
  //
  //        "parameters" : {
  //          "init" : "true"
  //        },
  //
  //        "headers" : {
  //          "accept" : "text/html",
  //          "accept-encoding" : "gzip, deflate",
  //          "accept-language" : "de-de,en-us;q=0.7,en;q=0.3",
  //          "user-agent" : "Mozilla/5.0"
  //        },
  //
  //        "cookies" : {
  //          "ARANGODB_SESSION_ID" : "0cwuzusd23nw3qiwui84uwqwqw23e"
  //        },
  //
  //        "requestType" : "GET",
  //        "requestBody" : "... only for PUT and POST ...",
  //        "user" : "authenticatedUser"
  //      }

  TRI_GET_GLOBAL_STRING(AuthorizedKey);
  if (request->authenticated()) {
    req->Set(context, AuthorizedKey, v8::True(isolate)).FromMaybe(false);
  } else {
    req->Set(context, AuthorizedKey, v8::False(isolate)).FromMaybe(false);
  }

  // create user or null
  std::string const& user = request->user();

  TRI_GET_GLOBAL_STRING(UserKey);
  if (user.empty()) {
    req->Set(context, UserKey, v8::Null(isolate)).FromMaybe(false);
  } else {
    req->Set(context, UserKey, TRI_V8_STD_STRING(isolate, user)).FromMaybe(false);
  }

  TRI_GET_GLOBAL_STRING(IsAdminUser);
  if (request->authenticated()) {
    if (user.empty() || ExecContext::current().isAdminUser()) {
      req->Set(context, IsAdminUser, v8::True(isolate)).FromMaybe(false);
    } else {
      req->Set(context, IsAdminUser, v8::False(isolate)).FromMaybe(false);
    }
  } else {
    req->Set(context, IsAdminUser, ExecContext::isAuthEnabled() ? v8::False(isolate)
                                                       : v8::True(isolate)).FromMaybe(false);;
  }

  // create database attribute
  std::string const& database = request->databaseName();
  TRI_ASSERT(!database.empty());

  TRI_GET_GLOBAL_STRING(DatabaseKey);
  req->Set(context, DatabaseKey, TRI_V8_STD_STRING(isolate, database)).FromMaybe(false);

  // set the full url
  std::string const& fullUrl = request->fullUrl();
  TRI_GET_GLOBAL_STRING(UrlKey);
  req->Set(context, UrlKey, TRI_V8_STD_STRING(isolate, fullUrl)).FromMaybe(false);

  // set the protocol
  TRI_GET_GLOBAL_STRING(ProtocolKey);
  if (request->transportType() == Endpoint::TransportType::HTTP) {
    req->Set(context, ProtocolKey, TRI_V8_ASCII_STRING(isolate, "http")).FromMaybe(false);
  } else if (request->transportType() == Endpoint::TransportType::VST) {
    req->Set(context, ProtocolKey, TRI_V8_ASCII_STRING(isolate, "vst")).FromMaybe(false);
  }

  // set the connection info
  ConnectionInfo const& info = request->connectionInfo();

  v8::Handle<v8::Object> serverArray = v8::Object::New(isolate);
  TRI_GET_GLOBAL_STRING(AddressKey);
  serverArray->Set(context, AddressKey, TRI_V8_STD_STRING(isolate, info.serverAddress)).FromMaybe(false);
  TRI_GET_GLOBAL_STRING(PortKey);
  serverArray->Set(context, PortKey, v8::Number::New(isolate, info.serverPort)).FromMaybe(false);
  TRI_GET_GLOBAL_STRING(EndpointKey);
  serverArray->Set(context,
                   EndpointKey,
                   TRI_V8_STD_STRING(isolate, Endpoint::uriForm(info.endpoint))).FromMaybe(false);
  TRI_GET_GLOBAL_STRING(ServerKey);
  req->Set(context,
           ServerKey,
           serverArray).FromMaybe(false);

  TRI_GET_GLOBAL_STRING(PortTypeKey);
  req->DefineOwnProperty(TRI_IGETC, PortTypeKey,
                         TRI_V8_STD_STRING(isolate, info.portType()),
                         static_cast<v8::PropertyAttribute>(v8::ReadOnly))
      .FromMaybe(false);  // ignoring return value

  v8::Handle<v8::Object> clientArray = v8::Object::New(isolate);
  clientArray->Set(context, AddressKey, TRI_V8_STD_STRING(isolate, info.clientAddress)).FromMaybe(false);
  clientArray->Set(context, PortKey, v8::Number::New(isolate, info.clientPort)).FromMaybe(false);
  TRI_GET_GLOBAL_STRING(IdKey);
  clientArray->Set(context, IdKey, TRI_V8_STD_STRING(isolate, std::string("0"))).FromMaybe(false);
  TRI_GET_GLOBAL_STRING(ClientKey);
  req->Set(context, ClientKey, clientArray).FromMaybe(false);

  req->Set(context, TRI_V8_ASCII_STRING(isolate, "internals"), v8::External::New(isolate, request)).FromMaybe(false);

  // copy prefix
  std::string path = request->prefix();
  TRI_GET_GLOBAL_STRING(PrefixKey);
  req->Set(context, PrefixKey, TRI_V8_STD_STRING(isolate, path)).FromMaybe(false);

  // copy header fields
  v8::Handle<v8::Object> headerFields = v8::Object::New(isolate);
  // intentional copy, as we will modify the headers later
  auto headers = request->headers();

  std::string const& acceptPlain = request->contentTypeResponsePlain();

  if (!acceptPlain.empty()) {
    headers.emplace(StaticStrings::Accept, acceptPlain);
  } else {
    switch (request->contentTypeResponse()) {
      case ContentType::UNSET:
      case ContentType::CUSTOM:  // use Content-Type from _headers
        break;
      case ContentType::JSON:    // application/json
        headers.emplace(StaticStrings::Accept, StaticStrings::MimeTypeJson);
        break;
      case ContentType::VPACK:   // application/x-velocypack
        headers.emplace(StaticStrings::Accept, StaticStrings::MimeTypeVPack);
        break;
      case ContentType::TEXT:    // text/plain
        headers.emplace(StaticStrings::Accept, StaticStrings::MimeTypeText);
        break;
      case ContentType::HTML:    // text/html
        headers.emplace(StaticStrings::Accept, StaticStrings::MimeTypeHtml);
        break;
      case ContentType::DUMP:    // application/x-arango-dump
        headers.emplace(StaticStrings::Accept, StaticStrings::MimeTypeDump);
        break;
    }
  }

  switch (request->contentType()) {
    case ContentType::UNSET:
    case ContentType::CUSTOM:  // use Content-Type from _headers
      break;
    case ContentType::JSON:    // application/json
      headers.emplace(StaticStrings::ContentTypeHeader, StaticStrings::MimeTypeJson);
      break;
    case ContentType::VPACK:   // application/x-velocypack
      headers.emplace(StaticStrings::ContentTypeHeader, StaticStrings::MimeTypeVPack);
      break;
    case ContentType::TEXT:    // text/plain
      headers.emplace(StaticStrings::ContentTypeHeader, StaticStrings::MimeTypeText);
      break;
    case ContentType::HTML:    // text/html
      headers.emplace(StaticStrings::ContentTypeHeader, StaticStrings::MimeTypeHtml);
      break;
    case ContentType::DUMP:    // application/x-arango-dump
      headers.emplace(StaticStrings::ContentTypeHeader, StaticStrings::MimeTypeDump);
      break;
  }

  TRI_GET_GLOBAL_STRING(HeadersKey);
  req->Set(context, HeadersKey, headerFields).FromMaybe(false);
  TRI_GET_GLOBAL_STRING(RequestTypeKey);
  TRI_GET_GLOBAL_STRING(RequestBodyKey);

  auto setRequestBodyJsonOrVPack = [&]() {
    if (rest::ContentType::UNSET == request->contentType()) {
      bool digesteable = false;
      try {
        auto parsed = request->payload(true);
        if (parsed.isObject() || parsed.isArray()) {
          request->setDefaultContentType();
          digesteable = true;
        }
      } catch ( ... ) {}
      // ok, no json/vpack after all ;-)
      auto raw = request->rawPayload();
      headers[StaticStrings::ContentLength] =
        StringUtils::itoa(raw.size());
      V8Buffer* buffer = V8Buffer::New(isolate, raw.data(), raw.size());
      auto bufObj = v8::Local<v8::Object>::New(isolate, buffer->_handle);
      TRI_GET_GLOBAL_STRING(RawRequestBodyKey);
      req->Set(context, RawRequestBodyKey, bufObj).FromMaybe(false);
      req->Set(context, RequestBodyKey, TRI_V8_PAIR_STRING(isolate, raw.data(), raw.size())).FromMaybe(false);
      if (!digesteable) {
        return;
      }
    }

    if (rest::ContentType::JSON == request->contentType()) {
      VPackStringRef body = request->rawPayload();
      req->Set(context, RequestBodyKey, TRI_V8_PAIR_STRING(isolate, body.data(), body.size())).FromMaybe(false);
      headers[StaticStrings::ContentLength] =
          StringUtils::itoa(request->contentLength());
    } else if (rest::ContentType::VPACK == request->contentType()) {
      // the VPACK is passed as it is to to JavaScript
      // FIXME not every VPack can be converted to JSON
      VPackSlice slice = request->payload();
      std::string jsonString = slice.toJson();

      LOG_TOPIC("8afce", DEBUG, Logger::COMMUNICATION)
          << "json handed into v8 request:\n"
          << jsonString;

      req->Set(context, RequestBodyKey, TRI_V8_STD_STRING(isolate, jsonString)).FromMaybe(false);
      headers[StaticStrings::ContentLength] = StringUtils::itoa(jsonString.size());
      headers[StaticStrings::ContentTypeHeader] = StaticStrings::MimeTypeJson;
    }
  };

  // copy request type
  switch (request->requestType()) {
    case rest::RequestType::POST: {
      TRI_GET_GLOBAL_STRING(PostConstant);
      req->Set(context, RequestTypeKey, PostConstant).FromMaybe(false);
      setRequestBodyJsonOrVPack();
      break;
    }

    case rest::RequestType::PUT: {
      TRI_GET_GLOBAL_STRING(PutConstant);
      req->Set(context, RequestTypeKey, PutConstant).FromMaybe(false);
      setRequestBodyJsonOrVPack();
      break;
    }

    case rest::RequestType::PATCH: {
      TRI_GET_GLOBAL_STRING(PatchConstant);
      req->Set(context, RequestTypeKey, PatchConstant).FromMaybe(false);
      setRequestBodyJsonOrVPack();
      break;
    }
    case rest::RequestType::OPTIONS: {
      TRI_GET_GLOBAL_STRING(OptionsConstant);
      req->Set(context, RequestTypeKey, OptionsConstant).FromMaybe(false);
      break;
    }
    case rest::RequestType::DELETE_REQ: {
      TRI_GET_GLOBAL_STRING(DeleteConstant);
      req->Set(context, RequestTypeKey, DeleteConstant).FromMaybe(false);
      setRequestBodyJsonOrVPack();
      break;
    }
    case rest::RequestType::HEAD: {
      TRI_GET_GLOBAL_STRING(HeadConstant);
      req->Set(context, RequestTypeKey, HeadConstant).FromMaybe(false);
      break;
    }
    case rest::RequestType::GET:
    default: {
      TRI_GET_GLOBAL_STRING(GetConstant);
      req->Set(context, RequestTypeKey, GetConstant).FromMaybe(false);
      break;
    }
  }

  for (auto const& it : headers) {
    headerFields->Set(context,
                      TRI_V8_STD_STRING(isolate, it.first),
                      TRI_V8_STD_STRING(isolate, it.second)).FromMaybe(false);
  }

  // copy request parameter
  v8::Handle<v8::Object> valuesObject = v8::Object::New(isolate);

  for (auto& it : request->values()) {
    valuesObject->Set(context,
                      TRI_V8_STD_STRING(isolate, it.first),
                      TRI_V8_STD_STRING(isolate, it.second)).FromMaybe(false);
  }

  // copy request array parameter (a[]=1&a[]=2&...)
  for (auto& arrayValue : request->arrayValues()) {
    std::string const& k = arrayValue.first;
    std::vector<std::string> const& v = arrayValue.second;

    v8::Handle<v8::Array> list = v8::Array::New(isolate, static_cast<int>(v.size()));

    for (size_t i = 0; i < v.size(); ++i) {
      list->Set(context, (uint32_t)i, TRI_V8_STD_STRING(isolate, v[i])).FromMaybe(false);
    }

    valuesObject->Set(context, TRI_V8_STD_STRING(isolate, k), list).FromMaybe(false);
  }

  TRI_GET_GLOBAL_STRING(ParametersKey);
  req->Set(context, ParametersKey, valuesObject).FromMaybe(false);

  // copy cookie -- only for http protocol
  if (request->transportType() == Endpoint::TransportType::HTTP) {  // FIXME
    v8::Handle<v8::Object> cookiesObject = v8::Object::New(isolate);

    HttpRequest* httpRequest = dynamic_cast<HttpRequest*>(request);
    if (httpRequest == nullptr) {
      // maybe we can just continue
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                     "invalid request type");
    } else {
      for (auto& it : httpRequest->cookieValues()) {
        cookiesObject->Set(context,
                           TRI_V8_STD_STRING(isolate, it.first),
                           TRI_V8_STD_STRING(isolate, it.second)).FromMaybe(false);
      }
    }
    TRI_GET_GLOBAL_STRING(CookiesKey);
    req->Set(context, CookiesKey, cookiesObject).FromMaybe(false);
  }

  // copy suffix, which comes from the action:
  std::vector<std::string> const& suffixes = request->decodedSuffixes();
  std::vector<std::string> const& rawSuffixes = request->suffixes();

  uint32_t index = 0;
  char const* sep = "";

  size_t const n = suffixes.size();
  v8::Handle<v8::Array> suffixArray =
      v8::Array::New(isolate, static_cast<int>(n - action->_urlParts));
  v8::Handle<v8::Array> rawSuffixArray =
      v8::Array::New(isolate, static_cast<int>(n - action->_urlParts));

  for (size_t s = action->_urlParts; s < n; ++s) {
    suffixArray->Set(context, index, TRI_V8_STD_STRING(isolate, suffixes[s])).FromMaybe(false);
    rawSuffixArray->Set(context, index, TRI_V8_STD_STRING(isolate, rawSuffixes[s])).FromMaybe(false);
    ++index;

    path += sep + suffixes[s];
    sep = "/";
  }

  TRI_GET_GLOBAL_STRING(SuffixKey);
  req->Set(context, SuffixKey, suffixArray).FromMaybe(false);
  TRI_GET_GLOBAL_STRING(RawSuffixKey);
  req->Set(context, RawSuffixKey, rawSuffixArray).FromMaybe(false);

  // copy full path
  TRI_GET_GLOBAL_STRING(PathKey);
  req->Set(context, PathKey, TRI_V8_STD_STRING(isolate, path)).FromMaybe(false);

  return req;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convert a C++ HttpRequest to a V8 request object
////////////////////////////////////////////////////////////////////////////////

// TODO this needs to be generalized
static void ResponseV8ToCpp(v8::Isolate* isolate, TRI_v8_global_t const* v8g,
                            GeneralRequest* request, v8::Handle<v8::Object> const res,
                            GeneralResponse* response) {
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  TRI_ASSERT(request != nullptr);

  using arangodb::Endpoint;

  // set response code
  TRI_GET_GLOBAL_STRING(ResponseCodeKey);
  if (TRI_HasProperty(context, isolate, res, ResponseCodeKey)) {
    uint64_t foxxcode = TRI_ObjectToInt64(isolate,
                                          res->Get(context,
                                                   ResponseCodeKey
                                                   ).FromMaybe(v8::Local<v8::Value>()));
    if (GeneralResponse::isValidResponseCode(foxxcode)) {
      response->setResponseCode(static_cast<rest::ResponseCode>(foxxcode));
    } else {
      response->setResponseCode(rest::ResponseCode::SERVER_ERROR);
      LOG_TOPIC("37d37", ERR, Logger::V8)
        << "invalid http status code specified " << foxxcode
        << " diverting to 500";
    }
  } else {
    response->setResponseCode(rest::ResponseCode::OK);
  }

  // string should not be used
  std::string contentType = StaticStrings::MimeTypeJsonNoEncoding;
  bool autoContent = true;
  TRI_GET_GLOBAL_STRING(ContentTypeKey);
  if (TRI_HasProperty(context, isolate, res, ContentTypeKey)) {
    contentType = TRI_ObjectToString(isolate, res->Get(context, ContentTypeKey).FromMaybe(v8::Local<v8::Value>()));

    if ((contentType.find(StaticStrings::MimeTypeJsonNoEncoding) == std::string::npos) &&
        (contentType.find(StaticStrings::MimeTypeVPack) == std::string::npos)) {
      autoContent = false;
    }
    switch (response->transportType()) {
      case Endpoint::TransportType::HTTP:
        if (autoContent) {
          response->setContentType(rest::ContentType::JSON);
        } else {
          response->setContentType(contentType);
        }
        break;

      case Endpoint::TransportType::VST:
        if (!autoContent) {
          response->setContentType(contentType);
        } else {
          response->setHeaderNC(arangodb::StaticStrings::ContentTypeHeader, contentType);
        }
        break;

      default:
        throw std::logic_error("unknown transport type");
    }
  }

  // .........................................................................
  // body
  // .........................................................................
  //

  bool bodySet = false;
  TRI_GET_GLOBAL_STRING(BodyKey);
  if (TRI_HasProperty(context, isolate, res, BodyKey)) {
    // check if we should apply result transformations
    // transformations turn the result from one type into another
    // a JavaScript action can request transformations by
    // putting a list of transformations into the res.transformations
    // array, e.g. res.transformations = [ "base64encode" ]
    TRI_GET_GLOBAL_STRING(TransformationsKey);
    v8::Handle<v8::Value> transformArray = res->Get(context, TransformationsKey).FromMaybe(v8::Local<v8::Value>());

    switch (response->transportType()) {
      case Endpoint::TransportType::HTTP: {
        //  OBI FIXME - vpack
        //  HTTP SHOULD USE vpack interface

        HttpResponse* httpResponse = dynamic_cast<HttpResponse*>(response);
        if (transformArray->IsArray()) {
          TRI_GET_GLOBAL_STRING(BodyKey);
          std::string out(TRI_ObjectToString(isolate, res->Get(context, BodyKey).FromMaybe(v8::Local<v8::Value>())));
          v8::Handle<v8::Array> transformations = transformArray.As<v8::Array>();

          for (uint32_t i = 0; i < transformations->Length(); i++) {
            v8::Handle<v8::Value> transformator =
              transformations->Get(context, v8::Integer::New(isolate, i)).FromMaybe(v8::Local<v8::Value>());
            std::string name = TRI_ObjectToString(isolate, transformator);

            // check available transformations
            if (name == "base64encode") {
              // base64-encode the result
              out = StringUtils::encodeBase64(out);
              // set the correct content-encoding header
              response->setHeaderNC(StaticStrings::ContentEncoding, StaticStrings::Base64);
            } else if (name == "base64decode") {
              // base64-decode the result
              out = StringUtils::decodeBase64(out);
              // set the correct content-encoding header
              response->setHeaderNC(StaticStrings::ContentEncoding, StaticStrings::Binary);
            }
          }

          // what type is out? always json?
          httpResponse->body().appendText(out);
          httpResponse->sealBody();
        } else {
          TRI_GET_GLOBAL_STRING(BodyKey);
          v8::Handle<v8::Value> b = res->Get(context, BodyKey).FromMaybe(v8::Local<v8::Value>());
          if (V8Buffer::hasInstance(isolate, b)) {
            // body is a Buffer
            auto obj = b.As<v8::Object>();
            httpResponse->body().appendText(V8Buffer::data(isolate, obj),
                                            V8Buffer::length(isolate, obj));
            httpResponse->sealBody();
          } else if (autoContent && request->contentTypeResponse() == rest::ContentType::VPACK) {
            // use velocypack
            try {
              std::string json = TRI_ObjectToString(isolate, res->Get(context, BodyKey).FromMaybe(v8::Local<v8::Value>()));
              VPackBuffer<uint8_t> buffer;
              VPackBuilder builder(buffer);
              VPackParser parser(builder);
              parser.parse(json);
              httpResponse->setContentType(rest::ContentType::VPACK);
              httpResponse->setPayload(std::move(buffer));
            } catch (...) {
              httpResponse->body().appendText(TRI_ObjectToString(isolate, res->Get(context, BodyKey).FromMaybe(v8::Local<v8::Value>())));
              httpResponse->sealBody();
            }
          } else {
            // treat body as a string
            httpResponse->body().appendText(TRI_ObjectToString(isolate, res->Get(context, BodyKey).FromMaybe(v8::Local<v8::Value>())));
            httpResponse->sealBody();
          }
        }
      } break;

      case Endpoint::TransportType::VST: {
        VPackBuffer<uint8_t> buffer;
        VPackBuilder builder(buffer);

        v8::Handle<v8::Value> v8Body = res->Get(context, BodyKey).FromMaybe(v8::Local<v8::Value>());
        std::string out;

        // decode and set out
        if (transformArray->IsArray()) {
          TRI_GET_GLOBAL_STRING(BodyKey);
          out = TRI_ObjectToString(isolate, res->Get(context, BodyKey).FromMaybe(v8::Local<v8::Value>()));  // there is one case where
                                                                 // we do not need a string
          v8::Handle<v8::Array> transformations = transformArray.As<v8::Array>();

          for (uint32_t i = 0; i < transformations->Length(); i++) {
            v8::Handle<v8::Value> transformator =
              transformations->Get(context, v8::Integer::New(isolate, i)).FromMaybe(v8::Local<v8::Value>());
            std::string name = TRI_ObjectToString(isolate, transformator);

            // we do not decode in the vst case
            // check available transformations
            if (name == "base64decode") {
              out = StringUtils::decodeBase64(out);
            }
          }
        }

        // out is not set
        if (out.empty()) {
          if (autoContent && !V8Buffer::hasInstance(isolate, v8Body)) {
            if (v8Body->IsString()) {
              out = TRI_ObjectToString(isolate, res->Get(context, BodyKey).FromMaybe(v8::Local<v8::Value>()));  // should get moved
            } else {
              TRI_V8ToVPack(isolate, builder, v8Body, false);
              response->setContentType(rest::ContentType::VPACK);
            }
          } else if (V8Buffer::hasInstance(isolate,
                                           v8Body)) {  // body form buffer - could
                                                       // contain json or not
            // REVIEW (fc) - is this correct?
            auto obj = v8Body.As<v8::Object>();
            out = std::string(V8Buffer::data(isolate, obj), V8Buffer::length(isolate, obj));
          } else {  // body is text - does not contain json
            out = TRI_ObjectToString(isolate, res->Get(context, BodyKey).FromMaybe(v8::Local<v8::Value>()));  // should get moved
          }
        }

        // there is a text body
        if (!out.empty()) {
          bool gotJson = false;
          if (autoContent) {  // the text body could contain an object
            try {
              VPackParser parser(builder);  // add json as vpack to the builder
              parser.parse(out, false);
              gotJson = true;
              response->setContentType(rest::ContentType::VPACK);
            } catch (...) {  // do nothing
                             // json could not be converted
                             // there was no json - change content type?
              LOG_TOPIC("32d35", DEBUG, Logger::COMMUNICATION)
                  << "failed to parse json:\n"
                  << out;
            }
          }

          if (!gotJson) {
            // don't go via the builder - when not added via parser
            buffer.reset();
            buffer.append(out);
          }
        }

        response->setPayload(std::move(buffer));
        break;
      }

      default: { throw std::logic_error("unknown transport type"); }
    }
    bodySet = true;
  }

  // .........................................................................
  // body from file
  // .........................................................................
  TRI_GET_GLOBAL_STRING(BodyFromFileKey);
  if (!bodySet && TRI_HasProperty(context, isolate, res, BodyFromFileKey)) {
    TRI_Utf8ValueNFC filename(isolate, res->Get(context, BodyFromFileKey).FromMaybe(v8::Local<v8::Value>()));
    size_t length;
    char* content = TRI_SlurpFile(*filename, &length);

    if (content == nullptr) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_FILE_NOT_FOUND,
                                     std::string("unable to read file '") +
                                         *filename + "'");
    }

    switch (response->transportType()) {
      case Endpoint::TransportType::HTTP: {
        HttpResponse* httpResponse = dynamic_cast<HttpResponse*>(response);
        httpResponse->body().appendText(content, length);
        TRI_FreeString(content);
        httpResponse->sealBody();
      }
      break;

      case Endpoint::TransportType::VST: {
        VPackBuffer<uint8_t> buffer;
        VPackBuilder builder(buffer);
        builder.add(VPackValuePair(reinterpret_cast<uint8_t const*>(content), length));
        TRI_FreeString(content);

        // create vpack from file
        response->setContentType(rest::ContentType::VPACK);
        response->setPayload(std::move(buffer));
      }
      break;

      default:
        TRI_FreeString(content);
        throw std::logic_error("unknown transport type");
    }
  }

  // .........................................................................
  // headers
  // .........................................................................

  TRI_GET_GLOBAL_STRING(HeadersKey);

  if (TRI_HasProperty(context, isolate, res, HeadersKey)) {
    v8::Handle<v8::Value> val = res->Get(context, HeadersKey).FromMaybe(v8::Local<v8::Value>());
    v8::Handle<v8::Object> v8Headers = val.As<v8::Object>();

    if (v8Headers->IsObject()) {
      v8::Handle<v8::Array> props = v8Headers->GetPropertyNames(TRI_IGETC).FromMaybe(v8::Local<v8::Array>());

      for (uint32_t i = 0; i < props->Length(); i++) {
        v8::Handle<v8::Value> key = props->Get(context, v8::Integer::New(isolate, i)).FromMaybe(v8::Local<v8::Value>());
        response->setHeader(TRI_ObjectToString(isolate, key),
                            TRI_ObjectToString(isolate, v8Headers->Get(context, key).FromMaybe(v8::Local<v8::Value>())));
      }
    }
  }

  // .........................................................................
  // cookies
  // .........................................................................

  TRI_GET_GLOBAL_STRING(CookiesKey);
  if (TRI_HasProperty(context, isolate, res, CookiesKey)) {
    v8::Handle<v8::Value> val = res->Get(context, CookiesKey).FromMaybe(v8::Local<v8::Value>());
    v8::Handle<v8::Object> v8Cookies = val.As<v8::Object>();

    switch (response->transportType()) {
      case Endpoint::TransportType::HTTP: {
        HttpResponse* httpResponse = dynamic_cast<HttpResponse*>(response);
        if (v8Cookies->IsArray()) {
          v8::Handle<v8::Array> v8Array = v8Cookies.As<v8::Array>();

          for (uint32_t i = 0; i < v8Array->Length(); i++) {
            v8::Handle<v8::Value> v8Cookie = v8Array->Get(context, i).FromMaybe(v8::Local<v8::Value>());
            if (v8Cookie->IsObject()) {
              AddCookie(isolate, v8g, httpResponse, v8Cookie.As<v8::Object>());
            }
          }
        } else if (v8Cookies->IsObject()) {
          // one cookie
          AddCookie(isolate, v8g, httpResponse, v8Cookies);
        }
      } break;

      case Endpoint::TransportType::VST:
        break;

      default:
        throw std::logic_error("unknown transport type");
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes an action
////////////////////////////////////////////////////////////////////////////////

static TRI_action_result_t ExecuteActionVocbase(TRI_vocbase_t* vocbase, v8::Isolate* isolate,
                                                TRI_action_t const* action,
                                                v8::Handle<v8::Function> callback,
                                                GeneralRequest* request,
                                                GeneralResponse* response) {
  v8::HandleScope scope(isolate);
  v8::TryCatch tryCatch(isolate);

  if (response == nullptr) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "invalid response");
  }

  TRI_GET_GLOBALS();

  v8::Handle<v8::Object> req = TRI_RequestCppToV8(isolate, v8g, request, action);

  // create the response object
  v8::Handle<v8::Object> res = v8::Object::New(isolate);

  // register request & response in the context
  v8g->_currentRequest = req;
  v8g->_currentResponse = res;

  // execute the callback
  v8::Handle<v8::Value> args[2] = {req, res};

  // handle C++ exceptions that happen during dynamic script execution
  int errorCode;
  std::string errorMessage;

  try {
    callback->Call(TRI_IGETC, callback, 2, args).FromMaybe(v8::Local<v8::Value>());;
    errorCode = TRI_ERROR_NO_ERROR;
  } catch (arangodb::basics::Exception const& ex) {
    errorCode = ex.code();
    errorMessage = ex.what();
  } catch (std::bad_alloc const&) {
    errorCode = TRI_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    errorCode = TRI_ERROR_INTERNAL;
  }

  // invalidate request / response objects
  v8g->_currentRequest = v8::Undefined(isolate);
  v8g->_currentResponse = v8::Undefined(isolate);

  // convert the result
  TRI_action_result_t result;
  result.isValid = true;

  if (errorCode != TRI_ERROR_NO_ERROR) {
    result.isValid = false;
    result.canceled = false;

    response->setResponseCode(rest::ResponseCode::SERVER_ERROR);

    if (errorMessage.empty()) {
      errorMessage = TRI_errno_string(errorCode);
    }

    VPackBuffer<uint8_t> buffer;
    VPackBuilder b(buffer);
    b.add(VPackValue(errorMessage));
    response->addPayload(std::move(buffer));
  }

  else if (v8g->_canceled) {
    result.isValid = false;
    result.canceled = true;
  }

  else if (tryCatch.HasCaught()) {
    if (tryCatch.CanContinue()) {
      response->setResponseCode(rest::ResponseCode::SERVER_ERROR);

      std::string jsError = TRI_StringifyV8Exception(isolate, &tryCatch);
      LOG_TOPIC("b8286", WARN, arangodb::Logger::V8)
          << "Caught an error while executing an action: " << jsError;
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
      VPackBuilder b;
      b.add(VPackValue(TRI_StringifyV8Exception(isolate, &tryCatch)));
      response->addPayload(b.slice());
#endif
    } else {
      v8g->_canceled = true;
      result.isValid = false;
      result.canceled = true;
    }
  }

  else {
    ResponseV8ToCpp(isolate, v8g, request, res, response);
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief defines a new action
///
/// @FUN{internal.defineAction(@FA{name}, @FA{callback}, @FA{parameter})}
////////////////////////////////////////////////////////////////////////////////

static void JS_DefineAction(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);
  TRI_GET_GLOBALS();

  if (args.Length() != 3) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "defineAction(<name>, <callback>, <parameter>)");
  }

  V8SecurityFeature& v8security = v8g->_server.getFeature<V8SecurityFeature>();

  if (!v8security.isAllowedToDefineHttpAction(isolate)) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_FORBIDDEN, "operation only allowed for internal scripts");
  }

  // extract the action name
  TRI_Utf8ValueNFC utf8name(isolate, args[0]);

  if (*utf8name == nullptr) {
    TRI_V8_THROW_TYPE_ERROR("<name> must be an UTF-8 string");
  }

  std::string name = *utf8name;

  // extract the action callback
  if (!args[1]->IsFunction()) {
    TRI_V8_THROW_TYPE_ERROR("<callback> must be a function");
  }

  v8::Handle<v8::Function> callback = v8::Handle<v8::Function>::Cast(args[1]);

  // extract the options
  v8::Handle<v8::Object> options;

  if (args[2]->IsObject()) {
    options = args[2]->ToObject(TRI_IGETC).FromMaybe(v8::Local<v8::Object>());
  } else {
    options = v8::Object::New(isolate);
  }

  // create an action with the given options
  auto action = std::make_shared<v8_action_t>();
  ParseActionOptions(isolate, v8g, action.get(), options);

  // store an action with the given name
  // note: this may return a previous action for the same name
  std::shared_ptr<TRI_action_t> actionForName = TRI_DefineActionVocBase(name, action);

  v8_action_t* v8ActionForName = dynamic_cast<v8_action_t*>(actionForName.get());

  if (v8ActionForName != nullptr) {
    v8ActionForName->createCallback(isolate, callback);
  } else {
    LOG_TOPIC("43be9", WARN, arangodb::Logger::FIXME)
        << "cannot create callback for V8 action";
  }

  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief eventually executes a function in all contexts
///
/// @FUN{internal.executeGlobalContextFunction(@FA{function-definition})}
////////////////////////////////////////////////////////////////////////////////

static void JS_ExecuteGlobalContextFunction(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "executeGlobalContextFunction(<function-type>)");
  }

  // extract the action name
  v8::String::Utf8Value utf8def(isolate, args[0]);

  if (*utf8def == nullptr) {
    TRI_V8_THROW_TYPE_ERROR("<definition> must be a UTF-8 function definition");
  }

  std::string const def = std::string(*utf8def, utf8def.length());

  // and pass it to the V8 contexts
  if (!V8DealerFeature::DEALER->addGlobalContextMethod(def)) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "invalid action definition");
  }

  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the current request
///
/// @FUN{internal.getCurrentRequest()}
////////////////////////////////////////////////////////////////////////////////

static void JS_GetCurrentRequest(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);
  TRI_GET_GLOBALS();

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("getCurrentRequest()");
  }

  TRI_V8_RETURN(v8g->_currentRequest);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the raw body of the current request
///
/// @FUN{internal.rawRequestBody()}
////////////////////////////////////////////////////////////////////////////////

static void JS_RawRequestBody(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);
  auto context = TRI_IGETC;

  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("rawRequestBody(req)");
  }

  v8::Handle<v8::Value> current = args[0];
  if (current->IsObject()) {
    v8::Handle<v8::Object> obj = v8::Handle<v8::Object>::Cast(current);
    v8::Handle<v8::Value> property =
      obj->Get(context, TRI_V8_ASCII_STRING(isolate, "internals")).FromMaybe(v8::Local<v8::Value>());
    if (property->IsExternal()) {
      v8::Handle<v8::External> e = v8::Handle<v8::External>::Cast(property);

      GeneralRequest* request = static_cast<GeneralRequest*>(e->Value());

      switch (request->transportType()) {
        case Endpoint::TransportType::HTTP: {
          auto httpRequest = static_cast<arangodb::HttpRequest*>(e->Value());
          if (httpRequest != nullptr) {
            V8Buffer* buffer;
            if (rest::ContentType::VPACK == request->contentType()) {
              VPackSlice slice = request->payload();
              std::string bodyStr = slice.toJson();
              buffer = V8Buffer::New(isolate, bodyStr.c_str(), bodyStr.size());
            } else {
              auto raw = httpRequest->rawPayload();
              buffer = V8Buffer::New(isolate, raw.data(), raw.size());
            }

            v8::Local<v8::Object> bufferObject =
              v8::Local<v8::Object>::New(isolate, buffer->_handle);
            TRI_V8_RETURN(bufferObject);
          }
        } break;

        case Endpoint::TransportType::VST: {
          if (request != nullptr) {
            auto raw = request->rawPayload();
            V8Buffer* buffer = V8Buffer::New(isolate, raw.data(), raw.size());
            v8::Local<v8::Object> bufferObject =
              v8::Local<v8::Object>::New(isolate, buffer->_handle);
            TRI_V8_RETURN(bufferObject);
          }
        } break;
      }
    }
  }

  // VPackSlice slice(data);
  // v8::Handle<v8::Value> result = TRI_VPackToV8(isolate, slice);
  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the raw body of the current request
///
/// @FUN{internal.rawRequestBody()}
////////////////////////////////////////////////////////////////////////////////

static void JS_RequestParts(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);
  auto context = TRI_IGETC;

  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("requestParts(req)");
  }

  v8::Handle<v8::Value> current = args[0];
  if (current->IsObject()) {
    v8::Handle<v8::Object> obj = v8::Handle<v8::Object>::Cast(current);
    v8::Handle<v8::Value> property =
      obj->Get(context, TRI_V8_ASCII_STRING(isolate, "internals")).FromMaybe(v8::Local<v8::Value>());
    if (property->IsExternal()) {
      v8::Handle<v8::External> e = v8::Handle<v8::External>::Cast(property);
      auto request = static_cast<arangodb::HttpRequest*>(e->Value());

      VPackStringRef bodyStr = request->rawPayload();
      char const* beg = bodyStr.data();
      char const* end = beg + bodyStr.size();

      while (beg < end && (*beg == '\r' || *beg == '\n' || *beg == ' ')) {
        ++beg;
      }

      // find delimiter
      char const* ptr = beg;
      while (ptr < end && *ptr == '-') {
        ++ptr;
      }

      while (ptr < end && *ptr != '\r' && *ptr != '\n') {
        ++ptr;
      }
      if (ptr == beg) {
        // oops
        TRI_V8_THROW_EXCEPTION_PARAMETER("request is no multipart request");
      }

      std::string const delimiter(beg, ptr - beg);
      if (ptr < end && *ptr == '\r') {
        ++ptr;
      }
      if (ptr < end && *ptr == '\n') {
        ++ptr;
      }

      std::vector<std::pair<char const*, size_t>> parts;

      while (ptr < end) {
        char const* p = TRI_IsContainedMemory(ptr, end - ptr, delimiter.c_str(),
                                              delimiter.size());
        if (p == nullptr || p + delimiter.size() + 2 >= end || p - 2 <= ptr) {
          TRI_V8_THROW_EXCEPTION_PARAMETER("bad request data");
        }

        char const* q = p;
        if (*(q - 1) == '\n') {
          --q;
        }
        if (*(q - 1) == '\r') {
          --q;
        }

        parts.push_back(std::make_pair(ptr, q - ptr));
        ptr = p + delimiter.size();
        if (*ptr == '-' && *(ptr + 1) == '-') {
          // eom
          break;
        }
        if (*ptr == '\r') {
          ++ptr;
        }
        if (ptr < end && *ptr == '\n') {
          ++ptr;
        }
      }

      v8::Handle<v8::Array> result = v8::Array::New(isolate);

      uint32_t j = 0;
      for (auto& part : parts) {
        v8::Handle<v8::Object> headersObject = v8::Object::New(isolate);

        auto ptr = part.first;
        auto end = part.first + part.second;
        char const* data = nullptr;

        while (ptr < end) {
          while (ptr < end && *ptr == ' ') {
            ++ptr;
          }
          if (ptr < end && (*ptr == '\r' || *ptr == '\n')) {
            // end of headers
            if (*ptr == '\r') {
              ++ptr;
            }
            if (ptr < end && *ptr == '\n') {
              ++ptr;
            }
            data = ptr;
            break;
          }

          // header line
          char const* eol = TRI_IsContainedMemory(ptr, end - ptr, "\r\n", 2);
          if (eol == nullptr) {
            eol = TRI_IsContainedMemory(ptr, end - ptr, "\n", 1);
          }
          if (eol == nullptr) {
            TRI_V8_THROW_EXCEPTION_PARAMETER("bad request data");
          }
          char const* colon = TRI_IsContainedMemory(ptr, end - ptr, ":", 1);
          if (colon == nullptr) {
            TRI_V8_THROW_EXCEPTION_PARAMETER("bad request data");
          }
          char const* p = colon;
          while (p > ptr && *(p - 1) == ' ') {
            --p;
          }
          ++colon;
          while (colon < eol && *colon == ' ') {
            ++colon;
          }
          char const* q = eol;
          while (q > ptr && *(q - 1) == ' ') {
            --q;
          }

          headersObject->Set(context,
                             TRI_V8_PAIR_STRING(isolate, ptr, (int)(p - ptr)),
                             TRI_V8_PAIR_STRING(isolate, colon, (int)(eol - colon))).FromMaybe(false);

          ptr = eol;
          if (*ptr == '\r') {
            ++ptr;
          }
          if (ptr < end && *ptr == '\n') {
            ++ptr;
          }
        }

        if (data == nullptr) {
          TRI_V8_THROW_EXCEPTION_PARAMETER("bad request data");
        }

        v8::Handle<v8::Object> partObject = v8::Object::New(isolate);
        partObject->Set(context,
                        TRI_V8_ASCII_STRING(isolate, "headers"), headersObject).FromMaybe(false);

        // cppcheck-suppress nullPointerArithmetic ; cannot get here, if data is nullptr
        V8Buffer* buffer = V8Buffer::New(isolate, data, end - data);
        auto localHandle = v8::Local<v8::Object>::New(isolate, buffer->_handle);

        partObject->Set(context, TRI_V8_ASCII_STRING(isolate, "data"), localHandle).FromMaybe(false);

        result->Set(context, j++, partObject).FromMaybe(false);
      }

      TRI_V8_RETURN(result);
    }
  }

  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the current response
///
/// @FUN{internal.getCurrentRequest()}
////////////////////////////////////////////////////////////////////////////////

static void JS_GetCurrentResponse(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);
  TRI_GET_GLOBALS();

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("getCurrentResponse()");
  }

  TRI_V8_RETURN(v8g->_currentResponse);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stores the V8 actions function inside the global variable
////////////////////////////////////////////////////////////////////////////////

void TRI_InitV8Actions(v8::Isolate* isolate) {
  v8::HandleScope scope(isolate);

  // .............................................................................
  // create the global functions
  // .............................................................................

  TRI_AddGlobalFunctionVocbase(
      isolate, TRI_V8_ASCII_STRING(isolate, "SYS_DEFINE_ACTION"), JS_DefineAction);
  TRI_AddGlobalFunctionVocbase(
      isolate,
      TRI_V8_ASCII_STRING(isolate, "SYS_EXECUTE_GLOBAL_CONTEXT_FUNCTION"),
      JS_ExecuteGlobalContextFunction, true);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate,
                                                   "SYS_GET_CURRENT_REQUEST"),
                               JS_GetCurrentRequest);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate,
                                                   "SYS_GET_CURRENT_RESPONSE"),
                               JS_GetCurrentResponse);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate,
                                                   "SYS_RAW_REQUEST_BODY"),
                               JS_RawRequestBody, true);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate,
                                                   "SYS_REQUEST_PARTS"),
                               JS_RequestParts, true);
}

////////////////////////////////////////////////////////////////////////////////
/// Below Debugging Functions. Only compiled in maintainer mode.
////////////////////////////////////////////////////////////////////////////////

#ifdef ARANGODB_ENABLE_FAILURE_TESTS
static int clusterSendToAllServers(v8::Isolate* isolate, std::string const& dbname,
                                   std::string const& path,  // Note: Has to be properly encoded!
                                   arangodb::rest::RequestType const& method,
                                   std::string const& body) {
  TRI_GET_GLOBALS();
  network::ConnectionPool* pool = v8g->_server.getFeature<NetworkFeature>().pool();
  if (!pool || !pool->config().clusterInfo) {
    LOG_TOPIC("98fc7", ERR, Logger::COMMUNICATION) << "Network pool unavailable.";
    return TRI_ERROR_SHUTTING_DOWN;
  }
  ClusterInfo& ci = *pool->config().clusterInfo;
  std::vector<ServerID> DBServers = ci.getCurrentDBServers();

  network::Headers headers;
  fuerte::RestVerb verb = network::arangoRestVerbToFuerte(method);

  network::RequestOptions reqOpts;

  reqOpts.database = dbname;
  reqOpts.timeout = network::Timeout(3600);
  reqOpts.contentType = StaticStrings::MimeTypeJsonNoEncoding;

  std::vector<futures::Future<network::Response>> futures;
  futures.reserve(DBServers.size());

  // Have to propagate to DB Servers
  for (auto const& sid : DBServers) {
    VPackBuffer<uint8_t> buffer(body.size());
    buffer.append(body);
    auto f = network::sendRequest(pool, "server:" + sid, verb, path,
                                  std::move(buffer), reqOpts, headers);
    futures.emplace_back(std::move(f));
  }

  for (auto& f : futures) {
    network::Response const& res = f.get();  // throws exceptions upwards
    int commError = network::fuerteToArangoErrorCode(res);
    if (commError != TRI_ERROR_NO_ERROR) {
      return commError;
    }
  }
  return TRI_ERROR_NO_ERROR;
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief intentionally causes a segfault
///
/// @FUN{internal.debugSegfault(@FA{message})}
///
/// intentionally cause a segmentation violation
////////////////////////////////////////////////////////////////////////////////

#ifdef ARANGODB_ENABLE_FAILURE_TESTS
static void JS_DebugTerminate(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  // extract arguments
  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("debugTerminate(<message>)");
  }

  std::string const message = TRI_ObjectToString(isolate, args[0]);

  TRI_TerminateDebugging(message.c_str());

  // we may get here if we are in non-maintainer mode

  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief sets a failure point
///
/// @FUN{internal.debugSetFailAt(@FA{point})}
///
/// Set a point for an intentional system failure
////////////////////////////////////////////////////////////////////////////////

#ifdef ARANGODB_ENABLE_FAILURE_TESTS
static void JS_DebugSetFailAt(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  TRI_GET_GLOBALS();

  if (v8g->_vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION_MEMORY();
  }
  std::string dbname(v8g->_vocbase->name());

  // extract arguments
  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("debugSetFailAt(<point>)");
  }

  std::string const point = TRI_ObjectToString(isolate, args[0]);

  TRI_AddFailurePointDebugging(point.c_str());

  if (ServerState::instance()->isCoordinator()) {
    int res = clusterSendToAllServers(isolate, dbname,
                                      "_admin/debug/failat/" + StringUtils::urlEncode(point),
                                      arangodb::rest::RequestType::PUT, "");
    if (res != TRI_ERROR_NO_ERROR) {
      TRI_V8_THROW_EXCEPTION(res);
    }
  }

  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief checks a failure point
///
/// @FUN{internal.debugShouldFailAt(@FA{point})}
///
/// Checks if a specific intentional failure point is set
////////////////////////////////////////////////////////////////////////////////

#ifdef ARANGODB_ENABLE_FAILURE_TESTS
static void JS_DebugShouldFailAt(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  TRI_GET_GLOBALS();

  if (v8g->_vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION_MEMORY();
  }

  // extract arguments
  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("debugShouldFailAt(<point>)");
  }

  std::string const point = TRI_ObjectToString(isolate, args[0]);

  TRI_V8_RETURN_BOOL(TRI_ShouldFailDebugging(point.c_str()));

  TRI_V8_TRY_CATCH_END
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief removes a failure point
///
/// @FUN{internal.debugRemoveFailAt(@FA{point})}
///
/// Remove a point for an intentional system failure
////////////////////////////////////////////////////////////////////////////////

#ifdef ARANGODB_ENABLE_FAILURE_TESTS
static void JS_DebugRemoveFailAt(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  TRI_GET_GLOBALS();

  if (v8g->_vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION_MEMORY();
  }
  std::string dbname(v8g->_vocbase->name());

  // extract arguments
  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("debugRemoveFailAt(<point>)");
  }

  std::string const point = TRI_ObjectToString(isolate, args[0]);

  TRI_RemoveFailurePointDebugging(point.c_str());

  if (ServerState::instance()->isCoordinator()) {
    int res =
        clusterSendToAllServers(isolate, dbname,
                                "_admin/debug/failat/" + StringUtils::urlEncode(point),
                                arangodb::rest::RequestType::DELETE_REQ, "");
    if (res != TRI_ERROR_NO_ERROR) {
      TRI_V8_THROW_EXCEPTION(res);
    }
  }

  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief clears all failure points
///
/// @FUN{internal.debugClearFailAt()}
///
/// Remove all points for intentional system failures
////////////////////////////////////////////////////////////////////////////////

static void JS_DebugClearFailAt(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  // extract arguments
  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("debugClearFailAt()");
  }

// if failure testing is not enabled, this is a no-op
#ifdef ARANGODB_ENABLE_FAILURE_TESTS
  TRI_ClearFailurePointsDebugging();

  if (ServerState::instance()->isCoordinator()) {
    TRI_GET_GLOBALS();

    if (v8g->_vocbase == nullptr) {
      TRI_V8_THROW_EXCEPTION_MEMORY();
    }
    std::string dbname(v8g->_vocbase->name());

    int res =
        clusterSendToAllServers(isolate, dbname, "_admin/debug/failat",
                                arangodb::rest::RequestType::DELETE_REQ, "");
    if (res != TRI_ERROR_NO_ERROR) {
      TRI_V8_THROW_EXCEPTION(res);
    }
  }

#endif

  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END
}

static void JS_IsFoxxApiDisabled(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate)
  v8::HandleScope scope(isolate);

  TRI_GET_GLOBALS();
  ServerSecurityFeature& security = v8g->_server.getFeature<ServerSecurityFeature>();
  TRI_V8_RETURN_BOOL(security.isFoxxApiDisabled());

  TRI_V8_TRY_CATCH_END
}

static void JS_IsFoxxStoreDisabled(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate)
  v8::HandleScope scope(isolate);

  TRI_GET_GLOBALS();
  ServerSecurityFeature& security = v8g->_server.getFeature<ServerSecurityFeature>();
  TRI_V8_RETURN_BOOL(security.isFoxxStoreDisabled());

  TRI_V8_TRY_CATCH_END
}

static void JS_RunInRestrictedContext(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate)
  v8::HandleScope scope(isolate);

  if (args.Length() != 1 || !args[0]->IsFunction()) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "runInRestrictedContext(<function>)");
  }

  v8::Handle<v8::Function> action = v8::Local<v8::Function>::Cast(args[0]);
  if (action.IsEmpty()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "cannot cannot function instance for runInRestrictedContext");
  }

  TRI_GET_GLOBALS();

  {
    // take a copy of the previous security context
    auto oldContext = v8g->_securityContext;

    // patch security context
    v8g->_securityContext = JavaScriptSecurityContext::createRestrictedContext();

    // make sure the old context will be restored
    auto guard = scopeGuard([&oldContext, &v8g]() {
      v8g->_securityContext = oldContext;
    });

    v8::Handle<v8::Object> current = isolate->GetCurrentContext()->Global();
    v8::Handle<v8::Value> callArgs[] = {v8::Null(isolate)};
    v8::Handle<v8::Value> rv = action->Call(TRI_IGETC, current, 0, callArgs).FromMaybe(v8::Local<v8::Value>());
    TRI_V8_RETURN(rv);
  }

  TRI_V8_TRY_CATCH_END
}

//////////////////////////////////////////////////////////////////////////////
/// @brief creates a hotbackup
//////////////////////////////////////////////////////////////////////////////

static void JS_CreateHotbackup(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);

  if (args.Length() != 1 || !args[0]->IsObject()) {
    TRI_V8_THROW_EXCEPTION_USAGE("createHotbackup(obj)");
  }
  VPackBuilder obj;
  int res = TRI_V8ToVPack(isolate, obj, args[0], false, true);
  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION_USAGE("createHotbackup(obj): could not convert body to object");
  }

  VPackBuilder result;
#if USE_ENTERPRISE
  TRI_GET_GLOBALS();
  HotBackup h(v8g->_server);
  auto r = h.execute("create", obj.slice(), result);
  if (r.fail()) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(r.errorNumber(), r.errorMessage());
  }
#else
  result.add(obj.slice());
#endif

  TRI_V8_RETURN(TRI_VPackToV8(isolate, result.slice()));
  TRI_V8_TRY_CATCH_END
}

void TRI_InitV8ServerUtils(v8::Isolate* isolate) {
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate, "SYS_IS_FOXX_API_DISABLED"), JS_IsFoxxApiDisabled, true);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate, "SYS_IS_FOXX_STORE_DISABLED"), JS_IsFoxxStoreDisabled, true);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate, "SYS_RUN_IN_RESTRICTED_CONTEXT"), JS_RunInRestrictedContext, true);
  
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate,
                                                   "SYS_CREATE_HOTBACKUP"),
                               JS_CreateHotbackup);

  // debugging functions
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate,
                                                   "SYS_DEBUG_CLEAR_FAILAT"),
                               JS_DebugClearFailAt);

#ifdef ARANGODB_ENABLE_FAILURE_TESTS
  TRI_AddGlobalFunctionVocbase(
      isolate, TRI_V8_ASCII_STRING(isolate, "SYS_DEBUG_TERMINATE"), JS_DebugTerminate);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate,
                                                   "SYS_DEBUG_SET_FAILAT"),
                               JS_DebugSetFailAt);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate,
                                                   "SYS_DEBUG_REMOVE_FAILAT"),
                               JS_DebugRemoveFailAt);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate,
                                                   "SYS_DEBUG_SHOULD_FAILAT"),
                               JS_DebugShouldFailAt);
#endif

  // poll interval for Foxx queues
  TRI_GET_GLOBALS();
  FoxxQueuesFeature& foxxQueuesFeature = v8g->_server.getFeature<FoxxQueuesFeature>();

  isolate->GetCurrentContext()
      ->Global()
      ->DefineOwnProperty(
          TRI_IGETC, TRI_V8_ASCII_STRING(isolate, "FOXX_QUEUES_POLL_INTERVAL"),
          v8::Number::New(isolate, foxxQueuesFeature.pollInterval()), v8::ReadOnly)
      .FromMaybe(false);  // ignore result

  isolate->GetCurrentContext()
      ->Global()
      ->DefineOwnProperty(
          TRI_IGETC,
          TRI_V8_ASCII_STRING(isolate, "FOXX_STARTUP_WAIT_FOR_SELF_HEAL"),
          v8::Boolean::New(isolate, foxxQueuesFeature.startupWaitForSelfHeal()), v8::ReadOnly)
      .FromMaybe(false);  // ignore result
}
