////////////////////////////////////////////////////////////////////////////////
/// @brief V8-vocbase bridge
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2012 triagens GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Copyright 2011-2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "v8-vocbase.h"

#include "Basics/StringUtils.h"
#include "BasicsC/conversions.h"
#include "BasicsC/csv.h"
#include "BasicsC/json.h"
#include "BasicsC/logging.h"
#include "BasicsC/strings.h"
#include "ShapedJson/shape-accessor.h"
#include "ShapedJson/shaped-json.h"
#include "V8/v8-conv.h"
#include "V8/v8-utils.h"
#include "VocBase/query.h"
#include "VocBase/query-base.h"
#include "VocBase/query-cursor.h"
#include "VocBase/query-parse.h"
#include "VocBase/query-execute.h"
#include "VocBase/simple-collection.h"

using namespace std;
using namespace triagens::basics;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private constants
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief slot for a type
////////////////////////////////////////////////////////////////////////////////

static int const SLOT_CLASS_TYPE = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief slot for a "C++ class"
////////////////////////////////////////////////////////////////////////////////

static int const SLOT_CLASS = 1;

////////////////////////////////////////////////////////////////////////////////
/// @brief slot for a "query"
////////////////////////////////////////////////////////////////////////////////

static int const SLOT_QUERY = 2;

////////////////////////////////////////////////////////////////////////////////
/// @brief slot for a "barrier"
////////////////////////////////////////////////////////////////////////////////

static int const SLOT_BARRIER = 2;

////////////////////////////////////////////////////////////////////////////////
/// @brief slot for a "result set"
////////////////////////////////////////////////////////////////////////////////

static int const SLOT_RESULT_SET = 3;

////////////////////////////////////////////////////////////////////////////////
/// @brief end marker
////////////////////////////////////////////////////////////////////////////////

static int const SLOT_END = 4;

////////////////////////////////////////////////////////////////////////////////
/// @brief wrapped class for TRI_vocbase_t
////////////////////////////////////////////////////////////////////////////////

static int32_t const WRP_VOCBASE_TYPE = 1;

////////////////////////////////////////////////////////////////////////////////
/// @brief wrapped class for TRI_vocbase_col_t
////////////////////////////////////////////////////////////////////////////////

static int32_t const WRP_VOCBASE_COL_TYPE = 2;

////////////////////////////////////////////////////////////////////////////////
/// @brief wrapped class for TRI_qry_where_t
////////////////////////////////////////////////////////////////////////////////

static int32_t const WRP_QRY_WHERE_TYPE = 3;

////////////////////////////////////////////////////////////////////////////////
/// @brief wrapped class for TRI_query_cursor_t
////////////////////////////////////////////////////////////////////////////////

static int32_t const WRP_RC_QUERY_CURSOR_TYPE = 4;

////////////////////////////////////////////////////////////////////////////////
/// @brief wrapped class for TRI_query_instance_t
////////////////////////////////////////////////////////////////////////////////

static int32_t const WRP_QUERY_INSTANCE_TYPE = 5;

////////////////////////////////////////////////////////////////////////////////
/// @brief wrapped class for TRI_shaped_json_t
///
/// Layout:
/// - SLOT_CLASS_TYPE
/// - SLOT_CLASS
/// - SLOT_BARRIER
////////////////////////////////////////////////////////////////////////////////

static int32_t const WRP_SHAPED_JSON_TYPE = 6;

////////////////////////////////////////////////////////////////////////////////
/// @brief wrapped class for TRI_rc_cursor_t - DEPRECATED
////////////////////////////////////////////////////////////////////////////////

static int32_t const WRP_RC_CURSOR_TYPE = 7;


////////////////////////////////////////////////////////////////////////////////
/// @brief wrapped class for TRI_query_t - DEPRECATED
////////////////////////////////////////////////////////////////////////////////

static int32_t const WRP_QUERY_TYPE = 8;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  HELPER FUNCTIONS
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                     private types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

typedef struct {
  double _distance;
  void const* _data;
}
geo_coordinate_distance_t;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief wraps a C++ into a v8::Object
////////////////////////////////////////////////////////////////////////////////

