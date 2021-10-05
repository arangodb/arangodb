////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include <tuple>

#include "v8-globals.h"

#include "Basics/debugging.h"
#include "Basics/StaticStrings.h"
#include "Basics/system-functions.h"

TRI_v8_global_t::TRI_v8_global_t(arangodb::application_features::ApplicationServer& server,
                                 v8::Isolate* isolate, size_t id)
    : AgencyTempl(),
      AgentTempl(),
      ClusterInfoTempl(),
      ServerStateTempl(),
      ClusterCommTempl(),
      ArangoErrorTempl(),
      VocbaseColTempl(),
      VocbaseViewTempl(),
      VocbaseTempl(),
      EnvTempl(),
      UsersTempl(),
      GeneralGraphModuleTempl(),
      GeneralGraphTempl(),
#ifdef USE_ENTERPRISE
      SmartGraphTempl(),
#endif
      BufferTempl(),
      StreamQueryCursorTempl(),

      BufferConstant(),
      DeleteConstant(),
      GetConstant(),
      HeadConstant(),
      OptionsConstant(),
      PatchConstant(),
      PostConstant(),
      PutConstant(),

      AddressKey(),
      AllowUseDatabaseKey(),
      AuthorizedKey(),
      BodyFromFileKey(),
      BodyKey(),
      ClientKey(),
      CodeKey(),
      ContentTypeKey(),
      CoordTransactionIDKey(),
      DatabaseKey(),
      DomainKey(),
      EndpointKey(),
      ErrorKey(),
      ErrorMessageKey(),
      ErrorNumKey(),
      HeadersKey(),
      HttpOnlyKey(),
      IdKey(),
      IsAdminUser(),
      InitTimeoutKey(),
      IsRestoreKey(),
      IsSystemKey(),
      KeepNullKey(),
      KeyOptionsKey(),
      LengthKey(),
      LifeTimeKey(),
      MergeObjectsKey(),
      NameKey(),
      OperationIDKey(),
      OverwriteKey(),
      OverwriteModeKey(),
      SkipDocumentValidationKey(),
      ParametersKey(),
      PathKey(),
      PrefixKey(),
      PortKey(),
      PortTypeKey(),
      ProtocolKey(),
      RawSuffixKey(),
      RequestBodyKey(),
      RawRequestBodyKey(),
      RequestTypeKey(),
      ResponseCodeKey(),
      ReturnNewKey(),
      ReturnOldKey(),
      SecureKey(),
      ServerKey(),
      ShardIDKey(),
      SilentKey(),
      SingleRequestKey(),
      StatusKey(),
      SuffixKey(),
      TimeoutKey(),
      ToJsonKey(),
      TransformationsKey(),
      UrlKey(),
      UserKey(),
      ValueKey(),
      VersionKeyHidden(),
      WaitForSyncKey(),

      _DbCacheKey(),
      _DbNameKey(),
      _IdKey(),
      _KeyKey(),
      _RevKey(),
      _FromKey(),
      _ToKey(),

      _currentRequest(),
      _currentResponse(),
      _transactionContext(nullptr),
      _expressionContext(nullptr),
      _vocbase(nullptr),
      _activeExternals(0),
      _canceled(false),
      _securityContext(arangodb::JavaScriptSecurityContext::createRestrictedContext()),
      _inForcedCollect(false),
      _id(id),
      _lastMaxTime(TRI_microtime()),
      _countOfTimes(0),
      _heapMax(0),
      _heapLow(0),
      _server(server) {
  v8::HandleScope scope(isolate);

  BufferConstant.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "Buffer"));
  DeleteConstant.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "DELETE"));
  GetConstant.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "GET"));
  HeadConstant.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "HEAD"));
  OptionsConstant.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "OPTIONS"));
  PatchConstant.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "PATCH"));
  PostConstant.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "POST"));
  PutConstant.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "PUT"));

  AddressKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "address"));
  AllowUseDatabaseKey.Reset(isolate,
                            TRI_V8_ASCII_STRING(isolate, "allowUseDatabase"));
  AuthorizedKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "authorized"));
  BodyFromFileKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "bodyFromFile"));
  BodyKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "body"));
  ClientKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "client"));
  CodeKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "code"));
  ContentTypeKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "contentType"));
  CookiesKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "cookies"));
  CoordTransactionIDKey.Reset(isolate,
                              TRI_V8_ASCII_STRING(isolate,
                                                  "coordTransactionID"));
  DatabaseKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "database"));
  DomainKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "domain"));
  EndpointKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "endpoint"));
  ErrorKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "error"));
  ErrorMessageKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "errorMessage"));
  ErrorNumKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "errorNum"));
  HeadersKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "headers"));
  HttpOnlyKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "httpOnly"));
  IdKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "id"));
  IsAdminUser.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "isAdminUser"));
  InitTimeoutKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "initTimeout"));
  IsRestoreKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "isRestore"));
  IsSynchronousReplicationKey.Reset(
      isolate, TRI_V8_ASCII_STRING(isolate, "isSynchronousReplication"));
  IsSystemKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "isSystem"));
  KeepNullKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "keepNull"));
  KeyOptionsKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "keyOptions"));
  LengthKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "length"));
  LifeTimeKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "lifeTime"));
  MergeObjectsKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "mergeObjects"));
  NameKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "name"));
  OperationIDKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "operationID"));
  OverwriteKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "overwrite"));
  OverwriteModeKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "overwriteMode"));
  SkipDocumentValidationKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "skipDocumentValidation"));
  ParametersKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "parameters"));
  PathKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "path"));
  PrefixKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "prefix"));
  PortKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "port"));
  PortTypeKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "portType"));
  ProtocolKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "protocol"));
  RawSuffixKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "rawSuffix"));
  RequestBodyKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "requestBody"));
  RawRequestBodyKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "rawRequestBody"));
  RequestTypeKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "requestType"));
  ResponseCodeKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "responseCode"));
  ReturnNewKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "returnNew"));
  ReturnOldKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "returnOld"));
  SecureKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "secure"));
  ServerKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "server"));
  ShardIDKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "shardID"));
  SilentKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "silent"));
  SingleRequestKey.Reset(isolate,
                         TRI_V8_ASCII_STRING(isolate, "singleRequest"));
  StatusKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "status"));
  SuffixKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "suffix"));
  TimeoutKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "timeout"));
  ToJsonKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "toJSON"));
  TransformationsKey.Reset(isolate,
                           TRI_V8_ASCII_STRING(isolate, "transformations"));
  UrlKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "url"));
  UserKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "user"));
  ValueKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "value"));
  VersionKeyHidden.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "*version"));
  WaitForSyncKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "waitForSync"));
  CompactKey.Reset(isolate, TRI_V8_ASCII_STD_STRING(isolate, arangodb::StaticStrings::Compact));

  _DbCacheKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "__dbcache__"));
  _DbNameKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "_dbName"));
  _IdKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "_id"));
  _KeyKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "_key"));
  _RevKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "_rev"));
  _FromKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "_from"));
  _ToKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "_to"));
}

