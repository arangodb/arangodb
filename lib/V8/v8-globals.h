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

#ifndef ARANGODB_V8_V8__GLOBALS_H
#define ARANGODB_V8_V8__GLOBALS_H 1

#include "Basics/Common.h"

#include "ApplicationFeatures/V8PlatformFeature.h"

struct TRI_vocbase_t;

/// @brief shortcut for fetching the isolate from the thread context
#define ISOLATE v8::Isolate* isolate = v8::Isolate::GetCurrent()

/// @brief macro to initiate a try-catch sequence for V8 callbacks
#define TRI_V8_TRY_CATCH_BEGIN(isolateVar) \
  auto isolateVar = args.GetIsolate();     \
  try {
/// @brief macro to terminate a try-catch sequence for V8 callbacks
#define TRI_V8_TRY_CATCH_END                                       \
  } catch (arangodb::basics::Exception const& ex) {                \
    TRI_V8_THROW_EXCEPTION_FULL(ex.code(), ex.what());             \
  } catch (std::exception const& ex) {                             \
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, ex.what()); \
  } catch (...) {                                                  \
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_INTERNAL);                    \
  }

/// @brief shortcut for creating a v8 symbol for the specified string
///   implicites isolate available.
/// @param name local string constant to source
#define TRI_V8_ASCII_STRING(name)                             \
  v8::String::NewFromOneByte(isolate, (uint8_t const*)(name), \
                             v8::String::kNormalString, (int)strlen(name))

#define TRI_V8_ASCII_STRING2(isolate, name)                   \
  v8::String::NewFromOneByte(isolate, (uint8_t const*)(name), \
                             v8::String::kNormalString, (int)strlen(name))

#define TRI_V8_ASCII_STD_STRING(isolate, name)                        \
  v8::String::NewFromOneByte(isolate, (uint8_t const*)(name.c_str()), \
                             v8::String::kNormalString, (int)name.size())

#define TRI_V8_ASCII_PAIR_STRING(name, length)                \
  v8::String::NewFromOneByte(isolate, (uint8_t const*)(name), \
                             v8::String::kNormalString, (int)(length))

/// @brief shortcut for creating a v8 string for the specified string
///  Implicit argument: isolate (is assumed to be defined)
/// @param name local char* to use
///  should be avoided if possible due to performance reasons!
#define TRI_V8_STRING(name)                                           \
  v8::String::NewFromUtf8(isolate, (name), v8::String::kNormalString, \
                          (int)strlen(name))

#define TRI_V8_STRING2(isolate, name)                                 \
  v8::String::NewFromUtf8(isolate, (name), v8::String::kNormalString, \
                          (int)strlen(name))

/// @brief shortcut for creating a v8 symbol for the specified string
///   implicites isolate available.
/// @param name local std::string to use
#define TRI_V8_STD_STRING(name)                                             \
  v8::String::NewFromUtf8(isolate, name.c_str(), v8::String::kNormalString, \
                          (int)name.length())

#define TRI_V8_STD_STRING2(isolate, name)                                   \
  v8::String::NewFromUtf8(isolate, name.c_str(), v8::String::kNormalString, \
                          (int)name.length())

/// @brief shortcut for creating a v8 symbol for the specified string
///   implicites isolate available.
#define TRI_V8_STRING_UTF16(name, length) \
  v8::String::NewFromTwoByte(isolate, (name), v8::String::kNormalString, length)

/// @brief shortcut for creating a v8 symbol for the specified string of known
/// length
///   implicites isolate available.
#define TRI_V8_PAIR_STRING(name, length)                              \
  v8::String::NewFromUtf8(isolate, (name), v8::String::kNormalString, \
                          (int)(length))

/// @brief shortcut for current v8 globals and scope
#define TRI_V8_CURRENT_GLOBALS_AND_SCOPE                            \
  TRI_v8_global_t* v8g = static_cast<TRI_v8_global_t*>(             \
      isolate->GetData(arangodb::V8PlatformFeature::V8_DATA_SLOT)); \
  v8::HandleScope scope(isolate);                                   \
  do {                                                              \
  } while (0)

/// @brief shortcut for throwing an exception with an error code
/// Implicitly demands *args* to be function arguments.
#define TRI_V8_SET_EXCEPTION(code)        \
  do {                                    \
    TRI_CreateErrorObject(isolate, code); \
  } while (0)

#define TRI_V8_THROW_EXCEPTION(code) \
  do {                               \
    TRI_V8_SET_EXCEPTION(code);      \
    return;                          \
  } while (0)

/// @brief shortcut for throwing an exception and returning
/// Implicitly demands *args* to be function arguments.
#define TRI_V8_SET_EXCEPTION_MESSAGE(code, message)      \
  do {                                                   \
    TRI_CreateErrorObject(isolate, code, message, true); \
  } while (0)

#define TRI_V8_THROW_EXCEPTION_MESSAGE(code, message) \
  do {                                                \
    TRI_V8_SET_EXCEPTION_MESSAGE(code, message);      \
    return;                                           \
  } while (0)

/// @brief shortcut for throwing an exception and returning
/// Implicitly demands *args* to be function arguments.
#define TRI_V8_THROW_EXCEPTION_FULL(code, message)        \
  do {                                                    \
    TRI_CreateErrorObject(isolate, code, message, false); \
    return;                                               \
  } while (0)

/// @brief shortcut for throwing a usage exception and returning
#define TRI_V8_THROW_EXCEPTION_USAGE(usage)                       \
  do {                                                            \
    std::string msg = "usage: ";                                  \
    msg += usage;                                                 \
    TRI_CreateErrorObject(isolate, TRI_ERROR_BAD_PARAMETER, msg, false); \
    return;                                                       \
  } while (0)

/// @brief shortcut for throwing an internal exception and returning
#define TRI_V8_THROW_EXCEPTION_INTERNAL(message)                 \
  do {                                                           \
    TRI_CreateErrorObject(isolate, TRI_ERROR_INTERNAL, message, false); \
    return;                                                      \
  } while (0)

/// @brief shortcut for throwing a parameter exception and returning
#define TRI_V8_THROW_EXCEPTION_PARAMETER(message)                     \
  do {                                                                \
    TRI_CreateErrorObject(isolate, TRI_ERROR_BAD_PARAMETER, message, false); \
    return;                                                           \
  } while (0)

/// @brief shortcut for throwing an out-of-memory exception and returning
#define TRI_V8_SET_EXCEPTION_MEMORY()                        \
  do {                                                       \
    TRI_CreateErrorObject(isolate, TRI_ERROR_OUT_OF_MEMORY); \
  } while (0)

#define TRI_V8_THROW_EXCEPTION_MEMORY() \
  do {                                  \
    TRI_V8_SET_EXCEPTION_MEMORY();      \
    return;                             \
  } while (0)

/// @brief shortcut for throwing an exception for an system error
#define TRI_V8_THROW_EXCEPTION_SYS(message)           \
  do {                                                \
    TRI_set_errno(TRI_ERROR_SYS_ERROR);               \
    std::string msg = message;                        \
    msg += ": ";                                      \
    msg += TRI_LAST_ERROR_STR;                        \
    TRI_CreateErrorObject(isolate, TRI_errno(), msg, false); \
    return;                                           \
  } while (0)

/// @brief shortcut for logging and forward throwing an error
#define TRI_V8_LOG_THROW_EXCEPTION(TRYCATCH) \
  do {                                       \
    TRI_LogV8Exception(isolate, &TRYCATCH);  \
    TRYCATCH.ReThrow();                      \
    return;                                  \
  } while (0)

/// @brief shortcut for throwing an error
#define TRI_V8_SET_ERROR(message)                                          \
  do {                                                                     \
    isolate->ThrowException(v8::Exception::Error(TRI_V8_STRING(message))); \
  } while (0)

#define TRI_V8_THROW_ERROR(message) \
  do {                              \
    TRI_V8_SET_ERROR(message);      \
    return;                         \
  } while (0)

/// @brief shortcut for throwing a range error
#define TRI_V8_THROW_RANGE_ERROR(message)                   \
  do {                                                      \
    isolate->ThrowException(                                \
        v8::Exception::RangeError(TRI_V8_STRING(message))); \
    return;                                                 \
  } while (0)

/// @brief shortcut for throwing a syntax error
#define TRI_V8_THROW_SYNTAX_ERROR(message)                   \
  do {                                                       \
    isolate->ThrowException(                                 \
        v8::Exception::SyntaxError(TRI_V8_STRING(message))); \
    return;                                                  \
  } while (0)

/// @brief shortcut for throwing a type error
#define TRI_V8_SET_TYPE_ERROR(message)                                         \
  do {                                                                         \
    isolate->ThrowException(v8::Exception::TypeError(TRI_V8_STRING(message))); \
  } while (0)

#define TRI_V8_THROW_TYPE_ERROR(message) \
  do {                                   \
    TRI_V8_SET_TYPE_ERROR(message);      \
    return;                              \
  } while (0)

/// @brief "not yet implemented" handler for sharding
#define TRI_THROW_SHARDING_COLLECTION_NOT_YET_IMPLEMENTED(collection) \
  if (collection != nullptr && !collection->isLocal()) {              \
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);                \
  }

/// @brief Return undefined (default..)
///   implicitly requires 'args and 'isolate' to be available
#define TRI_V8_RETURN_UNDEFINED()                    \
  args.GetReturnValue().Set(v8::Undefined(isolate)); \
  return

/// @brief Return 'true'
///   implicitly requires 'args and 'isolate' to be available
#define TRI_V8_RETURN_TRUE()                    \
  args.GetReturnValue().Set(v8::True(isolate)); \
  return

/// @brief Return 'false'
///   implicitly requires 'args and 'isolate' to be available
#define TRI_V8_RETURN_FALSE()                    \
  args.GetReturnValue().Set(v8::False(isolate)); \
  return

/// @brief return 'null'
///   implicitly requires 'args and 'isolate' to be available
#define TRI_V8_RETURN_NULL()                    \
  args.GetReturnValue().Set(v8::Null(isolate)); \
  return

/// @brief return any sort of V8-value
///   implicitly requires 'args and 'isolate' to be available
/// @param WHAT the name of the v8::Value/v8::Number/...
#define TRI_V8_RETURN(WHAT)        \
  args.GetReturnValue().Set(WHAT); \
  return

/// @brief return a char*
///   implicitly requires 'args and 'isolate' to be available
/// @param WHAT the name of the char* variable
#define TRI_V8_RETURN_STRING(WHAT)                                   \
  args.GetReturnValue().Set(v8::String::NewFromUtf8(isolate, WHAT)); \
  return

/// @brief return a std::string
///   implicitly requires 'args and 'isolate' to be available
/// @param WHAT the name of the std::string variable
#define TRI_V8_RETURN_STD_STRING(WHAT)                                        \
  args.GetReturnValue().Set(v8::String::NewFromUtf8(                          \
      isolate, WHAT.c_str(), v8::String::kNormalString, (int)WHAT.length())); \
  return

/// @brief return a string which you know the length of
///   implicitly requires 'args and 'isolate' to be available
/// @param WHAT the name of the char* variable
/// @param WHATLEn the name of the int variable containing the length of WHAT
#define TRI_V8_RETURN_PAIR_STRING(WHAT, WHATLEN)                \
  args.GetReturnValue().Set(v8::String::NewFromUtf8(            \
      isolate, WHAT, v8::String::kNormalString, (int)WHATLEN)); \
  return

/// @brief retrieve the instance of the TRI_v8_global of the current thread
///   implicitly creates a variable 'v8g' with a pointer to it.
///   implicitly requires 'isolate' to be available
#define TRI_GET_GLOBALS()                               \
  TRI_v8_global_t* v8g = static_cast<TRI_v8_global_t*>( \
      isolate->GetData(arangodb::V8PlatformFeature::V8_DATA_SLOT))

#define TRI_GET_GLOBALS2(isolate)                       \
  TRI_v8_global_t* v8g = static_cast<TRI_v8_global_t*>( \
      isolate->GetData(arangodb::V8PlatformFeature::V8_DATA_SLOT))

/// @brief fetch a string-member from the global into the local scope of the
/// function
///     will give you a variable of the same name.
///     implicitly requires 'v8g' and 'isolate' to be there.
/// @param WHICH the member string name to get as local variable
#define TRI_GET_GLOBAL_STRING(WHICH) \
  auto WHICH = v8::Local<v8::String>::New(isolate, v8g->WHICH)

/// @brief fetch a member from the global into the local scope of the function
///     will give you a variable of the same name.
///     implicitly requires 'v8g' and 'isolate' to be there.
/// @param WHICH the member name to get as local variable
/// @param TYPE  the type of the member to instantiate
#define TRI_GET_GLOBAL(WHICH, TYPE) \
  auto WHICH = v8::Local<TYPE>::New(isolate, v8g->WHICH)

/// @brief globals stored in the isolate
struct TRI_v8_global_t {
  explicit TRI_v8_global_t(v8::Isolate*);

  ~TRI_v8_global_t() {}

  /// @brief whether or not the context has active externals
  inline bool hasActiveExternals() const { return _activeExternals > 0; }

  /// @brief increase the number of active externals
  inline void increaseActiveExternals() { ++_activeExternals; }

  /// @brief decrease the number of active externals
  inline void decreaseActiveExternals() { --_activeExternals; }

  /// @brief collections mapping for weak pointers
  std::unordered_map<void*, v8::Persistent<v8::External>> JSCollections;
  
  /// @brief views mapping for weak pointers
  std::unordered_map<void*, v8::Persistent<v8::External>> JSViews;

  /// @brief agency template
  v8::Persistent<v8::ObjectTemplate> AgencyTempl;

  /// @brief local agent template
  v8::Persistent<v8::ObjectTemplate> AgentTempl;

  /// @brief clusterinfo template
  v8::Persistent<v8::ObjectTemplate> ClusterInfoTempl;

  /// @brief server state template
  v8::Persistent<v8::ObjectTemplate> ServerStateTempl;

  /// @brief cluster comm template
  v8::Persistent<v8::ObjectTemplate> ClusterCommTempl;

  /// @brief ArangoError template
  v8::Persistent<v8::ObjectTemplate> ArangoErrorTempl;

  /// @brief VPack template
  v8::Persistent<v8::ObjectTemplate> VPackTempl;
  
  /// @brief collection template
  v8::Persistent<v8::ObjectTemplate> VocbaseColTempl;
  
  /// @brief view template
  v8::Persistent<v8::ObjectTemplate> VocbaseViewTempl;

  /// @brief TRI_vocbase_t template
  v8::Persistent<v8::ObjectTemplate> VocbaseTempl;

  /// @brief TRI_vocbase_t template
  v8::Persistent<v8::ObjectTemplate> EnvTempl;
  
  /// @brief users template
  v8::Persistent<v8::ObjectTemplate> UsersTempl;

  /// @brief Buffer template
  v8::Persistent<v8::FunctionTemplate> BufferTempl;

  /// @brief "Buffer" constant
  v8::Persistent<v8::String> BufferConstant;

  /// @brief "DELETE" constant
  v8::Persistent<v8::String> DeleteConstant;

  /// @brief "GET" constant
  v8::Persistent<v8::String> GetConstant;

  /// @brief "HEAD" constant
  v8::Persistent<v8::String> HeadConstant;

  /// @brief "OPTIONS" constant
  v8::Persistent<v8::String> OptionsConstant;

  /// @brief "PATCH" constant
  v8::Persistent<v8::String> PatchConstant;

  /// @brief "POST" constant
  v8::Persistent<v8::String> PostConstant;

  /// @brief "PUT" constant
  v8::Persistent<v8::String> PutConstant;

  /// @brief "address" key name
  v8::Persistent<v8::String> AddressKey;

  /// @brief "allowUseDatabase" key name
  v8::Persistent<v8::String> AllowUseDatabaseKey;

  /// @brief "authorized" key name
  v8::Persistent<v8::String> AuthorizedKey;

  /// @brief "bodyFromFile" key name
  v8::Persistent<v8::String> BodyFromFileKey;

  /// @brief "body" key name
  v8::Persistent<v8::String> BodyKey;

  /// @brief "client" key name
  v8::Persistent<v8::String> ClientKey;

  /// @brief "clientTransactionID" key name
  v8::Persistent<v8::String> ClientTransactionIDKey;

  /// @brief "code" key name
  v8::Persistent<v8::String> CodeKey;

  /// @brief "compatibility" key name
  v8::Persistent<v8::String> CompatibilityKey;

  /// @brief "contentType" key name
  v8::Persistent<v8::String> ContentTypeKey;

  /// @brief "cookies" key name
  v8::Persistent<v8::String> CookiesKey;

  /// @brief "coordTransactionID" key name
  v8::Persistent<v8::String> CoordTransactionIDKey;

  /// @brief "database" key name
  v8::Persistent<v8::String> DatabaseKey;

  /// @brief "doCompact" key name
  v8::Persistent<v8::String> DoCompactKey;

  /// @brief "domain" key
  v8::Persistent<v8::String> DomainKey;

  /// @brief "endpoint" key name
  v8::Persistent<v8::String> EndpointKey;

  /// @brief "error" key name
  v8::Persistent<v8::String> ErrorKey;

