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
    : JSCollections(),
      JSViews(),

      AgencyTempl(),
      AgentTempl(),
      ClusterInfoTempl(),
      ServerStateTempl(),
      ClusterCommTempl(),
      ArangoErrorTempl(),
      VPackTempl(),
      VocbaseColTempl(),
      VocbaseViewTempl(),
      VocbaseTempl(),
      EnvTempl(),
      UsersTempl(),

      BufferTempl(),

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
      ClientTransactionIDKey(),
      CodeKey(),
      CompatibilityKey(),
      ContentTypeKey(),
      CoordTransactionIDKey(),
      DatabaseKey(),
      DoCompactKey(),
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
      IsVolatileKey(),
      JournalSizeKey(),
      KeepNullKey(),
      KeyOptionsKey(),
      LengthKey(),
      LifeTimeKey(),
      MergeObjectsKey(),
      NameKey(),
      OperationIDKey(),
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

  BufferConstant.Reset(isolate, TRI_V8_ASCII_STRING("Buffer"));
  DeleteConstant.Reset(isolate, TRI_V8_ASCII_STRING("DELETE"));
  GetConstant.Reset(isolate, TRI_V8_ASCII_STRING("GET"));
  HeadConstant.Reset(isolate, TRI_V8_ASCII_STRING("HEAD"));
  OptionsConstant.Reset(isolate, TRI_V8_ASCII_STRING("OPTIONS"));
  PatchConstant.Reset(isolate, TRI_V8_ASCII_STRING("PATCH"));
  PostConstant.Reset(isolate, TRI_V8_ASCII_STRING("POST"));
  PutConstant.Reset(isolate, TRI_V8_ASCII_STRING("PUT"));

  AddressKey.Reset(isolate, TRI_V8_ASCII_STRING("address"));
  AllowUseDatabaseKey.Reset(isolate, TRI_V8_ASCII_STRING("allowUseDatabase"));
  AuthorizedKey.Reset(isolate, TRI_V8_ASCII_STRING("authorized"));
  BodyFromFileKey.Reset(isolate, TRI_V8_ASCII_STRING("bodyFromFile"));
  BodyKey.Reset(isolate, TRI_V8_ASCII_STRING("body"));
  ClientKey.Reset(isolate, TRI_V8_ASCII_STRING("client"));
  ClientTransactionIDKey.Reset(isolate,
                               TRI_V8_ASCII_STRING("clientTransactionID"));
  CodeKey.Reset(isolate, TRI_V8_ASCII_STRING("code"));
  CompatibilityKey.Reset(isolate, TRI_V8_ASCII_STRING("compatibility"));
  ContentTypeKey.Reset(isolate, TRI_V8_ASCII_STRING("contentType"));
  CookiesKey.Reset(isolate, TRI_V8_ASCII_STRING("cookies"));
  CoordTransactionIDKey.Reset(isolate,
                              TRI_V8_ASCII_STRING("coordTransactionID"));
  DatabaseKey.Reset(isolate, TRI_V8_ASCII_STRING("database"));
  DoCompactKey.Reset(isolate, TRI_V8_ASCII_STRING("doCompact"));
  DomainKey.Reset(isolate, TRI_V8_ASCII_STRING("domain"));
  EndpointKey.Reset(isolate, TRI_V8_ASCII_STRING("endpoint"));
  ErrorKey.Reset(isolate, TRI_V8_ASCII_STRING("error"));
  ErrorMessageKey.Reset(isolate, TRI_V8_ASCII_STRING("errorMessage"));
  ErrorNumKey.Reset(isolate, TRI_V8_ASCII_STRING("errorNum"));
  HeadersKey.Reset(isolate, TRI_V8_ASCII_STRING("headers"));
  HttpOnlyKey.Reset(isolate, TRI_V8_ASCII_STRING("httpOnly"));
  IdKey.Reset(isolate, TRI_V8_ASCII_STRING("id"));
  InitTimeoutKey.Reset(isolate, TRI_V8_ASCII_STRING("initTimeout"));
  IsRestoreKey.Reset(isolate, TRI_V8_ASCII_STRING("isRestore"));
  IsSynchronousReplicationKey.Reset(isolate,
      TRI_V8_ASCII_STRING("isSynchronousReplication"));
  IsSystemKey.Reset(isolate, TRI_V8_ASCII_STRING("isSystem"));
  IsVolatileKey.Reset(isolate, TRI_V8_ASCII_STRING("isVolatile"));
  JournalSizeKey.Reset(isolate, TRI_V8_ASCII_STRING("journalSize"));
  KeepNullKey.Reset(isolate, TRI_V8_ASCII_STRING("keepNull"));
  KeyOptionsKey.Reset(isolate, TRI_V8_ASCII_STRING("keyOptions"));
  LengthKey.Reset(isolate, TRI_V8_ASCII_STRING("length"));
  LifeTimeKey.Reset(isolate, TRI_V8_ASCII_STRING("lifeTime"));
  MergeObjectsKey.Reset(isolate, TRI_V8_ASCII_STRING("mergeObjects"));
  NameKey.Reset(isolate, TRI_V8_ASCII_STRING("name"));
  OperationIDKey.Reset(isolate, TRI_V8_ASCII_STRING("operationID"));
  OverwriteKey.Reset(isolate, TRI_V8_ASCII_STRING("overwrite"));
  ParametersKey.Reset(isolate, TRI_V8_ASCII_STRING("parameters"));
  PathKey.Reset(isolate, TRI_V8_ASCII_STRING("path"));
  PrefixKey.Reset(isolate, TRI_V8_ASCII_STRING("prefix"));
  PortKey.Reset(isolate, TRI_V8_ASCII_STRING("port"));
  PortTypeKey.Reset(isolate, TRI_V8_ASCII_STRING("portType"));
  ProtocolKey.Reset(isolate, TRI_V8_ASCII_STRING("protocol"));
  RawSuffixKey.Reset(isolate, TRI_V8_ASCII_STRING("rawSuffix"));
  RequestBodyKey.Reset(isolate, TRI_V8_ASCII_STRING("requestBody"));
  RequestTypeKey.Reset(isolate, TRI_V8_ASCII_STRING("requestType"));
  ResponseCodeKey.Reset(isolate, TRI_V8_ASCII_STRING("responseCode"));
  ReturnNewKey.Reset(isolate, TRI_V8_ASCII_STRING("returnNew"));
  ReturnOldKey.Reset(isolate, TRI_V8_ASCII_STRING("returnOld"));
  SecureKey.Reset(isolate, TRI_V8_ASCII_STRING("secure"));
  ServerKey.Reset(isolate, TRI_V8_ASCII_STRING("server"));
  ShardIDKey.Reset(isolate, TRI_V8_ASCII_STRING("shardID"));
  SilentKey.Reset(isolate, TRI_V8_ASCII_STRING("silent"));
  SingleRequestKey.Reset(isolate, TRI_V8_ASCII_STRING("singleRequest"));
  StatusKey.Reset(isolate, TRI_V8_ASCII_STRING("status"));
  SuffixKey.Reset(isolate, TRI_V8_ASCII_STRING("suffix"));
  TimeoutKey.Reset(isolate, TRI_V8_ASCII_STRING("timeout"));
  ToJsonKey.Reset(isolate, TRI_V8_ASCII_STRING("toJSON"));
  TransformationsKey.Reset(isolate, TRI_V8_ASCII_STRING("transformations"));
  UrlKey.Reset(isolate, TRI_V8_ASCII_STRING("url"));
  UserKey.Reset(isolate, TRI_V8_ASCII_STRING("user"));
  ValueKey.Reset(isolate, TRI_V8_ASCII_STRING("value"));
  VersionKeyHidden.Reset(isolate, TRI_V8_ASCII_STRING("*version"));
  WaitForSyncKey.Reset(isolate, TRI_V8_ASCII_STRING("waitForSync"));

  _DbCacheKey.Reset(isolate, TRI_V8_ASCII_STRING("__dbcache__"));
  _DbNameKey.Reset(isolate, TRI_V8_ASCII_STRING("_dbName"));
  _IdKey.Reset(isolate, TRI_V8_ASCII_STRING("_id"));
  _KeyKey.Reset(isolate, TRI_V8_ASCII_STRING("_key"));
  _RevKey.Reset(isolate, TRI_V8_ASCII_STRING("_rev"));
  _FromKey.Reset(isolate, TRI_V8_ASCII_STRING("_from"));
  _ToKey.Reset(isolate, TRI_V8_ASCII_STRING("_to"));
}

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
