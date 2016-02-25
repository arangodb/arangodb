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
#include "V8/v8-conv.h"
#include "V8Server/v8-vocbaseprivate.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

////////////////////////////////////////////////////////////////////////////////
/// @brief slot for a "collection"
////////////////////////////////////////////////////////////////////////////////

static int const SLOT_COLLECTION = 2;

////////////////////////////////////////////////////////////////////////////////
/// @brief releases a collection
////////////////////////////////////////////////////////////////////////////////

void ReleaseCollection(TRI_vocbase_col_t const* collection) {
  TRI_ReleaseCollectionVocBase(collection->_vocbase,
                               const_cast<TRI_vocbase_col_t*>(collection));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convert a collection info into a TRI_vocbase_col_t
////////////////////////////////////////////////////////////////////////////////

TRI_vocbase_col_t* CoordinatorCollection(TRI_vocbase_t* vocbase,
                                         CollectionInfo const& ci) {
  auto c = std::make_unique<TRI_vocbase_col_t>(vocbase, ci.type(), ci.name(),
                                               ci.id(), "");

  c->_isLocal = false;
  c->_planId = ci.id();
  c->_status = ci.status();

  return c.release();
}

///////////////////////////////////////////////////////////////////////////////
/// @brief check if a name belongs to a collection
////////////////////////////////////////////////////////////////////////////////

bool EqualCollection(CollectionNameResolver const* resolver,
                     std::string const& collectionName,
                     TRI_vocbase_col_t const* collection) {
  if (collectionName == StringUtils::itoa(collection->_cid)) {
    return true;
  }

  if (collectionName == std::string(collection->_name)) {
    return true;
  }

  if (ServerState::instance()->isCoordinator()) {
    if (collectionName ==
        resolver->getCollectionNameCluster(collection->_cid)) {
      return true;
    }
    return false;
  }

  if (collectionName == resolver->getCollectionName(collection->_cid)) {
    return true;
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a v8 collection id value from the internal collection id
////////////////////////////////////////////////////////////////////////////////

static inline v8::Handle<v8::Value> V8CollectionId(v8::Isolate* isolate,
                                                   TRI_voc_cid_t cid) {
  char buffer[21];
  size_t len = TRI_StringUInt64InPlace((uint64_t)cid, (char*)&buffer);

  return TRI_V8_PAIR_STRING((char const*)buffer, (int)len);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief weak reference callback for collections
////////////////////////////////////////////////////////////////////////////////

static void WeakCollectionCallback(const v8::WeakCallbackData<
    v8::External, v8::Persistent<v8::External>>& data) {
  auto isolate = data.GetIsolate();
  auto persistent = data.GetParameter();
  auto myCollection = v8::Local<v8::External>::New(isolate, *persistent);
  auto collection = static_cast<TRI_vocbase_col_t*>(myCollection->Value());
  TRI_GET_GLOBALS();

  v8g->decreaseActiveExternals();

  // decrease the reference-counter for the database
  TRI_ReleaseVocBase(collection->_vocbase);

// find the persistent handle
#if ARANGODB_ENABLE_MAINTAINER_MODE
  auto const& it = v8g->JSCollections.find(collection);
  TRI_ASSERT(it != v8g->JSCollections.end());
#endif

  // dispose and clear the persistent handle
  v8g->JSCollections[collection].Reset();
  v8g->JSCollections.erase(collection);

  if (!collection->_isLocal) {
    delete collection;
  }
}

/////////////////////////////////////////////////////////////////////////////////
/// @brief wraps a TRI_vocbase_col_t
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Object> WrapCollection(v8::Isolate* isolate,
                                      TRI_vocbase_col_t const* collection) {
  v8::EscapableHandleScope scope(isolate);

  TRI_GET_GLOBALS();
  TRI_GET_GLOBAL(VocbaseColTempl, v8::ObjectTemplate);
  v8::Handle<v8::Object> result = VocbaseColTempl->NewInstance();

  if (!result.IsEmpty()) {
    TRI_vocbase_col_t* nonconstCollection =
        const_cast<TRI_vocbase_col_t*>(collection);

    result->SetInternalField(SLOT_CLASS_TYPE,
                             v8::Integer::New(isolate, WRP_VOCBASE_COL_TYPE));
    result->SetInternalField(SLOT_CLASS,
                             v8::External::New(isolate, nonconstCollection));

    auto const& it = v8g->JSCollections.find(nonconstCollection);

    if (it == v8g->JSCollections.end()) {
      // increase the reference-counter for the database
      TRI_UseVocBase(collection->_vocbase);
      auto externalCollection = v8::External::New(isolate, nonconstCollection);

      result->SetInternalField(SLOT_COLLECTION, externalCollection);

      v8g->JSCollections[nonconstCollection].Reset(isolate, externalCollection);
      v8g->JSCollections[nonconstCollection].SetWeak(
          &v8g->JSCollections[nonconstCollection], WeakCollectionCallback);
      v8g->increaseActiveExternals();
    } else {
      auto myCollection = v8::Local<v8::External>::New(isolate, it->second);

      result->SetInternalField(SLOT_COLLECTION, myCollection);
    }
    TRI_GET_GLOBAL_STRING(_IdKey);
    TRI_GET_GLOBAL_STRING(_DbNameKey);
    TRI_GET_GLOBAL_STRING(VersionKeyHidden);
    result->ForceSet(_IdKey, V8CollectionId(isolate, collection->_cid),
                     v8::ReadOnly);
    result->Set(_DbNameKey, TRI_V8_STRING(collection->dbName().c_str()));
    result->ForceSet(
        VersionKeyHidden,
        v8::Integer::NewFromUnsigned(isolate, collection->_internalVersion),
        v8::DontEnum);
  }

  return scope.Escape<v8::Object>(result);
}
