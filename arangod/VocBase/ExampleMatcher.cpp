////////////////////////////////////////////////////////////////////////////////
/// @brief Class to allow simple byExample matching of mptr.
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014-2015 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
/// @author Copyright 2014-2015, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "ExampleMatcher.h"
#include "V8Server/v8-vocbaseprivate.h"
#include "voc-shaper.h"
#include "V8/v8-utils.h"
#include "V8/v8-conv.h"
#include "Basics/JsonHelper.h"
#include "Basics/StringUtils.h"
#include "Utils/V8ResolverGuard.h"

using namespace std;
using namespace triagens::arango;
using namespace triagens::basics;


////////////////////////////////////////////////////////////////////////////////
/// @brief cleans up the example object
////////////////////////////////////////////////////////////////////////////////

void ExampleMatcher::cleanup () {
  auto zone = _shaper->_memoryZone;
  // clean shaped json objects
  for (auto& def : definitions) {
    for (auto& it : def._values) {
      TRI_FreeShapedJson(zone, it);
    }
  }
}

void ExampleMatcher::fillExampleDefinition (v8::Isolate* isolate,
                                            v8::Handle<v8::Object> const& example,
                                            v8::Handle<v8::Array> const& names,
                                            size_t& n,
                                            std::string& errorMessage,
                                            ExampleDefinition& def) {
  def._pids.reserve(n);
  def._values.reserve(n);

  for (size_t i = 0;  i < n;  ++i) {
    v8::Handle<v8::Value> key = names->Get((uint32_t) i);
    v8::Handle<v8::Value> val = example->Get(key);
    TRI_Utf8ValueNFC keyStr(TRI_UNKNOWN_MEM_ZONE, key);
    if (*keyStr != nullptr) {
      auto pid = _shaper->lookupAttributePathByName(_shaper, *keyStr);

      if (pid == 0) {
        // Internal attributes do have pid == 0.
        if (strncmp("_", *keyStr, 1) == 0) {
          string const key(*keyStr, (size_t) keyStr.length());
          string keyVal = TRI_ObjectToString(val);
          if (TRI_VOC_ATTRIBUTE_KEY == key) {
            def._internal.insert(make_pair(internalAttr::key, DocumentId(0, keyVal)));
          } 
          else if (TRI_VOC_ATTRIBUTE_REV == key) {
            def._internal.insert(make_pair(internalAttr::rev, DocumentId(0, keyVal)));
          } 
          else {
            // We need a Collection Name Resolver here now!
            TRI_vocbase_t* vocbase = GetContextVocBase(isolate);
            TRI_IF_FAILURE("ExampleNoContextVocbase") {
              // Explicitly delete the vocbase
              vocbase = nullptr;
            }
            if (vocbase == nullptr) {
              // This should never be thrown as we are already in a transaction
              THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
            }
            V8ResolverGuard resolverGuard(vocbase);
            CollectionNameResolver const* resolver = resolverGuard.getResolver();
            string colName = keyVal.substr(0, keyVal.find("/"));
            keyVal = keyVal.substr(keyVal.find("/") + 1, keyVal.length());
            if (TRI_VOC_ATTRIBUTE_ID == key) {
              def._internal.insert(make_pair(internalAttr::id, DocumentId(resolver->getCollectionId(colName), keyVal)));
            } 
            else if (TRI_VOC_ATTRIBUTE_FROM == key) {
              def._internal.insert(make_pair(internalAttr::from, DocumentId(resolver->getCollectionId(colName), keyVal)));
            } 
            else if (TRI_VOC_ATTRIBUTE_TO == key) {
              def._internal.insert(make_pair(internalAttr::to, DocumentId(resolver->getCollectionId(colName), keyVal)));
            } 
            else {
              // no attribute path found. this means the result will be empty
              THROW_ARANGO_EXCEPTION(TRI_RESULT_ELEMENT_NOT_FOUND);
            }
          }
        } 
        else {
          // no attribute path found. this means the result will be empty
          THROW_ARANGO_EXCEPTION(TRI_RESULT_ELEMENT_NOT_FOUND);
        }
      } 
      else {
        def._pids.push_back(pid);

        auto value = TRI_ShapedJsonV8Object(isolate, val, _shaper, false);

        if (value == nullptr) {
          THROW_ARANGO_EXCEPTION(TRI_RESULT_ELEMENT_NOT_FOUND);
        }
        def._values.push_back(value);
      }
    }
    else {
      errorMessage = "cannot convert attribute path to UTF8";
      THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
    }
  }
}

void ExampleMatcher::fillExampleDefinition (TRI_json_t const* example,
                                            CollectionNameResolver const* resolver,
                                            ExampleDefinition& def) {

  if ( TRI_IsStringJson(example) ) {
    // Example is an _id value
    char const* _key = strchr(example->_value._string.data, '/');
    if (_key != nullptr) {
      _key += 1;
      def._internal.insert(make_pair(internalAttr::key, DocumentId(0, _key)));
      return;
    } else {
      THROW_ARANGO_EXCEPTION(TRI_RESULT_ELEMENT_NOT_FOUND);
    }
  }
  TRI_vector_t objects = example->_value._objects;

  // Trolololol std::vector in C... ;(
  size_t n = TRI_LengthVector(&objects); 

  def._pids.reserve(n);
  def._values.reserve(n);

  try { 
    for (size_t i = 0; i < n; i += 2) {
      auto keyObj = static_cast<TRI_json_t const*>(TRI_AtVector(&objects, i));
      TRI_ASSERT(TRI_IsStringJson(keyObj));
      char const* keyStr = keyObj->_value._string.data;
      auto pid = _shaper->lookupAttributePathByName(_shaper, keyStr);

      if (pid == 0) {
        // Internal attributes do have pid == 0.
        if (strncmp("_", keyStr, 1) == 0) {
          string const key(keyStr);
          auto jsonValue = static_cast<TRI_json_t const*>(TRI_AtVector(&objects, i + 1));
          if (! TRI_IsStringJson(jsonValue)) {
            THROW_ARANGO_EXCEPTION(TRI_ERROR_TYPE_ERROR);
          }
          string keyVal(jsonValue->_value._string.data);
          if (TRI_VOC_ATTRIBUTE_KEY == key) {
            def._internal.insert(make_pair(internalAttr::key, DocumentId(0, keyVal)));
          } 
          else if (TRI_VOC_ATTRIBUTE_REV == key) {
            def._internal.insert(make_pair(internalAttr::rev, DocumentId(0, keyVal)));
          } 
          else {
            string colName = keyVal.substr(0, keyVal.find("/"));
            keyVal = keyVal.substr(keyVal.find("/") + 1, keyVal.length());
            if (TRI_VOC_ATTRIBUTE_ID == key) {
              def._internal.insert(make_pair(internalAttr::id, DocumentId(resolver->getCollectionId(colName), keyVal)));
            } 
            else if (TRI_VOC_ATTRIBUTE_FROM == key) {
              def._internal.insert(make_pair(internalAttr::from, DocumentId(resolver->getCollectionId(colName), keyVal)));
            } 
            else if (TRI_VOC_ATTRIBUTE_TO == key) {
              def._internal.insert(make_pair(internalAttr::to, DocumentId(resolver->getCollectionId(colName), keyVal)));
            } 
            else {
              // no attribute path found. this means the result will be empty
              THROW_ARANGO_EXCEPTION(TRI_RESULT_ELEMENT_NOT_FOUND);
            }
 
          }
        }
        else {
          // no attribute path found. this means the result will be empty
          ExampleMatcher::cleanup();
          THROW_ARANGO_EXCEPTION(TRI_RESULT_ELEMENT_NOT_FOUND);
        }
      }
      else {
        def._pids.push_back(pid);
        auto jsonValue = static_cast<TRI_json_t const*>(TRI_AtVector(&objects, i + 1));
        auto value = TRI_ShapedJsonJson(_shaper, jsonValue, false);

        if (value == nullptr) {
          ExampleMatcher::cleanup();
          THROW_ARANGO_EXCEPTION(TRI_RESULT_ELEMENT_NOT_FOUND);
        }

        def._values.push_back(value);
      }
    }
  } 
  catch (bad_alloc&) {
    ExampleMatcher::cleanup();
    throw;
  }
}
 

////////////////////////////////////////////////////////////////////////////////
/// @brief Constructor using a v8::Object example
////////////////////////////////////////////////////////////////////////////////
 
ExampleMatcher::ExampleMatcher (v8::Isolate* isolate,
                                v8::Handle<v8::Object> const example,
                                TRI_shaper_t* shaper,
                                std::string& errorMessage) 
  : _shaper(shaper) {

  v8::Handle<v8::Array> names = example->GetOwnPropertyNames();
  size_t n = names->Length();

  ExampleDefinition def;

  try { 
    ExampleMatcher::fillExampleDefinition(isolate, example, names, n, errorMessage, def);
  } 
  catch (...) {
    ExampleMatcher::cleanup();
    throw;
  }
  definitions.emplace_back(move(def)); 
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Constructor using an v8::Array of v8::Object examples
////////////////////////////////////////////////////////////////////////////////
 
ExampleMatcher::ExampleMatcher (v8::Isolate* isolate,
                                v8::Handle<v8::Array> const examples,
                                TRI_shaper_t* shaper,
                                std::string& errorMessage) 
  : _shaper(shaper) {

  size_t exCount = examples->Length();
  for (size_t j = 0; j < exCount; ++j) {
    auto tmp = examples->Get((uint32_t) j);
    if (! tmp->IsObject() || tmp->IsArray()) {
      // Right now silently ignore this example
      continue;
    }
    v8::Handle<v8::Object> example = v8::Handle<v8::Object>::Cast(tmp);
    v8::Handle<v8::Array> names = example->GetOwnPropertyNames();
    size_t n = names->Length();

    ExampleDefinition def;

    try { 
      ExampleMatcher::fillExampleDefinition(isolate, example, names, n, errorMessage, def);
    } 
    catch (...) {
      ExampleMatcher::cleanup();
      throw;
    }
    definitions.emplace_back(move(def)); 
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Constructor using a TRI_json_t object
////////////////////////////////////////////////////////////////////////////////

ExampleMatcher::ExampleMatcher (TRI_json_t const* example,
                                TRI_shaper_t* shaper,
                                CollectionNameResolver const* resolver) 
  : _shaper(shaper) {

  if (TRI_IsObjectJson(example) || TRI_IsStringJson(example)) {
    ExampleDefinition def;
    try { 
      ExampleMatcher::fillExampleDefinition(example, resolver, def);
    } 
    catch (...) {
      ExampleMatcher::cleanup();
      throw;
    }
    definitions.emplace_back(move(def)); 
  }
  else if (TRI_IsArrayJson(example)) {
    size_t size = TRI_LengthArrayJson(example);
    for (size_t i = 0; i < size; ++i) {
      ExampleDefinition def;
      try { 
        ExampleMatcher::fillExampleDefinition(TRI_LookupArrayJson(example, i), resolver, def);
      } 
      catch (...) {
        ExampleMatcher::cleanup();
        throw;
      }
      definitions.emplace_back(move(def)); 
    }
  }

}

////////////////////////////////////////////////////////////////////////////////
/// @brief Checks if the given mptr matches the examples in this class
////////////////////////////////////////////////////////////////////////////////

bool ExampleMatcher::matches (TRI_voc_cid_t cid, TRI_doc_mptr_t const* mptr) const {
  if (mptr == nullptr) {
    return false;
  }
  TRI_shaped_json_t document;
  TRI_EXTRACT_SHAPED_JSON_MARKER(document, mptr->getDataPtr());
  for (auto def : definitions) {
    if (def._internal.size() > 0) {
      // Match _key
      auto it = def._internal.find(internalAttr::key);
      if (it != def._internal.end()) {
        if (strcmp(it->second.key.c_str(), TRI_EXTRACT_MARKER_KEY(mptr)) != 0) {
          goto nextExample;
        }
      }
      // Match _rev
      it = def._internal.find(internalAttr::rev);
      if (it != def._internal.end()) {
        if (triagens::basics::StringUtils::uint64(it->second.key) != mptr->_rid) {
          goto nextExample;
        }
      }
      // Match _id
      it = def._internal.find(internalAttr::id);
      if (it != def._internal.end()) {
        if (cid != it->second.cid) {
          goto nextExample;
        }
        if (strcmp(it->second.key.c_str(), TRI_EXTRACT_MARKER_KEY(mptr)) != 0) {
          goto nextExample;
        }
      }
      // Match _to
      it = def._internal.find(internalAttr::to);
      if (it != def._internal.end()) {
        if (it->second.cid != TRI_EXTRACT_MARKER_TO_CID(mptr)) {
          goto nextExample;
        }
        if (strcmp(it->second.key.c_str(), TRI_EXTRACT_MARKER_TO_KEY(mptr)) != 0) {
          goto nextExample;
        }
      }
      // Match _from
      it = def._internal.find(internalAttr::from);
      if (it != def._internal.end()) {
        if (it->second.cid != TRI_EXTRACT_MARKER_FROM_CID(mptr)) {
          goto nextExample;
        }
        if (strcmp(it->second.key.c_str(), TRI_EXTRACT_MARKER_FROM_KEY(mptr)) != 0) {
          goto nextExample;
        }
      }
    }
    TRI_shaped_json_t result;
    TRI_shape_t const* shape;

    for (size_t i = 0;  i < def._values.size();  ++i) {
      TRI_shaped_json_t* example = def._values[i];

      bool ok = TRI_ExtractShapedJsonVocShaper(_shaper,
                                               &document,
                                               example->_sid,
                                               def._pids[i],
                                               &result,
                                               &shape);

      if (! ok || shape == nullptr) {
        goto nextExample;
      }

      if (result._data.length != example->_data.length) {
        // suppress excessive log spam
        // LOG_TRACE("expecting length %lu, got length %lu for path %lu",
        //           (unsigned long) result._data.length,
        //           (unsigned long) example->_data.length,
        //           (unsigned long) pids[i]);

        goto nextExample;
      }

      if (memcmp(result._data.data, example->_data.data, example->_data.length) != 0) {
        // suppress excessive log spam
        // LOG_TRACE("data mismatch at path %lu", (unsigned long) pids[i]);
        goto nextExample;
      }
    }
    // If you get here this example matches. Successful hit.
    return true;
    nextExample:
    // This Example does not match, try the next one. Goto requires a statement
    continue;
  }
  return false;
}
