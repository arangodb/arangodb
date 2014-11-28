////////////////////////////////////////////////////////////////////////////////
/// @brief V8 user data structures
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "v8-user-structures.h"
#include "Basics/ReadWriteLock.h"
#include "Basics/ReadLocker.h"
#include "Basics/WriteLocker.h"
#include "Basics/hashes.h"
#include "Basics/json.h"
#include "Basics/json-utilities.h"
#include "Basics/tri-strings.h"
#include "Utils/Exception.h"
#include "VocBase/vocbase.h"
#include "V8/v8-conv.h"
#include "V8/v8-utils.h"

// -----------------------------------------------------------------------------
// --SECTION--                                            struct KeySpaceElement
// -----------------------------------------------------------------------------

struct KeySpaceElement {
  KeySpaceElement () = delete;
  
  KeySpaceElement (char const* k,
                   size_t length,
                   TRI_json_t* json) 
    : key(nullptr),
      json(json) {

    key = TRI_DuplicateString2Z(TRI_UNKNOWN_MEM_ZONE, k, length);
    if (key == nullptr) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
    }
  }

  ~KeySpaceElement () {
    if (key != nullptr) {
      TRI_FreeString(TRI_UNKNOWN_MEM_ZONE, key);
    }
    if (json != nullptr) {
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
    }
  }

  void setValue (TRI_json_t* value) {
    if (json != nullptr) {
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
      json = nullptr;
    }
    json = value;
  }

  char*        key;
  TRI_json_t*  json;
};

// -----------------------------------------------------------------------------
// --SECTION--                                                    class KeySpace
// -----------------------------------------------------------------------------

class KeySpace {
  public:
    KeySpace (uint32_t initialSize) 
      : _lock() {

      TRI_InitAssociativePointer(&_hash, 
                                 TRI_UNKNOWN_MEM_ZONE,
                                 TRI_HashStringKeyAssociativePointer,
                                 HashHash,
                                 EqualHash,
                                 nullptr);

      if (initialSize > 0) {
        TRI_ReserveAssociativePointer(&_hash, initialSize);
      }
    } 

    ~KeySpace () {
      uint32_t const n = _hash._nrAlloc;
      for (uint32_t i = 0; i < n; ++i) {
        auto element = static_cast<KeySpaceElement*>(_hash._table[i]);

        if (element != nullptr) {
          delete element;
        }
      }
      TRI_DestroyAssociativePointer(&_hash);
    }

    static uint64_t HashHash (TRI_associative_pointer_t*,
                              void const* element) {
      return TRI_FnvHashString(static_cast<KeySpaceElement const*>(element)->key);
    }

    static bool EqualHash (TRI_associative_pointer_t*,
                           void const* key,
                           void const* element) {
      return TRI_EqualString(static_cast<char const*>(key), static_cast<KeySpaceElement const*>(element)->key);
    }

    uint32_t keyspaceCount () {
      READ_LOCKER(_lock);
      return _hash._nrUsed;
    }

    uint32_t keyspaceCount (std::string const& prefix) {
      uint32_t count = 0;
      READ_LOCKER(_lock);

      uint32_t const n = _hash._nrAlloc;
      for (uint32_t i = 0; i < n; ++i) {
        auto data = static_cast<KeySpaceElement*>(_hash._table[i]);

        if (data != nullptr) {
          if (TRI_IsPrefixString(data->key, prefix.c_str())) {
            ++count;
          }
        }
      }

      return count;
    }

    v8::Handle<v8::Value> keyspaceRemove () {
      v8::HandleScope scope;

      WRITE_LOCKER(_lock);

      uint32_t const n = _hash._nrAlloc;
      uint32_t deleted = 0;

      for (uint32_t i = 0; i < n; ++i) {
        auto element = static_cast<KeySpaceElement*>(_hash._table[i]);

        if (element != nullptr) {
          delete element;
          _hash._table[i] = nullptr;
          ++deleted;
        }
      }
      _hash._nrUsed = 0;

      return scope.Close(v8::Number::New(static_cast<int>(deleted))); 
    }

    v8::Handle<v8::Value> keyspaceRemove (std::string const& prefix) {
      v8::HandleScope scope;

      WRITE_LOCKER(_lock);

      uint32_t const n = _hash._nrAlloc;
      uint32_t i = 0;
      uint32_t deleted = 0;

      while (i < n) {
        auto element = static_cast<KeySpaceElement*>(_hash._table[i]);

        if (element != nullptr) {
          if (TRI_IsPrefixString(element->key, prefix.c_str())) {
            if (TRI_RemoveKeyAssociativePointer(&_hash, element->key) != nullptr) {
              delete element;
              ++deleted;
              continue;
            }
          }
        }
        ++i;
      }

      return scope.Close(v8::Number::New(static_cast<int>(deleted))); 
    }

    v8::Handle<v8::Value> keyspaceKeys () {
      v8::HandleScope scope;

      v8::Handle<v8::Array> result;
      {
        READ_LOCKER(_lock);

        uint32_t const n = _hash._nrAlloc;
        uint32_t count = 0;
        result = v8::Array::New(static_cast<int>(_hash._nrUsed));

        for (uint32_t i = 0; i < n; ++i) {
          auto element = static_cast<KeySpaceElement*>(_hash._table[i]);

          if (element != nullptr) {
            result->Set(count++, v8::String::New(element->key));
          }
        }
      }

      return scope.Close(result); 
    }

    v8::Handle<v8::Value> keyspaceKeys (std::string const& prefix) {
      v8::HandleScope scope;

      v8::Handle<v8::Array> result;
      {
        READ_LOCKER(_lock);

        uint32_t const n = _hash._nrAlloc;
        uint32_t count = 0;
        result = v8::Array::New();

        for (uint32_t i = 0; i < n; ++i) {
          auto element = static_cast<KeySpaceElement*>(_hash._table[i]);

          if (element != nullptr) {
            if (TRI_IsPrefixString(element->key, prefix.c_str())) {
              result->Set(count++, v8::String::New(element->key));
            }
          }
        }
      }

      return scope.Close(result); 
    }

    v8::Handle<v8::Value> keyspaceGet () {
      v8::HandleScope scope;

      v8::Handle<v8::Object> result = v8::Object::New();
      {
        READ_LOCKER(_lock);

        uint32_t const n = _hash._nrAlloc;

        for (uint32_t i = 0; i < n; ++i) {
          auto element = static_cast<KeySpaceElement*>(_hash._table[i]);

          if (element != nullptr) {
            result->Set(v8::String::New(element->key), TRI_ObjectJson(element->json));
          }
        }
      }

      return scope.Close(result); 
    }

