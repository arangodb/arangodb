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

#include "Basics/StringUtils.h"
#include "Basics/conversions.h"
#include "V8/v8-conv.h"
#include "V8Server/v8-externals.h"
#include "V8Server/v8-vocbaseprivate.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/vocbase.h"
#include "v8-collection.h"

using namespace arangodb;

/// @brief check if a name belongs to a collection
bool EqualCollection(CollectionNameResolver const* resolver, std::string const& collectionName,
                     LogicalCollection const* collection) {
  if (collectionName == collection->name()) {
    return true;
  }

  if (collectionName == std::to_string(collection->id())) {
    return true;
  }

  // Shouldn't it just be: If we are on DBServer we also have to check for
  // global ID name and cid should be the shard.
  if (ServerState::instance()->isCoordinator()) {
    if (collectionName == resolver->getCollectionNameCluster(collection->id())) {
      return true;
    }
    return false;
  }

  if (collectionName == resolver->getCollectionName(collection->id())) {
    return true;
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unwrap a LogicalCollection wrapped via WrapCollection(...)
/// @return collection or nullptr on failure
////////////////////////////////////////////////////////////////////////////////
arangodb::LogicalCollection* UnwrapCollection(v8::Isolate* isolate,
                                              v8::Local<v8::Object> const& holder) {
  return TRI_UnwrapClass<arangodb::LogicalCollection>(holder, WRP_VOCBASE_COL_TYPE, TRI_IGETC);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief wraps a LogicalCollection
////////////////////////////////////////////////////////////////////////////////
v8::Handle<v8::Object> WrapCollection( // wrap collection
    v8::Isolate* isolate, // isolate
    std::shared_ptr<arangodb::LogicalCollection> const& collection // collection
) {
  v8::EscapableHandleScope scope(isolate);
  TRI_GET_GLOBALS();
  auto context = TRI_IGETC;
  TRI_GET_GLOBAL(VocbaseColTempl, v8::ObjectTemplate);
  v8::Handle<v8::Object> result = VocbaseColTempl->NewInstance(TRI_IGETC).FromMaybe(v8::Local<v8::Object>());

  if (result.IsEmpty()) {
    return scope.Escape<v8::Object>(result);
  }

  auto value = std::shared_ptr<void>( // persistent value
    collection.get(), // value
    [collection](void*)->void { // ensure collection shared_ptr is not deallocated
      TRI_ASSERT(!collection->vocbase().isDangling());
      collection->vocbase().release(); // decrease the reference-counter for the database
    }
  );
  auto itr = TRI_v8_global_t::SharedPtrPersistent::emplace(*isolate, value);
  auto& entry = itr.first;

  TRI_ASSERT(!collection->vocbase().isDangling());
  collection->vocbase().forceUse(); // increase the reference-counter for the database (will be decremented by 'value' distructor above, valid for both new and existing mappings)

  result->SetInternalField(// required for TRI_UnwrapClass(...)
    SLOT_CLASS_TYPE, v8::Integer::New(isolate, WRP_VOCBASE_COL_TYPE) // args
  );
  result->SetInternalField(SLOT_CLASS, entry.get());

  TRI_GET_GLOBAL_STRING(_IdKey);
  TRI_GET_GLOBAL_STRING(_DbNameKey);
  TRI_GET_GLOBAL_STRING(VersionKeyHidden);
  result->DefineOwnProperty( // define own property
    TRI_IGETC, // context
    _IdKey, // key
    TRI_V8UInt64String<TRI_voc_cid_t>(isolate, collection->id()), // value
    v8::ReadOnly // attributes
  ).FromMaybe(false); // Ignore result...
  result->Set(context, // set value
    _DbNameKey, TRI_V8_STD_STRING(isolate, collection->vocbase().name()) // args
  ).FromMaybe(false);
  result->DefineOwnProperty( // define own property
    TRI_IGETC, // context
    VersionKeyHidden, // key
    v8::Integer::NewFromUnsigned(isolate, collection->v8CacheVersion()), // value
    v8::DontEnum // attributes
  ).FromMaybe(false);  // ignore return value

  return scope.Escape<v8::Object>(result);
}
