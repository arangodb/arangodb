////////////////////////////////////////////////////////////////////////////////
/// @brief V8-vocbase queries
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2012 triAGENS GmbH, Cologne, Germany
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

#include <string>

#include "v8-query.h"

#include "BasicsC/logging.h"
#include "HashIndex/hashindex.h"
#include "SkipLists/skiplistIndex.h"
#include "V8/v8-conv.h"
#include "V8/v8-utils.h"
#include "V8Server/v8-vocbase.h"

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
/// @brief extracts skip and limit
////////////////////////////////////////////////////////////////////////////////

static void ExtractSkipAndLimit (v8::Arguments const& argv,
                                 size_t pos,
                                 TRI_voc_ssize_t& skip,
                                 TRI_voc_size_t& limit) {

  skip = TRI_QRY_NO_SKIP;
  limit = TRI_QRY_NO_LIMIT;

  if (pos < (size_t) argv.Length() && ! argv[pos]->IsNull()) {
    skip = (TRI_voc_size_t) TRI_ObjectToDouble(argv[pos]);
  }

  if (pos + 1 < (size_t) argv.Length() && ! argv[pos + 1]->IsNull()) {
    limit = (TRI_voc_ssize_t) TRI_ObjectToDouble(argv[pos + 1]);
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
    s = skip;

    if (e < s) {
      s = e;
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
    e = s + limit;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief cleans up the example object
////////////////////////////////////////////////////////////////////////////////

static void CleanupExampleObject (TRI_shaper_t* shaper,
                                  size_t n,
                                  TRI_shape_pid_t* pids,
                                  TRI_shaped_json_t** values) {

  // clean shaped json objects
  for (size_t j = 0;  j < n;  ++j) {
    TRI_FreeShapedJson(shaper, values[j]);
  }

  TRI_Free(TRI_UNKNOWN_MEM_ZONE, values);

  if (pids != 0) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, pids);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets up the example object
////////////////////////////////////////////////////////////////////////////////

static int SetupExampleObject (v8::Handle<v8::Object> example,
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
  // TODO: memory allocation might fail
  values = (TRI_shaped_json_t**) TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, n * sizeof(TRI_shaped_json_t*), false);
  // TODO: memory allocation might fail

  // convert
  for (size_t i = 0;  i < n;  ++i) {
    v8::Handle<v8::Value> key = names->Get(i);
    v8::Handle<v8::Value> val = example->Get(key);

    v8::String::Utf8Value keyStr(key);

    if (*keyStr != 0) {
      pids[i] = shaper->findAttributePathByName(shaper, *keyStr);
      values[i] = TRI_ShapedJsonV8Object(val, shaper);
    }

    if (*keyStr == 0 || pids[i] == 0 || values[i] == 0) {
      CleanupExampleObject(shaper, i, pids, values);

      if (*keyStr == 0) {
        *err = TRI_CreateErrorObject(TRI_ERROR_BAD_PARAMETER,
                                     "cannot convert attribute path to UTF8");
        return TRI_ERROR_BAD_PARAMETER;
      }
      else if (pids[i] == 0) {
        *err = TRI_CreateErrorObject(TRI_ERROR_BAD_PARAMETER,
                                     "cannot convert to attribute path");
        return TRI_ERROR_BAD_PARAMETER;
      }
      else {
        *err = TRI_CreateErrorObject(TRI_ERROR_BAD_PARAMETER,
                                     "cannot convert value to JSON");
        return TRI_ERROR_BAD_PARAMETER;
      }

      assert(false);
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
  TRI_index_operator_t* lastOperator = 0;
  TRI_json_t* parameters = TRI_CreateListJson(TRI_UNKNOWN_MEM_ZONE);
  size_t numEq = 0;
  size_t lastNonEq = 0;

  if (parameters == 0) {
    return 0;
  }

  // iterate over all index fields
  for (size_t i = 1; i <= idx->_fields._length; ++i) {
    v8::Handle<v8::String> key = v8::String::New(idx->_fields._buffer[i - 1]);

    if (!conditions->HasOwnProperty(key)) {
      break;
    }
    v8::Handle<v8::Value> fieldConditions = conditions->Get(key);

    if (!fieldConditions->IsArray()) {
      // wrong data type for field conditions
      break;
    }

    // iterator over all conditions
    v8::Handle<v8::Array> values = v8::Handle<v8::Array>::Cast(fieldConditions);
    for (uint32_t j = 0; j < values->Length(); ++j) {
      v8::Handle<v8::Value> fieldCondition = values->Get(j);

      if (!fieldCondition->IsArray()) {
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

      if (!op->IsString()) {
        // wrong operator type
        goto MEM_ERROR;
      }

      TRI_json_t* json = TRI_JsonObject(value);

      if (!json) {
        goto MEM_ERROR;
      }

      std::string opValue = TRI_ObjectToString(op);
      if (opValue == "==") {
        // equality comparison

        if (lastNonEq > 0) {
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
          goto MEM_ERROR;
        }

        lastNonEq = i;

        TRI_json_t* cloned = TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, parameters);
        if (cloned == 0) {
          goto MEM_ERROR;
        }

        TRI_PushBack3ListJson(TRI_UNKNOWN_MEM_ZONE, cloned, json);

        if (numEq) {
          // create equality operator if one is in queue
          TRI_json_t* clonedParams = TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, parameters);
          if (clonedParams == 0) {
            TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, cloned);
            goto MEM_ERROR;
          }
          lastOperator = TRI_CreateIndexOperator(TRI_EQ_INDEX_OPERATOR, NULL, NULL, clonedParams, shaper, NULL, clonedParams->_value._objects._length, NULL);
          numEq = 0;
        }

        TRI_index_operator_t* current = 0;

        // create the operator for the current condition
        current = TRI_CreateIndexOperator(opType, NULL, NULL, cloned, shaper, NULL, cloned->_value._objects._length, NULL);
        if (current == 0) {
          TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, cloned);
          goto MEM_ERROR;
        }

        if (lastOperator == 0) {
          lastOperator = current;
        }
        else {
          // merge the current operator with previous operators using logical AND
          TRI_index_operator_t* newOperator = TRI_CreateIndexOperator(TRI_AND_INDEX_OPERATOR, lastOperator, current, NULL, shaper, NULL, 2, NULL);

          if (newOperator == 0) {
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
    assert(lastOperator == 0);
    assert(lastNonEq == 0);

    TRI_json_t* clonedParams = TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, parameters);
    if (clonedParams == 0) {
      goto MEM_ERROR;
    }
    lastOperator = TRI_CreateIndexOperator(TRI_EQ_INDEX_OPERATOR, NULL, NULL, clonedParams, shaper, NULL, clonedParams->_value._objects._length, NULL);
  }

  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, parameters);

  return lastOperator;

MEM_ERROR:
  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, parameters);

  if (lastOperator == 0) {
    TRI_FreeIndexOperator(lastOperator);
  }

  return 0;
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

  if (parameters == 0) {
    return 0;
  }

  for (size_t i = 0; i < idx->_fields._length; ++i) {
    v8::Handle<v8::String> key = v8::String::New(idx->_fields._buffer[i]);

    if (!example->HasOwnProperty(key)) {
      break;
    }

    v8::Handle<v8::Value> value = example->Get(key);

    TRI_json_t* json = TRI_JsonObject(value);

    if (!json) {
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, parameters);

      return 0;
    }

    TRI_PushBack3ListJson(TRI_UNKNOWN_MEM_ZONE, parameters, json);
  }

  if (parameters->_value._objects._length > 0) {
    // example means equality comparisons only
    return TRI_CreateIndexOperator(TRI_EQ_INDEX_OPERATOR, NULL, NULL, parameters, shaper, NULL, parameters->_value._objects._length, NULL);
  }

  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, parameters);
  return 0;
}




////////////////////////////////////////////////////////////////////////////////
/// @brief sets up the bitarray index operator for a bitarray condition query
////////////////////////////////////////////////////////////////////////////////

static TRI_index_operator_t* SetupConditionsBitarray (TRI_index_t* idx, TRI_shaper_t* shaper, v8::Handle<v8::Object> conditions) {
  assert(false);
}  

////////////////////////////////////////////////////////////////////////////////
/// @brief creates an index operator for a bitarray example query
///
/// this will set up a JSON container with the example values as a list
/// at the end, one skiplist equality operator is created for the entire list
////////////////////////////////////////////////////////////////////////////////

static TRI_index_operator_t* SetupExampleBitarray (TRI_index_t* idx, TRI_shaper_t* shaper, v8::Handle<v8::Object> example) {
  TRI_json_t* parameters = TRI_CreateListJson(TRI_UNKNOWN_MEM_ZONE);

  if (parameters == 0) {
    return 0;
  }

  
  // ........................................................................
  // Observe that the client can have sent any number of parameters which
  // do not match the list of attributes defined in the index.
  // These parameters are IGNORED -- no error is reported.
  // ........................................................................
  
  for (size_t i = 0; i < idx->_fields._length; ++i) {
    v8::Handle<v8::String> key = v8::String::New(idx->_fields._buffer[i]);
    TRI_json_t* json;

    // ......................................................................
    // The client may have sent values for all of the Attributes or for 
    // a subset of them. If the value for an Attribute is missing, then we
    // assume that the client wishes to IGNORE the value of that Attribute.
    // In the later case, we add the json object 'TRI_JSON_UNUSED' to 
    // indicate that this attribute is to be ignored. Notice that it is
    // possible to ignore all the attributes defined as part of the index.
    // ......................................................................
    

    if (example->HasOwnProperty(key)) {
      // ....................................................................
      // for this index attribute, there is such an attribute given as a 
      // as a parameter by the client -- determine the value (or values)
      // of this attribute parameter and store it for later use in the 
      // lookup
      // ....................................................................
      v8::Handle<v8::Value> value = example->Get(key);
      json = TRI_JsonObject(value);
    }
    
    else {
      // ....................................................................
      // for this index attribute we can not locate it in the list of parameters
      // sent to us by the client. Assign it an 'unused' (perhaps should be
      // renamed to 'unknown' or 'undefined').
      // ....................................................................
      json = (TRI_json_t*) TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_json_t), true);
      json->_type = TRI_JSON_UNUSED;
    }

    
    // ......................................................................
    // Check and ensure we have a json object defined before we store it.
    // ......................................................................
    
    if (json == 0) {
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, parameters);
      return 0;
    }

    
    // ......................................................................
    // store it in an list json object -- eventually wil be stored as part
    // of the index operator.
    // ......................................................................
    
    TRI_PushBack3ListJson(TRI_UNKNOWN_MEM_ZONE, parameters, json);
  }


  return TRI_CreateIndexOperator(TRI_EQ_INDEX_OPERATOR, NULL, NULL, parameters, shaper, NULL, parameters->_value._objects._length, NULL);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets up the example object for hash index
////////////////////////////////////////////////////////////////////////////////

static int SetupExampleObjectIndex (TRI_hash_index_t* hashIndex,
                                    v8::Handle<v8::Object> example,
                                    TRI_shaper_t* shaper,
                                    size_t& n,
                                    TRI_shaped_json_t**& values,
                                    v8::Handle<v8::Object>* err) {

  // extract attribute paths
  n = hashIndex->_paths._length;

  // setup storage
  values = (TRI_shaped_json_t**) TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, n * sizeof(TRI_shaped_json_t*), false);
  // TODO: memory allocation might fail

  // convert
  for (size_t i = 0;  i < n;  ++i) {
    TRI_shape_pid_t pid = * (TRI_shape_pid_t*) TRI_AtVector(&hashIndex->_paths, i);
    char const* name = TRI_AttributeNameShapePid(shaper, pid);

    if (name == NULL) {
      CleanupExampleObject(shaper, i, 0, values);
      *err = TRI_CreateErrorObject(TRI_ERROR_INTERNAL, "shaper failed");
      return TRI_ERROR_BAD_PARAMETER;
    }

    v8::Handle<v8::String> key = v8::String::New(name);

    if (example->HasOwnProperty(key)) {
      v8::Handle<v8::Value> val = example->Get(key);

      values[i] = TRI_ShapedJsonV8Object(val, shaper);
    }
    else {
      values[i] = TRI_ShapedJsonV8Object(v8::Null(), shaper);
    }

    if (values[i] == 0) {
      CleanupExampleObject(shaper, i, 0, values);

      *err = TRI_CreateErrorObject(TRI_ERROR_BAD_PARAMETER,
                                   "cannot convert value to JSON");
      return TRI_ERROR_BAD_PARAMETER;
    }
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief execute a skiplist query (by condition or by example)
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> ExecuteSkiplistQuery (v8::Arguments const& argv, std::string const& signature, const query_t type) {
  v8::HandleScope scope;

  // extract and use the simple collection
  v8::Handle<v8::Object> err;
  TRI_vocbase_col_t const* collection;
  TRI_sim_collection_t* sim = TRI_ExtractAndUseSimpleCollection(argv, collection, &err);

  if (sim == 0) {
    return scope.Close(v8::ThrowException(err));
  }

  // expecting index, example, skip, and limit
  if (argv.Length() < 2) {
    TRI_ReleaseCollection(collection);

    std::string usage("Usage: ");
    usage += signature;
    return scope.Close(v8::ThrowException(
                         TRI_CreateErrorObject(TRI_ERROR_BAD_PARAMETER,
                                               usage)));
  }

  if (! argv[1]->IsObject()) {
    TRI_ReleaseCollection(collection);
    std::string msg;

    if (type == QUERY_EXAMPLE) {
      msg = "<example> must be an object";
    }
    else {
      msg = "<conditions> must be an object";
    }
    return scope.Close(v8::ThrowException(
                       TRI_CreateErrorObject(TRI_ERROR_BAD_PARAMETER, msg)));
  }

  TRI_shaper_t* shaper = sim->base._shaper;

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

  collection->_collection->beginRead(collection->_collection);

  // extract the index
  TRI_index_t* idx = TRI_LookupIndexByHandle(sim->base.base._vocbase, collection, argv[0], false, &err);

  if (idx == 0) {
    collection->_collection->endRead(collection->_collection);

    TRI_ReleaseCollection(collection);
    return scope.Close(v8::ThrowException(err));
  }

  if (idx->_type != TRI_IDX_TYPE_SKIPLIST_INDEX) {
    collection->_collection->endRead(collection->_collection);

    TRI_ReleaseCollection(collection);
    return scope.Close(v8::ThrowException(TRI_CreateErrorObject(TRI_ERROR_BAD_PARAMETER, "index must be a skiplist index")));
  }

  TRI_index_operator_t* skiplistOperator;
  v8::Handle<v8::Object> values = argv[1]->ToObject();
  if (type == QUERY_EXAMPLE) {
    skiplistOperator = SetupExampleSkiplist(idx, shaper, values);
  }
  else {
    skiplistOperator = SetupConditionsSkiplist(idx, shaper, values);
  }

  if (!skiplistOperator) {
    collection->_collection->endRead(collection->_collection);

    TRI_ReleaseCollection(collection);
    return scope.Close(v8::ThrowException(TRI_CreateErrorObject(TRI_ERROR_BAD_PARAMETER, "setting up skiplist operator failed")));
  }

  TRI_skiplist_iterator_t* skiplistIterator = TRI_LookupSkiplistIndex(idx, skiplistOperator);

  TRI_barrier_t* barrier = 0;
  TRI_voc_ssize_t total = 0;
  TRI_voc_size_t count = 0;

  while (true) {
    SkiplistIndexElement* indexElement = (SkiplistIndexElement*) skiplistIterator->_next(skiplistIterator);

    if (indexElement == NULL) {
      break;
    }

    ++total;

    if (total > skip && count < limit) {
      if (barrier == 0) {
        barrier = TRI_CreateBarrierElement(&sim->base._barrierList);
      }
      // TODO: barrier might be 0
      documents->Set(count, TRI_WrapShapedJson(collection, (TRI_doc_mptr_t const*) indexElement->data, barrier));
      ++count;
    }
  }

  collection->_collection->endRead(collection->_collection);

  // .............................................................................
  // outside a write transaction
  // .............................................................................

  // free data allocated by skiplist index result
  TRI_FreeSkiplistIterator(skiplistIterator);

  result->Set(v8::String::New("total"), v8::Number::New((double) total));
  result->Set(v8::String::New("count"), v8::Number::New(count));

  TRI_ReleaseCollection(collection);

  return scope.Close(result);
}



////////////////////////////////////////////////////////////////////////////////
/// @brief execute a bitarray index query (by condition or by example)
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
// Example of a filter associated with an interator
////////////////////////////////////////////////////////////////////////////////

static bool BitarrayFilterExample(TRI_index_iterator_t* indexIterator) {
  BitarrayIndexElement* indexElement;
  TRI_bitarray_index_t* baIndex;
  // TRI_doc_mptr_t* doc;
    
    
  indexElement = (BitarrayIndexElement*) indexIterator->_next(indexIterator);

  if (indexElement == NULL) {
    return false;
  }

  baIndex = (TRI_bitarray_index_t*) indexIterator->_index;
    
  if (baIndex == NULL) {     
    return false;
  }
    
  /* doc = (TRI_doc_mptr_t*) indexElement->data; */
    
  // ..........................................................................
  // Now perform any additional filter operations you require on the doc
  // using baIndex which you now have access to.
  // ..........................................................................
    
  return true;
}

static v8::Handle<v8::Value> ExecuteBitarrayQuery (v8::Arguments const& argv, std::string const& signature, const query_t type) {
  v8::HandleScope scope;
  v8::Handle<v8::Object> err;
  const TRI_vocbase_col_t* collection;
  TRI_voc_ssize_t skip;
  TRI_voc_size_t limit;

  // ...........................................................................
  // extract and use the simple collection
  // ...........................................................................

  
  TRI_sim_collection_t* sim = TRI_ExtractAndUseSimpleCollection(argv, collection, &err);

  if (sim == 0) {
    return scope.Close(v8::ThrowException(err));
  }


                                               
  // ...........................................................................
  // Check the parameters, expecting index, example, skip, and limit
  // e.g. ("110597/962565", {"x":1}, null, null)
  // ...........................................................................

  if (argv.Length() < 2) {
    TRI_ReleaseCollection(collection);

    std::string usage("Usage: ");
    usage += signature;
    return scope.Close(v8::ThrowException(TRI_CreateErrorObject(TRI_ERROR_BAD_PARAMETER,usage)));
  }
  

  // ...........................................................................
  // Check that the second parameter is an associative array (json object)
  // ...........................................................................
  
  if (! argv[1]->IsObject()) {
    TRI_ReleaseCollection(collection);
    std::string msg;

    if (type == QUERY_EXAMPLE) {
      msg = "<example> must be an object";
    }
    else {
      msg = "<conditions> must be an object";
    }
    return scope.Close(v8::ThrowException(TRI_CreateErrorObject(TRI_ERROR_BAD_PARAMETER, msg)));
  }
 
  
  TRI_shaper_t* shaper = sim->base._shaper;

  
  // .............................................................................
  // extract skip and limit
  // .............................................................................

  ExtractSkipAndLimit(argv, 2, skip, limit);

  
  // .............................................................................
  // Create the json object result which stores documents located
  // .............................................................................
  
  v8::Handle<v8::Object> result = v8::Object::New();

  
  // .............................................................................
  // Create the array to store documents located
  // .............................................................................
  
  v8::Handle<v8::Array> documents = v8::Array::New();
  result->Set(v8::String::New("documents"), documents);

  
  // .............................................................................
  // inside a read transaction
  // .............................................................................

  collection->_collection->beginRead(collection->_collection);


  // .............................................................................
  // extract the index
  // .............................................................................
  
  TRI_index_t* idx = TRI_LookupIndexByHandle(sim->base.base._vocbase, collection, argv[0], false, &err);

  if (idx == 0) {
    collection->_collection->endRead(collection->_collection);

    TRI_ReleaseCollection(collection);
    return scope.Close(v8::ThrowException(err));
  }
  
  
  if (idx->_type != TRI_IDX_TYPE_BITARRAY_INDEX) {
    collection->_collection->endRead(collection->_collection);

    TRI_ReleaseCollection(collection);
    return scope.Close(v8::ThrowException(TRI_CreateErrorObject(TRI_ERROR_BAD_PARAMETER, "index must be a skiplist index")));
  }

  
  TRI_index_operator_t* indexOperator;
  v8::Handle<v8::Object> values = argv[1]->ToObject();
  if (type == QUERY_EXAMPLE) {
    indexOperator = SetupExampleBitarray(idx, shaper, values);
  }
  else {
    indexOperator = SetupConditionsBitarray(idx, shaper, values);
  }

  
  if (indexOperator == 0) { // something wrong
    collection->_collection->endRead(collection->_collection);

    TRI_ReleaseCollection(collection);
    return scope.Close(v8::ThrowException(TRI_CreateErrorObject(TRI_ERROR_BAD_PARAMETER, "setting up skiplist operator failed")));
  }

  
  // .............................................................................
  // attempt to locate the documents 
  // .............................................................................
  
  TRI_index_iterator_t* indexIterator = TRI_LookupBitarrayIndex(idx, indexOperator, BitarrayFilterExample);

  
  // .............................................................................
  // Take care of the case where the index iterator is returned as NULL -- may
  // occur when some catasphric error occurs.
  // .............................................................................
  
  TRI_barrier_t* barrier = 0;
  TRI_voc_ssize_t total = 0;
  TRI_voc_size_t count = 0;

  if (indexIterator != NULL) {
  
    while (true) {
      TRI_doc_mptr_t* data = (TRI_doc_mptr_t*) indexIterator->_next(indexIterator);

      if (data == NULL) {
        break;
      }

      
      ++total;

      if (total > skip && count < limit) {
        if (barrier == 0) {
          barrier = TRI_CreateBarrierElement(&sim->base._barrierList);
        }
        // TODO: barrier might be 0
        documents->Set(count, TRI_WrapShapedJson(collection, data, barrier));
        ++count;
      }
    }
    
    // free data allocated by index result
    TRI_FreeIndexIterator(indexIterator);
  }
  
  else {
    LOG_WARNING("index iterator returned with a NULL value in ExecuteBitarrayQuery");
  }
  
  collection->_collection->endRead(collection->_collection);

  // .............................................................................
  // outside a write transaction
  // .............................................................................


  result->Set(v8::String::New("total"), v8::Number::New((double) total));
  result->Set(v8::String::New("count"), v8::Number::New(count));

  TRI_ReleaseCollection(collection);

  return scope.Close(result);
}


////////////////////////////////////////////////////////////////////////////////
/// @brief sorts geo coordinates
////////////////////////////////////////////////////////////////////////////////

static int CompareGeoCoordinateDistance (geo_coordinate_distance_t* left,
                                         geo_coordinate_distance_t* right) {
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

#include "BasicsC/fsrt.inc"

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a geo result
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
    GeoIndex_CoordinatesFree(cors);
    return;
  }

  gtr = (tmp = (geo_coordinate_distance_t*) TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(geo_coordinate_distance_t) * n, false));
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
  // TODO: barrier might be 0

  // copy the documents
  for (gtr = tmp, i = 0;  gtr < gnd;  ++gtr, ++i) {
    documents->Set(i, TRI_WrapShapedJson(collection, (TRI_doc_mptr_t const*) gtr->_data, barrier));
    distances->Set(i, v8::Number::New(gtr->_distance));
  }

  TRI_Free(TRI_UNKNOWN_MEM_ZONE, tmp);
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
/// @brief looks up edges for given direction
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> EdgesQuery (TRI_edge_direction_e direction, v8::Arguments const& argv) {
  v8::HandleScope scope;

  // extract and use the simple collection
  v8::Handle<v8::Object> err;
  TRI_vocbase_col_t const* collection;
  TRI_sim_collection_t* sim = TRI_ExtractAndUseSimpleCollection(argv, collection, &err);

  if (sim == 0) {
    return scope.Close(v8::ThrowException(err));
  }

  // first and only argument schould be a list of document idenfifier
  if (argv.Length() != 1) {
    TRI_ReleaseCollection(collection);

    switch (direction) {
      case TRI_EDGE_UNUSED:
        return scope.Close(v8::ThrowException(
                             TRI_CreateErrorObject(TRI_ERROR_BAD_PARAMETER,
                                                   "usage: edges(<vertices>)")));

      case TRI_EDGE_IN:
        return scope.Close(v8::ThrowException(
                             TRI_CreateErrorObject(TRI_ERROR_BAD_PARAMETER,
                                                   "usage: inEdges(<vertices>)")));

      case TRI_EDGE_OUT:
        return scope.Close(v8::ThrowException(
                             TRI_CreateErrorObject(TRI_ERROR_BAD_PARAMETER,
                                                   "usage: outEdges(<vertices>)")));

      case TRI_EDGE_ANY:
        return scope.Close(v8::ThrowException(
                             TRI_CreateErrorObject(TRI_ERROR_BAD_PARAMETER,
                                                   "usage: edges(<vertices>)")));
    }
  }

  // setup result
  v8::Handle<v8::Array> documents = v8::Array::New();

  // .............................................................................
  // inside a read transaction
  // .............................................................................

  collection->_collection->beginRead(collection->_collection);

  TRI_barrier_t* barrier = 0;
  uint32_t count = 0;

  // argument is a list of vertices
  if (argv[0]->IsArray()) {
    v8::Handle<v8::Array> vertices = v8::Handle<v8::Array>::Cast(argv[0]);
    uint32_t len = vertices->Length();

    for (uint32_t i = 0;  i < len; ++i) {
      TRI_vector_pointer_t edges;
      TRI_voc_cid_t cid;
      TRI_voc_did_t did;
      TRI_voc_rid_t rid;

      TRI_vocbase_col_t const* vertexCollection = 0;
      v8::Handle<v8::Value> errMsg = TRI_ParseDocumentOrDocumentHandle(collection->_vocbase, vertexCollection, did, rid, vertices->Get(i));

      if (! errMsg.IsEmpty()) {
        if (vertexCollection != 0) {
          TRI_ReleaseCollection(vertexCollection);
        }

        continue;
      }

      cid = vertexCollection->_cid;
      TRI_ReleaseCollection(vertexCollection);

      edges = TRI_LookupEdgesSimCollection(sim, direction, cid, did);

      for (size_t j = 0;  j < edges._length;  ++j) {
        if (barrier == 0) {
          barrier = TRI_CreateBarrierElement(&sim->base._barrierList);
        }
        // TODO: barrier might be 0

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
    TRI_voc_rid_t rid;

    TRI_vocbase_col_t const* vertexCollection = 0;
    v8::Handle<v8::Value> errMsg = TRI_ParseDocumentOrDocumentHandle(collection->_vocbase, vertexCollection, did, rid, argv[0]);

    if (! errMsg.IsEmpty()) {
      if (vertexCollection != 0) {
        TRI_ReleaseCollection(vertexCollection);
      }

      collection->_collection->endRead(collection->_collection);

      TRI_ReleaseCollection(collection);
      return scope.Close(v8::ThrowException(errMsg));
    }

    cid = vertexCollection->_cid;
    TRI_ReleaseCollection(vertexCollection);

    edges = TRI_LookupEdgesSimCollection(sim, direction, cid, did);

    for (size_t j = 0;  j < edges._length;  ++j) {
      if (barrier == 0) {
        barrier = TRI_CreateBarrierElement(&sim->base._barrierList);
      }
      // TODO: barrier might be 0

      documents->Set(count++, TRI_WrapShapedJson(collection, (TRI_doc_mptr_t const*) edges._buffer[j], barrier));
    }

    TRI_DestroyVectorPointer(&edges);
  }

  collection->_collection->endRead(collection->_collection);

  // .............................................................................
  // outside a write transaction
  // .............................................................................

  TRI_ReleaseCollection(collection);
  return scope.Close(documents);
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
/// @brief selects all elements
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_AllQuery (v8::Arguments const& argv) {
  v8::HandleScope scope;

  // extract and use the simple collection
  v8::Handle<v8::Object> err;
  TRI_vocbase_col_t const* collection;
  TRI_sim_collection_t* sim = TRI_ExtractAndUseSimpleCollection(argv, collection, &err);

  if (sim == 0) {
    return scope.Close(v8::ThrowException(err));
  }

  // expecting two arguments
  if (argv.Length() != 2) {
    TRI_ReleaseCollection(collection);
    return scope.Close(v8::ThrowException(
                         TRI_CreateErrorObject(TRI_ERROR_BAD_PARAMETER,
                                               "usage: ALL(<skip>, <limit>)")));
  }

  // extract skip and limit
  TRI_voc_ssize_t skip;
  TRI_voc_size_t limit;

  ExtractSkipAndLimit(argv, 0, skip, limit);

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

  if (0 < total && 0 < limit) {
    TRI_barrier_t* barrier = 0;

    void** beg = sim->_primaryIndex._table;
    void** end = sim->_primaryIndex._table + sim->_primaryIndex._nrAlloc;
    void** ptr = beg;

    // skip from the beginning
    if (0 < skip) {
      for (;  ptr < end && 0 < skip;  ++ptr) {
        if (*ptr) {
          TRI_doc_mptr_t const* d = (TRI_doc_mptr_t const*) *ptr;

          if (d->_deletion == 0) {
            --skip;
          }
        }
      }
    }

    // skip from the end
    else if (skip < 0) {
      ptr = end - 1;

      for (;  beg <= ptr;  --ptr) {
        if (*ptr) {
          TRI_doc_mptr_t const* d = (TRI_doc_mptr_t const*) *ptr;

          if (d->_deletion == 0) {
            ++skip;

            if (skip == 0) {
              break;
            }
          }
        }
      }

      if (ptr < beg) {
        ptr = beg;
      }
    }

    // limit
    for (;  ptr < end && count < limit;  ++ptr) {
      if (*ptr) {
        TRI_doc_mptr_t const* d = (TRI_doc_mptr_t const*) *ptr;

        if (d->_deletion == 0) {
          if (barrier == 0) {
            barrier = TRI_CreateBarrierElement(&sim->base._barrierList);
          }
          // TODO: barrier might be 0

          documents->Set(count, TRI_WrapShapedJson(collection, d, barrier));
          ++count;
        }
      }
    }
  }

  collection->_collection->endRead(collection->_collection);

  // .............................................................................
  // outside a write transaction
  // .............................................................................

  result->Set(v8::String::New("total"), v8::Number::New((double) total));
  result->Set(v8::String::New("count"), v8::Number::New(count));

  TRI_ReleaseCollection(collection);
  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief selects elements by example (not using any index)
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_ByExampleQuery (v8::Arguments const& argv) {
  v8::HandleScope scope;

  // extract and use the simple collection
  v8::Handle<v8::Object> err;
  TRI_vocbase_col_t const* collection;
  TRI_sim_collection_t* sim = TRI_ExtractAndUseSimpleCollection(argv, collection, &err);

  if (sim == 0) {
    return scope.Close(v8::ThrowException(err));
  }

  TRI_shaper_t* shaper = sim->base._shaper;

  // expecting example, skip, limit
  if (argv.Length() < 1) {
    TRI_ReleaseCollection(collection);
    return scope.Close(v8::ThrowException(
                         TRI_CreateErrorObject(TRI_ERROR_BAD_PARAMETER,
                                               "usage: BY_EXAMPLE(<example>, <skip>, <limit>)")));
  }

  // extract the example
  if (! argv[0]->IsObject()) {
    TRI_ReleaseCollection(collection);
    return scope.Close(v8::ThrowException(
                         TRI_CreateErrorObject(TRI_ERROR_BAD_PARAMETER,
                                               "<example> must be an object")));
  }

  v8::Handle<v8::Object> example = argv[0]->ToObject();

  // extract skip and limit
  TRI_voc_ssize_t skip;
  TRI_voc_size_t limit;

  ExtractSkipAndLimit(argv, 1, skip, limit);

  // extract sub-documents
  TRI_shape_pid_t* pids;
  TRI_shaped_json_t** values;
  size_t n;

  int res = SetupExampleObject(example, shaper, n, pids, values, &err);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_ReleaseCollection(collection);
    return scope.Close(v8::ThrowException(err));
  }

  // setup result
  v8::Handle<v8::Object> result = v8::Object::New();

  v8::Handle<v8::Array> documents = v8::Array::New();
  result->Set(v8::String::New("documents"), documents);

  // .............................................................................
  // inside a read transaction
  // .............................................................................

  collection->_collection->beginRead(collection->_collection);

  // find documents by example
  TRI_vector_t filtered = TRI_SelectByExample(sim, n,  pids, values);

  // convert to list of shaped jsons
  size_t total = filtered._length;
  size_t count = 0;

  if (0 < total) {
    size_t s;
    size_t e;

    CalculateSkipLimitSlice(filtered._length, skip, limit, s, e);

    if (s < e) {
      // only go in here if something has to be done, otherwise barrier memory might be lost
      TRI_barrier_t* barrier = TRI_CreateBarrierElement(&collection->_collection->_barrierList);
      // TODO: barrier might be 0

      for (size_t j = s;  j < e;  ++j) {
        TRI_doc_mptr_t* mptr = (TRI_doc_mptr_t*) TRI_AtVector(&filtered, j);
        v8::Handle<v8::Value> document = TRI_WrapShapedJson(collection, mptr, barrier);

        documents->Set(count, document);
        ++count;
      }
    }
  }

  TRI_DestroyVector(&filtered);

  collection->_collection->endRead(collection->_collection);

  // .............................................................................
  // outside a write transaction
  // .............................................................................

  result->Set(v8::String::New("total"), v8::Number::New((double) total));
  result->Set(v8::String::New("count"), v8::Number::New(count));

  CleanupExampleObject(shaper, n, pids, values);

  TRI_ReleaseCollection(collection);
  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief selects elements by example using a hash index
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_ByExampleHashIndex (v8::Arguments const& argv) {
  v8::HandleScope scope;

  // extract and use the simple collection
  v8::Handle<v8::Object> err;
  TRI_vocbase_col_t const* collection;
  TRI_sim_collection_t* sim = TRI_ExtractAndUseSimpleCollection(argv, collection, &err);

  if (sim == 0) {
    return scope.Close(v8::ThrowException(err));
  }

  // expecting index, example, skip, and limit
  if (argv.Length() < 2) {
    TRI_ReleaseCollection(collection);
    return scope.Close(v8::ThrowException(
                         TRI_CreateErrorObject(TRI_ERROR_BAD_PARAMETER,
                                               "usage: BY_EXAMPLE_HASH(<index>, <example>, <skip>, <limit>)")));
  }

  // extract the example
  if (! argv[1]->IsObject()) {
    TRI_ReleaseCollection(collection);
    return scope.Close(v8::ThrowException(
                         TRI_CreateErrorObject(TRI_ERROR_BAD_PARAMETER,
                                               "<example> must be an object")));
  }

  v8::Handle<v8::Object> example = argv[1]->ToObject();

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

  collection->_collection->beginRead(collection->_collection);

  // extract the index
  TRI_index_t* idx = TRI_LookupIndexByHandle(sim->base.base._vocbase, collection, argv[0], false, &err);

  if (idx == 0) {
    collection->_collection->endRead(collection->_collection);

    TRI_ReleaseCollection(collection);
    return scope.Close(v8::ThrowException(err));
  }

  if (idx->_type != TRI_IDX_TYPE_HASH_INDEX) {
    collection->_collection->endRead(collection->_collection);

    TRI_ReleaseCollection(collection);
    return scope.Close(v8::ThrowException(TRI_CreateErrorObject(TRI_ERROR_BAD_PARAMETER, "index must be a hash index")));
  }

  TRI_hash_index_t* hashIndex = (TRI_hash_index_t*) idx;

  // convert the example (index is locked by beginRead)
  size_t n;
  TRI_shaped_json_t** values;

  TRI_shaper_t* shaper = sim->base._shaper;
  int res = SetupExampleObjectIndex(hashIndex, example, shaper, n, values, &err);

  if (res != TRI_ERROR_NO_ERROR) {
    collection->_collection->endRead(collection->_collection);

    TRI_ReleaseCollection(collection);
    return scope.Close(v8::ThrowException(err));
  }

  // find the matches
  TRI_hash_index_elements_t* list = TRI_LookupShapedJsonHashIndex(idx, values);

  // convert result
  size_t total = list->_numElements;
  size_t count = 0;

  if (0 < total) {
    size_t s;
    size_t e;

    CalculateSkipLimitSlice(total, skip, limit, s, e);

    if (s < e) {
      TRI_barrier_t* barrier = TRI_CreateBarrierElement(&sim->base._barrierList);
      // TODO: barrier might be 0

      for (size_t i = s;  i < e;  ++i) {
        documents->Set(count, TRI_WrapShapedJson(collection, (TRI_doc_mptr_t const*) list->_elements[i].data, barrier));
        ++count;
      }
    }
  }

  collection->_collection->endRead(collection->_collection);

  // .............................................................................
  // outside a write transaction
  // .............................................................................

  // free data allocated by hash index result
  TRI_FreeResultHashIndex(idx, list);

  result->Set(v8::String::New("total"), v8::Number::New((double) total));
  result->Set(v8::String::New("count"), v8::Number::New(count));

  CleanupExampleObject(shaper, n, 0, values);

  TRI_ReleaseCollection(collection);

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief selects elements by condition using a skiplist index
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_ByConditionSkiplist (v8::Arguments const& argv) {
  std::string signature("BY_CONDITION_SKIPLIST(<index>, <conditions>, <skip>, <limit>)");

  return ExecuteSkiplistQuery(argv, signature, QUERY_CONDITION);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief selects elements by example using a skiplist index
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_ByExampleSkiplist (v8::Arguments const& argv) {
  std::string signature("BY_EXAMPLE_SKIPLIST(<index>, <example>, <skip>, <limit>)");

  return ExecuteSkiplistQuery(argv, signature, QUERY_EXAMPLE);
}


////////////////////////////////////////////////////////////////////////////////
/// @brief selects elements by example using a bitarray index
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_ByExampleBitarray (v8::Arguments const& argv) {
  std::string signature("BY_EXAMPLE_BITARRAY(<index>, <example>, <skip>, <limit>)");

  return ExecuteBitarrayQuery(argv, signature, QUERY_EXAMPLE);
}




////////////////////////////////////////////////////////////////////////////////
/// @brief selects all edges for a set of vertices
///
/// @FUN{@FA{edge-collection}.edges(@FA{vertex})}
///
/// The @FN{edges} operator finds all edges starting from (outbound) or ending
/// in (inbound) @FA{vertex}.
///
/// @FUN{@FA{edge-collection}.edges(@FA{vertices})}
///
/// The @FN{edges} operator finds all edges starting from (outbound) or ending
/// in (inbound) a document from @FA{vertices}, which must a list of documents
/// or document handles.
///
/// @EXAMPLES
///
/// @verbinclude shell-edge-edges
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_EdgesQuery (v8::Arguments const& argv) {
  return EdgesQuery(TRI_EDGE_ANY, argv);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief selects all inbound edges
///
/// @FUN{@FA{edge-collection}.inEdges(@FA{vertex})}
///
/// The @FN{edges} operator finds all edges ending in (inbound) @FA{vertex}.
///
/// @FUN{@FA{edge-collection}.inEdges(@FA{vertices})}
///
/// The @FN{edges} operator finds all edges ending in (inbound) a document from
/// @FA{vertices}, which must a list of documents or document handles.
///
/// @EXAMPLES
///
/// @verbinclude shell-edge-in-edges
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_InEdgesQuery (v8::Arguments const& argv) {
  return EdgesQuery(TRI_EDGE_IN, argv);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief selects points near a given coordinate
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_NearQuery (v8::Arguments const& argv) {
  v8::HandleScope scope;

  // extract and use the simple collection
  v8::Handle<v8::Object> err;
  TRI_vocbase_col_t const* collection;
  TRI_sim_collection_t* sim = TRI_ExtractAndUseSimpleCollection(argv, collection, &err);

  if (sim == 0) {
    return scope.Close(v8::ThrowException(err));
  }

  // expect: NEAR(<index-id>, <latitude>, <longitude>, <limit>)
  if (argv.Length() != 4) {
    TRI_ReleaseCollection(collection);
    return scope.Close(v8::ThrowException(
                         TRI_CreateErrorObject(TRI_ERROR_BAD_PARAMETER,
                                               "usage: NEAR(<index-handle>, <latitude>, <longitude>, <limit>)")));
  }

  // .............................................................................
  // inside a read transaction
  // .............................................................................

  collection->_collection->beginRead(collection->_collection);

  // extract the index
  TRI_index_t* idx = TRI_LookupIndexByHandle(sim->base.base._vocbase, collection, argv[0], false, &err);

  if (idx == 0) {
    collection->_collection->endRead(collection->_collection);

    TRI_ReleaseCollection(collection);
    return scope.Close(v8::ThrowException(err));
  }

  if (idx->_type != TRI_IDX_TYPE_GEO1_INDEX && idx->_type != TRI_IDX_TYPE_GEO2_INDEX) {
    collection->_collection->endRead(collection->_collection);

    TRI_ReleaseCollection(collection);
    return scope.Close(v8::ThrowException(TRI_CreateErrorObject(TRI_ERROR_BAD_PARAMETER, "index must be a geo-index")));
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
    StoreGeoResult(collection, cors, documents, distances);
  }

  collection->_collection->endRead(collection->_collection);

  // .............................................................................
  // outside a write transaction
  // .............................................................................

  TRI_ReleaseCollection(collection);

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief selects all outbound edges
///
/// @FUN{@FA{edge-collection}.outEdges(@FA{vertex})}
///
/// The @FN{edges} operator finds all edges starting from (outbound)
/// @FA{vertices}.
///
/// @FUN{@FA{edge-collection}.outEdges(@FA{vertices})}
///
/// The @FN{edges} operator finds all edges starting from (outbound) a document
/// from @FA{vertices}, which must a list of documents or document handles.
///
/// @EXAMPLES
///
/// @verbinclude shell-edge-out-edges
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_OutEdgesQuery (v8::Arguments const& argv) {
  return EdgesQuery(TRI_EDGE_OUT, argv);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief selects points within a given radius
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_WithinQuery (v8::Arguments const& argv) {
  v8::HandleScope scope;

  // extract and use the simple collection
  v8::Handle<v8::Object> err;
  TRI_vocbase_col_t const* collection;
  TRI_sim_collection_t* sim = TRI_ExtractAndUseSimpleCollection(argv, collection, &err);

  if (sim == 0) {
    return scope.Close(v8::ThrowException(err));
  }

  // expect: WITHIN(<index-handle>, <latitude>, <longitude>, <limit>)
  if (argv.Length() != 4) {
    TRI_ReleaseCollection(collection);
    return scope.Close(v8::ThrowException(
                         TRI_CreateErrorObject(TRI_ERROR_BAD_PARAMETER,
                                               "usage: WITHIN(<index-handle>, <latitude>, <longitude>, <radius>)")));
  }

  // .............................................................................
  // inside a read transaction
  // .............................................................................

  collection->_collection->beginRead(collection->_collection);

  // extract the index
  TRI_index_t* idx = TRI_LookupIndexByHandle(sim->base.base._vocbase, collection, argv[0], false, &err);

  if (idx == 0) {
    collection->_collection->endRead(collection->_collection);

    TRI_ReleaseCollection(collection);
    return scope.Close(v8::ThrowException(err));
  }

  if (idx->_type != TRI_IDX_TYPE_GEO1_INDEX && idx->_type != TRI_IDX_TYPE_GEO2_INDEX) {
    collection->_collection->endRead(collection->_collection);

    TRI_ReleaseCollection(collection);
    return scope.Close(v8::ThrowException(TRI_CreateErrorObject(TRI_ERROR_BAD_PARAMETER, "index must be a geo-index")));
  }

  // extract latitude and longitude
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

  GeoCoordinates* cors = TRI_WithinGeoIndex(idx, latitude, longitude, radius);

  if (cors != 0) {
    StoreGeoResult(collection, cors, documents, distances);
  }

  collection->_collection->endRead(collection->_collection);

  // .............................................................................
  // outside a write transaction
  // .............................................................................

  TRI_ReleaseCollection(collection);
  return scope.Close(result);
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
/// @brief creates the query functions
////////////////////////////////////////////////////////////////////////////////

void TRI_InitV8Queries (v8::Handle<v8::Context> context) {
  v8::HandleScope scope;

  v8::Handle<v8::ObjectTemplate> rt;

  // check the isolate
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  TRI_v8_global_t* v8g = (TRI_v8_global_t*) isolate->GetData();

  if (v8g == 0) {
    v8g = new TRI_v8_global_t;
    isolate->SetData(v8g);
  }

  // .............................................................................
  // local function names
  // .............................................................................

  v8::Handle<v8::String> AllFuncName = v8::Persistent<v8::String>::New(v8::String::New("ALL"));
  v8::Handle<v8::String> ByConditionSkiplistFuncName = v8::Persistent<v8::String>::New(v8::String::New("BY_CONDITION_SKIPLIST"));
  v8::Handle<v8::String> ByExampleFuncName = v8::Persistent<v8::String>::New(v8::String::New("BY_EXAMPLE"));
  v8::Handle<v8::String> ByExampleBitarrayFuncName = v8::Persistent<v8::String>::New(v8::String::New("BY_EXAMPLE_BITARRAY"));
  v8::Handle<v8::String> ByExampleHashFuncName = v8::Persistent<v8::String>::New(v8::String::New("BY_EXAMPLE_HASH"));
  v8::Handle<v8::String> ByExampleSkiplistFuncName = v8::Persistent<v8::String>::New(v8::String::New("BY_EXAMPLE_SKIPLIST"));
  v8::Handle<v8::String> EdgesFuncName = v8::Persistent<v8::String>::New(v8::String::New("edges"));
  v8::Handle<v8::String> InEdgesFuncName = v8::Persistent<v8::String>::New(v8::String::New("inEdges"));
  v8::Handle<v8::String> NearFuncName = v8::Persistent<v8::String>::New(v8::String::New("NEAR"));
  v8::Handle<v8::String> OutEdgesFuncName = v8::Persistent<v8::String>::New(v8::String::New("outEdges"));
  v8::Handle<v8::String> WithinFuncName = v8::Persistent<v8::String>::New(v8::String::New("WITHIN"));

  // .............................................................................
  // generate the TRI_vocbase_col_t template
  // .............................................................................

  rt = v8g->VocbaseColTempl;

  rt->Set(AllFuncName, v8::FunctionTemplate::New(JS_AllQuery));
  rt->Set(ByConditionSkiplistFuncName, v8::FunctionTemplate::New(JS_ByConditionSkiplist));
  rt->Set(ByExampleBitarrayFuncName, v8::FunctionTemplate::New(JS_ByExampleBitarray));
  rt->Set(ByExampleFuncName, v8::FunctionTemplate::New(JS_ByExampleQuery));
  rt->Set(ByExampleHashFuncName, v8::FunctionTemplate::New(JS_ByExampleHashIndex));
  rt->Set(ByExampleSkiplistFuncName, v8::FunctionTemplate::New(JS_ByExampleSkiplist));
  rt->Set(NearFuncName, v8::FunctionTemplate::New(JS_NearQuery));
  rt->Set(WithinFuncName, v8::FunctionTemplate::New(JS_WithinQuery));

  // .............................................................................
  // generate the TRI_vocbase_col_t template for edges
  // .............................................................................

  rt = v8g->EdgesColTempl;

  rt->Set(AllFuncName, v8::FunctionTemplate::New(JS_AllQuery));
  rt->Set(ByConditionSkiplistFuncName, v8::FunctionTemplate::New(JS_ByConditionSkiplist));
  rt->Set(ByExampleBitarrayFuncName, v8::FunctionTemplate::New(JS_ByExampleBitarray));
  rt->Set(ByExampleFuncName, v8::FunctionTemplate::New(JS_ByExampleQuery));
  rt->Set(ByExampleHashFuncName, v8::FunctionTemplate::New(JS_ByExampleHashIndex));
  rt->Set(ByExampleSkiplistFuncName, v8::FunctionTemplate::New(JS_ByExampleSkiplist));
  rt->Set(EdgesFuncName, v8::FunctionTemplate::New(JS_EdgesQuery));
  rt->Set(InEdgesFuncName, v8::FunctionTemplate::New(JS_InEdgesQuery));
  rt->Set(NearFuncName, v8::FunctionTemplate::New(JS_NearQuery));
  rt->Set(OutEdgesFuncName, v8::FunctionTemplate::New(JS_OutEdgesQuery));
  rt->Set(WithinFuncName, v8::FunctionTemplate::New(JS_WithinQuery));
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}\\)"
// End:
