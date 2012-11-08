////////////////////////////////////////////////////////////////////////////////
/// @brief V8-vocbase bridge
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2012 triAGENS GmbH, Cologne, Germany
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
/// @author Copyright 2011-2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "v8-vocbase.h"

#include "Logger/Logger.h"
#include "Ahuacatl/ahuacatl-codegen.h"
#include "Ahuacatl/ahuacatl-context.h"
#include "Ahuacatl/ahuacatl-explain.h"
#include "Ahuacatl/ahuacatl-result.h"
#include "Basics/StringUtils.h"
#include "BasicsC/conversions.h"
#include "BasicsC/files.h"
#include "BasicsC/json.h"
#include "BasicsC/json-utilities.h"
#include "BasicsC/logging.h"
#include "BasicsC/strings.h"
#include "Rest/JsonContainer.h"
#include "ShapedJson/shape-accessor.h"
#include "ShapedJson/shaped-json.h"
#include "V8/v8-conv.h"
#include "V8/v8-execution.h"
#include "V8/v8-utils.h"
#include "V8Server/v8-objects.h"
#include "VocBase/datafile.h"
#include "VocBase/general-cursor.h"
#include "VocBase/document-collection.h"
#include "VocBase/edge-collection.h"
#include "VocBase/voc-shaper.h"
#include "Basics/Utf8Helper.h"
#include "v8.h"

#ifdef TRI_ENABLE_TRX
#include "VocBase/transaction.h"
#endif

using namespace std;
using namespace triagens::basics;

// -----------------------------------------------------------------------------
// --SECTION--                                              forward declarations
// -----------------------------------------------------------------------------

static v8::Handle<v8::Value> WrapGeneralCursor (void* cursor);

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
/// @brief wrapped class for transactions
///
/// Layout:
/// - SLOT_CLASS_TYPE
/// - SLOT_CLASS
////////////////////////////////////////////////////////////////////////////////

static int32_t const WRP_TRANSACTION_TYPE = 5;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////
 
// -----------------------------------------------------------------------------
// --SECTION--                                                    HELPER CLASSES
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                              AhuacatlContextGuard
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief scope guard for a TRI_aql_context_t*
////////////////////////////////////////////////////////////////////////////////

class AhuacatlContextGuard {
  public:
    AhuacatlContextGuard (TRI_vocbase_t* vocbase, const string& query) : 
      _context(0) {
      _context = TRI_CreateContextAql(vocbase, query.c_str());
  
      if (_context == 0) {
        LOGGER_DEBUG << "failed to create context for query %s" << query;
      }
    }

    ~AhuacatlContextGuard () {
      this->free();
    }

    void free () {
      if (_context != 0) {
        TRI_FreeContextAql(_context);
        _context = 0;
      }
    }
    
    inline TRI_aql_context_t* operator() () const {
      return _context;
    }
    
    inline TRI_aql_context_t* ptr () const {
      return _context;
    }
    
    inline const TRI_aql_context_t* const_ptr () const {
      return _context;
    }

    inline const bool valid () const {
      return _context != 0;
    }

  private:
    TRI_aql_context_t* _context;
};

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
/// @brief return the collection type the object is responsible for
/// - "db" will return TRI_COL_TYPE_DOCUMENT
/// - "edges" will return TRI_COL_TYPE_EDGE
////////////////////////////////////////////////////////////////////////////////