TRI_v8_global_t::SharedPtrPersistent::SharedPtrPersistent( // constructor
    v8::Isolate& isolateRef, // isolate
    std::shared_ptr<void> const& value // value
): _isolate(isolateRef), _value(value) {
  auto* isolate = &isolateRef;
  TRI_GET_GLOBALS();

  _persistent.Reset(isolate, v8::External::New(isolate, value.get()));
  _persistent.SetWeak( // set weak reference
    this, // parameter
    [](v8::WeakCallbackInfo<SharedPtrPersistent> const& data)->void { // callback
      auto isolate = data.GetIsolate();
      auto* persistent = data.GetParameter();
      TRI_GET_GLOBALS();

      auto* key = persistent->_value.get(); // same key as used in emplace(...)
      auto count = v8g->JSSharedPtrs.erase(key);
      TRI_ASSERT(count); // zero indicates that v8g was probably deallocated before calling the v8::WeakCallbackInfo::Callback
    },
    v8::WeakCallbackType::kFinalizer // callback type
  );
  v8g->increaseActiveExternals();
}

TRI_v8_global_t::SharedPtrPersistent::~SharedPtrPersistent() {
  auto* isolate = &_isolate;
  TRI_GET_GLOBALS();
  v8g->decreaseActiveExternals();
  _persistent.Reset(); // dispose and clear the persistent handle (SIGSEGV here may indicate that v8::Isolate was already deallocated)
}

/*static*/ std::pair<TRI_v8_global_t::SharedPtrPersistent&, bool> TRI_v8_global_t::SharedPtrPersistent::emplace( // emplace a persistent shared pointer
    v8::Isolate& isolateRef, // isolate
    std::shared_ptr<void> const& value // persistent pointer
) {
  auto* isolate = &isolateRef;
  TRI_GET_GLOBALS();

  auto entry = v8g->JSSharedPtrs.try_emplace( // ensure shared_ptr is not deallocated
    value.get(), // key
    isolateRef, value // value
  );

  return std::pair<SharedPtrPersistent&, bool>( // result
    entry.first->second, entry.second // args
  );
}