template<class T>
static v8::Handle<v8::Object> WrapClass (v8::Persistent<v8::ObjectTemplate> classTempl, int32_t type, T* y) {

  // handle scope for temporary handles
  v8::HandleScope scope;

  // create the new handle to return, and set its template type
  v8::Handle<v8::Object> result = classTempl->NewInstance();

  // set the c++ pointer for unwrapping later
  result->SetInternalField(SLOT_CLASS_TYPE, v8::Integer::New(type));
  result->SetInternalField(SLOT_CLASS, v8::External::New(y));

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unwraps a C++ class given a v8::Object
////////////////////////////////////////////////////////////////////////////////

template<class T>
static T* UnwrapClass (v8::Handle<v8::Object> obj, int32_t type) {
  if (obj->InternalFieldCount() <= SLOT_CLASS) {
    return 0;
  }

  if (obj->GetInternalField(SLOT_CLASS_TYPE)->Int32Value() != type) {
    return 0;
  }

  return static_cast<T*>(v8::Handle<v8::External>::Cast(obj->GetInternalField(SLOT_CLASS))->Value());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if argument is a document identifier
////////////////////////////////////////////////////////////////////////////////

static bool IsDocumentId (v8::Handle<v8::Value> arg, TRI_voc_cid_t& cid, TRI_voc_did_t& did) {
  TRI_v8_global_t* v8g;

  v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();

  if (arg->IsNumber()) {
    did = arg->ToNumber()->Value();
    return true;
  }

  if (! arg->IsString()) {
    return false;
  }

  v8::String::Utf8Value str(arg);
  char const* s = *str;

  if (s == 0) {
    return false;
  }

  regmatch_t matches[3];

  if (regexec(&v8g->DocumentIdRegex, s, sizeof(matches) / sizeof(matches[0]), matches, 0) == 0) {
    cid = TRI_UInt64String2(s + matches[1].rm_so, matches[1].rm_eo - matches[1].rm_so);
    did = TRI_UInt64String2(s + matches[2].rm_so, matches[2].rm_eo - matches[2].rm_so);
    return true;
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief loads a collection
////////////////////////////////////////////////////////////////////////////////

static TRI_vocbase_col_t const* LoadCollection (v8::Handle<v8::Object> collection,
                                                v8::Handle<v8::String>* err) {
                                                
  TRI_vocbase_col_t const* col = UnwrapClass<TRI_vocbase_col_t>(collection, WRP_VOCBASE_COL_TYPE);

  if (col == 0) {
    *err = v8::String::New("illegal collection pointer");
    return 0;
  }

  if (col->_corrupted) {
    *err = v8::String::New("collection is corrupted, please run collection check");
    return 0;
  }

  if (col->_loaded) {
    if (col->_collection == 0) {
      *err = v8::String::New("cannot load collection, check log");
      return 0;
    }

    return col;
  }

  if (col->_newBorn) {
    LOG_INFO("creating new collection: '%s'", col->_name);

    bool ok = TRI_ManifestCollectionVocBase(col->_vocbase, col);

    if (! ok) {
      string msg = "cannot create collection: " + string(TRI_last_error());
      *err = v8::String::New(msg.c_str());
      return 0;
    }

    LOG_DEBUG("collection created");
  }
  else {
    LOG_INFO("loading collection: '%s'", col->_name);
    col = TRI_LoadCollectionVocBase(col->_vocbase, col->_name);
    LOG_DEBUG("collection loaded");
  }

  if (col == 0 || col->_collection == 0) {
    *err = v8::String::New("cannot load collection, check log");
    return 0;
  }

  if (col->_corrupted) {
    *err = v8::String::New("collection is corrupted, please run collection check");
    return 0;
  }

  return col;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sorts geo coordinates
////////////////////////////////////////////////////////////////////////////////

static int CompareGeoCoordinateDistance (geo_coordinate_distance_t* left, geo_coordinate_distance_t* right) {
  if (left->_distance < right->_distance) {
    return -1;
  }
  else if (left->_distance > right->_distance) {
    return 1;
  }
  else {
    return 0;
  }
}

#define FSRT_NAME SortGeoCoordinates
#define FSRT_NAM2 SortGeoCoordinatesTmp
#define FSRT_TYPE geo_coordinate_distance_t

#define FSRT_COMP(l,r,s) CompareGeoCoordinateDistance(l,r)

uint32_t FSRT_Rand = 0;

static uint32_t RandomGeoCoordinateDistance (void) {
  return (FSRT_Rand = FSRT_Rand * 31415 + 27818);
}

#define FSRT__RAND \
  ((fs_b) + FSRT__UNIT * (RandomGeoCoordinateDistance() % FSRT__DIST(fs_e,fs_b,FSRT__SIZE)))

#include <BasicsC/fsrt.inc>

////////////////////////////////////////////////////////////////////////////////
/// @brief create result
////////////////////////////////////////////////////////////////////////////////

static void StoreGeoResult (TRI_vocbase_col_t const* collection,
                            GeoCoordinates* cors,
                            v8::Handle<v8::Array>& documents,
                            v8::Handle<v8::Array>& distances) {
  GeoCoordinate* end;
  GeoCoordinate* ptr;
  double* dtr;
  geo_coordinate_distance_t* gnd;
  geo_coordinate_distance_t* gtr;
  geo_coordinate_distance_t* tmp;
  size_t n;
  uint32_t i;
  TRI_barrier_t* barrier;

  // sort the result
  n = cors->length;

  if (n == 0) {
    return;
  }

  gtr = (tmp = (geo_coordinate_distance_t*) TRI_Allocate(sizeof(geo_coordinate_distance_t) * n));
  gnd = tmp + n;

  ptr = cors->coordinates;
  end = cors->coordinates + n;

  dtr = cors->distances;

  for (;  ptr < end;  ++ptr, ++dtr, ++gtr) {
    gtr->_distance = *dtr;
    gtr->_data = ptr->data;
  }

  GeoIndex_CoordinatesFree(cors);

  SortGeoCoordinates(tmp, gnd);

  barrier = TRI_CreateBarrierElement(&((TRI_doc_collection_t*) collection->_collection)->_barrierList);

  // copy the documents
  for (gtr = tmp, i = 0;  gtr < gnd;  ++gtr, ++i) {
    documents->Set(i, TRI_WrapShapedJson(collection, (TRI_doc_mptr_t const*) gtr->_data, barrier));
    distances->Set(i, v8::Number::New(gtr->_distance));
  }

  TRI_Free(tmp);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   QUERY FUNCTIONS
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up edges
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> EdgesQuery (TRI_edge_direction_e direction, v8::Arguments const& argv) {
  v8::HandleScope scope;

  // extract the collection
  v8::Handle<v8::Object> operand = argv.Holder();

  v8::Handle<v8::String> err;
  TRI_vocbase_col_t const* collection = LoadCollection(operand, &err);

  if (collection == 0) {
    return scope.Close(v8::ThrowException(err));
  }
  
  // handle various collection types
  TRI_doc_collection_t* doc = collection->_collection;

  if (doc->base._type != TRI_COL_TYPE_SIMPLE_DOCUMENT) {
    return scope.Close(v8::ThrowException(v8::String::New("unknown collection type")));
  }

  TRI_sim_collection_t* sim = (TRI_sim_collection_t*) doc;

  // first and only argument schould be a list of document idenfifier
  if (argv.Length() != 1) {
    switch (direction) {
      case TRI_EDGE_UNUSED:
        return scope.Close(v8::ThrowException(v8::String::New("usage: edge(<vertices>)")));

      case TRI_EDGE_IN:
        return scope.Close(v8::ThrowException(v8::String::New("usage: inEdge(<vertices>)")));

      case TRI_EDGE_OUT:
        return scope.Close(v8::ThrowException(v8::String::New("usage: outEdge(<vertices>)")));

      case TRI_EDGE_ANY:
        return scope.Close(v8::ThrowException(v8::String::New("usage: edge(<vertices>)")));
    }
  }

  // setup result
  v8::Handle<v8::Array> documents = v8::Array::New();

  // .............................................................................
  // inside a read transaction
  // .............................................................................

  collection->_collection->beginRead(collection->_collection);

  TRI_barrier_t* barrier = 0;
  size_t count = 0;

  // argument is a list of vertices
  if (argv[0]->IsArray()) {
    v8::Handle<v8::Array> vertices = v8::Handle<v8::Array>::Cast(argv[0]);
    uint32_t len = vertices->Length();

    for (uint32_t i = 0;  i < len;  ++i) {
      TRI_vector_pointer_t edges;
      // v8::Handle<v8::Value> val = vertices->Get(i);
      
      TRI_voc_cid_t cid;
      TRI_voc_did_t did;
      bool ok;
      
      ok = TRI_IdentifiersObjectReference(vertices->Get(i), cid, did);
      
      if (! ok || cid == 0) {
        continue;
      }
      
      edges = TRI_LookupEdgesSimCollection(sim, direction, cid, did);
      
      for (size_t j = 0;  j < edges._length;  ++j) {
        if (barrier == 0) {
          barrier = TRI_CreateBarrierElement(&doc->_barrierList);
        }
        
        documents->Set(count++, TRI_WrapShapedJson(collection, (TRI_doc_mptr_t const*) edges._buffer[j], barrier));
      }
      
      TRI_DestroyVectorPointer(&edges);
    }
  }

  // argument is a single vertex
  else {
    TRI_vector_pointer_t edges;
    TRI_voc_cid_t cid;
    TRI_voc_did_t did;
    bool ok;
      
    ok = TRI_IdentifiersObjectReference(argv[0], cid, did);

    if (! ok) {
      collection->_collection->endRead(collection->_collection);
      return scope.Close(v8::ThrowException(v8::String::New("<vertices> must be an array or a single object-reference")));
    }

    edges = TRI_LookupEdgesSimCollection(sim, direction, cid, did);
      
    for (size_t j = 0;  j < edges._length;  ++j) {
      if (barrier == 0) {
        barrier = TRI_CreateBarrierElement(&doc->_barrierList);
      }
        
      documents->Set(count++, TRI_WrapShapedJson(collection, (TRI_doc_mptr_t const*) edges._buffer[j], barrier));
    }
    
    TRI_DestroyVectorPointer(&edges);
  }
  
  collection->_collection->endRead(collection->_collection);

  // .............................................................................
  // outside a write transaction
  // .............................................................................

  return scope.Close(documents);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief weak reference callback for query - DEPRECATED
////////////////////////////////////////////////////////////////////////////////

static void WeakQueryCallback (v8::Persistent<v8::Value> object, void* parameter) {
  TRI_query_t* query;
  TRI_v8_global_t* v8g;

  v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();
  query = (TRI_query_t*) parameter;

  LOG_TRACE("weak-callback for query called");

  // find the persistent handle
  v8::Persistent<v8::Value> persistent = v8g->JSQueries[query];
  v8g->JSQueries.erase(query);

  // dispose and clear the persistent handle
  persistent.Dispose();
  persistent.Clear();

  // and free the instance
  TRI_FreeQuery(query);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stores a query in a javascript object - DEPRECATED
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Object> WrapQuery (TRI_query_t* query) {
  TRI_v8_global_t* v8g;
  
  v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();

  v8::Handle<v8::Object> queryObject = v8g->QueryTempl->NewInstance();
  map< void*, v8::Persistent<v8::Value> >::iterator i = v8g->JSQueries.find(query);

  if (i == v8g->JSQueries.end()) {
    v8::Persistent<v8::Value> persistent = v8::Persistent<v8::Value>::New(v8::External::New(query));

    queryObject->SetInternalField(SLOT_CLASS_TYPE, v8::Integer::New(WRP_QUERY_TYPE));
    queryObject->SetInternalField(SLOT_CLASS, persistent);

    v8g->JSQueries[query] = persistent;

    persistent.MakeWeak(query, WeakQueryCallback);
  }
  else {
    queryObject->SetInternalField(SLOT_CLASS_TYPE, v8::Integer::New(WRP_QUERY_TYPE));
    queryObject->SetInternalField(SLOT_CLASS, i->second);
  }

  return queryObject;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief weak reference callback for query instances
////////////////////////////////////////////////////////////////////////////////

static void WeakQueryInstanceCallback (v8::Persistent<v8::Value> object, void* parameter) {
  TRI_query_instance_t* instance;
  TRI_v8_global_t* v8g;

  v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();
  instance = (TRI_query_instance_t*) parameter;

  LOG_TRACE("weak-callback for query instance called");

  // find the persistent handle
  v8::Persistent<v8::Value> persistent = v8g->JSQueryInstances[instance];
  v8g->JSQueryInstances.erase(instance);

  // dispose and clear the persistent handle
  persistent.Dispose();
  persistent.Clear();

  // and free the instance
  TRI_FreeQueryTemplate(instance->_template);
  TRI_FreeQueryInstance(instance);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stores a query instance in a javascript object
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Object> WrapQueryInstance (TRI_query_instance_t* instance) {
  TRI_v8_global_t* v8g;
  v8::HandleScope scope;
  
  v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();

  v8::Handle<v8::Object> queryInstance = v8g->QueryInstanceTempl->NewInstance();
  map< void*, v8::Persistent<v8::Value> >::iterator i = v8g->JSQueryInstances.find(instance);

  if (i == v8g->JSQueryInstances.end()) {
    v8::Persistent<v8::Value> persistent = v8::Persistent<v8::Value>::New(v8::External::New(instance));

    queryInstance->SetInternalField(SLOT_CLASS_TYPE, v8::Integer::New(WRP_QUERY_INSTANCE_TYPE));
    queryInstance->SetInternalField(SLOT_CLASS, persistent);

    v8g->JSQueryInstances[instance] = persistent;

    LOG_TRACE("creating new query instance");
    persistent.MakeWeak(instance, WeakQueryInstanceCallback);
  }
  else {
    queryInstance->SetInternalField(SLOT_CLASS_TYPE, v8::Integer::New(WRP_QUERY_INSTANCE_TYPE));
    queryInstance->SetInternalField(SLOT_CLASS, i->second);
  }

  return scope.Close(queryInstance);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Create a query error in a javascript object
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Object> CreateQueryErrorObject (TRI_query_error_t* error) {
  TRI_v8_global_t* v8g;
  v8::HandleScope scope;

  v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();

  char* errorMessage = TRI_GetStringQueryError(error);

  v8::Handle<v8::Object> errorObject = v8g->QueryErrorTempl->NewInstance();
  errorObject->Set(v8::String::New("code"), 
                   v8::Integer::New(TRI_GetCodeQueryError(error)),
                   v8::ReadOnly);
  errorObject->Set(v8::String::New("message"), 
                   v8::String::New(errorMessage),
                   v8::ReadOnly);

  if (errorMessage) {
    TRI_Free(errorMessage);
  }

  return scope.Close(errorObject);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a query or uses existing result set - DEPRECATED
////////////////////////////////////////////////////////////////////////////////

static TRI_rc_cursor_t* ExecuteQuery (v8::Handle<v8::Object> queryObject,
                                      v8::Handle<v8::Value>* err) {
  v8::TryCatch tryCatch;

  TRI_query_t* query = UnwrapClass<TRI_query_t>(queryObject, WRP_QUERY_TYPE);
  if (query == 0) {
    *err = v8::String::New("corrupted query");
    return 0;
  }
  
  LOG_TRACE("executing query");

  TRI_rc_context_t* context = TRI_CreateContextQuery(query);

  if (context == 0) {
    if (tryCatch.HasCaught()) {
      *err = tryCatch.Exception();
    }
    else {
      *err = v8::String::New("cannot create query context");
    }

    return 0;
  }

  TRI_rc_cursor_t* cursor = TRI_ExecuteQueryAql(query, context);
  if (cursor == 0) {
    TRI_FreeContextQuery(context);

    if (tryCatch.HasCaught()) {
      *err = tryCatch.Exception();
    }
    else {
      *err = v8::String::New("cannot execute query");
    }

    return 0;
  }

  return cursor;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a query instance or uses existing result set
////////////////////////////////////////////////////////////////////////////////

static TRI_query_cursor_t* ExecuteQueryInstance (v8::Handle<v8::Object> queryObject,
                                                 bool doCount,
                                                 uint32_t max,
                                                 v8::Handle<v8::Value>* err) {
  v8::TryCatch tryCatch;

  TRI_query_instance_t* instance = UnwrapClass<TRI_query_instance_t>(queryObject, WRP_QUERY_INSTANCE_TYPE);
  
  if (!instance) {
    *err = v8::String::New("corrupted query instance");
    return 0;
  }
  
  LOG_TRACE("executing query");

  TRI_query_cursor_t* cursor = TRI_ExecuteQueryInstance(instance, doCount, max);
  if (!cursor) {
    if (tryCatch.HasCaught()) {
      *err = tryCatch.Exception();
    }
    else {
      *err = CreateQueryErrorObject(&instance->_error);
    }

    return NULL;
  }

  return cursor;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief weak reference callback for cursors
////////////////////////////////////////////////////////////////////////////////

static void WeakQueryCursorCallback (v8::Persistent<v8::Value> object, void* parameter) {
  TRI_query_cursor_t* cursor;

  LOG_TRACE("weak-callback for query cursor called");

  cursor = (TRI_query_cursor_t*) parameter;

  // mutex lock
  TRI_LockQueryCursor(cursor);

  try {
    // find the persistent handle
    TRI_v8_global_t* v8g;
    v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();
    v8::Persistent<v8::Value> persistent = v8g->JSQueryCursors[cursor];
    v8g->JSQueryCursors.erase(cursor);

    // dispose and clear the persistent handle
    persistent.Dispose();
    persistent.Clear();
  } 
  catch (...) {
  }

  if (!TRI_ReleaseShadowData(cursor->_vocbase->_cursors, cursor->_shadow)) {
    // unlock mutex
    TRI_UnlockQueryCursor(cursor);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stores a cursor in a javascript object
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Object> WrapQueryCursor (TRI_query_cursor_t* cursor) {
  TRI_v8_global_t* v8g;
  v8::HandleScope scope;

  v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();

  v8::Handle<v8::Object> cursorObject = v8g->QueryCursorTempl->NewInstance();
  map< void*, v8::Persistent<v8::Value> >::iterator i = v8g->JSQueryCursors.find(cursor);
  
  if (i == v8g->JSQueryCursors.end()) {
    v8::Persistent<v8::Value> persistent = v8::Persistent<v8::Value>::New(v8::External::New(cursor));

    cursorObject->SetInternalField(SLOT_CLASS_TYPE, v8::Integer::New(WRP_RC_QUERY_CURSOR_TYPE));
    cursorObject->SetInternalField(SLOT_CLASS, persistent);

    v8g->JSQueryCursors[cursor] = persistent;

    persistent.MakeWeak(cursor, WeakQueryCursorCallback);
  }
  else {
    cursorObject->SetInternalField(SLOT_CLASS_TYPE, v8::Integer::New(WRP_RC_QUERY_CURSOR_TYPE));
    cursorObject->SetInternalField(SLOT_CLASS, i->second);
  }
  
  return scope.Close(cursorObject);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts a cursor from a javascript object
////////////////////////////////////////////////////////////////////////////////

static TRI_query_cursor_t* UnwrapQueryCursor (v8::Handle<v8::Object> cursorObject) {
  return UnwrapClass<TRI_query_cursor_t>(cursorObject, WRP_RC_QUERY_CURSOR_TYPE);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief weak reference callback for cursors - DEPRECATED
////////////////////////////////////////////////////////////////////////////////

static void WeakCursorCallback (v8::Persistent<v8::Value> object, void* parameter) {
  TRI_rc_cursor_t* cursor;
  TRI_v8_global_t* v8g;

  v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();
  cursor = (TRI_rc_cursor_t*) parameter;

  LOG_TRACE("weak-callback for cursor called");

  // find the persistent handle
  v8::Persistent<v8::Value> persistent = v8g->JSCursors[cursor];
  v8g->JSCursors.erase(cursor);

  // dispose and clear the persistent handle
  persistent.Dispose();
  persistent.Clear();

  // and free the result set
//  cursor->free(cursor);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stores a cursor in a javascript object - DEPRECATED
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Object> WrapCursor (TRI_rc_cursor_t* cursor) {
  TRI_v8_global_t* v8g;

  v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();

  v8::Handle<v8::Object> cursorObject = v8g->CursorTempl->NewInstance();
  map< void*, v8::Persistent<v8::Value> >::iterator i = v8g->JSCursors.find(cursor);

  if (i == v8g->JSCursors.end()) {
    v8::Persistent<v8::Value> persistent = v8::Persistent<v8::Value>::New(v8::External::New(cursor));

    cursorObject->SetInternalField(SLOT_CLASS_TYPE, v8::Integer::New(WRP_RC_CURSOR_TYPE));
    cursorObject->SetInternalField(SLOT_CLASS, persistent);

    v8g->JSCursors[cursor] = persistent;

    persistent.MakeWeak(cursor, WeakCursorCallback);
  }
  else {
    cursorObject->SetInternalField(SLOT_CLASS_TYPE, v8::Integer::New(WRP_RC_CURSOR_TYPE));
    cursorObject->SetInternalField(SLOT_CLASS, i->second);
  }

  return cursorObject;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts a cursor from a javascript object - DEPRECATED
////////////////////////////////////////////////////////////////////////////////

static TRI_rc_cursor_t* UnwrapCursor (v8::Handle<v8::Object> cursorObject) {
  return UnwrapClass<TRI_rc_cursor_t>(cursorObject, WRP_RC_CURSOR_TYPE);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief weak reference callback for wheres - DEPRECATED
////////////////////////////////////////////////////////////////////////////////

static void WeakWhereCallback (v8::Persistent<v8::Value> object, void* parameter) {
  TRI_qry_where_t* where;
  TRI_v8_global_t* v8g;

  v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();
  where = (TRI_qry_where_t*) parameter;

  LOG_TRACE("weak-callback for where called");

  // find the persistent handle
  v8::Persistent<v8::Value> persistent = v8g->JSWheres[where];
  v8g->JSWheres.erase(where);

  // dispose and clear the persistent handle
  persistent.Dispose();
  persistent.Clear();

  // and free the result set
  where->free(where);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stores a where clause in a javascript object - DEPRECATED
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Object> WrapWhere (TRI_qry_where_t* where) {
  TRI_v8_global_t* v8g;

  v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();

  v8::Handle<v8::Object> whereObject = v8g->WhereTempl->NewInstance();
  map< void*, v8::Persistent<v8::Value> >::iterator i = v8g->JSWheres.find(where);

  if (i == v8g->JSWheres.end()) {
    v8::Persistent<v8::Value> persistent = v8::Persistent<v8::Value>::New(v8::External::New(where));

    whereObject->SetInternalField(SLOT_CLASS_TYPE, v8::Integer::New(WRP_QRY_WHERE_TYPE));
    whereObject->SetInternalField(SLOT_CLASS, persistent);

    v8g->JSWheres[where] = persistent;

    persistent.MakeWeak(where, WeakWhereCallback);
  }
  else {
    whereObject->SetInternalField(SLOT_CLASS_TYPE, v8::Integer::New(WRP_QRY_WHERE_TYPE));
    whereObject->SetInternalField(SLOT_CLASS, i->second);
  }

  return whereObject;
}
 
////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                              javascript functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief returns all elements
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_AllQuery (v8::Arguments const& argv) {
  v8::HandleScope scope;

  // extract the collection
  v8::Handle<v8::Object> operand = argv.Holder();

  v8::Handle<v8::String> err;
  TRI_vocbase_col_t const* collection = LoadCollection(operand, &err);

  if (collection == 0) {
    return scope.Close(v8::ThrowException(err));
  }
  
  // handle various collection types
  TRI_doc_collection_t* doc = collection->_collection;

  if (doc->base._type != TRI_COL_TYPE_SIMPLE_DOCUMENT) {
    return scope.Close(v8::ThrowException(v8::String::New("unknown collection type")));
  }

  TRI_sim_collection_t* sim = (TRI_sim_collection_t*) doc;

  // expecting two arguments
  if (argv.Length() != 2) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: ALL(<skip>, <limit>)")));
  }

  TRI_voc_size_t skip = TRI_QRY_NO_SKIP;
  TRI_voc_ssize_t limit = TRI_QRY_NO_LIMIT;

  if (! argv[0]->IsNull()) {
    skip = TRI_ObjectToDouble(argv[0]);
  }

  if (! argv[1]->IsNull()) {
    limit = TRI_ObjectToDouble(argv[1]);
  }

  // setup result
  v8::Handle<v8::Object> result = v8::Object::New();

  v8::Handle<v8::Array> documents = v8::Array::New();
  result->Set(v8::String::New("documents"), documents);

  // .............................................................................
  // inside a read transaction
  // .............................................................................

  collection->_collection->beginRead(collection->_collection);
  
  size_t total = sim->_primaryIndex._nrUsed;
  uint32_t count = 0;

  if (0 < total && 0 != limit) {
    TRI_barrier_t* barrier = 0;

    // skip from the beginning
    if (0 <= limit) {
      void** ptr = sim->_primaryIndex._table;
      void** end = sim->_primaryIndex._table + sim->_primaryIndex._nrAlloc;

      for (;  ptr < end && (TRI_voc_ssize_t) count < limit;  ++ptr) {
        if (*ptr) {
          TRI_doc_mptr_t const* d = (TRI_doc_mptr_t const*) *ptr;
          
          if (d->_deletion == 0) {
            if (0 < skip) {
              --skip;
            }
            else {
              if (barrier == 0) {
                barrier = TRI_CreateBarrierElement(&doc->_barrierList);
              }
            
              documents->Set(count, TRI_WrapShapedJson(collection, d, barrier));
              ++count;
            }
          }
        }
      }
    }

    // skip at the end
    else {
      limit = -limit;

      void** beg = sim->_primaryIndex._table;
      void** ptr = sim->_primaryIndex._table + sim->_primaryIndex._nrAlloc - 1;

      for (;  beg <= ptr && (TRI_voc_ssize_t) count < limit;  --ptr) {
        if (*ptr) {
          TRI_doc_mptr_t const* d = (TRI_doc_mptr_t const*) *ptr;
          
          if (d->_deletion == 0) {
            if (0 < skip) {
              --skip;
            }
            else {
              if (barrier == 0) {
                barrier = TRI_CreateBarrierElement(&doc->_barrierList);
              }
            
              documents->Set(count, TRI_WrapShapedJson(collection, d, barrier));
              ++count;
            }
          }
        }
      }

      // swap result
      if (1 < count) {
        for (uint32_t i = 0, j = count - 1;  i < j;  ++i, --j) {
          v8::Handle<v8::Value> tmp1 = documents->Get(i);
          v8::Handle<v8::Value> tmp2 = documents->Get(j);
          documents->Set(i, tmp2);
          documents->Set(j, tmp1);
        }
      }
    }
  }

  collection->_collection->endRead(collection->_collection);

  // .............................................................................
  // outside a write transaction
  // .............................................................................

  result->Set(v8::String::New("total"), v8::Number::New(total));
  result->Set(v8::String::New("count"), v8::Number::New(count));

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a document
///
/// @FUN{document(@FA{document-identifier})}
///
/// The @FN{document} operator finds a document given it's identifier.  It
/// returns the document. Undefined is returned if the document identifier is
/// not valid.
///
/// Note that the returned docuement contains two pseudo-attributes, namely
/// @LIT{_id} and @LIT{_rev}. The attribute @LIT{_id} contains the document
/// reference and @LIT{_rev} contains the document revision.
///
/// @EXAMPLES
///
/// @verbinclude simple1
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_DocumentQuery (v8::Arguments const& argv) {
  v8::HandleScope scope;

  // extract the collection
  v8::Handle<v8::Object> operand = argv.Holder();

  v8::Handle<v8::String> err;
  TRI_vocbase_col_t const* collection = LoadCollection(operand, &err);

  if (collection == 0) {
    return scope.Close(v8::ThrowException(err));
  }
  
  // first and only argument schould be an document idenfifier
  if (argv.Length() != 1) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: document_wrapped(<document-identifier>)")));
  }

  v8::Handle<v8::Value> arg1 = argv[0];
  TRI_voc_cid_t cid = collection->_collection->base._cid;
  TRI_voc_did_t did;

  if (! IsDocumentId(arg1, cid, did)) {
    return scope.Close(v8::ThrowException(v8::String::New("<document-idenifier> must be a document identifier")));
  }

  if (cid != collection->_collection->base._cid) {
    return scope.Close(v8::ThrowException(v8::String::New("cannot execute cross collection query")));
  }

  // .............................................................................
  // get document
  // .............................................................................

  TRI_doc_mptr_t const* document;
  
  // .............................................................................
  // inside a read transaction
  // .............................................................................

  collection->_collection->beginRead(collection->_collection);
  
  document = collection->_collection->read(collection->_collection, did);  
  v8::Handle<v8::Value> result;

  if (document != 0) {
    TRI_barrier_t* barrier;

    barrier = TRI_CreateBarrierElement(&collection->_collection->_barrierList);
    result = TRI_WrapShapedJson(collection, document, barrier);
  }

  collection->_collection->endRead(collection->_collection);

  // .............................................................................
  // outside a write transaction
  // .............................................................................

  if (document == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("document not found")));
  }
  
  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up all edges for a set of vertices
///
/// @FUN{edges(@FA{vertices})}
///
/// The @FN{edges} operator finds all edges starting from (outbound) or ending
/// in (inbound) a document from @FA{vertices}.
///
/// @EXAMPLES
///
/// @verbinclude simple11
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_EdgesQuery (v8::Arguments const& argv) {
  return EdgesQuery(TRI_EDGE_ANY, argv);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up all inbound edges
///
/// @FUN{inEdges(@FA{vertices})}
///
/// The @FN{edges} operator finds all edges ending in (inbound) a document from
/// @FA{vertices}.
///
/// @EXAMPLES
///
/// @verbinclude simple11
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_InEdgesQuery (v8::Arguments const& argv) {
  return EdgesQuery(TRI_EDGE_IN, argv);
}


////////////////////////////////////////////////////////////////////////////////
/// @brief finds points near a given coordinate
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_NearQuery (v8::Arguments const& argv) {
  v8::HandleScope scope;

  // extract the collection
  v8::Handle<v8::Object> operand = argv.Holder();

  v8::Handle<v8::String> err;
  TRI_vocbase_col_t const* collection = LoadCollection(operand, &err);

  if (collection == 0) {
    return scope.Close(v8::ThrowException(err));
  }
  
  // handle various collection types
  TRI_doc_collection_t* doc = collection->_collection;

  if (doc->base._type != TRI_COL_TYPE_SIMPLE_DOCUMENT) {
    return scope.Close(v8::ThrowException(v8::String::New("unknown collection type")));
  }

  // expect: NEAR(<index-id>, <latitiude>, <longitude>, <limit>)
  if (argv.Length() != 4) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: NEAR(<index-id>, <latitiude>, <longitude>, <limit>)")));
  }

  // extract the index
  TRI_idx_iid_t iid = TRI_ObjectToDouble(argv[0]);
  TRI_index_t* idx = TRI_LookupIndex(doc, iid);

  if (idx == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("unknown index identifier")));
  }

  if (idx->_type != TRI_IDX_TYPE_GEO_INDEX) {
    return scope.Close(v8::ThrowException(v8::String::New("index must be a geo-index")));
  }

  // extract latitiude and longitude
  double latitude = TRI_ObjectToDouble(argv[1]);
  double longitude = TRI_ObjectToDouble(argv[2]);

  // extract the limit
  TRI_voc_ssize_t limit = TRI_ObjectToDouble(argv[3]);

  // setup result
  v8::Handle<v8::Object> result = v8::Object::New();

  v8::Handle<v8::Array> documents = v8::Array::New();
  result->Set(v8::String::New("documents"), documents);

  v8::Handle<v8::Array> distances = v8::Array::New();
  result->Set(v8::String::New("distances"), distances);

  // .............................................................................
  // inside a read transaction
  // .............................................................................

  collection->_collection->beginRead(collection->_collection);
  
  GeoCoordinates* cors = TRI_NearestGeoIndex(idx, latitude, longitude, limit);

  if (cors != 0) {
    StoreGeoResult(collection, cors, documents, distances);
  }

  collection->_collection->endRead(collection->_collection);

  // .............................................................................
  // outside a write transaction
  // .............................................................................

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up all outbound edges
///
/// @FUN{outEdges(@FA{vertices})}
///
/// The @FN{edges} operator finds all edges starting from (outbound) a document
/// from @FA{vertices}.
///
/// @EXAMPLES
///
/// @verbinclude simple13
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_OutEdgesQuery (v8::Arguments const& argv) {
  return EdgesQuery(TRI_EDGE_OUT, argv);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finds points within a given radius
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_WithinQuery (v8::Arguments const& argv) {
  v8::HandleScope scope;

  // extract the collection
  v8::Handle<v8::Object> operand = argv.Holder();

  v8::Handle<v8::String> err;
  TRI_vocbase_col_t const* collection = LoadCollection(operand, &err);

  if (collection == 0) {
    return scope.Close(v8::ThrowException(err));
  }
  
  // handle various collection types
  TRI_doc_collection_t* doc = collection->_collection;

  if (doc->base._type != TRI_COL_TYPE_SIMPLE_DOCUMENT) {
    return scope.Close(v8::ThrowException(v8::String::New("unknown collection type")));
  }

  // expect: NEAR(<index-id>, <latitiude>, <longitude>, <limit>)
  if (argv.Length() != 4) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: NEAR(<index-id>, <latitiude>, <longitude>, <limit>)")));
  }

  // extract the index
  TRI_idx_iid_t iid = TRI_ObjectToDouble(argv[0]);
  TRI_index_t* idx = TRI_LookupIndex(doc, iid);

  if (idx == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("unknown index identifier")));
  }

  if (idx->_type != TRI_IDX_TYPE_GEO_INDEX) {
    return scope.Close(v8::ThrowException(v8::String::New("index must be a geo-index")));
  }

  // extract latitiude and longitude
  double latitude = TRI_ObjectToDouble(argv[1]);
  double longitude = TRI_ObjectToDouble(argv[2]);

  // extract the limit
  double radius = TRI_ObjectToDouble(argv[3]);

  // setup result
  v8::Handle<v8::Object> result = v8::Object::New();

  v8::Handle<v8::Array> documents = v8::Array::New();
  result->Set(v8::String::New("documents"), documents);

  v8::Handle<v8::Array> distances = v8::Array::New();
  result->Set(v8::String::New("distances"), distances);

  // .............................................................................
  // inside a read transaction
  // .............................................................................

  collection->_collection->beginRead(collection->_collection);
  
  GeoCoordinates* cors = TRI_WithinGeoIndex(idx, latitude, longitude, radius);

  if (cors != 0) {
    StoreGeoResult(collection, cors, documents, distances);
  }

  collection->_collection->endRead(collection->_collection);

  // .............................................................................
  // outside a write transaction
  // .............................................................................

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief get a (persistent) cursor by its id
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_Cursor (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 2) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: AQL_CURSOR(<db>, <id>)")));
  }

  v8::Handle<v8::Object> dbArg = argv[0]->ToObject();
  TRI_vocbase_t* vocbase = UnwrapClass<TRI_vocbase_t>(dbArg, WRP_VOCBASE_TYPE);

  if (!vocbase) {
    return scope.Close(v8::ThrowException(v8::String::New("corrupted vocbase")));
  }

  // get the id
  v8::Handle<v8::Value> idArg = argv[1]->ToNumber();
  if (!idArg->IsNumber()) {
    return scope.Close(v8::ThrowException(v8::String::New("expecting number for <id>")));
  }

  double id = TRI_ObjectToDouble(idArg);

  TRI_shadow_t* shadow = TRI_FindShadowData(vocbase->_cursors, (TRI_shadow_id) id);
  if (!shadow) {
    return scope.Close(v8::ThrowException(v8::String::New("no cursor found for id")));
  }

  TRI_query_cursor_t* cursor = (TRI_query_cursor_t*) shadow->_data;
  if (!cursor) {
    return scope.Close(v8::ThrowException(v8::String::New("corrupted cursor")));
  }

  TRI_LockQueryCursor(cursor);
  if (cursor->_deleted) {
    TRI_UnlockQueryCursor(cursor);
    return scope.Close(v8::ThrowException(v8::String::New("cursor has been deleted")));
  }
  TRI_UnlockQueryCursor(cursor);

  return scope.Close(WrapQueryCursor(cursor));
}

// -----------------------------------------------------------------------------
// --SECTION--                                            AVOCADO QUERY LANGUAGE
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                              javascript functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new constant where clause for a hash index
////////////////////////////////////////////////////////////////////////////////

static TRI_json_t* ConvertHelper(v8::Handle<v8::Value> parameter) {
  if (parameter->IsBoolean()) {
    v8::Handle<v8::Boolean> booleanParameter = parameter->ToBoolean();
    return TRI_CreateBooleanJson(booleanParameter->Value());
  }
  
  if (parameter->IsNumber()) {
    v8::Handle<v8::Number> numberParameter = parameter->ToNumber();
    return TRI_CreateNumberJson(numberParameter->Value());
  }

  if (parameter->IsString()) {
    v8::Handle<v8::String> stringParameter= parameter->ToString();
    v8::String::Utf8Value str(stringParameter);
    return TRI_CreateStringCopyJson(*str);
  }

  if (parameter->IsArray()) {
    v8::Handle<v8::Array> arrayParameter = v8::Handle<v8::Array>::Cast(parameter);
    TRI_json_t* listJson = TRI_CreateListJson();
    if (listJson) {
      for (size_t j = 0; j < arrayParameter->Length(); ++j) {
        v8::Handle<v8::Value> item = arrayParameter->Get(j);    
        TRI_json_t* result = ConvertHelper(item);
        TRI_PushBack2ListJson(listJson, result);
      }
    }
    return listJson;
  }
  if (parameter->IsObject()) {
    v8::Handle<v8::Array> arrayParameter = v8::Handle<v8::Array>::Cast(parameter);
    TRI_json_t* arrayJson = TRI_CreateArrayJson();
    if (arrayJson) {
      v8::Handle<v8::Array> names = arrayParameter->GetOwnPropertyNames();
      for (size_t j = 0; j < names->Length(); ++j) {
        v8::Handle<v8::Value> key = names->Get(j);
        v8::Handle<v8::Value> item = arrayParameter->Get(key);    
        TRI_json_t* result = ConvertHelper(item);
        TRI_Insert2ArrayJson(arrayJson, TRI_ObjectToString(key).c_str(), result);
      }
    }
    return arrayJson;
  }
  
  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new query from a string
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_PrepareAql (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() < 2) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: AQL_PREPARE(<db>, <querystring>, <bindvalues>)")));
  }

  v8::Handle<v8::Object> dbArg = argv[0]->ToObject();
  TRI_vocbase_t* vocbase = UnwrapClass<TRI_vocbase_t>(dbArg, WRP_VOCBASE_TYPE);

  if (!vocbase) {
    return scope.Close(v8::ThrowException(v8::String::New("corrupted vocbase")));
  }

  // get the query string
  v8::Handle<v8::Value> queryArg = argv[1];
  if (!queryArg->IsString()) {
    return scope.Close(v8::ThrowException(v8::String::New("expecting string for <querystring>")));
  }

  string queryString = TRI_ObjectToString(queryArg);

  TRI_json_t* parameters = NULL;
  if (argv.Length() > 2) {
    parameters = ConvertHelper(argv[2]);
  }

  // create a template object
  TRI_query_template_t* template_ = TRI_CreateQueryTemplate(queryString.c_str(), vocbase);
  if (template_) {
    bool ok = TRI_ParseQueryTemplate(template_);
    if (ok) {
      TRI_query_instance_t* instance = TRI_CreateQueryInstance(template_, parameters);
      if (parameters) {
        TRI_FreeJson(parameters);
      }
      if (instance) {
        if (!instance->_doAbort) {
          return scope.Close(WrapQueryInstance(instance));
        }
        v8::Handle<v8::Object> errorObject = CreateQueryErrorObject(&instance->_error);
        TRI_FreeQueryInstance(instance);
        TRI_FreeQueryTemplate(template_);
        return scope.Close(errorObject);
      }
      else {
        TRI_FreeQueryTemplate(template_);
      }
    }
    else {
      if (parameters) {
        TRI_FreeJson(parameters);
      }
      v8::Handle<v8::Object> errorObject = CreateQueryErrorObject(&template_->_error);
      TRI_FreeQueryTemplate(template_);
      return scope.Close(errorObject);
    }
  }
  if (parameters) {
    TRI_FreeJson(parameters);
  }
      
  return scope.Close(v8::ThrowException(v8::String::New("out of memory")));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new constant where clause using a boolean - DEPRECATED
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_WhereBooleanAql (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 1) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: whereConstant(<boolean>)")));
  }

  // extract the where clause
  v8::Handle<v8::Value> whereArg = argv[0];
  TRI_qry_where_t* where = 0;

  where = TRI_CreateQueryWhereBoolean(TRI_ObjectToBoolean(whereArg));

  // wrap it up
  return scope.Close(WrapWhere(where));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new where clause from JavaScript - DEPRECATED
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_WhereGeneralAql (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 1) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: whereGeneral(<where>)")));
  }

  // extract the where clause
  v8::Handle<v8::Value> whereArg = argv[0];
  TRI_qry_where_t* where = 0;

  string cmd = TRI_ObjectToString(whereArg);

  if (cmd.empty()) {
    return scope.Close(v8::ThrowException(v8::String::New("<where> must be a valid expression")));
  }

  where = TRI_CreateQueryWhereGeneral(cmd.c_str());

  // wrap it up
  return scope.Close(WrapWhere(where));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new constant where clause for primary index - DEPRECATED
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_WherePrimaryConstAql (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 1) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: wherePrimaryConst(<document-identifier>)")));
  }

  // extract the document identifier
  TRI_voc_cid_t cid = 0;
  TRI_voc_did_t did = 0;
  bool ok = IsDocumentId(argv[0], cid, did);

  if (! ok) {
    return scope.Close(v8::ThrowException(v8::String::New("expecting a <document-identifier>")));
  }

  // build document hash access
  TRI_qry_where_t* where = TRI_CreateQueryWherePrimaryConstant(did);

  // wrap it up
  return scope.Close(WrapWhere(where));
}


////////////////////////////////////////////////////////////////////////////////
/// @brief DEPRECATED
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_WhereHashConstAql (const v8::Arguments& argv) {
  v8::HandleScope scope;
  TRI_json_t* parameterList;

  if (argv.Length() < 2) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: AQL_WHERE_HASH_CONST(<index-identifier>, <value 1>, <value 2>,..., <value n>)")));
  }

  
  // ..........................................................................
  // check that the first parameter sent is a double value
  // ..........................................................................
  bool inValidType = true;
  TRI_idx_iid_t iid = TRI_ObjectToDouble(argv[0], inValidType); 
  
  if (inValidType || iid == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("<index-identifier> must be an positive integer")));       
  }  


  // ..........................................................................
  // Store the index field parameters in a json object 
  // ..........................................................................
  parameterList = TRI_CreateListJson();
  if (!parameterList) {
    return scope.Close(v8::ThrowException(v8::String::New("out of memory")));
  }

  for (int j = 1; j < argv.Length(); ++j) {  
    v8::Handle<v8::Value> parameter = argv[j];
    TRI_json_t* jsonParameter = ConvertHelper(parameter);
    if (jsonParameter == NULL) { // NOT the null json value! 
      return scope.Close(v8::ThrowException(v8::String::New("type value not currently supported for hash index")));       
    }
    TRI_PushBackListJson(parameterList, jsonParameter);
    
    /*
    if (parameter->IsBoolean() ) {
      v8::Handle<v8::Boolean> booleanParameter = parameter->ToBoolean();
      TRI_PushBackListJson(parameterList, TRI_CreateBooleanJson(booleanParameter->Value() ));
    }    
    else if ( parameter->IsNumber() ) {
      v8::Handle<v8::Number> numberParameter = parameter->ToNumber();
      TRI_PushBackListJson(parameterList, TRI_CreateNumberJson(numberParameter->Value() ));
    }
    else if ( parameter->IsString() ) {
      v8::Handle<v8::String>  stringParameter= parameter->ToString();
      v8::String::Utf8Value str(stringParameter);
      TRI_PushBackListJson(parameterList, TRI_CreateStringCopyJson(*str));
    }
    else if ( parameter->IsArray() ) {
      v8::Handle<v8::Array> arrayParameter = v8::Handle<v8::Array>(v8::Array::Cast(*parameter));
    }
    else {
      return scope.Close(v8::ThrowException(v8::String::New("type value not currently supported for hash index")));       
    }
    */
  }
  

  // build document hash access
  TRI_qry_where_t* where = TRI_CreateQueryWhereHashConstant(iid, parameterList);

  // wrap it up
  return scope.Close(WrapWhere(where));
  
}


////////////////////////////////////////////////////////////////////////////////
/// @brief DEPRECATED
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_WhereSkiplistConstAql (const v8::Arguments& argv) {
  v8::HandleScope scope;
  TRI_json_t* parameterList;

  if (argv.Length() < 2) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: AQL_WHERE_SL_CONST(<index-identifier>, <value 1>, <value 2>,..., <value n>)")));
  }

  
  // ..........................................................................
  // check that the first parameter sent is a double value
  // ..........................................................................
  bool inValidType = true;
  TRI_idx_iid_t iid = TRI_ObjectToDouble(argv[0], inValidType); 
  
  if (inValidType || iid == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("<index-identifier> must be an positive integer")));       
  }  


  // ..........................................................................
  // Store the index field parameters in a json object 
  // ..........................................................................
  parameterList = TRI_CreateListJson();
  if (!parameterList) {
    return scope.Close(v8::ThrowException(v8::String::New("out of memory")));
  }

  for (int j = 1; j < argv.Length(); ++j) {  
    v8::Handle<v8::Value> parameter = argv[j];
    TRI_json_t* jsonParameter = ConvertHelper(parameter);
    if (jsonParameter == NULL) { // NOT the null json value! 
      return scope.Close(v8::ThrowException(v8::String::New("type value not currently supported for skiplist index")));       
    }
    TRI_PushBackListJson(parameterList, jsonParameter);
  }
  

  // build where
  TRI_qry_where_t* where = TRI_CreateQueryWhereSkiplistConstant(iid, parameterList);

  // wrap it up
  return scope.Close(WrapWhere(where));
  
}

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new constant where clause for geo index - DEPRECATED
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_WhereWithinConstAql (v8::Arguments const& argv) {
  v8::HandleScope scope;

  char* nameDistance;

  if (argv.Length() == 4) {
    nameDistance = 0;
  }
  else if (argv.Length() == 5) {
    v8::String::Utf8Value name(argv[4]);

    if (*name == 0) {
      return scope.Close(v8::ThrowException(v8::String::New("<distance> must be an attribute name")));
    }

    nameDistance = TRI_DuplicateString(*name);
  }
  else {
    return scope.Close(v8::ThrowException(v8::String::New("usage: whereWithinConst(<index-identifier>, <latitiude>, <longitude>, <radius>[, <distance>])")));
  }

  // extract the document identifier
  TRI_idx_iid_t iid = TRI_ObjectToDouble(argv[0]);
  double latitiude = TRI_ObjectToDouble(argv[1]);
  double longitude = TRI_ObjectToDouble(argv[2]);
  double radius = TRI_ObjectToDouble(argv[3]);

  // build document hash access
  TRI_qry_where_t* where = TRI_CreateQueryWhereWithinConstant(iid, nameDistance, latitiude, longitude, radius);

  if (nameDistance != 0) {
    TRI_FreeString(nameDistance);
  }

  // wrap it up
  return scope.Close(WrapWhere(where));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new query from given parts - DEPRECATED
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_HashSelectAql (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 2) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: AQL_SELECT(collection, where)")));
  }

  // ...........................................................................
  // extract the primary collection
  // ...........................................................................
  v8::Handle<v8::Value> collectionArg = argv[0];
  if (! collectionArg->IsObject()) {
    return scope.Close(v8::ThrowException(v8::String::New("expecting a COLLECTION as second argument")));
  }
  v8::Handle<v8::Object> collectionObj = collectionArg->ToObject();
  v8::Handle<v8::String> err;
  const TRI_vocbase_col_t* collection = LoadCollection(collectionObj, &err);
  if (collection == 0) {
    return scope.Close(v8::ThrowException(err));
  }

  
  // ...........................................................................
  // Extract there hash where clause
  // ...........................................................................
  v8::Handle<v8::Value> whereArg = argv[1];
  TRI_qry_where_t* where = 0;
  if (whereArg->IsNull()) {
    return scope.Close(v8::ThrowException(v8::String::New("expecting a WHERE object as third argument")));
  }
  v8::Handle<v8::Object> whereObj = whereArg->ToObject();
  where = UnwrapClass<TRI_qry_where_t>(whereObj, WRP_QRY_WHERE_TYPE);
  if (where == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("corrupted WHERE")));
  }
    
  
  // ...........................................................................
  // Create the hash query 
  // ...........................................................................
  TRI_query_t* query = TRI_CreateHashQuery(where, collection->_collection); 
  if (!query) {
    return scope.Close(v8::ThrowException(v8::String::New("could not create query object")));
  }


  // ...........................................................................
  // wrap it up
  // ...........................................................................
  return scope.Close(WrapQuery(query));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new query from given parts - DEPRECATED
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_SkiplistSelectAql (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 2) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: AQL_SL_SELECT(collection, where)")));
  }

  // ...........................................................................
  // extract the primary collection
  // ...........................................................................
  v8::Handle<v8::Value> collectionArg = argv[0];
  if (! collectionArg->IsObject()) {
    return scope.Close(v8::ThrowException(v8::String::New("expecting a COLLECTION as second argument")));
  }
  v8::Handle<v8::Object> collectionObj = collectionArg->ToObject();
  v8::Handle<v8::String> err;
  const TRI_vocbase_col_t* collection = LoadCollection(collectionObj, &err);
  if (collection == 0) {
    return scope.Close(v8::ThrowException(err));
  }

  
  // ...........................................................................
  // Extract the where clause
  // ...........................................................................
  v8::Handle<v8::Value> whereArg = argv[1];
  TRI_qry_where_t* where = 0;
  if (whereArg->IsNull()) {
    return scope.Close(v8::ThrowException(v8::String::New("expecting a WHERE object as third argument")));
  }
  v8::Handle<v8::Object> whereObj = whereArg->ToObject();
  where = UnwrapClass<TRI_qry_where_t>(whereObj, WRP_QRY_WHERE_TYPE);
  if (where == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("corrupted WHERE")));
  }
    
  
  // ...........................................................................
  // Create the skiplist query 
  // ...........................................................................
  TRI_query_t* query = TRI_CreateSkiplistQuery(where, collection->_collection); 
  if (!query) {
    return scope.Close(v8::ThrowException(v8::String::New("could not create query object")));
  }


  // ...........................................................................
  // wrap it up
  // ...........................................................................
  return scope.Close(WrapQuery(query));
  
}


