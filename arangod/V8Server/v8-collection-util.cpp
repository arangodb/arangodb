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

#include "v8-collection.h"
#include "Basics/conversions.h"
#include "Basics/StringUtils.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/vocbase.h"
#include "V8/v8-conv.h"
#include "V8Server/v8-externals.h"
#include "V8Server/v8-vocbaseprivate.h"

using namespace arangodb;
using namespace arangodb::basics;

/// @brief check if a name belongs to a collection
bool EqualCollection(CollectionNameResolver const* resolver,
                     std::string const& collectionName,
                     LogicalCollection const* collection) {
  if (collectionName == collection->name()) {
    return true;
  }

  if (collectionName == std::to_string(collection->id())) {
    return true;
  }

  // Shouldn't it just be: If we are on DBServer we also have to check for global ID
  // name and cid should be the shard.
  if (ServerState::instance()->isCoordinator()) {
    if (collectionName ==
        resolver->getCollectionNameCluster(collection->id())) {
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
/// @brief wraps a LogicalCollection
/// Note that if collection is a local collection, then the object will never
/// be freed. If it is not a local collection (coordinator case), then delete
/// will be called when the V8 object is garbage collected.
////////////////////////////////////////////////////////////////////////////////
v8::Handle<v8::Object> WrapCollection(
    v8::Isolate* isolate,
    std::shared_ptr<arangodb::LogicalCollection> const& collection
) {
  v8::EscapableHandleScope scope(isolate);

  TRI_GET_GLOBALS();
  TRI_GET_GLOBAL(VocbaseColTempl, v8::ObjectTemplate);
  v8::Handle<v8::Object> result = VocbaseColTempl->NewInstance();

  if (!result.IsEmpty()) {
    auto* ptr = collection.get();
    auto itr = v8g->JSDatasources.emplace(
      std::piecewise_construct,
      std::forward_as_tuple(collection.get()),
      std::forward_as_tuple(
        isolate,
        collection,
        [ptr]()->void { // FIXME TODO find a way to move this callback code into DataSourcePersistent
          TRI_ASSERT(!ptr->vocbase().isDangling());
          ptr->vocbase().release(); // decrease the reference-counter for the database
        }
      )
    );
    auto& entry = itr.first->second;

    if (itr.second) { // FIXME TODO find a way to move this code into DataSourcePersistent
      TRI_ASSERT(!ptr->vocbase().isDangling());
      ptr->vocbase().forceUse(); // increase the reference-counter for the database
    }

    result->SetInternalField(
      SLOT_CLASS_TYPE, v8::Integer::New(isolate, WRP_VOCBASE_COL_TYPE)
    );
    result->SetInternalField(SLOT_CLASS, entry.get());
    result->SetInternalField(SLOT_EXTERNAL, entry.get());

    TRI_GET_GLOBAL_STRING(_IdKey);
    TRI_GET_GLOBAL_STRING(_DbNameKey);
    TRI_GET_GLOBAL_STRING(VersionKeyHidden);
    result->ForceSet(
      _IdKey,
      TRI_V8UInt64String<TRI_voc_cid_t>(isolate, collection->id()),
      v8::ReadOnly
    );
    result->Set(_DbNameKey, TRI_V8_STD_STRING(isolate, collection->vocbase().name()));
    result->ForceSet(
        VersionKeyHidden,
        v8::Integer::NewFromUnsigned(isolate, 5),
        v8::DontEnum);
  }

  return scope.Escape<v8::Object>(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unwrap a LogicalCollection wrapped via WrapCollection(...)
/// @return collection or nullptr on failure
////////////////////////////////////////////////////////////////////////////////
arangodb::LogicalCollection* UnwrapCollection(
    v8::Local<v8::Object> const& holder
) {
  return TRI_UnwrapClass<arangodb::LogicalCollection>(
    holder, WRP_VOCBASE_COL_TYPE
  );
}
