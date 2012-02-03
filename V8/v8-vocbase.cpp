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
#include "BasicsC/logging.h"
#include "BasicsC/strings.h"
#include "QL/ParserWrapper.h"
#include "ShapedJson/shape-accessor.h"
#include "ShapedJson/shaped-json.h"
#include "V8/v8-conv.h"
#include "V8/v8-utils.h"
#include "VocBase/fluent-query.h"
#include "VocBase/query.h"
#include "VocBase/simple-collection.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::avocado;

// -----------------------------------------------------------------------------
// --SECTION--                                              forward declarations
// -----------------------------------------------------------------------------

static bool OptimiseQuery (v8::Handle<v8::Object> queryObject);

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
/// @brief wrapped class for TRI_rc_cursor_t
////////////////////////////////////////////////////////////////////////////////

static int32_t const WRP_RC_CURSOR_TYPE = 4;

////////////////////////////////////////////////////////////////////////////////
/// @brief wrapped class for TRI_query_t
////////////////////////////////////////////////////////////////////////////////

static int32_t const WRP_QUERY_TYPE = 5;

////////////////////////////////////////////////////////////////////////////////
/// @brief wrapped class for TRI_shaped_json_t
////////////////////////////////////////////////////////////////////////////////

static int32_t const WRP_SHAPED_JSON_TYPE = 6;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  HELPER FUNCTIONS
// -----------------------------------------------------------------------------

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

  // point the 0 index Field to the c++ pointer for unwrapping later
  result->SetInternalField(SLOT_CLASS_TYPE, v8::Integer::New(type));
  result->SetInternalField(SLOT_CLASS, v8::External::New(y));

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unwraps a C++ class given a v8::Object
////////////////////////////////////////////////////////////////////////////////

