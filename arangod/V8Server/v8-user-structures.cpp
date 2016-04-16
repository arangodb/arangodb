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

#include "v8-user-structures.h"
#include "Basics/Exceptions.h"
#include "Basics/ReadWriteLock.h"
#include "Basics/ReadLocker.h"
#include "Basics/WriteLocker.h"
#include "Basics/hashes.h"
#include "Basics/json.h"
#include "Basics/json-utilities.h"
#include "Basics/tri-strings.h"
#include "VocBase/vocbase.h"
#include "V8/v8-conv.h"
#include "V8/v8-utils.h"

using namespace arangodb;

struct KeySpaceElement {
  KeySpaceElement() = delete;

  KeySpaceElement(char const* k, size_t length, TRI_json_t* json)
      : key(nullptr), json(json) {
    key = TRI_DuplicateString(TRI_UNKNOWN_MEM_ZONE, k, length);
    if (key == nullptr) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
    }
  }

  ~KeySpaceElement() {
    if (key != nullptr) {
      TRI_FreeString(TRI_UNKNOWN_MEM_ZONE, key);
    }
    if (json != nullptr) {
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
    }
  }

  void setValue(TRI_json_t* value) {
    if (json != nullptr) {
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
      json = nullptr;
    }
    json = value;
  }

  char* key;
  TRI_json_t* json;
};

class KeySpace {
 public:
  explicit KeySpace(uint32_t initialSize) : _lock() {}

  ~KeySpace() {
    for (auto& it : _hash) {
      auto element = it.second;

      if (element != nullptr) {
        delete element;
      }
    }
  }

  uint32_t keyspaceCount() {
    READ_LOCKER(readLocker, _lock);
    return static_cast<uint32_t>(_hash.size());
  }

  uint32_t keyspaceCount(std::string const& prefix) {
    uint32_t count = 0;
    READ_LOCKER(readLocker, _lock);

    for (auto const& it : _hash) {
      auto data = it.second;

      if (data != nullptr) {
        if (TRI_IsPrefixString(data->key, prefix.c_str())) {
          ++count;
        }
      }
    }

    return count;
  }

  v8::Handle<v8::Value> keyspaceRemove(v8::Isolate* isolate) {
    v8::EscapableHandleScope scope(isolate);

    WRITE_LOCKER(writeLocker, _lock);
    uint32_t deleted = 0;
 
    for (auto& it : _hash) {
      auto element = it.second;

      if (element != nullptr) {
        delete element;
        ++deleted;
      } 
    }
    _hash.clear();

    return scope.Escape<v8::Value>(
        v8::Number::New(isolate, static_cast<int>(deleted)));
  }

  v8::Handle<v8::Value> keyspaceRemove(v8::Isolate* isolate,
                                       std::string const& prefix) {
    v8::EscapableHandleScope scope(isolate);
    WRITE_LOCKER(writeLocker, _lock);

    uint32_t deleted = 0;

    for (auto it = _hash.begin(); it != _hash.end(); /* no hoisting */) {
      auto element = (*it).second;

      if (element != nullptr) {
        if (TRI_IsPrefixString(element->key, prefix.c_str())) {
          it = _hash.erase(it);
          delete element;
          ++deleted;
          continue;
        }
      }
      ++it;
    }

    return scope.Escape<v8::Value>(
        v8::Number::New(isolate, static_cast<int>(deleted)));
  }

  v8::Handle<v8::Value> keyspaceKeys(v8::Isolate* isolate) {
    v8::EscapableHandleScope scope(isolate);
    v8::Handle<v8::Array> result;
    {
      READ_LOCKER(readLocker, _lock);

      uint32_t count = 0;
      result = v8::Array::New(isolate, static_cast<int>(_hash.size()));

      for (auto const& it : _hash) {
        auto element = it.second;

        if (element != nullptr) {
          result->Set(count++, TRI_V8_STRING(element->key));
        }
      }
    }

    return scope.Escape<v8::Value>(result);
  }

  v8::Handle<v8::Value> keyspaceKeys(v8::Isolate* isolate,
                                     std::string const& prefix) {
    v8::EscapableHandleScope scope(isolate);
    v8::Handle<v8::Array> result;
    {
      READ_LOCKER(readLocker, _lock);

      uint32_t count = 0;
      result = v8::Array::New(isolate);

      for (auto const& it : _hash) {
        auto element = it.second;

        if (element != nullptr) {
          if (TRI_IsPrefixString(element->key, prefix.c_str())) {
            result->Set(count++, TRI_V8_STRING(element->key));
          }
        }
      }
    }

    return scope.Escape<v8::Value>(result);
  }

  v8::Handle<v8::Value> keyspaceGet(v8::Isolate* isolate) {
    v8::EscapableHandleScope scope(isolate);
    v8::Handle<v8::Object> result = v8::Object::New(isolate);
    {
      READ_LOCKER(readLocker, _lock);

      for (auto const& it : _hash) {
        auto element = it.second;

        if (element != nullptr) {
          result->Set(TRI_V8_STRING(element->key),
                      TRI_ObjectJson(isolate, element->json));
        }
      }
    }

    return scope.Escape<v8::Value>(result);
  }

  v8::Handle<v8::Value> keyspaceGet(v8::Isolate* isolate,
                                    std::string const& prefix) {
    v8::EscapableHandleScope scope(isolate);
    v8::Handle<v8::Object> result = v8::Object::New(isolate);
    {
      READ_LOCKER(readLocker, _lock);

      for (auto const& it : _hash) {
        auto element = it.second;

        if (element != nullptr) {
          if (TRI_IsPrefixString(element->key, prefix.c_str())) {
            result->Set(TRI_V8_STRING(element->key),
                        TRI_ObjectJson(isolate, element->json));
          }
        }
      }
    }

    return scope.Escape<v8::Value>(result);
  }

  bool keyCount(std::string const& key, uint32_t& result) {
    READ_LOCKER(readLocker, _lock);

    auto it = _hash.find(key);
    
    if (it != _hash.end()) {
      TRI_json_t const* value = (*it).second->json;

      if (TRI_IsArrayJson(value)) {
        result =
            static_cast<uint32_t>(TRI_LengthVector(&value->_value._objects));
        return true;
      }
      if (TRI_IsObjectJson(value)) {
        result = static_cast<uint32_t>(
            TRI_LengthVector(&value->_value._objects) / 2);
        return true;
      }
    }

    result = 0;
    return false;
  }

