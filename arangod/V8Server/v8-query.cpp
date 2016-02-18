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

#include "v8-query.h"
#include "Aql/Query.h"
#include "Basics/StringBuffer.h"
#include "Indexes/FulltextIndex.h"
#include "Indexes/GeoIndex2.h"
#include "Indexes/HashIndex.h"
#include "Indexes/SkiplistIndex.h"
#include "FulltextIndex/fulltext-index.h"
#include "FulltextIndex/fulltext-result.h"
#include "FulltextIndex/fulltext-query.h"
#include "Utils/transactions.h"
#include "V8/v8-globals.h"
#include "V8/v8-conv.h"
#include "V8/v8-utils.h"
#include "V8/v8-vpack.h"
#include "V8Server/v8-shape-conv.h"
#include "V8Server/v8-vocbase.h"
#include "V8Server/v8-vocindex.h"
#include "V8Server/v8-wrapshapedjson.h"
#include "VocBase/document-collection.h"
#include "VocBase/edge-collection.h"
#include "VocBase/ExampleMatcher.h"
#include "VocBase/vocbase.h"
#include "VocBase/VocShaper.h"

using namespace arangodb;
using namespace arangodb::basics;

////////////////////////////////////////////////////////////////////////////////
/// @brief shortcut to wrap a shaped-json object in a read-only transaction
////////////////////////////////////////////////////////////////////////////////

#define WRAP_SHAPED_JSON(...) \
  TRI_WrapShapedJson<SingleCollectionReadOnlyTransaction>(isolate, __VA_ARGS__)

////////////////////////////////////////////////////////////////////////////////
/// @brief geo coordinate container, also containing the distance
////////////////////////////////////////////////////////////////////////////////

typedef struct {
  double _distance;
  void const* _data;
} geo_coordinate_distance_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief query types
////////////////////////////////////////////////////////////////////////////////