    v8::Handle<v8::Value> keyspaceGet (std::string const& prefix) {
      v8::HandleScope scope;

      v8::Handle<v8::Object> result = v8::Object::New();
      {
        READ_LOCKER(_lock);

        uint32_t const n = _hash._nrAlloc;

        for (uint32_t i = 0; i < n; ++i) {
          auto element = static_cast<KeySpaceElement*>(_hash._table[i]);

          if (element != nullptr) {
            if (TRI_IsPrefixString(element->key, prefix.c_str())) {
              result->Set(v8::String::New(element->key), TRI_ObjectJson(element->json));
            }
          }
        }
      }

      return scope.Close(result); 
    }



    bool keyCount (std::string const& key, 
                   uint32_t& result) {
      READ_LOCKER(_lock);

      auto found = static_cast<KeySpaceElement*>(TRI_LookupByKeyAssociativePointer(&_hash, key.c_str()));

      if (found != nullptr) {
        TRI_json_t const* value = found->json;

        if (TRI_IsListJson(value)) {
          result = static_cast<uint32_t>(TRI_LengthVector(&value->_value._objects));
          return true;
        }
        if (TRI_IsArrayJson(value)) {
          result = static_cast<uint32_t>(TRI_LengthVector(&value->_value._objects) / 2);
          return true;
        }
      }

      result = 0;
      return false;
    }

    v8::Handle<v8::Value> keyGet (std::string const& key) {
      v8::HandleScope scope;

      v8::Handle<v8::Value> result;
      {
        READ_LOCKER(_lock);

        auto found = static_cast<KeySpaceElement*>(TRI_LookupByKeyAssociativePointer(&_hash, key.c_str()));

        if (found == nullptr) {
          result = v8::Undefined();
        }
        else {
          result = TRI_ObjectJson(found->json);
        }
      }

      return scope.Close(result); 
    }

    bool keySet (std::string const& key,
                 v8::Handle<v8::Value> const& value,
                 bool replace) {
      auto element = new KeySpaceElement(key.c_str(), key.size(), TRI_ObjectToJson(value));
      KeySpaceElement* found = nullptr;

      {
        WRITE_LOCKER(_lock);
 
        found = static_cast<KeySpaceElement*>(TRI_InsertKeyAssociativePointer(&_hash, element->key, element, replace));
      }
   
      if (found == nullptr) {
        return true;
      }
        
      if (replace) {
        delete found;
        return true;
      }
        
      delete element;
      return false;
    }

    int keyCas (std::string const& key,
                v8::Handle<v8::Value> const& value,
                v8::Handle<v8::Value> const& compare,
                bool& match) {
      auto element = new KeySpaceElement(key.c_str(), key.size(), TRI_ObjectToJson(value));

      WRITE_LOCKER(_lock);
 
      auto found = static_cast<KeySpaceElement*>(TRI_InsertKeyAssociativePointer(&_hash, element->key, element, false));
   
      if (compare->IsUndefined()) {
        if (found == nullptr) {
          // no object saved yet
          TRI_InsertKeyAssociativePointer(&_hash, element->key, element, false);
          match = true;
        }
        else {
          match = false;
        }
        return TRI_ERROR_NO_ERROR;
      }

      TRI_json_t* other = TRI_ObjectToJson(compare);

      if (other == nullptr) {
        delete element;
        // TODO: fix error message
        return TRI_ERROR_OUT_OF_MEMORY;
      }
        
      int res = TRI_CompareValuesJson(found->json, other);
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, other); 

      if (res != 0) {
        delete element;
        match = false;
      }
      else {
        TRI_InsertKeyAssociativePointer(&_hash, element->key, element, true);
        delete found;
        match = true;
      }

      return TRI_ERROR_NO_ERROR;
    }

    bool keyRemove (std::string const& key) {
      KeySpaceElement* found = nullptr;

      {
        WRITE_LOCKER(_lock);

        found = static_cast<KeySpaceElement*>(TRI_RemoveKeyAssociativePointer(&_hash, key.c_str()));
      }

      if (found != nullptr) {
        delete found;
        return true;
      }

      return false;
    }
 
    bool keyExists (std::string const& key) {
      READ_LOCKER(_lock);

      return (TRI_LookupByKeyAssociativePointer(&_hash, key.c_str()) != nullptr);
    }
 
    int keyIncr (std::string const& key,
                 double value,
                 double& result) {
      WRITE_LOCKER(_lock);

      auto found = static_cast<KeySpaceElement*>(TRI_LookupByKeyAssociativePointer(&_hash, key.c_str()));

      if (found == nullptr) {
        auto element = new KeySpaceElement(key.c_str(), key.size(), TRI_CreateNumberJson(TRI_UNKNOWN_MEM_ZONE, value));

        if (TRI_InsertKeyAssociativePointer(&_hash, element->key, static_cast<void*>(element), false) != TRI_ERROR_NO_ERROR) {
          delete element;
          return TRI_ERROR_OUT_OF_MEMORY;
        }
        result = value;
      }
      else {
        TRI_json_t* current = found->json;

        if (! TRI_IsNumberJson(current)) {
          // TODO: change error code
          return TRI_ERROR_ILLEGAL_NUMBER;
        }

        result = current->_value._number += value;
      }

      return TRI_ERROR_NO_ERROR;
    }

    int keyPush (std::string const& key,
                 v8::Handle<v8::Value> const& value) {
      WRITE_LOCKER(_lock);

      auto found = static_cast<KeySpaceElement*>(TRI_LookupByKeyAssociativePointer(&_hash, key.c_str()));

      if (found == nullptr) {
        TRI_json_t* list = TRI_CreateList2Json(TRI_UNKNOWN_MEM_ZONE, 1);

        if (list == nullptr) {
          return TRI_ERROR_OUT_OF_MEMORY;
        }

        if (TRI_PushBack3ListJson(TRI_UNKNOWN_MEM_ZONE, list, TRI_ObjectToJson(value)) != TRI_ERROR_NO_ERROR) {
          TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, list);
          return TRI_ERROR_OUT_OF_MEMORY;
        }

        auto element = new KeySpaceElement(key.c_str(), key.size(), list);

        if (TRI_InsertKeyAssociativePointer(&_hash, element->key, static_cast<void*>(element), false) != TRI_ERROR_NO_ERROR) {
          delete element;
          return TRI_ERROR_OUT_OF_MEMORY;
        }
      }
      else {
        TRI_json_t* current = found->json;

        if (! TRI_IsListJson(current)) {
          // TODO: change error code
          return TRI_ERROR_INTERNAL;
        }

        if (TRI_PushBack3ListJson(TRI_UNKNOWN_MEM_ZONE, current, TRI_ObjectToJson(value)) != TRI_ERROR_NO_ERROR) {
          return TRI_ERROR_OUT_OF_MEMORY;
        }
      }

      return TRI_ERROR_NO_ERROR;
    }

    v8::Handle<v8::Value> keyPop (std::string const& key) {
      v8::HandleScope scope;

      WRITE_LOCKER(_lock);

      auto found = static_cast<KeySpaceElement*>(TRI_LookupByKeyAssociativePointer(&_hash, key.c_str()));

      if (found == nullptr) {
        // TODO: change error code
        TRI_V8_EXCEPTION(scope, TRI_ERROR_INTERNAL); 
      }
        
      TRI_json_t* current = found->json;

      if (! TRI_IsListJson(current)) {
        // TODO: change error code
        TRI_V8_EXCEPTION(scope, TRI_ERROR_INTERNAL); 
      }

      size_t const n = TRI_LengthVector(&current->_value._objects);

      if (n == 0) {
        return scope.Close(v8::Undefined());
      }

      TRI_json_t* item = static_cast<TRI_json_t*>(TRI_AtVector(&current->_value._objects, n - 1));
      // hack: decrease the vector size
      --current->_value._objects._length;

      v8::Handle<v8::Value> result = TRI_ObjectJson(item);
      TRI_DestroyJson(TRI_UNKNOWN_MEM_ZONE, item);

      return scope.Close(result);
    }

    v8::Handle<v8::Value> keyTransfer (std::string const& keyFrom,
                                       std::string const& keyTo) {
      v8::HandleScope scope;

      WRITE_LOCKER(_lock);

      auto source = static_cast<KeySpaceElement*>(TRI_LookupByKeyAssociativePointer(&_hash, keyFrom.c_str()));

      if (source == nullptr) {
        return scope.Close(v8::Undefined());
      }
        
      TRI_json_t* current = source->json;

      if (! TRI_IsListJson(current)) {
        // TODO: change error code
        TRI_V8_EXCEPTION(scope, TRI_ERROR_INTERNAL); 
      }

      size_t const n = TRI_LengthVector(&source->json->_value._objects);

      if (n == 0) {
        return scope.Close(v8::Undefined());
      }

      TRI_json_t* sourceItem = static_cast<TRI_json_t*>(TRI_AtVector(&source->json->_value._objects, n - 1));

      auto dest = static_cast<KeySpaceElement*>(TRI_LookupByKeyAssociativePointer(&_hash, keyTo.c_str()));

      if (dest == nullptr) {
        TRI_json_t* list = TRI_CreateList2Json(TRI_UNKNOWN_MEM_ZONE, 1);

        if (list == nullptr) {
          TRI_V8_EXCEPTION_MEMORY(scope);
        }

        TRI_PushBack2ListJson(list, sourceItem);
 
        try {
          auto element = new KeySpaceElement(keyTo.c_str(), keyTo.size(), list);
          TRI_InsertKeyAssociativePointer(&_hash, element->key, element, false);
          // hack: decrease the vector size
          --current->_value._objects._length;
        
          return TRI_ObjectJson(sourceItem);
        }
        catch (...) {
          TRI_V8_EXCEPTION_MEMORY(scope);
        }
      }

      if (! TRI_IsListJson(dest->json)) {
        // TODO: change error code
        TRI_V8_EXCEPTION(scope, TRI_ERROR_INTERNAL); 
      }

      TRI_PushBack2ListJson(dest->json, sourceItem);

      // hack: decrease the vector size
      --current->_value._objects._length;

      return TRI_ObjectJson(sourceItem);
    }

    v8::Handle<v8::Value> keyKeys (std::string const& key) {
      v8::HandleScope scope;

      v8::Handle<v8::Value> result;
      {
        READ_LOCKER(_lock);

        auto found = static_cast<KeySpaceElement*>(TRI_LookupByKeyAssociativePointer(&_hash, key.c_str()));

        if (found == nullptr) {
          result = v8::Undefined();
        }
        else {
          result = TRI_KeysJson(found->json);
        }
      }

      return scope.Close(result); 
    }

    v8::Handle<v8::Value> keyValues (std::string const& key) {
      v8::HandleScope scope;

      v8::Handle<v8::Value> result;
      {
        READ_LOCKER(_lock);

        auto found = static_cast<KeySpaceElement*>(TRI_LookupByKeyAssociativePointer(&_hash, key.c_str()));

        if (found == nullptr) {
          result = v8::Undefined();
        }
        else {
          result = TRI_ValuesJson(found->json);
        }
      }

      return scope.Close(result); 
    }

    v8::Handle<v8::Value> keyGetAt (std::string const& key,
                                    int64_t index) {
      v8::HandleScope scope;

      v8::Handle<v8::Value> result;
      {
        READ_LOCKER(_lock);

        auto found = static_cast<KeySpaceElement*>(TRI_LookupByKeyAssociativePointer(&_hash, key.c_str()));

        if (found == nullptr) {
          result = v8::Undefined();
        }
        else {
          if (! TRI_IsListJson(found->json)) {
            // TODO: change error code
            TRI_V8_EXCEPTION(scope, TRI_ERROR_INTERNAL); 
          }

          size_t const n = found->json->_value._objects._length;
          if (index < 0) {
            index = static_cast<int64_t>(n) + index;
          }

          if (index >= static_cast<int64_t>(n)) {
            result = v8::Undefined();
          }
          else {
            auto item = static_cast<TRI_json_t const*>(TRI_AtVector(&found->json->_value._objects, static_cast<size_t>(index)));
            result = TRI_ObjectJson(item);
          }
        }
      }

      return scope.Close(result); 
    }
    
    bool keySetAt (std::string const& key,
                   int64_t index,
                   v8::Handle<v8::Value> const& value) {
      WRITE_LOCKER(_lock);

      auto found = static_cast<KeySpaceElement*>(TRI_LookupByKeyAssociativePointer(&_hash, key.c_str()));

      if (found == nullptr) {
        // TODO: change error code
        return false;
      }
      else {
        if (! TRI_IsListJson(found->json)) {
          // TODO: change error code
          return false;
        }

        size_t const n = found->json->_value._objects._length;
        if (index < 0) {
          // TODO: change error code
          return false;
        }

        auto json = TRI_ObjectToJson(value);
        if (json == nullptr) {
          // TODO: change error code
          return false;
        }

        if (index >= static_cast<int64_t>(n)) {
          // insert new element
          TRI_InsertVector(&found->json->_value._objects, json, static_cast<size_t>(index)); 
        }
        else {
          // overwrite existing element
          auto item = static_cast<TRI_json_t*>(TRI_AtVector(&found->json->_value._objects, static_cast<size_t>(index)));
          if (item != nullptr) {
            TRI_DestroyJson(TRI_UNKNOWN_MEM_ZONE, item);
          }

          TRI_SetVector(&found->json->_value._objects, static_cast<size_t>(index), json); 
        }

        // only free pointer to json, but not its internal structures
        TRI_Free(TRI_UNKNOWN_MEM_ZONE, json);
      }

      return true; 
    }

    char const* keyType (std::string const& key) {
      READ_LOCKER(_lock);

      void* found = TRI_LookupByKeyAssociativePointer(&_hash, key.c_str());

      if (found != nullptr) {
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
          case TRI_JSON_LIST:
            return "list";
          case TRI_JSON_ARRAY:
            return "object";
          case TRI_JSON_UNUSED:
            break;
        }
      }

      return "undefined";
    }

    v8::Handle<v8::Value> keyMerge (std::string const& key,
                                    v8::Handle<v8::Value> const& value,
                                    bool nullMeansRemove) {
      v8::HandleScope scope;

      if (! value->IsObject() || value->IsArray()) {
        // TODO: change error code
        TRI_V8_EXCEPTION(scope, TRI_ERROR_INTERNAL);
      }

      WRITE_LOCKER(_lock);
 
      auto found = static_cast<KeySpaceElement*>(TRI_LookupByKeyAssociativePointer(&_hash, key.c_str()));
   
      if (found == nullptr) {
        auto element = new KeySpaceElement(key.c_str(), key.size(), TRI_ObjectToJson(value));
        TRI_InsertKeyAssociativePointer(&_hash, element->key, element, false);

        return scope.Close(value);
      }

      if (! TRI_IsArrayJson(found->json)) {
        // TODO: change error code
        TRI_V8_EXCEPTION(scope, TRI_ERROR_INTERNAL);
      }

      TRI_json_t* other = TRI_ObjectToJson(value);

      if (other == nullptr) {
        TRI_V8_EXCEPTION(scope, TRI_ERROR_OUT_OF_MEMORY);
      }

      TRI_json_t* merged = TRI_MergeJson(TRI_UNKNOWN_MEM_ZONE, found->json, other, nullMeansRemove, false); 
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, other);

      if (merged == nullptr) {
        TRI_V8_EXCEPTION(scope, TRI_ERROR_OUT_OF_MEMORY);
      }

      found->setValue(merged);
 
      return scope.Close(TRI_ObjectJson(merged));
    }

  private:
        
    triagens::basics::ReadWriteLock _lock; 
    TRI_associative_pointer_t       _hash;
   
};