  v8::Handle<v8::Value> keyGet(v8::Isolate* isolate, std::string const& key) {
    v8::Handle<v8::Value> result;
    {
      READ_LOCKER(readLocker, _lock);

      auto it = _hash.find(key);
      if (it == _hash.end()) {
        result = v8::Undefined(isolate);
      } else {
        result = TRI_ObjectJson(isolate, (*it).second->json);
      }
    }

    return result;
  }

  bool keySet(v8::Isolate* isolate, std::string const& key,
              v8::Handle<v8::Value> const& value, bool replace) {
    auto element = new KeySpaceElement(key.c_str(), key.size(),
                                       TRI_ObjectToJson(isolate, value));
    {
      WRITE_LOCKER(writeLocker, _lock);

      if (replace) {
        // delete previous entry
        auto it = _hash.find(key);
        if (it != _hash.end()) {
          delete (*it).second;
          _hash.erase(it);
        }
      }

      auto it = _hash.emplace(key, element);
      if (it.second) {
        return true;
      }
    }

    // insertion failed
    delete element;
    return false;
  }

  int keyCas(v8::Isolate* isolate, std::string const& key,
             v8::Handle<v8::Value> const& value,
             v8::Handle<v8::Value> const& compare, bool& match) {
    auto element = new KeySpaceElement(key.c_str(), key.size(),
                                       TRI_ObjectToJson(isolate, value));

    WRITE_LOCKER(writeLocker, _lock);
  
    auto it = _hash.emplace(key, element);
    if (it.second) {
      // no object saved yet
      match = true;

      return TRI_ERROR_NO_ERROR;
    }

    auto it2 = _hash.find(key);
    if (it2 == _hash.end()) {
      return TRI_ERROR_INTERNAL;
    }

    KeySpaceElement* found = (*it2).second;

    if (compare->IsUndefined()) {
      // other object saved, but we compare it with nothing => no match
      delete element;
      match = false;

      return TRI_ERROR_NO_ERROR;
    }

    TRI_json_t* other = TRI_ObjectToJson(isolate, compare);

    if (other == nullptr) {
      delete element;
      match = false;

      return TRI_ERROR_OUT_OF_MEMORY;
    }

    int res = TRI_CompareValuesJson(found->json, other);
    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, other);

    if (res != 0) {
      delete element;
      match = false;
    } else {
      auto it = _hash.find(key);
      if (it != _hash.end()) {
        auto found = (*it).second;
        _hash.erase(it);
        delete found;
      }
      _hash.emplace(key, element);
      match = true;
    }

