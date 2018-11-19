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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "v8-globals.h"

TRI_v8_global_t::TRI_v8_global_t(v8::Isolate* isolate)
  :
      AgencyTempl(),
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
      ParametersKey(),
      PathKey(),
      PrefixKey(),
      PortKey(),
      PortTypeKey(),
      ProtocolKey(),
      RawSuffixKey(),
      RequestBodyKey(),
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
      _queryRegistry(nullptr),
      _query(nullptr),
      _vocbase(nullptr),
      _activeExternals(0),
      _canceled(false),
      _allowUseDatabase(true) {
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
  AllowUseDatabaseKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "allowUseDatabase"));
  AuthorizedKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "authorized"));
  BodyFromFileKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "bodyFromFile"));
  BodyKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "body"));
  ClientKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "client"));
  CodeKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "code"));
  ContentTypeKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "contentType"));
  CookiesKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "cookies"));
  CoordTransactionIDKey.Reset(isolate,
                              TRI_V8_ASCII_STRING(isolate, "coordTransactionID"));
  DatabaseKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "database"));
  DomainKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "domain"));
  EndpointKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "endpoint"));
  ErrorKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "error"));
  ErrorMessageKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "errorMessage"));
  ErrorNumKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "errorNum"));
  HeadersKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "headers"));
  HttpOnlyKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "httpOnly"));
  IdKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "id"));
  InitTimeoutKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "initTimeout"));
  IsRestoreKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "isRestore"));
  IsSynchronousReplicationKey.Reset(isolate,
      TRI_V8_ASCII_STRING(isolate, "isSynchronousReplication"));
  IsSystemKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "isSystem"));
  KeepNullKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "keepNull"));
  KeyOptionsKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "keyOptions"));
  LengthKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "length"));
  LifeTimeKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "lifeTime"));
  MergeObjectsKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "mergeObjects"));
  NameKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "name"));
  OperationIDKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "operationID"));
  OverwriteKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "overwrite"));
  ParametersKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "parameters"));
  PathKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "path"));
  PrefixKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "prefix"));
  PortKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "port"));
  PortTypeKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "portType"));
  ProtocolKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "protocol"));
  RawSuffixKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "rawSuffix"));
  RequestBodyKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "requestBody"));
  RequestTypeKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "requestType"));
  ResponseCodeKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "responseCode"));
  ReturnNewKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "returnNew"));
  ReturnOldKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "returnOld"));
  SecureKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "secure"));
  ServerKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "server"));
  ShardIDKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "shardID"));
  SilentKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "silent"));
  SingleRequestKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "singleRequest"));
  StatusKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "status"));
  SuffixKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "suffix"));
  TimeoutKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "timeout"));
  ToJsonKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "toJSON"));
  TransformationsKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "transformations"));
  UrlKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "url"));
  UserKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "user"));
  ValueKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "value"));
  VersionKeyHidden.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "*version"));
  WaitForSyncKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "waitForSync"));

  _DbCacheKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "__dbcache__"));
  _DbNameKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "_dbName"));
  _IdKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "_id"));
  _KeyKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "_key"));
  _RevKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "_rev"));
  _FromKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "_from"));
  _ToKey.Reset(isolate, TRI_V8_ASCII_STRING(isolate, "_to"));
}

TRI_v8_global_t::DataSourcePersistent::DataSourcePersistent(
    v8::Isolate* isolate,
    std::shared_ptr<arangodb::LogicalDataSource> const& datasource,
    std::function<void()>&& cleanupCallback
): _cleanupCallback(std::move(cleanupCallback)),
   _datasource(datasource),
   _isolate(isolate) {
  TRI_GET_GLOBALS();
  _persistent.Reset(isolate, v8::External::New(isolate, datasource.get()));
  _persistent.SetWeak(
    this,
    [](v8::WeakCallbackInfo<DataSourcePersistent> const& data)->void {
      auto isolate = data.GetIsolate();
      auto* persistent = data.GetParameter();

      persistent->_cleanupCallback();

      TRI_GET_GLOBALS();isolate = nullptr;
      auto* key = persistent->_datasource.get(); // same key as was used for v8g->JSDatasources.emplace(...)
      auto count = v8g->JSDatasources.erase(key);
      TRI_ASSERT(count); // zero indicates that v8g was probably deallocated before calling the v8::WeakCallbackInfo::Callback
    },
    v8::WeakCallbackType::kFinalizer
  );
  v8g->increaseActiveExternals();
}