////////////////////////////////////////////////////////////////////////////////
/// @brief executes a select query - DEPRECATED
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_SelectAql (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 4) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: AQL_SELECT(<db>, <collectionname>, <skip>, <limit>)")));
  }

  v8::Handle<v8::Object> dbArg = argv[0]->ToObject();
  TRI_vocbase_t* vocbase = UnwrapClass<TRI_vocbase_t>(dbArg, WRP_VOCBASE_TYPE);

  if (vocbase == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("corrupted vocbase")));
  }

  // select clause
  TRI_qry_select_t* select;
  select = TRI_CreateQuerySelectDocument();

  v8::Handle<v8::Value> nameArg = argv[1];
  string name = TRI_ObjectToString(nameArg);

  if (name.empty()) {
    select->free(select);
    return scope.Close(v8::ThrowException(v8::String::New("expecting a non-empty name for <collectionname>")));
  }

  // extract the skip value
  TRI_voc_size_t skip = 0;
  v8::Handle<v8::Value> skipArg = argv[2];

  if (skipArg->IsNull()) {
    skip = TRI_QRY_NO_SKIP;
  }
  else {
    double s = TRI_ObjectToDouble(skipArg);

    if (s < 0.0) {
      skip = 0;
    }
    else {
      skip = (TRI_voc_size_t) s;
    }
  }

  // extract the limit value
  TRI_voc_ssize_t limit = 0;
  v8::Handle<v8::Value> limitArg = argv[3];

  if (limitArg->IsNull()) {
    limit = TRI_QRY_NO_LIMIT;
  }
  else {
    double l = TRI_ObjectToDouble(limitArg);

    limit = (TRI_voc_ssize_t) l;
  }

  TRI_select_join_t* join;
  join = TRI_CreateSelectJoin();
  if (!join) {
    select->free(select);
    return scope.Close(v8::ThrowException(v8::String::New("could not create join struct")));
  }
  
  TRI_AddPartSelectJoinX(join, 
                        JOIN_TYPE_PRIMARY, 
                        NULL,
                        (char*) name.c_str(), 
                        (char*) "alias",
                        NULL);

  // create the query
  TRI_query_t* query = TRI_CreateQuery(vocbase,
                                       select,
                                       NULL,
                                       join,
                                       skip,
                                       limit); 

  if (!query) {
    select->free(select);
    return scope.Close(v8::ThrowException(v8::String::New("could not create query object")));
  }

  // wrap it up
  return scope.Close(WrapQuery(query));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a query - DEPRECATED
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_ExecuteAql (v8::Arguments const& argv) {
  v8::HandleScope scope;

  v8::Handle<v8::Value> err;
  TRI_rc_cursor_t* cursor = ExecuteQuery(argv.Holder(), &err);

  if (cursor == 0) {
    return v8::ThrowException(err);
  }

  return scope.Close(WrapCursor(cursor));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a query, returns a cursor
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_ExecuteQueryInstance (v8::Arguments const& argv) {
  v8::HandleScope scope;
  
  // return number of total records in cursor?
  bool doCount = false;
  if (argv.Length() > 0) {
    doCount = TRI_ObjectToBoolean(argv[0]);
  }

  // maximum number of results to return at once
  uint32_t max = 1000;
  if (argv.Length() > 1) {
    double maxValue = TRI_ObjectToDouble(argv[1]);
    if (maxValue >= 1.0) {
      max = (uint32_t) maxValue;
    }
  }
  
  v8::Handle<v8::Value> err;
  TRI_query_cursor_t* cursor = ExecuteQueryInstance(argv.Holder(), doCount, max, &err);

  if (!cursor) {
    return v8::ThrowException(err);
  }
  
  // we must do this to increase the refcount of the shadow
  TRI_shadow_t* shadow = TRI_FindShadowData(cursor->_vocbase->_cursors, cursor->_shadow->_id);
  assert(shadow);

  return scope.Close(WrapQueryCursor(cursor));
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                        AQL CURSOR
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy a cursor
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_DisposeQueryCursor (v8::Arguments const& argv) {
  v8::HandleScope scope;
  v8::TryCatch tryCatch;

  if (argv.Length() != 0) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: dispose()")));
  }

  v8::Handle<v8::Object> self = argv.Holder();
  TRI_query_cursor_t* cursor = UnwrapQueryCursor(self);
  if (!cursor) {
    return scope.Close(v8::ThrowException(v8::String::New("corrupted cursor")));
  }

  // set the deleted flag so the gc can catch this instance
  TRI_LockQueryCursor(cursor);
  cursor->_deleted = true;
  TRI_UnlockQueryCursor(cursor);

  return scope.Close(v8::BooleanObject::New(true));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the id of the cursor
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_IdQueryCursor (v8::Arguments const& argv) {
  v8::HandleScope scope;
  v8::TryCatch tryCatch;

  if (argv.Length() != 0) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: id()")));
  }

  v8::Handle<v8::Object> self = argv.Holder();
  TRI_query_cursor_t* cursor = UnwrapQueryCursor(self);

  if (!cursor) {
    return scope.Close(v8::ThrowException(v8::String::New("corrupted cursor")));
  }

  TRI_LockQueryCursor(cursor);
  if (cursor->_deleted) {
    TRI_UnlockQueryCursor(cursor);
    return scope.Close(v8::ThrowException(v8::String::New("corrupted cursor")));
  }

  TRI_shadow_id id = cursor->_shadow->_id;
  TRI_UnlockQueryCursor(cursor);

  return scope.Close(v8::Number::New(id));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the number of documents
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_CountQueryCursor (v8::Arguments const& argv) {
  v8::HandleScope scope;
  v8::TryCatch tryCatch;

  if (argv.Length() != 0) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: count()")));
  }

  v8::Handle<v8::Object> self = argv.Holder();
  TRI_query_cursor_t* cursor = UnwrapQueryCursor(self);

  if (!cursor) {
    return scope.Close(v8::ThrowException(v8::String::New("corrupted cursor")));
  }

  TRI_LockQueryCursor(cursor);
  if (cursor->_deleted) {
    TRI_UnlockQueryCursor(cursor);
    return scope.Close(v8::ThrowException(v8::String::New("corrupted cursor")));
  }
  TRI_select_size_t length = cursor->_length;
  TRI_UnlockQueryCursor(cursor);
  
  return scope.Close(v8::Number::New(length));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the next document 
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_NextQueryCursor (v8::Arguments const& argv) {
  v8::HandleScope scope;
  v8::TryCatch tryCatch;

  if (argv.Length() != 0) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: next()")));
  }

  v8::Handle<v8::Object> self = argv.Holder();
  TRI_query_cursor_t* cursor = UnwrapQueryCursor(self);

  if (!cursor) {
    return scope.Close(v8::ThrowException(v8::String::New("corrupted cursor")));
  }

  v8::Handle<v8::Value> value;

  TRI_LockQueryCursor(cursor);
  if (cursor->_deleted) {
    TRI_UnlockQueryCursor(cursor);
    return scope.Close(v8::ThrowException(v8::String::New("corrupted cursor")));
  }

  bool ok = true;
  TRI_js_exec_context_t context = NULL;
  
  // exceptions must be caught in the following part because we hold an exclusive
  // lock that might otherwise not be freed
  try {
    TRI_rc_result_t* next = cursor->next(cursor);
    if (!next) {
      value = v8::Undefined();
    }
    else {
      context = TRI_CreateExecutionContext(cursor->_functionCode);
      if (context) {
        TRI_DefineSelectExecutionContext(context, next);
        ok = TRI_ExecuteExecutionContext(context, (void*) &value);
      }
    }
  }
  catch (...) {
  }

  if (context) {
    TRI_FreeExecutionContext(context);
  }
  // always free lock
  TRI_UnlockQueryCursor(cursor);

  if (!ok) {
    if (tryCatch.HasCaught()) {
      return scope.Close(v8::ThrowException(tryCatch.Exception()));
    }
    else {
      return scope.Close(v8::ThrowException(v8::String::New("cannot convert to JavaScript")));
    }
  }

  return scope.Close(value);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the next x rows from the cursor in one go
///
/// This function constructs multiple rows at once and should be preferred over
/// hasNext()...next() when iterating over bigger result sets
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_GetRowsQueryCursor (v8::Arguments const& argv) {
  v8::HandleScope scope;
  v8::TryCatch tryCatch;

  if (argv.Length() != 0) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: getRows()")));
  }

  v8::Handle<v8::Object> self = argv.Holder();
  TRI_query_cursor_t* cursor = UnwrapQueryCursor(self);

  if (!cursor) {
    return scope.Close(v8::ThrowException(v8::String::New("corrupted cursor")));
  }
  
  v8::Handle<v8::Array> result = v8::Array::New();

  TRI_LockQueryCursor(cursor);
  if (cursor->_deleted) {
    TRI_UnlockQueryCursor(cursor);
    return scope.Close(v8::ThrowException(v8::String::New("corrupted cursor")));
  }
    
  TRI_js_exec_context_t context = NULL;
  bool ok = true;
  
  // exceptions must be caught in the following part because we hold an exclusive
  // lock that might otherwise not be freed
  try {
    uint32_t max = cursor->getMax(cursor);
    context = TRI_CreateExecutionContext(cursor->_functionCode);

    if (context) {
      for (uint32_t i = 0; i < max; i++) {
        TRI_rc_result_t* next = cursor->next(cursor);
        if (!next) {
          break;
        }
      
        TRI_DefineSelectExecutionContext(context, next);
        v8::Handle<v8::Value> value;
        ok = TRI_ExecuteExecutionContext(context, (void*) &value);
        if (ok) {
          result->Set(i, value);
        }
      }
    }
  }
  catch (...) {
  }
      
  if (context) {
    TRI_FreeExecutionContext(context);
  }
  // always free lock
  TRI_UnlockQueryCursor(cursor);

  if (!ok) {
    if (tryCatch.HasCaught()) {
      return scope.Close(v8::ThrowException(tryCatch.Exception()));
    }
    else {
      return scope.Close(v8::ThrowException(v8::String::New("cannot convert to JavaScript")));
    }
  }

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return max number of results per transfer for cursor
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_GetMaxQueryCursor (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 0) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: getMax()")));
  }

  v8::Handle<v8::Object> self = argv.Holder();
  TRI_query_cursor_t* cursor = UnwrapQueryCursor(self);

  if (!cursor) {
    return scope.Close(v8::ThrowException(v8::String::New("corrupted cursor")));
  }

  TRI_LockQueryCursor(cursor);
  if (cursor->_deleted) {
    TRI_UnlockQueryCursor(cursor);
    return scope.Close(v8::ThrowException(v8::String::New("corrupted cursor")));
  }

  uint32_t max = cursor->getMax(cursor);
  TRI_UnlockQueryCursor(cursor);
  
  return scope.Close(v8::Number::New(max));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return if count flag was set for cursor
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_HasCountQueryCursor (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 0) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: hasCount()")));
  }

  v8::Handle<v8::Object> self = argv.Holder();
  TRI_query_cursor_t* cursor = UnwrapQueryCursor(self);

  if (!cursor) {
    return scope.Close(v8::ThrowException(v8::String::New("corrupted cursor")));
  }

  TRI_LockQueryCursor(cursor);
  if (cursor->_deleted) {
    TRI_UnlockQueryCursor(cursor);
    return scope.Close(v8::ThrowException(v8::String::New("corrupted cursor")));
  }

  bool hasCount = cursor->hasCount(cursor);
  TRI_UnlockQueryCursor(cursor);
  
  return scope.Close(hasCount ? v8::True() : v8::False());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if the cursor is exhausted 
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_HasNextQueryCursor (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 0) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: hasNext()")));
  }

  v8::Handle<v8::Object> self = argv.Holder();
  TRI_query_cursor_t* cursor = UnwrapQueryCursor(self);

  if (!cursor) {
    return scope.Close(v8::ThrowException(v8::String::New("corrupted cursor")));
  }

  TRI_LockQueryCursor(cursor);
  if (cursor->_deleted) {
    TRI_UnlockQueryCursor(cursor);
    return scope.Close(v8::ThrowException(v8::String::New("corrupted cursor")));
  }

  bool hasNext = cursor->hasNext(cursor);
  TRI_UnlockQueryCursor(cursor);
  
  return scope.Close(hasNext ? v8::True() : v8::False());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the next document - DEPRECATED
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_CountCursor (v8::Arguments const& argv) {
  v8::HandleScope scope;
  v8::TryCatch tryCatch;

  if (argv.Length() != 0) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: count()")));
  }

  v8::Handle<v8::Object> self = argv.Holder();
  TRI_rc_cursor_t* cursor = UnwrapCursor(self);

  if (cursor == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("corrupted cursor")));
  }

  
  return scope.Close(v8::Number::New(cursor->_matchedDocuments));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the next document - DEPRECATED
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_NextCursor (v8::Arguments const& argv) {
  v8::HandleScope scope;
  v8::TryCatch tryCatch;

  if (argv.Length() != 0) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: next()")));
  }

  v8::Handle<v8::Object> self = argv.Holder();
  TRI_rc_cursor_t* cursor = UnwrapCursor(self);

  if (cursor == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("corrupted cursor")));
  }

  TRI_rc_result_t* next = cursor->next(cursor);

  if (next == 0) {
    return scope.Close(v8::Undefined());
  }

  v8::Handle<v8::Value> value;
  bool ok;

  TRI_qry_select_t* select = cursor->_select;
  if (select) {
    ok = select->toJavaScript(select, next, (void*) &value);
  }
  else {
    TRI_DefineSelectExecutionContext(cursor->_selectContext, next);
    ok = TRI_ExecuteExecutionContext(cursor->_selectContext, (void*) &value);
  }

  if (! ok) {
    if (tryCatch.HasCaught()) {
      return scope.Close(v8::ThrowException(tryCatch.Exception()));
    }
    else {
      return scope.Close(v8::ThrowException(v8::String::New("cannot convert to JavaScript")));
    }
  }

  return scope.Close(value);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the next document reference - DEPRECATED
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_NextRefCursor (v8::Arguments const& argv) {
  v8::HandleScope scope;
  v8::TryCatch tryCatch;

  if (argv.Length() != 0) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: nextRef()")));
  }

  v8::Handle<v8::Object> self = argv.Holder();
  TRI_rc_cursor_t* cursor = UnwrapCursor(self);

  if (cursor == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("corrupted cursor")));
  }

  TRI_rc_result_t* next = cursor->next(cursor);

  if (next == 0) {
    return scope.Close(v8::Undefined());
  }

  // always use the primary collection
  TRI_voc_cid_t cid = cursor->_context->_primary->base._cid;
  TRI_voc_did_t did = next->_primary->_did;
  string ref = StringUtils::itoa(cid) + ":" + StringUtils::itoa(did);

  return scope.Close(v8::String::New(ref.c_str()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief uses the next document - DEPRECATED
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_UseNextCursor (v8::Arguments const& argv) {
  v8::HandleScope scope;
  v8::TryCatch tryCatch;

  if (argv.Length() != 0) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: nextRef()")));
  }

  v8::Handle<v8::Object> self = argv.Holder();
  TRI_rc_cursor_t* cursor = UnwrapCursor(self);

  if (cursor == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("corrupted cursor")));
  }

  TRI_rc_result_t* next = cursor->next(cursor);

  if (next == 0) {
    return scope.Close(v8::Undefined());
  }

  return scope.Close(v8::True());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if the cursor is exhausted - DEPRECATED
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_HasNextCursor (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 0) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: hasNext()")));
  }

  v8::Handle<v8::Object> self = argv.Holder();
  TRI_rc_cursor_t* cursor = UnwrapCursor(self);

  if (cursor == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("corrupted cursor")));
  }

  if (cursor->hasNext(cursor)) {
    return scope.Close(v8::True());
  }
  else {
    return scope.Close(v8::False());
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                       TRI_VOCBASE_COL_T FUNCTIONS
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                              javascript functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief counts the number of documents in a result set
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_CountVocbaseCol (v8::Arguments const& argv) {
  v8::HandleScope scope;

  v8::Handle<v8::String> err;
  TRI_vocbase_col_t const* col = LoadCollection(argv.Holder(), &err);

  if (col == 0) {
    return scope.Close(v8::ThrowException(err));
  }

  TRI_doc_collection_t* collection = col->_collection;

  return scope.Close(v8::Number::New(collection->size(collection)));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes a document
///
/// @FUN{delete(@FA{document-identifier})}
///
/// Deletes a document with a given document identifier. If the document does
/// not exists, then @LIT{false} is returned. If the document existed and was
/// deleted, then @LIT{true} is returned. An exception is thrown in case of an
/// error.
///
/// @verbinclude fluent16
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_DeleteVocbaseCol (v8::Arguments const& argv) {
  v8::HandleScope scope;

  v8::Handle<v8::String> err;
  TRI_vocbase_col_t const* col = LoadCollection(argv.Holder(), &err);

  if (col == 0) {
    return scope.Close(v8::ThrowException(err));
  }

  TRI_doc_collection_t* collection = col->_collection;

  if (argv.Length() != 1) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: delete(<document-identifier>)")));
  }

  TRI_voc_cid_t cid = collection->base._cid;
  TRI_voc_did_t did;

  if (IsDocumentId(argv[0], cid, did)) {
    if (cid != collection->base._cid) {
      return scope.Close(v8::ThrowException(v8::String::New("cannot execute cross collection delete")));
    }
  }
  else {
    return scope.Close(v8::ThrowException(v8::String::New("expecting a document identifier")));
  }

  // .............................................................................
  // inside a write transaction
  // .............................................................................

  bool ok = collection->destroyLock(collection, did, 0, TRI_DOC_UPDATE_LAST_WRITE);

  // .............................................................................
  // outside a write transaction
  // .............................................................................

  if (! ok) {
    if (TRI_errno() == TRI_VOC_ERROR_DOCUMENT_NOT_FOUND) {
      return scope.Close(v8::False());
    }
    else {
      string err = "cannot delete document: ";
      err += TRI_last_error();

      return scope.Close(v8::ThrowException(v8::String::New(err.c_str())));
    }
  }

  return scope.Close(v8::True());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief drops an index
///
/// @FUN{dropIndex(@FA{index-identifier})}
///
/// Drops the index. If the index does not exists, then @LIT{false} is
/// returned. If the index existed and was dropped, then @LIT{true} is returned.
///
/// @verbinclude fluent17
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_DropIndexVocbaseCol (v8::Arguments const& argv) {
  v8::HandleScope scope;

  v8::Handle<v8::String> err;
  TRI_vocbase_col_t const* col = LoadCollection(argv.Holder(), &err);

  if (col == 0) {
    return scope.Close(v8::ThrowException(err));
  }

  TRI_doc_collection_t* collection = col->_collection;

  if (collection->base._type != TRI_COL_TYPE_SIMPLE_DOCUMENT) {
    return scope.Close(v8::ThrowException(v8::String::New("unknown collection type")));
  }

  TRI_sim_collection_t* sim = (TRI_sim_collection_t*) collection;

  if (argv.Length() != 1) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: dropIndex(<index-identifier>)")));
  }

  double iid = TRI_ObjectToDouble(argv[0]);

  // .............................................................................
  // inside a write transaction
  // .............................................................................

  bool ok = TRI_DropIndexSimCollection(sim, (TRI_idx_iid_t) iid);

  // .............................................................................
  // outside a write transaction
  // .............................................................................

  return scope.Close(ok ? v8::True() : v8::False());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that a geo index exists
///
/// @FUN{ensureGeoIndex(@FA{location})}
///
/// Creates a geo-spatial index on all documents using @FA{location} as path to
/// the coordinates. The value of the attribute must be a list with at least two
/// double values. The list must contain the latitiude (first value) and the
/// longitude (second value). All documents, which do not have the attribute
/// path or with value that are not suitable, are ignored.
///
/// In case that the index was successfully created, the index indetifier
/// is returned.
///
/// @FUN{ensureGeoIndex(@FA{location}, @LIT{true})}
///
/// As above which the exception, that the order within the list is longitude
/// followed by latitiude. This corresponds to the format described in
///
/// http://geojson.org/geojson-spec.html#positions
///
/// @FUN{ensureGeoIndex(@FA{latitiude}, @FA{longitude})}
///
/// Creates a geo-spatial index on all documents using @FA{latitiude} and
/// @FA{longitude} as paths the latitiude and the longitude. The value of the
/// attribute @FA{latitiude} and of the attribute @FA{longitude} must a
/// double. All documents, which do not have the attribute paths or which values
/// are not suitable, are ignored.
///
/// In case that the index was successfully created, the index indetifier
/// is returned.
///
/// @EXAMPLES
///
/// Create an geo index for a list attribute:
///
/// @verbinclude admin3
///
/// Create an geo index for a hash array attribute:
///
/// @verbinclude admin4
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_EnsureGeoIndexVocbaseCol (v8::Arguments const& argv) {
  v8::HandleScope scope;

  v8::Handle<v8::String> err;
  TRI_vocbase_col_t const* col = LoadCollection(argv.Holder(), &err);

  if (col == 0) {
    return scope.Close(v8::ThrowException(err));
  }

  TRI_doc_collection_t* collection = col->_collection;

  if (collection->base._type != TRI_COL_TYPE_SIMPLE_DOCUMENT) {
    return scope.Close(v8::ThrowException(v8::String::New("unknown collection type")));
  }

  TRI_sim_collection_t* sim = (TRI_sim_collection_t*) collection;
  TRI_idx_iid_t iid = 0;

  // .............................................................................
  // case: <location>
  // .............................................................................

  if (argv.Length() == 1) {
    v8::String::Utf8Value loc(argv[0]);

    if (*loc == 0) {
      return scope.Close(v8::ThrowException(v8::String::New("<location> must be an attribute path")));
    }

    iid = TRI_EnsureGeoIndexSimCollection(sim, *loc, false);
  }

  // .............................................................................
  // case: <location>, <geoJson>
  // .............................................................................

  else if (argv.Length() == 2 && (argv[1]->IsBoolean() || argv[1]->IsBooleanObject())) {
    v8::String::Utf8Value loc(argv[0]);

    if (*loc == 0) {
      return scope.Close(v8::ThrowException(v8::String::New("<location> must be an attribute path")));
    }

    iid = TRI_EnsureGeoIndexSimCollection(sim, *loc, TRI_ObjectToBoolean(argv[1]));
  }

  // .............................................................................
  // case: <latitiude>, <longitude>
  // .............................................................................

  else if (argv.Length() == 2) {
    v8::String::Utf8Value lat(argv[0]);
    v8::String::Utf8Value lon(argv[1]);

    if (*lat == 0) {
      return scope.Close(v8::ThrowException(v8::String::New("<latitiude> must be an attribute path")));
    }

    if (*lon == 0) {
      return scope.Close(v8::ThrowException(v8::String::New("<longitude> must be an attribute path")));
    }

    iid = TRI_EnsureGeoIndex2SimCollection(sim, *lat, *lon);
  }

  // .............................................................................
  // error case
  // .............................................................................

  else {
    return scope.Close(v8::ThrowException(v8::String::New("usage: ensureGeoIndex(<latitiude>, <longitude>) or ensureGeoIndex(<location>, [<geojson>])")));
  }

  return scope.Close(v8::Number::New(iid));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that a hash index exists
///
/// @FUN{ensureUniqueConstrain(@FA{field1}, @FA{field2}, ...,@FA{fieldn})}
///
/// Creates a hash index on all documents using attributes as paths to the
/// fields. At least one attribute must be given. The value of this attribute
/// must be a list. All documents, which do not have the attribute path or where
/// one or more values that are not suitable, are ignored.
///
/// In case that the index was successfully created, the index indetifier
/// is returned.
///
/// @EXAMPLES
///
/// @verbinclude admin5
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_EnsureUniqueConstraintVocbaseCol (v8::Arguments const& argv) {
  v8::HandleScope scope;  
  v8::Handle<v8::String> err;
  
  // .............................................................................
  // Check that we have a valid collection                      
  // .............................................................................

  TRI_vocbase_col_t const* col = LoadCollection(argv.Holder(), &err);
  
  if (col == 0) {
    return scope.Close(v8::ThrowException(err));
  }
  
  // .............................................................................
  // Check collection type
  // .............................................................................

  TRI_doc_collection_t* collection = col->_collection;
  
  if (collection->base._type != TRI_COL_TYPE_SIMPLE_DOCUMENT) {
    return scope.Close(v8::ThrowException(v8::String::New("unknown collection type")));
  }

  TRI_sim_collection_t* sim = (TRI_sim_collection_t*) collection;
  
  // .............................................................................
  // Ensure that there is at least one string parameter sent to this method
  // .............................................................................  

  if (argv.Length() == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("one or more string parameters required for the ensureUniqueConstraint(...) command")));
  }
  
  // .............................................................................
  // The index id which will either be an existing one or a newly created 
  // hash index. 
  // .............................................................................  

  TRI_idx_iid_t iid = 0;
  
  // .............................................................................
  // Return string when there is an error of some sort.
  // .............................................................................

  string errorString;
  
  // .............................................................................
  // Create a list of paths, these will be used to create a list of shapes
  // which will be used by the hash index.
  // .............................................................................
  
  TRI_vector_t attributes;
  TRI_InitVector(&attributes,sizeof(char*));
  
  bool ok = true;
  
  for (int j = 0; j < argv.Length(); ++j) {
    v8::Handle<v8::Value> argument = argv[j];

    if (! argument->IsString() ) {
      errorString = "invalid parameter passed to ensureUniqueConstraint(...) command";
      ok = false;
      break;
    }
    
    // ...........................................................................
    // convert the argument into a "C" string
    // ...........................................................................
    
    v8::String::Utf8Value argumentString(argument);   
    char* cArgument = *argumentString == 0 ? 0 : TRI_DuplicateString(*argumentString);

    if (cArgument == NULL) {
      errorString = "insuffient memory to complete ensureUniqueConstraint(...) command";
      ok = false;
      break;
    }  

    TRI_PushBackVector(&attributes, &cArgument);
  }
  
  // .............................................................................
  // Check that each parameter is unique
  // .............................................................................
  
  for (size_t j = 0; j < attributes._length; ++j) {  
    char* left = *((char**) (TRI_AtVector(&attributes, j)));    

    for (size_t k = j + 1; k < attributes._length; ++k) {
      char* right = *((char**) (TRI_AtVector(&attributes, k)));

      if (TRI_EqualString(left, right)) {
        errorString = "duplicate parameters sent to ensureUniqueConstraint(...) command";
        ok = false;
        break;
      }   
    }
  }    
  
  // .............................................................................
  // Some sort of error occurred -- display error message and abort index creation
  // (or index retrieval).
  // .............................................................................
  
  if (!ok) {

    // Remove the memory allocated to the list of attributes used for the hash index
    for (size_t j = 0; j < attributes._length; ++j) {
      char* cArgument = *((char**) (TRI_AtVector(&attributes, j)));
      TRI_Free(cArgument);
    }    

    TRI_DestroyVector(&attributes);
    return scope.Close(v8::ThrowException(v8::String::New(errorString.c_str(), errorString.length())));
  }  
  
  // .............................................................................
  // Actually create the index here
  // .............................................................................

  iid = TRI_EnsureHashIndexSimCollection(sim, &attributes, true);

  // .............................................................................
  // Remove the memory allocated to the list of attributes used for the hash index
  // .............................................................................   

  for (size_t j = 0; j < attributes._length; ++j) {
    char* cArgument = *((char**) (TRI_AtVector(&attributes, j)));
    TRI_Free(cArgument);
  }    

  TRI_DestroyVector(&attributes);

  if (iid == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("hash index could not be created")));
  }  
  
  // .............................................................................
  // Return the newly assigned index identifier
  // .............................................................................

  return scope.Close(v8::Number::New(iid));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that a hash index exists