    return TRI_ERROR_NO_ERROR;
  }

  bool keyRemove(std::string const& key) {
    KeySpaceElement* found = nullptr;

    {
      WRITE_LOCKER(writeLocker, _lock);

      auto it = _hash.find(key);
      if (it != _hash.end()) {
        found = (*it).second;
        _hash.erase(it);
      }
    }

    if (found != nullptr) {
      delete found;
      return true;
    }

    return false;
  }

  bool keyExists(std::string const& key) {
    READ_LOCKER(readLocker, _lock);

    return _hash.find(key) != _hash.end();
  }

  int keyIncr(std::string const& key, double value, double& result) {
    WRITE_LOCKER(writeLocker, _lock);

    KeySpaceElement* found = nullptr;
    auto it = _hash.find(key);
    if (it != _hash.end()) {
      found = (*it).second;
    }

    if (found == nullptr) {
      auto element = new KeySpaceElement(
          key.c_str(), key.size(),
          TRI_CreateNumberJson(TRI_UNKNOWN_MEM_ZONE, value));

      _hash.emplace(key, element);
      result = value;
    } else {
      TRI_json_t* current = found->json;

      if (!TRI_IsNumberJson(current)) {
        return TRI_ERROR_ILLEGAL_NUMBER;
      }

      result = current->_value._number += value;
    }

    return TRI_ERROR_NO_ERROR;
  }

  int keyPush(v8::Isolate* isolate, std::string const& key,
              v8::Handle<v8::Value> const& value) {
    WRITE_LOCKER(writeLocker, _lock);

    KeySpaceElement* found = nullptr;
    auto it = _hash.find(key);
    if (it != _hash.end()) {
      found = (*it).second;
    }

    if (found == nullptr) {
      TRI_json_t* list = TRI_CreateArrayJson(TRI_UNKNOWN_MEM_ZONE, 1);

      if (list == nullptr) {
        return TRI_ERROR_OUT_OF_MEMORY;
      }

      if (TRI_PushBack3ArrayJson(TRI_UNKNOWN_MEM_ZONE, list,
                                 TRI_ObjectToJson(isolate, value)) !=
          TRI_ERROR_NO_ERROR) {
        TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, list);
        return TRI_ERROR_OUT_OF_MEMORY;
      }

      auto element = new KeySpaceElement(key.c_str(), key.size(), list);

      _hash.emplace(key, element);
    } else {
      TRI_json_t* current = found->json;

      if (!TRI_IsArrayJson(current)) {
        return TRI_ERROR_INTERNAL;
      }

      if (TRI_PushBack3ArrayJson(TRI_UNKNOWN_MEM_ZONE, current,
                                 TRI_ObjectToJson(isolate, value)) !=
          TRI_ERROR_NO_ERROR) {
        return TRI_ERROR_OUT_OF_MEMORY;
      }
    }

    return TRI_ERROR_NO_ERROR;
  }

  void keyPop(v8::FunctionCallbackInfo<v8::Value> const& args,
              std::string const& key) {
    v8::Isolate* isolate = args.GetIsolate();
    v8::HandleScope scope(isolate);
    WRITE_LOCKER(writeLocker, _lock);

    KeySpaceElement* found = nullptr;
    auto it = _hash.find(key);
    if (it != _hash.end()) {
      found = (*it).second;
    }

    if (found == nullptr) {
      TRI_V8_THROW_EXCEPTION(TRI_ERROR_INTERNAL);
    }

    TRI_json_t* current = found->json;

    if (!TRI_IsArrayJson(current)) {
      TRI_V8_THROW_EXCEPTION(TRI_ERROR_INTERNAL);
    }

    size_t const n = TRI_LengthVector(&current->_value._objects);

    if (n == 0) {
      TRI_V8_RETURN_UNDEFINED();
    }

    TRI_json_t* item = static_cast<TRI_json_t*>(
        TRI_AtVector(&current->_value._objects, n - 1));
    // hack: decrease the vector size
    TRI_SetLengthVector(&current->_value._objects,
                        TRI_LengthVector(&current->_value._objects) - 1);

    v8::Handle<v8::Value> result = TRI_ObjectJson(isolate, item);
    TRI_DestroyJson(TRI_UNKNOWN_MEM_ZONE, item);

    TRI_V8_RETURN(result);
  }

  void keyTransfer(v8::FunctionCallbackInfo<v8::Value> const& args,
                   std::string const& keyFrom, std::string const& keyTo) {
    v8::Isolate* isolate = args.GetIsolate();
    v8::HandleScope scope(isolate);

    WRITE_LOCKER(writeLocker, _lock);

    KeySpaceElement* source = nullptr;
    auto it = _hash.find(keyFrom);
    if (it != _hash.end()) {
      source = (*it).second;
    }

    if (source == nullptr) {
      TRI_V8_RETURN_UNDEFINED();
    }

    TRI_json_t* current = source->json;

    if (!TRI_IsArrayJson(current)) {
      TRI_V8_THROW_EXCEPTION(TRI_ERROR_INTERNAL);
    }

    size_t const n = TRI_LengthVector(&source->json->_value._objects);

    if (n == 0) {
      TRI_V8_RETURN_UNDEFINED();
    }

    TRI_json_t* sourceItem = static_cast<TRI_json_t*>(
        TRI_AtVector(&source->json->_value._objects, n - 1));

    KeySpaceElement* dest = nullptr;
    auto it2 = _hash.find(keyTo);
    if (it2 != _hash.end()) {
      dest = (*it2).second;
    }

    if (dest == nullptr) {
      TRI_json_t* list = TRI_CreateArrayJson(TRI_UNKNOWN_MEM_ZONE, 1);

      if (list == nullptr) {
        TRI_V8_THROW_EXCEPTION_MEMORY();
      }

      TRI_PushBack2ArrayJson(list, sourceItem);

      try {
        auto element = new KeySpaceElement(keyTo.c_str(), keyTo.size(), list);
        _hash.emplace(keyTo, element);
        // hack: decrease the vector size
        TRI_SetLengthVector(&current->_value._objects,
                            TRI_LengthVector(&current->_value._objects) - 1);

        TRI_V8_RETURN(TRI_ObjectJson(isolate, sourceItem));
      } catch (...) {
        TRI_V8_THROW_EXCEPTION_MEMORY();
      }
    }

    TRI_ASSERT(dest != nullptr);

    if (!TRI_IsArrayJson(dest->json)) {
      TRI_V8_THROW_EXCEPTION(TRI_ERROR_INTERNAL);
    }

    TRI_PushBack2ArrayJson(dest->json, sourceItem);

    // hack: decrease the vector size
    TRI_SetLengthVector(&current->_value._objects,
                        TRI_LengthVector(&current->_value._objects) - 1);

    TRI_V8_RETURN(TRI_ObjectJson(isolate, sourceItem));
  }

  v8::Handle<v8::Value> keyKeys(v8::Isolate* isolate, std::string const& key) {
    v8::Handle<v8::Value> result;
    {
      READ_LOCKER(readLocker, _lock);

      auto it = _hash.find(key);
      if (it == _hash.end()) {
        result = v8::Undefined(isolate);
      } else {
        result = TRI_KeysJson(isolate, (*it).second->json);
      }
    }

    return result;
  }

  v8::Handle<v8::Value> keyValues(v8::Isolate* isolate,
                                  std::string const& key) {
    v8::EscapableHandleScope scope(isolate);
    v8::Handle<v8::Value> result;
    {
      READ_LOCKER(readLocker, _lock);

      auto it = _hash.find(key);
      if (it == _hash.end()) {
        result = v8::Undefined(isolate);
      } else {
        result = TRI_ValuesJson(isolate, (*it).second->json);
      }
    }

    return scope.Escape<v8::Value>(result);
  }

  void keyGetAt(v8::FunctionCallbackInfo<v8::Value> const& args,
                std::string const& key, int64_t index) {
    v8::Isolate* isolate = args.GetIsolate();
    v8::HandleScope scope(isolate);

    v8::Handle<v8::Value> result;
    {
      READ_LOCKER(readLocker, _lock);

      auto it = _hash.find(key);
      if (it == _hash.end()) {
        result = v8::Undefined(isolate);
      } else {
        KeySpaceElement* found = (*it).second;
        if (!TRI_IsArrayJson(found->json)) {
          TRI_V8_THROW_EXCEPTION(TRI_ERROR_INTERNAL);
        }

        size_t const n = TRI_LengthArrayJson(found->json);

        if (index < 0) {
          index = static_cast<int64_t>(n) + index;
        }

        if (index >= static_cast<int64_t>(n)) {
          result = v8::Undefined(isolate);
        } else {
          auto item = static_cast<TRI_json_t const*>(TRI_AtVector(
              &found->json->_value._objects, static_cast<size_t>(index)));
          result = TRI_ObjectJson(isolate, item);
        }
      }
    }

    TRI_V8_RETURN(result);
  }

  bool keySetAt(v8::Isolate* isolate, std::string const& key, int64_t index,
                v8::Handle<v8::Value> const& value) {
    WRITE_LOCKER(writeLocker, _lock);

    auto it = _hash.find(key);

    if (it == _hash.end()) {
      return false;
    } else {
      KeySpaceElement* found = (*it).second;
      if (!TRI_IsArrayJson(found->json)) {
        return false;
      }

      size_t const n = TRI_LengthArrayJson(found->json);

      if (index < 0) {
        return false;
      }

      auto json = TRI_ObjectToJson(isolate, value);
      if (json == nullptr) {
        return false;
      }

      if (index >= static_cast<int64_t>(n)) {
        // insert new element
        TRI_InsertVector(&found->json->_value._objects, json,
                         static_cast<size_t>(index));
      } else {
        // overwrite existing element
        auto item = static_cast<TRI_json_t*>(TRI_AtVector(
            &found->json->_value._objects, static_cast<size_t>(index)));
        if (item != nullptr) {
          TRI_DestroyJson(TRI_UNKNOWN_MEM_ZONE, item);
        }

        TRI_SetVector(&found->json->_value._objects, static_cast<size_t>(index),
                      json);
      }

      // only free pointer to json, but not its internal structures
      TRI_Free(TRI_UNKNOWN_MEM_ZONE, json);
    }

    return true;
  }

  char const* keyType(std::string const& key) {
    READ_LOCKER(readLocker, _lock);

    auto it = _hash.find(key);
    if (it != _hash.end()) {
      KeySpaceElement* found = (*it).second;
      TRI_json_t const* value = static_cast<KeySpaceElement*>(found)->json;

      switch (value->_type) {
        case TRI_JSON_NULL:
          return "null";
        case TRI_JSON_BOOLEAN:
          return "boolean";
        case TRI_JSON_NUMBER:
          return "number";
        case TRI_JSON_STRING:
        case TRI_JSON_STRING_REFERENCE:
          return "string";
        case TRI_JSON_ARRAY:
          return "list";
        case TRI_JSON_OBJECT:
          return "object";
        case TRI_JSON_UNUSED:
          break;
      }
    }

    return "undefined";
  }

  void keyMerge(v8::FunctionCallbackInfo<v8::Value> const& args,
                std::string const& key, v8::Handle<v8::Value> const& value,
                bool nullMeansRemove) {
    v8::Isolate* isolate = args.GetIsolate();
    v8::HandleScope scope(isolate);

    if (!value->IsObject() || value->IsArray()) {
      TRI_V8_THROW_EXCEPTION(TRI_ERROR_INTERNAL);
    }

    WRITE_LOCKER(writeLocker, _lock);

    auto it = _hash.find(key);
    if (it == _hash.end()) {
      auto element = new KeySpaceElement(key.c_str(), key.size(),
                                         TRI_ObjectToJson(isolate, value));
      _hash.emplace(key, element);

      TRI_V8_RETURN(value);
    }

    KeySpaceElement* found = (*it).second;

    if (!TRI_IsObjectJson(found->json)) {
      TRI_V8_THROW_EXCEPTION(TRI_ERROR_INTERNAL);
    }

    TRI_json_t* other = TRI_ObjectToJson(isolate, value);

    if (other == nullptr) {
      TRI_V8_THROW_EXCEPTION_MEMORY();
    }

    TRI_json_t* merged = TRI_MergeJson(TRI_UNKNOWN_MEM_ZONE, found->json, other,
                                       nullMeansRemove, false);
    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, other);

    if (merged == nullptr) {
      TRI_V8_THROW_EXCEPTION_MEMORY();
    }

    found->setValue(merged);

    TRI_V8_RETURN(TRI_ObjectJson(isolate, merged));
  }

 private:
  arangodb::basics::ReadWriteLock _lock;
  std::unordered_map<std::string, KeySpaceElement*> _hash;
};

