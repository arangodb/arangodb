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
/// @brief weak reference callback for collections
////////////////////////////////////////////////////////////////////////////////

static void WeakCollectionCallback(const v8::WeakCallbackInfo<
                                   v8::Persistent<v8::External>>& data) {
  auto isolate = data.GetIsolate();
  auto persistent = data.GetParameter();
  auto myCollection = v8::Local<v8::External>::New(isolate, *persistent);
  auto collection = static_cast<LogicalCollection*>(myCollection->Value());
  TRI_GET_GLOBALS();

  v8g->decreaseActiveExternals();

  // decrease the reference-counter for the database
  TRI_ASSERT(!collection->vocbase().isDangling());

  // find the persistent handle
#if ARANGODB_ENABLE_MAINTAINER_MODE
  auto const& it = v8g->JSCollections.find(collection);
  TRI_ASSERT(it != v8g->JSCollections.end());
#endif

  // dispose and clear the persistent handle
  v8g->JSCollections[collection].Reset();
  v8g->JSCollections.erase(collection);

  if (!collection->isLocal()) {
    collection->vocbase().release();
    delete collection;
  } else {
    collection->vocbase().release();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief wraps a LogicalCollection
/// Note that if collection is a local collection, then the object will never
/// be freed. If it is not a local collection (coordinator case), then delete
/// will be called when the V8 object is garbage collected.
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Object> WrapCollection(v8::Isolate* isolate,
                                      arangodb::LogicalCollection const* collection) {
  v8::EscapableHandleScope scope(isolate);

  TRI_GET_GLOBALS();
  TRI_GET_GLOBAL(VocbaseColTempl, v8::ObjectTemplate);
  v8::Handle<v8::Object> result = VocbaseColTempl->NewInstance();

  if (!result.IsEmpty()) {
    LogicalCollection* nonconstCollection =
        const_cast<LogicalCollection*>(collection);

    result->SetInternalField(SLOT_CLASS_TYPE,
                             v8::Integer::New(isolate, WRP_VOCBASE_COL_TYPE));
    result->SetInternalField(SLOT_CLASS,
                             v8::External::New(isolate, nonconstCollection));

    auto const& it = v8g->JSCollections.find(nonconstCollection);

    if (it == v8g->JSCollections.end()) {
      // increase the reference-counter for the database
      TRI_ASSERT(!nonconstCollection->vocbase().isDangling());
      nonconstCollection->vocbase().forceUse();

      try {
        auto externalCollection = v8::External::New(isolate, nonconstCollection);

        result->SetInternalField(SLOT_EXTERNAL, externalCollection);

        v8g->JSCollections[nonconstCollection].Reset(isolate, externalCollection);
        v8g->JSCollections[nonconstCollection].SetWeak(&v8g->JSCollections[nonconstCollection],
                                                       WeakCollectionCallback,
                                                       v8::WeakCallbackType::kFinalizer);
        v8g->increaseActiveExternals();
      } catch (...) {
        nonconstCollection->vocbase().release();
        throw;
      }
    } else {
      auto myCollection = v8::Local<v8::External>::New(isolate, it->second);

      result->SetInternalField(SLOT_EXTERNAL, myCollection);
    }
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