///
/// @FUN{ensureMutliHashIndex(@FA{field1}, @FA{field2}, ...,@FA{fieldn})}
///
/// Creates a non-unique hash index on all documents using attributes as paths
/// to the fields. At least one attribute must be given. All documents, which do
/// not have the attribute path or with one or more values that are not
/// suitable, are ignored.
///
/// In case that the index was successfully created, the index indetifier
/// is returned.
///
/// @verbinclude fluent14
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_EnsureMultiHashIndexVocbaseCol (v8::Arguments const& argv) {
  v8::HandleScope scope;  
  v8::Handle<v8::String> err;
  
  // .............................................................................
  // Check that we have a valid collection                      
  // .............................................................................

  TRI_vocbase_col_t const* col = LoadCollection(argv.Holder(), &err);
  
  if (col == 0) {
    return scope.Close(v8::ThrowException(err));
  }
  
  // .............................................................................
  // Check collection type
  // .............................................................................

  TRI_doc_collection_t* collection = col->_collection;
  
  if (collection->base._type != TRI_COL_TYPE_SIMPLE_DOCUMENT) {
    return scope.Close(v8::ThrowException(v8::String::New("unknown collection type")));
  }

  TRI_sim_collection_t* sim = (TRI_sim_collection_t*) collection;

  // .............................................................................
  // Ensure that there is at least one string parameter sent to this method
  // .............................................................................  

  if (argv.Length() == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("one or more string parameters required for the ensureHashIndex(...) command")));
  }
  
  // .............................................................................
  // The index id which will either be an existing one or a newly created 
  // hash index. 
  // .............................................................................  

  TRI_idx_iid_t iid = 0;
  
  // .............................................................................
  // Return string when there is an error of some sort.
  // .............................................................................

  string errorString;
  
  // .............................................................................
  // Create a list of paths, these will be used to create a list of shapes
  // which will be used by the hash index.
  // .............................................................................
  
  TRI_vector_t attributes;
  TRI_InitVector(&attributes,sizeof(char*));
  
  bool ok = true;
  
  for (int j = 0; j < argv.Length(); ++j) {
    v8::Handle<v8::Value> argument = argv[j];

    if (! argument->IsString() ) {
      errorString = "invalid parameter passed to ensureMultiHashIndex(...) command";
      ok = false;
      break;
    }
    
    // ...........................................................................
    // convert the argument into a "C" string
    // ...........................................................................
    
    v8::String::Utf8Value argumentString(argument);   
    char* cArgument = *argumentString == 0 ? 0 : TRI_DuplicateString(*argumentString);

    if (cArgument == NULL) {
      errorString = "insuffient memory to complete ensureMultiHashIndex(...) command";
      ok = false;
      break;
    }  
        
    TRI_PushBackVector(&attributes,&cArgument);
  }
  
  // .............................................................................
  // Check that each parameter is unique
  // .............................................................................
  
  for (size_t j = 0; j < attributes._length; ++j) {  
    char* left = *((char**) (TRI_AtVector(&attributes, j)));    

    for (size_t k = j + 1; k < attributes._length; ++k) {
      char* right = *((char**) (TRI_AtVector(&attributes, k)));

      if (TRI_EqualString(left,right)) {
        errorString = "duplicate parameters sent to ensureMultiHashIndex(...) command";
        ok = false;
        break;
      }   
    }
  }    
  
  // .............................................................................
  // Some sort of error occurred -- display error message and abort index creation
  // (or index retrieval).
  // .............................................................................
  
  if (!ok) {

    // Remove the memory allocated to the list of attributes used for the hash index
    for (size_t j = 0; j < attributes._length; ++j) {
      char* cArgument = *((char**) (TRI_AtVector(&attributes, j)));
      TRI_Free(cArgument);
    }    

    TRI_DestroyVector(&attributes);
    return scope.Close(v8::ThrowException(v8::String::New(errorString.c_str(), errorString.length())));
  }  
  
  // .............................................................................
  // Actually create the index here
  // .............................................................................

  iid = TRI_EnsureHashIndexSimCollection(sim, &attributes, false);

  // .............................................................................
  // Remove the memory allocated to the list of attributes used for the hash index
  // .............................................................................   

  for (size_t j = 0; j < attributes._length; ++j) {
    char* cArgument = *((char**) (TRI_AtVector(&attributes, j)));
    TRI_Free(cArgument);
  }    

  TRI_DestroyVector(&attributes);

  if (iid == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("non-unique hash index could not be created")));
  }  
  
  // .............................................................................
  // Return the newly assigned index identifier
  // .............................................................................

  return scope.Close(v8::Number::New(iid));
}




