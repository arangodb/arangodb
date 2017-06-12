////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "Indexes.h"
#include "Basics/Common.h"

#include "Agency/AgencyComm.h"
#include "Basics/StringUtils.h"
#include "Basics/ReadLocker.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/conversions.h"
#include "Basics/tri-strings.h"
#include "Basics/StringUtils.h"
#include "Cluster/ClusterComm.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "Rest/HttpRequest.h"
#include "RestServer/DatabaseFeature.h"
//#include "V8/v8-conv.h"
//#include "V8/v8-utils.h"
//#include "V8/v8-vpack.h"
#include "V8Server/v8-collection.h"
#include "V8Server/V8DealerFeature.h"
#include "VocBase/AuthInfo.h"
#include "VocBase/modes.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/vocbase.h"
#include "Indexes/Index.h"
#include "Indexes/IndexFactory.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "Transaction/StandaloneContext.h"
#include "Transaction/Helpers.h"
#include "Transaction/Hints.h"
#include "Utils/SingleCollectionTransaction.h"
#include "Utils/Events.h"

#include <velocypack/Builder.h>
#include <velocypack/Collection.h>
#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>
#include <regex>

using namespace arangodb;
using namespace arangodb::actions;
using namespace arangodb::basics;

Result Indexes::getIndex(arangodb::LogicalCollection const* collection,
                         VPackSlice const& indexId,
                         VPackBuilder& out) {
  
  // do some magic to parse the iid
  std::string name;
  VPackSlice id = indexId;
  if (id.isObject() && id.hasKey("id")) {
    id = id.get("id");
  }
  if (id.isString()) {
    std::regex re = std::regex("^([a-zA-Z0-9\\-_]+)\\/([0-9]+)$", std::regex::ECMAScript);
    if (std::regex_match(id.copyString(), re)) {
      name = id.copyString();
    } else {
      name = collection->name() + "/" + id.copyString();
    }
  } else if (id.isInteger()) {
    name = collection->name() + "/" + StringUtils::itoa(id.getUInt());
  } else {
    return Result(TRI_ERROR_ARANGO_INDEX_NOT_FOUND);
  }
  
  VPackBuilder tmp;
  Result res = Indexes::getAll(collection, false, tmp);
  if (res.ok()) {
    for (VPackSlice const& index : VPackArrayIterator(tmp.slice())) {
      if (index.get("id").compareString(name) == 0) {
        out.add(index);
        return Result();
      }
    }
  }
  return Result(TRI_ERROR_ARANGO_INDEX_NOT_FOUND);
}

