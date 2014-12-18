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

#include "v8-wrapshapedjson.h"
#include "v8-vocbaseprivate.h"

#include "Basics/conversions.h"
#include "V8/v8-conv.h"
#include "VocBase/key-generator.h"
#include "Utils/transactions.h"
#include "Utils/V8TransactionContext.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::arango;
using namespace triagens::rest;

////////////////////////////////////////////////////////////////////////////////
/// @brief wrapped class for TRI_shaped_json_t
///
/// Layout:
/// - SLOT_CLASS_TYPE
/// - SLOT_CLASS
/// - SLOT_BARRIER
////////////////////////////////////////////////////////////////////////////////

static int32_t const WRP_SHAPED_JSON_TYPE = 4;

////////////////////////////////////////////////////////////////////////////////
/// @brief slot for a "barrier"
////////////////////////////////////////////////////////////////////////////////

static int const SLOT_BARRIER = 2;

////////////////////////////////////////////////////////////////////////////////
/// @brief get basic attributes (_id, _key, _rev, _from, _to) for a JavaScript
/// document object
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Object> SetBasicDocumentAttributesJs (v8::Isolate* isolate,
                                                            CollectionNameResolver const* resolver,
                                                            TRI_v8_global_t* v8g,
                                                            TRI_voc_cid_t cid,
                                                            TRI_df_marker_t const* marker) {
  v8::EscapableHandleScope scope(isolate);
  TRI_ASSERT(marker != nullptr);

  v8::Handle<v8::Object> result = v8::Object::New(isolate);

  // buffer that we'll use for generating _id, _key, _rev, _from and _to values
  // using a single buffer will avoid several memory allocation
  char buffer[TRI_COL_NAME_LENGTH + TRI_VOC_KEY_MAX_LENGTH + 2];

  // _id
  size_t len = resolver->getCollectionName(buffer, cid);
  char const* docKey = TRI_EXTRACT_MARKER_KEY(marker);
  TRI_ASSERT(docKey != nullptr);
  size_t keyLength = strlen(docKey);
  buffer[len] = '/';
  memcpy(buffer + len + 1, docKey, keyLength);
  TRI_GET_GLOBAL_STRING(_IdKey);
  result->ForceSet(_IdKey, TRI_V8_PAIR_STRING(buffer, (int) (len + keyLength + 1)));

  // _key
  TRI_GET_GLOBAL_STRING(_KeyKey);
  result->ForceSet(_KeyKey, TRI_V8_PAIR_STRING(buffer + len + 1, (int) keyLength));

  // _rev
  TRI_voc_rid_t rid = TRI_EXTRACT_MARKER_RID(marker);
  TRI_ASSERT(rid > 0);
  len = TRI_StringUInt64InPlace((uint64_t) rid, (char*) &buffer);
  TRI_GET_GLOBAL_STRING(_RevKey);
  result->ForceSet(_RevKey, TRI_V8_PAIR_STRING((char const*) buffer, (int) len));

  TRI_df_marker_type_t type = marker->_type;
  char const* base = reinterpret_cast<char const*>(marker);

  if (type == TRI_DOC_MARKER_KEY_EDGE) {
    TRI_doc_edge_key_marker_t const* m = reinterpret_cast<TRI_doc_edge_key_marker_t const*>(marker); 

    // _from
    len = resolver->getCollectionNameCluster(buffer, m->_fromCid);
    keyLength = strlen(base + m->_offsetFromKey);
    buffer[len] = '/';
    memcpy(buffer + len + 1, base + m->_offsetFromKey, keyLength);
    TRI_GET_GLOBAL_STRING(_FromKey);
    result->ForceSet(_FromKey, TRI_V8_PAIR_STRING(buffer, (int) (len + keyLength + 1)));

    // _to
    if (m->_fromCid != m->_toCid) {
      // only lookup collection name if we haven't done it yet
      len = resolver->getCollectionNameCluster(buffer, m->_toCid);
    }
    keyLength = strlen(base + m->_offsetToKey);
    buffer[len] = '/';
    memcpy(buffer + len + 1, base + m->_offsetToKey, keyLength);
    TRI_GET_GLOBAL_STRING(_ToKey);
    result->ForceSet(_ToKey, TRI_V8_PAIR_STRING(buffer, (int) (len + keyLength + 1)));
  }
  else if (type == TRI_WAL_MARKER_EDGE) {
    triagens::wal::edge_marker_t const* m = reinterpret_cast<triagens::wal::edge_marker_t const*>(marker);

    // _from
    len = resolver->getCollectionNameCluster(buffer, m->_fromCid);
    keyLength = strlen(base + m->_offsetFromKey);
    buffer[len] = '/';
    memcpy(buffer + len + 1, base + m->_offsetFromKey, keyLength);
    TRI_GET_GLOBAL_STRING(_FromKey);
    result->ForceSet(_FromKey, TRI_V8_PAIR_STRING(buffer, (int) (len + keyLength + 1)));

    // _to
    if (m->_fromCid != m->_toCid) {
      // only lookup collection name if we haven't done it yet
      len = resolver->getCollectionNameCluster(buffer, m->_toCid);
    }
    keyLength = strlen(base + m->_offsetToKey);
    buffer[len] = '/';
    memcpy(buffer + len + 1, base + m->_offsetToKey, keyLength);
    TRI_GET_GLOBAL_STRING(_ToKey);
    result->ForceSet(_ToKey, TRI_V8_PAIR_STRING(buffer, (int) (len + keyLength + 1)));
  }

  return scope.Escape<v8::Object>(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add basic attributes (_id, _key, _rev, _from, _to) to a ShapedJson
/// object
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Object> SetBasicDocumentAttributesShaped (v8::Isolate* isolate,
                                                                CollectionNameResolver const* resolver,
                                                                TRI_v8_global_t* v8g,
                                                                TRI_voc_cid_t cid,
                                                                TRI_df_marker_t const* marker,
                                                                v8::Handle<v8::Object>& result) {
  v8::EscapableHandleScope scope(isolate);
  TRI_ASSERT(marker != nullptr);

  // buffer that we'll use for generating _id, _key, _rev, _from and _to values
  // using a single buffer will avoid several memory allocation
  char buffer[TRI_COL_NAME_LENGTH + TRI_VOC_KEY_MAX_LENGTH + 2];

  // _id
  size_t len = resolver->getCollectionName(buffer, cid);
  char const* docKey = TRI_EXTRACT_MARKER_KEY(marker);
  TRI_ASSERT(docKey != nullptr);
  size_t keyLength = strlen(docKey);
  buffer[len] = '/';
  memcpy(buffer + len + 1, docKey, keyLength);
  TRI_GET_GLOBAL_STRING(_IdKey);
  result->ForceSet(_IdKey, TRI_V8_PAIR_STRING(buffer, (int) (len + keyLength + 1)));

  TRI_df_marker_type_t type = marker->_type;
  char const* base = reinterpret_cast<char const*>(marker);

  if (type == TRI_DOC_MARKER_KEY_EDGE) {
    TRI_doc_edge_key_marker_t const* m = reinterpret_cast<TRI_doc_edge_key_marker_t const*>(marker); 

    // _from
    len = resolver->getCollectionNameCluster(buffer, m->_fromCid);
    keyLength = strlen(base + m->_offsetFromKey);
    buffer[len] = '/';
    memcpy(buffer + len + 1, base + m->_offsetFromKey, keyLength);
    TRI_GET_GLOBAL_STRING(_FromKey);
    result->ForceSet(_FromKey, TRI_V8_PAIR_STRING(buffer, (int) (len + keyLength + 1)));

    // _to
    if (m->_fromCid != m->_toCid) {
      // only lookup collection name if we haven't done it yet
      len = resolver->getCollectionNameCluster(buffer, m->_toCid);
    }
    keyLength = strlen(base + m->_offsetToKey);
    buffer[len] = '/';
    memcpy(buffer + len + 1, base + m->_offsetToKey, keyLength);
    TRI_GET_GLOBAL_STRING(_ToKey);
    result->ForceSet(_ToKey, TRI_V8_PAIR_STRING(buffer, (int) (len + keyLength + 1)));
  }
  else if (type == TRI_WAL_MARKER_EDGE) {
    triagens::wal::edge_marker_t const* m = reinterpret_cast<triagens::wal::edge_marker_t const*>(marker);

    // _from
    len = resolver->getCollectionNameCluster(buffer, m->_fromCid);
    keyLength = strlen(base + m->_offsetFromKey);
    buffer[len] = '/';
    memcpy(buffer + len + 1, base + m->_offsetFromKey, keyLength);
    TRI_GET_GLOBAL_STRING(_FromKey);
    result->ForceSet(_FromKey, TRI_V8_PAIR_STRING(buffer, (int) (len + keyLength + 1)));

    // _to
    if (m->_fromCid != m->_toCid) {
      // only lookup collection name if we haven't done it yet
      len = resolver->getCollectionNameCluster(buffer, m->_toCid);
    }
    keyLength = strlen(base + m->_offsetToKey);
    buffer[len] = '/';
    memcpy(buffer + len + 1, base + m->_offsetToKey, keyLength);
    TRI_GET_GLOBAL_STRING(_ToKey);
    result->ForceSet(_ToKey, TRI_V8_PAIR_STRING(buffer, (int) (len + keyLength + 1)));
  }
  return scope.Escape<v8::Object>(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief weak reference callback for a barrier
////////////////////////////////////////////////////////////////////////////////

static void WeakBarrierCallback (const v8::WeakCallbackData<v8::External, v8::Persistent<v8::External>>& data) {
  auto isolate      = data.GetIsolate();
  auto persistent   = data.GetParameter();
  auto myBarrier    = v8::Local<v8::External>::New(isolate, *persistent);

  TRI_barrier_blocker_t* barrier = static_cast<TRI_barrier_blocker_t*>(myBarrier->Value());

  TRI_GET_GLOBALS();

  TRI_ASSERT(barrier != nullptr);

  v8g->_hasDeadObjects = true;

  LOG_TRACE("weak-callback for barrier called");

  // find the persistent handle
  v8g->JSBarriers[barrier].Reset();
  v8g->JSBarriers.erase(barrier);

  // get the vocbase pointer from the barrier
  TRI_vocbase_t* vocbase = barrier->base._container->_collection->_vocbase;

  TRI_FreeBarrier(barrier, false /* fromTransaction */ );
  // we don't need the barrier anymore, maybe a transaction is still using it

  if (vocbase != nullptr) {
    // decrease the reference-counter for the database
    TRI_ReleaseVocBase(vocbase);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief wraps a TRI_shaped_json_t
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Value> TRI_WrapShapedJson (v8::Isolate* isolate,
                                          triagens::arango::CollectionNameResolver const* resolver,
                                          TRI_barrier_t* barrier,
                                          TRI_voc_cid_t cid,
                                          TRI_document_collection_t* collection,
                                          void const* data) {
  v8::EscapableHandleScope scope(isolate);
  TRI_df_marker_t const* marker = static_cast<TRI_df_marker_t const*>(data);
  TRI_ASSERT(marker != nullptr);
  TRI_ASSERT(barrier != nullptr);
  TRI_ASSERT(collection != nullptr);

  TRI_GET_GLOBALS();
  bool const doCopy = TRI_IsWalDataMarkerDatafile(marker);

  if (doCopy) {
    // we'll create a full copy of the document
    TRI_shaper_t* shaper = collection->getShaper();  // PROTECTED by trx from above

    TRI_shaped_json_t json;
    TRI_EXTRACT_SHAPED_JSON_MARKER(json, marker);  // PROTECTED by trx from above

    TRI_shape_t const* shape = shaper->lookupShapeId(shaper, json._sid);

    if (shape == nullptr) {
      return v8::Object::New(isolate);
    }

    v8::Handle<v8::Object> result = SetBasicDocumentAttributesJs(isolate, resolver, v8g, cid, marker);

    return scope.Escape<v8::Value>(TRI_JsonShapeData(isolate, result, shaper, shape, json._data.data, json._data.length));
  }

  // we'll create a document stub, with a pointer into the datafile

  // create the new handle to return, and set its template type
  TRI_GET_GLOBAL(ShapedJsonTempl, v8::ObjectTemplate);
  v8::Handle<v8::Object> result = ShapedJsonTempl->NewInstance();

  if (result.IsEmpty()) {
    // error
    return scope.Escape<v8::Value>(result);
  }

  // point the 0 index Field to the c++ pointer for unwrapping later
  result->SetInternalField(SLOT_CLASS_TYPE, v8::Integer::New(isolate, WRP_SHAPED_JSON_TYPE));
  result->SetInternalField(SLOT_CLASS, v8::External::New(isolate, (void*) marker));

  // tell everyone else that this barrier is used by an external
  reinterpret_cast<TRI_barrier_blocker_t*>(barrier)->_usedByExternal = true;

  auto it = v8g->JSBarriers.find(barrier);

  if (it == v8g->JSBarriers.end()) {
    // increase the reference-counter for the database
    TRI_ASSERT(barrier->_container != nullptr);
    TRI_ASSERT(barrier->_container->_collection != nullptr);
    TRI_UseVocBase(barrier->_container->_collection->_vocbase);
    auto externalBarrier = v8::External::New(isolate, barrier);
    v8::Persistent<v8::External>& per(v8g->JSBarriers[barrier]);
    per.Reset(isolate, externalBarrier);
    result->SetInternalField(SLOT_BARRIER, externalBarrier);
    per.SetWeak(&per, WeakBarrierCallback);
  }
  else {
    auto myBarrier = v8::Local<v8::External>::New(isolate, it->second);
      
    result->SetInternalField(SLOT_BARRIER, myBarrier);
  }

  return scope.Escape<v8::Value>(SetBasicDocumentAttributesShaped(isolate, resolver, v8g, cid, marker, result));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief selects the keys from the shaped json
////////////////////////////////////////////////////////////////////////////////

static void KeysOfShapedJson (const v8::PropertyCallbackInfo<v8::Array>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  // sanity check
  v8::Handle<v8::Object> self = args.Holder();

  if (self->InternalFieldCount() <= SLOT_BARRIER) {
    TRI_V8_RETURN(v8::Array::New(isolate));
  }

  // get shaped json
  void* marker = TRI_UnwrapClass<void*>(self, WRP_SHAPED_JSON_TYPE);

  if (marker == nullptr) {
    TRI_V8_RETURN(v8::Array::New(isolate));
  }

  TRI_barrier_t* barrier = static_cast<TRI_barrier_t*>(v8::Handle<v8::External>::Cast(self->GetInternalField(SLOT_BARRIER))->Value());
  TRI_document_collection_t* collection = barrier->_container->_collection;

  // check for array shape
  TRI_shaper_t* shaper = collection->getShaper();  // PROTECTED by BARRIER, checked by RUNTIME

  TRI_shape_sid_t sid;
  TRI_EXTRACT_SHAPE_IDENTIFIER_MARKER(sid, marker);

  TRI_shape_t const* shape = shaper->lookupShapeId(shaper, sid);

  if (shape == nullptr || shape->_type != TRI_SHAPE_ARRAY) {
    TRI_V8_RETURN(v8::Array::New(isolate));
  }

  TRI_array_shape_t const* s;
  TRI_shape_aid_t const* aids;
  char const* qtr;

  // shape is an array
  s = (TRI_array_shape_t const*) shape;

  // number of entries
  TRI_shape_size_t const n = s->_fixedEntries + s->_variableEntries;

  // calculate position of attribute ids
  qtr = (char const*) shape;
  qtr += sizeof(TRI_array_shape_t);
  qtr += n * sizeof(TRI_shape_sid_t);
  aids = (TRI_shape_aid_t const*) qtr;

  TRI_df_marker_type_t type = static_cast<TRI_df_marker_t const*>(marker)->_type;
  bool isEdge = (type == TRI_DOC_MARKER_KEY_EDGE || type == TRI_WAL_MARKER_EDGE);
  
  v8::Handle<v8::Array> result = v8::Array::New(isolate, (int) n + 3 + (isEdge ? 2 : 0));
  uint32_t count = 0;
  
  TRI_GET_GLOBALS();
  TRI_GET_GLOBAL_STRING(_IdKey);
  TRI_GET_GLOBAL_STRING(_RevKey);
  TRI_GET_GLOBAL_STRING(_KeyKey);
  result->Set(count++, _IdKey);
  result->Set(count++, _RevKey);
  result->Set(count++, _KeyKey);
  
  if (isEdge) {
    TRI_GET_GLOBAL_STRING(_FromKey);
    TRI_GET_GLOBAL_STRING(_ToKey);
    result->Set(count++, _FromKey);
    result->Set(count++, _ToKey);
  }

  for (TRI_shape_size_t i = 0;  i < n;  ++i, ++aids) {
    /// TODO: avoid strlen here!
    char const* att = shaper->lookupAttributeId(shaper, *aids);

    if (att != nullptr) {
      result->Set(count++, TRI_V8_STRING(att));
    }
  }

  TRI_V8_RETURN(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief copy all shaped json attributes into the object so we have regular
/// JavaScript attributes that can be modified
////////////////////////////////////////////////////////////////////////////////

static void CopyAttributes (v8::Isolate* isolate,
                            v8::Handle<v8::Object> self, 
                            void* marker) {
  TRI_barrier_t* barrier = static_cast<TRI_barrier_t*>(v8::Handle<v8::External>::Cast(self->GetInternalField(SLOT_BARRIER))->Value());
  TRI_document_collection_t* collection = barrier->_container->_collection;

  // check for array shape
  TRI_shaper_t* shaper = collection->getShaper();  // PROTECTED by BARRIER, checked by RUNTIME

  TRI_shape_sid_t sid;
  TRI_EXTRACT_SHAPE_IDENTIFIER_MARKER(sid, marker);

  TRI_shape_t const* shape = shaper->lookupShapeId(shaper, sid);

  if (shape == nullptr || shape->_type != TRI_SHAPE_ARRAY) {
    return;
  }
    
  // copy _key and _rev

  // _key
  TRI_GET_GLOBALS();
  char buffer[TRI_VOC_KEY_MAX_LENGTH + 1];
  char const* docKey = TRI_EXTRACT_MARKER_KEY(static_cast<TRI_df_marker_t const*>(marker));
  TRI_ASSERT(docKey != nullptr);
  size_t keyLength = strlen(docKey);// TODO: avoid strlen
  memcpy(buffer, docKey, keyLength);
  TRI_GET_GLOBAL_STRING(_KeyKey);
  self->ForceSet(_KeyKey, TRI_V8_PAIR_STRING(buffer, (int) keyLength));

   // _rev  
  TRI_voc_rid_t rid = TRI_EXTRACT_MARKER_RID(static_cast<TRI_df_marker_t const*>(marker));
  TRI_ASSERT(rid > 0);
  TRI_GET_GLOBAL_STRING(_RevKey);
  size_t len = TRI_StringUInt64InPlace((uint64_t) rid, (char*) &buffer);
  self->ForceSet(_RevKey, TRI_V8_PAIR_STRING((char const*) buffer, (int) len));

  // finally insert the dynamic attributes from the shaped json
  TRI_array_shape_t const* s;
  TRI_shape_aid_t const* aids;
  char const* qtr;

  // shape is an array
  s = (TRI_array_shape_t const*) shape;

  // number of entries
  TRI_shape_size_t const n = s->_fixedEntries + s->_variableEntries;

  // calculate position of attribute ids
  qtr = (char const*) shape;
  qtr += sizeof(TRI_array_shape_t);
  qtr += n * sizeof(TRI_shape_sid_t);
  aids = (TRI_shape_aid_t const*) qtr;
  
  TRI_shaped_json_t document;
  TRI_EXTRACT_SHAPED_JSON_MARKER(document, marker);
  
  TRI_shaped_json_t json;

  for (TRI_shape_size_t i = 0;  i < n;  ++i, ++aids) {
    /// TODO: avoid strlen
    char const* att = shaper->lookupAttributeId(shaper, *aids);
    
    if (att != nullptr) {
      TRI_shape_pid_t pid = shaper->lookupAttributePathByName(shaper, att);
      
      if (pid != 0) {
        bool ok = TRI_ExtractShapedJsonVocShaper(shaper, &document, 0, pid, &json, &shape);
  
        if (ok && shape != nullptr) {
          /// TODO: avoid strlen
          self->ForceSet(TRI_V8_STRING(att), TRI_JsonShapeData(isolate, shaper, shape, json._data.data, json._data.length));
        }
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief selects a named attribute from the shaped json
////////////////////////////////////////////////////////////////////////////////

static void MapGetNamedShapedJson (v8::Local<v8::String> name,
                                  const v8::PropertyCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  // sanity check
  v8::Handle<v8::Object> self = args.Holder();

  if (self->InternalFieldCount() <= SLOT_BARRIER) {
    // we better not throw here... otherwise this will cause a segfault
    TRI_V8_RETURN(v8::Handle<v8::Value>());
  }

  // get shaped json
  void* marker = TRI_UnwrapClass<void*>(self, WRP_SHAPED_JSON_TYPE);

  if (marker == nullptr) {
    TRI_V8_RETURN(v8::Handle<v8::Value>());
  }

  // convert the JavaScript string to a string
  // we take the fast path here and don't normalize the string
  v8::String::Utf8Value const str(name);
  string const key(*str, (size_t) str.length());

  if (key.empty()) {
    TRI_V8_RETURN(v8::Handle<v8::Value>());
  }

  if (key[0] == '_') { 
    char buffer[TRI_VOC_KEY_MAX_LENGTH + 1];

    if (key == TRI_VOC_ATTRIBUTE_KEY) {
      char const* docKey = TRI_EXTRACT_MARKER_KEY(static_cast<TRI_df_marker_t const*>(marker));
      TRI_ASSERT(docKey != nullptr);
      size_t keyLength = strlen(docKey);
      memcpy(buffer, docKey, keyLength);
      TRI_V8_RETURN_PAIR_STRING(buffer, (int) keyLength);
    }
    else if (key == TRI_VOC_ATTRIBUTE_REV) {
      TRI_voc_rid_t rid = TRI_EXTRACT_MARKER_RID(static_cast<TRI_df_marker_t const*>(marker));
      TRI_ASSERT(rid > 0);
      size_t len = TRI_StringUInt64InPlace((uint64_t) rid, (char*) &buffer);
      TRI_V8_RETURN_PAIR_STRING((char const*) buffer, (int) len);
    }

    if (key == TRI_VOC_ATTRIBUTE_ID || 
        key == TRI_VOC_ATTRIBUTE_FROM || 
        key == TRI_VOC_ATTRIBUTE_TO) {
      // strip reserved attributes
      TRI_V8_RETURN(v8::Handle<v8::Value>());
    }
  }

  // TODO: check whether accessing an attribute with a dot is actually possible, 
  // e.g. doc.a.b vs. doc["a.b"]
  if (strchr(key.c_str(), '.') != nullptr) {
    TRI_V8_RETURN(v8::Handle<v8::Value>());
  }

  // get the underlying collection
  TRI_barrier_t* barrier = static_cast<TRI_barrier_t*>(v8::Handle<v8::External>::Cast(self->GetInternalField(SLOT_BARRIER))->Value());
  TRI_ASSERT(barrier != nullptr);

  TRI_document_collection_t* collection = barrier->_container->_collection;

  // get shape accessor
  TRI_shaper_t* shaper = collection->getShaper();  // PROTECTED by trx here
  TRI_shape_pid_t pid = shaper->lookupAttributePathByName(shaper, key.c_str());

  if (pid == 0) {
    TRI_V8_RETURN(v8::Handle<v8::Value>());
  }

  TRI_shaped_json_t document;
  TRI_EXTRACT_SHAPED_JSON_MARKER(document, marker);

  TRI_shaped_json_t json;
  TRI_shape_t const* shape;

  bool ok = TRI_ExtractShapedJsonVocShaper(shaper, &document, 0, pid, &json, &shape);

  if (ok && shape != nullptr) {
    TRI_V8_RETURN(TRI_JsonShapeData(isolate, shaper, shape, json._data.data, json._data.length));
  }

  // we must not throw a v8 exception here because this will cause follow up errors
  TRI_V8_RETURN(v8::Handle<v8::Value>());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets a named attribute in the shaped json
/// Returns the value if the setter intercepts the request. 
/// Otherwise, returns an empty handle. 
////////////////////////////////////////////////////////////////////////////////

static void MapSetNamedShapedJson (v8::Local<v8::String> name,
                                   v8::Local<v8::Value> value,
                                   const v8::PropertyCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  // sanity check
  v8::Handle<v8::Object> self = args.Holder();

  if (self->InternalFieldCount() <= SLOT_BARRIER) {
    // we better not throw here... otherwise this will cause a segfault
    TRI_V8_RETURN(v8::Handle<v8::Value>());
  }

  // get shaped json
  void* marker = TRI_UnwrapClass<void*>(self, WRP_SHAPED_JSON_TYPE);

  if (marker == nullptr) {
    return;
  }

  if (self->HasRealNamedProperty(name)) {
    // object already has the property. use the regular property setter
    self->ForceSet(name, value);
    TRI_V8_RETURN_TRUE();
  }

  // copy all attributes from the shaped json into the object
  CopyAttributes(isolate, self, marker);

  // remove pointer to marker, so the object becomes stand-alone
  self->SetInternalField(SLOT_CLASS, v8::External::New(isolate, nullptr));

  // and now use the regular property setter
  self->ForceSet(name, value);
  TRI_V8_RETURN_TRUE();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes a named attribute from the shaped json
/// Returns a non-empty handle if the deleter intercepts the request. 
/// The return value is true if the property could be deleted and false otherwise. 
////////////////////////////////////////////////////////////////////////////////

static void MapDeleteNamedShapedJson (v8::Local<v8::String> name,
                                      const v8::PropertyCallbackInfo<v8::Boolean>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);
  
  // sanity check
  v8::Handle<v8::Object> self = args.Holder();

  if (self->InternalFieldCount() <= SLOT_BARRIER) {
    // we better not throw here... otherwise this will cause a segfault
    return;
  }

  // get shaped json
  void* marker = TRI_UnwrapClass<void*>(self, WRP_SHAPED_JSON_TYPE);

  if (marker == nullptr) {
    self->ForceDelete(name);
    TRI_V8_RETURN_TRUE();
  }
  
  // copy all attributes from the shaped json into the object
  CopyAttributes(isolate, self, marker);
  
  // remove pointer to marker, so the object becomes stand-alone
  self->SetInternalField(SLOT_CLASS, v8::External::New(isolate, nullptr));

  self->ForceDelete(name);
  TRI_V8_RETURN_TRUE();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check if a property is present
////////////////////////////////////////////////////////////////////////////////

static void PropertyQueryShapedJson (v8::Local<v8::String> name,
                                     const v8::PropertyCallbackInfo<v8::Integer>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  v8::Handle<v8::Object> self = args.Holder();

  // sanity check
  if (self->InternalFieldCount() <= SLOT_BARRIER) {
    TRI_V8_RETURN(v8::Handle<v8::Integer>());
  }

  // get shaped json
  void* marker = TRI_UnwrapClass<TRI_shaped_json_t>(self, WRP_SHAPED_JSON_TYPE);

  if (marker == nullptr) {
    TRI_V8_RETURN(v8::Handle<v8::Integer>());
  }

  // convert the JavaScript string to a string
  string const&& key = TRI_ObjectToString(name);

  if (key.empty()) {
    TRI_V8_RETURN(v8::Handle<v8::Integer>());
  }

  if (key[0] == '_') {
    if (key == TRI_VOC_ATTRIBUTE_KEY || 
        key == TRI_VOC_ATTRIBUTE_REV || 
        key == TRI_VOC_ATTRIBUTE_ID || 
        key == TRI_VOC_ATTRIBUTE_FROM || 
        key == TRI_VOC_ATTRIBUTE_TO) {
      TRI_V8_RETURN(v8::Handle<v8::Integer>(v8::Integer::New(isolate, v8::None)));
    }
  }

  // get underlying collection
  TRI_barrier_t* barrier = static_cast<TRI_barrier_t*>(v8::Handle<v8::External>::Cast(self->GetInternalField(SLOT_BARRIER))->Value());
  TRI_document_collection_t* collection = barrier->_container->_collection;

  // get shape accessor
  TRI_shaper_t* shaper = collection->getShaper();  // PROTECTED by BARRIER, checked by RUNTIME
  TRI_shape_pid_t pid = shaper->lookupAttributePathByName(shaper, key.c_str());

  if (pid == 0) {
    TRI_V8_RETURN(v8::Handle<v8::Integer>());
  }

  TRI_shape_sid_t sid;
  TRI_EXTRACT_SHAPE_IDENTIFIER_MARKER(sid, marker);

  if (sid == TRI_SHAPE_ILLEGAL) {
    // invalid shape
#ifdef TRI_ENABLE_MAINTAINER_MODE
    LOG_WARNING("invalid shape id '%llu' found for key '%s'", (unsigned long long) sid, key.c_str());
#endif
    TRI_V8_RETURN(v8::Handle<v8::Integer>());
  }

  TRI_shape_access_t const* acc = TRI_FindAccessorVocShaper(shaper, sid, pid);

  // key not found
  if (acc == nullptr || acc->_resultSid == TRI_SHAPE_ILLEGAL) {
    TRI_V8_RETURN(v8::Handle<v8::Integer>());
  }

  TRI_V8_RETURN(v8::Handle<v8::Integer>(v8::Integer::New(isolate, v8::None)));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief selects an indexed attribute from the shaped json
////////////////////////////////////////////////////////////////////////////////

static void MapGetIndexedShapedJson (uint32_t idx,
                                     const v8::PropertyCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  char buffer[11];
  size_t len = TRI_StringUInt32InPlace(idx, (char*) &buffer);

  v8::Local<v8::String> strVal = TRI_V8_PAIR_STRING((char*) &buffer, (int) len);

  MapGetNamedShapedJson(strVal, args);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets an indexed attribute in the shaped json
////////////////////////////////////////////////////////////////////////////////

static void MapSetIndexedShapedJson (uint32_t idx,
                                     v8::Local<v8::Value> value,
                                     const v8::PropertyCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  char buffer[11];
  size_t len = TRI_StringUInt32InPlace(idx, (char*) &buffer);

  v8::Local<v8::String> strVal = TRI_V8_PAIR_STRING((char*)&buffer, (int) len);

  MapSetNamedShapedJson(strVal, value, args);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief delete an indexed attribute in the shaped json
////////////////////////////////////////////////////////////////////////////////

static void MapDeleteIndexedShapedJson (uint32_t idx,
                                        const v8::PropertyCallbackInfo<v8::Boolean>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  char buffer[11];
  size_t len = TRI_StringUInt32InPlace(idx, (char*) &buffer);

  v8::Local<v8::String> strVal = TRI_V8_PAIR_STRING((char*) &buffer, (int) len);

  MapDeleteNamedShapedJson(strVal, args);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generate the TRI_shaped_json_t template
////////////////////////////////////////////////////////////////////////////////

void TRI_InitV8ShapedJson (v8::Isolate *isolate, 
                           v8::Handle<v8::Context> context,
                           size_t threadNumber,
                           TRI_v8_global_t* v8g) {
  v8::Handle<v8::ObjectTemplate> rt;
  v8::Handle<v8::FunctionTemplate> ft;

  ft = v8::FunctionTemplate::New(isolate);
  ft->SetClassName(TRI_V8_ASCII_STRING("ShapedJson"));

  rt = ft->InstanceTemplate();
  rt->SetInternalFieldCount(3);

  // accessor for named properties (e.g. doc.abcdef)
  rt->SetNamedPropertyHandler(MapGetNamedShapedJson,    // NamedPropertyGetter,
                              MapSetNamedShapedJson,    // NamedPropertySetter setter 
                              PropertyQueryShapedJson,  // NamedPropertyQuery,
                              MapDeleteNamedShapedJson, // NamedPropertyDeleter deleter,
                              KeysOfShapedJson          // NamedPropertyEnumerator,
                                                        // Handle<Value> data = Handle<Value>());
                              );

  // accessor for indexed properties (e.g. doc[1])
  rt->SetIndexedPropertyHandler(MapGetIndexedShapedJson,    // IndexedPropertyGetter,
                                MapSetIndexedShapedJson,    // IndexedPropertySetter,
                                0,                          // IndexedPropertyQuery,
                                MapDeleteIndexedShapedJson, // IndexedPropertyDeleter,
                                0                           // IndexedPropertyEnumerator,
                                                            // Handle<Value> data = Handle<Value>());
                               );

  v8g->ShapedJsonTempl.Reset(isolate, rt);
  TRI_AddGlobalFunctionVocbase(isolate, context, TRI_V8_ASCII_STRING("ShapedJson"), ft->GetFunction());
}

