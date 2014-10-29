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

#include "Ahuacatl/ahuacatl-collections.h"
#include "Ahuacatl/ahuacatl-explain.h"

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
  bool waitForSync = false;
  bool silent      = false;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief internal struct which is used for reading the different option
/// parameters for the update and replace functions
////////////////////////////////////////////////////////////////////////////////

struct UpdateOptions {
  bool overwrite   = false;
  bool keepNull    = true;
  bool waitForSync = false;
  bool silent      = false;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief internal struct which is used for reading the different option
/// parameters for the remove function
////////////////////////////////////////////////////////////////////////////////

struct RemoveOptions {
  bool overwrite   = false;
  bool waitForSync = false;
};

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------


////////////////////////////////////////////////////////////////////////////////
/// @brief extract the forceSync flag from the arguments
/// must specify the argument index starting from 1
////////////////////////////////////////////////////////////////////////////////

static inline bool ExtractWaitForSync (v8::Arguments const& argv,
                                       int index) {
  TRI_ASSERT(index > 0);

  return (argv.Length() >= index && TRI_ObjectToBoolean(argv[index - 1]));
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

static inline v8::Handle<v8::Value> V8CollectionId (TRI_voc_cid_t cid) {
  char buffer[21];
  size_t len = TRI_StringUInt64InPlace((uint64_t) cid, (char*) &buffer);

  return v8::String::New((const char*) buffer, (int) len);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts a document key from a document
////////////////////////////////////////////////////////////////////////////////

static int ExtractDocumentKey (TRI_v8_global_t* v8g,
                               v8::Handle<v8::Object> const arg,
                               std::unique_ptr<char[]>& key) {
  TRI_ASSERT(v8g != nullptr);
  TRI_ASSERT(key.get() == nullptr);

  v8::Local<v8::Object> obj = arg->ToObject();

  if (obj->Has(v8g->_KeyKey)) {
    v8::Handle<v8::Value> v = obj->Get(v8g->_KeyKey);

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

static v8::Handle<v8::Value> ParseDocumentOrDocumentHandle (TRI_vocbase_t* vocbase,
                                                            CollectionNameResolver const* resolver,
                                                            TRI_vocbase_col_t const*& collection,
                                                            std::unique_ptr<char[]>& key,
                                                            TRI_voc_rid_t& rid,
                                                            v8::Handle<v8::Value> const val) {
  v8::HandleScope scope;

  TRI_ASSERT(key.get() == nullptr);

  // reset the collection identifier and the revision
  string collectionName;
  rid = 0;

  // try to extract the collection name, key, and revision from the object passed
  if (! ExtractDocumentHandle(val, collectionName, key, rid)) {
    return scope.Close(TRI_CreateErrorObject(__FILE__,
                                             __LINE__,
                                             TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD));
  }

  // we have at least a key, we also might have a collection name
  TRI_ASSERT(key.get() != nullptr);


  if (collectionName.empty()) {
    // only a document key without collection name was passed
    if (collection == nullptr) {
      // we do not know the collection
      return scope.Close(TRI_CreateErrorObject(__FILE__,
                                               __LINE__,
                                               TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD));
    }
    // we use the current collection's name
    collectionName = resolver->getCollectionName(collection->_cid);
  }
  else {
    // we read a collection name from the document id
    // check cross-collection requests
    if (collection != nullptr) {
      if (! EqualCollection(resolver, collectionName, collection)) {
        return scope.Close(TRI_CreateErrorObject(__FILE__,
                                                 __LINE__,
                                                 TRI_ERROR_ARANGO_CROSS_COLLECTION_REQUEST));
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
      return scope.Close(TRI_CreateErrorObject(__FILE__,
                                               __LINE__,
                                               TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND));
    }

    collection = col;
  }

  TRI_ASSERT(collection != nullptr);

  return scope.Close(v8::Handle<v8::Value>());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief cluster coordinator case, parse a key and possible revision
////////////////////////////////////////////////////////////////////////////////

static int ParseKeyAndRef (v8::Handle<v8::Value> const arg,
                           string& key,
                           TRI_voc_rid_t& rev) {
  rev = 0;
  if (arg->IsString()) {
    key = TRI_ObjectToString(arg);
  }
  else if (arg->IsObject()) {
    TRI_v8_global_t* v8g = static_cast<TRI_v8_global_t*>(v8::Isolate::GetCurrent()->GetData());
    v8::Handle<v8::Object> obj = v8::Handle<v8::Object>::Cast(arg);

    if (obj->Has(v8g->_KeyKey) && obj->Get(v8g->_KeyKey)->IsString()) {
      key = TRI_ObjectToString(obj->Get(v8g->_KeyKey));
    }
    else if (obj->Has(v8g->_IdKey) && obj->Get(v8g->_IdKey)->IsString()) {
      key = TRI_ObjectToString(obj->Get(v8g->_IdKey));
      // part after / will be taken below
    }
    else {
      return TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID;
    }
    if (obj->Has(v8g->_RevKey) && obj->Get(v8g->_RevKey)->IsString()) {
      rev = TRI_ObjectToUInt64(obj->Get(v8g->_RevKey), true);
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

static v8::Handle<v8::Value> DocumentVocbaseColCoordinator (TRI_vocbase_col_t const* collection,
                                                            v8::Arguments const& argv,
                                                            bool generateDocument) {
  v8::HandleScope scope;

  // First get the initial data:
  string const dbname(collection->_dbName);
  // TODO: someone might rename the collection while we're reading its name...
  string const collname(collection->_name);

  string key;
  TRI_voc_rid_t rev = 0;
  int error = ParseKeyAndRef(argv[0], key, rev);

  if (error != TRI_ERROR_NO_ERROR) {
    TRI_V8_EXCEPTION(scope, error);
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
    TRI_V8_EXCEPTION(scope, error);
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
        TRI_V8_EXCEPTION(scope, TRI_ERROR_INTERNAL);
      }
      else {
        return scope.Close(v8::False());
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
      TRI_V8_EXCEPTION_MESSAGE(scope, errorNum, errorMessage);
    }
    else {
      return scope.Close(v8::False());
    }
  }
  if (generateDocument) {
    v8::Handle<v8::Value> ret = TRI_ObjectJson(json);

    if (nullptr != json) {
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
    }
    return scope.Close(ret);
  }
  else {
    // Note that for this case we will never get a 304 "NOT_MODIFIED"
    if (json != nullptr) {
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
    }
    return scope.Close(v8::True());
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a document and returns it
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> DocumentVocbaseCol (bool useCollection,
                                                 v8::Arguments const& argv) {
  v8::HandleScope scope;

  // first and only argument should be a document idenfifier
  if (argv.Length() != 1) {
    TRI_V8_EXCEPTION_USAGE(scope, "document(<document-handle>)");
  }

  std::unique_ptr<char[]> key;
  TRI_voc_rid_t rid;
  TRI_vocbase_t* vocbase;
  TRI_vocbase_col_t const* col = nullptr;

  if (useCollection) {
    // called as db.collection.document()
    col = TRI_UnwrapClass<TRI_vocbase_col_t>(argv.Holder(), WRP_VOCBASE_COL_TYPE);

    if (col == nullptr) {
      TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract collection");
    }

    vocbase = col->_vocbase;
  }
  else {
    // called as db._document()
    vocbase = GetContextVocBase();
  }

  if (vocbase == nullptr) {
    TRI_V8_EXCEPTION(scope, TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  V8ResolverGuard resolver(vocbase);
  v8::Handle<v8::Value> err = ParseDocumentOrDocumentHandle(vocbase, resolver.getResolver(), col, key, rid, argv[0]);

  LocalCollectionGuard g(useCollection ? nullptr : const_cast<TRI_vocbase_col_t*>(col));

  if (key.get() == nullptr) {
    TRI_V8_EXCEPTION(scope, TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD);
  }

  if (! err.IsEmpty()) {
    return scope.Close(v8::ThrowException(err));
  }

  TRI_ASSERT(col != nullptr);
  TRI_ASSERT(key.get() != nullptr);

  if (ServerState::instance()->isCoordinator()) {
    return scope.Close(DocumentVocbaseColCoordinator(col, argv, true));
  }

  SingleCollectionReadOnlyTransaction trx(new V8TransactionContext(true), vocbase, col->_cid);

  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_EXCEPTION(scope, res);
  }

  if (trx.orderBarrier(trx.trxCollection()) == nullptr) {
    TRI_V8_EXCEPTION_MEMORY(scope);
  }

  v8::Handle<v8::Value> result;
  TRI_doc_mptr_copy_t document;
  res = trx.read(&document, key.get());
  res = trx.finish(res);

  TRI_ASSERT(trx.hasBarrier());

  if (res == TRI_ERROR_NO_ERROR) {
    result = TRI_WrapShapedJson<SingleCollectionReadOnlyTransaction>(trx, col->_cid, document.getDataPtr());
  }

  if (res != TRI_ERROR_NO_ERROR || document.getDataPtr() == nullptr) {  // PROTECTED by trx here
    if (res == TRI_ERROR_NO_ERROR) {
      res = TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND;
    }

    TRI_V8_EXCEPTION(scope, res);
  }

  if (rid != 0 && document._rid != rid) {
    TRI_V8_EXCEPTION_MESSAGE(scope, TRI_ERROR_ARANGO_CONFLICT, "revision not found");
  }

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief loads a collection for usage
////////////////////////////////////////////////////////////////////////////////

TRI_vocbase_col_t const* UseCollection (v8::Handle<v8::Object> collection,
                                        v8::Handle<v8::Object>* err) {

  int res = TRI_ERROR_INTERNAL;
  TRI_vocbase_col_t* col = TRI_UnwrapClass<TRI_vocbase_col_t>(collection, WRP_VOCBASE_COL_TYPE);

  if (col != nullptr) {
    if (! col->_isLocal) {
      *err = TRI_CreateErrorObject(__FILE__,
                                   __LINE__,
                                   TRI_ERROR_NOT_IMPLEMENTED);
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
  *err = TRI_CreateErrorObject(__FILE__, __LINE__, res, "cannot use/load collection", true);
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

static v8::Handle<v8::Value> ExistsVocbaseCol (bool useCollection,
                                               v8::Arguments const& argv) {
  v8::HandleScope scope;

  // first and only argument should be a document idenfifier
  if (argv.Length() != 1) {
    TRI_V8_EXCEPTION_USAGE(scope, "exists(<document-handle>)");
  }

  std::unique_ptr<char[]> key;
  TRI_voc_rid_t rid;
  TRI_vocbase_t* vocbase;
  TRI_vocbase_col_t const* col = nullptr;

  if (useCollection) {
    // called as db.collection.exists()
    col = TRI_UnwrapClass<TRI_vocbase_col_t>(argv.Holder(), WRP_VOCBASE_COL_TYPE);

    if (col == nullptr) {
      TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract collection");
    }

    vocbase = col->_vocbase;
  }
  else {
    // called as db._exists()
    vocbase = GetContextVocBase();
  }

  if (vocbase == nullptr) {
    TRI_V8_EXCEPTION(scope, TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  V8ResolverGuard resolver(vocbase);
  v8::Handle<v8::Value> err = ParseDocumentOrDocumentHandle(vocbase, resolver.getResolver(), col, key, rid, argv[0]);

  LocalCollectionGuard g(useCollection ? nullptr : const_cast<TRI_vocbase_col_t*>(col));

  if (key.get() == nullptr) {
    TRI_V8_EXCEPTION(scope, TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD);
  }

  if (! err.IsEmpty()) {
    // check if we got an error object in return
    if (err->IsObject()) {
      // yes
      v8::Handle<v8::Object> e = v8::Handle<v8::Object>::Cast(err);

      // get the error object's error code
      if (e->Has(v8::String::New("errorNum"))) {
        // if error code is "collection not found", we'll return false
        if ((int) TRI_ObjectToInt64(e->Get(v8::String::New("errorNum"))) == TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND) {
          return scope.Close(v8::False());
        }
      }
    }

    // for any other error that happens, we'll rethrow it
    return scope.Close(v8::ThrowException(err));
  }

  TRI_ASSERT(col != nullptr);
  TRI_ASSERT(key.get() != nullptr);

  if (ServerState::instance()->isCoordinator()) {
    return scope.Close(DocumentVocbaseColCoordinator(col, argv, false));
  }

  SingleCollectionReadOnlyTransaction trx(new V8TransactionContext(true), vocbase, col->_cid);

  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_EXCEPTION(scope, res);
  }

  if (trx.orderBarrier(trx.trxCollection()) == nullptr) {
    TRI_V8_EXCEPTION_MEMORY(scope);
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
    return scope.Close(v8::True());
  }
  else if (res == TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND) {
    return scope.Close(v8::False());
  }

  TRI_V8_EXCEPTION(scope, res);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief modifies a document, coordinator case in a cluster
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> ModifyVocbaseColCoordinator (
                                  TRI_vocbase_col_t const* collection,
                                  TRI_doc_update_policy_e policy,
                                  bool waitForSync,
                                  bool isPatch,
                                  bool keepNull, // only counts if isPatch==true
                                  bool silent,
                                  v8::Arguments const& argv) {
  v8::HandleScope scope;

  TRI_ASSERT(collection != nullptr);

  // First get the initial data:
  string const dbname(collection->_dbName);
  string const collname(collection->_name);

  string key;
  TRI_voc_rid_t rev = 0;
  int error = ParseKeyAndRef(argv[0], key, rev);

  if (error != TRI_ERROR_NO_ERROR) {
    TRI_V8_EXCEPTION(scope, error);
  }

  TRI_json_t* json = TRI_ObjectToJson(argv[1]);
  if (! TRI_IsArrayJson(json)) {
    if (json != nullptr) {
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
    }
    TRI_V8_EXCEPTION(scope, TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
  }

  triagens::rest::HttpResponse::HttpResponseCode responseCode;
  map<string, string> headers;
  map<string, string> resultHeaders;
  string resultBody;

  error = triagens::arango::modifyDocumentOnCoordinator(
        dbname, collname, key, rev, policy, waitForSync, isPatch,
        keepNull, json, headers, responseCode, resultHeaders, resultBody);
  // Note that the json has been freed inside!

  if (error != TRI_ERROR_NO_ERROR) {
    TRI_V8_EXCEPTION(scope, error);
  }

  // report what the DBserver told us: this could now be 201/202 or
  // 400/404
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, resultBody.c_str());
  if (responseCode >= triagens::rest::HttpResponse::BAD) {
    if (! TRI_IsArrayJson(json)) {
      if (nullptr != json) {
        TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
      }
      TRI_V8_EXCEPTION(scope, TRI_ERROR_INTERNAL);
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
    TRI_V8_EXCEPTION_MESSAGE(scope, errorNum, errorMessage);
  }
 
  if (silent) {
    if (json != nullptr) {
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
    }
    return scope.Close(v8::True());
  }
  else {
    v8::Handle<v8::Value> ret = TRI_ObjectJson(json);
    if (json != nullptr) {
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
    }
    return scope.Close(ret);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief replaces a document
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> ReplaceVocbaseCol (bool useCollection,
                                                v8::Arguments const& argv) {
  v8::HandleScope scope;
  UpdateOptions options;
  TRI_doc_update_policy_e policy = TRI_DOC_UPDATE_ERROR;

  // check the arguments
  uint32_t const argLength = argv.Length();
  TRI_v8_global_t* v8g = static_cast<TRI_v8_global_t*>(v8::Isolate::GetCurrent()->GetData());

  if (argLength < 2) {
    TRI_V8_EXCEPTION_USAGE(scope, "replace(<document>, <data>, {overwrite: booleanValue, waitForSync: booleanValue})");
  }

  // we're only accepting "real" object documents
  if (! argv[1]->IsObject() || argv[1]->IsArray()) {
    TRI_V8_EXCEPTION(scope, TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
  }

  if (argv.Length() > 2) {
    if (argv[2]->IsObject()) {
      v8::Handle<v8::Object> optionsObject = argv[2].As<v8::Object>();
      if (optionsObject->Has(v8g->OverwriteKey)) {
        options.overwrite = TRI_ObjectToBoolean(optionsObject->Get(v8g->OverwriteKey));
        policy = ExtractUpdatePolicy(options.overwrite);
      }
      if (optionsObject->Has(v8g->WaitForSyncKey)) {
        options.waitForSync = TRI_ObjectToBoolean(optionsObject->Get(v8g->WaitForSyncKey));
      }
      if (optionsObject->Has(v8g->SilentKey)) {
        options.silent = TRI_ObjectToBoolean(optionsObject->Get(v8g->SilentKey));
      }
    }
    else {// old variant replace(<document>, <data>, <overwrite>, <waitForSync>)
      options.overwrite = TRI_ObjectToBoolean(argv[2]);
      policy = ExtractUpdatePolicy(options.overwrite);
      if (argLength > 3) {
        options.waitForSync = TRI_ObjectToBoolean(argv[3]);
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
    col = TRI_UnwrapClass<TRI_vocbase_col_t>(argv.Holder(), WRP_VOCBASE_COL_TYPE);

    if (col == nullptr) {
      TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract collection");
    }

    vocbase = col->_vocbase;
  }
  else {
    // called as db._replace()
    vocbase = GetContextVocBase();
  }

  if (vocbase == nullptr) {
    TRI_V8_EXCEPTION(scope, TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  V8ResolverGuard resolver(vocbase);
  v8::Handle<v8::Value> err = ParseDocumentOrDocumentHandle(vocbase, resolver.getResolver(), col, key, rid, argv[0]);

  LocalCollectionGuard g(useCollection ? nullptr : const_cast<TRI_vocbase_col_t*>(col));

  if (key.get() == nullptr) {
    TRI_V8_EXCEPTION(scope, TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD);
  }

  if (! err.IsEmpty()) {
    return scope.Close(v8::ThrowException(err));
  }

  TRI_ASSERT(col != nullptr);
  TRI_ASSERT(key.get() != nullptr);

  if (ServerState::instance()->isCoordinator()) {
    return scope.Close(ModifyVocbaseColCoordinator(col,
                                                   policy,
                                                   options.waitForSync,
                                                   false,  // isPatch
                                                   true,   // keepNull, does not matter
                                                   options.silent,
                                                   argv));
  }

  SingleCollectionWriteTransaction<1> trx(new V8TransactionContext(true), vocbase, col->_cid);
  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_EXCEPTION(scope, res);
  }

  TRI_document_collection_t* document = trx.documentCollection();
  TRI_memory_zone_t* zone = document->getShaper()->_memoryZone;  // PROTECTED by trx here

  TRI_doc_mptr_copy_t mptr;

  if (trx.orderBarrier(trx.trxCollection()) == nullptr) {
    TRI_V8_EXCEPTION_MEMORY(scope);
  }

  // we must lock here, because below we are
  // - reading the old document in coordinator case
  // - creating a shape, which might trigger a write into the collection
  trx.lockWrite();

  if (ServerState::instance()->isDBserver()) {
    // compare attributes in shardKeys
    const string cidString = StringUtils::itoa(document->_info._planId);

    TRI_json_t* json = TRI_ObjectToJson(argv[1]);

    if (json == nullptr) {
      TRI_V8_EXCEPTION_MEMORY(scope);
    }

    res = trx.read(&mptr, key.get());

    if (res != TRI_ERROR_NO_ERROR || mptr.getDataPtr() == nullptr) {  // PROTECTED by trx here
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
      TRI_V8_EXCEPTION(scope, res);
    }

    TRI_shaped_json_t shaped;
    TRI_EXTRACT_SHAPED_JSON_MARKER(shaped, mptr.getDataPtr());  // PROTECTED by trx here
    TRI_json_t* old = TRI_JsonShapedJson(document->getShaper(), &shaped);  // PROTECTED by trx here

    if (old == nullptr) {
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
      TRI_V8_EXCEPTION_MEMORY(scope);
    }

    if (shardKeysChanged(col->_dbName, cidString, old, json, false)) {
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, old);
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
      TRI_V8_EXCEPTION(scope, TRI_ERROR_CLUSTER_MUST_NOT_CHANGE_SHARDING_ATTRIBUTES);
    }

    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, old);
  }

  TRI_shaped_json_t* shaped = TRI_ShapedJsonV8Object(argv[1], document->getShaper(), true);  // PROTECTED by trx here

  if (shaped == nullptr) {
    TRI_V8_EXCEPTION_MESSAGE(scope, TRI_errno(), "<data> cannot be converted into JSON shape");
  }

  res = trx.updateDocument(key.get(), &mptr, shaped, policy, options.waitForSync, rid, &actualRevision);

  res = trx.finish(res);

  TRI_FreeShapedJson(zone, shaped);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_EXCEPTION(scope, res);
  }

  TRI_ASSERT(mptr.getDataPtr() != nullptr);  // PROTECTED by trx here

  if (options.silent) {
    return scope.Close(v8::True());
  }
  else {
    char const* docKey = TRI_EXTRACT_MARKER_KEY(&mptr);  // PROTECTED by trx here

    v8::Handle<v8::Object> result = v8::Object::New();
    result->Set(v8g->_IdKey, V8DocumentId(trx.resolver()->getCollectionName(col->_cid), docKey));
    result->Set(v8g->_RevKey, V8RevisionId(mptr._rid));
    result->Set(v8g->_OldRevKey, V8RevisionId(actualRevision));
    result->Set(v8g->_KeyKey, v8::String::New(docKey));

    return scope.Close(result);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief inserts a document
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> InsertVocbaseCol (TRI_vocbase_col_t* col,
                                               v8::Arguments const& argv) {
  v8::HandleScope scope;

  uint32_t const argLength = argv.Length();
  TRI_v8_global_t* v8g = static_cast<TRI_v8_global_t*>(v8::Isolate::GetCurrent()->GetData());

  if (argLength < 1 || argLength > 2) {
    TRI_V8_EXCEPTION_USAGE(scope, "insert(<data>, [<waitForSync>])");
  }

  InsertOptions options;
  if (argLength > 1 && argv[1]->IsObject()) {
    v8::Handle<v8::Object> optionsObject = argv[1].As<v8::Object>();
    if (optionsObject->Has(v8g->WaitForSyncKey)) {
      options.waitForSync = TRI_ObjectToBoolean(optionsObject->Get(v8g->WaitForSyncKey));
    }
    if (optionsObject->Has(v8g->SilentKey)) {
      options.silent = TRI_ObjectToBoolean(optionsObject->Get(v8g->SilentKey));
    }
  }
  else {
    options.waitForSync = ExtractWaitForSync(argv, 2);
  }

  // set document key
  std::unique_ptr<char[]> key;
  int res;

  if (argv[0]->IsObject() && ! argv[0]->IsArray()) {
    res = ExtractDocumentKey(v8g, argv[0]->ToObject(), key);

    if (res != TRI_ERROR_NO_ERROR && res != TRI_ERROR_ARANGO_DOCUMENT_KEY_MISSING) {
      TRI_V8_EXCEPTION(scope, res);
    }
  }
  else {
    TRI_V8_EXCEPTION(scope, TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
  }

  SingleCollectionWriteTransaction<1> trx(new V8TransactionContext(true), col->_vocbase, col->_cid);

  res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_EXCEPTION(scope, res);
  }

  // fetch a barrier so nobody unlinks datafiles with the shapes & attributes we might
  // need for this document
  if (trx.orderBarrier(trx.trxCollection()) == nullptr) {
    TRI_V8_EXCEPTION_MEMORY(scope);
  }

  TRI_document_collection_t* document = trx.documentCollection();
  TRI_memory_zone_t* zone = document->getShaper()->_memoryZone;  // PROTECTED by trx from above

  TRI_shaped_json_t* shaped = TRI_ShapedJsonV8Object(argv[0], document->getShaper(), true);  // PROTECTED by trx from above

  if (shaped == nullptr) {
    TRI_V8_EXCEPTION_MESSAGE(scope, TRI_errno(), "<data> cannot be converted into JSON shape");
  }

  TRI_doc_mptr_copy_t mptr;
  res = trx.createDocument(key.get(), &mptr, shaped, options.waitForSync);

  res = trx.finish(res);

  TRI_FreeShapedJson(zone, shaped);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_EXCEPTION(scope, res);
  }

  TRI_ASSERT(mptr.getDataPtr() != nullptr);  // PROTECTED by trx here

  if (options.silent) {
    return scope.Close(v8::True());
  }
  else {
    char const* docKey = TRI_EXTRACT_MARKER_KEY(&mptr);  // PROTECTED by trx here

    v8::Handle<v8::Object> result = v8::Object::New();
    result->Set(v8g->_IdKey, V8DocumentId(trx.resolver()->getCollectionName(col->_cid), docKey));
    result->Set(v8g->_RevKey, V8RevisionId(mptr._rid));
    result->Set(v8g->_KeyKey, v8::String::New(docKey));

    return scope.Close(result);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief updates (patches) a document
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> UpdateVocbaseCol (bool useCollection,
                                               v8::Arguments const& argv) {
  v8::HandleScope scope;
  UpdateOptions options;
  TRI_doc_update_policy_e policy = TRI_DOC_UPDATE_ERROR;

  // check the arguments
  uint32_t const argLength = argv.Length();
  
  TRI_v8_global_t* v8g = static_cast<TRI_v8_global_t*>(v8::Isolate::GetCurrent()->GetData());

  if (argLength < 2 || argLength > 5) {
    TRI_V8_EXCEPTION_USAGE(scope, "update(<document>, <data>, {overwrite: booleanValue, keepNull: booleanValue, waitForSync: booleanValue})");
  }

  if (argLength > 2) {
    if (argv[2]->IsObject()) {
      v8::Handle<v8::Object> optionsObject = argv[2].As<v8::Object>();
      if (optionsObject->Has(v8g->OverwriteKey)) {
        options.overwrite = TRI_ObjectToBoolean(optionsObject->Get(v8g->OverwriteKey));
        policy = ExtractUpdatePolicy(options.overwrite);
      }
      if (optionsObject->Has(v8g->KeepNullKey)) {
        options.keepNull = TRI_ObjectToBoolean(optionsObject->Get(v8g->KeepNullKey));
      }
      if (optionsObject->Has(v8g->WaitForSyncKey)) {
        options.waitForSync = TRI_ObjectToBoolean(optionsObject->Get(v8g->WaitForSyncKey));
      }
      if (optionsObject->Has(v8g->SilentKey)) {
        options.silent = TRI_ObjectToBoolean(optionsObject->Get(v8g->SilentKey));
      }
    }
    else { // old variant update(<document>, <data>, <overwrite>, <keepNull>, <waitForSync>)
      options.overwrite = TRI_ObjectToBoolean(argv[2]);
      policy = ExtractUpdatePolicy(options.overwrite);
      if (argLength > 3) {
        options.keepNull = TRI_ObjectToBoolean(argv[3]);
      }
      if (argLength > 4) {
        options.waitForSync = TRI_ObjectToBoolean(argv[4]);
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
    col = TRI_UnwrapClass<TRI_vocbase_col_t>(argv.Holder(), WRP_VOCBASE_COL_TYPE);

    if (col == nullptr) {
      TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract collection");
    }

    vocbase = col->_vocbase;
  }
  else {
    // called as db._update()
    vocbase = GetContextVocBase();
  }

  if (vocbase == nullptr) {
    TRI_V8_EXCEPTION(scope, TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  V8ResolverGuard resolver(vocbase);
  v8::Handle<v8::Value> err = ParseDocumentOrDocumentHandle(vocbase, resolver.getResolver(), col, key, rid, argv[0]);

  LocalCollectionGuard g(useCollection ? nullptr : const_cast<TRI_vocbase_col_t*>(col));

  if (key.get() == nullptr) {
    TRI_V8_EXCEPTION(scope, TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD);
  }

  if (! err.IsEmpty()) {
    return scope.Close(v8::ThrowException(err));
  }

  TRI_ASSERT(col != nullptr);
  TRI_ASSERT(key.get() != nullptr);

  if (ServerState::instance()->isCoordinator()) {
    return scope.Close(ModifyVocbaseColCoordinator(col,
                                                   policy,
                                                   options.waitForSync,
                                                   true,  // isPatch
                                                   options.keepNull,
                                                   options.silent,
                                                   argv));
  }

  if (! argv[1]->IsObject() || argv[1]->IsArray()) {
    // we're only accepting "real" object documents
    TRI_V8_EXCEPTION(scope, TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
  }

  TRI_json_t* json = TRI_ObjectToJson(argv[1]);

  if (json == nullptr) {
    TRI_V8_EXCEPTION_MESSAGE(scope, TRI_errno(), "<data> is no valid JSON");
  }


  SingleCollectionWriteTransaction<1> trx(new V8TransactionContext(true), vocbase, col->_cid);
  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
    TRI_V8_EXCEPTION(scope, res);
  }

  // we must use a write-lock that spans both the initial read and the update.
  // otherwise the operation is not atomic
  trx.lockWrite();

  TRI_doc_mptr_copy_t mptr;
  res = trx.read(&mptr, key.get());

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
    TRI_V8_EXCEPTION(scope, res);
  }

  if (trx.orderBarrier(trx.trxCollection()) == nullptr) {
    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
    TRI_V8_EXCEPTION_MEMORY(scope);
  }


  TRI_document_collection_t* document = trx.documentCollection();
  TRI_memory_zone_t* zone = document->getShaper()->_memoryZone;  // PROTECTED by trx here

  TRI_shaped_json_t shaped;
  TRI_EXTRACT_SHAPED_JSON_MARKER(shaped, mptr.getDataPtr());  // PROTECTED by trx here
  TRI_json_t* old = TRI_JsonShapedJson(document->getShaper(), &shaped);  // PROTECTED by trx here

  if (old == nullptr) {
    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
    TRI_V8_EXCEPTION_MEMORY(scope);
  }

  if (ServerState::instance()->isDBserver()) {
    // compare attributes in shardKeys
    const string cidString = StringUtils::itoa(document->_info._planId);

    if (shardKeysChanged(col->_dbName, cidString, old, json, true)) {
      TRI_FreeJson(document->getShaper()->_memoryZone, old);  // PROTECTED by trx here
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);

      TRI_V8_EXCEPTION(scope, TRI_ERROR_CLUSTER_MUST_NOT_CHANGE_SHARDING_ATTRIBUTES);
    }
  }

  TRI_json_t* patchedJson = TRI_MergeJson(TRI_UNKNOWN_MEM_ZONE, old, json, ! options.keepNull);
  TRI_FreeJson(zone, old);
  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);

  if (patchedJson == nullptr) {
    TRI_V8_EXCEPTION_MEMORY(scope);
  }

  res = trx.updateDocument(key.get(), &mptr, patchedJson, policy, options.waitForSync, rid, &actualRevision);

  res = trx.finish(res);

  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, patchedJson);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_EXCEPTION(scope, res);
  }

  TRI_ASSERT(mptr.getDataPtr() != nullptr);  // PROTECTED by trx here

  if (options.silent) {
    return scope.Close(v8::True());
  }
  else {
    char const* docKey = TRI_EXTRACT_MARKER_KEY(&mptr);  // PROTECTED by trx here

    v8::Handle<v8::Object> result = v8::Object::New();
    result->Set(v8g->_IdKey, V8DocumentId(trx.resolver()->getCollectionName(col->_cid), docKey));
    result->Set(v8g->_RevKey, V8RevisionId(mptr._rid));
    result->Set(v8g->_OldRevKey, V8RevisionId(actualRevision));
    result->Set(v8g->_KeyKey, v8::String::New(docKey));

    return scope.Close(result);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes a document, coordinator case in a cluster
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> RemoveVocbaseColCoordinator (TRI_vocbase_col_t const* collection,
                                                          TRI_doc_update_policy_e policy,
                                                          bool waitForSync,
                                                          v8::Arguments const& argv) {
  v8::HandleScope scope;

  // First get the initial data:
  string const dbname(collection->_dbName);
  string const collname(collection->_name);

  string key;
  TRI_voc_rid_t rev = 0;
  int error = ParseKeyAndRef(argv[0], key, rev);

  if (error != TRI_ERROR_NO_ERROR) {
    TRI_V8_EXCEPTION(scope, error);
  }

  triagens::rest::HttpResponse::HttpResponseCode responseCode;
  map<string, string> resultHeaders;
  string resultBody;
  map<string, string> headers;

  error = triagens::arango::deleteDocumentOnCoordinator(
            dbname, collname, key, rev, policy, waitForSync, headers,
            responseCode, resultHeaders, resultBody);

  if (error != TRI_ERROR_NO_ERROR) {
    TRI_V8_EXCEPTION(scope, error);
  }
  // report what the DBserver told us: this could now be 200/202 or
  // 404/412
  TRI_json_t* json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, resultBody.c_str());
  if (responseCode >= triagens::rest::HttpResponse::BAD) {
    if (! TRI_IsArrayJson(json)) {
      if (nullptr != json) {
        TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
      }
      TRI_V8_EXCEPTION(scope, TRI_ERROR_INTERNAL);
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
      return scope.Close(v8::False());
    }

    TRI_V8_EXCEPTION_MESSAGE(scope, errorNum, errorMessage);
  }

  if (json != nullptr) {
    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
  }

  return scope.Close(v8::True());
}


////////////////////////////////////////////////////////////////////////////////
/// @brief deletes a document
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> RemoveVocbaseCol (bool useCollection,
                                               v8::Arguments const& argv) {
  v8::HandleScope scope;
  RemoveOptions options;
  TRI_doc_update_policy_e policy = TRI_DOC_UPDATE_ERROR;

  // check the arguments
  uint32_t const argLength = argv.Length();
    
  TRI_v8_global_t* v8g = static_cast<TRI_v8_global_t*>(v8::Isolate::GetCurrent()->GetData());

  if (argLength < 1 || argLength > 3) {
    TRI_V8_EXCEPTION_USAGE(scope, "remove(<document>, <options>)");
  }

  if (argLength > 1) {
    if (argv[1]->IsObject()) {
      v8::Handle<v8::Object> optionsObject = argv[1].As<v8::Object>();
      if (optionsObject->Has(v8g->OverwriteKey)) {
        options.overwrite = TRI_ObjectToBoolean(optionsObject->Get(v8g->OverwriteKey));
        policy = ExtractUpdatePolicy(options.overwrite);
      }
      if (optionsObject->Has(v8g->WaitForSyncKey)) {
        options.waitForSync = TRI_ObjectToBoolean(optionsObject->Get(v8g->WaitForSyncKey));
      }
    }
    else {// old variant replace(<document>, <data>, <overwrite>, <waitForSync>)
      options.overwrite = TRI_ObjectToBoolean(argv[1]);
      policy = ExtractUpdatePolicy(options.overwrite);
      if (argLength > 2) {
        options.waitForSync = TRI_ObjectToBoolean(argv[2]);
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
    col = TRI_UnwrapClass<TRI_vocbase_col_t>(argv.Holder(), WRP_VOCBASE_COL_TYPE);

    if (col == nullptr) {
      TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract collection");
    }

    vocbase = col->_vocbase;
  }
  else {
    // called as db._remove()
    vocbase = GetContextVocBase();
  }

  if (vocbase == nullptr) {
    TRI_V8_EXCEPTION(scope, TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  V8ResolverGuard resolver(vocbase);
  v8::Handle<v8::Value> err = ParseDocumentOrDocumentHandle(vocbase, resolver.getResolver(), col, key, rid, argv[0]);

  LocalCollectionGuard g(useCollection ? nullptr : const_cast<TRI_vocbase_col_t*>(col));

  if (key.get() == nullptr) {
    TRI_V8_EXCEPTION(scope, TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD);
  }

  if (! err.IsEmpty()) {
    return scope.Close(v8::ThrowException(err));
  }

  TRI_ASSERT(col != nullptr);
  TRI_ASSERT(key.get() != nullptr);

  if (ServerState::instance()->isCoordinator()) {
    return scope.Close(RemoveVocbaseColCoordinator(col, policy, options.waitForSync, argv));
  }

  SingleCollectionWriteTransaction<1> trx(new V8TransactionContext(true), vocbase, col->_cid);
  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_EXCEPTION(scope, res);
  }

  res = trx.deleteDocument(key.get(), policy, options.waitForSync, rid, &actualRevision);
  res = trx.finish(res);

  if (res != TRI_ERROR_NO_ERROR) {
    if (res == TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND && policy == TRI_DOC_UPDATE_LAST_WRITE) {
      return scope.Close(v8::False());
    }
    else {
      TRI_V8_EXCEPTION(scope, res);
    }
  }

  return scope.Close(v8::True());
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

static v8::Handle<v8::Value> JS_DocumentVocbaseCol (v8::Arguments const& argv) {
  return DocumentVocbaseCol(true, argv);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief drops a collection, case of a coordinator in a cluster
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> DropVocbaseColCoordinator (TRI_vocbase_col_t* collection) {
  v8::HandleScope scope;

  if (! collection->_canDrop) {
    TRI_V8_EXCEPTION(scope, TRI_ERROR_FORBIDDEN);
  }

  string const databaseName(collection->_dbName);
  string const cid = StringUtils::itoa(collection->_cid);

  ClusterInfo* ci = ClusterInfo::instance();
  string errorMsg;

  int res = ci->dropCollectionCoordinator(databaseName, cid, errorMsg, 120.0);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_EXCEPTION_MESSAGE(scope, res, errorMsg);
  }

  collection->_status = TRI_VOC_COL_STATUS_DELETED;

  return scope.Close(v8::Undefined());
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

static v8::Handle<v8::Value> JS_DropVocbaseCol (v8::Arguments const& argv) {
  v8::HandleScope scope;

  TRI_vocbase_col_t* collection = TRI_UnwrapClass<TRI_vocbase_col_t>(argv.Holder(), WRP_VOCBASE_COL_TYPE);

  if (collection == nullptr) {
    TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract collection");
  }

  PREVENT_EMBEDDED_TRANSACTION(scope);

  // If we are a coordinator in a cluster, we have to behave differently:
  if (ServerState::instance()->isCoordinator()) {
    return scope.Close(DropVocbaseColCoordinator(collection));
  }

  int res = TRI_DropCollectionVocBase(collection->_vocbase, collection, true);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_EXCEPTION_MESSAGE(scope, res, "cannot drop collection");
  }

  return scope.Close(v8::Undefined());
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

static v8::Handle<v8::Value> JS_ExistsVocbaseCol (v8::Arguments const& argv) {
  return ExistsVocbaseCol(true, argv);
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

static v8::Handle<v8::Value> JS_FiguresVocbaseCol (v8::Arguments const& argv) {
  v8::HandleScope scope;

  TRI_vocbase_col_t* collection = TRI_UnwrapClass<TRI_vocbase_col_t>(argv.Holder(), WRP_VOCBASE_COL_TYPE);

  if (collection == nullptr) {
    TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract collection");
  }

  v8::Handle<v8::Object> result = v8::Object::New();

  TRI_doc_collection_info_t* info;

  if (ServerState::instance()->isCoordinator()) {
    info = GetFiguresCoordinator(collection);
  }
  else {
    info = GetFigures(collection);
  }

  if (info == nullptr) {
    TRI_V8_EXCEPTION_MEMORY(scope);
  }

  v8::Handle<v8::Object> alive = v8::Object::New();

  result->Set(v8::String::New("alive"), alive);
  alive->Set(v8::String::New("count"), v8::Number::New((double) info->_numberAlive));
  alive->Set(v8::String::New("size"), v8::Number::New((double) info->_sizeAlive));

  v8::Handle<v8::Object> dead = v8::Object::New();

  result->Set(v8::String::New("dead"), dead);
  dead->Set(v8::String::New("count"), v8::Number::New((double) info->_numberDead));
  dead->Set(v8::String::New("size"), v8::Number::New((double) info->_sizeDead));
  dead->Set(v8::String::New("deletion"), v8::Number::New((double) info->_numberDeletion));

  // datafile info
  v8::Handle<v8::Object> dfs = v8::Object::New();

  result->Set(v8::String::New("datafiles"), dfs);
  dfs->Set(v8::String::New("count"), v8::Number::New((double) info->_numberDatafiles));
  dfs->Set(v8::String::New("fileSize"), v8::Number::New((double) info->_datafileSize));

  // journal info
  v8::Handle<v8::Object> js = v8::Object::New();

  result->Set(v8::String::New("journals"), js);
  js->Set(v8::String::New("count"), v8::Number::New((double) info->_numberJournalfiles));
  js->Set(v8::String::New("fileSize"), v8::Number::New((double) info->_journalfileSize));

  // compactors info
  v8::Handle<v8::Object> cs = v8::Object::New();

  result->Set(v8::String::New("compactors"), cs);
  cs->Set(v8::String::New("count"), v8::Number::New((double) info->_numberCompactorfiles));
  cs->Set(v8::String::New("fileSize"), v8::Number::New((double) info->_compactorfileSize));

  // shapefiles info
  v8::Handle<v8::Object> sf = v8::Object::New();

  result->Set(v8::String::New("shapefiles"), sf);
  sf->Set(v8::String::New("count"), v8::Number::New((double) info->_numberShapefiles));
  sf->Set(v8::String::New("fileSize"), v8::Number::New((double) info->_shapefileSize));

  // shape info
  v8::Handle<v8::Object> shapes = v8::Object::New();
  result->Set(v8::String::New("shapes"), shapes);
  shapes->Set(v8::String::New("count"), v8::Number::New((double) info->_numberShapes));
  shapes->Set(v8::String::New("size"), v8::Number::New((double) info->_sizeShapes));

  // attributes info
  v8::Handle<v8::Object> attributes = v8::Object::New();
  result->Set(v8::String::New("attributes"), attributes);
  attributes->Set(v8::String::New("count"), v8::Number::New((double) info->_numberAttributes));
  attributes->Set(v8::String::New("size"), v8::Number::New((double) info->_sizeAttributes));

  v8::Handle<v8::Object> indexes = v8::Object::New();
  result->Set(v8::String::New("indexes"), indexes);
  indexes->Set(v8::String::New("count"), v8::Number::New((double) info->_numberIndexes));
  indexes->Set(v8::String::New("size"), v8::Number::New((double) info->_sizeIndexes));

  result->Set(v8::String::New("lastTick"), V8TickId(info->_tickMax));
  result->Set(v8::String::New("uncollectedLogfileEntries"), v8::Number::New((double) info->_uncollectedLogfileEntries));

  TRI_Free(TRI_UNKNOWN_MEM_ZONE, info);

  return scope.Close(result);
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

static v8::Handle<v8::Value> JS_LoadVocbaseCol (v8::Arguments const& argv) {
  v8::HandleScope scope;

  TRI_vocbase_t* vocbase = GetContextVocBase();

  if (vocbase == nullptr) {
    TRI_V8_EXCEPTION(scope, TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  if (ServerState::instance()->isCoordinator()) {
    TRI_vocbase_col_t const* collection = TRI_UnwrapClass<TRI_vocbase_col_t>(argv.Holder(), WRP_VOCBASE_COL_TYPE);

    if (collection == nullptr) {
      TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract collection");
    }

    string const databaseName(collection->_dbName);
    string const cid = StringUtils::itoa(collection->_cid);

    int res = ClusterInfo::instance()->setCollectionStatusCoordinator(databaseName, cid, TRI_VOC_COL_STATUS_LOADED);

    if (res != TRI_ERROR_NO_ERROR) {
      TRI_V8_EXCEPTION(scope, res);
    }

    return scope.Close(v8::Undefined());
  }

  v8::Handle<v8::Object> err;
  TRI_vocbase_col_t const* collection = UseCollection(argv.Holder(), &err);

  if (collection == nullptr) {
    return scope.Close(v8::ThrowException(err));
  }

  ReleaseCollection(collection);
  return scope.Close(v8::Undefined());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the name of a collection
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_NameVocbaseCol (v8::Arguments const& argv) {
  v8::HandleScope scope;

  TRI_vocbase_col_t const* collection = TRI_UnwrapClass<TRI_vocbase_col_t>(argv.Holder(), WRP_VOCBASE_COL_TYPE);

  if (collection == nullptr) {
    TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract collection");
  }

  if (! collection->_isLocal) {
    v8::Handle<v8::Value> result = v8::String::New(collection->_name);
    return scope.Close(result);
  }

  // this copies the name into a new place so we can safely access it later
  // if we wouldn't do this, we would risk other threads modifying the name while
  // we're reading it
  char* name = TRI_GetCollectionNameByIdVocBase(collection->_vocbase, collection->_cid);

  if (name == nullptr) {
    return scope.Close(v8::Undefined());
  }

  v8::Handle<v8::Value> result = v8::String::New(name);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, name);

  return scope.Close(result);
}

static v8::Handle<v8::Value> JS_PlanIdVocbaseCol (v8::Arguments const& argv) {
  v8::HandleScope scope;

  TRI_vocbase_col_t const* collection = TRI_UnwrapClass<TRI_vocbase_col_t>(argv.Holder(), WRP_VOCBASE_COL_TYPE);

  if (collection == nullptr) {
    TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract collection");
  }

  if (ServerState::instance()->isCoordinator()) {
    return scope.Close(V8CollectionId(collection->_cid));
  }

  return scope.Close(V8CollectionId(collection->_planId));
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

static v8::Handle<v8::Value> JS_PropertiesVocbaseCol (v8::Arguments const& argv) {
  v8::HandleScope scope;

  TRI_v8_global_t* v8g = static_cast<TRI_v8_global_t*>(v8::Isolate::GetCurrent()->GetData());

  TRI_vocbase_col_t const* collection = TRI_UnwrapClass<TRI_vocbase_col_t>(argv.Holder(), WRP_VOCBASE_COL_TYPE);

  if (collection == nullptr) {
    TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract collection");
  }

  if (ServerState::instance()->isCoordinator()) {
    std::string const databaseName = std::string(collection->_dbName);
    TRI_col_info_t info = ClusterInfo::instance()->getCollectionProperties(databaseName, StringUtils::itoa(collection->_cid));

    if (0 < argv.Length()) {
      v8::Handle<v8::Value> par = argv[0];

      if (par->IsObject()) {
        v8::Handle<v8::Object> po = par->ToObject();

        // extract doCompact flag
        if (po->Has(v8g->DoCompactKey)) {
          info._doCompact = TRI_ObjectToBoolean(po->Get(v8g->DoCompactKey));
        }

        // extract sync flag
        if (po->Has(v8g->WaitForSyncKey)) {
          info._waitForSync = TRI_ObjectToBoolean(po->Get(v8g->WaitForSyncKey));
        }

        // extract the journal size
        if (po->Has(v8g->JournalSizeKey)) {
          info._maximalSize = (TRI_voc_size_t) TRI_ObjectToUInt64(po->Get(v8g->JournalSizeKey), false);

          if (info._maximalSize < TRI_JOURNAL_MINIMAL_SIZE) {
            if (info._keyOptions != nullptr) {
              TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, info._keyOptions);
            }
            TRI_V8_EXCEPTION_PARAMETER(scope, "<properties>.journalSize too small");
          }
        }

        if (po->Has(v8g->IsVolatileKey)) {
          if (TRI_ObjectToBoolean(po->Get(v8g->IsVolatileKey)) != info._isVolatile) {
            if (info._keyOptions != nullptr) {
              TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, info._keyOptions);
            }
            TRI_V8_EXCEPTION_PARAMETER(scope, "isVolatile option cannot be changed at runtime");
          }
        }

        if (info._isVolatile && info._waitForSync) {
          if (info._keyOptions != nullptr) {
            TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, info._keyOptions);
          }
          TRI_V8_EXCEPTION_PARAMETER(scope, "volatile collections do not support the waitForSync option");
        }
      }

      int res = ClusterInfo::instance()->setCollectionPropertiesCoordinator(databaseName, StringUtils::itoa(collection->_cid), &info);

      if (res != TRI_ERROR_NO_ERROR) {
        if (info._keyOptions != nullptr) {
          TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, info._keyOptions);
        }
        TRI_V8_EXCEPTION(scope, res);
      }
    }


    // return the current parameter set
    v8::Handle<v8::Object> result = v8::Object::New();

    result->Set(v8g->DoCompactKey, info._doCompact ? v8::True() : v8::False());
    result->Set(v8g->IsSystemKey, info._isSystem ? v8::True() : v8::False());
    result->Set(v8g->IsVolatileKey, info._isVolatile ? v8::True() : v8::False());
    result->Set(v8g->JournalSizeKey, v8::Number::New(info._maximalSize));
    result->Set(v8g->WaitForSyncKey, info._waitForSync ? v8::True() : v8::False());

    shared_ptr<CollectionInfo> c = ClusterInfo::instance()->getCollection(databaseName, StringUtils::itoa(collection->_cid));
    v8::Handle<v8::Array> shardKeys = v8::Array::New();
    vector<string> const sks = (*c).shardKeys();
    for (size_t i = 0; i < sks.size(); ++i) {
      shardKeys->Set((uint32_t) i, v8::String::New(sks[i].c_str(), (int) sks[i].size()));
    }
    result->Set(v8::String::New("shardKeys"), shardKeys);
    result->Set(v8::String::New("numberOfShards"), v8::Number::New((*c).numberOfShards()));

    if (info._keyOptions != nullptr) {
      result->Set(v8g->KeyOptionsKey, TRI_ObjectJson(info._keyOptions)->ToObject());
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, info._keyOptions);
    }

    return scope.Close(result);
  }

  v8::Handle<v8::Object> err;
  collection = UseCollection(argv.Holder(), &err);

  if (collection == nullptr) {
    return scope.Close(v8::ThrowException(err));
  }

  TRI_document_collection_t* document = collection->_collection;
  TRI_collection_t* base = document;

  // check if we want to change some parameters
  if (0 < argv.Length()) {
    v8::Handle<v8::Value> par = argv[0];

    if (par->IsObject()) {
      v8::Handle<v8::Object> po = par->ToObject();

      // get the old values
      TRI_LOCK_JOURNAL_ENTRIES_DOC_COLLECTION(document);

      TRI_voc_size_t maximalSize = base->_info._maximalSize;
      bool doCompact     = base->_info._doCompact;
      bool waitForSync   = base->_info._waitForSync;

      TRI_UNLOCK_JOURNAL_ENTRIES_DOC_COLLECTION(document);

      // extract doCompact flag
      if (po->Has(v8g->DoCompactKey)) {
        doCompact = TRI_ObjectToBoolean(po->Get(v8g->DoCompactKey));
      }

      // extract sync flag
      if (po->Has(v8g->WaitForSyncKey)) {
        waitForSync = TRI_ObjectToBoolean(po->Get(v8g->WaitForSyncKey));
      }

      // extract the journal size
      if (po->Has(v8g->JournalSizeKey)) {
        maximalSize = (TRI_voc_size_t) TRI_ObjectToUInt64(po->Get(v8g->JournalSizeKey), false);

        if (maximalSize < TRI_JOURNAL_MINIMAL_SIZE) {
          ReleaseCollection(collection);
          TRI_V8_EXCEPTION_PARAMETER(scope, "<properties>.journalSize too small");
        }
      }

      if (po->Has(v8g->IsVolatileKey)) {
        if (TRI_ObjectToBoolean(po->Get(v8g->IsVolatileKey)) != base->_info._isVolatile) {
          ReleaseCollection(collection);
          TRI_V8_EXCEPTION_PARAMETER(scope, "isVolatile option cannot be changed at runtime");
        }
      }

      if (base->_info._isVolatile && waitForSync) {
        // the combination of waitForSync and isVolatile makes no sense
        ReleaseCollection(collection);
        TRI_V8_EXCEPTION_PARAMETER(scope, "volatile collections do not support the waitForSync option");
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
        TRI_V8_EXCEPTION(scope, res);
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
  v8::Handle<v8::Object> result = v8::Object::New();

  result->Set(v8g->DoCompactKey, base->_info._doCompact ? v8::True() : v8::False());
  result->Set(v8g->IsSystemKey, base->_info._isSystem ? v8::True() : v8::False());
  result->Set(v8g->IsVolatileKey, base->_info._isVolatile ? v8::True() : v8::False());
  result->Set(v8g->JournalSizeKey, v8::Number::New(base->_info._maximalSize));

  TRI_json_t* keyOptions = document->_keyGenerator->toJson(TRI_UNKNOWN_MEM_ZONE);

  if (keyOptions != nullptr) {
    result->Set(v8g->KeyOptionsKey, TRI_ObjectJson(keyOptions)->ToObject());

    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, keyOptions);
  }
  else {
    result->Set(v8g->KeyOptionsKey, v8::Array::New());
  }
  result->Set(v8g->WaitForSyncKey, base->_info._waitForSync ? v8::True() : v8::False());

  ReleaseCollection(collection);
  return scope.Close(result);
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

static v8::Handle<v8::Value> JS_RemoveVocbaseCol (v8::Arguments const& argv) {
  return RemoveVocbaseCol(true, argv);
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

static v8::Handle<v8::Value> JS_RenameVocbaseCol (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() < 1) {
    TRI_V8_EXCEPTION_USAGE(scope, "rename(<name>)");
  }

  if (ServerState::instance()->isCoordinator()) {
    // renaming a collection in a cluster is unsupported
    TRI_V8_EXCEPTION(scope, TRI_ERROR_CLUSTER_UNSUPPORTED);
  }

  string const name = TRI_ObjectToString(argv[0]);

  // second parameter "override" is to override renaming restrictions, e.g.
  // renaming from a system collection name to a non-system collection name and
  // vice versa. this parameter is not publicly exposed but used internally
  bool doOverride = false;
  if (argv.Length() > 1) {
    doOverride = TRI_ObjectToBoolean(argv[1]);
  }

  if (name.empty()) {
    TRI_V8_EXCEPTION_PARAMETER(scope, "<name> must be non-empty");
  }

  TRI_vocbase_col_t* collection = TRI_UnwrapClass<TRI_vocbase_col_t>(argv.Holder(), WRP_VOCBASE_COL_TYPE);

  if (collection == nullptr) {
    TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract collection");
  }

  PREVENT_EMBEDDED_TRANSACTION(scope);

  if (ServerState::instance()->isCoordinator()) {
    // renaming a collection in a cluster is unsupported
    TRI_V8_EXCEPTION(scope, TRI_ERROR_CLUSTER_UNSUPPORTED);
  }

  int res = TRI_RenameCollectionVocBase(collection->_vocbase,
                                        collection,
                                        name.c_str(),
                                        doOverride, 
                                        true);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_EXCEPTION_MESSAGE(scope, res, "cannot rename collection");
  }

  return scope.Close(v8::Undefined());
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

static v8::Handle<v8::Value> JS_ReplaceVocbaseCol (v8::Arguments const& argv) {
  return ReplaceVocbaseCol(true, argv);
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

static v8::Handle<v8::Value> JS_RevisionVocbaseCol (v8::Arguments const& argv) {
  v8::HandleScope scope;

  TRI_vocbase_col_t* collection = TRI_UnwrapClass<TRI_vocbase_col_t>(argv.Holder(), WRP_VOCBASE_COL_TYPE);

  if (collection == nullptr) {
    TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract collection");
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
    TRI_V8_EXCEPTION(scope, res);
  }

  return scope.Close(V8RevisionId(rid));
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

static v8::Handle<v8::Value> JS_RotateVocbaseCol (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (ServerState::instance()->isCoordinator()) {
    // renaming a collection in a cluster is unsupported
    TRI_V8_EXCEPTION(scope, TRI_ERROR_CLUSTER_UNSUPPORTED);
  }

  PREVENT_EMBEDDED_TRANSACTION(scope);

  v8::Handle<v8::Object> err;
  TRI_vocbase_col_t const* collection = UseCollection(argv.Holder(), &err);

  if (collection == nullptr) {
    return scope.Close(v8::ThrowException(err));
  }

  TRI_SHARDING_COLLECTION_NOT_YET_IMPLEMENTED(scope, collection);

  TRI_document_collection_t* document = collection->_collection;

  int res = TRI_RotateJournalDocumentCollection(document);

  ReleaseCollection(collection);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_EXCEPTION_MESSAGE(scope, res, "could not rotate journal");
  }

  return scope.Close(v8::Undefined());
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

static v8::Handle<v8::Value> JS_UpdateVocbaseCol (v8::Arguments const& argv) {
  return UpdateVocbaseCol(true, argv);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief saves a document, coordinator case in a cluster
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> InsertVocbaseColCoordinator (TRI_vocbase_col_t* collection,
                                                          v8::Arguments const& argv) {
  v8::HandleScope scope;

  // First get the initial data:
  string const dbname(collection->_dbName);

  // TODO: someone might rename the collection while we're reading its name...
  string const collname(collection->_name);

  // Now get the arguments
  uint32_t const argLength = argv.Length();
  if (argLength < 1 || argLength > 2) {
    TRI_V8_EXCEPTION_USAGE(scope, "insert(<data>, [<waitForSync>])");
  }

  InsertOptions options;
  if (argLength > 1 && argv[1]->IsObject()) {
    TRI_v8_global_t* v8g = static_cast<TRI_v8_global_t*>(v8::Isolate::GetCurrent()->GetData());
    v8::Handle<v8::Object> optionsObject = argv[1].As<v8::Object>();
    if (optionsObject->Has(v8g->WaitForSyncKey)) {
      options.waitForSync = TRI_ObjectToBoolean(optionsObject->Get(v8g->WaitForSyncKey));
    }
    if (optionsObject->Has(v8g->SilentKey)) {
      options.silent = TRI_ObjectToBoolean(optionsObject->Get(v8g->SilentKey));
    }
  }
  else {
    options.waitForSync = ExtractWaitForSync(argv, 2);
  }

  TRI_json_t* json = TRI_ObjectToJson(argv[0]);
  if (! TRI_IsArrayJson(json)) {
    if (json != nullptr) {
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
    }
    TRI_V8_EXCEPTION(scope, TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
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
    TRI_V8_EXCEPTION(scope, error);
  }
  // report what the DBserver told us: this could now be 201/202 or
  // 400/404
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, resultBody.c_str());
  if (responseCode >= triagens::rest::HttpResponse::BAD) {
    if (! TRI_IsArrayJson(json)) {
      if (json != nullptr) {
        TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
      }
      TRI_V8_EXCEPTION(scope, TRI_ERROR_INTERNAL);
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
    TRI_V8_EXCEPTION_MESSAGE(scope, errorNum, errorMessage);
  }

  if (options.silent) {
    if (json != nullptr) {
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
    }
    return scope.Close(v8::True());
  }

  v8::Handle<v8::Value> ret = TRI_ObjectJson(json);
  if (json != nullptr) {
    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
  }
  return scope.Close(ret);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extract a key from a v8 object
////////////////////////////////////////////////////////////////////////////////

static string GetId (v8::Handle<v8::Value> const arg) {
  if (arg->IsObject() && ! arg->IsArray()) {
    v8::Local<v8::Object> obj = arg->ToObject();

    TRI_v8_global_t* v8g = static_cast<TRI_v8_global_t*>(v8::Isolate::GetCurrent()->GetData());

    if (obj->Has(v8g->_IdKey)) {
      return TRI_ObjectToString(obj->Get(v8g->_IdKey));
    }
  }

  return TRI_ObjectToString(arg);
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

static v8::Handle<v8::Value> InsertEdgeCol (TRI_vocbase_col_t* col,
                                            v8::Arguments const& argv) {
  v8::HandleScope scope;

  TRI_v8_global_t* v8g = static_cast<TRI_v8_global_t*>(v8::Isolate::GetCurrent()->GetData());

  uint32_t const argLength = argv.Length();
  if (argLength < 3 || argLength > 4) {
    TRI_V8_EXCEPTION_USAGE(scope, "save(<from>, <to>, <data>, [<waitForSync>])");
  }

  InsertOptions options;

  // set document key
  std::unique_ptr<char[]> key;
  int res;

  if (argv[2]->IsObject() && ! argv[2]->IsArray()) {
    res = ExtractDocumentKey(v8g, argv[2]->ToObject(), key);

    if (res != TRI_ERROR_NO_ERROR && res != TRI_ERROR_ARANGO_DOCUMENT_KEY_MISSING) {
      TRI_V8_EXCEPTION(scope, res);
    }
  }
  else {
    TRI_V8_EXCEPTION(scope, TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
  }

  if (argLength > 3 && argv[3]->IsObject()) {
    v8::Handle<v8::Object> optionsObject = argv[3].As<v8::Object>();
    if (optionsObject->Has(v8g->WaitForSyncKey)) {
      options.waitForSync = TRI_ObjectToBoolean(optionsObject->Get(v8g->WaitForSyncKey));
    }
    if (optionsObject->Has(v8g->SilentKey)) {
      options.silent = TRI_ObjectToBoolean(optionsObject->Get(v8g->SilentKey));
    }
  }
  else {
    options.waitForSync = ExtractWaitForSync(argv, 4);
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
  res = TRI_ParseVertex(trx.resolver(), edge._fromCid, fromKey, argv[0]);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_EXCEPTION(scope, res);
  }
  edge._fromKey = fromKey.get();

  // extract to
  res = TRI_ParseVertex(trx.resolver(), edge._toCid, toKey, argv[1]);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_EXCEPTION(scope, res);
  }
  edge._toKey = toKey.get();


  //  start transaction
  res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_EXCEPTION(scope, res);
  }

  TRI_document_collection_t* document = trx.documentCollection();
  TRI_memory_zone_t* zone = document->getShaper()->_memoryZone;  // PROTECTED by trx from above
  
  // fetch a barrier so nobody unlinks datafiles with the shapes & attributes we might
  // need for this document
  if (trx.orderBarrier(trx.trxCollection()) == nullptr) {
    TRI_V8_EXCEPTION_MEMORY(scope);
  }

  // extract shaped data
  TRI_shaped_json_t* shaped = TRI_ShapedJsonV8Object(argv[2], document->getShaper(), true);  // PROTECTED by trx here

  if (shaped == nullptr) {
    TRI_V8_EXCEPTION_MESSAGE(scope, TRI_errno(), "<data> cannot be converted into JSON shape");
  }

  TRI_doc_mptr_copy_t mptr;
  res = trx.createEdge(key.get(), &mptr, shaped, options.waitForSync, &edge);

  res = trx.finish(res);

  TRI_FreeShapedJson(zone, shaped);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_EXCEPTION(scope, res);
  }

  TRI_ASSERT(mptr.getDataPtr() != nullptr);  // PROTECTED by trx here

  if (options.silent) {
     return scope.Close(v8::True());
  }
  else {
    char const* docKey = TRI_EXTRACT_MARKER_KEY(&mptr);  // PROTECTED by trx here

    v8::Handle<v8::Object> result = v8::Object::New();
    result->Set(v8g->_IdKey, V8DocumentId(trx.resolver()->getCollectionName(col->_cid), docKey));
    result->Set(v8g->_RevKey, V8RevisionId(mptr._rid));
    result->Set(v8g->_KeyKey, v8::String::New(docKey));

    return scope.Close(result); 
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief saves an edge, coordinator case in a cluster
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> InsertEdgeColCoordinator (TRI_vocbase_col_t* collection,
                                                       v8::Arguments const& argv) {
  v8::HandleScope scope;

  // First get the initial data:
  string const dbname(collection->_dbName);

  // TODO: someone might rename the collection while we're reading its name...
  string const collname(collection->_name);

  uint32_t const argLength = argv.Length();
  if (argLength < 3 || argLength > 4) {
    TRI_V8_EXCEPTION_USAGE(scope, "insert(<from>, <to>, <data>, [<waitForSync>])");
  }

  string _from = GetId(argv[0]);
  string _to   = GetId(argv[1]);

  TRI_json_t* json = TRI_ObjectToJson(argv[2]);

  if (! TRI_IsArrayJson(json)) {
    if (json != nullptr) {
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
    }
    TRI_V8_EXCEPTION(scope, TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
  }

  InsertOptions options;
  if (argLength > 3 && argv[3]->IsObject()) {
    TRI_v8_global_t* v8g = static_cast<TRI_v8_global_t*>(v8::Isolate::GetCurrent()->GetData());
    v8::Handle<v8::Object> optionsObject = argv[3].As<v8::Object>();
    if (optionsObject->Has(v8g->WaitForSyncKey)) {
      options.waitForSync = TRI_ObjectToBoolean(optionsObject->Get(v8g->WaitForSyncKey));
    }
    if (optionsObject->Has(v8g->SilentKey)) {
      options.silent = TRI_ObjectToBoolean(optionsObject->Get(v8g->SilentKey));
    }
  }
  else {
    options.waitForSync = ExtractWaitForSync(argv, 4);
  }

  triagens::rest::HttpResponse::HttpResponseCode responseCode;
  map<string, string> resultHeaders;
  string resultBody;

  int error = triagens::arango::createEdgeOnCoordinator(
            dbname, collname, options.waitForSync, json, _from.c_str(), _to.c_str(),
            responseCode, resultHeaders, resultBody);
  // Note that the json has been freed inside!

  if (error != TRI_ERROR_NO_ERROR) {
    TRI_V8_EXCEPTION(scope, error);
  }
  // report what the DBserver told us: this could now be 201/202 or
  // 400/404
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, resultBody.c_str());
  if (responseCode >= triagens::rest::HttpResponse::BAD) {
    if (! TRI_IsArrayJson(json)) {
      if (nullptr != json) {
        TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
      }
      TRI_V8_EXCEPTION(scope, TRI_ERROR_INTERNAL);
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
    TRI_V8_EXCEPTION_MESSAGE(scope, errorNum, errorMessage);
  }
  v8::Handle<v8::Value> ret = TRI_ObjectJson(json);
  if (nullptr != json) {
    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
  }
  return scope.Close(ret);
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

static v8::Handle<v8::Value> JS_InsertVocbaseCol (v8::Arguments const& argv) {
  v8::HandleScope scope;

  TRI_vocbase_col_t* collection = TRI_UnwrapClass<TRI_vocbase_col_t>(argv.Holder(), WRP_VOCBASE_COL_TYPE);

  if (collection == nullptr) {
    TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract collection");
  }

  if (ServerState::instance()->isCoordinator()) {
    // coordinator case
    if ((TRI_col_type_e) collection->_type == TRI_COL_TYPE_DOCUMENT) {
      return scope.Close(InsertVocbaseColCoordinator(collection, argv));
    }

    return scope.Close(InsertEdgeColCoordinator(collection, argv));
  }
    
  // single server case
  if ((TRI_col_type_e) collection->_type == TRI_COL_TYPE_DOCUMENT) {
    return scope.Close(InsertVocbaseCol(collection, argv));
  }

  return scope.Close(InsertEdgeCol(collection, argv));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the status of a collection
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_StatusVocbaseCol (v8::Arguments const& argv) {
  v8::HandleScope scope;

  TRI_vocbase_col_t* collection = TRI_UnwrapClass<TRI_vocbase_col_t>(argv.Holder(), WRP_VOCBASE_COL_TYPE);

  if (collection == nullptr) {
    TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract collection");
  }

  if (ServerState::instance()->isCoordinator()) {
    std::string const databaseName = std::string(collection->_dbName);

    shared_ptr<CollectionInfo> const& ci
        = ClusterInfo::instance()->getCollection(databaseName,
                                        StringUtils::itoa(collection->_cid));

    if ((*ci).empty()) {
      return scope.Close(v8::Number::New((int) TRI_VOC_COL_STATUS_DELETED));
    }
    return scope.Close(v8::Number::New((int) ci->status()));
  }
  // fallthru intentional

  TRI_READ_LOCK_STATUS_VOCBASE_COL(collection);
  TRI_vocbase_col_status_e status = collection->_status;
  TRI_READ_UNLOCK_STATUS_VOCBASE_COL(collection);

  return scope.Close(v8::Number::New((int) status));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief truncates a collection
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_TruncateVocbaseCol (v8::Arguments const& argv) {
  v8::HandleScope scope;

  bool const forceSync = ExtractWaitForSync(argv, 1);

  TRI_vocbase_col_t* collection = TRI_UnwrapClass<TRI_vocbase_col_t>(argv.Holder(), WRP_VOCBASE_COL_TYPE);

  if (collection == nullptr) {
    TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract collection");
  }

  TRI_SHARDING_COLLECTION_NOT_YET_IMPLEMENTED(scope, collection);

  SingleCollectionWriteTransaction<UINT64_MAX> trx(new V8TransactionContext(true), collection->_vocbase, collection->_cid);
  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_EXCEPTION(scope, res);
  }

  if (trx.orderBarrier(trx.trxCollection()) == nullptr) {
    TRI_V8_EXCEPTION_MEMORY(scope);
  }

  res = trx.truncate(forceSync);
  res = trx.finish(res);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_EXCEPTION(scope, res);
  }

  return scope.Close(v8::Undefined());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief truncates a datafile
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_TruncateDatafileVocbaseCol (v8::Arguments const& argv) {
  v8::HandleScope scope;

  TRI_vocbase_col_t* collection = TRI_UnwrapClass<TRI_vocbase_col_t>(argv.Holder(), WRP_VOCBASE_COL_TYPE);

  if (collection == nullptr) {
    TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract collection");
  }

  TRI_SHARDING_COLLECTION_NOT_YET_IMPLEMENTED(scope, collection);

  if (argv.Length() != 2) {
    TRI_V8_EXCEPTION_USAGE(scope, "truncateDatafile(<datafile>, <size>)");
  }

  string path = TRI_ObjectToString(argv[0]);
  size_t size = (size_t) TRI_ObjectToInt64(argv[1]);

  TRI_READ_LOCK_STATUS_VOCBASE_COL(collection);

  if (collection->_status != TRI_VOC_COL_STATUS_UNLOADED &&
      collection->_status != TRI_VOC_COL_STATUS_CORRUPTED) {
    TRI_READ_UNLOCK_STATUS_VOCBASE_COL(collection);
    TRI_V8_EXCEPTION(scope, TRI_ERROR_ARANGO_COLLECTION_NOT_UNLOADED);
  }

  int res = TRI_TruncateDatafile(path.c_str(), (TRI_voc_size_t) size);

  TRI_READ_UNLOCK_STATUS_VOCBASE_COL(collection);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_EXCEPTION_MESSAGE(scope, res, "cannot truncate datafile");
  }

  return scope.Close(v8::Undefined());
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

static v8::Handle<v8::Value> JS_TypeVocbaseCol (v8::Arguments const& argv) {
  v8::HandleScope scope;

  TRI_vocbase_col_t* collection = TRI_UnwrapClass<TRI_vocbase_col_t>(argv.Holder(), WRP_VOCBASE_COL_TYPE);

  if (collection == nullptr) {
    TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract collection");
  }

  if (ServerState::instance()->isCoordinator()) {
    std::string const databaseName = std::string(collection->_dbName);

    shared_ptr<CollectionInfo> const& ci
        = ClusterInfo::instance()->getCollection(databaseName,
                                      StringUtils::itoa(collection->_cid));

    if ((*ci).empty()) {
      return scope.Close(v8::Number::New((int) collection->_type));
    }
    return scope.Close(v8::Number::New((int) ci->type()));
  }
  // fallthru intentional

  TRI_READ_LOCK_STATUS_VOCBASE_COL(collection);
  TRI_col_type_e type = (TRI_col_type_e) collection->_type;
  TRI_READ_UNLOCK_STATUS_VOCBASE_COL(collection);

  return scope.Close(v8::Number::New((int) type));
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

static v8::Handle<v8::Value> JS_UnloadVocbaseCol (v8::Arguments const& argv) {
  v8::HandleScope scope;

  TRI_vocbase_col_t* collection = TRI_UnwrapClass<TRI_vocbase_col_t>(argv.Holder(), WRP_VOCBASE_COL_TYPE);

  if (collection == nullptr) {
    TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract collection");
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
    TRI_V8_EXCEPTION(scope, res);
  }

  return scope.Close(v8::Undefined());
}


////////////////////////////////////////////////////////////////////////////////
/// @brief returns the version of a collection
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_VersionVocbaseCol (v8::Arguments const& argv) {
  v8::HandleScope scope;

  TRI_vocbase_col_t* collection = TRI_UnwrapClass<TRI_vocbase_col_t>(argv.Holder(), WRP_VOCBASE_COL_TYPE);

  if (collection == nullptr) {
    TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract collection");
  }

  if (ServerState::instance()->isCoordinator()) {
    return scope.Close(v8::Number::New((int) TRI_COL_VERSION));
  }
  // fallthru intentional

  TRI_col_info_t info;

  TRI_READ_LOCK_STATUS_VOCBASE_COL(collection);
  int res = TRI_LoadCollectionInfo(collection->_path, &info, false);
  TRI_READ_UNLOCK_STATUS_VOCBASE_COL(collection);

  TRI_FreeCollectionInfoOptions(&info);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_EXCEPTION_MESSAGE(scope, res, "cannot fetch collection info");
  }

  return scope.Close(v8::Number::New((int) info._version));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks all data pointers in a collection
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_MAINTAINER_MODE
static v8::Handle<v8::Value> JS_CheckPointersVocbaseCol (v8::Arguments const& argv) {
  v8::HandleScope scope;

  TRI_vocbase_col_t* collection = TRI_UnwrapClass<TRI_vocbase_col_t>(argv.Holder(), WRP_VOCBASE_COL_TYPE);

  if (collection == nullptr) {
    TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract collection");
  }

  TRI_SHARDING_COLLECTION_NOT_YET_IMPLEMENTED(scope, collection);

  SingleCollectionReadOnlyTransaction trx(new V8TransactionContext(true), collection->_vocbase, collection->_cid);

  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_EXCEPTION(scope, res);
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
        TRI_V8_EXCEPTION(scope, TRI_ERROR_INTERNAL);
      }
    }
  }

  return scope.Close(v8::True());
}
#endif



////////////////////////////////////////////////////////////////////////////////
/// @brief changes the operation mode of the server
/// @startDocuBock TODO
/// `db._changeMode(<mode>)`
///
/// Sets the server to the given mode.
/// Possible parameters for mode are:
/// - Normal
/// - ReadOnly
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

static v8::Handle<v8::Value> JS_ChangeOperationModeVocbase (v8::Arguments const& argv) {
  v8::HandleScope scope;

  TRI_v8_global_t* v8g = static_cast<TRI_v8_global_t*>(v8::Isolate::GetCurrent()->GetData());

  bool allowModeChange = false;
  if (v8g->_currentRequest.IsEmpty()|| v8g->_currentRequest->IsUndefined()) {
    // console mode
    allowModeChange = true;
  }
  else if (v8g->_currentRequest->IsObject()) {
    v8::Handle<v8::Object> obj = v8::Handle<v8::Object>::Cast(v8g->_currentRequest);

    if (obj->Has(v8g->PortTypeKey)) {
      string const portType = TRI_ObjectToString(obj->Get(v8g->PortTypeKey));
      if (portType == "unix") {
        allowModeChange = true;
      }
    }
  }

  if (! allowModeChange) {
    TRI_V8_EXCEPTION(scope, TRI_ERROR_FORBIDDEN);
  }

  // expecting one argument
  if (argv.Length() != 1) {
    TRI_V8_EXCEPTION_USAGE(scope, "_changeMode(<mode>), with modes: 'Normal', 'NoCreate'");
  }

  string const newModeStr = TRI_ObjectToString(argv[0]);

  TRI_vocbase_operationmode_e newMode = TRI_VOCBASE_MODE_NORMAL;

  if (newModeStr == "NoCreate") {
    newMode = TRI_VOCBASE_MODE_NO_CREATE;
  }
  else if (newModeStr != "Normal") {
    TRI_V8_EXCEPTION_USAGE(scope, "illegal mode, allowed modes are: 'Normal' and 'NoCreate'");
  }

  TRI_ChangeOperationModeServer(newMode);

  return scope.Close(v8::True());
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

static v8::Handle<v8::Value> JS_CollectionVocbase (v8::Arguments const& argv) {
  v8::HandleScope scope;

  TRI_vocbase_t* vocbase = GetContextVocBase();

  if (vocbase == nullptr) {
    TRI_V8_EXCEPTION(scope, TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  // expecting one argument
  if (argv.Length() != 1) {
    TRI_V8_EXCEPTION_USAGE(scope, "_collection(<name>|<identifier>)");
  }

  v8::Handle<v8::Value> val = argv[0];
  TRI_vocbase_col_t const* collection = 0;

  if (ServerState::instance()->isCoordinator()) {
    string const name = TRI_ObjectToString(val);
    shared_ptr<CollectionInfo> const& ci
        = ClusterInfo::instance()->getCollection(vocbase->_name, name);

    if ((*ci).id() == 0 || (*ci).empty()) {
      // not found
      return scope.Close(v8::Null());
    }

    collection = CoordinatorCollection(vocbase, *ci);
  }
  else {
    collection = GetCollectionFromArgument(vocbase, val);
  }

  if (collection == nullptr) {
    return scope.Close(v8::Null());
  }

  v8::Handle<v8::Value> result = WrapCollection(collection);

  if (result.IsEmpty()) {
    TRI_V8_EXCEPTION_MEMORY(scope);
  }

  return scope.Close(result);
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

static v8::Handle<v8::Value> JS_CollectionsVocbase (v8::Arguments const& argv) {
  v8::HandleScope scope;

  TRI_vocbase_t* vocbase = GetContextVocBase();

  if (vocbase == nullptr) {
    TRI_V8_EXCEPTION(scope, TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
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
  v8::Handle<v8::Array> result = v8::Array::New();

  uint32_t n = (uint32_t) colls._length;
  for (uint32_t i = 0;  i < n;  ++i) {
    TRI_vocbase_col_t const* collection = (TRI_vocbase_col_t const*) colls._buffer[i];

    v8::Handle<v8::Value> c = WrapCollection(collection);

    if (c.IsEmpty()) {
      error = true;
      break;
    }

    result->Set(i, c);
  }

  TRI_DestroyVectorPointer(&colls);

  if (error) {
    TRI_V8_EXCEPTION_MEMORY(scope);
  }

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns all collection names
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_CompletionsVocbase (v8::Arguments const& argv) {
  v8::HandleScope scope;

  TRI_vocbase_t* vocbase = GetContextVocBase();

  if (vocbase == nullptr) {
    return scope.Close(v8::Array::New());
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

  v8::Handle<v8::Array> result = v8::Array::New();
  // add collection names
  for (size_t i = 0;  i < n;  ++i) {
    char const* name = TRI_AtVectorString(&names, i);

    if (name != nullptr) {
      result->Set(j++, v8::String::New(name));
    }
  }

  TRI_DestroyVectorString(&names);

  // add function names. these are hard coded
  result->Set(j++, v8::String::New("_changeMode()"));
  result->Set(j++, v8::String::New("_collection()"));
  result->Set(j++, v8::String::New("_collections()"));
  result->Set(j++, v8::String::New("_create()"));
  result->Set(j++, v8::String::New("_createDatabase()"));
  result->Set(j++, v8::String::New("_createDocumentCollection()"));
  result->Set(j++, v8::String::New("_createEdgeCollection()"));
  result->Set(j++, v8::String::New("_createStatement()"));
  result->Set(j++, v8::String::New("_document()"));
  result->Set(j++, v8::String::New("_drop()"));
  result->Set(j++, v8::String::New("_dropDatabase()"));
  result->Set(j++, v8::String::New("_executeTransaction()"));
  result->Set(j++, v8::String::New("_exists()"));
  result->Set(j++, v8::String::New("_id"));
  result->Set(j++, v8::String::New("_isSystem()"));
  result->Set(j++, v8::String::New("_listDatabases()"));
  result->Set(j++, v8::String::New("_name()"));
  result->Set(j++, v8::String::New("_path()"));
  result->Set(j++, v8::String::New("_query()"));
  result->Set(j++, v8::String::New("_remove()"));
  result->Set(j++, v8::String::New("_replace()"));
  result->Set(j++, v8::String::New("_update()"));
  result->Set(j++, v8::String::New("_useDatabase()"));
  result->Set(j++, v8::String::New("_version()"));

  return scope.Close(result);
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

static v8::Handle<v8::Value> JS_RemoveVocbase (v8::Arguments const& argv) {
  return RemoveVocbaseCol(false, argv);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a document and returns it
/// @startDocuBlock documentsDocumentName
/// `db._document(document)`
///
/// This method finds a document given its identifier.  It returns the document
/// if the document exists. An error is throw if no document with the given
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

static v8::Handle<v8::Value> JS_DocumentVocbase (v8::Arguments const& argv) {
  return DocumentVocbaseCol(false, argv);
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

static v8::Handle<v8::Value> JS_ExistsVocbase (v8::Arguments const& argv) {
  return ExistsVocbaseCol(false, argv);
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

static v8::Handle<v8::Value> JS_ReplaceVocbase (v8::Arguments const& argv) {
  return ReplaceVocbaseCol(false, argv);
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

static v8::Handle<v8::Value> JS_UpdateVocbase (v8::Arguments const& argv) {
  return UpdateVocbaseCol(false, argv);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the server version string
/// @startDocuBlock databaseVersion
/// `db._version()`
///
/// Returns the server version string.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_VersionServer (v8::Arguments const& argv) {
  v8::HandleScope scope;

  return scope.Close(v8::String::New(TRI_VERSION));
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

static v8::Handle<v8::Value> JS_CountVocbaseCol (v8::Arguments const& argv) {
  v8::HandleScope scope;

  TRI_vocbase_col_t* collection = TRI_UnwrapClass<TRI_vocbase_col_t>(argv.Holder(), WRP_VOCBASE_COL_TYPE);

  if (collection == nullptr) {
    TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract collection");
  }

  if (argv.Length() != 0) {
    TRI_V8_EXCEPTION_USAGE(scope, "count()");
  }

  if (ServerState::instance()->isCoordinator()) {
    // First get the initial data:
    string const dbname(collection->_dbName);

    // TODO: someone might rename the collection while we're reading its name...
    string const collname(collection->_name);

    uint64_t count = 0;
    int error = triagens::arango::countOnCoordinator(dbname, collname, count);

    if (error != TRI_ERROR_NO_ERROR) {
      TRI_V8_EXCEPTION(scope, error);
    }

    return scope.Close(v8::Number::New((double) count));
  }

  SingleCollectionReadOnlyTransaction trx(new V8TransactionContext(true), collection->_vocbase, collection->_cid);

  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_EXCEPTION(scope, res);
  }

  TRI_document_collection_t* document = trx.documentCollection();

  // READ-LOCK start
  trx.lockRead();

  const TRI_voc_size_t s = document->size(document);

  trx.finish(res);
  // READ-LOCK end

  return scope.Close(v8::Number::New((double) s));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns information about the datafiles
/// `collection.datafiles()`
///
/// Returns information about the datafiles. The collection must be unloaded.
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_DatafilesVocbaseCol (v8::Arguments const& argv) {
  v8::HandleScope scope;

  TRI_vocbase_col_t* collection = TRI_UnwrapClass<TRI_vocbase_col_t>(argv.Holder(), WRP_VOCBASE_COL_TYPE);

  if (collection == nullptr) {
    TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract collection");
  }

  TRI_SHARDING_COLLECTION_NOT_YET_IMPLEMENTED(scope, collection);

  TRI_READ_LOCK_STATUS_VOCBASE_COL(collection);

  if (collection->_status != TRI_VOC_COL_STATUS_UNLOADED &&
      collection->_status != TRI_VOC_COL_STATUS_CORRUPTED) {
    TRI_READ_UNLOCK_STATUS_VOCBASE_COL(collection);
    TRI_V8_EXCEPTION(scope, TRI_ERROR_ARANGO_COLLECTION_NOT_UNLOADED);
  }

  TRI_col_file_structure_t structure = TRI_FileStructureCollectionDirectory(collection->_path);

  // release lock
  TRI_READ_UNLOCK_STATUS_VOCBASE_COL(collection);

  // build result
  v8::Handle<v8::Object> result = v8::Object::New();

  // journals
  v8::Handle<v8::Array> journals = v8::Array::New();
  result->Set(v8::String::New("journals"), journals);

  for (size_t i = 0;  i < structure._journals._length;  ++i) {
    journals->Set((uint32_t) i, v8::String::New(structure._journals._buffer[i]));
  }

  // compactors
  v8::Handle<v8::Array> compactors = v8::Array::New();
  result->Set(v8::String::New("compactors"), compactors);

  for (size_t i = 0;  i < structure._compactors._length;  ++i) {
    compactors->Set((uint32_t) i, v8::String::New(structure._compactors._buffer[i]));
  }

  // datafiles
  v8::Handle<v8::Array> datafiles = v8::Array::New();
  result->Set(v8::String::New("datafiles"), datafiles);

  for (size_t i = 0;  i < structure._datafiles._length;  ++i) {
    datafiles->Set((uint32_t) i, v8::String::New(structure._datafiles._buffer[i]));
  }

  // free result
  TRI_DestroyFileStructureCollection(&structure);

  return scope.Close(result);
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

static v8::Handle<v8::Value> JS_DatafileScanVocbaseCol (v8::Arguments const& argv) {
  v8::HandleScope scope;

  TRI_vocbase_col_t* collection = TRI_UnwrapClass<TRI_vocbase_col_t>(argv.Holder(), WRP_VOCBASE_COL_TYPE);

  if (collection == nullptr) {
    TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract collection");
  }

  if (argv.Length() != 1) {
    TRI_V8_EXCEPTION_USAGE(scope, "datafileScan(<path>)");
  }

  string path = TRI_ObjectToString(argv[0]);

  TRI_READ_LOCK_STATUS_VOCBASE_COL(collection);

  if (collection->_status != TRI_VOC_COL_STATUS_UNLOADED &&
      collection->_status != TRI_VOC_COL_STATUS_CORRUPTED) {
    TRI_READ_UNLOCK_STATUS_VOCBASE_COL(collection);
    TRI_V8_EXCEPTION(scope, TRI_ERROR_ARANGO_COLLECTION_NOT_UNLOADED);
  }

  TRI_df_scan_t scan = TRI_ScanDatafile(path.c_str());

  // build result
  v8::Handle<v8::Object> result = v8::Object::New();

  result->Set(v8::String::New("currentSize"), v8::Number::New(scan._currentSize));
  result->Set(v8::String::New("maximalSize"), v8::Number::New(scan._maximalSize));
  result->Set(v8::String::New("endPosition"), v8::Number::New(scan._endPosition));
  result->Set(v8::String::New("numberMarkers"), v8::Number::New(scan._numberMarkers));
  result->Set(v8::String::New("status"), v8::Number::New(scan._status));
  result->Set(v8::String::New("isSealed"), v8::Boolean::New(scan._isSealed));

  v8::Handle<v8::Array> entries = v8::Array::New();
  result->Set(v8::String::New("entries"), entries);

  for (size_t i = 0;  i < scan._entries._length;  ++i) {
    TRI_df_scan_entry_t* entry = (TRI_df_scan_entry_t*) TRI_AtVector(&scan._entries, i);

    v8::Handle<v8::Object> o = v8::Object::New();

    o->Set(v8::String::New("position"), v8::Number::New(entry->_position));
    o->Set(v8::String::New("size"), v8::Number::New(entry->_size));
    o->Set(v8::String::New("realSize"), v8::Number::New(entry->_realSize));
    o->Set(v8::String::New("tick"), V8TickId(entry->_tick));
    o->Set(v8::String::New("type"), v8::Number::New((int) entry->_type));
    o->Set(v8::String::New("status"), v8::Number::New((int) entry->_status));

    entries->Set((uint32_t) i, o);
  }

  TRI_DestroyDatafileScan(&scan);

  TRI_READ_UNLOCK_STATUS_VOCBASE_COL(collection);
  return scope.Close(result);
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

  TRI_AddMethodVocbase(ArangoDBNS, "_changeMode", JS_ChangeOperationModeVocbase);
  TRI_AddMethodVocbase(ArangoDBNS, "_collection", JS_CollectionVocbase);
  TRI_AddMethodVocbase(ArangoDBNS, "_collections", JS_CollectionsVocbase);
  TRI_AddMethodVocbase(ArangoDBNS, "_COMPLETIONS", JS_CompletionsVocbase, true);
  TRI_AddMethodVocbase(ArangoDBNS, "_document", JS_DocumentVocbase);
  TRI_AddMethodVocbase(ArangoDBNS, "_exists", JS_ExistsVocbase);
  TRI_AddMethodVocbase(ArangoDBNS, "_remove", JS_RemoveVocbase);
  TRI_AddMethodVocbase(ArangoDBNS, "_replace", JS_ReplaceVocbase);
  TRI_AddMethodVocbase(ArangoDBNS, "_update", JS_UpdateVocbase);
  TRI_AddMethodVocbase(ArangoDBNS, "_version", JS_VersionServer);

  v8::Handle<v8::ObjectTemplate> rt;
  v8::Handle<v8::FunctionTemplate> ft;

  ft = v8::FunctionTemplate::New();
  ft->SetClassName(TRI_V8_SYMBOL("ArangoCollection"));

  rt = ft->InstanceTemplate();
  rt->SetInternalFieldCount(3);


#ifdef TRI_ENABLE_MAINTAINER_MODE
  TRI_AddMethodVocbase(rt, "checkPointers", JS_CheckPointersVocbaseCol);
#endif  
  TRI_AddMethodVocbase(rt, "count", JS_CountVocbaseCol);
  TRI_AddMethodVocbase(rt, "datafiles", JS_DatafilesVocbaseCol);
  TRI_AddMethodVocbase(rt, "datafileScan", JS_DatafileScanVocbaseCol);
  TRI_AddMethodVocbase(rt, "document", JS_DocumentVocbaseCol);
  TRI_AddMethodVocbase(rt, "drop", JS_DropVocbaseCol);
  TRI_AddMethodVocbase(rt, "exists", JS_ExistsVocbaseCol);
  TRI_AddMethodVocbase(rt, "figures", JS_FiguresVocbaseCol);
  TRI_AddMethodVocbase(rt, "insert", JS_InsertVocbaseCol);
  TRI_AddMethodVocbase(rt, "load", JS_LoadVocbaseCol);
  TRI_AddMethodVocbase(rt, "name", JS_NameVocbaseCol);
  TRI_AddMethodVocbase(rt, "planId", JS_PlanIdVocbaseCol);
  TRI_AddMethodVocbase(rt, "properties", JS_PropertiesVocbaseCol);
  TRI_AddMethodVocbase(rt, "remove", JS_RemoveVocbaseCol);
  TRI_AddMethodVocbase(rt, "revision", JS_RevisionVocbaseCol);
  TRI_AddMethodVocbase(rt, "rename", JS_RenameVocbaseCol);
  TRI_AddMethodVocbase(rt, "replace", JS_ReplaceVocbaseCol);
  TRI_AddMethodVocbase(rt, "rotate", JS_RotateVocbaseCol);
  TRI_AddMethodVocbase(rt, "save", JS_InsertVocbaseCol); // note: save is now an alias for insert
  TRI_AddMethodVocbase(rt, "status", JS_StatusVocbaseCol);
  TRI_AddMethodVocbase(rt, "TRUNCATE", JS_TruncateVocbaseCol, true);
  TRI_AddMethodVocbase(rt, "truncateDatafile", JS_TruncateDatafileVocbaseCol);
  TRI_AddMethodVocbase(rt, "type", JS_TypeVocbaseCol);
  TRI_AddMethodVocbase(rt, "unload", JS_UnloadVocbaseCol);
  TRI_AddMethodVocbase(rt, "update", JS_UpdateVocbaseCol);
  TRI_AddMethodVocbase(rt, "version", JS_VersionVocbaseCol);

  TRI_InitV8indexCollection(context, server, vocbase, loader, threadNumber, v8g, rt);

  v8g->VocbaseColTempl = v8::Persistent<v8::ObjectTemplate>::New(isolate, rt);
  TRI_AddGlobalFunctionVocbase(context, "ArangoCollection", ft->GetFunction());
}
