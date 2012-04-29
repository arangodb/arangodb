////////////////////////////////////////////////////////////////////////////////
/// @brief V8-vocbase bridge
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2012 triagens GmbH, Cologne, Germany
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

#include "Basics/StringUtils.h"
#include "BasicsC/conversions.h"
#include "BasicsC/csv.h"
#include "BasicsC/json.h"
#include "BasicsC/logging.h"
#include "BasicsC/strings.h"
#include "ShapedJson/shape-accessor.h"
#include "ShapedJson/shaped-json.h"
#include "V8/v8-conv.h"
#include "V8/v8-utils.h"
#include "VocBase/query.h"
#include "VocBase/simple-collection.h"
#include "VocBase/general-cursor.h"
#include "SkipLists/sl-operator.h"
#include "Ahuacatl/ast-codegen-js.h"
#include "Ahuacatl/ahuacatl-ast-node.h"
#include "Ahuacatl/ahuacatl-context.h"
#include "Ahuacatl/ahuacatl-result.h"

using namespace std;
using namespace triagens::basics;

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
/// @brief end marker
////////////////////////////////////////////////////////////////////////////////

static int const SLOT_END = 4;

////////////////////////////////////////////////////////////////////////////////
/// @brief wrapped class for TRI_vocbase_t
////////////////////////////////////////////////////////////////////////////////

static int32_t const WRP_VOCBASE_TYPE = 1;

////////////////////////////////////////////////////////////////////////////////
/// @brief wrapped class for TRI_vocbase_col_t
////////////////////////////////////////////////////////////////////////////////

static int32_t const WRP_VOCBASE_COL_TYPE = 2;

////////////////////////////////////////////////////////////////////////////////
/// @brief wrapped class for general cursors
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
/// @brief wrapped class for TRI_qry_where_t - DEPRECATED
////////////////////////////////////////////////////////////////////////////////

static int32_t const WRP_QRY_WHERE_TYPE = 5;

////////////////////////////////////////////////////////////////////////////////
/// @brief wrapped class for TRI_rc_cursor_t - DEPRECATED
////////////////////////////////////////////////////////////////////////////////

static int32_t const WRP_RC_CURSOR_TYPE = 6;

////////////////////////////////////////////////////////////////////////////////
/// @brief wrapped class for TRI_query_t - DEPRECATED
////////////////////////////////////////////////////////////////////////////////

static int32_t const WRP_QUERY_TYPE = 7;

////////////////////////////////////////////////////////////////////////////////
/// @brief wrapped class for SL Operator - DEPRECATED
////////////////////////////////////////////////////////////////////////////////

static int32_t const WRP_SL_OPERATOR_TYPE = 8;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  HELPER FUNCTIONS
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                     private types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

typedef struct {
  double _distance;
  void const* _data;
}
geo_coordinate_distance_t;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief wraps a C++ into a v8::Object
////////////////////////////////////////////////////////////////////////////////

template<class T>
static v8::Handle<v8::Object> WrapClass (v8::Persistent<v8::ObjectTemplate> classTempl, int32_t type, T* y) {

  // handle scope for temporary handles
  v8::HandleScope scope;

  // create the new handle to return, and set its template type
  v8::Handle<v8::Object> result = classTempl->NewInstance();

  // set the c++ pointer for unwrapping later
  result->SetInternalField(SLOT_CLASS_TYPE, v8::Integer::New(type));
  result->SetInternalField(SLOT_CLASS, v8::External::New(y));

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the vocbase pointer from the current V8 context
////////////////////////////////////////////////////////////////////////////////
  
static TRI_vocbase_t* GetContextVocBase () {
  v8::Handle<v8::Context> currentContext = v8::Context::GetCurrent(); 
  v8::Handle<v8::Object> db = currentContext->Global()->Get(v8::String::New("db"))->ToObject();

  return TRI_UnwrapClass<TRI_vocbase_t>(db, WRP_VOCBASE_TYPE);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if argument is a document identifier
////////////////////////////////////////////////////////////////////////////////

static bool IsDocumentHandle (v8::Handle<v8::Value> arg, TRI_voc_cid_t& cid, TRI_voc_did_t& did) {
  TRI_v8_global_t* v8g;

  v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();

  if (arg->IsNumber()) {
    did = (TRI_voc_did_t) arg->ToNumber()->Value();
    return true;
  }

  if (! arg->IsString()) {
    return false;
  }

  v8::String::Utf8Value str(arg);
  char const* s = *str;

  if (s == 0) {
    return false;
  }

  regmatch_t matches[3];

  if (regexec(&v8g->DocumentIdRegex, s, sizeof(matches) / sizeof(matches[0]), matches, 0) == 0) {
    cid = TRI_UInt64String2(s + matches[1].rm_so, matches[1].rm_eo - matches[1].rm_so);
    did = TRI_UInt64String2(s + matches[2].rm_so, matches[2].rm_eo - matches[2].rm_so);
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

  v8::String::Utf8Value str(arg);
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
/// @brief creates an error in a javascript object
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Object> CreateErrorObject (int errorNumber, string const& message) {
  TRI_v8_global_t* v8g;
  v8::HandleScope scope;

  v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();

  string msg = TRI_errno_string(errorNumber) + string(": ") + message;
  v8::Handle<v8::String> errorMessage = v8::String::New(msg.c_str());

  v8::Handle<v8::Object> errorObject = v8::Exception::Error(errorMessage)->ToObject();
  v8::Handle<v8::Value> proto = v8g->ErrorTempl->NewInstance();

  errorObject->Set(v8::String::New("errorNum"), v8::Number::New(errorNumber));
  errorObject->Set(v8::String::New("errorMessage"), errorMessage);
  errorObject->SetPrototype(proto);

  return errorObject;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief loads a collection for usage
////////////////////////////////////////////////////////////////////////////////

static TRI_vocbase_col_t const* UseCollection (v8::Handle<v8::Object> collection,
                                               v8::Handle<v8::Object>* err) {
                                                
  TRI_vocbase_col_t* col = TRI_UnwrapClass<TRI_vocbase_col_t>(collection, WRP_VOCBASE_COL_TYPE);

  int res = TRI_UseCollectionVocBase(col->_vocbase, col);

  if (res != TRI_ERROR_NO_ERROR) {
    *err = CreateErrorObject(res, "cannot use/load collection");
    return 0;
  }

  if (col->_collection == 0) {
    TRI_set_errno(TRI_ERROR_INTERNAL);
    *err = CreateErrorObject(TRI_ERROR_INTERNAL, "cannot use/load collection");
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
/// @brief sorts geo coordinates
////////////////////////////////////////////////////////////////////////////////

static int CompareGeoCoordinateDistance (geo_coordinate_distance_t* left, geo_coordinate_distance_t* right) {
  if (left->_distance < right->_distance) {
    return -1;
  }
  else if (left->_distance > right->_distance) {
    return 1;
  }
  else {
    return 0;
  }
}

#define FSRT_NAME SortGeoCoordinates
#define FSRT_NAM2 SortGeoCoordinatesTmp
#define FSRT_TYPE geo_coordinate_distance_t

#define FSRT_COMP(l,r,s) CompareGeoCoordinateDistance(l,r)

uint32_t FSRT_Rand = 0;

static uint32_t RandomGeoCoordinateDistance (void) {
  return (FSRT_Rand = FSRT_Rand * 31415 + 27818);
}

#define FSRT__RAND \
  ((fs_b) + FSRT__UNIT * (RandomGeoCoordinateDistance() % FSRT__DIST(fs_e,fs_b,FSRT__SIZE)))

#include <BasicsC/fsrt.inc>

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a geo result
////////////////////////////////////////////////////////////////////////////////

static void StoreGeoResult (TRI_vocbase_col_t const* collection,
                            GeoCoordinates* cors,
                            v8::Handle<v8::Array>& documents,
                            v8::Handle<v8::Array>& distances) {
  GeoCoordinate* end;
  GeoCoordinate* ptr;
  double* dtr;
  geo_coordinate_distance_t* gnd;
  geo_coordinate_distance_t* gtr;
  geo_coordinate_distance_t* tmp;
  size_t n;
  uint32_t i;
  TRI_barrier_t* barrier;

  // sort the result
  n = cors->length;

  if (n == 0) {
    return;
  }

  gtr = (tmp = (geo_coordinate_distance_t*) TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(geo_coordinate_distance_t) * n, false));
  gnd = tmp + n;

  ptr = cors->coordinates;
  end = cors->coordinates + n;

  dtr = cors->distances;

  for (;  ptr < end;  ++ptr, ++dtr, ++gtr) {
    gtr->_distance = *dtr;
    gtr->_data = ptr->data;
  }

  // GeoIndex_CoordinatesFree(cors);

  SortGeoCoordinates(tmp, gnd);

  barrier = TRI_CreateBarrierElement(&((TRI_doc_collection_t*) collection->_collection)->_barrierList);

  // copy the documents
  for (gtr = tmp, i = 0;  gtr < gnd;  ++gtr, ++i) {
    documents->Set(i, TRI_WrapShapedJson(collection, (TRI_doc_mptr_t const*) gtr->_data, barrier));
    distances->Set(i, v8::Number::New(gtr->_distance));
  }

  TRI_Free(TRI_UNKNOWN_MEM_ZONE, tmp);
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
/// @brief ensure a hash or skip-list index
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> EnsureHashSkipListIndex (string const& cmd,
                                                      v8::Arguments const& argv,
                                                      bool unique,
                                                      int type) {
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

  TRI_doc_collection_t* doc = collection->_collection;
  
  if (doc->base._type != TRI_COL_TYPE_SIMPLE_DOCUMENT) {
    ReleaseCollection(collection);
    return scope.Close(v8::ThrowException(CreateErrorObject(TRI_ERROR_INTERNAL, "unknown collection type")));
  }

  TRI_sim_collection_t* sim = (TRI_sim_collection_t*) doc;
  
  // .............................................................................
  // Ensure that there is at least one string parameter sent to this method
  // .............................................................................  

  if (argv.Length() == 0) {
    ReleaseCollection(collection);
    return scope.Close(v8::ThrowException(CreateErrorObject(TRI_ERROR_ILLEGAL_OPTION, "usage: " + cmd + "(<path>, ...))")));
  }
  
  // .............................................................................
  // Return string when there is an error of some sort.
  // .............................................................................

  int res = TRI_ERROR_NO_ERROR;
  string errorString;
  
  // .............................................................................
  // Create a list of paths, these will be used to create a list of shapes
  // which will be used by the hash index.
  // .............................................................................
  
  TRI_vector_t attributes;
  TRI_InitVector(&attributes, TRI_UNKNOWN_MEM_ZONE, sizeof(char*));
  
  for (int j = 0; j < argv.Length(); ++j) {
    v8::Handle<v8::Value> argument = argv[j];

    if (! argument->IsString() ) {
      res = TRI_ERROR_ILLEGAL_OPTION;
      errorString = "invalid parameter passed to " + cmd + "(...)";
      break;
    }
    
    // ...........................................................................
    // convert the argument into a "C" string
    // ...........................................................................
    
    v8::String::Utf8Value argumentString(argument);   
    char* cArgument = *argumentString == 0 ? 0 : TRI_DuplicateString(*argumentString);

    TRI_PushBackVector(&attributes, &cArgument);
  }
  
  // .............................................................................
  // Check that each parameter is unique
  // .............................................................................
  
  for (size_t j = 0; j < attributes._length; ++j) {  
    char* left = *((char**) (TRI_AtVector(&attributes, j)));    

    for (size_t k = j + 1; k < attributes._length; ++k) {
      char* right = *((char**) (TRI_AtVector(&attributes, k)));

      if (TRI_EqualString(left, right)) {
        res = TRI_ERROR_ILLEGAL_OPTION;
        errorString = "duplicate parameters sent to " + cmd + "(...)";
        break;
      }   
    }
  }    
  
  // .............................................................................
  // Some sort of error occurred -- display error message and abort index creation
  // (or index retrieval).
  // .............................................................................
  
  if (res != TRI_ERROR_NO_ERROR) {

    // Remove the memory allocated to the list of attributes used for the hash index
    for (size_t j = 0;  j < attributes._length;  ++j) {
      char* cArgument = *((char**) (TRI_AtVector(&attributes, j)));
      TRI_FreeString(TRI_UNKNOWN_MEM_ZONE, cArgument);
    }    

    TRI_DestroyVector(&attributes);

    ReleaseCollection(collection);
    return scope.Close(v8::ThrowException(CreateErrorObject(res, errorString)));
  }  
  
  // .............................................................................
  // Actually create the index here
  // .............................................................................

  bool created;
  TRI_index_t* idx;

  if (type == 0) {
    idx = TRI_EnsureHashIndexSimCollection(sim, &attributes, unique, &created);

    if (idx == 0) {
      res = TRI_errno();
    }
  }
  else if (type == 1) {
    idx = TRI_EnsureSkiplistIndexSimCollection(sim, &attributes, unique, &created);

    if (idx == 0) {
      res = TRI_errno();
    }
  }
  else {
    LOG_ERROR("unknown index type %d", type);
    res = TRI_ERROR_INTERNAL;
    idx = 0;
  }

  // .............................................................................
  // Remove the memory allocated to the list of attributes used for the hash index
  // .............................................................................   

  for (size_t j = 0; j < attributes._length; ++j) {
    char* cArgument = *((char**) (TRI_AtVector(&attributes, j)));
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, cArgument);
  }    

  TRI_DestroyVector(&attributes);

  if (idx == 0) {
    ReleaseCollection(collection);
    return scope.Close(v8::ThrowException(CreateErrorObject(res, "index could not be created")));
  }  
  
  // .............................................................................
  // Return the newly assigned index identifier
  // .............................................................................

  TRI_json_t* json = idx->json(idx, collection->_collection);

  if (!json) {
    ReleaseCollection(collection);
    return scope.Close(v8::ThrowException(v8::String::New("out of memory")));
  }

  v8::Handle<v8::Value> index = IndexRep(&collection->_collection->base, json);
  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
  
  if (index->IsObject()) {
    index->ToObject()->Set(v8::String::New("isNewlyCreated"), created ? v8::True() : v8::False());
  }

  ReleaseCollection(collection);
  return scope.Close(index);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parse document or document handle
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> ParseDocumentOrDocumentHandle (TRI_vocbase_t* vocbase,
                                                            TRI_vocbase_col_t const*& collection,
                                                            TRI_voc_did_t& did,
                                                            TRI_voc_rid_t& rid,
                                                            v8::Handle<v8::Value> val) {
  v8::HandleScope scope;
  TRI_v8_global_t* v8g;

  v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();

  // reset the collection identifier and the revision
  TRI_voc_cid_t cid = 0;
  rid = 0;

  // extract the document identifier and revision from a string
  if (val->IsString() || val->IsStringObject()) {
    if (! IsDocumentHandle(val, cid, did)) {
      return scope.Close(CreateErrorObject(TRI_ERROR_AVOCADO_DOCUMENT_HANDLE_BAD, 
                                           "<document-handle> must be a document-handle"));
    }
  }

  // extract the document identifier and revision from a string
  else if (val->IsObject()) {
    v8::Handle<v8::Object> obj = val->ToObject();
    v8::Handle<v8::Value> didVal = obj->Get(v8g->DidKey);

    if (! IsDocumentHandle(didVal, cid, did)) {
      return scope.Close(CreateErrorObject(TRI_ERROR_AVOCADO_DOCUMENT_HANDLE_BAD,
                                           "expecting a document-handle in _id"));
    }

    rid = TRI_ObjectToUInt64(obj->Get(v8g->RevKey));

    if (rid == 0) {
      return scope.Close(CreateErrorObject(TRI_ERROR_AVOCADO_DOCUMENT_HANDLE_BAD,
                                           "expecting a revision identifier in _rev"));
    }
  }

  // lockup the collection
  if (collection == 0) {
    TRI_vocbase_col_t* vc = TRI_LookupCollectionByIdVocBase(vocbase, cid);

    if (vc == 0) {
      return scope.Close(CreateErrorObject(TRI_ERROR_AVOCADO_COLLECTION_NOT_FOUND,
                                           "collection of <document-handle> is unknown"));;
    }

    // use the collection
    int res = TRI_UseCollectionVocBase(vocbase, vc);

    if (res != TRI_ERROR_NO_ERROR) {
      return scope.Close(CreateErrorObject(res, "cannot use/load collection"));;
    }

    collection = vc;

    if (collection->_collection == 0) {
      return scope.Close(CreateErrorObject(TRI_ERROR_INTERNAL, "cannot use/load collection"));
    }
  }

  // check cross collection requests
  if (cid != collection->_collection->base._cid) {
    if (cid == 0) {
      return scope.Close(CreateErrorObject(TRI_ERROR_AVOCADO_COLLECTION_NOT_FOUND, 
                                           "collection of <document-handle> unknown"));
    }
    else {
      return scope.Close(CreateErrorObject(TRI_ERROR_AVOCADO_CROSS_COLLECTION_REQUEST,
                                           "cannot execute cross collection query"));
    }
  }

  v8::Handle<v8::Value> empty;
  return scope.Close(empty);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convert v8 arguments to json_t values
////////////////////////////////////////////////////////////////////////////////

static TRI_json_t* ConvertHelper(v8::Handle<v8::Value> parameter) {
  if (parameter->IsBoolean()) {
    v8::Handle<v8::Boolean> booleanParameter = parameter->ToBoolean();
    return TRI_CreateBooleanJson(TRI_UNKNOWN_MEM_ZONE, booleanParameter->Value());
  }

  if (parameter->IsNull()) {
    return TRI_CreateNullJson(TRI_UNKNOWN_MEM_ZONE);
  }
  
  if (parameter->IsNumber()) {
    v8::Handle<v8::Number> numberParameter = parameter->ToNumber();
    return TRI_CreateNumberJson(TRI_UNKNOWN_MEM_ZONE, numberParameter->Value());
  }

  if (parameter->IsString()) {
    v8::Handle<v8::String> stringParameter= parameter->ToString();
    v8::String::Utf8Value str(stringParameter);
    return TRI_CreateStringCopyJson(TRI_UNKNOWN_MEM_ZONE, *str);
  }

  if (parameter->IsArray()) {
    v8::Handle<v8::Array> arrayParameter = v8::Handle<v8::Array>::Cast(parameter);
    TRI_json_t* listJson = TRI_CreateListJson(TRI_UNKNOWN_MEM_ZONE);
    if (listJson) {
      for (uint32_t j = 0; j < arrayParameter->Length(); ++j) {
        v8::Handle<v8::Value> item = arrayParameter->Get(j);    
        TRI_json_t* result = ConvertHelper(item);
        if (result) {
          TRI_PushBack2ListJson(listJson, result);
          TRI_Free(TRI_UNKNOWN_MEM_ZONE, result);
        }
      }
    }
    return listJson;
  }

  if (parameter->IsObject()) {
    v8::Handle<v8::Array> arrayParameter = v8::Handle<v8::Array>::Cast(parameter);
    TRI_json_t* arrayJson = TRI_CreateArrayJson(TRI_UNKNOWN_MEM_ZONE);
    if (arrayJson) {
      v8::Handle<v8::Array> names = arrayParameter->GetOwnPropertyNames();
      for (uint32_t j = 0; j < names->Length(); ++j) {
        v8::Handle<v8::Value> key = names->Get(j);
        v8::Handle<v8::Value> item = arrayParameter->Get(key);    
        TRI_json_t* result = ConvertHelper(item);
        if (result) {
          TRI_Insert2ArrayJson(TRI_UNKNOWN_MEM_ZONE, arrayJson, TRI_ObjectToString(key).c_str(), result);
          TRI_Free(TRI_UNKNOWN_MEM_ZONE, result);
        }
      }
    }
    return arrayJson;
  }
  
  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a index identifier
////////////////////////////////////////////////////////////////////////////////

static TRI_index_t* LookupIndexByHandle (TRI_vocbase_t* vocbase,
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
      *err = CreateErrorObject(TRI_ERROR_AVOCADO_INDEX_HANDLE_BAD,
                               "<index-handle> must be a index-handle");
      return 0;
    }
  }

  // extract the document identifier and revision from a string
  else if (val->IsObject()) {
    v8::Handle<v8::Object> obj = val->ToObject();
    v8::Handle<v8::Value> iidVal = obj->Get(v8g->IidKey);

    if (! IsIndexHandle(iidVal, cid, iid)) {
      *err = CreateErrorObject(TRI_ERROR_AVOCADO_INDEX_HANDLE_BAD, 
                               "expecting a index-handle in id");
      return 0;
    }
  }

  // lockup the collection
  if (collection == 0) {
    TRI_vocbase_col_t* vc = TRI_LookupCollectionByIdVocBase(vocbase, cid);

    if (vc == 0) {
      *err = CreateErrorObject(TRI_ERROR_AVOCADO_COLLECTION_NOT_FOUND, 
                               "collection of <index-handle> is unknown");
      return 0;
    }

    // use the collection
    int res = TRI_UseCollectionVocBase(vocbase, vc);

    if (res != TRI_ERROR_NO_ERROR) {
      *err = CreateErrorObject(res, "cannot use/load collection");
      return 0;
    }

    collection = vc;

    if (collection->_collection == 0) {
      *err = CreateErrorObject(TRI_ERROR_INTERNAL, "cannot use/load collection");
      return 0;
    }
  }

  // check cross collection requests
  if (cid != collection->_collection->base._cid) {
    if (cid == 0) {
      *err = CreateErrorObject(TRI_ERROR_AVOCADO_COLLECTION_NOT_FOUND,
                               "collection of <index-handle> unknown");
      return 0;
    }
    else {
      *err = CreateErrorObject(TRI_ERROR_AVOCADO_CROSS_COLLECTION_REQUEST,
                               "cannot execute cross collection index");
      return 0;
    }
  }

  TRI_index_t* idx = TRI_LookupIndex(collection->_collection, iid);

  if (idx == 0) {
    if (! ignoreNotFound) {
      *err = CreateErrorObject(TRI_ERROR_AVOCADO_INDEX_NOT_FOUND, "index is unknown");
    }

    return 0;
  }

  return idx;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a document
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> DocumentVocbaseCol (TRI_vocbase_t* vocbase,
                                                 TRI_vocbase_col_t const* collection,
                                                 v8::Arguments const& argv) {
  v8::HandleScope scope;

  // first and only argument schould be a document idenfifier
  if (argv.Length() != 1) {
    return scope.Close(v8::ThrowException(
                         CreateErrorObject(TRI_ERROR_BAD_PARAMETER, 
                                           "usage: document(<document-handle>)")));
  }

  TRI_voc_did_t did;
  TRI_voc_rid_t rid;
  v8::Handle<v8::Value> err = ParseDocumentOrDocumentHandle(vocbase, collection, did, rid, argv[0]);

  if (! err.IsEmpty()) {
    if (collection != 0) {
      ReleaseCollection(collection);
    }

    return scope.Close(v8::ThrowException(err));
  }

  // .............................................................................
  // get document
  // .............................................................................

  TRI_doc_mptr_t document;
  v8::Handle<v8::Value> result;
  
  // .............................................................................
  // inside a read transaction
  // .............................................................................

  collection->_collection->beginRead(collection->_collection);
  
  document = collection->_collection->read(collection->_collection, did);  

  if (document._did != 0) {
    TRI_barrier_t* barrier;

    barrier = TRI_CreateBarrierElement(&collection->_collection->_barrierList);
    result = TRI_WrapShapedJson(collection, &document, barrier);
  }

  collection->_collection->endRead(collection->_collection);

  // .............................................................................
  // outside a write transaction
  // .............................................................................

  ReleaseCollection(collection);

  if (document._did == 0) {
    return scope.Close(v8::ThrowException(
                         CreateErrorObject(TRI_ERROR_AVOCADO_DOCUMENT_NOT_FOUND,
                                           "document not found")));
  }

  if (rid != 0 && document._rid != rid) {
    return scope.Close(v8::ThrowException(
                         CreateErrorObject(TRI_ERROR_AVOCADO_CONFLICT,
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
    ReleaseCollection(collection);

    return scope.Close(v8::ThrowException(
                         CreateErrorObject(TRI_ERROR_BAD_PARAMETER, 
                                           "usage: replace(<document>, <data>, <overwrite>)")));
  }

  TRI_voc_did_t did;
  TRI_voc_rid_t rid;

  v8::Handle<v8::Value> err = ParseDocumentOrDocumentHandle(vocbase, collection, did, rid, argv[0]);

  if (! err.IsEmpty()) {
    if (collection != 0) {
      ReleaseCollection(collection);
    }

    return scope.Close(v8::ThrowException(err));
  }

  // convert data
  TRI_doc_collection_t* doc = collection->_collection;
  TRI_shaped_json_t* shaped = TRI_ShapedJsonV8Object(argv[1], doc->_shaper);

  if (shaped == 0) {
    ReleaseCollection(collection);
    return scope.Close(v8::ThrowException(
                         CreateErrorObject(TRI_errno(),
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

  // .............................................................................
  // inside a write transaction
  // .............................................................................

  collection->_collection->beginWrite(collection->_collection);

  TRI_voc_rid_t oldRid = 0;
  TRI_doc_mptr_t mptr = doc->update(doc, shaped, did, rid, &oldRid, policy, true);

  // .............................................................................
  // outside a write transaction
  // .............................................................................

  TRI_FreeShapedJson(doc->_shaper, shaped);

  if (mptr._did == 0) {
    ReleaseCollection(collection);
    return scope.Close(v8::ThrowException(
                         CreateErrorObject(TRI_errno(), 
                                           "cannot replace document")));
  }

  string id = StringUtils::itoa(doc->base._cid) + string(TRI_DOCUMENT_HANDLE_SEPARATOR_STR) + StringUtils::itoa(mptr._did);

  v8::Handle<v8::Object> result = v8::Object::New();
  result->Set(v8g->DidKey, v8::String::New(id.c_str()));
  result->Set(v8g->RevKey, v8::Number::New(mptr._rid));
  result->Set(v8g->OldRevKey, v8::Number::New(oldRid));

  ReleaseCollection(collection);
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
  if (argv.Length() < 1) {
    ReleaseCollection(collection);
    return scope.Close(v8::ThrowException(CreateErrorObject(TRI_ERROR_BAD_PARAMETER, 
                                                            "usage: delete(<document>, <overwrite>)")));
  }

  TRI_voc_did_t did;
  TRI_voc_rid_t rid;

  v8::Handle<v8::Value> err = ParseDocumentOrDocumentHandle(vocbase, collection, did, rid, argv[0]);

  if (! err.IsEmpty()) {
    if (collection != 0) {
      ReleaseCollection(collection);
    }

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

  // .............................................................................
  // inside a write transaction
  // .............................................................................

  TRI_doc_collection_t* doc = collection->_collection;
  TRI_voc_rid_t oldRid;

  int res = doc->destroyLock(doc, did, rid, &oldRid, policy);

  // .............................................................................
  // outside a write transaction
  // .............................................................................

  ReleaseCollection(collection);

  if (res != TRI_ERROR_NO_ERROR) {
    if (res == TRI_ERROR_AVOCADO_DOCUMENT_NOT_FOUND && policy == TRI_DOC_UPDATE_LAST_WRITE) {
      return scope.Close(v8::False());
    }
    else {
      return scope.Close(v8::ThrowException(CreateErrorObject(res, "cannot delete document")));
    }
  }

  return scope.Close(v8::True());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new collection
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> CreateVocBase (v8::Arguments const& argv, bool edge) {
  v8::HandleScope scope;

  TRI_vocbase_t* vocbase = TRI_UnwrapClass<TRI_vocbase_t>(argv.Holder(), WRP_VOCBASE_TYPE);
  
  if (vocbase == 0) {
    return scope.Close(v8::ThrowException(CreateErrorObject(TRI_ERROR_INTERNAL, "corrupted vocbase")));
  }

  // expecting at least one arguments
  if (argv.Length() < 1) {
    return scope.Close(v8::ThrowException(
                         CreateErrorObject(TRI_ERROR_BAD_PARAMETER, 
                                           "usage: _create(<name>, <properties>)")));
  }

  // extract the name
  string name = TRI_ObjectToString(argv[0]);

  // extract the parameter
  TRI_col_parameter_t parameter;

  if (2 <= argv.Length()) {
    if (! argv[1]->IsObject()) {
      return scope.Close(v8::ThrowException(CreateErrorObject(TRI_ERROR_BAD_PARAMETER, "<properties> must be an object")));
    }

    v8::Handle<v8::Object> p = argv[1]->ToObject();
    v8::Handle<v8::String> waitForSyncKey = v8::String::New("waitForSync");
    v8::Handle<v8::String> journalSizeKey = v8::String::New("journalSize");
    v8::Handle<v8::String> isSystemKey = v8::String::New("isSystem");

    if (p->Has(journalSizeKey)) {
      double s = TRI_ObjectToDouble(p->Get(journalSizeKey));

      if (s < TRI_JOURNAL_MINIMAL_SIZE) {
        return scope.Close(v8::ThrowException(
                             CreateErrorObject(TRI_ERROR_BAD_PARAMETER, 
                                               "<properties>.journalSize too small")));
      }

      TRI_InitParameterCollection(&parameter, name.c_str(), (TRI_voc_size_t) s);
    }
    else {
      TRI_InitParameterCollection(&parameter, name.c_str(), vocbase->_defaultMaximalSize);
    }

    if (p->Has(waitForSyncKey)) {
      parameter._waitForSync = TRI_ObjectToBoolean(p->Get(waitForSyncKey));
    }

    if (p->Has(isSystemKey)) {
      parameter._isSystem = TRI_ObjectToBoolean(p->Get(isSystemKey));
    }
  }
  else {
    TRI_InitParameterCollection(&parameter, name.c_str(), vocbase->_defaultMaximalSize);
  }


  TRI_vocbase_col_t const* collection = TRI_CreateCollectionVocBase(vocbase, &parameter);

  if (collection == NULL) {
    return scope.Close(v8::ThrowException(CreateErrorObject(TRI_errno(), "cannot create collection")));
  }

  return scope.Close(edge ? TRI_WrapEdgesCollection(collection) : TRI_WrapCollection(collection));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a single collection or null
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> CollectionVocBase (v8::Arguments const& argv, bool edge) {
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

  return scope.Close(edge ? TRI_WrapEdgesCollection(collection) : TRI_WrapCollection(collection));
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

  TRI_doc_collection_t* doc = collection->_collection;

  if (doc->base._type != TRI_COL_TYPE_SIMPLE_DOCUMENT) {
    ReleaseCollection(collection);
    return scope.Close(v8::ThrowException(CreateErrorObject(TRI_ERROR_INTERNAL, "unknown collection type")));
  }

  TRI_sim_collection_t* sim = (TRI_sim_collection_t*) doc;
  TRI_index_t* idx = 0;
  bool created;

  // .............................................................................
  // case: <location>
  // .............................................................................

  if (argv.Length() == 1) {
    v8::String::Utf8Value loc(argv[0]);

    if (*loc == 0) {
      ReleaseCollection(collection);
      return scope.Close(v8::ThrowException(CreateErrorObject(TRI_ERROR_ILLEGAL_OPTION, "<location> must be an attribute path")));
    }

    idx = TRI_EnsureGeoIndex1SimCollection(sim, *loc, false, constraint, &created);
  }

  // .............................................................................
  // case: <location>, <geoJson>
  // .............................................................................

  else if (argv.Length() == 2 && (argv[1]->IsBoolean() || argv[1]->IsBooleanObject())) {
    v8::String::Utf8Value loc(argv[0]);

    if (*loc == 0) {
      ReleaseCollection(collection);
      return scope.Close(v8::ThrowException(CreateErrorObject(TRI_ERROR_ILLEGAL_OPTION, "<location> must be an attribute path")));
    }

    idx = TRI_EnsureGeoIndex1SimCollection(sim, *loc, TRI_ObjectToBoolean(argv[1]), constraint, &created);
  }

  // .............................................................................
  // case: <latitude>, <longitude>
  // .............................................................................

  else if (argv.Length() == 2) {
    v8::String::Utf8Value lat(argv[0]);
    v8::String::Utf8Value lon(argv[1]);

    if (*lat == 0) {
      ReleaseCollection(collection);
      return scope.Close(v8::ThrowException(CreateErrorObject(TRI_ERROR_ILLEGAL_OPTION, "<latitude> must be an attribute path")));
    }

    if (*lon == 0) {
      ReleaseCollection(collection);
      return scope.Close(v8::ThrowException(CreateErrorObject(TRI_ERROR_ILLEGAL_OPTION, "<longitude> must be an attribute path")));
    }

    idx = TRI_EnsureGeoIndex2SimCollection(sim, *lat, *lon, constraint, &created);
  }

  // .............................................................................
  // error case
  // .............................................................................

  else {
    ReleaseCollection(collection);
    return scope.Close(v8::ThrowException(CreateErrorObject(TRI_ERROR_ILLEGAL_OPTION,
      "usage: ensureGeoIndex(<latitude>, <longitude>) or ensureGeoIndex(<location>, [<geojson>])")));
  }

  if (idx == 0) {
    ReleaseCollection(collection);
    return scope.Close(v8::ThrowException(CreateErrorObject(TRI_errno(), "index could not be created")));
  }  
  
  TRI_json_t* json = idx->json(idx, collection->_collection);

  if (!json) {
    return scope.Close(v8::ThrowException(v8::String::New("out of memory")));
  }

  v8::Handle<v8::Value> index = IndexRep(&collection->_collection->base, json);
  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
  
  if (index->IsObject()) {
    index->ToObject()->Set(v8::String::New("isNewlyCreated"), created ? v8::True() : v8::False());
  }

  ReleaseCollection(collection);
  return scope.Close(index);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   QUERY FUNCTIONS
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up edges
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> EdgesQuery (TRI_edge_direction_e direction, v8::Arguments const& argv) {
  v8::HandleScope scope;

  // extract the collection
  v8::Handle<v8::Object> operand = argv.Holder();

  v8::Handle<v8::Object> err;
  TRI_vocbase_col_t const* collection = UseCollection(operand, &err);

  if (collection == 0) {
    return scope.Close(v8::ThrowException(err));
  }
  
  // handle various collection types
  TRI_doc_collection_t* doc = collection->_collection;

  if (doc->base._type != TRI_COL_TYPE_SIMPLE_DOCUMENT) {
    ReleaseCollection(collection);
    return scope.Close(v8::ThrowException(CreateErrorObject(TRI_ERROR_INTERNAL, "unknown collection type")));
  }

  TRI_sim_collection_t* sim = (TRI_sim_collection_t*) doc;

  // first and only argument schould be a list of document idenfifier
  if (argv.Length() != 1) {
    ReleaseCollection(collection);

    switch (direction) {
      case TRI_EDGE_UNUSED:
        return scope.Close(v8::ThrowException(
                             CreateErrorObject(TRI_ERROR_BAD_PARAMETER, 
                                               "usage: edge(<vertices>)")));

      case TRI_EDGE_IN:
        return scope.Close(v8::ThrowException(
                             CreateErrorObject(TRI_ERROR_BAD_PARAMETER, 
                                               "usage: inEdge(<vertices>)")));

      case TRI_EDGE_OUT:
        return scope.Close(v8::ThrowException(
                             CreateErrorObject(TRI_ERROR_BAD_PARAMETER, 
                                               "usage: outEdge(<vertices>)")));

      case TRI_EDGE_ANY:
        return scope.Close(v8::ThrowException(
                             CreateErrorObject(TRI_ERROR_BAD_PARAMETER, 
                                               "usage: edge(<vertices>)")));
    }
  }

  // setup result
  v8::Handle<v8::Array> documents = v8::Array::New();

  // .............................................................................
  // inside a read transaction
  // .............................................................................

  collection->_collection->beginRead(collection->_collection);

  TRI_barrier_t* barrier = 0;
  uint32_t count = 0;

  // argument is a list of vertices
  if (argv[0]->IsArray()) {
    v8::Handle<v8::Array> vertices = v8::Handle<v8::Array>::Cast(argv[0]);
    uint32_t len = vertices->Length();

    for (uint32_t i = 0;  i < len;  ++i) {
      TRI_vector_pointer_t edges;
      TRI_voc_cid_t cid;
      TRI_voc_did_t did;
      TRI_voc_rid_t rid;
      
      TRI_vocbase_col_t const* vertexCollection = 0;
      v8::Handle<v8::Value> errMsg = ParseDocumentOrDocumentHandle(collection->_vocbase, vertexCollection, did, rid, vertices->Get(i));
      
      if (! errMsg.IsEmpty()) {
        if (vertexCollection != 0) {
          ReleaseCollection(vertexCollection);
        }

        continue;
      }

      cid = vertexCollection->_cid;
      ReleaseCollection(vertexCollection);
      
      edges = TRI_LookupEdgesSimCollection(sim, direction, cid, did);
      
      for (size_t j = 0;  j < edges._length;  ++j) {
        if (barrier == 0) {
          barrier = TRI_CreateBarrierElement(&doc->_barrierList);
        }
        
        documents->Set(count++, TRI_WrapShapedJson(collection, (TRI_doc_mptr_t const*) edges._buffer[j], barrier));
      }
      
      TRI_DestroyVectorPointer(&edges);
    }
  }

  // argument is a single vertex
  else {
    TRI_vector_pointer_t edges;
    TRI_voc_cid_t cid;
    TRI_voc_did_t did;
    TRI_voc_rid_t rid;
      
    TRI_vocbase_col_t const* vertexCollection = 0;
    v8::Handle<v8::Value> errMsg = ParseDocumentOrDocumentHandle(collection->_vocbase, vertexCollection, did, rid, argv[0]);
      
    if (! errMsg.IsEmpty()) {
      collection->_collection->endRead(collection->_collection);

      if (vertexCollection != 0) {
        ReleaseCollection(vertexCollection);
      }

      ReleaseCollection(collection);
      return scope.Close(v8::ThrowException(errMsg));
    }

    cid = vertexCollection->_cid;
    ReleaseCollection(vertexCollection);

    edges = TRI_LookupEdgesSimCollection(sim, direction, cid, did);
      
    for (size_t j = 0;  j < edges._length;  ++j) {
      if (barrier == 0) {
        barrier = TRI_CreateBarrierElement(&doc->_barrierList);
      }
        
      documents->Set(count++, TRI_WrapShapedJson(collection, (TRI_doc_mptr_t const*) edges._buffer[j], barrier));
    }
    
    TRI_DestroyVectorPointer(&edges);
  }
  
  collection->_collection->endRead(collection->_collection);

  // .............................................................................
  // outside a write transaction
  // .............................................................................

  ReleaseCollection(collection);
  return scope.Close(documents);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief weak reference callback for query - DEPRECATED
////////////////////////////////////////////////////////////////////////////////

static void WeakQueryCallback (v8::Persistent<v8::Value> object, void* parameter) {
  TRI_query_t* query;
  TRI_v8_global_t* v8g;

  v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();
  query = (TRI_query_t*) parameter;

  LOG_TRACE("weak-callback for query called");

  // find the persistent handle
  v8::Persistent<v8::Value> persistent = v8g->JSQueries[query];
  v8g->JSQueries.erase(query);

  // dispose and clear the persistent handle
  persistent.Dispose();
  persistent.Clear();

  // and free the instance
  TRI_FreeQuery(query);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stores a query in a javascript object - DEPRECATED
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Object> WrapQuery (TRI_query_t* query) {
  TRI_v8_global_t* v8g;
  
  v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();

  v8::Handle<v8::Object> queryObject = v8g->QueryTempl->NewInstance();
  map< void*, v8::Persistent<v8::Value> >::iterator i = v8g->JSQueries.find(query);

  if (i == v8g->JSQueries.end()) {
    v8::Persistent<v8::Value> persistent = v8::Persistent<v8::Value>::New(v8::External::New(query));

    queryObject->SetInternalField(SLOT_CLASS_TYPE, v8::Integer::New(WRP_QUERY_TYPE));
    queryObject->SetInternalField(SLOT_CLASS, persistent);

    v8g->JSQueries[query] = persistent;

    persistent.MakeWeak(query, WeakQueryCallback);
  }
  else {
    queryObject->SetInternalField(SLOT_CLASS_TYPE, v8::Integer::New(WRP_QUERY_TYPE));
    queryObject->SetInternalField(SLOT_CLASS, i->second);
  }

  return queryObject;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a query or uses existing result set - DEPRECATED
////////////////////////////////////////////////////////////////////////////////

static TRI_rc_cursor_t* ExecuteQuery (v8::Handle<v8::Object> queryObject,
                                      v8::Handle<v8::Value>* err) {
  v8::TryCatch tryCatch;

  TRI_query_t* query = TRI_UnwrapClass<TRI_query_t>(queryObject, WRP_QUERY_TYPE);
  if (query == 0) {
    *err = v8::String::New("corrupted query");
    return 0;
  }
  
  LOG_TRACE("executing query");

  TRI_rc_context_t* context = TRI_CreateContextQuery(query);

  if (context == 0) {
    if (tryCatch.HasCaught()) {
      *err = tryCatch.Exception();
    }
    else {
      *err = v8::String::New("cannot create query context");
    }

    return 0;
  }

  TRI_rc_cursor_t* cursor = TRI_ExecuteQueryAql(query, context);
  if (cursor == 0) {
    TRI_FreeContextQuery(context);

    if (tryCatch.HasCaught()) {
      *err = tryCatch.Exception();
    }
    else {
      *err = v8::String::New("cannot execute query");
    }

    return 0;
  }

  return cursor;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief weak reference callback for cursors - DEPRECATED
////////////////////////////////////////////////////////////////////////////////

static void WeakCursorCallback (v8::Persistent<v8::Value> object, void* parameter) {
  TRI_rc_cursor_t* cursor;
  TRI_v8_global_t* v8g;

  v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();
  cursor = (TRI_rc_cursor_t*) parameter;

  LOG_TRACE("weak-callback for cursor called");

  // find the persistent handle
  v8::Persistent<v8::Value> persistent = v8g->JSCursors[cursor];
  v8g->JSCursors.erase(cursor);

  // dispose and clear the persistent handle
  persistent.Dispose();
  persistent.Clear();

  // and free the result set
//  cursor->free(cursor);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stores a cursor in a javascript object - DEPRECATED
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Object> WrapCursor (TRI_rc_cursor_t* cursor) {
  TRI_v8_global_t* v8g;

  v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();

  v8::Handle<v8::Object> cursorObject = v8g->CursorTempl->NewInstance();
  map< void*, v8::Persistent<v8::Value> >::iterator i = v8g->JSCursors.find(cursor);

  if (i == v8g->JSCursors.end()) {
    v8::Persistent<v8::Value> persistent = v8::Persistent<v8::Value>::New(v8::External::New(cursor));

    cursorObject->SetInternalField(SLOT_CLASS_TYPE, v8::Integer::New(WRP_RC_CURSOR_TYPE));
    cursorObject->SetInternalField(SLOT_CLASS, persistent);

    v8g->JSCursors[cursor] = persistent;

    persistent.MakeWeak(cursor, WeakCursorCallback);
  }
  else {
    cursorObject->SetInternalField(SLOT_CLASS_TYPE, v8::Integer::New(WRP_RC_CURSOR_TYPE));
    cursorObject->SetInternalField(SLOT_CLASS, i->second);
  }

  return cursorObject;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief weak reference callback for wheres - DEPRECATED
////////////////////////////////////////////////////////////////////////////////

static void WeakWhereCallback (v8::Persistent<v8::Value> object, void* parameter) {
  TRI_qry_where_t* where;
  TRI_v8_global_t* v8g;

  v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();
  where = (TRI_qry_where_t*) parameter;

  LOG_TRACE("weak-callback for where called");

  // find the persistent handle
  v8::Persistent<v8::Value> persistent = v8g->JSWheres[where];
  v8g->JSWheres.erase(where);

  // dispose and clear the persistent handle
  persistent.Dispose();
  persistent.Clear();

  // and free the result set
  where->free(where);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stores a where clause in a javascript object - DEPRECATED
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Object> WrapWhere (TRI_qry_where_t* where) {
  TRI_v8_global_t* v8g;

  v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();

  v8::Handle<v8::Object> whereObject = v8g->WhereTempl->NewInstance();
  map< void*, v8::Persistent<v8::Value> >::iterator i = v8g->JSWheres.find(where);

  if (i == v8g->JSWheres.end()) {
    v8::Persistent<v8::Value> persistent = v8::Persistent<v8::Value>::New(v8::External::New(where));

    whereObject->SetInternalField(SLOT_CLASS_TYPE, v8::Integer::New(WRP_QRY_WHERE_TYPE));
    whereObject->SetInternalField(SLOT_CLASS, persistent);

    v8g->JSWheres[where] = persistent;

    persistent.MakeWeak(where, WeakWhereCallback);
  }
  else {
    whereObject->SetInternalField(SLOT_CLASS_TYPE, v8::Integer::New(WRP_QRY_WHERE_TYPE));
    whereObject->SetInternalField(SLOT_CLASS, i->second);
  }

  return whereObject;
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
/// @brief selects all elements
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_AllQuery (v8::Arguments const& argv) {
  v8::HandleScope scope;

  // extract the collection
  v8::Handle<v8::Object> operand = argv.Holder();

  v8::Handle<v8::Object> err;
  TRI_vocbase_col_t const* collection = UseCollection(operand, &err);

  if (collection == 0) {
    return scope.Close(v8::ThrowException(err));
  }
  
  // handle various collection types
  TRI_doc_collection_t* doc = collection->_collection;

  if (doc->base._type != TRI_COL_TYPE_SIMPLE_DOCUMENT) {
    ReleaseCollection(collection);
    return scope.Close(v8::ThrowException(CreateErrorObject(TRI_ERROR_INTERNAL, "unknown collection type")));
  }

  TRI_sim_collection_t* sim = (TRI_sim_collection_t*) doc;

  // expecting two arguments
  if (argv.Length() != 2) {
    ReleaseCollection(collection);
    return scope.Close(v8::ThrowException(
                         CreateErrorObject(TRI_ERROR_BAD_PARAMETER, 
                                           "usage: ALL(<skip>, <limit>)")));
  }

  TRI_voc_size_t skip = TRI_QRY_NO_SKIP;
  TRI_voc_ssize_t limit = TRI_QRY_NO_LIMIT;

  if (! argv[0]->IsNull()) {
    skip = (TRI_voc_size_t) TRI_ObjectToDouble(argv[0]);
  }

  if (! argv[1]->IsNull()) {
    limit = (TRI_voc_ssize_t) TRI_ObjectToDouble(argv[1]);
  }

  // setup result
  v8::Handle<v8::Object> result = v8::Object::New();

  v8::Handle<v8::Array> documents = v8::Array::New();
  result->Set(v8::String::New("documents"), documents);

  // .............................................................................
  // inside a read transaction
  // .............................................................................

  collection->_collection->beginRead(collection->_collection);
  
  size_t total = sim->_primaryIndex._nrUsed;
  uint32_t count = 0;

  if (0 < total && 0 != limit) {
    TRI_barrier_t* barrier = 0;

    // skip from the beginning
    if (0 <= limit) {
      void** ptr = sim->_primaryIndex._table;
      void** end = sim->_primaryIndex._table + sim->_primaryIndex._nrAlloc;

      for (;  ptr < end && (TRI_voc_ssize_t) count < limit;  ++ptr) {
        if (*ptr) {
          TRI_doc_mptr_t const* d = (TRI_doc_mptr_t const*) *ptr;
          
          if (d->_deletion == 0) {
            if (0 < skip) {
              --skip;
            }
            else {
              if (barrier == 0) {
                barrier = TRI_CreateBarrierElement(&doc->_barrierList);
              }
            
              documents->Set(count, TRI_WrapShapedJson(collection, d, barrier));
              ++count;
            }
          }
        }
      }
    }

    // skip at the end
    else {
      limit = -limit;

      void** beg = sim->_primaryIndex._table;
      void** ptr = sim->_primaryIndex._table + sim->_primaryIndex._nrAlloc - 1;

      for (;  beg <= ptr && (TRI_voc_ssize_t) count < limit;  --ptr) {
        if (*ptr) {
          TRI_doc_mptr_t const* d = (TRI_doc_mptr_t const*) *ptr;
          
          if (d->_deletion == 0) {
            if (0 < skip) {
              --skip;
            }
            else {
              if (barrier == 0) {
                barrier = TRI_CreateBarrierElement(&doc->_barrierList);
              }
            
              documents->Set(count, TRI_WrapShapedJson(collection, d, barrier));
              ++count;
            }
          }
        }
      }

      // swap result
      if (1 < count) {
        for (uint32_t i = 0, j = count - 1;  i < j;  ++i, --j) {
          v8::Handle<v8::Value> tmp1 = documents->Get(i);
          v8::Handle<v8::Value> tmp2 = documents->Get(j);
          documents->Set(i, tmp2);
          documents->Set(j, tmp1);
        }
      }
    }
  }

  collection->_collection->endRead(collection->_collection);

  // .............................................................................
  // outside a write transaction
  // .............................................................................

  result->Set(v8::String::New("total"), v8::Number::New((double) total));
  result->Set(v8::String::New("count"), v8::Number::New(count));

  ReleaseCollection(collection);
  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief selects elements by example
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_ByExampleQuery (v8::Arguments const& argv) {
  v8::HandleScope scope;

  // extract the collection
  v8::Handle<v8::Object> operand = argv.Holder();

  v8::Handle<v8::Object> err;
  TRI_vocbase_col_t const* collection = UseCollection(operand, &err);

  if (collection == 0) {
    return scope.Close(v8::ThrowException(err));
  }
  
  // handle various collection types
  TRI_doc_collection_t* doc = collection->_collection;

  if (doc->base._type != TRI_COL_TYPE_SIMPLE_DOCUMENT) {
    ReleaseCollection(collection);
    return scope.Close(v8::ThrowException(CreateErrorObject(TRI_ERROR_INTERNAL, "unknown collection type")));
  }

  TRI_sim_collection_t* sim = (TRI_sim_collection_t*) doc;
  TRI_shaper_t* shaper = sim->base._shaper;

  // extract example
  if (argv.Length() == 0 || (argv.Length() % 2 == 1)) {
    ReleaseCollection(collection);
    return scope.Close(v8::ThrowException(
                         CreateErrorObject(TRI_ERROR_BAD_PARAMETER, 
                                           "usage: document(<path1>, <value1>, ...)")));
  }

  size_t n = argv.Length() / 2;
  TRI_shape_pid_t* pids = (TRI_shape_pid_t*) TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, n * sizeof(TRI_shape_pid_t), false);
  // TODO FIXME: memory allocation might fail
  TRI_shaped_json_t** values = (TRI_shaped_json_t**) TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, n * sizeof(TRI_shaped_json_t*), false);
  // TODO FIXME: memory allocation might fail
  
  for (size_t i = 0;  i < n;  ++i) {
    v8::Handle<v8::Value> key = argv[2 * i];
    v8::Handle<v8::Value> val = argv[2 * i + 1];

    v8::String::Utf8Value keyStr(key);
    pids[i] = shaper->findAttributePathByName(shaper, *keyStr);
    values[i] = TRI_ShapedJsonV8Object(val, shaper);

    if (*keyStr == 0 || values[i] == 0) {
      for (size_t j = 0;  j < i;  ++j) {
        TRI_FreeShapedJson(shaper, values[i]);
      }

      TRI_Free(TRI_UNKNOWN_MEM_ZONE, values);
      TRI_Free(TRI_UNKNOWN_MEM_ZONE, pids);

      ReleaseCollection(collection);

      if (*keyStr == 0) {
        return scope.Close(v8::ThrowException(
                             CreateErrorObject(TRI_ERROR_BAD_PARAMETER, 
                                               "cannot convert attribute name to UTF8")));
      }
      else {
        return scope.Close(v8::ThrowException(
                             CreateErrorObject(TRI_ERROR_BAD_PARAMETER, 
                                               "cannot convert value to JSON")));
      }

      assert(false);
    }
  }

  // .............................................................................
  // inside a read transaction
  // .............................................................................

  collection->_collection->beginRead(collection->_collection);
  
  // find documents by example
  TRI_vector_t filtered = TRI_SelectByExample(sim, n,  pids, values);

  // convert to list of shaped jsons
  v8::Handle<v8::Array> result = v8::Array::New();

  if (0 < filtered._length) {
    TRI_barrier_t* barrier = TRI_CreateBarrierElement(&collection->_collection->_barrierList);

    for (size_t j = 0;  j < filtered._length;  ++j) {
      TRI_doc_mptr_t* mptr = (TRI_doc_mptr_t*) TRI_AtVector(&filtered, j);
      v8::Handle<v8::Value> document = TRI_WrapShapedJson(collection, mptr, barrier);

      result->Set(j, document);
    }
  }

  collection->_collection->endRead(collection->_collection);

  // .............................................................................
  // outside a write transaction
  // .............................................................................

  // free
  for (size_t j = 0;  j < n;  ++j) {
    TRI_FreeShapedJson(shaper, values[j]);
  }

  TRI_Free(TRI_UNKNOWN_MEM_ZONE, values);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, pids);

  ReleaseCollection(collection);
  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up all edges for a set of vertices
///
/// @FUN{@FA{edge-collection}.edges(@FA{vertex})}
///
/// The @FN{edges} operator finds all edges starting from (outbound) or ending
/// in (inbound) @FA{vertex}.
///
/// @FUN{@FA{edge-collection}.edges(@FA{vertices})}
///
/// The @FN{edges} operator finds all edges starting from (outbound) or ending
/// in (inbound) a document from @FA{vertices}.
///
/// @EXAMPLES
///
/// @verbinclude shell_edge-edges
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_EdgesQuery (v8::Arguments const& argv) {
  return EdgesQuery(TRI_EDGE_ANY, argv);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up all inbound edges
///
/// @FUN{@FA{edge-collection}.inEdges(@FA{vertex})}
///
/// The @FN{edges} operator finds all edges ending in (inbound) @FA{vertex}.
///
/// @FUN{@FA{edge-collection}.inEdges(@FA{vertices})}
///
/// The @FN{edges} operator finds all edges ending in (inbound) a document from
/// @FA{vertices}.
///
/// @EXAMPLES
///
/// @verbinclude shell_edge-in-edges
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_InEdgesQuery (v8::Arguments const& argv) {
  return EdgesQuery(TRI_EDGE_IN, argv);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finds points near a given coordinate
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_NearQuery (v8::Arguments const& argv) {
  v8::HandleScope scope;

  // extract the collection
  v8::Handle<v8::Object> operand = argv.Holder();

  v8::Handle<v8::Object> err;
  TRI_vocbase_col_t const* collection = UseCollection(operand, &err);

  if (collection == 0) {
    return scope.Close(v8::ThrowException(err));
  }
  
  // handle various collection types
  TRI_doc_collection_t* doc = collection->_collection;

  if (doc->base._type != TRI_COL_TYPE_SIMPLE_DOCUMENT) {
    ReleaseCollection(collection);
    return scope.Close(v8::ThrowException(CreateErrorObject(TRI_ERROR_INTERNAL, "unknown collection type")));
  }

  // expect: NEAR(<index-id>, <latitude>, <longitude>, <limit>)
  if (argv.Length() != 4) {
    ReleaseCollection(collection);
    return scope.Close(v8::ThrowException(
                         CreateErrorObject(TRI_ERROR_BAD_PARAMETER, 
                                           "usage: NEAR(<index-handle>, <latitude>, <longitude>, <limit>)")));
  }

  // extract the index
  TRI_index_t* idx = LookupIndexByHandle(doc->base._vocbase, collection, argv[0], false, &err);

  if (idx == 0) {
    ReleaseCollection(collection);
    return scope.Close(v8::ThrowException(err));
  }

  if (idx->_type != TRI_IDX_TYPE_GEO_INDEX1 && idx->_type != TRI_IDX_TYPE_GEO_INDEX2) {
    ReleaseCollection(collection);
    return scope.Close(v8::ThrowException(CreateErrorObject(TRI_ERROR_BAD_PARAMETER, "index must be a geo-index")));
  }

  // extract latitude and longitude
  double latitude = TRI_ObjectToDouble(argv[1]);
  double longitude = TRI_ObjectToDouble(argv[2]);

  // extract the limit
  TRI_voc_ssize_t limit = (TRI_voc_ssize_t) TRI_ObjectToDouble(argv[3]);

  // setup result
  v8::Handle<v8::Object> result = v8::Object::New();

  v8::Handle<v8::Array> documents = v8::Array::New();
  result->Set(v8::String::New("documents"), documents);

  v8::Handle<v8::Array> distances = v8::Array::New();
  result->Set(v8::String::New("distances"), distances);

  // .............................................................................
  // inside a read transaction
  // .............................................................................

  collection->_collection->beginRead(collection->_collection);
  
  GeoCoordinates* cors = TRI_NearestGeoIndex(idx, latitude, longitude, limit);

  if (cors != 0) {
    StoreGeoResult(collection, cors, documents, distances);
  }

  collection->_collection->endRead(collection->_collection);

  // .............................................................................
  // outside a write transaction
  // .............................................................................

  ReleaseCollection(collection);
  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up all outbound edges
///
/// @FUN{@FA{edge-collection}.outEdges(@FA{vertex})}
///
/// The @FN{edges} operator finds all edges starting from (outbound)
/// @FA{vertices}.
///
/// @FUN{@FA{edge-collection}.outEdges(@FA{vertices})}
///
/// The @FN{edges} operator finds all edges starting from (outbound) a document
/// from @FA{vertices}.
///
/// @EXAMPLES
///
/// @verbinclude shell_edge-out-edges
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_OutEdgesQuery (v8::Arguments const& argv) {
  return EdgesQuery(TRI_EDGE_OUT, argv);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finds points within a given radius
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_WithinQuery (v8::Arguments const& argv) {
  v8::HandleScope scope;

  // extract the collection
  v8::Handle<v8::Object> operand = argv.Holder();

  v8::Handle<v8::Object> err;
  TRI_vocbase_col_t const* collection = UseCollection(operand, &err);

  if (collection == 0) {
    return scope.Close(v8::ThrowException(err));
  }
  
  // handle various collection types
  TRI_doc_collection_t* doc = collection->_collection;

  if (doc->base._type != TRI_COL_TYPE_SIMPLE_DOCUMENT) {
    ReleaseCollection(collection);
    return scope.Close(v8::ThrowException(CreateErrorObject(TRI_ERROR_INTERNAL, "unknown collection type")));
  }

  // expect: WITHIN(<index-handle>, <latitude>, <longitude>, <limit>)
  if (argv.Length() != 4) {
    ReleaseCollection(collection);
    return scope.Close(v8::ThrowException(
                         CreateErrorObject(TRI_ERROR_BAD_PARAMETER, 
                                           "usage: WITHIN(<index-handle>, <latitude>, <longitude>, <radius>)")));
  }

  // extract the index
  TRI_index_t* idx = LookupIndexByHandle(doc->base._vocbase, collection, argv[0], false, &err);

  if (idx == 0) {
    ReleaseCollection(collection);
    return scope.Close(v8::ThrowException(err));
  }

  if (idx->_type != TRI_IDX_TYPE_GEO_INDEX1 && idx->_type != TRI_IDX_TYPE_GEO_INDEX2) {
    ReleaseCollection(collection);
    return scope.Close(v8::ThrowException(CreateErrorObject(TRI_ERROR_BAD_PARAMETER, "index must be a geo-index")));
  }

  // extract latitude and longitude
  double latitude = TRI_ObjectToDouble(argv[1]);
  double longitude = TRI_ObjectToDouble(argv[2]);

  // extract the limit
  double radius = TRI_ObjectToDouble(argv[3]);

  // setup result
  v8::Handle<v8::Object> result = v8::Object::New();

  v8::Handle<v8::Array> documents = v8::Array::New();
  result->Set(v8::String::New("documents"), documents);

  v8::Handle<v8::Array> distances = v8::Array::New();
  result->Set(v8::String::New("distances"), distances);

  // .............................................................................
  // inside a read transaction
  // .............................................................................

  collection->_collection->beginRead(collection->_collection);
  
  GeoCoordinates* cors = TRI_WithinGeoIndex(idx, latitude, longitude, radius);

  if (cors != 0) {
    StoreGeoResult(collection, cors, documents, distances);
  }

  collection->_collection->endRead(collection->_collection);

  // .............................................................................
  // outside a write transaction
  // .............................................................................

  ReleaseCollection(collection);
  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   GENERAL CURSORS
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief weak reference callback for general cursors
////////////////////////////////////////////////////////////////////////////////

static void WeakGeneralCursorCallback (v8::Persistent<v8::Value> object, void* parameter) {
  v8::HandleScope scope;

  LOG_TRACE("weak-callback for general cursor called");
  
  TRI_vocbase_t* vocbase = GetContextVocBase(); 
  if (!vocbase) {
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
/// @brief destroys a general cursor
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_DisposeGeneralCursor (v8::Arguments const& argv) {
  v8::HandleScope scope;
  v8::TryCatch tryCatch;

  if (argv.Length() != 0) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: dispose()")));
  }
  
  TRI_vocbase_t* vocbase = GetContextVocBase(); 
  if (!vocbase) {
    return scope.Close(v8::ThrowException(v8::String::New("corrupted vocbase")));
  }

  if (TRI_DeleteDataShadowData(vocbase->_cursors, UnwrapGeneralCursor(argv.Holder()))) {
    if (!tryCatch.HasCaught()) {
      return scope.Close(v8::True());
    }
  }

  return scope.Close(v8::ThrowException(v8::String::New("corrupted or already disposed cursor")));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the id of a general cursor
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_IdGeneralCursor (v8::Arguments const& argv) {
  v8::HandleScope scope;
  v8::TryCatch tryCatch;

  if (argv.Length() != 0) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: id()")));
  }
  
  TRI_vocbase_t* vocbase = GetContextVocBase(); 
  if (!vocbase) {
    return scope.Close(v8::ThrowException(v8::String::New("corrupted vocbase")));
  }
  
  TRI_shadow_id id = TRI_GetIdDataShadowData(vocbase->_cursors, UnwrapGeneralCursor(argv.Holder()));
  if (id && !tryCatch.HasCaught()) {
    return scope.Close(v8::Number::New((double) id));
  }
  
  return scope.Close(v8::ThrowException(v8::String::New("corrupted or already disposed cursor")));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the number of results
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_CountGeneralCursor (v8::Arguments const& argv) {
  v8::HandleScope scope;
  v8::TryCatch tryCatch;

  if (argv.Length() != 0) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: count()")));
  }
  
  TRI_vocbase_t* vocbase = GetContextVocBase(); 
  if (!vocbase) {
    return scope.Close(v8::ThrowException(v8::String::New("corrupted vocbase")));
  }
  
  TRI_general_cursor_t* cursor;
  
  cursor = (TRI_general_cursor_t*) TRI_BeginUsageDataShadowData(vocbase->_cursors, UnwrapGeneralCursor(argv.Holder()));

  if (cursor) {
    size_t  length = (size_t) cursor->_length;
    TRI_EndUsageDataShadowData(vocbase->_cursors, cursor);
    return scope.Close(v8::Number::New(length));
  }
  
  return scope.Close(v8::ThrowException(v8::String::New("corrupted or already freed cursor")));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the next result from the general cursor
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_NextGeneralCursor (v8::Arguments const& argv) {
  v8::HandleScope scope;
  v8::TryCatch tryCatch;

  if (argv.Length() != 0) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: next()")));
  }
  
  TRI_vocbase_t* vocbase = GetContextVocBase(); 
  if (!vocbase) {
    return scope.Close(v8::ThrowException(v8::String::New("corrupted vocbase")));
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
    try {
      TRI_general_cursor_row_t row = cursor->next(cursor);
      if (!row) {
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
    
    if (result && !tryCatch.HasCaught()) {
      return scope.Close(value);
    }
  }

  return scope.Close(v8::ThrowException(v8::String::New("corrupted or already freed cursor")));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief persist the general cursor for usage in subsequent requests
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_PersistGeneralCursor (v8::Arguments const& argv) {
  v8::HandleScope scope;
  v8::TryCatch tryCatch;

  if (argv.Length() != 0) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: persist()")));
  }
  
  TRI_vocbase_t* vocbase = GetContextVocBase(); 
  if (!vocbase) {
    return scope.Close(v8::ThrowException(v8::String::New("corrupted vocbase")));
  }

  bool result = TRI_PersistDataShadowData(vocbase->_cursors, UnwrapGeneralCursor(argv.Holder()));
  if (result && !tryCatch.HasCaught()) {
    return scope.Close(v8::True());
  }
    
  return scope.Close(v8::ThrowException(v8::String::New("corrupted or already freed cursor")));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the next x rows from the cursor in one go
///
/// This function constructs multiple rows at once and should be preferred over
/// hasNext()...next() when iterating over bigger result sets
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_GetRowsGeneralCursor (v8::Arguments const& argv) {
  v8::HandleScope scope;
  v8::TryCatch tryCatch;

  if (argv.Length() != 0) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: getRows()")));
  }
  
  TRI_vocbase_t* vocbase = GetContextVocBase(); 
  if (!vocbase) {
    return scope.Close(v8::ThrowException(v8::String::New("corrupted vocbase")));
  }

  bool result = false;  
  v8::Handle<v8::Array> rows = v8::Array::New();
  TRI_general_cursor_t* cursor;
  
  cursor = (TRI_general_cursor_t*) TRI_BeginUsageDataShadowData(vocbase->_cursors, UnwrapGeneralCursor(argv.Holder()));

  if (cursor) {
    TRI_LockGeneralCursor(cursor);

    // exceptions must be caught in the following part because we hold an exclusive
    // lock that might otherwise not be freed
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

    if (result && !tryCatch.HasCaught()) {
      return scope.Close(rows);
    }
  }

  return scope.Close(v8::ThrowException(v8::String::New("corrupted or already freed cursor")));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return max number of results per transfer for cursor
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_GetBatchSizeGeneralCursor (v8::Arguments const& argv) {
  v8::HandleScope scope;
  v8::TryCatch tryCatch;

  if (argv.Length() != 0) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: getBatchSize()")));
  }
  
  TRI_vocbase_t* vocbase = GetContextVocBase(); 
  if (!vocbase) {
    return scope.Close(v8::ThrowException(v8::String::New("corrupted vocbase")));
  }
  
  TRI_general_cursor_t* cursor;
  
  cursor = (TRI_general_cursor_t*) TRI_BeginUsageDataShadowData(vocbase->_cursors, UnwrapGeneralCursor(argv.Holder()));

  if (cursor) {
    uint32_t max = cursor->getBatchSize(cursor);
    
    TRI_EndUsageDataShadowData(vocbase->_cursors, cursor);
    return scope.Close(v8::Number::New(max));
  }

  return scope.Close(v8::ThrowException(v8::String::New("corrupted or already freed cursor")));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return if count flag was set for cursor
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_HasCountGeneralCursor (v8::Arguments const& argv) {
  v8::HandleScope scope;
  v8::TryCatch tryCatch;

  if (argv.Length() != 0) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: hasCount()")));
  }
  
  TRI_vocbase_t* vocbase = GetContextVocBase(); 
  if (!vocbase) {
    return scope.Close(v8::ThrowException(v8::String::New("corrupted vocbase")));
  }

  TRI_general_cursor_t* cursor;
  
  cursor = (TRI_general_cursor_t*) TRI_BeginUsageDataShadowData(vocbase->_cursors, UnwrapGeneralCursor(argv.Holder()));

  if (cursor) {
    bool hasCount = cursor->hasCount(cursor);

    TRI_EndUsageDataShadowData(vocbase->_cursors, cursor);
    return scope.Close(hasCount ? v8::True() : v8::False());
  }
  
  return scope.Close(v8::ThrowException(v8::String::New("corrupted or already freed cursor")));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if the cursor is exhausted 
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_HasNextGeneralCursor (v8::Arguments const& argv) {
  v8::HandleScope scope;
  v8::TryCatch tryCatch;

  if (argv.Length() != 0) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: hasNext()")));
  }
  
  TRI_vocbase_t* vocbase = GetContextVocBase(); 
  if (!vocbase) {
    return scope.Close(v8::ThrowException(v8::String::New("corrupted vocbase")));
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
  
  return scope.Close(v8::ThrowException(v8::String::New("corrupted or already freed cursor")));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get a (persistent) cursor by its id
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_Cursor (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 1) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: CURSOR(<cursor-id>)")));
  }

  TRI_vocbase_t* vocbase = GetContextVocBase(); 
  if (!vocbase) {
    return scope.Close(v8::ThrowException(v8::String::New("corrupted vocbase")));
  }

  // get the id
  v8::Handle<v8::Value> idArg = argv[0]->ToString();
  if (!idArg->IsString()) {
    return scope.Close(v8::ThrowException(v8::String::New("expecting string for <id>")));
  }
  string idString = TRI_ObjectToString(idArg);
  uint64_t id = TRI_UInt64String(idString.c_str());

  TRI_general_cursor_t* cursor;
  
  cursor = (TRI_general_cursor_t*) TRI_BeginUsageIdShadowData(vocbase->_cursors, id);
  if (!cursor) {
    return scope.Close(v8::ThrowException(v8::String::New("corrupted or already freed cursor")));
  }

  return scope.Close(WrapGeneralCursor(cursor));
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
/// @brief Create an Ahuacatl error in a javascript object
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Object> CreateErrorObjectAhuacatl (TRI_aql_error_t* error) {
  return CreateErrorObject(TRI_GetErrorCodeAql(error), TRI_GetErrorMessageAql(error));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates code for an Ahuacatl query and runs it
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_RunAhuacatl (v8::Arguments const& argv) {
  v8::HandleScope scope;
  v8::TryCatch tryCatch;

  if (argv.Length() < 1 || argv.Length() > 4) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: AHUACATL_RUN(<querystring>, <bindvalues>, <doCount>, <max>)")));
  }
 
  TRI_vocbase_t* vocbase = GetContextVocBase(); 
  if (!vocbase) {
    return scope.Close(v8::ThrowException(v8::String::New("corrupted vocbase")));
  }
  
  // get the query string
  v8::Handle<v8::Value> queryArg = argv[0];
  if (!queryArg->IsString()) {
    return scope.Close(v8::ThrowException(v8::String::New("expecting string for <querystring>")));
  }
  string queryString = TRI_ObjectToString(queryArg);

  // return number of total records in cursor?
  bool doCount = false;
  if (argv.Length() > 2) {
    doCount = TRI_ObjectToBoolean(argv[2]);
  }

  // maximum number of results to return at once
  uint32_t batchSize = 1000;
  if (argv.Length() > 3) {
    double maxValue = TRI_ObjectToDouble(argv[3]);
    if (maxValue >= 1.0) {
      batchSize = (uint32_t) maxValue;
    }
  }
  
  TRI_aql_context_t* context = TRI_CreateContextAql(vocbase, queryString.c_str()); 
  if (!context) {
    return scope.Close(v8::ThrowException(v8::String::New("out of memory")));
  }
 
  // parse & validate 
  if (!TRI_ValidateQueryContextAql(context)) {
    v8::Handle<v8::Object> errorObject = CreateErrorObjectAhuacatl(&context->_error);
    TRI_FreeContextAql(context);
    return scope.Close(errorObject);
  }

  // bind parameters
  TRI_json_t* parameters = NULL;
  if (argv.Length() > 1) {
    parameters = ConvertHelper(argv[1]);
  }

  if (!TRI_BindQueryContextAql(context, parameters)) {
    v8::Handle<v8::Object> errorObject = CreateErrorObjectAhuacatl(&context->_error);
    if (parameters) {
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, parameters);
    }
    TRI_FreeContextAql(context);
    return scope.Close(errorObject);
  }
  
  if (parameters) {
    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, parameters);
  }

  // optimise
  if (!TRI_OptimiseQueryContextAql(context)) {
    v8::Handle<v8::Object> errorObject = CreateErrorObjectAhuacatl(&context->_error);
    TRI_FreeContextAql(context);
    return scope.Close(errorObject);
  }

  // acquire locks
  if (!TRI_LockQueryContextAql(context)) {
    v8::Handle<v8::Object> errorObject = CreateErrorObjectAhuacatl(&context->_error);
    TRI_FreeContextAql(context);
    return scope.Close(errorObject);
  }

  TRI_general_cursor_t* cursor = 0;
  
  // generate code
  if (context->_first) {
    char* code = TRI_GenerateCodeAql((TRI_aql_node_t*) context->_first);
    
    if (code) {
      v8::Handle<v8::Value> result;
      result = TRI_ExecuteStringVocBase(v8::Context::GetCurrent(), v8::String::New(code), v8::String::New("query"));
      TRI_Free(TRI_UNKNOWN_MEM_ZONE, code);

      TRI_json_t* json = ConvertHelper(result);
      if (json) {
        TRI_general_cursor_result_t* cursorResult = TRI_CreateResultAql(json);
        if (cursorResult) {
          cursor = TRI_CreateGeneralCursor(cursorResult, doCount, batchSize);
          if (!cursor) {
            TRI_Free(TRI_UNKNOWN_MEM_ZONE, cursorResult);
            TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
          }
        }
        else {
          TRI_Free(TRI_UNKNOWN_MEM_ZONE, cursorResult);
          TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
        }
      }
    }
  }

  TRI_FreeContextAql(context);

  if (cursor) {
    TRI_StoreShadowData(vocbase->_cursors, (const void* const) cursor);
    return scope.Close(WrapGeneralCursor(cursor));
  }

  return scope.Close(v8::ThrowException(v8::String::New("cannot create cursor")));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parses a query and returns the parse result
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_ParseAhuacatl (v8::Arguments const& argv) {
  v8::HandleScope scope;
  v8::TryCatch tryCatch;
  
  if (argv.Length() != 1) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: AHUACATL_PARSE(<querystring>)")));
  }
  
  TRI_vocbase_t* vocbase = GetContextVocBase(); 
  if (!vocbase) {
    return scope.Close(v8::ThrowException(v8::String::New("corrupted vocbase")));
  }

  // get the query string
  v8::Handle<v8::Value> queryArg = argv[0];
  if (!queryArg->IsString()) {
    return scope.Close(v8::ThrowException(v8::String::New("expecting string for <querystring>")));
  }
  string queryString = TRI_ObjectToString(queryArg);

  TRI_aql_context_t* context = TRI_CreateContextAql(vocbase, queryString.c_str()); 
  if (!context) {
    return scope.Close(v8::ThrowException(v8::String::New("out of memory")));
  }

  // parse & validate 
  if (!TRI_ValidateQueryContextAql(context)) {
    v8::Handle<v8::Object> errorObject = CreateErrorObjectAhuacatl(&context->_error);
    TRI_FreeContextAql(context);
    return scope.Close(errorObject);
  }

  // return the bind parameter names
  v8::Handle<v8::Array> result = TRI_ArrayAssociativePointer(&context->_parameterNames);
    
  TRI_FreeContextAql(context);
  if (tryCatch.HasCaught()) {
    return scope.Close(v8::ThrowException(v8::String::New("out of memory")));
  }

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                            AVOCADO QUERY LANGUAGE
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                              javascript functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new constant where clause using a boolean - DEPRECATED
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_WhereBooleanAql (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 1) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: whereConstant(<boolean>)")));
  }

  // extract the where clause
  v8::Handle<v8::Value> whereArg = argv[0];
  TRI_qry_where_t* where = 0;

  where = TRI_CreateQueryWhereBoolean(TRI_ObjectToBoolean(whereArg));

  // wrap it up
  return scope.Close(WrapWhere(where));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new where clause from JavaScript - DEPRECATED
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_WhereGeneralAql (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 1) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: whereGeneral(<where>)")));
  }

  // extract the where clause
  v8::Handle<v8::Value> whereArg = argv[0];
  TRI_qry_where_t* where = 0;

  string cmd = TRI_ObjectToString(whereArg);

  if (cmd.empty()) {
    return scope.Close(v8::ThrowException(v8::String::New("<where> must be a valid expression")));
  }

  where = TRI_CreateQueryWhereGeneral(cmd.c_str());

  // wrap it up
  return scope.Close(WrapWhere(where));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new constant where clause for primary index - DEPRECATED
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_WherePrimaryConstAql (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 1) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: wherePrimaryConst(<document-handle>)")));
  }

  // extract the document handle
  TRI_voc_cid_t cid = 0;
  TRI_voc_did_t did = 0;
  bool ok = IsDocumentHandle(argv[0], cid, did);

  if (! ok) {
    return scope.Close(v8::ThrowException(v8::String::New("expecting a <document-handle>")));
  }

  // build document hash access
  TRI_qry_where_t* where = TRI_CreateQueryWherePrimaryConstant(did);

  // wrap it up
  return scope.Close(WrapWhere(where));
}


////////////////////////////////////////////////////////////////////////////////
/// @brief DEPRECATED
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_WhereHashConstAql (const v8::Arguments& argv) {
  v8::HandleScope scope;
  TRI_json_t* parameterList;

  if (argv.Length() < 2) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: AQL_WHERE_HASH_CONST(<index-identifier>, <value 1>, <value 2>,..., <value n>)")));
  }

  
  // ..........................................................................
  // check that the first parameter sent is a double value
  // ..........................................................................
  bool inValidType = true;
  TRI_idx_iid_t iid = TRI_ObjectToDouble(argv[0], inValidType); 
  
  if (inValidType || iid == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("<index-identifier> must be an positive integer")));       
  }  


  // ..........................................................................
  // Store the index field parameters in a json object 
  // ..........................................................................
  parameterList = TRI_CreateListJson(TRI_UNKNOWN_MEM_ZONE);
  if (!parameterList) {
    return scope.Close(v8::ThrowException(v8::String::New("out of memory")));
  }

  for (int j = 1; j < argv.Length(); ++j) {  
    v8::Handle<v8::Value> parameter = argv[j];
    TRI_json_t* jsonParameter = ConvertHelper(parameter);
    if (jsonParameter == NULL) { // NOT the null json value! 
      return scope.Close(v8::ThrowException(v8::String::New("type value not currently supported for hash index")));       
    }
    TRI_PushBackListJson(TRI_UNKNOWN_MEM_ZONE, parameterList, jsonParameter);
    
    /*
    if (parameter->IsBoolean() ) {
      v8::Handle<v8::Boolean> booleanParameter = parameter->ToBoolean();
      TRI_PushBackListJson(parameterList, TRI_CreateBooleanJson(booleanParameter->Value() ));
    }    
    else if ( parameter->IsNumber() ) {
      v8::Handle<v8::Number> numberParameter = parameter->ToNumber();
      TRI_PushBackListJson(parameterList, TRI_CreateNumberJson(numberParameter->Value() ));
    }
    else if ( parameter->IsString() ) {
      v8::Handle<v8::String>  stringParameter= parameter->ToString();
      v8::String::Utf8Value str(stringParameter);
      TRI_PushBackListJson(parameterList, TRI_CreateStringCopyJson(*str));
    }
    else if ( parameter->IsArray() ) {
      v8::Handle<v8::Array> arrayParameter = v8::Handle<v8::Array>(v8::Array::Cast(*parameter));
    }
    else {
      return scope.Close(v8::ThrowException(v8::String::New("type value not currently supported for hash index")));       
    }
    */
  }
  

  // build document hash access
  TRI_qry_where_t* where = TRI_CreateQueryWhereHashConstant(iid, parameterList);

  // wrap it up
  return scope.Close(WrapWhere(where));
  
}


////////////////////////////////////////////////////////////////////////////////
/// @brief DEPRECATED
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_WherePQConstAql (const v8::Arguments& argv) {
  v8::HandleScope scope;
  TRI_json_t* parameterList;

  if (argv.Length() > 2 ||  argv.Length() == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: AQL_WHERE_PQ_CONST(<index-identifier> {,<value 1>})")));
  }

  
  // ..........................................................................
  // check that the first parameter sent is a double value -- the index id
  // ..........................................................................
  
  bool inValidType = true;
  TRI_idx_iid_t iid = TRI_ObjectToDouble(argv[0], inValidType); 
  
  if (inValidType || iid == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("<index-identifier> must be an positive integer")));       
  }  


  // ..........................................................................
  // Store the index field parameters in a json object -- there is only one
  // possible parameter to be sent - the number of top documents to query.
  // ..........................................................................
  
  parameterList = TRI_CreateListJson(TRI_UNKNOWN_MEM_ZONE);
  if (!parameterList) {
    return scope.Close(v8::ThrowException(v8::String::New("out of memory in JS_WherePQConstAql")));
  }

  
  if (argv.Length() == 1) {
    TRI_json_t* jsonParameter = TRI_CreateNumberJson(TRI_UNKNOWN_MEM_ZONE, 1);
    if (jsonParameter == NULL) { // failure of some sort
      return scope.Close(v8::ThrowException(v8::String::New("internal error in JS_WherePQConstAql")));       
    }
    TRI_PushBackListJson(TRI_UNKNOWN_MEM_ZONE, parameterList, jsonParameter);
  }
  
  else {
    for (int j = 1; j < argv.Length(); ++j) {  
      v8::Handle<v8::Value> parameter = argv[j];
      TRI_json_t* jsonParameter = ConvertHelper(parameter);
      if (jsonParameter == NULL) { // NOT the null json value! 
        return scope.Close(v8::ThrowException(v8::String::New("type value not currently supported for priority queue index")));       
      }
      TRI_PushBackListJson(TRI_UNKNOWN_MEM_ZONE, parameterList, jsonParameter);
    }
  }  

  // build document priority queue access
  TRI_qry_where_t* where = TRI_CreateQueryWherePQConstant(iid, parameterList);

  // wrap it up
  return scope.Close(WrapWhere(where));
  
}


////////////////////////////////////////////////////////////////////////////////
/// @brief DEPRECATED
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_WhereSkiplistConstAql (const v8::Arguments& argv) {
  v8::HandleScope scope;
  TRI_json_t* parameterList;
  bool haveOperators;
  TRI_qry_where_t* where;
  
  if (argv.Length() < 2) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: AQL_WHERE_SL_CONST(<index-identifier>, <value 1>, <value 2>,..., <value n>)")));
  }

  
  // ..........................................................................
  // check that the first parameter sent is a double value
  // ..........................................................................
  bool inValidType = true;
  TRI_idx_iid_t iid = TRI_ObjectToDouble(argv[0], inValidType); 
  if (inValidType || iid == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("<index-identifier> must be an positive integer")));       
  }  

  // ..........................................................................
  // Do we have logical/relational operators or just constants
  // Only one or the other allowed
  // ..........................................................................
  haveOperators = false;
  for (int j = 1; j < argv.Length(); ++j) {  
    v8::Handle<v8::Value> parameter = argv[j];
    v8::Handle<v8::Object> operatorObject = parameter->ToObject();
    TRI_sl_operator_t* op  = TRI_UnwrapClass<TRI_sl_operator_t>(operatorObject, WRP_SL_OPERATOR_TYPE);
    if (op == 0) {
      if (!haveOperators) {
        continue;
      }
      return scope.Close(v8::ThrowException(v8::String::New("either logical/relational operators or constants allowed, but not both")));
    }
    else {
      if (!haveOperators) {
        haveOperators = true;
      }  
    }
  }  
  
  
  // ..........................................................................
  // We have a list of operators as parameters:
  // If more than one operator, all of the operators will be anded.
  // ..........................................................................  
  if (haveOperators) {    
    if (argv.Length() > 2) {  
      TRI_sl_operator_t* leftOp = NULL;
      v8::Handle<v8::Value> leftParameter  = argv[1];
      v8::Handle<v8::Object> leftObject    = leftParameter->ToObject();
      leftOp  = TRI_UnwrapClass<TRI_sl_operator_t>(leftObject, WRP_SL_OPERATOR_TYPE);
      if (leftOp == 0) {
        return scope.Close(v8::ThrowException(v8::String::New("either logical/relational operators or constants allowed, but not both")));
      }
      
      for (int j = 2; j < argv.Length(); ++j) {  
        v8::Handle<v8::Value> rightParameter = argv[j];
        v8::Handle<v8::Object> rightObject   = rightParameter->ToObject();
        TRI_sl_operator_t* rightOp = TRI_UnwrapClass<TRI_sl_operator_t>(rightObject, WRP_SL_OPERATOR_TYPE);
        if (rightOp == 0) {
          TRI_FreeSLOperator(leftOp); 
          return scope.Close(v8::ThrowException(v8::String::New("either logical/relational operators or constants allowed, but not both")));
        }
        TRI_sl_operator_t* tempAndOperator = CreateSLOperator(TRI_SL_AND_OPERATOR,leftOp, rightOp, NULL, NULL, NULL, 2, NULL);
        leftOp = tempAndOperator;
      }  
      where = TRI_CreateQueryWhereSkiplistConstant(iid, leftOp);
    }
    else {
      v8::Handle<v8::Value> parameter = argv[1];
      v8::Handle<v8::Object> operatorObject = parameter->ToObject();
      TRI_sl_operator_t* op  = TRI_UnwrapClass<TRI_sl_operator_t>(operatorObject, WRP_SL_OPERATOR_TYPE);      
      where = TRI_CreateQueryWhereSkiplistConstant(iid, op);
    }
  }
  
  // ..............................................................................
  // fallback: simple eq operator
  // ..............................................................................
  
  else {
    // ..........................................................................
    // Store the index field parameters in a json object 
    // ..........................................................................
    parameterList = TRI_CreateListJson(TRI_UNKNOWN_MEM_ZONE);
    if (!parameterList) {
      return scope.Close(v8::ThrowException(v8::String::New("out of memory")));
    }

    for (int j = 1; j < argv.Length(); ++j) {  
      v8::Handle<v8::Value> parameter = argv[j];
      TRI_json_t* jsonParameter = ConvertHelper(parameter);
      if (jsonParameter == NULL) { // NOT the null json value! 
        return scope.Close(v8::ThrowException(v8::String::New("type value not currently supported for skiplist index")));       
      }
      TRI_PushBackListJson(TRI_UNKNOWN_MEM_ZONE, parameterList, jsonParameter);
    }
    TRI_sl_operator_t* eqOperator = CreateSLOperator(TRI_SL_EQ_OPERATOR,NULL, NULL, parameterList, NULL, NULL, 
                                                     parameterList->_value._objects._length, NULL);
    where = TRI_CreateQueryWhereSkiplistConstant(iid, eqOperator);
  }

  if (where == NULL) {
    return scope.Close(v8::ThrowException(v8::String::New("Error detected in where statement")));
  }  
  
  // wrap it up
  return scope.Close(WrapWhere(where));  
}

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new constant where clause for geo index - DEPRECATED
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_WhereWithinConstAql (v8::Arguments const& argv) {
  v8::HandleScope scope;

  char* nameDistance;

  if (argv.Length() == 4) {
    nameDistance = 0;
  }
  else if (argv.Length() == 5) {
    v8::String::Utf8Value name(argv[4]);

    if (*name == 0) {
      return scope.Close(v8::ThrowException(v8::String::New("<distance> must be an attribute name")));
    }

    nameDistance = TRI_DuplicateString(*name);
  }
  else {
    return scope.Close(v8::ThrowException(v8::String::New("usage: whereWithinConst(<index-identifier>, <latitude>, <longitude>, <radius>[, <distance>])")));
  }

  // extract the document handle
  TRI_idx_iid_t iid = TRI_ObjectToDouble(argv[0]);
  double latitude = TRI_ObjectToDouble(argv[1]);
  double longitude = TRI_ObjectToDouble(argv[2]);
  double radius = TRI_ObjectToDouble(argv[3]);

  // build document hash access
  TRI_qry_where_t* where = TRI_CreateQueryWhereWithinConstant(iid, nameDistance, latitude, longitude, radius);

  if (nameDistance != 0) {
    TRI_FreeString(TRI_UNKNOWN_MEM_ZONE, nameDistance);
  }

  // wrap it up
  return scope.Close(WrapWhere(where));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new query from given parts - DEPRECATED
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_HashSelectAql (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 2) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: AQL_SELECT(collection, where)")));
  }

  // ...........................................................................
  // extract the primary collection
  // ...........................................................................

  v8::Handle<v8::Value> collectionArg = argv[0];

  if (! collectionArg->IsObject()) {
    return scope.Close(v8::ThrowException(v8::String::New("expecting a COLLECTION as second argument")));
  }
  v8::Handle<v8::Object> collectionObj = collectionArg->ToObject();

  v8::Handle<v8::Object> err;
  const TRI_vocbase_col_t* collection = UseCollection(collectionObj, &err);

  if (collection == 0) {
    return scope.Close(v8::ThrowException(err));
  }
  
  // ...........................................................................
  // Extract there hash where clause
  // ...........................................................................

  v8::Handle<v8::Value> whereArg = argv[1];

  if (whereArg->IsNull()) {
    ReleaseCollection(collection);
    return scope.Close(v8::ThrowException(v8::String::New("expecting a WHERE object as third argument")));
  }

  TRI_qry_where_t* where = 0;
  v8::Handle<v8::Object> whereObj = whereArg->ToObject();
  where = TRI_UnwrapClass<TRI_qry_where_t>(whereObj, WRP_QRY_WHERE_TYPE);

  if (where == 0) {
    ReleaseCollection(collection);
    return scope.Close(v8::ThrowException(v8::String::New("corrupted WHERE")));
  }

  // ...........................................................................
  // Create the hash query 
  // ...........................................................................

  TRI_query_t* query = TRI_CreateHashQuery(where, collection->_collection); 

  if (!query) {
    ReleaseCollection(collection);
    return scope.Close(v8::ThrowException(v8::String::New("could not create query object")));
  }

  // ...........................................................................
  // wrap it up
  // ...........................................................................

  ReleaseCollection(collection);
  return scope.Close(WrapQuery(query));
}





static v8::Handle<v8::Value> JS_PQSelectAql (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 2) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: AQL_PQ_SELECT(collection, where)")));
  }

  // ...........................................................................
  // extract the primary collection
  // ...........................................................................
  v8::Handle<v8::Value> collectionArg = argv[0];
  if (! collectionArg->IsObject()) {
    return scope.Close(v8::ThrowException(v8::String::New("expecting a COLLECTION as first argument")));
  }
  v8::Handle<v8::Object> collectionObj = collectionArg->ToObject();

  v8::Handle<v8::Object> err;
  const TRI_vocbase_col_t* collection = UseCollection(collectionObj, &err);

  if (collection == 0) {
    return scope.Close(v8::ThrowException(err));
  }
  
  // ...........................................................................
  // Extract the where clause
  // ...........................................................................
  v8::Handle<v8::Value> whereArg = argv[1];
  TRI_qry_where_t* where = 0;
  if (whereArg->IsNull()) {
    ReleaseCollection(collection);
    return scope.Close(v8::ThrowException(v8::String::New("expecting a WHERE object as second argument")));
  }
  v8::Handle<v8::Object> whereObj = whereArg->ToObject();
  where = TRI_UnwrapClass<TRI_qry_where_t>(whereObj, WRP_QRY_WHERE_TYPE);
  if (where == 0) {
    ReleaseCollection(collection);
    return scope.Close(v8::ThrowException(v8::String::New("corrupted WHERE")));
  }
    
  // ...........................................................................
  // Check the operators
  // ...........................................................................
  TRI_qry_where_priorityqueue_const_t* pqWhere = (TRI_qry_where_priorityqueue_const_t*)(where);
  TRI_priorityqueue_index_t* idx               = (TRI_priorityqueue_index_t*)(TRI_LookupIndex(collection->_collection, pqWhere->_iid));
  if (idx == NULL) {
    ReleaseCollection(collection);
    return scope.Close(v8::ThrowException(v8::String::New("invalid index in where statement")));
  }
  
  // ...........................................................................
  // Create the skiplist query 
  // ...........................................................................
  TRI_query_t* query = TRI_CreatePriorityQueueQuery(where, collection->_collection); 
  if (!query) {
    ReleaseCollection(collection);
    return scope.Close(v8::ThrowException(v8::String::New("could not create query object")));
  }


  // ...........................................................................
  // wrap it up
  // ...........................................................................

  ReleaseCollection(collection);
  return scope.Close(WrapQuery(query));
}



////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new query from given parts - DEPRECATED
////////////////////////////////////////////////////////////////////////////////

static bool CheckWhereSkiplistOperators(size_t fieldCount, TRI_sl_operator_t* slOperator) {
  TRI_sl_logical_operator_t*  logicalOperator;
  TRI_sl_relation_operator_t* relationOperator;
  bool ok = false;
  
  logicalOperator  = (TRI_sl_logical_operator_t*)(slOperator);
  relationOperator = (TRI_sl_relation_operator_t*)(slOperator);
  
  switch (slOperator->_type) {
    case TRI_SL_EQ_OPERATOR: 
    case TRI_SL_NE_OPERATOR: 
    case TRI_SL_LE_OPERATOR: 
    case TRI_SL_LT_OPERATOR: 
    case TRI_SL_GE_OPERATOR: 
    case TRI_SL_GT_OPERATOR: 
    {
      ok = (relationOperator->_numFields <= fieldCount);
      break;
    }
    case TRI_SL_AND_OPERATOR: 
    case TRI_SL_OR_OPERATOR: 
    {
      ok = (CheckWhereSkiplistOperators(fieldCount,logicalOperator->_left) &&
           CheckWhereSkiplistOperators(fieldCount,logicalOperator->_right));
      break;           
    }
    case TRI_SL_NOT_OPERATOR: 
    {
      ok = (CheckWhereSkiplistOperators(fieldCount,logicalOperator->_left));
      break;           
    }
  }
  return ok;
}

static v8::Handle<v8::Value> JS_SkiplistSelectAql (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 2) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: AQL_SL_SELECT(collection, where)")));
  }

  // ...........................................................................
  // extract the primary collection
  // ...........................................................................
  v8::Handle<v8::Value> collectionArg = argv[0];
  if (! collectionArg->IsObject()) {
    return scope.Close(v8::ThrowException(v8::String::New("expecting a COLLECTION as first argument")));
  }
  v8::Handle<v8::Object> collectionObj = collectionArg->ToObject();

  v8::Handle<v8::Object> err;
  const TRI_vocbase_col_t* collection = UseCollection(collectionObj, &err);

  if (collection == 0) {
    return scope.Close(v8::ThrowException(err));
  }
  
  // ...........................................................................
  // Extract the where clause
  // ...........................................................................
  v8::Handle<v8::Value> whereArg = argv[1];
  TRI_qry_where_t* where = 0;
  if (whereArg->IsNull()) {
    ReleaseCollection(collection);
    return scope.Close(v8::ThrowException(v8::String::New("expecting a WHERE object as second argument")));
  }
  v8::Handle<v8::Object> whereObj = whereArg->ToObject();
  where = TRI_UnwrapClass<TRI_qry_where_t>(whereObj, WRP_QRY_WHERE_TYPE);
  if (where == 0) {
    ReleaseCollection(collection);
    return scope.Close(v8::ThrowException(v8::String::New("corrupted WHERE")));
  }
    
  // ...........................................................................
  // Check the operators
  // ...........................................................................
  TRI_qry_where_skiplist_const_t* slWhere = (TRI_qry_where_skiplist_const_t*)(where);
  TRI_skiplist_index_t* idx               = (TRI_skiplist_index_t*)(TRI_LookupIndex(collection->_collection, slWhere->_iid));
  if (idx == NULL) {
    ReleaseCollection(collection);
    return scope.Close(v8::ThrowException(v8::String::New("invalid index in where statement")));
  }
  if (! CheckWhereSkiplistOperators(idx->_paths._length, slWhere->_operator)) {
    ReleaseCollection(collection);
    return scope.Close(v8::ThrowException(v8::String::New("One or more operators has invalid number of attributes")));
  }  
  
  // ...........................................................................
  // Create the skiplist query 
  // ...........................................................................
  TRI_query_t* query = TRI_CreateSkiplistQuery(where, collection->_collection); 
  if (!query) {
    ReleaseCollection(collection);
    return scope.Close(v8::ThrowException(v8::String::New("could not create query object")));
  }

  // ...........................................................................
  // wrap it up
  // ...........................................................................

  ReleaseCollection(collection);
  return scope.Close(WrapQuery(query));
}

////////////////////////////////////////////////////////////////////////////////
// SKIP LIST OPERATOR functions
////////////////////////////////////////////////////////////////////////////////

static void WeakSLOperatorCallback(v8::Persistent<v8::Value> object, void* parameter) {
  TRI_sl_operator_t* slOperator;
  TRI_v8_global_t* v8g;

  v8g         = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();
  slOperator  = (TRI_sl_operator_t*)(parameter);
  
  LOG_TRACE("weak-callback for query operators called");

  // find the persistent handle 
  v8::Persistent<v8::Value> persistent = v8g->JSOperators[slOperator];
  
  // remove it from the associative map
  v8g->JSOperators.erase(slOperator);

  // dispose and clear the persistent handle
  persistent.Dispose();
  persistent.Clear();

  // and free the left and right operand -- depends on the type
  TRI_FreeSLOperator(slOperator);
}

static v8::Handle<v8::Object> WrapSLOperator (TRI_sl_operator_t* slOperator) {
  TRI_v8_global_t* v8g;

  v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();

  v8::Handle<v8::Object> operatorObject = v8g->SLOperatorTempl->NewInstance();
  map< void*, v8::Persistent<v8::Value> >::iterator i = v8g->JSOperators.find(slOperator);

  if (i == v8g->JSOperators.end()) {
    v8::Persistent<v8::Value> persistent = v8::Persistent<v8::Value>::New(v8::External::New(slOperator));
    operatorObject->SetInternalField(SLOT_CLASS_TYPE, v8::Integer::New(WRP_SL_OPERATOR_TYPE));
    operatorObject->SetInternalField(SLOT_CLASS, persistent);
    v8g->JSCursors[slOperator] = persistent;
    persistent.MakeWeak(slOperator, WeakSLOperatorCallback);
  }
  else {
    operatorObject->SetInternalField(SLOT_CLASS_TYPE, v8::Integer::New(WRP_SL_OPERATOR_TYPE));
    operatorObject->SetInternalField(SLOT_CLASS, i->second);
  }

  return operatorObject;
}

static TRI_json_t* parametersToJson(v8::Arguments const& argv, int startPos, int endPos) {
  TRI_json_t* result = TRI_CreateListJson(TRI_UNKNOWN_MEM_ZONE);
  
  if (result == NULL) {
    v8::ThrowException(v8::String::New("out of memory"));
    return NULL;
  }

  for (int j = startPos; j < endPos; ++j) {  
    v8::Handle<v8::Value> parameter = argv[j];
    TRI_json_t* jsonParameter = ConvertHelper(parameter);
    if (jsonParameter == NULL) { // NOT the null json value! 
      v8::ThrowException(v8::String::New("type value not currently supported for skiplist index"));       
      return NULL;
    }
    TRI_PushBackListJson(TRI_UNKNOWN_MEM_ZONE, result, jsonParameter);
  }
  return result;
}

////////////////////////////////////////////////////////////////////////////////
// Extracts one or more parameters which are values for the skip list index 
// fields previously defined in the skip list index -- in the same order
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_Operator_AND (v8::Arguments const& argv) {
  v8::HandleScope scope;
  TRI_sl_logical_operator_t* logicalOperator;
  
  // ...........................................................................
  // We expect a list of constant values in the order in which the skip list
  // index has been defined. An unknown value can have a NULL
  // ...........................................................................  
  if (argv.Length() != 2) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: AND(<value 1>, <value 2>)")));
  }
  
  // ...........................................................................
  // We expect a two parameters AND(<left operator>,<right operator>)
  // ...........................................................................
  v8::Handle<v8::Value> leftOperatorArg  = argv[0];
  v8::Handle<v8::Value> rightOperatorArg = argv[1];
  
  if (leftOperatorArg->IsNull()) {    
    return scope.Close(v8::ThrowException(v8::String::New("expecting a relational or logical operator as first argument")));
  }
  if (rightOperatorArg->IsNull()) {
    return scope.Close(v8::ThrowException(v8::String::New("expecting a relational or logical operator as second argument")));
  }


  // ...........................................................................
  // Extract the left and right operands from the context
  // ...........................................................................
  v8::Handle<v8::Object> leftOperatorObject  = leftOperatorArg->ToObject();
  v8::Handle<v8::Object> rightOperatorObject = rightOperatorArg->ToObject();
  TRI_sl_operator_t* leftOperator  = TRI_UnwrapClass<TRI_sl_operator_t>(leftOperatorObject, WRP_SL_OPERATOR_TYPE);
  TRI_sl_operator_t* rightOperator = TRI_UnwrapClass<TRI_sl_operator_t>(rightOperatorObject, WRP_SL_OPERATOR_TYPE);  
  if (leftOperator == 0 || rightOperator == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("corrupted AND, possibly invalid parameters")));
  }

  // ...........................................................................
  // Allocate the storage for a logial (AND) operator and assign it that type
  // ...........................................................................  
  logicalOperator = (TRI_sl_logical_operator_t*)(CreateSLOperator(TRI_SL_AND_OPERATOR,
                                                                  CopySLOperator(leftOperator),
                                                                  CopySLOperator(rightOperator),NULL, NULL, NULL, 2, NULL));
  // ...........................................................................
  // Wrap it up for later use and return.
  // ...........................................................................
  return scope.Close(WrapSLOperator(&(logicalOperator->_base)));
}


static v8::Handle<v8::Value> JS_Operator_OR (v8::Arguments const& argv) {
  v8::HandleScope scope;
  TRI_sl_logical_operator_t* logicalOperator;
  
  // ...........................................................................
  // We expect a list of constant values in the order in which the skip list
  // index has been defined. An unknown value can have a NULL
  // ...........................................................................  
  if (argv.Length() != 2) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: OR(<value 1>, <value 2>)")));
  }

  
  // ...........................................................................
  // We expect a two parameters AND(<left operator>,<right operator>)
  // ...........................................................................
  v8::Handle<v8::Value> leftOperatorArg  = argv[0];
  v8::Handle<v8::Value> rightOperatorArg = argv[1];
  
  if (leftOperatorArg->IsNull()) {
    return scope.Close(v8::ThrowException(v8::String::New("expecting a relational or logical operator as first argument")));
  }
  if (rightOperatorArg->IsNull()) {
    return scope.Close(v8::ThrowException(v8::String::New("expecting a relational or logical operator as second argument")));
  }


  
  v8::Handle<v8::Object> leftOperatorObject  = leftOperatorArg->ToObject();
  v8::Handle<v8::Object> rightOperatorObject = rightOperatorArg->ToObject();
  TRI_sl_operator_t* leftOperator  = TRI_UnwrapClass<TRI_sl_operator_t>(leftOperatorObject, WRP_SL_OPERATOR_TYPE);
  TRI_sl_operator_t* rightOperator = TRI_UnwrapClass<TRI_sl_operator_t>(rightOperatorObject, WRP_SL_OPERATOR_TYPE);
  
  if (leftOperator == 0 || rightOperator == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("corrupted OR, possibly invalid parameters")));
  }
  

  // ...........................................................................
  // Allocate the storage for a logial (AND) operator and assign it that type
  // ...........................................................................  
  logicalOperator = (TRI_sl_logical_operator_t*)(CreateSLOperator(TRI_SL_OR_OPERATOR,
                                                                  CopySLOperator(leftOperator),
                                                                  CopySLOperator(rightOperator),NULL, NULL, NULL, 2, NULL));
  
  return scope.Close(WrapSLOperator(&(logicalOperator->_base)));
}


static v8::Handle<v8::Value> JS_Operator_EQ (v8::Arguments const& argv) {
  v8::HandleScope scope;
  TRI_sl_relation_operator_t* relationOperator;
  TRI_json_t* parameters;
  
  // ...........................................................................
  // We expect a list of constant values in the order in which the skip list
  // index has been defined. An unknown value can have a NULL
  // ...........................................................................  
  if (argv.Length() < 1) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: EQ(<value 1>, <value 2>,..., <value n>)")));
  }

  
  // ...........................................................................
  // We expect a two parameters EQ("a.b.c",23.32)
  // the first parameter is the name of the field we wish to ensure equality 
  // with the value of 23.32.
  // ...........................................................................
  
  parameters = parametersToJson(argv,0,argv.Length());
  if (parameters == NULL) {
    return scope.Close(v8::ThrowException(v8::String::New("unsupported type in EQ(...) parameter list")));
  }

  // ...........................................................................
  // Allocate the storage for a relation (EQ) operator and assign it that type
  // ...........................................................................  
  relationOperator = (TRI_sl_relation_operator_t*)(CreateSLOperator(TRI_SL_EQ_OPERATOR, NULL, NULL, parameters, NULL, NULL, 
                                                    parameters->_value._objects._length, NULL));
  
  return scope.Close(WrapSLOperator(&(relationOperator->_base)));
}

static v8::Handle<v8::Value> JS_Operator_GE (v8::Arguments const& argv) {
  v8::HandleScope scope;
  TRI_sl_relation_operator_t* relationOperator;
  TRI_json_t* parameters;
  
  // ...........................................................................
  // We expect a list of constant values in the order in which the skip list
  // index has been defined. An unknown value can have a NULL
  // ...........................................................................  
  if (argv.Length() < 1) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: GE(<value 1>, <value 2>,..., <value n>)")));
  }

  
  // ...........................................................................
  // We expect a two parameters GE("abc",23.32)
  // the first parameter is the name of the field we wish to ensure equality 
  // with the value of 23.32.
  // ...........................................................................
  
  parameters = parametersToJson(argv,0,argv.Length());
  if (parameters == NULL) {
    return scope.Close(v8::ThrowException(v8::String::New("unsupported type in GE(...) parameter list")));
  }
  
  // ...........................................................................
  // Allocate the storage for a relation (GE) operator and assign it that type
  // ...........................................................................  
  relationOperator = (TRI_sl_relation_operator_t*)( CreateSLOperator(TRI_SL_GE_OPERATOR, NULL, NULL, parameters, NULL, NULL, 
                                                     parameters->_value._objects._length, NULL) );
  
  return scope.Close(WrapSLOperator(&(relationOperator->_base)));
}


static v8::Handle<v8::Value> JS_Operator_GT (v8::Arguments const& argv) {
  v8::HandleScope scope;
  TRI_sl_relation_operator_t* relationOperator;
  TRI_json_t* parameters;
  
  // ...........................................................................
  // We expect a list of constant values in the order in which the skip list
  // index has been defined. An unknown value can have a NULL
  // ...........................................................................  
  if (argv.Length() < 1) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: GT(<value 1>, <value 2>,..., <value n>)")));
  }

  
  // ...........................................................................
  // We expect a two parameters GT("abc",23.32)
  // the first parameter is the name of the field we wish to ensure equality 
  // with the value of 23.32.
  // ...........................................................................
  
  parameters = parametersToJson(argv,0,argv.Length());
  if (parameters == NULL) {
    return scope.Close(v8::ThrowException(v8::String::New("unsupported type in GT(...) parameter list")));
  }
  
  // ...........................................................................
  // Allocate the storage for a relation (GT) operator and assign it that type
  // ...........................................................................  
  relationOperator = (TRI_sl_relation_operator_t*)( CreateSLOperator(TRI_SL_GT_OPERATOR, NULL, NULL, parameters, NULL, NULL, 
                                                    parameters->_value._objects._length, NULL) );
  
  return scope.Close(WrapSLOperator(&(relationOperator->_base)));
}


static v8::Handle<v8::Value> JS_Operator_LE (v8::Arguments const& argv) {
  v8::HandleScope scope;
  TRI_sl_relation_operator_t* relationOperator;
  TRI_json_t* parameters;
  
  // ...........................................................................
  // We expect a list of constant values in the order in which the skip list
  // index has been defined. An unknown value can have a NULL
  // ...........................................................................  
  if (argv.Length() < 1) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: LE(<value 1>, <value 2>,..., <value n>)")));
  }

  
  // ...........................................................................
  // We expect a two parameters LE("field value",23.32)
  // the first parameter is the name of the field we wish to ensure equality 
  // with the value of 23.32.
  // ...........................................................................
  
  parameters = parametersToJson(argv,0,argv.Length());
  if (parameters == NULL) {
    return scope.Close(v8::ThrowException(v8::String::New("unsupported type in LE(...) parameter list")));
  }

  // ...........................................................................
  // Allocate the storage for a relation (LE) operator and assign it that type
  // ...........................................................................  
  relationOperator = (TRI_sl_relation_operator_t*)( CreateSLOperator(TRI_SL_LE_OPERATOR, NULL, NULL,parameters, NULL, NULL, 
                                                     parameters->_value._objects._length, NULL) );  
  
  return scope.Close(WrapSLOperator(&(relationOperator->_base)));
}


static v8::Handle<v8::Value> JS_Operator_LT (v8::Arguments const& argv) {
  v8::HandleScope scope;
  TRI_sl_relation_operator_t* relationOperator;
  TRI_json_t* parameters;
  
  // ...........................................................................
  // We expect a list of constant values in the order in which the skip list
  // index has been defined. An unknown value can have a NULL
  // ...........................................................................  
  if (argv.Length() < 1) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: LT(<value 1>, <value 2>,..., <value n>)")));
  }

    
  // ...........................................................................
  // We expect a two parameters LT("field_string_value",23.32)
  // the first parameter is the name of the field we wish to ensure equality 
  // with the value of 23.32.
  // ...........................................................................
  
  parameters = parametersToJson(argv,0,argv.Length());
  if (parameters == NULL) {
    return scope.Close(v8::ThrowException(v8::String::New("unsupported type in LT(...) parameter list")));
  }
  
  // ...........................................................................
  // Allocate the storage for a relation (LT) operator and assign it that type
  // ...........................................................................  
  relationOperator = (TRI_sl_relation_operator_t*)( CreateSLOperator(TRI_SL_LT_OPERATOR, NULL,NULL,parameters, NULL, NULL, 
                                                    parameters->_value._objects._length, NULL) );
  
  return scope.Close(WrapSLOperator(&(relationOperator->_base)));
}


////////////////////////////////////////////////////////////////////////////////
/// @brief executes a select query - DEPRECATED
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_SelectAql (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 4) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: AQL_SELECT(<db>, <collectionname>, <skip>, <limit>)")));
  }

  v8::Handle<v8::Object> dbArg = argv[0]->ToObject();
  TRI_vocbase_t* vocbase = TRI_UnwrapClass<TRI_vocbase_t>(dbArg, WRP_VOCBASE_TYPE);

  if (vocbase == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("corrupted vocbase")));
  }

  // select clause
  TRI_qry_select_t* select;
  select = TRI_CreateQuerySelectDocument();

  v8::Handle<v8::Value> nameArg = argv[1];
  string name = TRI_ObjectToString(nameArg);

  if (name.empty()) {
    select->free(select);
    return scope.Close(v8::ThrowException(v8::String::New("expecting a non-empty name for <collectionname>")));
  }

  // extract the skip value
  TRI_voc_size_t skip = 0;
  v8::Handle<v8::Value> skipArg = argv[2];

  if (skipArg->IsNull()) {
    skip = TRI_QRY_NO_SKIP;
  }
  else {
    double s = TRI_ObjectToDouble(skipArg);

    if (s < 0.0) {
      skip = 0;
    }
    else {
      skip = (TRI_voc_size_t) s;
    }
  }

  // extract the limit value
  TRI_voc_ssize_t limit = 0;
  v8::Handle<v8::Value> limitArg = argv[3];

  if (limitArg->IsNull()) {
    limit = TRI_QRY_NO_LIMIT;
  }
  else {
    double l = TRI_ObjectToDouble(limitArg);

    limit = (TRI_voc_ssize_t) l;
  }

  TRI_select_join_t* join;
  join = TRI_CreateSelectJoin();
  if (!join) {
    select->free(select);
    return scope.Close(v8::ThrowException(v8::String::New("could not create join struct")));
  }
  
  TRI_AddPartSelectJoinX(join, 
                        JOIN_TYPE_PRIMARY, 
                        NULL,
                        (char*) name.c_str(), 
                        (char*) "alias",
                        NULL);

  // create the query
  TRI_query_t* query = TRI_CreateQuery(vocbase,
                                       select,
                                       NULL,
                                       join,
                                       skip,
                                       limit); 

  if (!query) {
    select->free(select);
    return scope.Close(v8::ThrowException(v8::String::New("could not create query object")));
  }

  // wrap it up
  return scope.Close(WrapQuery(query));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a query - DEPRECATED
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_ExecuteAql (v8::Arguments const& argv) {
  v8::HandleScope scope;

  v8::Handle<v8::Value> err;
  TRI_rc_cursor_t* cursor = ExecuteQuery(argv.Holder(), &err);

  if (cursor == 0) {
    return v8::ThrowException(err);
  }

  return scope.Close(WrapCursor(cursor));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts a cursor from a javascript object - DEPRECATED
////////////////////////////////////////////////////////////////////////////////

static TRI_rc_cursor_t* UnwrapCursor (v8::Handle<v8::Object> cursorObject) {
  return TRI_UnwrapClass<TRI_rc_cursor_t>(cursorObject, WRP_RC_CURSOR_TYPE);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the next document - DEPRECATED
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_CountCursor (v8::Arguments const& argv) {
  v8::HandleScope scope;
  v8::TryCatch tryCatch;

  if (argv.Length() != 0) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: count()")));
  }

  v8::Handle<v8::Object> self = argv.Holder();
  TRI_rc_cursor_t* cursor = UnwrapCursor(self);

  if (cursor == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("corrupted cursor")));
  }

  
  return scope.Close(v8::Number::New(cursor->_matchedDocuments));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the next document - DEPRECATED
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_NextCursor (v8::Arguments const& argv) {
  v8::HandleScope scope;
  v8::TryCatch tryCatch;

  if (argv.Length() != 0) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: next()")));
  }

  v8::Handle<v8::Object> self = argv.Holder();
  TRI_rc_cursor_t* cursor = UnwrapCursor(self);

  if (cursor == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("corrupted cursor")));
  }

  TRI_rc_result_t* next = cursor->next(cursor);

  if (next == 0) {
    return scope.Close(v8::Undefined());
  }

  v8::Handle<v8::Value> value;
  bool ok;

  TRI_qry_select_t* select = cursor->_select;
  if (select) {
    ok = select->toJavaScript(select, next, (void*) &value);
  }
  else {
    TRI_DefineSelectExecutionContext(cursor->_selectContext, next);
    ok = TRI_ExecuteExecutionContext(cursor->_selectContext, (void*) &value);
  }

  if (! ok) {
    if (tryCatch.HasCaught()) {
      return scope.Close(v8::ThrowException(tryCatch.Exception()));
    }
    else {
      return scope.Close(v8::ThrowException(v8::String::New("cannot convert to JavaScript")));
    }
  }

  return scope.Close(value);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the next document reference - DEPRECATED
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_NextRefCursor (v8::Arguments const& argv) {
  v8::HandleScope scope;
  v8::TryCatch tryCatch;

  if (argv.Length() != 0) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: nextRef()")));
  }

  v8::Handle<v8::Object> self = argv.Holder();
  TRI_rc_cursor_t* cursor = UnwrapCursor(self);

  if (cursor == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("corrupted cursor")));
  }

  TRI_rc_result_t* next = cursor->next(cursor);

  if (next == 0) {
    return scope.Close(v8::Undefined());
  }

  // always use the primary collection
  TRI_voc_cid_t cid = cursor->_context->_primary->base._cid;
  TRI_voc_did_t did = next->_primary->_did;
  string ref = StringUtils::itoa(cid) + TRI_DOCUMENT_HANDLE_SEPARATOR_STR + StringUtils::itoa(did);

  return scope.Close(v8::String::New(ref.c_str()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief uses the next document - DEPRECATED
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_UseNextCursor (v8::Arguments const& argv) {
  v8::HandleScope scope;
  v8::TryCatch tryCatch;

  if (argv.Length() != 0) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: nextRef()")));
  }

  v8::Handle<v8::Object> self = argv.Holder();
  TRI_rc_cursor_t* cursor = UnwrapCursor(self);

  if (cursor == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("corrupted cursor")));
  }

  TRI_rc_result_t* next = cursor->next(cursor);

  if (next == 0) {
    return scope.Close(v8::Undefined());
  }

  return scope.Close(v8::True());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if the cursor is exhausted - DEPRECATED
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_HasNextCursor (v8::Arguments const& argv) {
  v8::HandleScope scope;

  if (argv.Length() != 0) {
    return scope.Close(v8::ThrowException(v8::String::New("usage: hasNext()")));
  }

  v8::Handle<v8::Object> self = argv.Holder();
  TRI_rc_cursor_t* cursor = UnwrapCursor(self);

  if (cursor == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("corrupted cursor")));
  }

  if (cursor->hasNext(cursor)) {
    return scope.Close(v8::True());
  }
  else {
    return scope.Close(v8::False());
  }
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
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_CountVocbaseCol (v8::Arguments const& argv) {
  v8::HandleScope scope;

  v8::Handle<v8::Object> err;
  TRI_vocbase_col_t const* collection = UseCollection(argv.Holder(), &err);

  if (collection == 0) {
    return scope.Close(v8::ThrowException(err));
  }

  TRI_doc_collection_t* doc = collection->_collection;
  size_t s = doc->size(doc);

  ReleaseCollection(collection);
  return scope.Close(v8::Number::New((double) s));
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
/// @FUN{@FA{collection}.remove(@FA{document-handle}, @FA{data})}
///
/// As before. Instead of document a @FA{document-handle} can be passed as
/// first argument.
///
/// @EXAMPLES
///
/// Delete a document:
///
/// @verbinclude shell_remove-document
///
/// Delete a document with a conflict:
///
/// @verbinclude shell_remove-document-conflict
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
/// @brief looks up a document
///
/// @FUN{@FA{collection}.document(@FA{document})}
///
/// The @FN{document} method finds a document given it's identifier.  It
/// returns the document. Note that the returned docuement contains two
/// pseudo-attributes, namely @LIT{_id} and @LIT{_rev}. @LIT{_id}
/// contains the @FA{docuement-handle} and @LIT{_rev} the revision of
/// the document.
///
/// An error is thrown if there @LIT{_rev} does not longer match the current
/// revision of the document.
///
/// An error is thrown if the document does not exists.
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
/// Return the document for a document-handle:
///
/// @verbinclude shell_read-document
///
/// An error is raised if the document is unknown:
///
/// @verbinclude shell_read-document-not-found
///
/// An error is raised if the handle is invalid:
///
/// @verbinclude shell_read-document-bad-handle
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

  return DocumentVocbaseCol(collection->_vocbase, collection, argv);
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
    return scope.Close(v8::ThrowException(CreateErrorObject(res, "cannot drop collection")));
  }

  return scope.Close(v8::Undefined());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief drops an index
///
/// @FUN{@FA{collection}.dropIndex(@FA{index})}
///
/// Drops the index. If the index does not exists, then @LIT{false} is
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

  TRI_doc_collection_t* doc = collection->_collection;

  if (doc->base._type != TRI_COL_TYPE_SIMPLE_DOCUMENT) {
    ReleaseCollection(collection);
    return scope.Close(v8::ThrowException(CreateErrorObject(TRI_ERROR_INTERNAL, "unknown collection type")));
  }

  TRI_sim_collection_t* sim = (TRI_sim_collection_t*) doc;

  if (argv.Length() != 1) {
    ReleaseCollection(collection);
    return scope.Close(v8::ThrowException(CreateErrorObject(TRI_ERROR_ILLEGAL_OPTION, "usage: dropIndex(<index-handle>)")));
  }

  TRI_index_t* idx = LookupIndexByHandle(doc->base._vocbase, collection, argv[0], true, &err);

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

  bool ok = TRI_DropIndexSimCollection(sim, idx->_iid);

  // .............................................................................
  // outside a write transaction
  // .............................................................................

  ReleaseCollection(collection);
  return scope.Close(ok ? v8::True() : v8::False());
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
/// @brief ensures that a hash index exists
///
/// @FUN{ensureUniqueConstrain(@FA{field1}, @FA{field2}, ...,@FA{fieldn})}
///
/// Creates a hash index on all documents using attributes as paths to the
/// fields. At least one attribute must be given. The value of this attribute
/// must be a list. All documents, which do not have the attribute path or where
/// one or more values that are not suitable, are ignored.
///
/// In case that the index was successfully created, the index identifier
/// is returned.
///
/// @EXAMPLES
///
/// @verbinclude admin5
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_EnsureUniqueConstraintVocbaseCol (v8::Arguments const& argv) {
  return EnsureHashSkipListIndex("ensureUniqueConstrain", argv, true, 0);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that a hash index exists
///
/// @FUN{ensureHashIndex(@FA{field1}, @FA{field2}, ...,@FA{fieldn})}
///
/// Creates a non-unique hash index on all documents using attributes as paths
/// to the fields. At least one attribute must be given. All documents, which do
/// not have the attribute path or with one or more values that are not
/// suitable, are ignored.
///
/// In case that the index was successfully created, the index identifier
/// is returned.
///
/// @verbinclude fluent14
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_EnsureHashIndexVocbaseCol (v8::Arguments const& argv) {
  return EnsureHashSkipListIndex("ensureHashIndex", argv, false, 0);
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

  TRI_doc_collection_t* doc = collection->_collection;
  
  if (doc->base._type != TRI_COL_TYPE_SIMPLE_DOCUMENT) {
    ReleaseCollection(collection);
    return scope.Close(v8::ThrowException(v8::String::New("unknown collection type")));
  }

  TRI_sim_collection_t* sim = (TRI_sim_collection_t*) doc;

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
  
  TRI_vector_t attributes;
  TRI_InitVector(&attributes, TRI_UNKNOWN_MEM_ZONE, sizeof(char*));
  
  bool ok = true;
  
  for (int j = 0; j < argv.Length(); ++j) {
  
    v8::Handle<v8::Value> argument = argv[j];
    if (! argument->IsString() ) {
      errorString = "invalid parameter passed to ensurePQIndex(...) command";
      ok = false;
      break;
    }
    
    // ...........................................................................
    // convert the argument into a "C" string
    // ...........................................................................
    
    v8::String::Utf8Value argumentString(argument);   
    char* cArgument = (char*) (TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, argumentString.length() + 1, false));       
    if (cArgument == NULL) {
      errorString = "insuffient memory to complete ensurePQIndex(...) command";
      ok = false;
      break;
    }  
        
    memcpy(cArgument, *argumentString, argumentString.length());
    TRI_PushBackVector(&attributes,&cArgument);
  }

  // .............................................................................
  // Check that each parameter is unique
  // .............................................................................
  
  for (size_t j = 0; j < attributes._length; ++j) {  
    char* left = *((char**) (TRI_AtVector(&attributes, j)));    
    for (size_t k = j + 1; k < attributes._length; ++k) {
      char* right = *((char**) (TRI_AtVector(&attributes, k)));
      if (strcmp(left,right) == 0) {
        errorString = "duplicate parameters sent to ensurePQIndex(...) command";
        ok = false;
        break;
      }   
    }
  }    
  
  // .............................................................................
  // Some sort of error occurred -- display error message and abort index creation
  // (or index retrieval).
  // .............................................................................
  
  if (!ok) {
    // ...........................................................................
    // Remove the memory allocated to the list of attributes used for the hash index
    // ...........................................................................   
    for (size_t j = 0; j < attributes._length; ++j) {
      char* cArgument = *((char**) (TRI_AtVector(&attributes, j)));
      TRI_Free(TRI_UNKNOWN_MEM_ZONE, cArgument);
    }    
    TRI_DestroyVector(&attributes);

    ReleaseCollection(collection);
    return scope.Close(v8::String::New(errorString.c_str(),errorString.length()));
  }  
  
  // .............................................................................
  // Actually create the index here. Note that priority queue is never unique.
  // .............................................................................

  idx = TRI_EnsurePriorityQueueIndexSimCollection(sim, &attributes, false, &created);

  // .............................................................................
  // Remove the memory allocated to the list of attributes used for the hash index
  // .............................................................................   

  for (size_t j = 0; j < attributes._length; ++j) {
    char* cArgument = *((char**) (TRI_AtVector(&attributes, j)));
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, cArgument);
  }    

  TRI_DestroyVector(&attributes);

  if (idx == NULL) {
    ReleaseCollection(collection);
    return scope.Close(v8::String::New("Priority Queue index could not be created"));
  }  
  
  // .............................................................................
  // Return the newly assigned index identifier
  // .............................................................................

  TRI_json_t* json = idx->json(idx, collection->_collection);

  v8::Handle<v8::Value> index = IndexRep(&collection->_collection->base, json);
  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
  
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
/// @verbinclude fluent14
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_EnsureUniqueSkiplistVocbaseCol (v8::Arguments const& argv) {
  return EnsureHashSkipListIndex("ensureUniqueSkipList", argv, true, 1);
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
  return EnsureHashSkipListIndex("ensureSkipList", argv, false, 1);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the figures of a collection
///
/// @FUN{@FA{collection}.figures()}
///
/// Returns an object containing all collection figures.
///
/// - @LIT{alive.count}: The number of living documents.
///
/// - @LIT{alive.size}: The total size in bytes used by all
///   living documents.
///
/// - @LIT{dead.count}: The number of dead documents.
///
/// - @LIT{dead.size}: The total size in bytes used by all
///   dead documents.
///
/// - @LIT{dead.deletion}: The total number of deletion markers.
///
/// - @LIT{datafiles.count}: The number of active datafiles.
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

  TRI_doc_collection_t* doc = collection->_collection;

  doc->beginRead(doc);
  TRI_doc_collection_info_t* info = doc->figures(doc);
  doc->endRead(doc);

  v8::Handle<v8::Object> alive = v8::Object::New();

  result->Set(v8::String::New("alive"), alive);
  alive->Set(v8::String::New("count"), v8::Number::New(info->_numberAlive));
  alive->Set(v8::String::New("size"), v8::Number::New(info->_sizeAlive));

  v8::Handle<v8::Object> dead = v8::Object::New();

  result->Set(v8::String::New("dead"), dead);
  dead->Set(v8::String::New("count"), v8::Number::New(info->_numberDead));
  dead->Set(v8::String::New("size"), v8::Number::New(info->_sizeDead));
  dead->Set(v8::String::New("deletion"), v8::Number::New(info->_numberDeletion));

  v8::Handle<v8::Object> dfs = v8::Object::New();

  result->Set(v8::String::New("datafiles"), dfs);
  dfs->Set(v8::String::New("count"), v8::Number::New(info->_numberDatafiles));

  TRI_Free(TRI_UNKNOWN_MEM_ZONE, info);

  TRI_READ_UNLOCK_STATUS_VOCBASE_COL(collection);
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

  v8::Handle<v8::Object> err;
  TRI_vocbase_col_t const* collection = UseCollection(argv.Holder(), &err);

  if (collection == 0) {
    return scope.Close(v8::ThrowException(err));
  }

  TRI_doc_collection_t* doc = collection->_collection;

  if (doc->base._type != TRI_COL_TYPE_SIMPLE_DOCUMENT) {
    ReleaseCollection(collection);
    return scope.Close(v8::ThrowException(v8::String::New("unknown collection type")));
  }

  TRI_sim_collection_t* sim = (TRI_sim_collection_t*) doc;

  // get a list of indexes
  TRI_vector_pointer_t* indexes = TRI_IndexesSimCollection(sim);

  if (!indexes) {
    return scope.Close(v8::ThrowException(v8::String::New("out of memory")));
  }

  v8::Handle<v8::Array> result = v8::Array::New();

  uint32_t n = (uint32_t) indexes->_length;

  for (uint32_t i = 0, j = 0;  i < n;  ++i) {
    TRI_json_t* idx = (TRI_json_t*) indexes->_buffer[i];

    if (idx) {
      result->Set(j++, IndexRep(&doc->base, idx));
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, idx);
    }
  }

  ReleaseCollection(collection);

  TRI_FreeVectorPointer(TRI_UNKNOWN_MEM_ZONE, indexes);

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
/// Note that it is not possible to change the journal size after creation.
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

  TRI_doc_collection_t* doc = collection->_collection;

  if (doc->base._type != TRI_COL_TYPE_SIMPLE_DOCUMENT) {
    ReleaseCollection(collection);
    return scope.Close(v8::ThrowException(v8::String::New("unknown collection type")));
  }

  TRI_sim_collection_t* sim = (TRI_sim_collection_t*) doc;

  // check if we want to change some parameters
  if (0 < argv.Length()) {
    v8::Handle<v8::Value> par = argv[0];

    if (par->IsObject()) {
      v8::Handle<v8::Object> po = par->ToObject();

      // holding a lock on the vocbase collection: if we ever want to
      // change the maximal size a real lock is required.
      bool waitForSync = sim->base.base._waitForSync;

      // extract sync after objects
      if (po->Has(v8g->WaitForSyncKey)) {
        waitForSync = TRI_ObjectToBoolean(po->Get(v8g->WaitForSyncKey));
      }

      sim->base.base._waitForSync = waitForSync;

      // try to write new parameter to file
      int res = TRI_UpdateParameterInfoCollection(&sim->base.base);

      if (res != TRI_ERROR_NO_ERROR) {
        ReleaseCollection(collection);
        return scope.Close(v8::ThrowException(v8::String::New(TRI_last_error())));
      }
    }
  }

  // return the current parameter set
  v8::Handle<v8::Object> result = v8::Object::New();

  if (doc->base._type == TRI_COL_TYPE_SIMPLE_DOCUMENT) {
    TRI_voc_size_t maximalSize = sim->base.base._maximalSize;
    bool waitForSync = sim->base.base._waitForSync;

    result->Set(v8g->WaitForSyncKey, waitForSync ? v8::True() : v8::False());
    result->Set(v8g->JournalSizeKey, v8::Number::New(maximalSize));
  }

  ReleaseCollection(collection);
  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief renames a collection
///
/// @FUN{@FA{collection}.rename(@FA{new-name})}
///
/// Renames a collection using the @FA{new-name}. The @FA{new-name} must not
/// already be used for a different collection. If it is an error is thrown.
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
    return scope.Close(v8::ThrowException(CreateErrorObject(res, "cannot rename collection")));
  }

  return scope.Close(v8::Undefined());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief replaces a document
///
/// @FUN{@FA{collection}.replace(@FA{document}, @FA{data})}
///
/// Replaces an existing @FA{document}. The @FA{document} must be a document in
/// the current collection. This document is than replaced with the
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
/// @FUN{@FA{collection}.replace(@FA{document-handle}, @FA{data})}
///
/// As before. Instead of document a @FA{document-handle} can be passed as
/// first argument.
///
/// @EXAMPLES
///
/// Create and update a document:
///
/// @verbinclude shell_update-document
///
/// Use a document handle:
///
/// @verbinclude shell_update-document-handle
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
/// @brief saves a new document
///
/// @FUN{@FA{collection}.save(@FA{data})}
///
/// Creates a new document in the @FA{collection} from the given @FA{data}. The
/// @FA{data} must be an hash array. It must not contain attributes starting
/// with @LIT{_}.
///
/// The method returns a document with the attributes @LIT{_id} and @LIT{_rev}.
/// The attribute @LIT{_id} contains the document handle of the newly created
/// document, the attribute @LIT{_rev} contains the document revision.
///
/// @EXAMPLES
///
/// @verbinclude shell_create-document
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_SaveVocbaseCol (v8::Arguments const& argv) {
  v8::HandleScope scope;
  TRI_v8_global_t* v8g;

  v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();

  v8::Handle<v8::Object> err;
  TRI_vocbase_col_t const* collection = UseCollection(argv.Holder(), &err);

  if (collection == 0) {
    return scope.Close(v8::ThrowException(err));
  }

  TRI_doc_collection_t* doc = collection->_collection;

  if (argv.Length() != 1) {
    ReleaseCollection(collection);
    return scope.Close(v8::ThrowException(
                         CreateErrorObject(TRI_ERROR_BAD_PARAMETER, 
                                           "usage: save(<data>)")));
  }

  TRI_shaped_json_t* shaped = TRI_ShapedJsonV8Object(argv[0], doc->_shaper);

  if (shaped == 0) {
    ReleaseCollection(collection);
    return scope.Close(v8::ThrowException(
                         CreateErrorObject(TRI_errno(),
                                           "<data> cannot be converted into JSON shape")));
  }

  // .............................................................................
  // inside a write transaction
  // .............................................................................

  collection->_collection->beginWrite(collection->_collection);

  // the lock is freed in create
  TRI_doc_mptr_t mptr = doc->create(doc, TRI_DOC_MARKER_DOCUMENT, shaped, 0, true);

  // .............................................................................
  // outside a write transaction
  // .............................................................................

  TRI_FreeShapedJson(doc->_shaper, shaped);

  if (mptr._did == 0) {
    ReleaseCollection(collection);
    return scope.Close(v8::ThrowException(
                         CreateErrorObject(TRI_errno(), 
                                           "cannot save document")));
  }

  string id = StringUtils::itoa(doc->base._cid) + string(TRI_DOCUMENT_HANDLE_SEPARATOR_STR) + StringUtils::itoa(mptr._did);

  v8::Handle<v8::Object> result = v8::Object::New();
  result->Set(v8g->DidKey, v8::String::New(id.c_str()));
  result->Set(v8g->RevKey, v8::Number::New(mptr._rid));

  ReleaseCollection(collection);
  return scope.Close(result);
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
/// @brief unloads a collection
///
/// @FUN{@FA{collection}.unload()}
///
/// Starts unloading a collection into memory. Note that unloading is deferred
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
    return scope.Close(v8::ThrowException(CreateErrorObject(res, "cannot unload collection")));
  }

  return scope.Close(v8::Undefined());
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                 TRI_VOCBASE_COL_T EDGES FUNCTIONS
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                              javascript functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief saves a new document
///
/// @FUN{@FA{edge-collection}.save(@FA{from}, @FA{to}, @FA{document})}
///
/// Saves a new edge and returns the document-handle. @FA{from} and @FA{to}
/// must be documents or document references.
///
/// @verbinclude shell_create-edge
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_SaveEdgesCol (v8::Arguments const& argv) {
  v8::HandleScope scope;
  TRI_v8_global_t* v8g;

  v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();

  v8::Handle<v8::Object> err;
  TRI_vocbase_col_t const* collection = UseCollection(argv.Holder(), &err);

  if (collection == 0) {
    return scope.Close(v8::ThrowException(err));
  }

  TRI_doc_collection_t* doc = collection->_collection;

  if (argv.Length() != 3) {
    ReleaseCollection(collection);
    return scope.Close(v8::ThrowException(
                         CreateErrorObject(TRI_ERROR_BAD_PARAMETER, 
                                           "usage: save(<from>, <to>, <data>)")));
  }

  TRI_sim_edge_t edge;

  edge._fromCid = collection->_cid;
  edge._toCid = collection->_cid;

  v8::Handle<v8::Value> errMsg;

  // extract from
  TRI_vocbase_col_t const* fromCollection = 0;
  TRI_voc_rid_t fromRid;

  errMsg = ParseDocumentOrDocumentHandle(collection->_vocbase, fromCollection, edge._fromDid, fromRid, argv[0]);

  if (! errMsg.IsEmpty()) {
    ReleaseCollection(collection);

    if (fromCollection != 0) {
      ReleaseCollection(fromCollection);
    }

    return scope.Close(v8::ThrowException(errMsg));
  }

  edge._fromCid = fromCollection->_cid;
  ReleaseCollection(fromCollection);

  // extract to
  TRI_vocbase_col_t const* toCollection = 0;
  TRI_voc_rid_t toRid;

  errMsg = ParseDocumentOrDocumentHandle(collection->_vocbase, toCollection, edge._toDid, toRid, argv[1]);

  if (! errMsg.IsEmpty()) {
    ReleaseCollection(collection);

    if (toCollection != 0) {
      ReleaseCollection(toCollection);
    }

    return scope.Close(v8::ThrowException(errMsg));
  }

  edge._toCid = toCollection->_cid;
  ReleaseCollection(toCollection);

  // extract shaped data
  TRI_shaped_json_t* shaped = TRI_ShapedJsonV8Object(argv[2], doc->_shaper);

  if (shaped == 0) {
    ReleaseCollection(collection);
    return scope.Close(v8::ThrowException(
                         CreateErrorObject(TRI_errno(),
                                           "<data> cannot be converted into JSON shape")));
  }

  // .............................................................................
  // inside a write transaction
  // .............................................................................

  collection->_collection->beginWrite(collection->_collection);

  TRI_doc_mptr_t mptr = doc->create(doc, TRI_DOC_MARKER_EDGE, shaped, &edge, true);

  // .............................................................................
  // outside a write transaction
  // .............................................................................

  TRI_FreeShapedJson(doc->_shaper, shaped);

  if (mptr._did == 0) {
    ReleaseCollection(collection);
    return scope.Close(v8::ThrowException(
                         CreateErrorObject(TRI_errno(), 
                                           "cannot save document")));
  }

  string id = StringUtils::itoa(doc->base._cid) + string(TRI_DOCUMENT_HANDLE_SEPARATOR_STR) + StringUtils::itoa(mptr._did);

  v8::Handle<v8::Object> result = v8::Object::New();
  result->Set(v8g->DidKey, v8::String::New(id.c_str()));
  result->Set(v8g->RevKey, v8::Number::New(mptr._rid));

  ReleaseCollection(collection);
  return scope.Close(result);
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
    return scope.Close(v8::ThrowException(CreateErrorObject(TRI_ERROR_AVOCADO_ILLEGAL_NAME, "name must not be empty")));
  }

  if (   key == "toString"
      || key == "toJSON"
      || key == "hasOwnProperty"
      || key[0] == '_') {
    return v8::Handle<v8::Value>();
  }

  // look up the value if it exists
  TRI_vocbase_col_t const* collection = TRI_FindCollectionByNameVocBase(vocbase, key.c_str(), true);

  // if the key is not present return an empty handle as signal
  if (collection == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("cannot load or create collection")));
  }

  if (collection->_type != TRI_COL_TYPE_SIMPLE_DOCUMENT) {
    return scope.Close(v8::ThrowException(v8::String::New("collection is not an document collection")));
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

static v8::Handle<v8::Value> JS_CollectionVocBase (v8::Arguments const& argv) {
  return CollectionVocBase(argv, false);
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

static v8::Handle<v8::Value> JS_CollectionsVocBase (v8::Arguments const& argv) {
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

static v8::Handle<v8::Value> JS_CompletionsVocBase (v8::Arguments const& argv) {
  v8::HandleScope scope;
  v8::Handle<v8::Array> result = v8::Array::New();

  TRI_vocbase_t* vocbase = TRI_UnwrapClass<TRI_vocbase_t>(argv.Holder(), WRP_VOCBASE_TYPE);

  if (vocbase == 0) {
    return scope.Close(result);
  }

  TRI_vector_pointer_t colls = TRI_CollectionsVocBase(vocbase);

  uint32_t n = (uint32_t) colls._length;
  for (uint32_t i = 0;  i < n;  ++i) {
    TRI_vocbase_col_t const* collection = (TRI_vocbase_col_t const*) colls._buffer[i];

    result->Set(i, v8::String::New(collection->_name));
  }

  TRI_DestroyVectorPointer(&colls);

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new collection
///
/// @FUN{db._create(@FA{collection-name})}
///
/// Creates a new collection named @FA{collection-name}. If the collection name
/// already exists, than an error is thrown. The default value for
/// @LIT{waitForSync} is @LIT{false}.
///
/// @FUN{db._create(@FA{collection-name}, @FA{properties})}
///
/// @FA{properties} must be an object, with the following attribues:
///
/// - @LIT{waitForSync} (optional, default @LIT{false}): If @LIT{true} creating
///   a document will only return after the data was synced to disk.
///
/// - @LIT{journalSize} (optional, default is a @ref CommandLineAvocado
///   "configuration parameter"):  The maximal size of
///   a journal or datafile.  Note that this also limits the maximal
///   size of a single object. Must be at least 1MB.
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

static v8::Handle<v8::Value> JS_CreateVocBase (v8::Arguments const& argv) {
  return CreateVocBase(argv, false);
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
/// @FUN{@FA{db}._remove(@FA{document-handle}, @FA{data})}
///
/// As before. Instead of document a @FA{document-handle} can be passed as
/// first argument.
///
/// @EXAMPLES
///
/// Delete a document:
///
/// @verbinclude shell_remove-document-db
///
/// Delete a document with a conflict:
///
/// @verbinclude shell_remove-document-conflict-db
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
/// The @FN{document} method finds a document given it's identifier.  It
/// returns the document. Note that the returned docuement contains two
/// pseudo-attributes, namely @LIT{_id} and @LIT{_rev}. @LIT{_id}
/// contains the @FA{docuement-handle} and @LIT{_rev} the revision of
/// the document.
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
/// Return the document:
///
/// @verbinclude shell_read-document-db
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_DocumentVocbase (v8::Arguments const& argv) {
  v8::HandleScope scope;

  TRI_vocbase_t* vocbase = TRI_UnwrapClass<TRI_vocbase_t>(argv.Holder(), WRP_VOCBASE_TYPE);
  
  if (vocbase == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("corrupted vocbase")));
  }

  return DocumentVocbaseCol(vocbase, 0, argv);
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
/// @FUN{@FA{db}._replace(@FA{document-handle}, @FA{data})}
///
/// As before. Instead of document a @FA{document-handle} can be passed as
/// first argument.
///
/// @EXAMPLES
///
/// Create and update a document:
///
/// @verbinclude shell_update-document-db
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
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                     TRI_VOCBASE_T EDGES FUNCTIONS
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
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> MapGetEdges (v8::Local<v8::String> name,
                                            const v8::AccessorInfo& info) {
  v8::HandleScope scope;

  TRI_vocbase_t* vocbase = TRI_UnwrapClass<TRI_vocbase_t>(info.Holder(), WRP_VOCBASE_TYPE);

  if (vocbase == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("corrupted vocbase")));
  }

  // convert the JavaScript string to a string
  string key = TRI_ObjectToString(name);

  if (key == "") {
    return scope.Close(v8::ThrowException(CreateErrorObject(TRI_ERROR_AVOCADO_ILLEGAL_NAME, "name must not be empty")));
  }

  if (   key == "toString"
      || key == "toJSON"
      || key == "hasOwnProperty"
      || key[0] == '_') {
    return v8::Handle<v8::Value>();
  }

  // look up the value if it exists
  TRI_vocbase_col_t const* collection = TRI_FindCollectionByNameVocBase(vocbase, key.c_str(), true);

  // if the key is not present return an empty handle as signal
  if (collection == 0) {
    return scope.Close(v8::ThrowException(v8::String::New("cannot load or create edge collection")));
  }

  return scope.Close(TRI_WrapEdgesCollection(collection));
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
/// @FUN{edges._collection(@FA{collection-identifier})}
///
/// Returns the collection with the given identifier or null if no such
/// collection exists.
///
/// @FUN{edges._collection(@FA{collection-name})}
///
/// Returns the collection with the given name or null if no such collection
/// exists.
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_CollectionEdges (v8::Arguments const& argv) {
  return CollectionVocBase(argv, true);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns all collections
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_CollectionsEdges (v8::Arguments const& argv) {
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

    result->Set(i, TRI_WrapEdgesCollection(collection));
  }

  TRI_DestroyVectorPointer(&colls);

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new edge collection
///
/// @FUN{edges._create(@FA{collection-name})}
///
/// Creates a new collection named @FA{collection-name}. If the collection name
/// already exists, than an error is thrown. The default value for
/// @LIT{waitForSync} is @LIT{false}.
///
/// @FUN{edges._create(@FA{collection-name}, @FA{properties})}
///
/// @FA{properties} must be an object, with the following attribues:
///
/// - @LIT{waitForSync} (optional, default @LIT{false}): If @LIT{true} creating
///   a document will only return after the data was synced to disk.
///
/// - @LIT{journalSize} (optional, default is a @ref CommandLineAvocado
///   "configuration parameter"):  The maximal size of
///   a journal or datafile.  Note that this also limits the maximal
///   size of a single object. Must be at least 1MB.
///
/// - @LIT{isSystem} (optional, default is @LIT{false}): If true, create a
///   system collection. In this case @FA{collection-name} should start with
///   an underscore.
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_CreateEdges (v8::Arguments const& argv) {
  return CreateVocBase(argv, true);
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
/// @brief weak reference callback for a bridge
////////////////////////////////////////////////////////////////////////////////

static void WeakBridgeCallback (v8::Persistent<v8::Value> object, void* parameter) {
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
  TRI_doc_collection_t* collection = barrier->_container->_collection;

  // convert the JavaScript string to a string
  string key = TRI_ObjectToString(name);
  
  if (key == "") {
    return scope.Close(v8::ThrowException(CreateErrorObject(TRI_ERROR_AVOCADO_ILLEGAL_NAME, "name must not be empty")));
  }  
  
  if (key[0] == '_') {
    return scope.Close(v8::Handle<v8::Value>());
  }

  // get shape accessor
  TRI_shaper_t* shaper = collection->_shaper;
  TRI_shape_pid_t pid = shaper->findAttributePathByName(shaper, key.c_str()); 

  TRI_shape_sid_t sid;
  TRI_EXTRACT_SHAPE_IDENTIFIER_MARKER(sid, marker);

  TRI_shape_access_t* acc = TRI_ShapeAccessor(shaper, sid, pid);

  if (acc == NULL || acc->_shape == NULL) {
    if (acc != NULL) {
      TRI_FreeShapeAccessor(acc);
    }

    return scope.Close(v8::Handle<v8::Value>());
  }

  // convert to v8 value
  TRI_shape_t const* shape = acc->_shape;
  TRI_shaped_json_t json;

  TRI_shaped_json_t document;
  TRI_EXTRACT_SHAPED_JSON_MARKER(document, marker);

  if (TRI_ExecuteShapeAccessor(acc, &document, &json)) {    
    TRI_FreeShapeAccessor(acc);
    return scope.Close(TRI_JsonShapeData(shaper, shape, json._data.data, json._data.length));
  }

  TRI_FreeShapeAccessor(acc);
  return scope.Close(v8::ThrowException(v8::String::New("cannot extract attribute")));  
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
  TRI_doc_collection_t* collection = barrier->_container->_collection;

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
  TRI_doc_collection_t* collection = barrier->_container->_collection;

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
  
  // get shape accessor
  TRI_shaper_t* shaper = collection->_shaper;
  TRI_shape_pid_t pid = shaper->findAttributePathByName(shaper, key.c_str()); 

  TRI_shape_sid_t sid;
  TRI_EXTRACT_SHAPE_IDENTIFIER_MARKER(sid, marker);

  TRI_shape_access_t* acc = TRI_ShapeAccessor(shaper, sid, pid);

  // key not found
  if (acc == NULL || acc->_shape == NULL) {
    if (acc != NULL) {
      TRI_FreeShapeAccessor(acc);
    }

    return scope.Close(v8::Handle<v8::Integer>());
  }

  TRI_FreeShapeAccessor(acc);
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
/// @brief wraps a TRI_vocbase_t
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Object> TRI_WrapVocBase (TRI_vocbase_t const* database) {
  TRI_v8_global_t* v8g;
  v8::HandleScope scope;

  v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();
  v8::Handle<v8::Object> result = WrapClass(v8g->VocbaseTempl, 
                                            WRP_VOCBASE_TYPE,
                                            const_cast<TRI_vocbase_t*>(database));

  result->Set(v8::String::New("_path"),
              v8::String::New(database->_path),
              v8::ReadOnly);

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief wraps a TRI_vocbase_t for edges
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Object> TRI_WrapEdges (TRI_vocbase_t const* database) {
  TRI_v8_global_t* v8g;
  v8::HandleScope scope;

  v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();
  v8::Handle<v8::Object> result = WrapClass(v8g->EdgesTempl,
                                            WRP_VOCBASE_TYPE,
                                            const_cast<TRI_vocbase_t*>(database));

  result->Set(v8::String::New("_path"),
              v8::String::New(database->_path),
              v8::ReadOnly);

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

  result->Set(v8::String::New("_id"),
              v8::Number::New(collection->_cid),
              v8::ReadOnly);

  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief wraps a TRI_vocbase_col_t for edges
////////////////////////////////////////////////////////////////////////////////

v8::Handle<v8::Object> TRI_WrapEdgesCollection (TRI_vocbase_col_t const* collection) {
  TRI_v8_global_t* v8g;
  v8::HandleScope scope;

  v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();
  v8::Handle<v8::Object> result = WrapClass(v8g->EdgesColTempl, 
                                            WRP_VOCBASE_COL_TYPE,
                                            const_cast<TRI_vocbase_col_t*>(collection));

  result->Set(v8::String::New("_id"),
              v8::Number::New(collection->_cid),
              v8::ReadOnly);

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

  // point the 0 index Field to the c++ pointer for unwrapping later
  result->SetInternalField(SLOT_CLASS_TYPE, v8::Integer::New(WRP_SHAPED_JSON_TYPE));
  result->SetInternalField(SLOT_CLASS, v8::External::New(const_cast<void*>(document->_data)));

  map< void*, v8::Persistent<v8::Value> >::iterator i = v8g->JSBarriers.find(barrier);

  if (i == v8g->JSBarriers.end()) {
    v8::Persistent<v8::Value> persistent = v8::Persistent<v8::Value>::New(v8::External::New(barrier));
    result->SetInternalField(SLOT_BARRIER, persistent);

    v8g->JSBarriers[barrier] = persistent;

    persistent.MakeWeak(barrier, WeakBridgeCallback);
  }
  else {
    result->SetInternalField(SLOT_BARRIER, i->second);
  }

  // store the document reference
  TRI_voc_did_t did = document->_did;
  TRI_voc_rid_t rid = document->_rid;

  result->Set(v8g->DidKey, TRI_ObjectReference(collection->_collection->base._cid, did), v8::ReadOnly);
  result->Set(v8g->RevKey, v8::Number::New(rid), v8::ReadOnly);
  
  TRI_df_marker_type_t type = ((TRI_df_marker_t*) document->_data)->_type;

  if (type == TRI_DOC_MARKER_EDGE) {
    TRI_doc_edge_marker_t* marker = (TRI_doc_edge_marker_t*) document->_data;

    result->Set(v8g->FromKey, TRI_ObjectReference(marker->_fromCid, marker->_fromDid));
    result->Set(v8g->ToKey, TRI_ObjectReference(marker->_toCid, marker->_toDid));
  }

  // and return
  return scope.Close(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a TRI_vocbase_t global context
////////////////////////////////////////////////////////////////////////////////

void TRI_InitV8VocBridge (v8::Handle<v8::Context> context, TRI_vocbase_t* vocbase) {
  v8::HandleScope scope;

  v8::Handle<v8::ObjectTemplate> rt;
  v8::Handle<v8::FunctionTemplate> ft;

  // check the isolate
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  TRI_v8_global_t* v8g = (TRI_v8_global_t*) isolate->GetData();

  if (v8g == 0) {
    v8g = new TRI_v8_global_t;
    isolate->SetData(v8g);
  }

  // create the regular expressions
  string expr = "([0-9][0-9]*)" + string(TRI_DOCUMENT_HANDLE_SEPARATOR_STR) + "([0-9][0-9]*)";

  if (regcomp(&v8g->DocumentIdRegex, expr.c_str(), REG_ICASE | REG_EXTENDED) != 0) {
    LOG_FATAL("cannot compile regular expression");
    exit(EXIT_FAILURE);
  }

  if (regcomp(&v8g->IndexIdRegex, expr.c_str(), REG_ICASE | REG_EXTENDED) != 0) {
    LOG_FATAL("cannot compile regular expression");
    exit(EXIT_FAILURE);
  }

  // .............................................................................
  // global function names
  // .............................................................................

  if (v8g->OutputFuncName.IsEmpty()) {
    v8g->OutputFuncName = v8::Persistent<v8::String>::New(v8::String::New("output"));
  }

  // .............................................................................
  // local function names
  // .............................................................................

  v8::Handle<v8::String> AllFuncName = v8::Persistent<v8::String>::New(v8::String::New("ALL"));
  v8::Handle<v8::String> ByExampleFuncName = v8::Persistent<v8::String>::New(v8::String::New("BY_EXAMPLE"));
  v8::Handle<v8::String> NearFuncName = v8::Persistent<v8::String>::New(v8::String::New("NEAR"));
  v8::Handle<v8::String> WithinFuncName = v8::Persistent<v8::String>::New(v8::String::New("WITHIN"));

  v8::Handle<v8::String> CountFuncName = v8::Persistent<v8::String>::New(v8::String::New("count"));
  v8::Handle<v8::String> DisposeFuncName = v8::Persistent<v8::String>::New(v8::String::New("dispose"));
  v8::Handle<v8::String> DocumentFuncName = v8::Persistent<v8::String>::New(v8::String::New("document"));
  v8::Handle<v8::String> DropFuncName = v8::Persistent<v8::String>::New(v8::String::New("drop"));
  v8::Handle<v8::String> DropIndexFuncName = v8::Persistent<v8::String>::New(v8::String::New("dropIndex"));
  v8::Handle<v8::String> EdgesFuncName = v8::Persistent<v8::String>::New(v8::String::New("edges"));
  v8::Handle<v8::String> EnsureGeoIndexFuncName = v8::Persistent<v8::String>::New(v8::String::New("ensureGeoIndex"));
  v8::Handle<v8::String> EnsureHashIndexFuncName = v8::Persistent<v8::String>::New(v8::String::New("ensureHashIndex"));
  v8::Handle<v8::String> EnsurePriorityQueueIndexFuncName = v8::Persistent<v8::String>::New(v8::String::New("ensurePQIndex"));
  v8::Handle<v8::String> EnsureSkiplistFuncName = v8::Persistent<v8::String>::New(v8::String::New("ensureSkiplist"));
  v8::Handle<v8::String> EnsureUniqueConstraintFuncName = v8::Persistent<v8::String>::New(v8::String::New("ensureUniqueConstraint"));
  v8::Handle<v8::String> EnsureUniqueSkiplistFuncName = v8::Persistent<v8::String>::New(v8::String::New("ensureUniqueSkiplist"));
  v8::Handle<v8::String> ExecuteFuncName = v8::Persistent<v8::String>::New(v8::String::New("execute"));
  v8::Handle<v8::String> FiguresFuncName = v8::Persistent<v8::String>::New(v8::String::New("figures"));
  v8::Handle<v8::String> GetBatchSizeFuncName = v8::Persistent<v8::String>::New(v8::String::New("getBatchSize"));
  v8::Handle<v8::String> GetIndexesFuncName = v8::Persistent<v8::String>::New(v8::String::New("getIndexes"));
  v8::Handle<v8::String> GetRowsFuncName = v8::Persistent<v8::String>::New(v8::String::New("getRows"));
  v8::Handle<v8::String> HasCountFuncName = v8::Persistent<v8::String>::New(v8::String::New("hasCount"));
  v8::Handle<v8::String> HasNextFuncName = v8::Persistent<v8::String>::New(v8::String::New("hasNext"));
  v8::Handle<v8::String> IdFuncName = v8::Persistent<v8::String>::New(v8::String::New("id"));
  v8::Handle<v8::String> InEdgesFuncName = v8::Persistent<v8::String>::New(v8::String::New("inEdges"));
  v8::Handle<v8::String> LoadFuncName = v8::Persistent<v8::String>::New(v8::String::New("load"));
  v8::Handle<v8::String> NameFuncName = v8::Persistent<v8::String>::New(v8::String::New("name"));
  v8::Handle<v8::String> NextFuncName = v8::Persistent<v8::String>::New(v8::String::New("next"));
  v8::Handle<v8::String> NextRefFuncName = v8::Persistent<v8::String>::New(v8::String::New("nextRef"));
  v8::Handle<v8::String> OutEdgesFuncName = v8::Persistent<v8::String>::New(v8::String::New("outEdges"));
  v8::Handle<v8::String> PersistFuncName = v8::Persistent<v8::String>::New(v8::String::New("persist"));
  v8::Handle<v8::String> PropertiesFuncName = v8::Persistent<v8::String>::New(v8::String::New("properties"));
  v8::Handle<v8::String> RemoveFuncName = v8::Persistent<v8::String>::New(v8::String::New("remove"));
  v8::Handle<v8::String> RenameFuncName = v8::Persistent<v8::String>::New(v8::String::New("rename"));
  v8::Handle<v8::String> ReplaceFuncName = v8::Persistent<v8::String>::New(v8::String::New("replace"));
  v8::Handle<v8::String> SaveFuncName = v8::Persistent<v8::String>::New(v8::String::New("save"));
  v8::Handle<v8::String> StatusFuncName = v8::Persistent<v8::String>::New(v8::String::New("status"));
  v8::Handle<v8::String> UnloadFuncName = v8::Persistent<v8::String>::New(v8::String::New("unload"));
  v8::Handle<v8::String> UseNextFuncName = v8::Persistent<v8::String>::New(v8::String::New("useNext"));

  v8::Handle<v8::String> _CollectionFuncName = v8::Persistent<v8::String>::New(v8::String::New("_collection"));
  v8::Handle<v8::String> _CollectionsFuncName = v8::Persistent<v8::String>::New(v8::String::New("_collections"));
  v8::Handle<v8::String> _CompletionsFuncName = v8::Persistent<v8::String>::New(v8::String::New("_COMPLETIONS"));
  v8::Handle<v8::String> _CreateFuncName = v8::Persistent<v8::String>::New(v8::String::New("_create"));
  v8::Handle<v8::String> _RemoveFuncName = v8::Persistent<v8::String>::New(v8::String::New("_remove"));
  v8::Handle<v8::String> _DocumentFuncName = v8::Persistent<v8::String>::New(v8::String::New("_document"));
  v8::Handle<v8::String> _ReplaceFuncName = v8::Persistent<v8::String>::New(v8::String::New("_replace"));

  // .............................................................................
  // query types
  // .............................................................................

  v8g->CollectionQueryType = v8::Persistent<v8::String>::New(v8::String::New("collection"));

  // .............................................................................
  // keys
  // .............................................................................

  v8g->JournalSizeKey = v8::Persistent<v8::String>::New(v8::String::New("journalSize"));
  v8g->WaitForSyncKey = v8::Persistent<v8::String>::New(v8::String::New("waitForSync"));

  if (v8g->DidKey.IsEmpty()) {
    v8g->DidKey = v8::Persistent<v8::String>::New(v8::String::New("_id"));
  }

  if (v8g->FromKey.IsEmpty()) {
    v8g->FromKey = v8::Persistent<v8::String>::New(v8::String::New("_from"));
  }

  if (v8g->IidKey.IsEmpty()) {
    v8g->IidKey = v8::Persistent<v8::String>::New(v8::String::New("id"));
  }

  if (v8g->OldRevKey.IsEmpty()) {
    v8g->OldRevKey = v8::Persistent<v8::String>::New(v8::String::New("_oldRev"));
  }

  if (v8g->RevKey.IsEmpty()) {
    v8g->RevKey = v8::Persistent<v8::String>::New(v8::String::New("_rev"));
  }

  if (v8g->ToKey.IsEmpty()) {
    v8g->ToKey = v8::Persistent<v8::String>::New(v8::String::New("_to"));
  }

  // .............................................................................
  // generate the query error template
  // .............................................................................

  ft = v8::FunctionTemplate::New();
  ft->SetClassName(v8::String::New("AvocadoError"));

  rt = ft->InstanceTemplate();

  v8g->ErrorTempl = v8::Persistent<v8::ObjectTemplate>::New(rt);

  // must come after SetInternalFieldCount
  context->Global()->Set(v8::String::New("AvocadoError"),
                         ft->GetFunction());
  
  // .............................................................................
  // generate the TRI_vocbase_t template
  // .............................................................................

  ft = v8::FunctionTemplate::New();
  ft->SetClassName(v8::String::New("AvocadoDatabase"));

  rt = ft->InstanceTemplate();
  rt->SetInternalFieldCount(2);

  rt->SetNamedPropertyHandler(MapGetVocBase);

  rt->Set(_CollectionFuncName, v8::FunctionTemplate::New(JS_CollectionVocBase));
  rt->Set(_CollectionsFuncName, v8::FunctionTemplate::New(JS_CollectionsVocBase));
  rt->Set(_CompletionsFuncName, v8::FunctionTemplate::New(JS_CompletionsVocBase));
  rt->Set(_CreateFuncName, v8::FunctionTemplate::New(JS_CreateVocBase));

  rt->Set(_RemoveFuncName, v8::FunctionTemplate::New(JS_RemoveVocbase));
  rt->Set(_DocumentFuncName, v8::FunctionTemplate::New(JS_DocumentVocbase));
  rt->Set(_ReplaceFuncName, v8::FunctionTemplate::New(JS_ReplaceVocbase));

  v8g->VocbaseTempl = v8::Persistent<v8::ObjectTemplate>::New(rt);

  // must come after SetInternalFieldCount
  context->Global()->Set(v8::String::New("AvocadoDatabase"),
                         ft->GetFunction());

  // .............................................................................
  // generate the TRI_vocbase_t template for edges
  // .............................................................................

  ft = v8::FunctionTemplate::New();
  ft->SetClassName(v8::String::New("AvocadoEdges"));

  rt = ft->InstanceTemplate();
  rt->SetInternalFieldCount(2);

  rt->SetNamedPropertyHandler(MapGetEdges);

  rt->Set(_CollectionFuncName, v8::FunctionTemplate::New(JS_CollectionEdges));
  rt->Set(_CollectionsFuncName, v8::FunctionTemplate::New(JS_CollectionsEdges));
  rt->Set(_CompletionsFuncName, v8::FunctionTemplate::New(JS_CompletionsVocBase));
  rt->Set(_CreateFuncName, v8::FunctionTemplate::New(JS_CreateEdges));

  rt->Set(_RemoveFuncName, v8::FunctionTemplate::New(JS_RemoveVocbase));
  rt->Set(_DocumentFuncName, v8::FunctionTemplate::New(JS_DocumentVocbase));
  rt->Set(_ReplaceFuncName, v8::FunctionTemplate::New(JS_ReplaceVocbase));

  v8g->EdgesTempl = v8::Persistent<v8::ObjectTemplate>::New(rt);

  // must come after SetInternalFieldCount
  context->Global()->Set(v8::String::New("AvocadoEdges"),
                         ft->GetFunction());

  
  // .............................................................................
  // generate the TRI_shaped_json_t template
  // .............................................................................

  ft = v8::FunctionTemplate::New();
  ft->SetClassName(v8::String::New("ShapedJson"));

  rt = ft->InstanceTemplate();
  rt->SetInternalFieldCount(3); // TODO check the 3
  
  rt->SetNamedPropertyHandler(MapGetShapedJson,         // NamedPropertyGetter,
                              0,                        // NamedPropertySetter setter = 0
                              PropertyQueryShapedJson,  // NamedPropertyQuery,
                              0,                        // NamedPropertyDeleter deleter = 0,
                              KeysOfShapedJson          // NamedPropertyEnumerator,
                                                        // Handle<Value> data = Handle<Value>());
                              );

  v8g->ShapedJsonTempl = v8::Persistent<v8::ObjectTemplate>::New(rt);

  // must come after SetInternalFieldCount
  context->Global()->Set(v8::String::New("ShapedJson"),
                         ft->GetFunction());
  
  // .............................................................................
  // generate the TRI_vocbase_col_t template
  // .............................................................................

  ft = v8::FunctionTemplate::New();
  ft->SetClassName(v8::String::New("AvocadoCollection"));

  rt = ft->InstanceTemplate();
  rt->SetInternalFieldCount(SLOT_END); // FIXME

  v8g->VocbaseColTempl = v8::Persistent<v8::ObjectTemplate>::New(rt);

  rt->Set(AllFuncName, v8::FunctionTemplate::New(JS_AllQuery));
  rt->Set(ByExampleFuncName, v8::FunctionTemplate::New(JS_ByExampleQuery));

  rt->Set(CountFuncName, v8::FunctionTemplate::New(JS_CountVocbaseCol));
  rt->Set(DocumentFuncName, v8::FunctionTemplate::New(JS_DocumentVocbaseCol));
  rt->Set(DropFuncName, v8::FunctionTemplate::New(JS_DropVocbaseCol));
  rt->Set(DropIndexFuncName, v8::FunctionTemplate::New(JS_DropIndexVocbaseCol));
  rt->Set(EnsureGeoIndexFuncName, v8::FunctionTemplate::New(JS_EnsureGeoIndexVocbaseCol));
  rt->Set(EnsureHashIndexFuncName, v8::FunctionTemplate::New(JS_EnsureHashIndexVocbaseCol));
  rt->Set(EnsurePriorityQueueIndexFuncName, v8::FunctionTemplate::New(JS_EnsurePriorityQueueIndexVocbaseCol));
  rt->Set(EnsureSkiplistFuncName, v8::FunctionTemplate::New(JS_EnsureSkiplistVocbaseCol));
  rt->Set(EnsureUniqueConstraintFuncName, v8::FunctionTemplate::New(JS_EnsureUniqueConstraintVocbaseCol));
  rt->Set(EnsureUniqueSkiplistFuncName, v8::FunctionTemplate::New(JS_EnsureUniqueSkiplistVocbaseCol));
  rt->Set(FiguresFuncName, v8::FunctionTemplate::New(JS_FiguresVocbaseCol));
  rt->Set(GetIndexesFuncName, v8::FunctionTemplate::New(JS_GetIndexesVocbaseCol));
  rt->Set(LoadFuncName, v8::FunctionTemplate::New(JS_LoadVocbaseCol));
  rt->Set(NameFuncName, v8::FunctionTemplate::New(JS_NameVocbaseCol));
  rt->Set(NearFuncName, v8::FunctionTemplate::New(JS_NearQuery));
  rt->Set(PropertiesFuncName, v8::FunctionTemplate::New(JS_PropertiesVocbaseCol));
  rt->Set(RemoveFuncName, v8::FunctionTemplate::New(JS_RemoveVocbaseCol));
  rt->Set(RenameFuncName, v8::FunctionTemplate::New(JS_RenameVocbaseCol));
  rt->Set(StatusFuncName, v8::FunctionTemplate::New(JS_StatusVocbaseCol));
  rt->Set(UnloadFuncName, v8::FunctionTemplate::New(JS_UnloadVocbaseCol));
  rt->Set(WithinFuncName, v8::FunctionTemplate::New(JS_WithinQuery));
  
  rt->Set(SaveFuncName, v8::FunctionTemplate::New(JS_SaveVocbaseCol));
  rt->Set(ReplaceFuncName, v8::FunctionTemplate::New(JS_ReplaceVocbaseCol));

  // must come after SetInternalFieldCount
  context->Global()->Set(v8::String::New("AvocadoCollection"),
                         ft->GetFunction());

  // .............................................................................
  // generate the TRI_vocbase_col_t template for edges
  // .............................................................................

  ft = v8::FunctionTemplate::New();
  ft->SetClassName(v8::String::New("AvocadoEdgesCollection"));

  rt = ft->InstanceTemplate();
  rt->SetInternalFieldCount(SLOT_END); // FIXME

  rt->Set(AllFuncName, v8::FunctionTemplate::New(JS_AllQuery));
  rt->Set(ByExampleFuncName, v8::FunctionTemplate::New(JS_ByExampleQuery));

  rt->Set(CountFuncName, v8::FunctionTemplate::New(JS_CountVocbaseCol));
  rt->Set(DocumentFuncName, v8::FunctionTemplate::New(JS_DocumentVocbaseCol));
  rt->Set(DropFuncName, v8::FunctionTemplate::New(JS_DropVocbaseCol));
  rt->Set(DropIndexFuncName, v8::FunctionTemplate::New(JS_DropIndexVocbaseCol));
  rt->Set(EnsureGeoIndexFuncName, v8::FunctionTemplate::New(JS_EnsureGeoIndexVocbaseCol));
  rt->Set(EnsureHashIndexFuncName, v8::FunctionTemplate::New(JS_EnsureHashIndexVocbaseCol));
  rt->Set(EnsurePriorityQueueIndexFuncName, v8::FunctionTemplate::New(JS_EnsurePriorityQueueIndexVocbaseCol));
  rt->Set(EnsureSkiplistFuncName, v8::FunctionTemplate::New(JS_EnsureSkiplistVocbaseCol));
  rt->Set(EnsureUniqueConstraintFuncName, v8::FunctionTemplate::New(JS_EnsureUniqueConstraintVocbaseCol));
  rt->Set(EnsureUniqueSkiplistFuncName, v8::FunctionTemplate::New(JS_EnsureUniqueSkiplistVocbaseCol));
  rt->Set(FiguresFuncName, v8::FunctionTemplate::New(JS_FiguresVocbaseCol));
  rt->Set(GetIndexesFuncName, v8::FunctionTemplate::New(JS_GetIndexesVocbaseCol));
  rt->Set(LoadFuncName, v8::FunctionTemplate::New(JS_LoadVocbaseCol));
  rt->Set(NameFuncName, v8::FunctionTemplate::New(JS_NameVocbaseCol));
  rt->Set(NearFuncName, v8::FunctionTemplate::New(JS_NearQuery));
  rt->Set(PropertiesFuncName, v8::FunctionTemplate::New(JS_PropertiesVocbaseCol));
  rt->Set(RemoveFuncName, v8::FunctionTemplate::New(JS_RemoveVocbaseCol));
  rt->Set(RenameFuncName, v8::FunctionTemplate::New(JS_RenameVocbaseCol));
  rt->Set(ReplaceFuncName, v8::FunctionTemplate::New(JS_ReplaceVocbaseCol));
  rt->Set(StatusFuncName, v8::FunctionTemplate::New(JS_StatusVocbaseCol));
  rt->Set(UnloadFuncName, v8::FunctionTemplate::New(JS_UnloadVocbaseCol));
  rt->Set(WithinFuncName, v8::FunctionTemplate::New(JS_WithinQuery));

  rt->Set(SaveFuncName, v8::FunctionTemplate::New(JS_SaveEdgesCol));

  rt->Set(EdgesFuncName, v8::FunctionTemplate::New(JS_EdgesQuery));
  rt->Set(InEdgesFuncName, v8::FunctionTemplate::New(JS_InEdgesQuery));
  rt->Set(OutEdgesFuncName, v8::FunctionTemplate::New(JS_OutEdgesQuery));

  v8g->EdgesColTempl = v8::Persistent<v8::ObjectTemplate>::New(rt);

  // must come after SetInternalFieldCount
  context->Global()->Set(v8::String::New("AvocadoEdgesCollection"),
                         ft->GetFunction());

  // .............................................................................
  // generate the general error template
  // .............................................................................

  ft = v8::FunctionTemplate::New();
  ft->SetClassName(v8::String::New("AvocadoError"));

  rt = ft->InstanceTemplate();
  rt->SetInternalFieldCount(2);

  v8g->ErrorTempl = v8::Persistent<v8::ObjectTemplate>::New(rt);

  // must come after SetInternalFieldCount
  context->Global()->Set(v8::String::New("AvocadoError"), ft->GetFunction());
  
  // .............................................................................
  // generate the general cursor template
  // .............................................................................

  ft = v8::FunctionTemplate::New();
  ft->SetClassName(v8::String::New("AvocadoCursor"));

  rt = ft->InstanceTemplate();
  rt->SetInternalFieldCount(2);

  rt->Set(CountFuncName, v8::FunctionTemplate::New(JS_CountGeneralCursor));
  rt->Set(DisposeFuncName, v8::FunctionTemplate::New(JS_DisposeGeneralCursor));
  rt->Set(GetBatchSizeFuncName, v8::FunctionTemplate::New(JS_GetBatchSizeGeneralCursor));
  rt->Set(GetRowsFuncName, v8::FunctionTemplate::New(JS_GetRowsGeneralCursor));
  rt->Set(HasCountFuncName, v8::FunctionTemplate::New(JS_HasCountGeneralCursor));
  rt->Set(HasNextFuncName, v8::FunctionTemplate::New(JS_HasNextGeneralCursor));
  rt->Set(IdFuncName, v8::FunctionTemplate::New(JS_IdGeneralCursor));
  rt->Set(NextFuncName, v8::FunctionTemplate::New(JS_NextGeneralCursor));
  rt->Set(PersistFuncName, v8::FunctionTemplate::New(JS_PersistGeneralCursor));

  v8g->GeneralCursorTempl = v8::Persistent<v8::ObjectTemplate>::New(rt);

  // must come after SetInternalFieldCount
  context->Global()->Set(v8::String::New("AvocadoCursor"), ft->GetFunction());
  

/* DEPRECATED START */
  
  // .............................................................................
  // generate the cursor template - DEPRECATED
  // .............................................................................

  ft = v8::FunctionTemplate::New();
  ft->SetClassName(v8::String::New("AvocadoDeprecatedCursor"));

  rt = ft->InstanceTemplate();
  rt->SetInternalFieldCount(2);

  rt->Set(HasNextFuncName, v8::FunctionTemplate::New(JS_HasNextCursor));
  rt->Set(NextFuncName, v8::FunctionTemplate::New(JS_NextCursor));
  rt->Set(NextRefFuncName, v8::FunctionTemplate::New(JS_NextRefCursor));
  rt->Set(UseNextFuncName, v8::FunctionTemplate::New(JS_UseNextCursor));
  rt->Set(CountFuncName, v8::FunctionTemplate::New(JS_CountCursor));

  v8g->CursorTempl = v8::Persistent<v8::ObjectTemplate>::New(rt);

  // must come after SetInternalFieldCount
  context->Global()->Set(v8::String::New("AvocadoDeprecatedCursor"),
                         ft->GetFunction());

  // .............................................................................
  // generate the query template
  // .............................................................................

  ft = v8::FunctionTemplate::New();
  ft->SetClassName(v8::String::New("AvocadoDeprecatedQuery"));

  rt = ft->InstanceTemplate();
  rt->SetInternalFieldCount(2);

  rt->Set(ExecuteFuncName, v8::FunctionTemplate::New(JS_ExecuteAql));

  v8g->QueryTempl = v8::Persistent<v8::ObjectTemplate>::New(rt);

  // must come after SetInternalFieldCount
  context->Global()->Set(v8::String::New("AvocadoDeprecatedQuery"),
                         ft->GetFunction());

  // .............................................................................
  // generate the where clause template
  // .............................................................................

  ft = v8::FunctionTemplate::New();
  ft->SetClassName(v8::String::New("AvocadoWhereClause"));

  rt = ft->InstanceTemplate();
  rt->SetInternalFieldCount(2);

  v8g->WhereTempl = v8::Persistent<v8::ObjectTemplate>::New(rt);

  // must come after SetInternalFieldCount
  context->Global()->Set(v8::String::New("AvocadoWhereClause"),
                         ft->GetFunction());

  // .............................................................................
  // generate the skip list operator template
  // .............................................................................

  ft = v8::FunctionTemplate::New();
  ft->SetClassName(v8::String::New("SLOperator"));

  rt = ft->InstanceTemplate();
  rt->SetInternalFieldCount(2);
  v8g->SLOperatorTempl = v8::Persistent<v8::ObjectTemplate>::New(rt);

  // must come after SetInternalFieldCount
  context->Global()->Set(v8::String::New("SLOperator"),ft->GetFunction());
                         
  // .............................................................................
  // create the global functions
  // .............................................................................

  context->Global()->Set(v8::String::New("AQL_WHERE_BOOLEAN"),
                         v8::FunctionTemplate::New(JS_WhereBooleanAql)->GetFunction(),
                         v8::ReadOnly);

  context->Global()->Set(v8::String::New("AQL_WHERE_GENERAL"),
                         v8::FunctionTemplate::New(JS_WhereGeneralAql)->GetFunction(),
                         v8::ReadOnly);

  context->Global()->Set(v8::String::New("AQL_WHERE_HASH_CONST"),
                         v8::FunctionTemplate::New(JS_WhereHashConstAql)->GetFunction(),
                         v8::ReadOnly);
                         
  context->Global()->Set(v8::String::New("AQL_WHERE_PQ_CONST"),
                         v8::FunctionTemplate::New(JS_WherePQConstAql)->GetFunction(),
                         v8::ReadOnly);
                         
  context->Global()->Set(v8::String::New("AQL_WHERE_SL_CONST"),
                         v8::FunctionTemplate::New(JS_WhereSkiplistConstAql)->GetFunction(),
                         v8::ReadOnly);
                         
  context->Global()->Set(v8::String::New("AQL_WHERE_PRIMARY_CONST"),
                         v8::FunctionTemplate::New(JS_WherePrimaryConstAql)->GetFunction(),
                         v8::ReadOnly);

  context->Global()->Set(v8::String::New("AQL_WHERE_WITHIN_CONST"),
                         v8::FunctionTemplate::New(JS_WhereWithinConstAql)->GetFunction(),
                         v8::ReadOnly);

  context->Global()->Set(v8::String::New("AQL_SELECT"),
                         v8::FunctionTemplate::New(JS_SelectAql)->GetFunction(),
                         v8::ReadOnly);
                         
  context->Global()->Set(v8::String::New("AQL_HASH_SELECT"),
                         v8::FunctionTemplate::New(JS_HashSelectAql)->GetFunction(),
                         v8::ReadOnly);

  context->Global()->Set(v8::String::New("AQL_PQ_SELECT"),
                         v8::FunctionTemplate::New(JS_PQSelectAql)->GetFunction(),
                         v8::ReadOnly);

  context->Global()->Set(v8::String::New("AQL_SL_SELECT"),
                         v8::FunctionTemplate::New(JS_SkiplistSelectAql)->GetFunction(),
                         v8::ReadOnly);
                         
  context->Global()->Set(v8::String::New("AND"),
                         v8::FunctionTemplate::New(JS_Operator_AND)->GetFunction(),
                         v8::ReadOnly);
                         
  context->Global()->Set(v8::String::New("OR"),
                         v8::FunctionTemplate::New(JS_Operator_OR)->GetFunction(),
                         v8::ReadOnly);
                         
  context->Global()->Set(v8::String::New("EQ"),
                         v8::FunctionTemplate::New(JS_Operator_EQ)->GetFunction(),
                         v8::ReadOnly);
                         
  context->Global()->Set(v8::String::New("GE"),
                         v8::FunctionTemplate::New(JS_Operator_GE)->GetFunction(),
                         v8::ReadOnly);
                         
  context->Global()->Set(v8::String::New("GT"),
                         v8::FunctionTemplate::New(JS_Operator_GT)->GetFunction(),
                         v8::ReadOnly);
                         
  context->Global()->Set(v8::String::New("LE"),
                         v8::FunctionTemplate::New(JS_Operator_LE)->GetFunction(),
                         v8::ReadOnly);
                         
  context->Global()->Set(v8::String::New("LT"),
                         v8::FunctionTemplate::New(JS_Operator_LT)->GetFunction(),
                         v8::ReadOnly);
                         
/* DEPRECATED END */

  context->Global()->Set(v8::String::New("CURSOR"),
                         v8::FunctionTemplate::New(JS_Cursor)->GetFunction(),
                         v8::ReadOnly);
  
  context->Global()->Set(v8::String::New("AHUACATL_RUN"),
                         v8::FunctionTemplate::New(JS_RunAhuacatl)->GetFunction(),
                         v8::ReadOnly);
  
  context->Global()->Set(v8::String::New("AHUACATL_PARSE"),
                         v8::FunctionTemplate::New(JS_ParseAhuacatl)->GetFunction(),
                         v8::ReadOnly);
  
  // .............................................................................
  // create the global variables
  // .............................................................................

  context->Global()->Set(v8::String::New("db"),
                         TRI_WrapVocBase(vocbase),
                         v8::ReadOnly);

  context->Global()->Set(v8::String::New("edges"),
                         TRI_WrapEdges(vocbase),
                         v8::ReadOnly);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
