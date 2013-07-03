////////////////////////////////////////////////////////////////////////////////
/// @brief V8-vocbase bridge
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
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
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "v8-vocbase.h"

#include "build.h"

#include "Logger/Logger.h"
#include "Ahuacatl/ahuacatl-codegen.h"
#include "Ahuacatl/ahuacatl-collections.h"
#include "Ahuacatl/ahuacatl-context.h"
#include "Ahuacatl/ahuacatl-explain.h"
#include "Ahuacatl/ahuacatl-result.h"
#include "Basics/StringUtils.h"
#include "Basics/Utf8Helper.h"
#include "BasicsC/conversions.h"
#include "BasicsC/files.h"
#include "BasicsC/json.h"
#include "BasicsC/json-utilities.h"
#include "BasicsC/logging.h"
#include "BasicsC/tri-strings.h"
#include "FulltextIndex/fulltext-index.h"
#include "ShapedJson/shape-accessor.h"
#include "ShapedJson/shaped-json.h"
#include "Utils/AhuacatlGuard.h"
#include "Utils/AhuacatlTransaction.h"
#include "Utils/CollectionNameResolver.h"
#include "Utils/DocumentHelper.h"
#include "Utils/EmbeddableTransaction.h"
#include "Utils/ExplicitTransaction.h"
#include "Utils/SingleCollectionReadOnlyTransaction.h"
#include "Utils/SingleCollectionWriteTransaction.h"
#include "Utils/StandaloneTransaction.h"
#include "Utils/V8TransactionContext.h"
#include "Utilities/ResourceHolder.h"
#include "V8/v8-conv.h"
#include "V8/v8-execution.h"
#include "V8/v8-utils.h"
#include "VocBase/auth.h"
#include "VocBase/datafile.h"
#include "VocBase/general-cursor.h"
#include "VocBase/document-collection.h"
#include "VocBase/edge-collection.h"
#include "VocBase/key-generator.h"
#include "VocBase/voc-shaper.h"
#include "v8.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::arango;


// -----------------------------------------------------------------------------
// --SECTION--                                              forward declarations
// -----------------------------------------------------------------------------

static v8::Handle<v8::Value> WrapGeneralCursor (void* cursor);

// -----------------------------------------------------------------------------
// --SECTION--                                                   private defines
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief shortcut for read-only transaction class type
////////////////////////////////////////////////////////////////////////////////

#define ReadTransactionType SingleCollectionReadOnlyTransaction<EmbeddableTransaction<V8TransactionContext> >

////////////////////////////////////////////////////////////////////////////////
/// @brief macro to make sure we won't continue if we are inside a transaction
////////////////////////////////////////////////////////////////////////////////

#define PREVENT_EMBEDDED_TRANSACTION(scope)                               \
  if (V8TransactionContext::isEmbedded()) {                               \
    TRI_V8_EXCEPTION(scope, TRI_ERROR_TRANSACTION_DISALLOWED_OPERATION);  \
  }

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private constants
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief slot for a "barrier"
////////////////////////////////////////////////////////////////////////////////

static int const SLOT_BARRIER = 2;

////////////////////////////////////////////////////////////////////////////////
/// @brief wrapped class for TRI_vocbase_t
///
/// Layout:
/// - SLOT_CLASS_TYPE
/// - SLOT_CLASS
////////////////////////////////////////////////////////////////////////////////

static int32_t const WRP_VOCBASE_TYPE = 1;

////////////////////////////////////////////////////////////////////////////////
/// @brief wrapped class for TRI_vocbase_col_t
///
/// Layout:
/// - SLOT_CLASS_TYPE
/// - SLOT_CLASS
////////////////////////////////////////////////////////////////////////////////

static int32_t const WRP_VOCBASE_COL_TYPE = 2;

////////////////////////////////////////////////////////////////////////////////
/// @brief wrapped class for general cursors
///
/// Layout:
/// - SLOT_CLASS_TYPE
/// - SLOT_CLASS
////////////////////////////////////////////////////////////////////////////////

static int32_t const WRP_GENERAL_CURSOR_TYPE = 3;

////////////////////////////////////////////////////////////////////////////////
/// @brief wrapped class for TRI_shaped_json_t
///
/// Layout:
/// - SLOT_CLASS_TYPE
/// - SLOT_CLASS
/// - SLOT_BARRIER
////////////////////////////////////////////////////////////////////////////////

static int32_t const WRP_SHAPED_JSON_TYPE = 4;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  HELPER FUNCTIONS
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief create a v8 string value from an internal uint64_t id value
////////////////////////////////////////////////////////////////////////////////

static inline v8::Handle<v8::Value> V8StringId (const uint64_t id) {
  v8::HandleScope scope;

  const string idStr = StringUtils::itoa(id);

  v8::Handle<v8::Value> result = v8::String::New(idStr.c_str(), idStr.size());

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a v8 collection id value from the internal collection id
////////////////////////////////////////////////////////////////////////////////

static inline v8::Handle<v8::Value> V8CollectionId (const uint64_t cid) {
  return V8StringId(cid);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a v8 revision id value from the internal revision id
////////////////////////////////////////////////////////////////////////////////

static inline v8::Handle<v8::Value> V8RevisionId (const uint64_t rid) {
  return V8StringId(rid);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a v8 document id value from the parameters
////////////////////////////////////////////////////////////////////////////////

static inline v8::Handle<v8::Value> V8DocumentId (const string& collectionName,
                                                  const string& key) {
  v8::HandleScope scope;

  const string id = DocumentHelper::assembleDocumentId(collectionName, key);

  v8::Handle<v8::Value> result = v8::String::New(id.c_str(), id.size());

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief validate an attribute name
////////////////////////////////////////////////////////////////////////////////

static bool ValidateAttributeName (const char* attributeName,
                                   const bool allowInternal) {
  if (attributeName == 0 || *attributeName == '\0') {
    // empty name => fail
    return false;
  }

  if (! allowInternal && *attributeName == '_') {
    // internal attributes (_from, _to, _id, _key, ...) cannot be indexed => fail
    return false;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extract the forceSync flag from the arguments
/// must specify the argument index starting from 1
////////////////////////////////////////////////////////////////////////////////

static bool ExtractForceSync (v8::Arguments const& argv,
                              const int index) {
  assert(index > 0);

  const bool forceSync = (argv.Length() >= index && TRI_ObjectToBoolean(argv[index - 1]));
  return forceSync;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extract the update policy from the arguments
/// must specify the argument index starting from 1
////////////////////////////////////////////////////////////////////////////////

static TRI_doc_update_policy_e ExtractUpdatePolicy (v8::Arguments const& argv,
                                                    const int index) {
  assert(index > 0);

  // default value
  TRI_doc_update_policy_e policy = TRI_DOC_UPDATE_ERROR;

  if (argv.Length() >= index) {
    if (TRI_ObjectToBoolean(argv[index - 1])) {
      // overwrite!
      policy = TRI_DOC_UPDATE_LAST_WRITE;
    }
    else {
      policy = TRI_DOC_UPDATE_CONFLICT;
    }
  }

  return policy;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief wraps a C++ into a v8::Object
////////////////////////////////////////////////////////////////////////////////

template<class T>
static v8::Handle<v8::Object> WrapClass (v8::Persistent<v8::ObjectTemplate> classTempl, int32_t type, T* y) {

  // handle scope for temporary handles
  v8::HandleScope scope;

  // create the new handle to return, and set its template type
  v8::Handle<v8::Object> result = classTempl->NewInstance();

  if (result.IsEmpty()) {
    // error
    // TODO check for empty results
    return scope.Close(result);
  }

  // set the c++ pointer for unwrapping later
  result->SetInternalField(SLOT_CLASS_TYPE, v8::Integer::New(type));
  result->SetInternalField(SLOT_CLASS, v8::External::New(y));

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the vocbase pointer from the current V8 context
////////////////////////////////////////////////////////////////////////////////

static inline TRI_vocbase_t* GetContextVocBase () {
  TRI_v8_global_t* v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();

  assert(v8g->_vocbase != 0);
  TRI_vocbase_t* vocbase = static_cast<TRI_vocbase_t*>(v8g->_vocbase);

  return vocbase;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if argument is a document identifier
////////////////////////////////////////////////////////////////////////////////

static bool ParseDocumentHandle (v8::Handle<v8::Value> arg,
                                 string& collectionName,
                                 TRI_voc_key_t& key) {
  assert(collectionName == "");

  if (! arg->IsString()) {
    return false;
  }

  // string handle
  TRI_Utf8ValueNFC str(TRI_UNKNOWN_MEM_ZONE, arg);
  char const* s = *str;

  if (s == 0) {
    return false;
  }

  TRI_v8_global_t* v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();
  regmatch_t matches[3];

  // collection name / document key
  if (regexec(&v8g->DocumentIdRegex, s, sizeof(matches) / sizeof(matches[0]), matches, 0) == 0) {
    collectionName = string(s + matches[1].rm_so, matches[1].rm_eo - matches[1].rm_so);
    key = TRI_DuplicateString2Z(TRI_CORE_MEM_ZONE, s + matches[2].rm_so, matches[2].rm_eo - matches[2].rm_so);
    return true;
  }

  // document key only
  if (regexec(&v8g->DocumentKeyRegex, s, 0, NULL, 0) == 0) {
    key = TRI_DuplicateString2Z(TRI_CORE_MEM_ZONE, *str, str.length());
    return true;
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts a document key from a document
////////////////////////////////////////////////////////////////////////////////

static int ExtractDocumentKey (v8::Handle<v8::Value> arg,
                               TRI_voc_key_t& key) {
  TRI_v8_global_t* v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();
  key = 0;

  if (arg->IsObject() && ! arg->IsArray()) {
    v8::Handle<v8::Object> obj = arg->ToObject();

    if (obj->Has(v8g->_KeyKey)) {
      v8::Handle<v8::Value> v = obj->Get(v8g->_KeyKey);

      if (v->IsString()) {
        // string key
        TRI_Utf8ValueNFC str(TRI_CORE_MEM_ZONE, v);
        key = TRI_DuplicateString2(*str, str.length());

        return TRI_ERROR_NO_ERROR;
      }
      else {
        return TRI_ERROR_ARANGO_DOCUMENT_KEY_BAD;
      }
    }
    else {
      return TRI_ERROR_ARANGO_DOCUMENT_KEY_MISSING;
    }
  }
  else {
    // anything else than an object will be rejected
    return TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if argument is an index identifier
////////////////////////////////////////////////////////////////////////////////

static bool IsIndexHandle (v8::Handle<v8::Value> arg,
                           string& collectionName,
                           TRI_idx_iid_t& iid) {

  assert(collectionName == "");
  assert(iid == 0);

  if (arg->IsNumber()) {
    iid = (TRI_idx_iid_t) arg->ToNumber()->Value();
    return true;
  }

  if (! arg->IsString()) {
    return false;
  }

  TRI_Utf8ValueNFC str(TRI_UNKNOWN_MEM_ZONE, arg);
  char const* s = *str;

  if (s == 0) {
    return false;
  }

  TRI_v8_global_t* v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();
  regmatch_t matches[3];

  if (regexec(&v8g->IndexIdRegex, s, sizeof(matches) / sizeof(matches[0]), matches, 0) == 0) {
    collectionName = string(s + matches[1].rm_so, matches[1].rm_eo - matches[1].rm_so);
    iid = TRI_UInt64String2(s + matches[2].rm_so, matches[2].rm_eo - matches[2].rm_so);
    return true;
  }

  if (regexec(&v8g->IdRegex, s, sizeof(matches) / sizeof(matches[0]), matches, 0) == 0) {
    iid = TRI_UInt64String2(s + matches[1].rm_so, matches[1].rm_eo - matches[1].rm_so);
    return true;
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief loads a collection for usage
////////////////////////////////////////////////////////////////////////////////

static TRI_vocbase_col_t const* UseCollection (v8::Handle<v8::Object> collection,
                                               v8::Handle<v8::Object>* err) {

  TRI_vocbase_col_t* col = TRI_UnwrapClass<TRI_vocbase_col_t>(collection, WRP_VOCBASE_COL_TYPE);

  int res = TRI_UseCollectionVocBase(col->_vocbase, col);

  if (res != TRI_ERROR_NO_ERROR) {
    *err = TRI_CreateErrorObject(res, "cannot use/load collection", true);
    return 0;
  }

  if (col->_collection == 0) {
    TRI_set_errno(TRI_ERROR_INTERNAL);
    *err = TRI_CreateErrorObject(TRI_ERROR_INTERNAL, "cannot use/load collection", true);
    return 0;
  }

  return col;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief releases a collection
////////////////////////////////////////////////////////////////////////////////

static void ReleaseCollection (TRI_vocbase_col_t const* collection) {
  TRI_ReleaseCollectionVocBase(collection->_vocbase, const_cast<TRI_vocbase_col_t*>(collection));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the index representation
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> IndexRep (TRI_collection_t* col, TRI_json_t* idx) {
  v8::HandleScope scope;

  assert(idx);
  assert(col);

  v8::Handle<v8::Object> rep = TRI_ObjectJson(idx)->ToObject();

  string iid = TRI_ObjectToString(rep->Get(TRI_V8_SYMBOL("id")));
  const string id = string(col->_info._name) + TRI_INDEX_HANDLE_SEPARATOR_STR + iid;
  rep->Set(TRI_V8_SYMBOL("id"), v8::String::New(id.c_str(), id.size()));

  return scope.Close(rep);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts argument strings to TRI_vector_pointer_t
////////////////////////////////////////////////////////////////////////////////

static int AttributeNamesFromArguments (v8::Arguments const& argv,
                                        TRI_vector_pointer_t* result,
                                        size_t start,
                                        size_t end,
                                        string& error) {

  // ...........................................................................
  // convert the arguments into a "C" string and stuff them into a vector
  // ...........................................................................

  for (int j = start;  j < argv.Length();  ++j) {
    v8::Handle<v8::Value> argument = argv[j];

    if (! argument->IsString() ) {
      error = "invalid attribute name";

      TRI_FreeContentVectorPointer(TRI_CORE_MEM_ZONE, result);
      return TRI_set_errno(TRI_ERROR_BAD_PARAMETER);
    }

    TRI_Utf8ValueNFC argumentString(TRI_UNKNOWN_MEM_ZONE, argument);
    char* cArgument = *argumentString == 0 ? 0 : TRI_DuplicateString(*argumentString);

    if (cArgument == 0) {
      error = "out of memory";

      TRI_FreeContentVectorPointer(TRI_CORE_MEM_ZONE, result);
      return TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
    }

    TRI_ASSERT_MAINTAINER(cArgument != 0);

    // cannot index internal attributes such as _key, _rev, _id, _from, _to...
    if (! ValidateAttributeName(cArgument, false)) {
      error = "invalid attribute name";

      TRI_Free(TRI_CORE_MEM_ZONE, cArgument);
      TRI_FreeContentVectorPointer(TRI_CORE_MEM_ZONE, result);
      return TRI_set_errno(TRI_ERROR_BAD_PARAMETER);
    }

    TRI_PushBackVectorPointer(result, cArgument);
  }

  // .............................................................................
  // check that each parameter is unique
  // .............................................................................

  for (size_t j = 0;  j < result->_length;  ++j) {
    char* left = (char*) result->_buffer[j];

    for (size_t k = j + 1;  k < result->_length;  ++k) {
      char* right = (char*) result->_buffer[k];

      if (TRI_EqualString(left, right)) {
        error = "duplicate attribute names";

        TRI_FreeContentVectorPointer(TRI_CORE_MEM_ZONE, result);
        return TRI_set_errno(TRI_ERROR_BAD_PARAMETER);
      }
    }

  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ensure a hash or skip-list index
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> EnsurePathIndex (string const& cmd,
                                              v8::Arguments const& argv,
                                              bool unique,
                                              bool create,
                                              TRI_idx_type_e type) {
  v8::HandleScope scope;
  
  if (create) {
    PREVENT_EMBEDDED_TRANSACTION(scope);
  }

  // .............................................................................
  // Check that we have a valid collection
  // .............................................................................

  v8::Handle<v8::Object> err;
  TRI_vocbase_col_t const* collection = UseCollection(argv.Holder(), &err);

  if (collection == 0) {
    return scope.Close(v8::ThrowException(err));
  }

  // .............................................................................
  // Check collection type
  // .............................................................................

  TRI_primary_collection_t* primary = collection->_collection;

  if (! TRI_IS_DOCUMENT_COLLECTION(collection->_type)) {
    ReleaseCollection(collection);
    TRI_V8_EXCEPTION_INTERNAL(scope, "unknown collection type");
  }

  // .............................................................................
  // Ensure that there is at least one string parameter sent to this method
  // .............................................................................

  if (argv.Length() == 0) {
    ReleaseCollection(collection);
    string msg = cmd + "(<path>, ...)";
    TRI_V8_EXCEPTION_USAGE(scope, msg.c_str());
  }

  // .............................................................................
  // Create a list of paths, these will be used to create a list of shapes
  // which will be used by the hash index.
  // .............................................................................

  string errorString;

  TRI_vector_pointer_t attributes;
  TRI_InitVectorPointer(&attributes, TRI_CORE_MEM_ZONE);

  int res = AttributeNamesFromArguments(argv, &attributes, 0, argv.Length(), errorString);

  // .............................................................................
  // Some sort of error occurred -- display error message and abort index creation
  // (or index retrieval).
  // .............................................................................

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_DestroyVectorPointer(&attributes);

    ReleaseCollection(collection);
    TRI_V8_EXCEPTION_MESSAGE(scope, res, errorString);
  }
  
  // .............................................................................
  // Actually create the index here
  // .............................................................................

  bool created;
  TRI_index_t* idx;
  TRI_document_collection_t* document = (TRI_document_collection_t*) primary;

  if (type == TRI_IDX_TYPE_HASH_INDEX) {
    if (create) {
      idx = TRI_EnsureHashIndexDocumentCollection(document, &attributes, unique, &created);

      if (idx == 0) {
        res = TRI_errno();
      }
    }
    else {
      idx = TRI_LookupHashIndexDocumentCollection(document, &attributes, unique);
    }
  }
  else if (type == TRI_IDX_TYPE_SKIPLIST_INDEX) {
    if (create) {
      idx = TRI_EnsureSkiplistIndexDocumentCollection(document, &attributes, unique, &created);

      if (idx == 0) {
        res = TRI_errno();
      }
    }
    else {
      idx = TRI_LookupSkiplistIndexDocumentCollection(document, &attributes, unique);
    }
  }
  else {
    LOG_ERROR("unknown index type %d", (int) type);
    res = TRI_ERROR_INTERNAL;
    idx = 0;
  }

  // .............................................................................
  // remove the memory allocated to the list of attributes used for the hash index
  // .............................................................................

  TRI_FreeContentVectorPointer(TRI_CORE_MEM_ZONE, &attributes);
  TRI_DestroyVectorPointer(&attributes);

  if (idx == 0) {
    ReleaseCollection(collection);
    if (create) {
      TRI_V8_EXCEPTION_MESSAGE(scope, res, "index could not be created");
    }
    else {
      return scope.Close(v8::Null());
    }
  }

  // .............................................................................
  // return the newly assigned index identifier
  // .............................................................................

  TRI_json_t* json = idx->json(idx, primary);

  if (! json) {
    ReleaseCollection(collection);
    TRI_V8_EXCEPTION_MEMORY(scope);
  }

  v8::Handle<v8::Value> index = IndexRep(&primary->base, json);
  TRI_FreeJson(TRI_CORE_MEM_ZONE, json);

  if (create) {
    if (index->IsObject()) {
      index->ToObject()->Set(v8::String::New("isNewlyCreated"), created ? v8::True() : v8::False());
    }
  }

  ReleaseCollection(collection);
  return scope.Close(index);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a fulltext index
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> EnsureFulltextIndex (v8::Arguments const& argv,
                                                  const bool create) {
  v8::HandleScope scope;

  if (argv.Length() < 1 || argv.Length() > 2) {
    TRI_V8_EXCEPTION_USAGE(scope, "ensureFulltextIndex(<attribute>, <minLength>)");
  }
  
  PREVENT_EMBEDDED_TRANSACTION(scope);  

  string attributeName = TRI_ObjectToString(argv[0]);

  if (! ValidateAttributeName(attributeName.c_str(), false)) {
    TRI_V8_TYPE_ERROR(scope, "invalid index attribute name");
  }

  // 2013-01-17: deactivated substring indexing option because there is no working implementation
  // we might activate the option later
  bool indexSubstrings = false;

  int minWordLength = TRI_FULLTEXT_MIN_WORD_LENGTH_DEFAULT;
  if (argv.Length() == 2 && argv[1]->IsNumber()) {
    minWordLength = (int) TRI_ObjectToInt64(argv[1]);
  }

  // .............................................................................
  // Check that we have a valid collection
  // .............................................................................

  v8::Handle<v8::Object> err;
  TRI_vocbase_col_t const* collection = UseCollection(argv.Holder(), &err);

  if (collection == 0) {
    return scope.Close(v8::ThrowException(err));
  }

  // .............................................................................
  // Check collection type
  // .............................................................................

  TRI_primary_collection_t* primary = collection->_collection;

  if (! TRI_IS_DOCUMENT_COLLECTION(collection->_type)) {
    ReleaseCollection(collection);
    TRI_V8_EXCEPTION_INTERNAL(scope, "unknown collection type");
  }

  // .............................................................................
  // Actually create the index here
  // .............................................................................

  int res = TRI_ERROR_NO_ERROR;
  bool created;
  TRI_index_t* idx;

  TRI_document_collection_t* document = (TRI_document_collection_t*) primary;

  if (create) {
    idx = TRI_EnsureFulltextIndexDocumentCollection(document, attributeName.c_str(), indexSubstrings, minWordLength, &created);

    if (idx == 0) {
      res = TRI_errno();
    }
  }
  else {
    idx = TRI_LookupFulltextIndexDocumentCollection(document, attributeName.c_str(), indexSubstrings, minWordLength);
  }

  if (idx == 0) {
    ReleaseCollection(collection);

    if (create) {
      TRI_V8_EXCEPTION_MESSAGE(scope, res, "index could not be created");
    }
    else {
      return scope.Close(v8::Null());
    }
  }

  // .............................................................................
  // return the newly assigned index identifier
  // .............................................................................

  TRI_json_t* json = idx->json(idx, primary);

  if (! json) {
    ReleaseCollection(collection);
    TRI_V8_EXCEPTION_MEMORY(scope);
  }

  v8::Handle<v8::Value> index = IndexRep(&primary->base, json);
  TRI_FreeJson(TRI_CORE_MEM_ZONE, json);

  if (create) {
    if (index->IsObject()) {
      index->ToObject()->Set(v8::String::New("isNewlyCreated"), created ? v8::True() : v8::False());
    }
  }

  ReleaseCollection(collection);
  return scope.Close(index);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a document
///
/// it is the caller's responsibility to acquire and release the required locks
/// the collection must also have the correct status already. don't use this
/// function if you're unsure about it!
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> DocumentVocbaseCol (const bool useCollection,
                                                 v8::Arguments const& argv) {
  v8::HandleScope scope;

  // first and only argument should be a document idenfifier
  if (argv.Length() != 1) {
    TRI_V8_EXCEPTION_USAGE(scope, "document(<document-handle>)");
  }

  ResourceHolder holder;

  TRI_voc_key_t key = 0;
  TRI_voc_rid_t rid;
  TRI_vocbase_t* vocbase;
  TRI_vocbase_col_t const* col = 0;

  if (useCollection) {
    // called as db.collection.document()
    col = TRI_UnwrapClass<TRI_vocbase_col_t>(argv.Holder(), WRP_VOCBASE_COL_TYPE);

    if (col == 0) {
      TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract collection");
    }

    vocbase = col->_vocbase;
  }
  else {
    // called as db._document()
    vocbase = TRI_UnwrapClass<TRI_vocbase_t>(argv.Holder(), WRP_VOCBASE_TYPE);
  }

  assert(vocbase);

  CollectionNameResolver resolver(vocbase);
  v8::Handle<v8::Value> err = TRI_ParseDocumentOrDocumentHandle(resolver, col, key, rid, argv[0]);

  if (! holder.registerString(TRI_CORE_MEM_ZONE, key)) {
    TRI_V8_EXCEPTION(scope, TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD);
  }

  if (! err.IsEmpty()) {
    return scope.Close(v8::ThrowException(err));
  }

  assert(col);
  assert(key);

  ReadTransactionType trx(vocbase, resolver, col->_cid);

  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_EXCEPTION_MESSAGE(scope, res, "cannot fetch document");;
  }

  TRI_barrier_t* barrier = TRI_CreateBarrierElement(&(trx.primaryCollection()->_barrierList));

  if (barrier == 0) {
    TRI_V8_EXCEPTION_MEMORY(scope);
  }

  assert(barrier != 0);

  bool freeBarrier = true;

  v8::Handle<v8::Value> result;
  TRI_doc_mptr_t document;
  res = trx.read(&document, key);

  if (res == TRI_ERROR_NO_ERROR) {
    result = TRI_WrapShapedJson<ReadTransactionType >(trx, col->_cid, &document, barrier);

    if (! result.IsEmpty()) {
      freeBarrier = false;
    }
  }

  res = trx.finish(res);

  if (res != TRI_ERROR_NO_ERROR || document._key == 0 || document._data == 0) {
    if (freeBarrier) {
      TRI_FreeBarrier(barrier);
    }

    if (res == TRI_ERROR_NO_ERROR) {
      res = TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND;
    }

    TRI_V8_EXCEPTION_MESSAGE(scope, res, "document not found");
  }

  if (rid != 0 && document._rid != rid) {
    if (freeBarrier) {
      TRI_FreeBarrier(barrier);
    }

    TRI_V8_EXCEPTION_MESSAGE(scope, TRI_ERROR_ARANGO_CONFLICT, "revision not found");
  }

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief replaces a document
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> ReplaceVocbaseCol (const bool useCollection,
                                                v8::Arguments const& argv) {
  v8::HandleScope scope;

  // check the arguments
  if (argv.Length() < 2) {
    TRI_V8_EXCEPTION_USAGE(scope, "replace(<document>, <data>, <overwrite>, <waitForSync>)");
  }

  const TRI_doc_update_policy_e policy = ExtractUpdatePolicy(argv, 3);
  const bool forceSync = ExtractForceSync(argv, 4);

  ResourceHolder holder;
  TRI_voc_key_t key = 0;
  TRI_voc_rid_t rid;
  TRI_voc_rid_t actualRevision = 0;

  TRI_vocbase_t* vocbase;
  TRI_vocbase_col_t const* col = 0;

  if (useCollection) {
    // called as db.collection.replace()
    col = TRI_UnwrapClass<TRI_vocbase_col_t>(argv.Holder(), WRP_VOCBASE_COL_TYPE);

    if (col == 0) {
      TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract collection");
    }

    vocbase = col->_vocbase;
  }
  else {
    // called as db._replace()
    vocbase = TRI_UnwrapClass<TRI_vocbase_t>(argv.Holder(), WRP_VOCBASE_TYPE);
  }

  assert(vocbase);

  CollectionNameResolver resolver(vocbase);
  v8::Handle<v8::Value> err = TRI_ParseDocumentOrDocumentHandle(resolver, col, key, rid, argv[0]);

  if (! holder.registerString(TRI_CORE_MEM_ZONE, key)) {
    TRI_V8_EXCEPTION(scope, TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD);
  }

  if (! err.IsEmpty()) {
    return scope.Close(v8::ThrowException(err));
  }

  assert(col);
  assert(key);

  SingleCollectionWriteTransaction<EmbeddableTransaction<V8TransactionContext>, 1> trx(vocbase, resolver, col->_cid);
  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_EXCEPTION_MESSAGE(scope, res, "cannot replace document");
  }
    
  // we're only accepting "real" object documents
  if (! argv[1]->IsObject() || argv[1]->IsArray()) {
    TRI_V8_EXCEPTION(scope, TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
  }

  TRI_primary_collection_t* primary = trx.primaryCollection();
  TRI_shaped_json_t* shaped = TRI_ShapedJsonV8Object(argv[1], primary->_shaper);

  if (! holder.registerShapedJson(primary->_shaper, shaped)) {
    TRI_V8_EXCEPTION_MESSAGE(scope, TRI_errno(), "<data> cannot be converted into JSON shape");
  }

  TRI_doc_mptr_t document;
  res = trx.updateDocument(key, &document, shaped, policy, forceSync, rid, &actualRevision);

  res = trx.finish(res);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_EXCEPTION_MESSAGE(scope, res, "cannot replace document");
  }

  assert(document._key);

  TRI_v8_global_t* v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();

  v8::Handle<v8::Object> result = v8::Object::New();
  result->Set(v8g->_IdKey, V8DocumentId(resolver.getCollectionName(col->_cid), document._key));
  result->Set(v8g->_RevKey, V8RevisionId(document._rid));
  result->Set(v8g->_OldRevKey, V8RevisionId(actualRevision));
  result->Set(v8g->_KeyKey, v8::String::New(document._key));

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief saves a document
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> SaveVocbaseCol (
    SingleCollectionWriteTransaction<EmbeddableTransaction<V8TransactionContext>, 1>* trx,
    TRI_vocbase_col_t* col,
    v8::Arguments const& argv,
    bool replace) {
  v8::HandleScope scope;

  if (argv.Length() < 1 || argv.Length() > 2) {
    if (replace) {
      TRI_V8_EXCEPTION_USAGE(scope, "saveOrReplace(<data>, [<waitForSync>])");
    }
    else {
      TRI_V8_EXCEPTION_USAGE(scope, "save(<data>, [<waitForSync>])");
    }
  }

  CollectionNameResolver resolver(col->_vocbase);
  ResourceHolder holder;

  // set document key
  TRI_voc_key_t key = 0;
  TRI_v8_global_t* v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();
  int res;

  if (argv[0]->IsObject()) {
    res = ExtractDocumentKey(argv[0]->ToObject(), key);

    if (res != TRI_ERROR_NO_ERROR && res != TRI_ERROR_ARANGO_DOCUMENT_KEY_MISSING) {
      TRI_V8_EXCEPTION(scope, res);
    }
    else if (key != 0) {
      holder.registerString(TRI_CORE_MEM_ZONE, key);
    }
  }
  else {
    TRI_V8_EXCEPTION(scope, TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
  }

  TRI_primary_collection_t* primary = trx->primaryCollection();
  TRI_shaped_json_t* shaped = TRI_ShapedJsonV8Object(argv[0], primary->_shaper);

  if (! holder.registerShapedJson(primary->_shaper, shaped)) {
    TRI_V8_EXCEPTION_MESSAGE(scope, TRI_errno(), "<data> cannot be converted into JSON shape");
  }

  const bool forceSync = ExtractForceSync(argv, 2);

  TRI_doc_mptr_t document;
  res = trx->createDocument(key, &document, shaped, forceSync);

  res = trx->finish(res);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_EXCEPTION_MESSAGE(scope, res, "cannot save document");
  }

  assert(document._key != 0);

  v8::Handle<v8::Object> result = v8::Object::New();
  result->Set(v8g->_IdKey, V8DocumentId(resolver.getCollectionName(col->_cid), document._key));
  result->Set(v8g->_RevKey, V8RevisionId(document._rid));
  result->Set(v8g->_KeyKey, v8::String::New(document._key));

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief saves a new edge document
///
/// @FUN{@FA{edge-collection}.save(@FA{from}, @FA{to}, @FA{document})}
///
/// Saves a new edge and returns the document-handle. @FA{from} and @FA{to}
/// must be documents or document references.
///
/// @FUN{@FA{edge-collection}.save(@FA{from}, @FA{to}, @FA{document}, @FA{waitForSync})}
///
/// The optional @FA{waitForSync} parameter can be used to force
/// synchronisation of the document creation operation to disk even in case
/// that the @LIT{waitForSync} flag had been disabled for the entire collection.
/// Thus, the @FA{waitForSync} parameter can be used to force synchronisation
/// of just specific operations. To use this, set the @FA{waitForSync} parameter
/// to @LIT{true}. If the @FA{waitForSync} parameter is not specified or set to
/// @LIT{false}, then the collection's default @LIT{waitForSync} behavior is
/// applied. The @FA{waitForSync} parameter cannot be used to disable
/// synchronisation for collections that have a default @LIT{waitForSync} value
/// of @LIT{true}.
///
/// @EXAMPLES
///
/// @TINYEXAMPLE{shell_create-edge,create an edge}
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> SaveEdgeCol (
    SingleCollectionWriteTransaction<EmbeddableTransaction<V8TransactionContext>, 1>* trx,
    TRI_vocbase_col_t* col,
    v8::Arguments const& argv,
    bool replace) {
  v8::HandleScope scope;
  TRI_v8_global_t* v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();

  if (argv.Length() < 3 || argv.Length() > 4) {
    if (replace) {
      TRI_V8_EXCEPTION_USAGE(scope, "saveOrReplace(<from>, <to>, <data>, [<waitForSync>])");
    }
    else {
      TRI_V8_EXCEPTION_USAGE(scope, "save(<from>, <to>, <data>, [<waitForSync>])");
    }
  }

  CollectionNameResolver resolver(col->_vocbase);
  ResourceHolder holder;

  // set document key
  TRI_voc_key_t key = 0;
  int res;

  if (argv[2]->IsObject() && ! argv[2]->IsArray()) {
    res = ExtractDocumentKey(argv[2]->ToObject(), key);

    if (res != TRI_ERROR_NO_ERROR && res != TRI_ERROR_ARANGO_DOCUMENT_KEY_MISSING) {
      TRI_V8_EXCEPTION(scope, res);
    }
    else if (key != 0) {
      holder.registerString(TRI_CORE_MEM_ZONE, key);
    }
  }
  else {
    TRI_V8_EXCEPTION(scope, TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
  }

  const bool forceSync = ExtractForceSync(argv, 4);

  TRI_document_edge_t edge;
  // the following values are defaults that will be overridden below
  edge._fromCid = trx->cid();
  edge._toCid = trx->cid();
  edge._fromKey = 0;
  edge._toKey = 0;

  v8::Handle<v8::Value> err;

  // extract from
  TRI_vocbase_col_t const* fromCollection = 0;
  TRI_voc_rid_t fromRid;

  err = TRI_ParseDocumentOrDocumentHandle(resolver, fromCollection, edge._fromKey, fromRid, argv[0]);
  holder.registerString(TRI_CORE_MEM_ZONE, edge._fromKey);

  if (! err.IsEmpty()) {
    return scope.Close(v8::ThrowException(err));
  }

  edge._fromCid = fromCollection->_cid;

  // extract to
  TRI_vocbase_col_t const* toCollection = 0;
  TRI_voc_rid_t toRid;

  err = TRI_ParseDocumentOrDocumentHandle(resolver, toCollection, edge._toKey, toRid, argv[1]);
  holder.registerString(TRI_CORE_MEM_ZONE, edge._toKey);

  if (! err.IsEmpty()) {
    return scope.Close(v8::ThrowException(err));
  }

  edge._toCid = toCollection->_cid;

  TRI_primary_collection_t* primary = trx->primaryCollection();

  // extract shaped data
  TRI_shaped_json_t* shaped = TRI_ShapedJsonV8Object(argv[2], primary->_shaper);

  if (! holder.registerShapedJson(primary->_shaper, shaped)) {
    TRI_V8_EXCEPTION_MESSAGE(scope, TRI_errno(), "<data> cannot be converted into JSON shape");
  }


  TRI_doc_mptr_t document;
  res = trx->createEdge(key, &document, shaped, forceSync, &edge);
  res = trx->finish(res);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_EXCEPTION_MESSAGE(scope, res, "cannot save edge");
  }

  assert(document._key != 0);

  v8::Handle<v8::Object> result = v8::Object::New();
  result->Set(v8g->_IdKey, V8DocumentId(resolver.getCollectionName(col->_cid), document._key));
  result->Set(v8g->_RevKey, V8RevisionId(document._rid));
  result->Set(v8g->_KeyKey, v8::String::New(document._key));

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief updates (patches) a document
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> UpdateVocbaseCol (const bool useCollection,
                                               v8::Arguments const& argv) {
  v8::HandleScope scope;

  // check the arguments
  if (argv.Length() < 2 || argv.Length() > 5) {
    TRI_V8_EXCEPTION_USAGE(scope, "update(<document>, <data>, <overwrite>, <keepnull>, <waitForSync>)");
  }

  const TRI_doc_update_policy_e policy = ExtractUpdatePolicy(argv, 3);
  // delete null attributes
  // default value: null values are saved as Null
  const bool nullMeansRemove = (argv.Length() >= 4 && ! TRI_ObjectToBoolean(argv[3]));
  const bool forceSync = ExtractForceSync(argv, 5);


  ResourceHolder holder;
  TRI_voc_key_t key = 0;
  TRI_voc_rid_t rid;
  TRI_voc_rid_t actualRevision = 0;
  TRI_vocbase_t* vocbase;
  TRI_vocbase_col_t const* col = 0;

  if (useCollection) {
    // called as db.collection.update()
    col = TRI_UnwrapClass<TRI_vocbase_col_t>(argv.Holder(), WRP_VOCBASE_COL_TYPE);

    if (col == 0) {
      TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract collection");
    }

    vocbase = col->_vocbase;
  }
  else {
    // called as db._update()
    vocbase = TRI_UnwrapClass<TRI_vocbase_t>(argv.Holder(), WRP_VOCBASE_TYPE);
  }

  assert(vocbase);

  CollectionNameResolver resolver(vocbase);
  v8::Handle<v8::Value> err = TRI_ParseDocumentOrDocumentHandle(resolver, col, key, rid, argv[0]);

  if (! holder.registerString(TRI_CORE_MEM_ZONE, key)) {
    TRI_V8_EXCEPTION(scope, TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD);
  }

  if (! err.IsEmpty()) {
    return scope.Close(v8::ThrowException(err));
  }

  assert(col);
  assert(key);

  if (! argv[1]->IsObject() || argv[1]->IsArray()) {
    // we're only accepting "real" object documents
    TRI_V8_EXCEPTION(scope, TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
  }

  TRI_json_t* json = TRI_ObjectToJson(argv[1]);

  if (! holder.registerJson(TRI_UNKNOWN_MEM_ZONE, json)) {
    TRI_V8_EXCEPTION_MESSAGE(scope, TRI_errno(), "<data> is no valid JSON");
  }


  SingleCollectionWriteTransaction<EmbeddableTransaction<V8TransactionContext>, 1> trx(vocbase, resolver, col->_cid);
  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_EXCEPTION_MESSAGE(scope, res, "cannot update document");
  }

  // we must use a write-lock that spans both the initial read and the update.
  // otherwise the operation is not atomic
  trx.lockWrite();

  TRI_doc_mptr_t document;
  res = trx.read(&document, key);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_EXCEPTION_MESSAGE(scope, res, "cannot update document");
  }

  TRI_primary_collection_t* primary = trx.primaryCollection();

  TRI_shaped_json_t shaped;
  TRI_EXTRACT_SHAPED_JSON_MARKER(shaped, document._data);
  TRI_json_t* old = TRI_JsonShapedJson(primary->_shaper, &shaped);

  if (! holder.registerJson(primary->_shaper->_memoryZone, old)) {
    TRI_V8_EXCEPTION_MEMORY(scope);
  }

  TRI_json_t* patchedJson = TRI_MergeJson(TRI_UNKNOWN_MEM_ZONE, old, json, nullMeansRemove);

  if (! holder.registerJson(TRI_UNKNOWN_MEM_ZONE, patchedJson)) {
    TRI_V8_EXCEPTION_MEMORY(scope);
  }

  res = trx.updateDocument(key, &document, patchedJson, policy, forceSync, rid, &actualRevision);
  res = trx.finish(res);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_EXCEPTION_MESSAGE(scope, res, "cannot update document");
  }

  assert(document._key);

  TRI_v8_global_t* v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();

  v8::Handle<v8::Object> result = v8::Object::New();
  result->Set(v8g->_IdKey, V8DocumentId(resolver.getCollectionName(col->_cid), document._key));
  result->Set(v8g->_RevKey, V8RevisionId(document._rid));
  result->Set(v8g->_OldRevKey, V8RevisionId(actualRevision));
  result->Set(v8g->_KeyKey, v8::String::New(document._key));

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes a document
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> RemoveVocbaseCol (const bool useCollection,
                                               v8::Arguments const& argv) {
  v8::HandleScope scope;

  // check the arguments
  if (argv.Length() < 1 || argv.Length() > 3) {
    TRI_V8_EXCEPTION_USAGE(scope, "remove(<document>, <overwrite>, <waitForSync>)");
  }

  const TRI_doc_update_policy_e policy = ExtractUpdatePolicy(argv, 2);
  const bool forceSync = ExtractForceSync(argv, 3);

  ResourceHolder holder;
  TRI_voc_key_t key = 0;
  TRI_voc_rid_t rid;
  TRI_voc_rid_t actualRevision = 0;
  TRI_vocbase_t* vocbase;
  TRI_vocbase_col_t const* col = 0;

  if (useCollection) {
    // called as db.collection.remove()
    col = TRI_UnwrapClass<TRI_vocbase_col_t>(argv.Holder(), WRP_VOCBASE_COL_TYPE);

    if (col == 0) {
      TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract collection");
    }

    vocbase = col->_vocbase;
  }
  else {
    // called as db._remove()
    vocbase = TRI_UnwrapClass<TRI_vocbase_t>(argv.Holder(), WRP_VOCBASE_TYPE);
  }

  assert(vocbase);

  CollectionNameResolver resolver(vocbase);
  v8::Handle<v8::Value> err = TRI_ParseDocumentOrDocumentHandle(resolver, col, key, rid, argv[0]);

  if (! holder.registerString(TRI_CORE_MEM_ZONE, key)) {
    TRI_V8_EXCEPTION(scope, TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD);
  }

  if (! err.IsEmpty()) {
    return scope.Close(v8::ThrowException(err));
  }

  assert(col);
  assert(key);

  SingleCollectionWriteTransaction<EmbeddableTransaction<V8TransactionContext>, 1> trx(vocbase, resolver, col->_cid);
  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_EXCEPTION_MESSAGE(scope, res, "cannot delete document");
  }

  res = trx.deleteDocument(key, policy, forceSync, rid, &actualRevision);
  res = trx.finish(res);

  if (res != TRI_ERROR_NO_ERROR) {
    if (res == TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND && policy == TRI_DOC_UPDATE_LAST_WRITE) {
      return scope.Close(v8::False());
    }
    else {
      TRI_V8_EXCEPTION_MESSAGE(scope, res, "cannot delete document");
    }
  }

  return scope.Close(v8::True());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a collection
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> CreateVocBase (v8::Arguments const& argv, TRI_col_type_e collectionType) {
  v8::HandleScope scope;

  TRI_vocbase_t* vocbase = TRI_UnwrapClass<TRI_vocbase_t>(argv.Holder(), WRP_VOCBASE_TYPE);

  if (vocbase == 0) {
    TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract vocbase");
  }

  // expecting at least one arguments
  if (argv.Length() < 1 || argv.Length() > 2) {
    TRI_V8_EXCEPTION_USAGE(scope, "_create(<name>, <properties>)");
  }

  PREVENT_EMBEDDED_TRANSACTION(scope);  

  // set default journal size
  TRI_voc_size_t effectiveSize = vocbase->_defaultMaximalSize;

  // extract the name
  string name = TRI_ObjectToString(argv[0]);

  // extract the parameters
  TRI_col_info_t parameter;

  if (2 <= argv.Length()) {
    if (! argv[1]->IsObject()) {
      TRI_V8_TYPE_ERROR(scope, "<properties> must be an object");
    }

    v8::Handle<v8::Object> p = argv[1]->ToObject();

    TRI_v8_global_t* v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();

    if (p->Has(v8g->JournalSizeKey)) {
      double s = TRI_ObjectToDouble(p->Get(v8g->JournalSizeKey));

      if (s < TRI_JOURNAL_MINIMAL_SIZE) {
        TRI_V8_EXCEPTION_PARAMETER(scope, "<properties>.journalSize is too small");
      }

      // overwrite journal size with user-specified value
      effectiveSize = (TRI_voc_size_t) s;
    }

    // get optional values
    TRI_json_t* keyOptions = 0;
    if (p->Has(v8g->KeyOptionsKey)) {
      keyOptions = TRI_ObjectToJson(p->Get(v8g->KeyOptionsKey));
    }

    TRI_InitCollectionInfo(vocbase, &parameter, name.c_str(), collectionType, effectiveSize, keyOptions);

    if (p->Has(v8g->WaitForSyncKey)) {
      parameter._waitForSync = TRI_ObjectToBoolean(p->Get(v8g->WaitForSyncKey));
    }

    if (p->Has(v8g->IsSystemKey)) {
      parameter._isSystem = TRI_ObjectToBoolean(p->Get(v8g->IsSystemKey));
    }

    if (p->Has(v8g->IsVolatileKey)) {
#ifdef TRI_HAVE_ANONYMOUS_MMAP
      parameter._isVolatile = TRI_ObjectToBoolean(p->Get(v8g->IsVolatileKey));
#else
      TRI_FreeCollectionInfoOptions(&parameter);
      TRI_V8_EXCEPTION_PARAMETER(scope, "volatile collections are not supported on this platform");
#endif
    }

    if (parameter._isVolatile && parameter._waitForSync) {
      // the combination of waitForSync and isVolatile makes no sense
      TRI_FreeCollectionInfoOptions(&parameter);
      TRI_V8_EXCEPTION_PARAMETER(scope, "volatile collections do not support the waitForSync option");
    }

  }
  else {
    TRI_InitCollectionInfo(vocbase, &parameter, name.c_str(), collectionType, effectiveSize, 0);
  }

  TRI_vocbase_col_t const* collection = TRI_CreateCollectionVocBase(vocbase, &parameter, 0);

  TRI_FreeCollectionInfoOptions(&parameter);

  if (collection == 0) {
    TRI_V8_EXCEPTION_MESSAGE(scope, TRI_errno(), "cannot create collection");
  }

  return scope.Close(TRI_WrapCollection(collection));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that a geo index or constraint exists
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> EnsureGeoIndexVocbaseCol (v8::Arguments const& argv, bool unique) {
  v8::HandleScope scope;
  
  PREVENT_EMBEDDED_TRANSACTION(scope);  

  v8::Handle<v8::Object> err;
  TRI_vocbase_col_t const* collection = UseCollection(argv.Holder(), &err);

  if (collection == 0) {
    return scope.Close(v8::ThrowException(err));
  }

  TRI_primary_collection_t* primary = collection->_collection;

  if (! TRI_IS_DOCUMENT_COLLECTION(collection->_type)) {
    ReleaseCollection(collection);
    TRI_V8_EXCEPTION_INTERNAL(scope, "unknown collection type");
  }

  TRI_document_collection_t* document = (TRI_document_collection_t*) primary;
  TRI_index_t* idx = 0;
  bool created;
  int off = unique ? 1 : 0;
  bool ignoreNull = false;

  // .............................................................................
  // case: <location>
  // .............................................................................

  if (argv.Length() == 1 + off) {
    TRI_Utf8ValueNFC loc(TRI_UNKNOWN_MEM_ZONE, argv[0]);

    if (*loc == 0) {
      ReleaseCollection(collection);
      TRI_V8_EXCEPTION_PARAMETER(scope, "<location> must be an attribute path");
    }

    if (unique) {
      ignoreNull = TRI_ObjectToBoolean(argv[1]);
    }

    idx = TRI_EnsureGeoIndex1DocumentCollection(document, *loc, false, unique, ignoreNull, &created);
  }

  // .............................................................................
  // case: <location>, <geoJson>
  // .............................................................................

  else if (argv.Length() == 2 + off && (argv[1]->IsBoolean() || argv[1]->IsBooleanObject())) {
    TRI_Utf8ValueNFC loc(TRI_UNKNOWN_MEM_ZONE, argv[0]);

    if (*loc == 0) {
      ReleaseCollection(collection);
      TRI_V8_EXCEPTION_PARAMETER(scope, "<location> must be an attribute path");
    }

    if (unique) {
      ignoreNull = TRI_ObjectToBoolean(argv[2]);
    }

    idx = TRI_EnsureGeoIndex1DocumentCollection(document, *loc, TRI_ObjectToBoolean(argv[1]), unique, ignoreNull, &created);
  }

  // .............................................................................
  // case: <latitude>, <longitude>
  // .............................................................................

  else if (argv.Length() == 2 + off) {
    TRI_Utf8ValueNFC lat(TRI_UNKNOWN_MEM_ZONE, argv[0]);
    TRI_Utf8ValueNFC lon(TRI_UNKNOWN_MEM_ZONE, argv[1]);

    if (*lat == 0) {
      ReleaseCollection(collection);
      TRI_V8_EXCEPTION_PARAMETER(scope, "<latitude> must be an attribute path");
    }

    if (*lon == 0) {
      ReleaseCollection(collection);
      TRI_V8_EXCEPTION_PARAMETER(scope, "<longitude> must be an attribute path");
    }

    if (unique) {
      ignoreNull = TRI_ObjectToBoolean(argv[2]);
    }

    idx = TRI_EnsureGeoIndex2DocumentCollection(document, *lat, *lon, unique, ignoreNull, &created);
  }

  // .............................................................................
  // error case
  // .............................................................................

  else {
    ReleaseCollection(collection);

    if (unique) {
      TRI_V8_EXCEPTION_USAGE(
        scope, 
        "ensureGeoConstraint(<latitude>, <longitude>, <ignore-null>) "  \
        "or ensureGeoConstraint(<location>, [<geojson>], <ignore-null>)");
    }
    else {
      TRI_V8_EXCEPTION_USAGE(
        scope,
        "ensureGeoIndex(<latitude>, <longitude>) or ensureGeoIndex(<location>, [<geojson>])");
    }
  }

  if (idx == 0) {
    ReleaseCollection(collection);
    TRI_V8_EXCEPTION_MESSAGE(scope, TRI_errno(), "index could not be created");
  }

  ResourceHolder holder;
  TRI_json_t* json = idx->json(idx, primary);

  if (! holder.registerJson(TRI_CORE_MEM_ZONE, json)) {
    ReleaseCollection(collection);
    TRI_V8_EXCEPTION_MEMORY(scope);
  }

  v8::Handle<v8::Value> index = IndexRep(&primary->base, json);

  if (index->IsObject()) {
    index->ToObject()->Set(v8::String::New("isNewlyCreated"), created ? v8::True() : v8::False());
  }

  ReleaseCollection(collection);
  return scope.Close(index);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an Ahuacatl error in a javascript object
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Object> CreateErrorObjectAhuacatl (TRI_aql_error_t* error) {
  v8::HandleScope scope;

  char* message = TRI_GetErrorMessageAql(error);

  if (message) {
    std::string str(message);
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, message);

    return scope.Close(TRI_CreateErrorObject(TRI_GetErrorCodeAql(error), str));
  }

  return scope.Close(TRI_CreateErrorObject(TRI_ERROR_OUT_OF_MEMORY));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function that encapsulates execution of an AQL query
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> ExecuteQueryNativeAhuacatl (TRI_aql_context_t* const context,
                                                         const TRI_json_t* const parameters) {
  v8::HandleScope scope;

  // parse & validate
  // bind values
  if (! TRI_ValidateQueryContextAql(context) ||
      ! TRI_BindQueryContextAql(context, parameters) ||
      ! TRI_SetupCollectionsContextAql(context)) {
    v8::Handle<v8::Object> errorObject = CreateErrorObjectAhuacatl(&context->_error);

    return scope.Close(v8::ThrowException(errorObject));
  }

  // note: a query is not necessarily collection-based.
  // this means that the _collections array might contain 0 collections!
  CollectionNameResolver resolver(context->_vocbase);
  AhuacatlTransaction<EmbeddableTransaction<V8TransactionContext> > trx(context->_vocbase, resolver, context);

  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_EXCEPTION_MESSAGE(scope, res, "cannot execute query");
  }

  // optimise
  if (! TRI_OptimiseQueryContextAql(context)) {
    v8::Handle<v8::Object> errorObject = CreateErrorObjectAhuacatl(&context->_error);

    return scope.Close(v8::ThrowException(errorObject));
  }

  // add barriers for all collections used
  if (! TRI_AddBarrierCollectionsAql(context)) {
    TRI_V8_EXCEPTION_INTERNAL(scope, "cannot add barrier");
  }


  // generate code
  char* code = TRI_GenerateCodeAql(context);
  if (! code || context->_error._code != TRI_ERROR_NO_ERROR) {
    v8::Handle<v8::Object> errorObject = CreateErrorObjectAhuacatl(&context->_error);

    return scope.Close(v8::ThrowException(errorObject));
  }

  // execute code
  v8::Handle<v8::Value> result = TRI_ExecuteJavaScriptString(v8::Context::GetCurrent(), v8::String::New(code), v8::String::New("query"), false);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, code);

  trx.finish(TRI_ERROR_NO_ERROR);

  // return the result as a javascript array
  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief run a query and return the results as a cursor
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> ExecuteQueryCursorAhuacatl (TRI_vocbase_t* const vocbase,
                                                         TRI_aql_context_t* const context,
                                                         const TRI_json_t* const parameters,
                                                         const bool doCount,
                                                         const uint32_t batchSize,
                                                         const bool allowDirectReturn) {
  v8::HandleScope scope;
  v8::TryCatch tryCatch;

  v8::Handle<v8::Value> result = ExecuteQueryNativeAhuacatl(context, parameters);

  if (tryCatch.HasCaught()) {
    return scope.Close(v8::ThrowException(tryCatch.Exception()));
  }

  if (allowDirectReturn || ! result->IsArray()) {
    // return the value we got as it is. this is a performance optimisation
    return scope.Close(result);
  }

  // return the result as a cursor object
  TRI_json_t* json = TRI_ObjectToJson(result);

  if (! json) {
    TRI_V8_EXCEPTION_MEMORY(scope);
  }

  TRI_general_cursor_result_t* cursorResult = TRI_CreateResultAql(json);

  if (! cursorResult) {
    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
    TRI_V8_EXCEPTION_MEMORY(scope);
  }

  TRI_general_cursor_t* cursor = TRI_CreateGeneralCursor(cursorResult, doCount, batchSize);
  if (! cursor) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, cursorResult);
    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
    TRI_V8_EXCEPTION_MEMORY(scope);
  }

  assert(cursor);
  TRI_StoreShadowData(vocbase->_cursors, (const void* const) cursor);

  return scope.Close(WrapGeneralCursor(cursor));
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   GENERAL CURSORS
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief weak reference callback for general cursors
////////////////////////////////////////////////////////////////////////////////

static void WeakGeneralCursorCallback (v8::Isolate* isolate,
                                       v8::Persistent<v8::Value> object,
                                       void* parameter) {
  v8::HandleScope scope; // do not remove, will fail otherwise!!

  LOG_TRACE("weak-callback for general cursor called");

  TRI_vocbase_t* vocbase = GetContextVocBase();

  if (! vocbase) {
    return;
  }

  TRI_EndUsageDataShadowData(vocbase->_cursors, parameter);

  // dispose and clear the persistent handle
  object.Dispose(isolate);
  object.Clear();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stores a general cursor in a javascript object
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> WrapGeneralCursor (void* cursor) {
  v8::HandleScope scope;
  v8::TryCatch tryCatch;

  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  TRI_v8_global_t* v8g = (TRI_v8_global_t*) isolate->GetData();

  v8::Handle<v8::Object> cursorObject = v8g->GeneralCursorTempl->NewInstance();

  if (cursorObject.IsEmpty()) {
    // error
    // TODO check for empty results
    return scope.Close(cursorObject);
  }

  v8::Persistent<v8::Value> persistent = v8::Persistent<v8::Value>::New(isolate, v8::External::New(cursor));

  if (tryCatch.HasCaught()) {
    return scope.Close(v8::Undefined());
  }

  cursorObject->SetInternalField(SLOT_CLASS_TYPE, v8::Integer::New(WRP_GENERAL_CURSOR_TYPE));
  cursorObject->SetInternalField(SLOT_CLASS, persistent);

  persistent.MakeWeak(isolate, cursor, WeakGeneralCursorCallback);

  return scope.Close(cursorObject);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts a cursor from a javascript object
////////////////////////////////////////////////////////////////////////////////

static void* UnwrapGeneralCursor (v8::Handle<v8::Object> cursorObject) {
  return TRI_UnwrapClass<void>(cursorObject, WRP_GENERAL_CURSOR_TYPE);
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
/// @brief executes a transaction
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_Transaction (v8::Arguments const& argv) {
  v8::HandleScope scope;
  v8::TryCatch tryCatch;

  if (argv.Length() != 1 || ! argv[0]->IsObject()) {
    TRI_V8_EXCEPTION_USAGE(scope, "TRANSACTION(<object>)");
  }

  TRI_vocbase_t* vocbase = GetContextVocBase();

  if (vocbase == 0) {
    TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract vocbase");
  }

  // treat the argument as an object from now on
  v8::Handle<v8::Object> object = v8::Handle<v8::Object>::Cast(argv[0]);

  // extract the properties from the object
  
  // "lockTimeout"
  double lockTimeout = (double) (TRI_TRANSACTION_DEFAULT_LOCK_TIMEOUT / 1000000ULL);

  if (object->Has(TRI_V8_SYMBOL("lockTimeout"))) {
    static const string timeoutError = "<lockTimeout> must be a valid numeric value"; 
    
    if (! object->Get(TRI_V8_SYMBOL("lockTimeout"))->IsNumber()) {
      TRI_V8_EXCEPTION_PARAMETER(scope, timeoutError);
    }

    lockTimeout = (double) TRI_ObjectToDouble(object->Get(TRI_V8_SYMBOL("lockTimeout")));
    if (lockTimeout < 0.0) {
      TRI_V8_EXCEPTION_PARAMETER(scope, timeoutError);
    }
  }

  // "waitForSync"
  bool waitForSync = false;

  if (object->Has(TRI_V8_SYMBOL("waitForSync"))) {
    if (! object->Get(TRI_V8_SYMBOL("waitForSync"))->IsBoolean()) {
      TRI_V8_EXCEPTION_PARAMETER(scope, "<waitForSync> must be a boolean value");
    }
    
    waitForSync = TRI_ObjectToBoolean(object->Get(TRI_V8_SYMBOL("waitForSync")));
  }

  // "collections"
  static const string collectionError = "missing/invalid collections definition for transaction";

  if (! object->Has(TRI_V8_SYMBOL("collections")) || ! object->Get(TRI_V8_SYMBOL("collections"))->IsObject()) {
    TRI_V8_EXCEPTION_PARAMETER(scope, collectionError);
  }
  
  // extract collections
  v8::Handle<v8::Array> collections = v8::Handle<v8::Array>::Cast(object->Get(TRI_V8_SYMBOL("collections")));

  if (collections.IsEmpty()) {
    TRI_V8_EXCEPTION_PARAMETER(scope, collectionError);
  }
  
  bool isValid = true;
  vector<string> readCollections;
  vector<string> writeCollections;

  // collections.read
  if (collections->Has(TRI_V8_SYMBOL("read"))) {
    if (collections->Get(TRI_V8_SYMBOL("read"))->IsArray()) {
      v8::Handle<v8::Array> names = v8::Handle<v8::Array>::Cast(collections->Get(TRI_V8_SYMBOL("read")));

      for (uint32_t i = 0 ; i < names->Length(); ++i) {
        v8::Handle<v8::Value> collection = names->Get(i);
        if (! collection->IsString()) {
          isValid = false;
          break;
        }

        readCollections.push_back(TRI_ObjectToString(collection));
      }
    }
    else if (collections->Get(TRI_V8_SYMBOL("read"))->IsString()) {
      readCollections.push_back(TRI_ObjectToString(collections->Get(TRI_V8_SYMBOL("read"))));
    }
    else {
      isValid = false;
    }
  }
  
  // collections.write
  if (collections->Has(TRI_V8_SYMBOL("write"))) {
    if (collections->Get(TRI_V8_SYMBOL("write"))->IsArray()) {
      v8::Handle<v8::Array> names = v8::Handle<v8::Array>::Cast(collections->Get(TRI_V8_SYMBOL("write")));

      for (uint32_t i = 0 ; i < names->Length(); ++i) {
        v8::Handle<v8::Value> collection = names->Get(i);
        if (! collection->IsString()) {
          isValid = false;
          break;
        }
      
        writeCollections.push_back(TRI_ObjectToString(collection));
      }
    }
    else if (collections->Get(TRI_V8_SYMBOL("write"))->IsString()) {
      writeCollections.push_back(TRI_ObjectToString(collections->Get(TRI_V8_SYMBOL("write"))));
    }
    else {
      isValid = false;
    }
  }

  if (! isValid) {
    TRI_V8_EXCEPTION_PARAMETER(scope, collectionError);
  }

  // extract the "action" property
  static const string actionError = "missing/invalid action definition for transaction";

  if (! object->Has(TRI_V8_SYMBOL("action"))) {
    TRI_V8_EXCEPTION_PARAMETER(scope, actionError);
  }

  // function parameters
  v8::Handle<v8::Value> params;

  if (object->Has(TRI_V8_SYMBOL("params"))) {
    params = v8::Handle<v8::Array>::Cast(object->Get(TRI_V8_SYMBOL("params")));
  }
  else {
    params = v8::Undefined();
  }
  
  if (params.IsEmpty()) {
    TRI_V8_EXCEPTION(scope, TRI_ERROR_INTERNAL);
  }
  

  v8::Handle<v8::Object> current = v8::Context::GetCurrent()->Global();

  // callback function
  v8::Handle<v8::Function> action;

  if (object->Get(TRI_V8_SYMBOL("action"))->IsFunction()) {
    action = v8::Handle<v8::Function>::Cast(object->Get(TRI_V8_SYMBOL("action")));
  }
  else if (object->Get(TRI_V8_SYMBOL("action"))->IsString()) {
    // get built-in Function constructor (see ECMA-262 5th edition 15.3.2)
    v8::Local<v8::Function> ctor = v8::Local<v8::Function>::Cast(current->Get(v8::String::New("Function")));

    // Invoke Function constructor to create function with the given body and no arguments
    string body = TRI_ObjectToString(object->Get(TRI_V8_SYMBOL("action"))->ToString());
    body = "return (" + body + ")(params);";
    v8::Handle<v8::Value> argv[2] = { v8::String::New("params"), v8::String::New(body.c_str(), body.size()) };
    v8::Local<v8::Object> function = ctor->NewInstance(2, argv);

    action = v8::Local<v8::Function>::Cast(function);
  }
  else {
    TRI_V8_EXCEPTION_PARAMETER(scope, actionError);
  }
  
  if (action.IsEmpty()) {
    TRI_V8_EXCEPTION_PARAMETER(scope, actionError);
  }


  // start actual transaction
  CollectionNameResolver resolver(vocbase);
  ExplicitTransaction<StandaloneTransaction<V8TransactionContext> > trx(vocbase, 
                                                                        resolver, 
                                                                        readCollections, 
                                                                        writeCollections, 
                                                                        lockTimeout,
                                                                        waitForSync); 

  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_EXCEPTION(scope, res);
  }

  v8::Handle<v8::Value> args = params;
  v8::Handle<v8::Value> result = action->Call(current, 1, &args);

  if (tryCatch.HasCaught()) {
    trx.abort();

    return scope.Close(v8::ThrowException(tryCatch.Exception()));
  }

  res = trx.commit();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_EXCEPTION(scope, res);
  }
  
  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief normalize UTF 16 strings
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_normalize_string (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 1) {
    TRI_V8_EXCEPTION_USAGE(scope, "NORMALIZE_STRING(<string>)");
  }

  return scope.Close(TRI_normalize_V8_Obj(argv[0]));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compare two UTF 16 strings
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_compare_string (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 2) {
    TRI_V8_EXCEPTION_USAGE(scope, "COMPARE_STRING(<left string>, <right string>)");
  }

  v8::String::Value left(argv[0]);
  v8::String::Value right(argv[1]);

  // ..........................................................................
  // Take note here: we are assuming that the ICU type UChar is two bytes.
  // There is no guarantee that this will be the case on all platforms and
  // compilers.
  // ..........................................................................
  int result = Utf8Helper::DefaultUtf8Helper.compareUtf16(*left, left.length(), *right, right.length());

  return scope.Close(v8::Integer::New(result));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reloads the authentication info from collection _users
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_ReloadAuth (v8::Arguments const& argv) {
  v8::HandleScope scope;

  TRI_vocbase_t* vocbase = GetContextVocBase();

  if (vocbase == 0) {
    TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract vocbase");
  }

  if (argv.Length() != 0) {
    TRI_V8_EXCEPTION_USAGE(scope, "RELOAD_AUTH()");
  }

  bool result = TRI_ReloadAuthInfo(vocbase);

  return scope.Close(result ? v8::True() : v8::False());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates a general cursor from a list
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_CreateCursor (v8::Arguments const& argv) {
  v8::HandleScope scope;

  TRI_vocbase_t* vocbase = GetContextVocBase();

  if (vocbase == 0) {
    TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract vocbase");
  }

  if (argv.Length() < 1) {
    TRI_V8_EXCEPTION_USAGE(scope, "CREATE_CURSOR(<list>, <do-count>, <batch-size>)");
  }

  if (! argv[0]->IsArray()) {
    TRI_V8_TYPE_ERROR(scope, "<list> must be a list");
  }
    
  // extract objects
  v8::Handle<v8::Array> array = v8::Handle<v8::Array>::Cast(argv[0]);
  TRI_json_t* json = TRI_ObjectToJson(array);

  if (json == 0) {
    TRI_V8_TYPE_ERROR(scope, "cannot convert <list> to JSON");
  }

  // return number of total records in cursor?
  bool doCount = false;

  if (argv.Length() >= 2) {
    doCount = TRI_ObjectToBoolean(argv[1]);
  }

  // maximum number of results to return at once
  uint32_t batchSize = 1000;

  if (argv.Length() >= 3) {
    double maxValue = TRI_ObjectToDouble(argv[2]);

    if (maxValue >= 1.0) {
      batchSize = (uint32_t) maxValue;
    }
  }

  // create a cursor
  TRI_general_cursor_t* cursor = 0;
  TRI_general_cursor_result_t* cursorResult = TRI_CreateResultAql(json);

  if (cursorResult != 0) {
    cursor = TRI_CreateGeneralCursor(cursorResult, doCount, batchSize);

    if (cursor == 0) {
      TRI_Free(TRI_UNKNOWN_MEM_ZONE, cursorResult);
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
    }
  }
  else {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, cursorResult);
    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
  }

  if (cursor == 0) {
    TRI_V8_EXCEPTION_INTERNAL(scope, "cannot create cursor");
  }

  TRI_StoreShadowData(vocbase->_cursors, (const void* const) cursor);
  return scope.Close(WrapGeneralCursor(cursor));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a general cursor
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_DisposeGeneralCursor (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 0) {
    TRI_V8_EXCEPTION_USAGE(scope, "dispose()");
  }

  TRI_vocbase_t* vocbase = GetContextVocBase();

  if (vocbase == 0) {
    TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract vocbase");
  }

  bool found = TRI_DeleteDataShadowData(vocbase->_cursors, UnwrapGeneralCursor(argv.Holder()));

  return scope.Close(found ? v8::True() : v8::False());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the id of a general cursor
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_IdGeneralCursor (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 0) {
    TRI_V8_EXCEPTION_USAGE(scope, "id()");
  }

  TRI_vocbase_t* vocbase = GetContextVocBase();

  if (vocbase == 0) {
    TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract vocbase");
  }

  TRI_shadow_id id = TRI_GetIdDataShadowData(vocbase->_cursors, UnwrapGeneralCursor(argv.Holder()));

  if (id != 0) {
    return scope.Close(V8StringId(id));
  }

  TRI_V8_EXCEPTION(scope, TRI_ERROR_CURSOR_NOT_FOUND);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the number of results
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_CountGeneralCursor (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 0) {
    TRI_V8_EXCEPTION_USAGE(scope, "count()");
  }

  TRI_vocbase_t* vocbase = GetContextVocBase();

  if (vocbase == 0) {
    TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract vocbase");
  }

  TRI_general_cursor_t* cursor;

  cursor = (TRI_general_cursor_t*) TRI_BeginUsageDataShadowData(vocbase->_cursors, UnwrapGeneralCursor(argv.Holder()));

  if (cursor) {
    size_t length = (size_t) cursor->_length;
    TRI_EndUsageDataShadowData(vocbase->_cursors, cursor);

    return scope.Close(v8::Number::New(length));
  }

  TRI_V8_EXCEPTION(scope, TRI_ERROR_CURSOR_NOT_FOUND);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the next result from the general cursor
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_NextGeneralCursor (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 0) {
    TRI_V8_EXCEPTION_USAGE(scope, "next()");
  }

  TRI_vocbase_t* vocbase = GetContextVocBase();

  if (vocbase == 0) {
    TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract vocbase");
  }

  v8::Handle<v8::Value> value;
  TRI_general_cursor_t* cursor;

  cursor = (TRI_general_cursor_t*) TRI_BeginUsageDataShadowData(vocbase->_cursors, UnwrapGeneralCursor(argv.Holder()));

  if (cursor) {
    bool result = false;

    TRI_LockGeneralCursor(cursor);

    if (cursor->_length == 0) {
      TRI_UnlockGeneralCursor(cursor);
      TRI_EndUsageDataShadowData(vocbase->_cursors, cursor);

      return scope.Close(v8::Undefined());
    }

    // exceptions must be caught in the following part because we hold an exclusive
    // lock that might otherwise not be freed
    v8::TryCatch tryCatch;

    try {
      TRI_general_cursor_row_t row = cursor->next(cursor);

      if (row == 0) {
        value = v8::Undefined();
      }
      else {
        value = TRI_ObjectJson((TRI_json_t*) row);
        result = true;
      }
    }
    catch (...) {
    }

    TRI_UnlockGeneralCursor(cursor);

    TRI_EndUsageDataShadowData(vocbase->_cursors, cursor);

    if (result && ! tryCatch.HasCaught()) {
      return scope.Close(value);
    }

    if (tryCatch.HasCaught()) {
      return scope.Close(v8::ThrowException(tryCatch.Exception()));
    }
  }

  TRI_V8_EXCEPTION(scope, TRI_ERROR_CURSOR_NOT_FOUND);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief persist the general cursor for usage in subsequent requests
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_PersistGeneralCursor (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 0) {
    TRI_V8_EXCEPTION_USAGE(scope, "persist()");
  }

  TRI_vocbase_t* vocbase = GetContextVocBase();

  if (vocbase == 0) {
    TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract vocbase");
  }

  bool result = TRI_PersistDataShadowData(vocbase->_cursors, UnwrapGeneralCursor(argv.Holder()));

  if (result) {
    return scope.Close(v8::True());
  }

  TRI_V8_EXCEPTION(scope, TRI_ERROR_CURSOR_NOT_FOUND);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return all following rows from the cursor in one go
///
/// This function constructs multiple rows at once and should be preferred over
/// hasNext()...next() when iterating over bigger result sets
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_ToArrayGeneralCursor (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 0) {
    TRI_V8_EXCEPTION_USAGE(scope, "toArray()");
  }

  TRI_vocbase_t* vocbase = GetContextVocBase();

  if (vocbase == 0) {
    TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract vocbase");
  }

  v8::Handle<v8::Array> rows = v8::Array::New();
  TRI_general_cursor_t* cursor;

  cursor = (TRI_general_cursor_t*) TRI_BeginUsageDataShadowData(vocbase->_cursors, UnwrapGeneralCursor(argv.Holder()));

  if (cursor) {
    bool result = false;

    TRI_LockGeneralCursor(cursor);

    // exceptions must be caught in the following part because we hold an exclusive
    // lock that might otherwise not be freed
    v8::TryCatch tryCatch;

    try {
      uint32_t max = (uint32_t) cursor->getBatchSize(cursor);

      for (uint32_t i = 0; i < max; ++i) {
        TRI_general_cursor_row_t row = cursor->next(cursor);
        if (!row) {
          break;
        }
        rows->Set(i, TRI_ObjectJson((TRI_json_t*) row));
      }

      result = true;
    }
    catch (...) {
    }

    TRI_UnlockGeneralCursor(cursor);

    TRI_EndUsageDataShadowData(vocbase->_cursors, cursor);

    if (result && ! tryCatch.HasCaught()) {
      return scope.Close(rows);
    }

    if (tryCatch.HasCaught()) {
      return scope.Close(v8::ThrowException(tryCatch.Exception()));
    }
  }

  TRI_V8_EXCEPTION(scope, TRI_ERROR_CURSOR_NOT_FOUND);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief alias for toArray()
/// @deprecated
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_GetRowsGeneralCursor (v8::Arguments const& argv) {
  return JS_ToArrayGeneralCursor(argv);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return max number of results per transfer for cursor
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_GetBatchSizeGeneralCursor (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 0) {
    TRI_V8_EXCEPTION_USAGE(scope, "getBatchSize()");
  }

  TRI_vocbase_t* vocbase = GetContextVocBase();

  if (vocbase == 0) {
    TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract vocbase");
  }

  TRI_general_cursor_t* cursor;

  cursor = (TRI_general_cursor_t*) TRI_BeginUsageDataShadowData(vocbase->_cursors, UnwrapGeneralCursor(argv.Holder()));

  if (cursor) {
    uint32_t max = cursor->getBatchSize(cursor);

    TRI_EndUsageDataShadowData(vocbase->_cursors, cursor);
    return scope.Close(v8::Number::New(max));
  }

  TRI_V8_EXCEPTION(scope, TRI_ERROR_CURSOR_NOT_FOUND);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return if count flag was set for cursor
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_HasCountGeneralCursor (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 0) {
    TRI_V8_EXCEPTION_USAGE(scope, "hasCount()");
  }

  TRI_vocbase_t* vocbase = GetContextVocBase();

  if (vocbase == 0) {
    TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract vocbase");
  }

  TRI_general_cursor_t* cursor;

  cursor = (TRI_general_cursor_t*) TRI_BeginUsageDataShadowData(vocbase->_cursors, UnwrapGeneralCursor(argv.Holder()));

  if (cursor) {
    bool hasCount = cursor->hasCount(cursor);

    TRI_EndUsageDataShadowData(vocbase->_cursors, cursor);
    return scope.Close(hasCount ? v8::True() : v8::False());
  }

  TRI_V8_EXCEPTION(scope, TRI_ERROR_CURSOR_NOT_FOUND);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if the cursor is exhausted
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_HasNextGeneralCursor (v8::Arguments const& argv) {
  v8::HandleScope scope;
  v8::TryCatch tryCatch;

  if (argv.Length() != 0) {
    TRI_V8_EXCEPTION_USAGE(scope, "hasNext()");
  }

  TRI_vocbase_t* vocbase = GetContextVocBase();

  if (vocbase == 0) {
    TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract vocbase");
  }

  TRI_general_cursor_t* cursor;

  cursor = (TRI_general_cursor_t*) TRI_BeginUsageDataShadowData(vocbase->_cursors, UnwrapGeneralCursor(argv.Holder()));

  if (cursor) {
    TRI_LockGeneralCursor(cursor);
    bool hasNext = cursor->hasNext(cursor);
    TRI_UnlockGeneralCursor(cursor);

    TRI_EndUsageDataShadowData(vocbase->_cursors, cursor);
    return scope.Close(hasNext ? v8::True() : v8::False());
  }

  TRI_V8_EXCEPTION(scope, TRI_ERROR_CURSOR_NOT_FOUND);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unuse a general cursor
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_UnuseGeneralCursor (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 0) {
    TRI_V8_EXCEPTION_USAGE(scope, "unuse()");
  }

  TRI_vocbase_t* vocbase = GetContextVocBase();

  if (! vocbase) {
    TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract vocbase");
  }

  TRI_EndUsageDataShadowData(vocbase->_cursors, UnwrapGeneralCursor(argv.Holder()));

  return scope.Close(v8::Undefined());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get a (persistent) cursor by its id
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_Cursor (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 1) {
    TRI_V8_EXCEPTION_USAGE(scope, "CURSOR(<cursor-identifier>)");
  }

  TRI_vocbase_t* vocbase = GetContextVocBase();

  if (vocbase == 0) {
    TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract vocbase");
  }

  // get the id
  v8::Handle<v8::Value> idArg = argv[0]->ToString();

  if (! idArg->IsString()) {
    TRI_V8_TYPE_ERROR(scope, "expecting a string for <cursor-identifier>)");
  }

  const string idString = TRI_ObjectToString(idArg);
  uint64_t id = TRI_UInt64String(idString.c_str());

  TRI_general_cursor_t* cursor;

  cursor = (TRI_general_cursor_t*) TRI_BeginUsageIdShadowData(vocbase->_cursors, id);

  if (cursor == 0) {
    TRI_V8_EXCEPTION(scope, TRI_ERROR_CURSOR_NOT_FOUND);
  }

  return scope.Close(WrapGeneralCursor(cursor));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief delete a (persistent) cursor by its id
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_DeleteCursor (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 1) {
    TRI_V8_EXCEPTION_USAGE(scope, "DELETE_CURSOR(<cursor-identifier>)");
  }

  TRI_vocbase_t* vocbase = GetContextVocBase();

  if (vocbase == 0) {
    TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract vocbase");
  }

  // get the id
  v8::Handle<v8::Value> idArg = argv[0]->ToString();

  if (! idArg->IsString()) {
    TRI_V8_TYPE_ERROR(scope, "expecting a string for <cursor-identifier>)");
  }

  string idString = TRI_ObjectToString(idArg);
  uint64_t id = TRI_UInt64String(idString.c_str());

  bool found = TRI_DeleteIdShadowData(vocbase->_cursors, id);

  return scope.Close(found ? v8::True() : v8::False());
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                          AHUACATL
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief creates code for an AQL query and runs it
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_RunAhuacatl (v8::Arguments const& argv) {
  v8::HandleScope scope;
  v8::TryCatch tryCatch;
  const uint32_t argc = argv.Length();

  if (argc < 1 || argc > 5) {
    TRI_V8_EXCEPTION_USAGE(scope, "AHUACATL_RUN(<querystring>, <bindvalues>, <doCount>, <batchSize>, <allowDirectReturn>)");
  }

  TRI_vocbase_t* vocbase = GetContextVocBase();

  if (vocbase == 0) {
    TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract vocbase");
  }

  // get the query string
  v8::Handle<v8::Value> queryArg = argv[0];

  if (! queryArg->IsString()) {
    TRI_V8_TYPE_ERROR(scope, "expecting string for <querystring>");
  }

  const string queryString = TRI_ObjectToString(queryArg);

  // return number of total records in cursor?
  bool doCount             = false;

  // maximum number of results to return at once
  uint32_t batchSize       = 1000;

  // directly return the results as a javascript array instead of a cursor object (performance optimisation)
  bool allowDirectReturn   = false;

  if (argc > 2) {
    doCount = TRI_ObjectToBoolean(argv[2]);

    if (argc > 3) {
      double maxValue = TRI_ObjectToDouble(argv[3]);
      
      if (maxValue >= 1.0) {
        batchSize = (uint32_t) maxValue;
      }

      if (argc > 4) {
        allowDirectReturn = TRI_ObjectToBoolean(argv[4]);
      }
    }
  }

  // bind parameters
  ResourceHolder holder;

  TRI_json_t* parameters = 0;
  if (argc > 1) {
    parameters = TRI_ObjectToJson(argv[1]);
    holder.registerJson(TRI_UNKNOWN_MEM_ZONE, parameters);
  }

  AhuacatlGuard context(vocbase, queryString);

  if (! context.valid()) {
    TRI_V8_EXCEPTION_MEMORY(scope);
  }

  v8::Handle<v8::Value> result;
  result = ExecuteQueryCursorAhuacatl(vocbase, context.ptr(), parameters, doCount, batchSize, allowDirectReturn);
  context.free();

  if (tryCatch.HasCaught()) {
    if (tryCatch.Exception()->IsObject() && v8::Handle<v8::Array>::Cast(tryCatch.Exception())->HasOwnProperty(v8::String::New("errorNum"))) {
      // we already have an ArangoError object
      return scope.Close(v8::ThrowException(tryCatch.Exception()));
    }

    // create a new error object
    v8::Handle<v8::Object> errorObject = TRI_CreateErrorObject(TRI_ERROR_QUERY_SCRIPT, TRI_ObjectToString(tryCatch.Exception()).c_str());
    return scope.Close(v8::ThrowException(errorObject));
  }

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief explains an AQL query
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_ExplainAhuacatl (v8::Arguments const& argv) {
  v8::HandleScope scope;
  v8::TryCatch tryCatch;
  const uint32_t argc = argv.Length();

  if (argc < 1 || argc > 3) {
    TRI_V8_EXCEPTION_USAGE(scope, "AHUACATL_EXPLAIN(<querystring>, <bindvalues>, <performoptimisations>)");
  }

  TRI_vocbase_t* vocbase = GetContextVocBase();

  if (vocbase == 0) {
    TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract vocbase");
  }

  // get the query string
  v8::Handle<v8::Value> queryArg = argv[0];
  if (!queryArg->IsString()) {
    TRI_V8_TYPE_ERROR(scope, "expecting string for <querystring>");
  }

  const string queryString = TRI_ObjectToString(queryArg);

  // bind parameters
  ResourceHolder holder;

  TRI_json_t* parameters = 0;
  if (argc > 1) {
    parameters = TRI_ObjectToJson(argv[1]);
    holder.registerJson(TRI_UNKNOWN_MEM_ZONE, parameters);
  }

  AhuacatlGuard guard(vocbase, queryString);

  if (! guard.valid()) {
    TRI_V8_EXCEPTION_MEMORY(scope);
  }

  TRI_aql_context_t* context = guard.ptr();

  bool performOptimisations = true;
  if (argc > 2) {
    // turn off optimisations ?
    performOptimisations = TRI_ObjectToBoolean(argv[2]);
  }

  TRI_json_t* explain = 0;

  if (! TRI_ValidateQueryContextAql(context) ||
      ! TRI_BindQueryContextAql(context, parameters) ||
      ! TRI_SetupCollectionsContextAql(context)) {
    v8::Handle<v8::Object> errorObject = CreateErrorObjectAhuacatl(&context->_error);
    return scope.Close(v8::ThrowException(errorObject));
  }

  // note: a query is not necessarily collection-based.
  // this means that the _collections array might contain 0 collections!
  CollectionNameResolver resolver(vocbase);
  AhuacatlTransaction<EmbeddableTransaction<V8TransactionContext> > trx(vocbase, resolver, context);

  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_EXCEPTION_MESSAGE(scope, res, "cannot explain query");
  }

  if ((performOptimisations && ! TRI_OptimiseQueryContextAql(context)) ||
      ! (explain = TRI_ExplainAql(context))) {
    v8::Handle<v8::Object> errorObject = CreateErrorObjectAhuacatl(&context->_error);
    return scope.Close(v8::ThrowException(errorObject));
  }

  trx.finish(TRI_ERROR_NO_ERROR);

  assert(explain);

  v8::Handle<v8::Value> result;
  result = TRI_ObjectJson(explain);
  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, explain);
  guard.free();

  if (tryCatch.HasCaught()) {
    if (tryCatch.Exception()->IsObject() && v8::Handle<v8::Array>::Cast(tryCatch.Exception())->HasOwnProperty(v8::String::New("errorNum"))) {
      // we already have an ArangoError object
      return scope.Close(v8::ThrowException(tryCatch.Exception()));
    }

    // create a new error object
    v8::Handle<v8::Object> errorObject = TRI_CreateErrorObject(TRI_ERROR_QUERY_SCRIPT, TRI_ObjectToString(tryCatch.Exception()).c_str());
    return scope.Close(v8::ThrowException(errorObject));
  }

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parses an AQL query and returns the parse result
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_ParseAhuacatl (v8::Arguments const& argv) {
  v8::HandleScope scope;
  v8::TryCatch tryCatch;

  if (argv.Length() != 1) {
    TRI_V8_EXCEPTION_USAGE(scope, "AHUACATL_PARSE(<querystring>)");
  }

  TRI_vocbase_t* vocbase = GetContextVocBase();

  if (vocbase == 0) {
    TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract vocbase");
  }

  // get the query string
  v8::Handle<v8::Value> queryArg = argv[0];

  if (!queryArg->IsString()) {
    TRI_V8_TYPE_ERROR(scope, "expecting string for <querystring>");
  }

  string queryString = TRI_ObjectToString(queryArg);

  AhuacatlGuard context(vocbase, queryString);

  if (! context.valid()) {
    TRI_V8_EXCEPTION_MEMORY(scope);
  }

  // parse & validate
  if (! TRI_ValidateQueryContextAql(context.ptr())) {
    v8::Handle<v8::Object> errorObject = CreateErrorObjectAhuacatl(&(context.ptr())->_error);
    return scope.Close(v8::ThrowException(errorObject));
  }

  // setup result
  v8::Handle<v8::Object> result = v8::Object::New();

  result->Set(v8::String::New("parsed"), v8::True());

  // return the bind parameter names
  result->Set(v8::String::New("parameters"), TRI_ArrayAssociativePointer(&(context.ptr())->_parameters._names));
  // return the collection names
  result->Set(v8::String::New("collections"), TRI_ArrayAssociativePointer(&(context.ptr())->_collectionNames));
  context.free();

  if (tryCatch.HasCaught()) {
    if (tryCatch.Exception()->IsObject() && v8::Handle<v8::Array>::Cast(tryCatch.Exception())->HasOwnProperty(v8::String::New("errorNum"))) {
      // we already have an ArangoError object
      return scope.Close(v8::ThrowException(tryCatch.Exception()));
    }

    // create a new error object
    v8::Handle<v8::Object> errorObject = TRI_CreateErrorObject(TRI_ERROR_QUERY_SCRIPT, TRI_ObjectToString(tryCatch.Exception()).c_str());
    return scope.Close(v8::ThrowException(errorObject));
  }

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                          TRI_DATAFILE_T FUNCTIONS
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                              javascript functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief migrate an "old" collection to a newer version
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_UpgradeVocbaseCol (v8::Arguments const& argv) {
  v8::HandleScope scope;

  // some typedefs for deprecated markers, only used inside this function
  typedef uint64_t voc_did_t;

  typedef struct {
    TRI_df_marker_t base;

    voc_did_t _did;        // this is the tick for a create, but not an update
    TRI_voc_rid_t _rid;    // this is the tick for an create and update
    TRI_voc_tid_t _sid;

    TRI_shape_sid_t _shape;
  }
  doc_document_marker_t_deprecated;

  typedef struct {
    doc_document_marker_t_deprecated base;

    TRI_voc_cid_t _toCid;
    voc_did_t _toDid;

    TRI_voc_cid_t _fromCid;
    voc_did_t _fromDid;
  }
  doc_edge_marker_t_deprecated;

  typedef struct {
    TRI_df_marker_t base;

    voc_did_t _did;        // this is the tick for a create, but not an update
    TRI_voc_rid_t _rid;    // this is the tick for an create and update
    TRI_voc_tid_t _sid;
  }
  doc_deletion_marker_t_deprecated;

  ssize_t writeResult;

  if (argv.Length() != 0) {
    TRI_V8_EXCEPTION_USAGE(scope, "upgrade()");
  }

  TRI_vocbase_col_t const* collection;

  // extract the collection
  v8::Handle<v8::Object> err;
  collection = UseCollection(argv.Holder(), &err);

  if (collection == 0) {
    return scope.Close(v8::ThrowException(err));
  }

  TRI_primary_collection_t* primary = collection->_collection;

  if (! TRI_IS_DOCUMENT_COLLECTION(collection->_type)) {
    ReleaseCollection(collection);
    TRI_V8_EXCEPTION_INTERNAL(scope, "unknown collection type");
  }

  TRI_collection_t* col = &primary->base;

#ifdef TRI_ENABLE_LOGGER
  const char* name = col->_info._name;
#endif
  TRI_col_version_t version = col->_info._version;

  if (version >= 3) {
    LOG_ERROR("Cannot upgrade collection '%s' with version '%d' in directory '%s'", name, version, col->_directory);
    ReleaseCollection(collection);
    return scope.Close(v8::False());
  }

  LOG_INFO("Upgrading collection '%s' with version '%d' in directory '%s'", name, version, col->_directory);

  // get all filenames
  size_t i;
  TRI_vector_pointer_t files;
  TRI_InitVectorPointer(&files, TRI_UNKNOWN_MEM_ZONE);
  for (i = 0; i < col->_datafiles._length; ++i) {
    TRI_datafile_t* df = (TRI_datafile_t*) TRI_AtVectorPointer(&col->_datafiles, i);
    TRI_PushBackVectorPointer(&files, df);
  }
  for (i = 0; i < col->_journals._length; ++i) {
    TRI_datafile_t* df = (TRI_datafile_t*) TRI_AtVectorPointer(&col->_journals, i);
    TRI_PushBackVectorPointer(&files, df);
  }
  for (i = 0; i < col->_compactors._length; ++i) {
    TRI_datafile_t* df = (TRI_datafile_t*) TRI_AtVectorPointer(&col->_compactors, i);
    TRI_PushBackVectorPointer(&files, df);
  }

  // convert each file
  for (size_t j = 0; j < files._length; ++j) {
    int fd, fdout;

    TRI_datafile_t* df = (TRI_datafile_t*) TRI_AtVectorPointer(&files, j);

    int64_t fileSize = TRI_SizeFile(df->_filename);
    int64_t writtenSize = 0;

    LOG_INFO("convert file '%s' (size = %lld)", df->_filename, (long long) fileSize);

    fd = TRI_OPEN(df->_filename, O_RDONLY);
    if (fd < 0) {
      LOG_ERROR("could not open file '%s' for reading", df->_filename);

      TRI_DestroyVectorPointer(&files);
      ReleaseCollection(collection);

      return scope.Close(v8::False());
    }

    ostringstream outfile;
    outfile << df->_filename << ".new";

    fdout = TRI_CREATE(outfile.str().c_str(), O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR);
    if (fdout < 0) {
      LOG_ERROR("could not open file '%s' for writing", outfile.str().c_str());

      TRI_DestroyVectorPointer(&files);
      ReleaseCollection(collection);

      TRI_CLOSE(fd);
      return scope.Close(v8::False());
    }

    //LOG_INFO("fd: %d, fdout: %d", fd, fdout);

    TRI_df_marker_t marker;

    while (true) {
      // read marker header
      ssize_t bytesRead = TRI_READ(fd, &marker, sizeof(marker));

      if (bytesRead == 0) {
        // eof
        break;
      }

      if (bytesRead < (ssize_t) sizeof(marker)) {
        // eof
        LOG_WARNING("bytesRead = %d < sizeof(marker) = %d", (int) bytesRead, (int) sizeof(marker));
        break;
      }

      if (marker._size == 0) {
        // eof
        break;
      }

      if (bytesRead == sizeof(marker)) {
        // read marker body

        if (marker._size < sizeof(marker)) {
          // eof
          LOG_WARNING("marker._size = %d < sizeof(marker) = %d", (int) marker._size, (int) sizeof(marker));
          break;
        }

        off_t paddedSize = TRI_DF_ALIGN_BLOCK(marker._size);

        char* payload = new char[paddedSize];

        // copy header

        memcpy(payload, &marker, sizeof(marker));

        if (marker._size > sizeof(marker)) {
          //int r = ::read(fd, p + sizeof(marker), marker._size - sizeof(marker));
          int r = TRI_READ(fd, payload + sizeof(marker), paddedSize - sizeof(marker));
          if (r < (int) (paddedSize - sizeof(marker))) {
            LOG_WARNING("read less than paddedSize - sizeof(marker) = %d", r);
            break;
          }
        }

        if ((int) marker._type == 0) {
          // eof
          break;
        }

        switch (marker._type) {
          case TRI_DOC_MARKER_DOCUMENT: {
            doc_document_marker_t_deprecated* oldMarker = (doc_document_marker_t_deprecated*) payload;
            TRI_doc_document_key_marker_t newMarker;
            TRI_voc_size_t newMarkerSize = sizeof(TRI_doc_document_key_marker_t);

            char* body = ((char*) oldMarker) + sizeof(doc_document_marker_t_deprecated);
            TRI_voc_size_t bodySize = oldMarker->base._size - sizeof(doc_document_marker_t_deprecated);
            TRI_voc_size_t bodySizePadded = paddedSize - sizeof(doc_document_marker_t_deprecated);

            char* keyBody;
            TRI_voc_size_t keyBodySize;

            TRI_voc_size_t keySize;
            char didBuffer[33];

            memset(&newMarker, 0, newMarkerSize);

            sprintf(didBuffer,"%llu", (unsigned long long) oldMarker->_did);
            keySize = strlen(didBuffer) + 1;
            keyBodySize = TRI_DF_ALIGN_BLOCK(keySize);
            keyBody = (char*) TRI_Allocate(TRI_CORE_MEM_ZONE, keyBodySize, true);
            TRI_CopyString(keyBody, didBuffer, keySize);

            newMarker._rid = oldMarker->_rid;
            newMarker._tid = 0;
            newMarker._shape = oldMarker->_shape;
            newMarker._offsetKey = newMarkerSize;
            newMarker._offsetJson = newMarkerSize + keyBodySize;

            newMarker.base._type = TRI_DOC_MARKER_KEY_DOCUMENT;
            newMarker.base._tick = oldMarker->base._tick;
            newMarker.base._size = newMarkerSize + keyBodySize + bodySize;
            TRI_FillCrcKeyMarkerDatafile(df, &newMarker.base, newMarkerSize, keyBody, keyBodySize, body, bodySize);

            writeResult = TRI_WRITE(fdout, &newMarker, sizeof(newMarker));
            writeResult = TRI_WRITE(fdout, keyBody, keyBodySize);
            writeResult = TRI_WRITE(fdout, body, bodySizePadded);

            //LOG_INFO("found doc marker, type: '%d', did: '%d', rid: '%d', size: '%d', crc: '%d'", marker._type, oldMarker->_did, oldMarker->_rid,newMarker.base._size,newMarker.base._crc);

            TRI_Free(TRI_CORE_MEM_ZONE, keyBody);

            writtenSize += sizeof(newMarker) + keyBodySize + bodySizePadded;
            break;
          }

          case TRI_DOC_MARKER_EDGE: {
            doc_edge_marker_t_deprecated* oldMarker = (doc_edge_marker_t_deprecated*) payload;
            TRI_doc_edge_key_marker_t newMarker;
            TRI_voc_size_t newMarkerSize = sizeof(TRI_doc_edge_key_marker_t);

            char* body = ((char*) oldMarker) + sizeof(doc_edge_marker_t_deprecated);
            TRI_voc_size_t bodySize = oldMarker->base.base._size - sizeof(doc_edge_marker_t_deprecated);
            TRI_voc_size_t bodySizePadded = paddedSize - sizeof(doc_edge_marker_t_deprecated);

            char* keyBody;
            TRI_voc_size_t keyBodySize;

            size_t keySize;
            size_t toSize;
            size_t fromSize;

            char didBuffer[33];
            char toDidBuffer[33];
            char fromDidBuffer[33];

            memset(&newMarker, 0, newMarkerSize);

            sprintf(didBuffer,"%llu", (unsigned long long) oldMarker->base._did);
            sprintf(toDidBuffer,"%llu", (unsigned long long) oldMarker->_toDid);
            sprintf(fromDidBuffer,"%llu", (unsigned long long) oldMarker->_fromDid);

            keySize = strlen(didBuffer) + 1;
            toSize = strlen(toDidBuffer) + 1;
            fromSize = strlen(fromDidBuffer) + 1;

            keyBodySize = TRI_DF_ALIGN_BLOCK(keySize + toSize + fromSize);
            keyBody = (char*) TRI_Allocate(TRI_CORE_MEM_ZONE, keyBodySize, true);

            TRI_CopyString(keyBody,                    didBuffer,     keySize);
            TRI_CopyString(keyBody + keySize,          toDidBuffer,   toSize);
            TRI_CopyString(keyBody + keySize + toSize, fromDidBuffer, fromSize);

            newMarker.base._rid = oldMarker->base._rid;
            newMarker.base._tid = 0;
            newMarker.base._shape = oldMarker->base._shape;
            newMarker.base._offsetKey = newMarkerSize;
            newMarker.base._offsetJson = newMarkerSize + keyBodySize;

            newMarker._offsetToKey = newMarkerSize + keySize;
            newMarker._offsetFromKey = newMarkerSize + keySize + toSize;
            newMarker._toCid = oldMarker->_toCid;
            newMarker._fromCid = oldMarker->_fromCid;

            newMarker.base.base._size = newMarkerSize + keyBodySize + bodySize;
            newMarker.base.base._type = TRI_DOC_MARKER_KEY_EDGE;
            newMarker.base.base._tick = oldMarker->base.base._tick;
            TRI_FillCrcKeyMarkerDatafile(df, &newMarker.base.base, newMarkerSize, keyBody, keyBodySize, body, bodySize);

            writeResult = TRI_WRITE(fdout, &newMarker, newMarkerSize);
            (void) writeResult;
            writeResult = TRI_WRITE(fdout, keyBody, keyBodySize);
            (void) writeResult;
            writeResult = TRI_WRITE(fdout, body, bodySizePadded);
            (void) writeResult;

            //LOG_INFO("found edge marker, type: '%d', did: '%d', rid: '%d', size: '%d', crc: '%d'", marker._type, oldMarker->base._did, oldMarker->base._rid,newMarker.base.base._size,newMarker.base.base._crc);

            TRI_Free(TRI_CORE_MEM_ZONE, keyBody);

            writtenSize += newMarkerSize + keyBodySize + bodySizePadded;
            break;
          }

          case TRI_DOC_MARKER_DELETION: {
            doc_deletion_marker_t_deprecated* oldMarker = (doc_deletion_marker_t_deprecated*) payload;
            TRI_doc_deletion_key_marker_t newMarker;
            TRI_voc_size_t newMarkerSize = sizeof(TRI_doc_deletion_key_marker_t);

            TRI_voc_size_t keyBodySize;
            char* keyBody;
            TRI_voc_size_t keySize;
            char didBuffer[33];

            memset(&newMarker, 0, newMarkerSize);

            sprintf(didBuffer,"%llu", (unsigned long long) oldMarker->_did);
            keySize = strlen(didBuffer) + 1;
            keyBodySize = TRI_DF_ALIGN_BLOCK(keySize);
            keyBody = (char*) TRI_Allocate(TRI_CORE_MEM_ZONE, keyBodySize, true);
            TRI_CopyString(keyBody, didBuffer, keySize);

            newMarker._rid = oldMarker->_rid;
            newMarker._tid = 0;
            newMarker._offsetKey = newMarkerSize;

            newMarker.base._size = newMarkerSize + keyBodySize;
            newMarker.base._type = TRI_DOC_MARKER_KEY_DELETION;
            newMarker.base._tick = oldMarker->base._tick;
            TRI_FillCrcKeyMarkerDatafile(df, &newMarker.base, newMarkerSize, keyBody, keyBodySize, NULL, 0);

            writeResult = TRI_WRITE(fdout, &newMarker, newMarkerSize);
            (void) writeResult;
            writeResult = TRI_WRITE(fdout, (char*) keyBody, keyBodySize);
            (void) writeResult;

            //LOG_INFO("found deletion marker, type: '%d', did: '%d', rid: '%d'", marker._type, oldMarker->_did, oldMarker->_rid);

            TRI_Free(TRI_CORE_MEM_ZONE, keyBody);

            writtenSize += newMarker.base._size;
            break;
          }

          default: {
            // copy other types without modification
            writeResult = TRI_WRITE(fdout, payload, paddedSize);
            (void) writeResult;
            writtenSize += paddedSize;
            //LOG_INFO("found marker, type: '%d'", marker._type);

          }
        }

        delete [] payload;
      }
      else if (bytesRead == 0) {
        // eof
        break;
      }
      else {
        LOG_ERROR("Could not read data from file '%s' while upgrading collection '%s'.", df->_filename, name);
        LOG_ERROR("Remove collection manually.");
        TRI_CLOSE(fd);
        TRI_CLOSE(fdout);

        TRI_DestroyVectorPointer(&files);
        ReleaseCollection(collection);

        return scope.Close(v8::False());
      }
    }

    // fill up
    if (writtenSize < fileSize) {
      const int max = 10000;
      char b[max];
      memset(b, 0, max);

      while (writtenSize + max < fileSize) {
        writeResult = TRI_WRITE(fdout, b, max);
        (void) writeResult;
        writtenSize += max;
      }

      if (writtenSize < fileSize) {
        writeResult = TRI_WRITE(fdout, b, fileSize - writtenSize);
        (void) writeResult;
      }
    }

    // file converted!
    TRI_CLOSE(fd);
    TRI_CLOSE(fdout);
  }


  int ok = TRI_ERROR_NO_ERROR;

  for (size_t j = 0; j < files._length; ++j) {
    ostringstream outfile1;
    ostringstream outfile2;

    TRI_datafile_t* df = (TRI_datafile_t*) TRI_AtVectorPointer(&files, j);
    outfile1 << df->_filename << ".old";

    ok = TRI_RenameFile(df->_filename, outfile1.str().c_str());
    if (ok != TRI_ERROR_NO_ERROR) {
      LOG_ERROR("Could not rename file '%s' while upgrading collection '%s'.", df->_filename, name);
      break;
    }

    outfile2 << df->_filename << ".new";

    ok = TRI_RenameFile(outfile2.str().c_str(), df->_filename);
    if (ok != TRI_ERROR_NO_ERROR) {
      LOG_ERROR("Could not rename file '%s' while upgrading collection '%s'.", outfile2.str().c_str(), name);
      break;
    }
  }

  //  int TRI_UnlinkFile (char const* filename);


  TRI_DestroyVectorPointer(&files);

  ReleaseCollection(collection);

  if (ok != TRI_ERROR_NO_ERROR) {
    return scope.Close(v8::False());
  }

  return scope.Close(v8::True());
}

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

  if (collection == 0) {
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

  v8::Handle<v8::Array> entries = v8::Array::New();
  result->Set(v8::String::New("entries"), entries);

  for (size_t i = 0;  i < scan._entries._length;  ++i) {
    TRI_df_scan_entry_t* entry = (TRI_df_scan_entry_t*) TRI_AtVector(&scan._entries, i);

    v8::Handle<v8::Object> o = v8::Object::New();

    o->Set(v8::String::New("position"), v8::Number::New(entry->_position));
    o->Set(v8::String::New("size"), v8::Number::New(entry->_size));
    o->Set(v8::String::New("tick"), v8::Number::New(entry->_tick));
    o->Set(v8::String::New("type"), v8::Number::New((int) entry->_type));
    o->Set(v8::String::New("status"), v8::Number::New((int) entry->_status));

    entries->Set(i, o);
  }

  TRI_DestroyDatafileScan(&scan);

  TRI_READ_UNLOCK_STATUS_VOCBASE_COL(collection);
  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                       TRI_VOCBASE_COL_T FUNCTIONS
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                              javascript functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief counts the number of documents in a result set
///
/// @FUN{@FA{collection}.count()}
///
/// Returns the number of living documents in the collection.
///
/// @EXAMPLES
///
/// @verbinclude shell-collection-count
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_CountVocbaseCol (v8::Arguments const& argv) {
  v8::HandleScope scope;

  TRI_vocbase_col_t* collection = TRI_UnwrapClass<TRI_vocbase_col_t>(argv.Holder(), WRP_VOCBASE_COL_TYPE);

  if (collection == 0) {
      TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract collection");
  }

  CollectionNameResolver resolver(collection->_vocbase);
  ReadTransactionType trx(collection->_vocbase, resolver, collection->_cid);

  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_EXCEPTION_MESSAGE(scope, res, "cannot count documents");
  }

  TRI_primary_collection_t* primary = trx.primaryCollection();

  // READ-LOCK start
  trx.lockRead();

  const TRI_voc_size_t s = primary->size(primary);

  trx.finish(res);
  // READ-LOCK end

  return scope.Close(v8::Number::New((double) s));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns information about the datafiles
///
/// @FUN{@FA{collection}.datafiles()}
///
/// Returns information about the datafiles. The collection must be unloaded.
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_DatafilesVocbaseCol (v8::Arguments const& argv) {
  v8::HandleScope scope;

  TRI_vocbase_col_t* collection = TRI_UnwrapClass<TRI_vocbase_col_t>(argv.Holder(), WRP_VOCBASE_COL_TYPE);

  if (collection == 0) {
    TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract collection");
  }

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
    journals->Set(i, v8::String::New(structure._journals._buffer[i]));
  }

  // compactors
  v8::Handle<v8::Array> compactors = v8::Array::New();
  result->Set(v8::String::New("compactors"), compactors);

  for (size_t i = 0;  i < structure._compactors._length;  ++i) {
    compactors->Set(i, v8::String::New(structure._compactors._buffer[i]));
  }

  // datafiles
  v8::Handle<v8::Array> datafiles = v8::Array::New();
  result->Set(v8::String::New("datafiles"), datafiles);

  for (size_t i = 0;  i < structure._datafiles._length;  ++i) {
    datafiles->Set(i, v8::String::New(structure._datafiles._buffer[i]));
  }

  // free result
  TRI_DestroyFileStructureCollection(&structure);

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a document
///
/// @FUN{@FA{collection}.document(@FA{document})}
///
/// The @FN{document} method finds a document given it's identifier.  It returns
/// the document. Note that the returned document contains two
/// pseudo-attributes, namely @LIT{_id} and @LIT{_rev}. @LIT{_id} contains the
/// document-handle and @LIT{_rev} the revision of the document.
///
/// An error is thrown if there @LIT{_rev} does not longer match the current
/// revision of the document.
///
/// An error is also thrown if the document does not exist.
///
/// @FUN{@FA{collection}.document(@FA{document-handle})}
///
/// As before. Instead of document a @FA{document-handle} can be passed as
/// first argument.
///
/// @EXAMPLES
///
/// Returns the document for a document-handle:
///
/// @code
/// arango> db.example.document("1432124/2873916");
/// { "_id" : "1432124/2873916", "_rev" : "2873916", "Hello" : "World" }
/// @endcode
///
/// An error is raised if the document is unknown:
///
/// @code
/// arango> db.example.document("1432124/123456");
/// JavaScript exception in file '(arango)' at 1,12:
///   [ArangoError 1202: document not found: document not found]
/// !db.example.document("1432124/123456");
/// !           ^
/// @endcode
///
/// An error is raised if the handle is invalid:
///
/// @code
/// arango> db.example.document("12345");
/// JavaScript exception in file '(arango)' at 1,12:
///   [ArangoError 10: bad parameter: <document-identifier> must be a document identifier]
/// !db.example.document("12345");
/// !           ^
/// @endcode
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_DocumentVocbaseCol (v8::Arguments const& argv) {
  return DocumentVocbaseCol(true, argv);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief drops a collection
///
/// @FUN{@FA{collection}.drop()}
///
/// Drops a @FA{collection} and all its indexes.
///
/// @EXAMPLES
///
/// Drops a collection:
///
/// @verbinclude shell_collection-drop
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_DropVocbaseCol (v8::Arguments const& argv) {
  v8::HandleScope scope;
  int res;

  TRI_vocbase_col_t* collection = TRI_UnwrapClass<TRI_vocbase_col_t>(argv.Holder(), WRP_VOCBASE_COL_TYPE);

  if (collection == 0) {
    TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract collection");
  }
  
  PREVENT_EMBEDDED_TRANSACTION(scope);  

  res = TRI_DropCollectionVocBase(collection->_vocbase, collection);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_EXCEPTION_MESSAGE(scope, res, "cannot drop collection");
  }

  return scope.Close(v8::Undefined());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief drops an index
///
/// @FUN{@FA{collection}.dropIndex(@FA{index})}
///
/// Drops the index. If the index does not exist, then @LIT{false} is
/// returned. If the index existed and was dropped, then @LIT{true} is
/// returned. Note that you cannot drop the primary index.
///
/// @FUN{@FA{collection}.dropIndex(@FA{index-handle})}
///
/// Same as above. Instead of an index an index handle can be given.
///
/// @EXAMPLES
///
/// @verbinclude shell_index-drop-index
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_DropIndexVocbaseCol (v8::Arguments const& argv) {
  v8::HandleScope scope;
  
  PREVENT_EMBEDDED_TRANSACTION(scope);  

  v8::Handle<v8::Object> err;
  TRI_vocbase_col_t const* collection = UseCollection(argv.Holder(), &err);

  if (collection == 0) {
    return scope.Close(v8::ThrowException(err));
  }
  
  CollectionNameResolver resolver(collection->_vocbase);

  TRI_primary_collection_t* primary = collection->_collection;

  if (! TRI_IS_DOCUMENT_COLLECTION(collection->_type)) {
    ReleaseCollection(collection);
    TRI_V8_EXCEPTION_INTERNAL(scope, "unknown collection type");
  }

  TRI_document_collection_t* document = (TRI_document_collection_t*) primary;

  if (argv.Length() != 1) {
    ReleaseCollection(collection);
    TRI_V8_EXCEPTION_USAGE(scope, "dropIndex(<index-handle>)");
  }

  TRI_index_t* idx = TRI_LookupIndexByHandle(resolver, collection, argv[0], true, &err);

  if (idx == 0) {
    if (err.IsEmpty()) {
      ReleaseCollection(collection);
      return scope.Close(v8::False());
    }
    else {
      ReleaseCollection(collection);
      return scope.Close(v8::ThrowException(err));
    }
  }

  if (idx->_iid == 0) {
    ReleaseCollection(collection);
    return scope.Close(v8::False());
  }

  // .............................................................................
  // inside a write transaction
  // .............................................................................

  bool ok = TRI_DropIndexDocumentCollection(document, idx->_iid);

  // .............................................................................
  // outside a write transaction
  // .............................................................................

  ReleaseCollection(collection);
  return scope.Close(ok ? v8::True() : v8::False());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that a cap constraint exists
///
/// @FUN{@FA{collection}.ensureCapConstraint(@FA{size})}
///
/// Creates a size restriction aka cap for the collection of @FA{size}.  If the
/// restriction is in place and the (@FA{size} plus one) document is added to
/// the collection, then the least recently created or updated document is
/// removed.
///
/// Note that at most one cap constraint is allowed per collection.
///
/// Note that the collection should be empty. Otherwise the behavior is
/// undefined, i. e., it is undefined which documents will be removed first.
///
/// Note that this does not imply any restriction of the number of revisions
/// of documents.
///
/// @EXAMPLES
///
/// Restrict the number of document to at most 10 documents:
///
/// @verbinclude ensure-cap-constraint
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_EnsureCapConstraintVocbaseCol (v8::Arguments const& argv) {
  v8::HandleScope scope;
  
  PREVENT_EMBEDDED_TRANSACTION(scope);  

  v8::Handle<v8::Object> err;
  TRI_vocbase_col_t const* collection = UseCollection(argv.Holder(), &err);

  if (collection == 0) {
    return scope.Close(v8::ThrowException(err));
  }

  TRI_primary_collection_t* primary = collection->_collection;

  if (! TRI_IS_DOCUMENT_COLLECTION(collection->_type)) {
    ReleaseCollection(collection);
    TRI_V8_EXCEPTION_INTERNAL(scope, "unknown collection type");
  }

  TRI_document_collection_t* document = (TRI_document_collection_t*) primary;
  TRI_index_t* idx = 0;
  bool created;

  if (argv.Length() == 1) {
    size_t size = (size_t) TRI_ObjectToDouble(argv[0]);

    if (size <= 0) {
      ReleaseCollection(collection);
      TRI_V8_EXCEPTION_PARAMETER(scope, "<size> must be at least 1");
    }

    idx = TRI_EnsureCapConstraintDocumentCollection(document, size, &created);
  }

  // .............................................................................
  // error case
  // .............................................................................

  else {
    ReleaseCollection(collection);
    TRI_V8_EXCEPTION_USAGE(scope, "ensureCapConstraint(<size>)");
  }

  if (idx == 0) {
    ReleaseCollection(collection);
    TRI_V8_EXCEPTION_MESSAGE(scope, TRI_errno(), "index could not be created");
  }

  ResourceHolder holder;

  TRI_json_t* json = idx->json(idx, primary);

  if (! holder.registerJson(TRI_CORE_MEM_ZONE, json)) {
    ReleaseCollection(collection);
    TRI_V8_EXCEPTION_MEMORY(scope);
  }

  v8::Handle<v8::Value> index = IndexRep(&primary->base, json);

  if (index->IsObject()) {
    index->ToObject()->Set(v8::String::New("isNewlyCreated"), created ? v8::True() : v8::False());
  }

  ReleaseCollection(collection);
  return scope.Close(index);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that a bitarray index exists
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> EnsureBitarray (v8::Arguments const& argv, bool supportUndef) {

  v8::HandleScope scope;
  bool ok;
  string errorString;
  int errorCode;
  TRI_index_t* bitarrayIndex = 0;
  bool indexCreated;
  v8::Handle<v8::Value> theIndex;
  
  PREVENT_EMBEDDED_TRANSACTION(scope);  

  // .............................................................................
  // Check that we have a valid collection
  // .............................................................................

  v8::Handle<v8::Object> err;
  const TRI_vocbase_col_t* collection = UseCollection(argv.Holder(), &err);

  if (collection == 0) {
    return scope.Close(v8::ThrowException(err));
  }


  // .............................................................................
  // Check collection type
  // .............................................................................

  TRI_primary_collection_t* primary = collection->_collection;

  if (! TRI_IS_DOCUMENT_COLLECTION(collection->_type)) {
    ReleaseCollection(collection);
    TRI_V8_EXCEPTION_INTERNAL(scope, "unknown collection type");
  }

  TRI_document_collection_t* document = (TRI_document_collection_t*) primary;

  // .............................................................................
  // Ensure that there is at least one string parameter sent to this method
  // .............................................................................

  if ( (argv.Length() < 2) || (argv.Length() % 2 != 0) ) {
    LOG_WARNING("bitarray index creation failed -- invalid parameters (require key_1,values_1,...,key_n,values_n)");
    ReleaseCollection(collection);
    TRI_V8_EXCEPTION_USAGE(scope, "ensureBitarray(path 1, <list of values 1>, <path 2>, <list of values 2>, ...)");
  }


  // .............................................................................
  // Create a list of paths, these will be used to create a list of shapes
  // which will be used by the index.
  // .............................................................................

  TRI_vector_pointer_t attributes;
  TRI_InitVectorPointer(&attributes, TRI_CORE_MEM_ZONE);
  TRI_vector_pointer_t values;
  TRI_InitVectorPointer(&values, TRI_CORE_MEM_ZONE);
  ok = true;

  // .............................................................................
  // Parameters into this ensureBitarray(...) method are passed pairs. That is,
  // for every attribute next to it immediately on the right we have a list. For
  // example: ensureBitarray("a",[0,1,2])
  //          ensureBitarray("a",[0,1,2,["x","y"]],
  //                         "b",["red","white",[1,2,3,[[12,13,14]]]])
  // .............................................................................

  for (int j = 0;  j < argv.Length();  ++j) {
    v8::Handle<v8::Value> argument = argv[j];

    // ...........................................................................
    // Determine if we are expecting a string (attribute) or a list (set of values)
    // ...........................................................................

    if ((j % 2) == 0) { // we are expecting a string

      if (! argument->IsString() ) {
        errorString = "invalid parameter -- expected string parameter";
        errorCode   = TRI_ERROR_BAD_PARAMETER;
        ok = false;
        break;
      }

      TRI_Utf8ValueNFC argumentString(TRI_UNKNOWN_MEM_ZONE, argument);
      char* cArgument = *argumentString == 0 ? 0 : TRI_DuplicateString(*argumentString);
      TRI_PushBackVectorPointer(&attributes, cArgument);

    }

    else { // we are expecting a value or set of values

      // .........................................................................
      // Check that the javascript argument is in fact an array (list)
      // .........................................................................

      if (! argument->IsArray() ) {
        errorString = "invalid parameter -- expected an array (list)";
        errorCode   = TRI_ERROR_BAD_PARAMETER;
        ok = false;
        break;
      }

      // .........................................................................
      // Attempt to convert the V8 javascript function argument into a TRI_json_t
      // .........................................................................

      TRI_json_t* value = TRI_ObjectToJson(argument);


      // .........................................................................
      // If the conversion from V8 value into a TRI_json_t fails, exit
      // .........................................................................

      if (value == 0) {
        errorString = "invalid parameter -- expected an array (list)";
        errorCode   = TRI_ERROR_BAD_PARAMETER;
        ok = false;
        break;
      }


      // .........................................................................
      // If the TRI_json_t is NOT a list, then exit with an error
      // .........................................................................

      if (value->_type != TRI_JSON_LIST) {
        errorString = "invalid parameter -- expected an array (list)";
        errorCode   = TRI_ERROR_BAD_PARAMETER;
        ok = false;
        break;
      }

      TRI_PushBackVectorPointer(&values, value);

    }

  }


  if (ok) {
    // ...........................................................................
    // Check that we have as many attributes as values
    // ...........................................................................

    if (attributes._length != values._length) {
      errorString = "invalid parameter -- expected an array (list)";
      errorCode   = TRI_ERROR_BAD_PARAMETER;
      ok = false;
    }
  }

  // .............................................................................
  // Actually create the index here
  // .............................................................................

  if (ok) {
    char* errorStr = 0;
    bitarrayIndex = TRI_EnsureBitarrayIndexDocumentCollection(document, &attributes, &values, supportUndef, &indexCreated, &errorCode, &errorStr);

    if (bitarrayIndex == 0) {
      if (errorStr == 0) {
         errorString = "index could not be created from Simple Collection";
      }
      else {
         errorString = string(errorStr);
         TRI_Free(TRI_CORE_MEM_ZONE, errorStr); // strings are created in the CORE MEMORY ZONE
      }
      ok = false;
    }
  }


  // .............................................................................
  // remove the memory allocated to the list of attributes and values used for the
  // specification of the index
  // .............................................................................

  for (size_t j = 0; j < attributes._length; ++j) {
    char* attribute = (char*)(TRI_AtVectorPointer(&attributes, j));
    TRI_json_t* value = (TRI_json_t*)(TRI_AtVectorPointer(&values, j));
    TRI_Free(TRI_CORE_MEM_ZONE, attribute);
    TRI_FreeJson (TRI_UNKNOWN_MEM_ZONE, value);
  }

  TRI_DestroyVectorPointer(&attributes);
  TRI_DestroyVectorPointer(&values);


  if (ok && bitarrayIndex != 0) {
    // ...........................................................................
    // Create a json represention of the index
    // ...........................................................................

    TRI_json_t* json = bitarrayIndex->json(bitarrayIndex, primary);

    if (json == NULL) {
      errorCode = TRI_ERROR_OUT_OF_MEMORY;
      errorString = "out of memory";
      ok = false;
    }

    else {
      theIndex = IndexRep(&primary->base, json);
      if (theIndex->IsObject()) {
        theIndex->ToObject()->Set(v8::String::New("isNewlyCreated"), indexCreated ? v8::True() : v8::False());
      }

      TRI_FreeJson(TRI_CORE_MEM_ZONE, json);
    }
  }


  ReleaseCollection(collection);

  if (! ok || bitarrayIndex == 0) {
    return scope.Close(v8::ThrowException(TRI_CreateErrorObject(errorCode, errorString)));
  }

  return scope.Close(theIndex);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that a bitarray index exists
///
/// @FUN{@FA{collection}.ensureBitarray(@FA{field1}, @FA{value1}, @FA{field2}, @FA{value2},...,@FA{fieldn}, @FA{valuen})}
///
/// Creates a bitarray index on documents using attributes as paths to the
/// fields (@FA{field1},..., @FA{fieldn}). A value (@FA{value1},...,@FA{valuen})
/// consists of an array of possible values that the field can take. At least
/// one field and one set of possible values must be given.
///
/// All documents, which do not have *all* of the attribute paths are ignored
/// (that is, are not part of the bitarray index, they are however stored within
/// the collection). A document which contains all of the attribute paths yet
/// has one or more values which are *not* part of the defined range of values
/// will be rejected and the document will not inserted within the
/// collection. Note that, if a bitarray index is created subsequent to
/// any documents inserted in the given collection, then the creation of the
/// index will fail if one or more documents are rejected (due to
/// attribute values being outside the designated range).
///
/// In case that the index was successfully created, the index identifier is
/// returned.
///
/// In the example below we create a bitarray index with one field and that
/// field can have the values of either `0` or `1`. Any document which has the
/// attribute `x` defined and does not have a value of `0` or `1` will be
/// rejected and therefore not inserted within the collection. Documents without
/// the attribute `x` defined will not take part in the index.
///
/// @code
/// arango> arangod> db.example.ensureBitarray("x", [0,1]);
/// {
///   "id" : "2755894/3607862",
///   "unique" : false,
///   "type" : "bitarray",
///   "fields" : [["x", [0, 1]]],
///   "undefined" : false,
///   "isNewlyCreated" : true
/// }
/// @endcode
///
/// In the example below we create a bitarray index with one field and that
/// field can have the values of either `0`, `1` or *other* (indicated by
/// `[]`). Any document which has the attribute `x` defined will take part in
/// the index. Documents without the attribute `x` defined will not take part in
/// the index.
///
/// @code
/// arangod> db.example.ensureBitarray("x", [0,1,[]]);
/// {
///   "id" : "2755894/4263222",
///   "unique" : false,
///   "type" : "bitarray",
///   "fields" : [["x", [0, 1, [ ]]]],
///   "undefined" : false,
///   "isNewlyCreated" : true
/// }
/// @endcode
///
/// In the example below we create a bitarray index with two fields. Field `x`
/// can have the values of either `0` or `1`; while field `y` can have the values
/// of `2` or `"a"`. A document which does not have *both* attributes `x` and `y`
/// will not take part within the index.  A document which does have both attributes
/// `x` and `y` defined must have the values `0` or `1` for attribute `x` and
/// `2` or `a` for attribute `y`, otherwise the document will not be inserted
/// within the collection.
///
/// @code
/// arangod> db.example.ensureBitarray("x", [0,1], "y", [2,"a"]);
/// {
///   "id" : "2755894/5246262",
///   "unique" : false,
///   "type" : "bitarray",
///   "fields" : [["x", [0, 1]], ["y", [0, 1]]],
///   "undefined" : false,
///   "isNewlyCreated" : false
/// }
/// @endcode
///
/// In the example below we create a bitarray index with two fields. Field `x`
/// can have the values of either `0` or `1`; while field `y` can have the
/// values of `2`, `"a"` or *other* . A document which does not have *both*
/// attributes `x` and `y` will not take part within the index.  A document
/// which does have both attributes `x` and `y` defined must have the values `0`
/// or `1` for attribute `x` and any value for attribute `y` will be acceptable,
/// otherwise the document will not be inserted within the collection.
///
/// @code
/// arangod> db.example.ensureBitarray("x", [0,1], "y", [2,"a",[]]);
/// {
///   "id" : "2755894/5770550",
///   "unique" : false,
///   "type" : "bitarray",
///   "fields" : [["x", [0, 1]], ["y", [2, "a", [ ]]]],
///   "undefined" : false,
///   "isNewlyCreated" : true
/// }
/// @endcode
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_EnsureBitarrayVocbaseCol (v8::Arguments const& argv) {
  return EnsureBitarray(argv, false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that a bitarray index exists
///
/// @FUN{@FA{collection}.ensureUndefBitarray(@FA{field1}, @FA{value1}, @FA{field2}, @FA{value2},...,@FA{fieldn}, @FA{valuen})}
///
/// Creates a bitarray index on all documents using attributes as paths to
/// the fields. At least one attribute and one set of possible values must be given.
/// All documents, which do not have the attribute path or
/// with one or more values that are not suitable, are ignored.
///
/// In case that the index was successfully created, the index identifier
/// is returned.
///
/// @verbinclude fluent14
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_EnsureUndefBitarrayVocbaseCol (v8::Arguments const& argv) {
  return EnsureBitarray(argv, true);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that a geo index exists
///
/// @FUN{@FA{collection}.ensureGeoIndex(@FA{location})}
///
/// Creates a geo-spatial index on all documents using @FA{location} as path to
/// the coordinates. The value of the attribute must be a list with at least two
/// double values. The list must contain the latitude (first value) and the
/// longitude (second value). All documents, which do not have the attribute
/// path or with value that are not suitable, are ignored.
///
/// In case that the index was successfully created, the index identifier is
/// returned.
///
/// @FUN{@FA{collection}.ensureGeoIndex(@FA{location}, @LIT{true})}
///
/// As above which the exception, that the order within the list is longitude
/// followed by latitude. This corresponds to the format described in
///
/// http://geojson.org/geojson-spec.html#positions
///
/// @FUN{@FA{collection}.ensureGeoIndex(@FA{latitude}, @FA{longitude})}
///
/// Creates a geo-spatial index on all documents using @FA{latitude} and
/// @FA{longitude} as paths the latitude and the longitude. The value of the
/// attribute @FA{latitude} and of the attribute @FA{longitude} must a
/// double. All documents, which do not have the attribute paths or which values
/// are not suitable, are ignored.
///
/// In case that the index was successfully created, the index identifier
/// is returned.
///
/// @EXAMPLES
///
/// Create an geo index for a list attribute:
///
/// @verbinclude ensure-geo-index-list
///
/// Create an geo index for a hash array attribute:
///
/// @verbinclude ensure-geo-index-array
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_EnsureGeoIndexVocbaseCol (v8::Arguments const& argv) {
  return EnsureGeoIndexVocbaseCol(argv, false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that a geo constraint exists
///
/// @FUN{@FA{collection}.ensureGeoConstraint(@FA{location}, @FA{ignore-null})}
///
/// @FUN{@FA{collection}.ensureGeoConstraint(@FA{location}, @LIT{true}, @FA{ignore-null})}
///
/// @FUN{@FA{collection}.ensureGeoConstraint(@FA{latitude}, @FA{longitude}, @FA{ignore-null})}
///
/// Works like @FN{ensureGeoIndex} but requires that the documents contain
/// a valid geo definition. If @FA{ignore-null} is true, then documents with
/// a null in @FA{location} or at least one null in @FA{latitude} or
/// @FA{longitude} are ignored.
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_EnsureGeoConstraintVocbaseCol (v8::Arguments const& argv) {
  return EnsureGeoIndexVocbaseCol(argv, true);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that a unique constraint exists
///
/// @FUN{ensureUniqueConstraint(@FA{field1}, @FA{field2}, ...,@FA{fieldn})}
///
/// Creates a unique hash index on all documents using @FA{field1}, @FA{field2},
/// ... as attribute paths. At least one attribute path must be given.
///
/// When a unique constraint is in effect for a collection, then all documents
/// which contain the given attributes must differ in the attribute
/// values. Creating a new document or updating a document will fail, if the
/// uniqueness is violated. If any attribute value is null for a document, this
/// document is ignored by the index.
///
/// Note that non-existing attribute paths in a document are treated as if the
/// value were @LIT{null}.
///
/// In case that the index was successfully created, the index identifier is
/// returned.
///
/// @EXAMPLES
///
/// @verbinclude shell-index-create-unique-constraint
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_EnsureUniqueConstraintVocbaseCol (v8::Arguments const& argv) {
  return EnsurePathIndex("ensureUniqueConstraint", argv, true, true, TRI_IDX_TYPE_HASH_INDEX);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a unique constraint
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_LookupUniqueConstraintVocbaseCol (v8::Arguments const& argv) {
  return EnsurePathIndex("lookupUniqueConstraint", argv, true, false, TRI_IDX_TYPE_HASH_INDEX);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that a hash index exists
///
/// @FUN{ensureHashIndex(@FA{field1}, @FA{field2}, ...,@FA{fieldn})}
///
/// Creates a non-unique hash index on all documents using @FA{field1}, @FA{field2},
/// ... as attribute paths. At least one attribute path must be given.
///
/// Note that non-existing attribute paths in a document are treated as if the
/// value were @LIT{null}.
///
/// In case that the index was successfully created, the index identifier
/// is returned.
///
/// @EXAMPLES
///
/// @verbinclude shell-index-create-hash-index
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_EnsureHashIndexVocbaseCol (v8::Arguments const& argv) {
  return EnsurePathIndex("ensureHashIndex", argv, false, true, TRI_IDX_TYPE_HASH_INDEX);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a hash index
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_LookupHashIndexVocbaseCol (v8::Arguments const& argv) {
  return EnsurePathIndex("lookupHashIndex", argv, false, false, TRI_IDX_TYPE_HASH_INDEX);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that a priority queue index exists
///
/// @FUN{ensurePQIndex(@FA{field1})}
///
/// Creates a priority queue index on all documents using attributes as paths to
/// the fields. Currently only supports one attribute of the type double.
/// All documents, which do not have the attribute path are ignored.
///
/// In case that the index was successfully created, the index identifier
/// is returned.
///
/// @verbinclude fluent14
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_EnsurePriorityQueueIndexVocbaseCol (v8::Arguments const& argv) {
  v8::HandleScope scope;
  bool created = false;
  TRI_index_t* idx;

  PREVENT_EMBEDDED_TRANSACTION(scope);  

  // .............................................................................
  // Check that we have a valid collection
  // .............................................................................

  v8::Handle<v8::Object> err;
  TRI_vocbase_col_t const* collection = UseCollection(argv.Holder(), &err);

  if (collection == 0) {
    return scope.Close(v8::ThrowException(err));
  }

  // .............................................................................
  // Check collection type
  // .............................................................................

  TRI_primary_collection_t* primary = collection->_collection;

  if (! TRI_IS_DOCUMENT_COLLECTION(collection->_type)) {
    ReleaseCollection(collection);
    TRI_V8_EXCEPTION_INTERNAL(scope, "unknown collection type");
  }

  TRI_document_collection_t* document = (TRI_document_collection_t*) primary;

  // .............................................................................
  // Return string when there is an error of some sort.
  // .............................................................................

  string errorString;

  // .............................................................................
  // Ensure that there is at least one string parameter sent to this method
  // .............................................................................

  if (argv.Length() != 1) {
    ReleaseCollection(collection);

    errorString = "one string parameter required for the ensurePQIndex(...) command";
    return scope.Close(v8::String::New(errorString.c_str(),errorString.length()));
  }


  // .............................................................................
  // Create a list of paths, these will be used to create a list of shapes
  // which will be used by the priority queue index.
  // .............................................................................

  TRI_vector_pointer_t attributes;
  TRI_InitVectorPointer(&attributes, TRI_CORE_MEM_ZONE);

  int res = AttributeNamesFromArguments(argv, &attributes, 0, argv.Length(), errorString);

  // .............................................................................
  // Some sort of error occurred -- display error message and abort index creation
  // (or index retrieval).
  // .............................................................................

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_DestroyVectorPointer(&attributes);

    ReleaseCollection(collection);
    TRI_V8_EXCEPTION_MESSAGE(scope, res, errorString);
  }

  // .............................................................................
  // Actually create the index here. Note that priority queue is never unique.
  // .............................................................................

  idx = TRI_EnsurePriorityQueueIndexDocumentCollection(document, &attributes, false, &created);

  // .............................................................................
  // Remove the memory allocated to the list of attributes used for the hash index
  // .............................................................................

  TRI_FreeContentVectorPointer(TRI_CORE_MEM_ZONE, &attributes);
  TRI_DestroyVectorPointer(&attributes);

  if (idx == 0) {
    ReleaseCollection(collection);
    return scope.Close(v8::String::New("Priority Queue index could not be created"));
  }

  // .............................................................................
  // Return the newly assigned index identifier
  // .............................................................................

  TRI_json_t* json = idx->json(idx, primary);

  v8::Handle<v8::Value> index = IndexRep(&primary->base, json);
  TRI_FreeJson(TRI_CORE_MEM_ZONE, json);

  if (index->IsObject()) {
    index->ToObject()->Set(v8::String::New("isNewlyCreated"), created ? v8::True() : v8::False());
  }

  ReleaseCollection(collection);
  return scope.Close(index);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that a skiplist index exists
///
/// @FUN{ensureUniqueSkiplist(@FA{field1}, @FA{field2}, ...,@FA{fieldn})}
///
/// Creates a skiplist index on all documents using attributes as paths to
/// the fields. At least one attribute must be given.
/// All documents, which do not have the attribute path or
/// with ore or more values that are not suitable, are ignored.
///
/// In case that the index was successfully created, the index identifier
/// is returned.
///
/// @verbinclude unique-skiplist
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_EnsureUniqueSkiplistVocbaseCol (v8::Arguments const& argv) {
  return EnsurePathIndex("ensureUniqueSkiplist", argv, true, true, TRI_IDX_TYPE_SKIPLIST_INDEX);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a skiplist index
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_LookupUniqueSkiplistVocbaseCol (v8::Arguments const& argv) {
  return EnsurePathIndex("lookupUniqueSkiplist", argv, true, false, TRI_IDX_TYPE_SKIPLIST_INDEX);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that a multi skiplist index exists
///
/// @FUN{ensureSkiplist(@FA{field1}, @FA{field2}, ...,@FA{fieldn})}
///
/// Creates a multi skiplist index on all documents using attributes as paths to
/// the fields. At least one attribute must be given.
/// All documents, which do not have the attribute path or
/// with ore or more values that are not suitable, are ignored.
///
/// In case that the index was successfully created, the index identifier
/// is returned.
///
/// @verbinclude multi-skiplist
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_EnsureSkiplistVocbaseCol (v8::Arguments const& argv) {
  return EnsurePathIndex("ensureSkiplist", argv, false, true, TRI_IDX_TYPE_SKIPLIST_INDEX);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a multi skiplist index
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_LookupSkiplistVocbaseCol (v8::Arguments const& argv) {
  return EnsurePathIndex("lookupSkiplist", argv, false, false, TRI_IDX_TYPE_SKIPLIST_INDEX);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that a fulltext index exists
///
/// @FUN{ensureFulltextIndex(@FA{field}, @FA{minWordLength}}
///
/// Creates a fulltext index on all documents on attribute @FA{field}.
/// All documents, which do not have the attribute @FA{field} or that have a
/// non-textual value inside their @FA{field} attribute are ignored.
///
/// The minimum length of words that are indexed can be specified with the
/// @FA{minWordLength} parameter. Words shorter than @FA{minWordLength}
/// characters will not be indexed. @FA{minWordLength} has a default value of 2,
/// but this value might be changed in future versions of ArangoDB. It is thus
/// recommended to explicitly specify this value
///
/// In case that the index was successfully created, the index identifier
/// is returned.
///
/// @verbinclude fulltext
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_EnsureFulltextIndexVocbaseCol (v8::Arguments const& argv) {
  return EnsureFulltextIndex(argv, true);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a fulltext index
///
/// @FUN{lookupFulltextIndex(@FA{field}}
///
/// Checks whether a fulltext index on the give attribute @FA{field} exists.
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_LookupFulltextIndexVocbaseCol (v8::Arguments const& argv) {
  return EnsureFulltextIndex(argv, false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the figures of a collection
///
/// @FUN{@FA{collection}.figures()}
///
/// Returns an object containing all collection figures.
///
/// - @LIT{alive.count}: The number of living documents.
/// - @LIT{alive.size}: The total size in bytes used by all
///   living documents.
/// - @LIT{dead.count}: The number of dead documents.
/// - @LIT{dead.size}: The total size in bytes used by all
///   dead documents.
/// - @LIT{dead.deletion}: The total number of deletion markers.
/// - @LIT{datafiles.count}: The number of active datafiles.
/// - @LIT{datafiles.fileSize}: The total filesize of the active datafiles.
/// - @LIT{journals.count}: The number of journal files.
/// - @LIT{journals.fileSize}: The total filesize of the journal files.
/// - @LIT{shapes.count}: The total number of shapes used in the collection
///   (this includes shapes that are not in use anymore)
///
/// @EXAMPLES
///
/// @verbinclude shell_collection-figures
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_FiguresVocbaseCol (v8::Arguments const& argv) {
  v8::HandleScope scope;

  TRI_vocbase_col_t* collection = TRI_UnwrapClass<TRI_vocbase_col_t>(argv.Holder(), WRP_VOCBASE_COL_TYPE);

  if (collection == 0) {
    TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract collection");
  }

  v8::Handle<v8::Object> result = v8::Object::New();

  CollectionNameResolver resolver(collection->_vocbase);
  ReadTransactionType trx(collection->_vocbase, resolver, collection->_cid);

  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_EXCEPTION_MESSAGE(scope, res, "cannot fetch figures");
  }

  // READ-LOCK start
  trx.lockRead();

  TRI_primary_collection_t* primary = collection->_collection;
  TRI_doc_collection_info_t* info = primary->figures(primary);

  res = trx.finish(res);
  // READ-LOCK end

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_EXCEPTION_MESSAGE(scope, res, "cannot fetch figures");
  }

  if (info == NULL) {
    TRI_V8_EXCEPTION_MEMORY(scope);
  }

  v8::Handle<v8::Object> alive = v8::Object::New();

  result->Set(v8::String::New("alive"), alive);
  alive->Set(v8::String::New("count"), v8::Number::New(info->_numberAlive));
  alive->Set(v8::String::New("size"), v8::Number::New(info->_sizeAlive));

  v8::Handle<v8::Object> dead = v8::Object::New();

  result->Set(v8::String::New("dead"), dead);
  dead->Set(v8::String::New("count"), v8::Number::New(info->_numberDead));
  dead->Set(v8::String::New("size"), v8::Number::New(info->_sizeDead));
  dead->Set(v8::String::New("deletion"), v8::Number::New(info->_numberDeletion));

  // datafile info
  v8::Handle<v8::Object> dfs = v8::Object::New();

  result->Set(v8::String::New("datafiles"), dfs);
  dfs->Set(v8::String::New("count"), v8::Number::New(info->_numberDatafiles));
  dfs->Set(v8::String::New("fileSize"), v8::Number::New(info->_datafileSize));

  // journal info
  v8::Handle<v8::Object> js = v8::Object::New();

  result->Set(v8::String::New("journals"), js);
  js->Set(v8::String::New("count"), v8::Number::New(info->_numberJournalfiles));
  js->Set(v8::String::New("fileSize"), v8::Number::New(info->_journalfileSize));

  // shape info
  v8::Handle<v8::Object> shapes = v8::Object::New();
  result->Set(v8::String::New("shapes"), shapes);
  shapes->Set(v8::String::New("count"), v8::Number::New(info->_numberShapes));
  
  // attributes info
  v8::Handle<v8::Object> attributes = v8::Object::New();
  result->Set(v8::String::New("attributes"), attributes);
  attributes->Set(v8::String::New("count"), v8::Number::New(info->_numberAttributes));

  TRI_Free(TRI_UNKNOWN_MEM_ZONE, info);

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns information about the indexes
///
/// @FUN{getIndexes()}
///
/// Returns a list of all indexes defined for the collection.
///
/// @EXAMPLES
///
/// @verbinclude shell_index-read-all
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_GetIndexesVocbaseCol (v8::Arguments const& argv) {
  v8::HandleScope scope;

  TRI_vocbase_col_t* collection = TRI_UnwrapClass<TRI_vocbase_col_t>(argv.Holder(), WRP_VOCBASE_COL_TYPE);

  if (collection == 0) {
      TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract collection");
  }

  CollectionNameResolver resolver(collection->_vocbase);
  ReadTransactionType trx(collection->_vocbase, resolver, collection->_cid);

  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_EXCEPTION_MESSAGE(scope, res, "cannot get indexes");
  }

  TRI_document_collection_t* document = (TRI_document_collection_t*) trx.primaryCollection();
  TRI_collection_t* c = (TRI_collection_t*) &(document->base.base);

  // READ-LOCK start
  trx.lockRead();

  // get list of indexes
  TRI_vector_pointer_t* indexes = TRI_IndexesDocumentCollection(document);

  trx.finish(res);
  // READ-LOCK end

  if (! indexes) {
    TRI_V8_EXCEPTION_MEMORY(scope);
  }

  v8::Handle<v8::Array> result = v8::Array::New();

  uint32_t n = (uint32_t) indexes->_length;

  for (uint32_t i = 0, j = 0;  i < n;  ++i) {
    TRI_json_t* idx = (TRI_json_t*) indexes->_buffer[i];

    if (idx != NULL) {
      result->Set(j++, IndexRep(c, idx));
      TRI_FreeJson(TRI_CORE_MEM_ZONE, idx);
    }
  }

  TRI_FreeVectorPointer(TRI_CORE_MEM_ZONE, indexes);

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief loads a collection
///
/// @FUN{@FA{collection}.load()}
///
/// Loads a collection into memory.
///
/// @EXAMPLES
///
/// @verbinclude shell_collection-load
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_LoadVocbaseCol (v8::Arguments const& argv) {
  v8::HandleScope scope;

  v8::Handle<v8::Object> err;
  TRI_vocbase_col_t const* collection = UseCollection(argv.Holder(), &err);

  if (collection == 0) {
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

  if (collection == 0) {
    TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract collection");
  }

  // this copies the name into a new place so we can safely access it later
  // if we wouldn't do this, we would risk other threads modifying the name while
  // we're reading it
  char* name = TRI_GetCollectionNameByIdVocBase(collection->_vocbase, collection->_cid);
  if (name == 0) {
    return scope.Close(v8::Undefined());
  }

  v8::Handle<v8::Value> result = v8::String::New(name);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, name);

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief gets or sets the properties of a collection
///
/// @FUN{@FA{collection}.properties()}
///
/// Returns an object containing all collection properties.
///
/// - @LIT{waitForSync}: If @LIT{true} creating a document will only return
///   after the data was synced to disk.
///
/// - @LIT{journalSize} : The size of the journal in bytes.
///
/// - @LIT{isVolatile}: If @LIT{true} then the collection data will be
///   kept in memory only and ArangoDB will not write or sync the data
///   to disk.
///
/// - @LIT{keyOptions} (optional) additional options for key generation. This is
///   a JSON array containing the following attributes (note: some of the
///   attributes are optional):
///   - @LIT{type}: the type of the key generator used for the collection.
///   - @LIT{allowUserKeys}: if set to @LIT{true}, then it is allowed to supply
///     own key values in the @LIT{_key} attribute of a document. If set to
///     @LIT{false}, then the key generator will solely be responsible for
///     generating keys and supplying own key values in the @LIT{_key} attribute
///     of documents is considered an error.
///   - @LIT{increment}: increment value for @LIT{autoincrement} key generator.
///     Not used for other key generator types.
///   - @LIT{offset}: initial offset value for @LIT{autoincrement} key generator.
///     Not used for other key generator types.
///
/// @FUN{@FA{collection}.properties(@FA{properties})}
///
/// Changes the collection properties. @FA{properties} must be a object with
/// one or more of the following attribute(s):
///
/// - @LIT{waitForSync}: If @LIT{true} creating a document will only return
///   after the data was synced to disk.
///
/// - @LIT{journalSize} : The size of the journal in bytes.
///
/// Note that it is not possible to change the journal size after the journal or
/// datafile has been created. Changing this parameter will only effect newly
/// created journals. Also note that you cannot lower the journal size to less
/// then size of the largest document already stored in the collection.
///
/// Note: some other collection properties, such as @LIT{type}, @LIT{isVolatile},
/// or @LIT{keyOptions} cannot be changed once the collection is created.
///
/// @EXAMPLES
///
/// Read all properties
///
/// @verbinclude shell_collection-properties
///
/// Change a property
///
/// @verbinclude shell_collection-properties-change
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_PropertiesVocbaseCol (v8::Arguments const& argv) {
  v8::HandleScope scope;
  TRI_v8_global_t* v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();

  v8::Handle<v8::Object> err;
  TRI_vocbase_col_t const* collection = UseCollection(argv.Holder(), &err);

  if (collection == 0) {
    return scope.Close(v8::ThrowException(err));
  }

  TRI_primary_collection_t* primary = collection->_collection;
  TRI_collection_t* base = &primary->base;

  if (! TRI_IS_DOCUMENT_COLLECTION(base->_info._type)) {
    ReleaseCollection(collection);
    TRI_V8_EXCEPTION_INTERNAL(scope, "unknown collection type");
  }

  TRI_document_collection_t* document = (TRI_document_collection_t*) primary;

  // check if we want to change some parameters
  if (0 < argv.Length()) {
    v8::Handle<v8::Value> par = argv[0];

    if (par->IsObject()) {
      v8::Handle<v8::Object> po = par->ToObject();

      // get the old values
      TRI_LOCK_JOURNAL_ENTRIES_DOC_COLLECTION(document);

      bool waitForSync = base->_info._waitForSync;
      size_t maximalSize = base->_info._maximalSize;

      TRI_UNLOCK_JOURNAL_ENTRIES_DOC_COLLECTION(document);

      // extract sync flag
      if (po->Has(v8g->WaitForSyncKey)) {
        waitForSync = TRI_ObjectToBoolean(po->Get(v8g->WaitForSyncKey));
      }

      // extract the journal size
      if (po->Has(v8g->JournalSizeKey)) {
        maximalSize = TRI_ObjectToDouble(po->Get(v8g->JournalSizeKey));

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

      newParameter._maximalSize = maximalSize;
      newParameter._waitForSync = waitForSync;

      // try to write new parameter to file
      int res = TRI_UpdateCollectionInfo(base->_vocbase, base, &newParameter);

      if (res != TRI_ERROR_NO_ERROR) {
        ReleaseCollection(collection);
        TRI_V8_EXCEPTION(scope, res);
      }
    }
  }

  // return the current parameter set
  v8::Handle<v8::Object> result = v8::Object::New();

  if (TRI_IS_DOCUMENT_COLLECTION(base->_info._type)) {
    TRI_json_t* keyOptions = primary->_keyGenerator->toJson(primary->_keyGenerator);

    result->Set(v8g->IsVolatileKey, base->_info._isVolatile ? v8::True() : v8::False());
    result->Set(v8g->IsSystemKey, base->_info._isSystem ? v8::True() : v8::False());
    result->Set(v8g->JournalSizeKey, v8::Number::New(base->_info._maximalSize));
    if (keyOptions != 0) {
      result->Set(v8g->KeyOptionsKey, TRI_ObjectJson(keyOptions)->ToObject());
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, keyOptions);
    }
    else {
      result->Set(v8g->KeyOptionsKey, v8::Array::New());
    }
    result->Set(v8g->WaitForSyncKey, base->_info._waitForSync ? v8::True() : v8::False());
  }

  ReleaseCollection(collection);
  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes a document
///
/// @FUN{@FA{collection}.remove(@FA{document})}
///
/// Removes a document. If there is revision mismatch, then an error is thrown.
///
/// @FUN{@FA{collection}.remove(@FA{document}, true)}
///
/// Removes a document. If there is revision mismatch, then mismatch is ignored
/// and document is deleted. The function returns @LIT{true} if the document
/// existed and was deleted. It returns @LIT{false}, if the document was already
/// deleted.
///
/// @FUN{@FA{collection}.remove(@FA{document}, true, @FA{waitForSync})}
///
/// The optional @FA{waitForSync} parameter can be used to force synchronisation
/// of the document deletion operation to disk even in case that the
/// @LIT{waitForSync} flag had been disabled for the entire collection.  Thus,
/// the @FA{waitForSync} parameter can be used to force synchronisation of just
/// specific operations. To use this, set the @FA{waitForSync} parameter to
/// @LIT{true}. If the @FA{waitForSync} parameter is not specified or set to
/// @LIT{false}, then the collection's default @LIT{waitForSync} behavior is
/// applied. The @FA{waitForSync} parameter cannot be used to disable
/// synchronisation for collections that have a default @LIT{waitForSync} value
/// of @LIT{true}.
///
/// @FUN{@FA{collection}.remove(@FA{document-handle}, @FA{data})}
///
/// As before. Instead of document a @FA{document-handle} can be passed as
/// first argument.
///
/// @EXAMPLES
///
/// Remove a document:
///
/// @code
/// arango> a1 = db.example.save({ a : 1 });
/// { "_id" : "116308/3449537", "_rev" : "3449537" }
/// arango> db.example.document(a1);
/// { "_id" : "116308/3449537", "_rev" : "3449537", "a" : 1 }
/// arango> db.example.remove(a1);
/// true
/// arango> db.example.document(a1);
/// JavaScript exception in file '(arango)' at 1,12: [ArangoError 1202: document not found: document not found]
/// !db.example.document(a1);
/// !           ^
/// @endcode
///
/// Remove a document with a conflict:
///
/// @code
/// arango> a1 = db.example.save({ a : 1 });
/// { "_id" : "116308/3857139", "_rev" : "3857139" }
/// arango> a2 = db.example.replace(a1, { a : 2 });
/// { "_id" : "116308/3857139", "_rev" : "3922675", "_oldRev" : 3857139 }
/// arango> db.example.remove(a1);
/// JavaScript exception in file '(arango)' at 1,18: [ArangoError 1200: conflict: cannot remove document]
/// !db.example.remove(a1);
/// !                 ^
/// arango> db.example.remove(a1, true);
/// true
/// arango> db.example.document(a1);
/// JavaScript exception in file '(arango)' at 1,12: [ArangoError 1202: document not found: document not found]
/// !db.example.document(a1);
/// !           ^
/// @endcode
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_RemoveVocbaseCol (v8::Arguments const& argv) {
  return RemoveVocbaseCol(true, argv);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief renames a collection
///
/// @FUN{@FA{collection}.rename(@FA{new-name})}
///
/// Renames a collection using the @FA{new-name}. The @FA{new-name} must not
/// already be used for a different collection. @FA{new-name} must also be a
/// valid collection name. For more information on valid collection names please refer
/// to @ref NamingConventions.
///
/// If renaming fails for any reason, an error is thrown.
///
/// @EXAMPLES
///
/// @verbinclude shell_collection-rename
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_RenameVocbaseCol (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 1) {
    TRI_V8_EXCEPTION_USAGE(scope, "rename(<name>)");
  }

  string name = TRI_ObjectToString(argv[0]);

  if (name.empty()) {
    TRI_V8_EXCEPTION_PARAMETER(scope, "<name> must be non-empty");
  }

  TRI_vocbase_col_t* collection = TRI_UnwrapClass<TRI_vocbase_col_t>(argv.Holder(), WRP_VOCBASE_COL_TYPE);

  if (collection == 0) {
    TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract collection");
  }
  
  PREVENT_EMBEDDED_TRANSACTION(scope);  

  int res = TRI_RenameCollectionVocBase(collection->_vocbase, collection, name.c_str());

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_EXCEPTION_MESSAGE(scope, res, "cannot rename collection");
  }

  return scope.Close(v8::Undefined());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief replaces a document
///
/// @FUN{@FA{collection}.replace(@FA{document}, @FA{data})}
///
/// Replaces an existing @FA{document}. The @FA{document} must be a document in
/// the current collection. This document is then replaced with the
/// @FA{data} given as second argument.
///
/// The method returns a document with the attributes @LIT{_id}, @LIT{_rev} and
/// @LIT{_oldRev}.  The attribute @LIT{_id} contains the document handle of the
/// updated document, the attribute @LIT{_rev} contains the document revision of
/// the updated document, the attribute @LIT{_oldRev} contains the revision of
/// the old (now replaced) document.
///
/// If there is a conflict, i. e. if the revision of the @LIT{document} does not
/// match the revision in the collection, then an error is thrown.
///
/// @FUN{@FA{collection}.replace(@FA{document}, @FA{data}, true)}
///
/// As before, but in case of a conflict, the conflict is ignored and the old
/// document is overwritten.
///
/// @FUN{@FA{collection}.replace(@FA{document}, @FA{data}, true, @FA{waitForSync})}
///
/// The optional @FA{waitForSync} parameter can be used to force
/// synchronisation of the document replacement operation to disk even in case
/// that the @LIT{waitForSync} flag had been disabled for the entire collection.
/// Thus, the @FA{waitForSync} parameter can be used to force synchronisation
/// of just specific operations. To use this, set the @FA{waitForSync} parameter
/// to @LIT{true}. If the @FA{waitForSync} parameter is not specified or set to
/// @LIT{false}, then the collection's default @LIT{waitForSync} behavior is
/// applied. The @FA{waitForSync} parameter cannot be used to disable
/// synchronisation for collections that have a default @LIT{waitForSync} value
/// of @LIT{true}.
///
/// @FUN{@FA{collection}.replace(@FA{document-handle}, @FA{data})}
///
/// As before. Instead of document a @FA{document-handle} can be passed as
/// first argument.
///
/// @EXAMPLES
///
/// Create and update a document:
///
/// @TINYEXAMPLE{shell_replace-document,replacing a document}
///
/// Use a document handle:
///
/// @TINYEXAMPLE{shell_replace-document-handle,replacing a document}
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_ReplaceVocbaseCol (v8::Arguments const& argv) {
  return ReplaceVocbaseCol(true, argv);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief updates a document
///
/// @FUN{@FA{collection}.update(@FA{document}, @FA{data}, @FA{overwrite}, @FA{keepNull}, @FA{waitForSync})}
///
/// Updates an existing @FA{document}. The @FA{document} must be a document in
/// the current collection. This document is then patched with the
/// @FA{data} given as second argument. The optional @FA{overwrite} parameter can
/// be used to control the behavior in case of version conflicts (see below).
/// The optional @FA{keepNull} parameter can be used to modify the behavior when
/// handling @LIT{null} values. Normally, @LIT{null} values are stored in the
/// database. By setting the @FA{keepNull} parameter to @LIT{false}, this behavior
/// can be changed so that all attributes in @FA{data} with @LIT{null} values will
/// be removed from the target document.
///
/// The optional @FA{waitForSync} parameter can be used to force
/// synchronisation of the document update operation to disk even in case
/// that the @LIT{waitForSync} flag had been disabled for the entire collection.
/// Thus, the @FA{waitForSync} parameter can be used to force synchronisation
/// of just specific operations. To use this, set the @FA{waitForSync} parameter
/// to @LIT{true}. If the @FA{waitForSync} parameter is not specified or set to
/// @LIT{false}, then the collection's default @LIT{waitForSync} behavior is
/// applied. The @FA{waitForSync} parameter cannot be used to disable
/// synchronisation for collections that have a default @LIT{waitForSync} value
/// of @LIT{true}.
///
/// The method returns a document with the attributes @LIT{_id}, @LIT{_rev} and
/// @LIT{_oldRev}.  The attribute @LIT{_id} contains the document handle of the
/// updated document, the attribute @LIT{_rev} contains the document revision of
/// the updated document, the attribute @LIT{_oldRev} contains the revision of
/// the old (now replaced) document.
///
/// If there is a conflict, i. e. if the revision of the @LIT{document} does not
/// match the revision in the collection, then an error is thrown.
///
/// @FUN{@FA{collection}.update(@FA{document}, @FA{data}, true)}
///
/// As before, but in case of a conflict, the conflict is ignored and the old
/// document is overwritten.
///
/// @FUN{@FA{collection}.update(@FA{document-handle}, @FA{data})}
///
/// As before. Instead of document a @FA{document-handle} can be passed as
/// first argument.
///
/// @EXAMPLES
///
/// Create and update a document:
///
/// @TINYEXAMPLE{shell_update-document,updating a document}
///
/// Use a document handle:
///
/// @TINYEXAMPLE{shell_update-document-handle,updating a document}
///
/// Use the keepNull parameter to remove attributes with null values:
///
/// @TINYEXAMPLE{shell_update-document-keep-null,updating a document}
///
/// Patching array values:
///
/// @TINYEXAMPLE{shell_update-document-array,updating a document}
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_UpdateVocbaseCol (v8::Arguments const& argv) {
  return UpdateVocbaseCol(true, argv);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief saves a new document
///
/// @FUN{@FA{collection}.save(@FA{data})}
///
/// Creates a new document in the @FA{collection} from the given @FA{data}. The
/// @FA{data} must be a hash array. It must not contain attributes starting
/// with @LIT{_}.
///
/// The method returns a document with the attributes @LIT{_id} and @LIT{_rev}.
/// The attribute @LIT{_id} contains the document handle of the newly created
/// document, the attribute @LIT{_rev} contains the document revision.
///
/// @FUN{@FA{collection}.save(@FA{data}, @FA{waitForSync})}
///
/// Creates a new document in the @FA{collection} from the given @FA{data} as
/// above. The optional @FA{waitForSync} parameter can be used to force
/// synchronisation of the document creation operation to disk even in case
/// that the @LIT{waitForSync} flag had been disabled for the entire collection.
/// Thus, the @FA{waitForSync} parameter can be used to force synchronisation
/// of just specific operations. To use this, set the @FA{waitForSync} parameter
/// to @LIT{true}. If the @FA{waitForSync} parameter is not specified or set to
/// @LIT{false}, then the collection's default @LIT{waitForSync} behavior is
/// applied. The @FA{waitForSync} parameter cannot be used to disable
/// synchronisation for collections that have a default @LIT{waitForSync} value
/// of @LIT{true}.
///
/// @EXAMPLES
///
/// @verbinclude shell_create-document
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_SaveVocbaseCol (v8::Arguments const& argv) {
  v8::HandleScope scope;

  TRI_vocbase_col_t* col = TRI_UnwrapClass<TRI_vocbase_col_t>(argv.Holder(), WRP_VOCBASE_COL_TYPE);

  if (col == 0) {
    TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract collection");
  }

  CollectionNameResolver resolver(col->_vocbase);
  SingleCollectionWriteTransaction<EmbeddableTransaction<V8TransactionContext>, 1> trx(col->_vocbase, resolver, col->_cid);

  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_EXCEPTION_MESSAGE(scope, res, "cannot save document");
  }

  v8::Handle<v8::Value> result;

  if ((TRI_col_type_e) col->_type == TRI_COL_TYPE_DOCUMENT) {
    result = SaveVocbaseCol(&trx, col, argv, false);
  }
  else if ((TRI_col_type_e) col->_type == TRI_COL_TYPE_EDGE) {
    result = SaveEdgeCol(&trx, col, argv, false);
  }

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief saves or replaces a document
///
/// @FUN{@FA{collection}.saveOrReplace(@FA{data})}
///
/// Creates a new document with in the @FA{collection} from the given @FA{data}
/// or replaces an existing document. The @FA{data} must be a hash array. It
/// must not contain attributes starting with `_`, expect the document key
/// stored in `_key`. If there already exists a document with the given key it
/// is replaced.
///
/// The method returns a document with the attributes `_key`, `_id` and `_rev`.
/// The attribute `_id` contains the document handle of the newly created or
/// replaced document; the attribute `_key` contains the document key; the
/// attribute `_rev` contains the document revision.
///
/// @FUN{@FA{collection}.saveOrReplace(@FA{data}, @FA{waitForSync})}
///
/// The optional @FA{waitForSync} parameter can be used to force synchronisation
/// of the document creation operation to disk even in case that the
/// `waitForSync` flag had been disabled for the entire collection.  Thus, the
/// `waitForSync` parameter can be used to force synchronisation of just
/// specific operations. To use this, set the `waitForSync` parameter to
/// `true`. If the `waitForSync` parameter is not specified or set to `false`,
/// then the collection's default `waitForSync` behavior is applied. The
/// `waitForSync` parameter cannot be used to disable synchronisation for
/// collections that have a default `waitForSync` value of `true`.
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_OUTPUT{ShellSaveOrReplace1}
/// doc = db.saveOrReplace("aardvark", { german: "Erdferkell" });
/// doc = db.saveOrReplace("aardvark", { german: "Erdferkel" });
/// @END_EXAMPLE_ARANGOSH_OUTPUT
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_SaveOrReplaceVocbaseCol (v8::Arguments const& argv) {
  v8::HandleScope scope;

  TRI_vocbase_col_t* col = TRI_UnwrapClass<TRI_vocbase_col_t>(argv.Holder(), WRP_VOCBASE_COL_TYPE);

  if (col == 0) {
    TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract collection");
  }

  size_t pos;

  if ((TRI_col_type_e) col->_type == TRI_COL_TYPE_DOCUMENT) {
    if (argv.Length() < 1) {
      TRI_V8_EXCEPTION_USAGE(scope, "saveOrReplace(<data>, [<waitForSync>]");
    }

    pos = 0;
  }
  else if ((TRI_col_type_e) col->_type == TRI_COL_TYPE_EDGE) {
    if (argv.Length() < 1) {
      TRI_V8_EXCEPTION_USAGE(scope, "saveOrReplace(<from>, <to>, <data>, [<waitForSync>]");
    }

    pos = 2;
  }
  else {
    TRI_V8_EXCEPTION_INTERNAL(scope, "unknown collection type");
  }

  ResourceHolder holder;

  TRI_voc_key_t key;
  int res = ExtractDocumentKey(argv[pos], key);

  if (res != TRI_ERROR_NO_ERROR && res != TRI_ERROR_ARANGO_DOCUMENT_KEY_MISSING) {
    TRI_V8_EXCEPTION(scope, res);
  }
  else if (key != 0) {
    holder.registerString(TRI_CORE_MEM_ZONE, key);
  }

  CollectionNameResolver resolver(col->_vocbase);
  SingleCollectionWriteTransaction<EmbeddableTransaction<V8TransactionContext>, 1> trx(col->_vocbase, resolver, col->_cid);

  res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_EXCEPTION_MESSAGE(scope, res, "cannot save document: trx.begin failed");
  }

  res = trx.lockWrite();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_EXCEPTION_MESSAGE(scope, res, "cannot save document: trx.lockWrite failed");
  }

  if (key != 0) {
    TRI_doc_mptr_t document;
    res = trx.read(&document, key);
  }
  else {
    res = TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND;
  }

  v8::Handle<v8::Value> result;

  if (res == TRI_ERROR_NO_ERROR) {
    TRI_primary_collection_t* primary = trx.primaryCollection();
    TRI_shaped_json_t* shaped = TRI_ShapedJsonV8Object(argv[pos], primary->_shaper);

    if (! holder.registerShapedJson(primary->_shaper, shaped)) {
      TRI_V8_EXCEPTION_MESSAGE(scope, TRI_errno(), "<data> cannot be converted into JSON shape");
    }

    const bool forceSync = ExtractForceSync(argv, pos + 1);

    TRI_doc_mptr_t document;
    TRI_voc_rid_t rid = 0;
    TRI_voc_rid_t actualRevision = 0;
    res = trx.updateDocument(key, &document, shaped, TRI_DOC_UPDATE_LAST_WRITE, forceSync, rid, &actualRevision);

    res = trx.finish(res);

    if (res != TRI_ERROR_NO_ERROR) {
      TRI_V8_EXCEPTION_MESSAGE(scope, res, "cannot replace document");
    }

    TRI_v8_global_t* v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();

    v8::Handle<v8::Object> r = v8::Object::New();
    r->Set(v8g->_IdKey, V8DocumentId(resolver.getCollectionName(col->_cid), document._key));
    r->Set(v8g->_RevKey, V8RevisionId(document._rid));
    r->Set(v8g->_OldRevKey, V8RevisionId(actualRevision));
    r->Set(v8g->_KeyKey, v8::String::New(document._key));

    result = r;
  }
  else if (res == TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND) {
    if ((TRI_col_type_e) col->_type == TRI_COL_TYPE_DOCUMENT) {
      result = SaveVocbaseCol(&trx, col, argv, true);
    }
    else if ((TRI_col_type_e) col->_type == TRI_COL_TYPE_EDGE) {
      result = SaveEdgeCol(&trx, col, argv, true);
    }
  }
  else {
    TRI_V8_EXCEPTION_MESSAGE(scope, res, "cannot save document: trx.read failed");
  }

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets a parameter attribute of a collection
///
/// This function does evil things so it is hidden
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_SetAttributeVocbaseCol (v8::Arguments const& argv) {
  v8::HandleScope scope;

  TRI_vocbase_col_t* collection = TRI_UnwrapClass<TRI_vocbase_col_t>(argv.Holder(), WRP_VOCBASE_COL_TYPE);

  if (collection == 0) {
    TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract collection");
  }

  if (argv.Length() != 2) {
    TRI_V8_EXCEPTION_USAGE(scope, "setAttribute(<key>, <value>)");
  }

  string key = TRI_ObjectToString(argv[0]);
  string value = TRI_ObjectToString(argv[1]);

  TRI_WRITE_LOCK_STATUS_VOCBASE_COL(collection);
  TRI_col_info_t info;
  int res = TRI_LoadCollectionInfo(collection->_path, &info, false);

  if (res == TRI_ERROR_NO_ERROR) {
    if (key == "type") {
      info._type = (TRI_col_type_e) atoi(value.c_str());
    }
    else if (key == "version") {
      info._version = atoi(value.c_str());
    }
    else {
      res = TRI_ERROR_BAD_PARAMETER;
    }

    if (res == TRI_ERROR_NO_ERROR) {
      res = TRI_SaveCollectionInfo(collection->_path, &info, collection->_vocbase->_forceSyncProperties);
    }
  }

  TRI_FreeCollectionInfoOptions(&info);

  TRI_WRITE_UNLOCK_STATUS_VOCBASE_COL(collection);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_EXCEPTION_MESSAGE(scope, res, "setAttribute failed");
  }

  return scope.Close(v8::Undefined());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the status of a collection
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_StatusVocbaseCol (v8::Arguments const& argv) {
  v8::HandleScope scope;

  TRI_vocbase_col_t* collection = TRI_UnwrapClass<TRI_vocbase_col_t>(argv.Holder(), WRP_VOCBASE_COL_TYPE);

  if (collection == 0) {
    TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract collection");
  }

  TRI_READ_LOCK_STATUS_VOCBASE_COL(collection);
  TRI_vocbase_col_status_e status = collection->_status;
  TRI_READ_UNLOCK_STATUS_VOCBASE_COL(collection);

  return scope.Close(v8::Number::New((int) status));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the revision id of a collection
///
/// @FUN{@FA{collection}.revision()}
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
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_RevisionVocbaseCol (v8::Arguments const& argv) {
  v8::HandleScope scope;

  TRI_vocbase_col_t* collection = TRI_UnwrapClass<TRI_vocbase_col_t>(argv.Holder(), WRP_VOCBASE_COL_TYPE);

  if (collection == 0) {
    TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract collection");
  }

  CollectionNameResolver resolver(collection->_vocbase);
  ReadTransactionType trx(collection->_vocbase, resolver, collection->_cid);

  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_EXCEPTION_MESSAGE(scope, res, "cannot fetch revision");
  }

  // READ-LOCK start
  trx.lockRead();
  TRI_primary_collection_t* primary = collection->_collection;
  TRI_voc_tick_t tick = primary->base._info._tick;

  trx.finish(res);
  // READ-LOCK end

  return scope.Close(V8RevisionId(tick));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief truncates a collection
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_TruncateVocbaseCol (v8::Arguments const& argv) {
  v8::HandleScope scope;

  const bool forceSync = ExtractForceSync(argv, 1);

  TRI_vocbase_col_t* col = TRI_UnwrapClass<TRI_vocbase_col_t>(argv.Holder(), WRP_VOCBASE_COL_TYPE);

  if (col == 0) {
    TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract collection");
  }
  
  CollectionNameResolver resolver(col->_vocbase);
  SingleCollectionWriteTransaction<EmbeddableTransaction<V8TransactionContext>, UINT64_MAX> trx(col->_vocbase, resolver, col->_cid);
  int res = trx.begin();
  
  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_EXCEPTION_MESSAGE(scope, res, "cannot truncate collection");
  }
  
  TRI_barrier_t* barrier = TRI_CreateBarrierElement(&(trx.primaryCollection()->_barrierList));

  if (barrier == 0) {
    TRI_V8_EXCEPTION_MEMORY(scope);
  }

  res = trx.truncate(forceSync);
  res = trx.finish(res);

  TRI_FreeBarrier(barrier);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_EXCEPTION_MESSAGE(scope, res, "cannot truncate collection");
  }

  return scope.Close(v8::Undefined());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief truncates a datafile
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_TruncateDatafileVocbaseCol (v8::Arguments const& argv) {
  v8::HandleScope scope;

  TRI_vocbase_col_t* collection = TRI_UnwrapClass<TRI_vocbase_col_t>(argv.Holder(), WRP_VOCBASE_COL_TYPE);

  if (collection == 0) {
    TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract collection");
  }

  if (argv.Length() != 2) {
    TRI_V8_EXCEPTION_USAGE(scope, "truncateDatafile(<datafile>, <size>)");
  }

  string path = TRI_ObjectToString(argv[0]);
  size_t size = TRI_ObjectToDouble(argv[1]);

  TRI_READ_LOCK_STATUS_VOCBASE_COL(collection);

  if (collection->_status != TRI_VOC_COL_STATUS_UNLOADED) {
    TRI_READ_UNLOCK_STATUS_VOCBASE_COL(collection);
    TRI_V8_EXCEPTION(scope, TRI_ERROR_ARANGO_COLLECTION_NOT_UNLOADED);
  }

  int res = TRI_TruncateDatafile(path.c_str(), size);

  TRI_READ_UNLOCK_STATUS_VOCBASE_COL(collection);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_EXCEPTION_MESSAGE(scope, res, "cannot truncate datafile");
  }

  return scope.Close(v8::Undefined());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the type of a collection
///
/// @FUN{@FA{collection}.type()}
///
/// Returns the type of a collection. Possible values are:
/// - 2: document collection
/// - 3: edge collection
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_TypeVocbaseCol (v8::Arguments const& argv) {
  v8::HandleScope scope;

  TRI_vocbase_col_t* collection = TRI_UnwrapClass<TRI_vocbase_col_t>(argv.Holder(), WRP_VOCBASE_COL_TYPE);

  if (collection == 0) {
    TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract collection");
  }

  TRI_READ_LOCK_STATUS_VOCBASE_COL(collection);
  TRI_col_type_e type = (TRI_col_type_e) collection->_type;
  TRI_READ_UNLOCK_STATUS_VOCBASE_COL(collection);

  return scope.Close(v8::Number::New((int) type));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unloads a collection
///
/// @FUN{@FA{collection}.unload()}
///
/// Starts unloading a collection from memory. Note that unloading is deferred
/// until all query have finished.
///
/// @EXAMPLES
///
/// @verbinclude shell_collection-unload
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_UnloadVocbaseCol (v8::Arguments const& argv) {
  v8::HandleScope scope;

  TRI_vocbase_col_t* collection = TRI_UnwrapClass<TRI_vocbase_col_t>(argv.Holder(), WRP_VOCBASE_COL_TYPE);

  if (collection == 0) {
    TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract collection");
  }

  int res = TRI_UnloadCollectionVocBase(collection->_vocbase, collection);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_EXCEPTION_MESSAGE(scope, res, "cannot unload collection");
  }

  return scope.Close(v8::Undefined());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the version of a collection
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_VersionVocbaseCol (v8::Arguments const& argv) {
  v8::HandleScope scope;

  TRI_vocbase_col_t* collection = TRI_UnwrapClass<TRI_vocbase_col_t>(argv.Holder(), WRP_VOCBASE_COL_TYPE);

  if (collection == 0) {
    TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract collection");
  }

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
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                           TRI_VOCBASE_T FUNCTIONS
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief selects a collection from the vocbase
///
/// @FUN{db.@FA{collection-name}}
///
/// Returns the collection with the given @FA{collection-name}. If no such
/// collection exists, create a collection named @FA{collection-name} with the
/// default properties.
///
/// @EXAMPLES
///
/// @verbinclude shell_read-collection-short-cut
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> MapGetVocBase (v8::Local<v8::String> name,
                                            const v8::AccessorInfo& info) {
  v8::HandleScope scope;

  v8::Handle<v8::Object> holder = info.Holder()->ToObject();
  TRI_vocbase_t* vocbase = TRI_UnwrapClass<TRI_vocbase_t>(holder, WRP_VOCBASE_TYPE);

  if (vocbase == 0) {
    TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract vocbase");
  }

  // convert the JavaScript string to a string
  string key = TRI_ObjectToString(name);

  if (key == "") {
    return scope.Close(v8::Handle<v8::Value>());
  }

  if (   key == "toString"
      || key == "toJSON"
      || key == "hasOwnProperty" // this prevents calling the property getter again (i.e. recursion!)
      || TRI_IsSystemCollectionName(key.c_str())) { // hide system collections
    return scope.Close(v8::Handle<v8::Value>());
  }

  if (holder->HasRealNamedProperty(name)) {
    v8::Handle<v8::Object> value = holder->GetRealNamedProperty(name)->ToObject();
    TRI_vocbase_col_t* collection = TRI_UnwrapClass<TRI_vocbase_col_t>(value, WRP_VOCBASE_COL_TYPE);

    if (collection != 0) {
      TRI_READ_LOCK_STATUS_VOCBASE_COL(collection);
      TRI_vocbase_col_status_e status = collection->_status;
      TRI_READ_UNLOCK_STATUS_VOCBASE_COL(collection);
    
      if (status != TRI_VOC_COL_STATUS_DELETED) {
        return scope.Close(value);
      }
      else {
        holder->Delete(name);
      }
    }
  }

  // look up the collection
  TRI_vocbase_col_t const* collection = TRI_LookupCollectionByNameVocBase(vocbase, key.c_str());

  if (collection == 0) {
    return scope.Close(v8::Undefined());
  }

  if (! TRI_IS_DOCUMENT_COLLECTION(collection->_type)) {
    TRI_V8_TYPE_ERROR(scope, "collection is not a document or edge collection");
  }

  v8::Handle<v8::Value> result = TRI_WrapCollection(collection);

  // TODO: when this line is uncommented, the collection names are cached.
  // but this causes problems and confusion somewhere else. Need to find the reason!
  // holder->Set(name, result);

  return scope.Close(result);
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
/// @brief returns a single collection or null
///
/// @FUN{db._collection(@FA{collection-name})}
///
/// Returns the collection with the given name or null if no such collection
/// exists.
///
/// @FUN{db._collection(@FA{collection-identifier})}
///
/// Returns the collection with the given identifier or null if no such
/// collection exists. Accessing collections by identifier is discouraged for
/// end users. End users should access collections using the collection name.
///
/// @EXAMPLES
///
/// Get a collection by name:
///
/// @verbinclude shell_read-collection-name
///
/// Get a collection by id:
///
/// @verbinclude shell_read-collection-id
///
/// Unknown collection:
///
/// @verbinclude shell_read-collection-unknown
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_CollectionVocbase (v8::Arguments const& argv) {
  v8::HandleScope scope;

  TRI_vocbase_t* vocbase = TRI_UnwrapClass<TRI_vocbase_t>(argv.Holder(), WRP_VOCBASE_TYPE);

  if (vocbase == 0) {
    TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract vocbase");
  }

  // expecting one argument
  if (argv.Length() != 1) {
    TRI_V8_EXCEPTION_USAGE(scope, "_collection(<name>|<identifier>)");
  }

  v8::Handle<v8::Value> val = argv[0];
  TRI_vocbase_col_t const* collection = 0;

  // number
  if (val->IsNumber() || val->IsNumberObject()) {
    uint64_t cid = (uint64_t) TRI_ObjectToDouble(val);

    collection = TRI_LookupCollectionByIdVocBase(vocbase, cid);
  }
  else {
    string name = TRI_ObjectToString(val);

    collection = TRI_LookupCollectionByNameVocBase(vocbase, name.c_str());
  }

  if (collection == 0) {
    return scope.Close(v8::Null());
  }

  return scope.Close(TRI_WrapCollection(collection));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns all collections
///
/// @FUN{db._collections()}
///
/// Returns all collections of the given database.
///
/// @EXAMPLES
///
/// @verbinclude shell_read-collection-all
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_CollectionsVocbase (v8::Arguments const& argv) {
  v8::HandleScope scope;

  TRI_vocbase_t* vocbase = TRI_UnwrapClass<TRI_vocbase_t>(argv.Holder(), WRP_VOCBASE_TYPE);

  if (vocbase == 0) {
    TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract vocbase");
  }

  TRI_vector_pointer_t colls = TRI_CollectionsVocBase(vocbase);

  uint32_t n = (uint32_t) colls._length;
  // already create an array of the correct size
  v8::Handle<v8::Array> result = v8::Array::New(n);

  for (uint32_t i = 0;  i < n;  ++i) {
    TRI_vocbase_col_t const* collection = (TRI_vocbase_col_t const*) colls._buffer[i];

    result->Set(i, TRI_WrapCollection(collection));
  }

  TRI_DestroyVectorPointer(&colls);

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns all collection names
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_CompletionsVocbase (v8::Arguments const& argv) {
  v8::HandleScope scope;

  TRI_vocbase_t* vocbase = TRI_UnwrapClass<TRI_vocbase_t>(argv.Holder(), WRP_VOCBASE_TYPE);

  if (vocbase == 0) {
    return scope.Close(v8::Array::New());
  }


  TRI_vector_pointer_t colls = TRI_CollectionsVocBase(vocbase);

  uint32_t n = (uint32_t) colls._length;
  uint32_t j = 0;

  v8::Handle<v8::Array> result = v8::Array::New();
  // add collection names
  for (uint32_t i = 0;  i < n;  ++i) {
    TRI_vocbase_col_t const* collection = (TRI_vocbase_col_t const*) colls._buffer[i];

    // this copies the name into a new place so we can safely access it later
    // if we wouldn't do this, we would risk other threads modifying the name while
    // we're reading it
    char* name = TRI_GetCollectionNameByIdVocBase(collection->_vocbase, collection->_cid);
    if (name != 0) {
      result->Set(j++, v8::String::New(name));
      TRI_Free(TRI_UNKNOWN_MEM_ZONE, name);
    }
  }

  TRI_DestroyVectorPointer(&colls);

  // add function names. these are hard coded
  result->Set(j++, v8::String::New("_collection()"));
  result->Set(j++, v8::String::New("_collections()"));
  result->Set(j++, v8::String::New("_create()"));
  result->Set(j++, v8::String::New("_createDocumentCollection()"));
  result->Set(j++, v8::String::New("_createEdgeCollection()"));
  result->Set(j++, v8::String::New("_createStatement()"));
  result->Set(j++, v8::String::New("_document()"));
  result->Set(j++, v8::String::New("_drop()"));
  result->Set(j++, v8::String::New("_query()"));
  result->Set(j++, v8::String::New("_remove()"));
  result->Set(j++, v8::String::New("_replace()"));
  result->Set(j++, v8::String::New("_update()"));
  result->Set(j++, v8::String::New("_version()"));

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new document or edge collection
///
/// @FUN{db._create(@FA{collection-name})}
///
/// Creates a new collection named @FA{collection-name}.
/// If the collection name already exists or if the name format is invalid, an
/// error is thrown. For more information on valid collection names please refer
/// to @ref NamingConventions.
///
/// The type of the collection is automatically determined by the object that
/// @FA{_create} is invoked with:
/// - if invoked on @LIT{db}, a document collection will be created
/// - if invoked on @LIT{edges}, an edge collection will be created
///
/// @FUN{db._create(@FA{collection-name}, @FA{properties})}
///
/// @FA{properties} must be an object with the following attributes:
///
/// - @LIT{waitForSync} (optional, default @LIT{false}): If @LIT{true} creating
///   a document will only return after the data was synced to disk.
///
/// - @LIT{journalSize} (optional, default is a @ref CommandLineArangod
///   "configuration parameter"):  The maximal size of
///   a journal or datafile.  Note that this also limits the maximal
///   size of a single object. Must be at least 1MB.
///
/// - @LIT{isSystem} (optional, default is @LIT{false}): If @LIT{true}, create a
///   system collection. In this case @FA{collection-name} should start with
///   an underscore. End users should normally create non-system collections
///   only. API implementors may be required to create system collections in
///   very special occasions, but normally a regular collection will do.
///
/// - @LIT{isVolatile} (optional, default is @LIT{false}): If @LIT{true} then the
///   collection data is kept in-memory only and not made persistent. Unloading
///   the collection will cause the collection data to be discarded. Stopping
///   or re-starting the server will also cause full loss of data in the
///   collection. Setting this option will make the resulting collection be
///   slightly faster than regular collections because ArangoDB does not
///   enforce any synchronisation to disk and does not calculate any CRC
///   checksums for datafiles (as there are no datafiles).
///
/// - @LIT{keyOptions} (optional) additional options for key generation. If
///   specified, then @LIT{keyOptions} should be a JSON array containing the
///   following attributes (note: some of them are optional):
///   - @LIT{type}: specifies the type of the key generator. The currently
///     available generators are @LIT{traditional} and @LIT{autoincrement}.
///   - @LIT{allowUserKeys}: if set to @LIT{true}, then it is allowed to supply
///     own key values in the @LIT{_key} attribute of a document. If set to
///     @LIT{false}, then the key generator will solely be responsible for
///     generating keys and supplying own key values in the @LIT{_key} attribute
///     of documents is considered an error.
///   - @LIT{increment}: increment value for @LIT{autoincrement} key generator.
///     Not used for other key generator types.
///   - @LIT{offset}: initial offset value for @LIT{autoincrement} key generator.
///     Not used for other key generator types.
///
/// @EXAMPLES
///
/// With defaults:
///
/// @verbinclude shell_create-collection
///
/// With properties:
///
/// @verbinclude shell_create-collection-properties
///
/// With a key generator:
///
/// @verbinclude shell_create-collection-keygen
///
/// With a special key option:
///
/// @verbinclude shell_create-collection-keyoptions
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_CreateVocbase (v8::Arguments const& argv) {
  return CreateVocBase(argv, TRI_COL_TYPE_DOCUMENT);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new document collection
///
/// @FUN{db._createDocumentCollection(@FA{collection-name})}
///
/// @FUN{db._createDocumentCollection(@FA{collection-name}, @FA{properties})}
///
/// Creates a new document collection named @FA{collection-name}.
/// This is an alias for @ref JS_CreateVocbase, with the difference that the
/// collection type is not automatically detected.
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_CreateDocumentCollectionVocbase (v8::Arguments const& argv) {
  return CreateVocBase(argv, TRI_COL_TYPE_DOCUMENT);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new edge collection
///
/// @FUN{db._createEdgeCollection(@FA{collection-name})}
///
/// Creates a new edge collection named @FA{collection-name}. If the
/// collection name already exists, then an error is thrown. The default value
/// for @LIT{waitForSync} is @LIT{false}.
///
/// @FUN{db._createEdgeCollection(@FA{collection-name}, @FA{properties})}
///
/// @FA{properties} must be an object with the following attributes:
///
/// - @LIT{waitForSync} (optional, default @LIT{false}): If @LIT{true} creating
///   a document will only return after the data was synced to disk.
///
/// - @LIT{journalSize} (optional, default is a @ref CommandLineArangod
///   "configuration parameter"):  The maximal size of
///   a journal or datafile.  Note that this also limits the maximal
///   size of a single object. Must be at least 1MB.
///
/// @EXAMPLES
///
/// See @ref JS_CreateVocbase for examples.
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_CreateEdgeCollectionVocbase (v8::Arguments const& argv) {
  return CreateVocBase(argv, TRI_COL_TYPE_EDGE);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes a document
///
/// @FUN{@FA{db}._remove(@FA{document})}
///
/// Removes a document. If there is revision mismatch, then an error is thrown.
///
/// @FUN{@FA{db}._remove(@FA{document}, true)}
///
/// Removes a document. If there is revision mismatch, then mismatch is ignored
/// and document is deleted. The function returns @LIT{true} if the document
/// existed and was deleted. It returns @LIT{false}, if the document was already
/// deleted.
///
/// @FUN{@FA{db}._remove(@FA{document}, true, @FA{waitForSync})}
///
/// The optional @FA{waitForSync} parameter can be used to force synchronisation
/// of the document deletion operation to disk even in case that the
/// @LIT{waitForSync} flag had been disabled for the entire collection.  Thus,
/// the @FA{waitForSync} parameter can be used to force synchronisation of just
/// specific operations. To use this, set the @FA{waitForSync} parameter to
/// @LIT{true}. If the @FA{waitForSync} parameter is not specified or set to
/// @LIT{false}, then the collection's default @LIT{waitForSync} behavior is
/// applied. The @FA{waitForSync} parameter cannot be used to disable
/// synchronisation for collections that have a default @LIT{waitForSync} value
/// of @LIT{true}.
///
/// @FUN{@FA{db}._remove(@FA{document-handle}, @FA{data})}
///
/// As before. Instead of document a @FA{document-handle} can be passed as first
/// argument.
///
/// @EXAMPLES
///
/// Remove a document:
///
/// @code
/// arango> a1 = db.example.save({ a : 1 });
/// { "_id" : "116308/4214943", "_rev" : "4214943" }
/// arango> db._remove(a1);
/// true
/// arango> db._remove(a1);
/// JavaScript exception in file '(arango)' at 1,4: [ArangoError 1202: document not found: cannot remove document]
/// !db._remove(a1);
/// !   ^
/// arango> db._remove(a1, true);
/// false
/// @endcode
///
/// Remove a document with a conflict:
///
/// @code
/// arango> a1 = db.example.save({ a : 1 });
/// { "_id" : "116308/4042634", "_rev" : "4042634" }
/// arango> a2 = db._replace(a1, { a : 2 });
/// { "_id" : "116308/4042634", "_rev" : "4108170", "_oldRev" : 4042634 }
/// arango> db._delete(a1);
/// JavaScript exception in file '(arango)' at 1,4: [ArangoError 1200: conflict: cannot delete document]
/// !db._delete(a1);
/// !   ^
/// arango> db._delete(a1, true);
/// true
/// arango> db._document(a1);
/// JavaScript exception in file '(arango)' at 1,4: [ArangoError 1202: document not found: document not found]
/// !db._document(a1);
/// !   ^
/// @endcode
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_RemoveVocbase (v8::Arguments const& argv) {
  return RemoveVocbaseCol(false, argv);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a document
///
/// @FUN{@FA{db}._document(@FA{document})}
///
/// The @FN{document} method finds a document given it's identifier.  It returns
/// the document. Note that the returned document contains two
/// pseudo-attributes, namely @LIT{_id} and @LIT{_rev}. @LIT{_id} contains the
/// document handle and @LIT{_rev} the revision of the document.
///
/// An error is thrown if there @LIT{_rev} does not longer match the current
/// revision of the document.
///
/// @FUN{@FA{db}._document(@FA{document-handle})}
///
/// As before. Instead of document a @FA{document-handle} can be passed as
/// first argument.
///
/// @EXAMPLES
///
/// Returns the document:
///
/// @verbinclude shell_read-document-db
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_DocumentVocbase (v8::Arguments const& argv) {
  return DocumentVocbaseCol(false, argv);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief replaces a document
///
/// @FUN{@FA{db}._replace(@FA{document}, @FA{data})}
///
/// The method returns a document with the attributes @LIT{_id}, @LIT{_rev} and
/// @LIT{_oldRev}.  The attribute @LIT{_id} contains the document handle of the
/// updated document, the attribute @LIT{_rev} contains the document revision of
/// the updated document, the attribute @LIT{_oldRev} contains the revision of
/// the old (now replaced) document.
///
/// If there is a conflict, i. e. if the revision of the @LIT{document} does not
/// match the revision in the collection, then an error is thrown.
///
/// @FUN{@FA{db}._replace(@FA{document}, @FA{data}, true)}
///
/// As before, but in case of a conflict, the conflict is ignored and the old
/// document is overwritten.
///
/// @FUN{@FA{db}._replace(@FA{document}, @FA{data}, true, @FA{waitForSync})}
///
/// The optional @FA{waitForSync} parameter can be used to force
/// synchronisation of the document replacement operation to disk even in case
/// that the @LIT{waitForSync} flag had been disabled for the entire collection.
/// Thus, the @FA{waitForSync} parameter can be used to force synchronisation
/// of just specific operations. To use this, set the @FA{waitForSync} parameter
/// to @LIT{true}. If the @FA{waitForSync} parameter is not specified or set to
/// @LIT{false}, then the collection's default @LIT{waitForSync} behavior is
/// applied. The @FA{waitForSync} parameter cannot be used to disable
/// synchronisation for collections that have a default @LIT{waitForSync} value
/// of @LIT{true}.
///
/// @FUN{@FA{db}._replace(@FA{document-handle}, @FA{data})}
///
/// As before. Instead of document a @FA{document-handle} can be passed as
/// first argument.
///
/// @EXAMPLES
///
/// Create and replace a document:
///
/// @TINYEXAMPLE{shell_replace-document-db,replacing a document}
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_ReplaceVocbase (v8::Arguments const& argv) {
  return ReplaceVocbaseCol(false, argv);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief update a document
///
/// @FUN{@FA{db}._update(@FA{document}, @FA{data}, @FA{overwrite}, @FA{keepNull}, @FA{waitForSync})}
///
/// Updates an existing @FA{document}. The @FA{document} must be a document in
/// the current collection. This document is then patched with the
/// @FA{data} given as second argument. The optional @FA{overwrite} parameter can
/// be used to control the behavior in case of version conflicts (see below).
/// The optional @FA{keepNull} parameter can be used to modify the behavior when
/// handling @LIT{null} values. Normally, @LIT{null} values are stored in the
/// database. By setting the @FA{keepNull} parameter to @LIT{false}, this behavior
/// can be changed so that all attributes in @FA{data} with @LIT{null} values will
/// be removed from the target document.
///
/// The optional @FA{waitForSync} parameter can be used to force
/// synchronisation of the document update operation to disk even in case
/// that the @LIT{waitForSync} flag had been disabled for the entire collection.
/// Thus, the @FA{waitForSync} parameter can be used to force synchronisation
/// of just specific operations. To use this, set the @FA{waitForSync} parameter
/// to @LIT{true}. If the @FA{waitForSync} parameter is not specified or set to
/// @LIT{false}, then the collection's default @LIT{waitForSync} behavior is
/// applied. The @FA{waitForSync} parameter cannot be used to disable
/// synchronisation for collections that have a default @LIT{waitForSync} value
/// of @LIT{true}.
///
/// The method returns a document with the attributes @LIT{_id}, @LIT{_rev} and
/// @LIT{_oldRev}. The attribute @LIT{_id} contains the document handle of the
/// updated document, the attribute @LIT{_rev} contains the document revision of
/// the updated document, the attribute @LIT{_oldRev} contains the revision of
/// the old (now replaced) document.
///
/// If there is a conflict, i. e. if the revision of the @LIT{document} does not
/// match the revision in the collection, then an error is thrown.
///
/// @FUN{@FA{db}._update(@FA{document}, @FA{data}, true)}
///
/// As before, but in case of a conflict, the conflict is ignored and the old
/// document is overwritten.
///
/// @FUN{@FA{db}._update(@FA{document-handle}, @FA{data})}
///
/// As before. Instead of document a @FA{document-handle} can be passed as
/// first argument.
///
/// @EXAMPLES
///
/// Create and update a document:
///
/// @TINYEXAMPLE{shell_update-document-db,updating a document}
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_UpdateVocbase (v8::Arguments const& argv) {
  return UpdateVocbaseCol(false, argv);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the server version string
///
/// @FUN{@FA{db}._version()}
///
/// Returns the server version string.
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_VersionVocbase (v8::Arguments const& argv) {
  v8::HandleScope scope;

  return scope.Close(v8::String::New(TRIAGENS_VERSION));
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                             SHAPED JSON FUNCTIONS
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief weak reference callback for a barrier
////////////////////////////////////////////////////////////////////////////////

static void WeakBarrierCallback (v8::Isolate* isolate,
                                 v8::Persistent<v8::Value> object,
                                 void* parameter) {
  TRI_v8_global_t* v8g = (TRI_v8_global_t*) isolate->GetData();
  TRI_barrier_blocker_t* barrier = (TRI_barrier_blocker_t*) parameter;

  LOG_TRACE("weak-callback for barrier called");

  // find the persistent handle
  v8::Persistent<v8::Value> persistent = v8g->JSBarriers[barrier];
  v8g->JSBarriers.erase(barrier);

  // dispose and clear the persistent handle
  persistent.Dispose(isolate);
  persistent.Clear();

  // free the barrier
  TRI_FreeBarrier(&barrier->base);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief selects a named attribute from the shaped json
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> MapGetNamedShapedJson (v8::Local<v8::String> name,
                                                    const v8::AccessorInfo& info) {
  v8::HandleScope scope;

  // sanity check
  v8::Handle<v8::Object> self = info.Holder();

  if (self->InternalFieldCount() <= SLOT_BARRIER) {
    TRI_V8_EXCEPTION_INTERNAL(scope, "corrupted shaped json");
  }

  // get shaped json
  void* marker = TRI_UnwrapClass<void*>(self, WRP_SHAPED_JSON_TYPE);

  if (marker == 0) {
    TRI_V8_EXCEPTION_INTERNAL(scope, "corrupted shaped json");
  }

  TRI_barrier_t* barrier = static_cast<TRI_barrier_t*>(v8::Handle<v8::External>::Cast(self->GetInternalField(SLOT_BARRIER))->Value());
  TRI_primary_collection_t* collection = barrier->_container->_collection;

  // convert the JavaScript string to a string
  const string key = TRI_ObjectToString(name);

  if (key.size() == 0) {
    // we must not throw a v8 exception here because this will cause follow up errors
    return scope.Close(v8::Handle<v8::Value>());
  }

  char const* ckey = key.c_str();

  if (ckey[0] == '_' || strchr(ckey, '.') != 0) {
    return scope.Close(v8::Handle<v8::Value>());
  }

  // get shape accessor
  TRI_shaper_t* shaper = collection->_shaper;
  TRI_shape_pid_t pid = shaper->lookupAttributePathByName(shaper, ckey);

  if (pid == 0) {
    return scope.Close(v8::Handle<v8::Value>());
  }

  TRI_shaped_json_t document;
  TRI_EXTRACT_SHAPED_JSON_MARKER(document, marker);

  TRI_shaped_json_t json;
  TRI_shape_t const* shape;

  bool ok = TRI_ExtractShapedJsonVocShaper(shaper, &document, 0, pid, &json, &shape);

  if (ok) {
    if (shape == 0) {
      return scope.Close(v8::Handle<v8::Value>());
    }
    else {
      return scope.Close(TRI_JsonShapeData(shaper, shape, json._data.data, json._data.length));
    }
  }
  else {
    // we must not throw a v8 exception here because this will cause follow up errors
    return scope.Close(v8::Handle<v8::Value>());
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief selects the keys from the shaped json
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Array> KeysOfShapedJson (const v8::AccessorInfo& info) {
  v8::HandleScope scope;
  TRI_v8_global_t* v8g;

  v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();

  v8::Handle<v8::Array> result = v8::Array::New();

  // sanity check
  v8::Handle<v8::Object> self = info.Holder();

  if (self->InternalFieldCount() <= SLOT_BARRIER) {
    return scope.Close(result);
  }

  // get shaped json
  void* marker = TRI_UnwrapClass<void*>(self, WRP_SHAPED_JSON_TYPE);

  if (marker == 0) {
    return scope.Close(result);
  }

  TRI_barrier_t* barrier = static_cast<TRI_barrier_t*>(v8::Handle<v8::External>::Cast(self->GetInternalField(SLOT_BARRIER))->Value());
  TRI_primary_collection_t* collection = barrier->_container->_collection;

  // check for array shape
  TRI_shaper_t* shaper = collection->_shaper;

  TRI_shape_sid_t sid;
  TRI_EXTRACT_SHAPE_IDENTIFIER_MARKER(sid, marker);

  TRI_shape_t const* shape = shaper->lookupShapeId(shaper, sid);

  if (shape == 0 || shape->_type != TRI_SHAPE_ARRAY) {
    return scope.Close(result);
  }

  TRI_array_shape_t const* s;
  TRI_shape_aid_t const* aids;
  TRI_shape_size_t i;
  TRI_shape_size_t n;
  char const* qtr;
  uint32_t count = 0;

  // shape is an array
  s = (TRI_array_shape_t const*) shape;

  // number of entries
  n = s->_fixedEntries + s->_variableEntries;

  // calculation position of attribute ids
  qtr = (char const*) shape;
  qtr += sizeof(TRI_array_shape_t);
  qtr += n * sizeof(TRI_shape_sid_t);
  aids = (TRI_shape_aid_t const*) qtr;

  for (i = 0;  i < n;  ++i, ++aids) {
    char const* att = shaper->lookupAttributeId(shaper, *aids);

    if (att) {
      result->Set(count++, v8::String::New(att));
    }
  }

  result->Set(count++, v8g->_IdKey);
  result->Set(count++, v8g->_RevKey);
  result->Set(count++, v8g->_KeyKey);

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check if a property is present
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Integer> PropertyQueryShapedJson (v8::Local<v8::String> name, const v8::AccessorInfo& info) {
  v8::HandleScope scope;

  // sanity check
  v8::Handle<v8::Object> self = info.Holder();

  if (self->InternalFieldCount() <= SLOT_BARRIER) {
    return scope.Close(v8::Handle<v8::Integer>());
  }

  // get shaped json
  void* marker = TRI_UnwrapClass<TRI_shaped_json_t>(self, WRP_SHAPED_JSON_TYPE);

  if (marker == 0) {
    return scope.Close(v8::Handle<v8::Integer>());
  }

  TRI_barrier_t* barrier = static_cast<TRI_barrier_t*>(v8::Handle<v8::External>::Cast(self->GetInternalField(SLOT_BARRIER))->Value());
  TRI_primary_collection_t* collection = barrier->_container->_collection;

  // convert the JavaScript string to a string
  string key = TRI_ObjectToString(name);

  if (key == "") {
    return scope.Close(v8::Handle<v8::Integer>());
  }

  if (key[0] == '_') {
    if (key == "_id" || key == "_rev" || key == "_key") {
      return scope.Close(v8::Handle<v8::Integer>(v8::Integer::New(v8::ReadOnly)));
    }
  }

  // get shape accessor
  TRI_shaper_t* shaper = collection->_shaper;
  TRI_shape_pid_t pid = shaper->findAttributePathByName(shaper, key.c_str());

  TRI_shape_sid_t sid;
  TRI_EXTRACT_SHAPE_IDENTIFIER_MARKER(sid, marker);

  TRI_shape_access_t const* acc = TRI_FindAccessorVocShaper(shaper, sid, pid);

  // key not found
  if (acc == 0 || acc->_shape == 0) {
    return scope.Close(v8::Handle<v8::Integer>());
  }

  return scope.Close(v8::Handle<v8::Integer>(v8::Integer::New(v8::ReadOnly)));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief selects an indexed attribute from the shaped json
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> MapGetIndexedShapedJson (uint32_t index,
                                                      const v8::AccessorInfo& info) {
  v8::HandleScope scope;

  char* str = TRI_StringUInt32(index);
  v8::Local<v8::String> strVal = v8::String::New(str);
  TRI_Free(TRI_CORE_MEM_ZONE, str);

  return scope.Close(MapGetNamedShapedJson(strVal, info));
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                            MODULE
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief parse document or document handle from a v8 value (string | object)
////////////////////////////////////////////////////////////////////////////////

bool ExtractDocumentHandle (v8::Handle<v8::Value> val,
                            string& collectionName,
                            TRI_voc_key_t& key,
                            TRI_voc_rid_t& rid) {
  // reset the collection identifier and the revision
  collectionName = "";
  rid = 0;

  // extract the document identifier and revision from a string
  if (val->IsString()) {
    return ParseDocumentHandle(val, collectionName, key);
  }

  // extract the document identifier and revision from a document object
  if (val->IsObject()) {
    TRI_v8_global_t* v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();

    v8::Handle<v8::Object> obj = val->ToObject();
    v8::Handle<v8::Value> didVal = obj->Get(v8g->_IdKey);

    if (! ParseDocumentHandle(didVal, collectionName, key)) {
      return false;
    }

    rid = TRI_ObjectToUInt64(obj->Get(v8g->_RevKey), true);

    if (rid == 0) {
      return false;
    }

    return true;
  }

  // unknown value type. give up
  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parse document or document handle from a v8 value (string | object)
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Value> TRI_ParseDocumentOrDocumentHandle (const CollectionNameResolver& resolver,
                                                         TRI_vocbase_col_t const*& collection,
                                                         TRI_voc_key_t& key,
                                                         TRI_voc_rid_t& rid,
                                                         v8::Handle<v8::Value> val) {
  v8::HandleScope scope;

  assert(key == 0);

  // reset the collection identifier and the revision
  string collectionName = "";
  rid = 0;

  // try to extract the collection name, key, and revision from the object passed
  if (! ExtractDocumentHandle(val, collectionName, key, rid)) {
    return scope.Close(TRI_CreateErrorObject(TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD,
                                            "<document-handle> must be a valid document-handle"));
  }

  // we have at least a key, we also might have a collection name
  assert(key != 0);


  if (collectionName == "") {
    // only a document key without collection name was passed
    if (collection == 0) {
      // we do not know the collection
      return scope.Close(TRI_CreateErrorObject(TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD,
                                               "<document-handle> must be a document-handle"));
    }
    // we use the current collection's name
    collectionName = resolver.getCollectionName(collection->_cid);
  }
  else {
    // we read a collection name from the document id
    // check cross-collection requests
    if (collection != 0) {
      if (collectionName != resolver.getCollectionName(collection->_cid)) {
        return scope.Close(TRI_CreateErrorObject(TRI_ERROR_ARANGO_CROSS_COLLECTION_REQUEST,
                                                 "cannot execute cross collection query"));
      }
    }
  }

  assert(collectionName != "");

  if (collection == 0) {
    // no collection object was passed, now check the user-supplied collection name
    const TRI_vocbase_col_t* col = resolver.getCollectionStruct(collectionName);
    if (col == 0) {
      // collection not found
      return scope.Close(TRI_CreateErrorObject(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND,
                                               "collection of <document-handle> is unknown"));;
    }

    collection = col;
  }

  assert(collection != 0);

  v8::Handle<v8::Value> empty;
  return scope.Close(empty);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up an index identifier
////////////////////////////////////////////////////////////////////////////////

TRI_index_t* TRI_LookupIndexByHandle (const CollectionNameResolver& resolver,
                                      TRI_vocbase_col_t const* collection,
                                      v8::Handle<v8::Value> val,
                                      bool ignoreNotFound,
                                      v8::Handle<v8::Object>* err) {
  // reset the collection identifier and the revision
  string collectionName = "";
  TRI_idx_iid_t iid = 0;

  // assume we are already loaded
  assert(collection != 0);
  assert(collection->_collection != 0);

  // extract the document identifier and revision from a string
  if (val->IsString() || val->IsStringObject() || val->IsNumber()) {
    if (! IsIndexHandle(val, collectionName, iid)) {
      *err = TRI_CreateErrorObject(TRI_ERROR_ARANGO_INDEX_HANDLE_BAD,
                                   "<index-handle> must be an index-handle");
      return 0;
    }
  }

  // extract the document identifier and revision from a string
  else if (val->IsObject()) {
    TRI_v8_global_t* v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();

    v8::Handle<v8::Object> obj = val->ToObject();
    v8::Handle<v8::Value> iidVal = obj->Get(v8g->IdKey);

    if (! IsIndexHandle(iidVal, collectionName, iid)) {
      *err = TRI_CreateErrorObject(TRI_ERROR_ARANGO_INDEX_HANDLE_BAD,
                                   "expecting an index-handle in id");
      return 0;
    }
  }

  if (collectionName != "") {
    if (collectionName != collection->_name) {
      // I wish this error provided me with more information!
      // e.g. 'cannot access index outside the collection it was defined in'
      *err = TRI_CreateErrorObject(TRI_ERROR_ARANGO_CROSS_COLLECTION_REQUEST,
                                   "cannot execute cross collection index");
      return 0;
    }
  }

  TRI_index_t* idx = TRI_LookupIndex(collection->_collection, iid);

  if (idx == 0) {
    if (! ignoreNotFound) {
      *err = TRI_CreateErrorObject(TRI_ERROR_ARANGO_INDEX_NOT_FOUND, "index is unknown");
    }
  }

  return idx;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief wraps a TRI_vocbase_t
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Object> TRI_WrapVocBase (TRI_vocbase_t const* database) {
  v8::HandleScope scope;

  TRI_v8_global_t* v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();
  v8::Handle<v8::Object> result = WrapClass(v8g->VocbaseTempl,
                                            WRP_VOCBASE_TYPE,
                                            const_cast<TRI_vocbase_t*>(database));

  result->Set(TRI_V8_SYMBOL("_path"), v8::String::New(database->_path), v8::ReadOnly);

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief wraps a TRI_vocbase_col_t
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Object> TRI_WrapCollection (TRI_vocbase_col_t const* collection) {
  v8::HandleScope scope;

  TRI_v8_global_t* v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();
  v8::Handle<v8::Object> result = WrapClass(v8g->VocbaseColTempl,
                                            WRP_VOCBASE_COL_TYPE,
                                            const_cast<TRI_vocbase_col_t*>(collection));

  result->Set(v8g->_IdKey, V8CollectionId(collection->_cid), v8::ReadOnly);

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief wraps a TRI_shaped_json_t
////////////////////////////////////////////////////////////////////////////////

template<class T>
v8::Handle<v8::Value> TRI_WrapShapedJson (T& trx,
                                          TRI_voc_cid_t cid,
                                          TRI_doc_mptr_t const* document,
                                          TRI_barrier_t* barrier) {
  v8::HandleScope scope;
  
  TRI_ASSERT_MAINTAINER(document != 0);
  TRI_ASSERT_MAINTAINER(document->_key != 0);
  TRI_ASSERT_MAINTAINER(document->_data != 0);
  TRI_ASSERT_MAINTAINER(barrier != 0);

  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  TRI_v8_global_t* v8g = (TRI_v8_global_t*) isolate->GetData();

  // create the new handle to return, and set its template type
  v8::Handle<v8::Object> result = v8g->ShapedJsonTempl->NewInstance();

  if (result.IsEmpty()) {
    // error
    // TODO check for empty results
    return scope.Close(result);
  }

  TRI_barrier_blocker_t* blocker = (TRI_barrier_blocker_t*) barrier;
  bool doCopy = trx.mustCopyShapedJson();

  if (doCopy) {
    // we'll create our own copy of the data
    TRI_df_marker_t const* m = static_cast<TRI_df_marker_t const*>(document->_data);

    if (blocker->_data != NULL && blocker->_mustFree) {
      TRI_Free(TRI_UNKNOWN_MEM_ZONE, blocker->_data);
      blocker->_data = NULL;
      blocker->_mustFree = false;
    }

    blocker->_data = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, m->_size, false);

    if (blocker->_data == 0) {
      // out of memory
      return scope.Close(result);
    }

    memcpy(blocker->_data, m, m->_size);

    blocker->_mustFree = true;
  }
  else {
    // we'll use the pointer into the datafile
    blocker->_data = const_cast<void*>(document->_data);
  }


  // point the 0 index Field to the c++ pointer for unwrapping later
  result->SetInternalField(SLOT_CLASS_TYPE, v8::Integer::New(WRP_SHAPED_JSON_TYPE));
  result->SetInternalField(SLOT_CLASS, v8::External::New(blocker->_data));

  map< void*, v8::Persistent<v8::Value> >::iterator i = v8g->JSBarriers.find(barrier);

  if (i == v8g->JSBarriers.end()) {
    v8::Persistent<v8::Value> persistent = v8::Persistent<v8::Value>::New(isolate, v8::External::New(barrier));
    result->SetInternalField(SLOT_BARRIER, persistent);

    v8g->JSBarriers[barrier] = persistent;
    persistent.MakeWeak(isolate, barrier, WeakBarrierCallback);
  }
  else {
    result->SetInternalField(SLOT_BARRIER, i->second);
  }

  // store the document reference
  TRI_voc_rid_t rid = document->_rid;

  result->Set(v8g->_IdKey, V8DocumentId(trx.resolver().getCollectionName(cid), document->_key), v8::ReadOnly);
  result->Set(v8g->_RevKey, V8RevisionId(rid), v8::ReadOnly);
  result->Set(v8g->_KeyKey, v8::String::New(document->_key), v8::ReadOnly);

  TRI_df_marker_type_t type = ((TRI_df_marker_t*) document->_data)->_type;

  if (type == TRI_DOC_MARKER_KEY_EDGE) {
    TRI_doc_edge_key_marker_t* marker = (TRI_doc_edge_key_marker_t*) document->_data;

    result->Set(v8g->_FromKey, V8DocumentId(trx.resolver().getCollectionName(marker->_fromCid), ((char*) marker) + marker->_offsetFromKey));
    result->Set(v8g->_ToKey, V8DocumentId(trx.resolver().getCollectionName(marker->_toCid), ((char*) marker) + marker->_offsetToKey));
  }

  // and return
  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the private WRP_VOCBASE_COL_TYPE value
////////////////////////////////////////////////////////////////////////////////

int32_t TRI_GetVocBaseColType () {
  return WRP_VOCBASE_COL_TYPE;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a TRI_vocbase_t global context
////////////////////////////////////////////////////////////////////////////////

void TRI_InitV8VocBridge (v8::Handle<v8::Context> context,
                          TRI_vocbase_t* vocbase,
                          const size_t threadNumber) {
  v8::HandleScope scope;

  // check the isolate
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  TRI_v8_global_t* v8g = TRI_CreateV8Globals(isolate);

  // set the default database
  v8g->_vocbase = vocbase;

  // create the regular expressions
  string expr;

  // collection name / id (used for indexes)
  expr = "^(" TRI_COL_NAME_REGEX ")" TRI_DOCUMENT_HANDLE_SEPARATOR_STR "(" TRI_VOC_ID_REGEX ")$";
  if (regcomp(&v8g->IndexIdRegex, expr.c_str(), REG_EXTENDED) != 0) {
    LOG_FATAL_AND_EXIT("cannot compile regular expression");
  }

  // id only
  expr = "^(" TRI_VOC_ID_REGEX ")$";
  if (regcomp(&v8g->IdRegex, expr.c_str(), REG_EXTENDED) != 0) {
    LOG_FATAL_AND_EXIT("cannot compile regular expression");
  }

  // collection name / document key (used for documents)
  expr = "^(" TRI_COL_NAME_REGEX ")" TRI_DOCUMENT_HANDLE_SEPARATOR_STR "(" TRI_VOC_KEY_REGEX ")$";
  if (regcomp(&v8g->DocumentIdRegex, expr.c_str(), REG_EXTENDED) != 0) {
    LOG_FATAL_AND_EXIT("cannot compile regular expression");
  }

  // key only
  expr = "^" TRI_VOC_KEY_REGEX "$";
  if (regcomp(&v8g->DocumentKeyRegex, expr.c_str(), REG_EXTENDED) != 0) {
    LOG_FATAL_AND_EXIT("cannot compile regular expression");
  }

  v8::Handle<v8::ObjectTemplate> rt;
  v8::Handle<v8::FunctionTemplate> ft;
  v8::Handle<v8::Template> pt;

  // .............................................................................
  // generate the TRI_vocbase_t template
  // .............................................................................

  ft = v8::FunctionTemplate::New();
  ft->SetClassName(TRI_V8_SYMBOL("ArangoDatabase"));

  rt = ft->InstanceTemplate();
  rt->SetInternalFieldCount(2);
  rt->SetNamedPropertyHandler(MapGetVocBase);

  // for any database function added here, be sure to add it to in function
  // JS_CompletionsVocbase, too for the auto-completion
  TRI_AddMethodVocbase(rt, "_collection", JS_CollectionVocbase);
  TRI_AddMethodVocbase(rt, "_collections", JS_CollectionsVocbase);
  TRI_AddMethodVocbase(rt, "_COMPLETIONS", JS_CompletionsVocbase, true);
  TRI_AddMethodVocbase(rt, "_create", JS_CreateVocbase, true);
  TRI_AddMethodVocbase(rt, "_createDocumentCollection", JS_CreateDocumentCollectionVocbase);
  TRI_AddMethodVocbase(rt, "_createEdgeCollection", JS_CreateEdgeCollectionVocbase);
  TRI_AddMethodVocbase(rt, "_document", JS_DocumentVocbase);
  TRI_AddMethodVocbase(rt, "_remove", JS_RemoveVocbase);
  TRI_AddMethodVocbase(rt, "_replace", JS_ReplaceVocbase);
  TRI_AddMethodVocbase(rt, "_update", JS_UpdateVocbase);
  TRI_AddMethodVocbase(rt, "_version", JS_VersionVocbase);

  v8g->VocbaseTempl = v8::Persistent<v8::ObjectTemplate>::New(isolate, rt);
  TRI_AddGlobalFunctionVocbase(context, "ArangoDatabase", ft->GetFunction());

  // .............................................................................
  // generate the TRI_shaped_json_t template
  // .............................................................................

  ft = v8::FunctionTemplate::New();
  ft->SetClassName(TRI_V8_SYMBOL("ShapedJson"));

  rt = ft->InstanceTemplate();
  rt->SetInternalFieldCount(3);

  // accessor for named properties (e.g. doc.abcdef)
  rt->SetNamedPropertyHandler(MapGetNamedShapedJson,    // NamedPropertyGetter,
                              0,                        // NamedPropertySetter setter = 0
                              PropertyQueryShapedJson,  // NamedPropertyQuery,
                              0,                        // NamedPropertyDeleter deleter = 0,
                              KeysOfShapedJson          // NamedPropertyEnumerator,
                                                        // Handle<Value> data = Handle<Value>());
                              );
  
  // accessor for indexed properties (e.g. doc[1])
  rt->SetIndexedPropertyHandler(MapGetIndexedShapedJson,  // IndexedPropertyGetter,
                                0,                        // IndexedPropertySetter setter = 0
                                0,                        // IndexedPropertyQuery,
                                0,                        // IndexedPropertyDeleter deleter = 0,
                                0                         // IndexedPropertyEnumerator,
                                                          // Handle<Value> data = Handle<Value>());
                              );

  v8g->ShapedJsonTempl = v8::Persistent<v8::ObjectTemplate>::New(isolate, rt);
  TRI_AddGlobalFunctionVocbase(context, "ShapedJson", ft->GetFunction());

  // .............................................................................
  // generate the TRI_vocbase_col_t template
  // .............................................................................

  ft = v8::FunctionTemplate::New();
  ft->SetClassName(TRI_V8_SYMBOL("ArangoCollection"));

  rt = ft->InstanceTemplate();
  rt->SetInternalFieldCount(2);

  TRI_AddMethodVocbase(rt, "count", JS_CountVocbaseCol);
  TRI_AddMethodVocbase(rt, "datafiles", JS_DatafilesVocbaseCol);
  TRI_AddMethodVocbase(rt, "datafileScan", JS_DatafileScanVocbaseCol);
  TRI_AddMethodVocbase(rt, "document", JS_DocumentVocbaseCol);
  TRI_AddMethodVocbase(rt, "drop", JS_DropVocbaseCol);
  TRI_AddMethodVocbase(rt, "dropIndex", JS_DropIndexVocbaseCol);
  TRI_AddMethodVocbase(rt, "ensureBitarray", JS_EnsureBitarrayVocbaseCol);
  TRI_AddMethodVocbase(rt, "ensureUndefBitarray", JS_EnsureUndefBitarrayVocbaseCol);
  TRI_AddMethodVocbase(rt, "ensureCapConstraint", JS_EnsureCapConstraintVocbaseCol);
  TRI_AddMethodVocbase(rt, "ensureFulltextIndex", JS_EnsureFulltextIndexVocbaseCol);
  TRI_AddMethodVocbase(rt, "ensureGeoConstraint", JS_EnsureGeoConstraintVocbaseCol);
  TRI_AddMethodVocbase(rt, "ensureGeoIndex", JS_EnsureGeoIndexVocbaseCol);
  TRI_AddMethodVocbase(rt, "ensureHashIndex", JS_EnsureHashIndexVocbaseCol);
  TRI_AddMethodVocbase(rt, "ensurePQIndex", JS_EnsurePriorityQueueIndexVocbaseCol);
  TRI_AddMethodVocbase(rt, "ensureSkiplist", JS_EnsureSkiplistVocbaseCol);
  TRI_AddMethodVocbase(rt, "ensureUniqueConstraint", JS_EnsureUniqueConstraintVocbaseCol);
  TRI_AddMethodVocbase(rt, "ensureUniqueSkiplist", JS_EnsureUniqueSkiplistVocbaseCol);
  TRI_AddMethodVocbase(rt, "figures", JS_FiguresVocbaseCol);
  TRI_AddMethodVocbase(rt, "getIndexes", JS_GetIndexesVocbaseCol);
  TRI_AddMethodVocbase(rt, "load", JS_LoadVocbaseCol);
  TRI_AddMethodVocbase(rt, "lookupFulltextIndex", JS_LookupFulltextIndexVocbaseCol);
  TRI_AddMethodVocbase(rt, "lookupHashIndex", JS_LookupHashIndexVocbaseCol);
  TRI_AddMethodVocbase(rt, "lookupSkiplist", JS_LookupSkiplistVocbaseCol);
  TRI_AddMethodVocbase(rt, "lookupUniqueConstraint", JS_LookupUniqueConstraintVocbaseCol);
  TRI_AddMethodVocbase(rt, "lookupUniqueSkiplist", JS_LookupUniqueSkiplistVocbaseCol);
  TRI_AddMethodVocbase(rt, "name", JS_NameVocbaseCol);
  TRI_AddMethodVocbase(rt, "properties", JS_PropertiesVocbaseCol);
  TRI_AddMethodVocbase(rt, "remove", JS_RemoveVocbaseCol);
  TRI_AddMethodVocbase(rt, "revision", JS_RevisionVocbaseCol);
  TRI_AddMethodVocbase(rt, "rename", JS_RenameVocbaseCol);
  TRI_AddMethodVocbase(rt, "setAttribute", JS_SetAttributeVocbaseCol, true);
  TRI_AddMethodVocbase(rt, "status", JS_StatusVocbaseCol);
  TRI_AddMethodVocbase(rt, "truncate", JS_TruncateVocbaseCol);
  TRI_AddMethodVocbase(rt, "truncateDatafile", JS_TruncateDatafileVocbaseCol);
  TRI_AddMethodVocbase(rt, "type", JS_TypeVocbaseCol);
  TRI_AddMethodVocbase(rt, "unload", JS_UnloadVocbaseCol);
  TRI_AddMethodVocbase(rt, "upgrade", JS_UpgradeVocbaseCol, true);
  TRI_AddMethodVocbase(rt, "version", JS_VersionVocbaseCol);

  TRI_AddMethodVocbase(rt, "replace", JS_ReplaceVocbaseCol);
  TRI_AddMethodVocbase(rt, "save", JS_SaveVocbaseCol);
  TRI_AddMethodVocbase(rt, "saveOrReplace", JS_SaveOrReplaceVocbaseCol);
  TRI_AddMethodVocbase(rt, "update", JS_UpdateVocbaseCol);

  v8g->VocbaseColTempl = v8::Persistent<v8::ObjectTemplate>::New(isolate, rt);
  TRI_AddGlobalFunctionVocbase(context, "ArangoCollection", ft->GetFunction());

  // .............................................................................
  // generate the general cursor template
  // .............................................................................

  ft = v8::FunctionTemplate::New();
  ft->SetClassName(TRI_V8_SYMBOL("ArangoCursor"));

  rt = ft->InstanceTemplate();
  rt->SetInternalFieldCount(2);

  TRI_AddMethodVocbase(rt, "count", JS_CountGeneralCursor);
  TRI_AddMethodVocbase(rt, "dispose", JS_DisposeGeneralCursor);
  TRI_AddMethodVocbase(rt, "getBatchSize", JS_GetBatchSizeGeneralCursor);
  TRI_AddMethodVocbase(rt, "getRows", JS_GetRowsGeneralCursor);
  TRI_AddMethodVocbase(rt, "hasCount", JS_HasCountGeneralCursor);
  TRI_AddMethodVocbase(rt, "hasNext", JS_HasNextGeneralCursor);
  TRI_AddMethodVocbase(rt, "id", JS_IdGeneralCursor);
  TRI_AddMethodVocbase(rt, "next", JS_NextGeneralCursor);
  TRI_AddMethodVocbase(rt, "persist", JS_PersistGeneralCursor);
  TRI_AddMethodVocbase(rt, "toArray", JS_ToArrayGeneralCursor);
  TRI_AddMethodVocbase(rt, "unuse", JS_UnuseGeneralCursor);

  v8g->GeneralCursorTempl = v8::Persistent<v8::ObjectTemplate>::New(isolate, rt);
  TRI_AddGlobalFunctionVocbase(context, "ArangoCursor", ft->GetFunction());

  // .............................................................................
  // generate global functions
  // .............................................................................

  TRI_AddGlobalFunctionVocbase(context, "AHUACATL_RUN", JS_RunAhuacatl);
  TRI_AddGlobalFunctionVocbase(context, "AHUACATL_EXPLAIN", JS_ExplainAhuacatl);
  TRI_AddGlobalFunctionVocbase(context, "AHUACATL_PARSE", JS_ParseAhuacatl);

  TRI_AddGlobalFunctionVocbase(context, "CURSOR", JS_Cursor);
  TRI_AddGlobalFunctionVocbase(context, "CREATE_CURSOR", JS_CreateCursor);
  TRI_AddGlobalFunctionVocbase(context, "DELETE_CURSOR", JS_DeleteCursor);

  TRI_AddGlobalFunctionVocbase(context, "COMPARE_STRING", JS_compare_string);
  TRI_AddGlobalFunctionVocbase(context, "NORMALIZE_STRING", JS_normalize_string);

  TRI_AddGlobalFunctionVocbase(context, "RELOAD_AUTH", JS_ReloadAuth);
  TRI_AddGlobalFunctionVocbase(context, "TRANSACTION", JS_Transaction);

  // .............................................................................
  // create global variables
  // .............................................................................
  
  TRI_AddGlobalVariableVocbase(context, "db", TRI_WrapVocBase(vocbase));

  // current thread number
  context->Global()->Set(TRI_V8_SYMBOL("THREAD_NUMBER"), v8::Number::New(threadNumber), v8::ReadOnly);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