  /// @brief "errorMessage" key name
  v8::Persistent<v8::String> ErrorMessageKey;

  /// @brief "errorNum" key name
  v8::Persistent<v8::String> ErrorNumKey;

  /// @brief "headers" key name
  v8::Persistent<v8::String> HeadersKey;

  /// @brief "httpOnly" key
  v8::Persistent<v8::String> HttpOnlyKey;

  /// @brief "id" key name
  v8::Persistent<v8::String> IdKey;

  /// @brief "initTimeout" key name
  v8::Persistent<v8::String> InitTimeoutKey;

  /// @brief "isRestore" key name
  v8::Persistent<v8::String> IsRestoreKey;

  /// @brief "isSynchronousReplication" key name
  v8::Persistent<v8::String> IsSynchronousReplicationKey;

  /// @brief "isSystem" key name
  v8::Persistent<v8::String> IsSystemKey;

  /// @brief "isVolatile" key name
  v8::Persistent<v8::String> IsVolatileKey;

  /// @brief "journalSize" key name
  v8::Persistent<v8::String> JournalSizeKey;

  /// @brief "keepNull" key name
  v8::Persistent<v8::String> KeepNullKey;

  /// @brief "keyOptions" key name
  v8::Persistent<v8::String> KeyOptionsKey;

  /// @brief "length" key
  v8::Persistent<v8::String> LengthKey;

  /// @brief "lifeTime" key
  v8::Persistent<v8::String> LifeTimeKey;

  /// @brief "mergeObjects" key name
  v8::Persistent<v8::String> MergeObjectsKey;

  /// @brief "name" key
  v8::Persistent<v8::String> NameKey;

  /// @brief "operationID" key
  v8::Persistent<v8::String> OperationIDKey;

  /// @brief "overwrite" key
  v8::Persistent<v8::String> OverwriteKey;

  /// @brief "parameters" key name
  v8::Persistent<v8::String> ParametersKey;

  /// @brief "path" key name
  v8::Persistent<v8::String> PathKey;

  /// @brief "prefix" key name
  v8::Persistent<v8::String> PrefixKey;

  /// @brief "port" key name
  v8::Persistent<v8::String> PortKey;

  /// @brief "portType" key name
  v8::Persistent<v8::String> PortTypeKey;

  /// @brief "protocol" key name
  v8::Persistent<v8::String> ProtocolKey;
  
  /// @brief "rawSuffix" key name
  v8::Persistent<v8::String> RawSuffixKey;

  /// @brief "requestBody" key name
  v8::Persistent<v8::String> RequestBodyKey;

  /// @brief "requestType" key name
  v8::Persistent<v8::String> RequestTypeKey;

  /// @brief "responseCode" key name
  v8::Persistent<v8::String> ResponseCodeKey;

  /// @brief "returnNew" key name
  v8::Persistent<v8::String> ReturnNewKey;

  /// @brief "returnOld" key name
  v8::Persistent<v8::String> ReturnOldKey;

  /// @brief "secure" key
  v8::Persistent<v8::String> SecureKey;

  /// @brief "server" key
  v8::Persistent<v8::String> ServerKey;

  /// @brief "shardID" key name
  v8::Persistent<v8::String> ShardIDKey;

  /// @brief "silent" key name
  v8::Persistent<v8::String> SilentKey;

  /// @brief "singleRequest" key name
  v8::Persistent<v8::String> SingleRequestKey;

  /// @brief "status" key name
  v8::Persistent<v8::String> StatusKey;

  /// @brief "suffix" key name
  v8::Persistent<v8::String> SuffixKey;

  /// @brief "timeout" key name
  v8::Persistent<v8::String> TimeoutKey;

  /// @brief "toJson" key name
  v8::Persistent<v8::String> ToJsonKey;

  /// @brief "transformations" key name
  v8::Persistent<v8::String> TransformationsKey;

  /// @brief "url" key name
  v8::Persistent<v8::String> UrlKey;

  /// @brief "user" key name
  v8::Persistent<v8::String> UserKey;

  /// @brief "value" key
  v8::Persistent<v8::String> ValueKey;

  /// @brief "version" key
  v8::Persistent<v8::String> VersionKeyHidden;

  /// @brief "waitForSync" key name
  v8::Persistent<v8::String> WaitForSyncKey;

  /// @brief "_dbCache" key name
  v8::Persistent<v8::String> _DbCacheKey;

  /// @brief "_dbName" key name
  v8::Persistent<v8::String> _DbNameKey;