////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that a skiplist index exists
///
/// @FUN{ensureSLIndex(@FA{field1}, @FA{field2}, ...,@FA{fieldn})}
///
/// Creates a skiplist index on all documents using attributes as paths to
/// the fields. At least one attribute must be given.  
/// All documents, which do not have the attribute path or 
/// with ore or more values that are not suitable, are ignored.
///
/// In case that the index was successfully created, the index indetifier
/// is returned.
///
/// @verbinclude fluent14
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_EnsureSkiplistIndexVocbaseCol (v8::Arguments const& argv) {

  v8::HandleScope scope;  
  v8::Handle<v8::String> err;
  
  
  // .............................................................................
  // Check that we have a valid collection                      
  // .............................................................................
  TRI_vocbase_col_t const* col = LoadCollection(argv.Holder(), &err);
  
  if (col == 0) {
    return scope.Close(v8::ThrowException(err));
  }

  
  // .............................................................................
  // Check collection type
  // .............................................................................
  TRI_doc_collection_t* collection = col->_collection;
  
  if (collection->base._type != TRI_COL_TYPE_SIMPLE_DOCUMENT) {
    return scope.Close(v8::ThrowException(v8::String::New("unknown collection type")));
  }
  TRI_sim_collection_t* sim = (TRI_sim_collection_t*) collection;

  
  // .............................................................................
  // Return string when there is an error of some sort.
  // .............................................................................
  string errorString;
  
  
  // .............................................................................
  // Ensure that there is at least one string parameter sent to this method
  // .............................................................................  
  if (argv.Length() == 0) {
    errorString = "one or more string parameters required for the ensureSLIndex(...) command";
    return scope.Close(v8::String::New(errorString.c_str(),errorString.length()));
  }
  
    
  // .............................................................................
  // The index id which will either be an existing one or a newly created 
  // hash index. 
  // .............................................................................  
  TRI_idx_iid_t iid = 0;
  
  
  // .............................................................................
  // Create a list of paths, these will be used to create a list of shapes
  // which will be used by the hash index.
  // .............................................................................
  
  TRI_vector_t attributes;
  TRI_InitVector(&attributes,sizeof(char*));
  
  bool ok = true;
  
  for (int j = 0; j < argv.Length(); ++j) {
  
    v8::Handle<v8::Value> argument = argv[j];
    if (! argument->IsString() ) {
      errorString = "invalid parameter passed to ensureSLIndex(...) command";
      ok = false;
      break;
    }
    
    // ...........................................................................
    // convert the argument into a "C" string
    // ...........................................................................
    
    v8::String::Utf8Value argumentString(argument);   
    char* cArgument = (char*) (TRI_Allocate(argumentString.length() + 1));       
    if (cArgument == NULL) {
      errorString = "insuffient memory to complete ensureSLIndex(...) command";
      ok = false;
      break;
    }  
        
    memcpy(cArgument, *argumentString, argumentString.length());
    TRI_PushBackVector(&attributes,&cArgument);
  }

  
  // .............................................................................
  // Check that each parameter is unique
  // .............................................................................
  
  for (size_t j = 0; j < attributes._length; ++j) {  
    char* left = *((char**) (TRI_AtVector(&attributes, j)));    
    for (size_t k = j + 1; k < attributes._length; ++k) {
      char* right = *((char**) (TRI_AtVector(&attributes, k)));
      if (strcmp(left,right) == 0) {
        errorString = "duplicate parameters sent to ensureSLIndex(...) command";
        //printf("%s:%s:%u:%s:%s\n",__FILE__,__FUNCTION__,__LINE__,left,right);
        ok = false;
        break;
      }   
    }
  }    
     
  
  // .............................................................................
  // Some sort of error occurred -- display error message and abort index creation
  // (or index retrieval).
  // .............................................................................
  
  if (!ok) {
    // ...........................................................................
    // Remove the memory allocated to the list of attributes used for the hash index
    // ...........................................................................   
    for (size_t j = 0; j < attributes._length; ++j) {
      char* cArgument = *((char**) (TRI_AtVector(&attributes, j)));
      TRI_Free(cArgument);
    }    
    TRI_DestroyVector(&attributes);
    return scope.Close(v8::String::New(errorString.c_str(),errorString.length()));
  }  
  
  
  // .............................................................................
  // Actually create the index here
  // .............................................................................
  iid = TRI_EnsureSkiplistIndexSimCollection(sim, &attributes, true);

  // .............................................................................
  // Remove the memory allocated to the list of attributes used for the hash index
  // .............................................................................   
  for (size_t j = 0; j < attributes._length; ++j) {
    char* cArgument = *((char**) (TRI_AtVector(&attributes, j)));
    TRI_Free(cArgument);
  }    
  TRI_DestroyVector(&attributes);

  if (iid == 0) {
    return scope.Close(v8::String::New("skiplist index could not be created"));
  }  
  
  // .............................................................................
  // Return the newly assigned index identifier
  // .............................................................................
  return scope.Close(v8::Number::New(iid));
}



