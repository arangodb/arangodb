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
#include "Basics/Utf8Helper.h"
#include "Basics/conversions.h"
#include "Basics/json-utilities.h"
#include "Basics/ScopeGuard.h"
#include "Cluster/ClusterMethods.h"
#include "Indexes/PrimaryIndex.h"
#include "Storage/Options.h"
#include "Utils/transactions.h"
#include "Utils/V8ResolverGuard.h"
#include "Utils/V8TransactionContext.h"
#include "V8/v8-conv.h"
#include "V8/v8-utils.h"
#include "V8/v8-vpack.h"
#include "V8Server/v8-shape-conv.h"
#include "V8Server/v8-vocbase.h"
#include "V8Server/v8-vocbaseprivate.h"
#include "V8Server/v8-vocindex.h"
#include "V8Server/v8-wrapshapedjson.h"
#include "VocBase/auth.h"
#include "VocBase/DocumentAccessor.h"
#include "VocBase/KeyGenerator.h"
#include "Wal/LogfileManager.h"

#include <velocypack/Builder.h>
#include <velocypack/HexDump.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

#include "unicode/timezone.h"

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
/// @brief internal struct which is used for reading the different option
/// parameters for the save function
////////////////////////////////////////////////////////////////////////////////

struct InsertOptions {
  bool waitForSync = false;
  bool silent = false;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief internal struct which is used for reading the different option
/// parameters for the update and replace functions
////////////////////////////////////////////////////////////////////////////////

struct UpdateOptions {
  bool overwrite = false;
  bool keepNull = true;
  bool mergeObjects = true;
  bool waitForSync = false;
  bool silent = false;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief internal struct which is used for reading the different option
/// parameters for the remove function
////////////////////////////////////////////////////////////////////////////////

struct RemoveOptions {
  bool overwrite = false;
  bool waitForSync = false;
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
/// @brief extract the update policy from a boolean parameter
////////////////////////////////////////////////////////////////////////////////

static inline TRI_doc_update_policy_e ExtractUpdatePolicy(bool overwrite) {
  return (overwrite ? TRI_DOC_UPDATE_LAST_WRITE : TRI_DOC_UPDATE_ERROR);
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
/// @brief extracts a document key from a document
////////////////////////////////////////////////////////////////////////////////

static int ExtractDocumentKey(v8::Isolate* isolate, TRI_v8_global_t* v8g,
                              v8::Handle<v8::Object> const arg,
                              std::unique_ptr<char[]>& key) {
  TRI_ASSERT(v8g != nullptr);
  TRI_ASSERT(key.get() == nullptr);

  v8::Local<v8::Object> obj = arg->ToObject();

  TRI_GET_GLOBAL_STRING(_KeyKey);
  if (obj->Has(_KeyKey)) {
    v8::Handle<v8::Value> v = obj->Get(_KeyKey);

    if (v->IsString()) {
      // keys must not contain any special characters, so it is not necessary
      // to normalize them first
      v8::String::Utf8Value str(v);

      if (*str == 0) {
        return TRI_ERROR_ARANGO_DOCUMENT_KEY_BAD;
      }

      // copy the string from v8
      auto const length = str.length();
      auto buffer = new char[length + 1];
      memcpy(buffer, *str, length);
      buffer[length] = '\0';
      key.reset(buffer);

      return TRI_ERROR_NO_ERROR;
    }

    return TRI_ERROR_ARANGO_DOCUMENT_KEY_BAD;
  }

  return TRI_ERROR_ARANGO_DOCUMENT_KEY_MISSING;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parse document or document handle from a v8 value (string | object)
////////////////////////////////////////////////////////////////////////////////

static int ParseDocumentOrDocumentHandle(v8::Isolate* isolate,
                                         TRI_vocbase_t* vocbase,
                                         CollectionNameResolver const* resolver,
                                         TRI_vocbase_col_t const*& collection,
                                         std::unique_ptr<char[]>& key,
                                         TRI_voc_rid_t& rid,
                                         v8::Handle<v8::Value> const val) {
  v8::HandleScope scope(isolate);

  TRI_ASSERT(key.get() == nullptr);

  // reset the collection identifier and the revision
  std::string collectionName;
  rid = 0;

  // try to extract the collection name, key, and revision from the object
  // passed
  if (!ExtractDocumentHandle(isolate, val, collectionName, key, rid)) {
    return TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD;
  }

  // we have at least a key, we also might have a collection name
  TRI_ASSERT(key.get() != nullptr);

  if (collectionName.empty()) {
    // only a document key without collection name was passed
    if (collection == nullptr) {
      // we do not know the collection
      return TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD;
    }
    // we use the current collection's name
    collectionName = resolver->getCollectionName(collection->_cid);
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
/// @brief cluster coordinator case, parse a key and possible revision
////////////////////////////////////////////////////////////////////////////////

static int ParseKeyAndRef(v8::Isolate* isolate, v8::Handle<v8::Value> const arg,
                          std::string& key, TRI_voc_rid_t& rev) {
  rev = 0;
  if (arg->IsString()) {
    key = TRI_ObjectToString(arg);
  } else if (arg->IsObject()) {
    TRI_GET_GLOBALS();
    v8::Handle<v8::Object> obj = v8::Handle<v8::Object>::Cast(arg);

    TRI_GET_GLOBAL_STRING(_KeyKey);
    TRI_GET_GLOBAL_STRING(_IdKey);
    TRI_GET_GLOBAL_STRING(_RevKey);
    if (obj->Has(_KeyKey) && obj->Get(_KeyKey)->IsString()) {
      key = TRI_ObjectToString(obj->Get(_KeyKey));
    } else if (obj->Has(_IdKey) && obj->Get(_IdKey)->IsString()) {
      key = TRI_ObjectToString(obj->Get(_IdKey));
      // part after / will be taken below
    } else {
      return TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID;
    }
    if (obj->Has(_RevKey) && obj->Get(_RevKey)->IsString()) {
      rev = TRI_ObjectToUInt64(obj->Get(_RevKey), true);
    }
  } else {
    return TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID;
  }

  size_t pos = key.find('/');
  if (pos != std::string::npos) {
    key = key.substr(pos + 1);
  }
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a document, coordinator case in a cluster
///
/// If generateDocument is false, this implements ".exists" rather than
/// ".document".
////////////////////////////////////////////////////////////////////////////////

static void DocumentVocbaseColCoordinator(
    TRI_vocbase_col_t const* collection,
    v8::FunctionCallbackInfo<v8::Value> const& args, bool generateDocument) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  // First get the initial data:
  std::string const dbname(collection->_dbName);
  // TODO: someone might rename the collection while we're reading its name...
  std::string const collname(collection->_name);

  std::string key;
  TRI_voc_rid_t rev = 0;
  int error = ParseKeyAndRef(isolate, args[0], key, rev);

  if (error != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(error);
  }

  arangodb::rest::HttpResponse::HttpResponseCode responseCode;
  std::unique_ptr<std::map<std::string, std::string>> headers(
      new std::map<std::string, std::string>());
  std::map<std::string, std::string> resultHeaders;
  std::string resultBody;

  error = arangodb::getDocumentOnCoordinator(
      dbname, collname, key, rev, headers, generateDocument, responseCode,
      resultHeaders, resultBody);

  if (error != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(error);
  }

  // report what the DBserver told us: this could now be 200 or
  // 404/412
  // For the error processing we have to distinguish whether we are in
  // the ".exists" case (generateDocument==false) or the ".document" case
  // (generateDocument==true).
  VPackBuilder builder;
  if (generateDocument) {
    try {
      VPackParser parser(builder);
      parser.parse(resultBody);
    } catch (...) {
      // Do Nothing with the error
      // Just make sure the builder is clear
      builder.clear();
    }
  }
  VPackSlice slice = builder.slice();

  if (responseCode >= arangodb::rest::HttpResponse::BAD) {
    if (!slice.isObject()) {
      if (generateDocument) {
        TRI_V8_THROW_EXCEPTION(TRI_ERROR_INTERNAL);
      }
      TRI_V8_RETURN_FALSE();
    }
    if (generateDocument) {
      int errorNum = arangodb::basics::VelocyPackHelper::getNumericValue<int>(
          slice, "errorNum", 0);
      std::string errorMessage =
          arangodb::basics::VelocyPackHelper::getStringValue(
              slice, "errorMessage", "");
      TRI_V8_THROW_EXCEPTION_MESSAGE(errorNum, errorMessage);
    } else {
      TRI_V8_RETURN_FALSE();
    }
  }
  if (generateDocument) {
    v8::Handle<v8::Value> ret = TRI_VPackToV8(isolate, slice);
    TRI_V8_RETURN(ret);
  } else {
    // Note that for this case we will never get a 304 "NOT_MODIFIED"
    TRI_V8_RETURN_TRUE();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a document and returns it
////////////////////////////////////////////////////////////////////////////////

static void DocumentVocbaseCol(
    bool useCollection, v8::FunctionCallbackInfo<v8::Value> const& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  // first and only argument should be a document idenfifier
  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("document(<document-handle>)");
  }

  std::unique_ptr<char[]> key;
  TRI_voc_rid_t rid;
  TRI_vocbase_t* vocbase;
  TRI_vocbase_col_t const* col = nullptr;

  if (useCollection) {
    // called as db.collection.document()
    col =
        TRI_UnwrapClass<TRI_vocbase_col_t>(args.Holder(), WRP_VOCBASE_COL_TYPE);

    if (col == nullptr) {
      TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
    }

    vocbase = col->_vocbase;
  } else {
    // called as db._document()
    vocbase = GetContextVocBase(isolate);
  }

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  V8ResolverGuard resolver(vocbase);
  int err = ParseDocumentOrDocumentHandle(
      isolate, vocbase, resolver.getResolver(), col, key, rid, args[0]);

  LocalCollectionGuard g(useCollection ? nullptr
                                       : const_cast<TRI_vocbase_col_t*>(col));

  if (key.get() == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD);
  }

  if (err != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(err);
  }

  TRI_ASSERT(col != nullptr);
  TRI_ASSERT(key.get() != nullptr);

  if (ServerState::instance()->isCoordinator()) {
    DocumentVocbaseColCoordinator(col, args, true);
    return;
  }

  SingleCollectionReadOnlyTransaction trx(new V8TransactionContext(true),
                                          vocbase, col->_cid);

  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  if (trx.orderDitch(trx.trxCollection()) == nullptr) {
    TRI_V8_THROW_EXCEPTION_MEMORY();
  }

  v8::Handle<v8::Value> result;
  TRI_doc_mptr_copy_t document;
  res = trx.read(&document, key.get());
  res = trx.finish(res);

  TRI_ASSERT(trx.hasDitch());

  if (res == TRI_ERROR_NO_ERROR) {
    result = TRI_WrapShapedJson<SingleCollectionReadOnlyTransaction>(
        isolate, trx, col->_cid, document.getDataPtr());
  }

  if (res != TRI_ERROR_NO_ERROR ||
      document.getDataPtr() == nullptr) {  // PROTECTED by trx here
    if (res == TRI_ERROR_NO_ERROR) {
      res = TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND;
    }

    TRI_V8_THROW_EXCEPTION(res);
  }

  if (rid != 0 && document._rid != rid) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_CONFLICT,
                                   "revision not found");
  }

  TRI_V8_RETURN(result);
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

static void ExistsVocbaseCol(bool useCollection,
                             v8::FunctionCallbackInfo<v8::Value> const& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  // first and only argument should be a document idenfifier
  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("exists(<document-handle>)");
  }

  std::unique_ptr<char[]> key;
  TRI_voc_rid_t rid;
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

  V8ResolverGuard resolver(vocbase);
  int err = ParseDocumentOrDocumentHandle(
      isolate, vocbase, resolver.getResolver(), col, key, rid, args[0]);

  LocalCollectionGuard g(useCollection ? nullptr
                                       : const_cast<TRI_vocbase_col_t*>(col));

  if (key.get() == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD);
  }
  if (err != TRI_ERROR_NO_ERROR) {
    if (err == TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND) {
      TRI_V8_RETURN_FALSE();
    }
    TRI_V8_THROW_EXCEPTION(err);
  }

  TRI_ASSERT(col != nullptr);
  TRI_ASSERT(key.get() != nullptr);

  if (ServerState::instance()->isCoordinator()) {
    DocumentVocbaseColCoordinator(col, args, false);
    return;
  }

  SingleCollectionReadOnlyTransaction trx(new V8TransactionContext(true),
                                          vocbase, col->_cid);

  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  if (trx.orderDitch(trx.trxCollection()) == nullptr) {
    TRI_V8_THROW_EXCEPTION_MEMORY();
  }

  TRI_doc_mptr_copy_t document;
  res = trx.read(&document, key.get());
  res = trx.finish(res);

  if (res != TRI_ERROR_NO_ERROR ||
      document.getDataPtr() == nullptr) {  // PROTECTED by trx here
    if (res == TRI_ERROR_NO_ERROR) {
      res = TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND;
    }
  }

  if (res == TRI_ERROR_NO_ERROR && rid != 0 && document._rid != rid) {
    res = TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND;
  }

  if (res == TRI_ERROR_NO_ERROR) {
    TRI_V8_RETURN_TRUE();
  } else if (res == TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND) {
    TRI_V8_RETURN_FALSE();
  }

  TRI_V8_THROW_EXCEPTION(res);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief modifies a document, coordinator case in a cluster
////////////////////////////////////////////////////////////////////////////////

static void ModifyVocbaseColCoordinator(
    TRI_vocbase_col_t const* collection, TRI_doc_update_policy_e policy,
    bool waitForSync, bool isPatch,
    bool keepNull,      // only counts if isPatch==true
    bool mergeObjects,  // only counts if isPatch==true bool silent,
    bool silent, v8::FunctionCallbackInfo<v8::Value> const& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  TRI_ASSERT(collection != nullptr);

  // First get the initial data:
  std::string const dbname(collection->_dbName);
  std::string const collname(collection->_name);

  std::string key;
  TRI_voc_rid_t rev = 0;
  int error = ParseKeyAndRef(isolate, args[0], key, rev);

  if (error != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(error);
  }

  std::unique_ptr<TRI_json_t> json(TRI_ObjectToJson(isolate, args[1]));
  if (!TRI_IsObjectJson(json.get())) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
  }

  arangodb::rest::HttpResponse::HttpResponseCode responseCode;
  std::unique_ptr<std::map<std::string, std::string>> headers(
      new std::map<std::string, std::string>());
  std::map<std::string, std::string> resultHeaders;
  std::string resultBody;

  error = arangodb::modifyDocumentOnCoordinator(
      dbname, collname, key, rev, policy, waitForSync, isPatch, keepNull,
      mergeObjects, json, headers, responseCode, resultHeaders, resultBody);

  if (error != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(error);
  }

  // report what the DBserver told us: this could now be 201/202 or
  // 400/404
  json.reset(TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, resultBody.c_str()));
  if (responseCode >= arangodb::rest::HttpResponse::BAD) {
    if (!TRI_IsObjectJson(json.get())) {
      TRI_V8_THROW_EXCEPTION(TRI_ERROR_INTERNAL);
    }
    int errorNum = 0;
    TRI_json_t* subjson = TRI_LookupObjectJson(json.get(), "errorNum");
    if (TRI_IsNumberJson(subjson)) {
      errorNum = static_cast<int>(subjson->_value._number);
    }
    std::string errorMessage;
    subjson = TRI_LookupObjectJson(json.get(), "errorMessage");
    if (TRI_IsStringJson(subjson)) {
      errorMessage = std::string(subjson->_value._string.data,
                                 subjson->_value._string.length - 1);
    }
    TRI_V8_THROW_EXCEPTION_MESSAGE(errorNum, errorMessage);
  }

  if (silent) {
    TRI_V8_RETURN_TRUE();
  } else {
    v8::Handle<v8::Value> ret = TRI_ObjectJson(isolate, json.get());
    TRI_V8_RETURN(ret);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief replaces a document
////////////////////////////////////////////////////////////////////////////////

static void ReplaceVocbaseCol(bool useCollection,
                              v8::FunctionCallbackInfo<v8::Value> const& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);
  UpdateOptions options;
  TRI_doc_update_policy_e policy = TRI_DOC_UPDATE_ERROR;

  // check the arguments
  uint32_t const argLength = args.Length();
  TRI_GET_GLOBALS();

  if (argLength < 2) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "replace(<document>, <data>, {overwrite: booleanValue, waitForSync: "
        "booleanValue})");
  }