typedef enum { QUERY_EXAMPLE, QUERY_CONDITION } query_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief return an empty result set
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> EmptyResult(v8::Isolate* isolate) {
  v8::EscapableHandleScope scope(isolate);

  v8::Handle<v8::Object> result = v8::Object::New(isolate);
  result->Set(TRI_V8_ASCII_STRING("documents"), v8::Array::New(isolate));
  result->Set(TRI_V8_ASCII_STRING("total"), v8::Number::New(isolate, 0));
  result->Set(TRI_V8_ASCII_STRING("count"), v8::Number::New(isolate, 0));

  return scope.Escape<v8::Value>(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts skip and limit
////////////////////////////////////////////////////////////////////////////////

static void ExtractSkipAndLimit(v8::FunctionCallbackInfo<v8::Value> const& args,
                                size_t pos, int64_t& skip, uint64_t& limit) {
  skip = 0;
  limit = UINT64_MAX;

  if (pos < (size_t)args.Length() && !args[(int)pos]->IsNull() &&
      !args[(int)pos]->IsUndefined()) {
    skip = TRI_ObjectToInt64(args[(int)pos]);
  }

  if (pos + 1 < (size_t)args.Length() && !args[(int)pos + 1]->IsNull() &&
      !args[(int)pos + 1]->IsUndefined()) {
    limit = TRI_ObjectToUInt64(args[(int)pos + 1], false);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief calculates slice
////////////////////////////////////////////////////////////////////////////////

static void CalculateSkipLimitSlice(size_t length, int64_t skip, uint64_t limit,
                                    uint64_t& s, uint64_t& e) {
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
        } else {
          e = static_cast<uint64_t>(sum);
        }
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets up the skiplist operator for a skiplist condition query
////////////////////////////////////////////////////////////////////////////////

static TRI_index_operator_t* SetupConditionsSkiplist(
    v8::Isolate* isolate,
    std::vector<std::vector<arangodb::basics::AttributeName>> const& fields,
    VocShaper* shaper, v8::Handle<v8::Object> conditions) {
  size_t numEq = 0;
  size_t lastNonEq = 0;
  std::unique_ptr<TRI_index_operator_t> lastOperator;

  VPackBuilder parameters;
  try {
    VPackArrayBuilder b(&parameters);

    size_t i = 0;
    for (auto const& field : fields) {
      std::string fieldString;
      TRI_AttributeNamesToString(field, fieldString, true);
      v8::Handle<v8::String> key = TRI_V8_STD_STRING(fieldString);

      if (!conditions->HasOwnProperty(key)) {
        break;
      }
      v8::Handle<v8::Value> fieldConditions = conditions->Get(key);

      if (!fieldConditions->IsArray()) {
        // wrong data type for field conditions
        break;
      }

      // iterator over all conditions
      v8::Handle<v8::Array> values =
          v8::Handle<v8::Array>::Cast(fieldConditions);
      for (uint32_t j = 0; j < values->Length(); ++j) {
        v8::Handle<v8::Value> fieldCondition = values->Get(j);

        if (!fieldCondition->IsArray()) {
          // wrong data type for single condition
          return nullptr;
        }

        v8::Handle<v8::Array> condition =
            v8::Handle<v8::Array>::Cast(fieldCondition);

        if (condition->Length() != 2) {
          // wrong number of values in single condition
          return nullptr;
        }

        v8::Handle<v8::Value> op = condition->Get(0);
        v8::Handle<v8::Value> value = condition->Get(1);

        if (!op->IsString() && !op->IsStringObject()) {
          // wrong operator type
          return nullptr;
        }

        VPackBuilder element;
        int res = TRI_V8ToVPack(isolate, element, value, false);
        if (res != TRI_ERROR_NO_ERROR) {
          // Failed to parse or Out of Memory
          return nullptr;
        }

        std::string&& opValue = TRI_ObjectToString(op);
        if (opValue == "==") {
          // equality comparison

          if (lastNonEq > 0) {
            return nullptr;
          }

          parameters.add(element.slice());
          // creation of equality operator is deferred until it is finally
          // needed
          ++numEq;
          break;
        } else {
          if (lastNonEq > 0 && lastNonEq != i) {
            // if we already had a range condition and a previous field, we
            // cannot
            // continue
            // because the skiplist interface does not support such queries
            return nullptr;
          }

          TRI_index_operator_type_e opType;
          if (opValue == ">") {
            opType = TRI_GT_INDEX_OPERATOR;
          } else if (opValue == ">=") {
            opType = TRI_GE_INDEX_OPERATOR;
          } else if (opValue == "<") {
            opType = TRI_LT_INDEX_OPERATOR;
          } else if (opValue == "<=") {
            opType = TRI_LE_INDEX_OPERATOR;
          } else {
            // wrong operator type
            return nullptr;
          }

          lastNonEq = i;

          if (numEq > 0) {
            TRI_ASSERT(!parameters.isClosed());
            // TODO, check if this actually worked
            auto cloned = std::make_shared<VPackBuilder>(parameters);

            if (cloned == nullptr) {
              // Out of memory could not copy Builder
              return nullptr;
            }
            TRI_ASSERT(!cloned->isClosed());
            cloned->close();

            // Assert that the buffer is actualy copied and we can work with
            // both Builders
            TRI_ASSERT(cloned->isClosed());
            TRI_ASSERT(!parameters.isClosed());

            VPackSlice tmp = cloned->slice();
            lastOperator.reset(TRI_CreateIndexOperator(TRI_EQ_INDEX_OPERATOR,
                                                       nullptr, nullptr, cloned,
                                                       shaper, tmp.length()));
            numEq = 0;
          }

          std::unique_ptr<TRI_index_operator_t> current;

          {
            TRI_ASSERT(!parameters.isClosed());
            // TODO, check if this actually worked
            auto cloned = std::make_shared<VPackBuilder>(parameters);

            if (cloned == nullptr) {
              // Out of memory could not copy Builder
              return nullptr;
            }
            TRI_ASSERT(!cloned->isClosed());

            cloned->add(element.slice());

            cloned->close();

            // Assert that the buffer is actualy copied and we can work with
            // both Builders
            TRI_ASSERT(cloned->isClosed());
            TRI_ASSERT(!parameters.isClosed());
            VPackSlice tmp = cloned->slice();

            current.reset(TRI_CreateIndexOperator(
                opType, nullptr, nullptr, cloned, shaper, tmp.length()));

            if (current == nullptr) {
              return nullptr;
            }
          }

          if (lastOperator == nullptr) {
            lastOperator.swap(current);
          } else {
            // merge the current operator with previous operators using logical
            // AND

            std::unique_ptr<TRI_index_operator_t> newOperator(
                TRI_CreateIndexOperator(TRI_AND_INDEX_OPERATOR,
                                        lastOperator.get(), current.get(),
                                        nullptr, shaper, 2));

            if (newOperator == nullptr) {
              // current and lastOperator are still responsible and will free
              return nullptr;
            } else {
              // newOperator is now responsible for current and lastOperator.
              // release them
              current.release();
              lastOperator.release();
              lastOperator.swap(newOperator);
            }
          }
        }
      }
      ++i;
    }
  } catch (...) {
    // Out of Memory
    return nullptr;
  }

  if (numEq > 0) {
    // create equality operator if one is in queue
    TRI_ASSERT(lastOperator == nullptr);
    TRI_ASSERT(lastNonEq == 0);

    auto clonedParams = std::make_shared<VPackBuilder>(parameters);

    if (clonedParams == nullptr) {
      return nullptr;
    }
    VPackSlice tmp = clonedParams->slice();

    lastOperator.reset(TRI_CreateIndexOperator(TRI_EQ_INDEX_OPERATOR, nullptr,
                                               nullptr, clonedParams, shaper,
                                               tmp.length()));
  }
  return lastOperator.release();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets up the skiplist operator for a skiplist example query
///
/// this will set up a JSON container with the example values as a list
/// at the end, one skiplist equality operator is created for the entire list
////////////////////////////////////////////////////////////////////////////////

static TRI_index_operator_t* SetupExampleSkiplist(
    v8::Isolate* isolate,
    std::vector<std::vector<arangodb::basics::AttributeName>> const& fields,
    VocShaper* shaper, v8::Handle<v8::Object> example) {
  auto builder = std::make_shared<VPackBuilder>();
  try {
    VPackArrayBuilder b(builder.get());

    for (auto const& field : fields) {
      std::string fieldString;
      TRI_AttributeNamesToString(field, fieldString, true);
      v8::Handle<v8::String> key = TRI_V8_STD_STRING(fieldString);

      if (!example->HasOwnProperty(key)) {
        break;
      }

      v8::Handle<v8::Value> value = example->Get(key);

      int res = TRI_V8ToVPack(isolate, *builder, value, false);
      if (res != TRI_ERROR_NO_ERROR) {
        return nullptr;
      }
    }

  } catch (...) {
    return nullptr;
  }
  VPackSlice const slice = builder->slice();
  size_t l = static_cast<size_t>(slice.length());
  if (l > 0) {
    // example means equality comparisons only
    return TRI_CreateIndexOperator(TRI_EQ_INDEX_OPERATOR, nullptr, nullptr,
                                   builder, shaper, l);
  }
  return nullptr;

}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets up the example object for a hash index
////////////////////////////////////////////////////////////////////////////////

static int SetupSearchValue(
    std::vector<std::vector<std::pair<TRI_shape_pid_t, bool>>> const& paths,
    v8::Handle<v8::Object> example, VocShaper* shaper,
    TRI_hash_index_search_value_t& result, std::string& errorMessage,
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  v8::Isolate* isolate = args.GetIsolate();

  // extract attribute paths
  size_t const n = paths.size();

  // setup storage
  result.reserve(n);

  // convert
  for (size_t i = 0; i < n; ++i) {
    TRI_shape_pid_t pid = paths[i][0].first;

    TRI_ASSERT(pid != 0);
    char const* name = shaper->attributeNameShapePid(pid);

    if (name == nullptr) {
      errorMessage = "shaper failed";
      return TRI_ERROR_BAD_PARAMETER;
    }

    v8::Handle<v8::String> key = TRI_V8_STRING(name);
    int res;

    if (example->HasOwnProperty(key)) {
      v8::Handle<v8::Value> val = example->Get(key);

      res = TRI_FillShapedJsonV8Object(isolate, val, &result._values[i], shaper,
                                       false);
    } else {
      res = TRI_FillShapedJsonV8Object(isolate, v8::Null(isolate),
                                       &result._values[i], shaper, false);
    }

    if (res != TRI_ERROR_NO_ERROR) {
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

static void ExecuteSkiplistQuery(
    v8::FunctionCallbackInfo<v8::Value> const& args,
    std::string const& signature, query_t type) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  // expecting index, example, skip, and limit
  if (args.Length() < 2) {
    TRI_V8_THROW_EXCEPTION_USAGE(signature.c_str());
  }

  if (!args[1]->IsObject()) {
    std::string msg;

    if (type == QUERY_EXAMPLE) {
      msg = "<example> must be an object";
    } else {
      msg = "<conditions> must be an object";
    }

    TRI_V8_THROW_TYPE_ERROR(msg.c_str());
  }

  bool reverse = false;
  if (args.Length() > 4) {
    reverse = TRI_ObjectToBoolean(args[4]);
  }

  TRI_vocbase_col_t const* col = TRI_UnwrapClass<TRI_vocbase_col_t>(
      args.Holder(), TRI_GetVocBaseColType());

  if (col == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  TRI_THROW_SHARDING_COLLECTION_NOT_YET_IMPLEMENTED(col);

  SingleCollectionReadOnlyTransaction trx(new V8TransactionContext(true),
                                          col->_vocbase, col->_cid);

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
  auto idx =
      TRI_LookupIndexByHandle(isolate, trx.resolver(), col, args[0], false);

  if (idx == nullptr ||
      idx->type() != arangodb::Index::TRI_IDX_TYPE_SKIPLIST_INDEX) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_NO_INDEX);
  }

  TRI_index_operator_t* skiplistOperator;
  v8::Handle<v8::Object> values = args[1]->ToObject();

  if (type == QUERY_EXAMPLE) {
    skiplistOperator =
        SetupExampleSkiplist(isolate, idx->fields(), shaper, values);
  } else {
    skiplistOperator =
        SetupConditionsSkiplist(isolate, idx->fields(), shaper, values);
  }

  if (skiplistOperator == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
  }

  std::unique_ptr<SkiplistIterator> skiplistIterator(
      static_cast<arangodb::SkiplistIndex*>(idx)
          ->lookup(&trx, skiplistOperator, reverse));
  delete skiplistOperator;

  if (!skiplistIterator) {
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
    TRI_V8_THROW_EXCEPTION_MEMORY();
  }

  while (limit > 0) {
    TRI_index_element_t* indexElement = skiplistIterator->next();

    if (indexElement == nullptr) {
      break;
    }

    ++total;

    if (total > skip && count < limit) {
      v8::Handle<v8::Value> doc = WRAP_SHAPED_JSON(
          trx, col->_cid,
          ((TRI_doc_mptr_t const*)indexElement->document())->getDataPtr());

      if (doc.IsEmpty()) {
        error = true;
        break;
      } else {
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

  result->Set(TRI_V8_ASCII_STRING("total"),
              v8::Number::New(isolate, static_cast<double>(total)));
  result->Set(TRI_V8_ASCII_STRING("count"),
              v8::Number::New(isolate, static_cast<double>(count)));

  if (error) {
    TRI_V8_THROW_EXCEPTION_MEMORY();
  }

  return TRI_V8_RETURN(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a geo result
////////////////////////////////////////////////////////////////////////////////

static int StoreGeoResult(v8::Isolate* isolate,
                          SingleCollectionReadOnlyTransaction& trx,
                          TRI_vocbase_col_t const* collection,
                          GeoCoordinates* cors,
                          v8::Handle<v8::Array>& documents,
                          v8::Handle<v8::Array>& distances) {
  if (trx.orderDitch(trx.trxCollection()) == nullptr) {
    GeoIndex_CoordinatesFree(cors);
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  // sort the result
  size_t const n = cors->length;

  if (n == 0) {
    GeoIndex_CoordinatesFree(cors);
    return TRI_ERROR_NO_ERROR;
  }

  geo_coordinate_distance_t* tmp;
  geo_coordinate_distance_t* gtr =
      (tmp = (geo_coordinate_distance_t*)TRI_Allocate(
           TRI_UNKNOWN_MEM_ZONE, sizeof(geo_coordinate_distance_t) * n, false));

  if (gtr == nullptr) {
    GeoIndex_CoordinatesFree(cors);

    return TRI_ERROR_OUT_OF_MEMORY;
  }

  geo_coordinate_distance_t* gnd = tmp + n;

  GeoCoordinate* ptr = cors->coordinates;
  GeoCoordinate* end = cors->coordinates + n;

  double* dtr = cors->distances;

  for (; ptr < end; ++ptr, ++dtr, ++gtr) {
    gtr->_distance = *dtr;
    gtr->_data = ptr->data;
  }

  GeoIndex_CoordinatesFree(cors);

  // sort result by distance
  auto compareSort = [](geo_coordinate_distance_t const& left,
                        geo_coordinate_distance_t const& right) {
    return left._distance < right._distance;
  };
  std::sort(tmp, gnd, compareSort);

  // copy the documents
  bool error = false;
  uint32_t i;
  for (gtr = tmp, i = 0; gtr < gnd; ++gtr, ++i) {
    v8::Handle<v8::Value> doc =
        WRAP_SHAPED_JSON(trx, collection->_cid,
                         ((TRI_doc_mptr_t const*)gtr->_data)->getDataPtr());

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

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up edges for given direction
////////////////////////////////////////////////////////////////////////////////

static void EdgesQuery(TRI_edge_direction_e direction,
                       v8::FunctionCallbackInfo<v8::Value> const& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  TRI_vocbase_col_t const* col = TRI_UnwrapClass<TRI_vocbase_col_t>(
      args.Holder(), TRI_GetVocBaseColType());

  if (col == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  TRI_THROW_SHARDING_COLLECTION_NOT_YET_IMPLEMENTED(col);

  if (col->_type != TRI_COL_TYPE_EDGE) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_COLLECTION_TYPE_INVALID);
  }

  SingleCollectionReadOnlyTransaction trx(new V8TransactionContext(true),
                                          col->_vocbase, col->_cid);

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
      default: { TRI_V8_THROW_EXCEPTION_USAGE("edges(<vertices>)"); }
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

    for (uint32_t i = 0; i < len; ++i) {
      TRI_voc_cid_t cid;
      std::unique_ptr<char[]> key;

      res = TRI_ParseVertex(args, trx.resolver(), cid, key, vertices->Get(i));

      if (res != TRI_ERROR_NO_ERROR) {
        // error is just ignored
        continue;
      }

      std::vector<TRI_doc_mptr_copy_t>&& edges =
          TRI_LookupEdgesDocumentCollection(&trx, document, direction, cid,
                                            key.get());

      for (auto& edge : edges) {
        v8::Handle<v8::Value> doc =
            WRAP_SHAPED_JSON(trx, col->_cid, edge.getDataPtr());

        if (doc.IsEmpty()) {
          // error
          error = true;
          break;
        } else {
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

    std::vector<TRI_doc_mptr_copy_t>&& edges =
        TRI_LookupEdgesDocumentCollection(&trx, document, direction, cid,
                                          key.get());

    trx.finish(res);

    size_t const n = edges.size();
    documents = v8::Array::New(isolate, static_cast<int>(n));

    for (size_t i = 0; i < n; ++i) {
      v8::Handle<v8::Value> doc =
          WRAP_SHAPED_JSON(trx, col->_cid, edges[i].getDataPtr());

      if (doc.IsEmpty()) {
        error = true;
        break;
      } else {
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

////////////////////////////////////////////////////////////////////////////////
/// @brief selects all documents from a collection
////////////////////////////////////////////////////////////////////////////////

static void JS_AllQuery(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  // expecting two arguments
  if (args.Length() != 2) {
    TRI_V8_THROW_EXCEPTION_USAGE("ALL(<skip>, <limit>)");
  }

  TRI_vocbase_col_t const* col;
  col = TRI_UnwrapClass<TRI_vocbase_col_t>(args.Holder(),
                                           TRI_GetVocBaseColType());

  if (col == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  TRI_THROW_SHARDING_COLLECTION_NOT_YET_IMPLEMENTED(col);

  // extract skip and limit
  int64_t skip;
  uint64_t limit;
  ExtractSkipAndLimit(args, 0, skip, limit);

  uint64_t total = 0;
  std::vector<TRI_doc_mptr_copy_t> docs;

  SingleCollectionReadOnlyTransaction trx(new V8TransactionContext(true),
                                          col->_vocbase, col->_cid);

  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  res = trx.read(docs, skip, limit, total);
  TRI_ASSERT(docs.empty() || trx.hasDitch());

  res = trx.finish(res);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  uint64_t const n = static_cast<uint64_t>(docs.size());

  // setup result
  v8::Handle<v8::Object> result = v8::Object::New(isolate);
  v8::Handle<v8::Array> documents =
      v8::Array::New(isolate, static_cast<int>(n));
  // reserve full capacity in one go
  result->Set(TRI_V8_ASCII_STRING("documents"), documents);

  for (uint64_t i = 0; i < n; ++i) {
    v8::Handle<v8::Value> doc =
        WRAP_SHAPED_JSON(trx, col->_cid, docs[i].getDataPtr());

    if (doc.IsEmpty()) {
      TRI_V8_THROW_EXCEPTION_MEMORY();
    } else {
      documents->Set(static_cast<uint32_t>(i), doc);
    }
  }

  result->Set(TRI_V8_ASCII_STRING("total"),
              v8::Number::New(isolate, static_cast<double>(total)));
  result->Set(TRI_V8_ASCII_STRING("count"),
              v8::Number::New(isolate, static_cast<double>(n)));

  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief selects a random document
///
/// @FUN{@FA{collection}.any()}
///
/// The @FN{any} method returns a random document from the collection.  It
/// returns
/// @LIT{null} if the collection is empty.
////////////////////////////////////////////////////////////////////////////////

static void JS_AnyQuery(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  TRI_vocbase_col_t const* col = TRI_UnwrapClass<TRI_vocbase_col_t>(
      args.Holder(), TRI_GetVocBaseColType());

  if (col == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  TRI_doc_mptr_copy_t document;

  SingleCollectionReadOnlyTransaction trx(new V8TransactionContext(true),
                                          col->_vocbase, col->_cid);

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

  v8::Handle<v8::Value> doc =
      WRAP_SHAPED_JSON(trx, col->_cid, document.getDataPtr());

  if (doc.IsEmpty()) {
    TRI_V8_THROW_EXCEPTION_MEMORY();
  }

  TRI_V8_RETURN(doc);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief selects documents by example (not using any index)
////////////////////////////////////////////////////////////////////////////////

static void JS_ByExampleQuery(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  // expecting example, skip, limit
  if (args.Length() < 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("BY_EXAMPLE(<example>, <skip>, <limit>)");
  }

  // extract the example
  if (!args[0]->IsObject()) {
    TRI_V8_THROW_TYPE_ERROR("<example> must be an object");
  }

  TRI_vocbase_col_t const* col = TRI_UnwrapClass<TRI_vocbase_col_t>(
      args.Holder(), TRI_GetVocBaseColType());

  if (col == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  TRI_THROW_SHARDING_COLLECTION_NOT_YET_IMPLEMENTED(col);

  SingleCollectionReadOnlyTransaction trx(new V8TransactionContext(true),
                                          col->_vocbase, col->_cid);

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
  } catch (Exception& e) {
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

  std::vector<TRI_doc_mptr_copy_t> filtered;

  trx.lockRead();

  // find documents by example
  try {
    filtered = TRI_SelectByExample(trx.trxCollection(), *matcher.get());
  } catch (std::exception&) {
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
        v8::Handle<v8::Value> doc =
            WRAP_SHAPED_JSON(trx, col->_cid, filtered[j].getDataPtr());

        if (doc.IsEmpty()) {
          error = true;
          break;
        } else {
          documents->Set(static_cast<uint32_t>(count++), doc);
        }
      }
    }
  }

  result->Set(TRI_V8_ASCII_STRING("total"),
              v8::Number::New(isolate, static_cast<double>(total)));
  result->Set(TRI_V8_ASCII_STRING("count"),
              v8::Number::New(isolate, static_cast<double>(count)));

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

static void ByExampleHashIndexQuery(
    SingleCollectionReadOnlyTransaction& trx,
    TRI_vocbase_col_t const* collection,
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  // expecting index, example, skip, and limit
  if (args.Length() < 2) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "EXAMPLE_HASH(<index>, <example>, <skip>, <limit>)");
  }

  // extract the example
  if (!args[1]->IsObject()) {
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
  auto idx = TRI_LookupIndexByHandle(isolate, trx.resolver(), collection,
                                     args[0], false);

  if (idx == nullptr ||
      idx->type() != arangodb::Index::TRI_IDX_TYPE_HASH_INDEX) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_NO_INDEX);
  }

  auto hashIndex = static_cast<arangodb::HashIndex*>(idx);

  // convert the example (index is locked by lockRead)
  TRI_hash_index_search_value_t searchValue;

  TRI_document_collection_t* document = trx.documentCollection();
  auto shaper = document->getShaper();  // PROTECTED by trx from above
  {
    std::string errorMessage;
    int res = SetupSearchValue(hashIndex->paths(), example, shaper, searchValue,
                               errorMessage, args);

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
  std::vector<TRI_doc_mptr_t*> list;
  static_cast<arangodb::HashIndex*>(idx)->lookup(&trx, &searchValue, list);

  // convert result
  size_t total = list.size();
  size_t count = 0;
  bool error = false;

  if (0 < total) {
    uint64_t s, e;
    CalculateSkipLimitSlice(total, skip, limit, s, e);

    if (s < e) {
      for (uint64_t i = s; i < e; ++i) {
        v8::Handle<v8::Value> doc =
            WRAP_SHAPED_JSON(trx, collection->_cid, list[i]->getDataPtr());

        if (doc.IsEmpty()) {
          error = true;
          break;
        } else {
          documents->Set(static_cast<uint32_t>(count++), doc);
        }
      }
    }
  }

  result->Set(TRI_V8_ASCII_STRING("total"),
              v8::Number::New(isolate, static_cast<double>(total)));
  result->Set(TRI_V8_ASCII_STRING("count"),
              v8::Number::New(isolate, static_cast<double>(count)));

  if (error) {
    TRI_V8_THROW_EXCEPTION_MEMORY();
  }

  TRI_V8_RETURN(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief selects documents by example using a hash index
////////////////////////////////////////////////////////////////////////////////

static void JS_ByExampleHashIndex(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  TRI_vocbase_col_t const* col = TRI_UnwrapClass<TRI_vocbase_col_t>(
      args.Holder(), TRI_GetVocBaseColType());

  if (col == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  TRI_THROW_SHARDING_COLLECTION_NOT_YET_IMPLEMENTED(col);

  SingleCollectionReadOnlyTransaction trx(new V8TransactionContext(true),
                                          col->_vocbase, col->_cid);

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

static void JS_ByConditionSkiplist(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  std::string const signature(
      "BY_CONDITION_SKIPLIST(<index>, <conditions>, <skip>, <limit>, "
      "<reverse>)");

  return ExecuteSkiplistQuery(args, signature, QUERY_CONDITION);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief selects documents by example using a skiplist index
////////////////////////////////////////////////////////////////////////////////

static void JS_ByExampleSkiplist(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  std::string const signature(
      "BY_EXAMPLE_SKIPLIST(<index>, <example>, <skip>, <limit>, <reverse>)");

  return ExecuteSkiplistQuery(args, signature, QUERY_EXAMPLE);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief helper struct used when calculting checksums
////////////////////////////////////////////////////////////////////////////////

struct collection_checksum_t {
  explicit collection_checksum_t(CollectionNameResolver const* resolver)
      : _resolver(resolver), _checksum(0) {}

  CollectionNameResolver const* _resolver;
  TRI_string_buffer_t _buffer;
  uint32_t _checksum;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief callback for checksum calculation, WR = with _rid, WD = with data
////////////////////////////////////////////////////////////////////////////////

template <bool WR, bool WD>
static bool ChecksumCalculator(TRI_doc_mptr_t const* mptr,
                               TRI_document_collection_t* document,
                               void* data) {
  // This callback is only called in TRI_DocumentIteratorDocumentCollection
  // and there we have an ongoing transaction. Therefore all master pointer
  // and data pointer accesses here are safe!
  TRI_df_marker_t const* marker = static_cast<TRI_df_marker_t const*>(
      mptr->getDataPtr());  // PROTECTED by trx in calling function
                            // TRI_DocumentIteratorDocumentCollection
  collection_checksum_t* helper = static_cast<collection_checksum_t*>(data);
  uint32_t localCrc;

  if (marker->_type == TRI_DOC_MARKER_KEY_DOCUMENT ||
      marker->_type == TRI_WAL_MARKER_DOCUMENT) {
    localCrc = TRI_Crc32HashString(
        TRI_EXTRACT_MARKER_KEY(mptr));  // PROTECTED by trx in calling function
    // TRI_DocumentIteratorDocumentCollection
    if (WR) {
      localCrc += TRI_Crc32HashPointer(&mptr->_rid, sizeof(TRI_voc_rid_t));
    }
  } else if (marker->_type == TRI_DOC_MARKER_KEY_EDGE ||
             marker->_type == TRI_WAL_MARKER_EDGE) {
    // must convert _rid, _fromCid, _toCid into strings for portability
    localCrc = TRI_Crc32HashString(
        TRI_EXTRACT_MARKER_KEY(mptr));  // PROTECTED by trx in calling function
    // TRI_DocumentIteratorDocumentCollection
    if (WR) {
      localCrc += TRI_Crc32HashPointer(&mptr->_rid, sizeof(TRI_voc_rid_t));
    }

    if (marker->_type == TRI_DOC_MARKER_KEY_EDGE) {
      TRI_doc_edge_key_marker_t const* e =
          reinterpret_cast<TRI_doc_edge_key_marker_t const*>(marker);
      std::string const extra =
          helper->_resolver->getCollectionNameCluster(e->_toCid) +
          TRI_DOCUMENT_HANDLE_SEPARATOR_CHR +
          std::string(((char*)marker) + e->_offsetToKey) +
          helper->_resolver->getCollectionNameCluster(e->_fromCid) +
          TRI_DOCUMENT_HANDLE_SEPARATOR_CHR +
          std::string(((char*)marker) + e->_offsetFromKey);

      localCrc += TRI_Crc32HashPointer(extra.c_str(), extra.size());
    } else {
      arangodb::wal::edge_marker_t const* e =
          reinterpret_cast<arangodb::wal::edge_marker_t const*>(marker);
      std::string const extra =
          helper->_resolver->getCollectionNameCluster(e->_toCid) +
          TRI_DOCUMENT_HANDLE_SEPARATOR_CHR +
          std::string(((char*)marker) + e->_offsetToKey) +
          helper->_resolver->getCollectionNameCluster(e->_fromCid) +
          TRI_DOCUMENT_HANDLE_SEPARATOR_CHR +
          std::string(((char*)marker) + e->_offsetFromKey);

      localCrc += TRI_Crc32HashPointer(extra.c_str(), extra.size());
    }
  } else {
    return true;
  }

  if (WD) {
    // with data
    void const* d = static_cast<void const*>(marker);

    TRI_shaped_json_t shaped;
    TRI_EXTRACT_SHAPED_JSON_MARKER(shaped, d);

    TRI_StringifyArrayShapedJson(
        document->getShaper(), &helper->_buffer, &shaped,
        false);  // PROTECTED by trx in calling function
                 // TRI_DocumentIteratorDocumentCollection
    localCrc += TRI_Crc32HashPointer(TRI_BeginStringBuffer(&helper->_buffer),
                                     TRI_LengthStringBuffer(&helper->_buffer));
    TRI_ResetStringBuffer(&helper->_buffer);
  }

  helper->_checksum += localCrc;

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock collectionChecksum
////////////////////////////////////////////////////////////////////////////////

static void JS_ChecksumCollection(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (ServerState::instance()->isCoordinator()) {
    // renaming a collection in a cluster is unsupported
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_CLUSTER_UNSUPPORTED);
  }

  TRI_vocbase_col_t const* col = TRI_UnwrapClass<TRI_vocbase_col_t>(
      args.Holder(), TRI_GetVocBaseColType());

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

  SingleCollectionReadOnlyTransaction trx(new V8TransactionContext(true),
                                          col->_vocbase, col->_cid);

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
  std::string const rid = StringUtils::itoa(document->_info.revision());

  if (withData) {
    TRI_InitStringBuffer(&helper._buffer, TRI_CORE_MEM_ZONE);

    if (withRevisions) {
      TRI_DocumentIteratorDocumentCollection(&trx, document, &helper,
                                             &ChecksumCalculator<true, true>);
    } else {
      TRI_DocumentIteratorDocumentCollection(&trx, document, &helper,
                                             &ChecksumCalculator<false, true>);
    }

    TRI_DestroyStringBuffer(&helper._buffer);
  } else {
    if (withRevisions) {
      TRI_DocumentIteratorDocumentCollection(&trx, document, &helper,
                                             &ChecksumCalculator<true, false>);
    } else {
      TRI_DocumentIteratorDocumentCollection(&trx, document, &helper,
                                             &ChecksumCalculator<false, false>);
    }
  }

  trx.finish(res);

  // .............................................................................
  // outside a write transaction
  // .............................................................................

  v8::Handle<v8::Object> result = v8::Object::New(isolate);
  result->Set(TRI_V8_ASCII_STRING("checksum"),
              v8::Number::New(isolate, helper._checksum));
  result->Set(TRI_V8_ASCII_STRING("revision"), TRI_V8_STD_STRING(rid));

  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock edgeCollectionEdges
////////////////////////////////////////////////////////////////////////////////

static void JS_EdgesQuery(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  return EdgesQuery(TRI_EDGE_ANY, args);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock edgeCollectionInEdges
////////////////////////////////////////////////////////////////////////////////

static void JS_InEdgesQuery(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  return EdgesQuery(TRI_EDGE_IN, args);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock edgeCollectionOutEdges
////////////////////////////////////////////////////////////////////////////////

static void JS_OutEdgesQuery(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  return EdgesQuery(TRI_EDGE_OUT, args);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief selects the n first documents in the collection
////////////////////////////////////////////////////////////////////////////////

static void JS_FirstQuery(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() > 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("FIRST(<count>)");
  }

  int64_t count = 1;
  bool returnList = false;

  // if argument is supplied, we'll return a list - otherwise we simply return
  // the first doc
  if (args.Length() == 1) {
    if (!args[0]->IsUndefined()) {
      count = TRI_ObjectToInt64(args[0]);
      returnList = true;
    }
  }

  if (count < 1) {
    TRI_V8_THROW_EXCEPTION_PARAMETER("invalid value for <count>");
  }

  TRI_vocbase_col_t const* col;
  col = TRI_UnwrapClass<TRI_vocbase_col_t>(args.Holder(),
                                           TRI_GetVocBaseColType());

  if (col == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  SingleCollectionReadOnlyTransaction trx(new V8TransactionContext(true),
                                          col->_vocbase, col->_cid);

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
      v8::Handle<v8::Value> doc =
          WRAP_SHAPED_JSON(trx, col->_cid, documents[i].getDataPtr());

      if (doc.IsEmpty()) {
        // error
        TRI_V8_THROW_EXCEPTION_MEMORY();
      }

      result->Set(static_cast<uint32_t>(i), doc);
    }

    TRI_V8_RETURN(result);
  } else {
    if (n == 0) {
      TRI_V8_RETURN_NULL();
    }

    v8::Handle<v8::Value> result =
        WRAP_SHAPED_JSON(trx, col->_cid, documents[0].getDataPtr());

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

static void FulltextQuery(SingleCollectionReadOnlyTransaction& trx,
                          TRI_vocbase_col_t const* collection,
                          v8::FunctionCallbackInfo<v8::Value> const& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  // expect: FULLTEXT(<index-handle>, <query>, <limit>)
  if (args.Length() < 2) {
    TRI_V8_THROW_EXCEPTION_USAGE("FULLTEXT(<index-handle>, <query>, <limit>)");
  }

  // extract the index
  auto idx = TRI_LookupIndexByHandle(isolate, trx.resolver(), collection,
                                     args[0], false);

  if (idx == nullptr ||
      idx->type() != arangodb::Index::TRI_IDX_TYPE_FULLTEXT_INDEX) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_NO_INDEX);
  }

  std::string const&& queryString = TRI_ObjectToString(args[1]);
  bool isSubstringQuery = false;
  size_t maxResults = 0;  // 0 means "all results"

  if (args.Length() >= 3 && args[2]->IsNumber()) {
    int64_t value = TRI_ObjectToInt64(args[2]);
    if (value > 0) {
      maxResults = static_cast<size_t>(value);
    }
  }

  TRI_fulltext_query_t* query =
      TRI_CreateQueryFulltextIndex(TRI_FULLTEXT_SEARCH_MAX_WORDS, maxResults);

  if (!query) {
    TRI_V8_THROW_EXCEPTION_MEMORY();
  }

  int res = TRI_ParseQueryFulltextIndex(query, queryString.c_str(),
                                        &isSubstringQuery);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_FreeQueryFulltextIndex(query);

    TRI_V8_THROW_EXCEPTION(res);
  }

  auto fulltextIndex = static_cast<arangodb::FulltextIndex*>(idx);

  if (isSubstringQuery) {
    TRI_FreeQueryFulltextIndex(query);

    TRI_V8_THROW_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }

  TRI_fulltext_result_t* queryResult =
      TRI_QueryFulltextIndex(fulltextIndex->internals(), query);

  if (!queryResult) {
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
    v8::Handle<v8::Value> doc = WRAP_SHAPED_JSON(
        trx, collection->_cid,
        ((TRI_doc_mptr_t const*)queryResult->_documents[i])->getDataPtr());

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
/// @brief was docuBlock collectionFulltext
////////////////////////////////////////////////////////////////////////////////

static void JS_FulltextQuery(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  TRI_vocbase_col_t const* col = TRI_UnwrapClass<TRI_vocbase_col_t>(
      args.Holder(), TRI_GetVocBaseColType());

  if (col == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  TRI_THROW_SHARDING_COLLECTION_NOT_YET_IMPLEMENTED(col);

  SingleCollectionReadOnlyTransaction trx(new V8TransactionContext(true),
                                          col->_vocbase, col->_cid);

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

static void JS_LastQuery(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() > 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("LAST(<count>)");
  }

  int64_t count = 1;
  bool returnList = false;

  // if argument is supplied, we'll return a list - otherwise we simply return
  // the last doc
  if (args.Length() == 1) {
    if (!args[0]->IsUndefined()) {
      count = TRI_ObjectToInt64(args[0]);
      returnList = true;
    }
  }

  if (count < 1) {
    TRI_V8_THROW_EXCEPTION_PARAMETER("invalid value for <count>");
  }

  TRI_vocbase_col_t const* col = TRI_UnwrapClass<TRI_vocbase_col_t>(
      args.Holder(), TRI_GetVocBaseColType());

  if (col == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  TRI_THROW_SHARDING_COLLECTION_NOT_YET_IMPLEMENTED(col);

  SingleCollectionReadOnlyTransaction trx(new V8TransactionContext(true),
                                          col->_vocbase, col->_cid);

  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  std::vector<TRI_doc_mptr_copy_t> documents;
  res = trx.readPositional(documents, -1, count);
  trx.finish(res);

  uint64_t const n = static_cast<uint64_t>(documents.size());

  if (returnList) {
    v8::Handle<v8::Array> result = v8::Array::New(isolate, static_cast<int>(n));

    for (uint64_t i = 0; i < n; ++i) {
      v8::Handle<v8::Value> doc =
          WRAP_SHAPED_JSON(trx, col->_cid, documents[i].getDataPtr());

      if (doc.IsEmpty()) {
        // error
        TRI_V8_THROW_EXCEPTION_MEMORY();
      }

      result->Set(static_cast<uint32_t>(i), doc);
    }

    TRI_V8_RETURN(result);
  } else {
    if (n == 0) {
      TRI_V8_RETURN_NULL();
    }

    v8::Handle<v8::Value> result =
        WRAP_SHAPED_JSON(trx, col->_cid, documents[0].getDataPtr());

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

static void NearQuery(SingleCollectionReadOnlyTransaction& trx,
                      TRI_vocbase_col_t const* collection,
                      v8::FunctionCallbackInfo<v8::Value> const& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  // expect: NEAR(<index-id>, <latitude>, <longitude>, <limit>)
  if (args.Length() != 4) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "NEAR(<index-handle>, <latitude>, <longitude>, <limit>)");
  }

  // extract the index
  auto idx = TRI_LookupIndexByHandle(isolate, trx.resolver(), collection,
                                     args[0], false);

  if (idx == nullptr ||
      (idx->type() != arangodb::Index::TRI_IDX_TYPE_GEO1_INDEX &&
       idx->type() != arangodb::Index::TRI_IDX_TYPE_GEO2_INDEX)) {
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

  GeoCoordinates* cors = static_cast<arangodb::GeoIndex2*>(idx)
                             ->nearQuery(&trx, latitude, longitude, limit);

  if (cors != nullptr) {
    int res =
        StoreGeoResult(isolate, trx, collection, cors, documents, distances);

    if (res != TRI_ERROR_NO_ERROR) {
      TRI_V8_THROW_EXCEPTION(res);
    }
  }

  TRI_V8_RETURN(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief selects points near a given coordinate
////////////////////////////////////////////////////////////////////////////////

static void JS_NearQuery(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  TRI_vocbase_col_t const* col = TRI_UnwrapClass<TRI_vocbase_col_t>(
      args.Holder(), TRI_GetVocBaseColType());

  if (col == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  TRI_THROW_SHARDING_COLLECTION_NOT_YET_IMPLEMENTED(col);

  SingleCollectionReadOnlyTransaction trx(new V8TransactionContext(true),
                                          col->_vocbase, col->_cid);

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

static void WithinQuery(SingleCollectionReadOnlyTransaction& trx,
                        TRI_vocbase_col_t const* collection,
                        v8::FunctionCallbackInfo<v8::Value> const& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  // expect: WITHIN(<index-handle>, <latitude>, <longitude>, <radius>)
  if (args.Length() != 4) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "WITHIN(<index-handle>, <latitude>, <longitude>, <radius>)");
  }

  // extract the index
  auto idx = TRI_LookupIndexByHandle(isolate, trx.resolver(), collection,
                                     args[0], false);

  if (idx == nullptr ||
      (idx->type() != arangodb::Index::TRI_IDX_TYPE_GEO1_INDEX &&
       idx->type() != arangodb::Index::TRI_IDX_TYPE_GEO2_INDEX)) {
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

  GeoCoordinates* cors = static_cast<arangodb::GeoIndex2*>(idx)
                             ->withinQuery(&trx, latitude, longitude, radius);

  if (cors != nullptr) {
    int res =
        StoreGeoResult(isolate, trx, collection, cors, documents, distances);

    if (res != TRI_ERROR_NO_ERROR) {
      TRI_V8_THROW_EXCEPTION(res);
    }
  }

  TRI_V8_RETURN(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief selects points within a given radius
////////////////////////////////////////////////////////////////////////////////

static void JS_WithinQuery(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  TRI_vocbase_col_t const* col = TRI_UnwrapClass<TRI_vocbase_col_t>(
      args.Holder(), TRI_GetVocBaseColType());

  if (col == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  TRI_THROW_SHARDING_COLLECTION_NOT_YET_IMPLEMENTED(col);

  SingleCollectionReadOnlyTransaction trx(new V8TransactionContext(true),
                                          col->_vocbase, col->_cid);

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
/// @brief was docuBlock collectionLookupByKeys
////////////////////////////////////////////////////////////////////////////////

static void JS_LookupByKeys(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  TRI_vocbase_col_t const* col = TRI_UnwrapClass<TRI_vocbase_col_t>(
      args.Holder(), TRI_GetVocBaseColType());

  if (col == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  if (args.Length() != 1 || !args[0]->IsArray()) {
    TRI_V8_THROW_EXCEPTION_USAGE("documents(<keys>)");
  }

  arangodb::basics::Json bindVars(arangodb::basics::Json::Object, 2);
  bindVars("@collection", arangodb::basics::Json(std::string(col->_name)));
  bindVars("keys", arangodb::basics::Json(TRI_UNKNOWN_MEM_ZONE,
                                          TRI_ObjectToJson(isolate, args[0])));

  std::string const collectionName(col->name());

  arangodb::aql::BindParameters::StripCollectionNames(
      TRI_LookupObjectJson(bindVars.json(), "keys"), collectionName.c_str());

  std::string const aql(
      "FOR doc IN @@collection FILTER doc._key IN @keys RETURN doc");

  TRI_GET_GLOBALS();
  arangodb::aql::Query query(v8g->_applicationV8, true, col->_vocbase,
                             aql.c_str(), aql.size(), bindVars.steal(), nullptr,
                             arangodb::aql::PART_MAIN);

  auto queryResult = query.executeV8(
      isolate, static_cast<arangodb::aql::QueryRegistry*>(v8g->_queryRegistry));

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
/// @brief was docuBlock collectionRemoveByKeys
////////////////////////////////////////////////////////////////////////////////

static void JS_RemoveByKeys(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  TRI_vocbase_col_t const* col = TRI_UnwrapClass<TRI_vocbase_col_t>(
      args.Holder(), TRI_GetVocBaseColType());

  if (col == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  if (args.Length() != 1 || !args[0]->IsArray()) {
    TRI_V8_THROW_EXCEPTION_USAGE("removeByKeys(<keys>)");
  }

  arangodb::basics::Json bindVars(arangodb::basics::Json::Object, 2);
  bindVars("@collection", arangodb::basics::Json(std::string(col->_name)));
  bindVars("keys", arangodb::basics::Json(TRI_UNKNOWN_MEM_ZONE,
                                          TRI_ObjectToJson(isolate, args[0])));

  std::string const aql(
      "FOR key IN @keys REMOVE key IN @@collection OPTIONS { ignoreErrors: "
      "true }");

  TRI_GET_GLOBALS();
  arangodb::aql::Query query(v8g->_applicationV8, true, col->_vocbase,
                             aql.c_str(), aql.size(), bindVars.steal(), nullptr,
                             arangodb::aql::PART_MAIN);

  auto queryResult = query.executeV8(
      isolate, static_cast<arangodb::aql::QueryRegistry*>(v8g->_queryRegistry));

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
    VPackSlice stats = queryResult.stats->slice();

    if (!stats.isNone()) {
      TRI_ASSERT(stats.isObject());
      VPackSlice found = stats.get("writesIgnored");
      if (found.isNumber()) {
        ignored = found.getNumericValue<size_t>();
      }

      found = stats.get("writesExecuted");
      if (found.isNumber()) {
        removed = found.getNumericValue<size_t>();
      }
    }
  }

  v8::Handle<v8::Object> result = v8::Object::New(isolate);
  result->Set(TRI_V8_ASCII_STRING("removed"),
              v8::Number::New(isolate, static_cast<double>(removed)));
  result->Set(TRI_V8_ASCII_STRING("ignored"),
              v8::Number::New(isolate, static_cast<double>(ignored)));

  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates the query functions
////////////////////////////////////////////////////////////////////////////////

void TRI_InitV8Queries(v8::Isolate* isolate, v8::Handle<v8::Context> context) {
  v8::HandleScope scope(isolate);

  TRI_GET_GLOBALS();
  TRI_ASSERT(v8g != nullptr);
  TRI_GET_GLOBAL(VocbaseColTempl, v8::ObjectTemplate);

  // .............................................................................
  // generate the TRI_vocbase_col_t template
  // .............................................................................

  TRI_AddMethodVocbase(isolate, VocbaseColTempl, TRI_V8_ASCII_STRING("ALL"),
                       JS_AllQuery, true);
  TRI_AddMethodVocbase(isolate, VocbaseColTempl, TRI_V8_ASCII_STRING("ANY"),
                       JS_AnyQuery, true);
  TRI_AddMethodVocbase(isolate, VocbaseColTempl,
                       TRI_V8_ASCII_STRING("BY_CONDITION_SKIPLIST"),
                       JS_ByConditionSkiplist, true);
  TRI_AddMethodVocbase(isolate, VocbaseColTempl,
                       TRI_V8_ASCII_STRING("BY_EXAMPLE"), JS_ByExampleQuery,
                       true);
  TRI_AddMethodVocbase(isolate, VocbaseColTempl,
                       TRI_V8_ASCII_STRING("BY_EXAMPLE_HASH"),
                       JS_ByExampleHashIndex, true);
  TRI_AddMethodVocbase(isolate, VocbaseColTempl,
                       TRI_V8_ASCII_STRING("BY_EXAMPLE_SKIPLIST"),
                       JS_ByExampleSkiplist, true);
  TRI_AddMethodVocbase(isolate, VocbaseColTempl,
                       TRI_V8_ASCII_STRING("checksum"), JS_ChecksumCollection);
  TRI_AddMethodVocbase(isolate, VocbaseColTempl, TRI_V8_ASCII_STRING("EDGES"),
                       JS_EdgesQuery, true);
  TRI_AddMethodVocbase(isolate, VocbaseColTempl, TRI_V8_ASCII_STRING("FIRST"),
                       JS_FirstQuery, true);
  TRI_AddMethodVocbase(isolate, VocbaseColTempl,
                       TRI_V8_ASCII_STRING("FULLTEXT"), JS_FulltextQuery, true);
  TRI_AddMethodVocbase(isolate, VocbaseColTempl, TRI_V8_ASCII_STRING("INEDGES"),
                       JS_InEdgesQuery, true);
  TRI_AddMethodVocbase(isolate, VocbaseColTempl, TRI_V8_ASCII_STRING("LAST"),
                       JS_LastQuery, true);
  TRI_AddMethodVocbase(isolate, VocbaseColTempl, TRI_V8_ASCII_STRING("NEAR"),
                       JS_NearQuery, true);
  TRI_AddMethodVocbase(isolate, VocbaseColTempl,
                       TRI_V8_ASCII_STRING("OUTEDGES"), JS_OutEdgesQuery, true);
  TRI_AddMethodVocbase(isolate, VocbaseColTempl, TRI_V8_ASCII_STRING("WITHIN"),
                       JS_WithinQuery, true);
  TRI_AddMethodVocbase(isolate, VocbaseColTempl,
                       TRI_V8_ASCII_STRING("lookupByKeys"), JS_LookupByKeys,
                       true);  // an alias for .documents
  TRI_AddMethodVocbase(isolate, VocbaseColTempl,
                       TRI_V8_ASCII_STRING("documents"), JS_LookupByKeys, true);
  TRI_AddMethodVocbase(isolate, VocbaseColTempl,
                       TRI_V8_ASCII_STRING("removeByKeys"), JS_RemoveByKeys,
                       true);
}
