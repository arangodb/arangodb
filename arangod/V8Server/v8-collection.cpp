////////////////////////////////////////////////////////////////////////////////
/// @brief V8-vocbase bridge
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

#include "v8-collection.h"
#include "v8-vocbaseprivate.h"
#include "v8-wrapshapedjson.h"

#include "Basics/Utf8Helper.h"
#include "Basics/conversions.h"
#include "Basics/json-utilities.h"
#include "V8/v8-conv.h"
#include "Utils/transactions.h"
#include "Utils/V8TransactionContext.h"

#include "Aql/Query.h"
#include "Utils/V8ResolverGuard.h"
#include "V8/v8-utils.h"
#include "Wal/LogfileManager.h"

#include "VocBase/auth.h"
#include "VocBase/key-generator.h"

#include "Cluster/ClusterMethods.h"

#include "unicode/timezone.h"

#include "v8-vocbase.h"
#include "v8-vocindex.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::arango;
using namespace triagens::rest;


struct LocalCollectionGuard {
  LocalCollectionGuard (TRI_vocbase_col_t* collection)
    : _collection(collection) {
  }

  ~LocalCollectionGuard () {
    if (_collection != nullptr && ! _collection->_isLocal) {
      FreeCoordinatorCollection(_collection);
    }
  }

  TRI_vocbase_col_t* _collection;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief internal struct which is used for reading the different option
/// parameters for the save function
////////////////////////////////////////////////////////////////////////////////

struct InsertOptions {
  bool waitForSync  = false;
  bool silent       = false;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief internal struct which is used for reading the different option
/// parameters for the update and replace functions
////////////////////////////////////////////////////////////////////////////////

struct UpdateOptions {
  bool overwrite    = false;
  bool keepNull     = true;
  bool mergeObjects = true;
  bool waitForSync  = false;
  bool silent       = false;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief internal struct which is used for reading the different option
/// parameters for the remove function
////////////////////////////////////////////////////////////////////////////////

struct RemoveOptions {
  bool overwrite    = false;
  bool waitForSync  = false;
};

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------


////////////////////////////////////////////////////////////////////////////////
/// @brief extract the forceSync flag from the arguments
/// must specify the argument index starting from 1
////////////////////////////////////////////////////////////////////////////////

static inline bool ExtractWaitForSync (const v8::FunctionCallbackInfo<v8::Value>& args,
                                       int index) {
  TRI_ASSERT(index > 0);

  return (args.Length() >= index && TRI_ObjectToBoolean(args[index - 1]));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extract the update policy from a boolean parameter
////////////////////////////////////////////////////////////////////////////////

static inline TRI_doc_update_policy_e ExtractUpdatePolicy (bool overwrite) {
  return (overwrite ? TRI_DOC_UPDATE_LAST_WRITE : TRI_DOC_UPDATE_ERROR);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a v8 collection id value from the internal collection id
////////////////////////////////////////////////////////////////////////////////

static inline v8::Handle<v8::Value> V8CollectionId (v8::Isolate* isolate, TRI_voc_cid_t cid) {
  char buffer[21];
  size_t len = TRI_StringUInt64InPlace((uint64_t) cid, (char*) &buffer);

  return TRI_V8_PAIR_STRING((const char*) buffer, (int) len);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts a document key from a document
////////////////////////////////////////////////////////////////////////////////

static int ExtractDocumentKey (v8::Isolate* isolate,
                               TRI_v8_global_t* v8g,
                               v8::Handle<v8::Object> const arg,
                               std::unique_ptr<char[]>& key) {
  TRI_ASSERT(v8g != nullptr);
  TRI_ASSERT(key.get() == nullptr);

  v8::Local<v8::Object> obj = arg->ToObject();

  TRI_GET_GLOBAL_STRING(_KeyKey);
  if (obj->Has(_KeyKey)) {
    v8::Handle<v8::Value> v = obj->Get(_KeyKey);

    if (v->IsString()) {
      // string key
      // keys must not contain any special characters, so it is not necessary
      // to normalise them first
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

static int ParseDocumentOrDocumentHandle (TRI_vocbase_t* vocbase,
                                          CollectionNameResolver const* resolver,
                                          TRI_vocbase_col_t const*& collection,
                                          std::unique_ptr<char[]>& key,
                                          TRI_voc_rid_t& rid,
                                          v8::Handle<v8::Value> const val,
                                          const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  TRI_ASSERT(key.get() == nullptr);

  // reset the collection identifier and the revision
  string collectionName;
  rid = 0;

  // try to extract the collection name, key, and revision from the object passed
  if (! ExtractDocumentHandle(isolate, val, collectionName, key, rid)) {
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
  }
  else {
    // we read a collection name from the document id
    // check cross-collection requests
    if (collection != nullptr) {
      if (! EqualCollection(resolver, collectionName, collection)) {
        return TRI_ERROR_ARANGO_CROSS_COLLECTION_REQUEST;
      }
    }
  }

  TRI_ASSERT(! collectionName.empty());

  if (collection == nullptr) {
    // no collection object was passed, now check the user-supplied collection name

    TRI_vocbase_col_t const* col = nullptr;

    if (ServerState::instance()->isCoordinator()) {
      ClusterInfo* ci = ClusterInfo::instance();
      shared_ptr<CollectionInfo> const& c = ci->getCollection(vocbase->_name, collectionName);
      col = CoordinatorCollection(vocbase, *c);

      if (col != nullptr && col->_cid == 0) {
        FreeCoordinatorCollection(const_cast<TRI_vocbase_col_t*>(col));
        col = nullptr;
      }
    }
    else {
      col = resolver->getCollectionStruct(collectionName);
    }

    if (col == nullptr) {
      // collection not found
      return TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
    }

    collection = col;
  }

  TRI_ASSERT(collection != nullptr);

  TRI_V8_RETURN(v8::Handle<v8::Value>()) TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief cluster coordinator case, parse a key and possible revision
////////////////////////////////////////////////////////////////////////////////

static int ParseKeyAndRef (v8::Isolate* isolate,
                           v8::Handle<v8::Value> const arg,
                           string& key,
                           TRI_voc_rid_t& rev) {
  rev = 0;
  if (arg->IsString()) {
    key = TRI_ObjectToString(arg);
  }
  else if (arg->IsObject()) {
    TRI_GET_GLOBALS();
    v8::Handle<v8::Object> obj = v8::Handle<v8::Object>::Cast(arg);

    TRI_GET_GLOBAL_STRING(_KeyKey);
    TRI_GET_GLOBAL_STRING(_IdKey);
    TRI_GET_GLOBAL_STRING(_RevKey);
    if (obj->Has(_KeyKey) && obj->Get(_KeyKey)->IsString()) {
      key = TRI_ObjectToString(obj->Get(_KeyKey));
    }
    else if (obj->Has(_IdKey) && obj->Get(_IdKey)->IsString()) {
      key = TRI_ObjectToString(obj->Get(_IdKey));
      // part after / will be taken below
    }
    else {
      return TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID;
    }
    if (obj->Has(_RevKey) && obj->Get(_RevKey)->IsString()) {
      rev = TRI_ObjectToUInt64(obj->Get(_RevKey), true);
    }
  }
  else {
    return TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID;
  }

  size_t pos = key.find('/');
  if (pos != string::npos) {
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

static void DocumentVocbaseColCoordinator (TRI_vocbase_col_t const* collection,
                                           const v8::FunctionCallbackInfo<v8::Value>& args,
                                           bool generateDocument) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  // First get the initial data:
  string const dbname(collection->_dbName);
  // TODO: someone might rename the collection while we're reading its name...
  string const collname(collection->_name);

  string key;
  TRI_voc_rid_t rev = 0;
  int error = ParseKeyAndRef(isolate, args[0], key, rev);

  if (error != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(error);
  }

  triagens::rest::HttpResponse::HttpResponseCode responseCode;
  map<string, string> headers;
  map<string, string> resultHeaders;
  string resultBody;

  error = triagens::arango::getDocumentOnCoordinator(
            dbname, collname, key, rev, headers,
            generateDocument,
            responseCode, resultHeaders, resultBody);

  if (error != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(error);
  }

  // report what the DBserver told us: this could now be 200 or
  // 404/412
  // For the error processing we have to distinguish whether we are in
  // the ".exists" case (generateDocument==false) or the ".document" case
  // (generateDocument==true).
  TRI_json_t* json = nullptr;
  if (generateDocument) {
    json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, resultBody.c_str());
  }
  if (responseCode >= triagens::rest::HttpResponse::BAD) {
    if (! TRI_IsArrayJson(json)) {
      if (generateDocument) {
        if (nullptr != json) {
          TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
        }
        TRI_V8_THROW_EXCEPTION(TRI_ERROR_INTERNAL);
      }
      else {
        TRI_V8_RETURN_FALSE();
      }
    }
    if (generateDocument) {
      int errorNum = 0;
      string errorMessage;
      if (nullptr != json) {
        TRI_json_t* subjson = TRI_LookupArrayJson(json, "errorNum");
        if (nullptr != subjson && TRI_IsNumberJson(subjson)) {
          errorNum = static_cast<int>(subjson->_value._number);
        }
        subjson = TRI_LookupArrayJson(json, "errorMessage");
        if (nullptr != subjson && TRI_IsStringJson(subjson)) {
          errorMessage = string(subjson->_value._string.data,
                                subjson->_value._string.length - 1);
        }
        TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
      }
      TRI_V8_THROW_EXCEPTION_MESSAGE(errorNum, errorMessage);
    }
    else {
      TRI_V8_RETURN_FALSE();
    }
  }
  if (generateDocument) {
    v8::Handle<v8::Value> ret = TRI_ObjectJson(isolate, json);

    if (nullptr != json) {
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
    }
    TRI_V8_RETURN(ret);
  }
  else {
    // Note that for this case we will never get a 304 "NOT_MODIFIED"
    if (json != nullptr) {
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
    }
    TRI_V8_RETURN_TRUE();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a document and returns it
////////////////////////////////////////////////////////////////////////////////

static void DocumentVocbaseCol (bool useCollection,
                                const v8::FunctionCallbackInfo<v8::Value>& args) {
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
    col = TRI_UnwrapClass<TRI_vocbase_col_t>(args.Holder(), WRP_VOCBASE_COL_TYPE);

    if (col == nullptr) {
      TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
    }

    vocbase = col->_vocbase;
  }
  else {
    // called as db._document()
    vocbase = GetContextVocBase(isolate);
  }

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  V8ResolverGuard resolver(vocbase);
  int err = ParseDocumentOrDocumentHandle(vocbase, resolver.getResolver(), col, key, rid, args[0], args);

  LocalCollectionGuard g(useCollection ? nullptr : const_cast<TRI_vocbase_col_t*>(col));

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

  SingleCollectionReadOnlyTransaction trx(new V8TransactionContext(true), vocbase, col->_cid);

  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  if (trx.orderBarrier(trx.trxCollection()) == nullptr) {
    TRI_V8_THROW_EXCEPTION_MEMORY();
  }

  v8::Handle<v8::Value> result;
  TRI_doc_mptr_copy_t document;
  res = trx.read(&document, key.get());
  res = trx.finish(res);

  TRI_ASSERT(trx.hasBarrier());

  if (res == TRI_ERROR_NO_ERROR) {
    result = TRI_WrapShapedJson<SingleCollectionReadOnlyTransaction>(isolate, trx, col->_cid, document.getDataPtr());
  }

  if (res != TRI_ERROR_NO_ERROR || document.getDataPtr() == nullptr) {  // PROTECTED by trx here
    if (res == TRI_ERROR_NO_ERROR) {
      res = TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND;
    }

    TRI_V8_THROW_EXCEPTION(res);
  }

  if (rid != 0 && document._rid != rid) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_CONFLICT, "revision not found");
  }

  TRI_V8_RETURN(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief loads a collection for usage
////////////////////////////////////////////////////////////////////////////////

TRI_vocbase_col_t const* UseCollection (v8::Handle<v8::Object> collection,
                                        const v8::FunctionCallbackInfo<v8::Value>& args) {

  v8::Isolate* isolate = args.GetIsolate();
  int res = TRI_ERROR_INTERNAL;
  TRI_vocbase_col_t* col = TRI_UnwrapClass<TRI_vocbase_col_t>(collection, WRP_VOCBASE_COL_TYPE);

  if (col != nullptr) {
    if (! col->_isLocal) {
      TRI_CreateErrorObject(isolate, TRI_ERROR_NOT_IMPLEMENTED);
      TRI_set_errno(TRI_ERROR_NOT_IMPLEMENTED);
      return nullptr;
    }

    TRI_vocbase_col_status_e status;
    res = TRI_UseCollectionVocBase(col->_vocbase, col, status);

    if (res == TRI_ERROR_NO_ERROR &&
        col->_collection != nullptr) {
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

static TRI_vector_pointer_t GetCollectionsCluster (TRI_vocbase_t* vocbase) {
  TRI_vector_pointer_t result;
  TRI_InitVectorPointer(&result, TRI_UNKNOWN_MEM_ZONE);

  std::vector<shared_ptr<CollectionInfo> > const& collections
      = ClusterInfo::instance()->getCollections(vocbase->_name);

  for (size_t i = 0, n = collections.size(); i < n; ++i) {
    TRI_vocbase_col_t* c = CoordinatorCollection(vocbase, *(collections[i]));

    if (c != nullptr) {
      TRI_PushBackVectorPointer(&result, c);
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get all cluster collection names
////////////////////////////////////////////////////////////////////////////////

static TRI_vector_string_t GetCollectionNamesCluster (TRI_vocbase_t* vocbase) {
  TRI_vector_string_t result;
  TRI_InitVectorString(&result, TRI_UNKNOWN_MEM_ZONE);

  std::vector<shared_ptr<CollectionInfo> > const& collections
      = ClusterInfo::instance()->getCollections(vocbase->_name);

  for (size_t i = 0, n = collections.size(); i < n; ++i) {
    string const& name = collections[i]->name();
    char* s = TRI_DuplicateString2Z(TRI_UNKNOWN_MEM_ZONE, name.c_str(), name.size());

    if (s != nullptr) {
      TRI_PushBackVectorString(&result, s);
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a document and returns whether it exists
////////////////////////////////////////////////////////////////////////////////

static void ExistsVocbaseCol (bool useCollection,
                              const v8::FunctionCallbackInfo<v8::Value>& args) {
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
    col = TRI_UnwrapClass<TRI_vocbase_col_t>(args.Holder(), WRP_VOCBASE_COL_TYPE);

    if (col == nullptr) {
      TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
    }

    vocbase = col->_vocbase;
  }
  else {
    // called as db._exists()
    vocbase = GetContextVocBase(isolate);
  }

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  V8ResolverGuard resolver(vocbase);
  int err = ParseDocumentOrDocumentHandle(vocbase, resolver.getResolver(), col, key, rid, args[0], args);

  LocalCollectionGuard g(useCollection ? nullptr : const_cast<TRI_vocbase_col_t*>(col));

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

  SingleCollectionReadOnlyTransaction trx(new V8TransactionContext(true), vocbase, col->_cid);

  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  if (trx.orderBarrier(trx.trxCollection()) == nullptr) {
    TRI_V8_THROW_EXCEPTION_MEMORY();
  }

  TRI_doc_mptr_copy_t document;
  res = trx.read(&document, key.get());
  res = trx.finish(res);

  if (res != TRI_ERROR_NO_ERROR || document.getDataPtr() == nullptr) {  // PROTECTED by trx here
    if (res == TRI_ERROR_NO_ERROR) {
      res = TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND;
    }
  }

  if (res == TRI_ERROR_NO_ERROR && rid != 0 && document._rid != rid) {
    res = TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND;
  }

  if (res == TRI_ERROR_NO_ERROR) {
    TRI_V8_RETURN_TRUE();
  }
  else if (res == TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND) {
    TRI_V8_RETURN_FALSE();
  }

  TRI_V8_THROW_EXCEPTION(res);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief modifies a document, coordinator case in a cluster
////////////////////////////////////////////////////////////////////////////////

static void ModifyVocbaseColCoordinator (TRI_vocbase_col_t const* collection,
                                         TRI_doc_update_policy_e policy,
                                         bool waitForSync,
                                         bool isPatch,
                                         bool keepNull, // only counts if isPatch==true
                                         bool mergeObjects, // only counts if isPatch==true                                         bool silent,
                                         bool silent,
                                         const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  TRI_ASSERT(collection != nullptr);

  // First get the initial data:
  string const dbname(collection->_dbName);
  string const collname(collection->_name);

  string key;
  TRI_voc_rid_t rev = 0;
  int error = ParseKeyAndRef(isolate, args[0], key, rev);

  if (error != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(error);
  }

  TRI_json_t* json = TRI_ObjectToJson(isolate, args[1]);
  if (! TRI_IsArrayJson(json)) {
    if (json != nullptr) {
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
    }
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
  }

  triagens::rest::HttpResponse::HttpResponseCode responseCode;
  map<string, string> headers;
  map<string, string> resultHeaders;
  string resultBody;

  error = triagens::arango::modifyDocumentOnCoordinator(
        dbname, collname, key, rev, policy, waitForSync, isPatch,
        keepNull, mergeObjects, json, headers, responseCode, resultHeaders, resultBody);
  // Note that the json has been freed inside!

  if (error != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(error);
  }

  // report what the DBserver told us: this could now be 201/202 or
  // 400/404
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, resultBody.c_str());
  if (responseCode >= triagens::rest::HttpResponse::BAD) {
    if (! TRI_IsArrayJson(json)) {
      if (nullptr != json) {
        TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
      }
      TRI_V8_THROW_EXCEPTION(TRI_ERROR_INTERNAL);
    }
    int errorNum = 0;
    TRI_json_t* subjson = TRI_LookupArrayJson(json, "errorNum");
    if (TRI_IsNumberJson(subjson)) {
      errorNum = static_cast<int>(subjson->_value._number);
    }
    string errorMessage;
    subjson = TRI_LookupArrayJson(json, "errorMessage");
    if (TRI_IsStringJson(subjson)) {
      errorMessage = string(subjson->_value._string.data,
                            subjson->_value._string.length-1);
    }
    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
    TRI_V8_THROW_EXCEPTION_MESSAGE(errorNum, errorMessage);
  }
 
  if (silent) {
    if (json != nullptr) {
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
    }
    TRI_V8_RETURN_TRUE();
  }
  else {
    v8::Handle<v8::Value> ret = TRI_ObjectJson(isolate, json);
    if (json != nullptr) {
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
    }
    TRI_V8_RETURN(ret);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief replaces a document
////////////////////////////////////////////////////////////////////////////////

void ReplaceVocbaseCol (bool useCollection,
                        const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);
  UpdateOptions options;
  TRI_doc_update_policy_e policy = TRI_DOC_UPDATE_ERROR;

  // check the arguments
  uint32_t const argLength = args.Length();
  TRI_GET_GLOBALS();

  if (argLength < 2) {
    TRI_V8_THROW_EXCEPTION_USAGE("replace(<document>, <data>, {overwrite: booleanValue, waitForSync: booleanValue})");
  }

  // we're only accepting "real" object documents
  if (! args[1]->IsObject() || args[1]->IsArray()) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
  }

  if (args.Length() > 2) {
    if (args[2]->IsObject()) {
      v8::Handle<v8::Object> optionsObject = args[2].As<v8::Object>();
      TRI_GET_GLOBAL_STRING(OverwriteKey);
      if (optionsObject->Has(OverwriteKey)) {
        options.overwrite = TRI_ObjectToBoolean(optionsObject->Get(OverwriteKey));
        policy = ExtractUpdatePolicy(options.overwrite);
      }
      TRI_GET_GLOBAL_STRING(WaitForSyncKey);
      if (optionsObject->Has(WaitForSyncKey)) {
        options.waitForSync = TRI_ObjectToBoolean(optionsObject->Get(WaitForSyncKey));
      }
      TRI_GET_GLOBAL_STRING(SilentKey);
      if (optionsObject->Has(SilentKey)) {
        options.silent = TRI_ObjectToBoolean(optionsObject->Get(SilentKey));
      }
    }
    else {// old variant replace(<document>, <data>, <overwrite>, <waitForSync>)
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
    col = TRI_UnwrapClass<TRI_vocbase_col_t>(args.Holder(), WRP_VOCBASE_COL_TYPE);

    if (col == nullptr) {
      TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
    }

    vocbase = col->_vocbase;
  }
  else {
    // called as db._replace()
    vocbase = GetContextVocBase(isolate);
  }

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  V8ResolverGuard resolver(vocbase);
  int err = ParseDocumentOrDocumentHandle(vocbase, resolver.getResolver(), col, key, rid, args[0], args);

  LocalCollectionGuard g(useCollection ? nullptr : const_cast<TRI_vocbase_col_t*>(col));

  if (key.get() == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD);
  }

  if (err != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(err);
  }

  TRI_ASSERT(col != nullptr);
  TRI_ASSERT(key.get() != nullptr);

  if (ServerState::instance()->isCoordinator()) {
    ModifyVocbaseColCoordinator(col,
                                policy,
                                options.waitForSync,
                                false,  // isPatch
                                true,   // keepNull, does not matter
                                false,   // mergeObjects, does not matter                                options.silent,
                                options.silent,
                                args);
    return;
  }

  SingleCollectionWriteTransaction<1> trx(new V8TransactionContext(true), vocbase, col->_cid);
  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  TRI_document_collection_t* document = trx.documentCollection();
  TRI_memory_zone_t* zone = document->getShaper()->_memoryZone;  // PROTECTED by trx here

  TRI_doc_mptr_copy_t mptr;

  if (trx.orderBarrier(trx.trxCollection()) == nullptr) {
    TRI_V8_THROW_EXCEPTION_MEMORY();
  }

  // we must lock here, because below we are
  // - reading the old document in coordinator case
  // - creating a shape, which might trigger a write into the collection
  trx.lockWrite();

  if (ServerState::instance()->isDBserver()) {
    // compare attributes in shardKeys
    const string cidString = StringUtils::itoa(document->_info._planId);

    TRI_json_t* json = TRI_ObjectToJson(isolate, args[1]);

    if (json == nullptr) {
      TRI_V8_THROW_EXCEPTION_MEMORY();
    }

    res = trx.read(&mptr, key.get());

    if (res != TRI_ERROR_NO_ERROR || mptr.getDataPtr() == nullptr) {  // PROTECTED by trx here
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
      TRI_V8_THROW_EXCEPTION(res);
    }

    TRI_shaped_json_t shaped;
    TRI_EXTRACT_SHAPED_JSON_MARKER(shaped, mptr.getDataPtr());  // PROTECTED by trx here
    TRI_json_t* old = TRI_JsonShapedJson(document->getShaper(), &shaped);  // PROTECTED by trx here

    if (old == nullptr) {
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
      TRI_V8_THROW_EXCEPTION_MEMORY();
    }

    if (shardKeysChanged(col->_dbName, cidString, old, json, false)) {
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, old);
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
      TRI_V8_THROW_EXCEPTION(TRI_ERROR_CLUSTER_MUST_NOT_CHANGE_SHARDING_ATTRIBUTES);
    }

    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, old);
  }

  TRI_shaped_json_t* shaped = TRI_ShapedJsonV8Object(isolate, args[1], document->getShaper(), true);  // PROTECTED by trx here

  if (shaped == nullptr) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_errno(), "<data> cannot be converted into JSON shape");
  }

  res = trx.updateDocument(key.get(), &mptr, shaped, policy, options.waitForSync, rid, &actualRevision);

  res = trx.finish(res);

  TRI_FreeShapedJson(zone, shaped);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  TRI_ASSERT(mptr.getDataPtr() != nullptr);  // PROTECTED by trx here

  if (options.silent) {
    TRI_V8_RETURN_TRUE();
  }
  else {
    char const* docKey = TRI_EXTRACT_MARKER_KEY(&mptr);  // PROTECTED by trx here

    v8::Handle<v8::Object> result = v8::Object::New(isolate);
    TRI_GET_GLOBAL_STRING(_IdKey);
    TRI_GET_GLOBAL_STRING(_RevKey);
    TRI_GET_GLOBAL_STRING(_OldRevKey);
    TRI_GET_GLOBAL_STRING(_KeyKey);
    result->Set(_IdKey,     V8DocumentId(isolate, trx.resolver()->getCollectionName(col->_cid), docKey));
    result->Set(_RevKey,    V8RevisionId(isolate, mptr._rid));
    result->Set(_OldRevKey, V8RevisionId(isolate, actualRevision));
    result->Set(_KeyKey,    TRI_V8_STRING(docKey));

    TRI_V8_RETURN(result);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief inserts a document
////////////////////////////////////////////////////////////////////////////////

static void InsertVocbaseCol (TRI_vocbase_col_t* col,
                              const v8::FunctionCallbackInfo<v8::Value>& args) {
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
      options.waitForSync = TRI_ObjectToBoolean(optionsObject->Get(WaitForSyncKey));
    }
    TRI_GET_GLOBAL_STRING(SilentKey);
    if (optionsObject->Has(SilentKey)) {
      options.silent = TRI_ObjectToBoolean(optionsObject->Get(SilentKey));
    }
  }
  else {
    options.waitForSync = ExtractWaitForSync(args, 2);
  }

  // set document key
  std::unique_ptr<char[]> key;
  int res;

  if (args[0]->IsObject() && ! args[0]->IsArray()) {
    res = ExtractDocumentKey(isolate, v8g, args[0]->ToObject(), key);

    if (res != TRI_ERROR_NO_ERROR && res != TRI_ERROR_ARANGO_DOCUMENT_KEY_MISSING) {
      TRI_V8_THROW_EXCEPTION(res);
    }
  }
  else {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
  }

  SingleCollectionWriteTransaction<1> trx(new V8TransactionContext(true), col->_vocbase, col->_cid);

  res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  // fetch a barrier so nobody unlinks datafiles with the shapes & attributes we might
  // need for this document
  if (trx.orderBarrier(trx.trxCollection()) == nullptr) {
    TRI_V8_THROW_EXCEPTION_MEMORY();
  }

  TRI_document_collection_t* document = trx.documentCollection();
  TRI_memory_zone_t* zone = document->getShaper()->_memoryZone;  // PROTECTED by trx from above

  TRI_shaped_json_t* shaped = TRI_ShapedJsonV8Object(isolate, args[0], document->getShaper(), true);  // PROTECTED by trx from above

  if (shaped == nullptr) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_errno(), "<data> cannot be converted into JSON shape");
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
  }
  else {
    char const* docKey = TRI_EXTRACT_MARKER_KEY(&mptr);  // PROTECTED by trx here

    v8::Handle<v8::Object> result = v8::Object::New(isolate);
    TRI_GET_GLOBAL_STRING(_IdKey);
    TRI_GET_GLOBAL_STRING(_RevKey);
    TRI_GET_GLOBAL_STRING(_KeyKey);
    result->Set(_IdKey,  V8DocumentId(isolate, trx.resolver()->getCollectionName(col->_cid), docKey));
    result->Set(_RevKey, V8RevisionId(isolate, mptr._rid));
    result->Set(_KeyKey, TRI_V8_STRING(docKey));

    TRI_V8_RETURN(result);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief updates (patches) a document
////////////////////////////////////////////////////////////////////////////////

static void UpdateVocbaseCol (bool useCollection,
                              const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);
  UpdateOptions options;
  TRI_doc_update_policy_e policy = TRI_DOC_UPDATE_ERROR;

  // check the arguments
  uint32_t const argLength = args.Length();
  
  TRI_GET_GLOBALS();

  if (argLength < 2 || argLength > 5) {
    TRI_V8_THROW_EXCEPTION_USAGE("update(<document>, <data>, {overwrite: booleanValue, keepNull: booleanValue, mergeObjects: booleanValue, waitForSync: booleanValue})");
  }

  if (argLength > 2) {
    if (args[2]->IsObject()) {
      v8::Handle<v8::Object> optionsObject = args[2].As<v8::Object>();
      TRI_GET_GLOBAL_STRING(OverwriteKey);
      if (optionsObject->Has(OverwriteKey)) {
        options.overwrite = TRI_ObjectToBoolean(optionsObject->Get(OverwriteKey));
        policy = ExtractUpdatePolicy(options.overwrite);
      }
      TRI_GET_GLOBAL_STRING(KeepNullKey);
      if (optionsObject->Has(KeepNullKey)) {
        options.keepNull = TRI_ObjectToBoolean(optionsObject->Get(KeepNullKey));
      }
      TRI_GET_GLOBAL_STRING(MergeObjectsKey);
      if (optionsObject->Has(MergeObjectsKey)) {
        options.mergeObjects = TRI_ObjectToBoolean(optionsObject->Get(MergeObjectsKey));
      }
      TRI_GET_GLOBAL_STRING(WaitForSyncKey);
      if (optionsObject->Has(WaitForSyncKey)) {
        options.waitForSync = TRI_ObjectToBoolean(optionsObject->Get(WaitForSyncKey));
      }
      TRI_GET_GLOBAL_STRING(SilentKey);
      if (optionsObject->Has(SilentKey)) {
        options.silent = TRI_ObjectToBoolean(optionsObject->Get(SilentKey));
      }
    }
    else { // old variant update(<document>, <data>, <overwrite>, <keepNull>, <waitForSync>)
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
    col = TRI_UnwrapClass<TRI_vocbase_col_t>(args.Holder(), WRP_VOCBASE_COL_TYPE);

    if (col == nullptr) {
      TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
    }

    vocbase = col->_vocbase;
  }
  else {
    // called as db._update()
    vocbase = GetContextVocBase(isolate);
  }

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  V8ResolverGuard resolver(vocbase);
  int err = ParseDocumentOrDocumentHandle(vocbase, resolver.getResolver(), col, key, rid, args[0], args);

  LocalCollectionGuard g(useCollection ? nullptr : const_cast<TRI_vocbase_col_t*>(col));

  if (key.get() == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD);
  }

  if (err != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(err);
  }

  TRI_ASSERT(col != nullptr);
  TRI_ASSERT(key.get() != nullptr);

  if (ServerState::instance()->isCoordinator()) {
    ModifyVocbaseColCoordinator(col,
                                policy,
                                options.waitForSync,
                                true,  // isPatch
                                options.keepNull,
                                options.mergeObjects,
                                options.silent,
                                args);
    return;
  }

  if (! args[1]->IsObject() || args[1]->IsArray()) {
    // we're only accepting "real" object documents
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
  }

  TRI_json_t* json = TRI_ObjectToJson(isolate, args[1]);

  if (json == nullptr) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_errno(), "<data> is no valid JSON");
  }


  SingleCollectionWriteTransaction<1> trx(new V8TransactionContext(true), vocbase, col->_cid);
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

  if (trx.orderBarrier(trx.trxCollection()) == nullptr) {
    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
    TRI_V8_THROW_EXCEPTION_MEMORY();
  }


  TRI_document_collection_t* document = trx.documentCollection();
  TRI_memory_zone_t* zone = document->getShaper()->_memoryZone;  // PROTECTED by trx here

  TRI_shaped_json_t shaped;
  TRI_EXTRACT_SHAPED_JSON_MARKER(shaped, mptr.getDataPtr());  // PROTECTED by trx here
  TRI_json_t* old = TRI_JsonShapedJson(document->getShaper(), &shaped);  // PROTECTED by trx here

  if (old == nullptr) {
    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
    TRI_V8_THROW_EXCEPTION_MEMORY();
  }

  if (ServerState::instance()->isDBserver()) {
    // compare attributes in shardKeys
    const string cidString = StringUtils::itoa(document->_info._planId);

    if (shardKeysChanged(col->_dbName, cidString, old, json, true)) {
      TRI_FreeJson(document->getShaper()->_memoryZone, old);  // PROTECTED by trx here
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);

      TRI_V8_THROW_EXCEPTION(TRI_ERROR_CLUSTER_MUST_NOT_CHANGE_SHARDING_ATTRIBUTES);
    }
  }

  TRI_json_t* patchedJson = TRI_MergeJson(TRI_UNKNOWN_MEM_ZONE, old, json, ! options.keepNull, options.mergeObjects);
  TRI_FreeJson(zone, old);
  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);

  if (patchedJson == nullptr) {
    TRI_V8_THROW_EXCEPTION_MEMORY();
  }

  res = trx.updateDocument(key.get(), &mptr, patchedJson, policy, options.waitForSync, rid, &actualRevision);

  res = trx.finish(res);

  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, patchedJson);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  TRI_ASSERT(mptr.getDataPtr() != nullptr);  // PROTECTED by trx here

  if (options.silent) {
    TRI_V8_RETURN_TRUE();
  }
  else {
    char const* docKey = TRI_EXTRACT_MARKER_KEY(&mptr);  // PROTECTED by trx here

    v8::Handle<v8::Object> result = v8::Object::New(isolate);
    TRI_GET_GLOBAL_STRING(_IdKey);
    TRI_GET_GLOBAL_STRING(_RevKey);
    TRI_GET_GLOBAL_STRING(_OldRevKey);
    TRI_GET_GLOBAL_STRING(_KeyKey);
    result->Set(_IdKey,     V8DocumentId(isolate, trx.resolver()->getCollectionName(col->_cid), docKey));
    result->Set(_RevKey,    V8RevisionId(isolate, mptr._rid));
    result->Set(_OldRevKey, V8RevisionId(isolate, actualRevision));
    result->Set(_KeyKey,    TRI_V8_STRING(docKey));

    TRI_V8_RETURN(result);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes a document, coordinator case in a cluster
////////////////////////////////////////////////////////////////////////////////

static void RemoveVocbaseColCoordinator (TRI_vocbase_col_t const* collection,
                                         TRI_doc_update_policy_e policy,
                                         bool waitForSync,
                                         const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  // First get the initial data:
  string const dbname(collection->_dbName);
  string const collname(collection->_name);

  string key;
  TRI_voc_rid_t rev = 0;
  int error = ParseKeyAndRef(isolate, args[0], key, rev);

  if (error != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(error);
  }

  triagens::rest::HttpResponse::HttpResponseCode responseCode;
  map<string, string> resultHeaders;
  string resultBody;
  map<string, string> headers;

  error = triagens::arango::deleteDocumentOnCoordinator(
            dbname, collname, key, rev, policy, waitForSync, headers,
            responseCode, resultHeaders, resultBody);

  if (error != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(error);
  }
  // report what the DBserver told us: this could now be 200/202 or
  // 404/412
  TRI_json_t* json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, resultBody.c_str());
  if (responseCode >= triagens::rest::HttpResponse::BAD) {
    if (! TRI_IsArrayJson(json)) {
      if (nullptr != json) {
        TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
      }
      TRI_V8_THROW_EXCEPTION(TRI_ERROR_INTERNAL);
    }
    int errorNum = 0;
    TRI_json_t* subjson = TRI_LookupArrayJson(json, "errorNum");
    if (TRI_IsNumberJson(subjson)) {
      errorNum = static_cast<int>(subjson->_value._number);
    }
    string errorMessage;
    subjson = TRI_LookupArrayJson(json, "errorMessage");
    if (TRI_IsStringJson(subjson)) {
      errorMessage = string(subjson->_value._string.data,
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

static void RemoveVocbaseCol (bool useCollection,
                              const v8::FunctionCallbackInfo<v8::Value>& args) {
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
        options.overwrite = TRI_ObjectToBoolean(optionsObject->Get(OverwriteKey));
        policy = ExtractUpdatePolicy(options.overwrite);
      }
      TRI_GET_GLOBAL_STRING(WaitForSyncKey);
      if (optionsObject->Has(WaitForSyncKey)) {
        options.waitForSync = TRI_ObjectToBoolean(optionsObject->Get(WaitForSyncKey));
      }
    }
    else {// old variant replace(<document>, <data>, <overwrite>, <waitForSync>)
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
    col = TRI_UnwrapClass<TRI_vocbase_col_t>(args.Holder(), WRP_VOCBASE_COL_TYPE);

    if (col == nullptr) {
      TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
    }

    vocbase = col->_vocbase;
  }
  else {
    // called as db._remove()
    vocbase = GetContextVocBase(isolate);
  }

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  V8ResolverGuard resolver(vocbase);
  int err = ParseDocumentOrDocumentHandle(vocbase, resolver.getResolver(), col, key, rid, args[0], args);

  LocalCollectionGuard g(useCollection ? nullptr : const_cast<TRI_vocbase_col_t*>(col));

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

  SingleCollectionWriteTransaction<1> trx(new V8TransactionContext(true), vocbase, col->_cid);
  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  res = trx.deleteDocument(key.get(), policy, options.waitForSync, rid, &actualRevision);
  res = trx.finish(res);

  if (res != TRI_ERROR_NO_ERROR) {
    if (res == TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND && policy == TRI_DOC_UPDATE_LAST_WRITE) {
      TRI_V8_RETURN_FALSE();
    }
    else {
      TRI_V8_THROW_EXCEPTION(res);
    }
  }

  TRI_V8_RETURN_TRUE();
}


////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a document
/// @startDocuBlock documentsCollectionName
/// `collection.document(document)`
///
/// The *document* method finds a document given its identifier or a document
/// object containing the *_id* or *_key* attribute. The method returns
/// the document if it can be found.
///
/// An error is thrown if *_rev* is specified but the document found has a
/// different revision already. An error is also thrown if no document exists
/// with the given *_id* or *_key* value.
///
/// Please note that if the method is executed on the arangod server (e.g. from
/// inside a Foxx application), an immutable document object will be returned
/// for performance reasons. It is not possible to change attributes of this
/// immutable object. To update or patch the returned document, it needs to be
/// cloned/copied into a regular JavaScript object first. This is not necessary
/// if the *document* method is called from out of arangosh or from any other
/// client.
///
/// `collection.document(document-handle)`
///
/// As before. Instead of document a *document-handle* can be passed as
/// first argument.
///
/// *Examples*
///
/// Returns the document for a document-handle:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{documentsCollectionName}
/// ~ db._create("example");
/// ~ var myid = db.example.save({_key: "2873916"});
///   db.example.document("example/2873916");
/// ~ db._drop("example");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// An error is raised if the document is unknown:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{documentsCollectionNameUnknown}
/// ~ db._create("example");
/// ~ var myid = db.example.save({_key: "2873916"});
///   db.example.document("example/4472917");
/// ~ db._drop("example");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// An error is raised if the handle is invalid:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{documentsCollectionNameHandle}
/// ~ db._create("example");
///   db.example.document("");
/// ~ db._drop("example");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

static void JS_DocumentVocbaseCol (const v8::FunctionCallbackInfo<v8::Value>& args) {
  DocumentVocbaseCol(true, args);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief drops a collection, case of a coordinator in a cluster
////////////////////////////////////////////////////////////////////////////////

static void DropVocbaseColCoordinator (const v8::FunctionCallbackInfo<v8::Value>& args,
                                       TRI_vocbase_col_t* collection) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  if (! collection->_canDrop) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_FORBIDDEN);
  }

  string const databaseName(collection->_dbName);
  string const cid = StringUtils::itoa(collection->_cid);

  ClusterInfo* ci = ClusterInfo::instance();
  string errorMsg;

  int res = ci->dropCollectionCoordinator(databaseName, cid, errorMsg, 120.0);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(res, errorMsg);
  }

  collection->_status = TRI_VOC_COL_STATUS_DELETED;

  TRI_V8_RETURN_UNDEFINED();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief drops a collection
/// @startDocuBlock collectionDrop
/// `collection.drop()`
///
/// Drops a *collection* and all its indexes.
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_OUTPUT{collectionDrop}
/// ~ db._create("example");
///   col = db.example;
///   col.drop();
///   col;
/// ~ db._drop("example");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

static void JS_DropVocbaseCol (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  TRI_vocbase_col_t* collection = TRI_UnwrapClass<TRI_vocbase_col_t>(args.Holder(), WRP_VOCBASE_COL_TYPE);

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
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks whether a document exists
/// @startDocuBlock documentsCollectionExists
/// `collection.exists(document)`
///
/// The *exists* method determines whether a document exists given its
/// identifier.  Instead of returning the found document or an error, this
/// method will return either *true* or *false*. It can thus be used
/// for easy existence checks.
///
/// The *document* method finds a document given its identifier.  It returns
/// the document. Note that the returned document contains two
/// pseudo-attributes, namely *_id* and *_rev*. *_id* contains the
/// document-handle and *_rev* the revision of the document.
///
/// No error will be thrown if the sought document or collection does not
/// exist.
/// Still this method will throw an error if used improperly, e.g. when called
/// with a non-document handle, a non-document, or when a cross-collection
/// request is performed.
///
/// `collection.exists(document-handle)`
///
/// As before. Instead of document a *document-handle* can be passed as
/// first argument.
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

static void JS_ExistsVocbaseCol (const v8::FunctionCallbackInfo<v8::Value>& args) {
  return ExistsVocbaseCol(true, args);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief fetch the figures for a sharded collection
////////////////////////////////////////////////////////////////////////////////

static TRI_doc_collection_info_t* GetFiguresCoordinator (TRI_vocbase_col_t* collection) {
  TRI_ASSERT(collection != nullptr);

  string const databaseName(collection->_dbName);
  string const cid = StringUtils::itoa(collection->_cid);

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

static TRI_doc_collection_info_t* GetFigures (TRI_vocbase_col_t* collection) {
  TRI_ASSERT(collection != nullptr);

  SingleCollectionReadOnlyTransaction trx(new V8TransactionContext(true), collection->_vocbase, collection->_cid);

  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_set_errno(res);
    return nullptr;
  }

  // READ-LOCK start
  trx.lockRead();

  TRI_document_collection_t* document = collection->_collection;
  TRI_doc_collection_info_t* info = document->figures(document);

  res = trx.finish(res);
  // READ-LOCK end

  return info;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the figures of a collection
/// @startDocuBlock collectionFigures
/// `collection.figures()`
///
/// Returns an object containing statistics about the collection.
/// **Note** : Retrieving the figures will always load the collection into 
/// memory.
///
/// * *alive.count*: The number of curretly active documents in all datafiles and
///   journals of the collection. Documents that are contained in the
///   write-ahead log only are not reported in this figure.
/// * *alive.size*: The total size in bytes used by all active documents of the
///   collection. Documents that are contained in the write-ahead log only are
///   not reported in this figure.
/// - *dead.count*: The number of dead documents. This includes document
///   versions that have been deleted or replaced by a newer version. Documents
///   deleted or replaced that are contained in the write-ahead log only are not
///   reported in this figure.
/// * *dead.size*: The total size in bytes used by all dead documents.
/// * *dead.deletion*: The total number of deletion markers. Deletion markers
///   only contained in the write-ahead log are not reporting in this figure.
/// * *datafiles.count*: The number of datafiles.
/// * *datafiles.fileSize*: The total filesize of datafiles (in bytes).
/// * *journals.count*: The number of journal files.
/// * *journals.fileSize*: The total filesize of the journal files
///   (in bytes).
/// * *compactors.count*: The number of compactor files.
/// * *compactors.fileSize*: The total filesize of the compactor files
///   (in bytes).
/// * *shapefiles.count*: The number of shape files. This value is
///   deprecated and kept for compatibility reasons only. The value will always
///   be 0 since ArangoDB 2.0 and higher.
/// * *shapefiles.fileSize*: The total filesize of the shape files. This
///   value is deprecated and kept for compatibility reasons only. The value will
///   always be 0 in ArangoDB 2.0 and higher.
/// * *shapes.count*: The total number of shapes used in the collection.
///   This includes shapes that are not in use anymore. Shapes that are contained
///   in the write-ahead log only are not reported in this figure.
/// * *shapes.size*: The total size of all shapes (in bytes). This includes
///   shapes that are not in use anymore. Shapes that are contained in the
///   write-ahead log only are not reported in this figure.
/// * *attributes.count*: The total number of attributes used in the
///   collection. Note: the value includes data of attributes that are not in use
///   anymore. Attributes that are contained in the write-ahead log only are
///   not reported in this figure.
/// * *attributes.size*: The total size of the attribute data (in bytes).
///   Note: the value includes data of attributes that are not in use anymore.
///   Attributes that are contained in the write-ahead log only are not 
///   reported in this figure.
/// * *indexes.count*: The total number of indexes defined for the
///   collection, including the pre-defined indexes (e.g. primary index).
/// * *indexes.size*: The total memory allocated for indexes in bytes.
/// * *maxTick*: The tick of the last marker that was stored in a journal
///   of the collection. This might be 0 if the collection does not yet have
///   a journal.
/// * *uncollectedLogfileEntries*: The number of markers in the write-ahead
///   log for this collection that have not been transferred to journals or
///   datafiles.
///
/// **Note**: collection data that are stored in the write-ahead log only are
/// not reported in the results. When the write-ahead log is collected, documents
/// might be added to journals and datafiles of the collection, which may modify 
/// the figures of the collection.
///
/// Additionally, the filesizes of collection and index parameter JSON files are
/// not reported. These files should normally have a size of a few bytes
/// each. Please also note that the *fileSize* values are reported in bytes
/// and reflect the logical file sizes. Some filesystems may use optimisations
/// (e.g. sparse files) so that the actual physical file size is somewhat
/// different. Directories and sub-directories may also require space in the
/// file system, but this space is not reported in the *fileSize* results.
///
/// That means that the figures reported do not reflect the actual disk
/// usage of the collection with 100% accuracy. The actual disk usage of
/// a collection is normally slightly higher than the sum of the reported 
/// *fileSize* values. Still the sum of the *fileSize* values can still be 
/// used as a lower bound approximation of the disk usage.
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_OUTPUT{collectionFigures}
/// ~ require("internal").wal.flush(true, true);
///   db.demo.figures()
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

static void JS_FiguresVocbaseCol (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  TRI_vocbase_col_t* collection = TRI_UnwrapClass<TRI_vocbase_col_t>(args.Holder(), WRP_VOCBASE_COL_TYPE);

  if (collection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  v8::Handle<v8::Object> result = v8::Object::New(isolate);

  TRI_doc_collection_info_t* info;

  if (ServerState::instance()->isCoordinator()) {
    info = GetFiguresCoordinator(collection);
  }
  else {
    info = GetFigures(collection);
  }

  if (info == nullptr) {
    TRI_V8_THROW_EXCEPTION_MEMORY();
  }

  v8::Handle<v8::Object> alive = v8::Object::New(isolate);

  result->Set(TRI_V8_ASCII_STRING("alive"), alive);
  alive->Set(TRI_V8_ASCII_STRING("count"),       v8::Number::New(isolate, (double) info->_numberAlive));
  alive->Set(TRI_V8_ASCII_STRING("size"),        v8::Number::New(isolate, (double) info->_sizeAlive));

  v8::Handle<v8::Object> dead = v8::Object::New(isolate);

  result->Set(TRI_V8_ASCII_STRING("dead"), dead);
  dead->Set(TRI_V8_ASCII_STRING("count"),        v8::Number::New(isolate, (double) info->_numberDead));
  dead->Set(TRI_V8_ASCII_STRING("size"),         v8::Number::New(isolate, (double) info->_sizeDead));
  dead->Set(TRI_V8_ASCII_STRING("deletion"),     v8::Number::New(isolate, (double) info->_numberDeletion));

  // datafile info
  v8::Handle<v8::Object> dfs = v8::Object::New(isolate);

  result->Set(TRI_V8_ASCII_STRING("datafiles"), dfs);
  dfs->Set(TRI_V8_ASCII_STRING("count"),         v8::Number::New(isolate, (double) info->_numberDatafiles));
  dfs->Set(TRI_V8_ASCII_STRING("fileSize"),      v8::Number::New(isolate, (double) info->_datafileSize));

  // journal info
  v8::Handle<v8::Object> js = v8::Object::New(isolate);

  result->Set(TRI_V8_ASCII_STRING("journals"), js);
  js->Set(TRI_V8_ASCII_STRING("count"),          v8::Number::New(isolate, (double) info->_numberJournalfiles));
  js->Set(TRI_V8_ASCII_STRING("fileSize"),       v8::Number::New(isolate, (double) info->_journalfileSize));

  // compactors info
  v8::Handle<v8::Object> cs = v8::Object::New(isolate);

  result->Set(TRI_V8_ASCII_STRING("compactors"), cs);
  cs->Set(TRI_V8_ASCII_STRING("count"),          v8::Number::New(isolate, (double) info->_numberCompactorfiles));
  cs->Set(TRI_V8_ASCII_STRING("fileSize"),       v8::Number::New(isolate, (double) info->_compactorfileSize));

  // shapefiles info
  v8::Handle<v8::Object> sf = v8::Object::New(isolate);

  result->Set(TRI_V8_ASCII_STRING("shapefiles"), sf);
  sf->Set(TRI_V8_ASCII_STRING("count"),          v8::Number::New(isolate, (double) info->_numberShapefiles));
  sf->Set(TRI_V8_ASCII_STRING("fileSize"),       v8::Number::New(isolate, (double) info->_shapefileSize));

  // shape info
  v8::Handle<v8::Object> shapes = v8::Object::New(isolate);
  result->Set(TRI_V8_ASCII_STRING("shapes"),     shapes);
  shapes->Set(TRI_V8_ASCII_STRING("count"),      v8::Number::New(isolate, (double) info->_numberShapes));
  shapes->Set(TRI_V8_ASCII_STRING("size"),       v8::Number::New(isolate, (double) info->_sizeShapes));

  // attributes info
  v8::Handle<v8::Object> attributes = v8::Object::New(isolate);
  result->Set(TRI_V8_ASCII_STRING("attributes"), attributes);
  attributes->Set(TRI_V8_ASCII_STRING("count"),  v8::Number::New(isolate, (double) info->_numberAttributes));
  attributes->Set(TRI_V8_ASCII_STRING("size"),   v8::Number::New(isolate, (double) info->_sizeAttributes));

  v8::Handle<v8::Object> indexes = v8::Object::New(isolate);
  result->Set(TRI_V8_ASCII_STRING("indexes"),    indexes);
  indexes->Set(TRI_V8_ASCII_STRING("count"),     v8::Number::New(isolate, (double) info->_numberIndexes));
  indexes->Set(TRI_V8_ASCII_STRING("size"),      v8::Number::New(isolate, (double) info->_sizeIndexes));

  result->Set(TRI_V8_ASCII_STRING("lastTick"),   V8TickId(isolate, info->_tickMax));
  result->Set(TRI_V8_ASCII_STRING("uncollectedLogfileEntries"), v8::Number::New(isolate, (double) info->_uncollectedLogfileEntries));

  TRI_Free(TRI_UNKNOWN_MEM_ZONE, info);

  TRI_V8_RETURN(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief loads a collection
/// @startDocuBlock collectionLoad
/// `collection.load()`
///
/// Loads a collection into memory.
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_OUTPUT{collectionLoad}
/// ~ db._create("example");
///   col = db.example;
///   col.load();
///   col;
/// ~ db._drop("example");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

static void JS_LoadVocbaseCol (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  TRI_vocbase_t* vocbase = GetContextVocBase(isolate);

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  if (ServerState::instance()->isCoordinator()) {
    TRI_vocbase_col_t const* collection = TRI_UnwrapClass<TRI_vocbase_col_t>(args.Holder(), WRP_VOCBASE_COL_TYPE);

    if (collection == nullptr) {
      TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
    }

    string const databaseName(collection->_dbName);
    string const cid = StringUtils::itoa(collection->_cid);

    int res = ClusterInfo::instance()->setCollectionStatusCoordinator(databaseName, cid, TRI_VOC_COL_STATUS_LOADED);

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
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the name of a collection
////////////////////////////////////////////////////////////////////////////////

static void JS_NameVocbaseCol (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  TRI_vocbase_col_t const* collection = TRI_UnwrapClass<TRI_vocbase_col_t>(args.Holder(), WRP_VOCBASE_COL_TYPE);

  if (collection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  if (! collection->_isLocal) {
    v8::Handle<v8::Value> result = TRI_V8_STRING(collection->_name);
    TRI_V8_RETURN(result);
  }

  // this copies the name into a new place so we can safely access it later
  // if we wouldn't do this, we would risk other threads modifying the name while
  // we're reading it
  char* name = TRI_GetCollectionNameByIdVocBase(collection->_vocbase, collection->_cid);

  if (name == nullptr) {
    TRI_V8_RETURN_UNDEFINED();
  }

  v8::Handle<v8::Value> result = TRI_V8_STRING(name);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, name);

  TRI_V8_RETURN(result);
}

static void JS_PlanIdVocbaseCol (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  TRI_vocbase_col_t const* collection = TRI_UnwrapClass<TRI_vocbase_col_t>(args.Holder(), WRP_VOCBASE_COL_TYPE);

  if (collection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  if (ServerState::instance()->isCoordinator()) {
    TRI_V8_RETURN(V8CollectionId(isolate, collection->_cid));
  }

  TRI_V8_RETURN(V8CollectionId(isolate, collection->_planId));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief gets or sets the properties of a collection
/// @startDocuBlock collectionProperties
/// `collection.properties()`
///
/// Returns an object containing all collection properties.
///
/// * *waitForSync*: If *true* creating a document will only return
///   after the data was synced to disk.
///
/// * *journalSize* : The size of the journal in bytes.
///
/// * *isVolatile*: If *true* then the collection data will be
///   kept in memory only and ArangoDB will not write or sync the data
///   to disk.
///
/// * *keyOptions* (optional) additional options for key generation. This is
///   a JSON array containing the following attributes (note: some of the
///   attributes are optional):
///   * *type*: the type of the key generator used for the collection.
///   * *allowUserKeys*: if set to *true*, then it is allowed to supply
///     own key values in the *_key* attribute of a document. If set to
///     *false*, then the key generator will solely be responsible for
///     generating keys and supplying own key values in the *_key* attribute
///     of documents is considered an error.
///   * *increment*: increment value for *autoincrement* key generator.
///     Not used for other key generator types.
///   * *offset*: initial offset value for *autoincrement* key generator.
///     Not used for other key generator types.
///
/// In a cluster setup, the result will also contain the following attributes:
///
/// * *numberOfShards*: the number of shards of the collection.
///
/// * *shardKeys*: contains the names of document attributes that are used to
///   determine the target shard for documents.
///
/// `collection.properties(properties)`
///
/// Changes the collection properties. *properties* must be a object with
/// one or more of the following attribute(s):
///
/// * *waitForSync*: If *true* creating a document will only return
///   after the data was synced to disk.
///
/// * *journalSize* : The size of the journal in bytes.
///
/// *Note*: it is not possible to change the journal size after the journal or
/// datafile has been created. Changing this parameter will only effect newly
/// created journals. Also note that you cannot lower the journal size to less
/// then size of the largest document already stored in the collection.
///
/// **Note**: some other collection properties, such as *type*, *isVolatile*,
/// or *keyOptions* cannot be changed once the collection is created.
///
/// @EXAMPLES
///
/// Read all properties
///
/// @EXAMPLE_ARANGOSH_OUTPUT{collectionProperties}
/// ~ db._create("example");
///   db.example.properties();
/// ~ db._drop("example");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// Change a property
///
/// @EXAMPLE_ARANGOSH_OUTPUT{collectionProperty}
/// ~ db._create("example");
///   db.example.properties({ waitForSync : true });
/// ~ db._drop("example");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

static void JS_PropertiesVocbaseCol (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);
  TRI_GET_GLOBALS();

  TRI_vocbase_col_t const* collection = TRI_UnwrapClass<TRI_vocbase_col_t>(args.Holder(), WRP_VOCBASE_COL_TYPE);

  if (collection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  if (ServerState::instance()->isCoordinator()) {
    std::string const databaseName = std::string(collection->_dbName);
    TRI_col_info_t info = ClusterInfo::instance()->getCollectionProperties(databaseName, StringUtils::itoa(collection->_cid));

    if (0 < args.Length()) {
      v8::Handle<v8::Value> par = args[0];

      if (par->IsObject()) {
        v8::Handle<v8::Object> po = par->ToObject();

        // extract doCompact flag
        TRI_GET_GLOBAL_STRING(DoCompactKey);
        if (po->Has(DoCompactKey)) {
          info._doCompact = TRI_ObjectToBoolean(po->Get(DoCompactKey));
        }

        // extract sync flag
        TRI_GET_GLOBAL_STRING(WaitForSyncKey);
        if (po->Has(WaitForSyncKey)) {
          info._waitForSync = TRI_ObjectToBoolean(po->Get(WaitForSyncKey));
        }

        // extract the journal size
        TRI_GET_GLOBAL_STRING(JournalSizeKey);
        if (po->Has(JournalSizeKey)) {
          info._maximalSize = (TRI_voc_size_t) TRI_ObjectToUInt64(po->Get(JournalSizeKey), false);

          if (info._maximalSize < TRI_JOURNAL_MINIMAL_SIZE) {
            if (info._keyOptions != nullptr) {
              TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, info._keyOptions);
            }
            TRI_V8_THROW_EXCEPTION_PARAMETER("<properties>.journalSize too small");
          }
        }

        TRI_GET_GLOBAL_STRING(IsVolatileKey);
        if (po->Has(IsVolatileKey)) {
          if (TRI_ObjectToBoolean(po->Get(IsVolatileKey)) != info._isVolatile) {
            if (info._keyOptions != nullptr) {
              TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, info._keyOptions);
            }
            TRI_V8_THROW_EXCEPTION_PARAMETER("isVolatile option cannot be changed at runtime");
          }
        }

        if (info._isVolatile && info._waitForSync) {
          if (info._keyOptions != nullptr) {
            TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, info._keyOptions);
          }
          TRI_V8_THROW_EXCEPTION_PARAMETER("volatile collections do not support the waitForSync option");
        }
      }

      int res = ClusterInfo::instance()->setCollectionPropertiesCoordinator(databaseName, StringUtils::itoa(collection->_cid), &info);

      if (res != TRI_ERROR_NO_ERROR) {
        if (info._keyOptions != nullptr) {
          TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, info._keyOptions);
        }
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
    result->Set(DoCompactKey,   v8::Boolean::New(isolate, info._doCompact));
    result->Set(IsSystemKey,    v8::Boolean::New(isolate, info._isSystem));
    result->Set(IsVolatileKey,  v8::Boolean::New(isolate, info._isVolatile));
    result->Set(JournalSizeKey, v8::Number::New (isolate, info._maximalSize));
    result->Set(WaitForSyncKey, v8::Boolean::New(isolate, info._waitForSync));

    shared_ptr<CollectionInfo> c = ClusterInfo::instance()->getCollection(databaseName, StringUtils::itoa(collection->_cid));
    v8::Handle<v8::Array> shardKeys = v8::Array::New(isolate);
    vector<string> const sks = (*c).shardKeys();
    for (size_t i = 0; i < sks.size(); ++i) {
      shardKeys->Set((uint32_t) i, TRI_V8_STD_STRING(sks[i]));
    }
    result->Set(TRI_V8_ASCII_STRING("shardKeys"), shardKeys);
    result->Set(TRI_V8_ASCII_STRING("numberOfShards"), v8::Number::New(isolate, (*c).numberOfShards()));

    if (info._keyOptions != nullptr) {
      TRI_GET_GLOBAL_STRING(KeyOptionsKey);
      result->Set(KeyOptionsKey, TRI_ObjectJson(isolate, info._keyOptions)->ToObject());
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, info._keyOptions);
    }

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
      v8::Handle<v8::Object> po = par->ToObject();

      // get the old values
      TRI_LOCK_JOURNAL_ENTRIES_DOC_COLLECTION(document);

      TRI_voc_size_t maximalSize = base->_info._maximalSize;
      bool doCompact     = base->_info._doCompact;
      bool waitForSync   = base->_info._waitForSync;

      TRI_UNLOCK_JOURNAL_ENTRIES_DOC_COLLECTION(document);

      // extract doCompact flag
      TRI_GET_GLOBAL_STRING(DoCompactKey);
      if (po->Has(DoCompactKey)) {
        doCompact = TRI_ObjectToBoolean(po->Get(DoCompactKey));
      }

      // extract sync flag
      TRI_GET_GLOBAL_STRING(WaitForSyncKey);
      if (po->Has(WaitForSyncKey)) {
        waitForSync = TRI_ObjectToBoolean(po->Get(WaitForSyncKey));
      }

      // extract the journal size
      TRI_GET_GLOBAL_STRING(JournalSizeKey);
      if (po->Has(JournalSizeKey)) {
        maximalSize = (TRI_voc_size_t) TRI_ObjectToUInt64(po->Get(JournalSizeKey), false);

        if (maximalSize < TRI_JOURNAL_MINIMAL_SIZE) {
          ReleaseCollection(collection);
          TRI_V8_THROW_EXCEPTION_PARAMETER("<properties>.journalSize too small");
        }
      }

      TRI_GET_GLOBAL_STRING(IsVolatileKey);
      if (po->Has(IsVolatileKey)) {
        if (TRI_ObjectToBoolean(po->Get(IsVolatileKey)) != base->_info._isVolatile) {
          ReleaseCollection(collection);
          TRI_V8_THROW_EXCEPTION_PARAMETER("isVolatile option cannot be changed at runtime");
        }
      }

      if (base->_info._isVolatile && waitForSync) {
        // the combination of waitForSync and isVolatile makes no sense
        ReleaseCollection(collection);
        TRI_V8_THROW_EXCEPTION_PARAMETER("volatile collections do not support the waitForSync option");
      }

      // update collection
      TRI_col_info_t newParameter;

      newParameter._doCompact   = doCompact;
      newParameter._maximalSize = maximalSize;
      newParameter._waitForSync = waitForSync;

      // try to write new parameter to file
      bool doSync = base->_vocbase->_settings.forceSyncProperties;
      int res = TRI_UpdateCollectionInfo(base->_vocbase, base, &newParameter, doSync);

      if (res != TRI_ERROR_NO_ERROR) {
        ReleaseCollection(collection);
        TRI_V8_THROW_EXCEPTION(res);
      }

      TRI_json_t* json = TRI_CreateJsonCollectionInfo(&base->_info);

      // now log the property changes
      res = TRI_ERROR_NO_ERROR;

      try {
        triagens::wal::ChangeCollectionMarker marker(base->_vocbase->_id, base->_info._cid, JsonHelper::toString(json));
        triagens::wal::SlotInfoCopy slotInfo = triagens::wal::LogfileManager::instance()->allocateAndWrite(marker, false);

        if (slotInfo.errorCode != TRI_ERROR_NO_ERROR) {
          THROW_ARANGO_EXCEPTION(slotInfo.errorCode);
        }
      }
      catch (triagens::arango::Exception const& ex) {
        res = ex.code();
      }
      catch (...) {
        res = TRI_ERROR_INTERNAL;
      }

      if (res != TRI_ERROR_NO_ERROR) {
        // TODO: what to do here
        LOG_WARNING("could not save collection change marker in log: %s", TRI_errno_string(res));
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
  result->Set(DoCompactKey,   v8::Boolean::New(isolate, base->_info._doCompact));
  result->Set(IsSystemKey,    v8::Boolean::New(isolate, base->_info._isSystem));
  result->Set(IsVolatileKey,  v8::Boolean::New(isolate, base->_info._isVolatile));
  result->Set(JournalSizeKey, v8::Number::New( isolate, base->_info._maximalSize));

  TRI_json_t* keyOptions = document->_keyGenerator->toJson(TRI_UNKNOWN_MEM_ZONE);

  TRI_GET_GLOBAL_STRING(KeyOptionsKey);
  if (keyOptions != nullptr) {
    result->Set(KeyOptionsKey, TRI_ObjectJson(isolate, keyOptions)->ToObject());

    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, keyOptions);
  }
  else {
    result->Set(KeyOptionsKey, v8::Array::New(isolate));
  }
  TRI_GET_GLOBAL_STRING(WaitForSyncKey);
  result->Set(WaitForSyncKey, v8::Boolean::New(isolate, base->_info._waitForSync));

  ReleaseCollection(collection);
  TRI_V8_RETURN(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes a document
/// @startDocuBlock documentsCollectionRemove
/// `collection.remove(document)`
///
/// Removes a document. If there is revision mismatch, then an error is thrown.
///
/// `collection.remove(document, true)`
///
/// Removes a document. If there is revision mismatch, then mismatch is ignored
/// and document is deleted. The function returns *true* if the document
/// existed and was deleted. It returns *false*, if the document was already
/// deleted.
///
/// `collection.remove(document, true, waitForSync)`
///
/// The optional *waitForSync* parameter can be used to force synchronization
/// of the document deletion operation to disk even in case that the
/// *waitForSync* flag had been disabled for the entire collection.  Thus,
/// the *waitForSync* parameter can be used to force synchronization of just
/// specific operations. To use this, set the *waitForSync* parameter to
/// *true*. If the *waitForSync* parameter is not specified or set to
/// *false*, then the collection's default *waitForSync* behavior is
/// applied. The *waitForSync* parameter cannot be used to disable
/// synchronization for collections that have a default *waitForSync* value
/// of *true*.
///
/// `collection.remove(document-handle, data)`
///
/// As before. Instead of document a *document-handle* can be passed as
/// first argument.
///
/// @EXAMPLES
///
/// Remove a document:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{documentDocumentRemove}
/// ~ db._create("example");
///   a1 = db.example.save({ a : 1 });
///   db.example.document(a1);
///   db.example.remove(a1);
///   db.example.document(a1);
/// ~ db._drop("example");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// Remove a document with a conflict:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{documentDocumentRemoveConflict}
/// ~ db._create("example");
///   a1 = db.example.save({ a : 1 });
///   a2 = db.example.replace(a1, { a : 2 });
///   db.example.remove(a1);
///   db.example.remove(a1, true);
///   db.example.document(a1);
/// ~ db._drop("example");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

static void JS_RemoveVocbaseCol (const v8::FunctionCallbackInfo<v8::Value>& args) {
  return RemoveVocbaseCol(true, args);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief renames a collection
/// @startDocuBlock collectionRename
/// `collection.rename(new-name)`
///
/// Renames a collection using the *new-name*. The *new-name* must not
/// already be used for a different collection. *new-name* must also be a
/// valid collection name. For more information on valid collection names please refer
/// to the [naming conventions](../NamingConventions/README.md).
///
/// If renaming fails for any reason, an error is thrown.
///
/// **Note**: this method is not available in a cluster.
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_OUTPUT{collectionRename}
/// ~ db._create("example");
///   c = db.example;
///   c.rename("better-example");
///   c;
/// ~ db._drop("example");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

static void JS_RenameVocbaseCol (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  if (args.Length() < 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("rename(<name>)");
  }

  if (ServerState::instance()->isCoordinator()) {
    // renaming a collection in a cluster is unsupported
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_CLUSTER_UNSUPPORTED);
  }

  string const name = TRI_ObjectToString(args[0]);

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

  TRI_vocbase_col_t* collection = TRI_UnwrapClass<TRI_vocbase_col_t>(args.Holder(), WRP_VOCBASE_COL_TYPE);

  if (collection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  PREVENT_EMBEDDED_TRANSACTION();

  if (ServerState::instance()->isCoordinator()) {
    // renaming a collection in a cluster is unsupported
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_CLUSTER_UNSUPPORTED);
  }

  int res = TRI_RenameCollectionVocBase(collection->_vocbase,
                                        collection,
                                        name.c_str(),
                                        doOverride, 
                                        true);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(res, "cannot rename collection");
  }

  TRI_V8_RETURN_UNDEFINED();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief replaces a document
/// @startDocuBlock documentsCollectionReplace
/// `collection.replace(document, data)`
///
/// Replaces an existing *document*. The *document* must be a document in
/// the current collection. This document is then replaced with the
/// *data* given as second argument.
///
/// The method returns a document with the attributes *_id*, *_rev* and
/// *{_oldRev*.  The attribute *_id* contains the document handle of the
/// updated document, the attribute *_rev* contains the document revision of
/// the updated document, the attribute *_oldRev* contains the revision of
/// the old (now replaced) document.
///
/// If there is a conflict, i. e. if the revision of the *document* does not
/// match the revision in the collection, then an error is thrown.
///
/// `collection.replace(document, data, true)` or
/// `collection.replace(document, data, overwrite: true)`
///
/// As before, but in case of a conflict, the conflict is ignored and the old
/// document is overwritten.
///
/// `collection.replace(document, data, true, waitForSync)` or
/// `collection.replace(document, data, overwrite: true, waitForSync: true or false)`
///
/// The optional *waitForSync* parameter can be used to force
/// synchronization of the document replacement operation to disk even in case
/// that the *waitForSync* flag had been disabled for the entire collection.
/// Thus, the *waitForSync* parameter can be used to force synchronization
/// of just specific operations. To use this, set the *waitForSync* parameter
/// to *true*. If the *waitForSync* parameter is not specified or set to
/// *false*, then the collection's default *waitForSync* behavior is
/// applied. The *waitForSync* parameter cannot be used to disable
/// synchronization for collections that have a default *waitForSync* value
/// of *true*.
///m
/// `collection.replace(document-handle, data)`
///
/// As before. Instead of document a *document-handle* can be passed as
/// first argument.
///
/// @EXAMPLES
///
/// Create and update a document:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{documentsCollectionReplace}
/// ~ db._create("example");
///   a1 = db.example.save({ a : 1 });
///   a2 = db.example.replace(a1, { a : 2 });
///   a3 = db.example.replace(a1, { a : 3 });
/// ~ db._drop("example");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// Use a document handle:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{documentsCollectionReplaceHandle}
/// ~ db._create("example");
/// ~ var myid = db.example.save({_key: "3903044"});
///   a1 = db.example.save({ a : 1 });
///   a2 = db.example.replace("example/3903044", { a : 2 });
/// ~ db._drop("example");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

static void JS_ReplaceVocbaseCol (const v8::FunctionCallbackInfo<v8::Value>& args) {
  return ReplaceVocbaseCol(true, args);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief fetch the revision for a local collection
////////////////////////////////////////////////////////////////////////////////

static int GetRevision (TRI_vocbase_col_t* collection,
                        TRI_voc_rid_t& rid) {
  TRI_ASSERT(collection != nullptr);

  SingleCollectionReadOnlyTransaction trx(new V8TransactionContext(true), collection->_vocbase, collection->_cid);

  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  // READ-LOCK start
  trx.lockRead();
  rid = collection->_collection->_info._revision;
  trx.finish(res);
  // READ-LOCK end

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief fetch the revision for a sharded collection
////////////////////////////////////////////////////////////////////////////////

static int GetRevisionCoordinator (TRI_vocbase_col_t* collection,
                                   TRI_voc_rid_t& rid) {
  TRI_ASSERT(collection != nullptr);

  string const databaseName(collection->_dbName);
  string const cid = StringUtils::itoa(collection->_cid);

  int res = revisionOnCoordinator(databaseName, cid, rid);

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the revision id of a collection
/// @startDocuBlock collectionRevision
/// `collection.revision()`
///
/// Returns the revision id of the collection
///
/// The revision id is updated when the document data is modified, either by
/// inserting, deleting, updating or replacing documents in it.
///
/// The revision id of a collection can be used by clients to check whether
/// data in a collection has changed or if it is still unmodified since a
/// previous fetch of the revision id.
///
/// The revision id returned is a string value. Clients should treat this value
/// as an opaque string, and only use it for equality/non-equality comparisons.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

static void JS_RevisionVocbaseCol (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  TRI_vocbase_col_t* collection = TRI_UnwrapClass<TRI_vocbase_col_t>(args.Holder(), WRP_VOCBASE_COL_TYPE);

  if (collection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  TRI_voc_rid_t rid;
  int res;

  if (ServerState::instance()->isCoordinator()) {
    res = GetRevisionCoordinator(collection, rid);
  }
  else {
    res = GetRevision(collection, rid);
  }

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  TRI_V8_RETURN(V8RevisionId(isolate, rid));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief rotates the current journal of a collection
/// @startDocuBlock collectionRotate
/// `collection.rotate()`
///
/// Rotates the current journal of a collection. This operation makes the 
/// current journal of the collection a read-only datafile so it may become a
/// candidate for garbage collection. If there is currently no journal available
/// for the collection, the operation will fail with an error.
///
/// **Note**: this method is not available in a cluster.
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

static void JS_RotateVocbaseCol (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
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
}

////////////////////////////////////////////////////////////////////////////////
/// @brief updates a document
/// @startDocuBlock documentsCollectionUpdate
/// `collection.update(document, data, overwrite, keepNull, waitForSync)` or
/// `collection.update(document, data,
/// overwrite: true or false, keepNull: true or false, waitForSync: true or false)`
///
/// Updates an existing *document*. The *document* must be a document in
/// the current collection. This document is then patched with the
/// *data* given as second argument. The optional *overwrite* parameter can
/// be used to control the behavior in case of version conflicts (see below).
/// The optional *keepNull* parameter can be used to modify the behavior when
/// handling *null* values. Normally, *null* values are stored in the
/// database. By setting the *keepNull* parameter to *false*, this behavior
/// can be changed so that all attributes in *data* with *null* values will
/// be removed from the target document.
///
/// The optional *waitForSync* parameter can be used to force
/// synchronization of the document update operation to disk even in case
/// that the *waitForSync* flag had been disabled for the entire collection.
/// Thus, the *waitForSync* parameter can be used to force synchronization
/// of just specific operations. To use this, set the *waitForSync* parameter
/// to *true*. If the *waitForSync* parameter is not specified or set to
/// *false*, then the collection's default *waitForSync* behavior is
/// applied. The *waitForSync* parameter cannot be used to disable
/// synchronization for collections that have a default *waitForSync* value
/// of *true*.
///
/// The method returns a document with the attributes *_id*, *_rev* and
/// *_oldRev*.  The attribute *_id* contains the document handle of the
/// updated document, the attribute *_rev* contains the document revision of
/// the updated document, the attribute *_oldRev* contains the revision of
/// the old (now replaced) document.
///
/// If there is a conflict, i. e. if the revision of the *document* does not
/// match the revision in the collection, then an error is thrown.
///
/// `collection.update(document, data, true)`
///
/// As before, but in case of a conflict, the conflict is ignored and the old
/// document is overwritten.
///
/// collection.update(document-handle, data)`
///
/// As before. Instead of document a document-handle can be passed as
/// first argument.
///
/// *Examples*
///
/// Create and update a document:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{documentsCollectionUpdate}
/// ~ db._create("example");
///   a1 = db.example.save({"a" : 1});
///   a2 = db.example.update(a1, {"b" : 2, "c" : 3});
///   a3 = db.example.update(a1, {"d" : 4});
///   a4 = db.example.update(a2, {"e" : 5, "f" : 6 });
///   db.example.document(a4);
///   a5 = db.example.update(a4, {"a" : 1, c : 9, e : 42 });
///   db.example.document(a5);
/// ~ db._drop("example");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// Use a document handle:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{documentsCollectionUpdateHandle}
/// ~ db._create("example");
/// ~ var myid = db.example.save({_key: "18612115"});
///   a1 = db.example.save({"a" : 1});
///   a2 = db.example.update("example/18612115", { "x" : 1, "y" : 2 });
/// ~ db._drop("example");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// Use the keepNull parameter to remove attributes with null values:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{documentsCollectionUpdateHandleKeepNull}
/// ~ db._create("example");
/// ~ var myid = db.example.save({_key: "19988371"});
///   db.example.save({"a" : 1});
///   db.example.update("example/19988371", { "b" : null, "c" : null, "d" : 3 });
///   db.example.document("example/19988371");
///   db.example.update("example/19988371", { "a" : null }, false, false);
///   db.example.document("example/19988371");
///   db.example.update("example/19988371", { "b" : null, "c": null, "d" : null }, false, false);
///   db.example.document("example/19988371");
/// ~ db._drop("example");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// Patching array values:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{documentsCollectionUpdateHandleArray}
/// ~ db._create("example");
/// ~ var myid = db.example.save({_key: "20774803"});
///   db.example.save({"a" : { "one" : 1, "two" : 2, "three" : 3 }, "b" : { }});
///   db.example.update("example/20774803", {"a" : { "four" : 4 }, "b" : { "b1" : 1 }});
///   db.example.document("example/20774803");
///   db.example.update("example/20774803", { "a" : { "one" : null }, "b" : null }, false, false);
///   db.example.document("example/20774803");
/// ~ db._drop("example");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

static void JS_UpdateVocbaseCol (const v8::FunctionCallbackInfo<v8::Value>& args) {
  return UpdateVocbaseCol(true, args);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief saves a document, coordinator case in a cluster
////////////////////////////////////////////////////////////////////////////////

static void InsertVocbaseColCoordinator (TRI_vocbase_col_t* collection,
                                         const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  // First get the initial data:
  string const dbname(collection->_dbName);

  // TODO: someone might rename the collection while we're reading its name...
  string const collname(collection->_name);

  // Now get the arguments
  uint32_t const argLength = args.Length();
  if (argLength < 1 || argLength > 2) {
    TRI_V8_THROW_EXCEPTION_USAGE("insert(<data>, [<waitForSync>])");
  }

  InsertOptions options;
  if (argLength > 1 && args[1]->IsObject()) {
    TRI_GET_GLOBALS();
    v8::Handle<v8::Object> optionsObject = args[1].As<v8::Object>();
    TRI_GET_GLOBAL_STRING(WaitForSyncKey);
    if (optionsObject->Has(WaitForSyncKey)) {
      options.waitForSync = TRI_ObjectToBoolean(optionsObject->Get(WaitForSyncKey));
    }
    TRI_GET_GLOBAL_STRING(SilentKey);
    if (optionsObject->Has(SilentKey)) {
      options.silent = TRI_ObjectToBoolean(optionsObject->Get(SilentKey));
    }
  }
  else {
    options.waitForSync = ExtractWaitForSync(args, 2);
  }

  TRI_json_t* json = TRI_ObjectToJson(isolate, args[0]);
  if (! TRI_IsArrayJson(json)) {
    if (json != nullptr) {
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
    }
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
  }

  triagens::rest::HttpResponse::HttpResponseCode responseCode;
  map<string, string> headers;
  map<string, string> resultHeaders;
  string resultBody;

  int error = triagens::arango::createDocumentOnCoordinator(
            dbname, collname, options.waitForSync, json, headers,
            responseCode, resultHeaders, resultBody);
  // Note that the json has been freed inside!

  if (error != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(error);
  }
  // report what the DBserver told us: this could now be 201/202 or
  // 400/404
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, resultBody.c_str());
  if (responseCode >= triagens::rest::HttpResponse::BAD) {
    if (! TRI_IsArrayJson(json)) {
      if (json != nullptr) {
        TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
      }
      TRI_V8_THROW_EXCEPTION(TRI_ERROR_INTERNAL);
    }
    int errorNum = 0;
    TRI_json_t* subjson = TRI_LookupArrayJson(json, "errorNum");
    if (nullptr != subjson && TRI_IsNumberJson(subjson)) {
      errorNum = static_cast<int>(subjson->_value._number);
    }

    string errorMessage;
    subjson = TRI_LookupArrayJson(json, "errorMessage");
    if (nullptr != subjson && TRI_IsStringJson(subjson)) {
      errorMessage = string(subjson->_value._string.data,
                            subjson->_value._string.length-1);
    }
    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
    TRI_V8_THROW_EXCEPTION_MESSAGE(errorNum, errorMessage);
  }

  if (options.silent) {
    if (json != nullptr) {
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
    }
    TRI_V8_RETURN_TRUE();
  }

  v8::Handle<v8::Value> ret = TRI_ObjectJson(isolate, json);
  if (json != nullptr) {
    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
  }
  TRI_V8_RETURN(ret);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extract a key from a v8 object
////////////////////////////////////////////////////////////////////////////////

static string GetId (const v8::FunctionCallbackInfo<v8::Value>& args, int which) {
  v8::Isolate* isolate = args.GetIsolate();

  if ((args.Length() >= which)) { //// todo && ! args[which].IsArray()) {
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
/// @brief saves a new edge document
/// @startDocuBlock SaveEdgeCol
/// `edge-collection.save(from, to, document)`
///
/// Saves a new edge and returns the document-handle. *from* and *to*
/// must be documents or document references.
///
/// `edge-collection.save(from, to, document, waitForSync)`
///
/// The optional *waitForSync* parameter can be used to force
/// synchronization of the document creation operation to disk even in case
/// that the *waitForSync* flag had been disabled for the entire collection.
/// Thus, the *waitForSync* parameter can be used to force synchronization
/// of just specific operations. To use this, set the *waitForSync* parameter
/// to *true*. If the *waitForSync* parameter is not specified or set to
/// *false*, then the collection's default *waitForSync* behavior is
/// applied. The *waitForSync* parameter cannot be used to disable
/// synchronization for collections that have a default *waitForSync* value
/// of *true*.
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_OUTPUT{SaveEdgeCol}
/// ~ db._create("vertex");
///   v1 = db.vertex.save({ name : "vertex 1" });
///   v2 = db.vertex.save({ name : "vertex 2" });
///   e1 = db.relation.save(v1, v2, { label : "knows" });
///   db._document(e1);
/// ~ db._drop("vertex");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

static void InsertEdgeCol (TRI_vocbase_col_t* col,
                                            const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  TRI_GET_GLOBALS();

  uint32_t const argLength = args.Length();
  if (argLength < 3 || argLength > 4) {
    TRI_V8_THROW_EXCEPTION_USAGE("save(<from>, <to>, <data>, [<waitForSync>])");
  }

  InsertOptions options;

  // set document key
  std::unique_ptr<char[]> key;
  int res;

  if (args[2]->IsObject() && ! args[2]->IsArray()) {
    res = ExtractDocumentKey(isolate, v8g, args[2]->ToObject(), key);

    if (res != TRI_ERROR_NO_ERROR && res != TRI_ERROR_ARANGO_DOCUMENT_KEY_MISSING) {
      TRI_V8_THROW_EXCEPTION(res);
    }
  }
  else {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
  }

  if (argLength > 3 && args[3]->IsObject()) {
    v8::Handle<v8::Object> optionsObject = args[3].As<v8::Object>();
    TRI_GET_GLOBAL_STRING(WaitForSyncKey);
    if (optionsObject->Has(WaitForSyncKey)) {
      options.waitForSync = TRI_ObjectToBoolean(optionsObject->Get(WaitForSyncKey));
    }
    TRI_GET_GLOBAL_STRING(SilentKey);
    if (optionsObject->Has(SilentKey)) {
      options.silent = TRI_ObjectToBoolean(optionsObject->Get(SilentKey));
    }
  }
  else {
    options.waitForSync = ExtractWaitForSync(args, 4);
  }

  std::unique_ptr<char[]> fromKey;
  std::unique_ptr<char[]> toKey;

  TRI_document_edge_t edge;
  // the following values are defaults that will be overridden below
  edge._fromCid = 0;
  edge._toCid   = 0;
  edge._fromKey = nullptr;
  edge._toKey   = nullptr;

  SingleCollectionWriteTransaction<1> trx(new V8TransactionContext(true), col->_vocbase, col->_cid);

  // extract from
  res = TRI_ParseVertex(args, trx.resolver(), edge._fromCid, fromKey, args[0]);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }
  edge._fromKey = fromKey.get();

  // extract to
  res = TRI_ParseVertex(args, trx.resolver(), edge._toCid, toKey, args[1]);

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
  TRI_memory_zone_t* zone = document->getShaper()->_memoryZone;  // PROTECTED by trx from above
  
  // fetch a barrier so nobody unlinks datafiles with the shapes & attributes we might
  // need for this document
  if (trx.orderBarrier(trx.trxCollection()) == nullptr) {
    TRI_V8_THROW_EXCEPTION_MEMORY();
  }

  // extract shaped data
  TRI_shaped_json_t* shaped = TRI_ShapedJsonV8Object(isolate, args[2], document->getShaper(), true);  // PROTECTED by trx here

  if (shaped == nullptr) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_errno(), "<data> cannot be converted into JSON shape");
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
  }
  else {
    char const* docKey = TRI_EXTRACT_MARKER_KEY(&mptr);  // PROTECTED by trx here

    v8::Handle<v8::Object> result = v8::Object::New(isolate);
    TRI_GET_GLOBAL_STRING(_IdKey);
    TRI_GET_GLOBAL_STRING(_RevKey);
    TRI_GET_GLOBAL_STRING(_KeyKey);
    result->Set(_IdKey,  V8DocumentId(isolate, trx.resolver()->getCollectionName(col->_cid), docKey));
    result->Set(_RevKey, V8RevisionId(isolate, mptr._rid));
    result->Set(_KeyKey, TRI_V8_STRING(docKey));

    TRI_V8_RETURN(result); 
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief saves an edge, coordinator case in a cluster
////////////////////////////////////////////////////////////////////////////////

static void InsertEdgeColCoordinator (TRI_vocbase_col_t* collection,
                                                       const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  // First get the initial data:
  string const dbname(collection->_dbName);

  // TODO: someone might rename the collection while we're reading its name...
  string const collname(collection->_name);

  uint32_t const argLength = args.Length();
  if (argLength < 3 || argLength > 4) {
    TRI_V8_THROW_EXCEPTION_USAGE("insert(<from>, <to>, <data>, [<waitForSync>])");
  }

  string _from = GetId(args, 0);
  string _to   = GetId(args, 1);

  TRI_json_t* json = TRI_ObjectToJson(isolate, args[2]);

  if (! TRI_IsArrayJson(json)) {
    if (json != nullptr) {
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
    }
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
  }

  InsertOptions options;
  if (argLength > 3 && args[3]->IsObject()) {
    TRI_GET_GLOBALS();
    v8::Handle<v8::Object> optionsObject = args[3].As<v8::Object>();
    TRI_GET_GLOBAL_STRING(WaitForSyncKey);
    if (optionsObject->Has(WaitForSyncKey)) {
      options.waitForSync = TRI_ObjectToBoolean(optionsObject->Get(WaitForSyncKey));
    }
    TRI_GET_GLOBAL_STRING(SilentKey);
    if (optionsObject->Has(SilentKey)) {
      options.silent = TRI_ObjectToBoolean(optionsObject->Get(SilentKey));
    }
  }
  else {
    options.waitForSync = ExtractWaitForSync(args, 4);
  }

  triagens::rest::HttpResponse::HttpResponseCode responseCode;
  map<string, string> resultHeaders;
  string resultBody;

  int error = triagens::arango::createEdgeOnCoordinator(
            dbname, collname, options.waitForSync, json, _from.c_str(), _to.c_str(),
            responseCode, resultHeaders, resultBody);
  // Note that the json has been freed inside!

  if (error != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(error);
  }
  // report what the DBserver told us: this could now be 201/202 or
  // 400/404
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, resultBody.c_str());
  if (responseCode >= triagens::rest::HttpResponse::BAD) {
    if (! TRI_IsArrayJson(json)) {
      if (nullptr != json) {
        TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
      }
      TRI_V8_THROW_EXCEPTION(TRI_ERROR_INTERNAL);
    }
    int errorNum = 0;
    TRI_json_t* subjson = TRI_LookupArrayJson(json, "errorNum");
    if (nullptr != subjson && TRI_IsNumberJson(subjson)) {
      errorNum = static_cast<int>(subjson->_value._number);
    }
    string errorMessage;
    subjson = TRI_LookupArrayJson(json, "errorMessage");
    if (nullptr != subjson && TRI_IsStringJson(subjson)) {
      errorMessage = string(subjson->_value._string.data,
                            subjson->_value._string.length-1);
    }
    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
    TRI_V8_THROW_EXCEPTION_MESSAGE(errorNum, errorMessage);
  }
  v8::Handle<v8::Value> ret = TRI_ObjectJson(isolate, json);
  if (nullptr != json) {
    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
  }
  TRI_V8_RETURN(ret);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief saves a new document
/// @startDocuBlock documentsCollectionSave
/// `collection.save(data)`
///
/// Creates a new document in the *collection* from the given *data*. The
/// *data* must be a hash array. It must not contain attributes starting
/// with *_*.
///
/// The method returns a document with the attributes *_id* and *_rev*.
/// The attribute *_id* contains the document handle of the newly created
/// document, the attribute *_rev* contains the document revision.
///
/// `collection.save(data, waitForSync)`
///
/// Creates a new document in the *collection* from the given *data* as
/// above. The optional *waitForSync* parameter can be used to force
/// synchronization of the document creation operation to disk even in case
/// that the *waitForSync* flag had been disabled for the entire collection.
/// Thus, the *waitForSync* parameter can be used to force synchronization
/// of just specific operations. To use this, set the *waitForSync* parameter
/// to *true*. If the *waitForSync* parameter is not specified or set to
/// *false*, then the collection's default *waitForSync* behavior is
/// applied. The *waitForSync* parameter cannot be used to disable
/// synchronization for collections that have a default *waitForSync* value
/// of *true*.
///
/// Note: since ArangoDB 2.2, *insert* is an alias for *save*.
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_OUTPUT{documentsCollectionSave}
/// ~ db._create("example");
///   db.example.save({ Hello : "World" });
///   db.example.save({ Hello : "World" }, true);
/// ~ db._drop("example");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

static void JS_InsertVocbaseCol (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  TRI_vocbase_col_t* collection = TRI_UnwrapClass<TRI_vocbase_col_t>(args.Holder(), WRP_VOCBASE_COL_TYPE);

  if (collection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  if (ServerState::instance()->isCoordinator()) {
    // coordinator case
    if ((TRI_col_type_e) collection->_type == TRI_COL_TYPE_DOCUMENT) {
      InsertVocbaseColCoordinator(collection, args);
      return;
    }

    InsertEdgeColCoordinator(collection, args);
    return;
  }
    
  // single server case
  if ((TRI_col_type_e) collection->_type == TRI_COL_TYPE_DOCUMENT) {
    InsertVocbaseCol(collection, args);
  }
  else {
    InsertEdgeCol(collection, args);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the status of a collection
////////////////////////////////////////////////////////////////////////////////

static void JS_StatusVocbaseCol (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  TRI_vocbase_col_t* collection = TRI_UnwrapClass<TRI_vocbase_col_t>(args.Holder(), WRP_VOCBASE_COL_TYPE);

  if (collection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  if (ServerState::instance()->isCoordinator()) {
    std::string const databaseName = std::string(collection->_dbName);

    shared_ptr<CollectionInfo> const& ci
        = ClusterInfo::instance()->getCollection(databaseName,
                                        StringUtils::itoa(collection->_cid));

    if ((*ci).empty()) {
      TRI_V8_RETURN(v8::Number::New(isolate, (int) TRI_VOC_COL_STATUS_DELETED));
    }
    TRI_V8_RETURN(v8::Number::New(isolate, (int) ci->status()));
  }
  // fallthru intentional

  TRI_READ_LOCK_STATUS_VOCBASE_COL(collection);
  TRI_vocbase_col_status_e status = collection->_status;
  TRI_READ_UNLOCK_STATUS_VOCBASE_COL(collection);

  TRI_V8_RETURN(v8::Number::New(isolate, (int) status));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief truncates a collection
////////////////////////////////////////////////////////////////////////////////

static void JS_TruncateVocbaseCol (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  bool const forceSync = ExtractWaitForSync(args, 1);

  TRI_vocbase_col_t* collection = TRI_UnwrapClass<TRI_vocbase_col_t>(args.Holder(), WRP_VOCBASE_COL_TYPE);

  if (collection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  TRI_THROW_SHARDING_COLLECTION_NOT_YET_IMPLEMENTED(collection);

  SingleCollectionWriteTransaction<UINT64_MAX> trx(new V8TransactionContext(true), collection->_vocbase, collection->_cid);
  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  if (trx.orderBarrier(trx.trxCollection()) == nullptr) {
    TRI_V8_THROW_EXCEPTION_MEMORY();
  }

  res = trx.truncate(forceSync);
  res = trx.finish(res);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  TRI_V8_RETURN_UNDEFINED();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief truncates a datafile
////////////////////////////////////////////////////////////////////////////////

static void JS_TruncateDatafileVocbaseCol (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  TRI_vocbase_col_t* collection = TRI_UnwrapClass<TRI_vocbase_col_t>(args.Holder(), WRP_VOCBASE_COL_TYPE);

  if (collection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  TRI_THROW_SHARDING_COLLECTION_NOT_YET_IMPLEMENTED(collection);

  if (args.Length() != 2) {
    TRI_V8_THROW_EXCEPTION_USAGE("truncateDatafile(<datafile>, <size>)");
  }

  string path = TRI_ObjectToString(args[0]);
  size_t size = (size_t) TRI_ObjectToInt64(args[1]);

  TRI_READ_LOCK_STATUS_VOCBASE_COL(collection);

  if (collection->_status != TRI_VOC_COL_STATUS_UNLOADED &&
      collection->_status != TRI_VOC_COL_STATUS_CORRUPTED) {
    TRI_READ_UNLOCK_STATUS_VOCBASE_COL(collection);
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_COLLECTION_NOT_UNLOADED);
  }

  int res = TRI_TruncateDatafile(path.c_str(), (TRI_voc_size_t) size);

  TRI_READ_UNLOCK_STATUS_VOCBASE_COL(collection);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(res, "cannot truncate datafile");
  }

  TRI_V8_RETURN_UNDEFINED();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the type of a collection
/// @startDocuBlock collectionType
/// `collection.type()`
///
/// Returns the type of a collection. Possible values are:
/// - 2: document collection
/// - 3: edge collection
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

static void JS_TypeVocbaseCol (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  TRI_vocbase_col_t* collection = TRI_UnwrapClass<TRI_vocbase_col_t>(args.Holder(), WRP_VOCBASE_COL_TYPE);

  if (collection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  if (ServerState::instance()->isCoordinator()) {
    std::string const databaseName = std::string(collection->_dbName);

    shared_ptr<CollectionInfo> const& ci
        = ClusterInfo::instance()->getCollection(databaseName,
                                      StringUtils::itoa(collection->_cid));

    if ((*ci).empty()) {
      TRI_V8_RETURN(v8::Number::New(isolate, (int) collection->_type));
    }
    else {
      TRI_V8_RETURN(v8::Number::New(isolate, (int) ci->type()));
    }
  }
  // fallthru intentional

  TRI_READ_LOCK_STATUS_VOCBASE_COL(collection);
  TRI_col_type_e type = (TRI_col_type_e) collection->_type;
  TRI_READ_UNLOCK_STATUS_VOCBASE_COL(collection);

  TRI_V8_RETURN(v8::Number::New(isolate, (int) type));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unloads a collection
/// @startDocuBlock collectionUnload
/// `collection.unload()`
///
/// Starts unloading a collection from memory. Note that unloading is deferred
/// until all query have finished.
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_OUTPUT{CollectionUnload}
/// ~ db._create("example");
///   col = db.example;
///   col.unload();
///   col;
/// ~ db._drop("example");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

static void JS_UnloadVocbaseCol (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  TRI_vocbase_col_t* collection = TRI_UnwrapClass<TRI_vocbase_col_t>(args.Holder(), WRP_VOCBASE_COL_TYPE);

  if (collection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  int res;

  if (ServerState::instance()->isCoordinator()) {
    string const databaseName(collection->_dbName);

    res = ClusterInfo::instance()->setCollectionStatusCoordinator(databaseName, StringUtils::itoa(collection->_cid), TRI_VOC_COL_STATUS_UNLOADED);
  }
  else {
    res = TRI_UnloadCollectionVocBase(collection->_vocbase, collection, false);
  }

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  TRI_V8_RETURN_UNDEFINED();
}


////////////////////////////////////////////////////////////////////////////////
/// @brief returns the version of a collection
////////////////////////////////////////////////////////////////////////////////

static void JS_VersionVocbaseCol (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  TRI_vocbase_col_t* collection = TRI_UnwrapClass<TRI_vocbase_col_t>(args.Holder(), WRP_VOCBASE_COL_TYPE);

  if (collection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  if (ServerState::instance()->isCoordinator()) {
    TRI_V8_RETURN(v8::Number::New(isolate, (int) TRI_COL_VERSION));
  }
  // fallthru intentional

  TRI_col_info_t info;

  TRI_READ_LOCK_STATUS_VOCBASE_COL(collection);
  int res = TRI_LoadCollectionInfo(collection->_path, &info, false);
  TRI_READ_UNLOCK_STATUS_VOCBASE_COL(collection);

  TRI_FreeCollectionInfoOptions(&info);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(res, "cannot fetch collection info");
  }

  TRI_V8_RETURN(v8::Number::New(isolate, (int) info._version));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks all data pointers in a collection
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_MAINTAINER_MODE
static void JS_CheckPointersVocbaseCol (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  TRI_vocbase_col_t* collection = TRI_UnwrapClass<TRI_vocbase_col_t>(args.Holder(), WRP_VOCBASE_COL_TYPE);

  if (collection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  TRI_THROW_SHARDING_COLLECTION_NOT_YET_IMPLEMENTED(collection);

  SingleCollectionReadOnlyTransaction trx(new V8TransactionContext(true), collection->_vocbase, collection->_cid);

  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  TRI_document_collection_t* document = trx.documentCollection();

  // iterate over the primary index and de-reference all the pointers to data
  void** ptr = document->_primaryIndex._table;
  void** end = ptr + document->_primaryIndex._nrAlloc;

  for (;  ptr < end;  ++ptr) {
    if (*ptr) {
      char const* key = TRI_EXTRACT_MARKER_KEY((TRI_doc_mptr_t const*) *ptr);

      TRI_ASSERT(key != nullptr);
      // dereference the key
      if (*key == '\0') {
        TRI_V8_THROW_EXCEPTION(TRI_ERROR_INTERNAL);
      }
    }
  }

  TRI_V8_RETURN_TRUE();
}
#endif



////////////////////////////////////////////////////////////////////////////////
/// @brief changes the operation mode of the server
/// @startDocuBock TODO
/// `db._changeMode(<mode>)`
///
/// Sets the server to the given mode.
/// Possible values for mode are:
/// - Normal
/// - NoCreate
///
/// `db._changeMode(<mode>)`
///
/// *Examples*
///
/// db._changeMode("Normal") every user can do all CRUD operations
/// db._changeMode("NoCreate") the user cannot create databases, indexes,
///                            and collections, and cannot carry out any
///                            data-modifying operations but dropping databases,
///                            indexes and collections.
////////////////////////////////////////////////////////////////////////////////

static void JS_ChangeOperationModeVocbase (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  TRI_GET_GLOBALS();

  bool allowModeChange = false;
  TRI_GET_GLOBAL(_currentRequest, v8::Value);
  if (_currentRequest.IsEmpty()|| _currentRequest->IsUndefined()) {
    // console mode
    allowModeChange = true;
  }
  else if (_currentRequest->IsObject()) {
    v8::Handle<v8::Object> obj = v8::Handle<v8::Object>::Cast(_currentRequest);

    TRI_GET_GLOBAL_STRING(PortTypeKey);
    if (obj->Has(PortTypeKey)) {
      string const portType = TRI_ObjectToString(obj->Get(PortTypeKey));
      if (portType == "unix") {
        allowModeChange = true;
      }
    }
  }

  if (! allowModeChange) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_FORBIDDEN);
  }

  // expecting one argument
  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("_changeMode(<mode>), with modes: 'Normal', 'NoCreate'");
  }

  string const newModeStr = TRI_ObjectToString(args[0]);

  TRI_vocbase_operationmode_e newMode = TRI_VOCBASE_MODE_NORMAL;

  if (newModeStr == "NoCreate") {
    newMode = TRI_VOCBASE_MODE_NO_CREATE;
  }
  else if (newModeStr != "Normal") {
    TRI_V8_THROW_EXCEPTION_USAGE("illegal mode, allowed modes are: 'Normal' and 'NoCreate'");
  }

  TRI_ChangeOperationModeServer(newMode);

  TRI_V8_RETURN_TRUE();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief retrieves a collection from a V8 argument
////////////////////////////////////////////////////////////////////////////////

static TRI_vocbase_col_t* GetCollectionFromArgument (TRI_vocbase_t* vocbase,
                                                     v8::Handle<v8::Value> const val) {
  // number
  if (val->IsNumber() || val->IsNumberObject()) {
    uint64_t cid = (uint64_t) TRI_ObjectToUInt64(val, true);

    return TRI_LookupCollectionByIdVocBase(vocbase, cid);
  }

  string const name = TRI_ObjectToString(val);
  return TRI_LookupCollectionByNameVocBase(vocbase, name.c_str());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a single collection or null
/// @startDocuBlock collectionDatabaseName
/// `db._collection(collection-name)`
///
/// Returns the collection with the given name or null if no such collection
/// exists.
///
/// `db._collection(collection-identifier)`
///
/// Returns the collection with the given identifier or null if no such
/// collection exists. Accessing collections by identifier is discouraged for
/// end users. End users should access collections using the collection name.
///
/// @EXAMPLES
///
/// Get a collection by name:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{collectionDatabaseName}
///   db._collection("demo");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// Get a collection by id:
///
/// ```
/// arangosh> db._collection(123456);
/// [ArangoCollection 123456, "demo" (type document, status loaded)]
/// ```
///
/// Unknown collection:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{collectionDatabaseNameUnknown}
///   db._collection("unknown");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

static void JS_CollectionVocbase (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  TRI_vocbase_t* vocbase = GetContextVocBase(isolate);

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  // expecting one argument
  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("_collection(<name>|<identifier>)");
  }

  v8::Handle<v8::Value> val = args[0];
  TRI_vocbase_col_t const* collection = 0;

  if (ServerState::instance()->isCoordinator()) {
    string const name = TRI_ObjectToString(val);
    shared_ptr<CollectionInfo> const& ci
        = ClusterInfo::instance()->getCollection(vocbase->_name, name);

    if ((*ci).id() == 0 || (*ci).empty()) {
      // not found
      TRI_V8_RETURN_NULL();
    }

    collection = CoordinatorCollection(vocbase, *ci);
  }
  else {
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
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns all collections
/// @startDocuBlock collectionDatabaseNameAll
/// `db._collections()`
///
/// Returns all collections of the given database.
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_OUTPUT{collectionsDatabaseName}
/// ~ db._create("example");
///   db._collections();
/// ~ db._drop("example");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

static void JS_CollectionsVocbase (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  TRI_vocbase_t* vocbase = GetContextVocBase(isolate);

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  TRI_vector_pointer_t colls;

  // if we are a coordinator, we need to fetch the collection info from the agency
  if (ServerState::instance()->isCoordinator()) {
    colls = GetCollectionsCluster(vocbase);
  }
  else {
    colls = TRI_CollectionsVocBase(vocbase);
  }

  bool error = false;
  // already create an array of the correct size
  v8::Handle<v8::Array> result = v8::Array::New(isolate);

  uint32_t n = (uint32_t) colls._length;
  for (uint32_t i = 0;  i < n;  ++i) {
    TRI_vocbase_col_t const* collection = (TRI_vocbase_col_t const*) colls._buffer[i];

    v8::Handle<v8::Value> c = WrapCollection(isolate, collection);

    if (c.IsEmpty()) {
      error = true;
      break;
    }

    result->Set(i, c);
  }

  TRI_DestroyVectorPointer(&colls);

  if (error) {
    TRI_V8_THROW_EXCEPTION_MEMORY();
  }

  TRI_V8_RETURN(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns all collection names
////////////////////////////////////////////////////////////////////////////////

static void JS_CompletionsVocbase (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  TRI_vocbase_t* vocbase = GetContextVocBase(isolate);

  if (vocbase == nullptr) {
    TRI_V8_RETURN(v8::Array::New(isolate));
  }

  TRI_vector_string_t names;
  if (ServerState::instance()->isCoordinator()) {
    if (ClusterInfo::instance()->doesDatabaseExist(vocbase->_name)) {
      names = GetCollectionNamesCluster(vocbase);
    }
    else {
      TRI_InitVectorString(&names, TRI_UNKNOWN_MEM_ZONE);
    }
  }
  else {
    names = TRI_CollectionNamesVocBase(vocbase);
  }

  size_t n = names._length;
  uint32_t j = 0;

  v8::Handle<v8::Array> result = v8::Array::New(isolate);
  // add collection names
  for (size_t i = 0;  i < n;  ++i) {
    char const* name = TRI_AtVectorString(&names, i);

    if (name != nullptr) {
      result->Set(j++, TRI_V8_STRING(name));
    }
  }

  TRI_DestroyVectorString(&names);

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
}

// -----------------------------------------------------------------------------
// --SECTION--                                              javascript functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief removes a document
/// @startDocuBlock documentsDocumentRemove
/// `db._remove(document)`
///
/// Removes a document. If there is revision mismatch, then an error is thrown.
///
/// `db._remove(document, true)`
///
/// Removes a document. If there is revision mismatch, then mismatch is ignored
/// and document is deleted. The function returns *true* if the document
/// existed and was deleted. It returns *false*, if the document was already
/// deleted.
///
/// `db._remove(document, true, waitForSync)` or
/// `db._remove(document, {overwrite: true or false, waitForSync: true or false})`
///
/// The optional *waitForSync* parameter can be used to force synchronization
/// of the document deletion operation to disk even in case that the
/// *waitForSync* flag had been disabled for the entire collection.  Thus,
/// the *waitForSync* parameter can be used to force synchronization of just
/// specific operations. To use this, set the *waitForSync* parameter to
/// *true*. If the *waitForSync* parameter is not specified or set to
/// *false*, then the collection's default *waitForSync* behavior is
/// applied. The *waitForSync* parameter cannot be used to disable
/// synchronization for collections that have a default *waitForSync* value
/// of *true*.
///
/// `db._remove(document-handle, data)`
///
/// As before. Instead of document a *document-handle* can be passed as first
/// argument.
///
/// @EXAMPLES
///
/// Remove a document:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{documentsCollectionRemove}
/// ~ db._create("example");
///   a1 = db.example.save({ a : 1 });
///   db._remove(a1);
///   db._remove(a1);
///   db._remove(a1, true);
/// ~ db._drop("example");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// Remove a document with a conflict:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{documentsCollectionRemoveConflict}
/// ~ db._create("example");
///   a1 = db.example.save({ a : 1 });
///   a2 = db._replace(a1, { a : 2 });
///   db._remove(a1);
///   db._remove(a1, true);
///   db._document(a1);
/// ~ db._drop("example");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// Remove a document using new signature:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{documentsCollectionRemoveSignature}
/// ~ db._create("example");
///   db.example.save({ a:  1 } );
///   db.example.remove("example/11265325374", {overwrite: true, waitForSync: false})
/// ~ db._drop("example");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

static void JS_RemoveVocbase (const v8::FunctionCallbackInfo<v8::Value>& args) {
  return RemoveVocbaseCol(false, args);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a document and returns it
/// @startDocuBlock documentsDocumentName
/// `db._document(document)`
///
/// This method finds a document given its identifier. It returns the document
/// if the document exists. An error is thrown if no document with the given
/// identifier exists, or if the specified *_rev* value does not match the
/// current revision of the document.
///
/// **Note**: If the method is executed on the arangod server (e.g. from
/// inside a Foxx application), an immutable document object will be returned
/// for performance reasons. It is not possible to change attributes of this
/// immutable object. To update or patch the returned document, it needs to be
/// cloned/copied into a regular JavaScript object first. This is not necessary
/// if the *_document* method is called from out of arangosh or from any
/// other client.
///
/// `db._document(document-handle)`
///
/// As before. Instead of document a *document-handle* can be passed as
/// first argument.
///
/// @EXAMPLES
///
/// Returns the document:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{documentsDocumentName}
/// ~ db._create("example");
/// ~ var myid = db.example.save({_key: "12345"});
///   db._document("example/12345");
/// ~ db._drop("example");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

static void JS_DocumentVocbase (const v8::FunctionCallbackInfo<v8::Value>& args) {
  return DocumentVocbaseCol(false, args);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks whether a document exists
/// @startDocuBlock documentsDocumentExists
/// `db._exists(document)`
///
/// This method determines whether a document exists given its identifier.
/// Instead of returning the found document or an error, this method will
/// return either *true* or *false*. It can thus be used
/// for easy existence checks.
///
/// No error will be thrown if the sought document or collection does not
/// exist.
/// Still this method will throw an error if used improperly, e.g. when called
/// with a non-document handle.
///
/// `db._exists(document-handle)`
///
/// As before, but instead of a document a document-handle can be passed.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

static void JS_ExistsVocbase (const v8::FunctionCallbackInfo<v8::Value>& args) {
  return ExistsVocbaseCol(false, args);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief replaces a document
/// @startDocuBlock documentsDocumentReplace
/// `db._replace(document, data)`
///
/// The method returns a document with the attributes *_id*, *_rev* and
/// *_oldRev*.  The attribute *_id* contains the document handle of the
/// updated document, the attribute *_rev* contains the document revision of
/// the updated document, the attribute *_oldRev* contains the revision of
/// the old (now replaced) document.
///
/// If there is a conflict, i. e. if the revision of the *document* does not
/// match the revision in the collection, then an error is thrown.
///
/// `db._replace(document, data, true)`
///
/// As before, but in case of a conflict, the conflict is ignored and the old
/// document is overwritten.
///
/// `db._replace(document, data, true, waitForSync)`
///
/// The optional *waitForSync* parameter can be used to force
/// synchronization of the document replacement operation to disk even in case
/// that the *waitForSync* flag had been disabled for the entire collection.
/// Thus, the *waitForSync* parameter can be used to force synchronization
/// of just specific operations. To use this, set the *waitForSync* parameter
/// to *true*. If the *waitForSync* parameter is not specified or set to
/// *false*, then the collection's default *waitForSync* behavior is
/// applied. The *waitForSync* parameter cannot be used to disable
/// synchronization for collections that have a default *waitForSync* value
/// of *true*.
///
/// `db._replace(document-handle, data)`
///
/// As before. Instead of document a *document-handle* can be passed as
/// first argument.
///
/// @EXAMPLES
///
/// Create and replace a document:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{documentsDocumentReplace}
/// ~ db._create("example");
///   a1 = db.example.save({ a : 1 });
///   a2 = db._replace(a1, { a : 2 });
///   a3 = db._replace(a1, { a : 3 });
/// ~ db._drop("example");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

static void JS_ReplaceVocbase (const v8::FunctionCallbackInfo<v8::Value>& args) {
  return ReplaceVocbaseCol(false, args);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief update a document
/// @startDocuBlock documentsDocumentUpdate
/// `db._update(document, data, overwrite, keepNull, waitForSync)`
///
/// Updates an existing *document*. The *document* must be a document in
/// the current collection. This document is then patched with the
/// *data* given as second argument. The optional *overwrite* parameter can
/// be used to control the behavior in case of version conflicts (see below).
/// The optional *keepNull* parameter can be used to modify the behavior when
/// handling *null* values. Normally, *null* values are stored in the
/// database. By setting the *keepNull* parameter to *false*, this behavior
/// can be changed so that all attributes in *data* with *null* values will
/// be removed from the target document.
///
/// The optional *waitForSync* parameter can be used to force
/// synchronization of the document update operation to disk even in case
/// that the *waitForSync* flag had been disabled for the entire collection.
/// Thus, the *waitForSync* parameter can be used to force synchronization
/// of just specific operations. To use this, set the *waitForSync* parameter
/// to *true*. If the *waitForSync* parameter is not specified or set to
/// false*, then the collection's default *waitForSync* behavior is
/// applied. The *waitForSync* parameter cannot be used to disable
/// synchronization for collections that have a default *waitForSync* value
/// of *true*.
///
/// The method returns a document with the attributes *_id*, *_rev* and
/// *_oldRev*. The attribute *_id* contains the document handle of the
/// updated document, the attribute *_rev* contains the document revision of
/// the updated document, the attribute *_oldRev* contains the revision of
/// the old (now replaced) document.
///
/// If there is a conflict, i. e. if the revision of the *document* does not
/// match the revision in the collection, then an error is thrown.
///
/// `db._update(document, data, true)`
///
/// As before, but in case of a conflict, the conflict is ignored and the old
/// document is overwritten.
///
/// `db._update(document-handle, data)`
///
/// As before. Instead of document a *document-handle* can be passed as
/// first argument.
///
/// @EXAMPLES
///
/// Create and update a document:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{documentDocumentUpdate}
/// ~ db._create("example");
///   a1 = db.example.save({ a : 1 });
///   a2 = db._update(a1, { b : 2 });
///   a3 = db._update(a1, { c : 3 });
/// ~ db._drop("example");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

static void JS_UpdateVocbase (const v8::FunctionCallbackInfo<v8::Value>& args) {
  return UpdateVocbaseCol(false, args);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the server version string
/// @startDocuBlock databaseVersion
/// `db._version()`
///
/// Returns the server version string.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

static void JS_VersionServer (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  TRI_V8_RETURN(TRI_V8_ASCII_STRING(TRI_VERSION));
}

// -----------------------------------------------------------------------------
// --SECTION--                                              javascript functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief counts the number of documents in a result set
/// @startDocuBlock collectionCount
/// `collection.count()`
///
/// Returns the number of living documents in the collection.
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_OUTPUT{collectionCount}
/// ~ db._create("users");
///   db.users.count();
/// ~ db._drop("users");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

static void JS_CountVocbaseCol (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  TRI_vocbase_col_t* collection = TRI_UnwrapClass<TRI_vocbase_col_t>(args.Holder(), WRP_VOCBASE_COL_TYPE);

  if (collection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  if (args.Length() != 0) {
    TRI_V8_THROW_EXCEPTION_USAGE("count()");
  }

  if (ServerState::instance()->isCoordinator()) {
    // First get the initial data:
    string const dbname(collection->_dbName);

    // TODO: someone might rename the collection while we're reading its name...
    string const collname(collection->_name);

    uint64_t count = 0;
    int error = triagens::arango::countOnCoordinator(dbname, collname, count);

    if (error != TRI_ERROR_NO_ERROR) {
      TRI_V8_THROW_EXCEPTION(error);
    }

    TRI_V8_RETURN(v8::Number::New(isolate, (double) count));
  }

  SingleCollectionReadOnlyTransaction trx(new V8TransactionContext(true), collection->_vocbase, collection->_cid);

  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  TRI_document_collection_t* document = trx.documentCollection();

  // READ-LOCK start
  trx.lockRead();

  const TRI_voc_size_t s = document->size(document);

  trx.finish(res);
  // READ-LOCK end

  TRI_V8_RETURN(v8::Number::New(isolate, (double) s));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns information about the datafiles
/// `collection.datafiles()`
///
/// Returns information about the datafiles. The collection must be unloaded.
////////////////////////////////////////////////////////////////////////////////

static void JS_DatafilesVocbaseCol (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  TRI_vocbase_col_t* collection = TRI_UnwrapClass<TRI_vocbase_col_t>(args.Holder(), WRP_VOCBASE_COL_TYPE);

  if (collection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  TRI_THROW_SHARDING_COLLECTION_NOT_YET_IMPLEMENTED(collection);

  TRI_READ_LOCK_STATUS_VOCBASE_COL(collection);

  if (collection->_status != TRI_VOC_COL_STATUS_UNLOADED &&
      collection->_status != TRI_VOC_COL_STATUS_CORRUPTED) {
    TRI_READ_UNLOCK_STATUS_VOCBASE_COL(collection);
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_COLLECTION_NOT_UNLOADED);
  }

  TRI_col_file_structure_t structure = TRI_FileStructureCollectionDirectory(collection->_path);

  // release lock
  TRI_READ_UNLOCK_STATUS_VOCBASE_COL(collection);

  // build result
  v8::Handle<v8::Object> result = v8::Object::New(isolate);

  // journals
  v8::Handle<v8::Array> journals = v8::Array::New(isolate);
  result->Set(TRI_V8_ASCII_STRING("journals"), journals);

  for (size_t i = 0;  i < structure._journals._length;  ++i) {
    journals->Set((uint32_t) i, TRI_V8_STRING(structure._journals._buffer[i]));
  }

  // compactors
  v8::Handle<v8::Array> compactors = v8::Array::New(isolate);
  result->Set(TRI_V8_ASCII_STRING("compactors"), compactors);

  for (size_t i = 0;  i < structure._compactors._length;  ++i) {
    compactors->Set((uint32_t) i, TRI_V8_STRING(structure._compactors._buffer[i]));
  }

  // datafiles
  v8::Handle<v8::Array> datafiles = v8::Array::New(isolate);
  result->Set(TRI_V8_ASCII_STRING("datafiles"), datafiles);

  for (size_t i = 0;  i < structure._datafiles._length;  ++i) {
    datafiles->Set((uint32_t) i, TRI_V8_STRING(structure._datafiles._buffer[i]));
  }

  // free result
  TRI_DestroyFileStructureCollection(&structure);

  TRI_V8_RETURN(result);
}

// -----------------------------------------------------------------------------
// --SECTION--                                          TRI_DATAFILE_T FUNCTIONS
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                              javascript functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief returns information about the datafiles
///
/// @FUN{@FA{collection}.datafileScan(@FA{path})}
///
/// Returns information about the datafiles. The collection must be unloaded.
////////////////////////////////////////////////////////////////////////////////

static void JS_DatafileScanVocbaseCol (const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  TRI_vocbase_col_t* collection = TRI_UnwrapClass<TRI_vocbase_col_t>(args.Holder(), WRP_VOCBASE_COL_TYPE);

  if (collection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("datafileScan(<path>)");
  }

  string path = TRI_ObjectToString(args[0]);

  TRI_READ_LOCK_STATUS_VOCBASE_COL(collection);

  if (collection->_status != TRI_VOC_COL_STATUS_UNLOADED &&
      collection->_status != TRI_VOC_COL_STATUS_CORRUPTED) {
    TRI_READ_UNLOCK_STATUS_VOCBASE_COL(collection);
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_COLLECTION_NOT_UNLOADED);
  }

  TRI_df_scan_t scan = TRI_ScanDatafile(path.c_str());

  // build result
  v8::Handle<v8::Object> result = v8::Object::New(isolate);

  result->Set(TRI_V8_ASCII_STRING("currentSize"),   v8::Number::New(isolate, scan._currentSize));
  result->Set(TRI_V8_ASCII_STRING("maximalSize"),   v8::Number::New(isolate, scan._maximalSize));
  result->Set(TRI_V8_ASCII_STRING("endPosition"),   v8::Number::New(isolate, scan._endPosition));
  result->Set(TRI_V8_ASCII_STRING("numberMarkers"), v8::Number::New(isolate, scan._numberMarkers));
  result->Set(TRI_V8_ASCII_STRING("status"),        v8::Number::New(isolate, scan._status));
  result->Set(TRI_V8_ASCII_STRING("isSealed"),      v8::Boolean::New(isolate, scan._isSealed));

  v8::Handle<v8::Array> entries = v8::Array::New(isolate);
  result->Set(TRI_V8_ASCII_STRING("entries"), entries);

  for (size_t i = 0;  i < scan._entries._length;  ++i) {
    TRI_df_scan_entry_t* entry = (TRI_df_scan_entry_t*) TRI_AtVector(&scan._entries, i);

    v8::Handle<v8::Object> o = v8::Object::New(isolate);

    o->Set(TRI_V8_ASCII_STRING("position"), v8::Number::New(isolate, entry->_position));
    o->Set(TRI_V8_ASCII_STRING("size"),     v8::Number::New(isolate, entry->_size));
    o->Set(TRI_V8_ASCII_STRING("realSize"), v8::Number::New(isolate, entry->_realSize));
    o->Set(TRI_V8_ASCII_STRING("tick"),     V8TickId(isolate, entry->_tick));
    o->Set(TRI_V8_ASCII_STRING("type"),     v8::Number::New(isolate, (int) entry->_type));
    o->Set(TRI_V8_ASCII_STRING("status"),   v8::Number::New(isolate, (int) entry->_status));

    entries->Set((uint32_t) i, o);
  }

  TRI_DestroyDatafileScan(&scan);

  TRI_READ_UNLOCK_STATUS_VOCBASE_COL(collection);
  TRI_V8_RETURN(result);
}

// .............................................................................
// generate the TRI_vocbase_col_t template
// .............................................................................

void TRI_InitV8collection (v8::Handle<v8::Context> context,
                           TRI_server_t* server,
                           TRI_vocbase_t* vocbase,
                           JSLoader* loader,
                           const size_t threadNumber,
                           TRI_v8_global_t* v8g,
                           v8::Isolate* isolate,
                           v8::Handle<v8::ObjectTemplate>  ArangoDBNS){

  TRI_AddMethodVocbase(isolate, ArangoDBNS, TRI_V8_ASCII_STRING("_changeMode"), JS_ChangeOperationModeVocbase);
  TRI_AddMethodVocbase(isolate, ArangoDBNS, TRI_V8_ASCII_STRING("_collection"), JS_CollectionVocbase);
  TRI_AddMethodVocbase(isolate, ArangoDBNS, TRI_V8_ASCII_STRING("_collections"), JS_CollectionsVocbase);
  TRI_AddMethodVocbase(isolate, ArangoDBNS, TRI_V8_ASCII_STRING("_COMPLETIONS"), JS_CompletionsVocbase, true);
  TRI_AddMethodVocbase(isolate, ArangoDBNS, TRI_V8_ASCII_STRING("_document"), JS_DocumentVocbase);
  TRI_AddMethodVocbase(isolate, ArangoDBNS, TRI_V8_ASCII_STRING("_exists"), JS_ExistsVocbase);
  TRI_AddMethodVocbase(isolate, ArangoDBNS, TRI_V8_ASCII_STRING("_remove"), JS_RemoveVocbase);
  TRI_AddMethodVocbase(isolate, ArangoDBNS, TRI_V8_ASCII_STRING("_replace"), JS_ReplaceVocbase);
  TRI_AddMethodVocbase(isolate, ArangoDBNS, TRI_V8_ASCII_STRING("_update"), JS_UpdateVocbase);
  TRI_AddMethodVocbase(isolate, ArangoDBNS, TRI_V8_ASCII_STRING("_version"), JS_VersionServer);

  v8::Handle<v8::ObjectTemplate> rt;
  v8::Handle<v8::FunctionTemplate> ft;

  ft = v8::FunctionTemplate::New(isolate);
  ft->SetClassName(TRI_V8_ASCII_STRING("ArangoCollection"));

  rt = ft->InstanceTemplate();
  rt->SetInternalFieldCount(3);


#ifdef TRI_ENABLE_MAINTAINER_MODE
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("checkPointers"), JS_CheckPointersVocbaseCol);
#endif  
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("count"), JS_CountVocbaseCol);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("datafiles"), JS_DatafilesVocbaseCol);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("datafileScan"), JS_DatafileScanVocbaseCol);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("document"), JS_DocumentVocbaseCol);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("drop"), JS_DropVocbaseCol);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("exists"), JS_ExistsVocbaseCol);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("figures"), JS_FiguresVocbaseCol);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("insert"), JS_InsertVocbaseCol);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("load"), JS_LoadVocbaseCol);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("name"), JS_NameVocbaseCol);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("planId"), JS_PlanIdVocbaseCol);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("properties"), JS_PropertiesVocbaseCol);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("remove"), JS_RemoveVocbaseCol);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("revision"), JS_RevisionVocbaseCol);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("rename"), JS_RenameVocbaseCol);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("replace"), JS_ReplaceVocbaseCol);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("rotate"), JS_RotateVocbaseCol);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("save"), JS_InsertVocbaseCol); // note: save is now an alias for insert
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("status"), JS_StatusVocbaseCol);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("TRUNCATE"), JS_TruncateVocbaseCol, true);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("truncateDatafile"), JS_TruncateDatafileVocbaseCol);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("type"), JS_TypeVocbaseCol);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("unload"), JS_UnloadVocbaseCol);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("update"), JS_UpdateVocbaseCol);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("version"), JS_VersionVocbaseCol);

  TRI_InitV8indexCollection(isolate, rt);

  v8g->VocbaseColTempl.Reset(isolate, rt);
  TRI_AddGlobalFunctionVocbase(isolate, context, TRI_V8_ASCII_STRING("ArangoCollection"), ft->GetFunction());
}
