////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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

#include <velocypack/Builder.h>
#include <velocypack/Value.h>
#include <velocypack/velocypack-aliases.h>

#include "Basics/StaticStrings.h"
#include "Basics/conversions.h"
#include "V8/v8-conv.h"
#include "VocBase/Identifiers/RevisionId.h"
#include "VocBase/KeyGenerator.h"
#include "v8-vocbaseprivate.h"

using namespace arangodb;
using namespace arangodb::basics;

////////////////////////////////////////////////////////////////////////////////
/// @brief get the vocbase pointer from the current V8 context
////////////////////////////////////////////////////////////////////////////////

TRI_vocbase_t& GetContextVocBase(v8::Isolate* isolate) {
  TRI_GET_GLOBALS();

  TRI_ASSERT(v8g->_vocbase != nullptr);
  TRI_ASSERT(!v8g->_vocbase->isDangling());

  return *static_cast<TRI_vocbase_t*>(v8g->_vocbase);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if argument is a document identifier
////////////////////////////////////////////////////////////////////////////////

static bool ParseDocumentHandle(v8::Isolate* isolate, v8::Handle<v8::Value> const arg,
                                std::string& collectionName,
                                std::unique_ptr<char[]>& key) {
  TRI_ASSERT(collectionName.empty());

  if (!arg->IsString()) {
    return false;
  }

  // the handle must always be an ASCII string. These is no need to normalize it
  // first
  v8::String::Utf8Value str(isolate, arg);

  if (*str == nullptr) {
    return false;
  }

  // collection name / document key
  size_t split;
  if (KeyGenerator::validateId(*str, str.length(), &split)) {
    collectionName = std::string(*str, split);
    auto const length = str.length() - split - 1;
    auto buffer = new char[length + 1];
    memcpy(buffer, *str + split + 1, length);
    buffer[length] = '\0';
    key.reset(buffer);
    return true;
  }

  // document key only
  if (KeyGenerator::validateKey(*str, str.length())) {
    auto const length = str.length();
    auto buffer = new char[length + 1];
    memcpy(buffer, *str, length);
    buffer[length] = '\0';
    key.reset(buffer);

    return true;
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parse document or document handle from a v8 value (string | object)
/// Note that the builder must already be open with an object and that it
/// will remain open afterwards!
////////////////////////////////////////////////////////////////////////////////

bool ExtractDocumentHandle(v8::Isolate* isolate, v8::Handle<v8::Value> const val,
                           std::string& collectionName, VPackBuilder& builder,
                           bool includeRev) {
  auto context = TRI_IGETC;
  // reset the collection identifier and the revision
  TRI_ASSERT(collectionName.empty());

  std::unique_ptr<char[]> key;

  // extract the document identifier and revision from a string
  if (val->IsString()) {
    bool res = ParseDocumentHandle(isolate, val, collectionName, key);
    if (res) {
      if (key.get() == nullptr) {
        return false;
      }
      builder.add(StaticStrings::KeyString,
                  VPackValue(reinterpret_cast<char*>(key.get())));
    }
    return res;
  }

  // extract the document identifier and revision from a document object
  if (val->IsObject()) {
    TRI_GET_GLOBALS();

    v8::Handle<v8::Object> obj =
        val->ToObject(TRI_IGETC).FromMaybe(v8::Local<v8::Object>());
    TRI_GET_GLOBAL_STRING(_IdKey);
    TRI_GET_GLOBAL_STRING(_KeyKey);
    if (TRI_HasRealNamedProperty(context, isolate, obj, _IdKey)) {
      v8::Handle<v8::Value> didVal = obj->Get(context, _IdKey).FromMaybe(v8::Local<v8::Value>());

      if (!ParseDocumentHandle(isolate, didVal, collectionName, key)) {
        return false;
      }
    } else if (TRI_HasRealNamedProperty(context, isolate, obj, _KeyKey)) {
      v8::Handle<v8::Value> didVal = obj->Get(context, _KeyKey).FromMaybe(v8::Local<v8::Value>());

      if (!ParseDocumentHandle(isolate, didVal, collectionName, key)) {
        return false;
      }
    } else {
      return false;
    }

    if (key.get() == nullptr) {
      return false;
    }
    // If we get here we have a valid key
    builder.add(StaticStrings::KeyString, VPackValue(reinterpret_cast<char*>(key.get())));

    if (!includeRev) {
      return true;
    }

    TRI_GET_GLOBAL_STRING(_RevKey);
    if (!TRI_HasRealNamedProperty(context, isolate, obj, _RevKey)) {
      return true;
    }
    v8::Handle<v8::Value> revObj = obj->Get(context, _RevKey).FromMaybe(v8::Local<v8::Value>());
    if (!revObj->IsString()) {
      return true;
    }
    v8::String::Utf8Value str(isolate, revObj);
    bool isOld;
    RevisionId rid = RevisionId::fromString(*str, str.length(), isOld, false);

    if (rid.empty()) {
      return false;
    }
    builder.add(StaticStrings::RevString, VPackValue(rid.toString()));
    return true;
  }

  // unknown value type. give up
  return false;
}