  /// @brief system attribute names
  v8::Persistent<v8::String> _IdKey;
  v8::Persistent<v8::String> _KeyKey;
  v8::Persistent<v8::String> _RevKey;
  v8::Persistent<v8::String> _FromKey;
  v8::Persistent<v8::String> _ToKey;

  /// @brief currently request object (might be invalid!)
  v8::Handle<v8::Value> _currentRequest;

  /// @brief currently response object (might be invalid!)
  v8::Handle<v8::Value> _currentResponse;

  /// @brief information about the currently running transaction
  void* _transactionContext;

  /// @brief query registry - note: this shouldn't be changed once set
  void* _queryRegistry;

  /// @brief current AQL query
  void* _query;

  /// @brief pointer to the vocbase (TRI_vocbase_t*)
  TRI_vocbase_t* _vocbase;

  /// @brief number of v8 externals used in the context
  int64_t _activeExternals;

  /// @brief cancel has been caught
  bool _canceled;

  /// @brief whether or not useDatabase() is allowed
  bool _allowUseDatabase;
};

/// @brief creates a global context
TRI_v8_global_t* TRI_CreateV8Globals(v8::Isolate*);

/// @brief gets the global context
TRI_v8_global_t* TRI_GetV8Globals(v8::Isolate*);

/// @brief adds a method to the prototype of an object
template <typename TARGET>
void TRI_V8_AddProtoMethod(v8::Isolate* isolate, TARGET tpl,
                           v8::Handle<v8::String> name,
                           v8::FunctionCallback callback,
                           bool isHidden = false) {
  // hidden method
  if (isHidden) {
    tpl->PrototypeTemplate()->Set(
        name, v8::FunctionTemplate::New(isolate, callback), v8::DontEnum);
  }

  // normal method
  else {
    tpl->PrototypeTemplate()->Set(name,
                                  v8::FunctionTemplate::New(isolate, callback));
  }
}

/// @brief adds a method to an object
template <typename TARGET>
inline void TRI_V8_AddMethod(v8::Isolate* isolate, TARGET tpl,
                             v8::Handle<v8::String> name,
                             v8::Handle<v8::FunctionTemplate> callback,
                             bool isHidden = false) {
  // hidden method
  if (isHidden) {
    tpl->ForceSet(name, callback->GetFunction(), v8::DontEnum);
  }
  // normal method
  else {
    tpl->Set(name, callback->GetFunction());
  }
}

template <typename TARGET>
inline void TRI_V8_AddMethod(v8::Isolate* isolate, TARGET tpl,
                             v8::Handle<v8::String> name,
                             v8::FunctionCallback callback,
                             bool isHidden = false) {
  // hidden method
  if (isHidden) {
    tpl->ForceSet(name, v8::FunctionTemplate::New(isolate, callback)->GetFunction(), v8::DontEnum);
  }
  // normal method
  else {
    tpl->Set(name, v8::FunctionTemplate::New(isolate, callback)->GetFunction());
  }
}

template <>
inline void TRI_V8_AddMethod(v8::Isolate* isolate,
                             v8::Handle<v8::FunctionTemplate> tpl,
                             v8::Handle<v8::String> name,
                             v8::FunctionCallback callback, bool isHidden) {
  TRI_V8_AddMethod(isolate, tpl->GetFunction(), name, callback, isHidden);
}

/// @brief adds a method to an object
void TRI_AddMethodVocbase(
    v8::Isolate* isolate, v8::Handle<v8::ObjectTemplate> tpl,
    v8::Handle<v8::String> name,
    void (*func)(v8::FunctionCallbackInfo<v8::Value> const&),
    bool isHidden = false);

/// @brief adds a global function to the given context
void TRI_AddGlobalFunctionVocbase(
    v8::Isolate* isolate, 
    v8::Handle<v8::String> name,
    void (*func)(v8::FunctionCallbackInfo<v8::Value> const&),
    bool isHidden = false);

/// @brief adds a global function to the given context
void TRI_AddGlobalFunctionVocbase(v8::Isolate* isolate,
                                  v8::Handle<v8::String> name,
                                  v8::Handle<v8::Function> func,
                                  bool isHidden = false);

/// @brief adds a global read-only variable to the given context
void TRI_AddGlobalVariableVocbase(v8::Isolate* isolate,
                                  v8::Handle<v8::String> name,
                                  v8::Handle<v8::Value> value);

#endif