// -----------------------------------------------------------------------------
// --SECTION--                                             struct UserStructures
// -----------------------------------------------------------------------------

struct UserStructures {
  struct {
    triagens::basics::ReadWriteLock              lock; 
    std::unordered_map<std::string, KeySpace*>   data;
  }
  hashes;
};

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief get the vocbase pointer from the current V8 context
////////////////////////////////////////////////////////////////////////////////

static inline TRI_vocbase_t* GetContextVocBase () {
  TRI_v8_global_t* v8g = static_cast<TRI_v8_global_t*>(v8::Isolate::GetCurrent()->GetData());

  TRI_ASSERT_EXPENSIVE(v8g->_vocbase != nullptr);
  return static_cast<TRI_vocbase_t*>(v8g->_vocbase);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finds a hash array by name
/// note that at least the read-lock must be held to use this function
////////////////////////////////////////////////////////////////////////////////

static KeySpace* GetKeySpace (TRI_vocbase_t* vocbase,
                                std::string const& name) {
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

static v8::Handle<v8::Value> JS_KeyspaceCreate (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() < 1 || ! argv[0]->IsString()) {
    TRI_V8_EXCEPTION_USAGE(scope, "KEYSPACE_CREATE(<name>, <size>, <ignoreExisting>)");
  }

  TRI_vocbase_t* vocbase = GetContextVocBase();

  if (vocbase == nullptr) {
    TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract vocbase");
  }

  std::string const&& name = TRI_ObjectToString(argv[0]);
  int64_t size = 0;

  if (argv.Length() > 1) {
    size = TRI_ObjectToInt64(argv[1]);
    if (size < 0 || size > static_cast<decltype(size)>(UINT32_MAX)) {
      TRI_V8_EXCEPTION_PARAMETER(scope, "invalid value for <size>");
    }
  }

  bool ignoreExisting = false;
  if (argv.Length() > 2) {
    ignoreExisting = TRI_ObjectToBoolean(argv[2]);
  }

  std::unique_ptr<KeySpace> ptr(new KeySpace(static_cast<uint32_t>(size)));

  auto h = &(static_cast<UserStructures*>(vocbase->_userStructures)->hashes);
  {
    WRITE_LOCKER(h->lock);

    auto hash = GetKeySpace(vocbase, name);

    if (hash != nullptr) {
      if (! ignoreExisting) {
        // TODO: change error code
        TRI_V8_EXCEPTION_MESSAGE(scope, TRI_ERROR_INTERNAL, "hash already exists");
      }
    }
   
    try { 
      h->data.emplace(std::make_pair(name, ptr.release()));
    }
    catch (...) {
      TRI_V8_EXCEPTION_MEMORY(scope);
    }
  }

  return scope.Close(v8::True());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief drops a keyspace
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_KeyspaceDrop (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 1 || ! argv[0]->IsString()) {
    TRI_V8_EXCEPTION_USAGE(scope, "KEYSPACE_DROP(<name>)");
  }

  TRI_vocbase_t* vocbase = GetContextVocBase();

  if (vocbase == nullptr) {
    TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract vocbase");
  }

  std::string const&& name = TRI_ObjectToString(argv[0]);

  auto h = &(static_cast<UserStructures*>(vocbase->_userStructures)->hashes);
  {
    WRITE_LOCKER(h->lock);

    auto it = h->data.find(name);

    if (it == h->data.end()) {
      // TODO: change error code
      TRI_V8_EXCEPTION(scope, TRI_ERROR_INTERNAL);
    }

    delete (*it).second;
    h->data.erase(it);
  }

  return scope.Close(v8::True());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the number of items in the keyspace
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_KeyspaceCount (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() < 1 || ! argv[0]->IsString()) {
    TRI_V8_EXCEPTION_USAGE(scope, "KEYSPACE_COUNT(<name>, <prefix>)");
  }

  TRI_vocbase_t* vocbase = GetContextVocBase();

  if (vocbase == nullptr) {
    TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract vocbase");
  }

  std::string const&& name = TRI_ObjectToString(argv[0]);

  auto h = &(static_cast<UserStructures*>(vocbase->_userStructures)->hashes);
  uint32_t count; 
  {
    READ_LOCKER(h->lock);

    auto hash = GetKeySpace(vocbase, name);

    if (hash == nullptr) {
      // TODO: change error code
      TRI_V8_EXCEPTION(scope, TRI_ERROR_INTERNAL);
    }
  
    if (argv.Length() > 1) {
      std::string const&& prefix = TRI_ObjectToString(argv[1]);
      count = hash->keyspaceCount(prefix);
    }
    else {
      count = hash->keyspaceCount();
    }
  }

  return scope.Close(v8::Number::New(static_cast<int>(count)));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns whether a keyspace exists
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_KeyspaceExists (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 1 || ! argv[0]->IsString()) {
    TRI_V8_EXCEPTION_USAGE(scope, "KEYSPACE_EXISTS(<name>)");
  }

  TRI_vocbase_t* vocbase = GetContextVocBase();

  if (vocbase == nullptr) {
    TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract vocbase");
  }

  std::string const&& name = TRI_ObjectToString(argv[0]);

  auto h = &(static_cast<UserStructures*>(vocbase->_userStructures)->hashes);
   
  READ_LOCKER(h->lock);
  auto hash = GetKeySpace(vocbase, name);

  return scope.Close(v8::Boolean::New(hash != nullptr));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns all keys of the keyspace
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_KeyspaceKeys (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() < 1 || ! argv[0]->IsString()) {
    TRI_V8_EXCEPTION_USAGE(scope, "KEYSPACE_KEYS(<name>, <prefix>)");
  }

  TRI_vocbase_t* vocbase = GetContextVocBase();

  if (vocbase == nullptr) {
    TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract vocbase");
  }

  std::string const&& name = TRI_ObjectToString(argv[0]);

  auto h = &(static_cast<UserStructures*>(vocbase->_userStructures)->hashes);
  READ_LOCKER(h->lock);

  auto hash = GetKeySpace(vocbase, name);

  if (hash == nullptr) {
    // TODO: change error code
    TRI_V8_EXCEPTION(scope, TRI_ERROR_INTERNAL);
  }

  if (argv.Length() > 1) {
    std::string const&& prefix = TRI_ObjectToString(argv[1]);
    return scope.Close(hash->keyspaceKeys(prefix));
  }

  return scope.Close(hash->keyspaceKeys());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns all data of the keyspace
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_KeyspaceGet (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() < 1 || ! argv[0]->IsString()) {
    TRI_V8_EXCEPTION_USAGE(scope, "KEYSPACE_GET(<name>, <prefix>)");
  }

  TRI_vocbase_t* vocbase = GetContextVocBase();

  if (vocbase == nullptr) {
    TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract vocbase");
  }

  std::string const&& name = TRI_ObjectToString(argv[0]);

  auto h = &(static_cast<UserStructures*>(vocbase->_userStructures)->hashes);
  READ_LOCKER(h->lock);

  auto hash = GetKeySpace(vocbase, name);

  if (hash == nullptr) {
    // TODO: change error code
    TRI_V8_EXCEPTION(scope, TRI_ERROR_INTERNAL);
  }

  if (argv.Length() > 1) {
    std::string const&& prefix = TRI_ObjectToString(argv[1]);
    return scope.Close(hash->keyspaceGet(prefix));
  }

  return scope.Close(hash->keyspaceGet());
}


////////////////////////////////////////////////////////////////////////////////
/// @brief removes all keys from the keyspace
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_KeyspaceRemove (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() < 1 || ! argv[0]->IsString()) {
    TRI_V8_EXCEPTION_USAGE(scope, "KEYSPACE_REMOVE(<name>, <prefix>)");
  }

  TRI_vocbase_t* vocbase = GetContextVocBase();

  if (vocbase == nullptr) {
    TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract vocbase");
  }

  std::string const&& name = TRI_ObjectToString(argv[0]);

  auto h = &(static_cast<UserStructures*>(vocbase->_userStructures)->hashes);
  READ_LOCKER(h->lock);

  auto hash = GetKeySpace(vocbase, name);

  if (hash == nullptr) {
    // TODO: change error code
    TRI_V8_EXCEPTION(scope, TRI_ERROR_INTERNAL);
  }

  if (argv.Length() > 1) {
    std::string const&& prefix = TRI_ObjectToString(argv[1]);
    return scope.Close(hash->keyspaceRemove(prefix));
  }

  return scope.Close(hash->keyspaceRemove());
}


