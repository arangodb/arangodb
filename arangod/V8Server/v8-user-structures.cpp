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

#include "v8-user-structures.h"
#include "Basics/Exceptions.h"
#include "Basics/ReadLocker.h"
#include "Basics/ReadWriteLock.h"
#include "Basics/WriteLocker.h"
#include "V8/v8-conv.h"
#include "V8/v8-utils.h"
#include "V8/v8-vpack.h"
#include "V8Server/v8-vocbaseprivate.h"
#include "VocBase/vocbase.h"

#include <velocypack/Buffer.h>
#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

namespace arangodb {
  
DatabaseJavaScriptCache::~DatabaseJavaScriptCache() = default;

v8::Handle<v8::Value> CacheKeySpace::keyGet(v8::Isolate* isolate, std::string const& key) {
  READ_LOCKER(readLocker, _lock);

  auto it = _hash.find(key);
  
  if (it == _hash.end()) {
    return v8::Undefined(isolate);
  }

  return TRI_VPackToV8(isolate, VPackSlice((*it).second->data()));
}

bool CacheKeySpace::keySet(v8::Isolate* isolate, std::string const& key,
                           v8::Handle<v8::Value> const& value, bool replace) {
  VPackBuilder builder;
  TRI_V8ToVPack(isolate, builder, value, false);
  auto buffer = builder.steal();

  WRITE_LOCKER(writeLocker, _lock);

  if (replace) {
    auto [it, emplaced] = _hash.insert_or_assign(key, std::move(buffer));
    return emplaced;
  }
    
  auto [it, emplaced] = _hash.try_emplace(key, std::move(buffer));
  return emplaced;
}

} // namespace arangodb

using namespace arangodb;

/// @brief finds a keyspace by name
/// note that at least the read-lock must be held to use this function
static CacheKeySpace* GetKeySpace(TRI_vocbase_t& vocbase, std::string const& name) {
  DatabaseJavaScriptCache* cache = vocbase._cacheData.get();
  auto it = cache->keyspaces.find(name);

  if (it != cache->keyspaces.end()) {
    return (*it).second.get();
  }
  return nullptr;
}

/// @brief creates a keyspace
static void JS_KeyspaceCreate(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() < 1 || !args[0]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "KEYSPACE_CREATE(<name>, <size>, <ignoreExisting>)");
  }

  auto& vocbase = GetContextVocBase(isolate);
  std::string const name = TRI_ObjectToString(isolate, args[0]);

  bool ignoreExisting = false;

  if (args.Length() > 2) {
    ignoreExisting = TRI_ObjectToBoolean(isolate, args[2]);
  }

  auto ptr = std::make_unique<CacheKeySpace>();
  DatabaseJavaScriptCache* cache = vocbase._cacheData.get();

  {
    WRITE_LOCKER(writeLocker, cache->lock);
    
    if (cache->keyspaces.find(name) != cache->keyspaces.end()) {
      if (!ignoreExisting) {
        TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_FAILED,
                                       "keyspace already exists");
      }
      TRI_V8_RETURN_FALSE();
    }

    cache->keyspaces.try_emplace(name, std::move(ptr));
  }

  TRI_V8_RETURN_TRUE();
  TRI_V8_TRY_CATCH_END
}

/// @brief drops a keyspace
static void JS_KeyspaceDrop(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() != 1 || !args[0]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE("KEYSPACE_DROP(<name>)");
  }

  auto& vocbase = GetContextVocBase(isolate);
  std::string const name = TRI_ObjectToString(isolate, args[0]);
  DatabaseJavaScriptCache* cache = vocbase._cacheData.get();

  {
    WRITE_LOCKER(writeLocker, cache->lock);

    if (cache->keyspaces.erase(name) == 0) {
      TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_FAILED,
                                     "keyspace does not exist");
    }
  }

  TRI_V8_RETURN_TRUE();
  TRI_V8_TRY_CATCH_END
}

/// @brief returns whether a keyspace exists
static void JS_KeyspaceExists(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() != 1 || !args[0]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE("KEYSPACE_EXISTS(<name>)");
  }

  auto& vocbase = GetContextVocBase(isolate);
  std::string const name = TRI_ObjectToString(isolate, args[0]);
  DatabaseJavaScriptCache* cache = vocbase._cacheData.get();

  READ_LOCKER(readLocker, cache->lock);
  
  if (cache->keyspaces.find(name) != cache->keyspaces.end()) {
    TRI_V8_RETURN_TRUE();
  }

  TRI_V8_RETURN_FALSE();
  TRI_V8_TRY_CATCH_END
}

/// @brief returns the value for a key in the keyspace
static void JS_KeyGet(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() < 2 || !args[0]->IsString() || !args[1]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE("KEY_GET(<name>, <key>)");
  }

  auto& vocbase = GetContextVocBase(isolate);
  std::string const name = TRI_ObjectToString(isolate, args[0]);
  std::string const key = TRI_ObjectToString(isolate, args[1]);
  v8::Handle<v8::Value> result;

  DatabaseJavaScriptCache* cache = vocbase._cacheData.get();
  
  {
    READ_LOCKER(readLocker, cache->lock);
    auto keyspace = GetKeySpace(vocbase, name);

    if (keyspace == nullptr) {
      TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_FAILED, "keyspace does not exist");
    }

    result = keyspace->keyGet(isolate, key);
  }

  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

/// @brief set the value for a key in the keyspace
static void JS_KeySet(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() < 3 || !args[0]->IsString() || !args[1]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE("KEY_SET(<name>, <key>, <value>, <replace>)");
  }

  auto& vocbase = GetContextVocBase(isolate);
  std::string const name = TRI_ObjectToString(isolate, args[0]);
  std::string const key = TRI_ObjectToString(isolate, args[1]);
  bool replace = true;

  if (args.Length() > 3) {
    replace = TRI_ObjectToBoolean(isolate, args[3]);
  }

  DatabaseJavaScriptCache* cache = vocbase._cacheData.get();

  bool result;

  {
    READ_LOCKER(readLocker, cache->lock);
    auto keyspace = GetKeySpace(vocbase, name);

    if (keyspace == nullptr) {
      TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_FAILED, "keyspace does not exist");
    }

    result = keyspace->keySet(isolate, key, args[2], replace);
  }

  if (result) {
    TRI_V8_RETURN_TRUE();
  }

  TRI_V8_RETURN_FALSE();
  TRI_V8_TRY_CATCH_END
}

/// @brief creates the user structures functions
void TRI_InitV8UserStructures(v8::Isolate* isolate, v8::Handle<v8::Context> /*context*/) {
  v8::HandleScope scope(isolate);

  // NOTE: the following functions are all experimental and might
  // change without further notice
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate, "KEYSPACE_CREATE"),
                               JS_KeyspaceCreate, true);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate, "KEYSPACE_DROP"),
                               JS_KeyspaceDrop, true);
  TRI_AddGlobalFunctionVocbase(isolate,
                               TRI_V8_ASCII_STRING(isolate, "KEYSPACE_EXISTS"),
                               JS_KeyspaceExists, true);

  TRI_AddGlobalFunctionVocbase(isolate, TRI_V8_ASCII_STRING(isolate, "KEY_SET"),
                               JS_KeySet, true);
  TRI_AddGlobalFunctionVocbase(isolate, TRI_V8_ASCII_STRING(isolate, "KEY_GET"),
                               JS_KeyGet, true);
}
