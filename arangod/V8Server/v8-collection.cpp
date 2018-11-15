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

#include "v8-collection.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Aql/Query.h"
#include "Basics/FileUtils.h"
#include "Basics/LocalTaskQueue.h"
#include "Basics/ReadLocker.h"
#include "Basics/Result.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringBuffer.h"
#include "Basics/Utf8Helper.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/WriteLocker.h"
#include "Basics/conversions.h"
#include "Cluster/ClusterInfo.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "Indexes/Index.h"
#include "Cluster/FollowerInfo.h"
#include "Pregel/AggregatorHandler.h"
#include "Pregel/Conductor.h"
#include "Pregel/PregelFeature.h"
#include "Pregel/Worker.h"
#include "RestServer/DatabaseFeature.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/PhysicalCollection.h"
#include "StorageEngine/StorageEngine.h"
#include "Transaction/Hints.h"
#include "Transaction/V8Context.h"
#include "Utils/CollectionNameResolver.h"
#include "Utils/ExecContext.h"
#include "Utils/OperationOptions.h"
#include "Utils/OperationResult.h"
#include "Utils/SingleCollectionTransaction.h"
#include "V8/v8-conv.h"
#include "V8/v8-utils.h"
#include "V8/v8-vpack.h"
#include "V8Server/v8-externals.h"
#include "V8Server/v8-vocbase.h"
#include "V8Server/v8-vocbaseprivate.h"
#include "V8Server/v8-vocindex.h"
#include "VocBase/KeyGenerator.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/Methods/Collections.h"

#include <velocypack/Builder.h>
#include <velocypack/HexDump.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

namespace {

////////////////////////////////////////////////////////////////////////////////
/// @brief retrieves a collection from a V8 argument
////////////////////////////////////////////////////////////////////////////////
std::shared_ptr<arangodb::LogicalCollection> GetCollectionFromArgument(
    TRI_vocbase_t& vocbase,
    v8::Handle<v8::Value> const val
) {
  if (arangodb::ServerState::instance()->isCoordinator()) {
    auto* ci = arangodb::ClusterInfo::instance();

    return ci
        ? ci->getCollectionNT(vocbase.name(), TRI_ObjectToString(val)) : nullptr;
  }

  // number
  if (val->IsNumber() || val->IsNumberObject()) {
    uint64_t cid = TRI_ObjectToUInt64(val, true);

    return vocbase.lookupCollection(cid);
  }

  return vocbase.lookupCollection(TRI_ObjectToString(val));
}

}

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

////////////////////////////////////////////////////////////////////////////////
/// @brief extract a boolean flag from the arguments
/// must specify the argument index starting from 1
////////////////////////////////////////////////////////////////////////////////

static inline bool ExtractBooleanArgument(
    v8::FunctionCallbackInfo<v8::Value> const& args, int index) {
  TRI_ASSERT(index > 0);

  return (args.Length() >= index && TRI_ObjectToBoolean(args[index - 1]));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extract a string argument from the arguments
/// must specify the argument index starting from 1
////////////////////////////////////////////////////////////////////////////////

static inline void ExtractStringArgument(
    v8::FunctionCallbackInfo<v8::Value> const& args, int index,
    std::string& ret) {
  TRI_ASSERT(index > 0);

  if (args.Length() >= index && args[index - 1]->IsString()) {
    ret = TRI_ObjectToString(args[index - 1]);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts a string value referencing a documents _id
///        If value is a string it is simply returned.
///        If value is an object and has a string _id attribute, this is
///        returned
///        Otherwise the empty string is returned
////////////////////////////////////////////////////////////////////////////////

static std::string ExtractIdString(v8::Isolate* isolate,
                                   v8::Handle<v8::Value> const val) {
  if (val->IsString()) {
    return TRI_ObjectToString(val);
  }

  if (val->IsObject()) {
    TRI_GET_GLOBALS();
    v8::Handle<v8::Object> obj = val->ToObject();
    TRI_GET_GLOBAL_STRING(_IdKey);
    if (obj->HasRealNamedProperty(_IdKey)) {
      v8::Handle<v8::Value> idVal = obj->Get(_IdKey);
      if (idVal->IsString()) {
        return TRI_ObjectToString(idVal);
      }
    }
  }
  std::string empty;
  return empty;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parse document or document handle from a v8 value (string | object)
////////////////////////////////////////////////////////////////////////////////

static int ParseDocumentOrDocumentHandle(
    v8::Isolate* isolate,
    CollectionNameResolver const* resolver,
    std::shared_ptr<arangodb::LogicalCollection>& collection,
    std::string& collectionName,
    VPackBuilder& builder,
    bool includeRev,
    v8::Handle<v8::Value> const val
) {
  v8::HandleScope scope(isolate);

  // try to extract the collection name, key, and revision from the object
  // passed
  if (!ExtractDocumentHandle(isolate, val, collectionName, builder, includeRev)) {
    return TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD;
  }

  if (collectionName.empty()) {
    // only a document key without collection name was passed
    if (collection == nullptr) {
      // we do not know the collection
      return TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD;
    }
    // we use the current collection's name
    collectionName = resolver->getCollectionNameCluster(collection->id());
  } else {
    // we read a collection name from the document id
    // check cross-collection requests
    if (collection != nullptr) {
      if (!EqualCollection(resolver, collectionName, collection.get())) {
        return TRI_ERROR_ARANGO_CROSS_COLLECTION_REQUEST;
      }
    }
  }

  TRI_ASSERT(!collectionName.empty());

  if (collection == nullptr) {
    // no collection object was passed, now check the user-supplied collection
    // name
    collection = resolver->getCollection(collectionName);

    if (collection == nullptr) {
      // collection not found
      return TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND;
    }
  }
  TRI_ASSERT(collection != nullptr);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief V8ToVPack without _key and _rev, builder must be open with an
/// object and is left open at the end
////////////////////////////////////////////////////////////////////////////////

static int V8ToVPackNoKeyRevId(v8::Isolate* isolate,
                               VPackBuilder& builder,
                               v8::Local<v8::Value> const obj) {

  TRI_ASSERT(obj->IsObject() && !obj->IsArray());
  auto o = v8::Local<v8::Object>::Cast(obj);
  v8::Handle<v8::Array> names = o->GetOwnPropertyNames();
  uint32_t const n = names->Length();
  for (uint32_t i = 0; i < n; ++i) {
    v8::Handle<v8::Value> key = names->Get(i);
    TRI_Utf8ValueNFC str(key);
    if (*str == nullptr) {
      return TRI_ERROR_OUT_OF_MEMORY;
    }
    if (strcmp(*str, "_key") != 0 &&
        strcmp(*str, "_rev") != 0 &&
        strcmp(*str, "_id") != 0) {
      builder.add(VPackValue(*str));
      int res = TRI_V8ToVPack(isolate, builder, o->Get(key), false);
      if (res != TRI_ERROR_NO_ERROR) {
        return res;
      }
    }
  }
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get all cluster collections cloned, caller needs to cleanupb
////////////////////////////////////////////////////////////////////////////////

std::vector<std::shared_ptr<LogicalCollection>> GetCollections(
    TRI_vocbase_t& vocbase
) {
  if (arangodb::ServerState::instance()->isCoordinator()) {
    auto* ci = ClusterInfo::instance();

    return ci
      ? ci->getCollections(vocbase.name())
      : std::vector<std::shared_ptr<LogicalCollection>>()
      ;
  }

  return vocbase.collections(false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get all cluster collection names
////////////////////////////////////////////////////////////////////////////////

static std::vector<std::string> GetCollectionNamesCluster(
    TRI_vocbase_t* vocbase) {
  std::vector<std::string> result;

  std::vector<std::shared_ptr<LogicalCollection>> const collections =
      ClusterInfo::instance()->getCollections(vocbase->name());

  for (auto& collection : collections) {
    std::string const& name = collection->name();
    result.emplace_back(name);
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a document and returns whether it exists
////////////////////////////////////////////////////////////////////////////////

static void ExistsVocbaseVPack(
    bool useCollection, v8::FunctionCallbackInfo<v8::Value> const& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  // first and only argument should be a document idenfifier
  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("exists(<document-handle> or <document-key> )");
  }

  TRI_vocbase_t* vocbase;
  arangodb::LogicalCollection* col = nullptr;

  if (useCollection) {
    // called as db.collection.exists()
    col = UnwrapCollection(args.Holder());

    if (!col) {
      TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
    }

    vocbase = &(col->vocbase());
  } else {
    // called as db._exists()
    vocbase = &GetContextVocBase(isolate);
  }

  auto transactionContext =
    std::make_shared<transaction::V8Context>(*vocbase, true);
  VPackBuilder builder;
  std::shared_ptr<arangodb::LogicalCollection> collection(col, [](arangodb::LogicalCollection*)->void {});
  std::string collectionName;
  Result res;

  {
    VPackObjectBuilder guard(&builder);

    res = ParseDocumentOrDocumentHandle(
      isolate,
      &(transactionContext->resolver()),
      collection,
      collectionName,
      builder,
      true,
      args[0]
    );
  }

  if (!res.ok()) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  TRI_ASSERT(collection);
  TRI_ASSERT(!collectionName.empty());
  VPackSlice search = builder.slice();
  TRI_ASSERT(search.isObject());

  SingleCollectionTransaction trx(transactionContext, collectionName, AccessMode::Type::READ);
  trx.addHint(transaction::Hints::Hint::SINGLE_OPERATION);

  res = trx.begin();

  if (!res.ok()) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  OperationOptions options;
  options.silent = false;
  options.ignoreRevs = false;
  OperationResult opResult = trx.document(collectionName, search, options);

  res = trx.finish(opResult.result);

  if (opResult.fail()) {
    if (opResult.is(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND)) {
      TRI_V8_RETURN_FALSE();
    }
    TRI_V8_THROW_EXCEPTION(opResult.result);
  }

  if (!res.ok()) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  v8::Handle<v8::Value> result = TRI_VPackToV8(isolate, opResult.slice(),
      transactionContext->getVPackOptions());

  TRI_V8_RETURN(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up (a) document(s) and returns it/them, collection method
////////////////////////////////////////////////////////////////////////////////

static void DocumentVocbaseCol(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  // first and only argument should be a document handle or key or an object
  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("document(<document-handle> or <document-key> or <object> or <array>)");
  }

  OperationOptions options;
  options.ignoreRevs = false;

  // Find collection and vocbase
  auto* col = UnwrapCollection(args.Holder());

  if (!col) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  auto& collectionName = col->name();
  VPackBuilder searchBuilder;

  auto workOnOneDocument = [&](v8::Local<v8::Value> const searchValue, bool isBabies) {
    std::string collName;
    if (!ExtractDocumentHandle(isolate, searchValue, collName, searchBuilder,
                               true)) {
      if (!isBabies) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD);
      }
    }
    if (!collName.empty() && collName != collectionName) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_CROSS_COLLECTION_REQUEST);
    }
  };

  if (!args[0]->IsArray()) {
    VPackObjectBuilder guard(&searchBuilder);
    workOnOneDocument(args[0], false);
  } else {
    VPackArrayBuilder guard(&searchBuilder);
    auto searchVals = v8::Local<v8::Array>::Cast(args[0]);
    for (uint32_t i = 0; i < searchVals->Length(); ++i) {
      VPackObjectBuilder guard(&searchBuilder);
      workOnOneDocument(searchVals->Get(i), true);
    }
  }

  VPackSlice search = searchBuilder.slice();
  auto transactionContext =
    std::make_shared<transaction::V8Context>(col->vocbase(), true);
  SingleCollectionTransaction trx(transactionContext, collectionName,
                                  AccessMode::Type::READ);

  if (!args[0]->IsArray()) {
    trx.addHint(transaction::Hints::Hint::SINGLE_OPERATION);
  }

  Result res = trx.begin();
  if (!res.ok()) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  OperationResult opResult = trx.document(collectionName, search, options);

  res = trx.finish(opResult.result);

  if (opResult.fail()) {
    TRI_V8_THROW_EXCEPTION(opResult.result);
  }

  if (!res.ok()) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  v8::Handle<v8::Value> result = TRI_VPackToV8(isolate, opResult.slice(),
      transactionContext->getVPackOptions());

  TRI_V8_RETURN(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a document and returns it, database method
////////////////////////////////////////////////////////////////////////////////

static void DocumentVocbase(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  // first and only argument should be a document idenfifier
  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("document(<document-handle>)");
  }

  auto& vocbase = GetContextVocBase(isolate);

  if (vocbase.isDropped()) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  auto transactionContext =
    std::make_shared<transaction::V8Context>(vocbase, true);
  VPackBuilder builder;
  std::shared_ptr<arangodb::LogicalCollection> collection;
  std::string collectionName;

  {
    VPackObjectBuilder guard(&builder);
    int res = ParseDocumentOrDocumentHandle(
      isolate,
      &(transactionContext->resolver()),
      collection,
      collectionName,
      builder,
      true,
      args[0]
    );

    if (res != TRI_ERROR_NO_ERROR) {
      TRI_V8_THROW_EXCEPTION(res);
    }
  }

  TRI_ASSERT(collection);
  TRI_ASSERT(!collectionName.empty());

  VPackSlice search = builder.slice();
  TRI_ASSERT(search.isObject());

  OperationOptions options;
  options.ignoreRevs = false;

  SingleCollectionTransaction trx(transactionContext, collectionName,
                                  AccessMode::Type::READ);
  trx.addHint(transaction::Hints::Hint::SINGLE_OPERATION);

  Result res = trx.begin();

  if (!res.ok()) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  OperationResult opResult = trx.document(collectionName, search, options);

  res = trx.finish(opResult.result);

  if (opResult.fail()) {
    TRI_V8_THROW_EXCEPTION(opResult.result);
  }

  if (!res.ok()) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  v8::Handle<v8::Value> result = TRI_VPackToV8(isolate, opResult.slice(),
      transactionContext->getVPackOptions());

  TRI_V8_RETURN(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes (a) document(s), collection method
////////////////////////////////////////////////////////////////////////////////

static void RemoveVocbaseCol(v8::FunctionCallbackInfo<v8::Value> const& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);
  OperationOptions options;
  options.ignoreRevs = false;

  // check the arguments
  uint32_t const argLength = args.Length();

  TRI_GET_GLOBALS();

  if (argLength < 1 || argLength > 3) {
    TRI_V8_THROW_EXCEPTION_USAGE("remove(<document>, "
        "{overwrite: booleanValue, waitForSync: booleanValue, returnOld: "
        "booleanValue, silent:booleanValue})");
  }

  if (argLength > 1) {
    if (args[1]->IsObject()) {
      v8::Handle<v8::Object> optionsObject = args[1].As<v8::Object>();
      TRI_GET_GLOBAL_STRING(OverwriteKey);
      if (optionsObject->Has(OverwriteKey)) {
        options.ignoreRevs = TRI_ObjectToBoolean(optionsObject->Get(OverwriteKey));
      }
      TRI_GET_GLOBAL_STRING(WaitForSyncKey);
      if (optionsObject->Has(WaitForSyncKey)) {
        options.waitForSync =
            TRI_ObjectToBoolean(optionsObject->Get(WaitForSyncKey));
      }
      TRI_GET_GLOBAL_STRING(ReturnOldKey);
      if (optionsObject->Has(ReturnOldKey)) {
        options.returnOld = TRI_ObjectToBoolean(optionsObject->Get(ReturnOldKey));
      }
      TRI_GET_GLOBAL_STRING(SilentKey);
      if (optionsObject->Has(SilentKey)) {
        options.silent = TRI_ObjectToBoolean(optionsObject->Get(SilentKey));
      }
      TRI_GET_GLOBAL_STRING(IsSynchronousReplicationKey);
      if (optionsObject->Has(IsSynchronousReplicationKey)) {
        options.isSynchronousReplicationFrom
          = TRI_ObjectToString(optionsObject->Get(IsSynchronousReplicationKey));
      }
    } else {  // old variant remove(<document>, <overwrite>, <waitForSync>)
      options.ignoreRevs = TRI_ObjectToBoolean(args[1]);
      if (argLength > 2) {
        options.waitForSync = TRI_ObjectToBoolean(args[2]);
      }
    }
  }

  // Find collection and vocbase
  auto* col = UnwrapCollection(args.Holder());

  if (!col) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  auto& collectionName = col->name();
  VPackBuilder searchBuilder;

  auto workOnOneDocument = [&](v8::Local<v8::Value> const searchValue, bool isBabies) {
    std::string collName;
    if (!ExtractDocumentHandle(isolate, searchValue, collName, searchBuilder,
                               true)) {
      if (!isBabies) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD);
      }
      return;
    }
    if (!collName.empty() && collName != collectionName) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_CROSS_COLLECTION_REQUEST);
    }
  };

  if (!args[0]->IsArray()) {
    VPackObjectBuilder guard(&searchBuilder);
    workOnOneDocument(args[0], false);
  } else {
    VPackArrayBuilder guard(&searchBuilder);
    auto searchVals = v8::Local<v8::Array>::Cast(args[0]);
    for (uint32_t i = 0; i < searchVals->Length(); ++i) {
      VPackObjectBuilder guard(&searchBuilder);
      workOnOneDocument(searchVals->Get(i), true);
    }
  }

  VPackSlice toRemove = searchBuilder.slice();
  auto transactionContext =
    std::make_shared<transaction::V8Context>(col->vocbase(), true);
  SingleCollectionTransaction trx(transactionContext, collectionName, AccessMode::Type::WRITE);

  if (!args[0]->IsArray()) {
    trx.addHint(transaction::Hints::Hint::SINGLE_OPERATION);
  }

  Result res = trx.begin();
  if (!res.ok()) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  OperationResult result = trx.remove(collectionName, toRemove, options);

  res = trx.finish(result.result);

  if (!res.ok()) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  if (options.silent) {
    // no return value
    TRI_V8_RETURN_TRUE();
  }

  v8::Handle<v8::Value> finalResult = TRI_VPackToV8(isolate, result.slice(),
      transactionContext->getVPackOptions());

  TRI_V8_RETURN(finalResult);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes a document, database method
////////////////////////////////////////////////////////////////////////////////

static void RemoveVocbase(v8::FunctionCallbackInfo<v8::Value> const& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);
  OperationOptions options;
  options.ignoreRevs = false;

  // check the arguments
  uint32_t const argLength = args.Length();

  TRI_GET_GLOBALS();

  if (argLength < 1 || argLength > 3) {
    TRI_V8_THROW_EXCEPTION_USAGE("remove(<document>, <options>)");
  }

  if (argLength > 1) {
    if (args[1]->IsObject()) {
      v8::Handle<v8::Object> optionsObject = args[1].As<v8::Object>();
      TRI_GET_GLOBAL_STRING(OverwriteKey);
      if (optionsObject->Has(OverwriteKey)) {
        options.ignoreRevs = TRI_ObjectToBoolean(optionsObject->Get(OverwriteKey));
      }
      TRI_GET_GLOBAL_STRING(WaitForSyncKey);
      if (optionsObject->Has(WaitForSyncKey)) {
        options.waitForSync =
            TRI_ObjectToBoolean(optionsObject->Get(WaitForSyncKey));
      }
      TRI_GET_GLOBAL_STRING(ReturnOldKey);
      if (optionsObject->Has(ReturnOldKey)) {
        options.returnOld = TRI_ObjectToBoolean(optionsObject->Get(ReturnOldKey));
      }
      TRI_GET_GLOBAL_STRING(SilentKey);
      if (optionsObject->Has(SilentKey)) {
        options.silent = TRI_ObjectToBoolean(optionsObject->Get(SilentKey));
      }
    } else {  // old variant replace(<document>, <data>, <overwrite>,
              // <waitForSync>)
      options.ignoreRevs = TRI_ObjectToBoolean(args[1]);
      if (argLength > 2) {
        options.waitForSync = TRI_ObjectToBoolean(args[2]);
      }
    }
  }

  auto& vocbase = GetContextVocBase(isolate);

  if (vocbase.isDropped()) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  auto transactionContext =
    std::make_shared<transaction::V8Context>(vocbase, true);
  VPackBuilder builder;
  std::shared_ptr<arangodb::LogicalCollection> collection;
  std::string collectionName;

  {
    VPackObjectBuilder guard(&builder);
    int res = ParseDocumentOrDocumentHandle(
      isolate,
      &(transactionContext->resolver()),
      collection,
      collectionName,
      builder,
      !options.ignoreRevs,
      args[0]
    );

    if (res != TRI_ERROR_NO_ERROR) {
      TRI_V8_THROW_EXCEPTION(res);
    }
  }

  TRI_ASSERT(collection);
  TRI_ASSERT(!collectionName.empty());

  VPackSlice toRemove = builder.slice();
  TRI_ASSERT(toRemove.isObject());

  SingleCollectionTransaction trx(transactionContext, collectionName,
                                  AccessMode::Type::WRITE);
  trx.addHint(transaction::Hints::Hint::SINGLE_OPERATION);

  Result res = trx.begin();

  if (!res.ok()) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  OperationResult result = trx.remove(collectionName, toRemove, options);

  res = trx.finish(result.result);

  if (result.fail()) {
    TRI_V8_THROW_EXCEPTION(result.result);
  }

  if (!res.ok()) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  if (options.silent) {
    // no return value
    TRI_V8_RETURN_TRUE();
  }

  v8::Handle<v8::Value> finalResult = TRI_VPackToV8(
      isolate, result.slice(), transactionContext->getVPackOptions());

  TRI_V8_RETURN(finalResult);
}

// db.<collection>.document
static void JS_DocumentVocbaseCol(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  DocumentVocbaseCol(args);
  TRI_V8_TRY_CATCH_END
}

// db.<collection>.binaryDocument
static void JS_BinaryDocumentVocbaseCol(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  // first and only argument should be a document handle or key
  if (args.Length() != 2) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "binaryDocument(<document-handle> or <document-key>, <filename>)");
  }

  OperationOptions options;
  options.ignoreRevs = false;

  // Find collection and vocbase
  auto* col = UnwrapCollection(args.Holder());

  if (!col) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  VPackBuilder searchBuilder;
  v8::Local<v8::Value> const searchValue = args[0];
  auto& collectionName = col->name();

  {
    VPackObjectBuilder guard(&searchBuilder);

    std::string collName;
    if (!ExtractDocumentHandle(isolate, searchValue, collName, searchBuilder,
                               true)) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD);
    }

    if (!collName.empty() && collName != collectionName) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_CROSS_COLLECTION_REQUEST);
    }
  }

  VPackSlice search = searchBuilder.slice();
  auto transactionContext =
    std::make_shared<transaction::V8Context>(col->vocbase(), true);
  SingleCollectionTransaction trx(transactionContext, collectionName,
                                  AccessMode::Type::READ);

  trx.addHint(transaction::Hints::Hint::SINGLE_OPERATION);

  Result res = trx.begin();

  if (!res.ok()) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  OperationResult opResult = trx.document(collectionName, search, options);

  res = trx.finish(opResult.result);

  if (opResult.fail()) {
    TRI_V8_THROW_EXCEPTION(opResult.result);
  }

  if (!res.ok()) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  std::string filename = TRI_ObjectToString(args[1]);
  auto builder = std::make_shared<VPackBuilder>();

  {
    VPackObjectBuilder meta(builder.get());

    for (auto const& it : VPackObjectIterator(opResult.slice().resolveExternals())) {
      std::string key = it.key.copyString();

      if (key == StaticStrings::AttachmentString) {
        char const* att;
        velocypack::ValueLength length;

        try {
          att = it.value.getString(length);
        } catch (...) {
          TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID,
                                         "'_attachment' must be a string");
        }

        std::string attachment =
            StringUtils::decodeBase64(std::string(att, length));

        try {
          FileUtils::spit(filename, attachment);
        } catch (...) {
          TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_errno(), TRI_last_error());
        }
      } else {
        builder->add(key, it.value);
      }
    }
  }

  v8::Handle<v8::Value> result = TRI_VPackToV8(
      isolate, builder->slice(), transactionContext->getVPackOptions());

  TRI_V8_RETURN(result);

  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock collectionDrop
////////////////////////////////////////////////////////////////////////////////

static void JS_DropVocbaseCol(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);
  auto& vocbase = GetContextVocBase(isolate);

  if (vocbase.isDangling()) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  auto* collection = UnwrapCollection(args.Holder());

  if (!collection) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  PREVENT_EMBEDDED_TRANSACTION();

  bool allowDropSystem = false;
  double timeout = -1.0;  // forever, unless specified otherwise

  if (args.Length() > 0) {
    // options
    if (args[0]->IsObject()) {
      TRI_GET_GLOBALS();
      v8::Handle<v8::Object> optionsObject = args[0].As<v8::Object>();
      TRI_GET_GLOBAL_STRING(IsSystemKey);
      if (optionsObject->Has(IsSystemKey)) {
        allowDropSystem = TRI_ObjectToBoolean(optionsObject->Get(IsSystemKey));
      }
      TRI_GET_GLOBAL_STRING(TimeoutKey);
      if (optionsObject->Has(TimeoutKey)) {
        timeout = TRI_ObjectToDouble(optionsObject->Get(TimeoutKey));
      }
    } else {
      allowDropSystem = TRI_ObjectToBoolean(args[0]);
    }
  }

  auto res = methods::Collections::drop(
    &vocbase, collection, allowDropSystem, timeout
  );

  if (res.fail()) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock documentsCollectionExists
////////////////////////////////////////////////////////////////////////////////

static void JS_ExistsVocbaseVPack(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  return ExistsVocbaseVPack(true, args);

  // cppcheck-suppress style
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock collectionFigures
////////////////////////////////////////////////////////////////////////////////

static void JS_FiguresVocbaseCol(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  auto* collection = UnwrapCollection(args.Holder());

  if (!collection) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  SingleCollectionTransaction trx(
    transaction::V8Context::Create(collection->vocbase(), true),
    *collection,
    AccessMode::Type::READ
  );
  Result res = trx.begin();

  if (!res.ok()) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  auto builder = collection->figures();

  trx.finish(TRI_ERROR_NO_ERROR);

  v8::Handle<v8::Value> result = TRI_VPackToV8(isolate, builder->slice());

  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock collectionLoad
////////////////////////////////////////////////////////////////////////////////

static void JS_LoadVocbaseCol(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  auto& vocbase = GetContextVocBase(isolate);

  if (vocbase.isDropped()) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  auto* collection = UnwrapCollection(args.Holder());

  if (!collection) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  auto res = methods::Collections::load(vocbase, collection);

  if (res.fail()) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the name of a collection
////////////////////////////////////////////////////////////////////////////////

static void JS_NameVocbaseCol(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  auto* collection = UnwrapCollection(args.Holder());

  if (!collection) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  auto& collectionName = collection->name();

  if (collectionName.empty()) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
  }
  v8::Handle<v8::Value> result = TRI_V8_STD_STRING(isolate, collectionName);
  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the path of a collection
////////////////////////////////////////////////////////////////////////////////

static void JS_PathVocbaseCol(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  auto* collection = UnwrapCollection(args.Holder());

  if (!collection) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  std::string path(collection->getPhysical()->path());
  v8::Handle<v8::Value> result = TRI_V8_STD_STRING(isolate, path);

  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the collection's cluster plan id
////////////////////////////////////////////////////////////////////////////////

static void JS_PlanIdVocbaseCol(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  auto* collection = UnwrapCollection(args.Holder());

  if (!collection) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  if (ServerState::instance()->isCoordinator()) {
    TRI_V8_RETURN(TRI_V8UInt64String<TRI_voc_cid_t>(isolate, collection->id()));
  }

  TRI_V8_RETURN(TRI_V8UInt64String<TRI_voc_cid_t>(isolate, collection->planId()));
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock collectionProperties
////////////////////////////////////////////////////////////////////////////////

static void JS_PropertiesVocbaseCol(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  auto* consoleColl = UnwrapCollection(args.Holder());

  if (!consoleColl) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  bool const isModification = (args.Length() != 0);

  if (isModification) {
    v8::Handle<v8::Value> par = args[0];

    if (par->IsObject()) {
      VPackBuilder builder;
      {
        int res = TRI_V8ToVPack(isolate, builder, args[0], false);
        if (res != TRI_ERROR_NO_ERROR) {
          TRI_V8_THROW_EXCEPTION(res);
        }
      }

      TRI_ASSERT(builder.isClosed());

      auto res = methods::Collections::updateProperties(
        *consoleColl, builder.slice(), false // always a full-update
      );

      if (res.fail() && ServerState::instance()->isCoordinator()) {
        TRI_V8_THROW_EXCEPTION(res);
      }

      // TODO Review
      // TODO API compatibility, for now we ignore if persisting fails...
    }
  }

  // in the cluster the collection object might contain outdated
  // properties, which will break tests. We need an extra lookup
  VPackBuilder builder;

  methods::Collections::lookup(
    &(consoleColl->vocbase()),
    consoleColl->name(),
    [&](std::shared_ptr<LogicalCollection> const& coll)->void {
      TRI_ASSERT(coll);

    VPackObjectBuilder object(&builder, true);
    methods::Collections::Context ctxt(coll->vocbase(), *coll);
    Result res = methods::Collections::properties(ctxt, builder);

    if (res.fail()) {
      TRI_V8_THROW_EXCEPTION(res);
    }
  });

  // return the current parameter set
  TRI_V8_RETURN(TRI_VPackToV8(isolate, builder.slice())->ToObject());
  TRI_V8_TRY_CATCH_END
}

static void JS_RemoveVocbaseCol(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  RemoveVocbaseCol(args);
  // cppcheck-suppress style
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock collectionRename
////////////////////////////////////////////////////////////////////////////////

static void JS_RenameVocbaseCol(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (args.Length() < 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("rename(<name>)");
  }

  std::string const name = TRI_ObjectToString(args[0]);

  // second parameter "override" is to override renaming restrictions, e.g.
  // renaming from a system collection name to a non-system collection name and
  // vice versa. this parameter is not publicly exposed but used internally
  bool doOverride = false;
  if (args.Length() > 1) {
    doOverride = TRI_ObjectToBoolean(args[1]);
  }

  PREVENT_EMBEDDED_TRANSACTION();

  auto* collection = UnwrapCollection(args.Holder());

  if (!collection) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  auto res = methods::Collections::rename(*collection, name, doOverride);

  if (res.fail()) {
    TRI_V8_THROW_EXCEPTION(res);
  }
  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief option parsing for replace and update methods
////////////////////////////////////////////////////////////////////////////////

static void parseReplaceAndUpdateOptions(
    v8::Isolate* isolate,
    v8::FunctionCallbackInfo<v8::Value> const& args,
    OperationOptions& options,
    TRI_voc_document_operation_e operation) {

  TRI_GET_GLOBALS();
  TRI_ASSERT(args.Length() > 2);
  if (args[2]->IsObject()) {
    v8::Handle<v8::Object> optionsObject = args[2].As<v8::Object>();
    TRI_GET_GLOBAL_STRING(OverwriteKey);
    if (optionsObject->Has(OverwriteKey)) {
      options.ignoreRevs = TRI_ObjectToBoolean(optionsObject->Get(OverwriteKey));
    }
    TRI_GET_GLOBAL_STRING(WaitForSyncKey);
    if (optionsObject->Has(WaitForSyncKey)) {
      options.waitForSync =
          TRI_ObjectToBoolean(optionsObject->Get(WaitForSyncKey));
    }
    TRI_GET_GLOBAL_STRING(SilentKey);
    if (optionsObject->Has(SilentKey)) {
      options.silent = TRI_ObjectToBoolean(optionsObject->Get(SilentKey));
    }
    TRI_GET_GLOBAL_STRING(ReturnNewKey);
    if (optionsObject->Has(ReturnNewKey)) {
      options.returnNew = TRI_ObjectToBoolean(optionsObject->Get(ReturnNewKey));
    }
    TRI_GET_GLOBAL_STRING(ReturnOldKey);
    if (optionsObject->Has(ReturnOldKey)) {
      options.returnOld = TRI_ObjectToBoolean(optionsObject->Get(ReturnOldKey));
    }
    TRI_GET_GLOBAL_STRING(IsRestoreKey);
    if (optionsObject->Has(IsRestoreKey)) {
      options.isRestore = TRI_ObjectToBoolean(optionsObject->Get(IsRestoreKey));
    }
    TRI_GET_GLOBAL_STRING(IsSynchronousReplicationKey);
    if (optionsObject->Has(IsSynchronousReplicationKey)) {
      options.isSynchronousReplicationFrom
        = TRI_ObjectToString(optionsObject->Get(IsSynchronousReplicationKey));
    }
    if (operation == TRI_VOC_DOCUMENT_OPERATION_UPDATE) {
      // intentionally not called for TRI_VOC_DOCUMENT_OPERATION_REPLACE
      TRI_GET_GLOBAL_STRING(KeepNullKey);
      if (optionsObject->Has(KeepNullKey)) {
        options.keepNull = TRI_ObjectToBoolean(optionsObject->Get(KeepNullKey));
      }
      TRI_GET_GLOBAL_STRING(MergeObjectsKey);
      if (optionsObject->Has(MergeObjectsKey)) {
        options.mergeObjects =
            TRI_ObjectToBoolean(optionsObject->Get(MergeObjectsKey));
      }
    }
  } else {
    // old variants
    //   replace(<document>, <data>, <overwrite>, <waitForSync>)
    // and
    //   update(<document>, <data>, <overwrite>, <keepNull>, <waitForSync>
    options.ignoreRevs = TRI_ObjectToBoolean(args[2]);
    if (args.Length() > 3) {
      if (operation == TRI_VOC_DOCUMENT_OPERATION_REPLACE) {
        options.waitForSync = TRI_ObjectToBoolean(args[3]);
      } else {  // UPDATE
        options.keepNull = TRI_ObjectToBoolean(args[3]);
        if (args.Length() > 4) {
          options.waitForSync = TRI_ObjectToBoolean(args[4]);
        }
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ModifyVocbaseCol
////////////////////////////////////////////////////////////////////////////////

static void ModifyVocbaseCol(TRI_voc_document_operation_e operation,
                             v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  // check the arguments
  uint32_t const argLength = args.Length();

  if (argLength < 2 ||
      argLength > (operation == TRI_VOC_DOCUMENT_OPERATION_REPLACE ? 4UL : 5UL)) {
    if (operation == TRI_VOC_DOCUMENT_OPERATION_REPLACE) {
      TRI_V8_THROW_EXCEPTION_USAGE(
          "replace(<document(s)>, <data>, {overwrite: booleanValue,"
          " waitForSync: booleanValue, returnNew: booleanValue,"
          " returnOld: booleanValue, silent: booleanValue})");
    } else {   // UPDATE
      TRI_V8_THROW_EXCEPTION_USAGE(
          "update(<document>, <data>, {overwrite: booleanValue, keepNull: "
          "booleanValue, mergeObjects: booleanValue, waitForSync: "
          "booleanValue, returnNew: booleanValue, returnOld: booleanValue,"
          " silent: booleanValue})");
    }
  }

  // we're only accepting "real" object documents or arrays of such
  if (!args[1]->IsObject()) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
  }
  if (args[0]->IsArray() ^ args[1]->IsArray()) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
  }
  if (args[0]->IsArray()) {  // then both are arrays, check equal length
    auto a = v8::Local<v8::Array>::Cast(args[0]);
    auto b = v8::Local<v8::Array>::Cast(args[1]);
    if (a->Length() != b->Length()) {
      TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
    }
  }

  OperationOptions options;
  options.ignoreRevs = false;
  if (args.Length() > 2) {
    parseReplaceAndUpdateOptions(isolate, args, options, operation);
  }
  if (options.isRestore) {
    options.ignoreRevs = true;
  }

  // Find collection and vocbase
  auto* col = UnwrapCollection(args.Holder());

  if (!col) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  auto& collectionName = col->name();
  VPackBuilder updateBuilder;
  auto workOnOneSearchVal = [&](v8::Local<v8::Value> const searchVal, bool isBabies) {
    std::string collName;

    if (!ExtractDocumentHandle(isolate, searchVal, collName,
                               updateBuilder, !options.isRestore)) {
      // If this is no restore, then we must extract the _rev from the
      // search value. If options.isRestore is set, the _rev value must
      // be taken from the new value, see below in workOnOneDocument!
      if (!isBabies) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD);
      } else {
        return;
      }
    }

    if (!collName.empty() && collName != collectionName) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_CROSS_COLLECTION_REQUEST);
    }
  };

  auto workOnOneDocument = [&](v8::Local<v8::Value> const newVal) {
    if (!newVal->IsObject() || newVal->IsArray()) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
    }

    int res = V8ToVPackNoKeyRevId(isolate, updateBuilder, newVal);

    if (res != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(res);
    }

    if (options.isRestore) {
      // In this case we have to extract the _rev entry from newVal:
      TRI_GET_GLOBALS();
      v8::Handle<v8::Object> obj = newVal->ToObject();
      TRI_GET_GLOBAL_STRING(_RevKey);
      if (!obj->HasRealNamedProperty(_RevKey)) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_REV_BAD);
      }
      v8::Handle<v8::Value> revVal = obj->Get(_RevKey);
      if (!revVal->IsString()) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_REV_BAD);
      }
      v8::String::Utf8Value str(revVal);
      if (*str == nullptr) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_REV_BAD);
      }
      updateBuilder.add(StaticStrings::RevString, VPackValue(*str));
    }
  };

  if (!args[0]->IsArray()) {
    // we deal with the single document case:
    VPackObjectBuilder guard(&updateBuilder);
    workOnOneDocument(args[1]);
    workOnOneSearchVal(args[0], false);
  } else { // finally, the array case, note that we already know that the two
           // arrays have equal length!
    TRI_ASSERT(args[0]->IsArray() && args[1]->IsArray());
    VPackArrayBuilder guard(&updateBuilder);
    auto searchVals = v8::Local<v8::Array>::Cast(args[0]);
    auto documents = v8::Local<v8::Array>::Cast(args[1]);
    for (uint32_t i = 0; i < searchVals->Length(); ++i) {
      v8::Local<v8::Value> const newVal = documents->Get(i);
      if (!newVal->IsObject() || newVal->IsArray()) {
        // We insert a non-object that should fail later.
        updateBuilder.add(VPackValue(VPackValueType::Null));
        continue;
      }
      VPackObjectBuilder guard(&updateBuilder);
      workOnOneDocument(newVal);
      workOnOneSearchVal(searchVals->Get(i), true);
    }
  }

  VPackSlice const update = updateBuilder.slice();
  auto transactionContext =
    std::make_shared<transaction::V8Context>(col->vocbase(), true);

  // Now start the transaction:
  SingleCollectionTransaction trx(transactionContext, collectionName,
                                  AccessMode::Type::WRITE);

  if (!args[0]->IsArray()) {
    trx.addHint(transaction::Hints::Hint::SINGLE_OPERATION);
  }

  Result res = trx.begin();

  if (!res.ok()) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  OperationResult opResult;
  if (operation == TRI_VOC_DOCUMENT_OPERATION_REPLACE) {
    opResult = trx.replace(collectionName, update, options);
  } else {
    opResult = trx.update(collectionName, update, options);
  }
  res = trx.finish(opResult.result);

  if (!res.ok()) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  if (options.silent) {
    // no return value
    TRI_V8_RETURN_TRUE();
  }

  VPackSlice resultSlice = opResult.slice();
  TRI_V8_RETURN(TRI_VPackToV8(isolate, resultSlice,
                              transactionContext->getVPackOptions()));
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock documentsCollectionReplace
/// Replace a document, collection method
////////////////////////////////////////////////////////////////////////////////

static void JS_ReplaceVocbaseCol(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  ModifyVocbaseCol(TRI_VOC_DOCUMENT_OPERATION_REPLACE, args);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock documentsCollectionUpdate
////////////////////////////////////////////////////////////////////////////////

static void JS_UpdateVocbaseCol(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  ModifyVocbaseCol(TRI_VOC_DOCUMENT_OPERATION_UPDATE, args);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ModifyVocbase, database method, only single documents
////////////////////////////////////////////////////////////////////////////////

static void ModifyVocbase(TRI_voc_document_operation_e operation,
                          v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  // check the arguments
  uint32_t const argLength = args.Length();

  if (argLength < 2 ||
      argLength > (operation == TRI_VOC_DOCUMENT_OPERATION_REPLACE ? 4UL : 5UL)) {
    if (operation == TRI_VOC_DOCUMENT_OPERATION_REPLACE) {
      TRI_V8_THROW_EXCEPTION_USAGE(
          "_replace(<document>, <data>, {overwrite: booleanValue, waitForSync: "
          "booleanValue, returnNew: booleanValue, returnOld: booleanValue,"
          " silent: booleanValue})");
    } else {
      TRI_V8_THROW_EXCEPTION_USAGE(
          "_update(<document>, <data>, {overwrite: booleanValue, keepNull: "
          "booleanValue, mergeObjects: booleanValue, waitForSync: "
          "booleanValue, returnNew: booleanValue, returnOld: booleanValue,"
          " silent: booleanValue})");
    }
  }

  // we're only accepting "real" object documents
  if (!args[1]->IsObject() || args[1]->IsArray()) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
  }

  OperationOptions options;
  options.ignoreRevs = false;
  if (args.Length() > 2) {
    parseReplaceAndUpdateOptions(isolate, args, options, operation);
  }

  std::shared_ptr<arangodb::LogicalCollection> collection;
  std::string collectionName;
  auto& vocbase = GetContextVocBase(isolate);
  auto transactionContext =
    std::make_shared<transaction::V8Context>(vocbase, true);
  VPackBuilder updateBuilder;

  {
    VPackObjectBuilder guard(&updateBuilder);
    int res = V8ToVPackNoKeyRevId(isolate, updateBuilder, args[1]);

    if (res != TRI_ERROR_NO_ERROR) {
      TRI_V8_THROW_EXCEPTION(res);
    }

    res = ParseDocumentOrDocumentHandle(
      isolate,
      &(transactionContext->resolver()),
      collection,
      collectionName,
      updateBuilder,
      !options.ignoreRevs,
      args[0]
    );

    if (res != TRI_ERROR_NO_ERROR) {
      TRI_V8_THROW_EXCEPTION(res);
    }
  }

  SingleCollectionTransaction trx(transactionContext, collectionName,
                                  AccessMode::Type::WRITE);
  trx.addHint(transaction::Hints::Hint::SINGLE_OPERATION);

  Result res = trx.begin();
  if (!res.ok()) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  VPackSlice const update = updateBuilder.slice();

  OperationResult opResult;
  if (operation == TRI_VOC_DOCUMENT_OPERATION_REPLACE) {
    opResult = trx.replace(collectionName, update, options);
  } else {
    opResult = trx.update(collectionName, update, options);
  }

  res = trx.finish(opResult.result);

  if (opResult.fail()) {
    TRI_V8_THROW_EXCEPTION(opResult.result);
  }

  if (!res.ok()) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  if (options.silent) {
    // no return value
    TRI_V8_RETURN_TRUE();
  }

  VPackSlice resultSlice = opResult.slice();
  TRI_V8_RETURN(TRI_VPackToV8(isolate, resultSlice,
                              transactionContext->getVPackOptions()));
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock documentsDocumentReplace
////////////////////////////////////////////////////////////////////////////////

static void JS_ReplaceVocbase(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  ModifyVocbase(TRI_VOC_DOCUMENT_OPERATION_REPLACE, args);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock documentsDocumentUpdate
////////////////////////////////////////////////////////////////////////////////

static void JS_UpdateVocbase(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  ModifyVocbase(TRI_VOC_DOCUMENT_OPERATION_UPDATE, args);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Pregel Stuff
////////////////////////////////////////////////////////////////////////////////

static void JS_PregelStart(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);
  ServerState *ss = ServerState::instance();
  if (ss->isRunningInCluster() && !ss->isCoordinator()) {
    TRI_V8_THROW_EXCEPTION_USAGE("Only call on coordinator or in single server mode");
  }

  // check the arguments
  uint32_t const argLength = args.Length();
  if (argLength < 3 || !args[0]->IsString()) {
      // TODO extend this for named graphs, use the Graph class
      TRI_V8_THROW_EXCEPTION_USAGE("_pregelStart(<algorithm>, <vertexCollections>,"
                                   "<edgeCollections>[, {maxGSS:100, ...}]");
  }
  auto parse = [](v8::Local<v8::Value> const& value, std::vector<std::string> &out) {
    v8::Handle<v8::Array> array = v8::Handle<v8::Array>::Cast(value);
    uint32_t const n = array->Length();
    for (uint32_t i = 0; i < n; ++i) {
      v8::Handle<v8::Value> obj = array->Get(i);
      if (obj->IsString()) {
        out.push_back(TRI_ObjectToString(obj));
      }
    }
  };

  std::string algorithm = TRI_ObjectToString(args[0]);
  std::vector<std::string> paramVertices, paramEdges;
  if (args[1]->IsArray()) {
    parse(args[1], paramVertices);
  } else if (args[1]->IsString()) {
      paramVertices.push_back(TRI_ObjectToString(args[1]));
  } else {
      TRI_V8_THROW_EXCEPTION_USAGE("Specify an array of vertex collections (or a string)");
  }
  if (paramVertices.size() == 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("Specify at least one vertex collection");
  }
  if (args[2]->IsArray()) {
    parse(args[2], paramEdges);
  } else if (args[2]->IsString()) {
    paramEdges.push_back(TRI_ObjectToString(args[2]));
  } else {
    TRI_V8_THROW_EXCEPTION_USAGE("Specify an array of edge collections (or a string)");
  }
  if (paramEdges.size() == 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("Specify at least one edge collection");
  }
  VPackBuilder paramBuilder;
  if (argLength >= 4 && args[3]->IsObject()) {
      int res = TRI_V8ToVPack(isolate, paramBuilder, args[3], false);
      if (res != TRI_ERROR_NO_ERROR) {
          TRI_V8_THROW_EXCEPTION(res);
      }
  }

  auto& vocbase = GetContextVocBase(isolate);
  auto res = pregel::PregelFeature::startExecution(vocbase, algorithm,
                                                   paramVertices, paramEdges,
                                                   paramBuilder.slice());
  if (res.first.fail()) {
    TRI_V8_THROW_EXCEPTION(res.first);
  }

  TRI_V8_RETURN(v8::Number::New(isolate, static_cast<double>(res.second)));
  TRI_V8_TRY_CATCH_END
}

static void JS_PregelStatus(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  // check the arguments
  uint32_t const argLength = args.Length();
  if (argLength != 1 || (!args[0]->IsNumber() && !args[0]->IsString())) {
    // TODO extend this for named graphs, use the Graph class
    TRI_V8_THROW_EXCEPTION_USAGE("_pregelStatus(<executionNum>]");
  }
  uint64_t executionNum = TRI_ObjectToUInt64(args[0], true);
  auto c = pregel::PregelFeature::instance()->conductor(executionNum);
  if (!c) {
    TRI_V8_THROW_EXCEPTION_USAGE("Execution number is invalid");
  }

  VPackBuilder builder = c->toVelocyPack();
  TRI_V8_RETURN(TRI_VPackToV8(isolate, builder.slice()));
  TRI_V8_TRY_CATCH_END
}

static void JS_PregelCancel(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  // check the arguments
  uint32_t const argLength = args.Length();
  if (argLength != 1 || !(args[0]->IsNumber() || args[0]->IsString())) {
    // TODO extend this for named graphs, use the Graph class
    TRI_V8_THROW_EXCEPTION_USAGE("_pregelStatus(<executionNum>)");
  }
  uint64_t executionNum = TRI_ObjectToUInt64(args[0], true);
  auto c = pregel::PregelFeature::instance()->conductor(executionNum);
  if (!c) {
    TRI_V8_THROW_EXCEPTION_USAGE("Execution number is invalid");
  }
  c->cancel();
  pregel::PregelFeature::instance()->cleanupConductor(executionNum);

  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END
}

static void JS_PregelAQLResult(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  // check the arguments
  uint32_t const argLength = args.Length();
  if (argLength != 1 || !(args[0]->IsNumber() || args[0]->IsString())) {
    // TODO extend this for named graphs, use the Graph class
    TRI_V8_THROW_EXCEPTION_USAGE("_pregelAqlResult(<executionNum>)");
  }

  pregel::PregelFeature* feature = pregel::PregelFeature::instance();
  if (!feature) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_FAILED, "pregel is not enabled");
  }

  uint64_t executionNum = TRI_ObjectToUInt64(args[0], true);
  if (ServerState::instance()->isSingleServerOrCoordinator()) {
    auto c = pregel::PregelFeature::instance()->conductor(executionNum);
    if (!c) {
      TRI_V8_THROW_EXCEPTION_USAGE("Execution number is invalid");
    }

    VPackBuilder docs;
    c->collectAQLResults(docs);
    if (docs.isEmpty()) {
      TRI_V8_RETURN_NULL();
    }
    TRI_ASSERT(docs.slice().isArray());

    VPackOptions resultOptions = VPackOptions::Defaults;
    auto documents = TRI_VPackToV8(isolate, docs.slice(), &resultOptions);
    TRI_V8_RETURN(documents);
  } else {
    TRI_V8_THROW_EXCEPTION_USAGE("Only valid on the coordinator");
  }

  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock collectionRevision
////////////////////////////////////////////////////////////////////////////////

static void JS_RevisionVocbaseCol(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  auto* collection = UnwrapCollection(args.Holder());

  if (!collection) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  TRI_voc_rid_t revisionId;

  methods::Collections::Context ctxt(collection->vocbase(), *collection);
  auto res = methods::Collections::revisionId(ctxt, revisionId);

  if (res.fail()) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  std::string ridString = TRI_RidToString(revisionId);
  TRI_V8_RETURN(TRI_V8_STD_STRING(isolate, ridString));
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief inserts a document, using VPack
////////////////////////////////////////////////////////////////////////////////

static void InsertVocbaseCol(v8::Isolate* isolate,
                             v8::FunctionCallbackInfo<v8::Value> const& args,
                             std::string* attachment) {
  v8::HandleScope scope(isolate);

  auto* collection = UnwrapCollection(args.Holder());

  if (!collection) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  auto isEdgeCollection = (collection->type() == TRI_COL_TYPE_EDGE);
  uint32_t const argLength = args.Length();

  // Position of <data> and <options>
  // They differ for edge (old signature) and document.
  uint32_t docIdx = 0;
  uint32_t optsIdx = (attachment == nullptr) ? 1 : 2;

  TRI_GET_GLOBALS();

  if (argLength < 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("insert(<data>, [, <options>])");
  }

  bool oldEdgeSignature = false;

  if (isEdgeCollection && argLength >= 3) {
    oldEdgeSignature = true;
    if (argLength > 4) {
      TRI_V8_THROW_EXCEPTION_USAGE(
          "insert(<from>, <to>, <data> [, <options>])");
    }
    docIdx = 2;
    optsIdx = (attachment == nullptr) ? 3 : 4;
    if (args[2]->IsArray()) {
      TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
    }
  } else {
    if (argLength < 1 || argLength > 2) {
      TRI_V8_THROW_EXCEPTION_USAGE("insert(<data> [, <options>])");
    }
  }

  OperationOptions options;
  if (argLength > optsIdx && args[optsIdx]->IsObject()) {
    v8::Handle<v8::Object> optionsObject = args[optsIdx].As<v8::Object>();
    TRI_GET_GLOBAL_STRING(WaitForSyncKey);
    if (optionsObject->Has(WaitForSyncKey)) {
      options.waitForSync =
          TRI_ObjectToBoolean(optionsObject->Get(WaitForSyncKey));
    }
    TRI_GET_GLOBAL_STRING(OverwriteKey);
    if (optionsObject->Has(OverwriteKey)) {
      options.overwrite = TRI_ObjectToBoolean(optionsObject->Get(OverwriteKey));
    }
    TRI_GET_GLOBAL_STRING(SilentKey);
    if (optionsObject->Has(SilentKey)) {
      options.silent = TRI_ObjectToBoolean(optionsObject->Get(SilentKey));
    }
    TRI_GET_GLOBAL_STRING(ReturnNewKey);
    if (optionsObject->Has(ReturnNewKey)) {
      options.returnNew = TRI_ObjectToBoolean(optionsObject->Get(ReturnNewKey));
    }
    TRI_GET_GLOBAL_STRING(ReturnOldKey);
    if (optionsObject->Has(ReturnOldKey)) {
      options.returnOld = TRI_ObjectToBoolean(optionsObject->Get(ReturnOldKey)) && options.overwrite;
    }
    TRI_GET_GLOBAL_STRING(IsRestoreKey);
    if (optionsObject->Has(IsRestoreKey)) {
      options.isRestore = TRI_ObjectToBoolean(optionsObject->Get(IsRestoreKey));
    }
    TRI_GET_GLOBAL_STRING(IsSynchronousReplicationKey);
    if (optionsObject->Has(IsSynchronousReplicationKey)) {
      options.isSynchronousReplicationFrom
        = TRI_ObjectToString(optionsObject->Get(IsSynchronousReplicationKey));
    }
  } else {
    options.waitForSync = ExtractBooleanArgument(args, optsIdx + 1);
  }

  if (!args[docIdx]->IsObject()) {
    // invalid value type. must be a document
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
  }

  // copy default options (and set exclude handler in copy)
  VPackOptions vpackOptions = VPackOptions::Defaults;
  vpackOptions.attributeExcludeHandler =
      basics::VelocyPackHelper::getExcludeHandler();
  VPackBuilder builder(&vpackOptions);

  auto doOneDocument = [&](v8::Handle<v8::Value> obj) -> void {
    int res = TRI_V8ToVPack(isolate, builder, obj, true);

    if (res != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(res);
    }

    if (isEdgeCollection && oldEdgeSignature) {
      // Just insert from and to. Check is done later.
      std::string tmpId(ExtractIdString(isolate, args[0]));
      if (tmpId.empty()) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD);
      }
      builder.add(StaticStrings::FromString, VPackValue(tmpId));

      tmpId = ExtractIdString(isolate, args[1]);
      if (tmpId.empty()) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD);
      }
      builder.add(StaticStrings::ToString, VPackValue(tmpId));
    }

    if (attachment != nullptr) {
      builder.add(StaticStrings::AttachmentString,
                  VPackValue(*attachment));
    }

    builder.close();
  };

  v8::Handle<v8::Value> payload = args[docIdx];
  bool payloadIsArray;
  if (payload->IsArray()) {
    payloadIsArray = true;
    VPackArrayBuilder b(&builder);
    v8::Handle<v8::Array> array = v8::Handle<v8::Array>::Cast(payload);
    uint32_t const n = array->Length();
    for (uint32_t i = 0; i < n; ++i) {
      doOneDocument(array->Get(i));
    }
  } else {
    payloadIsArray = false;
    doOneDocument(payload);
  }

  // load collection
  auto transactionContext =
    std::make_shared<transaction::V8Context>(collection->vocbase(), true);
  SingleCollectionTransaction trx(
    transactionContext, *collection, AccessMode::Type::WRITE
  );

  if (!payloadIsArray && !options.overwrite) {
    trx.addHint(transaction::Hints::Hint::SINGLE_OPERATION);
  }

  Result res = trx.begin();

  if (!res.ok()) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  auto result = trx.insert(collection->name(), builder.slice(), options);

  res = trx.finish(result.result);

  if (!res.ok()) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  if (options.silent) {
    // no return value
    TRI_V8_RETURN_TRUE();
  }

  VPackSlice resultSlice = result.slice();

  auto v8Result = TRI_VPackToV8(isolate, resultSlice,
                                transactionContext->getVPackOptions());

  TRI_V8_RETURN(v8Result);
}

static void JS_InsertVocbaseCol(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  InsertVocbaseCol(isolate, args, nullptr);
  TRI_V8_TRY_CATCH_END
}

static void JS_BinaryInsertVocbaseCol(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  uint32_t const argLength = args.Length();

  if (argLength < 2 || argLength > 3) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "binaryInsert(<data>, <filename> [, <options>])");
  }

  std::string filename = TRI_ObjectToString(args[1]);
  std::string attachment;

  try {
    attachment = FileUtils::slurp(filename);
  } catch (...) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_errno(), TRI_last_error());
  }

  attachment = StringUtils::encodeBase64(attachment);

  InsertVocbaseCol(isolate, args, &attachment);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the globally unique id of a collection
////////////////////////////////////////////////////////////////////////////////

static void JS_GloballyUniqueIdVocbaseCol(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  auto* collection = UnwrapCollection(args.Holder());

  if (!collection) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  auto& uniqueId = collection->guid();

  TRI_V8_RETURN(TRI_V8_ASCII_STD_STRING(isolate, uniqueId));
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the status of a collection
////////////////////////////////////////////////////////////////////////////////

static void JS_StatusVocbaseCol(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  auto* collection = UnwrapCollection(args.Holder());

  if (!collection) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  if (ServerState::instance()->isCoordinator()) {
    auto& databaseName = collection->vocbase().name();

    auto ci = ClusterInfo::instance()->getCollectionNT(
        databaseName, std::to_string(collection->id())
      );
    if (ci != nullptr) {
      TRI_V8_RETURN(v8::Number::New(isolate, (int)ci->status()));
    }
    else {
      TRI_V8_RETURN(v8::Number::New(isolate, (int)TRI_VOC_COL_STATUS_DELETED));
    }
  }
  // intentionally falls through

  auto status = collection->status();

  TRI_V8_RETURN(v8::Number::New(isolate, (int)status));
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief truncates a collection
////////////////////////////////////////////////////////////////////////////////

static void JS_TruncateVocbaseCol(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  OperationOptions opOptions;
  opOptions.waitForSync = ExtractBooleanArgument(args, 1);
  ExtractStringArgument(args, 2, opOptions.isSynchronousReplicationFrom);

  auto* collection = UnwrapCollection(args.Holder());

  if (!collection) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  auto ctx = transaction::V8Context::Create(collection->vocbase(), true);
  SingleCollectionTransaction trx(
    ctx, *collection, AccessMode::Type::EXCLUSIVE
  );
  trx.addHint(transaction::Hints::Hint::INTERMEDIATE_COMMITS);
  trx.addHint(transaction::Hints::Hint::ALLOW_RANGE_DELETE);
  Result res = trx.begin();

  if (!res.ok()) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  auto result = trx.truncate(collection->name(), opOptions);

  res = trx.finish(result.result);

  if (result.fail()) {
    TRI_V8_THROW_EXCEPTION(result.result);
  }

  if (!res.ok()) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock collectionType
////////////////////////////////////////////////////////////////////////////////

static void JS_TypeVocbaseCol(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  auto* collection = UnwrapCollection(args.Holder());

  if (!collection) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  if (ServerState::instance()->isCoordinator()) {
    auto& databaseName = collection->vocbase().name();

    auto ci = ClusterInfo::instance()->getCollectionNT(
        databaseName, std::to_string(collection->id())
      );
    if (ci != nullptr) {
      TRI_V8_RETURN(v8::Number::New(isolate, (int)ci->type()));
    }
    else {
      TRI_V8_RETURN(v8::Number::New(isolate, (int)(collection->type())));
    }
  }
  // intentionally falls through

  auto type = collection->type();

  TRI_V8_RETURN(v8::Number::New(isolate, (int)type));
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock collectionUnload
////////////////////////////////////////////////////////////////////////////////

static void JS_UnloadVocbaseCol(
    v8::FunctionCallbackInfo<v8::Value> const& args) {

  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  auto* collection = UnwrapCollection(args.Holder());

  if (!collection) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  auto res = methods::Collections::unload(&(collection->vocbase()), collection);

  if (res.fail()) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the version of a collection
////////////////////////////////////////////////////////////////////////////////

static void JS_VersionVocbaseCol(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  auto* collection = UnwrapCollection(args.Holder());

  if (!collection) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  TRI_V8_RETURN(v8::Number::New(isolate, collection->version()));
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock collectionDatabaseName
////////////////////////////////////////////////////////////////////////////////

static void JS_CollectionVocbase(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  auto& vocbase = GetContextVocBase(isolate);

  if (vocbase.isDropped()) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  // expecting one argument
  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("_collection(<name>|<identifier>)");
  }

  v8::Handle<v8::Value> val = args[0];
  auto collection = GetCollectionFromArgument(vocbase, val);

  if (collection == nullptr) {
    TRI_V8_RETURN_NULL();
  }

  // check authentication after ensuring the collection exists
  if (ExecContext::CURRENT != nullptr &&
      !ExecContext::CURRENT->canUseCollection(collection->name(), auth::Level::RO)) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(
      TRI_ERROR_FORBIDDEN,
      std::string("No access to collection '") + TRI_ObjectToString(val) + "'"
    );
  }

  v8::Handle<v8::Value> result = WrapCollection(isolate, collection);
  if (result.IsEmpty()) {
    TRI_V8_THROW_EXCEPTION_MEMORY();
  }

  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock collectionDatabaseNameAll
////////////////////////////////////////////////////////////////////////////////
static void JS_CollectionsVocbase(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);
  auto& vocbase = GetContextVocBase(isolate);

  if (vocbase.isDropped()) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  auto colls = GetCollections(vocbase);

  std::sort(
    colls.begin(),
    colls.end(),
    [](
      std::shared_ptr<LogicalCollection> const& lhs,
      std::shared_ptr<LogicalCollection> const& rhs
    )->bool {
      return arangodb::basics::StringUtils::tolower(lhs->name()) < arangodb::basics::StringUtils::tolower(rhs->name());
    }
  );

  bool error = false;

  // already create an array of the correct size
  v8::Handle<v8::Array> result = v8::Array::New(isolate);
  size_t const n = colls.size();
  size_t x = 0;

  for (size_t i = 0; i < n; ++i) {
    auto& collection = colls[i];

    if (ExecContext::CURRENT != nullptr &&
        !ExecContext::CURRENT->canUseCollection(
          vocbase.name(), collection->name(), auth::Level::RO
        )
       ) {
      continue;
    }

    v8::Handle<v8::Value> c = WrapCollection(isolate, collection);
    if (c.IsEmpty()) {
      error = true;
      break;
    }

    result->Set(static_cast<uint32_t>(x++), c);
  }

  if (error) {
    TRI_V8_THROW_EXCEPTION_MEMORY();
  }

  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns all collection names
////////////////////////////////////////////////////////////////////////////////

static void JS_CompletionsVocbase(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  auto& vocbase = GetContextVocBase(isolate);

  if (vocbase.isDropped()) {
    TRI_V8_RETURN(v8::Array::New(isolate));
  }

  std::vector<std::string> names;

  if (ServerState::instance()->isCoordinator()) {
    if (ClusterInfo::instance()->doesDatabaseExist(vocbase.name())) {
      names = GetCollectionNamesCluster(&vocbase);
    }
  } else {
    names = vocbase.collectionNames();
  }

  uint32_t j = 0;

  v8::Handle<v8::Array> result = v8::Array::New(isolate);
  // add collection names
  for (auto& name : names) {
    result->Set(j++, TRI_V8_STD_STRING(isolate, name));
  }

  // add function names. these are hard coded
  result->Set(j++, TRI_V8_ASCII_STRING(isolate, "_collection()"));
  result->Set(j++, TRI_V8_ASCII_STRING(isolate, "_collections()"));
  result->Set(j++, TRI_V8_ASCII_STRING(isolate, "_create()"));
  result->Set(j++, TRI_V8_ASCII_STRING(isolate, "_createDatabase()"));
  result->Set(j++, TRI_V8_ASCII_STRING(isolate, "_createDocumentCollection()"));
  result->Set(j++, TRI_V8_ASCII_STRING(isolate, "_createEdgeCollection()"));
  result->Set(j++, TRI_V8_ASCII_STRING(isolate, "_createView()"));
  result->Set(j++, TRI_V8_ASCII_STRING(isolate, "_createStatement()"));
  result->Set(j++, TRI_V8_ASCII_STRING(isolate, "_document()"));
  result->Set(j++, TRI_V8_ASCII_STRING(isolate, "_drop()"));
  result->Set(j++, TRI_V8_ASCII_STRING(isolate, "_dropDatabase()"));
  result->Set(j++, TRI_V8_ASCII_STRING(isolate, "_dropView()"));
  result->Set(j++, TRI_V8_ASCII_STRING(isolate, "_engine()"));
  result->Set(j++, TRI_V8_ASCII_STRING(isolate, "_engineStats()"));
  result->Set(j++, TRI_V8_ASCII_STRING(isolate, "_executeTransaction()"));
  result->Set(j++, TRI_V8_ASCII_STRING(isolate, "_exists()"));
  result->Set(j++, TRI_V8_ASCII_STRING(isolate, "_id"));
  result->Set(j++, TRI_V8_ASCII_STRING(isolate, "_isSystem()"));
  result->Set(j++, TRI_V8_ASCII_STRING(isolate, "_databases()"));
  result->Set(j++, TRI_V8_ASCII_STRING(isolate, "_engine()"));
  result->Set(j++, TRI_V8_ASCII_STRING(isolate, "_name()"));
  result->Set(j++, TRI_V8_ASCII_STRING(isolate, "_path()"));
  result->Set(j++, TRI_V8_ASCII_STRING(isolate, "_parse()"));
  result->Set(j++, TRI_V8_ASCII_STRING(isolate, "_pregelStart()"));
  result->Set(j++, TRI_V8_ASCII_STRING(isolate, "_pregelStatus()"));
  result->Set(j++, TRI_V8_ASCII_STRING(isolate, "_pregelStop()"));
  result->Set(j++, TRI_V8_ASCII_STRING(isolate, "_profileQuery()"));
  result->Set(j++, TRI_V8_ASCII_STRING(isolate, "_query()"));
  result->Set(j++, TRI_V8_ASCII_STRING(isolate, "_remove()"));
  result->Set(j++, TRI_V8_ASCII_STRING(isolate, "_replace()"));
  result->Set(j++, TRI_V8_ASCII_STRING(isolate, "_update()"));
  result->Set(j++, TRI_V8_ASCII_STRING(isolate, "_useDatabase()"));
  result->Set(j++, TRI_V8_ASCII_STRING(isolate, "_version()"));
  result->Set(j++, TRI_V8_ASCII_STRING(isolate, "_view()"));
  result->Set(j++, TRI_V8_ASCII_STRING(isolate, "_views()"));

  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock documentsDocumentRemove
////////////////////////////////////////////////////////////////////////////////

static void JS_RemoveVocbase(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  RemoveVocbase(args);
  // cppcheck-suppress style
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock documentsDocumentName
////////////////////////////////////////////////////////////////////////////////

static void JS_DocumentVocbase(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  DocumentVocbase(args);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock documentsDocumentExists
////////////////////////////////////////////////////////////////////////////////

static void JS_ExistsVocbase(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  return ExistsVocbaseVPack(false, args);

  // cppcheck-suppress style
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock collectionCount
////////////////////////////////////////////////////////////////////////////////

static void JS_CountVocbaseCol(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  auto* col = UnwrapCollection(args.Holder());

  if (!col) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  if (args.Length() > 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("count()");
  }

  bool details = false;
  if (args.Length() == 1 && ServerState::instance()->isCoordinator()) {
    details = TRI_ObjectToBoolean(args[0]);
  }

  auto& collectionName = col->name();
  SingleCollectionTransaction trx(
    transaction::V8Context::Create(col->vocbase(), true),
    collectionName,
    AccessMode::Type::READ
  );

  if (trx.isLockedShard(collectionName)) {
    trx.addHint(transaction::Hints::Hint::LOCK_NEVER);
  }

  Result res = trx.begin();

  if (!res.ok()) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  OperationResult opResult = trx.count(collectionName, details ? transaction::CountType::Detailed : transaction::CountType::Normal);
  res = trx.finish(opResult.result);

  if (res.fail()) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  VPackSlice s = opResult.slice();
  if (details) {
    TRI_ASSERT(s.isObject());
    v8::Handle<v8::Value> result = TRI_VPackToV8(isolate, s);
    TRI_V8_RETURN(result);
  } else {
    TRI_ASSERT(s.isNumber());
    TRI_V8_RETURN(v8::Number::New(isolate, static_cast<double>(s.getNumber<double>())));
  }
  TRI_V8_TRY_CATCH_END
}

// .............................................................................
// Warmup Index caches
// .............................................................................

static void JS_WarmupVocbaseCol(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  auto* collection =UnwrapCollection(args.Holder());

  if (!collection) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  auto res =
    arangodb::methods::Collections::warmup(collection->vocbase(), *collection);

  if (res.fail()) {
    TRI_V8_THROW_EXCEPTION(res);
  }
  TRI_V8_RETURN_UNDEFINED();

  TRI_V8_TRY_CATCH_END
}

// .............................................................................
// generate the arangodb::LogicalCollection template
// .............................................................................

void TRI_InitV8Collections(v8::Handle<v8::Context> context,
                           TRI_vocbase_t* vocbase, TRI_v8_global_t* v8g,
                           v8::Isolate* isolate,
                           v8::Handle<v8::ObjectTemplate> ArangoDBNS) {
  TRI_AddMethodVocbase(isolate, ArangoDBNS, TRI_V8_ASCII_STRING(isolate, "_collection"),
                       JS_CollectionVocbase);
  TRI_AddMethodVocbase(isolate, ArangoDBNS, TRI_V8_ASCII_STRING(isolate, "_collections"),
                       JS_CollectionsVocbase);
  TRI_AddMethodVocbase(isolate, ArangoDBNS, TRI_V8_ASCII_STRING(isolate, "_COMPLETIONS"),
                       JS_CompletionsVocbase, true);
  TRI_AddMethodVocbase(isolate, ArangoDBNS, TRI_V8_ASCII_STRING(isolate, "_document"),
                       JS_DocumentVocbase);
  TRI_AddMethodVocbase(isolate, ArangoDBNS, TRI_V8_ASCII_STRING(isolate, "_exists"),
                       JS_ExistsVocbase);
  TRI_AddMethodVocbase(isolate, ArangoDBNS, TRI_V8_ASCII_STRING(isolate, "_remove"),
                       JS_RemoveVocbase);
  TRI_AddMethodVocbase(isolate, ArangoDBNS, TRI_V8_ASCII_STRING(isolate, "_replace"),
                       JS_ReplaceVocbase);
  TRI_AddMethodVocbase(isolate, ArangoDBNS, TRI_V8_ASCII_STRING(isolate, "_update"),
                       JS_UpdateVocbase);
  TRI_AddMethodVocbase(isolate, ArangoDBNS, TRI_V8_ASCII_STRING(isolate, "_pregelStart"),
                       JS_PregelStart);
  TRI_AddMethodVocbase(isolate, ArangoDBNS,
                       TRI_V8_ASCII_STRING(isolate, "_pregelStatus"), JS_PregelStatus);
  TRI_AddMethodVocbase(isolate, ArangoDBNS,
                       TRI_V8_ASCII_STRING(isolate, "_pregelCancel"), JS_PregelCancel);
  TRI_AddMethodVocbase(isolate, ArangoDBNS,
                       TRI_V8_ASCII_STRING(isolate, "_pregelAqlResult"),
                       JS_PregelAQLResult);

  v8::Handle<v8::ObjectTemplate> rt;
  v8::Handle<v8::FunctionTemplate> ft;

  ft = v8::FunctionTemplate::New(isolate);
  ft->SetClassName(TRI_V8_ASCII_STRING(isolate, "ArangoCollection"));

  rt = ft->InstanceTemplate();
  rt->SetInternalFieldCount(3);

  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "count"),
                       JS_CountVocbaseCol);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "document"),
                       JS_DocumentVocbaseCol);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "_binaryDocument"),
                       JS_BinaryDocumentVocbaseCol);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "drop"),
                       JS_DropVocbaseCol);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "exists"),
                       JS_ExistsVocbaseVPack);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "figures"),
                       JS_FiguresVocbaseCol);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "insert"),
                       JS_InsertVocbaseCol);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "_binaryInsert"),
                       JS_BinaryInsertVocbaseCol);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "globallyUniqueId"),
                       JS_GloballyUniqueIdVocbaseCol);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "load"),
                       JS_LoadVocbaseCol);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "name"),
                       JS_NameVocbaseCol);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "path"),
                       JS_PathVocbaseCol);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "planId"),
                       JS_PlanIdVocbaseCol);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "properties"),
                       JS_PropertiesVocbaseCol);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "remove"),
                       JS_RemoveVocbaseCol);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "revision"),
                       JS_RevisionVocbaseCol);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "rename"),
                       JS_RenameVocbaseCol);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "replace"),
                       JS_ReplaceVocbaseCol);
  TRI_AddMethodVocbase(
      isolate, rt, TRI_V8_ASCII_STRING(isolate, "save"),
      JS_InsertVocbaseCol);  // note: save is now an alias for insert
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "status"),
                       JS_StatusVocbaseCol);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "TRUNCATE"),
                       JS_TruncateVocbaseCol, true);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "type"),
                       JS_TypeVocbaseCol);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "unload"),
                       JS_UnloadVocbaseCol);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "update"),
                       JS_UpdateVocbaseCol);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "version"),
                       JS_VersionVocbaseCol);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING(isolate, "loadIndexesIntoMemory"),
                       JS_WarmupVocbaseCol);

  TRI_InitV8IndexCollection(isolate, rt);

  v8g->VocbaseColTempl.Reset(isolate, rt);
  TRI_AddGlobalFunctionVocbase(isolate, TRI_V8_ASCII_STRING(isolate, "ArangoCollection"),
                               ft->GetFunction());
}