////////////////////////////////////////////////////////////////////////////////
/// @brief returns the value for a key in the keyspace
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_KeyGet (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() < 2 || ! argv[0]->IsString() || ! argv[1]->IsString()) {
    TRI_V8_EXCEPTION_USAGE(scope, "KEY_GET(<name>, <key>)");
  }

  TRI_vocbase_t* vocbase = GetContextVocBase();

  if (vocbase == nullptr) {
    TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract vocbase");
  }

  std::string const&& name = TRI_ObjectToString(argv[0]);
  std::string const&& key  = TRI_ObjectToString(argv[1]);

  auto h = &(static_cast<UserStructures*>(vocbase->_userStructures)->hashes);
  v8::Handle<v8::Value> result;
  {
    READ_LOCKER(h->lock);

    auto hash = GetKeySpace(vocbase, name);

    if (hash == nullptr) {
      // TODO: change error code
      TRI_V8_EXCEPTION(scope, TRI_ERROR_INTERNAL);
    }

    result = hash->keyGet(key);
  }

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set the value for a key in the keyspace
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_KeySet (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() < 3 || ! argv[0]->IsString() || ! argv[1]->IsString()) {
    TRI_V8_EXCEPTION_USAGE(scope, "KEY_SET(<name>, <key>, <value>, <replace>)");
  }

  TRI_vocbase_t* vocbase = GetContextVocBase();

  if (vocbase == nullptr) {
    TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract vocbase");
  }

  std::string const&& name = TRI_ObjectToString(argv[0]);
  std::string const&& key  = TRI_ObjectToString(argv[1]);
  bool replace = true;

  if (argv.Length() > 3) {
    replace = TRI_ObjectToBoolean(argv[3]);
  }

  auto h = &(static_cast<UserStructures*>(vocbase->_userStructures)->hashes);
  bool result;
  {
    READ_LOCKER(h->lock);

    auto hash = GetKeySpace(vocbase, name);

    if (hash == nullptr) {
      // TODO: change error code
      TRI_V8_EXCEPTION(scope, TRI_ERROR_INTERNAL);
    }

    result = hash->keySet(key, argv[2], replace);
  }

  return scope.Close(result ? v8::True() : v8::False());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief conditionally set the value for a key in the keyspace
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_KeySetCas (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() < 4 || ! argv[0]->IsString() || ! argv[1]->IsString()) {
    TRI_V8_EXCEPTION_USAGE(scope, "KEY_SET_CAS(<name>, <key>, <value>, <compare>)");
  }

  TRI_vocbase_t* vocbase = GetContextVocBase();

  if (vocbase == nullptr) {
    TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract vocbase");
  }

  std::string const&& name = TRI_ObjectToString(argv[0]);
  std::string const&& key  = TRI_ObjectToString(argv[1]);

  if (argv[2]->IsUndefined()) {
    // TODO: change error code
    TRI_V8_EXCEPTION(scope, TRI_ERROR_INTERNAL);
  }

  auto h = &(static_cast<UserStructures*>(vocbase->_userStructures)->hashes);
  int res;
  bool match = false;
  {
    READ_LOCKER(h->lock);

    auto hash = GetKeySpace(vocbase, name);

    if (hash == nullptr) {
      // TODO: change error code
      TRI_V8_EXCEPTION(scope, TRI_ERROR_INTERNAL);
    }

    res = hash->keyCas(key, argv[2], argv[3], match);
  }

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_EXCEPTION(scope, res);
  }

  return scope.Close(v8::Boolean::New(match));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief remove the value for a key in the keyspace
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_KeyRemove (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() < 2 || ! argv[0]->IsString() || ! argv[1]->IsString()) {
    TRI_V8_EXCEPTION_USAGE(scope, "KEY_REMOVE(<name>, <key>)");
  }

  TRI_vocbase_t* vocbase = GetContextVocBase();

  if (vocbase == nullptr) {
    TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract vocbase");
  }
  
  std::string const&& name = TRI_ObjectToString(argv[0]);
  std::string const&& key  = TRI_ObjectToString(argv[1]);
 
  auto h = &(static_cast<UserStructures*>(vocbase->_userStructures)->hashes);
  bool result; 
  {
    READ_LOCKER(h->lock);

    auto hash = GetKeySpace(vocbase, name);

    if (hash == nullptr) {
      // TODO: change error code
      TRI_V8_EXCEPTION(scope, TRI_ERROR_INTERNAL);
    }

    result = hash->keyRemove(key);
  }

  return v8::Boolean::New(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if a key exists in the keyspace
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_KeyExists (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() < 2 || ! argv[0]->IsString() || ! argv[1]->IsString()) {
    TRI_V8_EXCEPTION_USAGE(scope, "KEY_EXISTS(<name>, <key>)");
  }

  TRI_vocbase_t* vocbase = GetContextVocBase();

  if (vocbase == nullptr) {
    TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract vocbase");
  }
  
  std::string const&& name = TRI_ObjectToString(argv[0]);
  std::string const&& key  = TRI_ObjectToString(argv[1]);
 
  auto h = &(static_cast<UserStructures*>(vocbase->_userStructures)->hashes);
  bool result; 
  {
    READ_LOCKER(h->lock);

    auto hash = GetKeySpace(vocbase, name);

    if (hash == nullptr) {
      // TODO: change error code
      TRI_V8_EXCEPTION(scope, TRI_ERROR_INTERNAL);
    }

    result = hash->keyExists(key);
  }

  return v8::Boolean::New(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief increase or decrease the value for a key in a keyspace
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_KeyIncr (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() < 2 || ! argv[0]->IsString() || ! argv[1]->IsString()) {
    TRI_V8_EXCEPTION_USAGE(scope, "KEY_INCR(<name>, <key>, <value>)");
  }

  if (argv.Length() >= 3 && ! argv[2]->IsNumber()) {
    TRI_V8_EXCEPTION_USAGE(scope, "KEY_INCR(<name>, <key>, <value>)");
  }

  TRI_vocbase_t* vocbase = GetContextVocBase();

  if (vocbase == nullptr) {
    TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract vocbase");
  }

  std::string const&& name = TRI_ObjectToString(argv[0]);
  std::string const&& key  = TRI_ObjectToString(argv[1]);

  double incr = 1.0;

  if (argv.Length() >= 3) {
    incr = TRI_ObjectToDouble(argv[2]);
  }

  double result;
  auto h = &(static_cast<UserStructures*>(vocbase->_userStructures)->hashes);
  {
    READ_LOCKER(h->lock);

    auto hash = GetKeySpace(vocbase, name);

    if (hash == nullptr) {
      // TODO: change error code
      TRI_V8_EXCEPTION(scope, TRI_ERROR_INTERNAL);
    }

    int res = hash->keyIncr(key, incr, result);

    if (res != TRI_ERROR_NO_ERROR) {
      TRI_V8_EXCEPTION(scope, res);
    }
  }

  return scope.Close(v8::Number::New(result));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief merges an object into the object with the specified key
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_KeyUpdate (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() < 3 || ! argv[0]->IsString() || ! argv[1]->IsString()) {
    TRI_V8_EXCEPTION_USAGE(scope, "KEY_UPDATE(<name>, <key>, <object>, <nullMeansRemove>)");
  }

  TRI_vocbase_t* vocbase = GetContextVocBase();

  if (vocbase == nullptr) {
    TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract vocbase");
  }

  std::string const&& name = TRI_ObjectToString(argv[0]);
  std::string const&& key  = TRI_ObjectToString(argv[1]);

  bool nullMeansRemove = false;
  if (argv.Length() > 3) {
    nullMeansRemove = TRI_ObjectToBoolean(argv[3]);
  }

  auto h = &(static_cast<UserStructures*>(vocbase->_userStructures)->hashes);
  READ_LOCKER(h->lock);

  auto hash = GetKeySpace(vocbase, name);

  if (hash == nullptr) {
    // TODO: change error code
    TRI_V8_EXCEPTION(scope, TRI_ERROR_INTERNAL);
  }

  return scope.Close(hash->keyMerge(key, argv[2], nullMeansRemove));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns all keys of the key
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_KeyKeys (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() < 2 || ! argv[0]->IsString() || ! argv[1]->IsString()) {
    TRI_V8_EXCEPTION_USAGE(scope, "KEY_KEYS(<name>, <key>)");
  }

  TRI_vocbase_t* vocbase = GetContextVocBase();

  if (vocbase == nullptr) {
    TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract vocbase");
  }

  std::string const&& name = TRI_ObjectToString(argv[0]);
  std::string const&& key  = TRI_ObjectToString(argv[1]);

  auto h = &(static_cast<UserStructures*>(vocbase->_userStructures)->hashes);
  READ_LOCKER(h->lock);

  auto hash = GetKeySpace(vocbase, name);

  if (hash == nullptr) {
    // TODO: change error code
    TRI_V8_EXCEPTION(scope, TRI_ERROR_INTERNAL);
  }

  return scope.Close(hash->keyKeys(key));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns all value of the hash array
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_KeyValues (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() < 2 || ! argv[0]->IsString() || ! argv[1]->IsString()) {
    TRI_V8_EXCEPTION_USAGE(scope, "KEY_VALUES(<name>, <key>)");
  }

  TRI_vocbase_t* vocbase = GetContextVocBase();

  if (vocbase == nullptr) {
    TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract vocbase");
  }

  std::string const&& name = TRI_ObjectToString(argv[0]);
  std::string const&& key  = TRI_ObjectToString(argv[1]);

  auto h = &(static_cast<UserStructures*>(vocbase->_userStructures)->hashes);
  READ_LOCKER(h->lock);

  auto hash = GetKeySpace(vocbase, name);

  if (hash == nullptr) {
    // TODO: change error code
    TRI_V8_EXCEPTION(scope, TRI_ERROR_INTERNAL);
  }

  return scope.Close(hash->keyValues(key));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief right-pushes an element into a list value
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_KeyPush (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() < 3 || ! argv[0]->IsString() || ! argv[1]->IsString()) {
    TRI_V8_EXCEPTION_USAGE(scope, "KEY_PUSH(<name>, <key>, <value>)");
  }

  TRI_vocbase_t* vocbase = GetContextVocBase();

  if (vocbase == nullptr) {
    TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract vocbase");
  }

  std::string const&& name = TRI_ObjectToString(argv[0]);
  std::string const&& key  = TRI_ObjectToString(argv[1]);

  auto h = &(static_cast<UserStructures*>(vocbase->_userStructures)->hashes);
  READ_LOCKER(h->lock);

  auto hash = GetKeySpace(vocbase, name);

  if (hash == nullptr) {
    // TODO: change error code
    TRI_V8_EXCEPTION(scope, TRI_ERROR_INTERNAL);
  }

  int res = hash->keyPush(key, argv[2]);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_EXCEPTION(scope, res);
  }

  return scope.Close(v8::True());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief pops an element from a list value
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_KeyPop (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() < 2 || ! argv[0]->IsString() || ! argv[1]->IsString()) {
    TRI_V8_EXCEPTION_USAGE(scope, "KEY_POP(<name>, <key>)");
  }

  TRI_vocbase_t* vocbase = GetContextVocBase();

  if (vocbase == nullptr) {
    TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract vocbase");
  }

  std::string const&& name = TRI_ObjectToString(argv[0]);
  std::string const&& key  = TRI_ObjectToString(argv[1]);

  auto h = &(static_cast<UserStructures*>(vocbase->_userStructures)->hashes);
  READ_LOCKER(h->lock);

  auto hash = GetKeySpace(vocbase, name);

  if (hash == nullptr) {
    // TODO: change error code
    TRI_V8_EXCEPTION(scope, TRI_ERROR_INTERNAL);
  }

  return scope.Close(hash->keyPop(key));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief transfer an element from a list value into another
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_KeyTransfer (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() < 3 || ! argv[0]->IsString() || ! argv[1]->IsString()) {
    TRI_V8_EXCEPTION_USAGE(scope, "KEY_TRANSFER(<name>, <key-from>, <key-to>)");
  }

  TRI_vocbase_t* vocbase = GetContextVocBase();

  if (vocbase == nullptr) {
    TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract vocbase");
  }

  std::string const&& name    = TRI_ObjectToString(argv[0]);
  std::string const&& keyFrom = TRI_ObjectToString(argv[1]);
  std::string const&& keyTo   = TRI_ObjectToString(argv[2]);

  auto h = &(static_cast<UserStructures*>(vocbase->_userStructures)->hashes);
  READ_LOCKER(h->lock);

  auto hash = GetKeySpace(vocbase, name);

  if (hash == nullptr) {
    // TODO: change error code
    TRI_V8_EXCEPTION(scope, TRI_ERROR_INTERNAL);
  }

  return scope.Close(hash->keyTransfer(keyFrom, keyTo));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get an element at a specific list position
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_KeyGetAt (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() < 3 || ! argv[0]->IsString() || ! argv[1]->IsString()) {
    TRI_V8_EXCEPTION_USAGE(scope, "KEY_GET_AT(<name>, <key>, <index>)");
  }

  TRI_vocbase_t* vocbase = GetContextVocBase();

  if (vocbase == nullptr) {
    TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract vocbase");
  }

  std::string const&& name = TRI_ObjectToString(argv[0]);
  std::string const&& key  = TRI_ObjectToString(argv[1]);
  int64_t offset = TRI_ObjectToInt64(argv[2]);

  auto h = &(static_cast<UserStructures*>(vocbase->_userStructures)->hashes);
  READ_LOCKER(h->lock);

  auto hash = GetKeySpace(vocbase, name);

  if (hash == nullptr) {
    // TODO: change error code
    TRI_V8_EXCEPTION(scope, TRI_ERROR_INTERNAL);
  }

  return scope.Close(hash->keyGetAt(key, offset));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set an element at a specific list position
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_KeySetAt (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() < 4 || ! argv[0]->IsString() || ! argv[1]->IsString()) {
    TRI_V8_EXCEPTION_USAGE(scope, "KEY_SET_AT(<name>, <key>, <index>, <value>)");
  }

  TRI_vocbase_t* vocbase = GetContextVocBase();

  if (vocbase == nullptr) {
    TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract vocbase");
  }

  std::string const&& name = TRI_ObjectToString(argv[0]);
  std::string const&& key  = TRI_ObjectToString(argv[1]);
  int64_t offset = TRI_ObjectToInt64(argv[2]);

  auto h = &(static_cast<UserStructures*>(vocbase->_userStructures)->hashes);
  READ_LOCKER(h->lock);

  auto hash = GetKeySpace(vocbase, name);

  if (hash == nullptr) {
    // TODO: change error code
    TRI_V8_EXCEPTION(scope, TRI_ERROR_INTERNAL);
  }

  int res = hash->keySetAt(key, offset, argv[3]);
  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_EXCEPTION(scope, res);
  }

  return scope.Close(v8::True());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the type of the value for a key
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_KeyType (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() < 2 || ! argv[0]->IsString() || ! argv[1]->IsString()) {
    TRI_V8_EXCEPTION_USAGE(scope, "KEY_TYPE(<name>, <key>)");
  }

  TRI_vocbase_t* vocbase = GetContextVocBase();

  if (vocbase == nullptr) {
    TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract vocbase");
  }
  
  std::string const&& name = TRI_ObjectToString(argv[0]);
  std::string const&& key  = TRI_ObjectToString(argv[1]);
 
  auto h = &(static_cast<UserStructures*>(vocbase->_userStructures)->hashes);
  char const* result; 
  {
    READ_LOCKER(h->lock);

    auto hash = GetKeySpace(vocbase, name);

    if (hash == nullptr) {
      // TODO: change error code
      TRI_V8_EXCEPTION(scope, TRI_ERROR_INTERNAL);
    }

    result = hash->keyType(key);
  }

  return scope.Close(v8::String::New(result));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the number of items in a compound value
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_KeyCount (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() < 2 || ! argv[0]->IsString() || ! argv[1]->IsString()) {
    TRI_V8_EXCEPTION_USAGE(scope, "KEY_COUNT(<name>, <key>)");
  }

  TRI_vocbase_t* vocbase = GetContextVocBase();

  if (vocbase == nullptr) {
    TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract vocbase");
  }
  
  std::string const&& name = TRI_ObjectToString(argv[0]);
  std::string const&& key  = TRI_ObjectToString(argv[1]);
 
  auto h = &(static_cast<UserStructures*>(vocbase->_userStructures)->hashes);
  uint32_t result; 
  bool valid;
  {
    READ_LOCKER(h->lock);

    auto hash = GetKeySpace(vocbase, name);

    if (hash == nullptr) {
      // TODO: change error code
      TRI_V8_EXCEPTION(scope, TRI_ERROR_INTERNAL);
    }

    valid = hash->keyCount(key, result);
  }

  if (valid) {
    return scope.Close(v8::Number::New(result));
  }

  return scope.Close(v8::Undefined());
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief creates the user structures for a database
////////////////////////////////////////////////////////////////////////////////

