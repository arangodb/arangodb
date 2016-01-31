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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "ExampleMatcher.h"
#include "Basics/JsonHelper.h"
#include "Basics/StringUtils.h"
#include "Utils/V8ResolverGuard.h"
#include "V8/v8-utils.h"
#include "V8/v8-conv.h"
#include "V8Server/v8-shape-conv.h"
#include "V8Server/v8-vocbaseprivate.h"
#include "VocBase/VocShaper.h"

using namespace arangodb;
using namespace arangodb::basics;

////////////////////////////////////////////////////////////////////////////////
/// @brief cleans up shaped json values
////////////////////////////////////////////////////////////////////////////////

static void CleanupShapes(std::vector<TRI_shaped_json_t*>& values) {
  for (auto& it : values) {
    if (it != nullptr) {
      TRI_FreeShapedJson(TRI_UNKNOWN_MEM_ZONE, it);
    }
  }
  values.clear();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief cleans up the example object
////////////////////////////////////////////////////////////////////////////////

void ExampleMatcher::cleanup() {
  // clean shaped json objects
  for (auto& def : definitions) {
    CleanupShapes(def._values);
  }
  definitions.clear();
}

void ExampleMatcher::fillExampleDefinition(
    v8::Isolate* isolate, v8::Handle<v8::Object> const& example,
    v8::Handle<v8::Array> const& names, size_t& n, std::string& errorMessage,
    ExampleDefinition& def) {
  TRI_IF_FAILURE("ExampleNoContextVocbase") {
    // intentionally fail
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  TRI_vocbase_t* vocbase = GetContextVocBase(isolate);
  if (vocbase == nullptr) {
    // This should never be thrown as we are already in a transaction
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  def._pids.reserve(n);
  def._values.reserve(n);

  for (size_t i = 0; i < n; ++i) {
    v8::Handle<v8::Value> key = names->Get((uint32_t)i);
    v8::Handle<v8::Value> val = example->Get(key);
    TRI_Utf8ValueNFC keyStr(TRI_UNKNOWN_MEM_ZONE, key);
    if (*keyStr != nullptr) {
      auto pid = _shaper->lookupAttributePathByName(*keyStr);

      if (pid == 0) {
        // Internal attributes do have pid == 0.
        char const* p = *keyStr;
        if (*p == '_') {
          std::string const key(p, keyStr.length());
          std::string keyVal = TRI_ObjectToString(val);
          if (TRI_VOC_ATTRIBUTE_KEY == key) {
            def._internal.insert(
                std::make_pair(internalAttr::key, DocumentId(0, keyVal)));
          } else if (TRI_VOC_ATTRIBUTE_REV == key) {
            def._internal.insert(
                std::make_pair(internalAttr::rev, DocumentId(0, keyVal)));
          } else {
            // We need a Collection Name Resolver here now!
            V8ResolverGuard resolverGuard(vocbase);
            CollectionNameResolver const* resolver =
                resolverGuard.getResolver();
            std::string colName = keyVal.substr(0, keyVal.find("/"));
            keyVal = keyVal.substr(keyVal.find("/") + 1, keyVal.length());
            if (TRI_VOC_ATTRIBUTE_ID == key) {
              def._internal.insert(std::make_pair(
                  internalAttr::id,
                  DocumentId(resolver->getCollectionId(colName), keyVal)));
            } else if (TRI_VOC_ATTRIBUTE_FROM == key) {
              def._internal.insert(std::make_pair(
                  internalAttr::from,
                  DocumentId(resolver->getCollectionId(colName), keyVal)));
            } else if (TRI_VOC_ATTRIBUTE_TO == key) {
              def._internal.insert(std::make_pair(
                  internalAttr::to,
                  DocumentId(resolver->getCollectionId(colName), keyVal)));
            } else {
              // no attribute path found. this means the result will be empty
              THROW_ARANGO_EXCEPTION(TRI_RESULT_ELEMENT_NOT_FOUND);
            }
          }
        } else {
          // no attribute path found. this means the result will be empty
          THROW_ARANGO_EXCEPTION(TRI_RESULT_ELEMENT_NOT_FOUND);
        }
      } else {
        def._pids.push_back(pid);

        std::unique_ptr<TRI_shaped_json_t> value(
            TRI_ShapedJsonV8Object(isolate, val, _shaper, false));

        if (value.get() == nullptr) {
          THROW_ARANGO_EXCEPTION(TRI_RESULT_ELEMENT_NOT_FOUND);
        }
        def._values.push_back(value.get());
        value.release();
      }
    } else {
      errorMessage = "cannot convert attribute path to UTF8";
      THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
    }
  }
}

void ExampleMatcher::fillExampleDefinition(
    TRI_json_t const* example, CollectionNameResolver const* resolver,
    ExampleDefinition& def) {
  if (TRI_IsStringJson(example)) {
    // Example is an _id value
    char const* _key = strchr(example->_value._string.data, '/');
    if (_key != nullptr) {
      _key += 1;
      def._internal.insert(
          std::make_pair(internalAttr::key, DocumentId(0, _key)));
      return;
    }
    THROW_ARANGO_EXCEPTION(TRI_RESULT_ELEMENT_NOT_FOUND);
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
      auto pid = _shaper->lookupAttributePathByName(keyStr);

      if (pid == 0) {
        // Internal attributes do have pid == 0.
        if (*keyStr == '_') {
          std::string const key(keyStr, keyObj->_value._string.length - 1);
          auto jsonValue =
              static_cast<TRI_json_t const*>(TRI_AtVector(&objects, i + 1));
          if (!TRI_IsStringJson(jsonValue)) {
            THROW_ARANGO_EXCEPTION(TRI_ERROR_TYPE_ERROR);
          }
          std::string keyVal(jsonValue->_value._string.data);
          if (TRI_VOC_ATTRIBUTE_KEY == key) {
            def._internal.insert(
                std::make_pair(internalAttr::key, DocumentId(0, keyVal)));
          } else if (TRI_VOC_ATTRIBUTE_REV == key) {
            def._internal.insert(
                std::make_pair(internalAttr::rev, DocumentId(0, keyVal)));
          } else {
            std::string colName = keyVal.substr(0, keyVal.find("/"));
            keyVal = keyVal.substr(keyVal.find("/") + 1, keyVal.length());
            if (TRI_VOC_ATTRIBUTE_ID == key) {
              def._internal.insert(std::make_pair(
                  internalAttr::id,
                  DocumentId(resolver->getCollectionId(colName), keyVal)));
            } else if (TRI_VOC_ATTRIBUTE_FROM == key) {
              def._internal.insert(std::make_pair(
                  internalAttr::from,
                  DocumentId(resolver->getCollectionId(colName), keyVal)));
            } else if (TRI_VOC_ATTRIBUTE_TO == key) {
              def._internal.insert(std::make_pair(
                  internalAttr::to,
                  DocumentId(resolver->getCollectionId(colName), keyVal)));
            } else {
              // no attribute path found. this means the result will be empty
              THROW_ARANGO_EXCEPTION(TRI_RESULT_ELEMENT_NOT_FOUND);
            }
          }
        } else {
          // no attribute path found. this means the result will be empty
          THROW_ARANGO_EXCEPTION(TRI_RESULT_ELEMENT_NOT_FOUND);
        }
      } else {
        def._pids.push_back(pid);
        auto jsonValue =
            static_cast<TRI_json_t const*>(TRI_AtVector(&objects, i + 1));
        auto value = TRI_ShapedJsonJson(_shaper, jsonValue, false);

        if (value == nullptr) {
          THROW_ARANGO_EXCEPTION(TRI_RESULT_ELEMENT_NOT_FOUND);
        }

        def._values.push_back(value);
      }
    }
  } catch (std::bad_alloc const&) {
    ExampleMatcher::cleanup();
    throw;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Constructor using a v8::Object example
////////////////////////////////////////////////////////////////////////////////

ExampleMatcher::ExampleMatcher(v8::Isolate* isolate,
                               v8::Handle<v8::Object> const example,
                               VocShaper* shaper, std::string& errorMessage)
    : _shaper(shaper) {
  v8::Handle<v8::Array> names = example->GetOwnPropertyNames();
  size_t n = names->Length();

  ExampleDefinition def;

  try {
    ExampleMatcher::fillExampleDefinition(isolate, example, names, n,
                                          errorMessage, def);
    definitions.emplace_back(std::move(def));
  } catch (...) {
    CleanupShapes(def._values);
    ExampleMatcher::cleanup();
    throw;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Constructor using an v8::Array of v8::Object examples
////////////////////////////////////////////////////////////////////////////////

ExampleMatcher::ExampleMatcher(v8::Isolate* isolate,
                               v8::Handle<v8::Array> const examples,
                               VocShaper* shaper, std::string& errorMessage)
    : _shaper(shaper) {
  size_t exCount = examples->Length();
  for (size_t j = 0; j < exCount; ++j) {
    auto tmp = examples->Get((uint32_t)j);
    if (!tmp->IsObject() || tmp->IsArray()) {
      // Right now silently ignore this example
      continue;
    }
    v8::Handle<v8::Object> example = v8::Handle<v8::Object>::Cast(tmp);
    v8::Handle<v8::Array> names = example->GetOwnPropertyNames();
    size_t n = names->Length();

    ExampleDefinition def;

    try {
      ExampleMatcher::fillExampleDefinition(isolate, example, names, n,
                                            errorMessage, def);
      definitions.emplace_back(std::move(def));
    } catch (...) {
      CleanupShapes(def._values);
      ExampleMatcher::cleanup();
      throw;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Constructor using a TRI_json_t object
////////////////////////////////////////////////////////////////////////////////

ExampleMatcher::ExampleMatcher(TRI_json_t const* example, VocShaper* shaper,
                               CollectionNameResolver const* resolver)
    : _shaper(shaper) {
  if (TRI_IsObjectJson(example) || TRI_IsStringJson(example)) {
    ExampleDefinition def;
    try {
      ExampleMatcher::fillExampleDefinition(example, resolver, def);
    } catch (...) {
      CleanupShapes(def._values);
      ExampleMatcher::cleanup();
      throw;
    }
    definitions.emplace_back(std::move(def));
  } else if (TRI_IsArrayJson(example)) {
    size_t size = TRI_LengthArrayJson(example);
    for (size_t i = 0; i < size; ++i) {
      ExampleDefinition def;
      try {
        ExampleMatcher::fillExampleDefinition(TRI_LookupArrayJson(example, i),
                                              resolver, def);
        definitions.emplace_back(std::move(def));
      } catch (arangodb::basics::Exception& e) {
        if (e.code() != TRI_RESULT_ELEMENT_NOT_FOUND) {
          CleanupShapes(def._values);
          ExampleMatcher::cleanup();
          throw;
        }
        // Result not found might happen. Ignore here because all other elemens
        // might be matched.
      }
    }
    if (definitions.empty()) {
      // None of the given examples could ever match.
      // Throw result not found so client can short circuit.
      THROW_ARANGO_EXCEPTION(TRI_RESULT_ELEMENT_NOT_FOUND);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Checks if the given mptr matches the examples in this class
////////////////////////////////////////////////////////////////////////////////

bool ExampleMatcher::matches(TRI_voc_cid_t cid,
                             TRI_doc_mptr_t const* mptr) const {
  if (mptr == nullptr) {
    return false;
  }
  TRI_shaped_json_t document;
  TRI_EXTRACT_SHAPED_JSON_MARKER(document, mptr->getDataPtr());
  for (auto const& def : definitions) {
    if (!def._internal.empty()) {
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
        if (arangodb::basics::StringUtils::uint64(it->second.key) !=
            mptr->_rid) {
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
        if (!TRI_IS_EDGE_MARKER(mptr)) {
          goto nextExample;
        }
        if (it->second.cid != TRI_EXTRACT_MARKER_TO_CID(mptr)) {
          goto nextExample;
        }
        if (strcmp(it->second.key.c_str(), TRI_EXTRACT_MARKER_TO_KEY(mptr)) !=
            0) {
          goto nextExample;
        }
      }
      // Match _from
      it = def._internal.find(internalAttr::from);
      if (it != def._internal.end()) {
        if (!TRI_IS_EDGE_MARKER(mptr)) {
          goto nextExample;
        }
        if (it->second.cid != TRI_EXTRACT_MARKER_FROM_CID(mptr)) {
          goto nextExample;
        }
        if (strcmp(it->second.key.c_str(), TRI_EXTRACT_MARKER_FROM_KEY(mptr)) !=
            0) {
          goto nextExample;
        }
      }
    }
    TRI_shaped_json_t result;
    TRI_shape_t const* shape;

    for (size_t i = 0; i < def._values.size(); ++i) {
      TRI_shaped_json_t* example = def._values[i];

      bool ok = _shaper->extractShapedJson(&document, example->_sid,
                                           def._pids[i], &result, &shape);

      if (!ok || shape == nullptr) {
        goto nextExample;
      }

      if (result._data.length != example->_data.length) {
        // suppress excessive log spam
        // LOG(TRACE) << "expecting length " << //           (unsigned long) result._data.length << ", got length " << //           (unsigned long) example->_data.length << " for path " << //           (unsigned long) pids[i];

        goto nextExample;
      }

      if (result._data.length > 0 &&
          memcmp(result._data.data, example->_data.data,
                 example->_data.length) != 0) {
        // suppress excessive log spam
        // LOG(TRACE) << "data mismatch at path " << pids[i];
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