TRI_v8_global_t::DataSourcePersistent::~DataSourcePersistent() {
  auto* isolate = _isolate;
  TRI_GET_GLOBALS();
  v8g->decreaseActiveExternals();
  _persistent.Reset(); // dispose and clear the persistent handle (SIGSEGV here may indicate that v8::Isolate was already deallocated)
}

TRI_v8_global_t::~TRI_v8_global_t() {}

/// @brief creates a global context
TRI_v8_global_t* TRI_CreateV8Globals(v8::Isolate* isolate) {
  TRI_GET_GLOBALS();

  TRI_ASSERT(v8g == nullptr);
  v8g = new TRI_v8_global_t(isolate);
  isolate->SetData(arangodb::V8PlatformFeature::V8_DATA_SLOT, v8g);

  return v8g;
}

/// @brief returns a global context
TRI_v8_global_t* TRI_GetV8Globals(v8::Isolate* isolate) {
  TRI_GET_GLOBALS();

  if (v8g == nullptr) {
    v8g = TRI_CreateV8Globals(isolate);
  }

  TRI_ASSERT(v8g != nullptr);
  return v8g;
}

/// @brief adds a method to an object
void TRI_AddMethodVocbase(
    v8::Isolate* isolate, v8::Handle<v8::ObjectTemplate> tpl,
    v8::Handle<v8::String> name,
    void (*func)(v8::FunctionCallbackInfo<v8::Value> const&), bool isHidden) {
  if (isHidden) {
    // hidden method
    tpl->Set(name, v8::FunctionTemplate::New(isolate, func), v8::DontEnum);
  } else {
    // normal method
    tpl->Set(name, v8::FunctionTemplate::New(isolate, func));
  }
}

/// @brief adds a global function to the given context
void TRI_AddGlobalFunctionVocbase(
    v8::Isolate* isolate,
    v8::Handle<v8::String> name,
    void (*func)(v8::FunctionCallbackInfo<v8::Value> const&), bool isHidden) {
  // all global functions are read-only
  if (isHidden) {
    isolate->GetCurrentContext()->Global()->ForceSet(
        name, v8::FunctionTemplate::New(isolate, func)->GetFunction(),
        static_cast<v8::PropertyAttribute>(v8::ReadOnly | v8::DontEnum));
  } else {
    isolate->GetCurrentContext()->Global()->ForceSet(
        name, v8::FunctionTemplate::New(isolate, func)->GetFunction(),
        v8::ReadOnly);
  }
}

/// @brief adds a global function to the given context
void TRI_AddGlobalFunctionVocbase(v8::Isolate* isolate,
                                  v8::Handle<v8::String> name,
                                  v8::Handle<v8::Function> func,
                                  bool isHidden) {
  // all global functions are read-only
  if (isHidden) {
    isolate->GetCurrentContext()->Global()->ForceSet(name, func, static_cast<v8::PropertyAttribute>(
                                                v8::ReadOnly | v8::DontEnum));
  } else {
    isolate->GetCurrentContext()->Global()->ForceSet(name, func, v8::ReadOnly);
  }
}

/// @brief adds a global read-only variable to the given context
void TRI_AddGlobalVariableVocbase(v8::Isolate* isolate,
                                  v8::Handle<v8::String> name,
                                  v8::Handle<v8::Value> value) {
  // all global variables are read-only
  isolate->GetCurrentContext()->Global()->ForceSet(name, value, v8::ReadOnly);
}
