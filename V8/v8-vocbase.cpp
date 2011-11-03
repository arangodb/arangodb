////////////////////////////////////////////////////////////////////////////////
/// @brief V8-vocbase bridge
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2011 triagens GmbH, Cologne, Germany
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
/// @author Copyright 2011, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "v8-vocbase.h"

#define TRI_WITHIN_C
#include <Basics/conversions.h>
#include <Basics/csv.h>
#include <Basics/logging.h>
#include <Basics/strings.h>

#include <VocBase/query.h>
#include <VocBase/simple-collection.h>
#include <VocBase/vocbase.h>

#include "ShapedJson/shaped-json.h"

#include "V8/v8-shell.h"
#undef TRI_WITHIN_C

#include <regex.h>

#include <map>
#include <string>

using namespace std;

// -----------------------------------------------------------------------------
// --SECTION--                                                     private types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBasePrivate VocBase (Private)
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief slot for a "C++ class"
////////////////////////////////////////////////////////////////////////////////

static int SLOT_CLASS = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief slot for a "query"
////////////////////////////////////////////////////////////////////////////////

static int SLOT_QUERY = 1;

////////////////////////////////////////////////////////////////////////////////
/// @brief slot for a "result set"
////////////////////////////////////////////////////////////////////////////////

static int SLOT_RESULT_SET = 2;

////////////////////////////////////////////////////////////////////////////////
/// @brief end marker
////////////////////////////////////////////////////////////////////////////////

static int SLOT_END = 3;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBasePrivate VocBase (Private)
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief query mapping for weak pointers
////////////////////////////////////////////////////////////////////////////////

static map< TRI_query_t*, v8::Persistent<v8::Value> > JSQueries;

////////////////////////////////////////////////////////////////////////////////
/// @brief result set mapping for weak pointers
////////////////////////////////////////////////////////////////////////////////

static map< TRI_result_set_t*, v8::Persistent<v8::Value> > JSResultSets;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                       JAVASCRIPT OBJECT TEMPLATES
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBasePrivate VocBase (Private)
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief query class template
////////////////////////////////////////////////////////////////////////////////

static v8::Persistent<v8::ObjectTemplate> QueryTempl;

////////////////////////////////////////////////////////////////////////////////
/// @brief TRI_vocbase_col_t template
////////////////////////////////////////////////////////////////////////////////

static v8::Persistent<v8::ObjectTemplate> VocbaseColTempl;

////////////////////////////////////////////////////////////////////////////////
/// @brief TRI_vocbase_t class template
////////////////////////////////////////////////////////////////////////////////

static v8::Persistent<v8::ObjectTemplate> VocbaseTempl;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                         JAVASCRIPT FUNCTION NAMES
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBasePrivate VocBase (Private)
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief "output" function name
////////////////////////////////////////////////////////////////////////////////

static v8::Persistent<v8::String> OutputFuncName;

////////////////////////////////////////////////////////////////////////////////
/// @brief "printQuery" function name
////////////////////////////////////////////////////////////////////////////////

static v8::Persistent<v8::String> PrintQueryFuncName;

////////////////////////////////////////////////////////////////////////////////
/// @brief "toString" function name
////////////////////////////////////////////////////////////////////////////////

static v8::Persistent<v8::String> ToStringFuncName;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                              JAVASCRIPT KEY NAMES
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBasePrivate VocBase (Private)
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief "journalSize" key name
////////////////////////////////////////////////////////////////////////////////

static v8::Persistent<v8::String> JournalSizeKey;

////////////////////////////////////////////////////////////////////////////////
/// @brief "syncAfterBytes" key name
////////////////////////////////////////////////////////////////////////////////

static v8::Persistent<v8::String> SyncAfterBytesKey;

////////////////////////////////////////////////////////////////////////////////
/// @brief "syncAfterObjects" key name
////////////////////////////////////////////////////////////////////////////////

static v8::Persistent<v8::String> SyncAfterObjectsKey;

////////////////////////////////////////////////////////////////////////////////
/// @brief "syncAfterTime" key name
////////////////////////////////////////////////////////////////////////////////

static v8::Persistent<v8::String> SyncAfterTimeKey;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                               SPECIAL QUERY TYPES
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBasePrivate VocBase (Private)
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief "collection" query type
////////////////////////////////////////////////////////////////////////////////

static v8::Persistent<v8::String> CollectionQueryType;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                               REGULAR EXPRESSIONS
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBasePrivate VocBase (Private)
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief document identifier as collection-id:document-id
////////////////////////////////////////////////////////////////////////////////

static regex_t DocumentIdRegex;

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
/// @addtogroup VocBasePrivate VocBase (Private)
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief wraps a C++ into a v8::Object
////////////////////////////////////////////////////////////////////////////////