struct UserStructures {
  struct {
    arangodb::basics::ReadWriteLock lock;
    std::unordered_map<std::string, KeySpace*> data;
  } hashes;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief get the vocbase pointer from the current V8 context
////////////////////////////////////////////////////////////////////////////////

static inline TRI_vocbase_t* GetContextVocBase(v8::Isolate* isolate) {
  TRI_GET_GLOBALS();

  TRI_ASSERT(v8g->_vocbase != nullptr);
  return static_cast<TRI_vocbase_t*>(v8g->_vocbase);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finds a hash array by name
/// note that at least the read-lock must be held to use this function
////////////////////////////////////////////////////////////////////////////////

static KeySpace* GetKeySpace(TRI_vocbase_t* vocbase, std::string const& name) {
  auto h = &(static_cast<UserStructures*>(vocbase->_userStructures)->hashes);
  auto it = h->data.find(name);

  if (it != h->data.end()) {
    return (*it).second;
  }
  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a keyspace
////////////////////////////////////////////////////////////////////////////////

static void JS_KeyspaceCreate(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() < 1 || !args[0]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "KEYSPACE_CREATE(<name>, <size>, <ignoreExisting>)");
  }

  TRI_vocbase_t* vocbase = GetContextVocBase(isolate);

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract vocbase");
  }

  std::string const&& name = TRI_ObjectToString(args[0]);
  int64_t size = 0;

  if (args.Length() > 1) {
    size = TRI_ObjectToInt64(args[1]);
    if (size < 0 || size > static_cast<decltype(size)>(UINT32_MAX)) {
      TRI_V8_THROW_EXCEPTION_PARAMETER("invalid value for <size>");
    }
  }

  bool ignoreExisting = false;
  if (args.Length() > 2) {
    ignoreExisting = TRI_ObjectToBoolean(args[2]);
  }

  auto ptr = std::make_unique<KeySpace>(static_cast<uint32_t>(size));

  auto h = &(static_cast<UserStructures*>(vocbase->_userStructures)->hashes);
  {
    WRITE_LOCKER(writeLocker, h->lock);

    auto hash = GetKeySpace(vocbase, name);

    if (hash != nullptr) {
      if (!ignoreExisting) {
        TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                       "hash already exists");
      }
      TRI_V8_RETURN_FALSE();
    }

    try {
      h->data.emplace(std::make_pair(name, ptr.get()));
      ptr.release();
    } catch (...) {
      TRI_V8_THROW_EXCEPTION_MEMORY();
    }
  }

  TRI_V8_RETURN_TRUE();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief drops a keyspace
////////////////////////////////////////////////////////////////////////////////

static void JS_KeyspaceDrop(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() != 1 || !args[0]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE("KEYSPACE_DROP(<name>)");
  }

  TRI_vocbase_t* vocbase = GetContextVocBase(isolate);

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract vocbase");
  }

  std::string const&& name = TRI_ObjectToString(args[0]);

  auto h = &(static_cast<UserStructures*>(vocbase->_userStructures)->hashes);
  {
    WRITE_LOCKER(writeLocker, h->lock);

    auto it = h->data.find(name);

    if (it == h->data.end()) {
      TRI_V8_THROW_EXCEPTION(TRI_ERROR_INTERNAL);
    }

    delete (*it).second;
    h->data.erase(it);
  }

  TRI_V8_RETURN_TRUE();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the number of items in the keyspace
