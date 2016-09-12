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
#include "Aql/QueryResultV8.h"
#include "Basics/StaticStrings.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/fasthash.h"
#include "Indexes/GeoIndex.h"
#include "Utils/OperationCursor.h"
#include "Utils/SingleCollectionTransaction.h"
#include "Utils/V8TransactionContext.h"
#include "V8/v8-globals.h"
#include "V8/v8-conv.h"
#include "V8/v8-utils.h"
#include "V8/v8-vpack.h"
#include "V8Server/v8-vocbase.h"
#include "V8Server/v8-vocindex.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/vocbase.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::basics;

////////////////////////////////////////////////////////////////////////////////
/// @brief run an AQL query and return the result as a V8 array 
////////////////////////////////////////////////////////////////////////////////

static aql::QueryResultV8 AqlQuery(v8::Isolate* isolate,
                                   arangodb::LogicalCollection const* col,
                                   std::string const& aql,
                                   std::shared_ptr<VPackBuilder> bindVars) {
  TRI_ASSERT(col != nullptr);

  TRI_GET_GLOBALS();
  arangodb::aql::Query query(true, col->vocbase(), aql.c_str(), aql.size(),
                             bindVars, nullptr, arangodb::aql::PART_MAIN);

  auto queryResult = query.executeV8(
      isolate, static_cast<arangodb::aql::QueryRegistry*>(v8g->_queryRegistry));

  if (queryResult.code != TRI_ERROR_NO_ERROR) {
    if (queryResult.code == TRI_ERROR_REQUEST_CANCELED ||
        queryResult.code == TRI_ERROR_QUERY_KILLED) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_REQUEST_CANCELED);
    }

    THROW_ARANGO_EXCEPTION_MESSAGE(queryResult.code, queryResult.details);
  }

  return queryResult;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up edges for given direction
////////////////////////////////////////////////////////////////////////////////

static void EdgesQuery(TRI_edge_direction_e direction,
                       v8::FunctionCallbackInfo<v8::Value> const& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);
  
  // first and only argument should be a list of document idenfifier
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
    
  auto buildFilter = [](TRI_edge_direction_e direction, std::string const& op) -> std::string {
    switch (direction) {
      case TRI_EDGE_IN:
        return "FILTER doc._to " + op + " @value";
      case TRI_EDGE_OUT:
        return "FILTER doc._from " + op + " @value";
      case TRI_EDGE_ANY:
        return "FILTER doc._from " + op + " @value || doc._to " + op + " @value";
    }

    THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
  };

  arangodb::LogicalCollection const* collection =
      TRI_UnwrapClass<arangodb::LogicalCollection>(args.Holder(),
                                                   TRI_GetVocBaseColType());

  if (collection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }
  
  if (collection->type() != TRI_COL_TYPE_EDGE) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_COLLECTION_TYPE_INVALID);
  }


  auto addOne = [](v8::Isolate* isolate, VPackBuilder* builder, v8::Handle<v8::Value> const val) {
    if (val->IsString() || val->IsStringObject()) {
      builder->add(VPackValue(TRI_ObjectToString(val)));
    } else if (val->IsObject()) {
      v8::Handle<v8::Object> obj = val->ToObject();
      if (obj->Has(TRI_V8_ASCII_STD_STRING(isolate, StaticStrings::IdString))) {
        builder->add(VPackValue(TRI_ObjectToString(obj->Get(TRI_V8_ASCII_STD_STRING(isolate, StaticStrings::IdString)))));
      } else {
        builder->add(VPackValue(""));
      }
    } else {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER, "invalid value type. expecting string or object value");
    }
  };
  
  auto bindVars = std::make_shared<VPackBuilder>();
  bindVars->openObject();
  bindVars->add("@collection", VPackValue(collection->name()));
  bindVars->add(VPackValue("value"));

  if (args[0]->IsArray()) {
    bindVars->openArray();
    v8::Handle<v8::Array> arr = v8::Handle<v8::Array>::Cast(args[0]);
    uint32_t n = arr->Length();
    for (uint32_t i = 0; i < n; ++i) {
      addOne(isolate, bindVars.get(), arr->Get(i));
    }
    bindVars->close();
  }
  else {
    addOne(isolate, bindVars.get(), args[0]);
  }
  bindVars->close();

  std::string filter;
  // argument is a list of vertices
  if (args[0]->IsArray()) {
    filter = buildFilter(direction, "IN");
  }
  else {
    filter = buildFilter(direction, "==");
  }

  std::string const queryString = "FOR doc IN @@collection " + filter + " RETURN doc";
  v8::Handle<v8::Value> result = AqlQuery(isolate, collection, queryString, bindVars).result;
    
  TRI_V8_RETURN(result);
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

  arangodb::LogicalCollection const* collection =
      TRI_UnwrapClass<arangodb::LogicalCollection>(args.Holder(),
                                                   TRI_GetVocBaseColType());

  if (collection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  // extract skip and limit
  int64_t skip = 0;
  uint64_t limit = UINT64_MAX;

  if (!args[0]->IsNull() && !args[0]->IsUndefined()) {
    skip = TRI_ObjectToInt64(args[0]);
  }

  if (!args[1]->IsNull() && !args[1]->IsUndefined()) {
    limit = TRI_ObjectToUInt64(args[1], false);
  }
  
  std::string const collectionName(collection->name());

  std::shared_ptr<V8TransactionContext> transactionContext =
      V8TransactionContext::Create(collection->vocbase(), true);
  SingleCollectionTransaction trx(transactionContext, collection->cid(),
                                  TRI_TRANSACTION_READ);

  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  // We directly read the entire cursor. so batchsize == limit
  std::unique_ptr<OperationCursor> opCursor =
      trx.indexScan(collectionName, Transaction::CursorType::ALL,
                    Transaction::IndexHandle(), {}, skip, limit, limit, false);

  if (opCursor->failed()) {
    TRI_V8_THROW_EXCEPTION(opCursor->code);
  }

  OperationResult countResult = trx.count(collectionName);
  res = trx.finish(countResult.code);

  if (countResult.failed()) {
    TRI_V8_THROW_EXCEPTION(countResult.code);
  }

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  VPackSlice count = countResult.slice();
  TRI_ASSERT(count.isNumber());

  if (!opCursor->hasMore()) {
    // OUT OF MEMORY. initial hasMore should return true even if index is empty
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }
  
  // copy default options
  VPackOptions resultOptions = VPackOptions::Defaults;
  resultOptions.customTypeHandler = transactionContext->orderCustomTypeHandler().get();

  auto batch = std::make_shared<OperationResult>(TRI_ERROR_NO_ERROR);
  opCursor->getMore(batch);
  // We only need this one call, limit == batchsize

  VPackSlice docs = batch->slice();
  TRI_ASSERT(docs.isArray());
  // setup result
  v8::Handle<v8::Object> result = v8::Object::New(isolate);
  auto documents = TRI_VPackToV8(isolate, docs, &resultOptions);
  result->Set(TRI_V8_ASCII_STRING("documents"), documents);
  result->Set(TRI_V8_ASCII_STRING("total"),
              v8::Number::New(isolate, count.getNumericValue<double>()));
  result->Set(TRI_V8_ASCII_STRING("count"),
              v8::Number::New(isolate, static_cast<double>(docs.length())));

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

  arangodb::LogicalCollection const* col = TRI_UnwrapClass<arangodb::LogicalCollection>(
      args.Holder(), TRI_GetVocBaseColType());

  if (col == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  std::string const collectionName(col->name());

  std::shared_ptr<V8TransactionContext> transactionContext =
      V8TransactionContext::Create(col->vocbase(), true);
  SingleCollectionTransaction trx(transactionContext, col->cid(),
                                  TRI_TRANSACTION_READ);

  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }
  
  OperationResult cursor = trx.any(collectionName);

  res = trx.finish(cursor.code);

  if (cursor.failed()) {
    TRI_V8_THROW_EXCEPTION(cursor.code);
  }

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  VPackSlice doc = cursor.slice();
  TRI_ASSERT(doc.isArray());

  if (doc.length() == 0) {
    // The collection is empty.
    TRI_V8_RETURN_NULL();
  }

  // copy default options
  VPackOptions resultOptions = VPackOptions::Defaults;
  resultOptions.customTypeHandler = transactionContext->orderCustomTypeHandler().get();
  TRI_V8_RETURN(TRI_VPackToV8(isolate, doc.at(0), &resultOptions));
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock collectionChecksum
////////////////////////////////////////////////////////////////////////////////

static void JS_ChecksumCollection(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  arangodb::LogicalCollection const* col = TRI_UnwrapClass<arangodb::LogicalCollection>(
      args.Holder(), TRI_GetVocBaseColType());

  if (col == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  TRI_THROW_SHARDING_COLLECTION_NOT_YET_IMPLEMENTED(col);

  bool withRevisions = false;
  bool withData = false;

  if (args.Length() > 0) {
    withRevisions = TRI_ObjectToBoolean(args[0]);
    if (args.Length() > 1) {
      withData = TRI_ObjectToBoolean(args[1]);
    }
  }

  SingleCollectionTransaction trx(V8TransactionContext::Create(col->vocbase(), true),
                                          col->cid(), TRI_TRANSACTION_READ);

  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  trx.orderDitch(col->cid()); // will throw when it fails
  
  // get last tick
  LogicalCollection* collection = trx.documentCollection();
  auto physical = collection->getPhysical();
  TRI_ASSERT(physical != nullptr);
  std::string const revisionId = std::to_string(physical->revision());
  uint64_t hash = 0;
        
  trx.invokeOnAllElements(col->name(), [&hash, &withData, &withRevisions](TRI_doc_mptr_t const* mptr) {
    VPackSlice const slice(mptr->vpack());

    uint64_t localHash = slice.get(StaticStrings::KeyString).hash();

    if (withRevisions) {
      localHash += slice.get(StaticStrings::RevString).hash();
    }

    if (withData) {
      // with data
      uint64_t const n = slice.length() ^ 0xf00ba44ba5;
      uint64_t seed = fasthash64(&n, sizeof(n), 0xdeadf054);

      for (auto const& it : VPackObjectIterator(slice, false)) {
        // loop over all attributes, but exclude _rev, _id and _key
        // _id is different for each collection anyway, _rev is covered by withRevisions, and _key
        // was already handled before
        VPackValueLength keyLength;
        char const* key = it.key.getString(keyLength);
        if (keyLength >= 3 && 
            key[0] == '_' &&
            ((keyLength == 3 && memcmp(key, "_id", 3) == 0) ||
             (keyLength == 4 && (memcmp(key, "_key", 4) == 0 || memcmp(key, "_rev", 4) == 0)))) {
          // exclude attribute
          continue;
        }

        localHash ^= it.key.hash(seed) ^ 0xba5befd00d; 
        localHash += it.value.normalizedHash(seed) ^ 0xd4129f526421; 
      }
    }
    
    hash ^= localHash;
    return true;
  });

  trx.finish(res);

  std::string const hashString = std::to_string(hash);

  v8::Handle<v8::Object> result = v8::Object::New(isolate);
  result->Set(TRI_V8_ASCII_STRING("checksum"), TRI_V8_STD_STRING(hashString));
  result->Set(TRI_V8_ASCII_STRING("revision"), TRI_V8_STD_STRING(revisionId));
    
  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock edgeCollectionEdges
////////////////////////////////////////////////////////////////////////////////

static void JS_EdgesQuery(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  return EdgesQuery(TRI_EDGE_ANY, args);

  // cppcheck-suppress *
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock edgeCollectionInEdges
////////////////////////////////////////////////////////////////////////////////

static void JS_InEdgesQuery(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  return EdgesQuery(TRI_EDGE_IN, args);

  // cppcheck-suppress *
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock edgeCollectionOutEdges
////////////////////////////////////////////////////////////////////////////////

static void JS_OutEdgesQuery(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  return EdgesQuery(TRI_EDGE_OUT, args);

  // cppcheck-suppress *
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock collectionFulltext
////////////////////////////////////////////////////////////////////////////////

static void JS_FulltextQuery(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  arangodb::LogicalCollection const* collection =
      TRI_UnwrapClass<arangodb::LogicalCollection>(args.Holder(),
                                                   TRI_GetVocBaseColType());

  if (collection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }
  
  TRI_THROW_SHARDING_COLLECTION_NOT_YET_IMPLEMENTED(collection);

  // expected: FULLTEXT(<index-handle>, <query>, <limit>)
  if (args.Length() < 2) {
    TRI_V8_THROW_EXCEPTION_USAGE("FULLTEXT(<index-handle>, <query>, <limit>)");
  }

  // extract the index attribute
  std::string attribute;
  {
    SingleCollectionTransaction trx(
        V8TransactionContext::Create(collection->vocbase(), true),
        collection->cid(), TRI_TRANSACTION_READ);

    int res = trx.begin();

    if (res != TRI_ERROR_NO_ERROR) {
      TRI_V8_THROW_EXCEPTION(res);
    }
    auto idx = TRI_LookupIndexByHandle(isolate, trx.resolver(), collection,
                                       args[0], false);

    if (idx == nullptr ||
        idx->type() != arangodb::Index::TRI_IDX_TYPE_FULLTEXT_INDEX) {
      TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_NO_INDEX);
    }

    // get index attribute
    TRI_AttributeNamesToString(idx->fields()[0], attribute);
    trx.finish(TRI_ERROR_NO_ERROR);
  }

  std::string const searchQuery(TRI_ObjectToString(args[1]));

  size_t maxResults = 0;  // 0 means "all results"

  if (args.Length() >= 3 && args[2]->IsNumber()) {
    int64_t value = TRI_ObjectToInt64(args[2]);
    if (value > 0) {
      maxResults = static_cast<size_t>(value);
    }
  }
  
  auto bindVars = std::make_shared<VPackBuilder>();
  bindVars->openObject();
  bindVars->add("@collection", VPackValue(collection->name()));
  bindVars->add("attribute", VPackValue(attribute));
  bindVars->add("query", VPackValue(searchQuery));
  bindVars->add("maxResults", VPackValue(maxResults));
  bindVars->close();

  std::string const queryString = "FOR doc IN FULLTEXT(@@collection, @attribute, @query, @maxResults) RETURN doc";
  aql::QueryResultV8 queryResult = AqlQuery(isolate, collection, queryString, bindVars);

  v8::Handle<v8::Object> result = v8::Object::New(isolate);
  result->Set(TRI_V8_ASCII_STRING("documents"), queryResult.result);

  TRI_V8_RETURN(result);

  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief selects points near a given coordinate
////////////////////////////////////////////////////////////////////////////////

static void JS_NearQuery(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  arangodb::LogicalCollection const* collection =
      TRI_UnwrapClass<arangodb::LogicalCollection>(args.Holder(),
                                                   TRI_GetVocBaseColType());

  if (collection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }
    
  TRI_THROW_SHARDING_COLLECTION_NOT_YET_IMPLEMENTED(collection);

  // expected: NEAR(<index-id>, <latitude>, <longitude>, <limit>)
  if (args.Length() < 4) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "WITHIN(<index-handle>, <latitude>, <longitude>, <limit>, <distance>)");
  }
  
  {
    SingleCollectionTransaction trx(
        V8TransactionContext::Create(collection->vocbase(), true),
        collection->cid(), TRI_TRANSACTION_READ);

    int res = trx.begin();

    if (res != TRI_ERROR_NO_ERROR) {
      TRI_V8_THROW_EXCEPTION(res);
    }

    // extract the index
    auto idx = TRI_LookupIndexByHandle(isolate, trx.resolver(), collection,
                                       args[0], false);

    if (idx == nullptr ||
        (idx->type() != arangodb::Index::TRI_IDX_TYPE_GEO1_INDEX &&
         idx->type() != arangodb::Index::TRI_IDX_TYPE_GEO2_INDEX)) {
      TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_NO_INDEX);
    }
    
    trx.finish(TRI_ERROR_NO_ERROR);
  }
  
  // extract latitude, longitude, limit, distance etc.
  std::string queryString;

  auto bindVars = std::make_shared<VPackBuilder>();
  bindVars->openObject();
  bindVars->add("@collection", VPackValue(collection->name()));
  bindVars->add("latitude", VPackValue(TRI_ObjectToDouble(args[1])));
  bindVars->add("longitude", VPackValue(TRI_ObjectToDouble(args[2])));
  bindVars->add("limit", VPackValue(TRI_ObjectToDouble(args[3])));
  if (args.Length() >= 5 && !args[4]->IsNull() && !args[4]->IsUndefined()) {
    // have a distance attribute
    bindVars->add("distanceAttribute", VPackValue(TRI_ObjectToString(args[4])));
    queryString = "FOR doc IN NEAR(@@collection, @latitude, @longitude, @radius, @distanceAttribute) RETURN doc";
  }
  else {
    queryString = "FOR doc IN NEAR(@@collection, @latitude, @longitude, @radius) RETURN doc";
  }
  bindVars->close();

  aql::QueryResultV8 queryResult = AqlQuery(isolate, collection, queryString, bindVars);

  v8::Handle<v8::Object> result = v8::Object::New(isolate);
  result->Set(TRI_V8_ASCII_STRING("documents"), queryResult.result);

  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief selects points within a given radius
////////////////////////////////////////////////////////////////////////////////

static void JS_WithinQuery(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  arangodb::LogicalCollection const* collection =
      TRI_UnwrapClass<arangodb::LogicalCollection>(args.Holder(),
                                                   TRI_GetVocBaseColType());

  if (collection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }
    
  TRI_THROW_SHARDING_COLLECTION_NOT_YET_IMPLEMENTED(collection);

  // expected: WITHIN(<index-handle>, <latitude>, <longitude>, <radius>)
  if (args.Length() < 4) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "WITHIN(<index-handle>, <latitude>, <longitude>, <radius>, <distance>)");
  }
  
  {
    SingleCollectionTransaction trx(
        V8TransactionContext::Create(collection->vocbase(), true),
        collection->cid(), TRI_TRANSACTION_READ);

    int res = trx.begin();

    if (res != TRI_ERROR_NO_ERROR) {
      TRI_V8_THROW_EXCEPTION(res);
    }

    // extract the index
    auto idx = TRI_LookupIndexByHandle(isolate, trx.resolver(), collection,
                                       args[0], false);

    if (idx == nullptr ||
        (idx->type() != arangodb::Index::TRI_IDX_TYPE_GEO1_INDEX &&
         idx->type() != arangodb::Index::TRI_IDX_TYPE_GEO2_INDEX)) {
      TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_NO_INDEX);
    }
    
    trx.finish(TRI_ERROR_NO_ERROR);
  }
  
  // extract latitude, longitude, radius, distance etc.
  std::string queryString;

  auto bindVars = std::make_shared<VPackBuilder>();
  bindVars->openObject();
  bindVars->add("@collection", VPackValue(collection->name()));
  bindVars->add("latitude", VPackValue(TRI_ObjectToDouble(args[1])));
  bindVars->add("longitude", VPackValue(TRI_ObjectToDouble(args[2])));
  bindVars->add("radius", VPackValue(TRI_ObjectToDouble(args[3])));
  if (args.Length() >= 5 && !args[4]->IsNull() && !args[4]->IsUndefined()) {
    // have a distance attribute
    bindVars->add("distanceAttribute", VPackValue(TRI_ObjectToString(args[4])));
    queryString = "FOR doc IN WITHIN(@@collection, @latitude, @longitude, @radius, @distanceAttribute) RETURN doc";
  }
  else {
    queryString = "FOR doc IN WITHIN(@@collection, @latitude, @longitude, @radius) RETURN doc";
  }
  bindVars->close();

  aql::QueryResultV8 queryResult = AqlQuery(isolate, collection, queryString, bindVars);

  v8::Handle<v8::Object> result = v8::Object::New(isolate);
  result->Set(TRI_V8_ASCII_STRING("documents"), queryResult.result);

  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock collectionLookupByKeys
////////////////////////////////////////////////////////////////////////////////

static void JS_LookupByKeys(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  arangodb::LogicalCollection const* collection =
      TRI_UnwrapClass<arangodb::LogicalCollection>(args.Holder(),
                                                   TRI_GetVocBaseColType());

  if (collection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  if (args.Length() != 1 || !args[0]->IsArray()) {
    TRI_V8_THROW_EXCEPTION_USAGE("documents(<keys>)");
  }

  auto bindVars = std::make_shared<VPackBuilder>();
  bindVars->openObject();
  bindVars->add("@collection", VPackValue(collection->name()));

  VPackBuilder keys;
  int res = TRI_V8ToVPack(isolate, keys, args[0], false);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  VPackBuilder strippedBuilder =
      arangodb::aql::BindParameters::StripCollectionNames(keys.slice(), collection->name().c_str());

  bindVars->add("keys", strippedBuilder.slice());
  bindVars->close();

  std::string const queryString(
      "FOR doc IN @@collection FILTER doc._key IN @keys RETURN doc");

  aql::QueryResultV8 queryResult = AqlQuery(isolate, collection, queryString, bindVars);

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

  arangodb::LogicalCollection const* collection =
      TRI_UnwrapClass<arangodb::LogicalCollection>(args.Holder(),
                                                   TRI_GetVocBaseColType());

  if (collection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  if (args.Length() != 1 || !args[0]->IsArray()) {
    TRI_V8_THROW_EXCEPTION_USAGE("removeByKeys(<keys>)");
  }

  auto bindVars = std::make_shared<VPackBuilder>();
  bindVars->openObject();
  bindVars->add("@collection", VPackValue(collection->name()));
  bindVars->add(VPackValue("keys"));

  int res = TRI_V8ToVPack(isolate, *(bindVars.get()), args[0], false);
  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }
  bindVars->close();

  std::string const queryString(
      "FOR key IN @keys REMOVE key IN @@collection OPTIONS { ignoreErrors: true }");

  aql::QueryResultV8 queryResult = AqlQuery(isolate, collection, queryString, bindVars);

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
  TRI_ASSERT(!VocbaseColTempl.IsEmpty());

  // .............................................................................
  // generate the arangodb::LogicalCollection template
  // .............................................................................

  TRI_AddMethodVocbase(isolate, VocbaseColTempl, TRI_V8_ASCII_STRING("ALL"),
                       JS_AllQuery, true);
  TRI_AddMethodVocbase(isolate, VocbaseColTempl, TRI_V8_ASCII_STRING("ANY"),
                       JS_AnyQuery, true);
  TRI_AddMethodVocbase(isolate, VocbaseColTempl,
                       TRI_V8_ASCII_STRING("checksum"), JS_ChecksumCollection);
  TRI_AddMethodVocbase(isolate, VocbaseColTempl, TRI_V8_ASCII_STRING("EDGES"),
                       JS_EdgesQuery, true);
  TRI_AddMethodVocbase(isolate, VocbaseColTempl,
                       TRI_V8_ASCII_STRING("FULLTEXT"), JS_FulltextQuery, true);
  TRI_AddMethodVocbase(isolate, VocbaseColTempl, TRI_V8_ASCII_STRING("INEDGES"),
                       JS_InEdgesQuery, true);
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
