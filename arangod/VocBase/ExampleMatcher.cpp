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
#include "Basics/StringUtils.h"
#include "Utils/CollectionNameResolver.h"
#include "V8/v8-utils.h"
#include "V8/v8-conv.h"
#include "V8/v8-vpack.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::basics;

VPackSlice ExampleMatcher::ExampleDefinition::slice() const {
  return _values.slice();
}

void ExampleMatcher::fillExampleDefinition(
    v8::Isolate* isolate, v8::Handle<v8::Object> const& example,
    v8::Handle<v8::Array> const& names, size_t& n, std::string& errorMessage,
    ExampleDefinition& def) {
  VPackArrayBuilder guard(&def._values);
  TRI_IF_FAILURE("ExampleNoContextVocbase") {
    // intentionally fail
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  std::unique_ptr<CollectionNameResolver> resolver;

  def._paths.reserve(n);

  for (size_t i = 0; i < n; ++i) {
    v8::Handle<v8::Value> key = names->Get((uint32_t)i);
    v8::Handle<v8::Value> val = example->Get(key);
    TRI_Utf8ValueNFC keyStr(TRI_UNKNOWN_MEM_ZONE, key);
    if (*keyStr != nullptr) {
      char const* p = *keyStr;

      // TODO: Match for _id
      std::string const key(p, keyStr.length());
      def._paths.emplace_back(arangodb::basics::StringUtils::split(key, "."));

      int res = TRI_V8ToVPack(isolate, def._values, val, false);
      if (res != TRI_ERROR_NO_ERROR) {
        THROW_ARANGO_EXCEPTION(res);
      }
    } else {
      errorMessage = "cannot convert attribute path to UTF8";
      THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
    }
  }
}

void ExampleMatcher::fillExampleDefinition(
    VPackSlice const& example, CollectionNameResolver const* resolver,
    ExampleDefinition& def) {
  TRI_ASSERT(def._values.isEmpty());
  VPackArrayBuilder guard(&def._values);
  if (example.isString()) {
    // Example is an _id value
    std::string tmp = example.copyString();
    char const* _key = strchr(tmp.c_str(), '/');
    if (_key != nullptr) {
      _key += 1;
      std::vector<std::string> key({TRI_VOC_ATTRIBUTE_KEY});
      def._paths.emplace_back(key);
      def._values.add(VPackValue(_key));
      return;
    }
    THROW_ARANGO_EXCEPTION(TRI_RESULT_ELEMENT_NOT_FOUND);
  }
  TRI_ASSERT(example.isObject());

  size_t n = static_cast<size_t>(example.length());

  def._paths.reserve(n);

  for (auto const& it : VPackObjectIterator(example)) {
    TRI_ASSERT(it.key.isString());
    std::string key = it.key.copyString();
    // TODO: Match for _id
    def._paths.emplace_back(arangodb::basics::StringUtils::split(key, "."));
    def._values.add(it.value);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Constructor using a v8::Object example
////////////////////////////////////////////////////////////////////////////////

ExampleMatcher::ExampleMatcher(v8::Isolate* isolate,
                               v8::Handle<v8::Object> const example,
                               std::string& errorMessage) {
  v8::Handle<v8::Array> names = example->GetOwnPropertyNames();
  size_t n = names->Length();

  ExampleDefinition def;
  ExampleMatcher::fillExampleDefinition(isolate, example, names, n,
                                        errorMessage, def);
  definitions.emplace_back(std::move(def));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Constructor using an v8::Array of v8::Object examples
////////////////////////////////////////////////////////////////////////////////

ExampleMatcher::ExampleMatcher(v8::Isolate* isolate,
                               v8::Handle<v8::Array> const examples,
                               std::string& errorMessage) {
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

    ExampleMatcher::fillExampleDefinition(isolate, example, names, n,
                                          errorMessage, def);
    definitions.emplace_back(std::move(def));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Constructor using a TRI_json_t object
///        DEPRECATED
////////////////////////////////////////////////////////////////////////////////

ExampleMatcher::ExampleMatcher(TRI_json_t const* example,
                               CollectionNameResolver const* resolver) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Constructor using a VelocyPack object
///        Note: allowStrings is used to define if strings in example-array
///        should be matched to _id
////////////////////////////////////////////////////////////////////////////////

ExampleMatcher::ExampleMatcher(VPackSlice const& example,
                               CollectionNameResolver const* resolver,
                               bool allowStrings) {
  if (example.isObject() || example.isString()) {
    ExampleDefinition def;
    ExampleMatcher::fillExampleDefinition(example, resolver, def);
    definitions.emplace_back(std::move(def));
  } else if (example.isArray()) {
    for (auto const& e : VPackArrayIterator(example)) {
      ExampleDefinition def;
      if (!allowStrings && e.isString()) {
        // We do not match strings in Array
        continue;
      }
      ExampleMatcher::fillExampleDefinition(e, resolver, def);
      definitions.emplace_back(std::move(def));
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

bool ExampleMatcher::matches(TRI_voc_cid_t, TRI_doc_mptr_t const* mptr) const {
  if (mptr == nullptr) {
    return false;
  }
  VPackSlice toMatch(mptr->vpack());
  for (auto const& def : definitions) {
    VPackSlice const compareValue = def.slice();
    size_t i = 0;
    for (i = 0; i < def._paths.size(); ++i) {
      VPackSlice toCheck = toMatch.get(def._paths[i]);
      if (toCheck.isNone()){
        break;
      }
      VPackSlice compare = compareValue.at(i);
      if (toCheck != compare) {
        break;
      }
    }
    if (i == def._paths.size()) {
      // If you get here this example matches. Successful hit.
      return true;
    }
  }
  return false;
}
