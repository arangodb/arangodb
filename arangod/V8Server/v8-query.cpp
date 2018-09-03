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
#include "Aql/QueryString.h"
#include "Basics/StaticStrings.h"
#include "Basics/VelocyPackHelper.h"
#include "Indexes/Index.h"
#include "StorageEngine/PhysicalCollection.h"
#include "Transaction/Helpers.h"
#include "Transaction/V8Context.h"
#include "Utils/OperationCursor.h"
#include "Utils/SingleCollectionTransaction.h"
#include "V8/v8-conv.h"
#include "V8/v8-utils.h"
#include "V8/v8-vpack.h"
#include "V8Server/v8-collection.h"
#include "V8Server/v8-externals.h"
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

aql::QueryResultV8 AqlQuery(
    v8::Isolate* isolate, arangodb::LogicalCollection const* col,
    std::string const& aql, std::shared_ptr<VPackBuilder> bindVars) {
  TRI_ASSERT(col != nullptr);

  TRI_GET_GLOBALS();
  // If we execute an AQL query from V8 we need to unset the nolock headers
  arangodb::aql::Query query(
    true,
    col->vocbase(),
    arangodb::aql::QueryString(aql),
    bindVars,
    nullptr,
    arangodb::aql::PART_MAIN
  );

  std::shared_ptr<arangodb::aql::SharedQueryState> ss = query.sharedState();
  ss->setContinueCallback(); 

  aql::QueryResultV8 queryResult;
  while (true) {
    auto state = query.executeV8(isolate,
        static_cast<arangodb::aql::QueryRegistry*>(v8g->_queryRegistry),
        queryResult);
    if (state != aql::ExecutionState::WAITING) {
      break;
    }
    ss->waitForAsyncResponse();
  }

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

    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "invalid edge index direction");
  };

  auto* collection = UnwrapCollection(args.Holder());

  if (!collection) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  if (TRI_COL_TYPE_EDGE != collection->type()) {
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
  auto queryResult = AqlQuery(isolate, collection, queryString, bindVars);

  if (!queryResult.result.IsEmpty()) {
    TRI_V8_RETURN(queryResult.result);
  }

  TRI_V8_RETURN_NULL();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief selects all documents from a collection
////////////////////////////////////////////////////////////////////////////////

static void JS_AllQuery(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  auto* collection = UnwrapCollection(args.Holder());

  if (!collection) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  std::string const collectionName(collection->name());

  auto transactionContext =
    transaction::V8Context::Create(collection->vocbase(), true);
  SingleCollectionTransaction trx(
    transactionContext, *collection, AccessMode::Type::READ
  );
  Result res = trx.begin();

  if (!res.ok()) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  // We directly read the entire cursor. so batchsize == limit
  std::unique_ptr<OperationCursor> opCursor =
      trx.indexScan(collectionName, transaction::Methods::CursorType::ALL);

  if (opCursor->fail()) {
    TRI_V8_THROW_EXCEPTION(opCursor->code);
  }

  if (!opCursor->hasMore()) {
    // OUT OF MEMORY. initial hasMore should return true even if index is empty
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }
  
  // copy default options
  VPackOptions resultOptions = VPackOptions::Defaults;
  resultOptions.customTypeHandler = transactionContext->orderCustomTypeHandler().get();

  VPackBuilder resultBuilder;
  resultBuilder.openArray();
  
  opCursor->allDocuments([&resultBuilder](LocalDocumentId const& token, VPackSlice slice) {
    resultBuilder.add(slice);
  }, 1000);

  resultBuilder.close();
  
  res = trx.finish(Result());
  
  if (res.fail()) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  VPackSlice docs = resultBuilder.slice();
  TRI_ASSERT(docs.isArray());
  // setup result
  v8::Handle<v8::Object> result = v8::Object::New(isolate);
  auto documents = TRI_VPackToV8(isolate, docs, &resultOptions);
  result->Set(TRI_V8_ASCII_STRING(isolate, "documents"), documents);
  result->Set(TRI_V8_ASCII_STRING(isolate, "total"),
              v8::Number::New(isolate, static_cast<double>(docs.length())));
  result->Set(TRI_V8_ASCII_STRING(isolate, "count"),
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

  auto* col = UnwrapCollection(args.Holder());

  if (!col) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  std::string const collectionName(col->name());

  auto transactionContext =
    transaction::V8Context::Create(col->vocbase(), true);
  SingleCollectionTransaction trx(
    transactionContext, *col, AccessMode::Type::READ
  );
  Result res = trx.begin();

  if (!res.ok()) {
    TRI_V8_THROW_EXCEPTION(res);
  }
  
  OperationResult cursor = trx.any(collectionName);

  res = trx.finish(cursor.result);

  if (cursor.fail()) {
    TRI_V8_THROW_EXCEPTION(cursor.result);
  }

  if (!res.ok()) {
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

  auto* col = UnwrapCollection(args.Holder());

  if (!col) {
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

  auto result = col->checksum(withRevisions, withData);

  if (!result.ok()) {
    TRI_V8_THROW_EXCEPTION(result);
  }

  TRI_V8_RETURN(TRI_VPackToV8(isolate, result.builder().slice()));
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock edgeCollectionEdges
////////////////////////////////////////////////////////////////////////////////

static void JS_EdgesQuery(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  EdgesQuery(TRI_EDGE_ANY, args);
  // cppcheck-suppress *
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock edgeCollectionInEdges
////////////////////////////////////////////////////////////////////////////////

static void JS_InEdgesQuery(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  EdgesQuery(TRI_EDGE_IN, args);
  // cppcheck-suppress *
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock edgeCollectionOutEdges
////////////////////////////////////////////////////////////////////////////////

static void JS_OutEdgesQuery(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  EdgesQuery(TRI_EDGE_OUT, args);
  // cppcheck-suppress *
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock collectionLookupByKeys
////////////////////////////////////////////////////////////////////////////////

static void JS_LookupByKeys(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  auto* collection = UnwrapCollection(args.Holder());

  if (!collection) {
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

  bindVars->add(VPackValue("keys"));
  arangodb::aql::BindParameters::stripCollectionNames(
    keys.slice(), collection->name(), *bindVars.get()
  );
  bindVars->close();

  std::string const queryString(
      "FOR doc IN @@collection FILTER doc._key IN @keys RETURN doc");

  auto queryResult = AqlQuery(isolate, collection, queryString, bindVars);

  v8::Handle<v8::Object> result = v8::Object::New(isolate);
  if (!queryResult.result.IsEmpty()) {
    result->Set(TRI_V8_ASCII_STRING(isolate, "documents"), queryResult.result);
  }

  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock collectionRemoveByKeys
////////////////////////////////////////////////////////////////////////////////

static void JS_RemoveByKeys(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  auto* collection = UnwrapCollection(args.Holder());

  if (!collection) {
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

  auto queryResult = AqlQuery(isolate, collection, queryString, bindVars);

  size_t ignored = 0;
  size_t removed = 0;

  if (queryResult.extra) {
    VPackSlice stats = queryResult.extra->slice().get("stats");
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
  result->Set(TRI_V8_ASCII_STRING(isolate, "removed"),
              v8::Number::New(isolate, static_cast<double>(removed)));
  result->Set(TRI_V8_ASCII_STRING(isolate, "ignored"),
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

  TRI_AddMethodVocbase(isolate, VocbaseColTempl, TRI_V8_ASCII_STRING(isolate, "ALL"),
                       JS_AllQuery, true);
  TRI_AddMethodVocbase(isolate, VocbaseColTempl, TRI_V8_ASCII_STRING(isolate, "ANY"),
                       JS_AnyQuery, true);
  TRI_AddMethodVocbase(isolate, VocbaseColTempl,
                       TRI_V8_ASCII_STRING(isolate, "checksum"), JS_ChecksumCollection);
  TRI_AddMethodVocbase(isolate, VocbaseColTempl, TRI_V8_ASCII_STRING(isolate, "EDGES"),
                       JS_EdgesQuery, true);
  TRI_AddMethodVocbase(isolate, VocbaseColTempl, TRI_V8_ASCII_STRING(isolate, "INEDGES"),
                       JS_InEdgesQuery, true);
  TRI_AddMethodVocbase(isolate, VocbaseColTempl,
                       TRI_V8_ASCII_STRING(isolate, "OUTEDGES"), JS_OutEdgesQuery, true);
  TRI_AddMethodVocbase(isolate, VocbaseColTempl,
                       TRI_V8_ASCII_STRING(isolate, "lookupByKeys"), JS_LookupByKeys,
                       true);  // an alias for .documents
  TRI_AddMethodVocbase(isolate, VocbaseColTempl,
                       TRI_V8_ASCII_STRING(isolate, "documents"), JS_LookupByKeys, true);
  TRI_AddMethodVocbase(isolate, VocbaseColTempl,
                       TRI_V8_ASCII_STRING(isolate, "removeByKeys"), JS_RemoveByKeys,
                       true);
}
