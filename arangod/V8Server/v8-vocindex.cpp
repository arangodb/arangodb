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

#include "v8-vocindex.h"
#include "Basics/conversions.h"
#include "Basics/StringUtils.h"
#include "Basics/tri-strings.h"
#include "Basics/VelocyPackHelper.h"
#include "FulltextIndex/fulltext-index.h"
#include "Indexes/EdgeIndex.h"
#include "Indexes/FulltextIndex.h"
#include "Indexes/GeoIndex2.h"
#include "Indexes/HashIndex.h"
#include "Indexes/Index.h"
#include "Indexes/PrimaryIndex.h"
#include "Indexes/SkiplistIndex.h"
#include "Utils/SingleCollectionTransaction.h"
#include "Utils/V8TransactionContext.h"
#include "V8/v8-conv.h"
#include "V8/v8-globals.h"
#include "V8/v8-utils.h"
#include "V8/v8-vpack.h"
#include "V8Server/v8-collection.h"
#include "V8Server/v8-vocbase.h"
#include "V8Server/v8-vocbaseprivate.h"

#ifdef ARANGODB_ENABLE_ROCKSDB
#include "Indexes/RocksDBIndex.h"
#endif

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

////////////////////////////////////////////////////////////////////////////////
/// @brief extract the unique flag from the data
////////////////////////////////////////////////////////////////////////////////

static bool ExtractBoolFlag(v8::Isolate* isolate,
                            v8::Handle<v8::Object> const obj,
                            v8::Handle<v8::String> name, bool defaultValue) {
  // extract unique flag
  if (obj->Has(name)) {
    return TRI_ObjectToBoolean(obj->Get(name));
  }

  return defaultValue;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if argument is an index identifier
////////////////////////////////////////////////////////////////////////////////

static bool IsIndexHandle(v8::Handle<v8::Value> const arg,
                          std::string& collectionName, TRI_idx_iid_t& iid) {
  TRI_ASSERT(collectionName.empty());
  TRI_ASSERT(iid == 0);

  if (arg->IsNumber()) {
    // numeric index id
    iid = (TRI_idx_iid_t)arg->ToNumber()->Value();
    return true;
  }

  if (!arg->IsString()) {
    return false;
  }

  v8::String::Utf8Value str(arg);

  if (*str == nullptr) {
    return false;
  }

  size_t split;
  if (arangodb::Index::validateHandle(*str, &split)) {
    collectionName = std::string(*str, split);
    iid = StringUtils::uint64(*str + split + 1, str.length() - split - 1);
    return true;
  }

  if (arangodb::Index::validateId(*str)) {
    iid = StringUtils::uint64(*str, str.length());
    return true;
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the index representation
////////////////////////////////////////////////////////////////////////////////

static v8::Handle<v8::Value> IndexRep(v8::Isolate* isolate,
                                      std::string const& collectionName,
                                      VPackSlice const& idx) {
  v8::EscapableHandleScope scope(isolate);
  TRI_ASSERT(!idx.isNone());

  v8::Handle<v8::Object> rep = TRI_VPackToV8(isolate, idx)->ToObject();

  std::string iid = TRI_ObjectToString(rep->Get(TRI_V8_ASCII_STRING("id")));
  std::string const id = collectionName + TRI_INDEX_HANDLE_SEPARATOR_STR + iid;
  rep->Set(TRI_V8_ASCII_STRING("id"), TRI_V8_STD_STRING(id));

  return scope.Escape<v8::Value>(rep);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief process the fields list and add them to the json
////////////////////////////////////////////////////////////////////////////////

static int ProcessIndexFields(v8::Isolate* isolate,
                              v8::Handle<v8::Object> const obj,
                              VPackBuilder& builder, int numFields,
                              bool create) {
  v8::HandleScope scope(isolate);
  std::set<std::string> fields;

  v8::Handle<v8::String> fieldsString = TRI_V8_ASCII_STRING("fields");
  if (obj->Has(fieldsString) && obj->Get(fieldsString)->IsArray()) {
    // "fields" is a list of fields
    v8::Handle<v8::Array> fieldList =
        v8::Handle<v8::Array>::Cast(obj->Get(fieldsString));

    uint32_t const n = fieldList->Length();

    for (uint32_t i = 0; i < n; ++i) {
      if (!fieldList->Get(i)->IsString()) {
        return TRI_ERROR_BAD_PARAMETER;
      }

      std::string const f = TRI_ObjectToString(fieldList->Get(i));

      if (f.empty() || (create && f == StaticStrings::IdString)) {
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

  if (fields.empty() || (numFields > 0 && (int)fields.size() != numFields)) {
    return TRI_ERROR_BAD_PARAMETER;
  }

  try {
    builder.add(VPackValue("fields"));
    int res = TRI_V8ToVPack(isolate, builder,
                            obj->Get(TRI_V8_ASCII_STRING("fields")), false);
    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }
  } catch (...) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief process the geojson flag and add it to the json
////////////////////////////////////////////////////////////////////////////////

static void ProcessIndexGeoJsonFlag(v8::Isolate* isolate,
                                    v8::Handle<v8::Object> const obj,
                                    VPackBuilder& builder) {
  v8::HandleScope scope(isolate);
  bool geoJson =
      ExtractBoolFlag(isolate, obj, TRI_V8_ASCII_STRING("geoJson"), false);
  builder.add("geoJson", VPackValue(geoJson));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief process the sparse flag and add it to the json
////////////////////////////////////////////////////////////////////////////////

static void ProcessIndexSparseFlag(v8::Isolate* isolate,
                                   v8::Handle<v8::Object> const obj,
                                   VPackBuilder& builder, bool create) {
  v8::HandleScope scope(isolate);
  if (obj->Has(TRI_V8_ASCII_STRING("sparse"))) {
    bool sparse =
        ExtractBoolFlag(isolate, obj, TRI_V8_ASCII_STRING("sparse"), false);
    builder.add("sparse", VPackValue(sparse));
  } else if (create) {
    // not set. now add a default value
    builder.add("sparse", VPackValue(false));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief process the unique flag and add it to the json
////////////////////////////////////////////////////////////////////////////////

static void ProcessIndexUniqueFlag(v8::Isolate* isolate,
                                   v8::Handle<v8::Object> const obj,
                                   VPackBuilder& builder) {
  v8::HandleScope scope(isolate);
  bool unique =
      ExtractBoolFlag(isolate, obj, TRI_V8_ASCII_STRING("unique"), false);
  builder.add("unique", VPackValue(unique));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief enhances the json of a geo1 index
////////////////////////////////////////////////////////////////////////////////

static int EnhanceJsonIndexGeo1(v8::Isolate* isolate,
                                v8::Handle<v8::Object> const obj,
                                VPackBuilder& builder, bool create) {
  int res = ProcessIndexFields(isolate, obj, builder, 1, create);
  if (ServerState::instance()->isCoordinator()) {
    builder.add("ignoreNull", VPackValue(true));
    builder.add("constraint", VPackValue(false));
  }
  builder.add("sparse", VPackValue(true));
  builder.add("unique", VPackValue(false));
  ProcessIndexGeoJsonFlag(isolate, obj, builder);
  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief enhances the json of a geo2 index
////////////////////////////////////////////////////////////////////////////////

static int EnhanceJsonIndexGeo2(v8::Isolate* isolate,
                                v8::Handle<v8::Object> const obj,
                                VPackBuilder& builder, bool create) {
  int res = ProcessIndexFields(isolate, obj, builder, 2, create);
  if (ServerState::instance()->isCoordinator()) {
    builder.add("ignoreNull", VPackValue(true));
    builder.add("constraint", VPackValue(false));
  }
  builder.add("sparse", VPackValue(true));
  builder.add("unique", VPackValue(false));
  ProcessIndexGeoJsonFlag(isolate, obj, builder);
  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief enhances the json of a hash index
////////////////////////////////////////////////////////////////////////////////

static int EnhanceJsonIndexHash(v8::Isolate* isolate,
                                v8::Handle<v8::Object> const obj,
                                VPackBuilder& builder, bool create) {
  int res = ProcessIndexFields(isolate, obj, builder, 0, create);
  ProcessIndexSparseFlag(isolate, obj, builder, create);
  ProcessIndexUniqueFlag(isolate, obj, builder);
  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief enhances the json of a skiplist index
////////////////////////////////////////////////////////////////////////////////

static int EnhanceJsonIndexSkiplist(v8::Isolate* isolate,
                                    v8::Handle<v8::Object> const obj,
                                    VPackBuilder& builder, bool create) {
  int res = ProcessIndexFields(isolate, obj, builder, 0, create);
  ProcessIndexSparseFlag(isolate, obj, builder, create);
  ProcessIndexUniqueFlag(isolate, obj, builder);
  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief enhances the json of a RocksDB index
////////////////////////////////////////////////////////////////////////////////

static int EnhanceJsonIndexRocksDB(v8::Isolate* isolate,
                                   v8::Handle<v8::Object> const obj,
                                   VPackBuilder& builder, bool create) {
  int res = ProcessIndexFields(isolate, obj, builder, 0, create);
  ProcessIndexSparseFlag(isolate, obj, builder, create);
  ProcessIndexUniqueFlag(isolate, obj, builder);
  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief enhances the json of a fulltext index
////////////////////////////////////////////////////////////////////////////////

static int EnhanceJsonIndexFulltext(v8::Isolate* isolate,
                                    v8::Handle<v8::Object> const obj,
                                    VPackBuilder& builder, bool create) {
  int res = ProcessIndexFields(isolate, obj, builder, 1, create);

  // handle "minLength" attribute
  int minWordLength = TRI_FULLTEXT_MIN_WORD_LENGTH_DEFAULT;

  if (obj->Has(TRI_V8_ASCII_STRING("minLength"))) {
    if (obj->Get(TRI_V8_ASCII_STRING("minLength"))->IsNumber() ||
        obj->Get(TRI_V8_ASCII_STRING("minLength"))->IsNumberObject()) {
      minWordLength =
          (int)TRI_ObjectToInt64(obj->Get(TRI_V8_ASCII_STRING("minLength")));
    } else if (!obj->Get(TRI_V8_ASCII_STRING("minLength"))->IsNull() &&
               !obj->Get(TRI_V8_ASCII_STRING("minLength"))->IsUndefined()) {
      return TRI_ERROR_BAD_PARAMETER;
    }
  }
  builder.add("minLength", VPackValue(minWordLength));
  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief enhances the json of an index
////////////////////////////////////////////////////////////////////////////////

static int EnhanceIndexJson(v8::FunctionCallbackInfo<v8::Value> const& args,
                            VPackBuilder& builder, bool create) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  v8::Handle<v8::Object> obj = args[0].As<v8::Object>();

  // extract index type
  arangodb::Index::IndexType type = arangodb::Index::TRI_IDX_TYPE_UNKNOWN;

  if (obj->Has(TRI_V8_ASCII_STRING("type")) &&
      obj->Get(TRI_V8_ASCII_STRING("type"))->IsString()) {
    TRI_Utf8ValueNFC typeString(TRI_UNKNOWN_MEM_ZONE,
                                obj->Get(TRI_V8_ASCII_STRING("type")));

    if (*typeString == nullptr) {
      return TRI_ERROR_OUT_OF_MEMORY;
    }

    std::string t(*typeString);
    // rewrite type "geo" into either "geo1" or "geo2", depending on the number
    // of fields
    if (t == "geo") {
      t = "geo1";

      if (obj->Has(TRI_V8_ASCII_STRING("fields")) &&
          obj->Get(TRI_V8_ASCII_STRING("fields"))->IsArray()) {
        v8::Handle<v8::Array> f = v8::Handle<v8::Array>::Cast(
            obj->Get(TRI_V8_ASCII_STRING("fields")));
        if (f->Length() == 2) {
          t = "geo2";
        }
      }
    }

    type = arangodb::Index::type(t.c_str());
  }

  if (type == arangodb::Index::TRI_IDX_TYPE_UNKNOWN) {
    return TRI_ERROR_BAD_PARAMETER;
  }

  if (create) {
    if (type == arangodb::Index::TRI_IDX_TYPE_PRIMARY_INDEX ||
        type == arangodb::Index::TRI_IDX_TYPE_EDGE_INDEX) {
      // creating these indexes yourself is forbidden
      return TRI_ERROR_FORBIDDEN;
    }
  }

  TRI_ASSERT(builder.isEmpty());
  int res = TRI_ERROR_INTERNAL;
  try {
    VPackObjectBuilder b(&builder);

    if (obj->Has(TRI_V8_ASCII_STRING("id"))) {
      uint64_t id = TRI_ObjectToUInt64(obj->Get(TRI_V8_ASCII_STRING("id")), true);
      if (id > 0) {
        char* idString = TRI_StringUInt64(id);
        builder.add("id", VPackValue(idString));
        TRI_FreeString(TRI_CORE_MEM_ZONE, idString);
      }
    }

    char const* idxType = arangodb::Index::typeName(type);
    builder.add("type", VPackValue(idxType));

    switch (type) {
      case arangodb::Index::TRI_IDX_TYPE_UNKNOWN: {
        res = TRI_ERROR_BAD_PARAMETER;
        break;
      }

      case arangodb::Index::TRI_IDX_TYPE_PRIMARY_INDEX:
      case arangodb::Index::TRI_IDX_TYPE_EDGE_INDEX: {
        break;
      }

      case arangodb::Index::TRI_IDX_TYPE_GEO1_INDEX:
        res = EnhanceJsonIndexGeo1(isolate, obj, builder, create);
        break;

      case arangodb::Index::TRI_IDX_TYPE_GEO2_INDEX:
        res = EnhanceJsonIndexGeo2(isolate, obj, builder, create);
        break;

      case arangodb::Index::TRI_IDX_TYPE_HASH_INDEX:
        res = EnhanceJsonIndexHash(isolate, obj, builder, create);
        break;

      case arangodb::Index::TRI_IDX_TYPE_SKIPLIST_INDEX:
        res = EnhanceJsonIndexSkiplist(isolate, obj, builder, create);
        break;
      
      case arangodb::Index::TRI_IDX_TYPE_ROCKSDB_INDEX:
        res = EnhanceJsonIndexRocksDB(isolate, obj, builder, create);
        break;

      case arangodb::Index::TRI_IDX_TYPE_FULLTEXT_INDEX:
        res = EnhanceJsonIndexFulltext(isolate, obj, builder, create);
        break;
    }
  } catch (...) {
    // TODO Check for different type of Errors
    return TRI_ERROR_OUT_OF_MEMORY;
  }
  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures an index, coordinator case
////////////////////////////////////////////////////////////////////////////////

static void EnsureIndexCoordinator(
    v8::FunctionCallbackInfo<v8::Value> const& args,
    TRI_vocbase_col_t const* collection, VPackSlice const slice, bool create) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  TRI_ASSERT(collection != nullptr);
  TRI_ASSERT(!slice.isNone());

  std::string const databaseName(collection->_dbName);
  std::string const cid = StringUtils::itoa(collection->_cid);
  // TODO: protect against races on _name
  std::string const collectionName(collection->_name);

  VPackBuilder resultBuilder;
  std::string errorMsg;
  int res = ClusterInfo::instance()->ensureIndexCoordinator(
      databaseName, cid, slice, create, &arangodb::Index::Compare,
      resultBuilder, errorMsg, 360.0);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(res, errorMsg);
  }

  if (resultBuilder.slice().isNone()) {
    if (!create) {
      // did not find a suitable index
      TRI_V8_RETURN_NULL();
    }

    TRI_V8_THROW_EXCEPTION_MEMORY();
  }

  v8::Handle<v8::Value> ret = IndexRep(isolate, collectionName, resultBuilder.slice());
  TRI_V8_RETURN(ret);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures an index, locally
////////////////////////////////////////////////////////////////////////////////

static void EnsureIndexLocal(v8::FunctionCallbackInfo<v8::Value> const& args,
                             TRI_vocbase_col_t const* collection,
                             VPackSlice const& slice, bool create) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  TRI_ASSERT(collection != nullptr);
  if (!slice.isObject()) {
    TRI_V8_THROW_EXCEPTION_MEMORY();
  }

  // extract type
  VPackSlice value = slice.get("type");

  if (!value.isString()) {
    TRI_V8_THROW_EXCEPTION_MEMORY();
  }

  std::string tmp = value.copyString();
  arangodb::Index::IndexType const type = arangodb::Index::type(tmp.c_str());

  // extract unique flag
  bool unique = arangodb::basics::VelocyPackHelper::getBooleanValue(
      slice, "unique", false);

  // extract sparse flag
  bool sparse = false;
  int sparsity = -1;  // not set
  value = slice.get("sparse");
  if (value.isBoolean()) {
    sparse = value.getBoolean();
    sparsity = sparse ? 1 : 0;
  }

  // extract id
  TRI_idx_iid_t iid = 0;
  value = slice.get("id");
  if (value.isString()) {
    std::string tmp = value.copyString();
    iid = StringUtils::uint64(tmp);
  }

  std::vector<std::string> attributes;

  // extract fields
  value = slice.get("fields");
  if (value.isArray()) {
    // note: "fields" is not mandatory for all index types

    // copy all field names (attributes)
    for (auto const& v : VPackArrayIterator(value)) {
      if (v.isString()) {
        std::string val = v.copyString();

        if (val.find("[*]") != std::string::npos) {
          if (type != arangodb::Index::TRI_IDX_TYPE_HASH_INDEX &&
              type != arangodb::Index::TRI_IDX_TYPE_SKIPLIST_INDEX &&
              type != arangodb::Index::TRI_IDX_TYPE_ROCKSDB_INDEX) {
            // expansion used in index type that does not support it
            TRI_V8_THROW_EXCEPTION_MESSAGE(
                TRI_ERROR_BAD_PARAMETER,
                "cannot use [*] expansion for this type of index");
          } else {
            // expansion used in index type that supports it

            // count number of [*] occurrences
            size_t found = 0;
            size_t offset = 0;

            while ((offset = val.find("[*]", offset)) != std::string::npos) {
              ++found;
              offset += strlen("[*]");
            }

            // only one occurrence is allowed
            if (found > 1) {
              TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                             "cannot use multiple [*] "
                                             "expansions for a single index "
                                             "field");
            }
          }
        }
        attributes.emplace_back(val);
      }
    }
  }
  
  READ_LOCKER(readLocker, collection->_vocbase->_inventoryLock);

  SingleCollectionTransaction trx(V8TransactionContext::Create(collection->_vocbase, true), collection->_cid, create ? TRI_TRANSACTION_WRITE : TRI_TRANSACTION_READ);

  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  TRI_document_collection_t* document = trx.documentCollection();
  std::string const& collectionName = std::string(collection->_name);

  // disallow index creation in read-only mode
  if (!TRI_IsSystemNameCollection(collectionName.c_str()) && create &&
      TRI_GetOperationModeServer() == TRI_VOCBASE_MODE_NO_CREATE) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_READ_ONLY);
  }

  bool created = false;
  arangodb::Index* idx = nullptr;

  switch (type) {
    case arangodb::Index::TRI_IDX_TYPE_UNKNOWN:
    case arangodb::Index::TRI_IDX_TYPE_PRIMARY_INDEX:
    case arangodb::Index::TRI_IDX_TYPE_EDGE_INDEX: {
      // these indexes cannot be created directly
      TRI_V8_THROW_EXCEPTION(TRI_ERROR_INTERNAL);
    }

    case arangodb::Index::TRI_IDX_TYPE_GEO1_INDEX: {
      if (attributes.size() != 1) {
        TRI_V8_THROW_EXCEPTION(TRI_ERROR_INTERNAL);
      }

      bool geoJson = arangodb::basics::VelocyPackHelper::getBooleanValue(
          slice, "geoJson", false);

      if (create) {
        idx = static_cast<arangodb::GeoIndex2*>(
            TRI_EnsureGeoIndex1DocumentCollection(
                &trx, document, iid, attributes[0], geoJson, created));
      } else {
        std::vector<std::string> location =
            arangodb::basics::StringUtils::split(attributes[0], ".");
        idx = static_cast<arangodb::GeoIndex2*>(
            TRI_LookupGeoIndex1DocumentCollection(document, location, geoJson));
      }
      break;
    }

    case arangodb::Index::TRI_IDX_TYPE_GEO2_INDEX: {
      if (attributes.size() != 2) {
        TRI_V8_THROW_EXCEPTION(TRI_ERROR_INTERNAL);
      }

      if (create) {
        idx = static_cast<arangodb::GeoIndex2*>(
            TRI_EnsureGeoIndex2DocumentCollection(
                &trx, document, iid, attributes[0], attributes[1], created));
      } else {
        std::vector<std::string> lat =
            arangodb::basics::StringUtils::split(attributes[0], ".");
        std::vector<std::string> lon =
            arangodb::basics::StringUtils::split(attributes[0], ".");
        idx = static_cast<arangodb::GeoIndex2*>(
            TRI_LookupGeoIndex2DocumentCollection(document, lon, lat));
      }
      break;
    }

    case arangodb::Index::TRI_IDX_TYPE_HASH_INDEX: {
      if (attributes.empty()) {
        TRI_V8_THROW_EXCEPTION(TRI_ERROR_INTERNAL);
      }

      if (create) {
        idx = static_cast<arangodb::HashIndex*>(
            TRI_EnsureHashIndexDocumentCollection(
                &trx, document, iid, attributes, sparse, unique, created));
      } else {
        idx = static_cast<arangodb::HashIndex*>(
            TRI_LookupHashIndexDocumentCollection(document, attributes,
                                                  sparsity, unique));
      }

      break;
    }

    case arangodb::Index::TRI_IDX_TYPE_SKIPLIST_INDEX: {
      if (attributes.empty()) {
        TRI_V8_THROW_EXCEPTION(TRI_ERROR_INTERNAL);
      }

      if (create) {
        idx = static_cast<arangodb::SkiplistIndex*>(
            TRI_EnsureSkiplistIndexDocumentCollection(
                &trx, document, iid, attributes, sparse, unique, created));
      } else {
        idx = static_cast<arangodb::SkiplistIndex*>(
            TRI_LookupSkiplistIndexDocumentCollection(document, attributes,
                                                      sparsity, unique));
      }
      break;
    }
    
    case arangodb::Index::TRI_IDX_TYPE_ROCKSDB_INDEX: {
      if (attributes.empty()) {
        TRI_V8_THROW_EXCEPTION(TRI_ERROR_INTERNAL);
      }

#ifdef ARANGODB_ENABLE_ROCKSDB
      if (create) {
        idx = static_cast<arangodb::RocksDBIndex*>(
            TRI_EnsureRocksDBIndexDocumentCollection(
                &trx, document, iid, attributes, sparse, unique, created));
      } else {
        idx = static_cast<arangodb::RocksDBIndex*>(
            TRI_LookupRocksDBIndexDocumentCollection(document, attributes,
                                                     sparsity, unique));
      }
      break;
#else
      TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_NOT_IMPLEMENTED, "index type not supported in this build");
#endif
    }

    case arangodb::Index::TRI_IDX_TYPE_FULLTEXT_INDEX: {
      if (attributes.size() != 1) {
        TRI_V8_THROW_EXCEPTION(TRI_ERROR_INTERNAL);
      }

      int minWordLength = TRI_FULLTEXT_MIN_WORD_LENGTH_DEFAULT;
      VPackSlice const value = slice.get("minLength");
      if (value.isNumber()) {
        minWordLength = value.getNumericValue<int>();
      } else if (!value.isNone()) {
        // minLength defined but no number
        TRI_V8_THROW_EXCEPTION_PARAMETER("<minLength> must be a number");
      }

      if (create) {
        idx = static_cast<arangodb::FulltextIndex*>(
            TRI_EnsureFulltextIndexDocumentCollection(
                &trx, document, iid, attributes[0], minWordLength, created));
      } else {
        idx = static_cast<arangodb::FulltextIndex*>(
            TRI_LookupFulltextIndexDocumentCollection(document, attributes[0],
                                                      minWordLength));
      }
      break;
    }
  }

  if (idx == nullptr && create) {
    // something went wrong during creation
    int res = TRI_errno();

    TRI_V8_THROW_EXCEPTION(res);
  }

  if (idx == nullptr && !create) {
    // no index found
    TRI_V8_RETURN_NULL();
  }

  // found some index to return
  std::shared_ptr<VPackBuilder> indexVPack;
  try {
    indexVPack = idx->toVelocyPack(false);
  } catch (...) {
    TRI_V8_THROW_EXCEPTION_MEMORY();
  }

  if (indexVPack == nullptr) {
    TRI_V8_THROW_EXCEPTION_MEMORY();
  }

  v8::Handle<v8::Value> ret =
      IndexRep(isolate, collectionName, indexVPack->slice());

  if (ret->IsObject()) {
    ret->ToObject()->Set(TRI_V8_ASCII_STRING("isNewlyCreated"),
                         v8::Boolean::New(isolate, create && created));
  }

  TRI_V8_RETURN(ret);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures an index
////////////////////////////////////////////////////////////////////////////////

static void EnsureIndex(v8::FunctionCallbackInfo<v8::Value> const& args,
                        bool create, char const* functionName) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  TRI_vocbase_col_t* collection =
      TRI_UnwrapClass<TRI_vocbase_col_t>(args.Holder(), WRP_VOCBASE_COL_TYPE);

  if (collection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  if (args.Length() != 1 || !args[0]->IsObject()) {
    std::string name(functionName);
    name.append("(<description>)");
    TRI_V8_THROW_EXCEPTION_USAGE(name.c_str());
  }

  VPackBuilder builder;
  int res = EnhanceIndexJson(args, builder, create);
  VPackSlice slice = builder.slice();
  if (res == TRI_ERROR_NO_ERROR && ServerState::instance()->isCoordinator()) {
    TRI_ASSERT(slice.isObject());

    std::string const dbname(collection->_dbName);
    // TODO: someone might rename the collection while we're reading its name...
    std::string const collname(collection->_name);
    std::shared_ptr<CollectionInfo> c =
        ClusterInfo::instance()->getCollection(dbname, collname);

    if (c->empty()) {
      TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
    }

    // check if there is an attempt to create a unique index on non-shard keys
    if (create) {
      VPackSlice v = slice.get("unique");

      if (v.isBoolean() && v.getBoolean()) {
        // unique index, now check if fields and shard keys match
        VPackSlice flds = slice.get("fields");
        if (flds.isArray() && c->numberOfShards() > 1) {
          std::vector<std::string> const& shardKeys = c->shardKeys();
          size_t n = static_cast<size_t>(flds.length());

          if (shardKeys.size() != n) {
            res = TRI_ERROR_CLUSTER_UNSUPPORTED;
          } else {
            for (size_t i = 0; i < n; ++i) {
              VPackSlice f = flds.at(i);
              if (!f.isString()) {
                res = TRI_ERROR_INTERNAL;
                continue;
              } else {
                std::string tmp = f.copyString();
                if (!TRI_EqualString(tmp.c_str(), shardKeys[i].c_str())) {
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
    TRI_V8_THROW_EXCEPTION(res);
  }

  TRI_ASSERT(!slice.isNone());
  // ensure an index, coordinator case
  if (ServerState::instance()->isCoordinator()) {
    EnsureIndexCoordinator(args, collection, slice, create);
  } else {
    EnsureIndexLocal(args, collection, slice, create);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a collection on the coordinator
////////////////////////////////////////////////////////////////////////////////

static void CreateCollectionCoordinator(
    v8::FunctionCallbackInfo<v8::Value> const& args,
    TRI_col_type_e collectionType, std::string const& databaseName,
    VocbaseCollectionInfo& parameters, TRI_vocbase_t* vocbase) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  std::string const name = TRI_ObjectToString(args[0]);

  if (!TRI_IsAllowedNameCollection(parameters.isSystem(), name.c_str())) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_ILLEGAL_NAME);
  }

  bool allowUserKeys = true;
  uint64_t numberOfShards = 1;
  std::vector<std::string> shardKeys;
  uint64_t replicationFactor = 1;

  // default shard key
  shardKeys.push_back("_key");

  std::string distributeShardsLike;

  std::string cid = "";  // Could come from properties
  if (2 <= args.Length()) {
    if (!args[1]->IsObject()) {
      TRI_V8_THROW_TYPE_ERROR("<properties> must be an object");
    }

    v8::Handle<v8::Object> p = args[1]->ToObject();

    if (p->Has(TRI_V8_ASCII_STRING("keyOptions")) &&
        p->Get(TRI_V8_ASCII_STRING("keyOptions"))->IsObject()) {
      v8::Handle<v8::Object> o = v8::Handle<v8::Object>::Cast(
          p->Get(TRI_V8_ASCII_STRING("keyOptions")));

      if (o->Has(TRI_V8_ASCII_STRING("type"))) {
        std::string const type =
            TRI_ObjectToString(o->Get(TRI_V8_ASCII_STRING("type")));

        if (type != "" && type != "traditional") {
          // invalid key generator
          TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_CLUSTER_UNSUPPORTED,
                                         "non-traditional key generators are "
                                         "not supported for sharded "
                                         "collections");
        }
      }

      if (o->Has(TRI_V8_ASCII_STRING("allowUserKeys"))) {
        allowUserKeys =
            TRI_ObjectToBoolean(o->Get(TRI_V8_ASCII_STRING("allowUserKeys")));
      }
    }

    if (p->Has(TRI_V8_ASCII_STRING("numberOfShards"))) {
      numberOfShards = TRI_ObjectToUInt64(
          p->Get(TRI_V8_ASCII_STRING("numberOfShards")), false);
    }

    if (p->Has(TRI_V8_ASCII_STRING("shardKeys"))) {
      shardKeys.clear();

      if (p->Get(TRI_V8_ASCII_STRING("shardKeys"))->IsArray()) {
        v8::Handle<v8::Array> k = v8::Handle<v8::Array>::Cast(
            p->Get(TRI_V8_ASCII_STRING("shardKeys")));

        for (uint32_t i = 0; i < k->Length(); ++i) {
          v8::Handle<v8::Value> v = k->Get(i);
          if (v->IsString()) {
            std::string const key = TRI_ObjectToString(v);

            // system attributes are not allowed (except _key)
            if (!key.empty() && (key[0] != '_' || key == "_key")) {
              shardKeys.push_back(key);
            }
          }
        }
      }
    }

    if (p->Has(TRI_V8_ASCII_STRING("distributeShardsLike")) &&
        p->Get(TRI_V8_ASCII_STRING("distributeShardsLike"))->IsString()) {
      distributeShardsLike = TRI_ObjectToString(
          p->Get(TRI_V8_ASCII_STRING("distributeShardsLike")));
    }

    auto idKey = TRI_V8_ASCII_STRING("id");
    if (p->Has(idKey) && p->Get(idKey)->IsString()) {
      cid = TRI_ObjectToString(p->Get(idKey));
    }

    if (p->Has(TRI_V8_ASCII_STRING("replicationFactor"))) {
      replicationFactor = TRI_ObjectToUInt64(p->Get(TRI_V8_ASCII_STRING("replicationFactor")), false);
    }
  }

  if (numberOfShards == 0 || numberOfShards > 1000) {
    TRI_V8_THROW_EXCEPTION_PARAMETER("invalid number of shards");
  }

  if (replicationFactor == 0 || replicationFactor > 10) {
    TRI_V8_THROW_EXCEPTION_PARAMETER("invalid replicationFactor");
  }

  if (shardKeys.empty() || shardKeys.size() > 8) {
    TRI_V8_THROW_EXCEPTION_PARAMETER("invalid number of shard keys");
  }

  ClusterInfo* ci = ClusterInfo::instance();

  // fetch a unique id for the new collection plus one for each shard to create
  uint64_t const id = ci->uniqid(1 + numberOfShards);
  if (cid.empty()) {
    // collection id is the first unique id we got
    cid = StringUtils::itoa(id);
    // if id was given, the first unique id is wasted, this does not matter
  }

  std::vector<std::string> dbServers;

  bool done = false;
  if (!distributeShardsLike.empty()) {
    CollectionNameResolver resolver(vocbase);
    TRI_voc_cid_t otherCid =
        resolver.getCollectionIdCluster(distributeShardsLike);
    if (otherCid != 0) {
      std::string otherCidString 
          = arangodb::basics::StringUtils::itoa(otherCid);
      std::shared_ptr<CollectionInfo> collInfo =
          ci->getCollection(databaseName, otherCidString);
      if (!collInfo->empty()) {
        auto shards = collInfo->shardIds();
        auto shardList = ci->getShardList(otherCidString);
        for (auto const& s : *shardList) {
          auto it = shards->find(s);
          if (it != shards->end()) {
            for (auto const& s : it->second) {
              dbServers.push_back(s);
            }
          }
        }
        done = true;
      }
    }
  }

  if (!done) {
    // fetch list of available servers in cluster, and shuffle them randomly
    dbServers = ci->getCurrentDBServers();

    if (dbServers.empty()) {
      TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                     "no database servers found in cluster");
    }

    random_shuffle(dbServers.begin(), dbServers.end());
  }

  // now create the shards
  std::map<std::string, std::vector<std::string>> shards;
  size_t count = 0;
  for (uint64_t i = 0; i < numberOfShards; ++i) {
    // determine responsible server(s)
    std::vector<std::string> serverIds;
    for (uint64_t j = 0; j < replicationFactor; ++j) {
      std::string candidate;
      size_t count2 = 0;
      bool found = true;
      do {
        candidate = dbServers[count++];
        if (count >= dbServers.size()) {
          count = 0;
        }
        if (++count2 == dbServers.size() + 1) {
          LOG(WARN) << "createCollectionCoordinator: replicationFactor is "
                       "too large for the number of DBservers";
          found = false;
          break;
        }
      } while (std::find(serverIds.begin(), serverIds.end(), candidate) != 
               serverIds.end());
      if (found) {
        serverIds.push_back(candidate);
      }
    }

    // determine shard id
    std::string shardId = "s" + StringUtils::itoa(id + 1 + i);

    shards.insert(std::make_pair(shardId, serverIds));
  }

  // now create the VelocyPack for the collection
  arangodb::velocypack::Builder velocy;
  using arangodb::velocypack::Value;
  using arangodb::velocypack::ValueType;
  using arangodb::velocypack::ObjectBuilder;
  using arangodb::velocypack::ArrayBuilder;

  {
    ObjectBuilder ob(&velocy);
    velocy("id",           Value(cid))
          ("name",         Value(name))
          ("type",         Value((int) collectionType))
          ("status",       Value((int) TRI_VOC_COL_STATUS_LOADED))
          ("deleted",      Value(parameters.deleted()))
          ("doCompact",    Value(parameters.doCompact()))
          ("isSystem",     Value(parameters.isSystem()))
          ("isVolatile",   Value(parameters.isVolatile()))
          ("waitForSync",  Value(parameters.waitForSync()))
          ("journalSize",  Value(parameters.maximalSize()))
          ("indexBuckets", Value(parameters.indexBuckets()))
          ("replicationFactor", Value(replicationFactor))
          ("keyOptions",   Value(ValueType::Object))
              ("type",          Value("traditional"))
              ("allowUserKeys", Value(allowUserKeys))
           .close();

    {
      ArrayBuilder ab(&velocy, "shardKeys");
      for (auto const& sk : shardKeys) {
        velocy(Value(sk));
      }
    }

    {
      ObjectBuilder ob(&velocy, "shards");
      for (auto const& p : shards) {
        ArrayBuilder ab(&velocy, p.first);
        for (auto const& s : p.second) {
          velocy(Value(s));
        }
      }
    }

    {
      ArrayBuilder ab(&velocy, "indexes");

      // create a dummy primary index
      TRI_document_collection_t* doc = nullptr;
      std::unique_ptr<arangodb::PrimaryIndex> primaryIndex(
          new arangodb::PrimaryIndex(doc));

      velocy.openObject();
      primaryIndex->toVelocyPack(velocy, false);
      velocy.close();

      if (collectionType == TRI_COL_TYPE_EDGE) {
        // create a dummy edge index
        auto edgeIndex = std::make_unique<arangodb::EdgeIndex>(id, nullptr);

        velocy.openObject();
        edgeIndex->toVelocyPack(velocy, false);
        velocy.close();
      }
    }
  }

  std::string errorMsg;
  int myerrno = ci->createCollectionCoordinator(
      databaseName, cid, numberOfShards, velocy.slice(), errorMsg, 240.0);

  if (myerrno != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(myerrno, errorMsg);
  }
  ci->loadPlan();

  std::shared_ptr<CollectionInfo> c = ci->getCollection(databaseName, cid);
  TRI_vocbase_col_t* newcoll = CoordinatorCollection(vocbase, *c);
  TRI_V8_RETURN(WrapCollection(isolate, newcoll));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock collectionEnsureIndex
////////////////////////////////////////////////////////////////////////////////

static void JS_EnsureIndexVocbaseCol(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  PREVENT_EMBEDDED_TRANSACTION();

  EnsureIndex(args, true, "ensureIndex");
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up an index
////////////////////////////////////////////////////////////////////////////////

static void JS_LookupIndexVocbaseCol(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  EnsureIndex(args, false, "lookupIndex");
  TRI_V8_TRY_CATCH_END
}
////////////////////////////////////////////////////////////////////////////////
/// @brief drops an index, coordinator case
////////////////////////////////////////////////////////////////////////////////

static void DropIndexCoordinator(
    v8::FunctionCallbackInfo<v8::Value> const& args,
    TRI_vocbase_col_t const* collection, v8::Handle<v8::Value> const val) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  std::string collectionName;
  TRI_idx_iid_t iid = 0;

  // extract the index identifier from a string
  if (val->IsString() || val->IsStringObject() || val->IsNumber()) {
    if (!IsIndexHandle(val, collectionName, iid)) {
      TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_INDEX_HANDLE_BAD);
    }
  }

  // extract the index identifier from an object
  else if (val->IsObject()) {
    TRI_GET_GLOBALS();

    v8::Handle<v8::Object> obj = val->ToObject();
    TRI_GET_GLOBAL_STRING(IdKey);
    v8::Handle<v8::Value> iidVal = obj->Get(IdKey);

    if (!IsIndexHandle(iidVal, collectionName, iid)) {
      TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_INDEX_HANDLE_BAD);
    }
  }

  if (!collectionName.empty()) {
    CollectionNameResolver resolver(collection->_vocbase);

    if (!EqualCollection(&resolver, collectionName, collection)) {
      TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_CROSS_COLLECTION_REQUEST);
    }
  }

  std::string const databaseName(collection->_dbName);
  std::string const cid = StringUtils::itoa(collection->_cid);
  std::string errorMsg;

  int res = ClusterInfo::instance()->dropIndexCoordinator(databaseName, cid,
                                                          iid, errorMsg, 0.0);

  if (res == TRI_ERROR_NO_ERROR) {
    TRI_V8_RETURN_TRUE();
  }
  TRI_V8_RETURN_FALSE();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock col_dropIndex
////////////////////////////////////////////////////////////////////////////////

static void JS_DropIndexVocbaseCol(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  PREVENT_EMBEDDED_TRANSACTION();

  TRI_vocbase_col_t* collection =
      TRI_UnwrapClass<TRI_vocbase_col_t>(args.Holder(), WRP_VOCBASE_COL_TYPE);

  if (collection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  if (args.Length() != 1) {
    TRI_V8_THROW_EXCEPTION_USAGE("dropIndex(<index-handle>)");
  }

  if (ServerState::instance()->isCoordinator()) {
    DropIndexCoordinator(args, collection, args[0]);
    return;
  }
  
  READ_LOCKER(readLocker, collection->_vocbase->_inventoryLock);

  SingleCollectionTransaction trx(V8TransactionContext::Create(collection->_vocbase, true), collection->_cid, TRI_TRANSACTION_WRITE);

  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  TRI_document_collection_t* document = trx.documentCollection();

  auto idx = TRI_LookupIndexByHandle(isolate, trx.resolver(), collection,
                                     args[0], true);

  if (idx == nullptr || idx->id() == 0) {
    TRI_V8_RETURN_FALSE();
  }

  if (!idx->canBeDropped()) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_FORBIDDEN);
  }

  bool ok = TRI_DropIndexDocumentCollection(document, idx->id(), true);

  if (ok) {
    TRI_V8_RETURN_TRUE();
  }

  TRI_V8_RETURN_FALSE();
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns information about the indexes, coordinator case
////////////////////////////////////////////////////////////////////////////////

static void GetIndexesCoordinator(
    v8::FunctionCallbackInfo<v8::Value> const& args,
    TRI_vocbase_col_t const* collection) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  std::string const databaseName(collection->_dbName);
  std::string const cid = StringUtils::itoa(collection->_cid);
  std::string const collectionName(collection->_name);

  std::shared_ptr<CollectionInfo> c =
      ClusterInfo::instance()->getCollection(databaseName, cid);

  if ((*c).empty()) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
  }

  v8::Handle<v8::Array> ret = v8::Array::New(isolate);

  VPackSlice slice = c->getIndexes();

  if (slice.isArray()) {
    uint32_t j = 0;
    for (auto const& v : VPackArrayIterator(slice)) {
      if (!v.isNone()) {
        ret->Set(j++, IndexRep(isolate, collectionName, v));
      }
    }
  }

  TRI_V8_RETURN(ret);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock collectionGetIndexes
////////////////////////////////////////////////////////////////////////////////

static void JS_GetIndexesVocbaseCol(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  v8::HandleScope scope(isolate);

  TRI_vocbase_col_t* collection =
      TRI_UnwrapClass<TRI_vocbase_col_t>(args.Holder(), WRP_VOCBASE_COL_TYPE);

  if (collection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }

  if (ServerState::instance()->isCoordinator()) {
    GetIndexesCoordinator(args, collection);
    return;
  }

  bool withFigures = false;
  if (args.Length() > 0) {
    withFigures = TRI_ObjectToBoolean(args[0]);
  }

  SingleCollectionTransaction trx(V8TransactionContext::Create(collection->_vocbase, true), collection->_cid, TRI_TRANSACTION_READ);

  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  // READ-LOCK start
  trx.lockRead();

  TRI_document_collection_t* document = trx.documentCollection();
  std::string const& collectionName = std::string(collection->_name);

  // get list of indexes
  auto indexes(TRI_IndexesDocumentCollection(document, withFigures));

  trx.finish(res);
  // READ-LOCK end

  size_t const n = indexes.size();
  v8::Handle<v8::Array> result = v8::Array::New(isolate, static_cast<int>(n));

  for (size_t i = 0; i < n; ++i) {
    auto const& idx = indexes[i];

    result->Set(static_cast<uint32_t>(i),
                IndexRep(isolate, collectionName, idx->slice()));
  }

  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up an index identifier
////////////////////////////////////////////////////////////////////////////////

arangodb::Index* TRI_LookupIndexByHandle(
    v8::Isolate* isolate, arangodb::CollectionNameResolver const* resolver,
    TRI_vocbase_col_t const* collection, v8::Handle<v8::Value> const val,
    bool ignoreNotFound) {
  // reset the collection identifier
  std::string collectionName;
  TRI_idx_iid_t iid = 0;

  // assume we are already loaded
  TRI_ASSERT(collection != nullptr);
  TRI_ASSERT(collection->_collection != nullptr);

  // extract the index identifier from a string
  if (val->IsString() || val->IsStringObject() || val->IsNumber()) {
    if (!IsIndexHandle(val, collectionName, iid)) {
      TRI_V8_SET_EXCEPTION(TRI_ERROR_ARANGO_INDEX_HANDLE_BAD);
      return nullptr;
    }
  }

  // extract the index identifier from an object
  else if (val->IsObject()) {
    TRI_GET_GLOBALS();

    v8::Handle<v8::Object> obj = val->ToObject();
    TRI_GET_GLOBAL_STRING(IdKey);
    v8::Handle<v8::Value> iidVal = obj->Get(IdKey);

    if (!IsIndexHandle(iidVal, collectionName, iid)) {
      TRI_V8_SET_EXCEPTION(TRI_ERROR_ARANGO_INDEX_HANDLE_BAD);
      return nullptr;
    }
  }

  if (!collectionName.empty()) {
    if (!EqualCollection(resolver, collectionName, collection)) {
      // I wish this error provided me with more information!
      // e.g. 'cannot access index outside the collection it was defined in'
      TRI_V8_SET_EXCEPTION(TRI_ERROR_ARANGO_CROSS_COLLECTION_REQUEST);
      return nullptr;
    }
  }

  auto idx = collection->_collection->lookupIndex(iid);

  if (idx == nullptr) {
    if (!ignoreNotFound) {
      TRI_V8_SET_EXCEPTION(TRI_ERROR_ARANGO_INDEX_NOT_FOUND);
      return nullptr;
    }
  }

  return idx;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a collection
////////////////////////////////////////////////////////////////////////////////

static void CreateVocBase(v8::FunctionCallbackInfo<v8::Value> const& args,
                          TRI_col_type_e collectionType) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  TRI_vocbase_t* vocbase = GetContextVocBase(isolate);

  if (vocbase == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  // ...........................................................................
  // We require exactly 1 or exactly 2 arguments -- anything else is an error
  // ...........................................................................

  if (args.Length() < 1 || args.Length() > 3) {
    TRI_V8_THROW_EXCEPTION_USAGE("_create(<name>, <properties>, <type>)");
  }

  if (TRI_GetOperationModeServer() == TRI_VOCBASE_MODE_NO_CREATE) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_READ_ONLY);
  }

  // optional, third parameter can override collection type
  if (args.Length() == 3 && args[2]->IsString()) {
    std::string typeString = TRI_ObjectToString(args[2]);
    if (typeString == "edge") {
      collectionType = TRI_COL_TYPE_EDGE;
    } else if (typeString == "document") {
      collectionType = TRI_COL_TYPE_DOCUMENT;
    }
  }

  PREVENT_EMBEDDED_TRANSACTION();

  // extract the name
  std::string const name = TRI_ObjectToString(args[0]);

  VPackBuilder builder;
  VPackSlice infoSlice;
  if (2 <= args.Length()) {
    if (!args[1]->IsObject()) {
      TRI_V8_THROW_TYPE_ERROR("<properties> must be an object");
    }

    int res = TRI_V8ToVPack(isolate, builder, args[1], false);

    if (res != TRI_ERROR_NO_ERROR) {
      TRI_V8_THROW_EXCEPTION(res);
    }

    infoSlice = builder.slice();
  }

  VocbaseCollectionInfo parameters(vocbase, name.c_str(), collectionType,
                                   infoSlice);

  if (ServerState::instance()->isCoordinator()) {
    CreateCollectionCoordinator(args, collectionType, vocbase->_name,
                                parameters, vocbase);
    return;
  }

  TRI_vocbase_col_t const* collection =
      TRI_CreateCollectionVocBase(vocbase, parameters, parameters.id(), true);

  if (collection == nullptr) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_errno(), "cannot create collection");
  }

  v8::Handle<v8::Value> result = WrapCollection(isolate, collection);

  if (result.IsEmpty()) {
    TRI_V8_THROW_EXCEPTION_MEMORY();
  }

  TRI_V8_RETURN(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock collectionDatabaseCreate
////////////////////////////////////////////////////////////////////////////////

static void JS_CreateVocbase(v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  CreateVocBase(args, TRI_COL_TYPE_DOCUMENT);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock collectionCreateDocumentCollection
////////////////////////////////////////////////////////////////////////////////

static void JS_CreateDocumentCollectionVocbase(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  CreateVocBase(args, TRI_COL_TYPE_DOCUMENT);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock collectionCreateEdgeCollection
////////////////////////////////////////////////////////////////////////////////

static void JS_CreateEdgeCollectionVocbase(
    v8::FunctionCallbackInfo<v8::Value> const& args) {
  TRI_V8_TRY_CATCH_BEGIN(isolate);
  CreateVocBase(args, TRI_COL_TYPE_EDGE);
  TRI_V8_TRY_CATCH_END
}

void TRI_InitV8indexArangoDB(v8::Isolate* isolate,
                             v8::Handle<v8::ObjectTemplate> rt) {
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("_create"),
                       JS_CreateVocbase, true);
  TRI_AddMethodVocbase(isolate, rt,
                       TRI_V8_ASCII_STRING("_createEdgeCollection"),
                       JS_CreateEdgeCollectionVocbase);
  TRI_AddMethodVocbase(isolate, rt,
                       TRI_V8_ASCII_STRING("_createDocumentCollection"),
                       JS_CreateDocumentCollectionVocbase);
}

void TRI_InitV8indexCollection(v8::Isolate* isolate,
                               v8::Handle<v8::ObjectTemplate> rt) {
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("dropIndex"),
                       JS_DropIndexVocbaseCol);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("ensureIndex"),
                       JS_EnsureIndexVocbaseCol);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("lookupIndex"),
                       JS_LookupIndexVocbaseCol);
  TRI_AddMethodVocbase(isolate, rt, TRI_V8_ASCII_STRING("getIndexes"),
                       JS_GetIndexesVocbaseCol);
}
