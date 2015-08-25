////////////////////////////////////////////////////////////////////////////////
/// @brief V8-vocbase queries
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

#include "v8-query.h"
#include "Aql/Query.h"
#include "Basics/logging.h"
#include "Basics/random.h"
#include "Basics/string-buffer.h"
#include "Indexes/FulltextIndex.h"
#include "Indexes/GeoIndex2.h"
#include "Indexes/SkiplistIndex2.h"
#include "FulltextIndex/fulltext-index.h"
#include "FulltextIndex/fulltext-result.h"
#include "FulltextIndex/fulltext-query.h"
#include "Utils/transactions.h"
#include "V8/v8-globals.h"
#include "V8/v8-conv.h"
#include "V8/v8-utils.h"
#include "V8Server/v8-shape-conv.h"
#include "V8Server/v8-vocbase.h"
#include "V8Server/v8-vocindex.h"
#include "V8Server/v8-wrapshapedjson.h"
#include "VocBase/document-collection.h"
#include "VocBase/edge-collection.h"
#include "VocBase/ExampleMatcher.h"
#include "VocBase/vocbase.h"
#include "VocBase/VocShaper.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::arango;

// -----------------------------------------------------------------------------
// --SECTION--                                                   private defines
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief shortcut to wrap a shaped-json object in a read-only transaction
////////////////////////////////////////////////////////////////////////////////

#define WRAP_SHAPED_JSON(...) TRI_WrapShapedJson<SingleCollectionReadOnlyTransaction>(isolate, __VA_ARGS__)

// -----------------------------------------------------------------------------
// --SECTION--                                                  HELPER FUNCTIONS
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                     private types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief geo coordinate container, also containing the distance
////////////////////////////////////////////////////////////////////////////////

typedef struct {
  double _distance;
  void const* _data;
}
geo_coordinate_distance_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief query types
////////////////////////////////////////////////////////////////////////////////