////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that a multi skiplist index exists
///
/// @FUN{ensureMultiSLIndex(@FA{field1}, @FA{field2}, ...,@FA{fieldn})}
///
/// Creates a multi skiplist index on all documents using attributes as paths to
/// the fields. At least one attribute must be given.  
/// All documents, which do not have the attribute path or 
/// with ore or more values that are not suitable, are ignored.
///
/// In case that the index was successfully created, the index indetifier
/// is returned.
///
/// @verbinclude fluent14
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_EnsureMultiSkiplistIndexVocbaseCol (v8::Arguments const& argv) {

  v8::HandleScope scope;  
  v8::Handle<v8::String> err;
  
  
  // .............................................................................
  // Check that we have a valid collection                      
  // .............................................................................
  TRI_vocbase_col_t const* col = LoadCollection(argv.Holder(), &err);
  
  if (col == 0) {
    return scope.Close(v8::ThrowException(err));
  }

  
  // .............................................................................
  // Check collection type
  // .............................................................................
  TRI_doc_collection_t* collection = col->_collection;
  
  if (collection->base._type != TRI_COL_TYPE_SIMPLE_DOCUMENT) {
    return scope.Close(v8::ThrowException(v8::String::New("unknown collection type")));
  }
  TRI_sim_collection_t* sim = (TRI_sim_collection_t*) collection;

  
  // .............................................................................
  // Return string when there is an error of some sort.
  // .............................................................................
  string errorString;
  
  
  // .............................................................................
  // Ensure that there is at least one string parameter sent to this method
  // .............................................................................  
  if (argv.Length() == 0) {
    errorString = "one or more string parameters required for the ensureMutliSLIndex(...) command";
    return scope.Close(v8::String::New(errorString.c_str(),errorString.length()));
  }
  
    
  // .............................................................................
  // The index id which will either be an existing one or a newly created 
  // hash index. 
  // .............................................................................  
  TRI_idx_iid_t iid = 0;
  
  
  // .............................................................................
  // Create a list of paths, these will be used to create a list of shapes
  // which will be used by the hash index.
  // .............................................................................
  
  TRI_vector_t attributes;
  TRI_InitVector(&attributes,sizeof(char*));
  
  bool ok = true;
  
  for (int j = 0; j < argv.Length(); ++j) {
  
    v8::Handle<v8::Value> argument = argv[j];
    if (! argument->IsString() ) {
      errorString = "invalid parameter passed to ensureMultiSLIndex(...) command";
      ok = false;
      break;
    }
    
    // ...........................................................................
    // convert the argument into a "C" string
    // ...........................................................................
    
    v8::String::Utf8Value argumentString(argument);   
    char* cArgument = (char*) (TRI_Allocate(argumentString.length() + 1));       
    if (cArgument == NULL) {
      errorString = "insuffient memory to complete ensureMultiSLIndex(...) command";
      ok = false;
      break;
    }  
        
    memcpy(cArgument, *argumentString, argumentString.length());
    TRI_PushBackVector(&attributes,&cArgument);
  }

  
  // .............................................................................
  // Check that each parameter is unique
  // .............................................................................
  
  for (size_t j = 0; j < attributes._length; ++j) {  
    char* left = *((char**) (TRI_AtVector(&attributes, j)));    
    for (size_t k = j + 1; k < attributes._length; ++k) {
      char* right = *((char**) (TRI_AtVector(&attributes, k)));
      if (strcmp(left,right) == 0) {
        errorString = "duplicate parameters sent to ensureMultiSLIndex(...) command";
        ok = false;
        break;
      }   
    }
  }    
     
  
  // .............................................................................
  // Some sort of error occurred -- display error message and abort index creation
  // (or index retrieval).
  // .............................................................................
  
  if (!ok) {
    // ...........................................................................
    // Remove the memory allocated to the list of attributes used for the hash index
    // ...........................................................................   
    for (size_t j = 0; j < attributes._length; ++j) {
      char* cArgument = *((char**) (TRI_AtVector(&attributes, j)));
      TRI_Free(cArgument);
    }    
    TRI_DestroyVector(&attributes);
    return scope.Close(v8::String::New(errorString.c_str(),errorString.length()));
  }  
  
  
  // .............................................................................
  // Actually create the index here
  // .............................................................................
  iid = TRI_EnsureSkiplistIndexSimCollection(sim, &attributes, false);

  // .............................................................................
  // Remove the memory allocated to the list of attributes used for the hash index
  // .............................................................................   
  for (size_t j = 0; j < attributes._length; ++j) {
    char* cArgument = *((char**) (TRI_AtVector(&attributes, j)));
    TRI_Free(cArgument);
  }    
  TRI_DestroyVector(&attributes);

  if (iid == 0) {
    return scope.Close(v8::String::New("multi skiplist index could not be created"));
  }  
  
  // .............................................................................
  // Return the newly assigned index identifier
  // .............................................................................
  return scope.Close(v8::Number::New(iid));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the figures of a collection
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_FiguresVocbaseCol (v8::Arguments const& argv) {
  v8::HandleScope scope;

  TRI_vocbase_col_t const* collection = UnwrapClass<TRI_vocbase_col_t>(argv.Holder(), WRP_VOCBASE_COL_TYPE);

  if (collection == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("illegal collection pointer")));
  }

  v8::Handle<v8::Object> result = v8::Object::New();

  if (! collection->_loaded) {
    return scope.Close(result);
  }

  if (collection->_collection == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("illegal collection pointer")));
  }

  TRI_doc_collection_t* doc = collection->_collection;

  doc->beginRead(doc);
  TRI_doc_collection_info_t* info = doc->figures(doc);
  doc->endRead(doc);

  result->Set(v8::String::New("numberDatafiles"), v8::Number::New(info->_numberDatafiles));
  result->Set(v8::String::New("numberAlive"), v8::Number::New(info->_numberAlive));
  result->Set(v8::String::New("numberDead"), v8::Number::New(info->_numberDead));
  result->Set(v8::String::New("sizeAlive"), v8::Number::New(info->_sizeAlive));
  result->Set(v8::String::New("sizeDead"), v8::Number::New(info->_sizeDead));
  result->Set(v8::String::New("numberDeletion"), v8::Number::New(info->_numberDeletion));

  TRI_Free(info);

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns information about the indexes
///
/// @FUN{getIndexes()}
///
/// Returns a list of all indexes defined for the collection.
///
/// @verbinclude fluent18
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_GetIndexesVocbaseCol (v8::Arguments const& argv) {
  v8::HandleScope scope;

  v8::Handle<v8::String> err;
  TRI_vocbase_col_t const* col = LoadCollection(argv.Holder(), &err);


  if (col == 0) {
    return scope.Close(v8::ThrowException(err));
  }

  TRI_doc_collection_t* collection = col->_collection;

  if (collection->base._type != TRI_COL_TYPE_SIMPLE_DOCUMENT) {
    return scope.Close(v8::ThrowException(v8::String::New("unknown collection type")));
  }

  TRI_sim_collection_t* sim = (TRI_sim_collection_t*) collection;

  // get a list of indexes
  TRI_vector_pointer_t* indexes = TRI_IndexesSimCollection(sim);

  v8::Handle<v8::Array> result = v8::Array::New();

  size_t n = indexes->_length;

  for (size_t i = 0;  i < n;  ++i) {
    TRI_json_t* idx = (TRI_json_t*) indexes->_buffer[i];

    result->Set(i, TRI_ObjectJson(idx));
  }

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief loads a collection
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_LoadVocbaseCol (v8::Arguments const& argv) {
  v8::HandleScope scope;

  v8::Handle<v8::String> err;
  TRI_vocbase_col_t const* col = LoadCollection(argv.Holder(), &err);

  if (col == 0) {
    return scope.Close(v8::ThrowException(err));
  }

  return scope.Close(v8::Undefined());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief gets or sets the parameters of a collection
///
/// @FUN{parameter()}
///
/// Returns an object containing all collection parameters.
///
/// - @LIT{waitForSync}: If @LIT{true} creating a document will only return
///   after the data was synced to disk.
/// - @LIT{journalSize} : The size of the journal in bytes.
///
/// @FUN{parameter(@FA{parameter-array})}
///
/// Changes the collection parameters.
///
/// @EXAMPLES
///
/// Read all parameters
///
/// @verbinclude admin1
///
/// Change a parameter
///
/// @verbinclude admin2
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_ParameterVocbaseCol (v8::Arguments const& argv) {
  TRI_v8_global_t* v8g;
  v8::HandleScope scope;

  v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();

  v8::Handle<v8::String> err;
  TRI_vocbase_col_t const* col = LoadCollection(argv.Holder(), &err);

  if (col == 0) {
    return scope.Close(v8::ThrowException(err));
  }

  TRI_doc_collection_t* collection = col->_collection;

  if (collection->base._type != TRI_COL_TYPE_SIMPLE_DOCUMENT) {
    return scope.Close(v8::ThrowException(v8::String::New("unknown collection type")));
  }

  TRI_sim_collection_t* sim = (TRI_sim_collection_t*) collection;

  // check if we want to change same parameters
  if (0 < argv.Length()) {
    v8::Handle<v8::Value> par = argv[0];

    if (par->IsObject()) {
      v8::Handle<v8::Object> po = par->ToObject();

      TRI_LockCondition(&sim->_journalsCondition);
      bool waitForSync = sim->base.base._waitForSync;
      TRI_UnlockCondition(&sim->_journalsCondition);

      // extract sync after objects
      if (po->Has(v8g->WaitForSyncKey)) {
        waitForSync = TRI_ObjectToBoolean(po->Get(v8g->WaitForSyncKey));
      }

      // try to write new parameter to file
      TRI_LockCondition(&sim->_journalsCondition);

      sim->base.base._waitForSync = waitForSync;
      bool ok = TRI_UpdateParameterInfoCollection(&sim->base.base);

      TRI_UnlockCondition(&sim->_journalsCondition);

      if (! ok) {
        return scope.Close(v8::ThrowException(v8::String::New(TRI_last_error())));
      }
    }
  }

  // return the current parameter set
  v8::Handle<v8::Object> result = v8::Object::New();

  if (collection->base._type == TRI_COL_TYPE_SIMPLE_DOCUMENT) {
    TRI_LockCondition(&sim->_journalsCondition);

    TRI_voc_size_t maximalSize = sim->base.base._maximalSize;
    bool waitForSync = sim->base.base._waitForSync;

    TRI_UnlockCondition(&sim->_journalsCondition);

    result->Set(v8g->WaitForSyncKey, waitForSync ? v8::True() : v8::False());
    result->Set(v8g->JournalSizeKey, v8::Number::New(maximalSize));
  }

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief replaces a document
///
/// @FUN{replace(@FA{document-identifier}, @FA{document})}
///
/// Replaces an existing document. The @FA{document-identifier} must point to a
/// document in the current collection. This document is than replaced with the
/// value given as second argument and the @FA{document-identifier} is returned.
///
/// @verbinclude fluent21
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_ReplaceVocbaseCol (v8::Arguments const& argv) {
  v8::HandleScope scope;

  v8::Handle<v8::String> err;
  TRI_vocbase_col_t const* col = LoadCollection(argv.Holder(), &err);

  if (col == 0) {
    return scope.Close(v8::ThrowException(err));
  }

  TRI_doc_collection_t* collection = col->_collection;

  if (argv.Length() != 2) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: replace(<document-identifier>, <document>)")));
  }

  TRI_voc_cid_t cid = collection->base._cid;
  TRI_voc_did_t did;

  if (IsDocumentId(argv[0], cid, did)) {
    if (cid != collection->base._cid) {
      return scope.Close(v8::ThrowException(v8::String::New("cannot execute cross collection update")));
    }
  }
  else {
    return scope.Close(v8::ThrowException(v8::String::New("expecting a document identifier")));
  }

  TRI_shaped_json_t* shaped = TRI_ShapedJsonV8Object(argv[1], collection->_shaper);

  if (shaped == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("<document> cannot be converted into JSON shape")));
  }

  // .............................................................................
  // inside a write transaction
  // .............................................................................

  bool ok = collection->updateLock(collection, shaped, did, 0, TRI_DOC_UPDATE_LAST_WRITE);

  // .............................................................................
  // outside a write transaction
  // .............................................................................

  TRI_FreeShapedJson(shaped);

  if (! ok) {
    string err = "cannot replace document: ";
    err += TRI_last_error();

    return scope.Close(v8::ThrowException(v8::String::New(err.c_str())));
  }

  char* cidStr = TRI_StringUInt64(collection->base._cid);
  char* didStr = TRI_StringUInt64(did);

  string name = cidStr + string(":") + didStr;

  TRI_FreeString(didStr);
  TRI_FreeString(cidStr);

  return scope.Close(v8::String::New(name.c_str()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the status of a collection
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_StatusVocbaseCol (v8::Arguments const& argv) {
  v8::HandleScope scope;

  TRI_vocbase_col_t const* collection = UnwrapClass<TRI_vocbase_col_t>(argv.Holder(), WRP_VOCBASE_COL_TYPE);

  if (collection == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("illegal collection pointer")));
  }

  if (collection->_corrupted) {
    return scope.Close(v8::Number::New(4));
  }
  else if (collection->_newBorn) {
    return scope.Close(v8::Number::New(1));
  }
  else if (collection->_loaded) {
    return scope.Close(v8::Number::New(3));
  }
  else {
    return scope.Close(v8::Number::New(2));
  }

  return scope.Close(v8::Number::New(5));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief saves a new document
///
/// @FUN{save(@FA{document})}
///
/// Saves a new document and returns the document-identifier.
///
/// @verbinclude fluent22
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_SaveVocbaseCol (v8::Arguments const& argv) {
  v8::HandleScope scope;

  v8::Handle<v8::String> err;
  TRI_vocbase_col_t const* col = LoadCollection(argv.Holder(), &err);

  if (col == 0) {
    return scope.Close(v8::ThrowException(err));
  }

  TRI_doc_collection_t* collection = col->_collection;

  if (argv.Length() != 1) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: save(<document>)")));
  }

  TRI_shaped_json_t* shaped = TRI_ShapedJsonV8Object(argv[0], collection->_shaper);

  if (shaped == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("<document> cannot be converted into JSON shape")));
  }

  // .............................................................................
  // inside a write transaction
  // .............................................................................

  TRI_voc_did_t did = collection->createLock(collection, TRI_DOC_MARKER_DOCUMENT, shaped, 0);

  // .............................................................................
  // outside a write transaction
  // .............................................................................

  TRI_FreeShapedJson(shaped);

  if (did == 0) {
    string err = "cannot save document: ";
    err += TRI_last_error();

    return scope.Close(v8::ThrowException(v8::String::New(err.c_str())));
  }

  char* cidStr = TRI_StringUInt64(collection->base._cid);
  char* didStr = TRI_StringUInt64(did);

  string name = cidStr + string(":") + didStr;

  TRI_FreeString(didStr);
  TRI_FreeString(cidStr);

  return scope.Close(v8::String::New(name.c_str()));
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                 TRI_VOCBASE_COL_T EDGES FUNCTIONS
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                              javascript functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief replaces a document
///
/// @FUN{replace(@FA{document-identifier}, @FA{document})}
///
/// Replaces an existing document. The @FA{document-identifier} must point to a
/// document in the current collection. This document is than replaced with the
/// value given as second argument and the @FA{document-identifier} is returned.
///
/// @verbinclude fluent21
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_ReplaceEdgesCol (v8::Arguments const& argv) {
  v8::HandleScope scope;

  v8::Handle<v8::String> err;
  TRI_vocbase_col_t const* col = LoadCollection(argv.Holder(), &err);

  if (col == 0) {
    return scope.Close(v8::ThrowException(err));
  }

  TRI_doc_collection_t* collection = col->_collection;

  if (argv.Length() != 2) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: replace(<document-identifier>, <document>)")));
  }

  TRI_voc_cid_t cid = collection->base._cid;
  TRI_voc_did_t did;

  if (IsDocumentId(argv[0], cid, did)) {
    if (cid != collection->base._cid) {
      return scope.Close(v8::ThrowException(v8::String::New("cannot execute cross collection update")));
    }
  }
  else {
    return scope.Close(v8::ThrowException(v8::String::New("expecting a document identifier")));
  }

  TRI_shaped_json_t* shaped = TRI_ShapedJsonV8Object(argv[1], collection->_shaper);

  if (shaped == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("<document> cannot be converted into JSON shape")));
  }

  // .............................................................................
  // inside a write transaction
  // .............................................................................

  bool ok = collection->updateLock(collection, shaped, did, 0, TRI_DOC_UPDATE_LAST_WRITE);

  // .............................................................................
  // outside a write transaction
  // .............................................................................

  TRI_FreeShapedJson(shaped);

  if (! ok) {
    string err = "cannot replace document: ";
    err += TRI_last_error();

    return scope.Close(v8::ThrowException(v8::String::New(err.c_str())));
  }

  return scope.Close(v8::Number::New(did));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief saves a new document
///
/// @FUN{save(@FA{from}, @FA{to}, @FA{document})}
///
/// Saves a new edge and returns the document-identifier. @FA{from} and @FA{to}
/// must be documents or document references.
///
/// @verbinclude fluent22
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_SaveEdgesCol (v8::Arguments const& argv) {
  v8::HandleScope scope;

  v8::Handle<v8::String> err;
  TRI_vocbase_col_t const* col = LoadCollection(argv.Holder(), &err);

  if (col == 0) {
    return scope.Close(v8::ThrowException(err));
  }

  TRI_doc_collection_t* collection = col->_collection;

  if (argv.Length() != 3) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: save(<from>, <to>, <document>)")));
  }

  TRI_sim_edge_t edge;

  edge._fromCid = col->_cid;
  edge._toCid = col->_cid;

  if (! IsDocumentId(argv[0], edge._fromCid, edge._fromDid)) {
    return scope.Close(v8::ThrowException(v8::String::New("<from> is not a document identifier")));
  }

  if (! IsDocumentId(argv[1], edge._toCid, edge._toDid)) {
    return scope.Close(v8::ThrowException(v8::String::New("<from> is not a document identifier")));
  }

  TRI_shaped_json_t* shaped = TRI_ShapedJsonV8Object(argv[2], collection->_shaper);

  if (shaped == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("<document> cannot be converted into JSON shape")));
  }

  // .............................................................................
  // inside a write transaction
  // .............................................................................

  TRI_voc_did_t did = collection->createLock(collection, TRI_DOC_MARKER_EDGE, shaped, &edge);

  // .............................................................................
  // outside a write transaction
  // .............................................................................

  TRI_FreeShapedJson(shaped);

  if (did == 0) {
    string err = "cannot save document: ";
    err += TRI_last_error();

    return scope.Close(v8::ThrowException(v8::String::New(err.c_str())));
  }

  char* cidStr = TRI_StringUInt64(collection->base._cid);
  char* didStr = TRI_StringUInt64(did);

  string name = cidStr + string(":") + didStr;

  TRI_FreeString(didStr);
  TRI_FreeString(cidStr);

  return scope.Close(v8::String::New(name.c_str()));
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                           TRI_VOCBASE_T FUNCTIONS
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief selects a collection from the vocbase
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> MapGetVocBase (v8::Local<v8::String> name,
                                            const v8::AccessorInfo& info) {
  v8::HandleScope scope;

  TRI_vocbase_t* vocbase = UnwrapClass<TRI_vocbase_t>(info.Holder(), WRP_VOCBASE_TYPE);

  if (vocbase == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("corrupted vocbase")));
  }

  // convert the JavaScript string to a string
  string key = TRI_ObjectToString(name);

  if (key == "") {
    return scope.Close(v8::ThrowException(v8::String::New("name must not be empty")));
  }

  if (   key == "toString"
      || key == "toJSON"
      || key == "hasOwnProperty"
      || key[0] == '_') {
    return v8::Handle<v8::Value>();
  }

  // look up the value if it exists
  TRI_vocbase_col_t const* collection = TRI_FindCollectionByNameVocBase(vocbase, key.c_str(), true);

  // if the key is not present return an empty handle as signal
  if (collection == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("cannot load or create collection")));
  }

  if (collection->_type != TRI_COL_TYPE_SIMPLE_DOCUMENT) {
    return scope.Close(v8::ThrowException(v8::String::New("collection is not an document collection")));
  }

  return scope.Close(TRI_WrapCollection(collection));
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                              javascript functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief returns all collections
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_CollectionsVocBase (v8::Arguments const& argv) {
  v8::HandleScope scope;

  TRI_vocbase_t* vocbase = UnwrapClass<TRI_vocbase_t>(argv.Holder(), WRP_VOCBASE_TYPE);
  
  if (vocbase == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("corrupted vocbase")));
  }

  v8::Handle<v8::Array> result = v8::Array::New();
  TRI_vector_pointer_t colls = TRI_CollectionsVocBase(vocbase);

  for (size_t i = 0;  i < colls._length;  ++i) {
    TRI_vocbase_col_t const* collection = (TRI_vocbase_col_t const*) colls._buffer[i];

    result->Set(i, TRI_WrapCollection(collection));
  }

  TRI_DestroyVectorPointer(&colls);

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns all collections
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_CompletionsVocBase (v8::Arguments const& argv) {
  v8::HandleScope scope;
  v8::Handle<v8::Array> result = v8::Array::New();

  TRI_vocbase_t* vocbase = UnwrapClass<TRI_vocbase_t>(argv.Holder(), WRP_VOCBASE_TYPE);

  if (vocbase == 0) {
    return scope.Close(result);
  }

  TRI_vector_pointer_t colls = TRI_CollectionsVocBase(vocbase);

  for (size_t i = 0;  i < colls._length;  ++i) {
    TRI_vocbase_col_t const* collection = (TRI_vocbase_col_t const*) colls._buffer[i];

    result->Set(i, v8::String::New(collection->_name));
  }

  TRI_DestroyVectorPointer(&colls);

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                     TRI_VOCBASE_T EDGES FUNCTIONS
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief selects a collection from the vocbase
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> MapGetEdges (v8::Local<v8::String> name,
                                            const v8::AccessorInfo& info) {
  v8::HandleScope scope;

  TRI_vocbase_t* vocbase = UnwrapClass<TRI_vocbase_t>(info.Holder(), WRP_VOCBASE_TYPE);

  if (vocbase == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("corrupted vocbase")));
  }

  // convert the JavaScript string to a string
  string key = TRI_ObjectToString(name);

  if (key == "") {
    return scope.Close(v8::ThrowException(v8::String::New("name must not be empty")));
  }

  if (   key == "toString"
      || key == "toJSON"
      || key == "hasOwnProperty"
      || key[0] == '_') {
    return v8::Handle<v8::Value>();
  }

  // look up the value if it exists
  TRI_vocbase_col_t const* collection = TRI_FindCollectionByNameVocBase(vocbase, key.c_str(), true);

  // if the key is not present return an empty handle as signal
  if (collection == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("cannot load or create edge collection")));
  }

  return scope.Close(TRI_WrapEdgesCollection(collection));
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                              javascript functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief returns all collections
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_CollectionsEdges (v8::Arguments const& argv) {
  v8::HandleScope scope;

  TRI_vocbase_t* vocbase = UnwrapClass<TRI_vocbase_t>(argv.Holder(), WRP_VOCBASE_TYPE);

  if (vocbase == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("corrupted vocbase")));
  }

  v8::Handle<v8::Array> result = v8::Array::New();
  TRI_vector_pointer_t colls = TRI_CollectionsVocBase(vocbase);

  for (size_t i = 0;  i < colls._length;  ++i) {
    TRI_vocbase_col_t const* collection = (TRI_vocbase_col_t const*) colls._buffer[i];

    result->Set(i, TRI_WrapEdgesCollection(collection));
  }

  TRI_DestroyVectorPointer(&colls);

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                             SHAPED JSON FUNCTIONS
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief weak reference callback for a bridge
////////////////////////////////////////////////////////////////////////////////

