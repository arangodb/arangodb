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

#include "Aql/ExecutionBlock.h"
#include "BasicsC/conversions.h"

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
/// @brief add basic attributes (_key, _rev, _from, _to) to a document object
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Object> AddBasicDocumentAttributes (
	CollectionNameResolver const* resolver,
	TRI_v8_global_t* v8g,
	TRI_voc_cid_t cid,
	TRI_df_marker_t const* marker,
	v8::Handle<v8::Object> result) {
  TRI_ASSERT(marker != nullptr);

  TRI_voc_rid_t rid = TRI_EXTRACT_MARKER_RID(marker);
  TRI_ASSERT(rid > 0);
  
  char const* docKey = TRI_EXTRACT_MARKER_KEY(marker);
  TRI_ASSERT(docKey != nullptr);

  result->Set(v8g->_IdKey, V8DocumentId(resolver->getCollectionName(cid), docKey), v8::ReadOnly);
  result->Set(v8g->_RevKey, V8RevisionId(rid), v8::ReadOnly);
  result->Set(v8g->_KeyKey, v8::String::New(docKey), v8::ReadOnly);

  TRI_df_marker_type_t type = marker->_type;
  char const* base = reinterpret_cast<char const*>(marker);

  if (type == TRI_DOC_MARKER_KEY_EDGE) {
    TRI_doc_edge_key_marker_t const* m = reinterpret_cast<TRI_doc_edge_key_marker_t const*>(marker); 

    result->Set(v8g->_FromKey, V8DocumentId(resolver->getCollectionNameCluster(m->_fromCid), base + m->_offsetFromKey));
    result->Set(v8g->_ToKey, V8DocumentId(resolver->getCollectionNameCluster(m->_toCid), base + m->_offsetToKey));
  }
  else if (type == TRI_WAL_MARKER_EDGE) {
    triagens::wal::edge_marker_t const* m = reinterpret_cast<triagens::wal::edge_marker_t const*>(marker);

    result->Set(v8g->_FromKey, V8DocumentId(resolver->getCollectionNameCluster(m->_fromCid), base + m->_offsetFromKey));
    result->Set(v8g->_ToKey, V8DocumentId(resolver->getCollectionNameCluster(m->_toCid), base + m->_offsetToKey));
  }

  return result;
}


////////////////////////////////////////////////////////////////////////////////
/// @brief weak reference callback for a barrier
////////////////////////////////////////////////////////////////////////////////