template<class T>
static v8::Handle<v8::Object> WrapClass (v8::Persistent<v8::ObjectTemplate> classTempl,
                                         T* y) {

  // handle scope for temporary handles
  v8::HandleScope scope;

  // create the new handle to return, and set its template type
  v8::Handle<v8::Object> result = classTempl->NewInstance();
  v8::Handle<v8::External> classPtr = v8::External::New(y);

  // point the 0 index Field to the c++ pointer for unwrapping later
  result->SetInternalField(SLOT_CLASS, classPtr);

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unwraps a C++ class given a v8::Object
////////////////////////////////////////////////////////////////////////////////

template<class T>
static T* UnwrapClass (v8::Handle<v8::Object> obj) {
  v8::Handle<v8::External> field;
  field = v8::Handle<v8::External>::Cast(obj->GetInternalField(SLOT_CLASS));
  void* ptr = field->Value();

  return static_cast<T*>(ptr);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if argument is a document identifier
////////////////////////////////////////////////////////////////////////////////

static bool IsDocumentId (v8::Handle<v8::Value> arg, TRI_voc_cid_t& cid, TRI_voc_did_t& did) {
  if (arg->IsNumber()) {
    cid = 0;
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

  if (regexec(&DocumentIdRegex, s, sizeof(matches) / sizeof(matches[0]), matches, 0) == 0) {
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
  TRI_vocbase_col_t const* col = UnwrapClass<TRI_vocbase_col_t>(collection);

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

    LOG_INFO("collection created");
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
// --SECTION--                                              javascript functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBasePrivate VocBase (Private)
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief calls the "toString" function
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_PrintUsingToString (v8::Arguments const& argv) {
  v8::HandleScope scope;

  v8::Handle<v8::Object> self = argv.Holder();
  v8::Handle<v8::Function> toString = v8::Handle<v8::Function>::Cast(self->Get(ToStringFuncName));

  v8::Handle<v8::Value> args1[] = { self };
  v8::Handle<v8::Value> str = toString->Call(self, 1, args1);

  v8::Handle<v8::Function> output = v8::Handle<v8::Function>::Cast(self->CreationContext()->Global()->Get(OutputFuncName));

  v8::Handle<v8::Value> args2[] = { str, v8::String::New("\n") };
  output->Call(output, 2, args2);

  return scope.Close(v8::Undefined());
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
/// @addtogroup VocBasePrivate VocBase (Private)
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief weak reference callback for result sets
////////////////////////////////////////////////////////////////////////////////

static void WeakResultSetCallback (v8::Persistent<v8::Value> object, void* parameter) {
  TRI_result_set_t* rs = (TRI_result_set_t*) parameter;

  LOG_TRACE("weak-callback for result-set called");

  // find the persistent handle
  v8::Persistent<v8::Value> persistent = JSResultSets[rs];
  JSResultSets.erase(rs);

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
  if (JSResultSets.find(rs) == JSResultSets.end()) {
    v8::Persistent<v8::Value> persistent = v8::Persistent<v8::Value>::New(v8::External::New(rs));

    rsObject->SetInternalField(SLOT_RESULT_SET, persistent);

    JSResultSets[rs] = persistent;

    persistent.MakeWeak(rs, WeakResultSetCallback);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief weak reference callback for queries
////////////////////////////////////////////////////////////////////////////////

static void WeakQueryCallback (v8::Persistent<v8::Value> object, void* parameter) {
  TRI_query_t* query = (TRI_query_t*) parameter;

  LOG_TRACE("weak-callback for query called");

  // find the persistent handle
  v8::Persistent<v8::Value> persistent = JSQueries[query];
  JSQueries.erase(query);

  // dispose and clear the persistent handle
  persistent.Dispose();
  persistent.Clear();

  // and free the result set
  query->free(query);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stores a query in a javascript object
////////////////////////////////////////////////////////////////////////////////

static void StoreQuery (v8::Handle<v8::Object> queryObject,
                        TRI_query_t* query) {
  if (JSQueries.find(query) == JSQueries.end()) {
    v8::Persistent<v8::Value> persistent = v8::Persistent<v8::Value>::New(v8::External::New(query));

    queryObject->SetInternalField(SLOT_QUERY, persistent);
    queryObject->SetInternalField(SLOT_RESULT_SET, v8::Null());

    JSQueries[query] = persistent;

    persistent.MakeWeak(query, WeakQueryCallback);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts a query from a javascript object
////////////////////////////////////////////////////////////////////////////////

static TRI_query_t* ExtractQuery (v8::Handle<v8::Object> queryObject,
                                  v8::Handle<v8::String>* err) {

  // the internal fields QUERY and RESULT_SET must be present
  if (queryObject->InternalFieldCount() <= SLOT_RESULT_SET) {
    *err = v8::String::New("corrupted query");
    return 0;
  }

  v8::Handle<v8::Value> value = queryObject->GetInternalField(SLOT_QUERY);

  // .............................................................................
  // special case: the whole collection
  // .............................................................................

  if (value == CollectionQueryType) {
    TRI_vocbase_col_t const* collection = LoadCollection(queryObject, err);

    if (collection == 0) {
      return false;
    }

    TRI_query_t* query = TRI_CreateCollectionQuery(collection);

    StoreQuery(queryObject, query);

    return query;
  }

  // .............................................................................
  // standard case: a normal query
  // .............................................................................

  else {
    TRI_query_t* query = static_cast<TRI_query_t*>(v8::Handle<v8::External>::Cast(value)->Value());

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

static TRI_result_set_t* ExecuteQuery (v8::Handle<v8::Object> queryObject,
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

    TRI_query_t* query = ExtractQuery(self, err);

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

  if (value == CollectionQueryType) {
    TRI_vocbase_col_t const* col = UnwrapClass<TRI_vocbase_col_t>(self);

    if (col == 0) {
      return "[unknown collection]";
    }

    return "[collection \"" + string(col->_name) + "\"]";
  }

  // .............................................................................
  // standard case: a normal query
  // .............................................................................

  else {
    TRI_query_t* query = static_cast<TRI_query_t*>(v8::Handle<v8::External>::Cast(value)->Value());

    if (query == 0) {
      return "[corrupted query]";
    }

    return query->_string;
  }

  return TRI_DuplicateString("[corrupted]");
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                              javascript functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBasePrivate VocBase (Private)
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief calls the "printQuery" function
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_PrintQuery (v8::Arguments const& argv) {
  v8::HandleScope scope;

  v8::Handle<v8::Object> self = argv.Holder();
  v8::Handle<v8::Function> print = v8::Handle<v8::Function>::Cast(self->CreationContext()->Global()->Get(PrintQueryFuncName));

  v8::Handle<v8::Value> args[] = { self };
  print->Call(print, 1, args);

  return scope.Close(v8::Undefined());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a query into a fluent interface representation
///
/// <tt>show()</tt>
///
/// Shows the representation of the query.
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
/// <tt>count()</tt>
///
/// The "count" operator counts the number of document in the result set and
/// returns that number.
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_CountQuery (v8::Arguments const& argv) {
  v8::HandleScope scope;

  v8::Handle<v8::String> err;
  TRI_result_set_t* rs = ExecuteQuery(argv.Holder(), &err);

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
/// @brief explains how a query was executed
///
/// <tt>explain()</tt>
///
/// In order to optimise queries you need to know how the storage engine
/// executed that type of query. You can use the "explain()" operator to
/// see how a query was executed.
///
/// @verbinclude fluent9
///
/// The "explain" operator returns an object with the following attributes.
///
/// - cursor: describes how the result set was computed.
/// - scannedIndexEntries: how many index entries were scanned
/// - scannedDocuments: how many documents were scanned
/// - matchedDocuments: the sum of all matched documents in each step
/// - runtime: the runtime in seconds
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_ExplainQuery (v8::Arguments const& argv) {
  v8::HandleScope scope;

  v8::Handle<v8::String> err;
  TRI_result_set_t* rs = ExecuteQuery(argv.Holder(), &err);

  if (rs == 0) {
    return scope.Close(v8::ThrowException(err));
  }

  v8::Handle<v8::Object> result = v8::Object::New();

  result->Set(v8::String::New("cursor"), v8::String::New(rs->_info._cursor));
  result->Set(v8::String::New("scannedIndexEntries"), v8::Number::New(rs->_info._scannedIndexEntries));
  result->Set(v8::String::New("scannedDocuments"), v8::Number::New(rs->_info._scannedDocuments));
  result->Set(v8::String::New("matchedDocuments"), v8::Number::New(rs->_info._matchedDocuments));
  result->Set(v8::String::New("runtime"), v8::Number::New(rs->_info._runtime));

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks for the next result document
///
/// <tt>hasNext()</tt>
///
/// The "hasNext" operator returns true, if the cursor still has documents.
/// In this case the next document can be accessed using the "next" operator,
/// which will advance the cursor.
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_HasNextQuery (v8::Arguments const& argv) {
  v8::HandleScope scope;

  v8::Handle<v8::String> err;
  TRI_result_set_t* rs = ExecuteQuery(argv.Holder(), &err);

  if (rs == 0) {
    return scope.Close(v8::ThrowException(err));
  }

  return scope.Close(rs->hasNext(rs) ? v8::True() : v8::False());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the next result document
///
/// <tt>next()</tt>
///
/// If the "hasNext" operator returns true, if the cursor still has documents.
/// In this case the next document can be accessed using the "next" operator,
/// which will advance the cursor.
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_NextQuery (v8::Arguments const& argv) {
  v8::HandleScope scope;

  v8::Handle<v8::String> err;
  TRI_result_set_t* rs = ExecuteQuery(argv.Holder(), &err);

  if (rs == 0) {
    return scope.Close(v8::ThrowException(err));
  }

  if (rs->hasNext(rs)) {
    TRI_doc_collection_t* collection = rs->_container->_collection;
    TRI_shaper_t* shaper = collection->_shaper;

    TRI_voc_did_t did;
    TRI_json_t const* augmented;
    TRI_shaped_json_t* element = rs->next(rs, &did, &augmented);

    v8::Handle<v8::Value> object = ObjectShapedJson(collection, did, shaper, element);

    if (augmented != NULL) {
      AugmentObject(object, augmented);
    }

    return scope.Close(object);
  }
  else {
    return scope.Close(v8::Undefined());
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a query and returns the result as array
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_ToArrayQuery (v8::Arguments const& argv) {
  v8::HandleScope scope;

  v8::Handle<v8::String> err;
  TRI_result_set_t* rs = ExecuteQuery(argv.Holder(), &err);

  if (rs == 0) {
    return scope.Close(v8::ThrowException(err));
  }

  return scope.Close(ArrayResultSet(rs));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns all elements
///
/// The "all" operator returns all documents of a collection.
///
/// <tt>all()</tt>
///
/// Selects all documents.
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_AllQuery (v8::Arguments const& argv) {
  v8::HandleScope scope;

  // extract the operand query
  v8::Handle<v8::Object> operand = argv.Holder();
  v8::Handle<v8::String> err;
  TRI_query_t* opQuery = ExtractQuery(operand, &err);

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
    v8::Handle<v8::Object> result = QueryTempl->NewInstance();

    StoreQuery(result, opQuery->clone(opQuery));

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
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_DistanceQuery (v8::Arguments const& argv) {
  v8::HandleScope scope;

  // extract the operand query
  v8::Handle<v8::Object> operand = argv.Holder();
  v8::Handle<v8::String> err;
  TRI_query_t* opQuery = ExtractQuery(operand, &err);

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
    v8::Handle<v8::Object> result = QueryTempl->NewInstance();

    StoreQuery(result, TRI_CreateDistanceQuery(opQuery->clone(opQuery), "_distance"));

    return scope.Close(result);
  }

  // .............................................................................
  // case: <distance>
  // .............................................................................

  else if (argv.Length() == 1) {
    string name = ObjectToString(argv[0]);

    if (name.empty()) {
      return scope.Close(v8::ThrowException(v8::String::New("<attribute> must be non-empty")));
    }

    v8::Handle<v8::Object> result = QueryTempl->NewInstance();

    StoreQuery(result, TRI_CreateDistanceQuery(opQuery->clone(opQuery), name.c_str()));

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
/// @brief looks up a document
///
/// The "document" operator finds a document given it's identifier.
///
/// <tt>document(identifier)</tt>
///
/// Returns the empty result set or a result set containing the document
/// with document identifier @c identifier.
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_DocumentQuery (v8::Arguments const& argv) {
  v8::HandleScope scope;

  // extract the operand query
  v8::Handle<v8::Object> operand = argv.Holder();
  v8::Handle<v8::String> err;
  TRI_query_t* opQuery = ExtractQuery(operand, &err);

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
  TRI_voc_cid_t cid;
  TRI_voc_did_t did;

  if (! IsDocumentId(arg1, cid, did)) {
    return scope.Close(v8::ThrowException(v8::String::New("<document-idenifier> must be a document identifier")));
  }

  if (cid != 0 && cid != opQuery->_collection->_collection->base._cid) {
    return scope.Close(v8::ThrowException(v8::String::New("cannot execute cross collection query")));
  }

  // .............................................................................
  // create new query
  // .............................................................................

  v8::Handle<v8::Object> result = QueryTempl->NewInstance();

  StoreQuery(result, TRI_CreateDocumentQuery(opQuery->clone(opQuery), did));

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a geo-spatial index
///
/// <tt>geo(location)</tt>
///
/// Next "near" operator will use the specific geo-spatial index.
///
/// <tt>geo(latitiude,longitude)</tt>
///
/// Next "near" operator will use the specific geo-spatial index.
///
/// Assume you a location stored as list in the attribute @c location
/// and a destination stored in the attribute @c destination. Than you
/// can use the "geo" operator to select, which coordinates to use in
/// a near query.
///
/// @verbinclude fluent15
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_GeoQuery (v8::Arguments const& argv) {
  v8::HandleScope scope;

  // extract the operand query
  v8::Handle<v8::Object> operand = argv.Holder();
  v8::Handle<v8::String> err;
  TRI_query_t* opQuery = ExtractQuery(operand, &err);

  if (opQuery == 0) {
    return scope.Close(v8::ThrowException(err));
  }

  if (IsExecutedQuery(operand)) {
    return scope.Close(v8::ThrowException(v8::String::New("query already executed")));
  }

  // .............................................................................
  // case: location
  // .............................................................................

  if (argv.Length() == 1) {
    string location = ObjectToString(argv[0]);

    v8::Handle<v8::Object> result = QueryTempl->NewInstance();

    StoreQuery(result, TRI_CreateGeoIndexQuery(opQuery->clone(opQuery), location.c_str(), 0, 0));

    return scope.Close(result);
  }

  // .............................................................................
  // case: latitiude and longitude
  // .............................................................................

  else if (argv.Length() == 2) {
    string latitiude = ObjectToString(argv[0]);
    string longitude = ObjectToString(argv[1]);

    v8::Handle<v8::Object> result = QueryTempl->NewInstance();

    StoreQuery(result, TRI_CreateGeoIndexQuery(opQuery->clone(opQuery), 0, latitiude.c_str(), longitude.c_str()));

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
/// @brief limits an existing query
///
/// <tt>limit(number)</tt>
///
/// Limits a result to the first @c number documents. If @c number is negative,
/// the result set is limited to the last @c -number documents.
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_LimitQuery (v8::Arguments const& argv) {
  v8::HandleScope scope;

  // extract the operand query
  v8::Handle<v8::Object> operand = argv.Holder();
  v8::Handle<v8::String> err;
  TRI_query_t* opQuery = ExtractQuery(operand, &err);

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

  double limit = ObjectToDouble(argv[0]);

  // .............................................................................
  // create new query
  // .............................................................................

  v8::Handle<v8::Object> result = QueryTempl->NewInstance();

  StoreQuery(result, TRI_CreateLimitQuery(opQuery->clone(opQuery), (TRI_voc_ssize_t) limit));

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finds points near a given coordinate
///
/// The "near" operator is rather complex, because there are two variants (list
/// or array) and some optional parameters. If you have defined only one
/// geo-index, however, the call is simply
///
/// <tt>near(latitiude, longitude)</tt>
///
/// This will find at most 100 documents near the coordinate (@c latitiude, @c
/// longitude). The returned list is sorted according to the distance, with the
/// nearest document coming first.
///
/// @verbinclude fluent10
///
/// If you need the distance as well, then you can use
///
/// <tt>near(latitiude, longitude).distance()</tt>
///
/// This will add an attribute "_distance" to all documents returned, which
/// contains the distance of the given point and the document in meter.
///
/// @verbinclude fluent11
///
/// <tt>near(latitiude, longitude).distance(name)</tt>
///
/// This will add an attribute "name" to all documents returned, which
/// contains the distance of the given point and the document in meter.
///
/// @verbinclude fluent12
///
/// <tt>near(latitiude, longitude).limit(count)</tt>
///
/// Limits the result to @c count documents. Note that @c count can be more than
/// 100. To get less or more than 100 documents with distances, use
///
/// <tt>near(latitiude, longitude).distance().limit(count)</tt>
///
/// This will return the first @c count documents together with their distances
/// in meters.
///
/// @verbinclude fluent13
///
/// If you have more then one geo-spatial index, you can use the operator
/// "geo" to select a particular index.
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_NearQuery (v8::Arguments const& argv) {
  static size_t const DEFAULT_LIMIT = 100;

  v8::HandleScope scope;

  // extract the operand query
  v8::Handle<v8::Object> operand = argv.Holder();
  v8::Handle<v8::String> err;
  TRI_query_t* opQuery = ExtractQuery(operand, &err);

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
    double latitiude = ObjectToDouble(argv[0]);
    double longitude = ObjectToDouble(argv[1]);

    v8::Handle<v8::Object> result = QueryTempl->NewInstance();

    StoreQuery(result, TRI_CreateNearQuery(opQuery->clone(opQuery), latitiude, longitude, DEFAULT_LIMIT, NULL));

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
/// @brief selects elements by example
///
/// The "select" operator finds all documents which match a given example.
///
/// <tt>select()</tt>
///
/// Selects all documents.
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_SelectQuery (v8::Arguments const& argv) {
  v8::HandleScope scope;

  // extract the operand query
  v8::Handle<v8::Object> operand = argv.Holder();
  v8::Handle<v8::String> err;
  TRI_query_t* opQuery = ExtractQuery(operand, &err);

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
    v8::Handle<v8::Object> result = QueryTempl->NewInstance();

    StoreQuery(result, opQuery->clone(opQuery));

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
/// @brief skips an existing query
///
/// <tt>skip(number)</tt>
///
/// Skips the first @c number documents.
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_SkipQuery (v8::Arguments const& argv) {
  v8::HandleScope scope;

  // extract the operand query
  v8::Handle<v8::Object> operand = argv.Holder();
  v8::Handle<v8::String> err;
  TRI_query_t* opQuery = ExtractQuery(operand, &err);

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

  double skip = ObjectToDouble(argv[0]);

  if (skip < 0.0) {
    skip = 0.0;
  }

  // .............................................................................
  // create new query
  // .............................................................................

  v8::Handle<v8::Object> result = QueryTempl->NewInstance();

  StoreQuery(result, TRI_CreateSkipQuery(opQuery->clone(opQuery), (TRI_voc_size_t) skip));

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finds points within a given radius
///
/// The "within" operator is rather complex, because there are two variants
/// (list or array) and some optional parameters. If you have defined only one
/// geo-index, however, the call is simply
///
/// <tt>distance(latitiude, longitude, radius)</tt>
///
/// This will find all documents with in a given radius around the coordinate
/// (@c latitiude, @c longitude). The returned list is sorted according to the
/// distance, with the nearest document coming first.
///
/// @verbinclude fluentXX
///
/// If you need the distance as well, then you can use
///
/// <tt>within(latitiude, longitude, radius).distance()</tt>
///
/// This will add an attribute "_distance" to all documents returned, which
/// contains the distance of the given point and the document in meter.
///
/// @verbinclude fluentXX
///
/// <tt>within(latitiude, longitude, radius).distance(name)</tt>
///
/// This will add an attribute "name" to all documents returned, which
/// contains the distance of the given point and the document in meter.
///
/// @verbinclude fluentXX
///
/// If you have more then one geo-spatial index, you can use the operator
/// "geo" to select a particular index.
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_WithinQuery (v8::Arguments const& argv) {
  v8::HandleScope scope;

  // extract the operand query
  v8::Handle<v8::Object> operand = argv.Holder();
  v8::Handle<v8::String> err;
  TRI_query_t* opQuery = ExtractQuery(operand, &err);

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
    double latitiude = ObjectToDouble(argv[0]);
    double longitude = ObjectToDouble(argv[1]);
    double radius = ObjectToDouble(argv[2]);

    v8::Handle<v8::Object> result = QueryTempl->NewInstance();

    StoreQuery(result, TRI_CreateWithinQuery(opQuery->clone(opQuery), latitiude, longitude, radius, NULL));

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
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                       TRI_VOCBASE_COL_T FUNCTIONS
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                              javascript functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBasePrivate VocBase (Private)
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that a geo index exists
///
/// <tt>ensureGeoIndex(location)</tt>
///
/// Creates a geo-spatial index on all documents using @c location as path the
/// coordinates. The value of the attribute must be a list with at least two
/// double values. The list must contain the latitiude (first value) and the
/// longitude (second value). All documents, which do not have the
/// attribute path or which value is not suitable, are ignored.
///
/// <tt>ensureGeoIndex(location, geojson)</tt>
///
/// As above. If @c geoJson is true, than the order within the list is
/// longitude followed by latitiude. This corresponds to the format
/// described in http://geojson.org/geojson-spec.html#positions.
///
/// @verbinclude fluent10
///
/// <tt>ensureGeoIndex(latitiude,longitude)</tt>
///
/// Creates a geo-spatial index on all documents using @c latitiude and @c
/// longitude as paths the latitiude and the longitude. The value of the
/// attribute @c latitiude and of the attribute @c longitude must a double. All
/// documents, which do not have the attribute paths or which values are not
/// suitable, are ignored.
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

  // .............................................................................
  // case: <location>
  // .............................................................................

  if (argv.Length() == 1) {
    v8::String::Utf8Value loc(argv[0]);

    if (*loc == 0) {
      return scope.Close(v8::ThrowException(v8::String::New("<location> must be an attribute path")));
    }

    TRI_EnsureGeoIndexSimCollection(sim, *loc, false);
  }

  // .............................................................................
  // case: <location>
  // .............................................................................

  else if (argv.Length() == 2 && (argv[1]->IsBoolean() || argv[1]->IsBooleanObject())) {
    v8::String::Utf8Value loc(argv[0]);

    if (*loc == 0) {
      return scope.Close(v8::ThrowException(v8::String::New("<location> must be an attribute path")));
    }

    TRI_EnsureGeoIndexSimCollection(sim, *loc, ObjectToBoolean(argv[1]));
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

    TRI_EnsureGeoIndex2SimCollection(sim, *lat, *lon);
  }

  // .............................................................................
  // error case
  // .............................................................................

  else {
    return scope.Close(v8::ThrowException(v8::String::New("usage: ensureGeoIndex(<latitiude>, <longitude>) or ensureGeoIndex(<location>, [<geojson>])")));
  }

  return scope.Close(v8::Undefined());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes a document
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

  TRI_voc_cid_t cid;
  TRI_voc_did_t did;

  if (IsDocumentId(argv[0], cid, did)) {
    if (cid != 0 && cid != collection->base._cid) {
      return scope.Close(v8::ThrowException(v8::String::New("cannot execute cross collection delete")));
    }
  }
  else {
    return scope.Close(v8::ThrowException(v8::String::New("expecting a document identifier")));
  }

  // .............................................................................
  // inside a write transaction
  // .............................................................................

  collection->beginWrite(collection);

  bool ok = collection->destroy(collection, did);

  collection->endWrite(collection);

  // .............................................................................
  // outside a write transaction
  // .............................................................................

  if (! ok) {
    string err = "cannot save document: ";
    err += TRI_last_error();

    return scope.Close(v8::ThrowException(v8::String::New(err.c_str())));
  }

  return scope.Close(v8::Undefined());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief gets or sets the parameters of a collection
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_ParameterVocbaseCol (v8::Arguments const& argv) {
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
      if (po->Has(SyncAfterObjectsKey)) {
        syncAfterObjects = ObjectToDouble(po->Get(SyncAfterObjectsKey), error);

        if (error || syncAfterObjects < 0.0) {
          return scope.Close(v8::ThrowException(v8::String::New("<parameter>.syncAfterObjects must be a number")));
        }
      }

      // extract sync after bytes
      if (po->Has(SyncAfterBytesKey)) {
        syncAfterBytes = ObjectToDouble(po->Get(SyncAfterBytesKey), error);

        if (error || syncAfterBytes < 0.0) {
          return scope.Close(v8::ThrowException(v8::String::New("<parameter>.syncAfterBytes must be a number")));
        }
      }

      // extract sync after times
      if (po->Has(SyncAfterTimeKey)) {
        syncAfterTime = ObjectToDouble(po->Get(SyncAfterTimeKey), error);

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

    result->Set(SyncAfterObjectsKey, v8::Number::New(syncAfterObjects));
    result->Set(SyncAfterBytesKey, v8::Number::New(syncAfterBytes));
    result->Set(SyncAfterTimeKey, v8::Number::New(syncAfterTime));
    result->Set(JournalSizeKey, v8::Number::New(maximalSize));
  }

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief replaces a document
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

  TRI_voc_cid_t cid;
  TRI_voc_did_t did;

  if (IsDocumentId(argv[0], cid, did)) {
    if (cid != 0 && cid != collection->base._cid) {
      return scope.Close(v8::ThrowException(v8::String::New("cannot execute cross collection update")));
    }
  }
  else {
    return scope.Close(v8::ThrowException(v8::String::New("expecting a document identifier")));
  }

  TRI_shaped_json_t* shaped = ShapedJsonV8Object(argv[1], collection->_shaper);

  if (shaped == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("<document> cannot be converted into JSON shape")));
  }

  // .............................................................................
  // inside a write transaction
  // .............................................................................

  collection->beginWrite(collection);

  bool ok = collection->update(collection, shaped, did);

  collection->endWrite(collection);

  // .............................................................................
  // outside a write transaction
  // .............................................................................

  TRI_FreeShapedJson(shaped);

  if (! ok) {
    string err = "cannot save document: ";
    err += TRI_last_error();

    return scope.Close(v8::ThrowException(v8::String::New(err.c_str())));
  }

  return scope.Close(v8::Undefined());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief saves a new document
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_SaveVocbaseCol (v8::Arguments const& argv) {
  v8::HandleScope scope;

  v8::Handle<v8::String> err;
  TRI_vocbase_col_t const* col = LoadCollection(argv.Holder(), &err);

  if (col == 0) {
    return scope.Close(v8::ThrowException(err));
  }

  TRI_doc_collection_t* collection = col->_collection;

  if (argv.Length() < 1) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: save(<document>)")));
  }

  TRI_shaped_json_t* shaped = ShapedJsonV8Object(argv[0], collection->_shaper);

  if (shaped == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("<document> cannot be converted into JSON shape")));
  }

  // .............................................................................
  // inside a write transaction
  // .............................................................................

  collection->beginWrite(collection);

  TRI_voc_did_t did = collection->create(collection, shaped);

  collection->endWrite(collection);

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
/// @brief converts a TRI_vocbase_col_t into a string
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_ToStringVocbaseCol (v8::Arguments const& argv) {
  v8::HandleScope scope;

  TRI_vocbase_col_t const* collection = UnwrapClass<TRI_vocbase_col_t>(argv.Holder());

  if (collection == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("illegal collection pointer")));
  }

  string name;

  if (collection->_corrupted) {
    name = "[corrupted collection \"" + string(collection->_name) + "\"]";
  }
  else if (collection->_newBorn) {
    name = "[new born collection \"" + string(collection->_name) + "\"]";
  }
  else if (collection->_loaded) {
    name = "[collection \"" + string(collection->_name) + "\"]";
  }
  else {
    name = "[unloaded collection \"" + string(collection->_name) + "\"]";
  }

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
/// @addtogroup VocBasePrivate VocBase (Private)
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief selects a collection from the vocbase
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> MapGetVocBase (v8::Local<v8::String> name,
                                            const v8::AccessorInfo& info) {
  v8::HandleScope scope;
  TRI_vocbase_t* vocbase = UnwrapClass<TRI_vocbase_t>(info.Holder());

  if (vocbase == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("corrupted vocbase")));
  }

  // convert the JavaScript string to a string
  string key = ObjectToString(name);

  // look up the value if it exists
  TRI_vocbase_col_t const* collection = TRI_FindCollectionByNameVocBase(vocbase, key.c_str());

  // if the key is not present return an empty handle as signal
  if (collection == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("cannot load or create collection")));
  }

  // .............................................................................
  // otherwise wrap collection into V8 object
  // .............................................................................

  v8::Handle<v8::Object> result = WrapClass(VocbaseColTempl, const_cast<TRI_vocbase_col_t*>(collection));

  // .............................................................................
  // create new query
  // .............................................................................

  result->SetInternalField(SLOT_QUERY, CollectionQueryType);
  result->SetInternalField(SLOT_RESULT_SET, v8::Null());

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                              javascript functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBasePrivate VocBase (Private)
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a TRI_vocbase_t into a string
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_ToStringVocBase (v8::Arguments const& argv) {
  v8::HandleScope scope;

  TRI_vocbase_t* vocbase = UnwrapClass<TRI_vocbase_t>(argv.Holder());

  if (vocbase == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("corrupted vocbase")));
  }

  string name = "[vocbase at \"" + string(vocbase->_path) + "\"]";

  return scope.Close(v8::String::New(name.c_str()));
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
/// @addtogroup VocBase VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a TRI_vocbase_t global context
////////////////////////////////////////////////////////////////////////////////

v8::Persistent<v8::Context> InitV8VocBridge (TRI_vocbase_t* vocbase) {
  v8::Handle<v8::ObjectTemplate> rt;

  // create the regular expressions
  if (regcomp(&DocumentIdRegex, "([0-9][0-9]*):([0-9][0-9]*)", REG_ICASE | REG_EXTENDED) != 0) {
    LOG_FATAL("cannot compile regular expression");
    exit(EXIT_FAILURE);
  }

  // create the global template
  v8::Handle<v8::ObjectTemplate> global = v8::ObjectTemplate::New();

  // create the global functions
  InitV8Shell(global);

  // create the context
  v8::Persistent<v8::Context> context = v8::Context::New(0, global);

  if (context.IsEmpty()) {
    printf("cannot initialize V8 engine\n");
    exit(EXIT_FAILURE);
  }

  context->Enter();

  // .............................................................................
  // global function names
  // .............................................................................

  OutputFuncName = v8::Persistent<v8::String>::New(v8::String::New("output"));
  PrintQueryFuncName = v8::Persistent<v8::String>::New(v8::String::New("printQuery"));
  ToStringFuncName = v8::Persistent<v8::String>::New(v8::String::New("toString"));

  // .............................................................................
  // local function names
  // .............................................................................

  v8::Handle<v8::String> AllFuncName = v8::Persistent<v8::String>::New(v8::String::New("all"));
  v8::Handle<v8::String> CountFuncName = v8::Persistent<v8::String>::New(v8::String::New("count"));
  v8::Handle<v8::String> DeleteFuncName = v8::Persistent<v8::String>::New(v8::String::New("delete"));
  v8::Handle<v8::String> DistanceFuncName = v8::Persistent<v8::String>::New(v8::String::New("distance"));
  v8::Handle<v8::String> DocumentFuncName = v8::Persistent<v8::String>::New(v8::String::New("document"));
  v8::Handle<v8::String> EnsureGeoIndexFuncName = v8::Persistent<v8::String>::New(v8::String::New("ensureGeoIndex"));
  v8::Handle<v8::String> ExplainFuncName = v8::Persistent<v8::String>::New(v8::String::New("explain"));
  v8::Handle<v8::String> GeoFuncName = v8::Persistent<v8::String>::New(v8::String::New("geo"));
  v8::Handle<v8::String> HasNextFuncName = v8::Persistent<v8::String>::New(v8::String::New("hasNext"));
  v8::Handle<v8::String> LimitFuncName = v8::Persistent<v8::String>::New(v8::String::New("limit"));
  v8::Handle<v8::String> NearFuncName = v8::Persistent<v8::String>::New(v8::String::New("near"));
  v8::Handle<v8::String> NextFuncName = v8::Persistent<v8::String>::New(v8::String::New("next"));
  v8::Handle<v8::String> ParameterFuncName = v8::Persistent<v8::String>::New(v8::String::New("parameter"));
  v8::Handle<v8::String> PrintFuncName = v8::Persistent<v8::String>::New(v8::String::New("print"));
  v8::Handle<v8::String> ReplaceFuncName = v8::Persistent<v8::String>::New(v8::String::New("replace"));
  v8::Handle<v8::String> SaveFuncName = v8::Persistent<v8::String>::New(v8::String::New("save"));
  v8::Handle<v8::String> SelectFuncName = v8::Persistent<v8::String>::New(v8::String::New("select"));
  v8::Handle<v8::String> ShowFuncName = v8::Persistent<v8::String>::New(v8::String::New("show"));
  v8::Handle<v8::String> SkipFuncName = v8::Persistent<v8::String>::New(v8::String::New("skip"));
  v8::Handle<v8::String> ToArrayFuncName = v8::Persistent<v8::String>::New(v8::String::New("toArray"));
  v8::Handle<v8::String> WithinFuncName = v8::Persistent<v8::String>::New(v8::String::New("within"));

  // .............................................................................
  // query types
  // .............................................................................

  CollectionQueryType = v8::Persistent<v8::String>::New(v8::String::New("collection"));

  // .............................................................................
  // keys
  // .............................................................................

  JournalSizeKey = v8::Persistent<v8::String>::New(v8::String::New("journalSize"));
  SyncAfterBytesKey = v8::Persistent<v8::String>::New(v8::String::New("syncAfterBytes"));
  SyncAfterObjectsKey = v8::Persistent<v8::String>::New(v8::String::New("syncAfterObjects"));
  SyncAfterTimeKey = v8::Persistent<v8::String>::New(v8::String::New("syncAfterTime"));

  // .............................................................................
  // generate the TRI_vocbase_t template
  // .............................................................................

  rt = v8::ObjectTemplate::New();

  rt->SetInternalFieldCount(1);

  rt->Set(PrintFuncName, v8::FunctionTemplate::New(JS_PrintUsingToString));
  rt->Set(ToStringFuncName, v8::FunctionTemplate::New(JS_ToStringVocBase));
  rt->SetNamedPropertyHandler(MapGetVocBase);

  VocbaseTempl = v8::Persistent<v8::ObjectTemplate>::New(rt);

  // .............................................................................
  // generate the TRI_vocbase_col_t template
  // .............................................................................

  rt = v8::ObjectTemplate::New();

  rt->SetInternalFieldCount(SLOT_END);

  rt->Set(AllFuncName, v8::FunctionTemplate::New(JS_AllQuery));
  rt->Set(CountFuncName, v8::FunctionTemplate::New(JS_CountQuery));
  rt->Set(DeleteFuncName, v8::FunctionTemplate::New(JS_DeleteVocbaseCol));
  rt->Set(DocumentFuncName, v8::FunctionTemplate::New(JS_DocumentQuery));
  rt->Set(EnsureGeoIndexFuncName, v8::FunctionTemplate::New(JS_EnsureGeoIndexVocbaseCol));
  rt->Set(ExplainFuncName, v8::FunctionTemplate::New(JS_ExplainQuery));
  rt->Set(GeoFuncName, v8::FunctionTemplate::New(JS_GeoQuery));
  rt->Set(LimitFuncName, v8::FunctionTemplate::New(JS_LimitQuery));
  rt->Set(NearFuncName, v8::FunctionTemplate::New(JS_NearQuery));
  rt->Set(ParameterFuncName, v8::FunctionTemplate::New(JS_ParameterVocbaseCol));
  rt->Set(PrintFuncName, v8::FunctionTemplate::New(JS_PrintUsingToString));
  rt->Set(ReplaceFuncName, v8::FunctionTemplate::New(JS_ReplaceVocbaseCol));
  rt->Set(SaveFuncName, v8::FunctionTemplate::New(JS_SaveVocbaseCol));
  rt->Set(SelectFuncName, v8::FunctionTemplate::New(JS_SelectQuery));
  rt->Set(SkipFuncName, v8::FunctionTemplate::New(JS_SkipQuery));
  rt->Set(ToArrayFuncName, v8::FunctionTemplate::New(JS_ToArrayQuery));
  rt->Set(ToStringFuncName, v8::FunctionTemplate::New(JS_ToStringVocbaseCol));
  rt->Set(WithinFuncName, v8::FunctionTemplate::New(JS_WithinQuery));

  VocbaseColTempl = v8::Persistent<v8::ObjectTemplate>::New(rt);

  // .............................................................................
  // generate the query template
  // .............................................................................

  rt = v8::ObjectTemplate::New();

  rt->SetInternalFieldCount(SLOT_END);

  rt->Set(AllFuncName, v8::FunctionTemplate::New(JS_AllQuery));
  rt->Set(CountFuncName, v8::FunctionTemplate::New(JS_CountQuery));
  rt->Set(DistanceFuncName, v8::FunctionTemplate::New(JS_DistanceQuery));
  rt->Set(DocumentFuncName, v8::FunctionTemplate::New(JS_DocumentQuery));
  rt->Set(ExplainFuncName, v8::FunctionTemplate::New(JS_ExplainQuery));
  rt->Set(GeoFuncName, v8::FunctionTemplate::New(JS_GeoQuery));
  rt->Set(HasNextFuncName, v8::FunctionTemplate::New(JS_HasNextQuery));
  rt->Set(LimitFuncName, v8::FunctionTemplate::New(JS_LimitQuery));
  rt->Set(NearFuncName, v8::FunctionTemplate::New(JS_NearQuery));
  rt->Set(NextFuncName, v8::FunctionTemplate::New(JS_NextQuery));
  rt->Set(PrintFuncName, v8::FunctionTemplate::New(JS_PrintQuery));
  rt->Set(SelectFuncName, v8::FunctionTemplate::New(JS_SelectQuery));
  rt->Set(ShowFuncName, v8::FunctionTemplate::New(JS_ShowQuery));
  rt->Set(SkipFuncName, v8::FunctionTemplate::New(JS_SkipQuery));
  rt->Set(ToArrayFuncName, v8::FunctionTemplate::New(JS_ToArrayQuery));
  rt->Set(WithinFuncName, v8::FunctionTemplate::New(JS_WithinQuery));

  QueryTempl = v8::Persistent<v8::ObjectTemplate>::New(rt);

  // .............................................................................
  // create the global variables
  // .............................................................................

  context->Global()->Set(v8::String::New("db"),
                         WrapClass(VocbaseTempl, vocbase),
                         v8::ReadOnly);

  // and return the context
  return context;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\)"
// End:
