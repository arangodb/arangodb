////////////////////////////////////////////////////////////////////////////////
/// @brief V8-vocbase bridge
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
/// @author Dr. Frank Celler
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "v8-collection.h"
#include "v8-vocbaseprivate.h"

#include "Basics/conversions.h"
#include "V8/v8-conv.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::arango;
using namespace triagens::rest;

////////////////////////////////////////////////////////////////////////////////
/// @brief slot for a "collection"
////////////////////////////////////////////////////////////////////////////////

static int const SLOT_COLLECTION = 2;

////////////////////////////////////////////////////////////////////////////////
/// @brief free a coordinator collection
////////////////////////////////////////////////////////////////////////////////

void FreeCoordinatorCollection (TRI_vocbase_col_t* collection) {
  TRI_DestroyReadWriteLock(&collection->_lock);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, collection);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief releases a collection
////////////////////////////////////////////////////////////////////////////////

void ReleaseCollection (TRI_vocbase_col_t const* collection) {
  TRI_ReleaseCollectionVocBase(collection->_vocbase, const_cast<TRI_vocbase_col_t*>(collection));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convert a collection info into a TRI_vocbase_col_t
////////////////////////////////////////////////////////////////////////////////

TRI_vocbase_col_t* CoordinatorCollection (TRI_vocbase_t* vocbase,
                                          CollectionInfo const& ci) {
  TRI_vocbase_col_t* c = static_cast<TRI_vocbase_col_t*>(TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_vocbase_col_t), false));

  if (c == nullptr) {
    return nullptr;
  }

  c->_isLocal    = false;
  c->_vocbase    = vocbase;
  c->_type       = ci.type();
  c->_cid        = ci.id();
  c->_planId     = ci.id();
  c->_status     = ci.status();
  c->_collection = 0;

  std::string const name = ci.name();

  memset(c->_name, 0, TRI_COL_NAME_LENGTH + 1);
  memcpy(c->_name, name.c_str(), name.size());
  memset(c->_path, 0, TRI_COL_PATH_LENGTH + 1);

  memset(c->_dbName, 0, TRI_COL_NAME_LENGTH + 1);
  memcpy(c->_dbName, vocbase->_name, strlen(vocbase->_name));

  c->_canDrop   = true;
  c->_canUnload = true;
  c->_canRename = true;

  if (TRI_IsSystemNameCollection(c->_name)) {
    // a few system collections have special behavior
    if (TRI_EqualString(c->_name, TRI_COL_NAME_USERS) ||
        TRI_IsPrefixString(c->_name, TRI_COL_NAME_STATISTICS)) {
      // these collections cannot be dropped or renamed
      c->_canDrop   = false;
      c->_canRename = false;
    }
  }

  TRI_InitReadWriteLock(&c->_lock);

  return c;
}

///////////////////////////////////////////////////////////////////////////////
/// @brief check if a name belongs to a collection
////////////////////////////////////////////////////////////////////////////////

bool EqualCollection (CollectionNameResolver const* resolver,
                      string const& collectionName,
                      TRI_vocbase_col_t const* collection) {
  if (collectionName == StringUtils::itoa(collection->_cid)) {
    return true;
  }

  if (collectionName == string(collection->_name)) {
    return true;
  }

  if (ServerState::instance()->isCoordinator()) {
    if (collectionName == resolver->getCollectionNameCluster(collection->_cid)) {
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

static inline v8::Handle<v8::Value> V8CollectionId (TRI_voc_cid_t cid) {
  char buffer[21];
  size_t len = TRI_StringUInt64InPlace((uint64_t) cid, (char*) &buffer);

  return v8::String::New((const char*) buffer, (int) len);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief weak reference callback for collections
////////////////////////////////////////////////////////////////////////////////

static void WeakCollectionCallback (v8::Isolate* isolate,
                                    v8::Persistent<v8::Value> object,
                                    void* parameter) {
  TRI_v8_global_t* v8g = static_cast<TRI_v8_global_t*>(v8::Isolate::GetCurrent()->GetData());
  TRI_vocbase_col_t* collection = static_cast<TRI_vocbase_col_t*>(parameter);

  v8g->_hasDeadObjects = true;

  v8::HandleScope scope; // do not remove, will fail otherwise!!

  // decrease the reference-counter for the database
  TRI_ReleaseVocBase(collection->_vocbase);

  // find the persistent handle
  v8::Persistent<v8::Value> persistent = v8g->JSCollections[collection];
  v8g->JSCollections.erase(collection);

  if (! collection->_isLocal) {
    FreeCoordinatorCollection(collection);
  }

  // dispose and clear the persistent handle
  persistent.Dispose(isolate);
  persistent.Clear();
}

/////////////////////////////////////////////////////////////////////////////////
/// @brief wraps a TRI_vocbase_col_t
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Object> WrapCollection (TRI_vocbase_col_t const* collection) {
  v8::HandleScope scope;

  TRI_v8_global_t* v8g = static_cast<TRI_v8_global_t*>(v8::Isolate::GetCurrent()->GetData());
  v8::Handle<v8::Object> result = v8g->VocbaseColTempl->NewInstance();

  if (! result.IsEmpty()) {
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    TRI_vocbase_col_t* c = const_cast<TRI_vocbase_col_t*>(collection);

    result->SetInternalField(SLOT_CLASS_TYPE, v8::Integer::New(WRP_VOCBASE_COL_TYPE));
    result->SetInternalField(SLOT_CLASS, v8::External::New(c));

    map< void*, v8::Persistent<v8::Value> >::iterator i = v8g->JSCollections.find(c);

    if (i == v8g->JSCollections.end()) {
      // increase the reference-counter for the database
      TRI_UseVocBase(collection->_vocbase);

      v8::Persistent<v8::Value> persistent = v8::Persistent<v8::Value>::New(isolate, v8::External::New(c));
      result->SetInternalField(SLOT_COLLECTION, persistent);

      v8g->JSCollections[c] = persistent;
      persistent.MakeWeak(isolate, c, WeakCollectionCallback);
    }
    else {
      result->SetInternalField(SLOT_COLLECTION, i->second);
    }

    result->Set(v8g->_IdKey, V8CollectionId(collection->_cid), v8::ReadOnly);
    result->Set(v8g->_DbNameKey, v8::String::New(collection->_dbName));
    result->Set(v8g->VersionKey, v8::Number::New((double) collection->_internalVersion), v8::DontEnum);
  }

  return scope.Close(result);
}