arangodb::Result Indexes::getAll(arangodb::LogicalCollection const* collection,
                                bool withFigures,
                                VPackBuilder& result) {

  VPackBuilder tmp;
  if (ServerState::instance()->isCoordinator()) {
    
    std::string const databaseName(collection->dbName());
    std::string const cid = collection->cid_as_string();
    std::string const collectionName(collection->name());
    
    auto c = ClusterInfo::instance()->getCollection(databaseName, cid);
    c->getIndexesVPack(tmp, withFigures, false);
    
  } else {
    // add locks for consistency
    
    SingleCollectionTransaction trx(transaction::StandaloneContext::Create(collection->vocbase()),
                                    collection->cid(), AccessMode::Type::READ);
    trx.addHint(transaction::Hints::Hint::NO_USAGE_LOCK);
    
    Result res = trx.begin();
    if (!res.ok()) {
      return res;
    }
    
    // READ-LOCK start
    trx.lockRead();
    std::string const collectionName(collection->name());
    
    // get list of indexes
    auto indexes = collection->getIndexes();
    tmp.openArray(true);
    for (std::shared_ptr<arangodb::Index> const& idx : indexes) {
      idx->toVelocyPack(tmp, withFigures, false);
    }
    tmp.close();
    trx.finish(res);
    // READ-LOCK end
  }
  
  double selectivity = 0, memory = 0, cacheSize = 0,
  cacheLiftimeHitRate = 0,
  cacheWindowHitRate = 0;
  
  VPackArrayBuilder a(&result);
  for (VPackSlice const& index : VPackArrayIterator(tmp.slice())) {
    VPackSlice type = index.get("type");
    std::string id = collection->name() + TRI_INDEX_HANDLE_SEPARATOR_CHR
                                        + index.get("id").copyString();
    VPackBuilder merge;
    merge.openObject(true);
    merge.add("id", VPackValue(id));
    
    if (type.isString() && type.compareString("edge") == 0) {
      VPackSlice fields = index.get("fields");
      TRI_ASSERT(fields.isArray() && fields.length() <= 2);
      if (fields.length() == 1) {// merge indexes
        
        // read out relevant values
        VPackSlice val = index.get("selectivityEstimate");
        if (val.isNumber()) {
          selectivity += val.getNumber<double>();
        }
        VPackSlice figures = index.get("figures");
        if (figures.isObject() && !figures.isEmptyObject()) {
          if ((val = figures.get("memory")).isNumber()) {
            memory += val.getNumber<double>();
          }
          if ((val = figures.get("cacheLiftimeHitRate")).isNumber()) {
            cacheLiftimeHitRate += val.getNumber<double>();
          }
          if ((val = figures.get("cacheWindowHitRate")).isNumber()) {
            cacheWindowHitRate += val.getNumber<double>();
          }
        }
        
        if (fields[0].compareString("_from") == 0) {
          continue;
        } else if (fields[0].compareString("_to") == 0) {
          merge.add("selectivityEstimate", VPackValue(selectivity / 2));
          if (withFigures) {
            merge.add("figures", VPackValue(VPackValueType::Object));
            merge.add("memory", VPackValue(memory));
            if (cacheSize != 0 || cacheLiftimeHitRate != 0) {
              merge.add("cacheSize", VPackValue(cacheSize));
              merge.add("cacheLiftimeHitRate", VPackValue(cacheLiftimeHitRate / 2));
              merge.add("cacheWindowHitRate", VPackValue(cacheWindowHitRate / 2));
            }
            merge.close();
          }
        }
      }
    }
    merge.close();
    merge = VPackCollection::merge(index, merge.slice(), true);
    result.add(merge.slice());
  }
  return Result();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures an index, locally
////////////////////////////////////////////////////////////////////////////////

static Result EnsureIndexLocal(arangodb::LogicalCollection* collection,
                               VPackSlice const& definition,
                               bool create, VPackBuilder& output) {
  
  TRI_ASSERT(collection != nullptr);
  READ_LOCKER(readLocker, collection->vocbase()->_inventoryLock);
  
  SingleCollectionTransaction trx(
                                  transaction::StandaloneContext::Create(collection->vocbase()),
                                  collection->cid(),
                                  create ? AccessMode::Type::EXCLUSIVE : AccessMode::Type::READ);
  
  Result res = trx.begin();
  if (!res.ok()) {
    return res;
  }
  
  // disallow index creation in read-only mode
  if (!collection->isSystem() && create &&
      TRI_GetOperationModeServer() == TRI_VOCBASE_MODE_NO_CREATE) {
    return Result(TRI_ERROR_ARANGO_READ_ONLY);
  }
  
  bool created = false;
  std::shared_ptr<arangodb::Index> idx;
  if (create) {
    // TODO Encapsulate in try{}catch(){} instead of errno()
    try {
      idx = collection->createIndex(&trx, definition, created);
    } catch (arangodb::basics::Exception e) {
      return Result(e.code());
    }
    if (idx == nullptr) {
      // something went wrong during creation
      int res = TRI_errno();
      return Result(res);
    }
  } else {
    idx = collection->lookupIndex(definition);
    if (idx == nullptr) {
      // Index not found
      return Result(TRI_ERROR_ARANGO_INDEX_NOT_FOUND);
    }
  }
  
  VPackBuilder tmp;
  try {
    idx->toVelocyPack(tmp, false, false);
  } catch (...) {
    return Result(TRI_ERROR_OUT_OF_MEMORY);
  }
  // builder->close();
  res = trx.commit();
  if (!res.ok()) {
    return res;
  }
  
  std::string iid = StringUtils::itoa(idx->id());
  VPackBuilder b;
  b.openObject();
  b.add("isNewlyCreated", VPackValue(created));
  b.add("id", VPackValue(collection->name() + TRI_INDEX_HANDLE_SEPARATOR_CHR + iid));
  b.close();
  output = VPackCollection::merge(tmp.slice(), b.slice(), false);
  return res;
}

Result Indexes::ensureIndex(arangodb::LogicalCollection* collection,
                                    VPackSlice const& definition,
                                    bool create,
                                    VPackBuilder& output) {
  
  VPackBuilder defBuilder;
  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  IndexFactory const* idxFactory = engine->indexFactory();
  int res = idxFactory->enhanceIndexDefinition(definition, defBuilder, create,
                                            ServerState::instance()->isCoordinator());
  
  if (res != TRI_ERROR_NO_ERROR) {
    return Result(res);
  }
  
  std::string const dbname(collection->dbName());
  std::string const cid = collection->cid_as_string();
  std::string const collname(collection->name());
  VPackSlice indexDef = defBuilder.slice();
  if (ServerState::instance()->isCoordinator()) {
    TRI_ASSERT(indexDef.isObject());
    auto c = ClusterInfo::instance()->getCollection(dbname, collname);
    
    // check if there is an attempt to create a unique index on non-shard keys
    if (create) {
      Index::validateFields(indexDef);
      VPackSlice v = indexDef.get("unique");
      
      /* the following combinations of shardKeys and indexKeys are allowed/not
       allowed:
       
       shardKeys     indexKeys
       a             a        ok
       a             b    not ok
       a           a b        ok
       a b             a    not ok
       a b             b    not ok
       a b           a b        ok
       a b         a b c        ok
       a b c           a b    not ok
       a b c         a b c        ok
       */
      
      if (v.isBoolean() && v.getBoolean()) {
        // unique index, now check if fields and shard keys match
        VPackSlice flds = indexDef.get("fields");
        if (flds.isArray() && c->numberOfShards() > 1) {
          std::vector<std::string> const& shardKeys = c->shardKeys();
          std::unordered_set<std::string> indexKeys;
          size_t n = static_cast<size_t>(flds.length());
          
          for (size_t i = 0; i < n; ++i) {
            VPackSlice f = flds.at(i);
            if (!f.isString()) {
              // index attributes must be strings
              return Result(TRI_ERROR_INTERNAL, "index field names should be strings");
            }
            indexKeys.emplace(f.copyString());
          }
          
          // all shard-keys must be covered by the index
          for (auto& it : shardKeys) {
            if (indexKeys.find(it) == indexKeys.end()) {
              return Result(TRI_ERROR_CLUSTER_UNSUPPORTED,
                            "shard key '" + it + "' must be present in unique index");
            }
          }
        }
      }
    }
  }
  
  TRI_ASSERT(!indexDef.isNone());
  events::CreateIndex(collection->name(), indexDef);
  
  // ensure an index, coordinator case
  if (ServerState::instance()->isCoordinator()) {
    std::string errorMsg;
    VPackBuilder tmp;
#ifdef USE_ENTERPRISE
    int res = EnsureIndexCoordinatorEnterprise(collection, indexDef, create,
                                               tmp, errorMsg);
#else
    int res = ClusterInfo::instance()->ensureIndexCoordinator(dbname, cid,
                                                              indexDef, create, &arangodb::Index::Compare,
                                                              tmp, errorMsg, 360.0);
#endif
    if (res != TRI_ERROR_NO_ERROR) {
      return Result(res);
    } else if (tmp.slice().isNone()) {
      // did not find a suitable index
      return Result(create ? TRI_ERROR_OUT_OF_MEMORY : TRI_ERROR_ARANGO_INDEX_NOT_FOUND);
    }
    // the cluster won't set a proper id value
    std::string iid = tmp.slice().get("id").copyString();
    VPackBuilder b;
    b.openObject();
    b.add("id", VPackValue(collection->name() + TRI_INDEX_HANDLE_SEPARATOR_CHR + iid));
    b.close();
    output = VPackCollection::merge(tmp.slice(), b.slice(), false);
    return Result();
  } else {
    return EnsureIndexLocal(collection, indexDef, create, output);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if argument is an index identifier
////////////////////////////////////////////////////////////////////////////////

static bool ExtractIndexHandle(VPackSlice const& arg,
                               std::string& collectionName,
                               TRI_idx_iid_t& iid) {
  TRI_ASSERT(collectionName.empty());
  TRI_ASSERT(iid == 0);
  
  if (arg.isNumber()) {
    // numeric index id
    iid = (TRI_idx_iid_t)arg.getUInt();
    return true;
  }
  
  if (!arg.isString()) {
    return false;
  }
  
  std::string str = arg.copyString();
  size_t split;
  if (arangodb::Index::validateHandle(str.data(), &split)) {
    collectionName = std::string(str.data(), split);
    iid = StringUtils::uint64(str.data() + split + 1, str.length() - split - 1);
    return true;
  }
  
  if (arangodb::Index::validateId(str.data())) {
    iid = StringUtils::uint64(str);
    return true;
  }
  return false;
}


////////////////////////////////////////////////////////////////////////////////
/// @brief looks up an index identifier
////////////////////////////////////////////////////////////////////////////////

Result Indexes::extractHandle(arangodb::LogicalCollection const* collection,
                               arangodb::CollectionNameResolver const* resolver,
                               VPackSlice const& val,
                               TRI_idx_iid_t& iid) {
  // reset the collection identifier
  std::string collectionName;
  
  // assume we are already loaded
  TRI_ASSERT(collection != nullptr);
  
  // extract the index identifier from a string
  if (val.isString() || val.isNumber()) {
    if (!ExtractIndexHandle(val, collectionName, iid)) {
      return Result(TRI_ERROR_ARANGO_INDEX_HANDLE_BAD);
    }
  }
  
  // extract the index identifier from an object
  else if (val.isObject()) {
    VPackSlice iidVal = val.get("id");
    
    if (!ExtractIndexHandle(iidVal, collectionName, iid)) {
      return Result(TRI_ERROR_ARANGO_INDEX_HANDLE_BAD);
    }
  }
  
  if (!collectionName.empty()) {
    if (!EqualCollection(resolver, collectionName, collection)) {
      // I wish this error provided me with more information!
      // e.g. 'cannot access index outside the collection it was defined in'
      return Result(TRI_ERROR_ARANGO_CROSS_COLLECTION_REQUEST);
    }
  }
  return Result();
}

arangodb::Result Indexes::drop(arangodb::LogicalCollection const* collection,
                               VPackSlice const& indexArg) {
  
  TRI_idx_iid_t iid = 0;
  if (ServerState::instance()->isCoordinator()) {
    
    CollectionNameResolver resolver(collection->vocbase());
    Result res = Indexes::extractHandle(collection, &resolver, indexArg, iid);
    if (!res.ok()) {
      return res;
    }
    
#ifdef USE_ENTERPRISE
    int ret = DropIndexCoordinatorEnterprise(collection, iid);
    return Result(ret);
#else
    std::string const databaseName(collection->dbName());
    std::string const cid = collection->cid_as_string();
    std::string errorMsg;
    int ret = ClusterInfo::instance()->dropIndexCoordinator(databaseName, cid, iid,
                                                           errorMsg, 0.0);
    return Result(ret, errorMsg);
#endif
  } else {
    READ_LOCKER(readLocker, collection->vocbase()->_inventoryLock);
    
    SingleCollectionTransaction trx(transaction::StandaloneContext::Create(collection->vocbase()),
                                    collection->cid(), AccessMode::Type::EXCLUSIVE);
    
    Result res = trx.begin();
    if (!res.ok()) {
      return res;
    }
    
    LogicalCollection* col = trx.documentCollection();
    res = Indexes::extractHandle(collection, trx.resolver(), indexArg, iid);
    if (!res.ok()) {
      return res;
    }
    
    std::shared_ptr<Index> idx = collection->lookupIndex(iid);
    if (!idx || idx->id() == 0) {
      return Result(TRI_ERROR_ARANGO_INDEX_NOT_FOUND);
    }
    if (!idx->canBeDropped()) {
      return Result(TRI_ERROR_FORBIDDEN);
    }
    
    bool ok = col->dropIndex(idx->id());
    return ok ? Result() : Result(TRI_ERROR_FAILED);
  }
}