  // we're only accepting "real" object documents
  if (!args[1]->IsObject() || args[1]->IsArray()) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
  }

  if (args.Length() > 2) {
    if (args[2]->IsObject()) {
      v8::Handle<v8::Object> optionsObject = args[2].As<v8::Object>();
      TRI_GET_GLOBAL_STRING(OverwriteKey);
      if (optionsObject->Has(OverwriteKey)) {
        options.overwrite =
            TRI_ObjectToBoolean(optionsObject->Get(OverwriteKey));
        policy = ExtractUpdatePolicy(options.overwrite);
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
    } else {  // old variant replace(<document>, <data>, <overwrite>,
              // <waitForSync>)
      options.overwrite = TRI_ObjectToBoolean(args[2]);
      policy = ExtractUpdatePolicy(options.overwrite);
      if (argLength > 3) {
        options.waitForSync = TRI_ObjectToBoolean(args[3]);
      }
    }
  }

  std::unique_ptr<char[]> key;
  TRI_voc_rid_t rid;
  TRI_voc_rid_t actualRevision = 0;

  TRI_vocbase_t* vocbase;
  TRI_vocbase_col_t const* col = nullptr;

  if (useCollection) {
    // called as db.collection.replace()
    col =
        TRI_UnwrapClass<TRI_vocbase_col_t>(args.Holder(), WRP_VOCBASE_COL_TYPE);

    if (col == nullptr) {
      TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
    }

    vocbase = col->_vocbase;
  } else {
    // called as db._replace()
    vocbase = GetContextVocBase(isolate);
  }

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  V8ResolverGuard resolver(vocbase);
  int err = ParseDocumentOrDocumentHandle(
      isolate, vocbase, resolver.getResolver(), col, key, rid, args[0]);

  LocalCollectionGuard g(useCollection ? nullptr
                                       : const_cast<TRI_vocbase_col_t*>(col));

  if (key.get() == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD);
  }

  if (err != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(err);
  }

  TRI_ASSERT(col != nullptr);
  TRI_ASSERT(key.get() != nullptr);

  if (ServerState::instance()->isCoordinator()) {
    ModifyVocbaseColCoordinator(
        col, policy, options.waitForSync,
        false,  // isPatch
        true,   // keepNull, does not matter
        false,  // mergeObjects, does not matter options.silent,
        options.silent, args);
    return;
  }

  SingleCollectionWriteTransaction<1> trx(new V8TransactionContext(true),
                                          vocbase, col->_cid);
  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  TRI_document_collection_t* document = trx.documentCollection();
  TRI_memory_zone_t* zone =
      document->getShaper()->memoryZone();  // PROTECTED by trx here

  TRI_doc_mptr_copy_t mptr;

  if (trx.orderDitch(trx.trxCollection()) == nullptr) {
    TRI_V8_THROW_EXCEPTION_MEMORY();
  }

  // we must lock here, because below we are
  // - reading the old document in coordinator case
  // - creating a shape, which might trigger a write into the collection
  trx.lockWrite();

  if (ServerState::instance()->isDBServer()) {
    // compare attributes in shardKeys
    std::string const cidString = StringUtils::itoa(document->_info.planId());

    TRI_json_t* json = TRI_ObjectToJson(isolate, args[1]);

    if (json == nullptr) {
      TRI_V8_THROW_EXCEPTION_MEMORY();
    }

    res = trx.read(&mptr, key.get());

    if (res != TRI_ERROR_NO_ERROR ||
        mptr.getDataPtr() == nullptr) {  // PROTECTED by trx here
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
      TRI_V8_THROW_EXCEPTION(res);
    }

    TRI_shaped_json_t shaped;
    TRI_EXTRACT_SHAPED_JSON_MARKER(shaped,
                                   mptr.getDataPtr());  // PROTECTED by trx here
    TRI_json_t* old = TRI_JsonShapedJson(document->getShaper(),
                                         &shaped);  // PROTECTED by trx here

    if (old == nullptr) {
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
      TRI_V8_THROW_EXCEPTION_MEMORY();
    }

    if (shardKeysChanged(col->_dbName, cidString, old, json, false)) {
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, old);
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
      TRI_V8_THROW_EXCEPTION(
          TRI_ERROR_CLUSTER_MUST_NOT_CHANGE_SHARDING_ATTRIBUTES);
    }

    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, old);
  }

  TRI_shaped_json_t* shaped = TRI_ShapedJsonV8Object(
      isolate, args[1], document->getShaper(), true);  // PROTECTED by trx here

  if (shaped == nullptr) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(
        TRI_errno(), "<data> cannot be converted into JSON shape");
  }

  res = trx.updateDocument(key.get(), &mptr, shaped, policy,
                           options.waitForSync, rid, &actualRevision);

  res = trx.finish(res);

  TRI_FreeShapedJson(zone, shaped);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  TRI_ASSERT(mptr.getDataPtr() != nullptr);  // PROTECTED by trx here

  if (options.silent) {
    TRI_V8_RETURN_TRUE();
  } else {
    char const* docKey =
        TRI_EXTRACT_MARKER_KEY(&mptr);  // PROTECTED by trx here

    v8::Handle<v8::Object> result = v8::Object::New(isolate);
    TRI_GET_GLOBAL_STRING(_IdKey);
    TRI_GET_GLOBAL_STRING(_RevKey);
    TRI_GET_GLOBAL_STRING(_OldRevKey);
    TRI_GET_GLOBAL_STRING(_KeyKey);
    result->Set(
        _IdKey,
        V8DocumentId(isolate, trx.resolver()->getCollectionName(col->_cid),
                     docKey));
    result->Set(_RevKey, V8RevisionId(isolate, mptr._rid));
    result->Set(_OldRevKey, V8RevisionId(isolate, actualRevision));
    result->Set(_KeyKey, TRI_V8_STRING(docKey));

    TRI_V8_RETURN(result);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief inserts a document
////////////////////////////////////////////////////////////////////////////////

static void InsertVocbaseCol(TRI_vocbase_col_t* col, uint32_t argOffset,
                             v8::FunctionCallbackInfo<v8::Value> const& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  uint32_t const argLength = args.Length() - argOffset;
  TRI_GET_GLOBALS();

  if (argLength < 1 || argLength > 2) {
    TRI_V8_THROW_EXCEPTION_USAGE("insert(<data>, [<waitForSync>])");
  }

  InsertOptions options;
  if (argLength > 1 && args[1 + argOffset]->IsObject()) {
    v8::Handle<v8::Object> optionsObject = args[1 + argOffset].As<v8::Object>();
    TRI_GET_GLOBAL_STRING(WaitForSyncKey);
    if (optionsObject->Has(WaitForSyncKey)) {
      options.waitForSync =
          TRI_ObjectToBoolean(optionsObject->Get(WaitForSyncKey));
    }
    TRI_GET_GLOBAL_STRING(SilentKey);
    if (optionsObject->Has(SilentKey)) {
      options.silent = TRI_ObjectToBoolean(optionsObject->Get(SilentKey));
    }
  } else {
    options.waitForSync = ExtractWaitForSync(args, 2 + argOffset);
  }

  if (!args[argOffset]->IsObject() || args[argOffset]->IsArray()) {
    // invalid value type. must be a document
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
  }

  // set document key
  std::unique_ptr<char[]> key;
  int res = ExtractDocumentKey(isolate, v8g, args[argOffset]->ToObject(), key);

  if (res != TRI_ERROR_NO_ERROR &&
      res != TRI_ERROR_ARANGO_DOCUMENT_KEY_MISSING) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  SingleCollectionWriteTransaction<1> trx(new V8TransactionContext(true),
                                          col->_vocbase, col->_cid);

  res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  // fetch a barrier so nobody unlinks datafiles with the shapes & attributes we
  // might
  // need for this document
  if (trx.orderDitch(trx.trxCollection()) == nullptr) {
    TRI_V8_THROW_EXCEPTION_MEMORY();
  }

  TRI_document_collection_t* document = trx.documentCollection();
  TRI_memory_zone_t* zone =
      document->getShaper()->memoryZone();  // PROTECTED by trx from above

  TRI_shaped_json_t* shaped =
      TRI_ShapedJsonV8Object(isolate, args[argOffset], document->getShaper(),
                             true);  // PROTECTED by trx from above

  if (shaped == nullptr) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(
        TRI_errno(), "<data> cannot be converted into JSON shape");
  }

  TRI_doc_mptr_copy_t mptr;
  res = trx.createDocument(key.get(), &mptr, shaped, options.waitForSync);

  res = trx.finish(res);

  TRI_FreeShapedJson(zone, shaped);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  TRI_ASSERT(mptr.getDataPtr() != nullptr);  // PROTECTED by trx here

  if (options.silent) {
    TRI_V8_RETURN_TRUE();
  } else {
    char const* docKey =
        TRI_EXTRACT_MARKER_KEY(&mptr);  // PROTECTED by trx here

    v8::Handle<v8::Object> result = v8::Object::New(isolate);
    TRI_GET_GLOBAL_STRING(_IdKey);
    TRI_GET_GLOBAL_STRING(_RevKey);
    TRI_GET_GLOBAL_STRING(_KeyKey);
    result->Set(
        _IdKey,
        V8DocumentId(isolate, trx.resolver()->getCollectionName(col->_cid),
                     docKey));
    result->Set(_RevKey, V8RevisionId(isolate, mptr._rid));
    result->Set(_KeyKey, TRI_V8_STRING(docKey));

    TRI_V8_RETURN(result);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds system attributes to the VPack
////////////////////////////////////////////////////////////////////////////////

static int AddSystemAttributes(Transaction* trx, TRI_voc_cid_t cid,
                               TRI_document_collection_t* document,
                               VPackBuilder& builder) {
  // generate a new tick value
  uint64_t const tick = TRI_NewTickServer();

  if (!builder.hasKey(TRI_VOC_ATTRIBUTE_KEY)) {
    // "_key" attribute not present in object
    std::string const keyString = document->_keyGenerator->generate(tick);
    builder.add(TRI_VOC_ATTRIBUTE_KEY, VPackValue(keyString));
  } else {
    // "_key" attribute is present in object
    VPackSlice key(builder.getKey(TRI_VOC_ATTRIBUTE_KEY));

    if (!key.isString()) {
      return TRI_ERROR_ARANGO_DOCUMENT_KEY_BAD;
    }

    int res = document->_keyGenerator->validate(key.copyString(), false);

    if (res != TRI_ERROR_NO_ERROR) {
      // invalid key value
      return res;
    }
  }

  // now add _id attribute
  uint8_t* p = builder.add(TRI_VOC_ATTRIBUTE_ID,
                           VPackValuePair(9ULL, VPackValueType::Custom));
  *p++ = 0xf0;
  arangodb::velocypack::storeUInt64(p, cid);

  // now add _rev attribute
  p = builder.add(TRI_VOC_ATTRIBUTE_REV,
                  VPackValuePair(9ULL, VPackValueType::Custom));
  *p++ = 0xf1;
  arangodb::velocypack::storeUInt64(p, tick);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief inserts a document, using a VPack marker
////////////////////////////////////////////////////////////////////////////////

static void InsertVocbaseVPack(
    TRI_vocbase_col_t* col, v8::FunctionCallbackInfo<v8::Value> const& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  uint32_t const argLength = args.Length();
  TRI_GET_GLOBALS();

  if (argLength < 1 || argLength > 2) {
    TRI_V8_THROW_EXCEPTION_USAGE("insert(<data>, [<waitForSync>])");
  }

  InsertOptions options;
  if (argLength > 1 && args[1]->IsObject()) {
    v8::Handle<v8::Object> optionsObject = args[1].As<v8::Object>();
    TRI_GET_GLOBAL_STRING(WaitForSyncKey);
    if (optionsObject->Has(WaitForSyncKey)) {
      options.waitForSync =
          TRI_ObjectToBoolean(optionsObject->Get(WaitForSyncKey));
    }
    TRI_GET_GLOBAL_STRING(SilentKey);
    if (optionsObject->Has(SilentKey)) {
      options.silent = TRI_ObjectToBoolean(optionsObject->Get(SilentKey));
    }
  } else {
    options.waitForSync = ExtractWaitForSync(args, 2);
  }

  if (!args[0]->IsObject() || args[0]->IsArray()) {
    // invalid value type. must be a document
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
  }

  // load collection
  SingleCollectionWriteTransaction<1> trx(new V8TransactionContext(true),
                                          col->_vocbase, col->_cid);

  int res = trx.openCollections();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  VPackBuilder builder(trx.vpackOptions());
  res = TRI_V8ToVPack(isolate, builder, args[0]->ToObject(), true);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  TRI_document_collection_t* document = trx.documentCollection();

  // the AddSystemAttributes() needs the collection already loaded because it
  // references the
  // collection's key generator
  res = AddSystemAttributes(&trx, col->_cid, document, builder);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  builder.close();

  VPackSlice slice(builder.slice());

  // fetch a barrier so nobody unlinks datafiles with the shapes & attributes we
  // might
  // need for this document
  if (trx.orderDitch(trx.trxCollection()) == nullptr) {
    TRI_V8_THROW_EXCEPTION_MEMORY();
  }

  res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  TRI_doc_mptr_copy_t mptr;
  res = document->insert(&trx, &slice, &mptr,
                         !trx.isLocked(document, TRI_TRANSACTION_WRITE),
                         options.waitForSync);
  res = trx.finish(res);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  TRI_ASSERT(mptr.getDataPtr() != nullptr);  // PROTECTED by trx here

  if (options.silent) {
    TRI_V8_RETURN_TRUE();
  }

  std::string key = TRI_EXTRACT_MARKER_KEY(&trx, &mptr);  // PROTECTED by trx here

  v8::Handle<v8::Object> result = v8::Object::New(isolate);
  TRI_GET_GLOBAL_STRING(_IdKey);
  TRI_GET_GLOBAL_STRING(_RevKey);
  TRI_GET_GLOBAL_STRING(_KeyKey);
  result->Set(
      _IdKey,
      V8DocumentId(isolate, trx.resolver()->getCollectionName(col->_cid), key));
  result->Set(_RevKey,
              V8RevisionId(isolate, TRI_EXTRACT_MARKER_RID(&trx, &mptr)));
  result->Set(_KeyKey, TRI_V8_STD_STRING(key));

  TRI_V8_RETURN(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief updates (patches) a document
////////////////////////////////////////////////////////////////////////////////

static void UpdateVocbaseCol(bool useCollection,
                             v8::FunctionCallbackInfo<v8::Value> const& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);
  UpdateOptions options;
  TRI_doc_update_policy_e policy = TRI_DOC_UPDATE_ERROR;

  // check the arguments
  uint32_t const argLength = args.Length();

  TRI_GET_GLOBALS();

  if (argLength < 2 || argLength > 5) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "update(<document>, <data>, {overwrite: booleanValue, keepNull: "
        "booleanValue, mergeObjects: booleanValue, waitForSync: "
        "booleanValue})");
  }

  if (argLength > 2) {
    if (args[2]->IsObject()) {
      v8::Handle<v8::Object> optionsObject = args[2].As<v8::Object>();
      TRI_GET_GLOBAL_STRING(OverwriteKey);
      if (optionsObject->Has(OverwriteKey)) {
        options.overwrite =
            TRI_ObjectToBoolean(optionsObject->Get(OverwriteKey));
        policy = ExtractUpdatePolicy(options.overwrite);
      }
      TRI_GET_GLOBAL_STRING(KeepNullKey);
      if (optionsObject->Has(KeepNullKey)) {
        options.keepNull = TRI_ObjectToBoolean(optionsObject->Get(KeepNullKey));
      }
      TRI_GET_GLOBAL_STRING(MergeObjectsKey);
      if (optionsObject->Has(MergeObjectsKey)) {
        options.mergeObjects =
            TRI_ObjectToBoolean(optionsObject->Get(MergeObjectsKey));
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
    } else {  // old variant update(<document>, <data>, <overwrite>, <keepNull>,
              // <waitForSync>)
      options.overwrite = TRI_ObjectToBoolean(args[2]);
      policy = ExtractUpdatePolicy(options.overwrite);
      if (argLength > 3) {
        options.keepNull = TRI_ObjectToBoolean(args[3]);
      }
      if (argLength > 4) {
        options.waitForSync = TRI_ObjectToBoolean(args[4]);
      }
    }
  }

  std::unique_ptr<char[]> key;
  TRI_voc_rid_t rid;
  TRI_voc_rid_t actualRevision = 0;
  TRI_vocbase_t* vocbase;
  TRI_vocbase_col_t const* col = nullptr;

  if (useCollection) {
    // called as db.collection.update()
    col =
        TRI_UnwrapClass<TRI_vocbase_col_t>(args.Holder(), WRP_VOCBASE_COL_TYPE);

    if (col == nullptr) {
      TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
    }

    vocbase = col->_vocbase;
  } else {
    // called as db._update()
    vocbase = GetContextVocBase(isolate);
  }

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  V8ResolverGuard resolver(vocbase);
  int err = ParseDocumentOrDocumentHandle(
      isolate, vocbase, resolver.getResolver(), col, key, rid, args[0]);

  LocalCollectionGuard g(useCollection ? nullptr
                                       : const_cast<TRI_vocbase_col_t*>(col));

  if (key.get() == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD);
  }

  if (err != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(err);
  }

  TRI_ASSERT(col != nullptr);
  TRI_ASSERT(key.get() != nullptr);

  if (ServerState::instance()->isCoordinator()) {
    ModifyVocbaseColCoordinator(col, policy, options.waitForSync,
                                true,  // isPatch
                                options.keepNull, options.mergeObjects,
                                options.silent, args);
    return;
  }

  if (!args[1]->IsObject() || args[1]->IsArray()) {
    // we're only accepting "real" object documents
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
  }

  TRI_json_t* json = TRI_ObjectToJson(isolate, args[1]);

  if (json == nullptr) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_errno(), "<data> is no valid JSON");
  }

  SingleCollectionWriteTransaction<1> trx(new V8TransactionContext(true),
                                          vocbase, col->_cid);
  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
    TRI_V8_THROW_EXCEPTION(res);
  }

  // we must use a write-lock that spans both the initial read and the update.
  // otherwise the operation is not atomic
  trx.lockWrite();

  TRI_doc_mptr_copy_t mptr;
  res = trx.read(&mptr, key.get());

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
    TRI_V8_THROW_EXCEPTION(res);
  }

  if (trx.orderDitch(trx.trxCollection()) == nullptr) {
    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
    TRI_V8_THROW_EXCEPTION_MEMORY();
  }

  TRI_document_collection_t* document = trx.documentCollection();
  TRI_memory_zone_t* zone =
      document->getShaper()->memoryZone();  // PROTECTED by trx here

  TRI_shaped_json_t shaped;
  TRI_EXTRACT_SHAPED_JSON_MARKER(shaped,
                                 mptr.getDataPtr());  // PROTECTED by trx here
  TRI_json_t* old = TRI_JsonShapedJson(document->getShaper(),
                                       &shaped);  // PROTECTED by trx here

  if (old == nullptr) {
    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
    TRI_V8_THROW_EXCEPTION_MEMORY();
  }

  if (ServerState::instance()->isDBServer()) {
    // compare attributes in shardKeys
    std::string const cidString = StringUtils::itoa(document->_info.planId());

    if (shardKeysChanged(col->_dbName, cidString, old, json, true)) {
      TRI_FreeJson(document->getShaper()->memoryZone(),
                   old);  // PROTECTED by trx here
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);

      TRI_V8_THROW_EXCEPTION(
          TRI_ERROR_CLUSTER_MUST_NOT_CHANGE_SHARDING_ATTRIBUTES);
    }
  }

  TRI_json_t* patchedJson = TRI_MergeJson(
      TRI_UNKNOWN_MEM_ZONE, old, json, !options.keepNull, options.mergeObjects);
  TRI_FreeJson(zone, old);
  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);

  if (patchedJson == nullptr) {
    TRI_V8_THROW_EXCEPTION_MEMORY();
  }

  res = trx.updateDocument(key.get(), &mptr, patchedJson, policy,
                           options.waitForSync, rid, &actualRevision);

  res = trx.finish(res);

  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, patchedJson);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  TRI_ASSERT(mptr.getDataPtr() != nullptr);  // PROTECTED by trx here

  if (options.silent) {
    TRI_V8_RETURN_TRUE();
  } else {
    char const* docKey =
        TRI_EXTRACT_MARKER_KEY(&mptr);  // PROTECTED by trx here

    v8::Handle<v8::Object> result = v8::Object::New(isolate);
    TRI_GET_GLOBAL_STRING(_IdKey);
    TRI_GET_GLOBAL_STRING(_RevKey);
    TRI_GET_GLOBAL_STRING(_OldRevKey);
    TRI_GET_GLOBAL_STRING(_KeyKey);
    result->Set(
        _IdKey,
        V8DocumentId(isolate, trx.resolver()->getCollectionName(col->_cid),
                     docKey));
    result->Set(_RevKey, V8RevisionId(isolate, mptr._rid));
    result->Set(_OldRevKey, V8RevisionId(isolate, actualRevision));
    result->Set(_KeyKey, TRI_V8_STRING(docKey));

    TRI_V8_RETURN(result);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes a document, coordinator case in a cluster
////////////////////////////////////////////////////////////////////////////////

static void RemoveVocbaseColCoordinator(
    TRI_vocbase_col_t const* collection, TRI_doc_update_policy_e policy,
    bool waitForSync, v8::FunctionCallbackInfo<v8::Value> const& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  // First get the initial data:
  std::string const dbname(collection->_dbName);
  std::string const collname(collection->_name);

  std::string key;
  TRI_voc_rid_t rev = 0;
  int error = ParseKeyAndRef(isolate, args[0], key, rev);

  if (error != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(error);
  }

  arangodb::rest::HttpResponse::HttpResponseCode responseCode;
  std::map<std::string, std::string> resultHeaders;
  std::string resultBody;
  std::unique_ptr<std::map<std::string, std::string>> headers(
      new std::map<std::string, std::string>());

  error = arangodb::deleteDocumentOnCoordinator(
      dbname, collname, key, rev, policy, waitForSync, headers, responseCode,
      resultHeaders, resultBody);

  if (error != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(error);
  }
  // report what the DBserver told us: this could now be 200/202 or
  // 404/412
  TRI_json_t* json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, resultBody.c_str());
  if (responseCode >= arangodb::rest::HttpResponse::BAD) {
    if (!TRI_IsObjectJson(json)) {
      if (nullptr != json) {
        TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
      }
      TRI_V8_THROW_EXCEPTION(TRI_ERROR_INTERNAL);
    }
    int errorNum = 0;
    TRI_json_t* subjson = TRI_LookupObjectJson(json, "errorNum");
    if (TRI_IsNumberJson(subjson)) {
      errorNum = static_cast<int>(subjson->_value._number);
    }
    std::string errorMessage;
    subjson = TRI_LookupObjectJson(json, "errorMessage");
    if (TRI_IsStringJson(subjson)) {
      errorMessage = std::string(subjson->_value._string.data,
                                 subjson->_value._string.length - 1);
    }
    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);

    if (errorNum == TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND &&
        policy == TRI_DOC_UPDATE_LAST_WRITE) {
      // this is not considered an error
      TRI_V8_RETURN_FALSE();
    }

    TRI_V8_THROW_EXCEPTION_MESSAGE(errorNum, errorMessage);
  }

  if (json != nullptr) {
    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
  }

  TRI_V8_RETURN_TRUE();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes a document