typedef enum {
  QUERY_EXAMPLE,
  QUERY_CONDITION
}
query_t;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief return an empty result set
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> EmptyResult (v8::Isolate* isolate) {
  v8::EscapableHandleScope scope(isolate);

  v8::Handle<v8::Object> result = v8::Object::New(isolate);
  result->Set(TRI_V8_ASCII_STRING("documents"), v8::Array::New(isolate));
  result->Set(TRI_V8_ASCII_STRING("total"),     v8::Number::New(isolate, 0));
  result->Set(TRI_V8_ASCII_STRING("count"),     v8::Number::New(isolate, 0));

  return scope.Escape<v8::Value>(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts skip and limit
////////////////////////////////////////////////////////////////////////////////

static void ExtractSkipAndLimit (const v8::FunctionCallbackInfo<v8::Value>& args,
                                 size_t pos,
                                 int64_t& skip,
                                 uint64_t& limit) {

  skip = 0;
  limit = UINT64_MAX;

  if (pos < (size_t) args.Length() && ! args[(int) pos]->IsNull() && ! args[(int) pos]->IsUndefined()) {
    skip = TRI_ObjectToInt64(args[(int) pos]);
  }

  if (pos + 1 < (size_t) args.Length() && ! args[(int) pos + 1]->IsNull() && ! args[(int) pos + 1]->IsUndefined()) {
    limit = TRI_ObjectToUInt64(args[(int) pos + 1], false);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief calculates slice
////////////////////////////////////////////////////////////////////////////////

static void CalculateSkipLimitSlice (size_t length,
                                     int64_t skip,
                                     uint64_t limit,
                                     uint64_t& s,
                                     uint64_t& e) {
  s = 0;
  e = static_cast<uint64_t>(length);

  // skip from the beginning
  if (0 < skip) {
    s = static_cast<uint64_t>(skip);

    if (e < s) {
      s = e;
    }
  }

  // skip from the end
  else if (skip < 0) {
    skip = -skip;

    if (skip < static_cast<decltype(skip)>(e)) {
      s = e - skip;
    }
  }

  // apply limit
  if (limit < UINT64_MAX && static_cast<int64_t>(limit) < INT64_MAX) {
    if (s + limit < e) {
      int64_t sum = static_cast<int64_t>(s) + static_cast<int64_t>(limit);

      if (sum < static_cast<int64_t>(e)) {
        if (sum >= INT64_MAX) {
          e = static_cast<uint64_t>(INT64_MAX);
        }
        else {
          e = static_cast<uint64_t>(sum);
        }
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets up the skiplist operator for a skiplist condition query
////////////////////////////////////////////////////////////////////////////////

static TRI_index_operator_t* SetupConditionsSkiplist (v8::Isolate* isolate,
                                                      std::vector<std::string> const& fields,
                                                      VocShaper* shaper,
                                                      v8::Handle<v8::Object> conditions) {
  TRI_index_operator_t* lastOperator = nullptr;
  size_t numEq = 0;
  size_t lastNonEq = 0;

  TRI_json_t* parameters = TRI_CreateArrayJson(TRI_UNKNOWN_MEM_ZONE);

  if (parameters == nullptr) {
    return nullptr;
  }

  // iterate over all index fields
  size_t i = 0;
  for (auto const& field : fields) {
    v8::Handle<v8::String> key = TRI_V8_STD_STRING(field);

    if (! conditions->HasOwnProperty(key)) {
      break;
    }
    v8::Handle<v8::Value> fieldConditions = conditions->Get(key);

    if (! fieldConditions->IsArray()) {
      // wrong data type for field conditions
      break;
    }

    // iterator over all conditions
    v8::Handle<v8::Array> values = v8::Handle<v8::Array>::Cast(fieldConditions);
    for (uint32_t j = 0; j < values->Length(); ++j) {
      v8::Handle<v8::Value> fieldCondition = values->Get(j);

      if (! fieldCondition->IsArray()) {
        // wrong data type for single condition
        goto MEM_ERROR;
      }

      v8::Handle<v8::Array> condition = v8::Handle<v8::Array>::Cast(fieldCondition);

      if (condition->Length() != 2) {
        // wrong number of values in single condition
        goto MEM_ERROR;
      }

      v8::Handle<v8::Value> op = condition->Get(0);
      v8::Handle<v8::Value> value = condition->Get(1);

      if (! op->IsString() && ! op->IsStringObject()) {
        // wrong operator type
        goto MEM_ERROR;
      }

      TRI_json_t* json = TRI_ObjectToJson(isolate, value);

      if (json == nullptr) {
        goto MEM_ERROR;
      }

      std::string&& opValue = TRI_ObjectToString(op);
      if (opValue == "==") {
        // equality comparison

        if (lastNonEq > 0) {
          TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
          goto MEM_ERROR;
        }

        TRI_PushBack3ArrayJson(TRI_UNKNOWN_MEM_ZONE, parameters, json);
        // creation of equality operator is deferred until it is finally needed
        ++numEq;
        break;
      }
      else {
        if (lastNonEq > 0 && lastNonEq != i) {
          // if we already had a range condition and a previous field, we cannot continue
          // because the skiplist interface does not support such queries
          TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
          goto MEM_ERROR;
        }

        TRI_index_operator_type_e opType;
        if (opValue == ">") {
          opType = TRI_GT_INDEX_OPERATOR;
        }
        else if (opValue == ">=") {
          opType = TRI_GE_INDEX_OPERATOR;
        }
        else if (opValue == "<") {
          opType = TRI_LT_INDEX_OPERATOR;
        }
        else if (opValue == "<=") {
          opType = TRI_LE_INDEX_OPERATOR;
        }
        else {
          // wrong operator type
          TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
          goto MEM_ERROR;
        }

        lastNonEq = i;

        TRI_json_t* cloned = TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, parameters);

        if (cloned == nullptr) {
          TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
          goto MEM_ERROR;
        }

        TRI_PushBack3ArrayJson(TRI_UNKNOWN_MEM_ZONE, cloned, json);

        if (numEq) {
          // create equality operator if one is in queue
          TRI_json_t* clonedParams = TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, parameters);

          if (clonedParams == nullptr) {
            TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, cloned);
            goto MEM_ERROR;
          }

          lastOperator = TRI_CreateIndexOperator(TRI_EQ_INDEX_OPERATOR, 
                                                 nullptr,
                                                 nullptr, 
                                                 clonedParams, 
                                                 shaper, 
                                                 TRI_LengthVector(&clonedParams->_value._objects)); 
          numEq = 0;
        }

        TRI_index_operator_t* current;

        // create the operator for the current condition
        current = TRI_CreateIndexOperator(opType,
                                          nullptr,
                                          nullptr, 
                                          cloned, 
                                          shaper, 
                                          TRI_LengthVector(&cloned->_value._objects)); 

        if (current == nullptr) {
          TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, cloned);
          goto MEM_ERROR;
        }

        if (lastOperator == nullptr) {
          lastOperator = current;
        }
        else {
          // merge the current operator with previous operators using logical AND
          TRI_index_operator_t* newOperator = TRI_CreateIndexOperator(TRI_AND_INDEX_OPERATOR, 
                                                                      lastOperator, 
                                                                      current, 
                                                                      nullptr, 
                                                                      shaper, 
                                                                      2);

          if (newOperator == nullptr) {
            TRI_FreeIndexOperator(current);
            goto MEM_ERROR;
          }
          else {
            lastOperator = newOperator;
          }
        }
      }
    }

    ++i;
  }

  if (numEq) {
    // create equality operator if one is in queue
    TRI_ASSERT(lastOperator == nullptr);
    TRI_ASSERT(lastNonEq == 0);

    TRI_json_t* clonedParams = TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, parameters);

    if (clonedParams == nullptr) {
      goto MEM_ERROR;
    }

    lastOperator = TRI_CreateIndexOperator(TRI_EQ_INDEX_OPERATOR, 
                                           nullptr,
                                           nullptr,
                                           clonedParams, 
                                           shaper, 
                                           TRI_LengthVector(&clonedParams->_value._objects)); 
  }

  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, parameters);

  return lastOperator;

MEM_ERROR:
  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, parameters);

  if (lastOperator != nullptr) {
    TRI_FreeIndexOperator(lastOperator);
  }

  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets up the skiplist operator for a skiplist example query
///
/// this will set up a JSON container with the example values as a list
/// at the end, one skiplist equality operator is created for the entire list
////////////////////////////////////////////////////////////////////////////////

static TRI_index_operator_t* SetupExampleSkiplist (v8::Isolate* isolate,
                                                   std::vector<std::string> const& fields,
                                                   VocShaper* shaper,
                                                   v8::Handle<v8::Object> example) {
  TRI_json_t* parameters = TRI_CreateArrayJson(TRI_UNKNOWN_MEM_ZONE);

  if (parameters == nullptr) {
    return nullptr;
  }

  for (auto const& field : fields) {
    v8::Handle<v8::String> key = TRI_V8_STD_STRING(field);

    if (! example->HasOwnProperty(key)) {
      break;
    }

    v8::Handle<v8::Value> value = example->Get(key);

    TRI_json_t* json = TRI_ObjectToJson(isolate, value);

    if (json == nullptr) {
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, parameters);

      return nullptr;
    }

    TRI_PushBack3ArrayJson(TRI_UNKNOWN_MEM_ZONE, parameters, json);
  }

  if (TRI_LengthArrayJson(parameters) > 0) {
    // example means equality comparisons only
    return TRI_CreateIndexOperator(TRI_EQ_INDEX_OPERATOR, 
                                   nullptr, 
                                   nullptr,
                                   parameters, 
                                   shaper, 
                                   TRI_LengthArrayJson(parameters));
  }

  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, parameters);
  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys the example object for a hash index
////////////////////////////////////////////////////////////////////////////////

static void DestroySearchValue (TRI_memory_zone_t* zone,
                                TRI_index_search_value_t& value) {
  size_t n = value._length;

  for (size_t i = 0;  i < n;  ++i) {
    TRI_DestroyShapedJson(zone, &value._values[i]);
  }

  TRI_Free(TRI_UNKNOWN_MEM_ZONE, value._values);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets up the example object for a hash index
////////////////////////////////////////////////////////////////////////////////

static int SetupSearchValue (std::vector<TRI_shape_pid_t> const& paths,
                             v8::Handle<v8::Object> example,
                             VocShaper* shaper,
                             TRI_index_search_value_t& result,
                             std::string& errorMessage,
                             const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();

  // extract attribute paths
  size_t const n = paths.size();

  // setup storage
  result._length = n;
  result._values = static_cast<TRI_shaped_json_t*>(TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, n * sizeof(TRI_shaped_json_t), true));

  if (result._values == nullptr) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  // convert
  for (size_t i = 0;  i < n;  ++i) {
    TRI_shape_pid_t pid = paths[i];

    TRI_ASSERT(pid != 0);
    char const* name = shaper->attributeNameShapePid(pid);

    if (name == nullptr) {
      DestroySearchValue(shaper->memoryZone(), result);
      errorMessage = "shaper failed";
      return TRI_ERROR_BAD_PARAMETER;
    }

    v8::Handle<v8::String> key = TRI_V8_STRING(name);
    int res;

    if (example->HasOwnProperty(key)) {
      v8::Handle<v8::Value> val = example->Get(key);

      res = TRI_FillShapedJsonV8Object(isolate, val, &result._values[i], shaper, false);
    }
    else {
      res = TRI_FillShapedJsonV8Object(isolate, v8::Null(isolate), &result._values[i], shaper, false);
    }

    if (res != TRI_ERROR_NO_ERROR) {
      DestroySearchValue(shaper->memoryZone(), result);

      if (res != TRI_RESULT_ELEMENT_NOT_FOUND) {
        errorMessage = "cannot convert value to JSON";
      }
      return res;
    }
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief execute a skiplist query (by condition or by example)
////////////////////////////////////////////////////////////////////////////////

static void ExecuteSkiplistQuery (const v8::FunctionCallbackInfo<v8::Value>& args,
                                                   std::string const& signature,
                                                   query_t type) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  // expecting index, example, skip, and limit
  if (args.Length() < 2) {
    TRI_V8_THROW_EXCEPTION_USAGE(signature.c_str());
  }

  if (! args[1]->IsObject()) {
    std::string msg;

    if (type == QUERY_EXAMPLE) {
      msg = "<example> must be an object";
    }
    else {
      msg = "<conditions> must be an object";
    }

    TRI_V8_THROW_TYPE_ERROR(msg.c_str());
  }

  bool reverse = false;
  if (args.Length() > 4) {
    reverse = TRI_ObjectToBoolean(args[4]);
  }

  TRI_vocbase_col_t const* col = TRI_UnwrapClass<TRI_vocbase_col_t>(args.Holder(), TRI_GetVocBaseColType());

  if (col == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  TRI_THROW_SHARDING_COLLECTION_NOT_YET_IMPLEMENTED(col);

  SingleCollectionReadOnlyTransaction trx(new V8TransactionContext(true), col->_vocbase, col->_cid);

  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  TRI_document_collection_t* document = trx.documentCollection();
  auto shaper = document->getShaper();  // PROTECTED by trx here

  // extract skip and limit
  int64_t skip;
  uint64_t limit;
  ExtractSkipAndLimit(args, 2, skip, limit);

  // setup result
  v8::Handle<v8::Object> result = v8::Object::New(isolate);

  v8::Handle<v8::Array> documents = v8::Array::New(isolate);
  result->Set(TRI_V8_ASCII_STRING("documents"), documents);

  // .............................................................................
  // inside a read transaction
  // .............................................................................

  trx.lockRead();

  // extract the index
  auto idx = TRI_LookupIndexByHandle(isolate, trx.resolver(), col, args[0], false);

  if (idx == nullptr ||
      idx->type() != triagens::arango::Index::TRI_IDX_TYPE_SKIPLIST_INDEX) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_NO_INDEX);
  }

  TRI_index_operator_t* skiplistOperator;
  v8::Handle<v8::Object> values = args[1]->ToObject();

  if (type == QUERY_EXAMPLE) {
    skiplistOperator = SetupExampleSkiplist(isolate, idx->fields(), shaper, values);
  }
  else {
    skiplistOperator = SetupConditionsSkiplist(isolate, idx->fields(), shaper, values);
  }

  if (skiplistOperator == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
  }

  TRI_skiplist_iterator_t* skiplistIterator = static_cast<triagens::arango::SkiplistIndex2*>(idx)->lookup(skiplistOperator, reverse);
  TRI_FreeIndexOperator(skiplistOperator);

  if (skiplistIterator == nullptr) {
    int res = TRI_errno();

    if (res == TRI_RESULT_ELEMENT_NOT_FOUND) {
      TRI_V8_RETURN(EmptyResult(isolate));
    }

    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_NO_INDEX);
  }

  int64_t total = 0;
  uint64_t count = 0;
  bool error = false;

  if (trx.orderDitch(trx.trxCollection()) == nullptr) {
    TRI_FreeSkiplistIterator(skiplistIterator);
    TRI_V8_THROW_EXCEPTION_MEMORY();
  }

  while (limit > 0) {
    TRI_skiplist_index_element_t* indexElement = skiplistIterator->next(skiplistIterator);

    if (indexElement == nullptr) {
      break;
    }

    ++total;

    if (total > skip && count < limit) {
      v8::Handle<v8::Value> doc = WRAP_SHAPED_JSON(trx,
                                                   col->_cid,
                                                   ((TRI_doc_mptr_t const*) indexElement->_document)->getDataPtr());

      if (doc.IsEmpty()) {
        error = true;
        break;
      }
      else {
        documents->Set(static_cast<uint32_t>(count), doc);

        if (++count >= limit) {
          break;
        }
      }

    }
  }

  trx.finish(res);

  // .............................................................................
  // outside a write transaction
  // .............................................................................

  // free data allocated by skiplist index result
  TRI_FreeSkiplistIterator(skiplistIterator);

  result->Set(TRI_V8_ASCII_STRING("total"), v8::Number::New(isolate, static_cast<double>(total)));
  result->Set(TRI_V8_ASCII_STRING("count"), v8::Number::New(isolate, static_cast<double>(count)));

  if (error) {
    TRI_V8_THROW_EXCEPTION_MEMORY();
  }

  return TRI_V8_RETURN(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a geo result
////////////////////////////////////////////////////////////////////////////////

static int StoreGeoResult (v8::Isolate* isolate,
                           SingleCollectionReadOnlyTransaction& trx,
                           TRI_vocbase_col_t const* collection,
                           GeoCoordinates* cors,
                           v8::Handle<v8::Array>& documents,
                           v8::Handle<v8::Array>& distances) {
  geo_coordinate_distance_t* tmp;

  if (trx.orderDitch(trx.trxCollection()) == nullptr) {
    GeoIndex_CoordinatesFree(cors);
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  // sort the result
  size_t n = cors->length;

  if (n == 0) {
    GeoIndex_CoordinatesFree(cors);
    return TRI_ERROR_NO_ERROR;
  }

  geo_coordinate_distance_t* gtr = (tmp = (geo_coordinate_distance_t*) TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(geo_coordinate_distance_t) * n, false));

  if (gtr == nullptr) {
    GeoIndex_CoordinatesFree(cors);

    return TRI_ERROR_OUT_OF_MEMORY;
  }

  geo_coordinate_distance_t* gnd = tmp + n;

  GeoCoordinate* ptr = cors->coordinates;
  GeoCoordinate* end = cors->coordinates + n;

  double* dtr = cors->distances;

  for (;  ptr < end;  ++ptr, ++dtr, ++gtr) {
    gtr->_distance = *dtr;
    gtr->_data = ptr->data;
  }

  GeoIndex_CoordinatesFree(cors);

  // sort result by distance
  auto compareSort = [] (geo_coordinate_distance_t const& left, geo_coordinate_distance_t const& right) {
    return left._distance < right._distance;
  };
  std::sort(tmp, gnd, compareSort);

  // copy the documents
  bool error = false;
  uint32_t i;
  for (gtr = tmp, i = 0;  gtr < gnd;  ++gtr, ++i) {
    v8::Handle<v8::Value> doc = WRAP_SHAPED_JSON(trx, collection->_cid, ((TRI_doc_mptr_t const*) gtr->_data)->getDataPtr());

    if (doc.IsEmpty()) {
      error = true;
      break;
    }

    documents->Set(i, doc);
    distances->Set(i, v8::Number::New(isolate, gtr->_distance));
  }

  TRI_Free(TRI_UNKNOWN_MEM_ZONE, tmp);

  if (error) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  return TRI_ERROR_NO_ERROR;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   QUERY FUNCTIONS
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up edges for given direction
////////////////////////////////////////////////////////////////////////////////

static void EdgesQuery (TRI_edge_direction_e direction,
                                         const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  TRI_vocbase_col_t const* col = TRI_UnwrapClass<TRI_vocbase_col_t>(args.Holder(), TRI_GetVocBaseColType());

  if (col == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  TRI_THROW_SHARDING_COLLECTION_NOT_YET_IMPLEMENTED(col);

  if (col->_type != TRI_COL_TYPE_EDGE) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_COLLECTION_TYPE_INVALID);
  }

  SingleCollectionReadOnlyTransaction trx(new V8TransactionContext(true), col->_vocbase, col->_cid);

  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  TRI_document_collection_t* document = trx.documentCollection();

  // first and only argument schould be a list of document idenfifier
  if (args.Length() != 1) {
    switch (direction) {
      case TRI_EDGE_IN:
        TRI_V8_THROW_EXCEPTION_USAGE("inEdges(<vertices>)");

      case TRI_EDGE_OUT:
        TRI_V8_THROW_EXCEPTION_USAGE("outEdges(<vertices>)");

      case TRI_EDGE_ANY:
      default: {
        TRI_V8_THROW_EXCEPTION_USAGE("edges(<vertices>)");
      }
    }
  }

  // setup result
  v8::Handle<v8::Array> documents;

  // .............................................................................
  // inside a read transaction
  // .............................................................................

  trx.lockRead();

  bool error = false;

  if (trx.orderDitch(trx.trxCollection()) == nullptr) {
    TRI_V8_THROW_EXCEPTION_MEMORY();
  }

  // argument is a list of vertices
  if (args[0]->IsArray()) {
    documents = v8::Array::New(isolate);
    v8::Handle<v8::Array> vertices = v8::Handle<v8::Array>::Cast(args[0]);
    uint32_t count = 0;
    uint32_t const len = vertices->Length();

    for (uint32_t i = 0;  i < len; ++i) {
      TRI_voc_cid_t cid;
      std::unique_ptr<char[]> key;

      res = TRI_ParseVertex(args, trx.resolver(), cid, key, vertices->Get(i));

      if (res != TRI_ERROR_NO_ERROR) {
        // error is just ignored
        continue;
      }

      std::vector<TRI_doc_mptr_copy_t>&& edges = TRI_LookupEdgesDocumentCollection(document, direction, cid, key.get());

      for (size_t j = 0;  j < edges.size();  ++j) {
        v8::Handle<v8::Value> doc = WRAP_SHAPED_JSON(trx, col->_cid, edges[j].getDataPtr());

        if (doc.IsEmpty()) {
          // error
          error = true;
          break;
        }
        else {
          documents->Set(count, doc);
          ++count;
        }

      }

      if (error) {
        break;
      }
    }
    trx.finish(res);
  }

  // argument is a single vertex
  else {
    std::unique_ptr<char[]> key;
    TRI_voc_cid_t cid;

    res = TRI_ParseVertex(args, trx.resolver(), cid, key, args[0]);

    if (res != TRI_ERROR_NO_ERROR) {
      TRI_V8_THROW_EXCEPTION(res);
    }

    std::vector<TRI_doc_mptr_copy_t>&& edges = TRI_LookupEdgesDocumentCollection(document, direction, cid, key.get());

    trx.finish(res);

    size_t const n = edges.size();
    documents = v8::Array::New(isolate, static_cast<int>(n));

    for (size_t i = 0;  i < n;  ++i) {
      v8::Handle<v8::Value> doc = WRAP_SHAPED_JSON(trx, col->_cid, edges[i].getDataPtr());

      if (doc.IsEmpty()) {
        error = true;
        break;
      }
      else {
        documents->Set(static_cast<uint32_t>(i), doc);
      }
    }
  }

  // .............................................................................
  // outside a read transaction
  // .............................................................................

  if (error) {
    TRI_V8_THROW_EXCEPTION_MEMORY();
  }

  TRI_V8_RETURN(documents);
}

// -----------------------------------------------------------------------------
// --SECTION--                                              javascript functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief selects all documents from a collection
////////////////////////////////////////////////////////////////////////////////

static void JS_AllQuery (const v8::FunctionCallbackInfo<v8::Value>& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  // expecting two arguments
  if (args.Length() != 2) {
    TRI_V8_THROW_EXCEPTION_USAGE("ALL(<skip>, <limit>)");
  }

  TRI_vocbase_col_t const* col;
  col = TRI_UnwrapClass<TRI_vocbase_col_t>(args.Holder(), TRI_GetVocBaseColType());

  if (col == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  TRI_THROW_SHARDING_COLLECTION_NOT_YET_IMPLEMENTED(col);

  // extract skip and limit
  int64_t skip;
  uint64_t limit;
  ExtractSkipAndLimit(args, 0, skip, limit);

  uint64_t total = 0;
  vector<TRI_doc_mptr_copy_t> docs;

  SingleCollectionReadOnlyTransaction trx(new V8TransactionContext(true), col->_vocbase, col->_cid);

  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }
 
  res = trx.read(docs, skip, limit, &total);
  TRI_ASSERT(docs.empty() || trx.hasDitch());
  
  res = trx.finish(res);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  uint64_t const n = static_cast<uint64_t>(docs.size());

  // setup result
  v8::Handle<v8::Object> result = v8::Object::New(isolate);
  v8::Handle<v8::Array> documents = v8::Array::New(isolate, static_cast<int>(n));
  // reserve full capacity in one go
  result->Set(TRI_V8_ASCII_STRING("documents"), documents);

  for (uint64_t i = 0; i < n; ++i) {
    v8::Handle<v8::Value> doc = WRAP_SHAPED_JSON(trx, col->_cid, docs[i].getDataPtr());

    if (doc.IsEmpty()) {
      TRI_V8_THROW_EXCEPTION_MEMORY();
    }
    else {
      documents->Set(static_cast<uint32_t>(i), doc);
    }
  }

  result->Set(TRI_V8_ASCII_STRING("total"), v8::Number::New(isolate, static_cast<double>(total)));
  result->Set(TRI_V8_ASCII_STRING("count"), v8::Number::New(isolate, static_cast<double>(n)));
 
  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief selects documents from a collection, hashing the document key and
/// only returning these documents which fall into a specific partition
////////////////////////////////////////////////////////////////////////////////

static void JS_NthQuery (const v8::FunctionCallbackInfo<v8::Value>& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  // expecting two arguments
  if (args.Length() != 2 || ! args[0]->IsNumber() || ! args[1]->IsNumber()) {
    TRI_V8_THROW_EXCEPTION_USAGE("NTH(<partitionId>, <numberOfPartitions>)");
  }

  TRI_vocbase_col_t const* col = TRI_UnwrapClass<TRI_vocbase_col_t>(args.Holder(), TRI_GetVocBaseColType());

  if (col == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  TRI_THROW_SHARDING_COLLECTION_NOT_YET_IMPLEMENTED(col);

  uint64_t const partitionId = TRI_ObjectToUInt64(args[0], false);
  uint64_t const numberOfPartitions = TRI_ObjectToUInt64(args[1], false);

  if (partitionId >= numberOfPartitions || numberOfPartitions == 0) {
    TRI_V8_THROW_EXCEPTION_PARAMETER("invalid value for <partitionId> or <numberOfPartitions>");
  }
  
  uint64_t total = 0;
  vector<TRI_doc_mptr_copy_t> docs;

  SingleCollectionReadOnlyTransaction trx(new V8TransactionContext(true), col->_vocbase, col->_cid);

  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  res = trx.readPartition(docs, partitionId, numberOfPartitions, &total);
  TRI_ASSERT(docs.empty() || trx.hasDitch());

  res = trx.finish(res);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  size_t const n = docs.size();

  // setup result
  v8::Handle<v8::Object> result = v8::Object::New(isolate);
  v8::Handle<v8::Array> documents = v8::Array::New(isolate, (int) n);
  // reserve full capacity in one go
  result->Set(TRI_V8_ASCII_STRING("documents"), documents);

  for (size_t i = 0; i < n; ++i) {
    v8::Handle<v8::Value> doc = WRAP_SHAPED_JSON(trx, col->_cid, docs[i].getDataPtr());

    if (doc.IsEmpty()) {
      TRI_V8_THROW_EXCEPTION_MEMORY();
    }
    else {
      documents->Set(static_cast<uint32_t>(i), doc);
    }
  }

  result->Set(TRI_V8_ASCII_STRING("total"), v8::Number::New(isolate, static_cast<double>(total)));
  result->Set(TRI_V8_ASCII_STRING("count"), v8::Number::New(isolate, static_cast<double>(n)));

  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief selects documents from a collection, hashing the document key and
/// only returning these documents which fall into a specific partition
////////////////////////////////////////////////////////////////////////////////

static void JS_Nth2Query (const v8::FunctionCallbackInfo<v8::Value>& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  // expecting two arguments
  if (args.Length() != 2 || ! args[0]->IsNumber() || ! args[1]->IsNumber()) {
    TRI_V8_THROW_EXCEPTION_USAGE("NTH2(<partitionId>, <numberOfPartitions>)");
  }

  TRI_vocbase_col_t const* col = TRI_UnwrapClass<TRI_vocbase_col_t>(args.Holder(), TRI_GetVocBaseColType());

  if (col == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  TRI_THROW_SHARDING_COLLECTION_NOT_YET_IMPLEMENTED(col);

  uint64_t const partitionId = TRI_ObjectToUInt64(args[0], false);
  uint64_t const numberOfPartitions = TRI_ObjectToUInt64(args[1], false);

  if (partitionId >= numberOfPartitions || numberOfPartitions == 0) {
    TRI_V8_THROW_EXCEPTION_PARAMETER("invalid value for <partitionId> or <numberOfPartitions>");
  }
  
  uint64_t total = 0;
  vector<TRI_doc_mptr_copy_t> docs;

  SingleCollectionReadOnlyTransaction trx(new V8TransactionContext(true), col->_vocbase, col->_cid);

  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  res = trx.readPartition(docs, partitionId, numberOfPartitions, &total);
  TRI_ASSERT(docs.empty() || trx.hasDitch());

  res = trx.finish(res);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  size_t const n = docs.size();

  // setup result
  v8::Handle<v8::Object> result = v8::Object::New(isolate);
  v8::Handle<v8::Array> documents = v8::Array::New(isolate, (int) n);
  // reserve full capacity in one go
  result->Set(TRI_V8_ASCII_STRING("documents"), documents);

  for (size_t i = 0; i < n; ++i) {
    char const* key = TRI_EXTRACT_MARKER_KEY(static_cast<TRI_df_marker_t const*>(docs[i].getDataPtr()));
    documents->Set(static_cast<uint32_t>(i), TRI_V8_STRING(key));
  }

  result->Set(TRI_V8_ASCII_STRING("total"), v8::Number::New(isolate, static_cast<double>(total)));
  result->Set(TRI_V8_ASCII_STRING("count"), v8::Number::New(isolate, static_cast<double>(n)));

  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief selects documents from a collection, using an offset into the
/// primary index. this can be used for incremental access
////////////////////////////////////////////////////////////////////////////////

static void JS_OffsetQuery (const v8::FunctionCallbackInfo<v8::Value>& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  // expecting two arguments
  if (args.Length() != 4) {
    TRI_V8_THROW_EXCEPTION_USAGE("OFFSET(<internalSkip>, <batchSize>, <skip>, <limit>)");
  }

  TRI_vocbase_col_t const* col = TRI_UnwrapClass<TRI_vocbase_col_t>(args.Holder(), TRI_GetVocBaseColType());

  if (col == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  TRI_THROW_SHARDING_COLLECTION_NOT_YET_IMPLEMENTED(col);

  uint64_t internalSkip = TRI_ObjectToUInt64(args[0], false);
  uint64_t batchSize    = TRI_ObjectToUInt64(args[1], false);

  // extract skip and limit
  int64_t skip;
  uint64_t limit;
  ExtractSkipAndLimit(args, 2, skip, limit);

  uint64_t total = 0;
  vector<TRI_doc_mptr_copy_t> docs;

  SingleCollectionReadOnlyTransaction trx(new V8TransactionContext(true), col->_vocbase, col->_cid);

  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  res = trx.readOffset(docs, internalSkip, batchSize, skip, limit, &total);
  TRI_ASSERT(docs.empty() || trx.hasDitch());

  res = trx.finish(res);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  size_t const n = docs.size();

  // setup result
  v8::Handle<v8::Object> result = v8::Object::New(isolate);
  v8::Handle<v8::Array> documents = v8::Array::New(isolate, static_cast<int>(n));
  // reserve full capacity in one go
  result->Set(TRI_V8_ASCII_STRING("documents"), documents);

  for (size_t i = 0; i < n; ++i) {
    v8::Handle<v8::Value> document = WRAP_SHAPED_JSON(trx, col->_cid, docs[i].getDataPtr());

    if (document.IsEmpty()) {
      TRI_V8_THROW_EXCEPTION_MEMORY();
    }
    else {
      documents->Set(static_cast<uint32_t>(i), document);
    }
  }

  result->Set(TRI_V8_ASCII_STRING("total"), v8::Number::New(isolate, static_cast<double>(total)));
  result->Set(TRI_V8_ASCII_STRING("count"), v8::Number::New(isolate, static_cast<double>(n)));
  result->Set(TRI_V8_ASCII_STRING("skip"), v8::Number::New(isolate, internalSkip));

  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief selects a random document
///
/// @FUN{@FA{collection}.any()}
///
/// The @FN{any} method returns a random document from the collection.  It returns
/// @LIT{null} if the collection is empty.
///
/// @EXAMPLES
///
/// @code
/// arangod> db.example.any()
/// { "_id" : "example/222716379559", "_rev" : "222716379559", "Hello" : "World" }
/// @endcode
////////////////////////////////////////////////////////////////////////////////

static void JS_AnyQuery (const v8::FunctionCallbackInfo<v8::Value>& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  TRI_vocbase_col_t const* col = TRI_UnwrapClass<TRI_vocbase_col_t>(args.Holder(), TRI_GetVocBaseColType());

  if (col == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  TRI_doc_mptr_copy_t document;

  SingleCollectionReadOnlyTransaction trx(new V8TransactionContext(true), col->_vocbase, col->_cid);

  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  res = trx.readRandom(&document);
  TRI_ASSERT(document.getDataPtr() == nullptr || trx.hasDitch());

  res = trx.finish(res);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  if (document.getDataPtr() == nullptr) {  // PROTECTED by trx here
    TRI_V8_RETURN_NULL();
  }

  v8::Handle<v8::Value> doc = WRAP_SHAPED_JSON(trx, col->_cid, document.getDataPtr());

  if (doc.IsEmpty()) {
    TRI_V8_THROW_EXCEPTION_MEMORY();
  }

  TRI_V8_RETURN(doc);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief selects documents by example (not using any index)
////////////////////////////////////////////////////////////////////////////////

static void JS_ByExampleQuery (const v8::FunctionCallbackInfo<v8::Value>& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  // expecting example, skip, limit
  if (args.Length() < 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("BY_EXAMPLE(<example>, <skip>, <limit>)");
  }

  // extract the example
  if (! args[0]->IsObject()) {
    TRI_V8_THROW_TYPE_ERROR("<example> must be an object");
  }

  TRI_vocbase_col_t const* col = TRI_UnwrapClass<TRI_vocbase_col_t>(args.Holder(), TRI_GetVocBaseColType());

  if (col == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  TRI_THROW_SHARDING_COLLECTION_NOT_YET_IMPLEMENTED(col);

  SingleCollectionReadOnlyTransaction trx(new V8TransactionContext(true), col->_vocbase, col->_cid);

  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  TRI_document_collection_t* document = trx.documentCollection();
  auto shaper = document->getShaper();  // PROTECTED by trx here

  v8::Handle<v8::Object> example = args[0]->ToObject();

  // extract skip and limit
  int64_t skip;
  uint64_t limit;
  ExtractSkipAndLimit(args, 1, skip, limit);

  // extract sub-documents

  if (trx.orderDitch(trx.trxCollection()) == nullptr) {
    TRI_V8_THROW_EXCEPTION_MEMORY();
  }

  std::string errorMessage;
  std::unique_ptr<ExampleMatcher> matcher;
  try {
    matcher.reset(new ExampleMatcher(isolate, example, shaper, errorMessage));
  } 
  catch (Exception& e) {
    if (e.code() == TRI_RESULT_ELEMENT_NOT_FOUND) {
      // empty result
      TRI_V8_RETURN(EmptyResult(isolate));
    }
    if (errorMessage.empty()) {
      TRI_V8_THROW_EXCEPTION(e.code());
    }
    TRI_V8_THROW_EXCEPTION_MESSAGE(e.code(), errorMessage);
  }

  // setup result
  v8::Handle<v8::Object> result = v8::Object::New(isolate);
  v8::Handle<v8::Array> documents = v8::Array::New(isolate);
  result->Set(TRI_V8_ASCII_STRING("documents"), documents);

  // ...........................................................................
  // inside a read transaction
  // ...........................................................................

  vector<TRI_doc_mptr_copy_t> filtered;

  trx.lockRead();

  // find documents by example
  try {
    filtered = TRI_SelectByExample(trx.trxCollection(), *matcher.get());
  }
  catch (std::exception&) {
    TRI_V8_THROW_EXCEPTION_MEMORY();
  }

  trx.finish(res);

  // ...........................................................................
  // outside a read transaction
  // ...........................................................................

  // convert to list of shaped jsons
  uint64_t const total = static_cast<uint64_t>(filtered.size());
  uint64_t count = 0;
  bool error = false;

  if (0 < total) {
    uint64_t s, e;
    CalculateSkipLimitSlice(filtered.size(), skip, limit, s, e);

    if (s < e) {
      for (uint64_t j = s; j < e; ++j) {
        v8::Handle<v8::Value> doc = WRAP_SHAPED_JSON(trx, col->_cid, filtered[j].getDataPtr());

        if (doc.IsEmpty()) {
          error = true;
          break;
        }
        else {
          documents->Set(static_cast<uint32_t>(count++), doc);
        }
      }
    }
  }

  result->Set(TRI_V8_ASCII_STRING("total"), v8::Integer::New(isolate, static_cast<double>(total)));
  result->Set(TRI_V8_ASCII_STRING("count"), v8::Integer::New(isolate, static_cast<double>(count)));

  if (error) {
    TRI_V8_THROW_EXCEPTION_MEMORY();
  }

  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief selects documents by example using a hash index
///
/// It is the callers responsibility to acquire and free the required locks
////////////////////////////////////////////////////////////////////////////////

static void ByExampleHashIndexQuery (SingleCollectionReadOnlyTransaction& trx,
                                     TRI_vocbase_col_t const* collection,
                                     const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  // expecting index, example, skip, and limit
  if (args.Length() < 2) {
    TRI_V8_THROW_EXCEPTION_USAGE("EXAMPLE_HASH(<index>, <example>, <skip>, <limit>)");
  }

  // extract the example
  if (! args[1]->IsObject()) {
    TRI_V8_THROW_TYPE_ERROR("<example> must be an object");
  }

  v8::Handle<v8::Object> example = args[1]->ToObject();

  if (trx.orderDitch(trx.trxCollection()) == nullptr) {
    TRI_V8_THROW_EXCEPTION_MEMORY();
  }

  // extract skip and limit
  int64_t skip;
  uint64_t limit;
  ExtractSkipAndLimit(args, 2, skip, limit);

  // setup result
  v8::Handle<v8::Object> result = v8::Object::New(isolate);

  v8::Handle<v8::Array> documents = v8::Array::New(isolate);
  result->Set(TRI_V8_ASCII_STRING("documents"), documents);

  // extract the index
  auto idx = TRI_LookupIndexByHandle(isolate, trx.resolver(), collection, args[0], false);

  if (idx == nullptr || 
      idx->type() != triagens::arango::Index::TRI_IDX_TYPE_HASH_INDEX) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_NO_INDEX);
  }

  auto hashIndex = static_cast<triagens::arango::HashIndex*>(idx);

  // convert the example (index is locked by lockRead)
  TRI_index_search_value_t searchValue;

  TRI_document_collection_t* document = trx.documentCollection();
  auto shaper = document->getShaper();  // PROTECTED by trx from above
  {
    std::string errorMessage;
    int res = SetupSearchValue(hashIndex->paths(), example, shaper, searchValue, errorMessage, args);

    if (res != TRI_ERROR_NO_ERROR) {
      if (res == TRI_RESULT_ELEMENT_NOT_FOUND) {
        TRI_V8_RETURN(EmptyResult(isolate));
      }
      if (errorMessage.empty()) {
        TRI_V8_THROW_EXCEPTION(res);
      }
      TRI_V8_THROW_EXCEPTION_MESSAGE(res, errorMessage);
    }
  }

  // find the matches
  TRI_vector_pointer_t list = static_cast<triagens::arango::HashIndex*>(idx)->lookup(&searchValue);
  DestroySearchValue(shaper->memoryZone(), searchValue);

  // convert result
  uint64_t total = static_cast<size_t>(TRI_LengthVectorPointer(&list));
  uint64_t count = 0;
  bool error = false;

  if (0 < total) {
    uint64_t s, e;
    CalculateSkipLimitSlice(total, skip, limit, s, e);

    if (s < e) {
      for (uint64_t i = s;  i < e;  ++i) {
        v8::Handle<v8::Value> doc = WRAP_SHAPED_JSON(trx,
                                                     collection->_cid,
                                                     static_cast<TRI_doc_mptr_t*>(TRI_AtVectorPointer(&list, static_cast<size_t>(i)))->getDataPtr());

        if (doc.IsEmpty()) {
          error = true;
          break;
        }
        else {
          documents->Set(static_cast<uint32_t>(count++), doc);
        }
      }
    }
  }

  // free data allocated by hash index result
  TRI_DestroyVectorPointer(&list);

  result->Set(TRI_V8_ASCII_STRING("total"), v8::Number::New(isolate, static_cast<double>(total)));
  result->Set(TRI_V8_ASCII_STRING("count"), v8::Number::New(isolate, static_cast<double>(count)));

  if (error) {
    TRI_V8_THROW_EXCEPTION_MEMORY();
  }

  TRI_V8_RETURN(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief selects documents by example using a hash index
////////////////////////////////////////////////////////////////////////////////

static void JS_ByExampleHashIndex (const v8::FunctionCallbackInfo<v8::Value>& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  TRI_vocbase_col_t const* col = TRI_UnwrapClass<TRI_vocbase_col_t>(args.Holder(), TRI_GetVocBaseColType());

  if (col == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  TRI_THROW_SHARDING_COLLECTION_NOT_YET_IMPLEMENTED(col);

  SingleCollectionReadOnlyTransaction trx(new V8TransactionContext(true), col->_vocbase, col->_cid);

  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  // .............................................................................
  // inside a read transaction
  // .............................................................................

  trx.lockRead();

  ByExampleHashIndexQuery(trx, col, args);

  trx.finish(res);

  // .............................................................................
  // outside a write transaction
  // .............................................................................

  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief selects documents by condition using a skiplist index
////////////////////////////////////////////////////////////////////////////////

static void JS_ByConditionSkiplist (const v8::FunctionCallbackInfo<v8::Value>& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  std::string const signature("BY_CONDITION_SKIPLIST(<index>, <conditions>, <skip>, <limit>, <reverse>)");

  return ExecuteSkiplistQuery(args, signature, QUERY_CONDITION);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief selects documents by example using a skiplist index
////////////////////////////////////////////////////////////////////////////////

static void JS_ByExampleSkiplist (const v8::FunctionCallbackInfo<v8::Value>& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  std::string const signature("BY_EXAMPLE_SKIPLIST(<index>, <example>, <skip>, <limit>, <reverse>)");

  return ExecuteSkiplistQuery(args, signature, QUERY_EXAMPLE);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief helper struct used when calculting checksums
////////////////////////////////////////////////////////////////////////////////

struct collection_checksum_t {
  collection_checksum_t (CollectionNameResolver const* resolver)
    : _resolver(resolver),
      _checksum(0) {
  }

  CollectionNameResolver const*  _resolver;
  TRI_string_buffer_t            _buffer;
  uint32_t                       _checksum;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief callback for checksum calculation, WR = with _rid, WD = with data
////////////////////////////////////////////////////////////////////////////////

template<bool WR, bool WD> static bool ChecksumCalculator (TRI_doc_mptr_t const* mptr,
                                                           TRI_document_collection_t* document,
                                                           void* data) {
  // This callback is only called in TRI_DocumentIteratorDocumentCollection
  // and there we have an ongoing transaction. Therefore all master pointer
  // and data pointer accesses here are safe!
  TRI_df_marker_t const* marker = static_cast<TRI_df_marker_t const*>(mptr->getDataPtr());  // PROTECTED by trx in calling function TRI_DocumentIteratorDocumentCollection
  collection_checksum_t* helper = static_cast<collection_checksum_t*>(data);
  uint32_t localCrc;

  if (marker->_type == TRI_DOC_MARKER_KEY_DOCUMENT ||
      marker->_type == TRI_WAL_MARKER_DOCUMENT) {
    localCrc = TRI_Crc32HashString(TRI_EXTRACT_MARKER_KEY(mptr));  // PROTECTED by trx in calling function TRI_DocumentIteratorDocumentCollection
    if (WR) {
      localCrc += TRI_Crc32HashPointer(&mptr->_rid, sizeof(TRI_voc_rid_t));
    }
  }
  else if (marker->_type == TRI_DOC_MARKER_KEY_EDGE ||
           marker->_type == TRI_WAL_MARKER_EDGE) {
    // must convert _rid, _fromCid, _toCid into strings for portability
    localCrc = TRI_Crc32HashString(TRI_EXTRACT_MARKER_KEY(mptr));  // PROTECTED by trx in calling function TRI_DocumentIteratorDocumentCollection
    if (WR) {
      localCrc += TRI_Crc32HashPointer(&mptr->_rid, sizeof(TRI_voc_rid_t));
    }

    if (marker->_type == TRI_DOC_MARKER_KEY_EDGE) {
      TRI_doc_edge_key_marker_t const* e = reinterpret_cast<TRI_doc_edge_key_marker_t const*>(marker);
      string const extra = helper->_resolver->getCollectionNameCluster(e->_toCid) + TRI_DOCUMENT_HANDLE_SEPARATOR_CHR + string(((char*) marker) + e->_offsetToKey) +
                           helper->_resolver->getCollectionNameCluster(e->_fromCid) + TRI_DOCUMENT_HANDLE_SEPARATOR_CHR + string(((char*) marker) + e->_offsetFromKey);

      localCrc += TRI_Crc32HashPointer(extra.c_str(), extra.size());
    }
    else {
      triagens::wal::edge_marker_t const* e = reinterpret_cast<triagens::wal::edge_marker_t const*>(marker);
      string const extra = helper->_resolver->getCollectionNameCluster(e->_toCid) + TRI_DOCUMENT_HANDLE_SEPARATOR_CHR + string(((char*) marker) + e->_offsetToKey) +
                           helper->_resolver->getCollectionNameCluster(e->_fromCid) + TRI_DOCUMENT_HANDLE_SEPARATOR_CHR + string(((char*) marker) + e->_offsetFromKey);

      localCrc += TRI_Crc32HashPointer(extra.c_str(), extra.size());
    }
  }
  else {
    return true;
  }

  if (WD) {
    // with data
    void const* d = static_cast<void const*>(marker);

    TRI_shaped_json_t shaped;
    TRI_EXTRACT_SHAPED_JSON_MARKER(shaped, d);

    TRI_StringifyArrayShapedJson(document->getShaper(), &helper->_buffer, &shaped, false);  // PROTECTED by trx in calling function TRI_DocumentIteratorDocumentCollection
    localCrc += TRI_Crc32HashPointer(TRI_BeginStringBuffer(&helper->_buffer), TRI_LengthStringBuffer(&helper->_buffer));
    TRI_ResetStringBuffer(&helper->_buffer);
  }

  helper->_checksum += localCrc;

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief calculates a checksum for the data in a collection
/// @startDocuBlock collectionChecksum
/// `collection.checksum(withRevisions, withData)`
///
/// The *checksum* operation calculates a CRC32 checksum of the keys
/// contained in collection *collection*.
///
/// If the optional argument *withRevisions* is set to *true*, then the
/// revision ids of the documents are also included in the checksumming.
///
/// If the optional argument *withData* is set to *true*, then the
/// actual document data is also checksummed. Including the document data in
/// checksumming will make the calculation slower, but is more accurate.
///
/// **Note**: this method is not available in a cluster.
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

static void JS_ChecksumCollection (const v8::FunctionCallbackInfo<v8::Value>& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (ServerState::instance()->isCoordinator()) {
    // renaming a collection in a cluster is unsupported
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_CLUSTER_UNSUPPORTED);
  }

  TRI_vocbase_col_t const* col = TRI_UnwrapClass<TRI_vocbase_col_t>(args.Holder(), TRI_GetVocBaseColType());

  if (col == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  TRI_THROW_SHARDING_COLLECTION_NOT_YET_IMPLEMENTED(col);

  bool withRevisions = false;
  if (args.Length() > 0) {
    withRevisions = TRI_ObjectToBoolean(args[0]);
  }

  bool withData = false;
  if (args.Length() > 1) {
    withData = TRI_ObjectToBoolean(args[1]);
  }

  SingleCollectionReadOnlyTransaction trx(new V8TransactionContext(true), col->_vocbase, col->_cid);

  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  TRI_document_collection_t* document = trx.documentCollection();

  if (trx.orderDitch(trx.trxCollection()) == nullptr) {
    TRI_V8_THROW_EXCEPTION_MEMORY();
  }

  collection_checksum_t helper(trx.resolver());

  // .............................................................................
  // inside a read transaction
  // .............................................................................

  trx.lockRead();
  // get last tick
  string const rid = StringUtils::itoa(document->_info._revision);

  if (withData) {
    TRI_InitStringBuffer(&helper._buffer, TRI_CORE_MEM_ZONE);

    if (withRevisions) {
      TRI_DocumentIteratorDocumentCollection(&trx, document, &helper, &ChecksumCalculator<true, true>);
    }
    else {
      TRI_DocumentIteratorDocumentCollection(&trx, document, &helper, &ChecksumCalculator<false, true>);
    }

    TRI_DestroyStringBuffer(&helper._buffer);
  }
  else {
    if (withRevisions) {
      TRI_DocumentIteratorDocumentCollection(&trx, document, &helper, &ChecksumCalculator<true, false>);
    }
    else {
      TRI_DocumentIteratorDocumentCollection(&trx, document, &helper, &ChecksumCalculator<false, false>);
    }
  }

  trx.finish(res);

  // .............................................................................
  // outside a write transaction
  // .............................................................................

  v8::Handle<v8::Object> result = v8::Object::New(isolate);
  result->Set(TRI_V8_ASCII_STRING("checksum"), v8::Number::New(isolate, helper._checksum));
  result->Set(TRI_V8_ASCII_STRING("revision"), TRI_V8_STD_STRING(rid));

  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief selects all edges for a set of vertices
/// @startDocuBlock edgeCollectionEdges
/// `edge-collection.edges(vertex)`
///
/// The *edges* operator finds all edges starting from (outbound) or ending
/// in (inbound) *vertex*.
///
/// `edge-collection.edges(vertices)`
///
/// The *edges* operator finds all edges starting from (outbound) or ending
/// in (inbound) a document from *vertices*, which must a list of documents
/// or document handles.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{EDGCOL_02_Relation}
///   db._create("vertex");
///   db._createEdgeCollection("relation");
/// ~ var myGraph = {};
///   myGraph.v1 = db.vertex.insert({ name : "vertex 1" });
///   myGraph.v2 = db.vertex.insert({ name : "vertex 2" });
///   myGraph.e1 = db.relation.insert(myGraph.v1, myGraph.v2, { label : "knows" });
///   db._document(myGraph.e1);
///   db.relation.edges(myGraph.e1._id);
/// ~ db._drop("relation");
/// ~ db._drop("vertex");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

static void JS_EdgesQuery (const v8::FunctionCallbackInfo<v8::Value>& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  return EdgesQuery(TRI_EDGE_ANY, args);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief selects all inbound edges
/// @startDocuBlock edgeCollectionInEdges
/// `edge-collection.inEdges(vertex)`
///
/// The *edges* operator finds all edges ending in (inbound) *vertex*.
///
/// `edge-collection.inEdges(vertices)`
///
/// The *edges* operator finds all edges ending in (inbound) a document from
/// *vertices*, which must a list of documents or document handles.
///
/// @EXAMPLES
/// @EXAMPLE_ARANGOSH_OUTPUT{EDGCOL_02_inEdges}
///   db._create("vertex");
///   db._createEdgeCollection("relation");
/// ~ var myGraph = {};
///   myGraph.v1 = db.vertex.insert({ name : "vertex 1" });
///   myGraph.v2 = db.vertex.insert({ name : "vertex 2" });
///   myGraph.e1 = db.relation.insert(myGraph.v1, myGraph.v2, { label : "knows" });
///   db._document(myGraph.e1);
///   db.relation.inEdges(myGraph.v1._id);
///   db.relation.inEdges(myGraph.v2._id);
/// ~ db._drop("relation");
/// ~ db._drop("vertex");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

static void JS_InEdgesQuery (const v8::FunctionCallbackInfo<v8::Value>& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  return EdgesQuery(TRI_EDGE_IN, args);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief selects all outbound edges
/// @startDocuBlock edgeCollectionOutEdges
/// `edge-collection.outEdges(vertex)`
///
/// The *edges* operator finds all edges starting from (outbound)
/// *vertices*.
///
/// `edge-collection.outEdges(vertices)`
///
/// The *edges* operator finds all edges starting from (outbound) a document
/// from *vertices*, which must a list of documents or document handles.
///
/// @EXAMPLES
/// @EXAMPLE_ARANGOSH_OUTPUT{EDGCOL_02_outEdges}
///   db._create("vertex");
///   db._createEdgeCollection("relation");
/// ~ var myGraph = {};
///   myGraph.v1 = db.vertex.insert({ name : "vertex 1" });
///   myGraph.v2 = db.vertex.insert({ name : "vertex 2" });
///   myGraph.e1 = db.relation.insert(myGraph.v1, myGraph.v2, { label : "knows" });
///   db._document(myGraph.e1);
///   db.relation.outEdges(myGraph.v1._id);
///   db.relation.outEdges(myGraph.v2._id);
/// ~ db._drop("relation");
/// ~ db._drop("vertex");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

static void JS_OutEdgesQuery (const v8::FunctionCallbackInfo<v8::Value>& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  return EdgesQuery(TRI_EDGE_OUT, args);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief selects the n first documents in the collection
////////////////////////////////////////////////////////////////////////////////

static void JS_FirstQuery (const v8::FunctionCallbackInfo<v8::Value>& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() > 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("FIRST(<count>)");
  }

  int64_t count = 1;
  bool returnList = false;

  // if argument is supplied, we'll return a list - otherwise we simply return the first doc
  if (args.Length() == 1) {
    if (! args[0]->IsUndefined()) {
      count = TRI_ObjectToInt64(args[0]);
      returnList = true;
    }
  }

  if (count < 1) {
    TRI_V8_THROW_EXCEPTION_PARAMETER("invalid value for <count>");
  }

  TRI_vocbase_col_t const* col;
  col = TRI_UnwrapClass<TRI_vocbase_col_t>(args.Holder(), TRI_GetVocBaseColType());

  if (col == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  SingleCollectionReadOnlyTransaction trx(new V8TransactionContext(true), col->_vocbase, col->_cid);

  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  std::vector<TRI_doc_mptr_copy_t> documents;
  res = trx.readPositional(documents, 0, count);
  trx.finish(res);

  size_t const n = documents.size();

  if (returnList) {
    v8::Handle<v8::Array> result = v8::Array::New(isolate, static_cast<int>(n));

    for (size_t i = 0; i < n; ++i) {
      v8::Handle<v8::Value> doc = WRAP_SHAPED_JSON(trx, col->_cid, documents[i].getDataPtr());

      if (doc.IsEmpty()) {
        // error
        TRI_V8_THROW_EXCEPTION_MEMORY();
      }

      result->Set(static_cast<uint32_t>(i), doc);
    }

    TRI_V8_RETURN(result);
  }
  else {
    if (n == 0) {
      TRI_V8_RETURN_NULL();
    }

    v8::Handle<v8::Value> result = WRAP_SHAPED_JSON(trx, col->_cid, documents[0].getDataPtr());

    if (result.IsEmpty()) {
      TRI_V8_THROW_EXCEPTION_MEMORY();
    }

    TRI_V8_RETURN(result);
  }
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief queries the fulltext index
///
/// the caller must ensure all relevant locks are acquired and freed
////////////////////////////////////////////////////////////////////////////////

static void FulltextQuery (SingleCollectionReadOnlyTransaction& trx,
                           TRI_vocbase_col_t const* collection,
                           const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  // expect: FULLTEXT(<index-handle>, <query>, <limit>)
  if (args.Length() < 2) {
    TRI_V8_THROW_EXCEPTION_USAGE("FULLTEXT(<index-handle>, <query>, <limit>)");
  }

  // extract the index
  auto idx = TRI_LookupIndexByHandle(isolate, trx.resolver(), collection, args[0], false);

  if (idx == nullptr || 
      idx->type() != triagens::arango::Index::TRI_IDX_TYPE_FULLTEXT_INDEX) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_NO_INDEX);
  }

  string const&& queryString = TRI_ObjectToString(args[1]);
  bool isSubstringQuery = false;
  size_t maxResults = 0; // 0 means "all results"

  if (args.Length() >= 3 && args[2]->IsNumber()) {
    int64_t value = TRI_ObjectToInt64(args[2]);
    if (value > 0) {
      maxResults = static_cast<size_t>(value);
    }
  }

  TRI_fulltext_query_t* query = TRI_CreateQueryFulltextIndex(TRI_FULLTEXT_SEARCH_MAX_WORDS, maxResults);

  if (! query) {
    TRI_V8_THROW_EXCEPTION_MEMORY();
  }

  int res = TRI_ParseQueryFulltextIndex(query, queryString.c_str(), &isSubstringQuery);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_FreeQueryFulltextIndex(query);

    TRI_V8_THROW_EXCEPTION(res);
  }

  auto fulltextIndex = static_cast<triagens::arango::FulltextIndex*>(idx);

  if (isSubstringQuery) {
    TRI_FreeQueryFulltextIndex(query);

    TRI_V8_THROW_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }

  TRI_fulltext_result_t* queryResult = TRI_QueryFulltextIndex(fulltextIndex->internals(), query);

  if (! queryResult) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("internal error in fulltext index query");
  }

  if (trx.orderDitch(trx.trxCollection()) == nullptr) {
    TRI_V8_THROW_EXCEPTION_MEMORY();
  }

  // setup result
  v8::Handle<v8::Object> result = v8::Object::New(isolate);

  v8::Handle<v8::Array> documents = v8::Array::New(isolate);
  result->Set(TRI_V8_ASCII_STRING("documents"), documents);

  bool error = false;

  for (uint32_t i = 0; i < queryResult->_numDocuments; ++i) {
    v8::Handle<v8::Value> doc = WRAP_SHAPED_JSON(trx, collection->_cid, ((TRI_doc_mptr_t const*) queryResult->_documents[i])->getDataPtr());

    if (doc.IsEmpty()) {
      error = true;
      break;
    }

    documents->Set(i, doc);
  }

  TRI_FreeResultFulltextIndex(queryResult);

  if (error) {
    TRI_V8_THROW_EXCEPTION_MEMORY();
  }

  TRI_V8_RETURN(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief queries the fulltext index
/// @startDocuBlock collectionFulltext
/// `collection.fulltext(attribute, query)`
///
/// The *fulltext* simple query functions performs a fulltext search on the specified
/// *attribute* and the specified *query*.
///
/// Details about the fulltext query syntax can be found below.
///
/// Note: the *fulltext* simple query function is **deprecated** as of ArangoDB 2.6. 
/// The function may be removed in future versions of ArangoDB. The preferred
/// way for executing fulltext queries is to use an AQL query using the *FULLTEXT*
/// [AQL function](../Aql/FulltextFunctions.md) as follows:
///
///     FOR doc IN FULLTEXT(@@collection, @attributeName, @queryString, @limit) 
///       RETURN doc
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_OUTPUT{collectionFulltext}
/// ~ db._drop("emails");
/// ~ db._create("emails");
///   db.emails.ensureFulltextIndex("content");
///   db.emails.save({ content: "Hello Alice, how are you doing? Regards, Bob" });
///   db.emails.save({ content: "Hello Charlie, do Alice and Bob know about it?" });
///   db.emails.save({ content: "I think they don't know. Regards, Eve" });
///   db.emails.fulltext("content", "charlie,|eve").toArray();
/// ~ db._drop("emails");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

static void JS_FulltextQuery (const v8::FunctionCallbackInfo<v8::Value>& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  TRI_vocbase_col_t const* col = TRI_UnwrapClass<TRI_vocbase_col_t>(args.Holder(), TRI_GetVocBaseColType());

  if (col == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  TRI_THROW_SHARDING_COLLECTION_NOT_YET_IMPLEMENTED(col);

  SingleCollectionReadOnlyTransaction trx(new V8TransactionContext(true), col->_vocbase, col->_cid);

  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  // .............................................................................
  // inside a read transaction
  // .............................................................................

  trx.lockRead();

  FulltextQuery(trx, col, args);

  trx.finish(res);

  // .............................................................................
  // outside a write transaction
  // .............................................................................

  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief selects the n last documents in the collection
////////////////////////////////////////////////////////////////////////////////

static void JS_LastQuery (const v8::FunctionCallbackInfo<v8::Value>& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() > 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("LAST(<count>)");
  }

  int64_t count = 1;
  bool returnList = false;

  // if argument is supplied, we'll return a list - otherwise we simply return the last doc
  if (args.Length() == 1) {
    if (! args[0]->IsUndefined()) {
      count = TRI_ObjectToInt64(args[0]);
      returnList = true;
    }
  }

  if (count < 1) {
    TRI_V8_THROW_EXCEPTION_PARAMETER("invalid value for <count>");
  }

  TRI_vocbase_col_t const* col = TRI_UnwrapClass<TRI_vocbase_col_t>(args.Holder(), TRI_GetVocBaseColType());

  if (col == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  TRI_THROW_SHARDING_COLLECTION_NOT_YET_IMPLEMENTED(col);

  SingleCollectionReadOnlyTransaction trx(new V8TransactionContext(true), col->_vocbase, col->_cid);

  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  vector<TRI_doc_mptr_copy_t> documents;
  res = trx.readPositional(documents, -1, count);
  trx.finish(res);

  uint64_t const n = static_cast<uint64_t>(documents.size());

  if (returnList) {
    v8::Handle<v8::Array> result = v8::Array::New(isolate, static_cast<int>(n));

    for (uint64_t i = 0; i < n; ++i) {
      v8::Handle<v8::Value> doc = WRAP_SHAPED_JSON(trx, col->_cid, documents[i].getDataPtr());

      if (doc.IsEmpty()) {
        // error
        TRI_V8_THROW_EXCEPTION_MEMORY();
      }

      result->Set(static_cast<uint32_t>(i), doc);
    }

    TRI_V8_RETURN(result);
  }
  else {
    if (n == 0) {
      TRI_V8_RETURN_NULL();
    }

    v8::Handle<v8::Value> result = WRAP_SHAPED_JSON(trx, col->_cid, documents[0].getDataPtr());

    if (result.IsEmpty()) {
      TRI_V8_THROW_EXCEPTION_MEMORY();
    }

    TRI_V8_RETURN(result);
  }
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief selects points near a given coordinate
///
/// the caller must ensure all relevant locks are acquired and freed
////////////////////////////////////////////////////////////////////////////////

static void NearQuery (SingleCollectionReadOnlyTransaction& trx,
                       TRI_vocbase_col_t const* collection,
                       const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  // expect: NEAR(<index-id>, <latitude>, <longitude>, <limit>)
  if (args.Length() != 4) {
    TRI_V8_THROW_EXCEPTION_USAGE("NEAR(<index-handle>, <latitude>, <longitude>, <limit>)");
  }

  // extract the index
  auto idx = TRI_LookupIndexByHandle(isolate, trx.resolver(), collection, args[0], false);

  if (idx == nullptr ||
      (idx->type() != triagens::arango::Index::TRI_IDX_TYPE_GEO1_INDEX &&
       idx->type() != triagens::arango::Index::TRI_IDX_TYPE_GEO2_INDEX)) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_NO_INDEX);
  }

  // extract latitude and longitude
  double latitude = TRI_ObjectToDouble(args[1]);
  double longitude = TRI_ObjectToDouble(args[2]);

  // extract the limit
  int64_t limit = TRI_ObjectToInt64(args[3]);

  // setup result
  v8::Handle<v8::Object> result = v8::Object::New(isolate);

  v8::Handle<v8::Array> documents = v8::Array::New(isolate);
  result->Set(TRI_V8_ASCII_STRING("documents"), documents);

  v8::Handle<v8::Array> distances = v8::Array::New(isolate);
  result->Set(TRI_V8_ASCII_STRING("distances"), distances);

  GeoCoordinates* cors = static_cast<triagens::arango::GeoIndex2*>(idx)->nearQuery(latitude, longitude, limit);

  if (cors != nullptr) {
    int res = StoreGeoResult(isolate, trx, collection, cors, documents, distances);

    if (res != TRI_ERROR_NO_ERROR) {
      TRI_V8_THROW_EXCEPTION(res);
    }
  }

  TRI_V8_RETURN(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief selects points near a given coordinate
////////////////////////////////////////////////////////////////////////////////

static void JS_NearQuery (const v8::FunctionCallbackInfo<v8::Value>& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  TRI_vocbase_col_t const* col = TRI_UnwrapClass<TRI_vocbase_col_t>(args.Holder(), TRI_GetVocBaseColType());

  if (col == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  TRI_THROW_SHARDING_COLLECTION_NOT_YET_IMPLEMENTED(col);

  SingleCollectionReadOnlyTransaction trx(new V8TransactionContext(true), col->_vocbase, col->_cid);

  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  // .............................................................................
  // inside a read transaction
  // .............................................................................

  trx.lockRead();

  NearQuery(trx, col, args);

  trx.finish(res);

  // .............................................................................
  // outside a write transaction
  // .............................................................................

  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief selects points within a given radius
///
/// the caller must ensure all relevant locks are acquired and freed
////////////////////////////////////////////////////////////////////////////////

static void WithinQuery (SingleCollectionReadOnlyTransaction& trx,
                         TRI_vocbase_col_t const* collection,
                         const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  // expect: WITHIN(<index-handle>, <latitude>, <longitude>, <radius>)
  if (args.Length() != 4) {
    TRI_V8_THROW_EXCEPTION_USAGE("WITHIN(<index-handle>, <latitude>, <longitude>, <radius>)");
  }

  // extract the index
  auto idx = TRI_LookupIndexByHandle(isolate, trx.resolver(), collection, args[0], false);

  if (idx == nullptr ||
      (idx->type() != triagens::arango::Index::TRI_IDX_TYPE_GEO1_INDEX &&
       idx->type() != triagens::arango::Index::TRI_IDX_TYPE_GEO2_INDEX)) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_NO_INDEX);
  }

  // extract latitude and longitude
  double latitude = TRI_ObjectToDouble(args[1]);
  double longitude = TRI_ObjectToDouble(args[2]);

  // extract the radius
  double radius = TRI_ObjectToDouble(args[3]);

  // setup result
  v8::Handle<v8::Object> result = v8::Object::New(isolate);

  v8::Handle<v8::Array> documents = v8::Array::New(isolate);
  result->Set(TRI_V8_ASCII_STRING("documents"), documents);

  v8::Handle<v8::Array> distances = v8::Array::New(isolate);
  result->Set(TRI_V8_ASCII_STRING("distances"), distances);

  GeoCoordinates* cors = static_cast<triagens::arango::GeoIndex2*>(idx)->withinQuery(latitude, longitude, radius);

  if (cors != nullptr) {
    int res = StoreGeoResult(isolate, trx, collection, cors, documents, distances);

    if (res != TRI_ERROR_NO_ERROR) {
      TRI_V8_THROW_EXCEPTION(res);
    }
  }

  TRI_V8_RETURN(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief selects points within a given radius
////////////////////////////////////////////////////////////////////////////////

static void JS_WithinQuery (const v8::FunctionCallbackInfo<v8::Value>& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  TRI_vocbase_col_t const* col = TRI_UnwrapClass<TRI_vocbase_col_t>(args.Holder(), TRI_GetVocBaseColType());

  if (col == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  TRI_THROW_SHARDING_COLLECTION_NOT_YET_IMPLEMENTED(col);

  SingleCollectionReadOnlyTransaction trx(new V8TransactionContext(true), col->_vocbase, col->_cid);

  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  // .............................................................................
  // inside a read transaction
  // .............................................................................

  trx.lockRead();

  WithinQuery(trx, col, args);

  trx.finish(res);

  // .............................................................................
  // outside a read transaction
  // .............................................................................
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief fetches multiple documents by their keys
/// @startDocuBlock collectionLookupByKeys
/// `collection.documents(keys)`
///
/// Looks up the documents in the specified collection using the array of keys
/// provided. All documents for which a matching key was specified in the *keys*
/// array and that exist in the collection will be returned. 
/// Keys for which no document can be found in the underlying collection are ignored, 
/// and no exception will be thrown for them.
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_OUTPUT{collectionLookupByKeys}
/// ~ db._drop("example");
/// ~ db._create("example");
///   keys = [ ];
/// | for (var i = 0; i < 10; ++i) {
/// |   db.example.insert({ _key: "test" + i, value: i });
/// |   keys.push("test" + i);
///   }
///   db.example.documents(keys);
/// ~ db._drop("example");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

static void JS_LookupByKeys (const v8::FunctionCallbackInfo<v8::Value>& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  TRI_vocbase_col_t const* col = TRI_UnwrapClass<TRI_vocbase_col_t>(args.Holder(), TRI_GetVocBaseColType());

  if (col == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }
  
  if (args.Length() != 1 || 
      ! args[0]->IsArray()) {
    TRI_V8_THROW_EXCEPTION_USAGE("documents(<keys>)");
  }
  
  triagens::basics::Json bindVars(triagens::basics::Json::Object, 2);
  bindVars("@collection", triagens::basics::Json(std::string(col->_name)));
  bindVars("keys", triagens::basics::Json(TRI_UNKNOWN_MEM_ZONE, TRI_ObjectToJson(isolate, args[0])));

  std::string const aql("FOR doc IN @@collection FILTER doc._key IN @keys RETURN doc");
    
  TRI_GET_GLOBALS();
  triagens::aql::Query query(v8g->_applicationV8, 
                             true, 
                             col->_vocbase, 
                             aql.c_str(),
                             aql.size(),
                             bindVars.steal(),
                             nullptr,
                             triagens::aql::PART_MAIN);

  auto queryResult = query.executeV8(isolate, static_cast<triagens::aql::QueryRegistry*>(v8g->_queryRegistry));

  if (queryResult.code != TRI_ERROR_NO_ERROR) {
    if (queryResult.code == TRI_ERROR_REQUEST_CANCELED || 
        queryResult.code == TRI_ERROR_QUERY_KILLED) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_REQUEST_CANCELED);
    }

    THROW_ARANGO_EXCEPTION_MESSAGE(queryResult.code, queryResult.details);
  }

  v8::Handle<v8::Object> result = v8::Object::New(isolate);
  result->Set(TRI_V8_ASCII_STRING("documents"), queryResult.result);

  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes multiple documents by their keys
/// @startDocuBlock collectionRemoveByKeys
/// `collection.removeByKeys(keys)`
///
/// Looks up the documents in the specified collection using the array of keys
/// provided, and removes all documents from the collection whose keys are
/// contained in the *keys* array. Keys for which no document can be found in
/// the underlying collection are ignored, and no exception will be thrown for 
/// them.
///
/// The method will return an object containing the number of removed documents 
/// in the *removed* sub-attribute, and the number of not-removed/ignored
/// documents in the *ignored* sub-attribute.
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_OUTPUT{collectionRemoveByKeys}
/// ~ db._drop("example");
/// ~ db._create("example");
///   keys = [ ];
/// | for (var i = 0; i < 10; ++i) {
/// |   db.example.insert({ _key: "test" + i, value: i });
/// |   keys.push("test" + i);
///   }
///   db.example.removeByKeys(keys);
/// ~ db._drop("example");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

static void JS_RemoveByKeys (const v8::FunctionCallbackInfo<v8::Value>& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  TRI_vocbase_col_t const* col = TRI_UnwrapClass<TRI_vocbase_col_t>(args.Holder(), TRI_GetVocBaseColType());

  if (col == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }
  
  if (args.Length() != 1 || 
      ! args[0]->IsArray()) {
    TRI_V8_THROW_EXCEPTION_USAGE("removeByKeys(<keys>)");
  }
  
  triagens::basics::Json bindVars(triagens::basics::Json::Object, 2);
  bindVars("@collection", triagens::basics::Json(std::string(col->_name)));
  bindVars("keys", triagens::basics::Json(TRI_UNKNOWN_MEM_ZONE, TRI_ObjectToJson(isolate, args[0])));

  std::string const aql("FOR key IN @keys REMOVE key IN @@collection OPTIONS { ignoreErrors: true }");
  
  TRI_GET_GLOBALS();
  triagens::aql::Query query(v8g->_applicationV8, 
                              true, 
                              col->_vocbase, 
                              aql.c_str(),
                              aql.size(),
                              bindVars.steal(),
                              nullptr,
                              triagens::aql::PART_MAIN);

  auto queryResult = query.executeV8(isolate, static_cast<triagens::aql::QueryRegistry*>(v8g->_queryRegistry));

  if (queryResult.code != TRI_ERROR_NO_ERROR) {
    if (queryResult.code == TRI_ERROR_REQUEST_CANCELED || 
        queryResult.code == TRI_ERROR_QUERY_KILLED) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_REQUEST_CANCELED);
    }

    THROW_ARANGO_EXCEPTION_MESSAGE(queryResult.code, queryResult.details);
  }
  
  size_t ignored = 0;
  size_t removed = 0;
    
  if (queryResult.stats != nullptr) {
    auto found = TRI_LookupObjectJson(queryResult.stats, "writesIgnored");
    if (TRI_IsNumberJson(found)) {
      ignored = static_cast<size_t>(found->_value._number);
    }
      
    found = TRI_LookupObjectJson(queryResult.stats, "writesExecuted");
    if (TRI_IsNumberJson(found)) {
      removed = static_cast<size_t>(found->_value._number);
    }
  }

  v8::Handle<v8::Object> result = v8::Object::New(isolate);
  result->Set(TRI_V8_ASCII_STRING("removed"), v8::Number::New(isolate, static_cast<double>(removed)));
  result->Set(TRI_V8_ASCII_STRING("ignored"), v8::Number::New(isolate, static_cast<double>(ignored)));

  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

// -----------------------------------------------------------------------------
// --SECTION--                                                            MODULE
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief creates the query functions
////////////////////////////////////////////////////////////////////////////////

void TRI_InitV8Queries (v8::Isolate* isolate,
                        v8::Handle<v8::Context> context) {
  v8::HandleScope scope(isolate);

  TRI_GET_GLOBALS();
  TRI_ASSERT(v8g != nullptr);
  TRI_GET_GLOBAL(VocbaseColTempl, v8::ObjectTemplate);

  // .............................................................................
  // generate the TRI_vocbase_col_t template
  // .............................................................................

  TRI_AddMethodVocbase(isolate, VocbaseColTempl, TRI_V8_ASCII_STRING("ALL"), JS_AllQuery, true);
  TRI_AddMethodVocbase(isolate, VocbaseColTempl, TRI_V8_ASCII_STRING("ANY"), JS_AnyQuery, true);
  TRI_AddMethodVocbase(isolate, VocbaseColTempl, TRI_V8_ASCII_STRING("BY_CONDITION_SKIPLIST"), JS_ByConditionSkiplist, true);
  TRI_AddMethodVocbase(isolate, VocbaseColTempl, TRI_V8_ASCII_STRING("BY_EXAMPLE"), JS_ByExampleQuery, true);
  TRI_AddMethodVocbase(isolate, VocbaseColTempl, TRI_V8_ASCII_STRING("BY_EXAMPLE_HASH"), JS_ByExampleHashIndex, true);
  TRI_AddMethodVocbase(isolate, VocbaseColTempl, TRI_V8_ASCII_STRING("BY_EXAMPLE_SKIPLIST"), JS_ByExampleSkiplist, true);
  TRI_AddMethodVocbase(isolate, VocbaseColTempl, TRI_V8_ASCII_STRING("checksum"), JS_ChecksumCollection);
  TRI_AddMethodVocbase(isolate, VocbaseColTempl, TRI_V8_ASCII_STRING("EDGES"), JS_EdgesQuery, true);
  TRI_AddMethodVocbase(isolate, VocbaseColTempl, TRI_V8_ASCII_STRING("FIRST"), JS_FirstQuery, true);
  TRI_AddMethodVocbase(isolate, VocbaseColTempl, TRI_V8_ASCII_STRING("FULLTEXT"), JS_FulltextQuery, true);
  TRI_AddMethodVocbase(isolate, VocbaseColTempl, TRI_V8_ASCII_STRING("INEDGES"), JS_InEdgesQuery, true);
  TRI_AddMethodVocbase(isolate, VocbaseColTempl, TRI_V8_ASCII_STRING("LAST"), JS_LastQuery, true);
  TRI_AddMethodVocbase(isolate, VocbaseColTempl, TRI_V8_ASCII_STRING("NEAR"), JS_NearQuery, true);
  TRI_AddMethodVocbase(isolate, VocbaseColTempl, TRI_V8_ASCII_STRING("OUTEDGES"), JS_OutEdgesQuery, true);
  TRI_AddMethodVocbase(isolate, VocbaseColTempl, TRI_V8_ASCII_STRING("WITHIN"), JS_WithinQuery, true);
  TRI_AddMethodVocbase(isolate, VocbaseColTempl, TRI_V8_ASCII_STRING("lookupByKeys"), JS_LookupByKeys, true); // an alias for .documents
  TRI_AddMethodVocbase(isolate, VocbaseColTempl, TRI_V8_ASCII_STRING("documents"), JS_LookupByKeys, true);
  TRI_AddMethodVocbase(isolate, VocbaseColTempl, TRI_V8_ASCII_STRING("removeByKeys"), JS_RemoveByKeys, true);
  
  // internal methods. not intended to be used by end-users
  TRI_AddMethodVocbase(isolate, VocbaseColTempl, TRI_V8_ASCII_STRING("NTH"), JS_NthQuery, true);
  TRI_AddMethodVocbase(isolate, VocbaseColTempl, TRI_V8_ASCII_STRING("NTH2"), JS_Nth2Query, true);
  TRI_AddMethodVocbase(isolate, VocbaseColTempl, TRI_V8_ASCII_STRING("OFFSET"), JS_OffsetQuery, true);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
