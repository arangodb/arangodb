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
#include "Aql/Query.h"
#include "Basics/Timers.h"
#include "Basics/Utf8Helper.h"
#include "Basics/conversions.h"
#include "Basics/ScopeGuard.h"
#include "Basics/StaticStrings.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/WriteLocker.h"
#include "Cluster/ClusterMethods.h"
#include "Indexes/PrimaryIndex.h"
#include "Utils/OperationOptions.h"
#include "Utils/OperationResult.h"
#include "Utils/SingleCollectionTransaction.h"
#include "Utils/V8TransactionContext.h"
#include "V8/v8-conv.h"
#include "V8/v8-utils.h"
#include "V8/v8-vpack.h"
#include "V8Server/v8-vocbase.h"
#include "V8Server/v8-vocbaseprivate.h"
#include "V8Server/v8-vocindex.h"
#include "VocBase/auth.h"
#include "VocBase/KeyGenerator.h"
#include "Wal/LogfileManager.h"

#include <velocypack/Builder.h>
#include <velocypack/HexDump.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

struct LocalCollectionGuard {
  explicit LocalCollectionGuard(TRI_vocbase_col_t* collection)
      : _collection(collection) {}

  ~LocalCollectionGuard() {
    if (_collection != nullptr && !_collection->_isLocal) {
      delete _collection;
    }
  }