////////////////////////////////////////////////////////////////////////////////

static void RemoveVocbaseCol(bool useCollection,
                             v8::FunctionCallbackInfo<v8::Value> const& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);
  RemoveOptions options;
  TRI_doc_update_policy_e policy = TRI_DOC_UPDATE_ERROR;

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
        options.overwrite =
            TRI_ObjectToBoolean(optionsObject->Get(OverwriteKey));
        policy = ExtractUpdatePolicy(options.overwrite);
      }
      TRI_GET_GLOBAL_STRING(WaitForSyncKey);
      if (optionsObject->Has(WaitForSyncKey)) {
        options.waitForSync =
            TRI_ObjectToBoolean(optionsObject->Get(WaitForSyncKey));
      }
    } else {  // old variant replace(<document>, <data>, <overwrite>,
              // <waitForSync>)
      options.overwrite = TRI_ObjectToBoolean(args[1]);
      policy = ExtractUpdatePolicy(options.overwrite);
      if (argLength > 2) {
        options.waitForSync = TRI_ObjectToBoolean(args[2]);
      }
    }
  }

  std::unique_ptr<char[]> key;
  TRI_voc_rid_t rid;
  TRI_voc_rid_t actualRevision = 0;
  TRI_vocbase_t* vocbase;
  TRI_vocbase_col_t const* col = nullptr;

  if (useCollection) {
    // called as db.collection.remove()
    col =
        TRI_UnwrapClass<TRI_vocbase_col_t>(args.Holder(), WRP_VOCBASE_COL_TYPE);

    if (col == nullptr) {
      TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
    }

    vocbase = col->_vocbase;
  } else {
    // called as db._remove()
    vocbase = GetContextVocBase(isolate);
  }

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  V8ResolverGuard resolver(vocbase);
  int err = ParseDocumentOrDocumentHandle(
      isolate, vocbase, resolver.getResolver(), col, key, rid, args[0]);

  LocalCollectionGuard g(useCollection ? nullptr
                                       : const_cast<TRI_vocbase_col_t*>(col));

  if (key.get() == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD);
  }

  if (err != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(err);
  }

  TRI_ASSERT(col != nullptr);
  TRI_ASSERT(key.get() != nullptr);

  if (ServerState::instance()->isCoordinator()) {
    RemoveVocbaseColCoordinator(col, policy, options.waitForSync, args);
    return;
  }

  SingleCollectionWriteTransaction<1> trx(new V8TransactionContext(true),
                                          vocbase, col->_cid);
  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  res = trx.deleteDocument(key.get(), policy, options.waitForSync, rid,
                           &actualRevision);
  res = trx.finish(res);

  if (res != TRI_ERROR_NO_ERROR) {
    if (res == TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND &&
        policy == TRI_DOC_UPDATE_LAST_WRITE) {
      TRI_V8_RETURN_FALSE();
    } else {
      TRI_V8_THROW_EXCEPTION(res);
    }
  }

  TRI_V8_RETURN_TRUE();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes a document, using a VPack marker
////////////////////////////////////////////////////////////////////////////////

static void RemoveVocbaseVPack(
    bool useCollection, v8::FunctionCallbackInfo<v8::Value> const& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);
  RemoveOptions options;
  TRI_doc_update_policy_e policy = TRI_DOC_UPDATE_ERROR;

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
        options.overwrite =
            TRI_ObjectToBoolean(optionsObject->Get(OverwriteKey));
        policy = ExtractUpdatePolicy(options.overwrite);
      }
      TRI_GET_GLOBAL_STRING(WaitForSyncKey);
      if (optionsObject->Has(WaitForSyncKey)) {
        options.waitForSync =
            TRI_ObjectToBoolean(optionsObject->Get(WaitForSyncKey));
      }
    } else {  // old variant replace(<document>, <data>, <overwrite>,
              // <waitForSync>)
      options.overwrite = TRI_ObjectToBoolean(args[1]);
      policy = ExtractUpdatePolicy(options.overwrite);
      if (argLength > 2) {
        options.waitForSync = TRI_ObjectToBoolean(args[2]);
      }
    }
  }

  TRI_vocbase_t* vocbase;
  TRI_vocbase_col_t const* col = nullptr;

  if (useCollection) {
    // called as db.collection.remove()
    col =
        TRI_UnwrapClass<TRI_vocbase_col_t>(args.Holder(), WRP_VOCBASE_COL_TYPE);

    if (col == nullptr) {
      TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
    }

    vocbase = col->_vocbase;
  } else {
    // called as db._remove()
    vocbase = GetContextVocBase(isolate);
  }

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  if (ServerState::instance()->isCoordinator()) {
    std::unique_ptr<char[]> key;
    TRI_voc_rid_t rid;
    V8ResolverGuard resolver(vocbase);
    int err = ParseDocumentOrDocumentHandle(
        isolate, vocbase, resolver.getResolver(), col, key, rid, args[0]);

    LocalCollectionGuard g(useCollection ? nullptr
                                         : const_cast<TRI_vocbase_col_t*>(col));

    if (key.get() == nullptr) {
      TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD);
    }

    if (err != TRI_ERROR_NO_ERROR) {
      TRI_V8_THROW_EXCEPTION(err);
    }

    RemoveVocbaseColCoordinator(col, policy, options.waitForSync, args);
    return;
  }

  std::unique_ptr<char[]> key;
  TRI_voc_rid_t rid;

  V8ResolverGuard resolver(vocbase);
  int err = ParseDocumentOrDocumentHandle(
      isolate, vocbase, resolver.getResolver(), col, key, rid, args[0]);

  if (key.get() == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD);
  }

  if (err != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(err);
  }

  TRI_ASSERT(col != nullptr);
  TRI_ASSERT(key.get() != nullptr);

  SingleCollectionWriteTransaction<1> trx(new V8TransactionContext(true),
                                          vocbase, col->_cid);

  int res = trx.openCollections();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  VPackOptions vpackOptions = trx.copyVPackOptions();
  vpackOptions.attributeExcludeHandler = nullptr;
  VPackBuilder builder(&vpackOptions);

  builder.add(VPackValue(VPackValueType::Object));
  builder.add(TRI_VOC_ATTRIBUTE_KEY,
              VPackValue(reinterpret_cast<char*>(key.get())));
  if (rid != 0) {
    // now add _rev attribute
    uint8_t* p = builder.add(TRI_VOC_ATTRIBUTE_REV,
                             VPackValuePair(9ULL, VPackValueType::Custom));
    *p++ = 0xf1;
    arangodb::velocypack::storeUInt64(p, rid);
  }

  builder.close();

  VPackSlice slice(builder.slice());

  TRI_voc_rid_t actualRevision = 0;
  TRI_doc_update_policy_t updatePolicy(policy, rid, &actualRevision);

  res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  TRI_document_collection_t* document = trx.documentCollection();

  res = document->remove(&trx, &slice, &updatePolicy,
                         !trx.isLocked(document, TRI_TRANSACTION_WRITE),
                         options.waitForSync);
  res = trx.finish(res);

  if (res != TRI_ERROR_NO_ERROR) {
    if (res == TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND &&
        policy == TRI_DOC_UPDATE_LAST_WRITE) {
      TRI_V8_RETURN_FALSE();
    } else {
      TRI_V8_THROW_EXCEPTION(res);
    }
  }

  TRI_V8_RETURN_TRUE();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock documentsCollectionName
////////////////////////////////////////////////////////////////////////////////

static void JS_DocumentVocbaseCol(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  DocumentVocbaseCol(true, args);
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

static void JS_ExistsVocbaseCol(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  return ExistsVocbaseCol(true, args);
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

  SingleCollectionReadOnlyTransaction trx(
      new V8TransactionContext(true), collection->_vocbase, collection->_cid);

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

  // shapefiles info
  v8::Handle<v8::Object> sf = v8::Object::New(isolate);

  result->Set(TRI_V8_ASCII_STRING("shapefiles"), sf);
  sf->Set(TRI_V8_ASCII_STRING("count"),
          v8::Number::New(isolate, (double)info->_numberShapefiles));
  sf->Set(TRI_V8_ASCII_STRING("fileSize"),
          v8::Number::New(isolate, (double)info->_shapefileSize));

  // shape info
  v8::Handle<v8::Object> shapes = v8::Object::New(isolate);
  result->Set(TRI_V8_ASCII_STRING("shapes"), shapes);
  shapes->Set(TRI_V8_ASCII_STRING("count"),
              v8::Number::New(isolate, (double)info->_numberShapes));
  shapes->Set(TRI_V8_ASCII_STRING("size"),
              v8::Number::New(isolate, (double)info->_sizeShapes));

  // attributes info
  v8::Handle<v8::Object> attributes = v8::Object::New(isolate);
  result->Set(TRI_V8_ASCII_STRING("attributes"), attributes);
  attributes->Set(TRI_V8_ASCII_STRING("count"),
                  v8::Number::New(isolate, (double)info->_numberAttributes));
  attributes->Set(TRI_V8_ASCII_STRING("size"),
                  v8::Number::New(isolate, (double)info->_sizeAttributes));

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
  char* name =
      TRI_GetCollectionNameByIdVocBase(collection->_vocbase, collection->_cid);

  if (name == nullptr) {
    TRI_V8_RETURN_UNDEFINED();
  }

  v8::Handle<v8::Value> result = TRI_V8_STRING(name);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, name);

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
              "indexBucket must be a two-power between 1 and 1024");
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
    result->Set(TRI_V8_ASCII_STRING("replicationFactor"),
        v8::Number::New(isolate, static_cast<double>(c->replicationFactor())));
    result->Set(TRI_V8_ASCII_STRING("replicationQuorum"),
        v8::Number::New(isolate, static_cast<double>(c->replicationQuorum())));

    TRI_V8_RETURN(result);
  }

  collection = UseCollection(args.Holder(), args);

  if (collection == nullptr) {
    return;
  }

  TRI_document_collection_t* document = collection->_collection;
  TRI_collection_t* base = document;

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
        TRI_LOCK_JOURNAL_ENTRIES_DOC_COLLECTION(document);
        arangodb::basics::ScopeGuard guard{
            []() -> void {},
            [&document]() -> void {
              TRI_UNLOCK_JOURNAL_ENTRIES_DOC_COLLECTION(document);
            }};

        if (base->_info.isVolatile() &&
            arangodb::basics::VelocyPackHelper::getBooleanValue(
                slice, "waitForSync", base->_info.waitForSync())) {
          ReleaseCollection(collection);
          // the combination of waitForSync and isVolatile makes no sense
          TRI_V8_THROW_EXCEPTION_PARAMETER(
              "volatile collections do not support the waitForSync option");
        }

        if (base->_info.isVolatile() !=
            arangodb::basics::VelocyPackHelper::getBooleanValue(
                slice, "isVolatile", base->_info.isVolatile())) {
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
              "indexBucket must be a two-power between 1 and 1024");
        }

      }  // Leave the scope and free the JOURNAL lock

      // try to write new parameter to file
      bool doSync = base->_vocbase->_settings.forceSyncProperties;
      res = TRI_UpdateCollectionInfo(base->_vocbase, base, slice, doSync);

      if (res != TRI_ERROR_NO_ERROR) {
        ReleaseCollection(collection);
        TRI_V8_THROW_EXCEPTION(res);
      }

      TRI_json_t* json = TRI_CreateJsonCollectionInfo(base->_info);

      if (json == nullptr) {
        ReleaseCollection(collection);
        TRI_V8_THROW_EXCEPTION(res);
      }

      // now log the property changes
      res = TRI_ERROR_NO_ERROR;

      try {
        arangodb::wal::ChangeCollectionMarker marker(
            base->_vocbase->_id, base->_info.id(), JsonHelper::toString(json));
        arangodb::wal::SlotInfoCopy slotInfo =
            arangodb::wal::LogfileManager::instance()->allocateAndWrite(marker,
                                                                        false);

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
        LOG(WARN) << "could not save collection change marker in log: " << TRI_errno_string(res);
      }

      TRI_FreeJson(TRI_CORE_MEM_ZONE, json);
    }
  }

  // return the current parameter set
  v8::Handle<v8::Object> result = v8::Object::New(isolate);

  TRI_GET_GLOBAL_STRING(DoCompactKey);
  TRI_GET_GLOBAL_STRING(IsSystemKey);
  TRI_GET_GLOBAL_STRING(IsVolatileKey);
  TRI_GET_GLOBAL_STRING(JournalSizeKey);
  result->Set(DoCompactKey, v8::Boolean::New(isolate, base->_info.doCompact()));
  result->Set(IsSystemKey, v8::Boolean::New(isolate, base->_info.isSystem()));
  result->Set(IsVolatileKey,
              v8::Boolean::New(isolate, base->_info.isVolatile()));
  result->Set(JournalSizeKey,
              v8::Number::New(isolate, base->_info.maximalSize()));
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
              v8::Boolean::New(isolate, base->_info.waitForSync()));

  ReleaseCollection(collection);
  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock documentsCollectionRemove
////////////////////////////////////////////////////////////////////////////////

static void JS_RemoveVocbaseCol(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  return RemoveVocbaseCol(true, args);
  TRI_V8_TRY_CATCH_END
}

static void JS_RemoveVocbaseVPack(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  return RemoveVocbaseVPack(true, args);
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
  buffer.appendText("require('@arangodb/general-graph')._renameCollection(\"");
  buffer.appendJsonEncoded(oldName.c_str(), oldName.size());
  buffer.appendText("\", \"");
  buffer.appendJsonEncoded(newName.c_str(), newName.size());
  buffer.appendText("\");");

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
/// @brief was docuBlock documentsCollectionReplace
////////////////////////////////////////////////////////////////////////////////

static void JS_ReplaceVocbaseCol(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  return ReplaceVocbaseCol(true, args);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief fetch the revision for a local collection
////////////////////////////////////////////////////////////////////////////////

static int GetRevision(TRI_vocbase_col_t* collection, TRI_voc_rid_t& rid) {
  TRI_ASSERT(collection != nullptr);

  SingleCollectionReadOnlyTransaction trx(
      new V8TransactionContext(true), collection->_vocbase, collection->_cid);

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

  int res = TRI_RotateJournalDocumentCollection(document);

  ReleaseCollection(collection);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(res, "could not rotate journal");
  }

  TRI_V8_RETURN_UNDEFINED();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock documentsCollectionUpdate
////////////////////////////////////////////////////////////////////////////////

static void JS_UpdateVocbaseCol(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  return UpdateVocbaseCol(true, args);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief saves a document, coordinator case in a cluster
////////////////////////////////////////////////////////////////////////////////

static void InsertVocbaseColCoordinator(
    TRI_vocbase_col_t* collection, uint32_t argOffset,
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  // First get the initial data:
  std::string const dbname(collection->_dbName);

  // TODO: someone might rename the collection while we're reading its name...
  std::string const collname(collection->_name);

  // Now get the arguments
  uint32_t const argLength = args.Length() - argOffset;
  if (argLength < 1 || argLength > 2) {
    TRI_V8_THROW_EXCEPTION_USAGE("insert(<data>, [<waitForSync>])");
  }

  InsertOptions options;
  if (argLength > 1 && args[1 + argOffset]->IsObject()) {
    TRI_GET_GLOBALS();
    v8::Handle<v8::Object> optionsObject = args[1 + argOffset].As<v8::Object>();
    TRI_GET_GLOBAL_STRING(WaitForSyncKey);
    if (optionsObject->Has(WaitForSyncKey)) {
      options.waitForSync =
          TRI_ObjectToBoolean(optionsObject->Get(WaitForSyncKey));
    }
    TRI_GET_GLOBAL_STRING(SilentKey);
    if (optionsObject->Has(SilentKey)) {
      options.silent = TRI_ObjectToBoolean(optionsObject->Get(SilentKey));
    }
  } else {
    options.waitForSync = ExtractWaitForSync(args, 2 + argOffset);
  }

  std::unique_ptr<TRI_json_t> json(TRI_ObjectToJson(isolate, args[argOffset]));
  if (!TRI_IsObjectJson(json.get())) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
  }

  arangodb::rest::HttpResponse::HttpResponseCode responseCode;
  std::map<std::string, std::string> headers;
  std::map<std::string, std::string> resultHeaders;
  std::string resultBody;

  int error = arangodb::createDocumentOnCoordinator(
      dbname, collname, options.waitForSync, json, headers, responseCode,
      resultHeaders, resultBody);

  if (error != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(error);
  }
  // report what the DBserver told us: this could now be 201/202 or
  // 400/404
  json.reset(TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, resultBody.c_str()));
  if (responseCode >= arangodb::rest::HttpResponse::BAD) {
    if (!TRI_IsObjectJson(json.get())) {
      TRI_V8_THROW_EXCEPTION(TRI_ERROR_INTERNAL);
    }
    int errorNum = 0;
    TRI_json_t* subjson = TRI_LookupObjectJson(json.get(), "errorNum");
    if (nullptr != subjson && TRI_IsNumberJson(subjson)) {
      errorNum = static_cast<int>(subjson->_value._number);
    }

    std::string errorMessage;
    subjson = TRI_LookupObjectJson(json.get(), "errorMessage");
    if (nullptr != subjson && TRI_IsStringJson(subjson)) {
      errorMessage = std::string(subjson->_value._string.data,
                                 subjson->_value._string.length - 1);
    }
    TRI_V8_THROW_EXCEPTION_MESSAGE(errorNum, errorMessage);
  }

  if (options.silent) {
    TRI_V8_RETURN_TRUE();
  }

  v8::Handle<v8::Value> ret = TRI_ObjectJson(isolate, json.get());
  TRI_V8_RETURN(ret);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extract a key from a v8 object
////////////////////////////////////////////////////////////////////////////////

static std::string GetId(v8::FunctionCallbackInfo<v8::Value> const& args,
                         int which) {
  v8::Isolate* isolate = args.GetIsolate();  // used in TRI_GET_GLOBALS

  if (args[which]->IsObject() && !args[which]->IsArray()) {
    TRI_GET_GLOBALS();
    TRI_GET_GLOBAL_STRING(_IdKey);

    v8::Local<v8::Object> obj = args[which]->ToObject();

    if (obj->Has(_IdKey)) {
      return TRI_ObjectToString(obj->Get(_IdKey));
    }
  }

  return TRI_ObjectToString(args[which]);
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

  std::string const name = TRI_ObjectToString(val);
  return TRI_LookupCollectionByNameVocBase(vocbase, name.c_str());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock InsertEdgeCol
////////////////////////////////////////////////////////////////////////////////

static void InsertEdgeCol(TRI_vocbase_col_t* col, uint32_t argOffset,
                          v8::FunctionCallbackInfo<v8::Value> const& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  TRI_GET_GLOBALS();

  uint32_t const argLength = args.Length() - argOffset;
  if (argLength < 3 || argLength > 4) {
    TRI_V8_THROW_EXCEPTION_USAGE("save(<from>, <to>, <data>, [<waitForSync>])");
  }

  InsertOptions options;

  if (!args[2 + argOffset]->IsObject() || args[2 + argOffset]->IsArray()) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
  }

  // set document key
  std::unique_ptr<char[]> key;
  int res =
      ExtractDocumentKey(isolate, v8g, args[2 + argOffset]->ToObject(), key);

  if (res != TRI_ERROR_NO_ERROR &&
      res != TRI_ERROR_ARANGO_DOCUMENT_KEY_MISSING) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  if (argLength > 3 && args[3 + argOffset]->IsObject()) {
    v8::Handle<v8::Object> optionsObject = args[3 + argOffset].As<v8::Object>();
    TRI_GET_GLOBAL_STRING(WaitForSyncKey);
    if (optionsObject->Has(WaitForSyncKey)) {
      options.waitForSync =
          TRI_ObjectToBoolean(optionsObject->Get(WaitForSyncKey));
    }
    TRI_GET_GLOBAL_STRING(SilentKey);
    if (optionsObject->Has(SilentKey)) {
      options.silent = TRI_ObjectToBoolean(optionsObject->Get(SilentKey));
    }
  } else {
    options.waitForSync = ExtractWaitForSync(args, 4 + argOffset);
  }

  std::unique_ptr<char[]> fromKey;
  std::unique_ptr<char[]> toKey;

  TRI_document_edge_t edge;
  // the following values are defaults that will be overridden below
  edge._fromCid = 0;
  edge._toCid = 0;
  edge._fromKey = nullptr;
  edge._toKey = nullptr;

  SingleCollectionWriteTransaction<1> trx(new V8TransactionContext(true),
                                          col->_vocbase, col->_cid);

  // extract from
  res = trx.setupState();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  res = TRI_ParseVertex(args, trx.resolver(), edge._fromCid, fromKey,
                        args[argOffset]);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }
  edge._fromKey = fromKey.get();

  // extract to
  res = TRI_ParseVertex(args, trx.resolver(), edge._toCid, toKey,
                        args[argOffset + 1]);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }
  edge._toKey = toKey.get();

  //  start transaction
  res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  TRI_document_collection_t* document = trx.documentCollection();
  TRI_memory_zone_t* zone =
      document->getShaper()->memoryZone();  // PROTECTED by trx from above

  // fetch a barrier so nobody unlinks datafiles with the shapes & attributes we
  // might
  // need for this document
  if (trx.orderDitch(trx.trxCollection()) == nullptr) {
    TRI_V8_THROW_EXCEPTION_MEMORY();
  }

  // extract shaped data
  TRI_shaped_json_t* shaped = TRI_ShapedJsonV8Object(
      isolate, args[2 + argOffset], document->getShaper(),
      true);  // PROTECTED by trx here

  if (shaped == nullptr) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(
        TRI_errno(), "<data> cannot be converted into JSON shape");
  }

  TRI_doc_mptr_copy_t mptr;
  res = trx.createEdge(key.get(), &mptr, shaped, options.waitForSync, &edge);

  res = trx.finish(res);

  TRI_FreeShapedJson(zone, shaped);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  TRI_ASSERT(mptr.getDataPtr() != nullptr);  // PROTECTED by trx here

  if (options.silent) {
    TRI_V8_RETURN_TRUE();
  } else {
    char const* docKey =
        TRI_EXTRACT_MARKER_KEY(&mptr);  // PROTECTED by trx here

    v8::Handle<v8::Object> result = v8::Object::New(isolate);
    TRI_GET_GLOBAL_STRING(_IdKey);
    TRI_GET_GLOBAL_STRING(_RevKey);
    TRI_GET_GLOBAL_STRING(_KeyKey);
    result->Set(
        _IdKey,
        V8DocumentId(isolate, trx.resolver()->getCollectionName(col->_cid),
                     docKey));
    result->Set(_RevKey, V8RevisionId(isolate, mptr._rid));
    result->Set(_KeyKey, TRI_V8_STRING(docKey));

    TRI_V8_RETURN(result);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief saves an edge, coordinator case in a cluster
////////////////////////////////////////////////////////////////////////////////

static void InsertEdgeColCoordinator(
    TRI_vocbase_col_t* collection, uint32_t argOffset,
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  // First get the initial data:
  std::string const dbname(collection->_dbName);

  // TODO: someone might rename the collection while we're reading its name...
  std::string const collname(collection->_name);

  uint32_t const argLength = args.Length() - argOffset;
  if (argLength < 3 || argLength > 4) {
    TRI_V8_THROW_EXCEPTION_USAGE(
        "insert(<from>, <to>, <data>, [<waitForSync>])");
  }

  std::string _from = GetId(args, argOffset);
  std::string _to = GetId(args, 1 + argOffset);

  std::unique_ptr<TRI_json_t> json(
      TRI_ObjectToJson(isolate, args[2 + argOffset]));

  if (!TRI_IsObjectJson(json.get())) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
  }

  InsertOptions options;
  if (argLength > 3 && args[3 + argOffset]->IsObject()) {
    TRI_GET_GLOBALS();
    v8::Handle<v8::Object> optionsObject = args[3 + argOffset].As<v8::Object>();
    TRI_GET_GLOBAL_STRING(WaitForSyncKey);
    if (optionsObject->Has(WaitForSyncKey)) {
      options.waitForSync =
          TRI_ObjectToBoolean(optionsObject->Get(WaitForSyncKey));
    }
    TRI_GET_GLOBAL_STRING(SilentKey);
    if (optionsObject->Has(SilentKey)) {
      options.silent = TRI_ObjectToBoolean(optionsObject->Get(SilentKey));
    }
  } else {
    options.waitForSync = ExtractWaitForSync(args, 4 + argOffset);
  }

  arangodb::rest::HttpResponse::HttpResponseCode responseCode;
  std::map<std::string, std::string> resultHeaders;
  std::string resultBody;

  int error = arangodb::createEdgeOnCoordinator(
      dbname, collname, options.waitForSync, json, _from.c_str(), _to.c_str(),
      responseCode, resultHeaders, resultBody);

  if (error != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(error);
  }
  // report what the DBserver told us: this could now be 201/202 or
  // 400/404
  json.reset(TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, resultBody.c_str()));
  if (responseCode >= arangodb::rest::HttpResponse::BAD) {
    if (!TRI_IsObjectJson(json.get())) {
      TRI_V8_THROW_EXCEPTION(TRI_ERROR_INTERNAL);
    }
    int errorNum = 0;
    TRI_json_t* subjson = TRI_LookupObjectJson(json.get(), "errorNum");
    if (nullptr != subjson && TRI_IsNumberJson(subjson)) {
      errorNum = static_cast<int>(subjson->_value._number);
    }
    std::string errorMessage;
    subjson = TRI_LookupObjectJson(json.get(), "errorMessage");
    if (nullptr != subjson && TRI_IsStringJson(subjson)) {
      errorMessage = std::string(subjson->_value._string.data,
                                 subjson->_value._string.length - 1);
    }
    TRI_V8_THROW_EXCEPTION_MESSAGE(errorNum, errorMessage);
  }
  v8::Handle<v8::Value> ret = TRI_ObjectJson(isolate, json.get());
  TRI_V8_RETURN(ret);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock documentsCollectionInsert
////////////////////////////////////////////////////////////////////////////////