template<class T>
static T* UnwrapClass (v8::Handle<v8::Object> obj, int32_t type) {
  if (obj->InternalFieldCount() < SLOT_CLASS) {
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
/// @brief weak reference callback for result sets
////////////////////////////////////////////////////////////////////////////////

static void WeakResultSetCallback (v8::Persistent<v8::Value> object, void* parameter) {
  TRI_result_set_t* rs;
  TRI_v8_global_t* v8g;

  v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();
  rs = (TRI_result_set_t*) parameter;

  LOG_TRACE("weak-callback for result-set called");

  // find the persistent handle
  v8::Persistent<v8::Value> persistent = v8g->JSResultSets[rs];
  v8g->JSResultSets.erase(rs);

  // dispose and clear the persistent handle
  persistent.Dispose();
  persistent.Clear();

  // and free the result set
  rs->free(rs);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stores a result set in a javascript object
////////////////////////////////////////////////////////////////////////////////

static void StoreResultSet (v8::Handle<v8::Object> rsObject,
                            TRI_result_set_t* rs) {
  TRI_v8_global_t* v8g;

  v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();

  if (v8g->JSResultSets.find(rs) == v8g->JSResultSets.end()) {
    v8::Persistent<v8::Value> persistent = v8::Persistent<v8::Value>::New(v8::External::New(rs));

    rsObject->SetInternalField(SLOT_RESULT_SET, persistent);

    v8g->JSResultSets[rs] = persistent;

    persistent.MakeWeak(rs, WeakResultSetCallback);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief weak reference callback for queries
////////////////////////////////////////////////////////////////////////////////

static void WeakFluentQueryCallback (v8::Persistent<v8::Value> object, void* parameter) {
  TRI_fluent_query_t* query;
  TRI_v8_global_t* v8g;

  v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();
  query = (TRI_fluent_query_t*) parameter;

  LOG_TRACE("weak-callback for query called");

  // find the persistent handle
  v8::Persistent<v8::Value> persistent = v8g->JSFluentQueries[query];
  v8g->JSFluentQueries.erase(query);

  // dispose and clear the persistent handle
  persistent.Dispose();
  persistent.Clear();

  // and free the result set
  query->free(query, true);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stores a fluent query in a javascript object
////////////////////////////////////////////////////////////////////////////////

static void StoreFluentQuery (v8::Handle<v8::Object> queryObject,
                              TRI_fluent_query_t* query) {
  TRI_v8_global_t* v8g;

  v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();

  if (v8g->JSFluentQueries.find(query) == v8g->JSFluentQueries.end()) {
    v8::Persistent<v8::Value> persistent = v8::Persistent<v8::Value>::New(v8::External::New(query));

    queryObject->SetInternalField(SLOT_QUERY, persistent);
    queryObject->SetInternalField(SLOT_RESULT_SET, v8::Null());

    v8g->JSFluentQueries[query] = persistent;

    persistent.MakeWeak(query, WeakFluentQueryCallback);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts a query from a javascript object
////////////////////////////////////////////////////////////////////////////////

static TRI_fluent_query_t* ExtractFluentQuery (v8::Handle<v8::Object> queryObject,
                                               v8::Handle<v8::String>* err) {
  TRI_v8_global_t* v8g;

  v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();

  // the internal fields QUERY and RESULT_SET must be present
  if (queryObject->InternalFieldCount() <= SLOT_RESULT_SET) {
    *err = v8::String::New("corrupted query");
    return 0;
  }

  v8::Handle<v8::Value> value = queryObject->GetInternalField(SLOT_QUERY);

  // .............................................................................
  // special case: the whole collection
  // .............................................................................

  if (value == v8g->CollectionQueryType) {
    TRI_vocbase_col_t const* collection = LoadCollection(queryObject, err);

    if (collection == 0) {
      return false;
    }

    TRI_fluent_query_t* query = TRI_CreateCollectionQuery(collection);

    StoreFluentQuery(queryObject, query);

    return query;
  }

  // .............................................................................
  // standard case: a normal query
  // .............................................................................

  else {
    TRI_fluent_query_t* query = static_cast<TRI_fluent_query_t*>(v8::Handle<v8::External>::Cast(value)->Value());

    if (query == 0) {
      *err = v8::String::New("corrupted query");
      return 0;
    }

    return query;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a query or uses existing result set
////////////////////////////////////////////////////////////////////////////////

static TRI_result_set_t* ExecuteFluentQuery (v8::Handle<v8::Object> queryObject,
                                             v8::Handle<v8::String>* err) {
  v8::Handle<v8::Object> self = queryObject->ToObject();

  if (self->InternalFieldCount() <= SLOT_RESULT_SET) {
    *err = v8::String::New("corrupted query");
    return 0;
  }

  v8::Handle<v8::Value> value = self->GetInternalField(SLOT_RESULT_SET);

  // .............................................................................
  // case: unexecuted query
  // .............................................................................

  if (value->IsNull()) {
    LOG_TRACE("executing query");

    bool ok = OptimiseQuery(self);

    if (! ok) {
      *err = v8::String::New("corrupted query");
      return 0;
    }

    TRI_fluent_query_t* query = ExtractFluentQuery(self, err);

    if (query == 0) {
      return 0;
    }

    // execute the query and get a result set
    TRI_result_set_t* rs = TRI_ExecuteQuery(query);

    if (rs == 0) {
      *err = v8::String::New("cannot execute query");
      return 0;
    }

    if (rs->_error != 0) {
      *err = v8::String::New(rs->_error);
      rs->free(rs);

      return 0;
    }

    StoreResultSet(queryObject, rs);

    return rs;
  }

  // .............................................................................
  // case: already executed query
  // .............................................................................

  else {
    TRI_result_set_t* rs = static_cast<TRI_result_set_t*>(v8::Handle<v8::External>::Cast(value)->Value());

    if (rs == 0) {
      *err = v8::String::New("cannot extract result set");
      return 0;
    }

    return rs;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns true if a query has been executed
////////////////////////////////////////////////////////////////////////////////

static bool IsExecutedQuery (v8::Handle<v8::Object> query) {
  if (query->InternalFieldCount() <= SLOT_RESULT_SET) {
    return false;
  }

  v8::Handle<v8::Value> value = query->GetInternalField(SLOT_RESULT_SET);

  return value->IsNull() ? false : true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stringifies a query
////////////////////////////////////////////////////////////////////////////////

static string StringifyQuery (v8::Handle<v8::Value> queryObject) {
  TRI_v8_global_t* v8g;

  v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();

  if (! queryObject->IsObject()) {
    return "[unknown]";
  }

  v8::Handle<v8::Object> self = queryObject->ToObject();

  if (self->InternalFieldCount() <= SLOT_RESULT_SET) {
    return "[unknown]";
  }

  v8::Handle<v8::Value> value = self->GetInternalField(SLOT_QUERY);

  // .............................................................................
  // special case: the whole collection
  // .............................................................................

  if (value == v8g->CollectionQueryType) {
    TRI_vocbase_col_t const* col = UnwrapClass<TRI_vocbase_col_t>(self, WRP_VOCBASE_COL_TYPE);

    if (col == 0) {
      return "[unknown collection]";
    }

    return "[collection \"" + string(col->_name) + "\"]";
  }

  // .............................................................................
  // standard case: a normal query
  // .............................................................................

  else {
    TRI_fluent_query_t* query = static_cast<TRI_fluent_query_t*>(v8::Handle<v8::External>::Cast(value)->Value());

    if (query == 0) {
      return "[corrupted query]";
    }

    TRI_string_buffer_t buffer;
    TRI_InitStringBuffer(&buffer);

    query->stringify(query, &buffer);
    string name = TRI_BeginStringBuffer(&buffer);

    TRI_DestroyStringBuffer(&buffer);

    return name;
  }

  return TRI_DuplicateString("[corrupted]");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief optimises a query
////////////////////////////////////////////////////////////////////////////////

static bool OptimiseQuery (v8::Handle<v8::Object> queryObject) {
  v8::Handle<v8::Value> value;
  TRI_v8_global_t* v8g;

  v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();
  value = queryObject->GetInternalField(SLOT_QUERY);

  if (value != v8g->CollectionQueryType) {
    TRI_fluent_query_t* query = static_cast<TRI_fluent_query_t*>(v8::Handle<v8::External>::Cast(value)->Value());

    if (query == 0) {
      return false;
    }

    TRI_fluent_query_t* optimised = query;

    optimised->optimise(&optimised);

    if (optimised != query) {

      // remove old query
      v8::Persistent<v8::Value> persistent = v8g->JSFluentQueries[query];

      v8g->JSFluentQueries.erase(query);
      persistent.Dispose();
      persistent.Clear();

      // add optimised query
      persistent = v8::Persistent<v8::Value>::New(v8::External::New(optimised));
      queryObject->SetInternalField(SLOT_QUERY, persistent);

      v8g->JSFluentQueries[optimised] = persistent;

      persistent.MakeWeak(optimised, WeakFluentQueryCallback);
    }
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up edges
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> EdgesQuery (TRI_edge_direction_e direction, v8::Arguments const& argv) {
  TRI_v8_global_t* v8g;
  v8::HandleScope scope;

  v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();

  // extract the operand query
  v8::Handle<v8::Object> operand = argv.Holder();
  v8::Handle<v8::String> err;
  TRI_fluent_query_t* opQuery = ExtractFluentQuery(operand, &err);

  if (opQuery == 0) {
    return scope.Close(v8::ThrowException(err));
  }

  if (IsExecutedQuery(operand)) {
    return scope.Close(v8::ThrowException(v8::String::New("query already executed")));
  }

  // first and only argument schould be an document idenfifier
  if (argv.Length() != 1) {
    switch (direction) {
      case TRI_EDGE_UNUSED:
        return scope.Close(v8::ThrowException(v8::String::New("usage: noneEdge(<edges-collection>)")));

      case TRI_EDGE_IN:
        return scope.Close(v8::ThrowException(v8::String::New("usage: inEdge(<edges-collection>)")));

      case TRI_EDGE_OUT:
        return scope.Close(v8::ThrowException(v8::String::New("usage: outEdge(<edges-collection>)")));

      case TRI_EDGE_ANY:
        return scope.Close(v8::ThrowException(v8::String::New("usage: edge(<edges-collection>)")));
    }
  }

  if (! argv[0]->IsObject()) {
    return scope.Close(v8::ThrowException(v8::String::New("<edges-collection> must be an object")));
  }

  TRI_vocbase_col_t const* edges = LoadCollection(argv[0]->ToObject(), &err);

  if (edges == 0) {
    return scope.Close(v8::ThrowException(err));
  }

  // .............................................................................
  // create new query
  // .............................................................................

  v8::Handle<v8::Object> result = v8g->FluentQueryTempl->NewInstance();

  StoreFluentQuery(result, TRI_CreateEdgesQuery(edges, opQuery->clone(opQuery), direction));

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief weak reference callback for queries
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

  // and free the result set
  TRI_FreeQuery(query);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stores a query in a javascript object
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Object> WrapQuery (TRI_query_t* query) {
  TRI_v8_global_t* v8g;

  v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();

  v8::Handle<v8::Object> queryObject = v8g->QueryTempl->NewInstance();
  map< TRI_query_t*, v8::Persistent<v8::Value> >::iterator i = v8g->JSQueries.find(query);

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
/// @brief executes a query or uses existing result set
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
/// @brief weak reference callback for cursors
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
  cursor->free(cursor);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stores a cursor in a javascript object
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Object> WrapCursor (TRI_rc_cursor_t* cursor) {
  TRI_v8_global_t* v8g;

  v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();

  v8::Handle<v8::Object> cursorObject = v8g->CursorTempl->NewInstance();
  map< TRI_rc_cursor_t*, v8::Persistent<v8::Value> >::iterator i = v8g->JSCursors.find(cursor);

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
/// @brief extracts a cursor from a javascript object
////////////////////////////////////////////////////////////////////////////////

static TRI_rc_cursor_t* UnwrapCursor (v8::Handle<v8::Object> cursorObject) {
  return UnwrapClass<TRI_rc_cursor_t>(cursorObject, WRP_RC_CURSOR_TYPE);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief weak reference callback for wheres
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
/// @brief stores a where clause in a javascript object
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Object> WrapWhere (TRI_qry_where_t* where) {
  TRI_v8_global_t* v8g;

  v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();

  v8::Handle<v8::Object> whereObject = v8g->WhereTempl->NewInstance();
  map< TRI_qry_where_t*, v8::Persistent<v8::Value> >::iterator i = v8g->JSWheres.find(where);

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
/// @brief extracts a where clause from a javascript object
////////////////////////////////////////////////////////////////////////////////

static TRI_qry_where_t* UnwrapWhere (v8::Handle<v8::Object> whereObject) {
  return UnwrapClass<TRI_qry_where_t>(whereObject, WRP_QRY_WHERE_TYPE);
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
/// @brief converts a query into a fluent interface representation
///
/// @FUN{show()}
///
/// Shows the representation of the query.
///
/// MISSIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIING
///
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_ShowQuery (v8::Arguments const& argv) {
  v8::HandleScope scope;

  v8::Handle<v8::Object> self = argv.Holder();

  string result = StringifyQuery(self);

  return scope.Close(v8::String::New(result.c_str()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief counts the number of documents in a result set
///
/// @FUN{count()}
///
/// The @FN{count} operator counts the number of document in the result set and
/// returns that number. The @FN{count} operator ignores any limits and returns
/// the total number of documents found.
///
/// @verbinclude fluent24
///
/// @FUN{count(@LIT{true})}
///
/// If the result set was limited by the @FN{limit} operator or documents were
/// skiped using the @FN{skip} operator, the @FN{count} operator with argument
/// @LIT{true} will use the number of elements in the final result set - after
/// applying @FN{limit} and @FN{skip}.
///
/// @verbinclude fluent25
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_CountQuery (v8::Arguments const& argv) {
  v8::HandleScope scope;

  v8::Handle<v8::String> err;
  TRI_result_set_t* rs = ExecuteFluentQuery(argv.Holder(), &err);

  if (rs == 0) {
    return scope.Close(v8::ThrowException(err));
  }

  if (0 < argv.Length() && argv[0]->IsTrue()) {
    return scope.Close(v8::Number::New(rs->count(rs, true)));
  }
  else {
    return scope.Close(v8::Number::New(rs->count(rs, false)));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief explains how a query was executed (TO BE DELETED)
///
/// @FUN{explain()}
///
/// In order to optimise queries you need to know how the storage engine
/// executed that type of query. You can use the @FN{explain} operator to see
/// how a query was executed.
///
/// @verbinclude fluent9
///
/// The @FN{explain} operator returns an object with the following attributes.
///
/// - cursor:
///     describes how the result set was computed.
/// - scannedIndexEntries:
///     how many index entries were scanned
/// - scannedDocuments:
///     how many documents were scanned
/// - matchedDocuments:
///     the sum of all matched documents in each step
/// - runtime:
///     the runtime in seconds
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_ExplainQuery (v8::Arguments const& argv) {
  TRI_v8_global_t* v8g;
  v8::HandleScope scope;

  v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();

  v8::Handle<v8::String> err;
  v8::Handle<v8::Object> self = argv.Holder();
  TRI_result_set_t* rs = ExecuteFluentQuery(self, &err);

  if (rs == 0) {
    return scope.Close(v8::ThrowException(err));
  }

  v8::Handle<v8::Object> result = v8::Object::New();

  result->Set(v8::String::New("cursor"), v8::String::New(rs->_info._cursor));
  result->Set(v8::String::New("scannedIndexEntries"), v8::Number::New(rs->_info._scannedIndexEntries));
  result->Set(v8::String::New("scannedDocuments"), v8::Number::New(rs->_info._scannedDocuments));
  result->Set(v8::String::New("matchedDocuments"), v8::Number::New(rs->_info._matchedDocuments));
  result->Set(v8::String::New("runtime"), v8::Number::New(rs->_info._runtime));

  v8::Handle<v8::Value> value = self->GetInternalField(SLOT_QUERY);

  if (value != v8g->CollectionQueryType) {
    TRI_fluent_query_t* query = static_cast<TRI_fluent_query_t*>(v8::Handle<v8::External>::Cast(value)->Value());

    if (query == 0) {
      return scope.Close(v8::ThrowException(v8::String::New("corrupted query")));
    }

    result->Set(v8::String::New("query"), TRI_ObjectJson(query->json(query)));
  }

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks for the next result document (TO BE DELETED)
///
/// @FUN{hasNext()}
///
/// The @FN{hasNext} operator returns @LIT{true}, then the cursor still has
/// documents.  In this case the next document can be accessed using the
/// @FN{next} operator, which will advance the cursor.
///
/// @verbinclude fluent3
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_HasNextQuery (v8::Arguments const& argv) {
  v8::HandleScope scope;

  v8::Handle<v8::String> err;
  TRI_result_set_t* rs = ExecuteFluentQuery(argv.Holder(), &err);

  if (rs == 0) {
    return scope.Close(v8::ThrowException(err));
  }

  return scope.Close(rs->hasNext(rs) ? v8::True() : v8::False());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the next result document (TO BE DELETED)
///
/// @FUN{next()}
///
/// If the @FN{hasNext} operator returns @LIT{true}, then the cursor still has
/// documents.  In this case the next document can be accessed using the @FN{next}
/// operator, which will advance the cursor. If you use @FN{next} on an
/// exhausted cursor, then @LIT{undefined} is returned.
///
/// @verbinclude fluent28
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_NextQuery (v8::Arguments const& argv) {
  v8::HandleScope scope;

  v8::Handle<v8::String> err;
  TRI_result_set_t* rs = ExecuteFluentQuery(argv.Holder(), &err);

  if (rs == 0) {
    return scope.Close(v8::ThrowException(err));
  }

  if (rs->hasNext(rs)) {
    TRI_doc_collection_t* collection = rs->_containerElement->_container->_collection;
    TRI_shaper_t* shaper = collection->_shaper;
    TRI_rs_entry_t const* entry = rs->next(rs);

    return scope.Close(TRI_ObjectRSEntry(collection, shaper, entry));
  }
  else {
    return scope.Close(v8::Undefined());
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the next result document reference (TO BE DELETED)
///
/// @FUN{nextRef()}
///
/// If the @FN{hasNext} operator returns @LIT{true}, then the cursor still has
/// documents.  In this case the next document reference can be
/// accessed using the @FN{nextRef} operator, which will advance the
/// cursor.
///
/// @verbinclude fluent51
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_NextRefQuery (v8::Arguments const& argv) {
  v8::HandleScope scope;

  v8::Handle<v8::String> err;
  TRI_result_set_t* rs = ExecuteFluentQuery(argv.Holder(), &err);

  if (rs == 0) {
    return scope.Close(v8::ThrowException(err));
  }

  if (rs->hasNext(rs)) {
    TRI_doc_collection_t* collection = rs->_containerElement->_container->_collection;
    TRI_rs_entry_t const* entry = rs->next(rs);

    string ref = StringUtils::itoa(collection->base._cid) + ":" + StringUtils::itoa(entry->_did);

    return scope.Close(v8::String::New(ref.c_str()));
  }
  else {
    return scope.Close(v8::Undefined());
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief optimises a query (TO BE DELETED)
///
/// @FUN{optimise()}
///
/// Optimises a query. This is done automatically when executing a query.
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_OptimiseQuery (v8::Arguments const& argv) {
  v8::HandleScope scope;

  v8::Handle<v8::Object> self = argv.Holder();

  if (self->InternalFieldCount() <= SLOT_RESULT_SET) {
    return scope.Close(v8::ThrowException(v8::String::New("corrupted query")));
  }

  if (IsExecutedQuery(self)) {
    return scope.Close(v8::ThrowException(v8::String::New("query already executed")));
  }

  bool ok = OptimiseQuery(self);

  if (! ok) {
    return scope.Close(v8::ThrowException(v8::String::New("corrupted query")));
  }

  return scope.Close(self);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a query and returns the result as array (TO BE DELETED)
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_ToArrayQuery (v8::Arguments const& argv) {
  v8::HandleScope scope;

  v8::Handle<v8::String> err;
  TRI_result_set_t* rs = ExecuteFluentQuery(argv.Holder(), &err);

  if (rs == 0) {
    return scope.Close(v8::ThrowException(err));
  }

  return scope.Close(TRI_ArrayResultSet(rs));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns all elements (TO BE DELETED)
///
/// @FUN{all()}
///
/// Selects all documents of a collection and returns a cursor.
///
/// @verbinclude fluent23
///
/// The corresponding AQL query would be:
///
/// @verbinclude fluent23-aql
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_AllQuery (v8::Arguments const& argv) {
  TRI_v8_global_t* v8g;
  v8::HandleScope scope;

  v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();

  // extract the operand query
  v8::Handle<v8::Object> operand = argv.Holder();
  v8::Handle<v8::String> err;
  TRI_fluent_query_t* opQuery = ExtractFluentQuery(operand, &err);

  if (opQuery == 0) {
    return scope.Close(v8::ThrowException(err));
  }

  if (IsExecutedQuery(operand)) {
    return scope.Close(v8::ThrowException(v8::String::New("query already executed")));
  }

  // .............................................................................
  // case: no arguments
  // .............................................................................

  if (argv.Length() == 0) {
    v8::Handle<v8::Object> result = v8g->FluentQueryTempl->NewInstance();

    StoreFluentQuery(result, opQuery->clone(opQuery));

    return scope.Close(result);
  }

  // .............................................................................
  // error case
  // .............................................................................

  else {
    return scope.Close(v8::ThrowException(v8::String::New("usage: all()")));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds distance to a result set
///
/// @FUN{distance()}
///
/// Aguments the result-set of a @FN{near} or @FN{within} query with the
/// distance between the document and the given point. The distance is returned
/// in an attribute @LIT{_distance}. This is the distance in meters.
///
/// @verbinclude fluent26
///
/// @FUN{distance(@FA{name})}
///
/// Same as above, with the exception, that the distance is returned in an
/// attribute called @FA{name}.
///
/// @verbinclude fluent27
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_DistanceQuery (v8::Arguments const& argv) {
  TRI_v8_global_t* v8g;
  v8::HandleScope scope;

  v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();

  // extract the operand query
  v8::Handle<v8::Object> operand = argv.Holder();
  v8::Handle<v8::String> err;
  TRI_fluent_query_t* opQuery = ExtractFluentQuery(operand, &err);

  if (opQuery == 0) {
    return scope.Close(v8::ThrowException(err));
  }

  if (IsExecutedQuery(operand)) {
    return scope.Close(v8::ThrowException(v8::String::New("query already executed")));
  }

  // .............................................................................
  // case: no arguments
  // .............................................................................

  if (argv.Length() == 0) {
    v8::Handle<v8::Object> result = v8g->FluentQueryTempl->NewInstance();

    StoreFluentQuery(result, TRI_CreateDistanceQuery(opQuery->clone(opQuery), "_distance"));

    return scope.Close(result);
  }

  // .............................................................................
  // case: <distance>
  // .............................................................................

  else if (argv.Length() == 1) {
    string name = TRI_ObjectToString(argv[0]);

    if (name.empty()) {
      return scope.Close(v8::ThrowException(v8::String::New("<attribute> must be non-empty")));
    }

    v8::Handle<v8::Object> result = v8g->FluentQueryTempl->NewInstance();

    StoreFluentQuery(result, TRI_CreateDistanceQuery(opQuery->clone(opQuery), name.c_str()));

    return scope.Close(result);
  }

  // .............................................................................
  // error case
  // .............................................................................

  else {
    return scope.Close(v8::ThrowException(v8::String::New("usage: distance([<attribute>])")));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a document (TO BE DELETED)
///
/// @FUN{document(@FA{document-identifier})}
///
/// The @FN{document} operator finds a document given it's identifier.  It returns
/// the empty result set or a result set containing the document with document
/// identifier @FA{document-identifier}.
///
/// @verbinclude fluent54
///
/// The corresponding AQL query would be:
///
/// @verbinclude fluent54-aql
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_DocumentQuery (v8::Arguments const& argv) {
  TRI_v8_global_t* v8g;
  v8::HandleScope scope;

  v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();

  // extract the operand query
  v8::Handle<v8::Object> operand = argv.Holder();
  v8::Handle<v8::String> err;
  TRI_fluent_query_t* opQuery = ExtractFluentQuery(operand, &err);

  if (opQuery == 0) {
    return scope.Close(v8::ThrowException(err));
  }

  if (IsExecutedQuery(operand)) {
    return scope.Close(v8::ThrowException(v8::String::New("query already executed")));
  }

  // first and only argument schould be an document idenfifier
  if (argv.Length() != 1) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: document(<document-identifier>)")));
  }

  v8::Handle<v8::Value> arg1 = argv[0];
  TRI_voc_cid_t cid = opQuery->_collection->_collection->base._cid;
  TRI_voc_did_t did;

  if (! IsDocumentId(arg1, cid, did)) {
    return scope.Close(v8::ThrowException(v8::String::New("<document-idenifier> must be a document identifier")));
  }

  if (cid != opQuery->_collection->_collection->base._cid) {
    return scope.Close(v8::ThrowException(v8::String::New("cannot execute cross collection query")));
  }

  // .............................................................................
  // create new query
  // .............................................................................

  v8::Handle<v8::Object> result = v8g->FluentQueryTempl->NewInstance();

  StoreFluentQuery(result, TRI_CreateDocumentQuery(opQuery->clone(opQuery), did));

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a document (TEST)
///
/// @FUN{document(@FA{document-identifier})}
///
/// The @FN{document} operator finds a document given it's identifier.  It returns
/// the empty result set or a result set containing the document with document
/// identifier @FA{document-identifier}.
///
/// @verbinclude fluent54
///
/// The corresponding AQL query would be:
///
/// @verbinclude fluent54-aql
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_DocumentQueryWrapped (v8::Arguments const& argv) {
  TRI_v8_global_t* v8g;
  v8::HandleScope scope;

  v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();

  // extract the operand query
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
  
  collection->_collection->beginRead(collection->_collection);
  
  document = collection->_collection->read(collection->_collection, did);  

  collection->_collection->endRead(collection->_collection);

  if (document == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("document not found")));
  }
  
  v8::Handle<v8::Value> result = TRI_WrapShapedJson(&document->_document, collection);
  
  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up all edges
///
/// @FUN{@FA{edges-collection})}
///
/// The @FN{edges} operator finds all edges starting from (outbound) or
/// ending in (inbound) the current vertices.
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_EdgesQuery (v8::Arguments const& argv) {
  return EdgesQuery(TRI_EDGE_ANY, argv);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up all inbound edges
///
/// @FUN{@FA{edges-collection})}
///
/// The @FN{edges} operator finds all edges starting from (outbound)
/// or ending in (inbound) the current vertices.
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_InEdgesQuery (v8::Arguments const& argv) {
  return EdgesQuery(TRI_EDGE_IN, argv);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a geo-spatial index
///
/// @FUN{geo(@FA{location})}
///
/// The next @FN{near} operator will use the specific geo-spatial index.
///
/// @FUN{geo(@FA{location}, @LIT{true})}
///
/// The next @FN{near} or @FN{within} operator will use the specific geo-spatial
/// index.
///
/// @FUN{geo(@FA{latitiude}, @FA{longitude})}
///
/// The next @FN{near} or @FN{within} operator will use the specific geo-spatial
/// index.
///
/// Assume you have a location stored as list in the attribute @LIT{home}
/// and a destination stored in the attribute @LIT{work}. Than you can use the
/// @FN{geo} operator to select, which coordinates to use in a near query.
///
/// @verbinclude fluent15
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_GeoQuery (v8::Arguments const& argv) {
  TRI_v8_global_t* v8g;
  v8::HandleScope scope;

  v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();

  // extract the operand query
  v8::Handle<v8::Object> operand = argv.Holder();
  v8::Handle<v8::String> err;
  TRI_fluent_query_t* opQuery = ExtractFluentQuery(operand, &err);

  if (opQuery == 0) {
    return scope.Close(v8::ThrowException(err));
  }

  if (IsExecutedQuery(operand)) {
    return scope.Close(v8::ThrowException(v8::String::New("query already executed")));
  }

  // .............................................................................
  // case: <location>
  // .............................................................................

  if (argv.Length() == 1) {
    string location = TRI_ObjectToString(argv[0]);

    v8::Handle<v8::Object> result = v8g->FluentQueryTempl->NewInstance();

    StoreFluentQuery(result, TRI_CreateGeoIndexQuery(opQuery->clone(opQuery), location.c_str(), 0, 0, false));

    return scope.Close(result);
  }

  // .............................................................................
  // case: <location>, <geoJson>
  // .............................................................................

  else if (argv.Length() == 2 && (argv[1]->IsBoolean() || argv[1]->IsBooleanObject())) {
    string location = TRI_ObjectToString(argv[0]);
    bool geoJson = TRI_ObjectToBoolean(argv[1]);

    v8::Handle<v8::Object> result = v8g->FluentQueryTempl->NewInstance();

    StoreFluentQuery(result, TRI_CreateGeoIndexQuery(opQuery->clone(opQuery), location.c_str(), 0, 0, geoJson));

    return scope.Close(result);
  }

  // .............................................................................
  // case: <latitiude>, <longitude>
  // .............................................................................

  else if (argv.Length() == 2) {
    string latitiude = TRI_ObjectToString(argv[0]);
    string longitude = TRI_ObjectToString(argv[1]);

    v8::Handle<v8::Object> result = v8g->FluentQueryTempl->NewInstance();

    StoreFluentQuery(result, TRI_CreateGeoIndexQuery(opQuery->clone(opQuery), 0, latitiude.c_str(), longitude.c_str(), false));

    return scope.Close(result);
  }

  // .............................................................................
  // error case
  // .............................................................................

  else {
    return scope.Close(v8::ThrowException(v8::String::New("usage: geo(<location>|<latitiude>,<longitude>)")));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief limits an existing query (TO BE DELETED)
///
/// @FUN{limit(@FA{number})}
///
/// Limits a result to the first @FA{number} documents. Specifying a limit of
/// @CODE{0} returns no documents at all. If you do not need a limit, just do
/// not add the limit operator. If you specifiy a negtive limit of @CODE{-n},
/// this will return the last @CODE{n} documents instead.
///
/// @verbinclude fluent30
///
/// The corresponding AQL queries would be:
///
/// @verbinclude fluent30-aql
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_LimitQuery (v8::Arguments const& argv) {
  TRI_v8_global_t* v8g;
  v8::HandleScope scope;

  v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();

  // extract the operand query
  v8::Handle<v8::Object> operand = argv.Holder();
  v8::Handle<v8::String> err;
  TRI_fluent_query_t* opQuery = ExtractFluentQuery(operand, &err);

  if (opQuery == 0) {
    return scope.Close(v8::ThrowException(err));
  }

  if (IsExecutedQuery(operand)) {
    return scope.Close(v8::ThrowException(v8::String::New("query already executed")));
  }

  // first and only argument schould be an integer
  if (argv.Length() != 1) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: limit(<limit>)")));
  }

  double limit = TRI_ObjectToDouble(argv[0]);

  // .............................................................................
  // create new query
  // .............................................................................

  v8::Handle<v8::Object> result = v8g->FluentQueryTempl->NewInstance();

  StoreFluentQuery(result, TRI_CreateLimitQuery(opQuery->clone(opQuery), (TRI_voc_ssize_t) limit));

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finds points near a given coordinate
///
/// @FUN{near(@FA{latitiude}, @FA{longitude})}
///
/// This will find at most 100 documents near the coordinate (@FA{latitiude},
/// @FA{longitude}). The returned list is sorted according to the distance, with
/// the nearest document coming first.
///
/// @verbinclude fluent10
///
/// If you need the distance as well, then you can use
///
/// @FUN{near(@FA{latitiude}, @FA{longitude}).distance()}
///
/// This will add an attribute @LIT{_distance} to all documents returned, which
/// contains the distance between the given point and the document in meter.
///
/// @verbinclude fluent11
///
/// @FUN{near(@FA{latitiude}, @FA{longitude}).distance(@FA{name})}
///
/// This will add an attribute @FA{name} to all documents returned, which
/// contains the distance between the given point and the document in meter.
///
/// @verbinclude fluent12
///
/// @FUN{near(@FA{latitiude}, @FA{longitude}).limit(@FA{count})}
///
/// Limits the result to @FA{count} documents. Note that @FA{count} can be more
/// than 100. To get less or more than 100 documents with distances, use
///
/// @FUN{near(@FA{latitiude}, @FA{longitude}).distance().limit(@FA{count})}
///
/// This will return the first @FA{count} documents together with their
/// distances in meters.
///
/// @verbinclude fluent13
///
/// If you have more then one geo-spatial index, you can use the @FN{geo}
/// operator to select a particular index.
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_NearQuery (v8::Arguments const& argv) {
  TRI_v8_global_t* v8g;
  v8::HandleScope scope;

  v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();

  // extract the operand query
  v8::Handle<v8::Object> operand = argv.Holder();
  v8::Handle<v8::String> err;
  TRI_fluent_query_t* opQuery = ExtractFluentQuery(operand, &err);

  if (opQuery == 0) {
    return scope.Close(v8::ThrowException(err));
  }

  if (IsExecutedQuery(operand)) {
    return scope.Close(v8::ThrowException(v8::String::New("query already executed")));
  }

  // .............................................................................
  // near(latitiude, longitude)
  // .............................................................................

  if (argv.Length() == 2) {
    double latitiude = TRI_ObjectToDouble(argv[0]);
    double longitude = TRI_ObjectToDouble(argv[1]);

    v8::Handle<v8::Object> result = v8g->FluentQueryTempl->NewInstance();

    StoreFluentQuery(result, TRI_CreateNearQuery(opQuery->clone(opQuery), latitiude, longitude, NULL));

    return scope.Close(result);
  }

  // .............................................................................
  // error case
  // .............................................................................

  else {
    return scope.Close(v8::ThrowException(v8::String::New("usage: near(<latitiude>,<longitude>)")));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up all outbound edges
///
/// @FUN{@FA{edges-collection})}
///
/// The @FN{edges} operator finds all edges starting from (outbound)
/// the current vertices.
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_OutEdgesQuery (v8::Arguments const& argv) {
  return EdgesQuery(TRI_EDGE_OUT, argv);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief selects elements by example
///
/// @FUN{select()}
///
/// The @FN{select} operator finds all documents.
///
/// @verbinclude fluent52
///
/// @FUN{select(@FA{example})}
///
/// This version selects all documents with match a given example.
///
/// @verbinclude fluent53
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_SelectQuery (v8::Arguments const& argv) {
  TRI_v8_global_t* v8g;
  v8::HandleScope scope;

  v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();

  // extract the operand query
  v8::Handle<v8::Object> operand = argv.Holder();
  v8::Handle<v8::String> err;
  TRI_fluent_query_t* opQuery = ExtractFluentQuery(operand, &err);

  if (opQuery == 0) {
    return scope.Close(v8::ThrowException(err));
  }

  if (IsExecutedQuery(operand)) {
    return scope.Close(v8::ThrowException(v8::String::New("query already executed")));
  }

  // .............................................................................
  // case: no arguments
  // .............................................................................

  if (argv.Length() == 0) {
    v8::Handle<v8::Object> result = v8g->FluentQueryTempl->NewInstance();

    StoreFluentQuery(result, opQuery->clone(opQuery));

    return scope.Close(result);
  }

  // .............................................................................
  // case: one arguments
  // .............................................................................

  if (argv.Length() == 1) {
    if (! argv[0]->IsObject()) {
      return scope.Close(v8::ThrowException(v8::String::New("expecting an object as <example>")));
    }

    v8::Handle<v8::Object> example = argv[0]->ToObject();
    v8::Handle<v8::Array> names = example->GetOwnPropertyNames();
    size_t n = names->Length();

    TRI_shaper_t* shaper = opQuery->_collection->_collection->_shaper;

    TRI_shape_pid_t* pids = (TRI_shape_pid_t*) TRI_Allocate(n * sizeof(TRI_shape_pid_t));
    TRI_shaped_json_t** values = (TRI_shaped_json_t**) TRI_Allocate(n * sizeof(TRI_shaped_json_t*));

    for (size_t i = 0;  i < n;  ++i) {
      v8::Handle<v8::Value> key = names->Get(i);
      v8::Handle<v8::Value> val = example->Get(key);

      v8::String::Utf8Value keyStr(key);
      pids[i] = shaper->findAttributePathByName(shaper, *keyStr);
      values[i] = TRI_ShapedJsonV8Object(val, shaper);

      if (*keyStr == 0 || values[i] == 0) {
        for (size_t j = 0;  j < i;  ++j) {
          TRI_FreeShapedJson(values[i]);
        }

        TRI_Free(values);
        TRI_Free(pids);

        if (*keyStr == 0) {
          return scope.Close(v8::ThrowException(v8::String::New("cannot convert attribute name to UTF8")));
        }
        else if (values[i] == 0) {
          return scope.Close(v8::ThrowException(v8::String::New("cannot convert value to JSON")));
        }

        assert(false);
      }
    }

    v8::Handle<v8::Object> result = v8g->FluentQueryTempl->NewInstance();

    StoreFluentQuery(result, TRI_CreateSelectFullQuery(opQuery->clone(opQuery), n, pids, values));

    return scope.Close(result);
  }

  // .............................................................................
  // error case
  // .............................................................................

  else {
    return scope.Close(v8::ThrowException(v8::String::New("usage: select([<example>])")));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief skips an existing query (TO BE DELETED)
///
/// @FUN{skip(@FA{number})}
///
/// Skips the first @FA{number} documents.
///
/// @verbinclude fluent31
///
/// The corresponding AQL queries would be:
///
/// @verbinclude fluent31-aql
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_SkipQuery (v8::Arguments const& argv) {
  TRI_v8_global_t* v8g;
  v8::HandleScope scope;

  v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();

  // extract the operand query
  v8::Handle<v8::Object> operand = argv.Holder();
  v8::Handle<v8::String> err;
  TRI_fluent_query_t* opQuery = ExtractFluentQuery(operand, &err);

  if (opQuery == 0) {
    return scope.Close(v8::ThrowException(err));
  }

  if (IsExecutedQuery(operand)) {
    return scope.Close(v8::ThrowException(v8::String::New("query already executed")));
  }

  // first and only argument schould be an integer
  if (argv.Length() != 1) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: skip(<number>)")));
  }

  double skip = TRI_ObjectToDouble(argv[0]);

  if (skip < 0.0) {
    skip = 0.0;
  }

  // .............................................................................
  // create new query
  // .............................................................................

  v8::Handle<v8::Object> result = v8g->FluentQueryTempl->NewInstance();

  StoreFluentQuery(result, TRI_CreateSkipQuery(opQuery->clone(opQuery), (TRI_voc_size_t) skip));

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finds points within a given radius
///
/// @FUN{within(@FA{latitiude}, @FA{longitude}, @FA{radius})}
///
/// This will find all documents with in a given radius around the coordinate
/// (@FA{latitiude}, @FA{longitude}). The returned list is unsorted.
///
/// @verbinclude fluent32
///
/// If you need the distance as well, then you can use
///
/// @FUN{within(@FA{latitiude}, @FA{longitude}, @FA{radius}).distance()}
///
/// This will add an attribute @LIT{_distance} to all documents returned, which
/// contains the distance between the given point and the document in meter.
///
/// @verbinclude fluent33
///
/// @FUN{within(@FA{latitiude}, @FA{longitude}, @FA{radius}).distance(@FA{name})}
///
/// This will add an attribute @FA{name} to all documents returned, which
/// contains the distance between the given point and the document in meter.
///
/// @verbinclude fluent34
///
/// If you have more then one geo-spatial index, you can use the @FN{geo}
/// operator to select a particular index.
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_WithinQuery (v8::Arguments const& argv) {
  TRI_v8_global_t* v8g;
  v8::HandleScope scope;

  v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();

  // extract the operand query
  v8::Handle<v8::Object> operand = argv.Holder();
  v8::Handle<v8::String> err;
  TRI_fluent_query_t* opQuery = ExtractFluentQuery(operand, &err);

  if (opQuery == 0) {
    return scope.Close(v8::ThrowException(err));
  }

  if (IsExecutedQuery(operand)) {
    return scope.Close(v8::ThrowException(v8::String::New("query already executed")));
  }

  // .............................................................................
  // within(latitiude, longitude, radius)
  // .............................................................................

  if (argv.Length() == 3) {
    double latitiude = TRI_ObjectToDouble(argv[0]);
    double longitude = TRI_ObjectToDouble(argv[1]);
    double radius = TRI_ObjectToDouble(argv[2]);

    v8::Handle<v8::Object> result = v8g->FluentQueryTempl->NewInstance();

    StoreFluentQuery(result, TRI_CreateWithinQuery(opQuery->clone(opQuery), latitiude, longitude, radius, NULL));

    return scope.Close(result);
  }

  // .............................................................................
  // error case
  // .............................................................................

  else {
    return scope.Close(v8::ThrowException(v8::String::New("usage: within(<latitiude>,<longitude>,<radius>)")));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

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
/// @brief constructs a new query from a string
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_PrepareAql (v8::Arguments const& argv) {
  v8::HandleScope scope;
  TRI_v8_global_t* v8g;

  v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();

  if (argv.Length() != 2) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: AQL_PREPARE(<db>, <query>)")));
  }

  v8::Handle<v8::Object> dbArg = argv[0]->ToObject();
  TRI_vocbase_t* vocbase = UnwrapClass<TRI_vocbase_t>(dbArg, WRP_VOCBASE_TYPE);

  if (vocbase == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("corrupted vocbase")));
  }

  // get the query string
  v8::Handle<v8::Value> queryArg = argv[1];
  if (!queryArg->IsString()) {
    return scope.Close(v8::ThrowException(v8::String::New("expecting string for <query>")));
  }

  string queryString = TRI_ObjectToString(queryArg);

  // create a parser object
  ParserWrapper parser = ParserWrapper(queryString.c_str());
  if (!parser.parse()) {
    // error object will be freed by parser destructor
    ParseError *error = parser.getParseError();
    if (error) {
      return scope.Close(v8::ThrowException(v8::String::New(error->getDescription().c_str())));
    }
    return scope.Close(v8::ThrowException(v8::String::New("got an unknown error from the parser")));
  }

  if (parser.getQueryType() == QLQueryTypeEmpty) {
    return scope.Close(v8::ThrowException(v8::String::New("query is empty")));
  }

  // construct FROM part
  char *primaryAlias = parser.getPrimaryAlias();
  char *primaryName  = parser.getPrimaryName();

  if (primaryAlias == 0 || primaryName == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("collection not found")));
  }

  TRI_vocbase_col_t const* collection = TRI_LoadCollectionVocBase(vocbase, primaryName);
  if (collection == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("collection not found")));
  }

  // create the query
  TRI_query_t* query  = TRI_CreateQuery(parser.getSelect(),
                                        parser.getWhere(),
                                        parser.getOrder(),
                                        primaryAlias,
                                        collection->_collection);

  query->_skip        = parser.getSkip();
  query->_limit       = parser.getLimit();

  // wrap it up
  return scope.Close(WrapQuery(query));
}


////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new constant where clause using a boolean
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_WhereBooleanAql (v8::Arguments const& argv) {
  v8::HandleScope scope;
  TRI_v8_global_t* v8g;

  v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();

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
/// @brief constructs a new where clause from JavaScript
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_WhereGeneralAql (v8::Arguments const& argv) {
  v8::HandleScope scope;
  TRI_v8_global_t* v8g;

  v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();

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
/// @brief constructs a new constant where clause for primary index
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_WherePrimaryConstAql (v8::Arguments const& argv) {
  v8::HandleScope scope;
  TRI_v8_global_t* v8g;

  v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();

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
/// @brief constructs a new constant where clause for geo index
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_WhereWithinConstAql (v8::Arguments const& argv) {
  v8::HandleScope scope;
  TRI_v8_global_t* v8g;

  v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();

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
/// @brief constructs a new query from given parts
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_SelectAql (v8::Arguments const& argv) {
  v8::HandleScope scope;
  TRI_v8_global_t* v8g;

  v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();

  if (argv.Length() != 7) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: AQL_SELECT(<select>, <name>, <primary>, <joins>, <where>, <skip>, <limit>)")));
  }

  // extract the select clause
  v8::Handle<v8::Value> selectArg = argv[0];
  TRI_qry_select_t* select;

  if (selectArg->IsNull()) {
    select = TRI_CreateQuerySelectDocument();
  }
  else {
    string cmd = TRI_ObjectToString(selectArg);

    select = TRI_CreateQuerySelectGeneral(cmd.c_str());
  }

  // extract the name of primary collection
  v8::Handle<v8::Value> nameArg = argv[1];
  string name = TRI_ObjectToString(nameArg);

  if (name.empty()) {
    select->free(select);
    return scope.Close(v8::ThrowException(v8::String::New("expecting a none-empty name for <primary>")));
  }

  // extract the primary collection
  v8::Handle<v8::Value> primaryArg = argv[2];

  if (! primaryArg->IsObject()) {
    select->free(select);
    return scope.Close(v8::ThrowException(v8::String::New("expecting a collection for <primary>")));
  }

  v8::Handle<v8::Object> primaryObj = primaryArg->ToObject();

  v8::Handle<v8::String> err;
  TRI_vocbase_col_t const* collection = LoadCollection(primaryObj, &err);

  if (collection == 0) {
    select->free(select);
    return scope.Close(v8::ThrowException(err));
  }

  // extract the where clause
  v8::Handle<v8::Value> whereArg = argv[4];
  TRI_qry_where_t* where = 0;

  if (! whereArg->IsNull()) {
    where = UnwrapWhere(whereArg->ToObject());

    if (where == 0) {
      select->free(select);
      return scope.Close(v8::ThrowException(v8::String::New("corrupted where")));
    }
  }

  // extract the skip value
  TRI_voc_size_t skip = 0;
  v8::Handle<v8::Value> skipArg = argv[5];

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
  v8::Handle<v8::Value> limitArg = argv[6];

  if (limitArg->IsNull()) {
    limit = TRI_QRY_NO_LIMIT;
  }
  else {
    double l = TRI_ObjectToDouble(limitArg);

    limit = (TRI_voc_ssize_t) l;
  }

  // create the query
  TRI_query_t* query = TRI_CreateQuery(select,
                                       where,
                                       NULL, 
                                       name.c_str(), 
                                       collection->_collection);

  query->_skip = skip;
  query->_limit = limit;

  select->free(select);

  // wrap it up
  return scope.Close(WrapQuery(query));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a query
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_ExecuteAql (v8::Arguments const& argv) {
  v8::HandleScope scope;

  TRI_v8_global_t* v8g;

  v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();

  v8::Handle<v8::Value> err;
  TRI_rc_cursor_t* cursor = ExecuteQuery(argv.Holder(), &err);

  if (cursor == 0) {
    return v8::ThrowException(err);
  }

  return scope.Close(WrapCursor(cursor));
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                        AQL CURSOR
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                              javascript functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the next document
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_NextCursor (v8::Arguments const& argv) {
  v8::HandleScope scope;
  v8::TryCatch tryCatch;

  TRI_v8_global_t* v8g;

  v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();

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

  TRI_qry_select_t* select = cursor->_select;

  v8::Handle<v8::Value> value;
  bool ok = select->toJavaScript(select, next, (void*) &value);

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
/// @brief returns the next document reference
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_NextRefCursor (v8::Arguments const& argv) {
  v8::HandleScope scope;
  v8::TryCatch tryCatch;

  TRI_v8_global_t* v8g;

  v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();

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
/// @brief uses the next document
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_UseNextCursor (v8::Arguments const& argv) {
  v8::HandleScope scope;
  v8::TryCatch tryCatch;

  TRI_v8_global_t* v8g;

  v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();

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
/// @brief checks if the cursor is exhausted
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_HasNextCursor (v8::Arguments const& argv) {
  v8::HandleScope scope;
  TRI_v8_global_t* v8g;

  v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();

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
///
/// @FUN{count()}
///
/// The @FN{count} operator counts the number of document in the result set and
/// returns that number. The @FN{count} operator ignores any limits and returns
/// the total number of documents found.
///
/// @verbinclude fluent24
///
/// @FUN{count(@LIT{true})}
///
/// If the result set was limited by the @FN{limit} operator or documents were
/// skiped using the @FN{skip} operator, the @FN{count} operator with argument
/// @LIT{true} will use the number of elements in the final result set - after
/// applying @FN{limit} and @FN{skip}.
///
/// @verbinclude fluent25
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
/// @verbinclude fluent10
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
/// @verbinclude fluent14
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
/// Returns the collection parameter.
///
/// @verbinclude fluent19
///
/// @FUN{parameter(@FA{parameter-array})}
///
/// Changes the collection parameter.
///
/// @verbinclude fluent20
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

      TRI_voc_size_t syncAfterObjects = sim->base.base._syncAfterObjects;
      TRI_voc_size_t syncAfterBytes = sim->base.base._syncAfterBytes;
      double syncAfterTime = sim->base.base._syncAfterTime;

      TRI_UnlockCondition(&sim->_journalsCondition);

      bool error;

      // extract sync after objects
      if (po->Has(v8g->SyncAfterObjectsKey)) {
        syncAfterObjects = TRI_ObjectToDouble(po->Get(v8g->SyncAfterObjectsKey), error);

        if (error || syncAfterObjects < 0.0) {
          return scope.Close(v8::ThrowException(v8::String::New("<parameter>.syncAfterObjects must be a number")));
        }
      }

      // extract sync after bytes
      if (po->Has(v8g->SyncAfterBytesKey)) {
        syncAfterBytes = TRI_ObjectToDouble(po->Get(v8g->SyncAfterBytesKey), error);

        if (error || syncAfterBytes < 0.0) {
          return scope.Close(v8::ThrowException(v8::String::New("<parameter>.syncAfterBytes must be a number")));
        }
      }

      // extract sync after times
      if (po->Has(v8g->SyncAfterTimeKey)) {
        syncAfterTime = TRI_ObjectToDouble(po->Get(v8g->SyncAfterTimeKey), error);

        if (error || syncAfterTime < 0.0) {
          return scope.Close(v8::ThrowException(v8::String::New("<parameter>.syncAfterTime must be a non-negative number")));
        }
      }

      // try to write new parameter to file
      TRI_LockCondition(&sim->_journalsCondition);

      sim->base.base._syncAfterObjects = syncAfterObjects;
      sim->base.base._syncAfterBytes = syncAfterBytes;
      sim->base.base._syncAfterTime = syncAfterTime;

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
    TRI_voc_size_t syncAfterObjects = sim->base.base._syncAfterObjects;
    TRI_voc_size_t syncAfterBytes = sim->base.base._syncAfterBytes;
    double syncAfterTime = sim->base.base._syncAfterTime;

    TRI_UnlockCondition(&sim->_journalsCondition);

    result->Set(v8g->SyncAfterObjectsKey, v8::Number::New(syncAfterObjects));
    result->Set(v8g->SyncAfterBytesKey, v8::Number::New(syncAfterBytes));
    result->Set(v8g->SyncAfterTimeKey, v8::Number::New(syncAfterTime));
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
      || key == "hasOwnProperties"
      || key == "COMPLETIONS"
      || key == "PRINT"
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
      || key == "hasOwnProperties"
      || key == "COMPLETIONS"
      || key == "PRINT"
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

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ShapedJSON
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief selects a attribute from the shaped json
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> MapWrappedShapedJson (v8::Local<v8::String> name,
                                            const v8::AccessorInfo& info) {
  v8::HandleScope scope;

  // get shaped json
  TRI_shaped_json_t* document = UnwrapClass<TRI_shaped_json_t>(info.Holder(), WRP_SHAPED_JSON_TYPE);
  if (document == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("corrupted shaped json")));
  }

  // convert the JavaScript string to a string
  string key = TRI_ObjectToString(name);
  
  
  if (key == "") {
    return scope.Close(v8::ThrowException(v8::String::New("name must not be empty")));
  }  
  
  if (key == "toString" || key == "toJSON" || key == "PRINT" || key[0] == '_') {
    return v8::Handle<v8::Value>();
  }
  
  if (key == "valueOf") {
    return v8::Handle<v8::Value>(v8::String::New(""));
  }

  // get collection
  if (info.Holder()->InternalFieldCount() < SLOT_QUERY) {
    return scope.Close(v8::ThrowException(v8::String::New("missing collection")));
  }
  TRI_vocbase_col_t* collection = static_cast<TRI_vocbase_col_t*>(v8::Handle<v8::External>::Cast(info.Holder()->GetInternalField(SLOT_QUERY))->Value());
  if (collection == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("corrupted shaped json")));
  }
  
  // get shape accessor
  TRI_shaper_t* shaper = collection->_collection->_shaper;
  TRI_shape_pid_t pid = shaper->findAttributePathByName(shaper, key.c_str()); 
  TRI_shape_sid_t sid = document->_sid;    
  TRI_shape_access_t* acc = TRI_ShapeAccessor(shaper, sid, pid);

  if (acc == NULL || acc->_shape == NULL) {
    return scope.Close(v8::ThrowException(v8::String::New("key not found")));
  }

  TRI_shape_t const* shape = acc->_shape;
  
//  if (acc->_shape->_sid != shaper->_sidNumber) {
//    return scope.Close(v8::ThrowException(v8::String::New("error in sid")));
//  }

  // convert to v8 value
  TRI_shaped_json_t json;
  if (TRI_ExecuteShapeAccessor(acc, document, &json)) {    
    return scope.Close(TRI_JsonShapeData(shaper, shape, json._data.data, json._data.length));
  }

  return scope.Close(v8::ThrowException(v8::String::New("error")));  
}

////////////////////////////////////////////////////////////////////////////////
/// @brief selects the keys from the shaped json
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Array> KeysOfShapedJson (const v8::AccessorInfo& info) {
  v8::HandleScope scope;
  v8::Handle<v8::Array> result = v8::Array::New();
  
  // get shaped json
  TRI_shaped_json_t* document = UnwrapClass<TRI_shaped_json_t>(info.Holder(), WRP_SHAPED_JSON_TYPE);
  if (document == 0) {
    return scope.Close(result);
  }

  // get collection
  if (info.Holder()->InternalFieldCount() < SLOT_QUERY) {
    return scope.Close(result);
  }
  TRI_vocbase_col_t* collection = static_cast<TRI_vocbase_col_t*>(v8::Handle<v8::External>::Cast(info.Holder()->GetInternalField(SLOT_QUERY))->Value());
  if (collection == 0) {
    return scope.Close(result);
  }
  
  
  TRI_shaper_t* shaper = collection->_collection->_shaper;  
  TRI_shape_t const* shape = shaper->lookupShapeId(shaper, document->_sid);

  // check for array shape
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
    if (att) result->Set(count++, v8::String::New(att));        
  }
 
  return scope.Close(result);  
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check if a property is present and if present, get its attributes.
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Integer> PropertyQueryShapedJson (v8::Local<v8::String> name, const v8::AccessorInfo& info) {
  v8::HandleScope scope;

  // get shaped json
  TRI_shaped_json_t* document = UnwrapClass<TRI_shaped_json_t>(info.Holder(), WRP_SHAPED_JSON_TYPE);
  if (document == 0) {
    return scope.Close(v8::Handle<v8::Integer>());
  }

  // convert the JavaScript string to a string
  string key = TRI_ObjectToString(name);
  
  
  if (key == "") {
    return scope.Close(v8::Handle<v8::Integer>());
  }  
  
  // get collection
  if (info.Holder()->InternalFieldCount() < SLOT_QUERY) {
    return scope.Close(v8::Handle<v8::Integer>(v8::Integer::New(v8::None)));
  }
  TRI_vocbase_col_t* collection = static_cast<TRI_vocbase_col_t*>(v8::Handle<v8::External>::Cast(info.Holder()->GetInternalField(SLOT_QUERY))->Value());
  if (collection == 0) {
    return scope.Close(v8::Handle<v8::Integer>());
  }
  
  // get shape accessor
  TRI_shaper_t* shaper = collection->_collection->_shaper;
  TRI_shape_pid_t pid = shaper->findAttributePathByName(shaper, key.c_str()); 
  TRI_shape_sid_t sid = document->_sid;    
  TRI_shape_access_t* acc = TRI_ShapeAccessor(shaper, sid, pid);

  if (acc == NULL || acc->_shape == NULL) {
    // key not found
    return scope.Close(v8::Handle<v8::Integer>());
  }

  return scope.Close(v8::Handle<v8::Integer>(v8::Integer::New(1)));
  
//  
//  TRI_shape_t const* shape = acc->_shape;
//  
//  TRI_shaped_json_t json;
//  if (TRI_ExecuteShapeAccessor(acc, document, &json)) {    
//    return scope.Close(v8::Handle<v8::Integer>(v8::Integer::New(1)));
//  }
//
//  // error
//  return scope.Close(v8::Handle<v8::Integer>());
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
// --SECTION--                                                            MODULE
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @page JavaScriptFuncIndex JavaScript Function Index
///
/// @section JSFDatabaseSelection Database Selection
///
/// - @ref MapGetVocBase "db".@FA{database}
///
/// - @ref MapGetVocBase "edges".@FA{database}
///
/// @section JSFDatabases Database Functions
///
/// - @ref JS_ParameterVocbaseCol "parameter"
///
/// @subsection JSFDocument Database Document Functions
///
/// - @ref JS_DeleteVocbaseCol "delete"
/// - @ref JS_ReplaceVocbaseCol "replace"
/// - @ref JS_SaveVocbaseCol "save"
/// - @ref JS_SaveEdgesCol "save" for edges
///
/// @subsection JSFIndex Database Index Functions
///
/// - @ref JS_DropIndexVocbaseCol "dropIndex"
/// - @ref JS_EnsureGeoIndexVocbaseCol "ensureGeoIndex"
/// - @ref JS_GetIndexesVocbaseCol "getIndexes"
///
/// @section JSFQueries Query Functions
///
/// @subsection JSFQueryBuilding Query Building Functions
///
/// - @ref JS_AllQuery "all"
/// - @ref JS_DistanceQuery "distance"
/// - @ref JS_DocumentQuery "document"
/// - @ref JS_GeoQuery "geo"
/// - @ref JS_LimitQuery "limit"
/// - @ref JS_NearQuery "near"
/// - @ref JS_SelectQuery "select"
/// - @ref JS_SkipQuery "skip"
/// - @ref JS_WithinQuery "within"
///
/// @subsection JSFQueryExecuting Query Execution Functions
///
/// - @ref JS_CountQuery "count"
/// - @ref JS_ExplainQuery "explain"
/// - @ref JS_HasNextQuery "hasNext"
/// - @ref JS_NextQuery "next"
/// - @ref JS_NextRefQuery "nextRef"
/// - @ref JS_ShowQuery "show"
///
/// @section JSFGlobal Global Functions
///
/// - @ref JS_Execute "execute"
/// - @ref JS_Load "load"
/// - @ref JS_LogLevel "logLevel"
/// - @ref JS_Output "output"
/// - @ref JSF_print "print"
/// - @ref JS_ProcessCsvFile "processCsvFile"
/// - @ref JS_ProcessJsonFile "processJsonFile"
/// - @ref JS_Read "read"
/// - @ref JS_Time "time"
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @page JavaScriptFunc JavaScript Functions
///
/// @section JSFDatabaseSelection Database Selection
///
/// @ref MapGetVocBase "db".@FA{database}
///
/// @section JSFDatabases Database Functions
///
/// @copydetails JS_ParameterVocbaseCol
///
/// @subsection JSFDocument Database Document Functions
///
/// @copydetails JS_DeleteVocbaseCol
///
/// @copydetails JS_ReplaceVocbaseCol
///
/// @copydetails JS_SaveVocbaseCol
///
/// @copydetails JS_SaveEdgesCol
///
/// @subsection JSFIndex Database Index Functions
///
/// @copydetails JS_DropIndexVocbaseCol
///
/// @copydetails JS_EnsureGeoIndexVocbaseCol
///
/// @copydetails JS_GetIndexesVocbaseCol
///
/// @section JSFQueries Query Functions
///
/// @subsection JSFQueryBuilding Query Building Functions
///
/// @copydetails JS_AllQuery
///
/// @copydetails JS_DistanceQuery
///
/// @copydetails JS_DocumentQuery
///
/// @copydetails JS_GeoQuery
///
/// @copydetails JS_LimitQuery
///
/// @copydetails JS_NearQuery
///
/// @copydetails JS_SelectQuery
///
/// @copydetails JS_SkipQuery
///
/// @copydetails JS_WithinQuery
///
/// @subsection JSFQueryExecuting Query Execution Functions
///
/// @copydetails JS_CountQuery
///
/// @copydetails JS_ExplainQuery
///
/// @copydetails JS_HasNextQuery
///
/// @copydetails JS_NextQuery
///
/// @copydetails JS_NextRefQuery
///
/// @copydetails JS_ShowQuery
///
/// @section JSFGlobal Global Functions
///
/// @copydetails JS_Execute
///
/// @copydetails JS_Load
///
/// @copydetails JS_LogLevel
///
/// @copydetails JS_Output
///
/// @copydetails JSF_print
///
/// @copydetails JS_ProcessCsvFile
///
/// @copydetails JS_ProcessJsonFile
///
/// @copydetails JS_Read
///
/// @copydetails JS_Time
////////////////////////////////////////////////////////////////////////////////

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

  result->Set(v8::String::New("_path"), v8::String::New(database->_path));

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

  result->Set(v8::String::New("_path"), v8::String::New(database->_path));

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

  result->Set(v8::String::New("_name"), v8::String::New(collection->_name));
  result->Set(v8::String::New("_id"), v8::Number::New(collection->_cid));

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

  result->Set(v8::String::New("_name"), v8::String::New(collection->_name));
  result->Set(v8::String::New("_id"), v8::Number::New(collection->_cid));

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief wraps a TRI_shaped_json_t
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Object> TRI_WrapShapedJson (TRI_shaped_json_t const* shaped_json, TRI_vocbase_col_t const* collection) {
  TRI_v8_global_t* v8g;
  v8::HandleScope scope;

  v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();
  v8::Handle<v8::Object> result = WrapClass(v8g->ShapedJsonTempl, 
                                            WRP_SHAPED_JSON_TYPE,
                                            const_cast<TRI_shaped_json_t*>(shaped_json));
  
  result->SetInternalField(SLOT_QUERY, v8::External::New(const_cast<TRI_vocbase_col_t*>(collection)));

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

  v8::Handle<v8::String> AllFuncName = v8::Persistent<v8::String>::New(v8::String::New("all"));
  v8::Handle<v8::String> CollectionsFuncName = v8::Persistent<v8::String>::New(v8::String::New("_collections"));
  v8::Handle<v8::String> CompletionsFuncName = v8::Persistent<v8::String>::New(v8::String::New("COMPLETIONS"));
  v8::Handle<v8::String> CountFuncName = v8::Persistent<v8::String>::New(v8::String::New("count"));
  v8::Handle<v8::String> DeleteFuncName = v8::Persistent<v8::String>::New(v8::String::New("delete"));
  v8::Handle<v8::String> DistanceFuncName = v8::Persistent<v8::String>::New(v8::String::New("distance"));
  v8::Handle<v8::String> DocumentFuncName = v8::Persistent<v8::String>::New(v8::String::New("document"));
  v8::Handle<v8::String> DropIndexFuncName = v8::Persistent<v8::String>::New(v8::String::New("dropIndex"));
  v8::Handle<v8::String> EdgesFuncName = v8::Persistent<v8::String>::New(v8::String::New("edges"));
  v8::Handle<v8::String> EnsureGeoIndexFuncName = v8::Persistent<v8::String>::New(v8::String::New("ensureGeoIndex"));
  v8::Handle<v8::String> ExecuteFuncName = v8::Persistent<v8::String>::New(v8::String::New("execute"));
  v8::Handle<v8::String> ExplainFuncName = v8::Persistent<v8::String>::New(v8::String::New("explain"));
  v8::Handle<v8::String> FiguresFuncName = v8::Persistent<v8::String>::New(v8::String::New("figures"));
  v8::Handle<v8::String> GeoFuncName = v8::Persistent<v8::String>::New(v8::String::New("geo"));
  v8::Handle<v8::String> GetIndexesFuncName = v8::Persistent<v8::String>::New(v8::String::New("getIndexes"));
  v8::Handle<v8::String> HasNextFuncName = v8::Persistent<v8::String>::New(v8::String::New("hasNext"));
  v8::Handle<v8::String> InEdgesFuncName = v8::Persistent<v8::String>::New(v8::String::New("inEdges"));
  v8::Handle<v8::String> LimitFuncName = v8::Persistent<v8::String>::New(v8::String::New("limit"));
  v8::Handle<v8::String> LoadFuncName = v8::Persistent<v8::String>::New(v8::String::New("load"));
  v8::Handle<v8::String> NearFuncName = v8::Persistent<v8::String>::New(v8::String::New("near"));
  v8::Handle<v8::String> NextFuncName = v8::Persistent<v8::String>::New(v8::String::New("next"));
  v8::Handle<v8::String> NextRefFuncName = v8::Persistent<v8::String>::New(v8::String::New("nextRef"));
  v8::Handle<v8::String> OptimiseFuncName = v8::Persistent<v8::String>::New(v8::String::New("optimise"));
  v8::Handle<v8::String> OutEdgesFuncName = v8::Persistent<v8::String>::New(v8::String::New("outEdges"));
  v8::Handle<v8::String> ParameterFuncName = v8::Persistent<v8::String>::New(v8::String::New("parameter"));
  v8::Handle<v8::String> ReplaceFuncName = v8::Persistent<v8::String>::New(v8::String::New("replace"));
  v8::Handle<v8::String> SaveFuncName = v8::Persistent<v8::String>::New(v8::String::New("save"));
  v8::Handle<v8::String> SelectFuncName = v8::Persistent<v8::String>::New(v8::String::New("select"));
  v8::Handle<v8::String> ShowFuncName = v8::Persistent<v8::String>::New(v8::String::New("show"));
  v8::Handle<v8::String> SkipFuncName = v8::Persistent<v8::String>::New(v8::String::New("skip"));
  v8::Handle<v8::String> StatusFuncName = v8::Persistent<v8::String>::New(v8::String::New("status"));
  v8::Handle<v8::String> ToArrayFuncName = v8::Persistent<v8::String>::New(v8::String::New("toArray"));
  v8::Handle<v8::String> UseNextFuncName = v8::Persistent<v8::String>::New(v8::String::New("useNext"));
  v8::Handle<v8::String> WithinFuncName = v8::Persistent<v8::String>::New(v8::String::New("within"));

  v8::Handle<v8::String> DocumentWrappedFuncName = v8::Persistent<v8::String>::New(v8::String::New("document_wrapped"));
  
  // .............................................................................
  // query types
  // .............................................................................

  v8g->CollectionQueryType = v8::Persistent<v8::String>::New(v8::String::New("collection"));

  // .............................................................................
  // keys
  // .............................................................................

  v8g->JournalSizeKey = v8::Persistent<v8::String>::New(v8::String::New("journalSize"));
  v8g->SyncAfterBytesKey = v8::Persistent<v8::String>::New(v8::String::New("syncAfterBytes"));
  v8g->SyncAfterObjectsKey = v8::Persistent<v8::String>::New(v8::String::New("syncAfterObjects"));
  v8g->SyncAfterTimeKey = v8::Persistent<v8::String>::New(v8::String::New("syncAfterTime"));

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
  
  rt->SetNamedPropertyHandler(MapWrappedShapedJson,     // NamedPropertyGetter,
                              0,                        // NamedPropertySetter setter = 0,
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

  rt->Set(AllFuncName, v8::FunctionTemplate::New(JS_AllQuery)); // TO BE DELETED
  rt->Set(DocumentFuncName, v8::FunctionTemplate::New(JS_DocumentQuery)); // TO BE DELETED
  rt->Set(EdgesFuncName, v8::FunctionTemplate::New(JS_EdgesQuery));
  rt->Set(GeoFuncName, v8::FunctionTemplate::New(JS_GeoQuery));
  rt->Set(InEdgesFuncName, v8::FunctionTemplate::New(JS_InEdgesQuery));
  rt->Set(NearFuncName, v8::FunctionTemplate::New(JS_NearQuery));
  rt->Set(OutEdgesFuncName, v8::FunctionTemplate::New(JS_OutEdgesQuery));
  rt->Set(SelectFuncName, v8::FunctionTemplate::New(JS_SelectQuery));
  rt->Set(WithinFuncName, v8::FunctionTemplate::New(JS_WithinQuery));

  rt->Set(CountFuncName, v8::FunctionTemplate::New(JS_CountVocbaseCol));
  rt->Set(DeleteFuncName, v8::FunctionTemplate::New(JS_DeleteVocbaseCol));
  rt->Set(DropIndexFuncName, v8::FunctionTemplate::New(JS_DropIndexVocbaseCol));
  rt->Set(EnsureGeoIndexFuncName, v8::FunctionTemplate::New(JS_EnsureGeoIndexVocbaseCol));
  rt->Set(FiguresFuncName, v8::FunctionTemplate::New(JS_FiguresVocbaseCol));
  rt->Set(GetIndexesFuncName, v8::FunctionTemplate::New(JS_GetIndexesVocbaseCol));
  rt->Set(LoadFuncName, v8::FunctionTemplate::New(JS_LoadVocbaseCol));
  rt->Set(ParameterFuncName, v8::FunctionTemplate::New(JS_ParameterVocbaseCol));
  rt->Set(ReplaceFuncName, v8::FunctionTemplate::New(JS_ReplaceVocbaseCol));
  rt->Set(SaveFuncName, v8::FunctionTemplate::New(JS_SaveVocbaseCol));
  rt->Set(StatusFuncName, v8::FunctionTemplate::New(JS_StatusVocbaseCol));
  
  rt->Set(DocumentWrappedFuncName, v8::FunctionTemplate::New(JS_DocumentQueryWrapped));
  
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
  rt->Set(DocumentFuncName, v8::FunctionTemplate::New(JS_DocumentQuery));
  rt->Set(EdgesFuncName, v8::FunctionTemplate::New(JS_EdgesQuery));
  rt->Set(GeoFuncName, v8::FunctionTemplate::New(JS_GeoQuery));
  rt->Set(InEdgesFuncName, v8::FunctionTemplate::New(JS_InEdgesQuery));
  rt->Set(NearFuncName, v8::FunctionTemplate::New(JS_NearQuery));
  rt->Set(OutEdgesFuncName, v8::FunctionTemplate::New(JS_OutEdgesQuery));
  rt->Set(SelectFuncName, v8::FunctionTemplate::New(JS_SelectQuery));
  rt->Set(WithinFuncName, v8::FunctionTemplate::New(JS_WithinQuery));

  rt->Set(CountFuncName, v8::FunctionTemplate::New(JS_CountVocbaseCol));
  rt->Set(DeleteFuncName, v8::FunctionTemplate::New(JS_DeleteVocbaseCol));
  rt->Set(DropIndexFuncName, v8::FunctionTemplate::New(JS_DropIndexVocbaseCol));
  rt->Set(EnsureGeoIndexFuncName, v8::FunctionTemplate::New(JS_EnsureGeoIndexVocbaseCol));
  rt->Set(FiguresFuncName, v8::FunctionTemplate::New(JS_FiguresVocbaseCol));
  rt->Set(GetIndexesFuncName, v8::FunctionTemplate::New(JS_GetIndexesVocbaseCol));
  rt->Set(LoadFuncName, v8::FunctionTemplate::New(JS_LoadVocbaseCol));
  rt->Set(ParameterFuncName, v8::FunctionTemplate::New(JS_ParameterVocbaseCol));
  rt->Set(ReplaceFuncName, v8::FunctionTemplate::New(JS_ReplaceEdgesCol));
  rt->Set(SaveFuncName, v8::FunctionTemplate::New(JS_SaveEdgesCol));
  rt->Set(StatusFuncName, v8::FunctionTemplate::New(JS_StatusVocbaseCol));

  v8g->EdgesColTempl = v8::Persistent<v8::ObjectTemplate>::New(rt);

  // must come after SetInternalFieldCount
  context->Global()->Set(v8::String::New("AvocadoEdgesCollection"),
                         ft->GetFunction());

  // .............................................................................
  // generate the fluent query template
  // .............................................................................

  ft = v8::FunctionTemplate::New();
  ft->SetClassName(v8::String::New("AvocadoFluentQuery"));

  rt = ft->InstanceTemplate();
  rt->SetInternalFieldCount(SLOT_END);

  rt->Set(AllFuncName, v8::FunctionTemplate::New(JS_AllQuery));
  rt->Set(CountFuncName, v8::FunctionTemplate::New(JS_CountQuery));
  rt->Set(DistanceFuncName, v8::FunctionTemplate::New(JS_DistanceQuery));
  rt->Set(DocumentFuncName, v8::FunctionTemplate::New(JS_DocumentQuery));
  rt->Set(EdgesFuncName, v8::FunctionTemplate::New(JS_EdgesQuery));
  rt->Set(ExplainFuncName, v8::FunctionTemplate::New(JS_ExplainQuery));
  rt->Set(GeoFuncName, v8::FunctionTemplate::New(JS_GeoQuery));
  rt->Set(HasNextFuncName, v8::FunctionTemplate::New(JS_HasNextQuery));
  rt->Set(InEdgesFuncName, v8::FunctionTemplate::New(JS_InEdgesQuery));
  rt->Set(LimitFuncName, v8::FunctionTemplate::New(JS_LimitQuery));
  rt->Set(NearFuncName, v8::FunctionTemplate::New(JS_NearQuery));
  rt->Set(NextFuncName, v8::FunctionTemplate::New(JS_NextQuery));
  rt->Set(NextRefFuncName, v8::FunctionTemplate::New(JS_NextRefQuery));
  rt->Set(OptimiseFuncName, v8::FunctionTemplate::New(JS_OptimiseQuery));
  rt->Set(OutEdgesFuncName, v8::FunctionTemplate::New(JS_OutEdgesQuery));
  rt->Set(SelectFuncName, v8::FunctionTemplate::New(JS_SelectQuery));
  rt->Set(ShowFuncName, v8::FunctionTemplate::New(JS_ShowQuery));
  rt->Set(SkipFuncName, v8::FunctionTemplate::New(JS_SkipQuery));
  rt->Set(ToArrayFuncName, v8::FunctionTemplate::New(JS_ToArrayQuery));
  rt->Set(WithinFuncName, v8::FunctionTemplate::New(JS_WithinQuery));

  v8g->FluentQueryTempl = v8::Persistent<v8::ObjectTemplate>::New(rt);

  // must come after SetInternalFieldCount
  context->Global()->Set(v8::String::New("AvocadoFluentQuery"),
                         ft->GetFunction());

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

  // .............................................................................
  // generate the cursor template
  // .............................................................................

  ft = v8::FunctionTemplate::New();
  ft->SetClassName(v8::String::New("AvocadoCursor"));

  rt = ft->InstanceTemplate();
  rt->SetInternalFieldCount(2);

  rt->Set(HasNextFuncName, v8::FunctionTemplate::New(JS_HasNextCursor));
  rt->Set(NextFuncName, v8::FunctionTemplate::New(JS_NextCursor));
  rt->Set(NextRefFuncName, v8::FunctionTemplate::New(JS_NextRefCursor));
  rt->Set(UseNextFuncName, v8::FunctionTemplate::New(JS_UseNextCursor));

  v8g->CursorTempl = v8::Persistent<v8::ObjectTemplate>::New(rt);

  // must come after SetInternalFieldCount
  context->Global()->Set(v8::String::New("AvocadoCursor"),
                         ft->GetFunction());

  // .............................................................................
  // create the global functions
  // .............................................................................

  context->Global()->Set(v8::String::New("AQL_WHERE_BOOLEAN"),
                         v8::FunctionTemplate::New(JS_WhereBooleanAql)->GetFunction(),
                         v8::ReadOnly);

  context->Global()->Set(v8::String::New("AQL_WHERE_GENERAL"),
                         v8::FunctionTemplate::New(JS_WhereGeneralAql)->GetFunction(),
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

  context->Global()->Set(v8::String::New("AQL_PREPARE"),
                         v8::FunctionTemplate::New(JS_PrepareAql)->GetFunction(),
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