  TRI_vocbase_col_t* _collection;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief extract the forceSync flag from the arguments
/// must specify the argument index starting from 1
////////////////////////////////////////////////////////////////////////////////

static inline bool ExtractWaitForSync(
    v8::FunctionCallbackInfo<v8::Value> const& args, int index) {
  TRI_ASSERT(index > 0);

  return (args.Length() >= index && TRI_ObjectToBoolean(args[index - 1]));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a v8 collection id value from the internal collection id
////////////////////////////////////////////////////////////////////////////////

static inline v8::Handle<v8::Value> V8CollectionId(v8::Isolate* isolate,
                                                   TRI_voc_cid_t cid) {
  char buffer[21];
  size_t len = TRI_StringUInt64InPlace((uint64_t)cid, (char*)&buffer);

  return TRI_V8_PAIR_STRING((char const*)buffer, (int)len);
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
                                         TRI_vocbase_col_t const*& collection,
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
    collectionName = resolver->getCollectionNameCluster(collection->_cid);
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

    TRI_vocbase_col_t const* col = nullptr;

    if (ServerState::instance()->isCoordinator()) {
      ClusterInfo* ci = ClusterInfo::instance();
      std::shared_ptr<CollectionInfo> c =
          ci->getCollection(vocbase->_name, collectionName);
      col = CoordinatorCollection(vocbase, *c);

      if (col != nullptr && col->_cid == 0) {
        delete col;
        col = nullptr;
      }
    } else {
      col = resolver->getCollectionStruct(collectionName);
    }

    if (col == nullptr) {
      // collection not found
      return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
    }

    collection = col;
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
/// @brief loads a collection for usage
////////////////////////////////////////////////////////////////////////////////

static TRI_vocbase_col_t const* UseCollection(
    v8::Handle<v8::Object> collection,
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  v8::Isolate* isolate = args.GetIsolate();
  int res = TRI_ERROR_INTERNAL;
  TRI_vocbase_col_t* col =
      TRI_UnwrapClass<TRI_vocbase_col_t>(collection, WRP_VOCBASE_COL_TYPE);

  if (col != nullptr) {
    if (!col->_isLocal) {
      TRI_CreateErrorObject(isolate, TRI_ERROR_NOT_IMPLEMENTED);
      TRI_set_errno(TRI_ERROR_NOT_IMPLEMENTED);
      return nullptr;
    }

    TRI_vocbase_col_status_e status;
    res = TRI_UseCollectionVocBase(col->_vocbase, col, status);

    if (res == TRI_ERROR_NO_ERROR && col->_collection != nullptr) {
      // no error
      return col;
    }
  }

  // some error occurred
  TRI_CreateErrorObject(isolate, res, "cannot use/load collection", true);
  TRI_set_errno(res);
  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get all cluster collections
////////////////////////////////////////////////////////////////////////////////

static std::vector<TRI_vocbase_col_t*> GetCollectionsCluster(
    TRI_vocbase_t* vocbase) {
  std::vector<TRI_vocbase_col_t*> result;

  std::vector<std::shared_ptr<CollectionInfo>> const collections =
      ClusterInfo::instance()->getCollections(vocbase->_name);

  for (auto& collection : collections) {
    TRI_vocbase_col_t* c = CoordinatorCollection(vocbase, *(collection));

    try {
      result.emplace_back(c);
    } catch (...) {
      delete c;
      throw;
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get all cluster collection names
////////////////////////////////////////////////////////////////////////////////

static std::vector<std::string> GetCollectionNamesCluster(
    TRI_vocbase_t* vocbase) {
  std::vector<std::string> result;

  std::vector<std::shared_ptr<CollectionInfo>> const collections =
      ClusterInfo::instance()->getCollections(vocbase->_name);

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
  TRI_vocbase_col_t const* col = nullptr;

  if (useCollection) {
    // called as db.collection.exists()
    col =
        TRI_UnwrapClass<TRI_vocbase_col_t>(args.Holder(), WRP_VOCBASE_COL_TYPE);

    if (col == nullptr) {
      TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
    }

    vocbase = col->_vocbase;
  } else {
    // called as db._exists()
    vocbase = GetContextVocBase(isolate);
  }

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  auto transactionContext = std::make_shared<V8TransactionContext>(vocbase, true);

  VPackBuilder builder;
  std::string collectionName;

  int res;
  { 
    VPackObjectBuilder guard(&builder);
    res = ParseDocumentOrDocumentHandle(
      isolate, vocbase, transactionContext->getResolver(), col,
      collectionName, builder, true, args[0]);
  }

  LocalCollectionGuard g(useCollection ? nullptr
                                       : const_cast<TRI_vocbase_col_t*>(col));

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  TRI_ASSERT(col != nullptr);

  TRI_ASSERT(!collectionName.empty());
  VPackSlice search = builder.slice();
  TRI_ASSERT(search.isObject());

  SingleCollectionTransaction trx(transactionContext, collectionName, TRI_TRANSACTION_READ);
  trx.addHint(TRI_TRANSACTION_HINT_SINGLE_OPERATION, false);

  res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
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

  if (res != TRI_ERROR_NO_ERROR) {
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
  
  TIMER_START(JS_DOCUMENT_ALL);

  // first and only argument should be a document handle or key or an object
  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("document(<document-handle> or <document-key> or <object> or <array>)");
  }

  OperationOptions options;
  options.ignoreRevs = false;

  // Find collection and vocbase
  std::string collectionName;
  TRI_vocbase_col_t const* col
      = TRI_UnwrapClass<TRI_vocbase_col_t>(args.Holder(), WRP_VOCBASE_COL_TYPE);
  if (col == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }
  TRI_vocbase_t* vocbase = col->_vocbase;
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


  TIMER_START(JS_DOCUMENT_CREATE_TRX);
  auto transactionContext = std::make_shared<V8TransactionContext>(vocbase, true);

  SingleCollectionTransaction trx(transactionContext, collectionName, 
                                  TRI_TRANSACTION_READ);
  if (!args[0]->IsArray()) {
    trx.addHint(TRI_TRANSACTION_HINT_SINGLE_OPERATION, false);
  }
  
  TIMER_STOP(JS_DOCUMENT_CREATE_TRX);

  int res = trx.begin();
  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }
  
  TIMER_START(JS_DOCUMENT_DOCUMENT);
  OperationResult opResult = trx.document(collectionName, search, options);
  TIMER_STOP(JS_DOCUMENT_DOCUMENT);

  res = trx.finish(opResult.code);

  if (!opResult.successful()) {
    TRI_V8_THROW_EXCEPTION(opResult.code);
  }

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }
  
  TIMER_START(JS_DOCUMENT_VPACK_TO_V8);

  v8::Handle<v8::Value> result = TRI_VPackToV8(isolate, opResult.slice(),
      transactionContext->getVPackOptions());
  
  TIMER_STOP(JS_DOCUMENT_VPACK_TO_V8);

  TIMER_STOP(JS_DOCUMENT_ALL);

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
  TRI_vocbase_col_t const* col = nullptr;

  vocbase = GetContextVocBase(isolate);
  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  auto transactionContext = std::make_shared<V8TransactionContext>(vocbase, true);

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

  LocalCollectionGuard g(const_cast<TRI_vocbase_col_t*>(col));

  TRI_ASSERT(col != nullptr);
  TRI_ASSERT(!collectionName.empty());

  VPackSlice search = builder.slice();
  TRI_ASSERT(search.isObject());
  
  OperationOptions options;
  options.ignoreRevs = false;

  SingleCollectionTransaction trx(transactionContext, collectionName,
                                  TRI_TRANSACTION_READ);
  trx.addHint(TRI_TRANSACTION_HINT_SINGLE_OPERATION, false);

  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  OperationResult opResult = trx.document(collectionName, search, options);

  res = trx.finish(opResult.code);

  if (!opResult.successful()) {
    TRI_V8_THROW_EXCEPTION(opResult.code);
  }

  if (res != TRI_ERROR_NO_ERROR) {
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
    } else {  // old variant remove(<document>, <overwrite>, <waitForSync>)
      options.ignoreRevs = TRI_ObjectToBoolean(args[1]);
      if (argLength > 2) {
        options.waitForSync = TRI_ObjectToBoolean(args[2]);
      }
    }
  }

  // Find collection and vocbase
  std::string collectionName;
  TRI_vocbase_col_t const* col
      = TRI_UnwrapClass<TRI_vocbase_col_t>(args.Holder(), WRP_VOCBASE_COL_TYPE);
  if (col == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }
  TRI_vocbase_t* vocbase = col->_vocbase;
  collectionName = col->name();
  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  auto transactionContext = std::make_shared<V8TransactionContext>(vocbase, true);

  SingleCollectionTransaction trx(transactionContext, collectionName, TRI_TRANSACTION_WRITE);
  if (!args[0]->IsArray()) {
    trx.addHint(TRI_TRANSACTION_HINT_SINGLE_OPERATION, false);
  }

  int res = trx.begin();
  if (res != TRI_ERROR_NO_ERROR) {
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

  if (res != TRI_ERROR_NO_ERROR) {
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
  TRI_vocbase_col_t const* col = nullptr;

  vocbase = GetContextVocBase(isolate);
  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  auto transactionContext = std::make_shared<V8TransactionContext>(vocbase, true);

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

  LocalCollectionGuard g(const_cast<TRI_vocbase_col_t*>(col));

  TRI_ASSERT(col != nullptr);
  TRI_ASSERT(!collectionName.empty());

  VPackSlice toRemove = builder.slice();
  TRI_ASSERT(toRemove.isObject());

  SingleCollectionTransaction trx(transactionContext, collectionName,
                                  TRI_TRANSACTION_WRITE);
  trx.addHint(TRI_TRANSACTION_HINT_SINGLE_OPERATION, false);

  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  OperationResult result = trx.remove(collectionName, toRemove, options);

  res = trx.finish(result.code);

  if (!result.successful()) {
    TRI_V8_THROW_EXCEPTION(result.code);
  }

  if (res != TRI_ERROR_NO_ERROR) {
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
/// @brief was docuBlock documentsCollectionName
////////////////////////////////////////////////////////////////////////////////

static void JS_DocumentVocbaseCol(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  DocumentVocbaseCol(args);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief drops a collection, case of a coordinator in a cluster
////////////////////////////////////////////////////////////////////////////////

static void DropVocbaseColCoordinator(
    v8::FunctionCallbackInfo<v8::Value> const& args,
    TRI_vocbase_col_t* collection) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  if (!collection->_canDrop) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_FORBIDDEN);
  }

  std::string const databaseName(collection->_dbName);
  std::string const cid = StringUtils::itoa(collection->_cid);

  ClusterInfo* ci = ClusterInfo::instance();
  std::string errorMsg;

  int res = ci->dropCollectionCoordinator(databaseName, cid, errorMsg, 120.0);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(res, errorMsg);
  }

  collection->_status = TRI_VOC_COL_STATUS_DELETED;

  TRI_V8_RETURN_UNDEFINED();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock collectionDrop
////////////////////////////////////////////////////////////////////////////////

static void JS_DropVocbaseCol(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  TRI_vocbase_col_t* collection =
      TRI_UnwrapClass<TRI_vocbase_col_t>(args.Holder(), WRP_VOCBASE_COL_TYPE);

  if (collection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  PREVENT_EMBEDDED_TRANSACTION();

  // If we are a coordinator in a cluster, we have to behave differently:
  if (ServerState::instance()->isCoordinator()) {
    DropVocbaseColCoordinator(args, collection);
    return;
  }

  int res = TRI_DropCollectionVocBase(collection->_vocbase, collection, true);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(res, "cannot drop collection");
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
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief fetch the figures for a sharded collection
////////////////////////////////////////////////////////////////////////////////

static TRI_doc_collection_info_t* GetFiguresCoordinator(
    TRI_vocbase_col_t* collection) {
  TRI_ASSERT(collection != nullptr);

  std::string const databaseName(collection->_dbName);
  std::string const cid = StringUtils::itoa(collection->_cid);

  TRI_doc_collection_info_t* result = nullptr;

  int res = figuresOnCoordinator(databaseName, cid, result);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_set_errno(res);
    return nullptr;
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief fetch the figures for a local collection
////////////////////////////////////////////////////////////////////////////////

static TRI_doc_collection_info_t* GetFigures(TRI_vocbase_col_t* collection) {
  TRI_ASSERT(collection != nullptr);

  SingleCollectionTransaction trx(V8TransactionContext::Create(collection->_vocbase, true), collection->_cid, TRI_TRANSACTION_READ);

  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_set_errno(res);
    return nullptr;
  }

  // READ-LOCK start
  trx.lockRead();

  TRI_document_collection_t* document = collection->_collection;
  TRI_doc_collection_info_t* info = document->figures();

  trx.finish(res);
  // READ-LOCK end

  return info;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock collectionFigures
////////////////////////////////////////////////////////////////////////////////

static void JS_FiguresVocbaseCol(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  TRI_vocbase_col_t* collection =
      TRI_UnwrapClass<TRI_vocbase_col_t>(args.Holder(), WRP_VOCBASE_COL_TYPE);

  if (collection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  v8::Handle<v8::Object> result = v8::Object::New(isolate);

  TRI_doc_collection_info_t* info;

  if (ServerState::instance()->isCoordinator()) {
    info = GetFiguresCoordinator(collection);
  } else {
    info = GetFigures(collection);
  }

  if (info == nullptr) {
    TRI_V8_THROW_EXCEPTION_MEMORY();
  }

  v8::Handle<v8::Object> alive = v8::Object::New(isolate);

  result->Set(TRI_V8_ASCII_STRING("alive"), alive);
  alive->Set(TRI_V8_ASCII_STRING("count"),
             v8::Number::New(isolate, (double)info->_numberAlive));
  alive->Set(TRI_V8_ASCII_STRING("size"),
             v8::Number::New(isolate, (double)info->_sizeAlive));

  v8::Handle<v8::Object> dead = v8::Object::New(isolate);

  result->Set(TRI_V8_ASCII_STRING("dead"), dead);
  dead->Set(TRI_V8_ASCII_STRING("count"),
            v8::Number::New(isolate, (double)info->_numberDead));
  dead->Set(TRI_V8_ASCII_STRING("size"),
            v8::Number::New(isolate, (double)info->_sizeDead));
  dead->Set(TRI_V8_ASCII_STRING("deletion"),
            v8::Number::New(isolate, (double)info->_numberDeletions));

  // datafile info
  v8::Handle<v8::Object> dfs = v8::Object::New(isolate);

  result->Set(TRI_V8_ASCII_STRING("datafiles"), dfs);
  dfs->Set(TRI_V8_ASCII_STRING("count"),
           v8::Number::New(isolate, (double)info->_numberDatafiles));
  dfs->Set(TRI_V8_ASCII_STRING("fileSize"),
           v8::Number::New(isolate, (double)info->_datafileSize));

  // journal info
  v8::Handle<v8::Object> js = v8::Object::New(isolate);

  result->Set(TRI_V8_ASCII_STRING("journals"), js);
  js->Set(TRI_V8_ASCII_STRING("count"),
          v8::Number::New(isolate, (double)info->_numberJournalfiles));
  js->Set(TRI_V8_ASCII_STRING("fileSize"),
          v8::Number::New(isolate, (double)info->_journalfileSize));

  // compactors info
  v8::Handle<v8::Object> cs = v8::Object::New(isolate);

  result->Set(TRI_V8_ASCII_STRING("compactors"), cs);
  cs->Set(TRI_V8_ASCII_STRING("count"),
          v8::Number::New(isolate, (double)info->_numberCompactorfiles));
  cs->Set(TRI_V8_ASCII_STRING("fileSize"),
          v8::Number::New(isolate, (double)info->_compactorfileSize));

  v8::Handle<v8::Object> indexes = v8::Object::New(isolate);
  result->Set(TRI_V8_ASCII_STRING("indexes"), indexes);
  indexes->Set(TRI_V8_ASCII_STRING("count"),
               v8::Number::New(isolate, (double)info->_numberIndexes));
  indexes->Set(TRI_V8_ASCII_STRING("size"),
               v8::Number::New(isolate, (double)info->_sizeIndexes));

  result->Set(TRI_V8_ASCII_STRING("lastTick"),
              V8TickId(isolate, info->_tickMax));
  result->Set(
      TRI_V8_ASCII_STRING("uncollectedLogfileEntries"),
      v8::Number::New(isolate, (double)info->_uncollectedLogfileEntries));
  result->Set(TRI_V8_ASCII_STRING("documentReferences"),
              v8::Number::New(isolate, (double)info->_numberDocumentDitches));

  char const* wfd = "-";
  if (info->_waitingForDitch != nullptr) {
    wfd = info->_waitingForDitch;
  }
  result->Set(TRI_V8_ASCII_STRING("waitingFor"), TRI_V8_ASCII_STRING(wfd));

  v8::Handle<v8::Object> compaction = v8::Object::New(isolate);
  if (info->_lastCompactionStatus != nullptr) {
    compaction->Set(TRI_V8_ASCII_STRING("message"),
                    TRI_V8_ASCII_STRING(info->_lastCompactionStatus));
    compaction->Set(TRI_V8_ASCII_STRING("time"),
                    TRI_V8_ASCII_STRING(&info->_lastCompactionStamp[0]));
  } else {
    compaction->Set(TRI_V8_ASCII_STRING("message"), TRI_V8_ASCII_STRING("-"));
    compaction->Set(TRI_V8_ASCII_STRING("time"), TRI_V8_ASCII_STRING("-"));
  }
  result->Set(TRI_V8_ASCII_STRING("compactionStatus"), compaction);

  TRI_Free(TRI_UNKNOWN_MEM_ZONE, info);

  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock collectionLoad
////////////////////////////////////////////////////////////////////////////////

static void JS_LeaderResign(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  TRI_vocbase_t* vocbase = GetContextVocBase(isolate);

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  if (ServerState::instance()->isDBServer()) {
    TRI_vocbase_col_t const* collection =
        TRI_UnwrapClass<TRI_vocbase_col_t>(args.Holder(), WRP_VOCBASE_COL_TYPE);

    if (collection == nullptr) {
      TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
    }

    TRI_vocbase_t* vocbase = collection->_vocbase;
    std::string collectionName = collection->name();
    if (vocbase == nullptr) {
      TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
    }

    auto transactionContext = std::make_shared<V8TransactionContext>(vocbase, true);

    SingleCollectionTransaction trx(transactionContext, collectionName, 
                                    TRI_TRANSACTION_READ);
    int res = trx.begin();
    if (res != TRI_ERROR_NO_ERROR) {
      TRI_V8_THROW_EXCEPTION(res);
    }
    TRI_document_collection_t* docColl = trx.documentCollection();
    docColl->followers()->clear();
  }

  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock collectionLoad
////////////////////////////////////////////////////////////////////////////////

static void JS_LoadVocbaseCol(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  TRI_vocbase_t* vocbase = GetContextVocBase(isolate);

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  if (ServerState::instance()->isCoordinator()) {
    TRI_vocbase_col_t const* collection =
        TRI_UnwrapClass<TRI_vocbase_col_t>(args.Holder(), WRP_VOCBASE_COL_TYPE);

    if (collection == nullptr) {
      TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
    }

    std::string const databaseName(collection->_dbName);
    std::string const cid = StringUtils::itoa(collection->_cid);

    int res = ClusterInfo::instance()->setCollectionStatusCoordinator(
        databaseName, cid, TRI_VOC_COL_STATUS_LOADED);

    if (res != TRI_ERROR_NO_ERROR) {
      TRI_V8_THROW_EXCEPTION(res);
    }

    TRI_V8_RETURN_UNDEFINED();
  }

  TRI_vocbase_col_t const* collection = UseCollection(args.Holder(), args);

  if (collection == nullptr) {
    return;
  }

  ReleaseCollection(collection);
  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the name of a collection
////////////////////////////////////////////////////////////////////////////////

static void JS_NameVocbaseCol(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  TRI_vocbase_col_t const* collection =
      TRI_UnwrapClass<TRI_vocbase_col_t>(args.Holder(), WRP_VOCBASE_COL_TYPE);

  if (collection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  if (!collection->_isLocal) {
    std::string const collectionName(collection->name());
    v8::Handle<v8::Value> result = TRI_V8_STRING(collectionName.c_str());
    TRI_V8_RETURN(result);
  }

  // this copies the name into a new place so we can safely access it later
  // if we wouldn't do this, we would risk other threads modifying the name
  // while
  // we're reading it
  std::string name(TRI_GetCollectionNameByIdVocBase(collection->_vocbase, collection->_cid));

  v8::Handle<v8::Value> result = TRI_V8_STD_STRING(name);

  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the path of a collection
////////////////////////////////////////////////////////////////////////////////

static void JS_PathVocbaseCol(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  TRI_vocbase_col_t const* collection =
      TRI_UnwrapClass<TRI_vocbase_col_t>(args.Holder(), WRP_VOCBASE_COL_TYPE);

  if (collection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  std::string const path(collection->path());

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

  TRI_vocbase_col_t const* collection =
      TRI_UnwrapClass<TRI_vocbase_col_t>(args.Holder(), WRP_VOCBASE_COL_TYPE);

  if (collection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  if (ServerState::instance()->isCoordinator()) {
    TRI_V8_RETURN(V8CollectionId(isolate, collection->_cid));
  }

  TRI_V8_RETURN(V8CollectionId(isolate, collection->_planId));
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock collectionProperties
////////////////////////////////////////////////////////////////////////////////

static void JS_PropertiesVocbaseCol(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);
  TRI_GET_GLOBALS();

  TRI_vocbase_col_t const* collection =
      TRI_UnwrapClass<TRI_vocbase_col_t>(args.Holder(), WRP_VOCBASE_COL_TYPE);

  if (collection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  if (ServerState::instance()->isCoordinator()) {
    std::string const databaseName = std::string(collection->_dbName);
    arangodb::VocbaseCollectionInfo info =
        ClusterInfo::instance()->getCollectionProperties(
            databaseName, StringUtils::itoa(collection->_cid));

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
        if (slice.hasKey("journalSize")) {
          VPackSlice maxSizeSlice = slice.get("journalSize");
          TRI_voc_size_t maximalSize =
              maxSizeSlice.getNumericValue<TRI_voc_size_t>();
          if (maximalSize < TRI_JOURNAL_MINIMAL_SIZE) {
            TRI_V8_THROW_EXCEPTION_PARAMETER(
                "<properties>.journalSize too small");
          }
        }
        if (info.isVolatile() !=
            arangodb::basics::VelocyPackHelper::getBooleanValue(
                slice, "isVolatile", info.isVolatile())) {
          TRI_V8_THROW_EXCEPTION_PARAMETER(
              "isVolatile option cannot be changed at runtime");
        }
        if (info.isVolatile() && info.waitForSync()) {
          TRI_V8_THROW_EXCEPTION_PARAMETER(
              "volatile collections do not support the waitForSync option");
        }
        uint32_t tmp =
            arangodb::basics::VelocyPackHelper::getNumericValue<uint32_t>(
                slice, "indexBuckets",
                2 /*Just for validation, this default Value passes*/);
        if (tmp == 0 || tmp > 1024) {
          TRI_V8_THROW_EXCEPTION_PARAMETER(
              "indexBuckets must be a two-power between 1 and 1024");
        }
        info.update(slice, false, collection->_vocbase);
      }

      int res = ClusterInfo::instance()->setCollectionPropertiesCoordinator(
          databaseName, StringUtils::itoa(collection->_cid), &info);

      if (res != TRI_ERROR_NO_ERROR) {
        TRI_V8_THROW_EXCEPTION(res);
      }
    }

    // return the current parameter set
    v8::Handle<v8::Object> result = v8::Object::New(isolate);

    TRI_GET_GLOBAL_STRING(DoCompactKey);
    TRI_GET_GLOBAL_STRING(IsSystemKey);
    TRI_GET_GLOBAL_STRING(IsVolatileKey);
    TRI_GET_GLOBAL_STRING(JournalSizeKey);
    TRI_GET_GLOBAL_STRING(WaitForSyncKey);
    result->Set(DoCompactKey, v8::Boolean::New(isolate, info.doCompact()));
    result->Set(IsSystemKey, v8::Boolean::New(isolate, info.isSystem()));
    result->Set(IsVolatileKey, v8::Boolean::New(isolate, info.isVolatile()));
    result->Set(JournalSizeKey, v8::Number::New(isolate, info.maximalSize()));
    result->Set(WaitForSyncKey, v8::Boolean::New(isolate, info.waitForSync()));
    result->Set(TRI_V8_ASCII_STRING("indexBuckets"),
                v8::Number::New(isolate, info.indexBuckets()));

    std::shared_ptr<CollectionInfo> c = ClusterInfo::instance()->getCollection(
        databaseName, StringUtils::itoa(collection->_cid));
    v8::Handle<v8::Array> shardKeys = v8::Array::New(isolate);
    std::vector<std::string> const sks = (*c).shardKeys();
    for (size_t i = 0; i < sks.size(); ++i) {
      shardKeys->Set((uint32_t)i, TRI_V8_STD_STRING(sks[i]));
    }
    result->Set(TRI_V8_ASCII_STRING("shardKeys"), shardKeys);
    result->Set(TRI_V8_ASCII_STRING("numberOfShards"),
                v8::Number::New(isolate, (*c).numberOfShards()));
    auto keyOpts = info.keyOptions();
    if (keyOpts != nullptr && keyOpts->size() > 0) {
      TRI_GET_GLOBAL_STRING(KeyOptionsKey);
      VPackSlice const slice(keyOpts->data());
      result->Set(KeyOptionsKey, TRI_VPackToV8(isolate, slice)->ToObject());
    }
    result->Set(
        TRI_V8_ASCII_STRING("replicationFactor"),
        v8::Number::New(isolate, static_cast<double>(c->replicationFactor())));

    TRI_V8_RETURN(result);
  }

  collection = UseCollection(args.Holder(), args);

  if (collection == nullptr) {
    return;
  }

  TRI_document_collection_t* document = collection->_collection;

  // check if we want to change some parameters
  if (0 < args.Length()) {
    v8::Handle<v8::Value> par = args[0];

    if (par->IsObject()) {
      VPackBuilder builder;
      int res = TRI_V8ToVPack(isolate, builder, args[0], false);

      if (res != TRI_ERROR_NO_ERROR) {
        TRI_V8_THROW_EXCEPTION(res);
      }

      VPackSlice const slice = builder.slice();

      {
        // only work under the lock
        WRITE_LOCKER(writeLocker, document->_infoLock);

        if (document->_info.isVolatile() &&
            arangodb::basics::VelocyPackHelper::getBooleanValue(
                slice, "waitForSync", document->_info.waitForSync())) {
          ReleaseCollection(collection);
          // the combination of waitForSync and isVolatile makes no sense
          TRI_V8_THROW_EXCEPTION_PARAMETER(
              "volatile collections do not support the waitForSync option");
        }

        if (document->_info.isVolatile() !=
            arangodb::basics::VelocyPackHelper::getBooleanValue(
                slice, "isVolatile", document->_info.isVolatile())) {
          TRI_V8_THROW_EXCEPTION_PARAMETER(
              "isVolatile option cannot be changed at runtime");
        }

        uint32_t tmp =
            arangodb::basics::VelocyPackHelper::getNumericValue<uint32_t>(
                slice, "indexBuckets",
                2 /*Just for validation, this default Value passes*/);
        if (tmp == 0 || tmp > 1024) {
          ReleaseCollection(collection);
          TRI_V8_THROW_EXCEPTION_PARAMETER(
              "indexBuckets must be a two-power between 1 and 1024");
        }

      }  // Leave the scope and free the lock

      // try to write new parameter to file
      bool doSync = document->_vocbase->_settings.forceSyncProperties;
      res = document->updateCollectionInfo(document->_vocbase, slice, doSync);

      if (res != TRI_ERROR_NO_ERROR) {
        ReleaseCollection(collection);
        TRI_V8_THROW_EXCEPTION(res);
      }

      try {
        VPackBuilder infoBuilder;
        infoBuilder.openObject();
        TRI_CreateVelocyPackCollectionInfo(document->_info, infoBuilder);
        infoBuilder.close();

        // now log the property changes
        res = TRI_ERROR_NO_ERROR;

        arangodb::wal::CollectionMarker marker(TRI_DF_MARKER_VPACK_CHANGE_COLLECTION, document->_vocbase->_id, document->_info.id(), infoBuilder.slice());
        arangodb::wal::SlotInfoCopy slotInfo =
            arangodb::wal::LogfileManager::instance()->allocateAndWrite(marker, false);

        if (slotInfo.errorCode != TRI_ERROR_NO_ERROR) {
          THROW_ARANGO_EXCEPTION(slotInfo.errorCode);
        }
      } catch (arangodb::basics::Exception const& ex) {
        res = ex.code();
      } catch (...) {
        res = TRI_ERROR_INTERNAL;
      }

      if (res != TRI_ERROR_NO_ERROR) {
        // TODO: what to do here
        LOG(WARN) << "could not save collection change marker in log: "
                  << TRI_errno_string(res);
      }
    }
  }

  // return the current parameter set
  v8::Handle<v8::Object> result = v8::Object::New(isolate);

  TRI_GET_GLOBAL_STRING(DoCompactKey);
  TRI_GET_GLOBAL_STRING(IsSystemKey);
  TRI_GET_GLOBAL_STRING(IsVolatileKey);
  TRI_GET_GLOBAL_STRING(JournalSizeKey);
  result->Set(DoCompactKey, v8::Boolean::New(isolate, document->_info.doCompact()));
  result->Set(IsSystemKey, v8::Boolean::New(isolate, document->_info.isSystem()));
  result->Set(IsVolatileKey,
              v8::Boolean::New(isolate, document->_info.isVolatile()));
  result->Set(JournalSizeKey,
              v8::Number::New(isolate, document->_info.maximalSize()));
  result->Set(TRI_V8_ASCII_STRING("indexBuckets"),
              v8::Number::New(isolate, document->_info.indexBuckets()));

  TRI_GET_GLOBAL_STRING(KeyOptionsKey);
  try {
    VPackBuilder optionsBuilder;
    optionsBuilder.openObject();
    document->_keyGenerator->toVelocyPack(optionsBuilder);
    optionsBuilder.close();
    result->Set(KeyOptionsKey,
                TRI_VPackToV8(isolate, optionsBuilder.slice())->ToObject());
  } catch (...) {
    // Could not build the VPack
    result->Set(KeyOptionsKey, v8::Array::New(isolate));
  }
  TRI_GET_GLOBAL_STRING(WaitForSyncKey);
  result->Set(WaitForSyncKey,
              v8::Boolean::New(isolate, document->_info.waitForSync()));

  ReleaseCollection(collection);
  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

static void JS_RemoveVocbaseCol(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  return RemoveVocbaseCol(args);
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

  TRI_vocbase_col_t* collection =
      TRI_UnwrapClass<TRI_vocbase_col_t>(args.Holder(), WRP_VOCBASE_COL_TYPE);

  if (collection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  PREVENT_EMBEDDED_TRANSACTION();

  if (ServerState::instance()->isCoordinator()) {
    // renaming a collection in a cluster is unsupported
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_CLUSTER_UNSUPPORTED);
  }

  std::string const oldName(collection->_name);

  int res = TRI_RenameCollectionVocBase(collection->_vocbase, collection,
                                        name.c_str(), doOverride, true);

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

  // Find collection and vocbase
  TRI_vocbase_col_t const* col =
      TRI_UnwrapClass<TRI_vocbase_col_t>(args.Holder(), WRP_VOCBASE_COL_TYPE);
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
                               updateBuilder, true)) {
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


  auto transactionContext = std::make_shared<V8TransactionContext>(vocbase, true);
  
  // Now start the transaction:
  SingleCollectionTransaction trx(transactionContext, collectionName,
                                  TRI_TRANSACTION_WRITE);
  if (!args[0]->IsArray()) {
    trx.addHint(TRI_TRANSACTION_HINT_SINGLE_OPERATION, false);
  }

  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
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

  if (res != TRI_ERROR_NO_ERROR) {
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

  TRI_vocbase_col_t const* col = nullptr;
  std::string collectionName;

  TRI_vocbase_t* vocbase = GetContextVocBase(isolate);

  auto transactionContext = std::make_shared<V8TransactionContext>(vocbase, true);
  
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
  LocalCollectionGuard g(const_cast<TRI_vocbase_col_t*>(col));

  SingleCollectionTransaction trx(transactionContext, collectionName,
                                  TRI_TRANSACTION_WRITE);
  trx.addHint(TRI_TRANSACTION_HINT_SINGLE_OPERATION, false);

  int res = trx.begin();
  if (res != TRI_ERROR_NO_ERROR) {
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

  if (res != TRI_ERROR_NO_ERROR) {
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
/// @brief fetch the revision for a local collection
////////////////////////////////////////////////////////////////////////////////

static int GetRevision(TRI_vocbase_col_t* collection, TRI_voc_rid_t& rid) {
  TRI_ASSERT(collection != nullptr);

  SingleCollectionTransaction trx(V8TransactionContext::Create(collection->_vocbase, true), collection->_cid, TRI_TRANSACTION_READ);

  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  // READ-LOCK start
  trx.lockRead();
  rid = collection->_collection->_info.revision();
  trx.finish(res);
  // READ-LOCK end

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief fetch the revision for a sharded collection
////////////////////////////////////////////////////////////////////////////////

static int GetRevisionCoordinator(TRI_vocbase_col_t* collection,
                                  TRI_voc_rid_t& rid) {
  TRI_ASSERT(collection != nullptr);

  std::string const databaseName(collection->_dbName);
  std::string const cid = StringUtils::itoa(collection->_cid);

  int res = revisionOnCoordinator(databaseName, cid, rid);

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock collectionRevision
////////////////////////////////////////////////////////////////////////////////

static void JS_RevisionVocbaseCol(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  TRI_vocbase_col_t* collection =
      TRI_UnwrapClass<TRI_vocbase_col_t>(args.Holder(), WRP_VOCBASE_COL_TYPE);

  if (collection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  TRI_voc_rid_t rid;
  int res;

  if (ServerState::instance()->isCoordinator()) {
    res = GetRevisionCoordinator(collection, rid);
  } else {
    res = GetRevision(collection, rid);
  }

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  TRI_V8_RETURN(V8RevisionId(isolate, rid));
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock collectionRotate
////////////////////////////////////////////////////////////////////////////////

static void JS_RotateVocbaseCol(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  if (ServerState::instance()->isCoordinator()) {
    // renaming a collection in a cluster is unsupported
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_CLUSTER_UNSUPPORTED);
  }

  PREVENT_EMBEDDED_TRANSACTION();

  TRI_vocbase_col_t const* collection = UseCollection(args.Holder(), args);

  if (collection == nullptr) {
    return;
  }

  TRI_THROW_SHARDING_COLLECTION_NOT_YET_IMPLEMENTED(collection);

  TRI_document_collection_t* document = collection->_collection;

  int res = document->rotateActiveJournal();

  ReleaseCollection(collection);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(res, "could not rotate journal");
  }

  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief retrieves a collection from a V8 argument
////////////////////////////////////////////////////////////////////////////////

static TRI_vocbase_col_t* GetCollectionFromArgument(
    TRI_vocbase_t* vocbase, v8::Handle<v8::Value> const val) {
  // number
  if (val->IsNumber() || val->IsNumberObject()) {
    uint64_t cid = TRI_ObjectToUInt64(val, true);
    return TRI_LookupCollectionByIdVocBase(vocbase, cid);
  }

  return TRI_LookupCollectionByNameVocBase(vocbase, TRI_ObjectToString(val));
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

  if (TRI_IsDeletedVocBase(vocbase)) {
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

  int res = TRI_V8ToVPack(isolate, builder, doc, false);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  // load collection
  auto transactionContext(V8TransactionContext::Create(vocbase, true));
  SingleCollectionTransaction trx(transactionContext,
                                  collectionName, TRI_TRANSACTION_WRITE);
  trx.addHint(TRI_TRANSACTION_HINT_SINGLE_OPERATION, false);

  res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  OperationResult result = trx.insert(collectionName, builder.slice(), options);

  res = trx.finish(result.code);

  if (res != TRI_ERROR_NO_ERROR) {
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

static void JS_InsertVocbaseCol(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  TIMER_START(JS_INSERT_ALL);

  auto collection = TRI_UnwrapClass<TRI_vocbase_col_t>(args.Holder(), WRP_VOCBASE_COL_TYPE);

  if (collection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  bool const isEdgeCollection =
      ((TRI_col_type_e)collection->_type == TRI_COL_TYPE_EDGE);

  uint32_t const argLength = args.Length();

  // Position of <data> and <options>
  // They differ for edge (old signature) and document.
  uint32_t docIdx = 0;
  uint32_t optsIdx = 1;

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
    optsIdx = 3;
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
  } else {
    options.waitForSync = ExtractWaitForSync(args, optsIdx + 1);
  }

  if (!args[docIdx]->IsObject()) {
    // invalid value type. must be a document
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
  }
  
  TIMER_START(JS_INSERT_V8_TO_VPACK);

  // copy default options (and set exclude handler in copy)
  VPackOptions vpackOptions = VPackOptions::Defaults;
  vpackOptions.attributeExcludeHandler = basics::VelocyPackHelper::getExcludeHandler();
  VPackBuilder builder(&vpackOptions);

  auto doOneDocument = [&](v8::Handle<v8::Value> obj) -> void {
    TIMER_START(JS_INSERT_V8_TO_VPACK2);
    int res = TRI_V8ToVPack(isolate, builder, obj, true);
    TIMER_STOP(JS_INSERT_V8_TO_VPACK2);

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
  
  TIMER_STOP(JS_INSERT_V8_TO_VPACK);

  TIMER_START(JS_INSERT_CREATE_TRX);
  // load collection
  auto transactionContext = std::make_shared<V8TransactionContext>(collection->_vocbase, true);

  SingleCollectionTransaction trx(transactionContext, collection->_cid, TRI_TRANSACTION_WRITE);
  if (!payloadIsArray) {
    trx.addHint(TRI_TRANSACTION_HINT_SINGLE_OPERATION, false);
  }
  TIMER_STOP(JS_INSERT_CREATE_TRX);

  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  TIMER_START(JS_INSERT_INSERT);
  OperationResult result = trx.insert(collection->_name, builder.slice(), options);
  TIMER_STOP(JS_INSERT_INSERT);

  res = trx.finish(result.code);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  if (options.silent) {
    // no return value
    TRI_V8_RETURN_TRUE();
  }

  VPackSlice resultSlice = result.slice();
  
  TIMER_START(JS_INSERT_VPACK_TO_V8);
  
  auto v8Result = TRI_VPackToV8(isolate, resultSlice,
                                transactionContext->getVPackOptions());
  
  TIMER_STOP(JS_INSERT_VPACK_TO_V8);

  TIMER_STOP(JS_INSERT_ALL);
  
  TRI_V8_RETURN(v8Result);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the status of a collection
////////////////////////////////////////////////////////////////////////////////

static void JS_StatusVocbaseCol(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  TRI_vocbase_col_t* collection =
      TRI_UnwrapClass<TRI_vocbase_col_t>(args.Holder(), WRP_VOCBASE_COL_TYPE);

  if (collection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  if (ServerState::instance()->isCoordinator()) {
    std::string const databaseName = std::string(collection->_dbName);

    std::shared_ptr<CollectionInfo> const ci =
        ClusterInfo::instance()->getCollection(
            databaseName, StringUtils::itoa(collection->_cid));

    if ((*ci).empty()) {
      TRI_V8_RETURN(v8::Number::New(isolate, (int)TRI_VOC_COL_STATUS_DELETED));
    }
    TRI_V8_RETURN(v8::Number::New(isolate, (int)ci->status()));
  }
  // fallthru intentional

  TRI_vocbase_col_status_e status;
  {
    READ_LOCKER(readLocker, collection->_lock);
    status = collection->_status;
  }

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
  opOptions.waitForSync = ExtractWaitForSync(args, 1);

  TRI_vocbase_col_t* collection =
      TRI_UnwrapClass<TRI_vocbase_col_t>(args.Holder(), WRP_VOCBASE_COL_TYPE);

  if (collection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  SingleCollectionTransaction trx(V8TransactionContext::Create(collection->_vocbase, true), collection->_cid, TRI_TRANSACTION_WRITE);

  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  OperationResult result = trx.truncate(collection->_name, opOptions);

  res = trx.finish(result.code);

  if (result.failed()) {
    TRI_V8_THROW_EXCEPTION(result.code);
  }

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief truncates a datafile
////////////////////////////////////////////////////////////////////////////////

static void JS_TruncateDatafileVocbaseCol(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  TRI_vocbase_col_t* collection =
      TRI_UnwrapClass<TRI_vocbase_col_t>(args.Holder(), WRP_VOCBASE_COL_TYPE);

  if (collection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  TRI_THROW_SHARDING_COLLECTION_NOT_YET_IMPLEMENTED(collection);

  if (args.Length() != 2) {
    TRI_V8_THROW_EXCEPTION_USAGE("truncateDatafile(<datafile>, <size>)");
  }

  std::string path = TRI_ObjectToString(args[0]);
  size_t size = (size_t)TRI_ObjectToInt64(args[1]);

  int res;
  {
    READ_LOCKER(readLocker, collection->_lock);

    if (collection->_status != TRI_VOC_COL_STATUS_UNLOADED &&
        collection->_status != TRI_VOC_COL_STATUS_CORRUPTED) {
      TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_COLLECTION_NOT_UNLOADED);
    }

    res = TRI_TruncateDatafile(path.c_str(), (TRI_voc_size_t)size);
  }

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(res, "cannot truncate datafile");
  }

  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief truncates a datafile
////////////////////////////////////////////////////////////////////////////////

static void JS_TryRepairDatafileVocbaseCol(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  TRI_vocbase_col_t* collection =
      TRI_UnwrapClass<TRI_vocbase_col_t>(args.Holder(), WRP_VOCBASE_COL_TYPE);

  if (collection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  TRI_THROW_SHARDING_COLLECTION_NOT_YET_IMPLEMENTED(collection);

  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("tryRepairDatafile(<datafile>)");
  }

  std::string path = TRI_ObjectToString(args[0]);

  bool result;
  {
    READ_LOCKER(readLocker, collection->_lock);

    if (collection->_status != TRI_VOC_COL_STATUS_UNLOADED &&
        collection->_status != TRI_VOC_COL_STATUS_CORRUPTED) {
      TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_COLLECTION_NOT_UNLOADED);
    }

    result = TRI_TryRepairDatafile(path.c_str());
  }

  if (result) {
    TRI_V8_RETURN_TRUE();
  }

  TRI_V8_RETURN_FALSE();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock collectionType
////////////////////////////////////////////////////////////////////////////////

static void JS_TypeVocbaseCol(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  TRI_vocbase_col_t* collection =
      TRI_UnwrapClass<TRI_vocbase_col_t>(args.Holder(), WRP_VOCBASE_COL_TYPE);

  if (collection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  if (ServerState::instance()->isCoordinator()) {
    std::string const databaseName = std::string(collection->_dbName);

    std::shared_ptr<CollectionInfo> const ci =
        ClusterInfo::instance()->getCollection(
            databaseName, StringUtils::itoa(collection->_cid));

    if ((*ci).empty()) {
      TRI_V8_RETURN(v8::Number::New(isolate, (int)collection->_type));
    } else {
      TRI_V8_RETURN(v8::Number::New(isolate, (int)ci->type()));
    }
  }
  // fallthru intentional

  TRI_col_type_e type;
  {
    READ_LOCKER(readLocker, collection->_lock);
    type = (TRI_col_type_e)collection->_type;
  }

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

  TRI_vocbase_col_t* collection =
      TRI_UnwrapClass<TRI_vocbase_col_t>(args.Holder(), WRP_VOCBASE_COL_TYPE);

  if (collection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  int res;

  if (ServerState::instance()->isCoordinator()) {
    std::string const databaseName(collection->_dbName);

    res = ClusterInfo::instance()->setCollectionStatusCoordinator(
        databaseName, StringUtils::itoa(collection->_cid),
        TRI_VOC_COL_STATUS_UNLOADED);
  } else {
    res = TRI_UnloadCollectionVocBase(collection->_vocbase, collection, false);
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

  TRI_vocbase_col_t* collection =
      TRI_UnwrapClass<TRI_vocbase_col_t>(args.Holder(), WRP_VOCBASE_COL_TYPE);

  if (collection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  if (ServerState::instance()->isCoordinator()) {
    TRI_V8_RETURN(v8::Number::New(isolate, (int)TRI_COL_VERSION));
  }

  // fallthru intentional
  READ_LOCKER(readLocker, collection->_lock);
  try {
    std::string const collectionName(collection->name());
    VocbaseCollectionInfo info = VocbaseCollectionInfo::fromFile(
        collection->pathc_str(), collection->vocbase(), collectionName.c_str(),
        false);

    TRI_V8_RETURN(v8::Number::New(isolate, (int)info.version()));
  } catch (arangodb::basics::Exception const& e) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(e.code(), "cannot fetch collection info");
  } catch (...) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "cannot fetch collection info");
  }
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

  if (TRI_IsDeletedVocBase(vocbase)) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  // expecting one argument
  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("_collection(<name>|<identifier>)");
  }

  v8::Handle<v8::Value> val = args[0];
  TRI_vocbase_col_t const* collection = nullptr;

  if (ServerState::instance()->isCoordinator()) {
    std::string const name = TRI_ObjectToString(val);
    std::shared_ptr<CollectionInfo> const ci =
        ClusterInfo::instance()->getCollection(vocbase->_name, name);

    if ((*ci).id() == 0 || (*ci).empty()) {
      // not found
      TRI_V8_RETURN_NULL();
    }

    collection = CoordinatorCollection(vocbase, *ci);
  } else {
    collection = GetCollectionFromArgument(vocbase, val);
  }

  if (collection == nullptr) {
    TRI_V8_RETURN_NULL();
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

  std::vector<TRI_vocbase_col_t*> colls;

  // if we are a coordinator, we need to fetch the collection info from the
  // agency
  if (ServerState::instance()->isCoordinator()) {
    colls = GetCollectionsCluster(vocbase);
  } else {
    colls = TRI_CollectionsVocBase(vocbase);
  }

  bool error = false;
  // already create an array of the correct size
  v8::Handle<v8::Array> result = v8::Array::New(isolate);

  size_t const n = colls.size();

  for (size_t i = 0; i < n; ++i) {
    auto collection = colls[i];

    v8::Handle<v8::Value> c = WrapCollection(isolate, collection);

    if (c.IsEmpty()) {
      error = true;
      break;
    }

    result->Set(static_cast<uint32_t>(i), c);
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
    if (ClusterInfo::instance()->doesDatabaseExist(vocbase->_name)) {
      names = GetCollectionNamesCluster(vocbase);
    }
  } else {
    names = TRI_CollectionNamesVocBase(vocbase);
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
  result->Set(j++, TRI_V8_ASCII_STRING("_createStatement()"));
  result->Set(j++, TRI_V8_ASCII_STRING("_document()"));
  result->Set(j++, TRI_V8_ASCII_STRING("_drop()"));
  result->Set(j++, TRI_V8_ASCII_STRING("_dropDatabase()"));
  result->Set(j++, TRI_V8_ASCII_STRING("_executeTransaction()"));
  result->Set(j++, TRI_V8_ASCII_STRING("_exists()"));
  result->Set(j++, TRI_V8_ASCII_STRING("_id"));
  result->Set(j++, TRI_V8_ASCII_STRING("_isSystem()"));
  result->Set(j++, TRI_V8_ASCII_STRING("_databases()"));
  result->Set(j++, TRI_V8_ASCII_STRING("_name()"));
  result->Set(j++, TRI_V8_ASCII_STRING("_path()"));
  result->Set(j++, TRI_V8_ASCII_STRING("_query()"));
  result->Set(j++, TRI_V8_ASCII_STRING("_remove()"));
  result->Set(j++, TRI_V8_ASCII_STRING("_replace()"));
  result->Set(j++, TRI_V8_ASCII_STRING("_update()"));
  result->Set(j++, TRI_V8_ASCII_STRING("_useDatabase()"));
  result->Set(j++, TRI_V8_ASCII_STRING("_version()"));

  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock documentsDocumentRemove
////////////////////////////////////////////////////////////////////////////////

static void JS_RemoveVocbase(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  return RemoveVocbase(args);
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
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock collectionCount
////////////////////////////////////////////////////////////////////////////////

static void JS_CountVocbaseCol(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  TRI_vocbase_col_t const* col =
      TRI_UnwrapClass<TRI_vocbase_col_t>(args.Holder(), WRP_VOCBASE_COL_TYPE);

  if (col == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("count()");
  }
    
  TRI_vocbase_t* vocbase = col->_vocbase;
  
  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }
  
  std::string collectionName(col->name());

  SingleCollectionTransaction trx(V8TransactionContext::Create(vocbase, true), collectionName, TRI_TRANSACTION_READ);

  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  OperationResult opResult = trx.count(collectionName);
  
  res = trx.finish(opResult.code);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  VPackSlice s = opResult.slice();
  TRI_ASSERT(s.isNumber());

  TRI_V8_RETURN(v8::Number::New(isolate, static_cast<double>(s.getNumber<double>())));
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns information about the datafiles
/// `collection.datafiles()`
///
/// Returns information about the datafiles. The collection must be unloaded.
////////////////////////////////////////////////////////////////////////////////

static void JS_DatafilesVocbaseCol(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  TRI_vocbase_col_t* collection =
      TRI_UnwrapClass<TRI_vocbase_col_t>(args.Holder(), WRP_VOCBASE_COL_TYPE);

  if (collection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  TRI_THROW_SHARDING_COLLECTION_NOT_YET_IMPLEMENTED(collection);

  TRI_col_file_structure_t structure;
  {
    READ_LOCKER(readLocker, collection->_lock);

    if (collection->_status != TRI_VOC_COL_STATUS_UNLOADED &&
        collection->_status != TRI_VOC_COL_STATUS_CORRUPTED) {
      TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_COLLECTION_NOT_UNLOADED);
    }

    structure = TRI_FileStructureCollectionDirectory(collection->pathc_str());
  }

  // build result
  v8::Handle<v8::Object> result = v8::Object::New(isolate);

  // journals
  v8::Handle<v8::Array> journals = v8::Array::New(isolate);
  result->Set(TRI_V8_ASCII_STRING("journals"), journals);

  uint32_t i = 0;
  for (auto& it : structure.journals) {
    journals->Set(i++, TRI_V8_STD_STRING(it));
  }

  // compactors
  v8::Handle<v8::Array> compactors = v8::Array::New(isolate);
  result->Set(TRI_V8_ASCII_STRING("compactors"), compactors);

  i = 0;
  for (auto& it : structure.compactors) {
    compactors->Set(i++, TRI_V8_STD_STRING(it));
  }

  // datafiles
  v8::Handle<v8::Array> datafiles = v8::Array::New(isolate);
  result->Set(TRI_V8_ASCII_STRING("datafiles"), datafiles);

  i = 0;
  for (auto& it : structure.datafiles) {
    datafiles->Set(i++, TRI_V8_STD_STRING(it));
  }

  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns information about the datafiles
///
/// @FUN{@FA{collection}.datafileScan(@FA{path})}
///
/// Returns information about the datafiles. The collection must be unloaded.
////////////////////////////////////////////////////////////////////////////////

static void JS_DatafileScanVocbaseCol(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  TRI_vocbase_col_t* collection =
      TRI_UnwrapClass<TRI_vocbase_col_t>(args.Holder(), WRP_VOCBASE_COL_TYPE);

  if (collection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("datafileScan(<path>)");
  }

  std::string path = TRI_ObjectToString(args[0]);

  v8::Handle<v8::Object> result;
  {
    READ_LOCKER(readLocker, collection->_lock);

    if (collection->_status != TRI_VOC_COL_STATUS_UNLOADED &&
        collection->_status != TRI_VOC_COL_STATUS_CORRUPTED) {
      TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_COLLECTION_NOT_UNLOADED);
    }

    DatafileScan scan = TRI_ScanDatafile(path.c_str());

    // build result
    result = v8::Object::New(isolate);

    result->Set(TRI_V8_ASCII_STRING("currentSize"),
                v8::Number::New(isolate, scan.currentSize));
    result->Set(TRI_V8_ASCII_STRING("maximalSize"),
                v8::Number::New(isolate, scan.maximalSize));
    result->Set(TRI_V8_ASCII_STRING("endPosition"),
                v8::Number::New(isolate, scan.endPosition));
    result->Set(TRI_V8_ASCII_STRING("numberMarkers"),
                v8::Number::New(isolate, scan.numberMarkers));
    result->Set(TRI_V8_ASCII_STRING("status"),
                v8::Number::New(isolate, scan.status));
    result->Set(TRI_V8_ASCII_STRING("isSealed"),
                v8::Boolean::New(isolate, scan.isSealed));

    v8::Handle<v8::Array> entries = v8::Array::New(isolate);
    result->Set(TRI_V8_ASCII_STRING("entries"), entries);

    uint32_t i = 0;
    for (auto const& entry : scan.entries) {
      v8::Handle<v8::Object> o = v8::Object::New(isolate);

      o->Set(TRI_V8_ASCII_STRING("position"),
             v8::Number::New(isolate, entry.position));
      o->Set(TRI_V8_ASCII_STRING("size"),
             v8::Number::New(isolate, entry.size));
      o->Set(TRI_V8_ASCII_STRING("realSize"),
             v8::Number::New(isolate, entry.realSize));
      o->Set(TRI_V8_ASCII_STRING("tick"), V8TickId(isolate, entry.tick));
      o->Set(TRI_V8_ASCII_STRING("type"),
             v8::Number::New(isolate, static_cast<int>(entry.type)));
      o->Set(TRI_V8_ASCII_STRING("status"),
             v8::Number::New(isolate, static_cast<int>(entry.status)));

      if (!entry.key.empty()) {
        o->Set(TRI_V8_ASCII_STRING("key"), TRI_V8_STD_STRING(entry.key));
      }

      if (entry.typeName != nullptr) {
        o->Set(TRI_V8_ASCII_STRING("typeName"),
               TRI_V8_ASCII_STRING(entry.typeName));
      }

      if (!entry.diagnosis.empty()) {
        o->Set(TRI_V8_ASCII_STRING("diagnosis"),
               TRI_V8_STD_STRING(entry.diagnosis));
      }

      entries->Set(i++, o);
    }
  }

  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

// .............................................................................
// generate the TRI_vocbase_col_t template
// .............................................................................

void TRI_InitV8collection(v8::Handle<v8::Context> context, TRI_server_t* server,
                          TRI_vocbase_t* vocbase, size_t const threadNumber,
                          TRI_v8_global_t* v8g, v8::Isolate* isolate,
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
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("datafiles"),
                       JS_DatafilesVocbaseCol);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("datafileScan"),
                       JS_DatafileScanVocbaseCol, true);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("document"),
                       JS_DocumentVocbaseCol);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("drop"),
                       JS_DropVocbaseCol);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("exists"),
                       JS_ExistsVocbaseVPack);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("figures"),
                       JS_FiguresVocbaseCol);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("insert"),
                       JS_InsertVocbaseCol);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("leaderResign"),
                       JS_LeaderResign, true);
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
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("rotate"),
                       JS_RotateVocbaseCol);
  TRI_AddMethodVocbase(
      isolate, rt, TRI_V8_ASCII_STRING("save"),
      JS_InsertVocbaseCol);  // note: save is now an alias for insert
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("status"),
                       JS_StatusVocbaseCol);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("TRUNCATE"),
                       JS_TruncateVocbaseCol, true);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("truncateDatafile"),
                       JS_TruncateDatafileVocbaseCol, true);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("tryRepairDatafile"),
                       JS_TryRepairDatafileVocbaseCol, true);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("type"),
                       JS_TypeVocbaseCol);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("unload"),
                       JS_UnloadVocbaseCol);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("update"),
                       JS_UpdateVocbaseCol);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("version"),
                       JS_VersionVocbaseCol);

  TRI_InitV8indexCollection(isolate, rt);

  v8g->VocbaseColTempl.Reset(isolate, rt);
  TRI_AddGlobalFunctionVocbase(isolate, context,
                               TRI_V8_ASCII_STRING("ArangoCollection"),
                               ft->GetFunction());
}
