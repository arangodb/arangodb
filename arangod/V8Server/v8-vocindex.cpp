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

#include "v8-vocindex.h"
#include "v8-vocbase.h"
#include "v8-vocbaseprivate.h"
#include "v8-collection.h"

#include "Basics/conversions.h"
#include "V8/v8-conv.h"
#include "Utils/transactions.h"
#include "Utils/V8TransactionContext.h"

#include "CapConstraint/cap-constraint.h"
#include "Utils/AhuacatlGuard.h"
#include "V8/v8-utils.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::arango;
using namespace triagens::rest;


////////////////////////////////////////////////////////////////////////////////
/// @brief extract the unique flag from the data
////////////////////////////////////////////////////////////////////////////////

bool ExtractBoolFlag (v8::Handle<v8::Object> const obj,
                      char const* name,
                      bool defaultValue) {
  // extract unique flag
  if (obj->Has(TRI_V8_SYMBOL(name))) {
    return TRI_ObjectToBoolean(obj->Get(TRI_V8_SYMBOL(name)));
  }

  return defaultValue;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if argument is an index identifier
////////////////////////////////////////////////////////////////////////////////

static bool IsIndexHandle (v8::Handle<v8::Value> const arg,
                           string& collectionName,
                           TRI_idx_iid_t& iid) {

  TRI_ASSERT(collectionName.empty());
  TRI_ASSERT(iid == 0);

  if (arg->IsNumber()) {
    // numeric index id
    iid = (TRI_idx_iid_t) arg->ToNumber()->Value();
    return true;
  }

  if (! arg->IsString()) {
    return false;
  }

  v8::String::Utf8Value str(arg);

  if (*str == 0) {
    return false;
  }

  size_t split;
  if (TRI_ValidateIndexIdIndex(*str, &split)) {
    collectionName = string(*str, split);
    iid = TRI_UInt64String2(*str + split + 1, str.length() - split - 1);
    return true;
  }

  if (TRI_ValidateIdIndex(*str)) {
    iid = TRI_UInt64String2(*str, str.length());
    return true;
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the index representation
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> IndexRep (string const& collectionName,
                                       TRI_json_t const* idx) {
  v8::HandleScope scope;

  TRI_ASSERT(idx != nullptr);

  v8::Handle<v8::Object> rep = TRI_ObjectJson(idx)->ToObject();

  string iid = TRI_ObjectToString(rep->Get(TRI_V8_SYMBOL("id")));
  string const id = collectionName + TRI_INDEX_HANDLE_SEPARATOR_STR + iid;
  rep->Set(TRI_V8_SYMBOL("id"), v8::String::New(id.c_str(), (int) id.size()));

  return scope.Close(rep);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief process the fields list and add them to the json
////////////////////////////////////////////////////////////////////////////////

int ProcessIndexFields (v8::Handle<v8::Object> const obj,
                        TRI_json_t* json,
                        int numFields,
                        bool create) {
  set<string> fields;

  v8::Handle<v8::String> fieldsString = v8::String::New("fields");
  if (obj->Has(fieldsString) && obj->Get(fieldsString)->IsArray()) {
    // "fields" is a list of fields
    v8::Handle<v8::Array> fieldList = v8::Handle<v8::Array>::Cast(obj->Get(fieldsString));

    uint32_t const n = fieldList->Length();

    for (uint32_t i = 0; i < n; ++i) {
      if (! fieldList->Get(i)->IsString()) {
        return TRI_ERROR_BAD_PARAMETER;
      }

      string const f = TRI_ObjectToString(fieldList->Get(i));

      if (f.empty() || (create && f[0] == '_')) {
        // accessing internal attributes is disallowed
        return TRI_ERROR_BAD_PARAMETER;
      }

      if (fields.find(f) != fields.end()) {
        // duplicate attribute name
        return TRI_ERROR_BAD_PARAMETER;
      }

      fields.insert(f);
    }
  }

  if (fields.empty() ||
      (numFields > 0 && (int) fields.size() != numFields)) {
    return TRI_ERROR_BAD_PARAMETER;
  }

  TRI_json_t* fieldJson = TRI_ObjectToJson(obj->Get(TRI_V8_SYMBOL("fields")));

  if (fieldJson == nullptr) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, json, "fields", fieldJson);

  return TRI_ERROR_NO_ERROR;
}
////////////////////////////////////////////////////////////////////////////////
/// @brief process the geojson flag and add it to the json
////////////////////////////////////////////////////////////////////////////////

int ProcessIndexGeoJsonFlag (v8::Handle<v8::Object> const obj,
                            TRI_json_t* json) {
  bool geoJson = ExtractBoolFlag(obj, "geoJson", false);
  TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, json, "geoJson", TRI_CreateBooleanJson(TRI_UNKNOWN_MEM_ZONE, geoJson));

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief process the unique flag and add it to the json
////////////////////////////////////////////////////////////////////////////////

int ProcessIndexUniqueFlag (v8::Handle<v8::Object> const obj,
                            TRI_json_t* json,
                            bool fillConstraint = false) {
  bool unique = ExtractBoolFlag(obj, "unique", false);
  TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, json, "unique", TRI_CreateBooleanJson(TRI_UNKNOWN_MEM_ZONE, unique));
  if (fillConstraint) {
    TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, json, "constraint", TRI_CreateBooleanJson(TRI_UNKNOWN_MEM_ZONE, unique));
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief process the ignoreNull flag and add it to the json
////////////////////////////////////////////////////////////////////////////////

int ProcessIndexIgnoreNullFlag (v8::Handle<v8::Object> const obj,
                                TRI_json_t* json) {
  bool ignoreNull = ExtractBoolFlag(obj, "ignoreNull", false);
  TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, json, "ignoreNull", TRI_CreateBooleanJson(TRI_UNKNOWN_MEM_ZONE, ignoreNull));

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief process the undefined flag and add it to the json
////////////////////////////////////////////////////////////////////////////////

int ProcessIndexUndefinedFlag (v8::Handle<v8::Object> const obj,
                               TRI_json_t* json) {
  bool undefined = ExtractBoolFlag(obj, "undefined", false);
  TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, json, "undefined", TRI_CreateBooleanJson(TRI_UNKNOWN_MEM_ZONE, undefined));

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief enhances the json of a geo1 index
////////////////////////////////////////////////////////////////////////////////

static int EnhanceJsonIndexGeo1 (v8::Handle<v8::Object> const obj,
                                 TRI_json_t* json,
                                 bool create) {
  int res = ProcessIndexFields(obj, json, 1, create);
  ProcessIndexUniqueFlag(obj, json, true);
  ProcessIndexIgnoreNullFlag(obj, json);
  ProcessIndexGeoJsonFlag(obj, json);
  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief enhances the json of a geo2 index
////////////////////////////////////////////////////////////////////////////////

static int EnhanceJsonIndexGeo2 (v8::Handle<v8::Object> const obj,
                                 TRI_json_t* json,
                                 bool create) {
  int res = ProcessIndexFields(obj, json, 2, create);
  ProcessIndexUniqueFlag(obj, json, true);
  ProcessIndexIgnoreNullFlag(obj, json);
  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief enhances the json of a hash index
////////////////////////////////////////////////////////////////////////////////

static int EnhanceJsonIndexHash (v8::Handle<v8::Object> const obj,
                                 TRI_json_t* json,
                                 bool create) {
  int res = ProcessIndexFields(obj, json, 0, create);
  ProcessIndexUniqueFlag(obj, json);
  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief enhances the json of a skiplist index
////////////////////////////////////////////////////////////////////////////////

static int EnhanceJsonIndexSkiplist (v8::Handle<v8::Object> const obj,
                                     TRI_json_t* json,
                                     bool create) {
  int res = ProcessIndexFields(obj, json, 0, create);
  ProcessIndexUniqueFlag(obj, json);
  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief enhances the json of a fulltext index
////////////////////////////////////////////////////////////////////////////////

static int EnhanceJsonIndexFulltext (v8::Handle<v8::Object> const obj,
                                     TRI_json_t* json,
                                     bool create) {
  int res = ProcessIndexFields(obj, json, 1, create);

  // handle "minLength" attribute
  int minWordLength = TRI_FULLTEXT_MIN_WORD_LENGTH_DEFAULT;
  if (obj->Has(TRI_V8_SYMBOL("minLength")) && obj->Get(TRI_V8_SYMBOL("minLength"))->IsNumber()) {
    minWordLength = (int) TRI_ObjectToInt64(obj->Get(TRI_V8_SYMBOL("minLength")));
  }
  TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, json, "minLength", TRI_CreateNumberJson(TRI_UNKNOWN_MEM_ZONE, minWordLength));

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief enhances the json of a cap constraint
////////////////////////////////////////////////////////////////////////////////

static int EnhanceJsonIndexCap (v8::Handle<v8::Object> const obj,
                                TRI_json_t* json) {
  // handle "size" attribute
  size_t count = 0;
  if (obj->Has(TRI_V8_SYMBOL("size")) && obj->Get(TRI_V8_SYMBOL("size"))->IsNumber()) {
    int64_t value = TRI_ObjectToInt64(obj->Get(TRI_V8_SYMBOL("size")));

    if (value < 0 || value > UINT32_MAX) {
      return TRI_ERROR_BAD_PARAMETER;
    }
    count = (size_t) value;
  }

  // handle "byteSize" attribute
  int64_t byteSize = 0;
  if (obj->Has(TRI_V8_SYMBOL("byteSize")) && obj->Get(TRI_V8_SYMBOL("byteSize"))->IsNumber()) {
    byteSize = TRI_ObjectToInt64(obj->Get(TRI_V8_SYMBOL("byteSize")));
  }

  if (count == 0 && byteSize <= 0) {
    return TRI_ERROR_BAD_PARAMETER;
  }

  if (byteSize < 0 || (byteSize > 0 && byteSize < TRI_CAP_CONSTRAINT_MIN_SIZE)) {
    return TRI_ERROR_BAD_PARAMETER;
  }

  TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, json, "size", TRI_CreateNumberJson(TRI_UNKNOWN_MEM_ZONE, (double) count));
  TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, json, "byteSize", TRI_CreateNumberJson(TRI_UNKNOWN_MEM_ZONE, (double) byteSize));

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief enhances the json of an index
////////////////////////////////////////////////////////////////////////////////

static int EnhanceIndexJson (v8::Arguments const& argv,
                             TRI_json_t*& json,
                             bool create) {
  v8::Handle<v8::Object> obj = argv[0].As<v8::Object>();

  // extract index type
  TRI_idx_type_e type = TRI_IDX_TYPE_UNKNOWN;

  if (obj->Has(TRI_V8_SYMBOL("type")) && obj->Get(TRI_V8_SYMBOL("type"))->IsString()) {
    TRI_Utf8ValueNFC typeString(TRI_UNKNOWN_MEM_ZONE, obj->Get(TRI_V8_SYMBOL("type")));

    if (*typeString == 0) {
      return TRI_ERROR_OUT_OF_MEMORY;
    }

    string t(*typeString);
    // rewrite type "geo" into either "geo1" or "geo2", depending on the number of fields
    if (t == "geo") {
      t = "geo1";

      if (obj->Has(TRI_V8_SYMBOL("fields")) && obj->Get(TRI_V8_SYMBOL("fields"))->IsArray()) {
        v8::Handle<v8::Array> f = v8::Handle<v8::Array>::Cast(obj->Get(TRI_V8_SYMBOL("fields")));
        if (f->Length() == 2) {
          t = "geo2";
        }
      }
    }

    type = TRI_TypeIndex(t.c_str());
  }

  if (type == TRI_IDX_TYPE_UNKNOWN) {
    return TRI_ERROR_BAD_PARAMETER;
  }

  if (create) {
    if (type == TRI_IDX_TYPE_PRIMARY_INDEX ||
        type == TRI_IDX_TYPE_EDGE_INDEX) {
      // creating these indexes yourself is forbidden
      return TRI_ERROR_FORBIDDEN;
    }
  }

  json = TRI_CreateArrayJson(TRI_UNKNOWN_MEM_ZONE);

  if (json == nullptr) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  if (obj->Has(TRI_V8_SYMBOL("id"))) {
    uint64_t id = TRI_ObjectToUInt64(obj->Get(TRI_V8_SYMBOL("id")), true);
    if (id > 0) {
      char* idString = TRI_StringUInt64(id);
      TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, json, "id", TRI_CreateStringCopyJson(TRI_UNKNOWN_MEM_ZONE, idString));
      TRI_FreeString(TRI_CORE_MEM_ZONE, idString);
    }
  }

  TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE,
                       json,
                       "type",
                       TRI_CreateStringCopyJson(TRI_UNKNOWN_MEM_ZONE, TRI_TypeNameIndex(type)));

  int res = TRI_ERROR_INTERNAL;

  switch (type) {
    case TRI_IDX_TYPE_UNKNOWN:
    case TRI_IDX_TYPE_PRIORITY_QUEUE_INDEX: {
      res = TRI_ERROR_BAD_PARAMETER;
      break;
    }

    case TRI_IDX_TYPE_PRIMARY_INDEX:
    case TRI_IDX_TYPE_EDGE_INDEX: 
    case TRI_IDX_TYPE_BITARRAY_INDEX: {
      break;
    }

    case TRI_IDX_TYPE_GEO1_INDEX:
      res = EnhanceJsonIndexGeo1(obj, json, create);
      break;
    case TRI_IDX_TYPE_GEO2_INDEX:
      res = EnhanceJsonIndexGeo2(obj, json, create);
      break;
    case TRI_IDX_TYPE_HASH_INDEX:
      res = EnhanceJsonIndexHash(obj, json, create);
      break;
    case TRI_IDX_TYPE_SKIPLIST_INDEX:
      res = EnhanceJsonIndexSkiplist(obj, json, create);
      break;
    case TRI_IDX_TYPE_FULLTEXT_INDEX:
      res = EnhanceJsonIndexFulltext(obj, json, create);
      break;
    case TRI_IDX_TYPE_CAP_CONSTRAINT:
      res = EnhanceJsonIndexCap(obj, json);
      break;
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures an index, coordinator case
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> EnsureIndexCoordinator (TRI_vocbase_col_t const* collection,
                                                     TRI_json_t const* json,
                                                     bool create) {
  v8::HandleScope scope;

  TRI_ASSERT(collection != 0);
  TRI_ASSERT(json != 0);

  string const databaseName(collection->_dbName);
  string const cid = StringUtils::itoa(collection->_cid);
  // TODO: protect against races on _name
  string const collectionName(collection->_name);

  TRI_json_t* resultJson = 0;
  string errorMsg;
  int res = ClusterInfo::instance()->ensureIndexCoordinator(databaseName,
                                                            cid,
                                                            json,
                                                            create,
                                                            &IndexComparator,
                                                            resultJson,
                                                            errorMsg,
                                                            360.0);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_EXCEPTION_MESSAGE(scope, res, errorMsg);
  }

  if (resultJson == 0) {
    if (! create) {
      // did not find a suitable index
      return scope.Close(v8::Null());
    }

    TRI_V8_EXCEPTION_MEMORY(scope);
  }

  v8::Handle<v8::Value> ret = IndexRep(collectionName, resultJson);
  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, resultJson);

  return scope.Close(ret);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures an index, locally
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> EnsureIndexLocal (TRI_vocbase_col_t const* collection,
                                               TRI_json_t const* json,
                                               bool create) {
  v8::HandleScope scope;

  TRI_ASSERT(collection != nullptr);
  TRI_ASSERT(json != nullptr);

  // extract type
  TRI_json_t* value = TRI_LookupArrayJson(json, "type");
  TRI_ASSERT(TRI_IsStringJson(value));

  TRI_idx_type_e type = TRI_TypeIndex(value->_value._string.data);

  // extract unique
  bool unique = false;
  value = TRI_LookupArrayJson(json, "unique");
  if (TRI_IsBooleanJson(value)) {
    unique = value->_value._boolean;
  }

  TRI_vector_pointer_t attributes;
  TRI_InitVectorPointer(&attributes, TRI_CORE_MEM_ZONE);

  TRI_vector_pointer_t values;
  TRI_InitVectorPointer(&values, TRI_CORE_MEM_ZONE);

  // extract id
  TRI_idx_iid_t iid = 0;
  value = TRI_LookupArrayJson(json, "id");
  if (TRI_IsStringJson(value)) {
    iid = TRI_UInt64String2(value->_value._string.data, value->_value._string.length - 1);
  }

  // extract fields
  value = TRI_LookupArrayJson(json, "fields");
  if (TRI_IsListJson(value)) {
    // note: "fields" is not mandatory for all index types

    // copy all field names (attributes)
    for (size_t i = 0; i < value->_value._objects._length; ++i) {
      TRI_json_t const* v = static_cast<TRI_json_t const*>(TRI_AtVector(&value->_value._objects, i));

      TRI_ASSERT(TRI_IsStringJson(v));
      TRI_PushBackVectorPointer(&attributes, v->_value._string.data);
    }
  }

  V8ReadTransaction trx(collection->_vocbase, collection->_cid);

  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_DestroyVectorPointer(&values);
    TRI_DestroyVectorPointer(&attributes);
    TRI_V8_EXCEPTION(scope, res);
  }


  TRI_document_collection_t* document = trx.documentCollection();
  const string collectionName = string(collection->_name);

  // disallow index creation in read-only mode
  if (! TRI_IsSystemNameCollection(collectionName.c_str()) 
      && create
      && TRI_GetOperationModeServer() == TRI_VOCBASE_MODE_NO_CREATE) {
    TRI_V8_EXCEPTION(scope, TRI_ERROR_ARANGO_READ_ONLY);
  }

  bool created = false;
  TRI_index_t* idx = 0;

  switch (type) {
    case TRI_IDX_TYPE_UNKNOWN:
    case TRI_IDX_TYPE_PRIMARY_INDEX:
    case TRI_IDX_TYPE_EDGE_INDEX:
    case TRI_IDX_TYPE_PRIORITY_QUEUE_INDEX: 
    case TRI_IDX_TYPE_BITARRAY_INDEX: {
      // these indexes cannot be created directly
      TRI_V8_EXCEPTION(scope, TRI_ERROR_INTERNAL);
    }

    case TRI_IDX_TYPE_GEO1_INDEX: {
      TRI_ASSERT(attributes._length == 1);

      bool ignoreNull = false;
      TRI_json_t* value = TRI_LookupArrayJson(json, "ignoreNull");
      if (TRI_IsBooleanJson(value)) {
        ignoreNull = value->_value._boolean;
      }

      bool geoJson = false;
      value = TRI_LookupArrayJson(json, "geoJson");
      if (TRI_IsBooleanJson(value)) {
        geoJson = value->_value._boolean;
      }

      if (create) {
        idx = TRI_EnsureGeoIndex1DocumentCollection(document,
                                                    iid,
                                                    (char const*) TRI_AtVectorPointer(&attributes, 0),
                                                    geoJson,
                                                    unique,
                                                    ignoreNull,
                                                    &created);
      }
      else {
        idx = TRI_LookupGeoIndex1DocumentCollection(document,
                                                    (char const*) TRI_AtVectorPointer(&attributes, 0),
                                                     geoJson,
                                                     unique,
                                                     ignoreNull);
      }
      break;
    }

    case TRI_IDX_TYPE_GEO2_INDEX: {
      TRI_ASSERT(attributes._length == 2);

      bool ignoreNull = false;
      TRI_json_t* value = TRI_LookupArrayJson(json, "ignoreNull");
      if (TRI_IsBooleanJson(value)) {
        ignoreNull = value->_value._boolean;
      }

      if (create) {
        idx = TRI_EnsureGeoIndex2DocumentCollection(document,
                                                    iid,
                                                    (char const*) TRI_AtVectorPointer(&attributes, 0),
                                                    (char const*) TRI_AtVectorPointer(&attributes, 1),
                                                    unique,
                                                    ignoreNull,
                                                    &created);
      }
      else {
        idx = TRI_LookupGeoIndex2DocumentCollection(document,
                                                    (char const*) TRI_AtVectorPointer(&attributes, 0),
                                                    (char const*) TRI_AtVectorPointer(&attributes, 1),
                                                     unique,
                                                     ignoreNull);
      }
      break;
    }

    case TRI_IDX_TYPE_HASH_INDEX: {
      TRI_ASSERT(attributes._length > 0);

      if (create) {
        idx = TRI_EnsureHashIndexDocumentCollection(document,
                                                    iid,
                                                    &attributes,
                                                    unique,
                                                    &created);
      }
      else {
        idx = TRI_LookupHashIndexDocumentCollection(document,
                                                    &attributes,
                                                    unique);
      }

      break;
    }

    case TRI_IDX_TYPE_SKIPLIST_INDEX: {
      TRI_ASSERT(attributes._length > 0);

      if (create) {
        idx = TRI_EnsureSkiplistIndexDocumentCollection(document,
                                                        iid,
                                                        &attributes,
                                                        unique,
                                                        &created);
      }
      else {
        idx = TRI_LookupSkiplistIndexDocumentCollection(document,
                                                        &attributes,
                                                        unique);
      }
      break;
    }

    case TRI_IDX_TYPE_FULLTEXT_INDEX: {
      TRI_ASSERT(attributes._length == 1);

      int minWordLength = TRI_FULLTEXT_MIN_WORD_LENGTH_DEFAULT;
      TRI_json_t* value = TRI_LookupArrayJson(json, "minLength");
      if (TRI_IsNumberJson(value)) {
        minWordLength = (int) value->_value._number;
      }

      if (create) {
        idx = TRI_EnsureFulltextIndexDocumentCollection(document,
                                                        iid,
                                                        (char const*) TRI_AtVectorPointer(&attributes, 0),
                                                        false,
                                                        minWordLength,
                                                        &created);
      }
      else {
        idx = TRI_LookupFulltextIndexDocumentCollection(document,
                                                        (char const*) TRI_AtVectorPointer(&attributes, 0),
                                                        false,
                                                        minWordLength);
      }
      break;
    }

    case TRI_IDX_TYPE_CAP_CONSTRAINT: {
      size_t size = 0;
      TRI_json_t* value = TRI_LookupArrayJson(json, "size");
      if (TRI_IsNumberJson(value)) {
        size = (size_t) value->_value._number;
      }

      int64_t byteSize = 0;
      value = TRI_LookupArrayJson(json, "byteSize");
      if (TRI_IsNumberJson(value)) {
        byteSize = (int64_t) value->_value._number;
      }

      if (create) {
        idx = TRI_EnsureCapConstraintDocumentCollection(document,
                                                        iid,
                                                        size,
                                                        byteSize,
                                                        &created);
      }
      else {
        idx = TRI_LookupCapConstraintDocumentCollection(document);
      }
      break;
    }
  }

  if (idx == 0 && create) {
    // something went wrong during creation
    int res = TRI_errno();
    TRI_DestroyVectorPointer(&values);
    TRI_DestroyVectorPointer(&attributes);

    TRI_V8_EXCEPTION(scope, res);
  }

  TRI_DestroyVectorPointer(&values);
  TRI_DestroyVectorPointer(&attributes);


  if (idx == 0 && ! create) {
    // no index found
    return scope.Close(v8::Null());
  }

  // found some index to return
  TRI_json_t* indexJson = idx->json(idx);

  if (indexJson == 0) {
    TRI_V8_EXCEPTION_MEMORY(scope);
  }

  v8::Handle<v8::Value> ret = IndexRep(collectionName, indexJson);
  TRI_FreeJson(TRI_CORE_MEM_ZONE, indexJson);

  if (ret->IsObject()) {
    ret->ToObject()->Set(v8::String::New("isNewlyCreated"), v8::Boolean::New(create && created));
  }

  return scope.Close(ret);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures an index
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> EnsureIndex (v8::Arguments const& argv,
                                          bool create,
                                          char const* functionName) {
  v8::HandleScope scope;

  TRI_vocbase_col_t* collection = TRI_UnwrapClass<TRI_vocbase_col_t>(argv.Holder(), WRP_VOCBASE_COL_TYPE);

  if (collection == nullptr) {
    TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract collection");
  }

  if (argv.Length() != 1 || ! argv[0]->IsObject()) {
    string name(functionName);
    name.append("(<description>)");
    TRI_V8_EXCEPTION_USAGE(scope, name.c_str());
  }

  TRI_json_t* json = nullptr;
  int res = EnhanceIndexJson(argv, json, create);


  if (res == TRI_ERROR_NO_ERROR &&
      ServerState::instance()->isCoordinator()) {
    string const dbname(collection->_dbName);
    // TODO: someone might rename the collection while we're reading its name...
    string const collname(collection->_name);
    shared_ptr<CollectionInfo> const& c = ClusterInfo::instance()->getCollection(dbname, collname);

    if (c->empty()) {
      TRI_V8_EXCEPTION(scope, TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
    }

    // check if there is an attempt to create a unique index on non-shard keys
    if (create) {
      TRI_json_t const* v = TRI_LookupArrayJson(json, "unique");

      if (TRI_IsBooleanJson(v) && v->_value._boolean) {
        // unique index, now check if fields and shard keys match
        TRI_json_t const* flds = TRI_LookupArrayJson(json, "fields");

        if (TRI_IsListJson(flds) && c->numberOfShards() > 1) {
          vector<string> const& shardKeys = c->shardKeys();
          size_t const n = flds->_value._objects._length;

          if (shardKeys.size() != n) {
            res = TRI_ERROR_CLUSTER_UNSUPPORTED;
          }
          else {
            for (size_t i = 0; i < n; ++i) {
              TRI_json_t const* f = TRI_LookupListJson(flds, i);

              if (! TRI_IsStringJson(f)) {
                res = TRI_ERROR_INTERNAL;
                continue;
              }
              else {
                if (! TRI_EqualString(f->_value._string.data, shardKeys[i].c_str())) {
                  res = TRI_ERROR_CLUSTER_UNSUPPORTED;
                }
              }
            }
          }
        }
      }
    }

  }

  if (res != TRI_ERROR_NO_ERROR) {
    if (json != nullptr) {
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
    }
    TRI_V8_EXCEPTION(scope, res);
  }

  TRI_ASSERT(json != nullptr);

  v8::Handle<v8::Value> ret;

  // ensure an index, coordinator case
  if (ServerState::instance()->isCoordinator()) {
    ret = EnsureIndexCoordinator(collection, json, create);
  }
  else {
    ret = EnsureIndexLocal(collection, json, create);
  }

  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);

  return scope.Close(ret);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a collection on the coordinator
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> CreateCollectionCoordinator (
                                  v8::Arguments const& argv,
                                  TRI_col_type_e collectionType,
                                  std::string const& databaseName,
                                  TRI_col_info_t& parameter,
                                  TRI_vocbase_t* vocbase) {
  v8::HandleScope scope;

  string const name = TRI_ObjectToString(argv[0]);

  if (! TRI_IsAllowedNameCollection(parameter._isSystem, name.c_str())) {
    TRI_V8_EXCEPTION(scope, TRI_ERROR_ARANGO_ILLEGAL_NAME);
  }

  bool allowUserKeys = true;
  uint64_t numberOfShards = 1;
  vector<string> shardKeys;

  // default shard key
  shardKeys.push_back("_key");

  string distributeShardsLike;

  if (2 <= argv.Length()) {
    if (! argv[1]->IsObject()) {
      TRI_V8_TYPE_ERROR(scope, "<properties> must be an object");
    }

    v8::Handle<v8::Object> p = argv[1]->ToObject();

    if (p->Has(TRI_V8_SYMBOL("keyOptions")) && p->Get(TRI_V8_SYMBOL("keyOptions"))->IsObject()) {
      v8::Handle<v8::Object> o = v8::Handle<v8::Object>::Cast(p->Get(TRI_V8_SYMBOL("keyOptions")));

      if (o->Has(TRI_V8_SYMBOL("type"))) {
        string const type = TRI_ObjectToString(o->Get(TRI_V8_SYMBOL("type")));

        if (type != "" && type != "traditional") {
          // invalid key generator
          TRI_V8_EXCEPTION_MESSAGE(scope,
                                   TRI_ERROR_CLUSTER_UNSUPPORTED,
                                   "non-traditional key generators are not supported for sharded collections");
        }
      }

      if (o->Has(TRI_V8_SYMBOL("allowUserKeys"))) {
        allowUserKeys = TRI_ObjectToBoolean(o->Get(TRI_V8_SYMBOL("allowUserKeys")));
      }
    }

    if (p->Has(TRI_V8_SYMBOL("numberOfShards"))) {
      numberOfShards = TRI_ObjectToUInt64(p->Get(TRI_V8_SYMBOL("numberOfShards")), false);
    }

    if (p->Has(TRI_V8_SYMBOL("shardKeys"))) {
      shardKeys.clear();

      if (p->Get(TRI_V8_SYMBOL("shardKeys"))->IsArray()) {
        v8::Handle<v8::Array> k = v8::Handle<v8::Array>::Cast(p->Get(TRI_V8_SYMBOL("shardKeys")));

        for (uint32_t i = 0 ; i < k->Length(); ++i) {
          v8::Handle<v8::Value> v = k->Get(i);
          if (v->IsString()) {
            string const key = TRI_ObjectToString(v);

            // system attributes are not allowed (except _key)
            if (! key.empty() && (key[0] != '_' || key == "_key")) {
              shardKeys.push_back(key);
            }
          }
        }
      }
    }

    if (p->Has(TRI_V8_SYMBOL("distributeShardsLike")) &&
        p->Get(TRI_V8_SYMBOL("distributeShardsLike"))->IsString()) {
      distributeShardsLike
        = TRI_ObjectToString(p->Get(TRI_V8_SYMBOL("distributeShardsLike")));
    }
  }

  if (numberOfShards == 0 || numberOfShards > 1000) {
    TRI_V8_EXCEPTION_PARAMETER(scope, "invalid number of shards");
  }

  if (shardKeys.empty() || shardKeys.size() > 8) {
    TRI_V8_EXCEPTION_PARAMETER(scope, "invalid number of shard keys");
  }

  ClusterInfo* ci = ClusterInfo::instance();

  // fetch a unique id for the new collection plus one for each shard to create
  uint64_t const id = ci->uniqid(1 + numberOfShards);

  // collection id is the first unique id we got
  string const cid = StringUtils::itoa(id);

  vector<string> dbServers;

  if (distributeShardsLike.empty()) {
    // fetch list of available servers in cluster, and shuffle them randomly
    dbServers = ci->getCurrentDBServers();

    if (dbServers.empty()) {
      TRI_V8_EXCEPTION_MESSAGE(scope, TRI_ERROR_INTERNAL, "no database servers found in cluster");
    }

    random_shuffle(dbServers.begin(), dbServers.end());
  }
  else {
    CollectionNameResolver resolver(vocbase);
    TRI_voc_cid_t otherCid 
      = resolver.getCollectionIdCluster(distributeShardsLike);
    shared_ptr<CollectionInfo> collInfo = ci->getCollection(databaseName,
                               triagens::basics::StringUtils::itoa(otherCid));
    auto shards = collInfo->shardIds();
    for (auto it = shards.begin(); it != shards.end(); ++it) {
      dbServers.push_back(it->second);
    }
    // FIXME: need to sort shards numerically and not alphabetically
  }

  // now create the shards
  std::map<std::string, std::string> shards;
  for (uint64_t i = 0; i < numberOfShards; ++i) {
    // determine responsible server
    string serverId = dbServers[i % dbServers.size()];

    // determine shard id
    string shardId = "s" + StringUtils::itoa(id + 1 + i);

    shards.insert(std::make_pair(shardId, serverId));
  }

  // now create the JSON for the collection
  TRI_json_t* json = TRI_CreateArrayJson(TRI_UNKNOWN_MEM_ZONE);

  if (json == nullptr) {
    TRI_V8_EXCEPTION(scope, TRI_ERROR_OUT_OF_MEMORY);
  }

  TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, json, "id", TRI_CreateString2CopyJson(TRI_UNKNOWN_MEM_ZONE, cid.c_str(), cid.size()));
  TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, json, "name", TRI_CreateString2CopyJson(TRI_UNKNOWN_MEM_ZONE, name.c_str(), name.size()));
  TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, json, "type", TRI_CreateNumberJson(TRI_UNKNOWN_MEM_ZONE, (int) collectionType));
  TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, json, "status", TRI_CreateNumberJson(TRI_UNKNOWN_MEM_ZONE, (int) TRI_VOC_COL_STATUS_LOADED));
  TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, json, "deleted", TRI_CreateBooleanJson(TRI_UNKNOWN_MEM_ZONE, parameter._deleted));
  TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, json, "doCompact", TRI_CreateBooleanJson(TRI_UNKNOWN_MEM_ZONE, parameter._doCompact));
  TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, json, "isSystem", TRI_CreateBooleanJson(TRI_UNKNOWN_MEM_ZONE, parameter._isSystem));
  TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, json, "isVolatile", TRI_CreateBooleanJson(TRI_UNKNOWN_MEM_ZONE, parameter._isVolatile));
  TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, json, "waitForSync", TRI_CreateBooleanJson(TRI_UNKNOWN_MEM_ZONE, parameter._waitForSync));
  TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, json, "journalSize", TRI_CreateNumberJson(TRI_UNKNOWN_MEM_ZONE, parameter._maximalSize));

  TRI_json_t* keyOptions = TRI_CreateArrayJson(TRI_UNKNOWN_MEM_ZONE);
  if (keyOptions != 0) {
    TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, keyOptions, "type", TRI_CreateStringCopyJson(TRI_UNKNOWN_MEM_ZONE, "traditional"));
    TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, keyOptions, "allowUserKeys", TRI_CreateBooleanJson(TRI_UNKNOWN_MEM_ZONE, allowUserKeys));

    TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, json, "keyOptions", TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, keyOptions));
  }

  TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, json, "shardKeys", JsonHelper::stringList(TRI_UNKNOWN_MEM_ZONE, shardKeys));
  TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, json, "shards", JsonHelper::stringObject(TRI_UNKNOWN_MEM_ZONE, shards));

  TRI_json_t* indexes = TRI_CreateListJson(TRI_UNKNOWN_MEM_ZONE);

  if (indexes == 0) {
    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
    TRI_V8_EXCEPTION(scope, TRI_ERROR_OUT_OF_MEMORY);
  }

  // create a dummy primary index
  TRI_index_t* idx = TRI_CreatePrimaryIndex(0);

  if (idx == 0) {
    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, indexes);
    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
    TRI_V8_EXCEPTION(scope, TRI_ERROR_OUT_OF_MEMORY);
  }

  TRI_json_t* idxJson = idx->json(idx);
  TRI_FreeIndex(idx);

  TRI_PushBack3ListJson(TRI_UNKNOWN_MEM_ZONE, indexes, TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, idxJson));
  TRI_FreeJson(TRI_CORE_MEM_ZONE, idxJson);

  if (collectionType == TRI_COL_TYPE_EDGE) {
    // create a dummy edge index
    idx = TRI_CreateEdgeIndex(0, id);

    if (idx == 0) {
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, indexes);
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);
      TRI_V8_EXCEPTION(scope, TRI_ERROR_OUT_OF_MEMORY);
    }

    idxJson = idx->json(idx);
    TRI_FreeIndex(idx);

    TRI_PushBack3ListJson(TRI_UNKNOWN_MEM_ZONE, indexes, TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, idxJson));
    TRI_FreeJson(TRI_CORE_MEM_ZONE, idxJson);
  }

  TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, json, "indexes", indexes);

  string errorMsg;
  int myerrno = ci->createCollectionCoordinator(databaseName,
                                                cid,
                                                numberOfShards,
                                                json,
                                                errorMsg,
                                                240.0);

  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);

  if (myerrno != TRI_ERROR_NO_ERROR) {
    TRI_V8_EXCEPTION_MESSAGE(scope, myerrno, errorMsg);
  }
  ci->loadPlannedCollections();

  shared_ptr<CollectionInfo> const& c = ci->getCollection(databaseName, cid);
  TRI_vocbase_col_t* newcoll = CoordinatorCollection(vocbase, *c);
  return scope.Close(WrapCollection(newcoll));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures that an index exists
/// @startDocuBlock collectionEnsureIndex
/// `collection.ensureIndex(index-description)`
///
/// Ensures that an index according to the *index-description* exists. A
/// new index will be created if none exists with the given description.
///
/// The *index-description* must contain at least a *type* attribute.
/// *type* can be one of the following values:
/// - *hash*: hash index
/// - *skiplist*: skiplist index
/// - *fulltext*: fulltext index
/// - *geo1*: geo index, with one attribute
/// - *geo2*: geo index, with two attributes
/// - *cap*: cap constraint
///
/// Other attributes may be necessary, depending on the index type.
///
/// Calling this method returns an index object. Whether or not the index
/// object existed before the call is indicated in the return attribute
/// *isNewlyCreated*.
///
/// @EXAMPLES
///
/// ```js
/// arango> db.example.ensureIndex({ type: "hash", fields: [ "name" ], unique: true });
/// {
///   "id" : "example/30242599562",
///   "type" : "hash",
///   "unique" : true,
///   "fields" : [
///     "name"
///    ],
///   "isNewlyCreated" : true
/// }
/// ```
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_EnsureIndexVocbaseCol (v8::Arguments const& argv) {
  v8::HandleScope scope;

  PREVENT_EMBEDDED_TRANSACTION(scope);

  return scope.Close(EnsureIndex(argv, true, "ensureIndex"));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up an index
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_LookupIndexVocbaseCol (v8::Arguments const& argv) {
  v8::HandleScope scope;

  return scope.Close(EnsureIndex(argv, false, "lookupIndex"));
}
////////////////////////////////////////////////////////////////////////////////
/// @brief drops an index, coordinator case
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> DropIndexCoordinator (TRI_vocbase_col_t const* collection,
                                                   v8::Handle<v8::Value> const val) {
  v8::HandleScope scope;

  string collectionName;
  TRI_idx_iid_t iid = 0;

  // extract the index identifier from a string
  if (val->IsString() || val->IsStringObject() || val->IsNumber()) {
    if (! IsIndexHandle(val, collectionName, iid)) {
      TRI_V8_EXCEPTION(scope, TRI_ERROR_ARANGO_INDEX_HANDLE_BAD);
    }
  }

  // extract the index identifier from an object
  else if (val->IsObject()) {
    TRI_v8_global_t* v8g = (TRI_v8_global_t*) v8::Isolate::GetCurrent()->GetData();

    v8::Handle<v8::Object> obj = val->ToObject();
    v8::Handle<v8::Value> iidVal = obj->Get(v8g->IdKey);

    if (! IsIndexHandle(iidVal, collectionName, iid)) {
      TRI_V8_EXCEPTION(scope, TRI_ERROR_ARANGO_INDEX_HANDLE_BAD);
    }
  }

  if (! collectionName.empty()) {
    CollectionNameResolver resolver(collection->_vocbase);

    if (! EqualCollection(&resolver, collectionName, collection)) {
      TRI_V8_EXCEPTION(scope, TRI_ERROR_ARANGO_CROSS_COLLECTION_REQUEST);
    }
  }


  string const databaseName(collection->_dbName);
  string const cid = StringUtils::itoa(collection->_cid);
  string errorMsg;

  int res = ClusterInfo::instance()->dropIndexCoordinator(databaseName, cid, iid, errorMsg, 0.0);

  return scope.Close(v8::Boolean::New(res == TRI_ERROR_NO_ERROR));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief drops an index
/// @startDocuBlock col_dropIndex
/// `collection.dropIndex(index)`
///
/// Drops the index. If the index does not exist, then *false* is
/// returned. If the index existed and was dropped, then *true* is
/// returned. Note that you cannot drop some special indexes (e.g. the primary
/// index of a collection or the edge index of an edge collection).
///
/// `collection.dropIndex(index-handle)`
///
/// Same as above. Instead of an index an index handle can be given.
///
/// @EXAMPLES
///
/// ```js
/// arango> db.example.ensureSkiplist("a", "b");
/// { "id" : "example/991154", "unique" : false, "type" : "skiplist", "fields" : ["a", "b"], "isNewlyCreated" : true }
/// 
/// arango> i = db.example.getIndexes();
/// [
///   { "id" : "example/0", "type" : "primary", "fields" : ["_id"] },
///   { "id" : "example/991154", "unique" : false, "type" : "skiplist", "fields" : ["a", "b"] }
///   ]
/// 
/// arango> db.example.dropIndex(i[0])
/// false
/// 
/// arango> db.example.dropIndex(i[1].id)
/// true
/// 
/// arango> i = db.example.getIndexes();
/// [{ "id" : "example/0", "type" : "primary", "fields" : ["_id"] }]
/// ```
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_DropIndexVocbaseCol (v8::Arguments const& argv) {
  v8::HandleScope scope;

  PREVENT_EMBEDDED_TRANSACTION(scope);

  TRI_vocbase_col_t* collection = TRI_UnwrapClass<TRI_vocbase_col_t>(argv.Holder(), WRP_VOCBASE_COL_TYPE);

  if (collection == nullptr) {
    TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract collection");
  }

  if (argv.Length() != 1) {
    TRI_V8_EXCEPTION_USAGE(scope, "dropIndex(<index-handle>)");
  }

  if (ServerState::instance()->isCoordinator()) {
    return scope.Close(DropIndexCoordinator(collection, argv[0]));
  }

  V8ReadTransaction trx(collection->_vocbase, collection->_cid);

  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_EXCEPTION(scope, res);
  }

  TRI_document_collection_t* document = trx.documentCollection();

  v8::Handle<v8::Object> err;
  TRI_index_t* idx = TRI_LookupIndexByHandle(trx.resolver(), collection, argv[0], true, &err);

  if (idx == nullptr) {
    if (err.IsEmpty()) {
      return scope.Close(v8::False());
    }
    else {
      return scope.Close(v8::ThrowException(err));
    }
  }

  if (idx->_iid == 0) {
    return scope.Close(v8::False());
  }

  if (idx->_type == TRI_IDX_TYPE_PRIMARY_INDEX ||
      idx->_type == TRI_IDX_TYPE_EDGE_INDEX) {
    TRI_V8_EXCEPTION(scope, TRI_ERROR_FORBIDDEN);
  }

  // .............................................................................
  // inside a write transaction, write-lock is acquired by TRI_DropIndex...
  // .............................................................................

  bool ok = TRI_DropIndexDocumentCollection(document, idx->_iid, true);

  // .............................................................................
  // outside a write transaction
  // .............................................................................

  return scope.Close(v8::Boolean::New(ok));
}
////////////////////////////////////////////////////////////////////////////////
/// @brief returns information about the indexes, coordinator case
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> GetIndexesCoordinator (TRI_vocbase_col_t const* collection) {
  v8::HandleScope scope;

  string const databaseName(collection->_dbName);
  string const cid = StringUtils::itoa(collection->_cid);
  string const collectionName(collection->_name);

  shared_ptr<CollectionInfo> c = ClusterInfo::instance()->getCollection(databaseName, cid);

  if ((*c).empty()) {
    TRI_V8_EXCEPTION(scope, TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
  }

  v8::Handle<v8::Array> ret = v8::Array::New();

  TRI_json_t const* json = (*c).getIndexes();
  if (TRI_IsListJson(json)) {
    uint32_t j = 0;

    for (size_t i = 0;  i < json->_value._objects._length; ++i) {
      TRI_json_t const* v = TRI_LookupListJson(json, i);

      if (v != nullptr) {
        ret->Set(j++, IndexRep(collectionName, v));
      }
    }
  }

  return scope.Close(ret);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns information about the indexes
/// @startDocuBlock collectionGetIndexes
/// `getIndexes()`
///
/// Returns a list of all indexes defined for the collection.
///
/// @EXAMPLES
///
/// ```js
/// [
///   { 
///     "id" : "demo/0", 
///     "type" : "primary",
///     "fields" : [ "_id" ]
///   }, 
///   { 
///     "id" : "demo/2290971", 
///     "unique" : true, 
///     "type" : "hash", 
///     "fields" : [ "a" ] 
///   }, 
///   { 
///     "id" : "demo/2946331",
///     "unique" : false, 
///     "type" : "hash", 
///     "fields" : [ "b" ] 
///   },
///   { 
///     "id" : "demo/3077403", 
///     "unique" : false, 
///     "type" : "skiplist", 
///     "fields" : [ "c" ]
///   }
/// ]
/// ```
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_GetIndexesVocbaseCol (v8::Arguments const& argv) {
  v8::HandleScope scope;

  TRI_vocbase_col_t* collection = TRI_UnwrapClass<TRI_vocbase_col_t>(argv.Holder(), WRP_VOCBASE_COL_TYPE);

  if (collection == nullptr) {
    TRI_V8_EXCEPTION_INTERNAL(scope, "cannot extract collection");
  }

  if (ServerState::instance()->isCoordinator()) {
    return scope.Close(GetIndexesCoordinator(collection));
  }

  V8ReadTransaction trx(collection->_vocbase, collection->_cid);

  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_EXCEPTION(scope, res);
  }

  // READ-LOCK start
  trx.lockRead();

  TRI_document_collection_t* document = trx.documentCollection();
  const string collectionName = string(collection->_name);

  // get list of indexes
  TRI_vector_pointer_t* indexes = TRI_IndexesDocumentCollection(document);

  trx.finish(res);
  // READ-LOCK end

  if (indexes == nullptr) {
    TRI_V8_EXCEPTION_MEMORY(scope);
  }

  v8::Handle<v8::Array> result = v8::Array::New();

  uint32_t n = (uint32_t) indexes->_length;

  for (uint32_t i = 0, j = 0;  i < n;  ++i) {
    TRI_json_t* idx = static_cast<TRI_json_t*>(indexes->_buffer[i]);

    if (idx != nullptr) {
      result->Set(j++, IndexRep(collectionName, idx));
      TRI_FreeJson(TRI_CORE_MEM_ZONE, idx);
    }
  }

  TRI_FreeVectorPointer(TRI_CORE_MEM_ZONE, indexes);

  return scope.Close(result);
}


////////////////////////////////////////////////////////////////////////////////
/// @brief looks up an index identifier
////////////////////////////////////////////////////////////////////////////////

TRI_index_t* TRI_LookupIndexByHandle (CollectionNameResolver const* resolver,
                                      TRI_vocbase_col_t const* collection,
                                      v8::Handle<v8::Value> const val,
                                      bool ignoreNotFound,
                                      v8::Handle<v8::Object>* err) {
  // reset the collection identifier
  string collectionName;
  TRI_idx_iid_t iid = 0;

  // assume we are already loaded
  TRI_ASSERT(collection != nullptr);
  TRI_ASSERT(collection->_collection != nullptr);

  // extract the index identifier from a string
  if (val->IsString() || val->IsStringObject() || val->IsNumber()) {
    if (! IsIndexHandle(val, collectionName, iid)) {
      *err = TRI_CreateErrorObject(__FILE__, __LINE__, TRI_ERROR_ARANGO_INDEX_HANDLE_BAD);
      return nullptr;
    }
  }

  // extract the index identifier from an object
  else if (val->IsObject()) {
    TRI_v8_global_t* v8g = static_cast<TRI_v8_global_t*>(v8::Isolate::GetCurrent()->GetData());

    v8::Handle<v8::Object> obj = val->ToObject();
    v8::Handle<v8::Value> iidVal = obj->Get(v8g->IdKey);

    if (! IsIndexHandle(iidVal, collectionName, iid)) {
      *err = TRI_CreateErrorObject(__FILE__, __LINE__, TRI_ERROR_ARANGO_INDEX_HANDLE_BAD);
      return nullptr;
    }
  }

  if (! collectionName.empty()) {
    if (! EqualCollection(resolver, collectionName, collection)) {
      // I wish this error provided me with more information!
      // e.g. 'cannot access index outside the collection it was defined in'
      *err = TRI_CreateErrorObject(__FILE__, __LINE__, TRI_ERROR_ARANGO_CROSS_COLLECTION_REQUEST);
      return nullptr;
    }
  }

  TRI_index_t* idx = TRI_LookupIndex(collection->_collection, iid);

  if (idx == nullptr) {
    if (! ignoreNotFound) {
      *err = TRI_CreateErrorObject(__FILE__, __LINE__, TRI_ERROR_ARANGO_INDEX_NOT_FOUND);
    }
  }

  return idx;
}


////////////////////////////////////////////////////////////////////////////////
/// @brief create a collection
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> CreateVocBase (v8::Arguments const& argv,
                                            TRI_col_type_e collectionType) {
  v8::HandleScope scope;

  TRI_vocbase_t* vocbase = GetContextVocBase();

  if (vocbase == nullptr) {
    TRI_V8_EXCEPTION(scope, TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  // ...........................................................................
  // We require exactly 1 or exactly 2 arguments -- anything else is an error
  // ...........................................................................

  if (argv.Length() < 1 || argv.Length() > 2) {
    TRI_V8_EXCEPTION_USAGE(scope, "_create(<name>, <properties>)");
  }

  if (TRI_GetOperationModeServer() == TRI_VOCBASE_MODE_NO_CREATE) {
    TRI_V8_EXCEPTION(scope, TRI_ERROR_ARANGO_READ_ONLY);
  }


  PREVENT_EMBEDDED_TRANSACTION(scope);


  // set default journal size
  TRI_voc_size_t effectiveSize = vocbase->_settings.defaultMaximalSize;

  // extract the name
  string const name = TRI_ObjectToString(argv[0]);

  // extract the parameters
  TRI_col_info_t parameter;
  TRI_voc_cid_t cid = 0;

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
    TRI_json_t* keyOptions = nullptr;
    if (p->Has(v8g->KeyOptionsKey)) {
      keyOptions = TRI_ObjectToJson(p->Get(v8g->KeyOptionsKey));
    }

    // TRI_InitCollectionInfo will copy keyOptions
    TRI_InitCollectionInfo(vocbase, &parameter, name.c_str(), collectionType, effectiveSize, keyOptions);

    if (keyOptions != nullptr) {
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, keyOptions);
    }

    if (p->Has(v8::String::New("planId"))) {
      parameter._planId = TRI_ObjectToUInt64(p->Get(v8::String::New("planId")), true);
    }

    if (p->Has(v8g->WaitForSyncKey)) {
      parameter._waitForSync = TRI_ObjectToBoolean(p->Get(v8g->WaitForSyncKey));
    }

    if (p->Has(v8g->DoCompactKey)) {
      parameter._doCompact = TRI_ObjectToBoolean(p->Get(v8g->DoCompactKey));
    }
    else {
      // default value for compaction
      parameter._doCompact = true;
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
    
    if (p->Has(v8g->IdKey)) {
      // specify collection id - used for testing only
      cid = TRI_ObjectToUInt64(p->Get(v8g->IdKey), true);
    }

  }
  else {
    TRI_InitCollectionInfo(vocbase, &parameter, name.c_str(), collectionType, effectiveSize, nullptr);
  }


  if (ServerState::instance()->isCoordinator()) {
    v8::Handle<v8::Value> result = CreateCollectionCoordinator(argv, collectionType, vocbase->_name, parameter, vocbase);
    TRI_FreeCollectionInfoOptions(&parameter);

    return scope.Close(result);
  }

  TRI_vocbase_col_t const* collection = TRI_CreateCollectionVocBase(vocbase,
                                                                    &parameter,
                                                                    cid, 
                                                                    true);

  TRI_FreeCollectionInfoOptions(&parameter);

  if (collection == nullptr) {
    TRI_V8_EXCEPTION_MESSAGE(scope, TRI_errno(), "cannot create collection");
  }

  v8::Handle<v8::Value> result = WrapCollection(collection);

  if (result.IsEmpty()) {
    TRI_V8_EXCEPTION_MEMORY(scope);
  }

  return scope.Close(result);
}


////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new document or edge collection
/// @startDocuBlock collectionDatabaseCreate
/// `db._create(collection-name)`
///
/// Creates a new document collection named *collection-name*.
/// If the collection name already exists or if the name format is invalid, an
/// error is thrown. For more information on valid collection names please refer
/// to the [naming conventions](../NamingConvention/README.md).
///
/// `db._create(collection-name, properties)`
///
/// *properties* must be an object with the following attributes:
///
/// * *waitForSync* (optional, default *false*): If *true* creating
///   a document will only return after the data was synced to disk.
///
/// * *journalSize* (optional, default is a
///   [configuration parameter](../CommandLineOptions/Arangod.md): The maximal
///   size of a journal or datafile.  Note that this also limits the maximal
///   size of a single object. Must be at least 1MB.
///
/// * *isSystem* (optional, default is *false*): If *true*, create a
///   system collection. In this case *collection-name* should start with
///   an underscore. End users should normally create non-system collections
///   only. API implementors may be required to create system collections in
///   very special occasions, but normally a regular collection will do.
///
/// * *isVolatile* (optional, default is *false*): If *true then the
///   collection data is kept in-memory only and not made persistent. Unloading
///   the collection will cause the collection data to be discarded. Stopping
///   or re-starting the server will also cause full loss of data in the
///   collection. Setting this option will make the resulting collection be
///   slightly faster than regular collections because ArangoDB does not
///   enforce any synchronization to disk and does not calculate any CRC
///   checksums for datafiles (as there are no datafiles).
///
/// * *keyOptions* (optional): additional options for key generation. If
///   specified, then *keyOptions* should be a JSON array containing the
///   following attributes (**note**: some of them are optional):
///   * *type*: specifies the type of the key generator. The currently
///     available generators are *traditional* and *autoincrement*.
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
/// * *numberOfShards* (optional, default is *1*): in a cluster, this value
///   determines the number of shards to create for the collection. In a single
///   server setup, this option is meaningless.
///
/// * *shardKeys* (optional, default is *[ "_key" ]*): in a cluster, this
///   attribute determines which document attributes are used to determine the
///   target shard for documents. Documents are sent to shards based on the
///   values they have in their shard key attributes. The values of all shard
///   key attributes in a document are hashed, and the hash value is used to
///   determine the target shard. Note that values of shard key attributes cannot
///   be changed once set.
///   This option is meaningless in a single server setup.
///
///   When choosing the shard keys, one must be aware of the following
///   rules and limitations: In a sharded collection with more than
///   one shard it is not possible to set up a unique constraint on
///   an attribute that is not the one and only shard key given in
///   *shardKeys*. This is because enforcing a unique constraint
///   would otherwise make a global index necessary or need extensive
///   communication for every single write operation. Furthermore, if
///   *_key* is not the one and only shard key, then it is not possible
///   to set the *_key* attribute when inserting a document, provided
///   the collection has more than one shard. Again, this is because
///   the database has to enforce the unique constraint on the *_key*
///   attribute and this can only be done efficiently if this is the
///   only shard key by delegating to the individual shards.
///
/// `db._create(collection-name, properties, type)`
///
/// Specifies the optional *type* of the collection, it can either be *document* 
/// or *edge*. On default it is document. Instead of giving a type you can also use 
/// *db._createEdgeCollection* or *db._createDocumentCollection*.
///
/// @EXAMPLES
///
/// With defaults:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{collectionDatabaseCreate}
///   c = db._create("users");
///   c.properties();
/// ~ db._drop("users");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// With properties:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{collectionDatabaseCreateProperties}
///   c = db._create("users", { waitForSync : true, journalSize : 1024 * 1204 });
///   c.properties();
/// ~ db._drop("users");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// With a key generator:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{collectionDatabaseCreateKey}
///   db._create("users", { keyOptions: { type: "autoincrement", offset: 10, increment: 5 } });
///   db.users.save({ name: "user 1" });
///   db.users.save({ name: "user 2" });
///   db.users.save({ name: "user 3" });
/// ~ db._drop("users");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// With a special key option:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{collectionDatabaseCreateSpecialKey}
///   db._create("users", { keyOptions: { allowUserKeys: false } });
///   db.users.save({ name: "user 1" });
///   db.users.save({ name: "user 2", _key: "myuser" });
///   db.users.save({ name: "user 3" });
/// ~ db._drop("users");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_CreateVocbase (v8::Arguments const& argv) {
  return CreateVocBase(argv, TRI_COL_TYPE_DOCUMENT);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new document collection
/// @startDocuBlock collectionCreateDocumentCollection
/// `db._createDocumentCollection(collection-name)`
///
/// Creates a new document collection named *collection-name*. If the
/// document name already exists and error is thrown. 
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_CreateDocumentCollectionVocbase (v8::Arguments const& argv) {
  return CreateVocBase(argv, TRI_COL_TYPE_DOCUMENT);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new edge collection
/// @startDocuBlock collectionCreateEdgeCollection
/// `db._createEdgeCollection(collection-name)`
///
/// Creates a new edge collection named *collection-name*. If the
/// collection name already exists an error is thrown. The default value
/// for *waitForSync* is *false*.
///
/// `db._createEdgeCollection(collection-name, properties)`
///
/// *properties* must be an object with the following attributes:
///
/// * *waitForSync* (optional, default *false*): If *true* creating
///   a document will only return after the data was synced to disk.
/// * *journalSize* (optional, default is 
///   "configuration parameter"):  The maximal size of
///   a journal or datafile.  Note that this also limits the maximal
///   size of a single object and must be at least 1MB.
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> JS_CreateEdgeCollectionVocbase (v8::Arguments const& argv) {
  return CreateVocBase(argv, TRI_COL_TYPE_EDGE);
}

void TRI_InitV8indexArangoDB (v8::Handle<v8::Context> context,
                              TRI_server_t* server,
                              TRI_vocbase_t* vocbase,
                              JSLoader* loader,
                              const size_t threadNumber,
                              TRI_v8_global_t* v8g,
                              v8::Handle<v8::ObjectTemplate> rt){

  TRI_AddMethodVocbase(rt, "_create", JS_CreateVocbase, true);
  TRI_AddMethodVocbase(rt, "_createEdgeCollection", JS_CreateEdgeCollectionVocbase);
  TRI_AddMethodVocbase(rt, "_createDocumentCollection", JS_CreateDocumentCollectionVocbase);

}


void TRI_InitV8indexCollection (v8::Handle<v8::Context> context,
                                TRI_server_t* server,
                                TRI_vocbase_t* vocbase,
                                JSLoader* loader,
                                const size_t threadNumber,
                                TRI_v8_global_t* v8g,
                                v8::Handle<v8::ObjectTemplate> rt){

  TRI_AddMethodVocbase(rt, "dropIndex", JS_DropIndexVocbaseCol);
  TRI_AddMethodVocbase(rt, "ensureIndex", JS_EnsureIndexVocbaseCol);
  TRI_AddMethodVocbase(rt, "lookupIndex", JS_LookupIndexVocbaseCol);
  TRI_AddMethodVocbase(rt, "getIndexes", JS_GetIndexesVocbaseCol);

}