////////////////////////////////////////////////////////////////////////////////

static void JS_KeyspaceCount(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() < 1 || !args[0]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE("KEYSPACE_COUNT(<name>, <prefix>)");
  }

  TRI_vocbase_t* vocbase = GetContextVocBase(isolate);

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract vocbase");
  }

  std::string const&& name = TRI_ObjectToString(args[0]);

  auto h = &(static_cast<UserStructures*>(vocbase->_userStructures)->hashes);
  uint32_t count;
  {
    READ_LOCKER(readLocker, h->lock);

    auto hash = GetKeySpace(vocbase, name);

    if (hash == nullptr) {
      TRI_V8_THROW_EXCEPTION(TRI_ERROR_INTERNAL);
    }

    if (args.Length() > 1) {
      std::string const&& prefix = TRI_ObjectToString(args[1]);
      count = hash->keyspaceCount(prefix);
    } else {
      count = hash->keyspaceCount();
    }
  }

  TRI_V8_RETURN(v8::Number::New(isolate, static_cast<int>(count)));
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns whether a keyspace exists
////////////////////////////////////////////////////////////////////////////////

static void JS_KeyspaceExists(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() != 1 || !args[0]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE("KEYSPACE_EXISTS(<name>)");
  }

  TRI_vocbase_t* vocbase = GetContextVocBase(isolate);

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract vocbase");
  }

  std::string const&& name = TRI_ObjectToString(args[0]);

  auto h = &(static_cast<UserStructures*>(vocbase->_userStructures)->hashes);

  READ_LOCKER(readLocker, h->lock);
  auto hash = GetKeySpace(vocbase, name);

  if (hash != nullptr) {
    TRI_V8_RETURN_TRUE();
  }
  TRI_V8_RETURN_FALSE();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns all keys of the keyspace
////////////////////////////////////////////////////////////////////////////////

static void JS_KeyspaceKeys(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() < 1 || !args[0]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE("KEYSPACE_KEYS(<name>, <prefix>)");
  }

  TRI_vocbase_t* vocbase = GetContextVocBase(isolate);

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract vocbase");
  }

  std::string const&& name = TRI_ObjectToString(args[0]);

  auto h = &(static_cast<UserStructures*>(vocbase->_userStructures)->hashes);
  READ_LOCKER(readLocker, h->lock);

  auto hash = GetKeySpace(vocbase, name);

  if (hash == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_INTERNAL);
  }

  if (args.Length() > 1) {
    std::string const&& prefix = TRI_ObjectToString(args[1]);
    TRI_V8_RETURN(hash->keyspaceKeys(isolate, prefix));
  }

  TRI_V8_RETURN(hash->keyspaceKeys(isolate));
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns all data of the keyspace
////////////////////////////////////////////////////////////////////////////////

static void JS_KeyspaceGet(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() < 1 || !args[0]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE("KEYSPACE_GET(<name>, <prefix>)");
  }

  TRI_vocbase_t* vocbase = GetContextVocBase(isolate);

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract vocbase");
  }

  std::string const&& name = TRI_ObjectToString(args[0]);

  auto h = &(static_cast<UserStructures*>(vocbase->_userStructures)->hashes);
  READ_LOCKER(readLocker, h->lock);

  auto hash = GetKeySpace(vocbase, name);

  if (hash == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_INTERNAL);
  }

  if (args.Length() > 1) {
    std::string const&& prefix = TRI_ObjectToString(args[1]);
    TRI_V8_RETURN(hash->keyspaceGet(isolate, prefix));
  }

  TRI_V8_RETURN(hash->keyspaceGet(isolate));
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes all keys from the keyspace
////////////////////////////////////////////////////////////////////////////////

static void JS_KeyspaceRemove(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() < 1 || !args[0]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE("KEYSPACE_REMOVE(<name>, <prefix>)");
  }

  TRI_vocbase_t* vocbase = GetContextVocBase(isolate);

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract vocbase");
  }

  std::string const&& name = TRI_ObjectToString(args[0]);

  auto h = &(static_cast<UserStructures*>(vocbase->_userStructures)->hashes);
  READ_LOCKER(readLocker, h->lock);

  auto hash = GetKeySpace(vocbase, name);

  if (hash == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_INTERNAL);
  }

  if (args.Length() > 1) {
    std::string const&& prefix = TRI_ObjectToString(args[1]);
    TRI_V8_RETURN(hash->keyspaceRemove(isolate, prefix));
  }

  TRI_V8_RETURN(hash->keyspaceRemove(isolate));
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the value for a key in the keyspace
////////////////////////////////////////////////////////////////////////////////

static void JS_KeyGet(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() < 2 || !args[0]->IsString() || !args[1]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE("KEY_GET(<name>, <key>)");
  }

  TRI_vocbase_t* vocbase = GetContextVocBase(isolate);

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract vocbase");
  }

  std::string const&& name = TRI_ObjectToString(args[0]);
  std::string const&& key = TRI_ObjectToString(args[1]);

  auto h = &(static_cast<UserStructures*>(vocbase->_userStructures)->hashes);
  v8::Handle<v8::Value> result;
  {
    READ_LOCKER(readLocker, h->lock);

    auto hash = GetKeySpace(vocbase, name);

    if (hash == nullptr) {
      TRI_V8_THROW_EXCEPTION(TRI_ERROR_INTERNAL);
    }

    result = hash->keyGet(isolate, key);
  }

  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set the value for a key in the keyspace
////////////////////////////////////////////////////////////////////////////////

static void JS_KeySet(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() < 3 || !args[0]->IsString() || !args[1]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE("KEY_SET(<name>, <key>, <value>, <replace>)");
  }

  TRI_vocbase_t* vocbase = GetContextVocBase(isolate);

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract vocbase");
  }

  std::string const&& name = TRI_ObjectToString(args[0]);
  std::string const&& key = TRI_ObjectToString(args[1]);
  bool replace = true;

  if (args.Length() > 3) {
    replace = TRI_ObjectToBoolean(args[3]);
  }

  auto h = &(static_cast<UserStructures*>(vocbase->_userStructures)->hashes);
  bool result;
  {
    READ_LOCKER(readLocker, h->lock);

    auto hash = GetKeySpace(vocbase, name);

    if (hash == nullptr) {
      TRI_V8_THROW_EXCEPTION(TRI_ERROR_INTERNAL);
    }

    result = hash->keySet(isolate, key, args[2], replace);
  }

  if (result) {
    TRI_V8_RETURN_TRUE();
  }
  TRI_V8_RETURN_FALSE();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief conditionally set the value for a key in the keyspace
////////////////////////////////////////////////////////////////////////////////

static void JS_KeySetCas(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() < 4 || !args[0]->IsString() || !args[1]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "KEY_SET_CAS(<name>, <key>, <value>, <compare>)");
  }

  TRI_vocbase_t* vocbase = GetContextVocBase(isolate);

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract vocbase");
  }

  std::string const&& name = TRI_ObjectToString(args[0]);
  std::string const&& key = TRI_ObjectToString(args[1]);

  if (args[2]->IsUndefined()) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_INTERNAL);
  }

  auto h = &(static_cast<UserStructures*>(vocbase->_userStructures)->hashes);
  int res;
  bool match = false;
  {
    READ_LOCKER(readLocker, h->lock);

    auto hash = GetKeySpace(vocbase, name);

    if (hash == nullptr) {
      TRI_V8_THROW_EXCEPTION(TRI_ERROR_INTERNAL);
    }

    res = hash->keyCas(isolate, key, args[2], args[3], match);
  }

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  if (match) {
    TRI_V8_RETURN_TRUE();
  }
  TRI_V8_RETURN_FALSE();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief remove the value for a key in the keyspace
////////////////////////////////////////////////////////////////////////////////

static void JS_KeyRemove(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() < 2 || !args[0]->IsString() || !args[1]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE("KEY_REMOVE(<name>, <key>)");
  }

  TRI_vocbase_t* vocbase = GetContextVocBase(isolate);

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract vocbase");
  }

  std::string const&& name = TRI_ObjectToString(args[0]);
  std::string const&& key = TRI_ObjectToString(args[1]);

  auto h = &(static_cast<UserStructures*>(vocbase->_userStructures)->hashes);
  bool result;
  {
    READ_LOCKER(readLocker, h->lock);

    auto hash = GetKeySpace(vocbase, name);

    if (hash == nullptr) {
      TRI_V8_THROW_EXCEPTION(TRI_ERROR_INTERNAL);
    }

    result = hash->keyRemove(key);
  }

  if (result) {
    TRI_V8_RETURN_TRUE();
  }
  TRI_V8_RETURN_FALSE();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if a key exists in the keyspace
////////////////////////////////////////////////////////////////////////////////

static void JS_KeyExists(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() < 2 || !args[0]->IsString() || !args[1]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE("KEY_EXISTS(<name>, <key>)");
  }

  TRI_vocbase_t* vocbase = GetContextVocBase(isolate);

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract vocbase");
  }

  std::string const&& name = TRI_ObjectToString(args[0]);
  std::string const&& key = TRI_ObjectToString(args[1]);

  auto h = &(static_cast<UserStructures*>(vocbase->_userStructures)->hashes);
  bool result;
  {
    READ_LOCKER(readLocker, h->lock);

    auto hash = GetKeySpace(vocbase, name);

    if (hash == nullptr) {
      TRI_V8_THROW_EXCEPTION(TRI_ERROR_INTERNAL);
    }

    result = hash->keyExists(key);
  }

  if (result) {
    TRI_V8_RETURN_TRUE();
  }
  TRI_V8_RETURN_FALSE();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief increase or decrease the value for a key in a keyspace
////////////////////////////////////////////////////////////////////////////////

static void JS_KeyIncr(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() < 2 || !args[0]->IsString() || !args[1]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE("KEY_INCR(<name>, <key>, <value>)");
  }

  if (args.Length() >= 3 && !args[2]->IsNumber()) {
    TRI_V8_THROW_EXCEPTION_USAGE("KEY_INCR(<name>, <key>, <value>)");
  }

  TRI_vocbase_t* vocbase = GetContextVocBase(isolate);

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract vocbase");
  }

  std::string const&& name = TRI_ObjectToString(args[0]);
  std::string const&& key = TRI_ObjectToString(args[1]);

  double incr = 1.0;

  if (args.Length() >= 3) {
    incr = TRI_ObjectToDouble(args[2]);
  }

  double result;
  auto h = &(static_cast<UserStructures*>(vocbase->_userStructures)->hashes);
  {
    READ_LOCKER(readLocker, h->lock);

    auto hash = GetKeySpace(vocbase, name);

    if (hash == nullptr) {
      TRI_V8_THROW_EXCEPTION(TRI_ERROR_INTERNAL);
    }

    int res = hash->keyIncr(key, incr, result);

    if (res != TRI_ERROR_NO_ERROR) {
      TRI_V8_THROW_EXCEPTION(res);
    }
  }

  TRI_V8_RETURN(v8::Number::New(isolate, result));
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief merges an object into the object with the specified key
////////////////////////////////////////////////////////////////////////////////

static void JS_KeyUpdate(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() < 3 || !args[0]->IsString() || !args[1]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "KEY_UPDATE(<name>, <key>, <object>, <nullMeansRemove>)");
  }

  TRI_vocbase_t* vocbase = GetContextVocBase(isolate);

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract vocbase");
  }

  std::string const&& name = TRI_ObjectToString(args[0]);
  std::string const&& key = TRI_ObjectToString(args[1]);

  bool nullMeansRemove = false;
  if (args.Length() > 3) {
    nullMeansRemove = TRI_ObjectToBoolean(args[3]);
  }

  auto h = &(static_cast<UserStructures*>(vocbase->_userStructures)->hashes);
  READ_LOCKER(readLocker, h->lock);

  auto hash = GetKeySpace(vocbase, name);

  if (hash == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_INTERNAL);
  }

  hash->keyMerge(args, key, args[2], nullMeansRemove);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns all keys of the key
////////////////////////////////////////////////////////////////////////////////

static void JS_KeyKeys(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() < 2 || !args[0]->IsString() || !args[1]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE("KEY_KEYS(<name>, <key>)");
  }

  TRI_vocbase_t* vocbase = GetContextVocBase(isolate);

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract vocbase");
  }

  std::string const&& name = TRI_ObjectToString(args[0]);
  std::string const&& key = TRI_ObjectToString(args[1]);

  auto h = &(static_cast<UserStructures*>(vocbase->_userStructures)->hashes);
  READ_LOCKER(readLocker, h->lock);

  auto hash = GetKeySpace(vocbase, name);

  if (hash == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_INTERNAL);
  }

  TRI_V8_RETURN(hash->keyKeys(isolate, key));
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns all value of the hash array
////////////////////////////////////////////////////////////////////////////////

static void JS_KeyValues(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() < 2 || !args[0]->IsString() || !args[1]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE("KEY_VALUES(<name>, <key>)");
  }

  TRI_vocbase_t* vocbase = GetContextVocBase(isolate);

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract vocbase");
  }

  std::string const&& name = TRI_ObjectToString(args[0]);
  std::string const&& key = TRI_ObjectToString(args[1]);

  auto h = &(static_cast<UserStructures*>(vocbase->_userStructures)->hashes);
  READ_LOCKER(readLocker, h->lock);

  auto hash = GetKeySpace(vocbase, name);

  if (hash == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_INTERNAL);
  }

  TRI_V8_RETURN(hash->keyValues(isolate, key));
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief right-pushes an element into a list value
////////////////////////////////////////////////////////////////////////////////

static void JS_KeyPush(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() < 3 || !args[0]->IsString() || !args[1]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE("KEY_PUSH(<name>, <key>, <value>)");
  }

  TRI_vocbase_t* vocbase = GetContextVocBase(isolate);

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract vocbase");
  }

  std::string const&& name = TRI_ObjectToString(args[0]);
  std::string const&& key = TRI_ObjectToString(args[1]);

  auto h = &(static_cast<UserStructures*>(vocbase->_userStructures)->hashes);
  READ_LOCKER(readLocker, h->lock);

  auto hash = GetKeySpace(vocbase, name);

  if (hash == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_INTERNAL);
  }

  int res = hash->keyPush(isolate, key, args[2]);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  TRI_V8_RETURN_TRUE();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief pops an element from a list value
////////////////////////////////////////////////////////////////////////////////

static void JS_KeyPop(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() < 2 || !args[0]->IsString() || !args[1]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE("KEY_POP(<name>, <key>)");
  }

  TRI_vocbase_t* vocbase = GetContextVocBase(isolate);

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract vocbase");
  }

  std::string const&& name = TRI_ObjectToString(args[0]);
  std::string const&& key = TRI_ObjectToString(args[1]);

  auto h = &(static_cast<UserStructures*>(vocbase->_userStructures)->hashes);
  READ_LOCKER(readLocker, h->lock);

  auto hash = GetKeySpace(vocbase, name);

  if (hash == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_INTERNAL);
  }

  hash->keyPop(args, key);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief transfer an element from a list value into another
////////////////////////////////////////////////////////////////////////////////

static void JS_KeyTransfer(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() < 3 || !args[0]->IsString() || !args[1]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE("KEY_TRANSFER(<name>, <key-from>, <key-to>)");
  }

  TRI_vocbase_t* vocbase = GetContextVocBase(isolate);

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract vocbase");
  }

  std::string const&& name = TRI_ObjectToString(args[0]);
  std::string const&& keyFrom = TRI_ObjectToString(args[1]);
  std::string const&& keyTo = TRI_ObjectToString(args[2]);

  auto h = &(static_cast<UserStructures*>(vocbase->_userStructures)->hashes);
  READ_LOCKER(readLocker, h->lock);

  auto hash = GetKeySpace(vocbase, name);

  if (hash == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_INTERNAL);
  }

  hash->keyTransfer(args, keyFrom, keyTo);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get an element at a specific list position
////////////////////////////////////////////////////////////////////////////////

static void JS_KeyGetAt(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() < 3 || !args[0]->IsString() || !args[1]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE("KEY_GET_AT(<name>, <key>, <index>)");
  }

  TRI_vocbase_t* vocbase = GetContextVocBase(isolate);

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract vocbase");
  }

  std::string const&& name = TRI_ObjectToString(args[0]);
  std::string const&& key = TRI_ObjectToString(args[1]);
  int64_t offset = TRI_ObjectToInt64(args[2]);

  auto h = &(static_cast<UserStructures*>(vocbase->_userStructures)->hashes);
  READ_LOCKER(readLocker, h->lock);

  auto hash = GetKeySpace(vocbase, name);

  if (hash == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_INTERNAL);
  }

  hash->keyGetAt(args, key, offset);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set an element at a specific list position
////////////////////////////////////////////////////////////////////////////////

static void JS_KeySetAt(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() < 4 || !args[0]->IsString() || !args[1]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE("KEY_SET_AT(<name>, <key>, <index>, <value>)");
  }

  TRI_vocbase_t* vocbase = GetContextVocBase(isolate);

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract vocbase");
  }

  std::string const&& name = TRI_ObjectToString(args[0]);
  std::string const&& key = TRI_ObjectToString(args[1]);
  int64_t offset = TRI_ObjectToInt64(args[2]);

  auto h = &(static_cast<UserStructures*>(vocbase->_userStructures)->hashes);
  READ_LOCKER(readLocker, h->lock);

  auto hash = GetKeySpace(vocbase, name);

  if (hash == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_INTERNAL);
  }

  int res = hash->keySetAt(isolate, key, offset, args[3]);
  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  TRI_V8_RETURN_TRUE();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the type of the value for a key
////////////////////////////////////////////////////////////////////////////////

static void JS_KeyType(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() < 2 || !args[0]->IsString() || !args[1]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE("KEY_TYPE(<name>, <key>)");
  }

  TRI_vocbase_t* vocbase = GetContextVocBase(isolate);

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract vocbase");
  }

  std::string const&& name = TRI_ObjectToString(args[0]);
  std::string const&& key = TRI_ObjectToString(args[1]);

  auto h = &(static_cast<UserStructures*>(vocbase->_userStructures)->hashes);
  char const* result;
  {
    READ_LOCKER(readLocker, h->lock);

    auto hash = GetKeySpace(vocbase, name);

    if (hash == nullptr) {
      TRI_V8_THROW_EXCEPTION(TRI_ERROR_INTERNAL);
    }

    result = hash->keyType(key);
  }

  TRI_V8_RETURN_STRING(result);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the number of items in a compound value
////////////////////////////////////////////////////////////////////////////////

static void JS_KeyCount(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() < 2 || !args[0]->IsString() || !args[1]->IsString()) {
    TRI_V8_THROW_EXCEPTION_USAGE("KEY_COUNT(<name>, <key>)");
  }

  TRI_vocbase_t* vocbase = GetContextVocBase(isolate);

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract vocbase");
  }

  std::string const&& name = TRI_ObjectToString(args[0]);
  std::string const&& key = TRI_ObjectToString(args[1]);

  auto h = &(static_cast<UserStructures*>(vocbase->_userStructures)->hashes);
  uint32_t result;
  bool valid;
  {
    READ_LOCKER(readLocker, h->lock);

    auto hash = GetKeySpace(vocbase, name);

    if (hash == nullptr) {
      TRI_V8_THROW_EXCEPTION(TRI_ERROR_INTERNAL);
    }

    valid = hash->keyCount(key, result);
  }

  if (valid) {
    TRI_V8_RETURN(v8::Number::New(isolate, result));
  }

  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates the user structures for a database
////////////////////////////////////////////////////////////////////////////////

void TRI_CreateUserStructuresVocBase(TRI_vocbase_t* vocbase) {
  TRI_ASSERT(vocbase != nullptr);
  TRI_ASSERT(vocbase->_userStructures == nullptr);

  vocbase->_userStructures = new UserStructures;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief drops the user structures for a database
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeUserStructuresVocBase(TRI_vocbase_t* vocbase) {
  if (vocbase->_userStructures != nullptr) {
    auto us = static_cast<UserStructures*>(vocbase->_userStructures);
    for (auto& hash : us->hashes.data) {
      if (hash.second != nullptr) {
        delete hash.second;
      }
    }
    delete us;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates the user structures functions
////////////////////////////////////////////////////////////////////////////////

void TRI_InitV8UserStructures(v8::Isolate* isolate,
                              v8::Handle<v8::Context> context) {
  v8::HandleScope scope(isolate);

  // NOTE: the following functions are all experimental and might
  // change without further notice
  TRI_AddGlobalFunctionVocbase(isolate, context,
                               TRI_V8_ASCII_STRING("KEYSPACE_CREATE"),
                               JS_KeyspaceCreate, true);
  TRI_AddGlobalFunctionVocbase(isolate, context,
                               TRI_V8_ASCII_STRING("KEYSPACE_DROP"),
                               JS_KeyspaceDrop, true);
  TRI_AddGlobalFunctionVocbase(isolate, context,
                               TRI_V8_ASCII_STRING("KEYSPACE_COUNT"),
                               JS_KeyspaceCount, true);
  TRI_AddGlobalFunctionVocbase(isolate, context,
                               TRI_V8_ASCII_STRING("KEYSPACE_EXISTS"),
                               JS_KeyspaceExists, true);
  TRI_AddGlobalFunctionVocbase(isolate, context,
                               TRI_V8_ASCII_STRING("KEYSPACE_KEYS"),
                               JS_KeyspaceKeys, true);
  TRI_AddGlobalFunctionVocbase(isolate, context,
                               TRI_V8_ASCII_STRING("KEYSPACE_REMOVE"),
                               JS_KeyspaceRemove, true);
  TRI_AddGlobalFunctionVocbase(isolate, context,
                               TRI_V8_ASCII_STRING("KEYSPACE_GET"),
                               JS_KeyspaceGet, true);

  TRI_AddGlobalFunctionVocbase(isolate, context, TRI_V8_ASCII_STRING("KEY_SET"),
                               JS_KeySet, true);
  TRI_AddGlobalFunctionVocbase(
      isolate, context, TRI_V8_ASCII_STRING("KEY_SET_CAS"), JS_KeySetCas, true);
  TRI_AddGlobalFunctionVocbase(isolate, context, TRI_V8_ASCII_STRING("KEY_GET"),
                               JS_KeyGet, true);
  TRI_AddGlobalFunctionVocbase(
      isolate, context, TRI_V8_ASCII_STRING("KEY_REMOVE"), JS_KeyRemove, true);
  TRI_AddGlobalFunctionVocbase(
      isolate, context, TRI_V8_ASCII_STRING("KEY_EXISTS"), JS_KeyExists, true);
  TRI_AddGlobalFunctionVocbase(
      isolate, context, TRI_V8_ASCII_STRING("KEY_TYPE"), JS_KeyType, true);

  // numeric functions
  TRI_AddGlobalFunctionVocbase(
      isolate, context, TRI_V8_ASCII_STRING("KEY_INCR"), JS_KeyIncr, true);

  // list / array functions
  TRI_AddGlobalFunctionVocbase(
      isolate, context, TRI_V8_ASCII_STRING("KEY_UPDATE"), JS_KeyUpdate, true);
  TRI_AddGlobalFunctionVocbase(
      isolate, context, TRI_V8_ASCII_STRING("KEY_KEYS"), JS_KeyKeys, true);
  TRI_AddGlobalFunctionVocbase(
      isolate, context, TRI_V8_ASCII_STRING("KEY_VALUES"), JS_KeyValues, true);
  TRI_AddGlobalFunctionVocbase(
      isolate, context, TRI_V8_ASCII_STRING("KEY_COUNT"), JS_KeyCount, true);
  TRI_AddGlobalFunctionVocbase(
      isolate, context, TRI_V8_ASCII_STRING("KEY_PUSH"), JS_KeyPush, true);
  TRI_AddGlobalFunctionVocbase(isolate, context, TRI_V8_ASCII_STRING("KEY_POP"),
                               JS_KeyPop, true);
  TRI_AddGlobalFunctionVocbase(isolate, context,
                               TRI_V8_ASCII_STRING("KEY_TRANSFER"),
                               JS_KeyTransfer, true);
  TRI_AddGlobalFunctionVocbase(
      isolate, context, TRI_V8_ASCII_STRING("KEY_GET_AT"), JS_KeyGetAt, true);
  TRI_AddGlobalFunctionVocbase(
      isolate, context, TRI_V8_ASCII_STRING("KEY_SET_AT"), JS_KeySetAt, true);
}