static inline TRI_col_type_e GetVocBaseCollectionType (const v8::Handle<v8::Object>& obj) {
  v8::Handle<v8::Value> type = obj->Get(TRI_V8_SYMBOL("_type"));

  if (type->IsNumber()) {
    return (TRI_col_type_e) TRI_ObjectToInt64(type);
  }

  return TRI_COL_TYPE_DOCUMENT;
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
  v8::Handle<v8::Context> currentContext = v8::Context::GetCurrent();
  v8::Handle<v8::Object> db = currentContext->Global()->Get(TRI_V8_SYMBOL("db"))->ToObject();

  return TRI_UnwrapClass<TRI_vocbase_t>(db, WRP_VOCBASE_TYPE);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if argument is a document identifier
////////////////////////////////////////////////////////////////////////////////

static bool IsDocumentHandle (v8::Handle<v8::Value> arg, TRI_voc_cid_t& cid, TRI_voc_key_t& key) {
  TRI_v8_global_t* v8g;

  v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();

  if (! arg->IsString()) {
    return false;
  }

  TRI_Utf8ValueNFC str(TRI_UNKNOWN_MEM_ZONE, arg);
  char const* s = *str;

  if (s == 0) {
    return false;
  }

  regmatch_t matches[3];

  // "cid/key"
  if (regexec(&v8g->DocumentIdRegex, s, sizeof(matches) / sizeof(matches[0]), matches, 0) == 0) {
    cid = TRI_UInt64String2(s + matches[1].rm_so, matches[1].rm_eo - matches[1].rm_so);
    key = TRI_DuplicateString2Z(TRI_CORE_MEM_ZONE, s + matches[2].rm_so, matches[2].rm_eo - matches[2].rm_so);
    return true;
  }
  
  // "key"
  if (regexec(&v8g->DocumentKeyRegex, s, 0, NULL, 0) == 0) {
    key = TRI_DuplicateString2Z(TRI_CORE_MEM_ZONE, *str, str.length());
    return true;
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if argument is a document identifier
////////////////////////////////////////////////////////////////////////////////

static bool IsIndexHandle (v8::Handle<v8::Value> arg, TRI_voc_cid_t& cid, TRI_idx_iid_t& iid) {
  TRI_v8_global_t* v8g;

  v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();

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

  regmatch_t matches[3];

  if (regexec(&v8g->IndexIdRegex, s, sizeof(matches) / sizeof(matches[0]), matches, 0) == 0) {
    cid = TRI_UInt64String2(s + matches[1].rm_so, matches[1].rm_eo - matches[1].rm_so);
    iid = TRI_UInt64String2(s + matches[2].rm_so, matches[2].rm_eo - matches[2].rm_so);
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
/// @brief returns the index representation
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> IndexRep (TRI_collection_t* col, TRI_json_t* idx) {
  v8::HandleScope scope;

  v8::Handle<v8::Object> rep = TRI_ObjectJson(idx)->ToObject();

  string iid = TRI_ObjectToString(rep->Get(v8::String::New("id")));
  string id = StringUtils::itoa(col->_cid) + string(TRI_INDEX_HANDLE_SEPARATOR_STR) + iid;
  rep->Set(v8::String::New("id"), v8::String::New(id.c_str()));

  return scope.Close(rep);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts argument strings to TRI_vector_pointer_t
////////////////////////////////////////////////////////////////////////////////

int FillVectorPointerFromArguments (v8::Arguments const& argv,
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
      error = "invalid parameter";

      TRI_FreeContentVectorPointer(TRI_CORE_MEM_ZONE, result);
      return TRI_set_errno(TRI_ERROR_ILLEGAL_OPTION);
    }

    TRI_Utf8ValueNFC argumentString(TRI_UNKNOWN_MEM_ZONE, argument);
    char* cArgument = *argumentString == 0 ? 0 : TRI_DuplicateString(*argumentString);

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
        error = "duplicate parameters";

        TRI_FreeContentVectorPointer(TRI_CORE_MEM_ZONE, result);
        return TRI_set_errno(TRI_ERROR_ILLEGAL_OPTION);
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

  if (! TRI_IS_DOCUMENT_COLLECTION(primary->base._type)) {
    TRI_ReleaseCollection(collection);
    return scope.Close(v8::ThrowException(TRI_CreateErrorObject(TRI_ERROR_INTERNAL, "unknown collection type")));
  }

  TRI_document_collection_t* sim = (TRI_document_collection_t*) primary;

  // .............................................................................
  // Ensure that there is at least one string parameter sent to this method
  // .............................................................................

  if (argv.Length() == 0) {
    TRI_ReleaseCollection(collection);
    return scope.Close(v8::ThrowException(TRI_CreateErrorObject(TRI_ERROR_ILLEGAL_OPTION, "usage: " + cmd + "(<path>, ...)")));
  }

  // .............................................................................
  // Create a list of paths, these will be used to create a list of shapes
  // which will be used by the hash index.
  // .............................................................................

  string errorString;

  TRI_vector_pointer_t attributes;
  TRI_InitVectorPointer(&attributes, TRI_CORE_MEM_ZONE);

  int res = FillVectorPointerFromArguments(argv, &attributes, 0, argv.Length(), errorString);

  // .............................................................................
  // Some sort of error occurred -- display error message and abort index creation
  // (or index retrieval).
  // .............................................................................

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_DestroyVectorPointer(&attributes);

    TRI_ReleaseCollection(collection);
    return scope.Close(v8::ThrowException(TRI_CreateErrorObject(res, errorString)));
  }

  // .............................................................................
  // Actually create the index here
  // .............................................................................

  bool created;
  TRI_index_t* idx;

  if (type == TRI_IDX_TYPE_HASH_INDEX) {
    if (create) {
      idx = TRI_EnsureHashIndexDocumentCollection(sim, &attributes, unique, &created);

      if (idx == 0) {
        res = TRI_errno();
      }
    }
    else {
      idx = TRI_LookupHashIndexDocumentCollection(sim, &attributes, unique);
    }
  }
  else if (type == TRI_IDX_TYPE_SKIPLIST_INDEX) {
    if (create) {
      idx = TRI_EnsureSkiplistIndexDocumentCollection(sim, &attributes, unique, &created);

      if (idx == 0) {
        res = TRI_errno();
      }
    }
    else {
      idx = TRI_LookupSkiplistIndexDocumentCollection(sim, &attributes, unique);
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
    if (create) {
      TRI_ReleaseCollection(collection);
      return scope.Close(v8::ThrowException(TRI_CreateErrorObject(res, "index could not be created", true)));
    }
    else {
      TRI_ReleaseCollection(collection);
      return scope.Close(v8::Null());
    }
  }

  // .............................................................................
  // return the newly assigned index identifier
  // .............................................................................

  TRI_json_t* json = idx->json(idx, collection->_collection);

  if (!json) {
    TRI_ReleaseCollection(collection);
    return scope.Close(v8::ThrowException(v8::String::New("out of memory")));
  }

  v8::Handle<v8::Value> index = IndexRep(&primary->base, json);
  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);

  if (create) {
    if (index->IsObject()) {
      index->ToObject()->Set(v8::String::New("isNewlyCreated"), created ? v8::True() : v8::False());
    }
  }

  TRI_ReleaseCollection(collection);
  return scope.Close(index);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a document
///
/// it is the caller's responsibility to acquire and release the required locks
/// the collection must also have the correct status already. don't use this
/// function if you're unsure about it!
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> DocumentVocbaseCol (TRI_vocbase_t* vocbase,
                                                 TRI_vocbase_col_t const* collection,
                                                 v8::Arguments const& argv,
                                                 const bool lock) {
  v8::HandleScope scope;

  // first and only argument should be a document idenfifier
  if (argv.Length() != 1) {
    return scope.Close(v8::ThrowException(
                         TRI_CreateErrorObject(TRI_ERROR_BAD_PARAMETER,
                                               "usage: document(<document-handle>)")));
  }

  TRI_voc_key_t key = 0;
  TRI_voc_rid_t rid;
  v8::Handle<v8::Value> err = TRI_ParseDocumentOrDocumentHandle(vocbase, collection, key, rid, lock, argv[0]);

  if (! err.IsEmpty()) {
    if (collection != 0 && lock) {
      TRI_ReleaseCollection(collection);
    }
    if (key) TRI_FreeString(TRI_CORE_MEM_ZONE, key);
    return scope.Close(v8::ThrowException(err));
  }

  // .............................................................................
  // get document
  // .............................................................................

  TRI_doc_mptr_t document;
  v8::Handle<v8::Value> result;
  TRI_primary_collection_t* primary = collection->_collection;
  
  TRI_doc_operation_context_t readContext;
  TRI_InitReadContextPrimaryCollection(&readContext, primary);

  // .............................................................................
  // inside a read transaction
  // .............................................................................

  if (lock) {
    primary->beginRead(primary);
  }

  document = primary->read(&readContext, key);
  TRI_FreeString(TRI_CORE_MEM_ZONE, key);

  if (document._key != 0) {
    TRI_barrier_t* barrier;

    barrier = TRI_CreateBarrierElement(&primary->_barrierList);
    result = TRI_WrapShapedJson(collection, &document, barrier);
  }

  if (lock) {
    primary->endRead(primary);
  }

  // .............................................................................
  // outside a write transaction
  // .............................................................................

  if (lock) {
    TRI_ReleaseCollection(collection);
  }

  if (document._key == 0) {
    return scope.Close(v8::ThrowException(
                         TRI_CreateErrorObject(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND,
                                               "document not found")));
  }

  if (rid != 0 && document._rid != rid) {
    return scope.Close(v8::ThrowException(
                         TRI_CreateErrorObject(TRI_ERROR_ARANGO_CONFLICT,
                                               "revision not found")));
  }

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief replaces a document
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> ReplaceVocbaseCol (TRI_vocbase_t* vocbase,
                                                TRI_vocbase_col_t const* collection,
                                                v8::Arguments const& argv) {
  v8::HandleScope scope;
  TRI_v8_global_t* v8g;

  v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();

  // check the arguments
  if (argv.Length() < 2) {
    TRI_ReleaseCollection(collection);

    return scope.Close(v8::ThrowException(
                         TRI_CreateErrorObject(TRI_ERROR_BAD_PARAMETER,
                                               "usage: replace(<document>, <data>, <overwrite>, <waitForSync>)")));
  }

  TRI_voc_key_t key = 0;
  TRI_voc_rid_t rid;

  v8::Handle<v8::Value> err = TRI_ParseDocumentOrDocumentHandle(vocbase, collection, key, rid, true, argv[0]);

  if (! err.IsEmpty()) {
    if (collection != 0) {
      TRI_ReleaseCollection(collection);
    }

    if (key) TRI_FreeString(TRI_CORE_MEM_ZONE, key);
    return scope.Close(v8::ThrowException(err));
  }

  // convert data
  TRI_primary_collection_t* primary = collection->_collection;
  TRI_shaped_json_t* shaped = TRI_ShapedJsonV8Object(argv[1], primary->_shaper);

  if (shaped == 0) {
    TRI_ReleaseCollection(collection);
    if (key) TRI_FreeString(TRI_CORE_MEM_ZONE, key);
    return scope.Close(v8::ThrowException(
                         TRI_CreateErrorObject(TRI_errno(),
                                               "<data> cannot be converted into JSON shape")));
  }

  // check policy
  TRI_doc_update_policy_e policy = TRI_DOC_UPDATE_ERROR;

  if (3 <= argv.Length()) {
    bool overwrite = TRI_ObjectToBoolean(argv[2]);

    if (overwrite) {
      policy = TRI_DOC_UPDATE_LAST_WRITE;
    }
    else {
      policy = TRI_DOC_UPDATE_CONFLICT;
    }
  }

  bool forceSync = false;
  if (4 == argv.Length()) {
    forceSync = TRI_ObjectToBoolean(argv[3]);
  }

  TRI_voc_rid_t oldRid = 0;

  TRI_doc_operation_context_t context;
  TRI_InitContextPrimaryCollection(&context, primary, policy, forceSync);
  context._expectedRid = rid;
  context._previousRid = &oldRid;
  context._release = true;

  // .............................................................................
  // inside a write transaction
  // .............................................................................

  primary->beginWrite(primary);

  TRI_doc_mptr_t mptr = primary->update(&context, shaped, key);
  if (key) {
    TRI_FreeString(TRI_CORE_MEM_ZONE, key);
  }

  // .............................................................................
  // outside a write transaction
  // .............................................................................

  TRI_FreeShapedJson(primary->_shaper, shaped);

  if (mptr._key == 0) {
    TRI_ReleaseCollection(collection);
    return scope.Close(v8::ThrowException(TRI_CreateErrorObject(TRI_errno(), "cannot replace document", true)));
  }

  string id = StringUtils::itoa(primary->base._cid) + string(TRI_DOCUMENT_HANDLE_SEPARATOR_STR) + string(mptr._key);

  v8::Handle<v8::Object> result = v8::Object::New();
  result->Set(v8g->DidKey, v8::String::New(id.c_str()));
  result->Set(v8g->RevKey, v8::Number::New(mptr._rid));
  result->Set(v8g->OldRevKey, v8::Number::New(oldRid));
  result->Set(v8g->KeyKey, v8::String::New(mptr._key));

  TRI_ReleaseCollection(collection);
  return scope.Close(result);
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
/// Thus, the @FA{waitForSync} URL parameter can be used to force synchronisation
/// of just specific operations. To use this, set the @FA{waitForSync} parameter
/// to @LIT{true}. If the @FA{waitForSync} parameter is not specified or set to 
/// @LIT{false}, then the collection's default @LIT{waitForSync} behavior is 
/// applied. The @FA{waitForSync} URL parameter cannot be used to disable
/// synchronisation for collections that have a default @LIT{waitForSync} value
/// of @LIT{true}.
///
/// @EXAMPLES
///
/// @verbinclude shell_create-document
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> SaveVocbaseCol (TRI_vocbase_col_t const* collection, 
                                             v8::Arguments const& argv) {
  v8::HandleScope scope;
  TRI_v8_global_t* v8g;

  v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();

  TRI_primary_collection_t* primary = collection->_collection;

  if (argv.Length() < 1 || argv.Length() > 2) {
    return scope.Close(v8::ThrowException(
                         TRI_CreateErrorObject(TRI_ERROR_BAD_PARAMETER,
                                               "usage: save(<data>, <waitForSync>)")));
  }

  // set document key
  TRI_voc_key_t key = 0;

  if (argv[0]->IsObject()) {
    v8::Handle<v8::Value> v = argv[0]->ToObject()->Get(v8g->KeyKey);
    if (v->IsString()) {
      TRI_Utf8ValueNFC str(TRI_CORE_MEM_ZONE, v);
      key = TRI_DuplicateString2(*str, str.length());
    }
  }
  
  bool forceSync = false;
  if (2 == argv.Length()) {
    forceSync = TRI_ObjectToBoolean(argv[1]);
  }

  TRI_shaped_json_t* shaped = TRI_ShapedJsonV8Object(argv[0], primary->_shaper);

  if (shaped == 0) {
    if (key) {
      TRI_FreeString(TRI_CORE_MEM_ZONE, key);
    }
    return scope.Close(v8::ThrowException(
                         TRI_CreateErrorObject(TRI_errno(),
                                               "<data> cannot be converted into JSON shape")));
  }

  // .............................................................................
  // inside a write transaction
  // .............................................................................

  TRI_doc_operation_context_t context;
  TRI_InitContextPrimaryCollection(&context, primary, TRI_DOC_UPDATE_ERROR, forceSync);
  context._release = true;

  primary->beginWrite(primary);

  // the lock is freed in create
  TRI_doc_mptr_t mptr = primary->create(&context, TRI_DOC_MARKER_KEY_DOCUMENT, shaped, 0, key);

  if (key) {
    TRI_Free(TRI_CORE_MEM_ZONE, key);
  }
  
  // .............................................................................
  // outside a write transaction
  // .............................................................................

  TRI_FreeShapedJson(primary->_shaper, shaped);

  if (mptr._key == 0) {
    return scope.Close(v8::ThrowException(TRI_CreateErrorObject(TRI_errno(), "cannot save document", true)));
  }

  string id = StringUtils::itoa(primary->base._cid) + string(TRI_DOCUMENT_HANDLE_SEPARATOR_STR) + string(mptr._key);

  v8::Handle<v8::Object> result = v8::Object::New();
  result->Set(v8g->DidKey, v8::String::New(id.c_str()));
  result->Set(v8g->RevKey, v8::Number::New(mptr._rid));
  result->Set(v8g->KeyKey, v8::String::New(mptr._key));

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
/// Thus, the @FA{waitForSync} URL parameter can be used to force synchronisation
/// of just specific operations. To use this, set the @FA{waitForSync} parameter
/// to @LIT{true}. If the @FA{waitForSync} parameter is not specified or set to 
/// @LIT{false}, then the collection's default @LIT{waitForSync} behavior is 
/// applied. The @FA{waitForSync} URL parameter cannot be used to disable
/// synchronisation for collections that have a default @LIT{waitForSync} value
/// of @LIT{true}.
///
/// @EXAMPLES
///
/// @TINYEXAMPLE{shell_create-edge,create an edge}
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> SaveEdgeCol (TRI_vocbase_col_t const* collection, 
                                          v8::Arguments const& argv) {
  v8::HandleScope scope;
  TRI_v8_global_t* v8g;
  
  v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();

  TRI_primary_collection_t* primary = collection->_collection;

  if (argv.Length() < 3 || argv.Length() > 4) {
    return scope.Close(v8::ThrowException(
                         TRI_CreateErrorObject(TRI_ERROR_BAD_PARAMETER,
                                               "usage: save(<from>, <to>, <data>, <waitForSync>)")));
  }
  
  bool isBidirectional = false;
  
  // set document key
  TRI_voc_key_t key = 0;

  if (argv[2]->IsObject()) {
    v8::Handle<v8::Object> obj = argv[2]->ToObject();
    v8::Handle<v8::Value> v = obj->Get(v8g->KeyKey);
    if (v->IsString()) {
      TRI_Utf8ValueNFC str(TRI_CORE_MEM_ZONE, v);
      key = TRI_DuplicateString2(*str, str.length());
    }

    if (obj->Has(v8g->BidirectionalKey)) {
      isBidirectional = TRI_ObjectToBoolean(obj->Get(v8g->BidirectionalKey));
    }
  }
    
  bool forceSync = false;
  if (4 == argv.Length()) {
    forceSync = TRI_ObjectToBoolean(argv[3]);
  }

  TRI_document_edge_t edge;

  edge._fromCid = collection->_cid;
  edge._toCid = collection->_cid;
  edge._toKey = 0;
  edge._fromKey = 0;
  edge._isBidirectional = isBidirectional;

  v8::Handle<v8::Value> errMsg;

  // extract from
  TRI_vocbase_col_t const* fromCollection = 0;
  TRI_voc_rid_t fromRid;

  errMsg = TRI_ParseDocumentOrDocumentHandle(collection->_vocbase, fromCollection, edge._fromKey, fromRid, true, argv[0]);

  if (! errMsg.IsEmpty()) {
    if (fromCollection != 0) {
      TRI_ReleaseCollection(fromCollection);
    }

    if (key) TRI_FreeString(TRI_CORE_MEM_ZONE, key);
    if (edge._fromKey) TRI_FreeString(TRI_CORE_MEM_ZONE, edge._fromKey);
    
    return scope.Close(v8::ThrowException(errMsg));
  }

  edge._fromCid = fromCollection->_cid;
  TRI_ReleaseCollection(fromCollection);

  // extract to
  TRI_vocbase_col_t const* toCollection = 0;
  TRI_voc_rid_t toRid;

  errMsg = TRI_ParseDocumentOrDocumentHandle(collection->_vocbase, toCollection, edge._toKey, toRid, true, argv[1]);

  if (! errMsg.IsEmpty()) {
    if (toCollection != 0) {
      TRI_ReleaseCollection(toCollection);
    }

    if (key) {
      TRI_FreeString(TRI_CORE_MEM_ZONE, key);
    }
    if (edge._fromKey) {
      TRI_FreeString(TRI_CORE_MEM_ZONE, edge._fromKey);
    }
    if (edge._toKey) {
      TRI_FreeString(TRI_CORE_MEM_ZONE, edge._toKey);
    }
    
    return scope.Close(v8::ThrowException(errMsg));
  }

  edge._toCid = toCollection->_cid;
  TRI_ReleaseCollection(toCollection);

  // extract shaped data
  TRI_shaped_json_t* shaped = TRI_ShapedJsonV8Object(argv[2], primary->_shaper);

  if (shaped == 0) {
    
    if (key) {
      TRI_FreeString(TRI_CORE_MEM_ZONE, key);
    }
    if (edge._fromKey) {
      TRI_FreeString(TRI_CORE_MEM_ZONE, edge._fromKey);
    }
    if (edge._toKey) {
      TRI_FreeString(TRI_CORE_MEM_ZONE, edge._toKey);
    }
    
    return scope.Close(v8::ThrowException(
                         TRI_CreateErrorObject(TRI_errno(),
                                               "<data> cannot be converted into JSON shape")));
  }

  // .............................................................................
  // inside a write transaction
  // .............................................................................
  
  TRI_doc_operation_context_t context;
  TRI_InitContextPrimaryCollection(&context, primary, TRI_DOC_UPDATE_ERROR, forceSync);
  context._release = true;

  primary->beginWrite(primary);

  // the lock is freed in create
  TRI_doc_mptr_t mptr = primary->create(&context, TRI_DOC_MARKER_KEY_EDGE, shaped, &edge, key);

  // .............................................................................
  // outside a write transaction
  // .............................................................................

  TRI_FreeShapedJson(primary->_shaper, shaped);

  if (key) {
   TRI_FreeString(TRI_CORE_MEM_ZONE, key);
  }
  if (edge._fromKey) {
    TRI_FreeString(TRI_CORE_MEM_ZONE, edge._fromKey);
  }
  if (edge._toKey) {
    TRI_FreeString(TRI_CORE_MEM_ZONE, edge._toKey);
  }
  
  if (mptr._key == 0) {
    return scope.Close(v8::ThrowException(TRI_CreateErrorObject(TRI_errno(), "cannot save document", true)));
  }

  //string id = StringUtils::itoa(primary->base._cid) + string(TRI_DOCUMENT_HANDLE_SEPARATOR_STR) + StringUtils::itoa(mptr._did);
  string id = StringUtils::itoa(primary->base._cid) + string(TRI_DOCUMENT_HANDLE_SEPARATOR_STR) + string(mptr._key);

  v8::Handle<v8::Object> result = v8::Object::New();
  result->Set(v8g->DidKey, v8::String::New(id.c_str()));
  result->Set(v8g->RevKey, v8::Number::New(mptr._rid));
  result->Set(v8g->KeyKey, v8::String::New(mptr._key));

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief updates a document
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> UpdateVocbaseCol (TRI_vocbase_t* vocbase,
                                               TRI_vocbase_col_t const* collection,
                                               v8::Arguments const& argv) {
  v8::HandleScope scope;
  TRI_v8_global_t* v8g;

  v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();

  // check the arguments
  if (argv.Length() < 2 || argv.Length() > 5) {
    TRI_ReleaseCollection(collection);

    return scope.Close(v8::ThrowException(
                         TRI_CreateErrorObject(TRI_ERROR_BAD_PARAMETER,
                                               "usage: update(<document>, <data>, <overwrite>, <keepnull>, <waitForSync>)")));
  }

  TRI_voc_key_t key = NULL;
  TRI_voc_rid_t rid;

  v8::Handle<v8::Value> err = TRI_ParseDocumentOrDocumentHandle(vocbase, collection, key, rid, true, argv[0]);

  if (! err.IsEmpty()) {
    if (collection != 0) {
      TRI_ReleaseCollection(collection);
    }

    if (key) TRI_FreeString(TRI_CORE_MEM_ZONE, key);
    return scope.Close(v8::ThrowException(err));
  }

  // convert data
  TRI_primary_collection_t* primary = collection->_collection;
  TRI_json_t* json = TRI_JsonObject(argv[1]);

  if (json == 0) {
    TRI_ReleaseCollection(collection);
    if (key) TRI_FreeString(TRI_CORE_MEM_ZONE, key);
    return scope.Close(v8::ThrowException(
                         TRI_CreateErrorObject(TRI_errno(),
                                               "<data> is no valid JSON")));
  }

  // check policy
  TRI_doc_update_policy_e policy = TRI_DOC_UPDATE_ERROR;

  if (3 <= argv.Length()) {
    bool overwrite = TRI_ObjectToBoolean(argv[2]);

    if (overwrite) {
      policy = TRI_DOC_UPDATE_LAST_WRITE;
    }
    else {
      policy = TRI_DOC_UPDATE_CONFLICT;
    }
  }

  bool nullMeansRemove;
  if (4 <= argv.Length()) {
    // delete null attributes
    nullMeansRemove = !TRI_ObjectToBoolean(argv[3]);
  }
  else {
    // default value: null values are saved as Null
    nullMeansRemove = false; 
  }

  bool forceSync = false;
  if (5 == argv.Length()) {
    forceSync = TRI_ObjectToBoolean(argv[4]);
  }

  // .............................................................................
  // inside a write transaction
  // .............................................................................
      
  TRI_doc_operation_context_t readContext;
  TRI_InitReadContextPrimaryCollection(&readContext, primary);

  primary->beginWrite(primary);
  
  // init target document
  TRI_doc_mptr_t mptr;

  TRI_voc_rid_t oldRid = 0;
  TRI_doc_mptr_t document = primary->read(&readContext, key);
  TRI_shaped_json_t shapedJson;
  TRI_EXTRACT_SHAPED_JSON_MARKER(shapedJson, document._data);
  TRI_json_t* old = TRI_JsonShapedJson(primary->_shaper, &shapedJson);

  if (old != 0) {
    TRI_json_t* patchedJson = TRI_MergeJson(TRI_UNKNOWN_MEM_ZONE, old, json, nullMeansRemove);
    TRI_FreeJson(primary->_shaper->_memoryZone, old);

    if (patchedJson != 0) {
      TRI_doc_operation_context_t context;

      TRI_InitContextPrimaryCollection(&context, primary, policy, forceSync);     
      context._release = true; 
      context._expectedRid = rid;
      context._previousRid = &oldRid;

      mptr = primary->updateJson(&context, patchedJson, key);
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, patchedJson);
    }
  }
  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);

  if (key) {
    TRI_FreeString(TRI_CORE_MEM_ZONE, key);
  }

  // .............................................................................
  // outside a write transaction
  // .............................................................................

  if (mptr._key == 0) {
    TRI_ReleaseCollection(collection);
    return scope.Close(v8::ThrowException(TRI_CreateErrorObject(TRI_errno(), "cannot update document", true)));
  }

  string id = StringUtils::itoa(primary->base._cid) + string(TRI_DOCUMENT_HANDLE_SEPARATOR_STR) + string(mptr._key);

  v8::Handle<v8::Object> result = v8::Object::New();
  result->Set(v8g->DidKey, v8::String::New(id.c_str()));
  result->Set(v8g->RevKey, v8::Number::New(mptr._rid));
  result->Set(v8g->OldRevKey, v8::Number::New(oldRid));
  result->Set(v8g->KeyKey, v8::String::New(mptr._key));

  TRI_ReleaseCollection(collection);
  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes a document
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> DeleteVocbaseCol (TRI_vocbase_t* vocbase,
                                               TRI_vocbase_col_t const* collection,
                                               v8::Arguments const& argv) {
  v8::HandleScope scope;

  // check the arguments
  if (argv.Length() < 1 || argv.Length() > 3) {
    TRI_ReleaseCollection(collection);
    return scope.Close(v8::ThrowException(TRI_CreateErrorObject(TRI_ERROR_BAD_PARAMETER,
                                                                "usage: delete(<document>, <overwrite>, <waitForSync>)")));
  }

  TRI_voc_key_t key = 0;
  TRI_voc_rid_t rid;

  v8::Handle<v8::Value> err = TRI_ParseDocumentOrDocumentHandle(vocbase, collection, key, rid, true, argv[0]);

  if (! err.IsEmpty()) {
    if (collection != 0) {
      TRI_ReleaseCollection(collection);
    }

    if (key) TRI_FreeString(TRI_CORE_MEM_ZONE, key);
    return scope.Close(v8::ThrowException(err));
  }

  // check policy
  TRI_doc_update_policy_e policy = TRI_DOC_UPDATE_ERROR;

  if (2 <= argv.Length()) {
    bool overwrite = TRI_ObjectToBoolean(argv[1]);

    if (overwrite) {
      policy = TRI_DOC_UPDATE_LAST_WRITE;
    }
    else {
      policy = TRI_DOC_UPDATE_CONFLICT;
    }
  }

  bool forceSync = false;
  if (3 == argv.Length()) {
    forceSync = TRI_ObjectToBoolean(argv[2]);
  }

  // .............................................................................
  // inside a write transaction
  // .............................................................................

  TRI_primary_collection_t* primary = collection->_collection;
  TRI_voc_rid_t oldRid;
  
  TRI_doc_operation_context_t context;
  TRI_InitContextPrimaryCollection(&context, primary, policy, forceSync);
  context._release = true;
  context._expectedRid = rid;
  context._previousRid = &oldRid;

  primary->beginWrite(primary);
  int res = primary->destroy(&context, key);
  if (key) {
    TRI_FreeString(TRI_CORE_MEM_ZONE, key);
  }

  // .............................................................................
  // outside a write transaction
  // .............................................................................

  TRI_ReleaseCollection(collection);

  if (res != TRI_ERROR_NO_ERROR) {
    if (res == TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND && policy == TRI_DOC_UPDATE_LAST_WRITE) {
      return scope.Close(v8::False());
    }
    else {
      return scope.Close(v8::ThrowException(TRI_CreateErrorObject(res, "cannot delete document", true)));
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
    return scope.Close(v8::ThrowException(TRI_CreateErrorObject(TRI_ERROR_INTERNAL, "corrupted vocbase")));
  }

  // expecting at least one arguments
  if (argv.Length() < 1 || argv.Length() > 3) {
    return scope.Close(v8::ThrowException(
                         TRI_CreateErrorObject(TRI_ERROR_BAD_PARAMETER,
                                               "usage: _create(<name>, <properties>)")));
  }

  // set default journal size
  TRI_voc_size_t effectiveSize = vocbase->_defaultMaximalSize;

  // extract the name
  string name = TRI_ObjectToString(argv[0]);

  // extract the parameters
  TRI_col_parameter_t parameter;
  
  if (2 <= argv.Length()) {
    if (! argv[1]->IsObject()) {
      return scope.Close(v8::ThrowException(TRI_CreateErrorObject(TRI_ERROR_BAD_PARAMETER, "<properties> must be an object")));
    }

    v8::Handle<v8::Object> p = argv[1]->ToObject();
    v8::Handle<v8::String> waitForSyncKey = v8::String::New("waitForSync");
    v8::Handle<v8::String> journalSizeKey = v8::String::New("journalSize");
    v8::Handle<v8::String> isSystemKey = v8::String::New("isSystem");

    if (p->Has(journalSizeKey)) {
      double s = TRI_ObjectToDouble(p->Get(journalSizeKey));

      if (s < TRI_JOURNAL_MINIMAL_SIZE) {
        return scope.Close(v8::ThrowException(
                             TRI_CreateErrorObject(TRI_ERROR_BAD_PARAMETER,
                                                   "<properties>.journalSize too small")));
      }

      // overwrite journal size with user-specified value
      effectiveSize = (TRI_voc_size_t) s;
    }

    TRI_InitParameterCollection(vocbase, &parameter, name.c_str(), collectionType, effectiveSize);

    if (p->Has(waitForSyncKey)) {
      parameter._waitForSync = TRI_ObjectToBoolean(p->Get(waitForSyncKey));
    }

    if (p->Has(isSystemKey)) {
      parameter._isSystem = TRI_ObjectToBoolean(p->Get(isSystemKey));
    }
  }
  else {
    TRI_InitParameterCollection(vocbase, &parameter, name.c_str(), collectionType, effectiveSize);
  }

  TRI_vocbase_col_t const* collection = TRI_CreateCollectionVocBase(vocbase, &parameter, 0);

  if (collection == 0) {
    return scope.Close(v8::ThrowException(TRI_CreateErrorObject(TRI_errno(), "cannot create collection", true)));
  }

  return scope.Close(TRI_WrapCollection(collection));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a single collection or null
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> CollectionVocBase (v8::Arguments const& argv) {
  v8::HandleScope scope;

  TRI_vocbase_t* vocbase = TRI_UnwrapClass<TRI_vocbase_t>(argv.Holder(), WRP_VOCBASE_TYPE);

  if (vocbase == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("corrupted vocbase")));
  }

  // expecting one argument
  if (argv.Length() != 1) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: _collection(<name>|<identifier>)")));
  }

  v8::Handle<v8::Value> val = argv[0];
  TRI_vocbase_col_t const* collection = 0;

  // number
  if (val->IsNumber() || val->IsNumberObject()) {
    uint64_t id = (uint64_t) TRI_ObjectToDouble(val);

    collection = TRI_LookupCollectionByIdVocBase(vocbase, id);
  }
  else {
    string name = TRI_ObjectToString(val);

    collection = TRI_FindCollectionByNameVocBase(vocbase, name.c_str(), false);
  }

  if (collection == 0) {
    return scope.Close(v8::Null());
  }

  return scope.Close(TRI_WrapCollection(collection));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that a geo index or constraint exists
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> EnsureGeoIndexVocbaseCol (v8::Arguments const& argv, bool constraint) {
  v8::HandleScope scope;

  v8::Handle<v8::Object> err;
  TRI_vocbase_col_t const* collection = UseCollection(argv.Holder(), &err);

  if (collection == 0) {
    return scope.Close(v8::ThrowException(err));
  }

  TRI_primary_collection_t* primary = collection->_collection;

  if (! TRI_IS_DOCUMENT_COLLECTION(primary->base._type)) {
    TRI_ReleaseCollection(collection);
    return scope.Close(v8::ThrowException(TRI_CreateErrorObject(TRI_ERROR_INTERNAL, "unknown collection type")));
  }

  TRI_document_collection_t* sim = (TRI_document_collection_t*) primary;
  TRI_index_t* idx = 0;
  bool created;
  int off = constraint ? 1 : 0;
  bool ignoreNull = false;

  // .............................................................................
  // case: <location>
  // .............................................................................

  if (argv.Length() == 1 + off) {
    TRI_Utf8ValueNFC loc(TRI_UNKNOWN_MEM_ZONE, argv[0]);

    if (*loc == 0) {
      TRI_ReleaseCollection(collection);
      return scope.Close(v8::ThrowException(
          TRI_CreateErrorObject(TRI_ERROR_ILLEGAL_OPTION,
                                "<location> must be an attribute path")));
    }

    if (constraint) {
      ignoreNull = TRI_ObjectToBoolean(argv[1]);
    }

    idx = TRI_EnsureGeoIndex1DocumentCollection(sim, *loc, false, constraint, ignoreNull, &created);
  }

  // .............................................................................
  // case: <location>, <geoJson>
  // .............................................................................

  else if (argv.Length() == 2 + off && (argv[1]->IsBoolean() || argv[1]->IsBooleanObject())) {
    TRI_Utf8ValueNFC loc(TRI_UNKNOWN_MEM_ZONE, argv[0]);

    if (*loc == 0) {
      TRI_ReleaseCollection(collection);
      return scope.Close(v8::ThrowException(
          TRI_CreateErrorObject(TRI_ERROR_ILLEGAL_OPTION,
                                "<location> must be an attribute path")));
    }

    if (constraint) {
      ignoreNull = TRI_ObjectToBoolean(argv[2]);
    }

    idx = TRI_EnsureGeoIndex1DocumentCollection(sim, *loc, TRI_ObjectToBoolean(argv[1]), constraint, ignoreNull, &created);
  }

  // .............................................................................
  // case: <latitude>, <longitude>
  // .............................................................................

  else if (argv.Length() == 2 + off) {
    TRI_Utf8ValueNFC lat(TRI_UNKNOWN_MEM_ZONE, argv[0]);
    TRI_Utf8ValueNFC lon(TRI_UNKNOWN_MEM_ZONE, argv[1]);

    if (*lat == 0) {
      TRI_ReleaseCollection(collection);
      return scope.Close(v8::ThrowException(
          TRI_CreateErrorObject(TRI_ERROR_ILLEGAL_OPTION,
                                "<latitude> must be an attribute path")));
    }

    if (*lon == 0) {
      TRI_ReleaseCollection(collection);
      return scope.Close(v8::ThrowException(
          TRI_CreateErrorObject(TRI_ERROR_ILLEGAL_OPTION,
                                "<longitude> must be an attribute path")));
    }

    if (constraint) {
      ignoreNull = TRI_ObjectToBoolean(argv[2]);
    }

    idx = TRI_EnsureGeoIndex2DocumentCollection(sim, *lat, *lon, constraint, ignoreNull, &created);
  }

  // .............................................................................
  // error case
  // .............................................................................

  else {
    TRI_ReleaseCollection(collection);

    if (constraint) {
      return scope.Close(v8::ThrowException(
          TRI_CreateErrorObject(TRI_ERROR_ILLEGAL_OPTION,
                                "usage: ensureGeoConstraint(<latitude>, <longitude>, <ignore-null>) " \
                                "or ensureGeoConstraint(<location>, [<geojson>], <ignore-null>)")));
    }
    else {
      return scope.Close(v8::ThrowException(
          TRI_CreateErrorObject(TRI_ERROR_ILLEGAL_OPTION,
                                "usage: ensureGeoIndex(<latitude>, <longitude>) or ensureGeoIndex(<location>, [<geojson>])")));
    }
  }

  if (idx == 0) {
    TRI_ReleaseCollection(collection);
    return scope.Close(v8::ThrowException(
        TRI_CreateErrorObject(TRI_errno(), "index could not be created", true)));
  }

  TRI_json_t* json = idx->json(idx, collection->_collection);

  if (! json) {
    TRI_ReleaseCollection(collection);
    return scope.Close(v8::ThrowException(v8::String::New("out of memory")));
  }

  v8::Handle<v8::Value> index = IndexRep(&primary->base, json);
  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);

  if (index->IsObject()) {
    index->ToObject()->Set(v8::String::New("isNewlyCreated"), created ? v8::True() : v8::False());
  }

  TRI_ReleaseCollection(collection);
  return scope.Close(index);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an Ahuacatl error in a javascript object
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Object> CreateErrorObjectAhuacatl (TRI_aql_error_t* error) {
  char* message = TRI_GetErrorMessageAql(error);

  if (message) {
    std::string str(message);
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, message);

    return TRI_CreateErrorObject(TRI_GetErrorCodeAql(error), str);
  }

  return TRI_CreateErrorObject(TRI_ERROR_OUT_OF_MEMORY);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function that encapsulates execution of an AQL query
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> ExecuteQueryNativeAhuacatl (TRI_aql_context_t* const context,
                                                         const TRI_json_t* const parameters) {
  v8::HandleScope scope;

  // parse & validate
  // bind values
  // optimise
  // lock
  
  if (!TRI_ValidateQueryContextAql(context) ||
      !TRI_BindQueryContextAql(context, parameters) ||
      !TRI_LockQueryContextAql(context) ||
      !TRI_OptimiseQueryContextAql(context)) {
    v8::Handle<v8::Object> errorObject = CreateErrorObjectAhuacatl(&context->_error);

    return scope.Close(v8::ThrowException(errorObject));
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

  if (allowDirectReturn || !result->IsArray()) {
    // return the value we got as it is. this is a performance optimisation
    return scope.Close(result);
  }

  // return the result as a cursor object
  TRI_json_t* json = TRI_JsonObject(result);

  if (!json) {
    v8::Handle<v8::Object> errorObject = TRI_CreateErrorObject(TRI_ERROR_OUT_OF_MEMORY);

    return scope.Close(v8::ThrowException(errorObject));
  }

  TRI_general_cursor_result_t* cursorResult = TRI_CreateResultAql(json);

  if (!cursorResult) {
    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);

    v8::Handle<v8::Object> errorObject = TRI_CreateErrorObject(TRI_ERROR_OUT_OF_MEMORY);

    return scope.Close(v8::ThrowException(errorObject));
  }

  TRI_general_cursor_t* cursor = TRI_CreateGeneralCursor(cursorResult, doCount, batchSize);
  if (!cursor) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, cursorResult);
    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);

    v8::Handle<v8::Object> errorObject = TRI_CreateErrorObject(TRI_ERROR_OUT_OF_MEMORY);

    return scope.Close(v8::ThrowException(errorObject));
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

static void WeakGeneralCursorCallback (v8::Persistent<v8::Value> object, void* parameter) {
  v8::HandleScope scope; // do not remove, will fail otherwise!!

  LOG_TRACE("weak-callback for general cursor called");

  TRI_vocbase_t* vocbase = GetContextVocBase();
  if (! vocbase) {
    return;
  }

  TRI_EndUsageDataShadowData(vocbase->_cursors, parameter);

  // find the persistent handle
  TRI_v8_global_t* v8g;
  v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();
  v8::Persistent<v8::Value> persistent = v8g->JSGeneralCursors[parameter];
  v8g->JSGeneralCursors.erase(parameter);

  // dispose and clear the persistent handle
  persistent.Dispose();
  persistent.Clear();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stores a general cursor in a javascript object
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> WrapGeneralCursor (void* cursor) {
  v8::HandleScope scope;
  v8::TryCatch tryCatch;

  TRI_v8_global_t* v8g;
  v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();

  v8::Handle<v8::Object> cursorObject = v8g->GeneralCursorTempl->NewInstance();
  
  if (cursorObject.IsEmpty()) {
    // error 
    // TODO check for empty results
    return scope.Close(cursorObject);
  }  
  
  map< void*, v8::Persistent<v8::Value> >::iterator i = v8g->JSGeneralCursors.find(cursor);

  if (i == v8g->JSGeneralCursors.end()) {
    v8::Persistent<v8::Value> persistent = v8::Persistent<v8::Value>::New(v8::External::New(cursor));

    if (tryCatch.HasCaught()) {
      return scope.Close(v8::Undefined());
    }

    cursorObject->SetInternalField(SLOT_CLASS_TYPE, v8::Integer::New(WRP_GENERAL_CURSOR_TYPE));
    cursorObject->SetInternalField(SLOT_CLASS, persistent);
    v8g->JSGeneralCursors[cursor] = persistent;

    persistent.MakeWeak(cursor, WeakGeneralCursorCallback);
  }
  else {
    cursorObject->SetInternalField(SLOT_CLASS_TYPE, v8::Integer::New(WRP_GENERAL_CURSOR_TYPE));
    cursorObject->SetInternalField(SLOT_CLASS, i->second);
  }

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
// --SECTION--                                                      TRANSACTIONS
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_TRX

////////////////////////////////////////////////////////////////////////////////
/// @brief weak reference callback for transactions
////////////////////////////////////////////////////////////////////////////////

static void WeakTransactionCallback (v8::Persistent<v8::Value> object, void* parameter) {
  v8::HandleScope scope; // do not remove, will fail otherwise!!
  
  LOG_TRACE("weak-callback for transaction called");

  TRI_transaction_t* trx = static_cast<TRI_transaction_t*>(parameter);
  if (trx != 0) {
    TRI_FreeTransaction(trx);
  }

  object.Dispose();
  object.Clear();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stores a transaction in a javascript object
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> WrapTransaction (void* transaction) {
  v8::HandleScope scope;
  v8::TryCatch tryCatch;

  TRI_v8_global_t* v8g;
  v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();

  v8::Handle<v8::Object> trxObject = v8g->TransactionTempl->NewInstance();
  
  if (trxObject.IsEmpty()) {
    // error 
    // TODO check for empty results
    return scope.Close(trxObject);
  }  
    
  v8::Persistent<v8::Value> persistent = v8::Persistent<v8::Value>::New(v8::External::New(transaction));
  if (tryCatch.HasCaught()) {
    return scope.Close(v8::Undefined());
  }

  trxObject->SetInternalField(SLOT_CLASS_TYPE, v8::Integer::New(WRP_TRANSACTION_TYPE));
  trxObject->SetInternalField(SLOT_CLASS, persistent);

  persistent.MakeWeak(transaction, WeakTransactionCallback);

  return scope.Close(trxObject);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts a transaction from a javascript object
////////////////////////////////////////////////////////////////////////////////

static inline TRI_transaction_t* UnwrapTransaction (v8::Handle<v8::Object> trxObject) {
  return TRI_UnwrapClass<TRI_transaction_t>(trxObject, WRP_TRANSACTION_TYPE);
}

#endif

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

#ifdef TRI_ENABLE_TRX

////////////////////////////////////////////////////////////////////////////////
/// @brief create a transaction Javascript object
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_CreateTransaction (v8::Arguments const& argv) {
  v8::HandleScope scope;
  
  v8::Handle<v8::Context> currentContext = v8::Context::GetCurrent();
  void* ptr = *(currentContext->Global());


  TRI_vocbase_t* vocbase = GetContextVocBase();
  TRI_transaction_context_t* context = vocbase->_transactionContext;
  TRI_transaction_t* trx = TRI_CreateTransaction(context, TRI_TRANSACTION_READ_REPEATABLE, ptr);
  
  return scope.Close(WrapTransaction(trx));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add a collection to a Javascript transaction object
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_AddCollectionTransaction (v8::Arguments const& argv) {
  v8::HandleScope scope;
  
  TRI_transaction_t* trx = UnwrapTransaction(argv.Holder());
  if (trx == 0) {
    return scope.Close(v8::ThrowException(TRI_CreateErrorObject(TRI_ERROR_INTERNAL, "invalid transaction")));
  }

  if (argv.Length() != 2) {
    return scope.Close(v8::ThrowException(
                       TRI_CreateErrorObject(TRI_ERROR_ILLEGAL_OPTION,
                                             "usage: addCollection(<name>, <write>)")));
  }
    
  if (trx->_status != TRI_TRANSACTION_CREATED) {
    return scope.Close(v8::ThrowException(
                       TRI_CreateErrorObject(TRI_ERROR_TRANSACTION_INVALID_STATE,
                                             "cannot add collection to an already started transaction")));
  }

  string name = TRI_ObjectToString(argv[0]);
  const TRI_transaction_type_e type = (TRI_ObjectToBoolean(argv[1]) ? TRI_TRANSACTION_WRITE : TRI_TRANSACTION_READ);
  
  if (! TRI_AddCollectionTransaction(trx, name.c_str(), type)) {
    return scope.Close(v8::ThrowException(
                       TRI_CreateErrorObject(TRI_ERROR_INTERNAL, "internal error")));
  }

  return scope.Close(v8::True());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief abort a transaction
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_AbortTransaction (v8::Arguments const& argv) {
  v8::HandleScope scope;
  
  TRI_transaction_t* trx = UnwrapTransaction(argv.Holder());
  if (trx == 0) {
    return scope.Close(v8::ThrowException(TRI_CreateErrorObject(TRI_ERROR_INTERNAL, "invalid transaction")));
  }
    
  if (trx->_status != TRI_TRANSACTION_RUNNING) {
    return scope.Close(v8::ThrowException(
                       TRI_CreateErrorObject(TRI_ERROR_TRANSACTION_INVALID_STATE,
                                             "cannot abort a non-running transaction")));
  }
  
  const int res = TRI_AbortTransaction(trx);
  if (res != TRI_ERROR_NO_ERROR) {
    return scope.Close(v8::ThrowException(TRI_CreateErrorObject(res)));
  }

  return scope.Close(v8::True());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief begin a transaction
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_BeginTransaction (v8::Arguments const& argv) {
  v8::HandleScope scope;
  
  TRI_transaction_t* trx = UnwrapTransaction(argv.Holder());
  if (trx != 0) {
    if (trx->_status != TRI_TRANSACTION_CREATED) {
      return scope.Close(v8::ThrowException(
                         TRI_CreateErrorObject(TRI_ERROR_TRANSACTION_INVALID_STATE,
                                               "cannot begin an already started transaction")));
    }

    const int res = TRI_StartTransaction(trx);
    if (res != TRI_ERROR_NO_ERROR) {
      return scope.Close(v8::ThrowException(TRI_CreateErrorObject(res)));
    }
  }

  return scope.Close(v8::True());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief commit a transaction
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_CommitTransaction (v8::Arguments const& argv) {
  v8::HandleScope scope;
  
  TRI_transaction_t* trx = UnwrapTransaction(argv.Holder());
  if (trx == 0) {
    return scope.Close(v8::ThrowException(TRI_CreateErrorObject(TRI_ERROR_INTERNAL, "invalid transaction")));
  }

  if (trx->_status != TRI_TRANSACTION_RUNNING) {
    return scope.Close(v8::ThrowException(
                       TRI_CreateErrorObject(TRI_ERROR_TRANSACTION_INVALID_STATE,
                                             "cannot commit non-running transaction")));
  }

  int res;
  
  if (trx->_type == TRI_TRANSACTION_READ) {
    res = TRI_FinishTransaction(trx);
  }
  else {
    res = TRI_CommitTransaction(trx);
  }

  if (res != TRI_ERROR_NO_ERROR) {
    return scope.Close(v8::ThrowException(TRI_CreateErrorObject(res)));
  }

  return scope.Close(v8::True());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief dump a transaction
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_DumpTransaction (v8::Arguments const& argv) {
  v8::HandleScope scope;
  
  TRI_transaction_t* trx = UnwrapTransaction(argv.Holder());
  if (trx == 0) {
    return scope.Close(v8::ThrowException(TRI_CreateErrorObject(TRI_ERROR_INTERNAL, "invalid transaction")));
  }

  TRI_DumpTransaction(trx);

  return scope.Close(v8::Undefined());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the current status of a transaction
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_StatusTransaction (v8::Arguments const& argv) {
  v8::HandleScope scope;

  TRI_transaction_t* trx = UnwrapTransaction(argv.Holder());
  if (trx == 0) {
    return scope.Close(v8::ThrowException(TRI_CreateErrorObject(TRI_ERROR_INTERNAL, "invalid transaction")));
  }

  return scope.Close(v8::Integer::New((int32_t) trx->_status));
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief normalize UTF 16 strings
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_normalize_string (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 1) {
    return scope.Close(v8::ThrowException(
                         TRI_CreateErrorObject(TRI_ERROR_ILLEGAL_OPTION,
                                               "usage: NORMALIZE_STRING(<string>)")));
  }

  return scope.Close(TRI_normalize_V8_Obj(argv[0]));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compare two UTF 16 strings
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_compare_string (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 2) {
    return scope.Close(v8::ThrowException(
                         TRI_CreateErrorObject(TRI_ERROR_ILLEGAL_OPTION,
                                               "usage: COMPARE_STRING(<left string>, <right string>)")));
  }

  v8::String::Value left(argv[0]);
  v8::String::Value right(argv[1]);
  
  int result = Utf8Helper::DefaultUtf8Helper.compareUtf16(*left, left.length(), *right, right.length());
  
  return scope.Close(v8::Integer::New(result));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates a general cursor from a list
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_CreateCursor (v8::Arguments const& argv) {
  v8::HandleScope scope;

  TRI_vocbase_t* vocbase = GetContextVocBase();

  if (vocbase == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("corrupted vocbase")));
  }

  if (argv.Length() < 1) {
    return scope.Close(v8::ThrowException(
                         TRI_CreateErrorObject(TRI_ERROR_ILLEGAL_OPTION,
                                               "usage: CREATE_CURSOR(<list>, <do-count>, <batch-size>)")));
  }

  if (! argv[0]->IsArray()) {
    return scope.Close(v8::ThrowException(
                         TRI_CreateErrorObject(TRI_ERROR_ILLEGAL_OPTION,
                                               "<list> must be a list")));
  }

  // extract objects
  v8::Handle<v8::Array> array = v8::Handle<v8::Array>::Cast(argv[0]);
  TRI_json_t* json = TRI_JsonObject(array);

  if (json == 0) {
    return scope.Close(v8::ThrowException(
                         TRI_CreateErrorObject(TRI_ERROR_ILLEGAL_OPTION,
                                               "cannot convert <list> to JSON")));
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
    return scope.Close(v8::ThrowException(
                         TRI_CreateErrorObject(TRI_ERROR_INTERNAL, "cannot create cursor")));
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
    return scope.Close(v8::ThrowException(
                         TRI_CreateErrorObject(TRI_ERROR_ILLEGAL_OPTION,
                                               "usage: dispose()")));
  }

  TRI_vocbase_t* vocbase = GetContextVocBase();

  if (! vocbase) {
    return scope.Close(v8::ThrowException(
                         TRI_CreateErrorObject(TRI_ERROR_INTERNAL,
                                               "corrupted vocbase")));
  }

  bool found = TRI_DeleteDataShadowData(vocbase->_cursors, UnwrapGeneralCursor(argv.Holder()));

  if (found) {
    return scope.Close(v8::True());
  }

  return scope.Close(v8::False());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the id of a general cursor
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_IdGeneralCursor (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 0) {
    return scope.Close(v8::ThrowException(
                         TRI_CreateErrorObject(TRI_ERROR_ILLEGAL_OPTION,
                                               "usage: id()")));
  }

  TRI_vocbase_t* vocbase = GetContextVocBase();

  if (vocbase == 0) {
    return scope.Close(v8::ThrowException(
                         TRI_CreateErrorObject(TRI_ERROR_INTERNAL,
                                               "corrupted vocbase")));
  }

  TRI_shadow_id id = TRI_GetIdDataShadowData(vocbase->_cursors, UnwrapGeneralCursor(argv.Holder()));

  if (id != 0) {
    return scope.Close(v8::Number::New((double) id));
  }

  return scope.Close(v8::ThrowException(
                       TRI_CreateErrorObject(TRI_ERROR_CURSOR_NOT_FOUND)));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the number of results
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_CountGeneralCursor (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 0) {
    return scope.Close(v8::ThrowException(
                         TRI_CreateErrorObject(TRI_ERROR_ILLEGAL_OPTION,
                                               "usage: count()")));
  }

  TRI_vocbase_t* vocbase = GetContextVocBase();

  if (vocbase == 0) {
    return scope.Close(v8::ThrowException(
                         TRI_CreateErrorObject(TRI_ERROR_INTERNAL,
                                               "corrupted vocbase")));
  }

  TRI_general_cursor_t* cursor;

  cursor = (TRI_general_cursor_t*) TRI_BeginUsageDataShadowData(vocbase->_cursors, UnwrapGeneralCursor(argv.Holder()));

  if (cursor) {
    size_t length = (size_t) cursor->_length;
    TRI_EndUsageDataShadowData(vocbase->_cursors, cursor);

    return scope.Close(v8::Number::New(length));
  }

  return scope.Close(v8::ThrowException(
                       TRI_CreateErrorObject(TRI_ERROR_CURSOR_NOT_FOUND))); 
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the next result from the general cursor
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_NextGeneralCursor (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 0) {
    return scope.Close(v8::ThrowException(
                         TRI_CreateErrorObject(TRI_ERROR_ILLEGAL_OPTION,
                                               "usage: count()")));
  }

  TRI_vocbase_t* vocbase = GetContextVocBase();

  if (vocbase == 0) {
    return scope.Close(v8::ThrowException(
                         TRI_CreateErrorObject(TRI_ERROR_INTERNAL,
                                               "corrupted vocbase")));
  }

  bool result = false;
  v8::Handle<v8::Value> value;
  TRI_general_cursor_t* cursor;

  cursor = (TRI_general_cursor_t*) TRI_BeginUsageDataShadowData(vocbase->_cursors, UnwrapGeneralCursor(argv.Holder()));

  if (cursor) {
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

  return scope.Close(v8::ThrowException(TRI_CreateErrorObject(TRI_ERROR_CURSOR_NOT_FOUND)));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief persist the general cursor for usage in subsequent requests
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_PersistGeneralCursor (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 0) {
    return scope.Close(v8::ThrowException(
                         TRI_CreateErrorObject(TRI_ERROR_ILLEGAL_OPTION,
                                               "usage: persist()")));
  }

  TRI_vocbase_t* vocbase = GetContextVocBase();

  if (vocbase == 0) {
    return scope.Close(v8::ThrowException(
                         TRI_CreateErrorObject(TRI_ERROR_INTERNAL,
                                               "corrupted vocbase")));
  }

  bool result = TRI_PersistDataShadowData(vocbase->_cursors, UnwrapGeneralCursor(argv.Holder()));

  if (result) {
    return scope.Close(v8::True());
  }

  return scope.Close(v8::ThrowException(TRI_CreateErrorObject(TRI_ERROR_CURSOR_NOT_FOUND)));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the next x rows from the cursor in one go
///
/// This function constructs multiple rows at once and should be preferred over
/// hasNext()...next() when iterating over bigger result sets
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_GetRowsGeneralCursor (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 0) {
    return scope.Close(v8::ThrowException(
                         TRI_CreateErrorObject(TRI_ERROR_ILLEGAL_OPTION,
                                               "usage: getRows()")));
  }

  TRI_vocbase_t* vocbase = GetContextVocBase();

  if (vocbase == 0) {
    return scope.Close(v8::ThrowException(
                         TRI_CreateErrorObject(TRI_ERROR_INTERNAL,
                                               "corrupted vocbase")));
  }

  bool result = false;
  v8::Handle<v8::Array> rows = v8::Array::New();
  TRI_general_cursor_t* cursor;

  cursor = (TRI_general_cursor_t*) TRI_BeginUsageDataShadowData(vocbase->_cursors, UnwrapGeneralCursor(argv.Holder()));

  if (cursor) {
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

  return scope.Close(v8::ThrowException(TRI_CreateErrorObject(TRI_ERROR_CURSOR_NOT_FOUND)));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return max number of results per transfer for cursor
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_GetBatchSizeGeneralCursor (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 0) {
    return scope.Close(v8::ThrowException(
                         TRI_CreateErrorObject(TRI_ERROR_ILLEGAL_OPTION,
                                               "usage: getBatchSize()")));
  }

  TRI_vocbase_t* vocbase = GetContextVocBase();

  if (vocbase == 0) {
    return scope.Close(v8::ThrowException(
                         TRI_CreateErrorObject(TRI_ERROR_INTERNAL,
                                               "corrupted vocbase")));
  }

  TRI_general_cursor_t* cursor;

  cursor = (TRI_general_cursor_t*) TRI_BeginUsageDataShadowData(vocbase->_cursors, UnwrapGeneralCursor(argv.Holder()));

  if (cursor) {
    uint32_t max = cursor->getBatchSize(cursor);

    TRI_EndUsageDataShadowData(vocbase->_cursors, cursor);
    return scope.Close(v8::Number::New(max));
  }

  return scope.Close(v8::ThrowException(TRI_CreateErrorObject(TRI_ERROR_CURSOR_NOT_FOUND)));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return if count flag was set for cursor
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_HasCountGeneralCursor (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 0) {
    return scope.Close(v8::ThrowException(
                         TRI_CreateErrorObject(TRI_ERROR_ILLEGAL_OPTION,
                                               "usage: hasCount()")));
  }

  TRI_vocbase_t* vocbase = GetContextVocBase();

  if (vocbase == 0) {
    return scope.Close(v8::ThrowException(
                         TRI_CreateErrorObject(TRI_ERROR_INTERNAL,
                                               "corrupted vocbase")));
  }

  TRI_general_cursor_t* cursor;

  cursor = (TRI_general_cursor_t*) TRI_BeginUsageDataShadowData(vocbase->_cursors, UnwrapGeneralCursor(argv.Holder()));

  if (cursor) {
    bool hasCount = cursor->hasCount(cursor);

    TRI_EndUsageDataShadowData(vocbase->_cursors, cursor);
    return scope.Close(hasCount ? v8::True() : v8::False());
  }

  return scope.Close(v8::ThrowException(TRI_CreateErrorObject(TRI_ERROR_CURSOR_NOT_FOUND)));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if the cursor is exhausted
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_HasNextGeneralCursor (v8::Arguments const& argv) {
  v8::HandleScope scope;
  v8::TryCatch tryCatch;

  if (argv.Length() != 0) {
    return scope.Close(v8::ThrowException(
                         TRI_CreateErrorObject(TRI_ERROR_ILLEGAL_OPTION,
                                               "usage: hasNext()")));
  }

  TRI_vocbase_t* vocbase = GetContextVocBase();

  if (vocbase == 0) {
    return scope.Close(v8::ThrowException(
                         TRI_CreateErrorObject(TRI_ERROR_INTERNAL,
                                               "corrupted vocbase")));
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

  return scope.Close(v8::ThrowException(TRI_CreateErrorObject(TRI_ERROR_CURSOR_NOT_FOUND)));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unuse a general cursor
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_UnuseGeneralCursor (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 0) {
    return scope.Close(v8::ThrowException(
                         TRI_CreateErrorObject(TRI_ERROR_ILLEGAL_OPTION,
                                               "usage: unuse()")));
  }

  TRI_vocbase_t* vocbase = GetContextVocBase();

  if (!vocbase) {
    return scope.Close(v8::ThrowException(
                         TRI_CreateErrorObject(TRI_ERROR_INTERNAL,
                                               "corrupted vocbase")));
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
    return scope.Close(v8::ThrowException(
                         TRI_CreateErrorObject(TRI_ERROR_ILLEGAL_OPTION,
                                               "usage: CURSOR(<cursor-identifier>)")));
  }

  TRI_vocbase_t* vocbase = GetContextVocBase();

  if (vocbase == 0) {
    return scope.Close(v8::ThrowException(
                         TRI_CreateErrorObject(TRI_ERROR_INTERNAL,
                                               "corrupted vocbase")));
  }

  // get the id
  v8::Handle<v8::Value> idArg = argv[0]->ToString();

  if (! idArg->IsString()) {
    return scope.Close(v8::ThrowException(
                         TRI_CreateErrorObject(TRI_ERROR_ILLEGAL_OPTION,
                                               "expecting a string for <cursor-identifier>)")));
  }

  string idString = TRI_ObjectToString(idArg);
  uint64_t id = TRI_UInt64String(idString.c_str());

  TRI_general_cursor_t* cursor;

  cursor = (TRI_general_cursor_t*) TRI_BeginUsageIdShadowData(vocbase->_cursors, id);

  if (cursor == 0) {
    return scope.Close(v8::ThrowException(TRI_CreateErrorObject(TRI_ERROR_CURSOR_NOT_FOUND)));
  }

  return scope.Close(WrapGeneralCursor(cursor));
}
 
////////////////////////////////////////////////////////////////////////////////
/// @brief delete a (persistent) cursor by its id
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_DeleteCursor (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 1) {
    return scope.Close(v8::ThrowException(
                         TRI_CreateErrorObject(TRI_ERROR_ILLEGAL_OPTION,
                                               "usage: DELETE_CURSOR(<cursor-identifier>)")));
  }

  TRI_vocbase_t* vocbase = GetContextVocBase();

  if (vocbase == 0) {
    return scope.Close(v8::ThrowException(
                         TRI_CreateErrorObject(TRI_ERROR_INTERNAL,
                                               "corrupted vocbase")));
  }

  // get the id
  v8::Handle<v8::Value> idArg = argv[0]->ToString();

  if (! idArg->IsString()) {
    return scope.Close(v8::ThrowException(
                         TRI_CreateErrorObject(TRI_ERROR_ILLEGAL_OPTION,
                                               "expecting a string for <cursor-identifier>)")));
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
    return scope.Close(v8::ThrowException(v8::String::New("usage: AHUACATL_RUN(<querystring>, <bindvalues>, <doCount>, <batchSize>, <allowDirectReturn>)")));
  }

  TRI_vocbase_t* vocbase = GetContextVocBase();
  if (!vocbase) {
    v8::Handle<v8::Object> errorObject = TRI_CreateErrorObject(TRI_ERROR_INTERNAL, "corrupted vocbase");

    return scope.Close(v8::ThrowException(errorObject));
  }

  // get the query string
  v8::Handle<v8::Value> queryArg = argv[0];
  if (!queryArg->IsString()) {
    v8::Handle<v8::Object> errorObject = TRI_CreateErrorObject(TRI_ERROR_BAD_PARAMETER, "expecting string for <querystring>");

    return scope.Close(v8::ThrowException(errorObject));
  }

  const string queryString = TRI_ObjectToString(queryArg);
  // return number of total records in cursor?
  bool doCount = false;
  // maximum number of results to return at once
  uint32_t batchSize = 1000;
  // directly return the results as a javascript array instead of a cursor object (performance optimisation)
  bool allowDirectReturn = false;
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
  triagens::rest::JsonContainer parameters(TRI_UNKNOWN_MEM_ZONE, argc > 1 ? TRI_JsonObject(argv[1]) : 0);

  AhuacatlContextGuard context(vocbase, queryString);
  if (! context.valid()) {
    v8::Handle<v8::Object> errorObject = TRI_CreateErrorObject(TRI_ERROR_OUT_OF_MEMORY);

    return scope.Close(v8::ThrowException(errorObject));
  }

  v8::Handle<v8::Value> result;
  result = ExecuteQueryCursorAhuacatl(vocbase, context.ptr(), parameters.ptr(), doCount, batchSize, allowDirectReturn);
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
    return scope.Close(v8::ThrowException(v8::String::New("usage: AHUACATL_EXPLAIN(<querystring>, <bindvalues>, <performoptimisations>)")));
  }

  TRI_vocbase_t* vocbase = GetContextVocBase();
  if (!vocbase) {
    v8::Handle<v8::Object> errorObject = TRI_CreateErrorObject(TRI_ERROR_INTERNAL, "corrupted vocbase");

    return scope.Close(v8::ThrowException(errorObject));
  }

  // get the query string
  v8::Handle<v8::Value> queryArg = argv[0];
  if (!queryArg->IsString()) {
    v8::Handle<v8::Object> errorObject = TRI_CreateErrorObject(TRI_ERROR_BAD_PARAMETER, "expecting string for <querystring>");

    return scope.Close(v8::ThrowException(errorObject));
  }

  const string queryString = TRI_ObjectToString(queryArg);

  // bind parameters
  triagens::rest::JsonContainer parameters(TRI_UNKNOWN_MEM_ZONE, argc > 1 ? TRI_JsonObject(argv[1]) : 0);

  AhuacatlContextGuard context(vocbase, queryString);
  if (! context.valid()) {
    v8::Handle<v8::Object> errorObject = TRI_CreateErrorObject(TRI_ERROR_OUT_OF_MEMORY);

    return scope.Close(v8::ThrowException(errorObject));
  }
  
  bool performOptimisations = true;
  if (argc > 2) {
    // turn off optimisations ? 
    performOptimisations = TRI_ObjectToBoolean(argv[2]);
  }

  TRI_json_t* explain = 0;

  if (!TRI_ValidateQueryContextAql(context.ptr()) ||
      !TRI_BindQueryContextAql(context.ptr(), parameters.ptr()) ||
      !TRI_LockQueryContextAql(context.ptr()) ||
      (performOptimisations && !TRI_OptimiseQueryContextAql(context.ptr())) ||
      !(explain = TRI_ExplainAql(context.ptr()))) {
    v8::Handle<v8::Object> errorObject = CreateErrorObjectAhuacatl(&(context.ptr())->_error);
    return scope.Close(v8::ThrowException(errorObject));
  }

  assert(explain);

  v8::Handle<v8::Value> result;
  result = TRI_ObjectJson(explain);
  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, explain);
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
/// @brief parses an AQL query and returns the parse result
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_ParseAhuacatl (v8::Arguments const& argv) {
  v8::HandleScope scope;
  v8::TryCatch tryCatch;

  if (argv.Length() != 1) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: AHUACATL_PARSE(<querystring>)")));
  }

  TRI_vocbase_t* vocbase = GetContextVocBase();
  if (!vocbase) {
    v8::Handle<v8::Object> errorObject = TRI_CreateErrorObject(TRI_ERROR_INTERNAL, "corrupted vocbase");

    return scope.Close(v8::ThrowException(errorObject));
  }

  // get the query string
  v8::Handle<v8::Value> queryArg = argv[0];
  if (!queryArg->IsString()) {
    return scope.Close(v8::ThrowException(v8::String::New("expecting string for <querystring>")));
  }
  string queryString = TRI_ObjectToString(queryArg);

  AhuacatlContextGuard context(vocbase, queryString);
  if (! context.valid()) {
    v8::Handle<v8::Object> errorObject = TRI_CreateErrorObject(TRI_ERROR_OUT_OF_MEMORY);

    return scope.Close(v8::ThrowException(errorObject));
  }

  // parse & validate
  if (!TRI_ValidateQueryContextAql(context.ptr())) {
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
    TRI_voc_eid_t _sid;

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
    TRI_voc_eid_t _sid;
  }
  doc_deletion_marker_t_deprecated;


  if (argv.Length() != 0) {
    return scope.Close(v8::ThrowException(TRI_CreateErrorObject(TRI_ERROR_ILLEGAL_OPTION, "usage: upgrade()")));
  }
  
  v8::Handle<v8::Object> err;
  TRI_vocbase_col_t const* collection;
  TRI_document_collection_t* sim = TRI_ExtractAndUseSimpleCollection(argv, collection, &err);
  
  if (sim == 0) {
    return scope.Close(v8::ThrowException(err));
  }

  TRI_collection_t* col = &sim->base.base;
  const char* name = col->_name;
  TRI_col_version_t version = col->_version;

  if (version >= 3) {
    LOG_ERROR("Cannot upgrade collection '%s' with version '%d' in directory '%s'", name, version, col->_directory);
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
    
    LOG_INFO("convert file '%s' (size = %d)", df->_filename, fileSize);
  
    fd = TRI_OPEN(df->_filename, O_RDONLY);
    if (fd < 0) {
      LOG_ERROR("could not open file '%s' for reading", df->_filename);

      TRI_DestroyVectorPointer(&files);
      TRI_ReleaseCollection(collection);
  
      return scope.Close(v8::False());
    }

    ostringstream outfile;
    outfile << df->_filename << ".new";
   
    fdout = TRI_CREATE(outfile.str().c_str(), O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR);
    if (fdout < 0) {
      LOG_ERROR("could not open file '%s' for writing", outfile.str().c_str());
      
      TRI_DestroyVectorPointer(&files);
      TRI_ReleaseCollection(collection);
  
      close(fd);
      return scope.Close(v8::False());
    }

    //LOG_INFO("fd: %d, fdout: %d", fd, fdout);

    TRI_df_marker_t marker;

    while (true) {
      // read marker header
      ssize_t bytesRead = ::read(fd, &marker, sizeof(marker));

      if (bytesRead == 0) {
        // eof
        break;
      }

      if (bytesRead < (ssize_t) sizeof(marker)) {
        // eof
        LOG_WARNING("bytesRead = %d < sizeof(marker) = %d", bytesRead, sizeof(marker));
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
          LOG_WARNING("marker._size = %d < sizeof(marker) = %d", marker._size, sizeof(marker));
          break;
        }
        
        off_t paddedSize = ((marker._size + TRI_DF_BLOCK_ALIGN - 1) / TRI_DF_BLOCK_ALIGN) * TRI_DF_BLOCK_ALIGN;
        
        char payload[paddedSize];
        char* p = (char*) &payload;
      
        // copy header
                
        memcpy(&payload, &marker, sizeof(marker));
        
        if (marker._size > sizeof(marker)) {
          //int r = ::read(fd, p + sizeof(marker), marker._size - sizeof(marker));
          int r = ::read(fd, p + sizeof(marker), paddedSize - sizeof(marker));
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
            doc_document_marker_t_deprecated* oldMarker = (doc_document_marker_t_deprecated*) &payload;
            TRI_doc_document_key_marker_t newMarker;
            TRI_voc_size_t newMarkerSize = sizeof(TRI_doc_document_key_marker_t);

            char* body = ((char*) oldMarker) + sizeof(doc_document_marker_t_deprecated);
            TRI_voc_size_t bodySize = oldMarker->base._size - sizeof(doc_document_marker_t_deprecated); 
            TRI_voc_size_t bodySizePadded = paddedSize - sizeof(doc_document_marker_t_deprecated); 
            
            char* keyBody = 0;
            TRI_voc_size_t keyBodySize = 0; 
            
            TRI_voc_size_t keySize = 0;
            char didBuffer[33];  

            memset(&newMarker, 0, newMarkerSize); 

            sprintf(didBuffer,"%d", (unsigned int) oldMarker->_did);
            keySize = strlen(didBuffer) + 1;
            keyBodySize = ((keySize + TRI_DF_BLOCK_ALIGN - 1) / TRI_DF_BLOCK_ALIGN) * TRI_DF_BLOCK_ALIGN;
            keyBody = (char*) TRI_Allocate(TRI_CORE_MEM_ZONE, keyBodySize, true);
            TRI_CopyString(keyBody, didBuffer, keySize);      

            newMarker._rid = oldMarker->_rid;
            newMarker._sid = oldMarker->_sid;
            newMarker._shape = oldMarker->_shape;
            newMarker._offsetKey = newMarkerSize;
            newMarker._offsetJson = newMarkerSize + keyBodySize;
            
            newMarker.base._type = TRI_DOC_MARKER_KEY_DOCUMENT;
            newMarker.base._tick = oldMarker->base._tick;
            newMarker.base._size = newMarkerSize + keyBodySize + bodySize;
            TRI_FillCrcKeyMarkerDatafile(&newMarker.base, newMarkerSize, keyBody, keyBodySize, body, bodySize);

            write(fdout, &newMarker, sizeof(newMarker));
            write(fdout, keyBody, keyBodySize);
            write(fdout, body, bodySizePadded);

            //LOG_INFO("found doc marker, type: '%d', did: '%d', rid: '%d', size: '%d', crc: '%d'", marker._type, oldMarker->_did, oldMarker->_rid,newMarker.base._size,newMarker.base._crc);

            TRI_Free(TRI_CORE_MEM_ZONE, keyBody);

            writtenSize += sizeof(newMarker) + keyBodySize + bodySizePadded;
            break;
          }
            
          case TRI_DOC_MARKER_EDGE: {
            doc_edge_marker_t_deprecated* oldMarker = (doc_edge_marker_t_deprecated*) &payload;            
            TRI_doc_edge_key_marker_t newMarker;
            TRI_voc_size_t newMarkerSize = sizeof(TRI_doc_edge_key_marker_t);
            
            char* body = ((char*) oldMarker) + sizeof(doc_edge_marker_t_deprecated);
            TRI_voc_size_t bodySize = oldMarker->base.base._size - sizeof(doc_edge_marker_t_deprecated); 
            TRI_voc_size_t bodySizePadded = paddedSize - sizeof(doc_edge_marker_t_deprecated); 
            
            char* keyBody = 0;
            TRI_voc_size_t keyBodySize = 0;
            
            size_t keySize = 0;
            size_t toSize = 0;
            size_t fromSize = 0;            
            
            char didBuffer[33];  
            char toDidBuffer[33];  
            char fromDidBuffer[33];  

            memset(&newMarker, 0, newMarkerSize); 

            sprintf(didBuffer,"%d", (unsigned int) oldMarker->base._did);
            sprintf(toDidBuffer,"%d", (unsigned int) oldMarker->_toDid);
            sprintf(fromDidBuffer,"%d", (unsigned int) oldMarker->_fromDid);
            
            keySize = strlen(didBuffer) + 1;
            toSize = strlen(toDidBuffer) + 1;
            fromSize = strlen(fromDidBuffer) + 1;

            keyBodySize = ((keySize + toSize + fromSize + TRI_DF_BLOCK_ALIGN - 1) / TRI_DF_BLOCK_ALIGN) * TRI_DF_BLOCK_ALIGN;            
            keyBody = (char*) TRI_Allocate(TRI_CORE_MEM_ZONE, keyBodySize, true);
            
            TRI_CopyString(keyBody,                    didBuffer,     keySize);      
            TRI_CopyString(keyBody + keySize,          toDidBuffer,   toSize);      
            TRI_CopyString(keyBody + keySize + toSize, fromDidBuffer, fromSize);      

            newMarker.base._rid = oldMarker->base._rid;
            newMarker.base._sid = oldMarker->base._sid;                        
            newMarker.base._shape = oldMarker->base._shape;
            newMarker.base._offsetKey = newMarkerSize;
            newMarker.base._offsetJson = newMarkerSize + keyBodySize;
            
            newMarker._offsetToKey = newMarkerSize + keySize;
            newMarker._offsetFromKey = newMarkerSize + keySize + toSize;
            newMarker._toCid = oldMarker->_toCid;
            newMarker._fromCid = oldMarker->_fromCid;
            newMarker._isBidirectional = 0;
            
            newMarker.base.base._size = newMarkerSize + keyBodySize + bodySize;
            newMarker.base.base._type = TRI_DOC_MARKER_KEY_EDGE;
            newMarker.base.base._tick = oldMarker->base.base._tick;

            TRI_FillCrcKeyMarkerDatafile(&newMarker.base.base, newMarkerSize, keyBody, keyBodySize, body, bodySize);

            write(fdout, &newMarker, newMarkerSize);
            write(fdout, keyBody, keyBodySize);
            write(fdout, body, bodySizePadded);

            //LOG_INFO("found edge marker, type: '%d', did: '%d', rid: '%d', size: '%d', crc: '%d'", marker._type, oldMarker->base._did, oldMarker->base._rid,newMarker.base.base._size,newMarker.base.base._crc);

            TRI_Free(TRI_CORE_MEM_ZONE, keyBody);

            writtenSize += newMarkerSize + keyBodySize + bodySizePadded;
            break;
          }

          case TRI_DOC_MARKER_DELETION: {
            doc_deletion_marker_t_deprecated* oldMarker = (doc_deletion_marker_t_deprecated*) &payload;                        
            TRI_doc_deletion_key_marker_t newMarker;
            TRI_voc_size_t newMarkerSize = sizeof(TRI_doc_deletion_key_marker_t);
            
            TRI_voc_size_t keyBodySize = 0; 
            char* keyBody = 0;
            TRI_voc_size_t keySize = 0;
            char didBuffer[33];  

            memset(&newMarker, 0, newMarkerSize); 

            sprintf(didBuffer,"%d", (unsigned int) oldMarker->_did);
            keySize = strlen(didBuffer) + 1;
            keyBodySize = ((keySize + TRI_DF_BLOCK_ALIGN - 1) / TRI_DF_BLOCK_ALIGN) * TRI_DF_BLOCK_ALIGN;
            keyBody = (char*) TRI_Allocate(TRI_CORE_MEM_ZONE, keyBodySize, true);
            TRI_CopyString(keyBody, didBuffer, keySize);      

            newMarker._rid = oldMarker->_rid;
            newMarker._sid = oldMarker->_sid;
            newMarker._offsetKey = newMarkerSize;
            
            newMarker.base._size = newMarkerSize + keyBodySize;
            newMarker.base._type = TRI_DOC_MARKER_KEY_DELETION;
            newMarker.base._tick = oldMarker->base._tick;

            TRI_FillCrcKeyMarkerDatafile(&newMarker.base, newMarkerSize, keyBody, keyBodySize, NULL, 0);

            write(fdout, &newMarker, newMarkerSize);
            write(fdout, (char*) keyBody, keyBodySize);

            //LOG_INFO("found deletion marker, type: '%d', did: '%d', rid: '%d'", marker._type, oldMarker->_did, oldMarker->_rid);

            TRI_Free(TRI_CORE_MEM_ZONE, keyBody);
            
            writtenSize += newMarker.base._size;
            break;
          }

          default: {
            // copy other types without modification
            
            write(fdout, &payload, sizeof(payload));
            writtenSize += sizeof(payload);
            //LOG_INFO("found marker, type: '%d'", marker._type);

          }
        }

      }
      else if (bytesRead == 0) {
        // eof
        break;
      }
      else {
        LOG_ERROR("Could not read data from file '%s' while upgrading collection '%s'.", df->_filename, name);
        LOG_ERROR("Remove collection manually.");
        close(fd);
        close(fdout);

        TRI_DestroyVectorPointer(&files);
        TRI_ReleaseCollection(collection);
  
        return scope.Close(v8::False());
      }
    }

    // fill up
    if (writtenSize < fileSize) {
      const int max = 10000;
      char b[max];
      memset(b, 0, max); 
      
      while (writtenSize + max < fileSize) {
        write(fdout, b, max);
        writtenSize += max;
      }
      
      if (writtenSize < fileSize) {
        write(fdout, b, fileSize - writtenSize);
      }
    }

    // file converted!
    close(fd);
    close(fdout);
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
  
  TRI_ReleaseCollection(collection);
  
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
    return scope.Close(v8::ThrowException(v8::String::New("illegal collection pointer")));
  }

  if (argv.Length() != 1) {
    return scope.Close(v8::ThrowException(TRI_CreateErrorObject(TRI_ERROR_ILLEGAL_OPTION, "usage: datafileScan(<path>)")));
  }

  string path = TRI_ObjectToString(argv[0]);

  TRI_READ_LOCK_STATUS_VOCBASE_COL(collection);

  if (collection->_status != TRI_VOC_COL_STATUS_UNLOADED) {
    TRI_READ_UNLOCK_STATUS_VOCBASE_COL(collection);
    return scope.Close(v8::ThrowException(TRI_CreateErrorObject(TRI_ERROR_ARANGO_COLLECTION_NOT_UNLOADED))); 
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

  v8::Handle<v8::Object> err;
  TRI_vocbase_col_t const* collection = UseCollection(argv.Holder(), &err);

  if (collection == 0) {
    return scope.Close(v8::ThrowException(err));
  }

  TRI_primary_collection_t* primary = collection->_collection;
  size_t s = primary->size(primary);

  TRI_ReleaseCollection(collection);
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
    return scope.Close(v8::ThrowException(v8::String::New("illegal collection pointer")));
  }

  TRI_READ_LOCK_STATUS_VOCBASE_COL(collection);

  if (collection->_status != TRI_VOC_COL_STATUS_UNLOADED) {
    TRI_READ_UNLOCK_STATUS_VOCBASE_COL(collection);
    return scope.Close(v8::ThrowException(TRI_CreateErrorObject(TRI_ERROR_ARANGO_COLLECTION_NOT_UNLOADED)));
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
/// An error is thrown if the document does not exist.
///
/// The document must be part of the @FA{collection}; otherwise, an error
/// is thrown.
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
/// @TINYEXAMPLE{shell-read-document,read document from a collection}
///
/// An error is raised if the document is unknown:
///
/// @TINYEXAMPLE{shell-read-document-not-found,unknown handle}
///
/// An error is raised if the handle is invalid:
///
/// @TINYEXAMPLE{shell-read-document-bad-handle,invalid handle}
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_DocumentVocbaseCol (v8::Arguments const& argv) {
  v8::HandleScope scope;

  // extract the collection
  v8::Handle<v8::Object> operand = argv.Holder();

  v8::Handle<v8::Object> err;
  TRI_vocbase_col_t const* collection = UseCollection(operand, &err);

  if (collection == 0) {
    return scope.Close(v8::ThrowException(err));
  }

  return DocumentVocbaseCol(collection->_vocbase, collection, argv, true);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a document
///
/// it is the caller's responsibility to acquire and release the required locks
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_DocumentNLVocbaseCol (v8::Arguments const& argv) {
  v8::HandleScope scope;

  // extract the collection
  TRI_vocbase_col_t* col = TRI_UnwrapClass<TRI_vocbase_col_t>(argv.Holder(), WRP_VOCBASE_COL_TYPE);
  if (col == 0 || col->_collection == 0) {
    return scope.Close(v8::ThrowException(TRI_CreateErrorObject(TRI_ERROR_INTERNAL, "cannot use/load collection")));
  }

  TRI_vocbase_col_t const* collection = col;

  return DocumentVocbaseCol(collection->_vocbase, collection, argv, false);
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
    res = TRI_ERROR_INTERNAL;
  }
  else {
    res = TRI_DropCollectionVocBase(collection->_vocbase, collection);
  }

  if (res != TRI_ERROR_NO_ERROR) {
    return scope.Close(v8::ThrowException(TRI_CreateErrorObject(res, "cannot drop collection", true)));
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

  v8::Handle<v8::Object> err;
  TRI_vocbase_col_t const* collection = UseCollection(argv.Holder(), &err);

  if (collection == 0) {
    return scope.Close(v8::ThrowException(err));
  }

  TRI_primary_collection_t* primary = collection->_collection;

  if (! TRI_IS_DOCUMENT_COLLECTION(primary->base._type)) {
    TRI_ReleaseCollection(collection);
    return scope.Close(v8::ThrowException(TRI_CreateErrorObject(TRI_ERROR_INTERNAL, "unknown collection type")));
  }

  TRI_document_collection_t* sim = (TRI_document_collection_t*) primary;

  if (argv.Length() != 1) {
    TRI_ReleaseCollection(collection);
    return scope.Close(v8::ThrowException(TRI_CreateErrorObject(TRI_ERROR_ILLEGAL_OPTION, "usage: dropIndex(<index-handle>)")));
  }

  TRI_index_t* idx = TRI_LookupIndexByHandle(primary->base._vocbase, collection, argv[0], true, &err);

  if (idx == 0) {
    if (err.IsEmpty()) {
      TRI_ReleaseCollection(collection);
      return scope.Close(v8::False());
    }
    else {
      TRI_ReleaseCollection(collection);
      return scope.Close(v8::ThrowException(err));
    }
  }

  if (idx->_iid == 0) {
    TRI_ReleaseCollection(collection);
    return scope.Close(v8::False());
  }

  // .............................................................................
  // inside a write transaction
  // .............................................................................

  bool ok = TRI_DropIndexDocumentCollection(sim, idx->_iid);

  // .............................................................................
  // outside a write transaction
  // .............................................................................

  TRI_ReleaseCollection(collection);
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

  v8::Handle<v8::Object> err;
  TRI_vocbase_col_t const* collection = UseCollection(argv.Holder(), &err);

  if (collection == 0) {
    return scope.Close(v8::ThrowException(err));
  }

  TRI_primary_collection_t* primary = collection->_collection;

  if (! TRI_IS_DOCUMENT_COLLECTION(primary->base._type)) {
    TRI_ReleaseCollection(collection);
    return scope.Close(v8::ThrowException(TRI_CreateErrorObject(TRI_ERROR_INTERNAL, "unknown collection type")));
  }

  TRI_document_collection_t* sim = (TRI_document_collection_t*) primary;
  TRI_index_t* idx = 0;
  bool created;

  if (argv.Length() == 1) {
    size_t size = (size_t) TRI_ObjectToDouble(argv[0]);

    if (size <= 0) {
      TRI_ReleaseCollection(collection);
      return scope.Close(v8::ThrowException(
        TRI_CreateErrorObject(TRI_ERROR_ILLEGAL_OPTION,
                              "<size> must be at least 1")));
    }

    idx = TRI_EnsureCapConstraintDocumentCollection(sim, size, &created);
  }

  // .............................................................................
  // error case
  // .............................................................................

  else {
    TRI_ReleaseCollection(collection);

    return scope.Close(v8::ThrowException(
      TRI_CreateErrorObject(TRI_ERROR_ILLEGAL_OPTION,
                            "ensureCapConstraint(<size>)")));
  }

  if (idx == 0) {
    TRI_ReleaseCollection(collection);
    return scope.Close(v8::ThrowException(TRI_CreateErrorObject(TRI_errno(), "index could not be created", true)));
  }

  TRI_json_t* json = idx->json(idx, collection->_collection);
  v8::Handle<v8::Value> index = IndexRep(&primary->base, json);
  TRI_FreeJson(TRI_CORE_MEM_ZONE, json);

  if (index->IsObject()) {
    index->ToObject()->Set(v8::String::New("isNewlyCreated"), created ? v8::True() : v8::False());
  }

  TRI_ReleaseCollection(collection);
  return scope.Close(index);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that a bitarray index exists
///
/// @FUN{@FA{collection}.ensureBitarray(@FA{field1}, @FA{value1}, @FA{field2}, @FA{value2},...,@FA{fieldn}, @FA{valuen})}
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

static v8::Handle<v8::Value> EnsureBitarray (v8::Arguments const& argv, bool supportUndef) {

  v8::HandleScope scope;
  bool ok;
  string errorString;
  int errorCode;
  TRI_index_t* bitarrayIndex = 0;
  bool indexCreated;
  v8::Handle<v8::Value> theIndex;
  
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

  if (! TRI_IS_DOCUMENT_COLLECTION(primary->base._type)) {
    TRI_ReleaseCollection(collection);
    return scope.Close(v8::ThrowException(TRI_CreateErrorObject(TRI_ERROR_INTERNAL, "unknown collection type")));
  }

  TRI_document_collection_t* sim = (TRI_document_collection_t*) primary;
  
  // .............................................................................
  // Ensure that there is at least one string parameter sent to this method
  // .............................................................................

  if ( (argv.Length() < 2) || (argv.Length() % 2 != 0) ) {
    LOG_WARNING("bitarray index creation failed -- invalid parameters (require key_1,values_1,...,key_n,values_n)");
    TRI_ReleaseCollection(collection);
    return scope.Close(v8::ThrowException(TRI_CreateErrorObject(TRI_ERROR_ILLEGAL_OPTION, "usage: ensureBitarray(path 1, <list of values 1>, <path 2>, <list of values 2>, ...)")));
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
        errorCode   = TRI_ERROR_ILLEGAL_OPTION;
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
        errorCode   = TRI_ERROR_ILLEGAL_OPTION;
        ok = false;
        break;
      }
    

      // .........................................................................
      // Attempt to convert the V8 javascript function argument into a TRI_json_t
      // .........................................................................
      
      TRI_json_t* value = TRI_JsonObject(argument);
      
      
      // .........................................................................
      // If the conversion from V8 value into a TRI_json_t fails, exit
      // .........................................................................
      
      if (value == NULL) {
        errorString = "invalid parameter -- expected an array (list)";
        errorCode   = TRI_ERROR_ILLEGAL_OPTION;
        ok = false;
        break;
      }
      

      // .........................................................................
      // If the TRI_json_t is NOT a list, then exit with an error
      // .........................................................................

      if (value->_type != TRI_JSON_LIST) {
        errorString = "invalid parameter -- expected an array (list)";
        errorCode   = TRI_ERROR_ILLEGAL_OPTION;
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
      errorCode   = TRI_ERROR_ILLEGAL_OPTION;
      ok = false;
    }
  }
  
  
  
  // .............................................................................
  // Actually create the index here
  // .............................................................................

  if (ok) {
    char* errorStr = 0;
    bitarrayIndex = TRI_EnsureBitarrayIndexDocumentCollection(sim, &attributes, &values, supportUndef, &indexCreated, &errorCode, &errorStr);
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

    TRI_json_t* json = bitarrayIndex->json(bitarrayIndex, collection->_collection);

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
    }
      
    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
  }

  
  TRI_ReleaseCollection(collection);

  if (!ok || bitarrayIndex == 0) {
    return scope.Close(v8::ThrowException(TRI_CreateErrorObject(errorCode, errorString)));
  }

  return scope.Close(theIndex);
}

static v8::Handle<v8::Value> JS_EnsureBitarrayVocbaseCol (v8::Arguments const& argv) {
  return EnsureBitarray(argv, false);
}

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
/// Note that non-existing attribute paths in a document are treat as if the
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
/// Creates a unique hash index on all documents using @FA{field1}, @FA{field2},
/// ... as attribute paths. At least one attribute path must be given.
///
/// Note that non-existing attribute paths in a document are treat as if the
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
/// @FUN{ensureSLIndex(@FA{field1})}
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

  if (! TRI_IS_DOCUMENT_COLLECTION(primary->base._type)) {
    TRI_ReleaseCollection(collection);
    return scope.Close(v8::ThrowException(v8::String::New("unknown collection type")));
  }

  TRI_document_collection_t* sim = (TRI_document_collection_t*) primary;

  // .............................................................................
  // Return string when there is an error of some sort.
  // .............................................................................

  string errorString;

  // .............................................................................
  // Ensure that there is at least one string parameter sent to this method
  // .............................................................................

  if (argv.Length() != 1) {
    TRI_ReleaseCollection(collection);

    errorString = "one string parameter required for the ensurePQIndex(...) command";
    return scope.Close(v8::String::New(errorString.c_str(),errorString.length()));
  }


  // .............................................................................
  // Create a list of paths, these will be used to create a list of shapes
  // which will be used by the priority queue index.
  // .............................................................................

  TRI_vector_pointer_t attributes;
  TRI_InitVectorPointer(&attributes, TRI_CORE_MEM_ZONE);

  int res = FillVectorPointerFromArguments(argv, &attributes, 0, argv.Length(), errorString);

  // .............................................................................
  // Some sort of error occurred -- display error message and abort index creation
  // (or index retrieval).
  // .............................................................................

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_DestroyVectorPointer(&attributes);

    TRI_ReleaseCollection(collection);
    return scope.Close(v8::ThrowException(TRI_CreateErrorObject(res, errorString)));
  }

  // .............................................................................
  // Actually create the index here. Note that priority queue is never unique.
  // .............................................................................

  idx = TRI_EnsurePriorityQueueIndexDocumentCollection(sim, &attributes, false, &created);

  // .............................................................................
  // Remove the memory allocated to the list of attributes used for the hash index
  // .............................................................................

  TRI_FreeContentVectorPointer(TRI_CORE_MEM_ZONE, &attributes);
  TRI_DestroyVectorPointer(&attributes);

  if (idx == 0) {
    TRI_ReleaseCollection(collection);
    return scope.Close(v8::String::New("Priority Queue index could not be created"));
  }

  // .............................................................................
  // Return the newly assigned index identifier
  // .............................................................................

  TRI_json_t* json = idx->json(idx, collection->_collection);

  v8::Handle<v8::Value> index = IndexRep(&primary->base, json);
  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);

  if (index->IsObject()) {
    index->ToObject()->Set(v8::String::New("isNewlyCreated"), created ? v8::True() : v8::False());
  }

  TRI_ReleaseCollection(collection);
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
/// @verbinclude fluent14
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
/// @verbinclude fluent14
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
///
/// @EXAMPLES
///
/// @verbinclude shell_collection-figures
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_FiguresVocbaseCol (v8::Arguments const& argv) {
  v8::HandleScope scope;

  TRI_vocbase_col_t* collection = TRI_UnwrapClass<TRI_vocbase_col_t>(argv.Holder(), WRP_VOCBASE_COL_TYPE);

  if (collection == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("illegal collection pointer")));
  }

  v8::Handle<v8::Object> result = v8::Object::New();

  TRI_READ_LOCK_STATUS_VOCBASE_COL(collection);
  TRI_vocbase_col_status_e status = collection->_status;

  if (status != TRI_VOC_COL_STATUS_LOADED) {
    TRI_READ_UNLOCK_STATUS_VOCBASE_COL(collection);
    return scope.Close(result);
  }

  if (collection->_collection == 0) {
    TRI_READ_UNLOCK_STATUS_VOCBASE_COL(collection);
    return scope.Close(v8::ThrowException(v8::String::New("illegal collection pointer")));
  }

  TRI_primary_collection_t* primary = collection->_collection;

  primary->beginRead(primary);
  TRI_doc_collection_info_t* info = primary->figures(primary);
  primary->endRead(primary);

  if (info == NULL) {
    TRI_READ_UNLOCK_STATUS_VOCBASE_COL(collection);
    v8::Handle<v8::Object> errorObject = TRI_CreateErrorObject(TRI_ERROR_OUT_OF_MEMORY);

    return scope.Close(v8::ThrowException(errorObject));
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

  TRI_Free(TRI_UNKNOWN_MEM_ZONE, info);

  TRI_READ_UNLOCK_STATUS_VOCBASE_COL(collection);
  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns information about the indexes
///
/// it is the caller's responsibility to acquire and release all required locks
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> GetIndexesVocbaseCol (v8::Arguments const& argv, 
                                                   const bool lock) {
  v8::HandleScope scope;

  v8::Handle<v8::Object> err;
  TRI_vocbase_col_t const* collection = 0;
  
  if (lock) {
    collection = UseCollection(argv.Holder(), &err);
  }
  else {
    TRI_vocbase_col_t const* col = TRI_UnwrapClass<TRI_vocbase_col_t>(argv.Holder(), WRP_VOCBASE_COL_TYPE);
    if (col == 0 || col->_collection == 0) {
      return scope.Close(TRI_CreateErrorObject(TRI_ERROR_INTERNAL, "cannot use/load collection"));;
    }

    collection = col;
  }

  if (collection == 0) {
    return scope.Close(v8::ThrowException(err));
  }

  TRI_primary_collection_t* primary = collection->_collection;

  if (! TRI_IS_DOCUMENT_COLLECTION(primary->base._type)) {
    if (lock) {
      TRI_ReleaseCollection(collection);
    }
    return scope.Close(v8::ThrowException(v8::String::New("unknown collection type")));
  }

  TRI_document_collection_t* sim = (TRI_document_collection_t*) primary;

  // get a list of indexes
  TRI_vector_pointer_t* indexes = TRI_IndexesDocumentCollection(sim, lock);
 
  if (lock) { 
    TRI_ReleaseCollection(collection);
  }

  if (!indexes) {
    return scope.Close(v8::ThrowException(v8::String::New("out of memory")));
  }

  v8::Handle<v8::Array> result = v8::Array::New();

  uint32_t n = (uint32_t) indexes->_length;

  for (uint32_t i = 0, j = 0;  i < n;  ++i) {
    TRI_json_t* idx = (TRI_json_t*) indexes->_buffer[i];

    if (idx) {
      result->Set(j++, IndexRep(&primary->base, idx));
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, idx);
    }
  }

  TRI_FreeVectorPointer(TRI_UNKNOWN_MEM_ZONE, indexes);

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
  return GetIndexesVocbaseCol(argv, true);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns information about the indexes
///
/// it is the caller's responsibility to acquire and release all required locks
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_GetIndexesNLVocbaseCol (v8::Arguments const& argv) {
  return GetIndexesVocbaseCol(argv, false);
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

  TRI_ReleaseCollection(collection);
  return scope.Close(v8::Undefined());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the name of a collection
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_NameVocbaseCol (v8::Arguments const& argv) {
  v8::HandleScope scope;

  TRI_vocbase_col_t const* collection = TRI_UnwrapClass<TRI_vocbase_col_t>(argv.Holder(), WRP_VOCBASE_COL_TYPE);

  if (collection == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("illegal collection pointer")));
  }

  return scope.Close(v8::String::New(collection->_name));
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
  TRI_v8_global_t* v8g;
  v8::HandleScope scope;

  v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();

  v8::Handle<v8::Object> err;
  TRI_vocbase_col_t const* collection = UseCollection(argv.Holder(), &err);

  if (collection == 0) {
    return scope.Close(v8::ThrowException(err));
  }

  TRI_primary_collection_t* primary = collection->_collection;

  if (! TRI_IS_DOCUMENT_COLLECTION(primary->base._type)) {
    TRI_ReleaseCollection(collection);
    return scope.Close(v8::ThrowException(v8::String::New("unknown collection type")));
  }

  TRI_document_collection_t* sim = (TRI_document_collection_t*) primary;

  // check if we want to change some parameters
  if (0 < argv.Length()) {
    v8::Handle<v8::Value> par = argv[0];

    if (par->IsObject()) {
      v8::Handle<v8::Object> po = par->ToObject();

      // get the old values
      TRI_LOCK_JOURNAL_ENTRIES_DOC_COLLECTION(sim);

      bool waitForSync = sim->base.base._waitForSync;
      size_t maximalSize = sim->base.base._maximalSize;
      size_t maximumMarkerSize = sim->base.base._maximumMarkerSize;

      TRI_UNLOCK_JOURNAL_ENTRIES_DOC_COLLECTION(sim);

      // extract sync flag
      if (po->Has(v8g->WaitForSyncKey)) {
        waitForSync = TRI_ObjectToBoolean(po->Get(v8g->WaitForSyncKey));
      }

      // extract the journal size
      if (po->Has(v8g->JournalSizeKey)) {
        maximalSize = TRI_ObjectToDouble(po->Get(v8g->JournalSizeKey));

        if (maximalSize < TRI_JOURNAL_MINIMAL_SIZE) {
          TRI_ReleaseCollection(collection);
          return scope.Close(v8::ThrowException(
                               TRI_CreateErrorObject(TRI_ERROR_BAD_PARAMETER,
                                                     "<properties>.journalSize too small")));
        }

        if (maximalSize < maximumMarkerSize + TRI_JOURNAL_OVERHEAD) {
          TRI_ReleaseCollection(collection);
          return scope.Close(v8::ThrowException(
                               TRI_CreateErrorObject(TRI_ERROR_BAD_PARAMETER,
                                                     "<properties>.journalSize too small")));
        }
      }

      // update collection
      TRI_col_parameter_t newParameter;

      newParameter._maximalSize = maximalSize;
      newParameter._waitForSync = waitForSync;

      // try to write new parameter to file
      int res = TRI_UpdateParameterInfoCollection(collection->_vocbase, &sim->base.base, &newParameter);

      if (res != TRI_ERROR_NO_ERROR) {
        TRI_ReleaseCollection(collection);
        return scope.Close(v8::ThrowException(v8::String::New(TRI_last_error())));
      }
    }
  }

  // return the current parameter set
  v8::Handle<v8::Object> result = v8::Object::New();

  if (TRI_IS_DOCUMENT_COLLECTION(primary->base._type)) {
    TRI_voc_size_t maximalSize = sim->base.base._maximalSize;
    bool waitForSync = sim->base.base._waitForSync;

    result->Set(v8g->WaitForSyncKey, waitForSync ? v8::True() : v8::False());
    result->Set(v8g->JournalSizeKey, v8::Number::New(maximalSize));
  }

  TRI_ReleaseCollection(collection);
  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes a document
///
/// @FUN{@FA{collection}.remove(@FA{document})}
///
/// Deletes a document. If there is revision mismatch, then an error is thrown.
///
/// @FUN{@FA{collection}.remove(@FA{document}, true)}
///
/// Deletes a document. If there is revision mismatch, then mismatch
/// is ignored and document is deleted. The function returns
/// @LIT{true} if the document existed and was deleted. It returns
/// @LIT{false}, if the document was already deleted.
///
/// @FUN{@FA{collection}.remove(@FA{document}, true, @FA{waitForSync})}
///
/// The optional @FA{waitForSync} parameter can be used to force 
/// synchronisation of the document deletion operation to disk even in case
/// that the @LIT{waitForSync} flag had been disabled for the entire collection.
/// Thus, the @FA{waitForSync} URL parameter can be used to force synchronisation
/// of just specific operations. To use this, set the @FA{waitForSync} parameter
/// to @LIT{true}. If the @FA{waitForSync} parameter is not specified or set to 
/// @LIT{false}, then the collection's default @LIT{waitForSync} behavior is 
/// applied. The @FA{waitForSync} URL parameter cannot be used to disable
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
/// Delete a document:
///
/// @TINYEXAMPLE{shell_remove-document,delete a document}
///
/// Delete a document with a conflict:
///
/// @TINYEXAMPLE{shell_remove-document-conflict,delete a document}
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_RemoveVocbaseCol (v8::Arguments const& argv) {
  v8::HandleScope scope;

  v8::Handle<v8::Object> err;
  TRI_vocbase_col_t const* collection = UseCollection(argv.Holder(), &err);

  if (collection == 0) {
    return scope.Close(v8::ThrowException(err));
  }

  return DeleteVocbaseCol(collection->_vocbase, collection, argv);
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
    return scope.Close(v8::ThrowException(v8::String::New("usage: rename(<name>)")));
  }

  string name = TRI_ObjectToString(argv[0]);

  if (name.empty()) {
    return scope.Close(v8::ThrowException(v8::String::New("<name> must be non-empty")));
  }

  TRI_vocbase_col_t* collection = TRI_UnwrapClass<TRI_vocbase_col_t>(argv.Holder(), WRP_VOCBASE_COL_TYPE);

  if (collection == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("illegal collection pointer")));
  }

  int res = TRI_RenameCollectionVocBase(collection->_vocbase, collection, name.c_str());

  if (res != TRI_ERROR_NO_ERROR) {
    return scope.Close(v8::ThrowException(TRI_CreateErrorObject(res, "cannot rename collection", true)));
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
/// Thus, the @FA{waitForSync} URL parameter can be used to force synchronisation
/// of just specific operations. To use this, set the @FA{waitForSync} parameter
/// to @LIT{true}. If the @FA{waitForSync} parameter is not specified or set to 
/// @LIT{false}, then the collection's default @LIT{waitForSync} behavior is 
/// applied. The @FA{waitForSync} URL parameter cannot be used to disable
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
  v8::HandleScope scope;

  // extract the collection
  v8::Handle<v8::Object> err;
  TRI_vocbase_col_t const* collection = UseCollection(argv.Holder(), &err);

  if (collection == 0) {
    return scope.Close(v8::ThrowException(err));
  }

  return ReplaceVocbaseCol(collection->_vocbase, collection, argv);
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
/// Thus, the @FA{waitForSync} URL parameter can be used to force synchronisation
/// of just specific operations. To use this, set the @FA{waitForSync} parameter
/// to @LIT{true}. If the @FA{waitForSync} parameter is not specified or set to 
/// @LIT{false}, then the collection's default @LIT{waitForSync} behavior is 
/// applied. The @FA{waitForSync} URL parameter cannot be used to disable
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
  v8::HandleScope scope;

  // extract the collection
  v8::Handle<v8::Object> err;
  TRI_vocbase_col_t const* collection = UseCollection(argv.Holder(), &err);

  if (collection == 0) {
    return scope.Close(v8::ThrowException(err));
  }

  return UpdateVocbaseCol(collection->_vocbase, collection, argv);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief saves a document
///
/// This function makes the distinction between document and edge collections
/// and dispatches the request to the collection's specialised save function
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_SaveVocbaseCol (v8::Arguments const& argv) {
  v8::HandleScope scope;

  v8::Handle<v8::Object> err;
  TRI_vocbase_col_t const* collection = UseCollection(argv.Holder(), &err);

  if (collection == 0) {
    return scope.Close(v8::ThrowException(err));
  }

  v8::Handle<v8::Value> result;

  if (collection->_type == TRI_COL_TYPE_DOCUMENT) {
    result = SaveVocbaseCol(collection, argv);
  }
  else if (collection->_type == TRI_COL_TYPE_EDGE) {
    result = SaveEdgeCol(collection, argv);
  }

  TRI_ReleaseCollection(collection);
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
    return scope.Close(v8::ThrowException(v8::String::New("illegal collection pointer")));
  }
  
  if (argv.Length() != 2) {
    return scope.Close(v8::ThrowException(TRI_CreateErrorObject(TRI_ERROR_ILLEGAL_OPTION, "usage: setAttribute(<key>, <value>)")));
  }

  int res = TRI_ERROR_NO_ERROR;

  string key = TRI_ObjectToString(argv[0]);
  string value = TRI_ObjectToString(argv[1]);

  TRI_WRITE_LOCK_STATUS_VOCBASE_COL(collection);
  TRI_col_info_t info;
  res = TRI_LoadParameterInfoCollection(collection->_path, &info);

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
      res = TRI_SaveParameterInfoCollection(collection->_path, &info);
    }
  }

  TRI_WRITE_UNLOCK_STATUS_VOCBASE_COL(collection);

  if (res != TRI_ERROR_NO_ERROR) {
    return scope.Close(v8::ThrowException(TRI_CreateErrorObject(res, "setAttribute failed", true)));
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
    return scope.Close(v8::ThrowException(v8::String::New("illegal collection pointer")));
  }

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

  bool forceSync = false;
  if (1 == argv.Length()) {
    forceSync = TRI_ObjectToBoolean(argv[0]);
  }

  // extract and use the simple collection
  v8::Handle<v8::Object> err;
  TRI_vocbase_col_t const* collection;
  TRI_document_collection_t* sim = TRI_ExtractAndUseSimpleCollection(argv, collection, &err);

  if (sim == 0) {
    return scope.Close(v8::ThrowException(err));
  }

  TRI_primary_collection_t* primary = collection->_collection;

  TRI_doc_update_policy_e policy = TRI_DOC_UPDATE_LAST_WRITE;
  TRI_voc_rid_t oldRid = 0;

  TRI_vector_pointer_t documents;
  TRI_InitVectorPointer(&documents, TRI_UNKNOWN_MEM_ZONE);
  
  TRI_doc_mptr_t const** ptr;
  TRI_doc_mptr_t const** end;
  
  TRI_doc_operation_context_t context;
  TRI_InitContextPrimaryCollection(&context, primary, policy, forceSync);

  primary->beginWrite(collection->_collection);
  
  ptr = (TRI_doc_mptr_t const**) (primary->_primaryIndex._table);
  end = (TRI_doc_mptr_t const**) ptr + primary->_primaryIndex._nrAlloc;
  
  // first, collect all document pointers by traversing the primary index
  for (;  ptr < end;  ++ptr) {
    if (*ptr && (*ptr)->_validTo == 0) {
      TRI_PushBackVectorPointer(&documents, (void*) *ptr);
    }
  }
  
  // now delete all documents. this will modify the index as well
  for (size_t i = 0; i < documents._length; ++i) {
    TRI_doc_mptr_t const* d = (TRI_doc_mptr_t const*) documents._buffer[i];
  
    context._expectedRid = d->_rid;
    context._previousRid = &oldRid;
    
    int res = primary->destroy(&context, d->_key);
    if (res != TRI_ERROR_NO_ERROR) {
      // an error occurred, but we simply go on because truncate should remove all documents
    }
  }

  primary->endWrite(collection->_collection);
  
  TRI_ReleaseCollection(collection);

  TRI_DestroyVectorPointer(&documents);

  return scope.Close(v8::Undefined());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief truncates a datafile
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_TruncateDatafileVocbaseCol (v8::Arguments const& argv) {
  v8::HandleScope scope;

  TRI_vocbase_col_t* collection = TRI_UnwrapClass<TRI_vocbase_col_t>(argv.Holder(), WRP_VOCBASE_COL_TYPE);

  if (collection == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("illegal collection pointer")));
  }

  if (argv.Length() != 2) {
    return scope.Close(v8::ThrowException(TRI_CreateErrorObject(TRI_ERROR_ILLEGAL_OPTION, "usage: truncateDatafile(<datafile>, <size>)")));
  }

  string path = TRI_ObjectToString(argv[0]);
  size_t size = TRI_ObjectToDouble(argv[1]);

  TRI_READ_LOCK_STATUS_VOCBASE_COL(collection);

  if (collection->_status != TRI_VOC_COL_STATUS_UNLOADED) {
    TRI_READ_UNLOCK_STATUS_VOCBASE_COL(collection);
    return scope.Close(v8::ThrowException(TRI_CreateErrorObject(TRI_ERROR_ARANGO_COLLECTION_NOT_UNLOADED)));
  }

  int res = TRI_TruncateDatafile(path.c_str(), size);

  TRI_READ_UNLOCK_STATUS_VOCBASE_COL(collection);

  if (res != TRI_ERROR_NO_ERROR) {
    return scope.Close(v8::ThrowException(TRI_CreateErrorObject(res, "cannot truncate datafile", true)));
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
    return scope.Close(v8::ThrowException(v8::String::New("illegal collection pointer")));
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
    return scope.Close(v8::ThrowException(v8::String::New("illegal collection pointer")));
  }

  int res = TRI_UnloadCollectionVocBase(collection->_vocbase, collection);

  if (res != TRI_ERROR_NO_ERROR) {
    return scope.Close(v8::ThrowException(TRI_CreateErrorObject(res, "cannot unload collection", true)));
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
    return scope.Close(v8::ThrowException(v8::String::New("illegal collection pointer")));
  }

  TRI_READ_LOCK_STATUS_VOCBASE_COL(collection);
  TRI_col_info_t info;
  int res = TRI_LoadParameterInfoCollection(collection->_path, &info);
  TRI_READ_UNLOCK_STATUS_VOCBASE_COL(collection);

  if (res != TRI_ERROR_NO_ERROR) {
    return scope.Close(v8::ThrowException(TRI_CreateErrorObject(res, "cannot fetch collection info", true)));
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

  TRI_vocbase_t* vocbase = TRI_UnwrapClass<TRI_vocbase_t>(info.Holder(), WRP_VOCBASE_TYPE);

  if (vocbase == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("corrupted vocbase")));
  }

  // convert the JavaScript string to a string
  string key = TRI_ObjectToString(name);

  if (key == "") {
    return scope.Close(v8::ThrowException(TRI_CreateErrorObject(TRI_ERROR_ARANGO_ILLEGAL_NAME, "name must not be empty")));
  }

  if (   key == "toString"
      || key == "toJSON"
      || key == "hasOwnProperty" // this prevents calling the property getter again (i.e. recursion!)
      || key[0] == '_') { // hide system collections
    return v8::Handle<v8::Value>();
  }

  // get the collection type (document/edge)
  const TRI_col_type_e collectionType = GetVocBaseCollectionType(info.Holder());

  // look up the value if it exists
  TRI_vocbase_col_t const* collection;
 
  if (collectionType == TRI_COL_TYPE_EDGE) {
    collection = TRI_FindEdgeCollectionByNameVocBase(vocbase, key.c_str(), true);
  }
  else {
    collection = TRI_FindDocumentCollectionByNameVocBase(vocbase, key.c_str(), true);
  }

  // if the key is not present return an empty handle as signal
  if (collection == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("cannot load or create collection")));
  }

  if (! TRI_IS_DOCUMENT_COLLECTION(collection->_type)) {
    return scope.Close(v8::ThrowException(v8::String::New("collection is not a document or edge collection")));
  }

  return scope.Close(TRI_WrapCollection(collection));
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
/// @FUN{db._collection(@FA{collection-identifier})}
///
/// Returns the collection with the given identifier or null if no such
/// collection exists.
///
/// @FUN{db._collection(@FA{collection-name})}
///
/// Returns the collection with the given name or null if no such collection
/// exists.
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
  return CollectionVocBase(argv);
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
    return scope.Close(v8::ThrowException(v8::String::New("corrupted vocbase")));
  }

  v8::Handle<v8::Array> result = v8::Array::New();
  TRI_vector_pointer_t colls = TRI_CollectionsVocBase(vocbase);

  uint32_t n = (uint32_t) colls._length;

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
  v8::Handle<v8::Array> result = v8::Array::New();

  TRI_vocbase_t* vocbase = TRI_UnwrapClass<TRI_vocbase_t>(argv.Holder(), WRP_VOCBASE_TYPE);

  if (vocbase == 0) {
    return scope.Close(result);
  }

  // get the collection type (document/edge)
  //const TRI_col_type_e collectionType = GetVocBaseCollectionType(argv.Holder());

  TRI_vector_pointer_t colls = TRI_CollectionsVocBase(vocbase);

  uint32_t n = (uint32_t) colls._length;
  for (uint32_t i = 0;  i < n;  ++i) {
    TRI_vocbase_col_t const* collection = (TRI_vocbase_col_t const*) colls._buffer[i];

    // only include collections of the right type, currently disabled
    // if (collectionType == collection->_type) {
    result->Set(i, v8::String::New(collection->_name));
    // }
  }

  TRI_DestroyVectorPointer(&colls);

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
/// @FA{properties} must be an object, with the following attribues:
///
/// - @LIT{waitForSync} (optional, default @LIT{false}): If @LIT{true} creating
///   a document will only return after the data was synced to disk.
///
/// - @LIT{journalSize} (optional, default is a @ref CommandLineArango
///   "configuration parameter"):  The maximal size of
///   a journal or datafile.  Note that this also limits the maximal
///   size of a single object. Must be at least 1MB.
///
/// - @LIT{isSystem} (optional, default is @LIT{false}): If true, create a
///   system collection. In this case @FA{collection-name} should start with
///   an underscore. End users should normally create non-system collections
///   only. API implementors may be required to create system collections in
///   very special occasions, but normally a regular collection will do.
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
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_CreateVocbase (v8::Arguments const& argv) {
  // get the collection type (document/edge)
  const TRI_col_type_e collectionType = GetVocBaseCollectionType(argv.Holder());

  return CreateVocBase(argv, collectionType);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new document collection
///
/// @FUN{db._createDocumentCollection(@FA{collection-name})}
///
/// @FUN{db._createDocumentCollection(@FA{collection-name}, @FA{properties})}
///
/// Creates a new document collection named @FA{collection-name}. 
/// This is an alias for @ref JS_CreateVocBase, with the difference that the 
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
/// @FA{properties} must be an object, with the following attribues:
///
/// - @LIT{waitForSync} (optional, default @LIT{false}): If @LIT{true} creating
///   a document will only return after the data was synced to disk.
///
/// - @LIT{journalSize} (optional, default is a @ref CommandLineArango
///   "configuration parameter"):  The maximal size of
///   a journal or datafile.  Note that this also limits the maximal
///   size of a single object. Must be at least 1MB.
///
/// @EXAMPLES
///
/// See @ref JS_CreateVocBase for examples.
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_CreateEdgeCollectionVocbase (v8::Arguments const& argv) {
  return CreateVocBase(argv, TRI_COL_TYPE_EDGE);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes a document
///
/// @FUN{@FA{db}._remove(@FA{document})}
///
/// Deletes a document. If there is revision mismatch, then an error is thrown.
///
/// @FUN{@FA{db}._remove(@FA{document}, true)}
///
/// Deletes a document. If there is revision mismatch, then mismatch
/// is ignored and document is deleted. The function returns
/// @LIT{true} if the document existed and was deleted. It returns
/// @LIT{false}, if the document was already deleted.
///
/// @FUN{@FA{db}._remove(@FA{document}, true, @FA{waitForSync})}
///
/// The optional @FA{waitForSync} parameter can be used to force 
/// synchronisation of the document deletion operation to disk even in case
/// that the @LIT{waitForSync} flag had been disabled for the entire collection.
/// Thus, the @FA{waitForSync} URL parameter can be used to force synchronisation
/// of just specific operations. To use this, set the @FA{waitForSync} parameter
/// to @LIT{true}. If the @FA{waitForSync} parameter is not specified or set to 
/// @LIT{false}, then the collection's default @LIT{waitForSync} behavior is 
/// applied. The @FA{waitForSync} URL parameter cannot be used to disable
/// synchronisation for collections that have a default @LIT{waitForSync} value
/// of @LIT{true}.
///
/// @FUN{@FA{db}._remove(@FA{document-handle}, @FA{data})}
///
/// As before. Instead of document a @FA{document-handle} can be passed as
/// first argument.
///
/// @EXAMPLES
///
/// Delete a document:
///
/// @TINYEXAMPLE{shell_remove-document-db,delete a document}
///
/// Delete a document with a conflict:
///
/// @TINYEXAMPLE{shell_remove-document-conflict-db,delete a document}
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_RemoveVocbase (v8::Arguments const& argv) {
  v8::HandleScope scope;

  TRI_vocbase_t* vocbase = TRI_UnwrapClass<TRI_vocbase_t>(argv.Holder(), WRP_VOCBASE_TYPE);

  if (vocbase == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("corrupted vocbase")));
  }

  return DeleteVocbaseCol(vocbase, 0, argv);
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
  v8::HandleScope scope;

  TRI_vocbase_t* vocbase = TRI_UnwrapClass<TRI_vocbase_t>(argv.Holder(), WRP_VOCBASE_TYPE);

  if (vocbase == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("corrupted vocbase")));
  }

  return DocumentVocbaseCol(vocbase, 0, argv, true);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a document
///
/// it is the caller's responsibility to acquire and release the required locks
/// the collection must also have the correct status already. don't use this
/// function if you're unsure about it!
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_DocumentNLVocbase (v8::Arguments const& argv) {
  v8::HandleScope scope;

  TRI_vocbase_t* vocbase = TRI_UnwrapClass<TRI_vocbase_t>(argv.Holder(), WRP_VOCBASE_TYPE);

  if (vocbase == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("corrupted vocbase")));
  }

  return DocumentVocbaseCol(vocbase, 0, argv, false);
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
/// Thus, the @FA{waitForSync} URL parameter can be used to force synchronisation
/// of just specific operations. To use this, set the @FA{waitForSync} parameter
/// to @LIT{true}. If the @FA{waitForSync} parameter is not specified or set to 
/// @LIT{false}, then the collection's default @LIT{waitForSync} behavior is 
/// applied. The @FA{waitForSync} URL parameter cannot be used to disable
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
  v8::HandleScope scope;

  TRI_vocbase_t* vocbase = TRI_UnwrapClass<TRI_vocbase_t>(argv.Holder(), WRP_VOCBASE_TYPE);

  if (vocbase == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("corrupted vocbase")));
  }

  return ReplaceVocbaseCol(vocbase, 0, argv);
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
/// Thus, the @FA{waitForSync} URL parameter can be used to force synchronisation
/// of just specific operations. To use this, set the @FA{waitForSync} parameter
/// to @LIT{true}. If the @FA{waitForSync} parameter is not specified or set to 
/// @LIT{false}, then the collection's default @LIT{waitForSync} behavior is 
/// applied. The @FA{waitForSync} URL parameter cannot be used to disable
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
  v8::HandleScope scope;

  TRI_vocbase_t* vocbase = TRI_UnwrapClass<TRI_vocbase_t>(argv.Holder(), WRP_VOCBASE_TYPE);

  if (vocbase == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("corrupted vocbase")));
  }

  return UpdateVocbaseCol(vocbase, 0, argv);
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

static void WeakBarrierCallback (v8::Persistent<v8::Value> object, void* parameter) {
  TRI_barrier_t* barrier;
  TRI_v8_global_t* v8g;

  v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();
  barrier = (TRI_barrier_t*) parameter;

  LOG_TRACE("weak-callback for barrier called");

  // find the persistent handle
  v8::Persistent<v8::Value> persistent = v8g->JSBarriers[barrier];
  v8g->JSBarriers.erase(barrier);

  // dispose and clear the persistent handle
  persistent.Dispose();
  persistent.Clear();

  // free the barrier
  TRI_FreeBarrier(barrier);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief selects an attribute from the shaped json
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> MapGetShapedJson (v8::Local<v8::String> name,
                                               const v8::AccessorInfo& info) {
  v8::HandleScope scope;

  // sanity check
  v8::Handle<v8::Object> self = info.Holder();

  if (self->InternalFieldCount() <= SLOT_BARRIER) {
    return scope.Close(v8::ThrowException(v8::String::New("corrupted shaped json")));
  }

  // get shaped json
  void* marker = TRI_UnwrapClass<void*>(self, WRP_SHAPED_JSON_TYPE);

  if (marker == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("corrupted shaped json")));
  }

  TRI_barrier_t* barrier = static_cast<TRI_barrier_t*>(v8::Handle<v8::External>::Cast(self->GetInternalField(SLOT_BARRIER))->Value());
  TRI_primary_collection_t* collection = barrier->_container->_collection;

  // convert the JavaScript string to a string
  string key = TRI_ObjectToString(name);

  if (key == "") {
    return scope.Close(v8::ThrowException(TRI_CreateErrorObject(TRI_ERROR_ARANGO_ILLEGAL_NAME, "name must not be empty")));
  }

  if (key[0] == '_') {
    return scope.Close(v8::Handle<v8::Value>());
  }

  // get shape accessor
  TRI_shaper_t* shaper = collection->_shaper;
  TRI_shape_pid_t pid = shaper->findAttributePathByName(shaper, key.c_str());

  // TRI_shape_sid_t sid;
  // TRI_EXTRACT_SHAPE_IDENTIFIER_MARKER(sid, marker);

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
    return scope.Close(v8::ThrowException(v8::String::New("cannot extract attribute")));
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
  char const* qtr = 0;
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

  result->Set(count++, v8g->DidKey);
  result->Set(count++, v8g->RevKey);
  result->Set(count++, v8g->KeyKey);

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

  if (key == "_id") {
    return scope.Close(v8::Handle<v8::Integer>(v8::Integer::New(v8::ReadOnly)));
  }

  if (key == "_rev") {
    return scope.Close(v8::Handle<v8::Integer>(v8::Integer::New(v8::ReadOnly)));
  }

  if (key == "_key") {
    return scope.Close(v8::Handle<v8::Integer>(v8::Integer::New(v8::ReadOnly)));
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
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                            MODULE
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief add a method to a prototype object
////////////////////////////////////////////////////////////////////////////////

void TRI_AddProtoMethodVocbase (v8::Handle<v8::Template> tpl, 
                                const char* const name, 
                                v8::Handle<v8::Value>(*func)(v8::Arguments const&), 
                                const bool isHidden) {
  if (isHidden) {
    // hidden method
    tpl->Set(TRI_V8_SYMBOL(name), v8::FunctionTemplate::New(func), v8::DontEnum);
  }
  else {
    // normal method
    tpl->Set(TRI_V8_SYMBOL(name), v8::FunctionTemplate::New(func));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add a method to an object
////////////////////////////////////////////////////////////////////////////////

void TRI_AddMethodVocbase (v8::Handle<v8::ObjectTemplate> tpl, 
                           const char* const name, 
                           v8::Handle<v8::Value>(*func)(v8::Arguments const&), 
                           const bool isHidden) {
  if (isHidden) {
    // hidden method
    tpl->Set(TRI_V8_SYMBOL(name), v8::FunctionTemplate::New(func), v8::DontEnum);
  }
  else {
    // normal method
    tpl->Set(TRI_V8_SYMBOL(name), v8::FunctionTemplate::New(func));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add a global function to the given context
////////////////////////////////////////////////////////////////////////////////
  
void TRI_AddGlobalFunctionVocbase (v8::Handle<v8::Context> context, 
                                   const char* const name, 
                                   v8::Handle<v8::Value>(*func)(v8::Arguments const&)) {
  // all global functions are read-only
  context->Global()->Set(TRI_V8_SYMBOL(name), v8::FunctionTemplate::New(func)->GetFunction(), v8::ReadOnly);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add a global function to the given context
////////////////////////////////////////////////////////////////////////////////

void TRI_AddGlobalFunctionVocbase (v8::Handle<v8::Context> context, 
                                   const char* const name, 
                                   v8::Handle<v8::Function> func) {
  // all global functions are read-only
  context->Global()->Set(TRI_V8_SYMBOL(name), func, v8::ReadOnly);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts the collection, but doesn't lock it
///
/// it is the caller's responsibility to acquire and release the required locks
/// the collection must also have the correct status already. don't use this
/// function if you're unsure about it!
////////////////////////////////////////////////////////////////////////////////

TRI_document_collection_t* TRI_ExtractSimpleCollection (v8::Arguments const& argv,
                                                        TRI_vocbase_col_t const*& collection,
                                                        v8::Handle<v8::Object>* err) {
  // extract the collection
  TRI_vocbase_col_t* col = TRI_UnwrapClass<TRI_vocbase_col_t>(argv.Holder(), WRP_VOCBASE_COL_TYPE);
  if (col == 0 || col->_collection == 0) {
    return 0;
  }

  collection = col;

  // handle various collection types
  TRI_primary_collection_t* primary = collection->_collection;

  if (! TRI_IS_DOCUMENT_COLLECTION(primary->base._type)) {
    *err = TRI_CreateErrorObject(TRI_ERROR_INTERNAL, "unknown collection type");
    return 0;
  }

  return (TRI_document_collection_t*) primary;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts and locks the collection
////////////////////////////////////////////////////////////////////////////////

TRI_document_collection_t* TRI_ExtractAndUseSimpleCollection (v8::Arguments const& argv,
                                                              TRI_vocbase_col_t const*& collection,
                                                              v8::Handle<v8::Object>* err) {
  // extract the collection
  collection = UseCollection(argv.Holder(), err);

  if (collection == 0) {
    return 0;
  }

  // handle various collection types
  TRI_primary_collection_t* primary = collection->_collection;

  if (! TRI_IS_DOCUMENT_COLLECTION(primary->base._type)) {
    TRI_ReleaseCollection(collection);
    *err = TRI_CreateErrorObject(TRI_ERROR_INTERNAL, "unknown collection type");
    return 0;
  }

  return (TRI_document_collection_t*) primary;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief releases a collection
////////////////////////////////////////////////////////////////////////////////

void TRI_ReleaseCollection (TRI_vocbase_col_t const* collection) {
  TRI_ReleaseCollectionVocBase(collection->_vocbase, const_cast<TRI_vocbase_col_t*>(collection));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parse document or document handle
///
/// @note this will lock (aka "use") the collection. You must release the
/// collection yourself.
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Value> TRI_ParseDocumentOrDocumentHandle (TRI_vocbase_t* vocbase,
                                                         TRI_vocbase_col_t const*& collection,
                                                         TRI_voc_key_t& key,
                                                         TRI_voc_rid_t& rid,
                                                         const bool lock,
                                                         v8::Handle<v8::Value> val) {
  v8::HandleScope scope;
  TRI_v8_global_t* v8g;

  v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();

  // reset the collection identifier and the revision
  TRI_voc_cid_t cid = 0;
  rid = 0;

  // extract the document identifier and revision from a string
  if (val->IsString() || val->IsStringObject()) {
    if (! IsDocumentHandle(val, cid, key)) {
      return scope.Close(TRI_CreateErrorObject(TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD,
                                           "<document-handle> must be a document-handle"));
    }
  }

  // extract the document identifier and revision from a string
  else if (val->IsObject()) {
    v8::Handle<v8::Object> obj = val->ToObject();
    v8::Handle<v8::Value> didVal = obj->Get(v8g->DidKey);

    if (! IsDocumentHandle(didVal, cid, key)) {
      return scope.Close(TRI_CreateErrorObject(TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD,
                                               "expecting a document-handle in _id"));
    }

    rid = TRI_ObjectToUInt64(obj->Get(v8g->RevKey));

    if (rid == 0) {
      return scope.Close(TRI_CreateErrorObject(TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD,
                                               "expecting a revision identifier in _rev"));
    }
  }

  // give up
  else {
    return scope.Close(TRI_CreateErrorObject(TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD,
                                             "<document-handle> must be a document-handle"));
  }

  // lookup the collection
  if (collection == 0) {
    TRI_vocbase_col_t* vc = TRI_LookupCollectionByIdVocBase(vocbase, cid);

    if (vc == 0) {
      return scope.Close(TRI_CreateErrorObject(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND,
                                               "collection of <document-handle> is unknown"));;
    }

    if (lock) {
      // use the collection
      int res = TRI_UseCollectionVocBase(vocbase, vc);

      if (res != TRI_ERROR_NO_ERROR) {
        return scope.Close(TRI_CreateErrorObject(res, "cannot use/load collection", true));
      }
    }

    collection = vc;

    if (collection->_collection == 0) {
      return scope.Close(TRI_CreateErrorObject(TRI_ERROR_INTERNAL, "cannot use/load collection"));
    }
  }

  // check cross collection requests
  if (cid != collection->_collection->base._cid) {
    if (cid == 0) {
      //return scope.Close(TRI_CreateErrorObject(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND,
      //                                         "collection of <document-handle> unknown"));
    }
    else {
      return scope.Close(TRI_CreateErrorObject(TRI_ERROR_ARANGO_CROSS_COLLECTION_REQUEST,
                                               "cannot execute cross collection query"));
    }
  }

  v8::Handle<v8::Value> empty;
  return scope.Close(empty);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a index identifier
////////////////////////////////////////////////////////////////////////////////

TRI_index_t* TRI_LookupIndexByHandle (TRI_vocbase_t* vocbase,
                                      TRI_vocbase_col_t const*& collection,
                                      v8::Handle<v8::Value> val,
                                      bool ignoreNotFound,
                                      v8::Handle<v8::Object>* err) {
  TRI_v8_global_t* v8g;

  v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();

  // reset the collection identifier and the revision
  TRI_voc_cid_t cid = 0;
  TRI_idx_iid_t iid = 0;

  // extract the document identifier and revision from a string
  if (val->IsString() || val->IsStringObject()) {
    if (! IsIndexHandle(val, cid, iid)) {
      *err = TRI_CreateErrorObject(TRI_ERROR_ARANGO_INDEX_HANDLE_BAD,
                                   "<index-handle> must be a index-handle");
      return 0;
    }
  }

  // extract the document identifier and revision from a string
  else if (val->IsObject()) {
    v8::Handle<v8::Object> obj = val->ToObject();
    v8::Handle<v8::Value> iidVal = obj->Get(v8g->IidKey);

    if (! IsIndexHandle(iidVal, cid, iid)) {
      *err = TRI_CreateErrorObject(TRI_ERROR_ARANGO_INDEX_HANDLE_BAD,
                                   "expecting a index-handle in id");
      return 0;
    }
  }

  // lookup the collection
  if (collection == 0) {
    TRI_vocbase_col_t* vc = TRI_LookupCollectionByIdVocBase(vocbase, cid);

    if (vc == 0) {
      *err = TRI_CreateErrorObject(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND,
                                   "collection of <index-handle> is unknown");
      return 0;
    }

    // use the collection
    int res = TRI_UseCollectionVocBase(vocbase, vc);

    if (res != TRI_ERROR_NO_ERROR) {
      *err = TRI_CreateErrorObject(res, "cannot use/load collection", true);
      return 0;
    }

    collection = vc;

    if (collection->_collection == 0) {
      *err = TRI_CreateErrorObject(TRI_ERROR_INTERNAL, "cannot use/load collection");
      return 0;
    }
  }

  // check cross collection requests
  if (cid != collection->_collection->base._cid) {
    if (cid == 0) {
      *err = TRI_CreateErrorObject(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND,
                                   "collection of <index-handle> unknown");
      return 0;
    }
    else {
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

    return 0;
  }

  return idx;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief wraps a TRI_vocbase_t
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Object> TRI_WrapVocBase (TRI_vocbase_t const* database, 
                                        TRI_col_type_e type) {
  TRI_v8_global_t* v8g;
  v8::HandleScope scope;

  v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();
  v8::Handle<v8::Object> result = WrapClass(v8g->VocbaseTempl,
                                            WRP_VOCBASE_TYPE,
                                            const_cast<TRI_vocbase_t*>(database));

  result->Set(TRI_V8_SYMBOL("_path"), v8::String::New(database->_path), v8::ReadOnly);
  result->Set(TRI_V8_SYMBOL("_type"), v8::Integer::New((int) type), v8::ReadOnly);

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief wraps a TRI_vocbase_col_t
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Object> TRI_WrapCollection (TRI_vocbase_col_t const* collection) {
  TRI_v8_global_t* v8g;
  v8::HandleScope scope;

  v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();
  v8::Handle<v8::Object> result = WrapClass(v8g->VocbaseColTempl,
                                            WRP_VOCBASE_COL_TYPE,
                                            const_cast<TRI_vocbase_col_t*>(collection));

  result->Set(v8g->DidKey, v8::Number::New(collection->_cid), v8::ReadOnly);

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief wraps a TRI_shaped_json_t
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Value> TRI_WrapShapedJson (TRI_vocbase_col_t const* collection,
                                          TRI_doc_mptr_t const* document,
                                          TRI_barrier_t* barrier) {
  TRI_v8_global_t* v8g;
  v8::HandleScope scope;

  v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();

  // create the new handle to return, and set its template type
  v8::Handle<v8::Object> result = v8g->ShapedJsonTempl->NewInstance();

  if (result.IsEmpty()) {
    // error 
    // TODO check for empty results
    return scope.Close(result);
  }  

  // point the 0 index Field to the c++ pointer for unwrapping later
  result->SetInternalField(SLOT_CLASS_TYPE, v8::Integer::New(WRP_SHAPED_JSON_TYPE));
  result->SetInternalField(SLOT_CLASS, v8::External::New(const_cast<void*>(document->_data)));

  map< void*, v8::Persistent<v8::Value> >::iterator i = v8g->JSBarriers.find(barrier);

  if (i == v8g->JSBarriers.end()) {
    v8::Persistent<v8::Value> persistent = v8::Persistent<v8::Value>::New(v8::External::New(barrier));
    result->SetInternalField(SLOT_BARRIER, persistent);

    v8g->JSBarriers[barrier] = persistent;

    persistent.MakeWeak(barrier, WeakBarrierCallback);
  }
  else {
    result->SetInternalField(SLOT_BARRIER, i->second);
  }

  // store the document reference
  TRI_voc_rid_t rid = document->_rid;

  result->Set(v8g->DidKey, TRI_ObjectReference(collection->_collection->base._cid, document->_key), v8::ReadOnly);
  result->Set(v8g->RevKey, v8::Number::New(rid), v8::ReadOnly);
  result->Set(v8g->KeyKey, v8::String::New(document->_key), v8::ReadOnly);

  TRI_df_marker_type_t type = ((TRI_df_marker_t*) document->_data)->_type;
  
  if (type == TRI_DOC_MARKER_KEY_EDGE) {
    TRI_doc_edge_key_marker_t* marker = (TRI_doc_edge_key_marker_t*) document->_data;

    if (marker->_isBidirectional) {
      // bdirectional edge
      result->Set(v8g->BidirectionalKey, v8::True());
      
      // create _vertices array
      v8::Handle<v8::Array> vertices = v8::Array::New();
      vertices->Set(0, TRI_ObjectReference(marker->_fromCid, ((char*) marker) + marker->_offsetFromKey));
      vertices->Set(1, TRI_ObjectReference(marker->_toCid, ((char*) marker) + marker->_offsetToKey));
      result->Set(v8g->VerticesKey, vertices);
    }
    else {
      // unidirectional edge
      result->Set(v8g->BidirectionalKey, v8::False());

      result->Set(v8g->FromKey, TRI_ObjectReference(marker->_fromCid, ((char*) marker) + marker->_offsetFromKey));
      result->Set(v8g->ToKey, TRI_ObjectReference(marker->_toCid, ((char*) marker) + marker->_offsetToKey));
    }
  }
  
  // and return
  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a TRI_vocbase_t global context
////////////////////////////////////////////////////////////////////////////////

TRI_v8_global_t* TRI_InitV8VocBridge (v8::Handle<v8::Context> context, TRI_vocbase_t* vocbase) {
  v8::HandleScope scope;

  v8::Handle<v8::ObjectTemplate> rt;
  v8::Handle<v8::FunctionTemplate> ft;
  v8::Handle<v8::Template> pt;

  // check the isolate
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  TRI_v8_global_t* v8g = (TRI_v8_global_t*) isolate->GetData();

  if (v8g == 0) {
    v8g = new TRI_v8_global_t;
    isolate->SetData(v8g);
  }

  // create the regular expressions
  string expr = "([0-9][0-9]*)" + string(TRI_DOCUMENT_HANDLE_SEPARATOR_STR) + "([0-9a-zA-Z][0-9a-zA-Z]*)";

  if (regcomp(&v8g->DocumentIdRegex, expr.c_str(), REG_ICASE | REG_EXTENDED) != 0) {
    LOG_FATAL("cannot compile regular expression");
    TRI_FlushLogging();
    exit(EXIT_FAILURE);
  }

  if (regcomp(&v8g->IndexIdRegex, expr.c_str(), REG_ICASE | REG_EXTENDED) != 0) {
    LOG_FATAL("cannot compile regular expression");
    TRI_FlushLogging();
    exit(EXIT_FAILURE);
  }

  expr = "^[0-9a-zA-Z][_0-9a-zA-Z]*$";
  if (regcomp(&v8g->DocumentKeyRegex, expr.c_str(), REG_ICASE | REG_EXTENDED) != 0) {
    LOG_FATAL("cannot compile regular expression");
    TRI_FlushLogging();
    exit(EXIT_FAILURE);
  }
  

  // .............................................................................
  // keys
  // .............................................................................

  v8g->JournalSizeKey = v8::Persistent<v8::String>::New(v8::String::New("journalSize"));
  v8g->WaitForSyncKey = v8::Persistent<v8::String>::New(v8::String::New("waitForSync"));
  
  if (v8g->BidirectionalKey.IsEmpty()) {
    v8g->BidirectionalKey = v8::Persistent<v8::String>::New(TRI_V8_SYMBOL("_bidirectional"));
  }

  if (v8g->DidKey.IsEmpty()) {
    v8g->DidKey = v8::Persistent<v8::String>::New(TRI_V8_SYMBOL("_id"));
  }
  
  if (v8g->KeyKey.IsEmpty()) {
    v8g->KeyKey = v8::Persistent<v8::String>::New(TRI_V8_SYMBOL("_key"));
  }

  if (v8g->FromKey.IsEmpty()) {
    v8g->FromKey = v8::Persistent<v8::String>::New(TRI_V8_SYMBOL("_from"));
  }

  if (v8g->IidKey.IsEmpty()) {
    v8g->IidKey = v8::Persistent<v8::String>::New(TRI_V8_SYMBOL("id"));
  }
  
  if (v8g->OldRevKey.IsEmpty()) {
    v8g->OldRevKey = v8::Persistent<v8::String>::New(TRI_V8_SYMBOL("_oldRev"));
  }

  if (v8g->RevKey.IsEmpty()) {
    v8g->RevKey = v8::Persistent<v8::String>::New(TRI_V8_SYMBOL("_rev"));
  }

  if (v8g->ToKey.IsEmpty()) {
    v8g->ToKey = v8::Persistent<v8::String>::New(TRI_V8_SYMBOL("_to"));
  }
  
  if (v8g->VerticesKey.IsEmpty()) {
    v8g->VerticesKey = v8::Persistent<v8::String>::New(TRI_V8_SYMBOL("_vertices"));
  }
  
  // .............................................................................
  // generate the TRI_vocbase_t template
  // .............................................................................

  ft = v8::FunctionTemplate::New();
  ft->SetClassName(TRI_V8_SYMBOL("ArangoDatabase"));

  rt = ft->InstanceTemplate();
  rt->SetInternalFieldCount(2);
  rt->SetNamedPropertyHandler(MapGetVocBase);

  TRI_AddMethodVocbase(rt, "_collection", JS_CollectionVocbase);
  TRI_AddMethodVocbase(rt, "_collections", JS_CollectionsVocbase);
  TRI_AddMethodVocbase(rt, "_COMPLETIONS", JS_CompletionsVocbase, true);
  TRI_AddMethodVocbase(rt, "_create", JS_CreateVocbase, true);
  TRI_AddMethodVocbase(rt, "_createDocumentCollection", JS_CreateDocumentCollectionVocbase);
  TRI_AddMethodVocbase(rt, "_createEdgeCollection", JS_CreateEdgeCollectionVocbase);
  TRI_AddMethodVocbase(rt, "_document", JS_DocumentVocbase);
  TRI_AddMethodVocbase(rt, "_document_nl", JS_DocumentNLVocbase, true);
  TRI_AddMethodVocbase(rt, "_remove", JS_RemoveVocbase);
  TRI_AddMethodVocbase(rt, "_replace", JS_ReplaceVocbase);
  TRI_AddMethodVocbase(rt, "_update", JS_UpdateVocbase);

  v8g->VocbaseTempl = v8::Persistent<v8::ObjectTemplate>::New(rt);
  TRI_AddGlobalFunctionVocbase(context, "ArangoDatabase", ft->GetFunction());


  // .............................................................................
  // generate the TRI_shaped_json_t template
  // .............................................................................

  ft = v8::FunctionTemplate::New();
  ft->SetClassName(TRI_V8_SYMBOL("ShapedJson"));

  rt = ft->InstanceTemplate();
  rt->SetInternalFieldCount(3);

  rt->SetNamedPropertyHandler(MapGetShapedJson,         // NamedPropertyGetter,
                              0,                        // NamedPropertySetter setter = 0
                              PropertyQueryShapedJson,  // NamedPropertyQuery,
                              0,                        // NamedPropertyDeleter deleter = 0,
                              KeysOfShapedJson          // NamedPropertyEnumerator,
                                                        // Handle<Value> data = Handle<Value>());
                              );

  v8g->ShapedJsonTempl = v8::Persistent<v8::ObjectTemplate>::New(rt);
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
  TRI_AddMethodVocbase(rt, "document_nl", JS_DocumentNLVocbaseCol, true);
  TRI_AddMethodVocbase(rt, "drop", JS_DropVocbaseCol);
  TRI_AddMethodVocbase(rt, "dropIndex", JS_DropIndexVocbaseCol);
  TRI_AddMethodVocbase(rt, "ensureBitarray", JS_EnsureBitarrayVocbaseCol);
  TRI_AddMethodVocbase(rt, "ensureUndefBitarray", JS_EnsureUndefBitarrayVocbaseCol);
  TRI_AddMethodVocbase(rt, "ensureCapConstraint", JS_EnsureCapConstraintVocbaseCol);
  TRI_AddMethodVocbase(rt, "ensureGeoConstraint", JS_EnsureGeoConstraintVocbaseCol);
  TRI_AddMethodVocbase(rt, "ensureGeoIndex", JS_EnsureGeoIndexVocbaseCol);
  TRI_AddMethodVocbase(rt, "ensureHashIndex", JS_EnsureHashIndexVocbaseCol);
  TRI_AddMethodVocbase(rt, "ensurePQIndex", JS_EnsurePriorityQueueIndexVocbaseCol);
  TRI_AddMethodVocbase(rt, "ensureSkiplist", JS_EnsureSkiplistVocbaseCol);
  TRI_AddMethodVocbase(rt, "ensureUniqueConstraint", JS_EnsureUniqueConstraintVocbaseCol);
  TRI_AddMethodVocbase(rt, "ensureUniqueSkiplist", JS_EnsureUniqueSkiplistVocbaseCol);
  TRI_AddMethodVocbase(rt, "figures", JS_FiguresVocbaseCol);
  TRI_AddMethodVocbase(rt, "getIndexes", JS_GetIndexesVocbaseCol);
  TRI_AddMethodVocbase(rt, "getIndexesNL", JS_GetIndexesNLVocbaseCol, true);
  TRI_AddMethodVocbase(rt, "load", JS_LoadVocbaseCol);
  TRI_AddMethodVocbase(rt, "lookupHashIndex", JS_LookupHashIndexVocbaseCol);
  TRI_AddMethodVocbase(rt, "lookupSkiplist", JS_LookupSkiplistVocbaseCol);
  TRI_AddMethodVocbase(rt, "lookupUniqueConstraint", JS_LookupUniqueConstraintVocbaseCol);
  TRI_AddMethodVocbase(rt, "lookupUniqueSkiplist", JS_LookupUniqueSkiplistVocbaseCol);
  TRI_AddMethodVocbase(rt, "name", JS_NameVocbaseCol);
  TRI_AddMethodVocbase(rt, "properties", JS_PropertiesVocbaseCol);
  TRI_AddMethodVocbase(rt, "remove", JS_RemoveVocbaseCol);
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
  TRI_AddMethodVocbase(rt, "update", JS_UpdateVocbaseCol);

  v8g->VocbaseColTempl = v8::Persistent<v8::ObjectTemplate>::New(rt);
  TRI_AddGlobalFunctionVocbase(context, "ArangoCollection", ft->GetFunction());


  // .............................................................................
  // generate the general error template
  // .............................................................................

  ft = v8::FunctionTemplate::New();
  ft->SetClassName(TRI_V8_SYMBOL("ArangoError"));

  rt = ft->InstanceTemplate();
  v8g->ErrorTempl = v8::Persistent<v8::ObjectTemplate>::New(rt);
  TRI_AddGlobalFunctionVocbase(context, "ArangoError", ft->GetFunction());
  

  // .............................................................................
  // generate the general cursor template
  // .............................................................................

  ft = v8::FunctionTemplate::New();
  ft->SetClassName(TRI_V8_SYMBOL("ArangoCursor"));

  pt = ft->PrototypeTemplate();
  TRI_AddProtoMethodVocbase(pt, "count", JS_CountGeneralCursor);
  TRI_AddProtoMethodVocbase(pt, "dispose", JS_DisposeGeneralCursor);
  TRI_AddProtoMethodVocbase(pt, "getBatchSize", JS_GetBatchSizeGeneralCursor);
  TRI_AddProtoMethodVocbase(pt, "getRows", JS_GetRowsGeneralCursor);
  TRI_AddProtoMethodVocbase(pt, "hasCount", JS_HasCountGeneralCursor);
  TRI_AddProtoMethodVocbase(pt, "hasNext", JS_HasNextGeneralCursor);
  TRI_AddProtoMethodVocbase(pt, "id", JS_IdGeneralCursor);
  TRI_AddProtoMethodVocbase(pt, "next", JS_NextGeneralCursor);
  TRI_AddProtoMethodVocbase(pt, "persist", JS_PersistGeneralCursor);
  TRI_AddProtoMethodVocbase(pt, "unuse", JS_UnuseGeneralCursor);
  
  rt = ft->InstanceTemplate();
  rt->SetInternalFieldCount(2);
  v8g->GeneralCursorTempl = v8::Persistent<v8::ObjectTemplate>::New(rt);
  TRI_AddGlobalFunctionVocbase(context, "ArangoCursor", ft->GetFunction());
  
  
#ifdef TRI_ENABLE_TRX
  // .............................................................................
  // generate the transaction template
  // .............................................................................

  ft = v8::FunctionTemplate::New();
  ft->SetClassName(TRI_V8_SYMBOL("ArangoTransaction"));

  pt = ft->PrototypeTemplate();
  TRI_AddProtoMethodVocbase(pt, "abort", JS_AbortTransaction);
  TRI_AddProtoMethodVocbase(pt, "addCollection", JS_AddCollectionTransaction);
  TRI_AddProtoMethodVocbase(pt, "begin", JS_BeginTransaction);
  TRI_AddProtoMethodVocbase(pt, "commit", JS_CommitTransaction);
  TRI_AddProtoMethodVocbase(pt, "dump", JS_DumpTransaction);
  TRI_AddProtoMethodVocbase(pt, "status", JS_StatusTransaction);
  
  rt = ft->InstanceTemplate();
  rt->SetInternalFieldCount(2);
  v8g->TransactionTempl = v8::Persistent<v8::ObjectTemplate>::New(rt);
  TRI_AddGlobalFunctionVocbase(context, "ArangoTransaction", JS_CreateTransaction);
#endif


  // .............................................................................
  // generate global functions
  // .............................................................................

  TRI_AddGlobalFunctionVocbase(context, "CURSOR", JS_Cursor);
  TRI_AddGlobalFunctionVocbase(context, "CREATE_CURSOR", JS_CreateCursor);
  TRI_AddGlobalFunctionVocbase(context, "DELETE_CURSOR", JS_DeleteCursor);

  TRI_AddGlobalFunctionVocbase(context, "AHUACATL_RUN", JS_RunAhuacatl);
  TRI_AddGlobalFunctionVocbase(context, "AHUACATL_EXPLAIN", JS_ExplainAhuacatl);
  TRI_AddGlobalFunctionVocbase(context, "AHUACATL_PARSE", JS_ParseAhuacatl);
  
  TRI_AddGlobalFunctionVocbase(context, "COMPARE_STRING", JS_compare_string);
  TRI_AddGlobalFunctionVocbase(context, "NORMALIZE_STRING", JS_normalize_string);
 
  
  // .............................................................................
  // create global variables
  // .............................................................................
  
  context->Global()->Set(TRI_V8_SYMBOL("HAS_ICU"),
#ifdef TRI_ICU_VERSION
                         v8::Boolean::New(true),
#else
                         v8::Boolean::New(false),
#endif
                         v8::ReadOnly);
  

  context->Global()->Set(TRI_V8_SYMBOL("db"),
                         TRI_WrapVocBase(vocbase, TRI_COL_TYPE_DOCUMENT),
                         v8::ReadOnly);

  // DEPRECATED: only here for compatibility
  context->Global()->Set(TRI_V8_SYMBOL("edges"),
                         TRI_WrapVocBase(vocbase, TRI_COL_TYPE_EDGE),
                         v8::ReadOnly);

  return v8g;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}\\)"
// End:
