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
#include "Basics/ReadLocker.h"
#include "Basics/StringUtils.h"
#include "Basics/tri-strings.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ClusterMethods.h"
#include "FulltextIndex/fulltext-index.h"
#include "Indexes/EdgeIndex.h"
#include "Indexes/Index.h"
#include "Indexes/PrimaryIndex.h"
#include "Utils/SingleCollectionTransaction.h"
#include "Utils/V8TransactionContext.h"
#include "V8/v8-conv.h"
#include "V8/v8-globals.h"
#include "V8/v8-utils.h"
#include "V8/v8-vpack.h"
#include "V8Server/v8-collection.h"
#include "V8Server/v8-vocbase.h"
#include "V8Server/v8-vocbaseprivate.h"
#include "VocBase/modes.h"
#include "VocBase/LogicalCollection.h"

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

    type = arangodb::Index::type(t);
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
        builder.add("id", VPackValue(std::to_string(id)));
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
    LogicalCollection const* collection, VPackSlice const slice, bool create) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  TRI_ASSERT(collection != nullptr);
  TRI_ASSERT(!slice.isNone());

  std::string const databaseName(collection->dbName());
  std::string const cid = collection->cid_as_string();
  std::string const collectionName(collection->name());

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
                             arangodb::LogicalCollection* collection,
                             VPackSlice const& slice, bool create) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  TRI_ASSERT(collection != nullptr);
  READ_LOCKER(readLocker, collection->vocbase()->_inventoryLock);

  SingleCollectionTransaction trx(
      V8TransactionContext::Create(collection->vocbase(), true),
      collection->cid(), create ? TRI_TRANSACTION_WRITE : TRI_TRANSACTION_READ);

  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  // disallow index creation in read-only mode
  if (!collection->isSystem() && create &&
      TRI_GetOperationModeServer() == TRI_VOCBASE_MODE_NO_CREATE) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_READ_ONLY);
  }

  bool created = false;
  std::shared_ptr<arangodb::Index> idx;
  if (create) {
    // TODO Encapsulate in try{}catch(){} instead of errno()
    idx = collection->createIndex(&trx, slice, created);
    if (idx == nullptr) {
      // something went wrong during creation
      int res = TRI_errno();
      TRI_V8_THROW_EXCEPTION(res);
    }
  } else {
    idx = collection->lookupIndex(slice);
    if (idx == nullptr) {
      // Index not found
      TRI_V8_RETURN_NULL();
    }
  }
  TransactionBuilderLeaser builder(&trx);
  builder->openObject();
  try {
    idx->toVelocyPack(*(builder.get()), false);
  } catch (...) {
    TRI_V8_THROW_EXCEPTION_MEMORY();
  }
  builder->close();

  v8::Handle<v8::Value> ret =
      IndexRep(isolate, collection->name(), builder->slice());

  if (ret->IsObject()) {
    ret->ToObject()->Set(TRI_V8_ASCII_STRING("isNewlyCreated"),
                         v8::Boolean::New(isolate, created));
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

  arangodb::LogicalCollection* collection =
      TRI_UnwrapClass<arangodb::LogicalCollection>(args.Holder(), WRP_VOCBASE_COL_TYPE);

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

    std::string const dbname(collection->dbName());
    std::string const collname(collection->name());
    std::shared_ptr<LogicalCollection> c =
        ClusterInfo::instance()->getCollection(dbname, collname);

    if (c == nullptr) {
      TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
    }

    // check if there is an attempt to create a unique index on non-shard keys
    if (create) {
      Index::validateFields(slice);

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
                if (tmp != shardKeys[i]) {
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
    LogicalCollection* parameters) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  std::string const name = TRI_ObjectToString(args[0]);

  bool allowUserKeys = true;
  uint64_t numberOfShards = 1;
  std::vector<std::string> shardKeys;
  uint64_t replicationFactor = 1;

  // default shard key
  shardKeys.push_back(StaticStrings::KeyString);

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
            // remove : char at the beginning or end (for enterprise)
            std::string stripped;
            if (!key.empty()) {
              if (key.front() == ':') {
                stripped = key.substr(1);
              } else if (key.back() == ':') {
                stripped = key.substr(0, key.size()-1);
              } else {
                stripped = key;
              }
            }
            // system attributes are not allowed (except _key)
            if (!stripped.empty() && stripped != StaticStrings::IdString &&
                stripped != StaticStrings::RevString) {
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

  // fetch a unique id for the new collection, this is also used for the
  // edge index, if that is needed, therefore we create the id anyway:
  uint64_t const id = ci->uniqid(1);
  if (cid.empty()) {
    // collection id is the first unique id we got
    cid = StringUtils::itoa(id);
    // if id was given, the first unique id is wasted, this does not matter
  }

  std::vector<std::string> dbServers;

  if (!distributeShardsLike.empty()) {
    CollectionNameResolver resolver(parameters->vocbase());
    TRI_voc_cid_t otherCid =
        resolver.getCollectionIdCluster(distributeShardsLike);
    if (otherCid != 0) {
      std::string otherCidString 
          = arangodb::basics::StringUtils::itoa(otherCid);
      try {
        std::shared_ptr<LogicalCollection> collInfo =
            ci->getCollection(databaseName, otherCidString);
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
      } catch (...) {
      }
    }
  }

  // If the list dbServers is still empty, it will be filled in
  // distributeShards below.

  // Now create the shards:
  std::map<std::string, std::vector<std::string>> shards
      = arangodb::distributeShards(numberOfShards, replicationFactor,
                                   dbServers);
  if (shards.empty()) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "no database servers found in cluster");
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
          ("deleted",      Value(parameters->deleted()))
          ("doCompact",    Value(parameters->doCompact()))
          ("isSystem",     Value(parameters->isSystem()))
          ("isVolatile",   Value(parameters->isVolatile()))
          ("waitForSync",  Value(parameters->waitForSync()))
          ("journalSize",  Value(parameters->journalSize()))
          ("indexBuckets", Value(parameters->indexBuckets()))
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
      arangodb::LogicalCollection* doc = nullptr;
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

  std::shared_ptr<LogicalCollection> c = ci->getCollection(databaseName, cid);
  // If we get a nullptr here the create collection should have failed before.
  TRI_ASSERT(c != nullptr);
  auto newCol = std::make_unique<LogicalCollection>(c);
  TRI_V8_RETURN(WrapCollection(isolate, newCol.release()));
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
    arangodb::LogicalCollection const* collection, v8::Handle<v8::Value> const val) {
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
    CollectionNameResolver resolver(collection->vocbase());

    if (!EqualCollection(&resolver, collectionName, collection)) {
      TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_CROSS_COLLECTION_REQUEST);
    }
  }

  std::string const databaseName(collection->dbName());
  std::string const cid = collection->cid_as_string();
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

  arangodb::LogicalCollection* collection =
      TRI_UnwrapClass<arangodb::LogicalCollection>(args.Holder(), WRP_VOCBASE_COL_TYPE);

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
  
  READ_LOCKER(readLocker, collection->vocbase()->_inventoryLock);

  SingleCollectionTransaction trx(
      V8TransactionContext::Create(collection->vocbase(), true),
      collection->cid(), TRI_TRANSACTION_WRITE);

  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  LogicalCollection* col = trx.documentCollection();

  auto idx = TRI_LookupIndexByHandle(isolate, trx.resolver(), collection,
                                     args[0], true);

  if (idx == nullptr || idx->id() == 0) {
    TRI_V8_RETURN_FALSE();
  }

  if (!idx->canBeDropped()) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_FORBIDDEN);
  }

  bool ok = col->dropIndex(idx->id(), true);

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
    arangodb::LogicalCollection const* collection, bool withFigures) {
  // warning This may be obsolete.
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);

  std::string const databaseName(collection->dbName());
  std::string const cid = collection->cid_as_string();
  std::string const collectionName(collection->name());

  std::shared_ptr<LogicalCollection> c =
      ClusterInfo::instance()->getCollection(databaseName, cid);

  if (c == nullptr) {
    TRI_V8_THROW_EXCEPTION(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND);
  }

  v8::Handle<v8::Array> ret = v8::Array::New(isolate);

  VPackBuilder tmp;
  c->getIndexesVPack(tmp, withFigures);
  VPackSlice slice = tmp.slice();

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

  arangodb::LogicalCollection* collection =
      TRI_UnwrapClass<arangodb::LogicalCollection>(args.Holder(), WRP_VOCBASE_COL_TYPE);

  if (collection == nullptr) {
    TRI_V8_THROW_EXCEPTION_INTERNAL("cannot extract collection");
  }
  
  bool withFigures = false;
  if (args.Length() > 0) {
    withFigures = TRI_ObjectToBoolean(args[0]);
  }

  if (ServerState::instance()->isCoordinator()) {
    GetIndexesCoordinator(args, collection, withFigures);
    return;
  }

  SingleCollectionTransaction trx(
      V8TransactionContext::Create(collection->vocbase(), true),
      collection->cid(), TRI_TRANSACTION_READ);

  int res = trx.begin();

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_V8_THROW_EXCEPTION(res);
  }

  // READ-LOCK start
  trx.lockRead();

  arangodb::LogicalCollection* col = trx.documentCollection();
  std::string const collectionName(col->name());

  // get list of indexes
  TransactionBuilderLeaser builder(&trx);
  auto indexes = col->getIndexes();

  trx.finish(res);
  // READ-LOCK end

  size_t const n = indexes.size();
  v8::Handle<v8::Array> result = v8::Array::New(isolate, static_cast<int>(n));

  for (size_t i = 0; i < n; ++i) {
    auto const& idx = indexes[i];
    builder->clear();
    builder->openObject();
    idx->toVelocyPack(*(builder.get()), withFigures);
    builder->close();
    result->Set(static_cast<uint32_t>(i),
                IndexRep(isolate, collectionName, builder->slice()));
  }

  TRI_V8_RETURN(result);
  TRI_V8_TRY_CATCH_END
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up an index identifier
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<arangodb::Index> TRI_LookupIndexByHandle(
    v8::Isolate* isolate, arangodb::CollectionNameResolver const* resolver,
    arangodb::LogicalCollection const* collection,
    v8::Handle<v8::Value> const val, bool ignoreNotFound) {
  // reset the collection identifier
  std::string collectionName;
  TRI_idx_iid_t iid = 0;

  // assume we are already loaded
  TRI_ASSERT(collection != nullptr);

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

  auto idx = collection->lookupIndex(iid);

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
    v8::Handle<v8::Object> obj = args[1]->ToObject();
    // Add the type and name into the object. Easier in v8 than in VPack
    obj->Set(TRI_V8_ASCII_STRING("type"),
             v8::Number::New(isolate, static_cast<int>(collectionType)));
    obj->Set(TRI_V8_ASCII_STRING("name"), TRI_V8_STD_STRING(name));

    int res = TRI_V8ToVPack(isolate, builder, obj, false);

    if (res != TRI_ERROR_NO_ERROR) {
      TRI_V8_THROW_EXCEPTION(res);
    }

  } else {
    // create an empty properties object
    builder.openObject();
    builder.add("type", VPackValue(static_cast<int>(collectionType)));
    builder.add("name", VPackValue(name));
    builder.close();
  }
    
  infoSlice = builder.slice();

  if (ServerState::instance()->isCoordinator()) {
    auto parameters = std::make_unique<LogicalCollection>(vocbase, infoSlice, false);
    CreateCollectionCoordinator(args, collectionType, vocbase->name(),
                                parameters.get());
    return;
  }

  try {
    TRI_voc_cid_t cid = 0;
    arangodb::LogicalCollection const* collection =
        vocbase->createCollection(infoSlice, cid, true);

    TRI_ASSERT(collection != nullptr);

    v8::Handle<v8::Value> result = WrapCollection(isolate, collection);
    if (result.IsEmpty()) {
      TRI_V8_THROW_EXCEPTION_MEMORY();
    }

    TRI_V8_RETURN(result);
  } catch (basics::Exception const& ex) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(ex.code(), ex.what());
  } catch (std::exception const& ex) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, ex.what());
  } catch (...) {
    TRI_V8_THROW_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "cannot create collection");
  }
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