static void WeakBridgeCallback (v8::Persistent<v8::Value> object, void* parameter) {
  TRI_barrier_t* barrier;
  TRI_v8_global_t* v8g;

  v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();
  barrier = (TRI_barrier_t*) parameter;

  LOG_TRACE("weak-callback for barrier called");

  // find the persistent handle
  v8::Persistent<v8::Value> persistent = v8g->JSBarriers[barrier];
  v8g->JSBarriers.erase(barrier);

  // dispose and clear the persistent handle
  persistent.Dispose();
  persistent.Clear();

  // free the barrier
  TRI_FreeBarrier(barrier);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief selects an attribute from the shaped json
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> MapGetShapedJson (v8::Local<v8::String> name,
                                               const v8::AccessorInfo& info) {
  v8::HandleScope scope;

  // sanity check
  v8::Handle<v8::Object> self = info.Holder();

  if (self->InternalFieldCount() <= SLOT_BARRIER) {
    return scope.Close(v8::ThrowException(v8::String::New("corrupted shaped json")));
  }

  // get shaped json
  TRI_shaped_json_t* document = UnwrapClass<TRI_shaped_json_t>(self, WRP_SHAPED_JSON_TYPE);

  if (document == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("corrupted shaped json")));
  }

  TRI_barrier_t* barrier = static_cast<TRI_barrier_t*>(v8::Handle<v8::External>::Cast(self->GetInternalField(SLOT_BARRIER))->Value());
  TRI_doc_collection_t* collection = barrier->_container->_collection;

  // convert the JavaScript string to a string
  string key = TRI_ObjectToString(name);
  
  if (key == "") {
    return scope.Close(v8::ThrowException(v8::String::New("name must not be empty")));
  }  
  
  if (key[0] == '_') {
    return scope.Close(v8::Handle<v8::Value>());
  }

  // get shape accessor
  TRI_shaper_t* shaper = collection->_shaper;
  TRI_shape_pid_t pid = shaper->findAttributePathByName(shaper, key.c_str()); 
  TRI_shape_sid_t sid = document->_sid;    
  TRI_shape_access_t* acc = TRI_ShapeAccessor(shaper, sid, pid);

  if (acc == NULL || acc->_shape == NULL) {
    if (acc) {
      TRI_FreeShapeAccessor(acc);
    }
    return scope.Close(v8::Handle<v8::Value>());
  }

  // convert to v8 value
  TRI_shape_t const* shape = acc->_shape;
  TRI_shaped_json_t json;

  if (TRI_ExecuteShapeAccessor(acc, document, &json)) {    
    TRI_FreeShapeAccessor(acc);
    return scope.Close(TRI_JsonShapeData(shaper, shape, json._data.data, json._data.length));
  }
  TRI_FreeShapeAccessor(acc);

  return scope.Close(v8::ThrowException(v8::String::New("cannot extract attribute")));  
}

////////////////////////////////////////////////////////////////////////////////
/// @brief selects the keys from the shaped json
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Array> KeysOfShapedJson (const v8::AccessorInfo& info) {
  v8::HandleScope scope;
  TRI_v8_global_t* v8g;

  v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();

  v8::Handle<v8::Array> result = v8::Array::New();
  
  // sanity check
  v8::Handle<v8::Object> self = info.Holder();

  if (self->InternalFieldCount() <= SLOT_BARRIER) {
    return scope.Close(result);
  }

  // get shaped json
  TRI_shaped_json_t* document = UnwrapClass<TRI_shaped_json_t>(info.Holder(), WRP_SHAPED_JSON_TYPE);

  if (document == 0) {
    return scope.Close(result);
  }

  TRI_barrier_t* barrier = static_cast<TRI_barrier_t*>(v8::Handle<v8::External>::Cast(self->GetInternalField(SLOT_BARRIER))->Value());
  TRI_doc_collection_t* collection = barrier->_container->_collection;

  // check for array shape
  TRI_shaper_t* shaper = collection->_shaper;  
  TRI_shape_t const* shape = shaper->lookupShapeId(shaper, document->_sid);

  if (shape == 0 || shape->_type != TRI_SHAPE_ARRAY) {
    return scope.Close(result);
  }
  
  TRI_array_shape_t const* s;
  TRI_shape_aid_t const* aids;
  TRI_shape_size_t i;
  TRI_shape_size_t n;
  char const* qtr = 0;
  uint32_t count = 0;

  // shape is an array
  s = (TRI_array_shape_t const*) shape;
  
  // number of entries
  n = s->_fixedEntries + s->_variableEntries;

  // calculation position of attribute ids
  qtr = (char const*) shape;
  qtr += sizeof(TRI_array_shape_t);
  qtr += n * sizeof(TRI_shape_sid_t);    
  aids = (TRI_shape_aid_t const*) qtr;
  
  for (i = 0;  i < n;  ++i, ++aids) {      
    char const* att = shaper->lookupAttributeId(shaper, *aids);

    if (att) {
      result->Set(count++, v8::String::New(att));        
    }
  }

  result->Set(count++, v8g->DidKey);
  result->Set(count++, v8g->RevKey);
 
  return scope.Close(result);  
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check if a property is present
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Integer> PropertyQueryShapedJson (v8::Local<v8::String> name, const v8::AccessorInfo& info) {
  v8::HandleScope scope;

  // sanity check
  v8::Handle<v8::Object> self = info.Holder();

  if (self->InternalFieldCount() <= SLOT_BARRIER) {
    return scope.Close(v8::Handle<v8::Integer>());
  }

  // get shaped json
  TRI_shaped_json_t* document = UnwrapClass<TRI_shaped_json_t>(self, WRP_SHAPED_JSON_TYPE);

  if (document == 0) {
    return scope.Close(v8::Handle<v8::Integer>());
  }

  TRI_barrier_t* barrier = static_cast<TRI_barrier_t*>(v8::Handle<v8::External>::Cast(self->GetInternalField(SLOT_BARRIER))->Value());
  TRI_doc_collection_t* collection = barrier->_container->_collection;

  // convert the JavaScript string to a string
  string key = TRI_ObjectToString(name);
  
  if (key == "") {
    return scope.Close(v8::Handle<v8::Integer>());
  }  
  
  if (key == "_id") {
    return scope.Close(v8::Handle<v8::Integer>(v8::Integer::New(v8::ReadOnly)));
  }  
  
  if (key == "_rev") {
    return scope.Close(v8::Handle<v8::Integer>(v8::Integer::New(v8::ReadOnly)));
  }  
  
  // get shape accessor
  TRI_shaper_t* shaper = collection->_shaper;
  TRI_shape_pid_t pid = shaper->findAttributePathByName(shaper, key.c_str()); 
  TRI_shape_sid_t sid = document->_sid;    
  TRI_shape_access_t* acc = TRI_ShapeAccessor(shaper, sid, pid);

  // key not found
  if (acc == NULL || acc->_shape == NULL) {
    if (acc) {
      TRI_FreeShapeAccessor(acc);
    }
    return scope.Close(v8::Handle<v8::Integer>());
  }
  TRI_FreeShapeAccessor(acc);

  return scope.Close(v8::Handle<v8::Integer>(v8::Integer::New(v8::ReadOnly)));
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                            MODULE
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief wraps a TRI_vocbase_t
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Object> TRI_WrapVocBase (TRI_vocbase_t const* database) {
  TRI_v8_global_t* v8g;
  v8::HandleScope scope;

  v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();
  v8::Handle<v8::Object> result = WrapClass(v8g->VocbaseTempl, 
                                            WRP_VOCBASE_TYPE,
                                            const_cast<TRI_vocbase_t*>(database));

  result->Set(v8::String::New("_path"),
              v8::String::New(database->_path),
              v8::ReadOnly);

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief wraps a TRI_vocbase_t for edges
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Object> TRI_WrapEdges (TRI_vocbase_t const* database) {
  TRI_v8_global_t* v8g;
  v8::HandleScope scope;

  v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();
  v8::Handle<v8::Object> result = WrapClass(v8g->EdgesTempl,
                                            WRP_VOCBASE_TYPE,
                                            const_cast<TRI_vocbase_t*>(database));

  result->Set(v8::String::New("_path"),
              v8::String::New(database->_path),
              v8::ReadOnly);

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief wraps a TRI_vocbase_col_t
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Object> TRI_WrapCollection (TRI_vocbase_col_t const* collection) {
  TRI_v8_global_t* v8g;
  v8::HandleScope scope;

  v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();
  v8::Handle<v8::Object> result = WrapClass(v8g->VocbaseColTempl,
                                            WRP_VOCBASE_COL_TYPE,
                                            const_cast<TRI_vocbase_col_t*>(collection));

  result->SetInternalField(SLOT_QUERY, v8g->CollectionQueryType);
  result->SetInternalField(SLOT_RESULT_SET, v8::Null());

  result->Set(v8::String::New("_name"),
              v8::String::New(collection->_name),
              v8::ReadOnly);

  result->Set(v8::String::New("_id"),
              v8::Number::New(collection->_cid),
              v8::ReadOnly);

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief wraps a TRI_vocbase_col_t for edges
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Object> TRI_WrapEdgesCollection (TRI_vocbase_col_t const* collection) {
  TRI_v8_global_t* v8g;
  v8::HandleScope scope;

  v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();
  v8::Handle<v8::Object> result = WrapClass(v8g->EdgesColTempl, 
                                            WRP_VOCBASE_COL_TYPE,
                                            const_cast<TRI_vocbase_col_t*>(collection));

  result->SetInternalField(SLOT_QUERY, v8g->CollectionQueryType);
  result->SetInternalField(SLOT_RESULT_SET, v8::Null());

  result->Set(v8::String::New("_name"),
              v8::String::New(collection->_name),
              v8::ReadOnly);

  result->Set(v8::String::New("_id"),
              v8::Number::New(collection->_cid),
              v8::ReadOnly);

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief wraps a TRI_shaped_json_t
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Value> TRI_WrapShapedJson (TRI_vocbase_col_t const* collection,
                                          TRI_doc_mptr_t const* document,
                                          TRI_barrier_t* barrier) {
  TRI_v8_global_t* v8g;
  v8::HandleScope scope;

  v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();

  // create the new handle to return, and set its template type
  v8::Handle<v8::Object> result = v8g->ShapedJsonTempl->NewInstance();

  // point the 0 index Field to the c++ pointer for unwrapping later
  result->SetInternalField(SLOT_CLASS_TYPE, v8::Integer::New(WRP_SHAPED_JSON_TYPE));
  result->SetInternalField(SLOT_CLASS, v8::External::New(const_cast<TRI_shaped_json_t*>(&document->_document)));

  map< void*, v8::Persistent<v8::Value> >::iterator i = v8g->JSBarriers.find(barrier);

  if (i == v8g->JSBarriers.end()) {
    v8::Persistent<v8::Value> persistent = v8::Persistent<v8::Value>::New(v8::External::New(barrier));
    result->SetInternalField(SLOT_BARRIER, persistent);

    v8g->JSBarriers[barrier] = persistent;

    persistent.MakeWeak(barrier, WeakBridgeCallback);
  }
  else {
    result->SetInternalField(SLOT_BARRIER, i->second);
  }

  // store the document reference
  TRI_voc_did_t did = document->_did;
  TRI_voc_rid_t rid = document->_rid;

  result->Set(v8g->DidKey, TRI_ObjectReference(collection->_collection->base._cid, did), v8::ReadOnly);
  result->Set(v8g->RevKey, v8::Number::New(rid), v8::ReadOnly);
  
  TRI_df_marker_type_t type = ((TRI_df_marker_t*) document->_data)->_type;

  if (type == TRI_DOC_MARKER_EDGE) {
    TRI_doc_edge_marker_t* marker = (TRI_doc_edge_marker_t*) document->_data;

    result->Set(v8g->FromKey, TRI_ObjectReference(marker->_fromCid, marker->_fromDid));
    result->Set(v8g->ToKey, TRI_ObjectReference(marker->_toCid, marker->_toDid));
  }

  // and return
  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a TRI_vocbase_t global context
////////////////////////////////////////////////////////////////////////////////

void TRI_InitV8VocBridge (v8::Handle<v8::Context> context, TRI_vocbase_t* vocbase) {
  v8::HandleScope scope;

  v8::Handle<v8::ObjectTemplate> rt;
  v8::Handle<v8::FunctionTemplate> ft;

  // check the isolate
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  TRI_v8_global_t* v8g = (TRI_v8_global_t*) isolate->GetData();

  if (v8g == 0) {
    v8g = new TRI_v8_global_t;
    isolate->SetData(v8g);
  }

  // create the regular expressions
  if (regcomp(&v8g->DocumentIdRegex, "([0-9][0-9]*):([0-9][0-9]*)", REG_ICASE | REG_EXTENDED) != 0) {
    LOG_FATAL("cannot compile regular expression");
    exit(EXIT_FAILURE);
  }

  // .............................................................................
  // global function names
  // .............................................................................

  if (v8g->OutputFuncName.IsEmpty()) {
    v8g->OutputFuncName = v8::Persistent<v8::String>::New(v8::String::New("output"));
  }

  // .............................................................................
  // local function names
  // .............................................................................

  v8::Handle<v8::String> AllFuncName = v8::Persistent<v8::String>::New(v8::String::New("ALL"));
  v8::Handle<v8::String> CompletionsFuncName = v8::Persistent<v8::String>::New(v8::String::New("_COMPLETIONS"));
  v8::Handle<v8::String> NearFuncName = v8::Persistent<v8::String>::New(v8::String::New("NEAR"));
  v8::Handle<v8::String> WithinFuncName = v8::Persistent<v8::String>::New(v8::String::New("WITHIN"));

  v8::Handle<v8::String> CollectionsFuncName = v8::Persistent<v8::String>::New(v8::String::New("_collections"));
  v8::Handle<v8::String> CountFuncName = v8::Persistent<v8::String>::New(v8::String::New("count"));
  v8::Handle<v8::String> DeleteFuncName = v8::Persistent<v8::String>::New(v8::String::New("delete"));
  v8::Handle<v8::String> DisposeFuncName = v8::Persistent<v8::String>::New(v8::String::New("dispose"));
  v8::Handle<v8::String> DocumentFuncName = v8::Persistent<v8::String>::New(v8::String::New("document"));
  v8::Handle<v8::String> DropIndexFuncName = v8::Persistent<v8::String>::New(v8::String::New("dropIndex"));
  v8::Handle<v8::String> EdgesFuncName = v8::Persistent<v8::String>::New(v8::String::New("edges"));
  v8::Handle<v8::String> EnsureGeoIndexFuncName = v8::Persistent<v8::String>::New(v8::String::New("ensureGeoIndex"));
  v8::Handle<v8::String> EnsureMultiHashIndexFuncName = v8::Persistent<v8::String>::New(v8::String::New("ensureMultiHashIndex"));
  v8::Handle<v8::String> EnsureMultiSkiplistIndexFuncName = v8::Persistent<v8::String>::New(v8::String::New("ensureMultiSLIndex"));
  v8::Handle<v8::String> EnsureSkiplistIndexFuncName = v8::Persistent<v8::String>::New(v8::String::New("ensureSLIndex"));
  v8::Handle<v8::String> EnsureUniqueConstraintFuncName = v8::Persistent<v8::String>::New(v8::String::New("ensureUniqueConstraint"));
  v8::Handle<v8::String> ExecuteFuncName = v8::Persistent<v8::String>::New(v8::String::New("execute"));
  v8::Handle<v8::String> FiguresFuncName = v8::Persistent<v8::String>::New(v8::String::New("figures"));
  v8::Handle<v8::String> GetIndexesFuncName = v8::Persistent<v8::String>::New(v8::String::New("getIndexes"));
  v8::Handle<v8::String> GetMaxFuncName = v8::Persistent<v8::String>::New(v8::String::New("getMax"));
  v8::Handle<v8::String> GetRowsFuncName = v8::Persistent<v8::String>::New(v8::String::New("getRows"));
  v8::Handle<v8::String> HasCountFuncName = v8::Persistent<v8::String>::New(v8::String::New("hasCount"));
  v8::Handle<v8::String> HasNextFuncName = v8::Persistent<v8::String>::New(v8::String::New("hasNext"));
  v8::Handle<v8::String> IdFuncName = v8::Persistent<v8::String>::New(v8::String::New("id"));
  v8::Handle<v8::String> InEdgesFuncName = v8::Persistent<v8::String>::New(v8::String::New("inEdges"));
  v8::Handle<v8::String> LoadFuncName = v8::Persistent<v8::String>::New(v8::String::New("load"));
  v8::Handle<v8::String> NextFuncName = v8::Persistent<v8::String>::New(v8::String::New("next"));
  v8::Handle<v8::String> NextRefFuncName = v8::Persistent<v8::String>::New(v8::String::New("nextRef"));
  v8::Handle<v8::String> OutEdgesFuncName = v8::Persistent<v8::String>::New(v8::String::New("outEdges"));
  v8::Handle<v8::String> ParameterFuncName = v8::Persistent<v8::String>::New(v8::String::New("parameter"));
  v8::Handle<v8::String> ReplaceFuncName = v8::Persistent<v8::String>::New(v8::String::New("replace"));
  v8::Handle<v8::String> SaveFuncName = v8::Persistent<v8::String>::New(v8::String::New("save"));
  v8::Handle<v8::String> StatusFuncName = v8::Persistent<v8::String>::New(v8::String::New("status"));
  v8::Handle<v8::String> UseNextFuncName = v8::Persistent<v8::String>::New(v8::String::New("useNext"));

  // .............................................................................
  // query types
  // .............................................................................

  v8g->CollectionQueryType = v8::Persistent<v8::String>::New(v8::String::New("collection"));

  // .............................................................................
  // keys
  // .............................................................................

  v8g->JournalSizeKey = v8::Persistent<v8::String>::New(v8::String::New("journalSize"));
  v8g->WaitForSyncKey = v8::Persistent<v8::String>::New(v8::String::New("waitForSync"));

  if (v8g->DidKey.IsEmpty()) {
    v8g->DidKey = v8::Persistent<v8::String>::New(v8::String::New("_id"));
  }

  if (v8g->FromKey.IsEmpty()) {
    v8g->FromKey = v8::Persistent<v8::String>::New(v8::String::New("_from"));
  }

  if (v8g->RevKey.IsEmpty()) {
    v8g->RevKey = v8::Persistent<v8::String>::New(v8::String::New("_rev"));
  }

  if (v8g->ToKey.IsEmpty()) {
    v8g->ToKey = v8::Persistent<v8::String>::New(v8::String::New("_to"));
  }

  // .............................................................................
  // generate the TRI_vocbase_t template
  // .............................................................................

  ft = v8::FunctionTemplate::New();
  ft->SetClassName(v8::String::New("AvocadoDatabase"));

  rt = ft->InstanceTemplate();
  rt->SetInternalFieldCount(2);

  rt->SetNamedPropertyHandler(MapGetVocBase);

  rt->Set(CollectionsFuncName, v8::FunctionTemplate::New(JS_CollectionsVocBase));
  rt->Set(CompletionsFuncName, v8::FunctionTemplate::New(JS_CompletionsVocBase));

  v8g->VocbaseTempl = v8::Persistent<v8::ObjectTemplate>::New(rt);

  // must come after SetInternalFieldCount
  context->Global()->Set(v8::String::New("AvocadoDatabase"),
                         ft->GetFunction());

  // .............................................................................
  // generate the TRI_vocbase_t template for edges
  // .............................................................................

  ft = v8::FunctionTemplate::New();
  ft->SetClassName(v8::String::New("AvocadoEdges"));

  rt = ft->InstanceTemplate();
  rt->SetInternalFieldCount(2);

  rt->SetNamedPropertyHandler(MapGetEdges);

  rt->Set(CollectionsFuncName, v8::FunctionTemplate::New(JS_CollectionsEdges));
  rt->Set(CompletionsFuncName, v8::FunctionTemplate::New(JS_CompletionsVocBase));

  v8g->EdgesTempl = v8::Persistent<v8::ObjectTemplate>::New(rt);

  // must come after SetInternalFieldCount
  context->Global()->Set(v8::String::New("AvocadoEdges"),
                         ft->GetFunction());

  
  // .............................................................................
  // generate the TRI_shaped_json_t template
  // .............................................................................

  ft = v8::FunctionTemplate::New();
  ft->SetClassName(v8::String::New("ShapedJson"));

  rt = ft->InstanceTemplate();
  rt->SetInternalFieldCount(3);
  
  rt->SetNamedPropertyHandler(MapGetShapedJson,         // NamedPropertyGetter,
                              0,                        // NamedPropertySetter setter = 0
                              PropertyQueryShapedJson,  // NamedPropertyQuery,
                              0,                        // NamedPropertyDeleter deleter = 0,
                              KeysOfShapedJson          // NamedPropertyEnumerator,
                                                        // Handle<Value> data = Handle<Value>());
                              );

  v8g->ShapedJsonTempl = v8::Persistent<v8::ObjectTemplate>::New(rt);

  // must come after SetInternalFieldCount
  context->Global()->Set(v8::String::New("ShapedJson"),
                         ft->GetFunction());
  
  // .............................................................................
  // generate the TRI_vocbase_col_t template
  // .............................................................................

  ft = v8::FunctionTemplate::New();
  ft->SetClassName(v8::String::New("AvocadoCollection"));

  rt = ft->InstanceTemplate();
  rt->SetInternalFieldCount(SLOT_END);

  v8g->VocbaseColTempl = v8::Persistent<v8::ObjectTemplate>::New(rt);

  rt->Set(AllFuncName, v8::FunctionTemplate::New(JS_AllQuery));
  rt->Set(CountFuncName, v8::FunctionTemplate::New(JS_CountVocbaseCol));
  rt->Set(DeleteFuncName, v8::FunctionTemplate::New(JS_DeleteVocbaseCol));
  rt->Set(DocumentFuncName, v8::FunctionTemplate::New(JS_DocumentQuery));
  rt->Set(DropIndexFuncName, v8::FunctionTemplate::New(JS_DropIndexVocbaseCol));
  rt->Set(EnsureGeoIndexFuncName, v8::FunctionTemplate::New(JS_EnsureGeoIndexVocbaseCol));
  rt->Set(EnsureMultiHashIndexFuncName, v8::FunctionTemplate::New(JS_EnsureMultiHashIndexVocbaseCol));
  rt->Set(EnsureMultiSkiplistIndexFuncName, v8::FunctionTemplate::New(JS_EnsureMultiSkiplistIndexVocbaseCol));
  rt->Set(EnsureSkiplistIndexFuncName, v8::FunctionTemplate::New(JS_EnsureSkiplistIndexVocbaseCol));
  rt->Set(EnsureUniqueConstraintFuncName, v8::FunctionTemplate::New(JS_EnsureUniqueConstraintVocbaseCol));
  rt->Set(FiguresFuncName, v8::FunctionTemplate::New(JS_FiguresVocbaseCol));
  rt->Set(GetIndexesFuncName, v8::FunctionTemplate::New(JS_GetIndexesVocbaseCol));
  rt->Set(LoadFuncName, v8::FunctionTemplate::New(JS_LoadVocbaseCol));
  rt->Set(NearFuncName, v8::FunctionTemplate::New(JS_NearQuery));
  rt->Set(ParameterFuncName, v8::FunctionTemplate::New(JS_ParameterVocbaseCol));
  rt->Set(StatusFuncName, v8::FunctionTemplate::New(JS_StatusVocbaseCol));
  rt->Set(WithinFuncName, v8::FunctionTemplate::New(JS_WithinQuery));
  
  rt->Set(SaveFuncName, v8::FunctionTemplate::New(JS_SaveVocbaseCol));
  rt->Set(ReplaceFuncName, v8::FunctionTemplate::New(JS_ReplaceVocbaseCol));

  // must come after SetInternalFieldCount
  context->Global()->Set(v8::String::New("AvocadoCollection"),
                         ft->GetFunction());

  // .............................................................................
  // generate the TRI_vocbase_col_t template for edges
  // .............................................................................

  ft = v8::FunctionTemplate::New();
  ft->SetClassName(v8::String::New("AvocadoEdgesCollection"));

  rt = ft->InstanceTemplate();
  rt->SetInternalFieldCount(SLOT_END);

  rt->Set(AllFuncName, v8::FunctionTemplate::New(JS_AllQuery));
  rt->Set(CountFuncName, v8::FunctionTemplate::New(JS_CountVocbaseCol));
  rt->Set(DeleteFuncName, v8::FunctionTemplate::New(JS_DeleteVocbaseCol));
  rt->Set(DocumentFuncName, v8::FunctionTemplate::New(JS_DocumentQuery));
  rt->Set(DropIndexFuncName, v8::FunctionTemplate::New(JS_DropIndexVocbaseCol));
  rt->Set(EnsureGeoIndexFuncName, v8::FunctionTemplate::New(JS_EnsureGeoIndexVocbaseCol));
  rt->Set(EnsureMultiHashIndexFuncName, v8::FunctionTemplate::New(JS_EnsureMultiHashIndexVocbaseCol));
  rt->Set(EnsureMultiSkiplistIndexFuncName, v8::FunctionTemplate::New(JS_EnsureMultiSkiplistIndexVocbaseCol));
  rt->Set(EnsureSkiplistIndexFuncName, v8::FunctionTemplate::New(JS_EnsureSkiplistIndexVocbaseCol));
  rt->Set(EnsureUniqueConstraintFuncName, v8::FunctionTemplate::New(JS_EnsureUniqueConstraintVocbaseCol));
  rt->Set(FiguresFuncName, v8::FunctionTemplate::New(JS_FiguresVocbaseCol));
  rt->Set(GetIndexesFuncName, v8::FunctionTemplate::New(JS_GetIndexesVocbaseCol));
  rt->Set(LoadFuncName, v8::FunctionTemplate::New(JS_LoadVocbaseCol));
  rt->Set(NearFuncName, v8::FunctionTemplate::New(JS_NearQuery));
  rt->Set(ParameterFuncName, v8::FunctionTemplate::New(JS_ParameterVocbaseCol));
  rt->Set(StatusFuncName, v8::FunctionTemplate::New(JS_StatusVocbaseCol));
  rt->Set(WithinFuncName, v8::FunctionTemplate::New(JS_WithinQuery));

  rt->Set(ReplaceFuncName, v8::FunctionTemplate::New(JS_ReplaceEdgesCol));
  rt->Set(SaveFuncName, v8::FunctionTemplate::New(JS_SaveEdgesCol));

  rt->Set(EdgesFuncName, v8::FunctionTemplate::New(JS_EdgesQuery));
  rt->Set(InEdgesFuncName, v8::FunctionTemplate::New(JS_InEdgesQuery));
  rt->Set(OutEdgesFuncName, v8::FunctionTemplate::New(JS_OutEdgesQuery));

  v8g->EdgesColTempl = v8::Persistent<v8::ObjectTemplate>::New(rt);

  // must come after SetInternalFieldCount
  context->Global()->Set(v8::String::New("AvocadoEdgesCollection"),
                         ft->GetFunction());

/* DEPRECATED */
  // .............................................................................
  // generate the where clause template
  // .............................................................................

  ft = v8::FunctionTemplate::New();
  ft->SetClassName(v8::String::New("AvocadoWhereClause"));

  rt = ft->InstanceTemplate();
  rt->SetInternalFieldCount(2);

  v8g->WhereTempl = v8::Persistent<v8::ObjectTemplate>::New(rt);

  // must come after SetInternalFieldCount
  context->Global()->Set(v8::String::New("AvocadoWhereClause"),
                         ft->GetFunction());

  // .............................................................................
  // generate the query template
  // .............................................................................

  ft = v8::FunctionTemplate::New();
  ft->SetClassName(v8::String::New("AvocadoQuery"));

  rt = ft->InstanceTemplate();
  rt->SetInternalFieldCount(2);

  rt->Set(ExecuteFuncName, v8::FunctionTemplate::New(JS_ExecuteAql));

  v8g->QueryTempl = v8::Persistent<v8::ObjectTemplate>::New(rt);

  // must come after SetInternalFieldCount
  context->Global()->Set(v8::String::New("AvocadoQuery"),
                         ft->GetFunction());
/* DEPRECATED END */
  
  // .............................................................................
  // generate the query instance template
  // .............................................................................

  ft = v8::FunctionTemplate::New();
  ft->SetClassName(v8::String::New("AvocadoQueryInstance"));

  rt = ft->InstanceTemplate();
  rt->SetInternalFieldCount(2);

  rt->Set(ExecuteFuncName, v8::FunctionTemplate::New(JS_ExecuteQueryInstance));

  v8g->QueryInstanceTempl = v8::Persistent<v8::ObjectTemplate>::New(rt);

  // must come after SetInternalFieldCount
  context->Global()->Set(v8::String::New("AvocadoQueryInstance"),
                         ft->GetFunction());

  // .............................................................................
  // generate the query error template
  // .............................................................................

  ft = v8::FunctionTemplate::New();
  ft->SetClassName(v8::String::New("AvocadoQueryError"));

  rt = ft->InstanceTemplate();
  rt->SetInternalFieldCount(2);

  v8g->QueryErrorTempl = v8::Persistent<v8::ObjectTemplate>::New(rt);

  // must come after SetInternalFieldCount
  context->Global()->Set(v8::String::New("AvocadoQueryError"),
                         ft->GetFunction());
  

  ft = v8::FunctionTemplate::New();
  ft->SetClassName(v8::String::New("AvocadoQueryCursor"));

  rt = ft->InstanceTemplate();
  rt->SetInternalFieldCount(2);

  rt->Set(CountFuncName, v8::FunctionTemplate::New(JS_CountQueryCursor));
  rt->Set(DisposeFuncName, v8::FunctionTemplate::New(JS_DisposeQueryCursor));
  rt->Set(GetMaxFuncName, v8::FunctionTemplate::New(JS_GetMaxQueryCursor));
  rt->Set(GetRowsFuncName, v8::FunctionTemplate::New(JS_GetRowsQueryCursor));
  rt->Set(HasCountFuncName, v8::FunctionTemplate::New(JS_HasCountQueryCursor));
  rt->Set(HasNextFuncName, v8::FunctionTemplate::New(JS_HasNextQueryCursor));
  rt->Set(IdFuncName, v8::FunctionTemplate::New(JS_IdQueryCursor));
  rt->Set(NextFuncName, v8::FunctionTemplate::New(JS_NextQueryCursor));

  v8g->QueryCursorTempl = v8::Persistent<v8::ObjectTemplate>::New(rt);

  // must come after SetInternalFieldCount
  context->Global()->Set(v8::String::New("AvocadoQueryCursor"),
                         ft->GetFunction());

  // .............................................................................
  // generate the cursor template - DEPRECATED
  // .............................................................................

  ft = v8::FunctionTemplate::New();
  ft->SetClassName(v8::String::New("AvocadoCursor"));

  rt = ft->InstanceTemplate();
  rt->SetInternalFieldCount(2);

  rt->Set(HasNextFuncName, v8::FunctionTemplate::New(JS_HasNextCursor));
  rt->Set(NextFuncName, v8::FunctionTemplate::New(JS_NextCursor));
  rt->Set(NextRefFuncName, v8::FunctionTemplate::New(JS_NextRefCursor));
  rt->Set(UseNextFuncName, v8::FunctionTemplate::New(JS_UseNextCursor));
  rt->Set(CountFuncName, v8::FunctionTemplate::New(JS_CountCursor));

  v8g->CursorTempl = v8::Persistent<v8::ObjectTemplate>::New(rt);

  // must come after SetInternalFieldCount
  context->Global()->Set(v8::String::New("AvocadoCursor"),
                         ft->GetFunction());

  // .............................................................................
  // create the global functions
  // .............................................................................

/* DEPRECATED */
  context->Global()->Set(v8::String::New("AQL_WHERE_BOOLEAN"),
                         v8::FunctionTemplate::New(JS_WhereBooleanAql)->GetFunction(),
                         v8::ReadOnly);

  context->Global()->Set(v8::String::New("AQL_WHERE_GENERAL"),
                         v8::FunctionTemplate::New(JS_WhereGeneralAql)->GetFunction(),
                         v8::ReadOnly);

  context->Global()->Set(v8::String::New("AQL_WHERE_HASH_CONST"),
                         v8::FunctionTemplate::New(JS_WhereHashConstAql)->GetFunction(),
                         v8::ReadOnly);
                         
  context->Global()->Set(v8::String::New("AQL_WHERE_SL_CONST"),
                         v8::FunctionTemplate::New(JS_WhereSkiplistConstAql)->GetFunction(),
                         v8::ReadOnly);
                         
  context->Global()->Set(v8::String::New("AQL_WHERE_PRIMARY_CONST"),
                         v8::FunctionTemplate::New(JS_WherePrimaryConstAql)->GetFunction(),
                         v8::ReadOnly);

  context->Global()->Set(v8::String::New("AQL_WHERE_WITHIN_CONST"),
                         v8::FunctionTemplate::New(JS_WhereWithinConstAql)->GetFunction(),
                         v8::ReadOnly);

  context->Global()->Set(v8::String::New("AQL_SELECT"),
                         v8::FunctionTemplate::New(JS_SelectAql)->GetFunction(),
                         v8::ReadOnly);
                         
  context->Global()->Set(v8::String::New("AQL_HASH_SELECT"),
                         v8::FunctionTemplate::New(JS_HashSelectAql)->GetFunction(),
                         v8::ReadOnly);

  context->Global()->Set(v8::String::New("AQL_SL_SELECT"),
                         v8::FunctionTemplate::New(JS_SkiplistSelectAql)->GetFunction(),
                         v8::ReadOnly);
/* DEPRECATED END */
  context->Global()->Set(v8::String::New("AQL_PREPARE"),
                         v8::FunctionTemplate::New(JS_PrepareAql)->GetFunction(),
                         v8::ReadOnly);

  context->Global()->Set(v8::String::New("AQL_CURSOR"),
                         v8::FunctionTemplate::New(JS_Cursor)->GetFunction(),
                         v8::ReadOnly);

  // .............................................................................
  // create the global variables
  // .............................................................................

  context->Global()->Set(v8::String::New("db"),
                         TRI_WrapVocBase(vocbase),
                         v8::ReadOnly);

  context->Global()->Set(v8::String::New("edges"),
                         TRI_WrapEdges(vocbase),
                         v8::ReadOnly);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