static void WeakBarrierCallback (v8::Isolate* isolate,
                                 v8::Persistent<v8::Value> object,
                                 void* parameter) {
  TRI_v8_global_t* v8g = static_cast<TRI_v8_global_t*>(isolate->GetData());
  TRI_barrier_blocker_t* barrier = (TRI_barrier_blocker_t*) parameter;

  TRI_ASSERT(barrier != nullptr);

  v8g->_hasDeadObjects = true;

  LOG_TRACE("weak-callback for barrier called");

  // find the persistent handle
  v8::Persistent<v8::Value> persistent = v8g->JSBarriers[barrier];
  v8g->JSBarriers.erase(barrier);

  // dispose and clear the persistent handle
  persistent.Dispose(isolate);
  persistent.Clear();

  // get the vocbase pointer from the barrier
  TRI_vocbase_t* vocbase = barrier->base._container->_collection->_vocbase;

  // mark that we don't need the barrier anymore
  barrier->_usedByExternal = false;

  // free the barrier
  if (! barrier->_usedByTransaction) {
    // the underlying transaction is over. we are the only user of this barrier
    // and must destroy it
    TRI_FreeBarrier(&barrier->base);
  }

  if (vocbase != nullptr) {
    // decrease the reference-counter for the database
    TRI_ReleaseVocBase(vocbase);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief wraps a TRI_shaped_json_t
////////////////////////////////////////////////////////////////////////////////
v8::Handle<v8::Value> TRI_WrapShapedJson (triagens::arango::CollectionNameResolver const* resolver,
                                          TRI_barrier_t* barrier,
                                          TRI_voc_cid_t cid,
                                          TRI_document_collection_t* collection,
                                          void const* data) {
  TRI_df_marker_t const* marker = static_cast<TRI_df_marker_t const*>(data);
  TRI_ASSERT(marker != nullptr);
  TRI_ASSERT(barrier != nullptr);
  TRI_ASSERT(collection != nullptr);

  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  TRI_v8_global_t* v8g = static_cast<TRI_v8_global_t*>(isolate->GetData());

  bool const doCopy = TRI_IsWalDataMarkerDatafile(marker);

  if (doCopy) {
    // we'll create a full copy of the document
    TRI_shaper_t* shaper = collection->getShaper();  // PROTECTED by trx from above

    TRI_shaped_json_t json;
    TRI_EXTRACT_SHAPED_JSON_MARKER(json, marker);  // PROTECTED by trx from above

    TRI_shape_t const* shape = shaper->lookupShapeId(shaper, json._sid);

    if (shape == nullptr) {
      return v8::Object::New();
    }

    v8::Handle<v8::Object> result = v8::Object::New();
    result = AddBasicDocumentAttributes(resolver, v8g, cid, marker, result);

    return TRI_JsonShapeData(result, shaper, shape, json._data.data, json._data.length);
  }

  // we'll create a document stub, with a pointer into the datafile

  // create the new handle to return, and set its template type
  v8::Handle<v8::Object> result = v8g->ShapedJsonTempl->NewInstance();

  if (result.IsEmpty()) {
    // error
    return result;
  }

  // point the 0 index Field to the c++ pointer for unwrapping later
  result->SetInternalField(SLOT_CLASS_TYPE, v8::Integer::New(WRP_SHAPED_JSON_TYPE));
  result->SetInternalField(SLOT_CLASS, v8::External::New((void*) marker));

  // tell everyone else that this barrier is used by an external
  reinterpret_cast<TRI_barrier_blocker_t*>(barrier)->_usedByExternal = true;

  map<void*, v8::Persistent<v8::Value> >::iterator i = v8g->JSBarriers.find(barrier);

  if (i == v8g->JSBarriers.end()) {
    // increase the reference-counter for the database
    TRI_ASSERT(barrier->_container != nullptr);
    TRI_ASSERT(barrier->_container->_collection != nullptr);
    TRI_UseVocBase(barrier->_container->_collection->_vocbase);

    v8::Persistent<v8::Value> persistent = v8::Persistent<v8::Value>::New(isolate, v8::External::New(barrier));
    result->SetInternalField(SLOT_BARRIER, persistent);

    v8g->JSBarriers[barrier] = persistent;
    persistent.MakeWeak(isolate, barrier, WeakBarrierCallback);
  }
  else {
    result->SetInternalField(SLOT_BARRIER, i->second);
  }

  return AddBasicDocumentAttributes(resolver, v8g, cid, marker, result);
}


////////////////////////////////////////////////////////////////////////////////
/// @brief selects the keys from the shaped json
////////////////////////////////////////////////////////////////////////////////
static v8::Handle<v8::Array> KeysOfShapedJson (const v8::AccessorInfo& info) {
  v8::HandleScope scope;

  // sanity check
  v8::Handle<v8::Object> self = info.Holder();

  if (self->InternalFieldCount() <= SLOT_BARRIER) {
    return scope.Close(v8::Array::New());
  }

  // get shaped json
  void* marker = TRI_UnwrapClass<void*>(self, WRP_SHAPED_JSON_TYPE);

  if (marker == nullptr) {
    return scope.Close(v8::Array::New());
  }

  TRI_barrier_t* barrier = static_cast<TRI_barrier_t*>(v8::Handle<v8::External>::Cast(self->GetInternalField(SLOT_BARRIER))->Value());
  TRI_document_collection_t* collection = barrier->_container->_collection;

  // check for array shape
  TRI_shaper_t* shaper = collection->getShaper();  // PROTECTED by BARRIER, checked by RUNTIME

  TRI_shape_sid_t sid;
  TRI_EXTRACT_SHAPE_IDENTIFIER_MARKER(sid, marker);

  TRI_shape_t const* shape = shaper->lookupShapeId(shaper, sid);

  if (shape == nullptr || shape->_type != TRI_SHAPE_ARRAY) {
    return scope.Close(v8::Array::New());
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

  v8::Handle<v8::Array> result = v8::Array::New((int) n);
  uint32_t count = 0;

  for (TRI_shape_size_t i = 0;  i < n;  ++i, ++aids) {
    char const* att = shaper->lookupAttributeId(shaper, *aids);

    if (att != nullptr) {
      result->Set(count++, v8::String::New(att));
    }
  }

  TRI_v8_global_t* v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();
  result->Set(count++, v8g->_IdKey);
  result->Set(count++, v8g->_RevKey);
  result->Set(count++, v8g->_KeyKey);

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief selects a named attribute from the shaped json
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> MapGetNamedShapedJson (v8::Local<v8::String> name,
                                                    v8::AccessorInfo const& info) {
  v8::HandleScope scope;

  // sanity check
  v8::Handle<v8::Object> self = info.Holder();

  if (self->InternalFieldCount() <= SLOT_BARRIER) {
    // we better not throw here... otherwise this will cause a segfault
    return scope.Close(v8::Handle<v8::Value>());
  }

  // get shaped json
  void* marker = TRI_UnwrapClass<void*>(self, WRP_SHAPED_JSON_TYPE);

  if (marker == nullptr) {
    return scope.Close(v8::Handle<v8::Value>());
  }

  // convert the JavaScript string to a string
  // we take the fast path here and don't normalize the string
  v8::String::Utf8Value const str(name);
  string const key(*str, (size_t) str.length());

  if (key.empty() || key[0] == '_' || strchr(key.c_str(), '.') != 0) {
    return scope.Close(v8::Handle<v8::Value>());
  }

  // get the underlying collection
  TRI_barrier_t* barrier = static_cast<TRI_barrier_t*>(v8::Handle<v8::External>::Cast(self->GetInternalField(SLOT_BARRIER))->Value());
  TRI_ASSERT(barrier != nullptr);

  TRI_document_collection_t* collection = barrier->_container->_collection;

  // get shape accessor
  TRI_shaper_t* shaper = collection->getShaper();  // PROTECTED by trx here
  TRI_shape_pid_t pid = shaper->lookupAttributePathByName(shaper, key.c_str());

  if (pid == 0) {
    return scope.Close(v8::Handle<v8::Value>());
  }

  TRI_shaped_json_t document;
  TRI_EXTRACT_SHAPED_JSON_MARKER(document, marker);

  TRI_shaped_json_t json;
  TRI_shape_t const* shape;

  bool ok = TRI_ExtractShapedJsonVocShaper(shaper, &document, 0, pid, &json, &shape);

  if (ok && shape != nullptr) {
    return scope.Close(TRI_JsonShapeData(shaper, shape, json._data.data, json._data.length));
  }

  // we must not throw a v8 exception here because this will cause follow up errors
  return scope.Close(v8::Handle<v8::Value>());
}


////////////////////////////////////////////////////////////////////////////////
/// @brief check if a property is present
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Integer> PropertyQueryShapedJson (v8::Local<v8::String> name,
                                                        const v8::AccessorInfo& info) {
  v8::HandleScope scope;

  v8::Handle<v8::Object> self = info.Holder();

  // sanity check
  if (self->InternalFieldCount() <= SLOT_BARRIER) {
    return scope.Close(v8::Handle<v8::Integer>());
  }

  // get shaped json
  void* marker = TRI_UnwrapClass<TRI_shaped_json_t>(self, WRP_SHAPED_JSON_TYPE);

  if (marker == nullptr) {
    return scope.Close(v8::Handle<v8::Integer>());
  }

  // convert the JavaScript string to a string
  string const&& key = TRI_ObjectToString(name);

  if (key.empty()) {
    return scope.Close(v8::Handle<v8::Integer>());
  }

  if (key[0] == '_') {
    if (key == "_id" || key == TRI_VOC_ATTRIBUTE_REV || key == TRI_VOC_ATTRIBUTE_KEY) {
      return scope.Close(v8::Handle<v8::Integer>(v8::Integer::New(v8::ReadOnly)));
    }
  }

  // get underlying collection
  TRI_barrier_t* barrier = static_cast<TRI_barrier_t*>(v8::Handle<v8::External>::Cast(self->GetInternalField(SLOT_BARRIER))->Value());
  TRI_document_collection_t* collection = barrier->_container->_collection;

  // get shape accessor
  TRI_shaper_t* shaper = collection->getShaper();  // PROTECTED by BARRIER, checked by RUNTIME
  TRI_shape_pid_t pid = shaper->lookupAttributePathByName(shaper, key.c_str());

  if (pid == 0) {
    return scope.Close(v8::Handle<v8::Integer>());
  }

  TRI_shape_sid_t sid;
  TRI_EXTRACT_SHAPE_IDENTIFIER_MARKER(sid, marker);

  if (sid == TRI_SHAPE_ILLEGAL) {
    // invalid shape
#ifdef TRI_ENABLE_MAINTAINER_MODE
    LOG_WARNING("invalid shape id '%llu' found for key '%s'", (unsigned long long) sid, key.c_str());
#endif
    return scope.Close(v8::Handle<v8::Integer>());
  }

  TRI_shape_access_t const* acc = TRI_FindAccessorVocShaper(shaper, sid, pid);

  // key not found
  if (acc == nullptr || acc->_resultSid == TRI_SHAPE_ILLEGAL) {
    return scope.Close(v8::Handle<v8::Integer>());
  }

  return scope.Close(v8::Handle<v8::Integer>(v8::Integer::New(v8::ReadOnly)));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief selects an indexed attribute from the shaped json
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> MapGetIndexedShapedJson (uint32_t idx,
                                                      const v8::AccessorInfo& info) {
  v8::HandleScope scope;

  char buffer[11];
  size_t len = TRI_StringUInt32InPlace(idx, (char*) &buffer);

  v8::Local<v8::String> strVal = v8::String::New((char*) &buffer, (int) len);

  return scope.Close(MapGetNamedShapedJson(strVal, info));
}


  // .............................................................................
  // generate the TRI_shaped_json_t template
  // .............................................................................
void TRI_InitV8shaped_json (v8::Handle<v8::Context> context,
			    TRI_server_t* server,
			    TRI_vocbase_t* vocbase,
			    triagens::arango::JSLoader* loader,
			    const size_t threadNumber,
			    TRI_v8_global_t* v8g){
  v8::Handle<v8::ObjectTemplate> rt;
  v8::Handle<v8::FunctionTemplate> ft;
  v8::Isolate* isolate = v8::Isolate::GetCurrent();

  ft = v8::FunctionTemplate::New();
  ft->SetClassName(TRI_V8_SYMBOL("ShapedJson"));

  rt = ft->InstanceTemplate();
  rt->SetInternalFieldCount(3);

  // accessor for named properties (e.g. doc.abcdef)
  rt->SetNamedPropertyHandler(MapGetNamedShapedJson,    // NamedPropertyGetter,
                              0,                        // NamedPropertySetter setter = 0
                              PropertyQueryShapedJson,  // NamedPropertyQuery,
                              0,                        // NamedPropertyDeleter deleter = 0,
                              KeysOfShapedJson          // NamedPropertyEnumerator,
                                                        // Handle<Value> data = Handle<Value>());
                              );

  // accessor for indexed properties (e.g. doc[1])
  rt->SetIndexedPropertyHandler(MapGetIndexedShapedJson,  // IndexedPropertyGetter,
                                0,                        // IndexedPropertySetter setter = 0
                                0,                        // IndexedPropertyQuery,
                                0,                        // IndexedPropertyDeleter deleter = 0,
                                0                         // IndexedPropertyEnumerator,
                                                          // Handle<Value> data = Handle<Value>());
                              );

  v8g->ShapedJsonTempl = v8::Persistent<v8::ObjectTemplate>::New(isolate, rt);
  TRI_AddGlobalFunctionVocbase(context, "ShapedJson", ft->GetFunction());

}