TRI_v8_global_t::~TRI_v8_global_t() = default;

/// @brief creates a global context
TRI_v8_global_t* TRI_CreateV8Globals(arangodb::application_features::ApplicationServer& server,
                                     v8::Isolate* isolate, size_t id) {
  TRI_GET_GLOBALS();

  TRI_ASSERT(v8g == nullptr);
  v8g = new TRI_v8_global_t(server, isolate, id);
  isolate->SetData(arangodb::V8PlatformFeature::V8_DATA_SLOT, v8g);

  return v8g;
}

/// @brief returns a global context
TRI_v8_global_t* TRI_GetV8Globals(v8::Isolate* isolate) {
  TRI_GET_GLOBALS();

  TRI_ASSERT(v8g != nullptr);
  return v8g;
}

/// @brief adds a method to an object
bool TRI_AddMethodVocbase(v8::Isolate* isolate, v8::Handle<v8::ObjectTemplate> tpl,
                          v8::Handle<v8::String> name,
                          void (*func)(v8::FunctionCallbackInfo<v8::Value> const&),
                          bool isHidden) {
  if (isHidden) {
    // hidden method
    tpl->Set(name, v8::FunctionTemplate::New(isolate, func), v8::DontEnum);
  } else {
    // normal method
    tpl->Set(name, v8::FunctionTemplate::New(isolate, func));
  }
  return true;
}

/// @brief adds a global function to the given context
bool TRI_AddGlobalFunctionVocbase(v8::Isolate* isolate, v8::Handle<v8::String> name,
                                  void (*func)(v8::FunctionCallbackInfo<v8::Value> const&),
                                  bool isHidden) {
  // all global functions are read-only
  if (isHidden) {
    return isolate->GetCurrentContext()
        ->Global()
        ->DefineOwnProperty(TRI_IGETC, name,
                            v8::FunctionTemplate::New(isolate, func)->GetFunction(TRI_IGETC).FromMaybe(v8::Local<v8::Function>()),
                            static_cast<v8::PropertyAttribute>(v8::ReadOnly | v8::DontEnum))
        .FromMaybe(false);
  } else {
    return isolate->GetCurrentContext()
        ->Global()
        ->DefineOwnProperty(TRI_IGETC, name,
                            v8::FunctionTemplate::New(isolate, func)->GetFunction(TRI_IGETC).FromMaybe(v8::Local<v8::Function>()),
                            v8::ReadOnly)
        .FromMaybe(false);
  }
}

/// @brief adds a global function to the given context
bool TRI_AddGlobalFunctionVocbase(v8::Isolate* isolate, v8::Handle<v8::String> name,
                                  v8::Handle<v8::Function> func, bool isHidden) {
  // all global functions are read-only
  if (isHidden) {
    return isolate->GetCurrentContext()
        ->Global()
        ->DefineOwnProperty(TRI_IGETC, name, func,
                            static_cast<v8::PropertyAttribute>(v8::ReadOnly | v8::DontEnum))
        .FromMaybe(false);
  } else {
    return isolate->GetCurrentContext()
        ->Global()
        ->DefineOwnProperty(TRI_IGETC, name, func, v8::ReadOnly)
        .FromMaybe(false);
  }
}

/// @brief adds a global read-only variable to the given context
bool TRI_AddGlobalVariableVocbase(v8::Isolate* isolate, v8::Handle<v8::String> name,
                                  v8::Handle<v8::Value> value) {
  // all global variables are read-only
  return isolate->GetCurrentContext()
      ->Global()
      ->DefineOwnProperty(TRI_IGETC, name, value, v8::ReadOnly)
      .FromMaybe(false);
}

template <>
v8::Local<v8::String> v8Utf8StringFactoryT<std::string_view>(v8::Isolate* isolate,
                                                             std::string_view const& arg) {
  return v8Utf8StringFactory(isolate, arg.data(), arg.size());
}

template <>
v8::Local<v8::String> v8Utf8StringFactoryT<std::string>(v8::Isolate* isolate,
                                                        std::string const& arg) {
  return v8Utf8StringFactory(isolate, arg.data(), arg.size());
}

template <>
v8::Local<v8::String> v8Utf8StringFactoryT<char const*>(v8::Isolate* isolate,
                                                        char const* const& arg) {
  return v8Utf8StringFactory(isolate, arg, strlen(arg));
}

template <>
v8::Local<v8::String> v8Utf8StringFactoryT<arangodb::basics::StringBuffer>(
    v8::Isolate* isolate, arangodb::basics::StringBuffer const& arg) {
  return v8Utf8StringFactory(isolate, arg.data(), arg.size());
}
