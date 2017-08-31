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
#include "Basics/ScopeGuard.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringBuffer.h"
#include "Basics/Utf8Helper.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/WriteLocker.h"
#include "Basics/conversions.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ClusterMethods.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "Indexes/Index.h"
#include "Cluster/FollowerInfo.h"
#include "Pregel/AggregatorHandler.h"
#include "Pregel/Conductor.h"
#include "Pregel/PregelFeature.h"
#include "Pregel/Worker.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/FeatureCacheFeature.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/PhysicalCollection.h"
#include "StorageEngine/StorageEngine.h"
#include "Transaction/Hints.h"
#include "Transaction/V8Context.h"
#include "Utils/CollectionNameResolver.h"
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
#include "VocBase/modes.h"

#include <velocypack/Builder.h>
#include <velocypack/HexDump.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

struct LocalCollectionGuard {
  explicit LocalCollectionGuard(LogicalCollection* collection)
      : _collection(collection) {}

  ~LocalCollectionGuard() {
    if (_collection != nullptr && !_collection->isLocal()) {
      delete _collection;
    }
  }

  LogicalCollection* _collection;
};

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

static int ParseDocumentOrDocumentHandle(v8::Isolate* isolate,
                                         TRI_vocbase_t* vocbase,
                                         CollectionNameResolver const* resolver,
                                         LogicalCollection const*& collection,
                                         std::string& collectionName,
                                         VPackBuilder& builder,
                                         bool includeRev,
                                         v8::Handle<v8::Value> const val) {
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
    collectionName = resolver->getCollectionNameCluster(collection->cid());
  } else {
    // we read a collection name from the document id
    // check cross-collection requests
    if (collection != nullptr) {
      if (!EqualCollection(resolver, collectionName, collection)) {
        return TRI_ERROR_ARANGO_CROSS_COLLECTION_REQUEST;
      }
    }
  }

  TRI_ASSERT(!collectionName.empty());

  if (collection == nullptr) {
    // no collection object was passed, now check the user-supplied collection
    // name
    if (ServerState::instance()->isCoordinator()) {
      ClusterInfo* ci = ClusterInfo::instance();
      try {
        std::shared_ptr<LogicalCollection> col =
            ci->getCollection(vocbase->name(), collectionName);
        auto colCopy = col->clone();
        collection = colCopy.release();
      } catch (...) {
        return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
      }
    } else {
      collection = resolver->getCollectionStruct(collectionName);
    }
    if (collection == nullptr) {
      // collection not found
      return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
    }
  }
  TRI_ASSERT(collection != nullptr);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief V8ToVPack without _key and _rev, builder must be open with an
/// object and is left open at the end
////////////////////////////////////////////////////////////////////////////////

static int V8ToVPackNoKeyRevId (v8::Isolate* isolate,
                                VPackBuilder& builder,
                                v8::Local<v8::Value> const obj) {

  TRI_ASSERT(obj->IsObject() && !obj->IsArray());
  auto o = v8::Local<v8::Object>::Cast(obj);
  v8::Handle<v8::Array> names = o->GetOwnPropertyNames();
  uint32_t const n = names->Length();
  for (uint32_t i = 0; i < n; ++i) {
    v8::Handle<v8::Value> key = names->Get(i);
    TRI_Utf8ValueNFC str(TRI_UNKNOWN_MEM_ZONE, key);
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
/// @brief get all cluster collections
////////////////////////////////////////////////////////////////////////////////

static std::vector<LogicalCollection*> GetCollectionsCluster(
    TRI_vocbase_t* vocbase) {
  std::vector<LogicalCollection*> result;

  std::vector<std::shared_ptr<LogicalCollection>> const collections =
      ClusterInfo::instance()->getCollections(vocbase->name());

  for (auto& collection : collections) {
    std::unique_ptr<LogicalCollection> c(collection->clone());
    result.emplace_back(c.get());
    c.release();
  }

  return result;
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
  LogicalCollection const* col = nullptr;

  if (useCollection) {
    // called as db.collection.exists()
    col =
        TRI_UnwrapClass<LogicalCollection>(args.Holder(), WRP_VOCBASE_COL_TYPE);

    if (col == nullptr) {
      TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
    }

    vocbase = col->vocbase();
  } else {
    // called as db._exists()
    vocbase = GetContextVocBase(isolate);
  }

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  auto transactionContext = std::make_shared<transaction::V8Context>(vocbase, true);

  VPackBuilder builder;
  std::string collectionName;

  Result res;
  {
    VPackObjectBuilder guard(&builder);
    res = ParseDocumentOrDocumentHandle(
      isolate, vocbase, transactionContext->getResolver(), col,
      collectionName, builder, true, args[0]);
  }

  LocalCollectionGuard g(
      useCollection ? nullptr : const_cast<LogicalCollection*>(col));

  if (!res.ok()) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  TRI_ASSERT(col != nullptr);

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

  res = trx.finish(opResult.code);

  if (!opResult.successful()) {
    if (opResult.code == TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND) {
      TRI_V8_RETURN_FALSE();
    }
    TRI_V8_THROW_EXCEPTION(opResult.code);
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
  std::string collectionName;
  arangodb::LogicalCollection const* col
      = TRI_UnwrapClass<arangodb::LogicalCollection>(args.Holder(), WRP_VOCBASE_COL_TYPE);
  if (col == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }
  TRI_vocbase_t* vocbase = col->vocbase();
  collectionName = col->name();
  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

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

  auto transactionContext = std::make_shared<transaction::V8Context>(vocbase, true);

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

  res = trx.finish(opResult.code);

  if (!opResult.successful()) {
    TRI_V8_THROW_EXCEPTION(opResult.code);
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

  TRI_vocbase_t* vocbase;
  LogicalCollection const* col = nullptr;

  vocbase = GetContextVocBase(isolate);
  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  auto transactionContext = std::make_shared<transaction::V8Context>(vocbase, true);

  VPackBuilder builder;
  std::string collectionName;

  {
    VPackObjectBuilder guard(&builder);
    int res = ParseDocumentOrDocumentHandle(
        isolate, vocbase, transactionContext->getResolver(), col,
        collectionName, builder, true, args[0]);
    if (res != TRI_ERROR_NO_ERROR) {
      TRI_V8_THROW_EXCEPTION(res);
    }
  }

  LocalCollectionGuard g(const_cast<LogicalCollection*>(col));

  TRI_ASSERT(col != nullptr);
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

  res = trx.finish(opResult.code);

  if (!opResult.successful()) {
    TRI_V8_THROW_EXCEPTION(opResult.code);
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
  std::string collectionName;
  arangodb::LogicalCollection const* col
      = TRI_UnwrapClass<arangodb::LogicalCollection>(args.Holder(), WRP_VOCBASE_COL_TYPE);
  if (col == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }
  TRI_vocbase_t* vocbase = col->vocbase();
  collectionName = col->name();
  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  auto transactionContext = std::make_shared<transaction::V8Context>(vocbase, true);

  SingleCollectionTransaction trx(transactionContext, collectionName, AccessMode::Type::WRITE);
  if (!args[0]->IsArray()) {
    trx.addHint(transaction::Hints::Hint::SINGLE_OPERATION);
  }

  Result res = trx.begin();
  if (!res.ok()) {
    TRI_V8_THROW_EXCEPTION(res);
  }

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

  OperationResult result = trx.remove(collectionName, toRemove, options);

  res = trx.finish(result.code);

  if (!result.successful()) {
    TRI_V8_THROW_EXCEPTION(result.code);
  }

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

  TRI_vocbase_t* vocbase;
  LogicalCollection const* col = nullptr;

  vocbase = GetContextVocBase(isolate);
  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  auto transactionContext = std::make_shared<transaction::V8Context>(vocbase, true);

  VPackBuilder builder;
  std::string collectionName;

  {
    VPackObjectBuilder guard(&builder);
    int res = ParseDocumentOrDocumentHandle(
        isolate, vocbase, transactionContext->getResolver(), col, collectionName, builder,
        !options.ignoreRevs, args[0]);

    if (res != TRI_ERROR_NO_ERROR) {
      TRI_V8_THROW_EXCEPTION(res);
    }
  }

  LocalCollectionGuard g(const_cast<LogicalCollection*>(col));

  TRI_ASSERT(col != nullptr);
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

  res = trx.finish(result.code);

  if (!result.successful()) {
    TRI_V8_THROW_EXCEPTION(result.code);
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
  std::string collectionName;
  arangodb::LogicalCollection const* col =
      TRI_UnwrapClass<arangodb::LogicalCollection>(args.Holder(),
                                                   WRP_VOCBASE_COL_TYPE);

  if (col == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  TRI_vocbase_t* vocbase = col->vocbase();

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  VPackBuilder searchBuilder;
  v8::Local<v8::Value> const searchValue = args[0];
  collectionName = col->name();

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
      std::make_shared<transaction::V8Context>(vocbase, true);

  SingleCollectionTransaction trx(transactionContext, collectionName,
                                  AccessMode::Type::READ);

  trx.addHint(transaction::Hints::Hint::SINGLE_OPERATION);

  Result res = trx.begin();

  if (!res.ok()) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  OperationResult opResult = trx.document(collectionName, search, options);

  res = trx.finish(opResult.code);

  if (!opResult.successful()) {
    TRI_V8_THROW_EXCEPTION(opResult.code);
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

#ifndef USE_ENTERPRISE

////////////////////////////////////////////////////////////////////////////////
/// @brief unloads a collection, case of a coordinator in a cluster
////////////////////////////////////////////////////////////////////////////////

static int ULVocbaseColCoordinator(std::string const& databaseName,
                                   std::string const& collectionCID,
                                   TRI_vocbase_col_status_e status) {

  return ClusterInfo::instance()->setCollectionStatusCoordinator(
    databaseName, collectionCID, status);

}

////////////////////////////////////////////////////////////////////////////////
/// @brief drops a collection, case of a coordinator in a cluster
////////////////////////////////////////////////////////////////////////////////

static void DropVocbaseColCoordinator(
    v8::FunctionCallbackInfo<v8::Value> const& args,
    arangodb::LogicalCollection* collection) {
  // cppcheck-suppress *
  v8::Isolate* isolate = args.GetIsolate();

  if (collection->isSystem()) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_FORBIDDEN);
  }

  std::string const databaseName(collection->dbName());
  std::string const cid = collection->cid_as_string();

  ClusterInfo* ci = ClusterInfo::instance();
  std::string errorMsg;

  int res = ci->dropCollectionCoordinator(databaseName, cid, errorMsg, 120.0);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(res, errorMsg);
  }

  collection->setStatus(TRI_VOC_COL_STATUS_DELETED);

  TRI_V8_RETURN_UNDEFINED();
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock collectionDrop
////////////////////////////////////////////////////////////////////////////////

static void JS_DropVocbaseCol(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  arangodb::LogicalCollection* collection =
  TRI_UnwrapClass<arangodb::LogicalCollection>(args.Holder(), WRP_VOCBASE_COL_TYPE);
  if (collection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  AuthenticationFeature* auth = AuthenticationFeature::INSTANCE;
  TRI_ASSERT(auth != nullptr);
  if (ExecContext::CURRENT != nullptr && auth->isActive()) {
    AuthLevel level = ExecContext::CURRENT->databaseAuthLevel();
    AuthLevel level2 = auth->canUseCollection(ExecContext::CURRENT->user(),
                                              ExecContext::CURRENT->database(),
                                              collection->name());
    if (level != AuthLevel::RW || level2 != AuthLevel::RW) {
      TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_FORBIDDEN,
                                     "Insufficient rights to drop collection");
    }
  }

  PREVENT_EMBEDDED_TRANSACTION();

  std::string const dbname = collection->dbName();
  std::string const collName = collection->name();

  // If we are a coordinator in a cluster, we have to behave differently:
  if (ServerState::instance()->isCoordinator()) {
#ifdef USE_ENTERPRISE
    DropVocbaseColCoordinatorEnterprise(args, collection);
#else
    DropVocbaseColCoordinator(args, collection);
#endif
  } else {
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

    int res = collection->vocbase()->dropCollection(collection, allowDropSystem, timeout);
    if (res != TRI_ERROR_NO_ERROR) {
      TRI_V8_THROW_EXCEPTION_MESSAGE(res, "cannot drop collection");
    }
  }

  if (ServerState::instance()->isCoordinator() ||
      !ServerState::instance()->isRunningInCluster()) {
    auth->authInfo()->enumerateUsers([&](AuthUserEntry& entry) {
      entry.removeCollection(dbname, collName);
    });
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

  arangodb::LogicalCollection* collection =
      TRI_UnwrapClass<arangodb::LogicalCollection>(args.Holder(),
                                                   WRP_VOCBASE_COL_TYPE);

  if (collection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  SingleCollectionTransaction trx(transaction::V8Context::Create(collection->vocbase(), true), collection->cid(),
                                  AccessMode::Type::READ);
  Result res = trx.begin();

  if (!res.ok()) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  std::shared_ptr<VPackBuilder> builder = collection->figures();

  trx.finish(TRI_ERROR_NO_ERROR);

  v8::Handle<v8::Value> result = TRI_VPackToV8(isolate, builder->slice());

  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock assumeLeadership
////////////////////////////////////////////////////////////////////////////////

static void JS_SetTheLeader(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  TRI_vocbase_t* vocbase = GetContextVocBase(isolate);

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  if (ServerState::instance()->isDBServer()) {
    arangodb::LogicalCollection const* v8Collection =
        TRI_UnwrapClass<arangodb::LogicalCollection>(args.Holder(),
                                                     WRP_VOCBASE_COL_TYPE);

    if (v8Collection == nullptr) {
      TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
    }

    TRI_vocbase_t* vocbase = v8Collection->vocbase();
    if (vocbase == nullptr) {
      TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
    }

    std::string collectionName = v8Collection->name();
    auto collection = vocbase->lookupCollection(collectionName);
    if (collection == nullptr) {
      TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
    }
    std::string theLeader;
    if (args.Length() >= 1 && args[0]->IsString()) {
      TRI_Utf8ValueNFC l(TRI_UNKNOWN_MEM_ZONE, args[0]);
      theLeader = std::string(*l, l.length());
    }
    collection->followers()->setTheLeader(theLeader);
    if (theLeader.empty()) {
      collection->followers()->clear();
    }
    // do not reset followers when we resign at this time...we are
    // still the only source of truth to trust, in particular, in the
    // planned leader resignation, we will shortly after the call to
    // this function here report the controlled resignation to the
    // agency. This report must still contain the correct follower list
    // or else the supervision is super angry with us.
  }

  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock getLeader
////////////////////////////////////////////////////////////////////////////////

static void JS_GetLeader(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  TRI_vocbase_t* vocbase = GetContextVocBase(isolate);

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  std::string theLeader;
  if (ServerState::instance()->isDBServer()) {
    arangodb::LogicalCollection const* collection =
        TRI_UnwrapClass<arangodb::LogicalCollection>(args.Holder(),
                                                     WRP_VOCBASE_COL_TYPE);

    if (collection == nullptr) {
      TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
    }

    TRI_vocbase_t* vocbase = collection->vocbase();
    std::string collectionName = collection->name();
    if (vocbase == nullptr) {
      TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
    }

    auto realCollection = vocbase->lookupCollection(collectionName);
    if (realCollection == nullptr) {
      TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
    }
    theLeader = realCollection->followers()->getLeader();
  }

  v8::Handle<v8::String> res = TRI_V8_STD_STRING2(isolate, theLeader);
  TRI_V8_RETURN(res);
  TRI_V8_TRY_CATCH_END
}

#ifdef DEBUG_SYNC_REPLICATION

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock getFollowers
////////////////////////////////////////////////////////////////////////////////

static void JS_AddFollower(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  TRI_vocbase_t* vocbase = GetContextVocBase(isolate);

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  if (args.Length() < 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("addFollower(<name>)");
  }

  ServerID const serverId = TRI_ObjectToString(args[0]);

  if (ServerState::instance()->isDBServer()) {
    arangodb::LogicalCollection const* v8Collection =
        TRI_UnwrapClass<arangodb::LogicalCollection>(args.Holder(),
                                                     WRP_VOCBASE_COL_TYPE);

    if (v8Collection == nullptr) {
      TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
    }

    TRI_vocbase_t* vocbase = v8Collection->vocbase();
    if (vocbase == nullptr) {
      TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
    }

    std::string collectionName = v8Collection->name();
    auto collection = vocbase->lookupCollection(collectionName);
    if (collection == nullptr) {
      TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
    }
    collection->followers()->add(serverId);
  }

  TRI_V8_RETURN_TRUE();
  TRI_V8_TRY_CATCH_END
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock removeFollower
////////////////////////////////////////////////////////////////////////////////

static void JS_RemoveFollower(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  TRI_vocbase_t* vocbase = GetContextVocBase(isolate);

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  if (args.Length() < 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("removeFollower(<name>)");
  }

  ServerID const serverId = TRI_ObjectToString(args[0]);

  if (ServerState::instance()->isDBServer()) {
    arangodb::LogicalCollection const* v8Collection =
        TRI_UnwrapClass<arangodb::LogicalCollection>(args.Holder(),
                                                     WRP_VOCBASE_COL_TYPE);

    if (v8Collection == nullptr) {
      TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
    }

    TRI_vocbase_t* vocbase = v8Collection->vocbase();
    if (vocbase == nullptr) {
      TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
    }

    std::string collectionName = v8Collection->name();
    auto collection = vocbase->lookupCollection(collectionName);
    if (collection == nullptr) {
      TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
    }
    collection->followers()->remove(serverId);
  }

  TRI_V8_RETURN_TRUE();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock getFollowers
////////////////////////////////////////////////////////////////////////////////

static void JS_GetFollowers(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  TRI_vocbase_t* vocbase = GetContextVocBase(isolate);

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  v8::Handle<v8::Array> list = v8::Array::New(isolate);
  if (ServerState::instance()->isDBServer()) {
    arangodb::LogicalCollection const* v8Collection =
        TRI_UnwrapClass<arangodb::LogicalCollection>(args.Holder(),
                                                     WRP_VOCBASE_COL_TYPE);

    if (v8Collection == nullptr) {
      TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
    }

    TRI_vocbase_t* vocbase = v8Collection->vocbase();
    if (vocbase == nullptr) {
      TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
    }

    std::string collectionName = v8Collection->name();
    auto collection = vocbase->lookupCollection(collectionName);
    if (collection == nullptr) {
      TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
    }
    std::unique_ptr<arangodb::FollowerInfo> const& followerInfo = collection->followers();
    std::shared_ptr<std::vector<ServerID> const> followers = followerInfo->get();
    uint32_t i = 0;
    for (auto const& n : *followers) {
      list->Set(i++, TRI_V8_STD_STRING(n));
    }
  }

  TRI_V8_RETURN(list);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock collectionLoad
////////////////////////////////////////////////////////////////////////////////

static void JS_LoadVocbaseCol(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  arangodb::LogicalCollection const* collection =
      TRI_UnwrapClass<arangodb::LogicalCollection>(args.Holder(),
                                                   WRP_VOCBASE_COL_TYPE);

  if (collection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  if (ServerState::instance()->isCoordinator()) {
    int res =
#ifdef USE_ENTERPRISE
      ULVocbaseColCoordinatorEnterprise(
        collection->dbName(), collection->cid_as_string(),
        TRI_VOC_COL_STATUS_LOADED);
#else
      ULVocbaseColCoordinator(
        collection->dbName(), collection->cid_as_string(),
        TRI_VOC_COL_STATUS_LOADED);
#endif
    if (res != TRI_ERROR_NO_ERROR) {
      TRI_V8_THROW_EXCEPTION(res);
    }
    TRI_V8_RETURN_UNDEFINED();
  }

  SingleCollectionTransaction trx(
    transaction::V8Context::Create(collection->vocbase(), true),
    collection->cid(), AccessMode::Type::READ);

  Result res = trx.begin();

  if (!res.ok()) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  res = trx.finish(res);

  if (!res.ok()) {
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

  arangodb::LogicalCollection const* collection =
      TRI_UnwrapClass<arangodb::LogicalCollection>(args.Holder(),
                                                   WRP_VOCBASE_COL_TYPE);

  if (collection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  std::string const collectionName(collection->name());

  if (collectionName.empty()) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
  }
  v8::Handle<v8::Value> result = TRI_V8_STD_STRING(collectionName);
  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the path of a collection
////////////////////////////////////////////////////////////////////////////////

static void JS_PathVocbaseCol(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  arangodb::LogicalCollection const* collection =
      TRI_UnwrapClass<arangodb::LogicalCollection>(args.Holder(),
                                                   WRP_VOCBASE_COL_TYPE);

  if (collection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  std::string const path(collection->getPhysical()->path());
  v8::Handle<v8::Value> result = TRI_V8_STD_STRING(path);

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

  arangodb::LogicalCollection const* collection =
      TRI_UnwrapClass<arangodb::LogicalCollection>(args.Holder(),
                                                   WRP_VOCBASE_COL_TYPE);

  if (collection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  if (ServerState::instance()->isCoordinator()) {
    TRI_V8_RETURN(TRI_V8UInt64String<TRI_voc_cid_t>(isolate, collection->cid()));
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


  arangodb::LogicalCollection* collection =
  TRI_UnwrapClass<arangodb::LogicalCollection>(args.Holder(), WRP_VOCBASE_COL_TYPE);

  if (collection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  bool const isModification = (args.Length() != 0);
  if (ExecContext::CURRENT != nullptr) {
    AuthenticationFeature *auth = AuthenticationFeature::INSTANCE;
    auto level = ExecContext::CURRENT->databaseAuthLevel();
    auto level2 = auth->canUseCollection(ExecContext::CURRENT->user(),
                                         ExecContext::CURRENT->database(),
                                         collection->name());
    if ((isModification && (level != AuthLevel::RW || level2 != AuthLevel::RW)) ||
        level == AuthLevel::NONE || level2 == AuthLevel::NONE) {
      TRI_V8_THROW_EXCEPTION(TRI_ERROR_FORBIDDEN);
    }
  }

  if (ServerState::instance()->isCoordinator()) {
    std::string const databaseName(collection->dbName());
    std::shared_ptr<LogicalCollection> info =
        ClusterInfo::instance()->getCollection(
            databaseName, collection->cid_as_string());
    if (0 < args.Length()) {
      v8::Handle<v8::Value> par = args[0];

      if (par->IsObject()) {
        VPackBuilder builder;
        {
          int res = TRI_V8ToVPack(isolate, builder, args[0], false);

          if (res != TRI_ERROR_NO_ERROR) {
            TRI_V8_THROW_EXCEPTION(res);
          }
        }

        VPackSlice const slice = builder.slice();

        arangodb::Result res = info->updateProperties(slice, false);

        if (!res.ok()) {
          TRI_V8_THROW_EXCEPTION_MESSAGE(res.errorNumber(), res.errorMessage());
        }
      }

    }

    auto c = ClusterInfo::instance()->getCollection(
        databaseName, StringUtils::itoa(collection->cid()));

    std::unordered_set<std::string> const ignoreKeys{
        "allowUserKeys", "cid",  "count",  "deleted", "id",
        "indexes",       "name", "path",   "planId",  "shards",
        "status",        "type", "version"};
    VPackBuilder vpackProperties = c->toVelocyPackIgnore(ignoreKeys, true, false);

    // return the current parameter set
    v8::Handle<v8::Object> result =
                  TRI_VPackToV8(isolate, vpackProperties.slice())->ToObject();
    TRI_V8_RETURN(result);
  }

  SingleCollectionTransaction trx(
      transaction::V8Context::Create(collection->vocbase(), true),
      collection->cid(),
      isModification ? AccessMode::Type::EXCLUSIVE : AccessMode::Type::READ);

  if (!isModification) {
    trx.addHint(transaction::Hints::Hint::NO_USAGE_LOCK);
  }

  Result res = trx.begin();

  if (!res.ok()) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  // check if we want to change some parameters
  if (isModification) {
    v8::Handle<v8::Value> par = args[0];

    if (par->IsObject()) {
      VPackBuilder builder;
      int res = TRI_V8ToVPack(isolate, builder, args[0], false);

      if (res != TRI_ERROR_NO_ERROR) {
        TRI_V8_THROW_EXCEPTION(res);
      }

      VPackSlice const slice = builder.slice();

      // try to write new parameter to file
      bool doSync = application_features::ApplicationServer::getFeature<DatabaseFeature>("Database")->forceSyncProperties();
      arangodb::Result updateRes = collection->updateProperties(slice, doSync);

      if (!updateRes.ok()) {
        TRI_V8_THROW_EXCEPTION_MESSAGE(updateRes.errorNumber(), updateRes.errorMessage());
      }

      auto physical = collection->getPhysical();
      TRI_ASSERT(physical != nullptr);
      arangodb::Result res2 = physical->persistProperties();
      // TODO Review
      // TODO API compatibility, for now we ignore if persisting fails...
    }
  }

  std::unordered_set<std::string> const ignoreKeys{
      "allowUserKeys", "cid", "count", "deleted", "id", "indexes", "name",
      "path", "planId", "shards", "status", "type", "version",
      /* These are only relevant for cluster */
      "distributeShardsLike", "isSmart", "numberOfShards", "replicationFactor",
      "shardKeys"};
  VPackBuilder vpackProperties = collection->toVelocyPackIgnore(ignoreKeys, true, false);

  // return the current parameter set
  v8::Handle<v8::Object> result =
                TRI_VPackToV8(isolate, vpackProperties.slice())->ToObject();

  trx.finish(res);

  TRI_V8_RETURN(result);
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
/// @brief helper function to rename collections in _graphs as well
////////////////////////////////////////////////////////////////////////////////

static int RenameGraphCollections(v8::Isolate* isolate,
                                  std::string const& oldName,
                                  std::string const& newName) {
  v8::HandleScope scope(isolate);

  StringBuffer buffer(TRI_UNKNOWN_MEM_ZONE);
  buffer.appendText("require('@arangodb/general-graph')._renameCollection(");
  buffer.appendJsonEncoded(oldName.c_str(), oldName.size());
  buffer.appendChar(',');
  buffer.appendJsonEncoded(newName.c_str(), newName.size());
  buffer.appendText(");");

  TRI_ExecuteJavaScriptString(
      isolate, isolate->GetCurrentContext(),
      TRI_V8_ASCII_PAIR_STRING(buffer.c_str(), buffer.length()),
      TRI_V8_ASCII_STRING("collection rename"), false);

  return TRI_ERROR_NO_ERROR;
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

  if (ServerState::instance()->isCoordinator()) {
    // renaming a collection in a cluster is unsupported
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_CLUSTER_UNSUPPORTED);
  }

  std::string const name = TRI_ObjectToString(args[0]);
  if (ExecContext::CURRENT != nullptr) {
    AuthenticationFeature* auth = AuthenticationFeature::INSTANCE;
    TRI_ASSERT(auth != nullptr);
    AuthLevel level = ExecContext::CURRENT->databaseAuthLevel();
    AuthLevel level2 = auth->canUseCollection(ExecContext::CURRENT->user(),
                                              ExecContext::CURRENT->database(),
                                              name);
    if (level != AuthLevel::RW || level2 != AuthLevel::RW) {
      TRI_V8_THROW_EXCEPTION(TRI_ERROR_FORBIDDEN);
    }
  }

  // second parameter "override" is to override renaming restrictions, e.g.
  // renaming from a system collection name to a non-system collection name and
  // vice versa. this parameter is not publicly exposed but used internally
  bool doOverride = false;
  if (args.Length() > 1) {
    doOverride = TRI_ObjectToBoolean(args[1]);
  }

  if (name.empty()) {
    TRI_V8_THROW_EXCEPTION_PARAMETER("<name> must be non-empty");
  }

  arangodb::LogicalCollection* collection =
      TRI_UnwrapClass<arangodb::LogicalCollection>(args.Holder(), WRP_VOCBASE_COL_TYPE);

  if (collection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  PREVENT_EMBEDDED_TRANSACTION();

  if (ServerState::instance()->isCoordinator()) {
    // renaming a collection in a cluster is unsupported
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_CLUSTER_UNSUPPORTED);
  }

  std::string const oldName(collection->name());

  int res =
      collection->vocbase()->renameCollection(collection, name, doOverride);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(res, "cannot rename collection");
  }

  // rename collection inside _graphs as well
  RenameGraphCollections(isolate, oldName, name);

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
  arangodb::LogicalCollection const* col =
      TRI_UnwrapClass<arangodb::LogicalCollection>(args.Holder(), WRP_VOCBASE_COL_TYPE);
  if (col == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }
  TRI_vocbase_t* vocbase = col->vocbase();
  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  std::string collectionName = col->name();


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


  auto transactionContext = std::make_shared<transaction::V8Context>(vocbase, true);

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
  res = trx.finish(opResult.code);

  if (!opResult.successful()) {
    TRI_V8_THROW_EXCEPTION(opResult.code);
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

  LogicalCollection const* col = nullptr;
  std::string collectionName;

  TRI_vocbase_t* vocbase = GetContextVocBase(isolate);

  auto transactionContext = std::make_shared<transaction::V8Context>(vocbase, true);

  VPackBuilder updateBuilder;

  {
    VPackObjectBuilder guard(&updateBuilder);
    int res = V8ToVPackNoKeyRevId(isolate, updateBuilder, args[1]);
    if (res != TRI_ERROR_NO_ERROR) {
      TRI_V8_THROW_EXCEPTION(res);
    }

    res = ParseDocumentOrDocumentHandle(
        isolate, vocbase, transactionContext->getResolver(), col,
        collectionName, updateBuilder, !options.ignoreRevs, args[0]);
    if (res != TRI_ERROR_NO_ERROR) {
      TRI_V8_THROW_EXCEPTION(res);
    }
  }

  // We need to free the collection object in the end
  LocalCollectionGuard g(const_cast<LogicalCollection*>(col));

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

  res = trx.finish(opResult.code);

  if (!opResult.successful()) {
    TRI_V8_THROW_EXCEPTION(opResult.code);
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
      TRI_V8_THROW_EXCEPTION_USAGE(
                                   "_pregelStart(<algorithm>, <vertexCollections>,"
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

  // now check the access rights to collections
  if (ExecContext::CURRENT != nullptr) {
    VPackSlice storeSlice = paramBuilder.slice().get("store");
    bool storeResults = !storeSlice.isBool() || storeSlice.getBool();
    AuthenticationFeature *auth = AuthenticationFeature::INSTANCE;
    for (std::string const& ec : paramVertices) {
      AuthLevel lvl = auth->canUseCollection(ExecContext::CURRENT->user(),
                                             ExecContext::CURRENT->database(), ec);
      if ((storeResults && lvl != AuthLevel::RW) || lvl == AuthLevel::NONE) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_FORBIDDEN);
      }
    }
    for (std::string const& ec : paramEdges) {
      AuthLevel lvl = auth->canUseCollection(ExecContext::CURRENT->user(),
                                             ExecContext::CURRENT->database(), ec);
      if ((storeResults && lvl != AuthLevel::RW) || lvl == AuthLevel::NONE) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_FORBIDDEN);
      }
    }
  }

  TRI_vocbase_t* vocbase = GetContextVocBase(isolate);
  for (std::string const& name : paramVertices) {
    if (ss->isCoordinator()) {
      try {
        auto coll =
        ClusterInfo::instance()->getCollection(vocbase->name(), name);
        if (coll->isSystem()) {
          TRI_V8_THROW_EXCEPTION_USAGE(
                                       "Cannot use pregel on system collection");
        }
        if (coll->status() == TRI_VOC_COL_STATUS_DELETED || coll->deleted()) {
          TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND, name);
        }
      } catch (...) {
        TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND, name);
      }
    } else  if (ss->getRole() == ServerState::ROLE_SINGLE) {
      LogicalCollection *coll = vocbase->lookupCollection(name);
      if (coll == nullptr || coll->status() == TRI_VOC_COL_STATUS_DELETED
          || coll->deleted()) {
        TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND, name);
      }
    } else {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
    }
  }

  std::vector<CollectionID> edgeColls;
  // load edge collection
  for (std::string const& name : paramEdges) {
    if (ss->isCoordinator()) {
      try {
        auto coll =
        ClusterInfo::instance()->getCollection(vocbase->name(), name);
        if (coll->isSystem()) {
          TRI_V8_THROW_EXCEPTION_USAGE(
                                       "Cannot use pregel on system collection");
        }
        if (!coll->isSmart()) {
          std::vector<std::string> eKeys = coll->shardKeys();
          if ( eKeys.size() != 1 || eKeys[0] != "vertex") {
            TRI_V8_THROW_EXCEPTION_USAGE(
                                         "Edge collection needs to be sharded after 'vertex', or use "
                                         "smart graphs");
          }
        }
        if (coll->status() == TRI_VOC_COL_STATUS_DELETED || coll->deleted()) {
          TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND, name);
        }
        // smart edge collections contain multiple actual collections
        std::vector<std::string> actual = coll->realNamesForRead();
        edgeColls.insert(edgeColls.end(), actual.begin(), actual.end());
      } catch (...) {
        TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND, name);
      }
    } else if (ss->getRole() == ServerState::ROLE_SINGLE) {
      LogicalCollection *coll = vocbase->lookupCollection(name);
      if (coll == nullptr || coll->deleted()) {
        TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND, name);
      }
      std::vector<std::string> actual = coll->realNamesForRead();
      edgeColls.insert(edgeColls.end(), actual.begin(), actual.end());
    } else {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
    }
  }

  uint64_t en = pregel::PregelFeature::instance()->createExecutionNumber();
  auto c = std::make_unique<pregel::Conductor>(en, vocbase, paramVertices, edgeColls,
                                               algorithm, paramBuilder.slice());
  pregel::PregelFeature::instance()->addConductor(c.get(), en);
  c->start();
  c.release();

  TRI_V8_RETURN(v8::Number::New(isolate, static_cast<double>(en)));
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
    TRI_V8_THROW_EXCEPTION_USAGE("_pregelStatus(<executionNum>]");
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
    TRI_V8_THROW_EXCEPTION_USAGE("_pregelStatus(<executionNum>]");
  }

  uint64_t executionNum = TRI_ObjectToUInt64(args[0], true);
  if (ServerState::instance()->isCoordinator()) {
    auto c = pregel::PregelFeature::instance()->conductor(executionNum);
    if (!c) {
      TRI_V8_THROW_EXCEPTION_USAGE("Execution number is invalid");
    }

    VPackBuilder docs = c->collectAQLResults();
    TRI_ASSERT(docs.slice().isArray());
    VPackOptions resultOptions = VPackOptions::Defaults;
    auto documents = TRI_VPackToV8(isolate, docs.slice(), &resultOptions);
    TRI_V8_RETURN(documents);
  } else {
    TRI_V8_THROW_EXCEPTION_USAGE("Only valid on the conductor");
  }

  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief fetch the revision for a local collection
////////////////////////////////////////////////////////////////////////////////

static int GetRevision(arangodb::LogicalCollection* collection, TRI_voc_rid_t& rid) {
  TRI_ASSERT(collection != nullptr);

  SingleCollectionTransaction trx(
      transaction::V8Context::Create(collection->vocbase(), true),
      collection->cid(), AccessMode::Type::READ);

  Result res = trx.begin();

  if (!res.ok()) {
    return res.errorNumber();
  }

  rid = collection->revision(&trx);
  trx.finish(res);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief fetch the revision for a sharded collection
////////////////////////////////////////////////////////////////////////////////

static int GetRevisionCoordinator(arangodb::LogicalCollection* collection,
                                  TRI_voc_rid_t& rid) {
  TRI_ASSERT(collection != nullptr);

  std::string const databaseName(collection->dbName());
  std::string const cid = collection->cid_as_string();

  return revisionOnCoordinator(databaseName, cid, rid);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock collectionRevision
////////////////////////////////////////////////////////////////////////////////

static void JS_RevisionVocbaseCol(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  arangodb::LogicalCollection* collection =
      TRI_UnwrapClass<arangodb::LogicalCollection>(args.Holder(), WRP_VOCBASE_COL_TYPE);

  if (collection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  TRI_voc_rid_t revisionId;
  int res;

  if (ServerState::instance()->isCoordinator()) {
    res = GetRevisionCoordinator(collection, revisionId);
  } else {
    res = GetRevision(collection, revisionId);
  }

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  std::string ridString = TRI_RidToString(revisionId);
  TRI_V8_RETURN(TRI_V8_STD_STRING(ridString));
  TRI_V8_TRY_CATCH_END
}


////////////////////////////////////////////////////////////////////////////////
/// @brief retrieves a collection from a V8 argument
////////////////////////////////////////////////////////////////////////////////

static arangodb::LogicalCollection* GetCollectionFromArgument(
    TRI_vocbase_t* vocbase, v8::Handle<v8::Value> const val) {
  // number
  if (val->IsNumber() || val->IsNumberObject()) {
    uint64_t cid = TRI_ObjectToUInt64(val, true);
    return vocbase->lookupCollection(cid);
  }

  return vocbase->lookupCollection(TRI_ObjectToString(val));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief __save(collection, document). This method is used internally and not
/// part of the public API
////////////////////////////////////////////////////////////////////////////////

static void JS_SaveVocbase(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  TRI_vocbase_t* vocbase = GetContextVocBase(isolate);

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  if (vocbase->isDropped()) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  // expecting two arguments
  if (args.Length() != 2) {
    TRI_V8_THROW_EXCEPTION_USAGE("__save(<collection-name>, <doc>)");
  }

  v8::Handle<v8::Value> val = args[0];
  if (!val->IsString()) {
    // invalid value type. Collection Name must be a string
    TRI_V8_THROW_EXCEPTION_PARAMETER("<collection-name> must be a string");
  }
  std::string const collectionName = TRI_ObjectToString(val);

  // We cannot give any options here. Use default.
  OperationOptions options;
  VPackBuilder builder;

  v8::Handle<v8::Value> doc = args[1];

  if (!doc->IsObject()) {
    // invalid value type. must be a document
    TRI_V8_THROW_EXCEPTION_PARAMETER("<doc> must be a document");
  }

  Result res = TRI_V8ToVPack(isolate, builder, doc, false);

  if (!res.ok()) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  // load collection
  auto transactionContext(transaction::V8Context::Create(vocbase, true));
  SingleCollectionTransaction trx(transactionContext,
                                  collectionName, AccessMode::Type::WRITE);
  trx.addHint(transaction::Hints::Hint::SINGLE_OPERATION);

  res = trx.begin();

  if (!res.ok()) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  OperationResult result = trx.insert(collectionName, builder.slice(), options);

  res = trx.finish(result.code);

  if (!res.ok()) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  VPackSlice resultSlice = result.slice();

  TRI_V8_RETURN(TRI_VPackToV8(isolate, resultSlice,
                              transactionContext->getVPackOptions()));
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief inserts a document, using VPack
////////////////////////////////////////////////////////////////////////////////

static void InsertVocbaseCol(v8::Isolate* isolate,
                             v8::FunctionCallbackInfo<v8::Value> const& args,
                             std::string* attachment) {
  v8::HandleScope scope(isolate);

  auto collection = TRI_UnwrapClass<arangodb::LogicalCollection>(
      args.Holder(), WRP_VOCBASE_COL_TYPE);

  if (collection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  bool const isEdgeCollection =
      ((TRI_col_type_e)collection->type() == TRI_COL_TYPE_EDGE);

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
    TRI_GET_GLOBAL_STRING(SilentKey);
    if (optionsObject->Has(SilentKey)) {
      options.silent = TRI_ObjectToBoolean(optionsObject->Get(SilentKey));
    }
    TRI_GET_GLOBAL_STRING(ReturnNewKey);
    if (optionsObject->Has(ReturnNewKey)) {
      options.returnNew = TRI_ObjectToBoolean(optionsObject->Get(ReturnNewKey));
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

  SingleCollectionTransaction trx(transactionContext, collection->cid(),
                                  AccessMode::Type::WRITE);
  if (!payloadIsArray) {
    trx.addHint(transaction::Hints::Hint::SINGLE_OPERATION);
  }

  Result res = trx.begin();

  if (!res.ok()) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  OperationResult result =
      trx.insert(collection->name(), builder.slice(), options);

  res = trx.finish(Result(result.code, result.errorMessage));

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
/// @brief returns the status of a collection
////////////////////////////////////////////////////////////////////////////////

static void JS_StatusVocbaseCol(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  arangodb::LogicalCollection* collection =
      TRI_UnwrapClass<arangodb::LogicalCollection>(args.Holder(), WRP_VOCBASE_COL_TYPE);

  if (collection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  if (ServerState::instance()->isCoordinator()) {
    std::string const databaseName(collection->dbName());

    try {
      std::shared_ptr<LogicalCollection> const ci =
          ClusterInfo::instance()->getCollection(databaseName,
                                                 collection->cid_as_string());
      TRI_V8_RETURN(v8::Number::New(isolate, (int)ci->getStatusLocked()));
    } catch (...) {
      TRI_V8_RETURN(v8::Number::New(isolate, (int)TRI_VOC_COL_STATUS_DELETED));
    }
  }
  // fallthru intentional

  TRI_vocbase_col_status_e status = collection->getStatusLocked();

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

  arangodb::LogicalCollection* collection =
      TRI_UnwrapClass<arangodb::LogicalCollection>(args.Holder(), WRP_VOCBASE_COL_TYPE);

  if (collection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  // Manually check this here, because truncate messes up the return code
  AuthenticationFeature* auth = FeatureCacheFeature::instance()->authenticationFeature();
  if (auth->isActive() && ExecContext::CURRENT != nullptr) {
    CollectionNameResolver resolver(collection->vocbase());
    std::string const cName = resolver.getCollectionNameCluster(collection->cid());
    AuthLevel level = auth->canUseCollection(ExecContext::CURRENT->user(),
                                             collection->vocbase()->name(), cName);
    if (level != AuthLevel::RW) {
      TRI_V8_THROW_EXCEPTION(TRI_ERROR_FORBIDDEN);
    }
  }

  SingleCollectionTransaction trx(
      transaction::V8Context::Create(collection->vocbase(), true),
                                  collection->cid(), AccessMode::Type::EXCLUSIVE);

  Result res = trx.begin();
  if (!res.ok()) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  OperationResult result = trx.truncate(collection->name(), opOptions);

  res = trx.finish(result.code);

  if (result.failed()) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(result.code, result.errorMessage);
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

  arangodb::LogicalCollection* collection =
      TRI_UnwrapClass<arangodb::LogicalCollection>(args.Holder(), WRP_VOCBASE_COL_TYPE);

  if (collection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  if (ServerState::instance()->isCoordinator()) {
    std::string const databaseName = collection->dbName();

    try {
      std::shared_ptr<LogicalCollection> const ci =
          ClusterInfo::instance()->getCollection(databaseName,
                                                 collection->cid_as_string());
      TRI_V8_RETURN(v8::Number::New(isolate, (int)ci->type()));
    } catch (...) {
      TRI_V8_RETURN(v8::Number::New(isolate, (int)collection->type()));
    }
  }
  // fallthru intentional

  TRI_col_type_e type = collection->type();

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

  arangodb::LogicalCollection* collection =
      TRI_UnwrapClass<arangodb::LogicalCollection>(args.Holder(), WRP_VOCBASE_COL_TYPE);

  if (collection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  int res;

  if (ServerState::instance()->isCoordinator()) {

    res =
#ifdef USE_ENTERPRISE
      ULVocbaseColCoordinatorEnterprise(
        collection->dbName(), collection->cid_as_string(),
        TRI_VOC_COL_STATUS_UNLOADED);
#else
      ULVocbaseColCoordinator(
        collection->dbName(), collection->cid_as_string(),
        TRI_VOC_COL_STATUS_UNLOADED);
#endif

  } else {
    res = collection->vocbase()->unloadCollection(collection, false);
  }

  if (res != TRI_ERROR_NO_ERROR) {
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

  arangodb::LogicalCollection* collection =
      TRI_UnwrapClass<arangodb::LogicalCollection>(args.Holder(), WRP_VOCBASE_COL_TYPE);

  if (collection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  TRI_V8_RETURN(v8::Number::New(isolate, collection->version()));
  TRI_V8_TRY_CATCH_END
}

static void JS_ChangeOperationModeVocbase(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  TRI_GET_GLOBALS();

  bool allowModeChange = false;
  TRI_GET_GLOBAL(_currentRequest, v8::Value);
  if (_currentRequest.IsEmpty() || _currentRequest->IsUndefined()) {
    // console mode
    allowModeChange = true;
  } else if (_currentRequest->IsObject()) {
    v8::Handle<v8::Object> obj = v8::Handle<v8::Object>::Cast(_currentRequest);

    TRI_GET_GLOBAL_STRING(PortTypeKey);
    if (obj->Has(PortTypeKey)) {
      std::string const portType = TRI_ObjectToString(obj->Get(PortTypeKey));
      if (portType == "unix") {
        allowModeChange = true;
      }
    }
  }

  if (!allowModeChange) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_FORBIDDEN);
  }

  // expecting one argument
  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "_changeMode(<mode>), with modes: 'Normal', 'NoCreate'");
  }

  std::string const newModeStr = TRI_ObjectToString(args[0]);

  TRI_vocbase_operationmode_e newMode = TRI_VOCBASE_MODE_NORMAL;

  if (newModeStr == "NoCreate") {
    newMode = TRI_VOCBASE_MODE_NO_CREATE;
  } else if (newModeStr != "Normal") {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "illegal mode, allowed modes are: 'Normal' and 'NoCreate'");
  }

  TRI_ChangeOperationModeServer(newMode);

  TRI_V8_RETURN_TRUE();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock collectionDatabaseName
////////////////////////////////////////////////////////////////////////////////

static void JS_CollectionVocbase(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  TRI_vocbase_t* vocbase = GetContextVocBase(isolate);

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  if (vocbase->isDropped()) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  // expecting one argument
  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("_collection(<name>|<identifier>)");
  }

  v8::Handle<v8::Value> val = args[0];
  arangodb::LogicalCollection const* collection = nullptr;

  std::string const name = TRI_ObjectToString(val);
  if (ServerState::instance()->isCoordinator()) {
    try {
      std::shared_ptr<LogicalCollection> const ci =
          ClusterInfo::instance()->getCollection(vocbase->name(), name);
      auto colCopy = ci->clone();
      collection = colCopy.release();
    } catch (...) {
      // not found
      TRI_V8_RETURN_NULL();
    }
  } else {
    collection = GetCollectionFromArgument(vocbase, val);
  }

  if (collection == nullptr) {
    TRI_V8_RETURN_NULL();
  }

  AuthenticationFeature* auth = AuthenticationFeature::INSTANCE;
  if (ExecContext::CURRENT != nullptr && auth != nullptr) {
    AuthLevel level = auth->canUseCollection(ExecContext::CURRENT->user(),
                                             ExecContext::CURRENT->database(),
                                             name);
    if (level == AuthLevel::NONE) {
      TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_FORBIDDEN,
                                     "No access to collection");
    }
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

  TRI_vocbase_t* vocbase = GetContextVocBase(isolate);

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  std::vector<LogicalCollection*> colls;

  // clean memory
  std::function<void()> cleanup;

  // if we are a coordinator, we need to fetch the collection info from the
  // agency
  if (ServerState::instance()->isCoordinator()) {
    cleanup = [&colls]() {
      for (auto& it : colls) {
        if (it != nullptr) {
          delete it;
        }
      }
    };
    colls = GetCollectionsCluster(vocbase);
  } else {
    // no cleanup needed on single server / dbserver
    cleanup = []() {};
    colls = vocbase->collections(false);
  }

  // make sure memory is cleaned up
  TRI_DEFER(cleanup());

  std::sort(colls.begin(), colls.end(), [](LogicalCollection* lhs, LogicalCollection* rhs) -> bool {
    return StringUtils::tolower(lhs->name()) < StringUtils::tolower(rhs->name());
  });

  AuthenticationFeature* auth = FeatureCacheFeature::instance()->authenticationFeature();
  bool error = false;

  // already create an array of the correct size
  v8::Handle<v8::Array> result = v8::Array::New(isolate);
  size_t const n = colls.size();
  size_t x = 0;
  for (size_t i = 0; i < n; ++i) {
    auto& collection = colls[i];

    if (auth->isActive() && ExecContext::CURRENT != nullptr) {
      AuthLevel level = auth->canUseCollection(ExecContext::CURRENT->user(),
                             vocbase->name(), collection->name());
      if (level == AuthLevel::NONE) {
        continue;
      }
    }

    v8::Handle<v8::Value> c = WrapCollection(isolate, collection);
    if (c.IsEmpty()) {
      error = true;
      break;
    }
    // avoid duplicate deletion
    collection = nullptr;
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

  TRI_vocbase_t* vocbase = GetContextVocBase(isolate);

  if (vocbase == nullptr) {
    TRI_V8_RETURN(v8::Array::New(isolate));
  }

  std::vector<std::string> names;

  if (ServerState::instance()->isCoordinator()) {
    if (ClusterInfo::instance()->doesDatabaseExist(vocbase->name())) {
      names = GetCollectionNamesCluster(vocbase);
    }
  } else {
    names = vocbase->collectionNames();
  }

  uint32_t j = 0;

  v8::Handle<v8::Array> result = v8::Array::New(isolate);
  // add collection names
  for (auto& name : names) {
    result->Set(j++, TRI_V8_STD_STRING(name));
  }

  // add function names. these are hard coded
  result->Set(j++, TRI_V8_ASCII_STRING("_changeMode()"));
  result->Set(j++, TRI_V8_ASCII_STRING("_collection()"));
  result->Set(j++, TRI_V8_ASCII_STRING("_collections()"));
  result->Set(j++, TRI_V8_ASCII_STRING("_create()"));
  result->Set(j++, TRI_V8_ASCII_STRING("_createDatabase()"));
  result->Set(j++, TRI_V8_ASCII_STRING("_createDocumentCollection()"));
  result->Set(j++, TRI_V8_ASCII_STRING("_createEdgeCollection()"));
  result->Set(j++, TRI_V8_ASCII_STRING("_createView()"));
  result->Set(j++, TRI_V8_ASCII_STRING("_createStatement()"));
  result->Set(j++, TRI_V8_ASCII_STRING("_currentWalFiles()"));
  result->Set(j++, TRI_V8_ASCII_STRING("_document()"));
  result->Set(j++, TRI_V8_ASCII_STRING("_drop()"));
  result->Set(j++, TRI_V8_ASCII_STRING("_dropDatabase()"));
  result->Set(j++, TRI_V8_ASCII_STRING("_dropView()"));
  result->Set(j++, TRI_V8_ASCII_STRING("_engine()"));
  result->Set(j++, TRI_V8_ASCII_STRING("_engineStats()"));
  result->Set(j++, TRI_V8_ASCII_STRING("_executeTransaction()"));
  result->Set(j++, TRI_V8_ASCII_STRING("_exists()"));
  result->Set(j++, TRI_V8_ASCII_STRING("_id"));
  result->Set(j++, TRI_V8_ASCII_STRING("_isSystem()"));
  result->Set(j++, TRI_V8_ASCII_STRING("_databases()"));
  result->Set(j++, TRI_V8_ASCII_STRING("_engine()"));
  result->Set(j++, TRI_V8_ASCII_STRING("_name()"));
  result->Set(j++, TRI_V8_ASCII_STRING("_path()"));
  result->Set(j++, TRI_V8_ASCII_STRING("_pregelStart()"));
  result->Set(j++, TRI_V8_ASCII_STRING("_pregelStatus()"));
  result->Set(j++, TRI_V8_ASCII_STRING("_pregelStop()"));
  result->Set(j++, TRI_V8_ASCII_STRING("_query()"));
  result->Set(j++, TRI_V8_ASCII_STRING("_remove()"));
  result->Set(j++, TRI_V8_ASCII_STRING("_replace()"));
  result->Set(j++, TRI_V8_ASCII_STRING("_update()"));
  result->Set(j++, TRI_V8_ASCII_STRING("_useDatabase()"));
  result->Set(j++, TRI_V8_ASCII_STRING("_version()"));
  result->Set(j++, TRI_V8_ASCII_STRING("_view()"));
  result->Set(j++, TRI_V8_ASCII_STRING("_views()"));

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

  arangodb::LogicalCollection const* col =
      TRI_UnwrapClass<arangodb::LogicalCollection>(args.Holder(), WRP_VOCBASE_COL_TYPE);

  if (col == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  if (args.Length() > 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("count()");
  }

  bool details = false;
  if (args.Length() == 1 && ServerState::instance()->isCoordinator()) {
    details = TRI_ObjectToBoolean(args[0]);
  }

  TRI_vocbase_t* vocbase = col->vocbase();

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  std::string collectionName(col->name());

  SingleCollectionTransaction trx(transaction::V8Context::Create(vocbase, true), collectionName, AccessMode::Type::READ);

  Result res = trx.begin();

  if (!res.ok()) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  OperationResult opResult = trx.count(collectionName, !details);
  res = trx.finish(opResult.code);

  if (!res.ok()) {
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

  arangodb::LogicalCollection* collection =
      TRI_UnwrapClass<arangodb::LogicalCollection>(args.Holder(),
                                                   WRP_VOCBASE_COL_TYPE);

  if (collection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  if (ServerState::instance()->isCoordinator()) {
    std::string const databaseName(collection->dbName());
    std::string const cid = collection->cid_as_string();
    int res = warmupOnCoordinator(databaseName, cid);
    if (res != TRI_ERROR_NO_ERROR) {
      TRI_V8_THROW_EXCEPTION(res);
    }

    TRI_V8_RETURN_UNDEFINED();
  }

  SingleCollectionTransaction trx(
      transaction::V8Context::Create(collection->vocbase(), true),
      collection->cid(),
      AccessMode::Type::READ);

  Result trxRes = trx.begin();

  if (!trxRes.ok()) {
    TRI_V8_THROW_EXCEPTION(trxRes);
  }

  auto idxs = collection->getIndexes();
  auto queue = std::make_shared<basics::LocalTaskQueue>();

  for (auto& idx : idxs) {
    idx->warmup(&trx, queue);
  }

  queue->dispatchAndWait();
  if (queue->status() == TRI_ERROR_NO_ERROR) {
    trxRes = trx.commit();
  } else {
    TRI_V8_THROW_EXCEPTION(queue->status());
  }

  if (!trxRes.ok()) {
    TRI_V8_THROW_EXCEPTION(trxRes);
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
  TRI_AddMethodVocbase(isolate, ArangoDBNS, TRI_V8_ASCII_STRING("_changeMode"),
                       JS_ChangeOperationModeVocbase);
  TRI_AddMethodVocbase(isolate, ArangoDBNS, TRI_V8_ASCII_STRING("_collection"),
                       JS_CollectionVocbase);
  TRI_AddMethodVocbase(isolate, ArangoDBNS, TRI_V8_ASCII_STRING("_collections"),
                       JS_CollectionsVocbase);
  TRI_AddMethodVocbase(isolate, ArangoDBNS, TRI_V8_ASCII_STRING("_COMPLETIONS"),
                       JS_CompletionsVocbase, true);
  TRI_AddMethodVocbase(isolate, ArangoDBNS, TRI_V8_ASCII_STRING("_document"),
                       JS_DocumentVocbase);
  TRI_AddMethodVocbase(isolate, ArangoDBNS, TRI_V8_ASCII_STRING("_exists"),
                       JS_ExistsVocbase);
  TRI_AddMethodVocbase(isolate, ArangoDBNS, TRI_V8_ASCII_STRING("_remove"),
                       JS_RemoveVocbase);
  TRI_AddMethodVocbase(isolate, ArangoDBNS, TRI_V8_ASCII_STRING("_replace"),
                       JS_ReplaceVocbase);
  TRI_AddMethodVocbase(isolate, ArangoDBNS, TRI_V8_ASCII_STRING("_update"),
                       JS_UpdateVocbase);
  TRI_AddMethodVocbase(isolate, ArangoDBNS, TRI_V8_ASCII_STRING("_pregelStart"),
                       JS_PregelStart);
  TRI_AddMethodVocbase(isolate, ArangoDBNS,
                       TRI_V8_ASCII_STRING("_pregelStatus"), JS_PregelStatus);
  TRI_AddMethodVocbase(isolate, ArangoDBNS,
                       TRI_V8_ASCII_STRING("_pregelCancel"), JS_PregelCancel);
  TRI_AddMethodVocbase(isolate, ArangoDBNS,
                       TRI_V8_ASCII_STRING("_pregelAqlResult"),
                       JS_PregelAQLResult);

  // an internal API used for storing a document without wrapping a V8
  // collection object
  TRI_AddMethodVocbase(isolate, ArangoDBNS, TRI_V8_ASCII_STRING("__save"),
                       JS_SaveVocbase, true);

  v8::Handle<v8::ObjectTemplate> rt;
  v8::Handle<v8::FunctionTemplate> ft;

  ft = v8::FunctionTemplate::New(isolate);
  ft->SetClassName(TRI_V8_ASCII_STRING("ArangoCollection"));

  rt = ft->InstanceTemplate();
  rt->SetInternalFieldCount(3);

  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("count"),
                       JS_CountVocbaseCol);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("document"),
                       JS_DocumentVocbaseCol);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("_binaryDocument"),
                       JS_BinaryDocumentVocbaseCol);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("drop"),
                       JS_DropVocbaseCol);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("exists"),
                       JS_ExistsVocbaseVPack);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("figures"),
                       JS_FiguresVocbaseCol);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("insert"),
                       JS_InsertVocbaseCol);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("_binaryInsert"),
                       JS_BinaryInsertVocbaseCol);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("setTheLeader"),
                       JS_SetTheLeader, true);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("getLeader"),
                       JS_GetLeader, true);
#ifdef DEBUG_SYNC_REPLICATION
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("addFollower"),
                       JS_AddFollower, true);
#endif
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("removeFollower"),
                       JS_RemoveFollower, true);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("getFollowers"),
                       JS_GetFollowers, true);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("load"),
                       JS_LoadVocbaseCol);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("name"),
                       JS_NameVocbaseCol);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("path"),
                       JS_PathVocbaseCol);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("planId"),
                       JS_PlanIdVocbaseCol);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("properties"),
                       JS_PropertiesVocbaseCol);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("remove"),
                       JS_RemoveVocbaseCol);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("revision"),
                       JS_RevisionVocbaseCol);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("rename"),
                       JS_RenameVocbaseCol);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("replace"),
                       JS_ReplaceVocbaseCol);
  TRI_AddMethodVocbase(
      isolate, rt, TRI_V8_ASCII_STRING("save"),
      JS_InsertVocbaseCol);  // note: save is now an alias for insert
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("status"),
                       JS_StatusVocbaseCol);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("TRUNCATE"),
                       JS_TruncateVocbaseCol, true);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("type"),
                       JS_TypeVocbaseCol);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("unload"),
                       JS_UnloadVocbaseCol);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("update"),
                       JS_UpdateVocbaseCol);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("version"),
                       JS_VersionVocbaseCol);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("loadIndexesIntoMemory"),
                       JS_WarmupVocbaseCol);

  TRI_InitV8IndexCollection(isolate, rt);

  v8g->VocbaseColTempl.Reset(isolate, rt);
  TRI_AddGlobalFunctionVocbase(isolate, TRI_V8_ASCII_STRING("ArangoCollection"),
                               ft->GetFunction());
}
