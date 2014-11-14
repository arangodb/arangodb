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
#include "v8-vocindex.h"

#include "Basics/logging.h"
#include "Basics/random.h"
#include "Basics/string-buffer.h"
#include "GeoIndex/geo-index.h"
#include "HashIndex/hash-index.h"
#include "FulltextIndex/fulltext-index.h"
#include "FulltextIndex/fulltext-result.h"
#include "FulltextIndex/fulltext-query.h"
#include "SkipLists/skiplistIndex.h"
#include "Utils/transactions.h"
#include "V8/v8-globals.h"
#include "V8/v8-conv.h"
#include "V8/v8-utils.h"
#include "V8Server/v8-vocbase.h"
#include "VocBase/edge-collection.h"
#include "VocBase/vocbase.h"
#include "v8-wrapshapedjson.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::arango;

// -----------------------------------------------------------------------------
// --SECTION--                                                   private defines
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief shortcut to wrap a shaped-json object in a read-only transaction
////////////////////////////////////////////////////////////////////////////////

#define WRAP_SHAPED_JSON(...) TRI_WrapShapedJson<SingleCollectionReadOnlyTransaction>(__VA_ARGS__)

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

static v8::Handle<v8::Value> EmptyResult () {
  v8::HandleScope scope;

  v8::Handle<v8::Object> result = v8::Object::New();
  result->Set(v8::String::New("documents"), v8::Array::New());
  result->Set(v8::String::New("total"), v8::Number::New(0));
  result->Set(v8::String::New("count"), v8::Number::New(0));

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts skip and limit
////////////////////////////////////////////////////////////////////////////////

static void ExtractSkipAndLimit (v8::Arguments const& argv,
                                 size_t pos,
                                 TRI_voc_ssize_t& skip,
                                 TRI_voc_size_t& limit) {

  skip = TRI_QRY_NO_SKIP;
  limit = TRI_QRY_NO_LIMIT;

  if (pos < (size_t) argv.Length() && ! argv[(int) pos]->IsNull() && ! argv[(int) pos]->IsUndefined()) {
    skip = (TRI_voc_size_t) TRI_ObjectToDouble(argv[(int) pos]);
  }

  if (pos + 1 < (size_t) argv.Length() && ! argv[(int) pos + 1]->IsNull() && ! argv[(int) pos + 1]->IsUndefined()) {
    limit = (TRI_voc_ssize_t) TRI_ObjectToDouble(argv[(int) pos + 1]);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief calculates slice
////////////////////////////////////////////////////////////////////////////////

static void CalculateSkipLimitSlice (size_t length,
                                     TRI_voc_ssize_t skip,
                                     TRI_voc_size_t limit,
                                     size_t& s,
                                     size_t& e) {
  s = 0;
  e = length;

  // skip from the beginning
  if (0 < skip) {
    s = (size_t) skip;

    if (e < s) {
      s = (size_t) e;
    }
  }

  // skip from the end
  else if (skip < 0) {
    skip = -skip;

    if ((size_t) skip < e) {
      s = e - skip;
    }
  }

  // apply limit
  if (s + limit < e) {
    int64_t sum = (int64_t) s + (int64_t) limit;
    if (sum < (int64_t) e) {
      if (sum >= (int64_t) TRI_QRY_NO_LIMIT) {
        e = TRI_QRY_NO_LIMIT;
      }
      else {
        e = (size_t) sum;
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief cleans up the example object
////////////////////////////////////////////////////////////////////////////////

static void CleanupExampleObject (TRI_memory_zone_t* zone,
                                  size_t n,
                                  TRI_shape_pid_t* pids,
                                  TRI_shaped_json_t** values) {

  // clean shaped json objects
  for (size_t j = 0;  j < n;  ++j) {
    if (values[j] != 0) {
      TRI_FreeShapedJson(zone, values[j]);
    }
  }

  TRI_Free(TRI_UNKNOWN_MEM_ZONE, values);

  if (pids != 0) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, pids);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets up the example object
////////////////////////////////////////////////////////////////////////////////

static int SetupExampleObject (v8::Handle<v8::Object> const example,
                               TRI_shaper_t* shaper,
                               size_t& n,
                               TRI_shape_pid_t*& pids,
                               TRI_shaped_json_t**& values,
                               v8::Handle<v8::Object>* err) {

  // get own properties of example
  v8::Handle<v8::Array> names = example->GetOwnPropertyNames();
  n = names->Length();

  // setup storage
  pids = (TRI_shape_pid_t*) TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, n * sizeof(TRI_shape_pid_t), false);

  if (pids == 0) {
    // out of memory
    *err = TRI_CreateErrorObject(__FILE__, __LINE__, TRI_ERROR_OUT_OF_MEMORY);

    return TRI_ERROR_OUT_OF_MEMORY;
  }

  values = (TRI_shaped_json_t**) TRI_Allocate(TRI_UNKNOWN_MEM_ZONE,
                                              n * sizeof(TRI_shaped_json_t*), false);

  if (values == 0) {
    // out of memory
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, pids);
    pids = 0;
    *err = TRI_CreateErrorObject(__FILE__, __LINE__, TRI_ERROR_OUT_OF_MEMORY);

    return TRI_ERROR_OUT_OF_MEMORY;
  }

  // convert
  for (size_t i = 0;  i < n;  ++i) {
    v8::Handle<v8::Value> key = names->Get((uint32_t) i);
    v8::Handle<v8::Value> val = example->Get(key);

    // property initialise the memory
    values[i] = 0;

    TRI_Utf8ValueNFC keyStr(TRI_UNKNOWN_MEM_ZONE, key);

    if (*keyStr != 0) {
      pids[i] = shaper->lookupAttributePathByName(shaper, *keyStr);

      if (pids[i] == 0) {
        // no attribute path found. this means the result will be empty
        CleanupExampleObject(shaper->_memoryZone, i, pids, values);
        return TRI_RESULT_ELEMENT_NOT_FOUND;
      }

      values[i] = TRI_ShapedJsonV8Object(val, shaper, false);

      if (values[i] == 0) {
        CleanupExampleObject(shaper->_memoryZone, i, pids, values);
        return TRI_RESULT_ELEMENT_NOT_FOUND;
      }
    }
    else {
      CleanupExampleObject(shaper->_memoryZone, i, pids, values);
      *err = TRI_CreateErrorObject(__FILE__,
                                   __LINE__,
                                   TRI_ERROR_BAD_PARAMETER,
                                   "cannot convert attribute path to UTF8");
      return TRI_ERROR_BAD_PARAMETER;
    }
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets up the skiplist operator for a skiplist condition query
////////////////////////////////////////////////////////////////////////////////

static TRI_index_operator_t* SetupConditionsSkiplist (TRI_index_t* idx,
                                                      TRI_shaper_t* shaper,
                                                      v8::Handle<v8::Object> conditions) {
  TRI_index_operator_t* lastOperator = nullptr;
  size_t numEq = 0;
  size_t lastNonEq = 0;

  TRI_json_t* parameters = TRI_CreateListJson(TRI_UNKNOWN_MEM_ZONE);

  if (parameters == nullptr) {
    return nullptr;
  }

  // iterate over all index fields
  for (size_t i = 1; i <= idx->_fields._length; ++i) {
    v8::Handle<v8::String> key = v8::String::New(idx->_fields._buffer[i - 1]);

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

      TRI_json_t* json = TRI_ObjectToJson(value);

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

        TRI_PushBack3ListJson(TRI_UNKNOWN_MEM_ZONE, parameters, json);
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

        TRI_PushBack3ListJson(TRI_UNKNOWN_MEM_ZONE, cloned, json);

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
                                                 clonedParams->_value._objects._length); 
          numEq = 0;
        }

        TRI_index_operator_t* current;

        // create the operator for the current condition
        current = TRI_CreateIndexOperator(opType,
                                          nullptr,
                                          nullptr, 
                                          cloned, 
                                          shaper, 
                                          cloned->_value._objects._length); 

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
                                           clonedParams->_value._objects._length); 
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

static TRI_index_operator_t* SetupExampleSkiplist (TRI_index_t* idx,
                                                   TRI_shaper_t* shaper,
                                                   v8::Handle<v8::Object> example) {
  TRI_json_t* parameters = TRI_CreateListJson(TRI_UNKNOWN_MEM_ZONE);

  if (parameters == nullptr) {
    return nullptr;
  }

  for (size_t i = 0; i < idx->_fields._length; ++i) {
    v8::Handle<v8::String> key = v8::String::New(idx->_fields._buffer[i]);

    if (! example->HasOwnProperty(key)) {
      break;
    }

    v8::Handle<v8::Value> value = example->Get(key);

    TRI_json_t* json = TRI_ObjectToJson(value);

    if (json == nullptr) {
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, parameters);

      return nullptr;
    }

    TRI_PushBack3ListJson(TRI_UNKNOWN_MEM_ZONE, parameters, json);
  }

  if (parameters->_value._objects._length > 0) {
    // example means equality comparisons only
    return TRI_CreateIndexOperator(TRI_EQ_INDEX_OPERATOR, 
                                   nullptr, 
                                   nullptr,
                                   parameters, 
                                   shaper, 
                                   parameters->_value._objects._length);
  }

  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, parameters);
  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys the example object for a hash index
////////////////////////////////////////////////////////////////////////////////

static void DestroySearchValue (TRI_memory_zone_t* zone,
                                TRI_index_search_value_t& value) {
  size_t n;

  n = value._length;

  for (size_t j = 0;  j < n;  ++j) {
    TRI_DestroyShapedJson(zone, &value._values[j]);
  }

  TRI_Free(TRI_CORE_MEM_ZONE, value._values);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets up the example object for a hash index
////////////////////////////////////////////////////////////////////////////////

static int SetupSearchValue (TRI_vector_t const* paths,
                             v8::Handle<v8::Object> example,
                             TRI_shaper_t* shaper,
                             TRI_index_search_value_t& result,
                             v8::Handle<v8::Object>* err) {
  size_t n;

  // extract attribute paths
  n = paths->_length;

  // setup storage
  result._length = n;
  result._values = (TRI_shaped_json_t*) TRI_Allocate(TRI_CORE_MEM_ZONE,
                                                     n * sizeof(TRI_shaped_json_t),
                                                     true);

  // convert
  for (size_t i = 0;  i < n;  ++i) {
    TRI_shape_pid_t pid = * (TRI_shape_pid_t*) TRI_AtVector(paths, i);

    TRI_ASSERT(pid != 0);
    char const* name = TRI_AttributeNameShapePid(shaper, pid);

    if (name == NULL) {
      DestroySearchValue(shaper->_memoryZone, result);
      *err = TRI_CreateErrorObject(__FILE__,
                                   __LINE__,
                                   TRI_ERROR_INTERNAL,
                                   "shaper failed");
      return TRI_ERROR_BAD_PARAMETER;
    }

    v8::Handle<v8::String> key = v8::String::New(name);
    int res;

    if (example->HasOwnProperty(key)) {
      v8::Handle<v8::Value> val = example->Get(key);

      res = TRI_FillShapedJsonV8Object(val, &result._values[i], shaper, false);
    }
    else {
      res = TRI_FillShapedJsonV8Object(v8::Null(), &result._values[i], shaper, false);
    }

    if (res != TRI_ERROR_NO_ERROR) {
      DestroySearchValue(shaper->_memoryZone, result);

      if (res != TRI_RESULT_ELEMENT_NOT_FOUND) {
        *err = TRI_CreateErrorObject(__FILE__,
                                     __LINE__,
                                     res,
                                     "cannot convert value to JSON");
      }
      return res;
    }
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief execute a skiplist query (by condition or by example)
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> ExecuteSkiplistQuery (v8::Arguments const& argv,
                                                   std::string const& signature,
                                                   query_t type) {
  v8::HandleScope scope;

  // expecting index, example, skip, and limit
  if (argv.Length() < 2) {
    TRI_V8_EXCEPTION_USAGE(scope, signature.c_str());
  }

  if (! argv[1]->IsObject()) {
    std::string msg;

    if (type == QUERY_EXAMPLE) {
      msg = "<example> must be an object";
    }
    else {
      msg = "<conditions> must be an object";
    }

    TRI_V8_TYPE_ERROR(scope, msg.c_str());
  }

  bool reverse = false;
  if (argv.Length() > 4) {
    reverse = TRI_ObjectToBoolean(argv[4]);
  }

  TRI_vocbase_col_t const* col;
  col = TRI_UnwrapClass<TRI_vocbase_col_t>(argv.Holder(), TRI_GetVocBaseColType());

  if (col == nullptr) {
    TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract collection");
  }

  TRI_SHARDING_COLLECTION_NOT_YET_IMPLEMENTED(scope, col);

  SingleCollectionReadOnlyTransaction trx(new V8TransactionContext(true), col->_vocbase, col->_cid);

  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_EXCEPTION(scope, res);
  }

  v8::Handle<v8::Object> err;

  TRI_document_collection_t* document = trx.documentCollection();
  TRI_shaper_t* shaper = document->getShaper();  // PROTECTED by trx here

  // extract skip and limit
  TRI_voc_ssize_t skip;
  TRI_voc_size_t limit;

  ExtractSkipAndLimit(argv, 2, skip, limit);

  // setup result
  v8::Handle<v8::Object> result = v8::Object::New();

  v8::Handle<v8::Array> documents = v8::Array::New();
  result->Set(v8::String::New("documents"), documents);

  // .............................................................................
  // inside a read transaction
  // .............................................................................

  trx.lockRead();


  // extract the index
  TRI_index_t* idx = TRI_LookupIndexByHandle(trx.resolver(), col, argv[0], false, &err);

  if (idx == nullptr) {
    return scope.Close(v8::ThrowException(err));
  }

  if (idx->_type != TRI_IDX_TYPE_SKIPLIST_INDEX) {
    TRI_V8_EXCEPTION(scope, TRI_ERROR_ARANGO_NO_INDEX);
  }

  TRI_index_operator_t* skiplistOperator;
  v8::Handle<v8::Object> values = argv[1]->ToObject();

  if (type == QUERY_EXAMPLE) {
    skiplistOperator = SetupExampleSkiplist(idx, shaper, values);
  }
  else {
    skiplistOperator = SetupConditionsSkiplist(idx, shaper, values);
  }

  if (skiplistOperator == nullptr) {
    TRI_V8_EXCEPTION(scope, TRI_ERROR_BAD_PARAMETER);
  }

  TRI_skiplist_iterator_t* skiplistIterator = TRI_LookupSkiplistIndex(idx, skiplistOperator, reverse);
  TRI_FreeIndexOperator(skiplistOperator);

  if (skiplistIterator == nullptr) {
    int res = TRI_errno();
    if (res == TRI_RESULT_ELEMENT_NOT_FOUND) {
      return scope.Close(EmptyResult());
    }

    TRI_V8_EXCEPTION(scope, TRI_ERROR_ARANGO_NO_INDEX);
  }

  TRI_voc_ssize_t total = 0;
  TRI_voc_size_t count = 0;
  bool error = false;

  if (trx.orderBarrier(trx.trxCollection()) == nullptr) {
    TRI_FreeSkiplistIterator(skiplistIterator);
    TRI_V8_EXCEPTION(scope, TRI_ERROR_OUT_OF_MEMORY);
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
        documents->Set(count, doc);

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

  result->Set(v8::String::New("total"), v8::Number::New((double) total));
  result->Set(v8::String::New("count"), v8::Number::New(count));

  if (error) {
    TRI_V8_EXCEPTION_MEMORY(scope);
  }

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a geo result
////////////////////////////////////////////////////////////////////////////////

static int StoreGeoResult (SingleCollectionReadOnlyTransaction& trx,
                           TRI_vocbase_col_t const* collection,
                           GeoCoordinates* cors,
                           v8::Handle<v8::Array>& documents,
                           v8::Handle<v8::Array>& distances) {
  GeoCoordinate* end;
  GeoCoordinate* ptr;
  double* dtr;
  geo_coordinate_distance_t* gtr;
  geo_coordinate_distance_t* tmp;
  uint32_t i;

  if (trx.orderBarrier(trx.trxCollection()) == nullptr) {
    GeoIndex_CoordinatesFree(cors);
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  // sort the result
  size_t n = cors->length;

  if (n == 0) {
    GeoIndex_CoordinatesFree(cors);
    return TRI_ERROR_NO_ERROR;
  }

  gtr = (tmp = (geo_coordinate_distance_t*) TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(geo_coordinate_distance_t) * n, false));

  if (gtr == nullptr) {
    GeoIndex_CoordinatesFree(cors);

    return TRI_ERROR_OUT_OF_MEMORY;
  }

  geo_coordinate_distance_t* gnd = tmp + n;

  ptr = cors->coordinates;
  end = cors->coordinates + n;

  dtr = cors->distances;

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
  for (gtr = tmp, i = 0;  gtr < gnd;  ++gtr, ++i) {
    v8::Handle<v8::Value> doc = WRAP_SHAPED_JSON(trx, collection->_cid, ((TRI_doc_mptr_t const*) gtr->_data)->getDataPtr());

    if (doc.IsEmpty()) {
      error = true;
      break;
    }

    documents->Set(i, doc);
    distances->Set(i, v8::Number::New(gtr->_distance));
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

static v8::Handle<v8::Value> EdgesQuery (TRI_edge_direction_e direction,
                                         v8::Arguments const& argv) {
  v8::HandleScope scope;

  TRI_vocbase_col_t const* col;
  col = TRI_UnwrapClass<TRI_vocbase_col_t>(argv.Holder(), TRI_GetVocBaseColType());

  if (col == nullptr) {
    TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract collection");
  }

  TRI_SHARDING_COLLECTION_NOT_YET_IMPLEMENTED(scope, col);

  if (col->_type != TRI_COL_TYPE_EDGE) {
    TRI_V8_EXCEPTION(scope, TRI_ERROR_ARANGO_COLLECTION_TYPE_INVALID);
  }

  SingleCollectionReadOnlyTransaction trx(new V8TransactionContext(true), col->_vocbase, col->_cid);

  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_EXCEPTION(scope, res);
  }

  TRI_document_collection_t* document = trx.documentCollection();

  // first and only argument schould be a list of document idenfifier
  if (argv.Length() != 1) {
    switch (direction) {
      case TRI_EDGE_IN:
        TRI_V8_EXCEPTION_USAGE(scope, "inEdges(<vertices>)");

      case TRI_EDGE_OUT:
        TRI_V8_EXCEPTION_USAGE(scope, "outEdges(<vertices>)");

      case TRI_EDGE_ANY:
      default: {
        TRI_V8_EXCEPTION_USAGE(scope, "edges(<vertices>)");
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

  if (trx.orderBarrier(trx.trxCollection()) == nullptr) {
    TRI_V8_EXCEPTION(scope, TRI_ERROR_OUT_OF_MEMORY);
  }

  // argument is a list of vertices
  if (argv[0]->IsArray()) {
    documents = v8::Array::New();
    v8::Handle<v8::Array> vertices = v8::Handle<v8::Array>::Cast(argv[0]);
    uint32_t count = 0;
    const uint32_t len = vertices->Length();

    for (uint32_t i = 0;  i < len; ++i) {
      TRI_voc_cid_t cid;
      std::unique_ptr<char[]> key;

      res = TRI_ParseVertex(trx.resolver(), cid, key, vertices->Get(i));

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

    res = TRI_ParseVertex(trx.resolver(), cid, key, argv[0]);

    if (res != TRI_ERROR_NO_ERROR) {
      TRI_V8_EXCEPTION(scope, res);
    }

    std::vector<TRI_doc_mptr_copy_t>&& edges = TRI_LookupEdgesDocumentCollection(document, direction, cid, key.get());

    trx.finish(res);

    uint32_t const n = static_cast<uint32_t>(edges.size());
    documents = v8::Array::New(static_cast<int>(n));
    for (size_t j = 0;  j < n;  ++j) {
      v8::Handle<v8::Value> doc = WRAP_SHAPED_JSON(trx, col->_cid, edges[j].getDataPtr());

      if (doc.IsEmpty()) {
        error = true;
        break;
      }
      else {
        documents->Set(static_cast<uint32_t>(j), doc);
      }
    }
  }

  // .............................................................................
  // outside a read transaction
  // .............................................................................

  if (error) {
    TRI_V8_EXCEPTION_MEMORY(scope);
  }

  return scope.Close(documents);
}

// -----------------------------------------------------------------------------
// --SECTION--                                              javascript functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief selects all documents from a collection
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_AllQuery (v8::Arguments const& argv) {
  v8::HandleScope scope;

  // expecting two arguments
  if (argv.Length() != 2) {
    TRI_V8_EXCEPTION_USAGE(scope, "ALL(<skip>, <limit>)");
  }

  TRI_vocbase_col_t const* col;
  col = TRI_UnwrapClass<TRI_vocbase_col_t>(argv.Holder(), TRI_GetVocBaseColType());

  if (col == nullptr) {
    TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract collection");
  }

  TRI_SHARDING_COLLECTION_NOT_YET_IMPLEMENTED(scope, col);

  // extract skip and limit
  TRI_voc_ssize_t skip;
  TRI_voc_size_t limit;
  ExtractSkipAndLimit(argv, 0, skip, limit);

  uint32_t total = 0;
  vector<TRI_doc_mptr_copy_t> docs;

  SingleCollectionReadOnlyTransaction trx(new V8TransactionContext(true), col->_vocbase, col->_cid);

  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_EXCEPTION(scope, res);
  }
 
  res = trx.read(docs, skip, limit, &total);
  TRI_ASSERT(docs.empty() || trx.hasBarrier());
  
  res = trx.finish(res);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_EXCEPTION(scope, res);
  }

  size_t const n = docs.size();
  uint32_t count = 0;

  // setup result
  v8::Handle<v8::Object> result = v8::Object::New();
  v8::Handle<v8::Array> documents = v8::Array::New((int) n);
  // reserve full capacity in one go
  result->Set(v8::String::New("documents"), documents);

  for (size_t i = 0; i < n; ++i) {
    v8::Handle<v8::Value> doc = WRAP_SHAPED_JSON(trx, col->_cid, docs[i].getDataPtr());

    if (doc.IsEmpty()) {
      TRI_V8_EXCEPTION_MEMORY(scope);
    }
    else {
      documents->Set(count++, doc);
    }
  }

  result->Set(v8::String::New("total"), v8::Number::New(total));
  result->Set(v8::String::New("count"), v8::Number::New(count));
 
  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief selects documents from a collection, hashing the document key and
/// only returning these documents which fall into a specific partition
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_NthQuery (v8::Arguments const& argv) {
  v8::HandleScope scope;

  // expecting two arguments
  if (argv.Length() != 2 || ! argv[0]->IsNumber() || ! argv[1]->IsNumber()) {
    TRI_V8_EXCEPTION_USAGE(scope, "NTH(<partitionId>, <numberOfPartitions>)");
  }

  TRI_vocbase_col_t const* col;
  col = TRI_UnwrapClass<TRI_vocbase_col_t>(argv.Holder(), TRI_GetVocBaseColType());

  if (col == nullptr) {
    TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract collection");
  }

  TRI_SHARDING_COLLECTION_NOT_YET_IMPLEMENTED(scope, col);

  uint64_t const partitionId = TRI_ObjectToUInt64(argv[0], false);
  uint64_t const numberOfPartitions = TRI_ObjectToUInt64(argv[1], false);

  if (partitionId >= numberOfPartitions || numberOfPartitions == 0) {
    TRI_V8_EXCEPTION_PARAMETER(scope, "invalid value for <partitionId> or <numberOfPartitions>");
  }
  
  uint32_t total = 0;
  vector<TRI_doc_mptr_copy_t> docs;

  SingleCollectionReadOnlyTransaction trx(new V8TransactionContext(true), col->_vocbase, col->_cid);

  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_EXCEPTION(scope, res);
  }

  res = trx.readPartition(docs, partitionId, numberOfPartitions, &total);
  TRI_ASSERT(docs.empty() || trx.hasBarrier());

  res = trx.finish(res);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_EXCEPTION(scope, res);
  }

  size_t const n = docs.size();
  uint32_t count = 0;

  // setup result
  v8::Handle<v8::Object> result = v8::Object::New();
  v8::Handle<v8::Array> documents = v8::Array::New((int) n);
  // reserve full capacity in one go
  result->Set(v8::String::New("documents"), documents);

  for (size_t i = 0; i < n; ++i) {
    v8::Handle<v8::Value> doc = WRAP_SHAPED_JSON(trx, col->_cid, docs[i].getDataPtr());

    if (doc.IsEmpty()) {
      TRI_V8_EXCEPTION_MEMORY(scope);
    }
    else {
      documents->Set(count++, doc);
    }
  }

  result->Set(v8::String::New("total"), v8::Number::New(total));
  result->Set(v8::String::New("count"), v8::Number::New(count));

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief selects documents from a collection, hashing the document key and
/// only returning these documents which fall into a specific partition
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_Nth2Query (v8::Arguments const& argv) {
  v8::HandleScope scope;

  // expecting two arguments
  if (argv.Length() != 2 || ! argv[0]->IsNumber() || ! argv[1]->IsNumber()) {
    TRI_V8_EXCEPTION_USAGE(scope, "NTH2(<partitionId>, <numberOfPartitions>)");
  }

  TRI_vocbase_col_t const* col;
  col = TRI_UnwrapClass<TRI_vocbase_col_t>(argv.Holder(), TRI_GetVocBaseColType());

  if (col == nullptr) {
    TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract collection");
  }

  TRI_SHARDING_COLLECTION_NOT_YET_IMPLEMENTED(scope, col);

  uint64_t const partitionId = TRI_ObjectToUInt64(argv[0], false);
  uint64_t const numberOfPartitions = TRI_ObjectToUInt64(argv[1], false);

  if (partitionId >= numberOfPartitions || numberOfPartitions == 0) {
    TRI_V8_EXCEPTION_PARAMETER(scope, "invalid value for <partitionId> or <numberOfPartitions>");
  }
  
  uint32_t total = 0;
  vector<TRI_doc_mptr_copy_t> docs;

  SingleCollectionReadOnlyTransaction trx(new V8TransactionContext(true), col->_vocbase, col->_cid);

  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_EXCEPTION(scope, res);
  }

  res = trx.readPartition(docs, partitionId, numberOfPartitions, &total);
  TRI_ASSERT(docs.empty() || trx.hasBarrier());

  res = trx.finish(res);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_EXCEPTION(scope, res);
  }

  size_t const n = docs.size();
  uint32_t count = 0;

  // setup result
  v8::Handle<v8::Object> result = v8::Object::New();
  v8::Handle<v8::Array> documents = v8::Array::New((int) n);
  // reserve full capacity in one go
  result->Set(v8::String::New("documents"), documents);

  for (size_t i = 0; i < n; ++i) {
    char const* key = TRI_EXTRACT_MARKER_KEY(static_cast<TRI_df_marker_t const*>(docs[i].getDataPtr()));
    documents->Set(count++, v8::String::New(key));
  }

  result->Set(v8::String::New("total"), v8::Number::New(total));
  result->Set(v8::String::New("count"), v8::Number::New(count));

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief @mchacki
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_Nth3Query (v8::Arguments const& argv) {
  v8::HandleScope scope;

  // expecting two arguments
  if (argv.Length() != 2 || ! argv[0]->IsString() || ! argv[1]->IsNumber()) {
    TRI_V8_EXCEPTION_USAGE(scope, "NTH3(<key>, <numberOfPartitions>)");
  }

  std::string const key(TRI_ObjectToString(argv[0]));
  uint64_t const numberOfPartitions = TRI_ObjectToUInt64(argv[1], false);

  if (numberOfPartitions == 0) {
    TRI_V8_EXCEPTION_PARAMETER(scope, "invalid value for <numberOfPartitions>");
  }
  
  uint64_t hash = TRI_FnvHashPointer(static_cast<void const*>(key.c_str()), key.size());

  return scope.Close(v8::Number::New(static_cast<int>(hash % numberOfPartitions)));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief selects documents from a collection, using an offset into the
/// primary index. this can be used for incremental access
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_OffsetQuery (v8::Arguments const& argv) {
  v8::HandleScope scope;

  // expecting two arguments
  if (argv.Length() != 4) {
    TRI_V8_EXCEPTION_USAGE(scope, "OFFSET(<internalSkip>, <batchSize>, <skip>, <limit>)");
  }

  TRI_vocbase_col_t const* col;
  col = TRI_UnwrapClass<TRI_vocbase_col_t>(argv.Holder(), TRI_GetVocBaseColType());

  if (col == nullptr) {
    TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract collection");
  }

  TRI_SHARDING_COLLECTION_NOT_YET_IMPLEMENTED(scope, col);

  TRI_voc_size_t internalSkip = (TRI_voc_size_t) TRI_ObjectToDouble(argv[0]);
  TRI_voc_size_t batchSize = (TRI_voc_size_t) TRI_ObjectToDouble(argv[1]);

  // extract skip and limit
  TRI_voc_ssize_t skip;
  TRI_voc_size_t limit;
  ExtractSkipAndLimit(argv, 2, skip, limit);

  uint32_t total = 0;
  vector<TRI_doc_mptr_copy_t> docs;

  SingleCollectionReadOnlyTransaction trx(new V8TransactionContext(true), col->_vocbase, col->_cid);

  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_EXCEPTION(scope, res);
  }

  res = trx.readOffset(docs, internalSkip, batchSize, skip, limit, &total);
  TRI_ASSERT(docs.empty() || trx.hasBarrier());

  res = trx.finish(res);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_EXCEPTION(scope, res);
  }

  size_t const n = docs.size();
  uint32_t count = 0;

  // setup result
  v8::Handle<v8::Object> result = v8::Object::New();
  v8::Handle<v8::Array> documents = v8::Array::New((int) n);
  // reserve full capacity in one go
  result->Set(v8::String::New("documents"), documents);

  for (size_t i = 0; i < n; ++i) {
    v8::Handle<v8::Value> document = WRAP_SHAPED_JSON(trx, col->_cid, docs[i].getDataPtr());

    if (document.IsEmpty()) {
      TRI_V8_EXCEPTION_MEMORY(scope);
    }
    else {
      documents->Set(count++, document);
    }
  }

  result->Set(v8::String::New("total"), v8::Number::New(total));
  result->Set(v8::String::New("count"), v8::Number::New(count));
  result->Set(v8::String::New("skip"), v8::Number::New(internalSkip));

  return scope.Close(result);
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

static v8::Handle<v8::Value> JS_AnyQuery (v8::Arguments const& argv) {
  v8::HandleScope scope;

  TRI_vocbase_col_t const* col;
  col = TRI_UnwrapClass<TRI_vocbase_col_t>(argv.Holder(), TRI_GetVocBaseColType());

  if (col == nullptr) {
    TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract collection");
  }

  TRI_doc_mptr_copy_t document;

  SingleCollectionReadOnlyTransaction trx(new V8TransactionContext(true), col->_vocbase, col->_cid);

  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_EXCEPTION(scope, res);
  }

  res = trx.readRandom(&document);
  TRI_ASSERT(document.getDataPtr() == nullptr || trx.hasBarrier());

  res = trx.finish(res);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_EXCEPTION(scope, res);
  }

  if (document.getDataPtr() == nullptr) {  // PROTECTED by trx here
    return scope.Close(v8::Null());
  }

  v8::Handle<v8::Value> doc = WRAP_SHAPED_JSON(trx, col->_cid, document.getDataPtr());

  if (doc.IsEmpty()) {
    TRI_V8_EXCEPTION_MEMORY(scope);
  }

  return scope.Close(doc);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief selects documents by example (not using any index)
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_ByExampleQuery (v8::Arguments const& argv) {
  v8::HandleScope scope;

  // expecting example, skip, limit
  if (argv.Length() < 1) {
    TRI_V8_EXCEPTION_USAGE(scope, "BY_EXAMPLE(<example>, <skip>, <limit>)");
  }

  // extract the example
  if (! argv[0]->IsObject()) {
    TRI_V8_TYPE_ERROR(scope, "<example> must be an object");
  }


  TRI_vocbase_col_t const* col;
  col = TRI_UnwrapClass<TRI_vocbase_col_t>(argv.Holder(), TRI_GetVocBaseColType());

  if (col == nullptr) {
    TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract collection");
  }

  TRI_SHARDING_COLLECTION_NOT_YET_IMPLEMENTED(scope, col);

  SingleCollectionReadOnlyTransaction trx(new V8TransactionContext(true), col->_vocbase, col->_cid);

  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_EXCEPTION(scope, res);
  }

  TRI_document_collection_t* document = trx.documentCollection();
  TRI_shaper_t* shaper = document->getShaper();  // PROTECTED by trx here

  v8::Handle<v8::Object> example = argv[0]->ToObject();

  // extract skip and limit
  TRI_voc_ssize_t skip;
  TRI_voc_size_t limit;
  ExtractSkipAndLimit(argv, 1, skip, limit);

  // extract sub-documents
  TRI_shape_pid_t* pids;
  TRI_shaped_json_t** values = 0;
  size_t n;

  if (trx.orderBarrier(trx.trxCollection()) == nullptr) {
    TRI_V8_EXCEPTION(scope, TRI_ERROR_OUT_OF_MEMORY);
  }

  v8::Handle<v8::Object> err;
  res = SetupExampleObject(example, shaper, n, pids, values, &err);

  if (res == TRI_RESULT_ELEMENT_NOT_FOUND) {
    // empty result
    return scope.Close(EmptyResult());
  }

  if (res != TRI_ERROR_NO_ERROR) {
    return scope.Close(v8::ThrowException(err));
  }

  // setup result
  v8::Handle<v8::Object> result = v8::Object::New();
  v8::Handle<v8::Array> documents = v8::Array::New();
  result->Set(v8::String::New("documents"), documents);

  // ...........................................................................
  // inside a read transaction
  // ...........................................................................

  vector<TRI_doc_mptr_copy_t> filtered;

  trx.lockRead();

  // find documents by example
  try {
    filtered = TRI_SelectByExample(trx.trxCollection(), n,  pids, values);
  }
  catch (std::exception&) {
    TRI_V8_EXCEPTION_MEMORY(scope);
  }

  trx.finish(res);

  // ...........................................................................
  // outside a read transaction
  // ...........................................................................

  // convert to list of shaped jsons
  size_t total = filtered.size();
  size_t count = 0;
  bool error = false;

  if (0 < total) {
    size_t s;
    size_t e;

    CalculateSkipLimitSlice(filtered.size(), skip, limit, s, e);

    if (s < e) {
      for (size_t j = s; j < e; ++j) {
        v8::Handle<v8::Value> doc = WRAP_SHAPED_JSON(trx, col->_cid, filtered[j].getDataPtr());

        if (doc.IsEmpty()) {
          error = true;
          break;
        }
        else {
          documents->Set((uint32_t) count++, doc);
        }
      }
    }
  }


  result->Set(v8::String::New("total"), v8::Integer::New((int32_t) total));
  result->Set(v8::String::New("count"), v8::Integer::New((int32_t) count));

  CleanupExampleObject(shaper->_memoryZone, n, pids, values);

  if (error) {
    TRI_V8_EXCEPTION_MEMORY(scope);
  }

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief selects documents by example using a hash index
///
/// It is the callers responsibility to acquire and free the required locks
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> ByExampleHashIndexQuery (SingleCollectionReadOnlyTransaction& trx,
                                                      TRI_vocbase_col_t const* collection,
                                                      v8::Handle<v8::Object>* err,
                                                      v8::Arguments const& argv) {
  v8::HandleScope scope;

  // expecting index, example, skip, and limit
  if (argv.Length() < 2) {
    TRI_V8_EXCEPTION_USAGE(scope, "EXAMPLE_HASH(<index>, <example>, <skip>, <limit>)");
  }

  // extract the example
  if (! argv[1]->IsObject()) {
    TRI_V8_TYPE_ERROR(scope, "<example> must be an object");
  }

  v8::Handle<v8::Object> example = argv[1]->ToObject();

  if (trx.orderBarrier(trx.trxCollection()) == nullptr) {
    TRI_V8_EXCEPTION_MEMORY(scope);
  }

  // extract skip and limit
  TRI_voc_ssize_t skip;
  TRI_voc_size_t limit;

  ExtractSkipAndLimit(argv, 2, skip, limit);

  // setup result
  v8::Handle<v8::Object> result = v8::Object::New();

  v8::Handle<v8::Array> documents = v8::Array::New();
  result->Set(v8::String::New("documents"), documents);

  // extract the index
  TRI_index_t* idx = TRI_LookupIndexByHandle(trx.resolver(), collection, argv[0], false, err);

  if (idx == nullptr) {
    return scope.Close(v8::ThrowException(*err));
  }

  if (idx->_type != TRI_IDX_TYPE_HASH_INDEX) {
    TRI_V8_EXCEPTION(scope, TRI_ERROR_ARANGO_NO_INDEX);
  }

  TRI_hash_index_t* hashIndex = (TRI_hash_index_t*) idx;

  // convert the example (index is locked by lockRead)
  TRI_index_search_value_t searchValue;

  TRI_document_collection_t* document = trx.documentCollection();
  TRI_shaper_t* shaper = document->getShaper();  // PROTECTED by trx from above
  int res = SetupSearchValue(&hashIndex->_paths, example, shaper, searchValue, err);

  if (res != TRI_ERROR_NO_ERROR) {
    if (res == TRI_RESULT_ELEMENT_NOT_FOUND) {
      return scope.Close(EmptyResult());
    }

    return scope.Close(v8::ThrowException(*err));
  }

  // find the matches
  TRI_index_result_t list = TRI_LookupHashIndex(idx, &searchValue);
  DestroySearchValue(shaper->_memoryZone, searchValue);

  // convert result
  size_t total = list._length;
  size_t count = 0;
  bool error = false;

  if (0 < total) {
    size_t s;
    size_t e;

    CalculateSkipLimitSlice(total, skip, limit, s, e);

    if (s < e) {
      for (size_t i = s;  i < e;  ++i) {
        v8::Handle<v8::Value> doc = WRAP_SHAPED_JSON(trx, collection->_cid, list._documents[i]->getDataPtr());

        if (doc.IsEmpty()) {
          error = true;
          break;
        }
        else {
          documents->Set((uint32_t) count++, doc);
        }
      }
    }
  }

  // free data allocated by hash index result
  TRI_DestroyIndexResult(&list);

  result->Set(v8::String::New("total"), v8::Number::New((double) total));
  result->Set(v8::String::New("count"), v8::Number::New((double) count));

  if (error) {
    TRI_V8_EXCEPTION_MEMORY(scope);
  }

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief selects documents by example using a hash index
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_ByExampleHashIndex (v8::Arguments const& argv) {
  v8::HandleScope scope;

  TRI_vocbase_col_t const* col;
  col = TRI_UnwrapClass<TRI_vocbase_col_t>(argv.Holder(), TRI_GetVocBaseColType());

  if (col == nullptr) {
    TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract collection");
  }

  TRI_SHARDING_COLLECTION_NOT_YET_IMPLEMENTED(scope, col);

  SingleCollectionReadOnlyTransaction trx(new V8TransactionContext(true), col->_vocbase, col->_cid);

  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_EXCEPTION(scope, res);
  }

  v8::Handle<v8::Object> err;

  // .............................................................................
  // inside a read transaction
  // .............................................................................

  trx.lockRead();

  v8::Handle<v8::Value> result = ByExampleHashIndexQuery(trx, col, &err, argv);

  trx.finish(res);

  // .............................................................................
  // outside a write transaction
  // .............................................................................

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief selects documents by condition using a skiplist index
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_ByConditionSkiplist (v8::Arguments const& argv) {
  std::string const signature("BY_CONDITION_SKIPLIST(<index>, <conditions>, <skip>, <limit>, <reverse>)");

  return ExecuteSkiplistQuery(argv, signature, QUERY_CONDITION);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief selects documents by example using a skiplist index
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_ByExampleSkiplist (v8::Arguments const& argv) {
  std::string const signature("BY_EXAMPLE_SKIPLIST(<index>, <example>, <skip>, <limit>, <reverse>)");

  return ExecuteSkiplistQuery(argv, signature, QUERY_EXAMPLE);
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

static v8::Handle<v8::Value> JS_ChecksumCollection (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (ServerState::instance()->isCoordinator()) {
    // renaming a collection in a cluster is unsupported
    TRI_V8_EXCEPTION(scope, TRI_ERROR_CLUSTER_UNSUPPORTED);
  }

  TRI_vocbase_col_t const* col;
  col = TRI_UnwrapClass<TRI_vocbase_col_t>(argv.Holder(), TRI_GetVocBaseColType());

  if (col == nullptr) {
    TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract collection");
  }

  TRI_SHARDING_COLLECTION_NOT_YET_IMPLEMENTED(scope, col);

  bool withRevisions = false;
  if (argv.Length() > 0) {
    withRevisions = TRI_ObjectToBoolean(argv[0]);
  }

  bool withData = false;
  if (argv.Length() > 1) {
    withData = TRI_ObjectToBoolean(argv[1]);
  }

  SingleCollectionReadOnlyTransaction trx(new V8TransactionContext(true), col->_vocbase, col->_cid);

  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_EXCEPTION(scope, res);
  }

  TRI_document_collection_t* document = trx.documentCollection();

  if (trx.orderBarrier(trx.trxCollection()) == nullptr) {
    TRI_V8_EXCEPTION(scope, TRI_ERROR_OUT_OF_MEMORY);
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

  v8::Handle<v8::Object> result = v8::Object::New();
  result->Set(v8::String::New("checksum"), v8::Number::New(helper._checksum));
  result->Set(v8::String::New("revision"), v8::String::New(rid.c_str(), (int) rid.size()));

  return scope.Close(result);
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
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_OUTPUT{edgeCollectionEdges}
/// ~ db._create("example");
///   db.relation.edges("vertex/1593622");
/// ~ db._drop("example");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_EdgesQuery (v8::Arguments const& argv) {
  return EdgesQuery(TRI_EDGE_ANY, argv);
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
///
/// @EXAMPLE_ARANGOSH_OUTPUT{edgeCollectionInEdges}
/// ~ db._create("example");
///   db.relation.inEdges("vertex/1528086");
///   db.relation.inEdges("vertex/1593622");
/// ~ db._drop("example");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_InEdgesQuery (v8::Arguments const& argv) {
  return EdgesQuery(TRI_EDGE_IN, argv);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief selects the n first documents in the collection
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_FirstQuery (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() > 1) {
    TRI_V8_EXCEPTION_USAGE(scope, "FIRST(<count>)");
  }

  int64_t count = 1;
  bool returnList = false;

  // if argument is supplied, we'll return a list - otherwise we simply return the first doc
  if (argv.Length() == 1) {
    if (! argv[0]->IsUndefined()) {
      count = TRI_ObjectToInt64(argv[0]);
      returnList = true;
    }
  }

  if (count < 1) {
    TRI_V8_EXCEPTION_PARAMETER(scope, "invalid value for <count>");
  }

  TRI_vocbase_col_t const* col;
  col = TRI_UnwrapClass<TRI_vocbase_col_t>(argv.Holder(), TRI_GetVocBaseColType());

  if (col == nullptr) {
    TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract collection");
  }

  SingleCollectionReadOnlyTransaction trx(new V8TransactionContext(true), col->_vocbase, col->_cid);

  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_EXCEPTION(scope, res);
  }

  std::vector<TRI_doc_mptr_copy_t> documents;
  res = trx.readPositional(documents, 0, count);
  trx.finish(res);

  size_t const n = documents.size();

  if (returnList) {
    v8::Handle<v8::Array> result = v8::Array::New((int) n);

    uint32_t j = 0;

    for (size_t i = 0; i < n; ++i) {
      v8::Handle<v8::Value> doc = WRAP_SHAPED_JSON(trx, col->_cid, documents[i].getDataPtr());

      if (doc.IsEmpty()) {
        // error
        TRI_V8_EXCEPTION_MEMORY(scope);
      }

      result->Set(j++, doc);
    }

    return scope.Close(result);
  }
  else {
    if (n == 0) {
      return scope.Close(v8::Null());
    }

    v8::Handle<v8::Value> result = WRAP_SHAPED_JSON(trx, col->_cid, documents[0].getDataPtr());

    if (result.IsEmpty()) {
      TRI_V8_EXCEPTION_MEMORY(scope);
    }

    return scope.Close(result);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief queries the fulltext index
///
/// the caller must ensure all relevant locks are acquired and freed
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> FulltextQuery (SingleCollectionReadOnlyTransaction& trx,
                                            TRI_vocbase_col_t const* collection,
                                            v8::Handle<v8::Object>* err,
                                            v8::Arguments const& argv) {
  v8::HandleScope scope;

  // expect: FULLTEXT(<index-handle>, <query>)
  if (argv.Length() != 2) {
    TRI_V8_EXCEPTION_USAGE(scope, "FULLTEXT(<index-handle>, <query>)");
  }

  // extract the index
  TRI_index_t* idx = TRI_LookupIndexByHandle(trx.resolver(), collection, argv[0], false, err);

  if (idx == nullptr) {
    return scope.Close(v8::ThrowException(*err));
  }

  if (idx->_type != TRI_IDX_TYPE_FULLTEXT_INDEX) {
    TRI_V8_EXCEPTION(scope, TRI_ERROR_ARANGO_NO_INDEX);
  }

  string const&& queryString = TRI_ObjectToString(argv[1]);
  bool isSubstringQuery = false;

  TRI_fulltext_query_t* query = TRI_CreateQueryFulltextIndex(TRI_FULLTEXT_SEARCH_MAX_WORDS);

  if (! query) {
    TRI_V8_EXCEPTION_MEMORY(scope);
  }

  int res = TRI_ParseQueryFulltextIndex(query, queryString.c_str(), &isSubstringQuery);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_FreeQueryFulltextIndex(query);

    TRI_V8_EXCEPTION(scope, res);
  }

  TRI_fulltext_index_t* fulltextIndex = (TRI_fulltext_index_t*) idx;

  if (isSubstringQuery && ! fulltextIndex->_indexSubstrings) {
    TRI_FreeQueryFulltextIndex(query);

    TRI_V8_EXCEPTION(scope, TRI_ERROR_NOT_IMPLEMENTED);
  }

  TRI_fulltext_result_t* queryResult = TRI_QueryFulltextIndex(fulltextIndex->_fulltextIndex, query);

  if (! queryResult) {
    TRI_V8_EXCEPTION_INTERNAL(scope, "internal error in fulltext index query");
  }

  if (trx.orderBarrier(trx.trxCollection()) == nullptr) {
    TRI_V8_EXCEPTION(scope, TRI_ERROR_OUT_OF_MEMORY);
  }

  // setup result
  v8::Handle<v8::Object> result = v8::Object::New();

  v8::Handle<v8::Array> documents = v8::Array::New();
  result->Set(v8::String::New("documents"), documents);

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
    TRI_V8_EXCEPTION_MEMORY(scope);
  }

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief queries the fulltext index
/// @startDocuBlock collectionFulltext
/// `collection.fulltext(attribute, query)`
///
/// The *FULLTEXT* operator performs a fulltext search on the specified
/// *attribute* and the specified *query*.
///
/// Details about the fulltext query syntax can be found below.
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_OUTPUT{collectionFulltext}
/// ~ db._drop("emails");
/// ~ db._create("emails");
///   db.emails.ensureFulltextIndex("content").id;
///   db.emails.save({ content: "Hello Alice, how are you doing? Regards, Bob" });
///   db.emails.save({ content: "Hello Charlie, do Alice and Bob know about it?" });
///   db.emails.save({ content: "I think they don't know. Regards, Eve" });
///   db.emails.fulltext("content", "charlie,|eve").toArray();
/// ~ db._drop("emails");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_FulltextQuery (v8::Arguments const& argv) {
  v8::HandleScope scope;

  TRI_vocbase_col_t const* col;
  col = TRI_UnwrapClass<TRI_vocbase_col_t>(argv.Holder(), TRI_GetVocBaseColType());

  if (col == nullptr) {
    TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract collection");
  }

  TRI_SHARDING_COLLECTION_NOT_YET_IMPLEMENTED(scope, col);

  SingleCollectionReadOnlyTransaction trx(new V8TransactionContext(true), col->_vocbase, col->_cid);

  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_EXCEPTION(scope, res);
  }

  v8::Handle<v8::Object> err;

  // .............................................................................
  // inside a read transaction
  // .............................................................................

  trx.lockRead();

  v8::Handle<v8::Value> result = FulltextQuery(trx, col, &err, argv);

  trx.finish(res);

  // .............................................................................
  // outside a write transaction
  // .............................................................................

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief selects the n last documents in the collection
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_LastQuery (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() > 1) {
    TRI_V8_EXCEPTION_USAGE(scope, "LAST(<count>)");
  }

  int64_t count = 1;
  bool returnList = false;

  // if argument is supplied, we'll return a list - otherwise we simply return the last doc
  if (argv.Length() == 1) {
    if (! argv[0]->IsUndefined()) {
      count = TRI_ObjectToInt64(argv[0]);
      returnList = true;
    }
  }

  if (count < 1) {
    TRI_V8_EXCEPTION_PARAMETER(scope, "invalid value for <count>");
  }

  TRI_vocbase_col_t const* col;
  col = TRI_UnwrapClass<TRI_vocbase_col_t>(argv.Holder(), TRI_GetVocBaseColType());

  if (col == nullptr) {
    TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract collection");
  }

  TRI_SHARDING_COLLECTION_NOT_YET_IMPLEMENTED(scope, col);

  SingleCollectionReadOnlyTransaction trx(new V8TransactionContext(true), col->_vocbase, col->_cid);

  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_EXCEPTION(scope, res);
  }

  vector<TRI_doc_mptr_copy_t> documents;
  res = trx.readPositional(documents, -1, count);
  trx.finish(res);

  size_t const n = documents.size();

  if (returnList) {
    v8::Handle<v8::Array> result = v8::Array::New((int) n);

    uint32_t j = 0;

    for (size_t i = 0; i < n; ++i) {
      v8::Handle<v8::Value> doc = WRAP_SHAPED_JSON(trx, col->_cid, documents[i].getDataPtr());

      if (doc.IsEmpty()) {
        // error
        TRI_V8_EXCEPTION_MEMORY(scope);
      }

      result->Set(j++, doc);
    }

    return scope.Close(result);
  }
  else {
    if (n == 0) {
      return scope.Close(v8::Null());
    }

    v8::Handle<v8::Value> result = WRAP_SHAPED_JSON(trx, col->_cid, documents[0].getDataPtr());

    if (result.IsEmpty()) {
      TRI_V8_EXCEPTION_MEMORY(scope);
    }

    return scope.Close(result);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief selects points near a given coordinate
///
/// the caller must ensure all relevant locks are acquired and freed
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> NearQuery (SingleCollectionReadOnlyTransaction& trx,
                                        TRI_vocbase_col_t const* collection,
                                        v8::Handle<v8::Object>* err,
                                        v8::Arguments const& argv) {
  v8::HandleScope scope;

  // expect: NEAR(<index-id>, <latitude>, <longitude>, <limit>)
  if (argv.Length() != 4) {
    TRI_V8_EXCEPTION_USAGE(scope, "NEAR(<index-handle>, <latitude>, <longitude>, <limit>)");
  }

  // extract the index
  TRI_index_t* idx = TRI_LookupIndexByHandle(trx.resolver(), collection, argv[0], false, err);

  if (idx == nullptr) {
    return scope.Close(v8::ThrowException(*err));
  }

  if (idx->_type != TRI_IDX_TYPE_GEO1_INDEX &&
      idx->_type != TRI_IDX_TYPE_GEO2_INDEX) {
    TRI_V8_EXCEPTION(scope, TRI_ERROR_ARANGO_NO_INDEX);
  }

  // extract latitude and longitude
  double latitude = TRI_ObjectToDouble(argv[1]);
  double longitude = TRI_ObjectToDouble(argv[2]);

  // extract the limit
  TRI_voc_ssize_t limit = (TRI_voc_ssize_t) TRI_ObjectToDouble(argv[3]);

  // setup result
  v8::Handle<v8::Object> result = v8::Object::New();

  v8::Handle<v8::Array> documents = v8::Array::New();
  result->Set(v8::String::New("documents"), documents);

  v8::Handle<v8::Array> distances = v8::Array::New();
  result->Set(v8::String::New("distances"), distances);

  GeoCoordinates* cors = TRI_NearestGeoIndex(idx, latitude, longitude, limit);

  if (cors != 0) {
    int res = StoreGeoResult(trx, collection, cors, documents, distances);

    if (res != TRI_ERROR_NO_ERROR) {
      TRI_V8_EXCEPTION(scope, res);
    }
  }

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief selects points near a given coordinate
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_NearQuery (v8::Arguments const& argv) {
  v8::HandleScope scope;

  TRI_vocbase_col_t const* col;
  col = TRI_UnwrapClass<TRI_vocbase_col_t>(argv.Holder(), TRI_GetVocBaseColType());

  if (col == nullptr) {
    TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract collection");
  }

  TRI_SHARDING_COLLECTION_NOT_YET_IMPLEMENTED(scope, col);

  SingleCollectionReadOnlyTransaction trx(new V8TransactionContext(true), col->_vocbase, col->_cid);

  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_EXCEPTION(scope, res);
  }

  v8::Handle<v8::Object> err;

  // .............................................................................
  // inside a read transaction
  // .............................................................................

  trx.lockRead();

  v8::Handle<v8::Value> result = NearQuery(trx, col, &err, argv);

  trx.finish(res);

  // .............................................................................
  // outside a write transaction
  // .............................................................................

  return scope.Close(result);
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
///
/// @EXAMPLE_ARANGOSH_OUTPUT{edgeCollectionOutEdges}
/// ~ db._create("example");
///   db.relation.inEdges("vertex/1528086");
///   db.relation.inEdges("vertex/1593622");
/// ~ db._drop("example");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_OutEdgesQuery (v8::Arguments const& argv) {
  return EdgesQuery(TRI_EDGE_OUT, argv);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief selects points within a given radius
///
/// the caller must ensure all relevant locks are acquired and freed
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> WithinQuery (SingleCollectionReadOnlyTransaction& trx,
                                          TRI_vocbase_col_t const* collection,
                                          v8::Handle<v8::Object>* err,
                                          v8::Arguments const& argv) {
  v8::HandleScope scope;

  // expect: WITHIN(<index-handle>, <latitude>, <longitude>, <radius>)
  if (argv.Length() != 4) {
    TRI_V8_EXCEPTION_USAGE(scope, "WITHIN(<index-handle>, <latitude>, <longitude>, <radius>)");
  }

  // extract the index
  TRI_index_t* idx = TRI_LookupIndexByHandle(trx.resolver(), collection, argv[0], false, err);

  if (idx == nullptr) {
    return scope.Close(v8::ThrowException(*err));
  }

  if (idx->_type != TRI_IDX_TYPE_GEO1_INDEX &&
      idx->_type != TRI_IDX_TYPE_GEO2_INDEX) {
    TRI_V8_EXCEPTION(scope, TRI_ERROR_ARANGO_NO_INDEX);
  }

  // extract latitude and longitude
  double latitude = TRI_ObjectToDouble(argv[1]);
  double longitude = TRI_ObjectToDouble(argv[2]);

  // extract the radius
  double radius = TRI_ObjectToDouble(argv[3]);

  // setup result
  v8::Handle<v8::Object> result = v8::Object::New();

  v8::Handle<v8::Array> documents = v8::Array::New();
  result->Set(v8::String::New("documents"), documents);

  v8::Handle<v8::Array> distances = v8::Array::New();
  result->Set(v8::String::New("distances"), distances);

  GeoCoordinates* cors = TRI_WithinGeoIndex(idx, latitude, longitude, radius);

  if (cors != 0) {
    int res = StoreGeoResult(trx, collection, cors, documents, distances);

    if (res != TRI_ERROR_NO_ERROR) {
      TRI_V8_EXCEPTION(scope, res);
    }
  }

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief selects points within a given radius
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_WithinQuery (v8::Arguments const& argv) {
  v8::HandleScope scope;

  TRI_vocbase_col_t const* col;
  col = TRI_UnwrapClass<TRI_vocbase_col_t>(argv.Holder(), TRI_GetVocBaseColType());

  if (col == nullptr) {
    TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract collection");
  }

  TRI_SHARDING_COLLECTION_NOT_YET_IMPLEMENTED(scope, col);

  SingleCollectionReadOnlyTransaction trx(new V8TransactionContext(true), col->_vocbase, col->_cid);

  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_EXCEPTION(scope, res);
  }

  v8::Handle<v8::Object> err;

  // .............................................................................
  // inside a read transaction
  // .............................................................................

  trx.lockRead();

  v8::Handle<v8::Value> result = WithinQuery(trx, col, &err, argv);

  trx.finish(res);

  // .............................................................................
  // outside a write transaction
  // .............................................................................

  return scope.Close(result);
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

void TRI_InitV8Queries (v8::Handle<v8::Context> context) {
  v8::HandleScope scope;

  v8::Handle<v8::ObjectTemplate> rt;

  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  TRI_v8_global_t* v8g = static_cast<TRI_v8_global_t*>(isolate->GetData());

  TRI_ASSERT(v8g != nullptr);

  // .............................................................................
  // generate the TRI_vocbase_col_t template
  // .............................................................................

  rt = v8g->VocbaseColTempl;

  TRI_AddMethodVocbase(rt, "ALL", JS_AllQuery, true);
  TRI_AddMethodVocbase(rt, "ANY", JS_AnyQuery, true);
  TRI_AddMethodVocbase(rt, "BY_CONDITION_SKIPLIST", JS_ByConditionSkiplist, true);
  TRI_AddMethodVocbase(rt, "BY_EXAMPLE", JS_ByExampleQuery, true);
  TRI_AddMethodVocbase(rt, "BY_EXAMPLE_HASH", JS_ByExampleHashIndex, true);
  TRI_AddMethodVocbase(rt, "BY_EXAMPLE_SKIPLIST", JS_ByExampleSkiplist, true);
  TRI_AddMethodVocbase(rt, "checksum", JS_ChecksumCollection);
  TRI_AddMethodVocbase(rt, "EDGES", JS_EdgesQuery, true);
  TRI_AddMethodVocbase(rt, "FIRST", JS_FirstQuery, true);
  TRI_AddMethodVocbase(rt, "FULLTEXT", JS_FulltextQuery, true);
  TRI_AddMethodVocbase(rt, "INEDGES", JS_InEdgesQuery, true);
  TRI_AddMethodVocbase(rt, "LAST", JS_LastQuery, true);
  TRI_AddMethodVocbase(rt, "NEAR", JS_NearQuery, true);
  
  // internal method. not intended to be used by end-users
  TRI_AddMethodVocbase(rt, "NTH", JS_NthQuery, true);
  TRI_AddMethodVocbase(rt, "NTH2", JS_Nth2Query, true);
  TRI_AddMethodVocbase(rt, "NTH3", JS_Nth3Query, true);

  // internal method. not intended to be used by end-users
  TRI_AddMethodVocbase(rt, "OFFSET", JS_OffsetQuery, true);

  TRI_AddMethodVocbase(rt, "OUTEDGES", JS_OutEdgesQuery, true);
  TRI_AddMethodVocbase(rt, "WITHIN", JS_WithinQuery, true);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