static void JS_InsertVocbaseCol(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  TRI_vocbase_col_t* collection =
      TRI_UnwrapClass<TRI_vocbase_col_t>(args.Holder(), WRP_VOCBASE_COL_TYPE);

  if (collection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  if (ServerState::instance()->isCoordinator()) {
    // coordinator case
    if ((TRI_col_type_e)collection->_type == TRI_COL_TYPE_DOCUMENT) {
      InsertVocbaseColCoordinator(collection, 0, args);
      return;
    }

    InsertEdgeColCoordinator(collection, 0, args);
    return;
  }

  // single server case
  if ((TRI_col_type_e)collection->_type == TRI_COL_TYPE_DOCUMENT) {
    InsertVocbaseCol(collection, 0, args);
  } else {
    InsertEdgeCol(collection, 0, args);
  }
  TRI_V8_TRY_CATCH_END
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
  TRI_vocbase_col_t* collection = nullptr;

  if (ServerState::instance()->isCoordinator()) {
    std::string const name = TRI_ObjectToString(val);
    std::shared_ptr<CollectionInfo> const& ci =
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
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  if (ServerState::instance()->isCoordinator()) {
    // coordinator case
    if ((TRI_col_type_e)collection->_type == TRI_COL_TYPE_DOCUMENT) {
      InsertVocbaseColCoordinator(collection, 1, args);
      return;
    }

    InsertEdgeColCoordinator(collection, 1, args);
    return;
  }

  // single server case
  if ((TRI_col_type_e)collection->_type == TRI_COL_TYPE_DOCUMENT) {
    InsertVocbaseCol(collection, 1, args);
  } else {
    InsertEdgeCol(collection, 1, args);
  }
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief inserts a document, using a VPack marker
////////////////////////////////////////////////////////////////////////////////

static void JS_InsertVocbaseVPack(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  TRI_vocbase_col_t* collection =
      TRI_UnwrapClass<TRI_vocbase_col_t>(args.Holder(), WRP_VOCBASE_COL_TYPE);

  if (collection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  if (ServerState::instance()->isCoordinator()) {
    // coordinator case
    if ((TRI_col_type_e)collection->_type == TRI_COL_TYPE_DOCUMENT) {
      InsertVocbaseColCoordinator(collection, 0, args);
      return;
    }

    InsertEdgeColCoordinator(collection, 0, args);
    return;
  }

  // single server case
  if ((TRI_col_type_e)collection->_type == TRI_COL_TYPE_DOCUMENT) {
    InsertVocbaseVPack(collection, args);
  } else {
    InsertEdgeCol(collection, 0, args);
  }
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

  bool const forceSync = ExtractWaitForSync(args, 1);

  TRI_vocbase_col_t* collection =
      TRI_UnwrapClass<TRI_vocbase_col_t>(args.Holder(), WRP_VOCBASE_COL_TYPE);

  if (collection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  TRI_THROW_SHARDING_COLLECTION_NOT_YET_IMPLEMENTED(collection);

  SingleCollectionWriteTransaction<UINT64_MAX> trx(
      new V8TransactionContext(true), collection->_vocbase, collection->_cid);
  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  if (trx.orderDitch(trx.trxCollection()) == nullptr) {
    TRI_V8_THROW_EXCEPTION_MEMORY();
  }

  res = trx.truncate(forceSync);
  res = trx.finish(res);

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

////////////////////////////////////////////////////////////////////////////////
/// @brief checks all data pointers in a collection
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_MAINTAINER_MODE
static void JS_CheckPointersVocbaseCol(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  TRI_vocbase_col_t* collection =
      TRI_UnwrapClass<TRI_vocbase_col_t>(args.Holder(), WRP_VOCBASE_COL_TYPE);

  if (collection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  TRI_THROW_SHARDING_COLLECTION_NOT_YET_IMPLEMENTED(collection);

  SingleCollectionReadOnlyTransaction trx(
      new V8TransactionContext(true), collection->_vocbase, collection->_cid);

  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  TRI_document_collection_t* document = trx.documentCollection();
  auto work = [&](TRI_doc_mptr_t const* ptr) -> void {
    char const* key = TRI_EXTRACT_MARKER_KEY(ptr);
    TRI_ASSERT(key != nullptr);
    // dereference the key
    if (*key == '\0') {
      TRI_V8_THROW_EXCEPTION(TRI_ERROR_INTERNAL);
    }
  };
  document->primaryIndex()->invokeOnAllElements(work);

  TRI_V8_RETURN_TRUE();
  TRI_V8_TRY_CATCH_END
}
#endif

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
  result->Set(j++, TRI_V8_ASCII_STRING("_listDatabases()"));
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
  return RemoveVocbaseCol(false, args);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock documentsDocumentName
////////////////////////////////////////////////////////////////////////////////

static void JS_DocumentVocbase(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  DocumentVocbaseCol(false, args);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock documentsDocumentExists
////////////////////////////////////////////////////////////////////////////////

static void JS_ExistsVocbase(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  return ExistsVocbaseCol(false, args);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock documentsDocumentReplace
////////////////////////////////////////////////////////////////////////////////

static void JS_ReplaceVocbase(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  return ReplaceVocbaseCol(false, args);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock documentsDocumentUpdate
////////////////////////////////////////////////////////////////////////////////

static void JS_UpdateVocbase(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  return UpdateVocbaseCol(false, args);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock collectionCount
////////////////////////////////////////////////////////////////////////////////

static void JS_CountVocbaseCol(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  TRI_vocbase_col_t* collection =
      TRI_UnwrapClass<TRI_vocbase_col_t>(args.Holder(), WRP_VOCBASE_COL_TYPE);

  if (collection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("count()");
  }

  if (ServerState::instance()->isCoordinator()) {
    // First get the initial data:
    std::string const dbname(collection->_dbName);

    // TODO: someone might rename the collection while we're reading its name...
    std::string const collname(collection->_name);

    uint64_t count = 0;
    int error = arangodb::countOnCoordinator(dbname, collname, count);

    if (error != TRI_ERROR_NO_ERROR) {
      TRI_V8_THROW_EXCEPTION(error);
    }

    TRI_V8_RETURN(v8::Number::New(isolate, (double)count));
  }

  SingleCollectionReadOnlyTransaction trx(
      new V8TransactionContext(true), collection->_vocbase, collection->_cid);

  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  TRI_document_collection_t* document = trx.documentCollection();

  // READ-LOCK start
  trx.lockRead();

  uint64_t const s = document->size();

  trx.finish(res);
  // READ-LOCK end

  TRI_V8_RETURN(v8::Number::New(isolate, static_cast<double>(s)));
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

  for (size_t i = 0; i < structure._journals._length; ++i) {
    journals->Set((uint32_t)i, TRI_V8_STRING(structure._journals._buffer[i]));
  }

  // compactors
  v8::Handle<v8::Array> compactors = v8::Array::New(isolate);
  result->Set(TRI_V8_ASCII_STRING("compactors"), compactors);

  for (size_t i = 0; i < structure._compactors._length; ++i) {
    compactors->Set((uint32_t)i,
                    TRI_V8_STRING(structure._compactors._buffer[i]));
  }

  // datafiles
  v8::Handle<v8::Array> datafiles = v8::Array::New(isolate);
  result->Set(TRI_V8_ASCII_STRING("datafiles"), datafiles);

  for (size_t i = 0; i < structure._datafiles._length; ++i) {
    datafiles->Set((uint32_t)i, TRI_V8_STRING(structure._datafiles._buffer[i]));
  }

  // free result
  TRI_DestroyFileStructureCollection(&structure);

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

    TRI_df_scan_t scan = TRI_ScanDatafile(path.c_str());

    // build result
    result = v8::Object::New(isolate);

    result->Set(TRI_V8_ASCII_STRING("currentSize"),
                v8::Number::New(isolate, scan._currentSize));
    result->Set(TRI_V8_ASCII_STRING("maximalSize"),
                v8::Number::New(isolate, scan._maximalSize));
    result->Set(TRI_V8_ASCII_STRING("endPosition"),
                v8::Number::New(isolate, scan._endPosition));
    result->Set(TRI_V8_ASCII_STRING("numberMarkers"),
                v8::Number::New(isolate, scan._numberMarkers));
    result->Set(TRI_V8_ASCII_STRING("status"),
                v8::Number::New(isolate, scan._status));
    result->Set(TRI_V8_ASCII_STRING("isSealed"),
                v8::Boolean::New(isolate, scan._isSealed));

    v8::Handle<v8::Array> entries = v8::Array::New(isolate);
    result->Set(TRI_V8_ASCII_STRING("entries"), entries);

    size_t const n = TRI_LengthVector(&scan._entries);
    for (size_t i = 0; i < n; ++i) {
      auto entry = static_cast<TRI_df_scan_entry_t const*>(
          TRI_AddressVector(&scan._entries, i));

      v8::Handle<v8::Object> o = v8::Object::New(isolate);

      o->Set(TRI_V8_ASCII_STRING("position"),
             v8::Number::New(isolate, entry->_position));
      o->Set(TRI_V8_ASCII_STRING("size"),
             v8::Number::New(isolate, entry->_size));
      o->Set(TRI_V8_ASCII_STRING("realSize"),
             v8::Number::New(isolate, entry->_realSize));
      o->Set(TRI_V8_ASCII_STRING("tick"), V8TickId(isolate, entry->_tick));
      o->Set(TRI_V8_ASCII_STRING("type"),
             v8::Number::New(isolate, (int)entry->_type));
      o->Set(TRI_V8_ASCII_STRING("status"),
             v8::Number::New(isolate, (int)entry->_status));

      if (entry->_key != nullptr) {
        o->Set(TRI_V8_ASCII_STRING("key"), TRI_V8_ASCII_STRING(entry->_key));
      }

      if (entry->_typeName != nullptr) {
        o->Set(TRI_V8_ASCII_STRING("typeName"),
               TRI_V8_ASCII_STRING(entry->_typeName));
      }

      if (entry->_diagnosis != nullptr) {
        o->Set(TRI_V8_ASCII_STRING("diagnosis"),
               TRI_V8_ASCII_STRING(entry->_diagnosis));
      }

      entries->Set((uint32_t)i, o);
    }

    TRI_DestroyDatafileScan(&scan);
  }

  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

// .............................................................................
// generate the TRI_vocbase_col_t template
// .............................................................................

void TRI_InitV8collection(v8::Handle<v8::Context> context, TRI_server_t* server,
                          TRI_vocbase_t* vocbase, JSLoader* loader,
                          size_t const threadNumber, TRI_v8_global_t* v8g,
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

#ifdef TRI_ENABLE_MAINTAINER_MODE
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("checkPointers"),
                       JS_CheckPointersVocbaseCol);
#endif
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
                       JS_ExistsVocbaseCol);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("figures"),
                       JS_FiguresVocbaseCol);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("insert"),
                       JS_InsertVocbaseCol);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("insertv"),
                       JS_InsertVocbaseVPack);
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
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("removev"),
                       JS_RemoveVocbaseVPack);
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