void TRI_CreateUserStructuresVocBase (TRI_vocbase_t* vocbase) {
  TRI_ASSERT(vocbase != nullptr);
  TRI_ASSERT(vocbase->_userStructures == nullptr);

  vocbase->_userStructures = new UserStructures;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief drops the user structures for a database
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeUserStructuresVocBase (TRI_vocbase_t* vocbase) {
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

void TRI_InitV8UserStructures (v8::Handle<v8::Context> context) {
  v8::HandleScope scope;

  // NOTE: the following functions are all experimental and might 
  // change without further notice
  TRI_AddGlobalFunctionVocbase(context, "KEYSPACE_CREATE", JS_KeyspaceCreate, true);
  TRI_AddGlobalFunctionVocbase(context, "KEYSPACE_DROP", JS_KeyspaceDrop, true);
  TRI_AddGlobalFunctionVocbase(context, "KEYSPACE_COUNT", JS_KeyspaceCount, true);
  TRI_AddGlobalFunctionVocbase(context, "KEYSPACE_EXISTS", JS_KeyspaceExists, true);
  TRI_AddGlobalFunctionVocbase(context, "KEYSPACE_KEYS", JS_KeyspaceKeys, true);
  TRI_AddGlobalFunctionVocbase(context, "KEYSPACE_REMOVE", JS_KeyspaceRemove, true);
  TRI_AddGlobalFunctionVocbase(context, "KEYSPACE_GET", JS_KeyspaceGet, true);

  TRI_AddGlobalFunctionVocbase(context, "KEY_SET", JS_KeySet, true);
  TRI_AddGlobalFunctionVocbase(context, "KEY_SET_CAS", JS_KeySetCas, true);
  TRI_AddGlobalFunctionVocbase(context, "KEY_GET", JS_KeyGet, true);
  TRI_AddGlobalFunctionVocbase(context, "KEY_REMOVE", JS_KeyRemove, true);
  TRI_AddGlobalFunctionVocbase(context, "KEY_EXISTS", JS_KeyExists, true);
  TRI_AddGlobalFunctionVocbase(context, "KEY_TYPE", JS_KeyType, true);
  
  // numeric functions
  TRI_AddGlobalFunctionVocbase(context, "KEY_INCR", JS_KeyIncr, true);
  
  // list / array functions
  TRI_AddGlobalFunctionVocbase(context, "KEY_UPDATE", JS_KeyUpdate, true);
  TRI_AddGlobalFunctionVocbase(context, "KEY_KEYS", JS_KeyKeys, true);
  TRI_AddGlobalFunctionVocbase(context, "KEY_VALUES", JS_KeyValues, true);
  TRI_AddGlobalFunctionVocbase(context, "KEY_COUNT", JS_KeyCount, true);
  TRI_AddGlobalFunctionVocbase(context, "KEY_PUSH", JS_KeyPush, true);
  TRI_AddGlobalFunctionVocbase(context, "KEY_POP", JS_KeyPop, true);
  TRI_AddGlobalFunctionVocbase(context, "KEY_TRANSFER", JS_KeyTransfer, true);
  TRI_AddGlobalFunctionVocbase(context, "KEY_GET_AT", JS_KeyGetAt, true);
  TRI_AddGlobalFunctionVocbase(context, "KEY_SET_AT", JS_KeySetAt, true);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
