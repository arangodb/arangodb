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

#include "Basics/Common.h"
#include "Basics/ReadLocker.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/conversions.h"
#include "Basics/tri-strings.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ClusterMethods.h"
#include "Cluster/ServerState.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "Indexes.h"
#include "Indexes/Index.h"
#include "Indexes/IndexFactory.h"
#include "RestServer/DatabaseFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "Transaction/Helpers.h"
#include "Transaction/Hints.h"
#include "Transaction/StandaloneContext.h"
#include "Transaction/V8Context.h"
#include "Utils/Events.h"
#include "Utils/ExecContext.h"
#include "Utils/SingleCollectionTransaction.h"
#include "V8Server/v8-collection.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/vocbase.h"
#include "Logger/Logger.h"
#include "Logger/LogMacros.h"

#include <velocypack/Builder.h>
#include <velocypack/Collection.h>
#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>
#include <regex>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::methods;

Result Indexes::getIndex(LogicalCollection const* collection, VPackSlice const& indexId,
                         VPackBuilder& out, transaction::Methods* trx) {
  // do some magic to parse the iid
  std::string id;  // will (eventually) be fully-qualified; "collection/identifier"
  std::string name;  // will be just name or id (no "collection/")
  VPackSlice idSlice = indexId;
  if (idSlice.isObject() && idSlice.hasKey(StaticStrings::IndexId)) {
    idSlice = idSlice.get(StaticStrings::IndexId);
  }
  if (idSlice.isString()) {
    std::regex re = std::regex("^([a-zA-Z0-9\\-_]+)\\/([a-zA-Z0-9\\-_]+)$",
                               std::regex::ECMAScript);
    if (std::regex_match(idSlice.copyString(), re)) {
      id = idSlice.copyString();
      name = id.substr(id.find_first_of("/") + 1);
    } else {
      name = idSlice.copyString();
      id = collection->name() + "/" + name;
    }
  } else if (idSlice.isInteger()) {
    name = StringUtils::itoa(idSlice.getUInt());
    id = collection->name() + "/" + name;
  } else {
    return Result(TRI_ERROR_ARANGO_INDEX_NOT_FOUND);
  }

  VPackBuilder tmp;
  Result res =
      Indexes::getAll(collection, Index::makeFlags(), /*withHidden*/ true, tmp, trx);
  if (res.ok()) {
    for (VPackSlice const& index : VPackArrayIterator(tmp.slice())) {
      if (index.get(StaticStrings::IndexId).compareString(id) == 0 ||
          index.get(StaticStrings::IndexName).compareString(name) == 0) {
        out.add(index);
        return Result();
      }
    }
  }
  return Result(TRI_ERROR_ARANGO_INDEX_NOT_FOUND);
}

/// @brief get all indexes, skips view links
arangodb::Result Indexes::getAll(LogicalCollection const* collection,
                                 std::underlying_type<Index::Serialize>::type flags,
                                 bool withHidden, VPackBuilder& result,
                                 transaction::Methods* inputTrx) {
  VPackBuilder tmp;
  if (ServerState::instance()->isCoordinator()) {
    TRI_ASSERT(collection);
    auto& databaseName = collection->vocbase().name();
    std::string const& cid = collection->name();

    std::unordered_map<std::string, double> estimates;

    if (Index::hasFlag(flags, Index::Serialize::Estimates)) {
      auto& feature = collection->vocbase().server().getFeature<ClusterFeature>();
      Result rv = selectivityEstimatesOnCoordinator(feature, databaseName, cid, estimates);
      if (rv.fail()) {
        return Result(rv.errorNumber(), "could not retrieve estimates: '" + rv.errorMessage() + "'");
      }

      // we will merge in the index estimates later
      flags &= ~Index::makeFlags(Index::Serialize::Estimates);
    }

    VPackBuilder tmpInner;
    auto& ci = collection->vocbase().server().getFeature<ClusterFeature>().clusterInfo();
    auto c = ci.getCollection(databaseName, cid);
    c->getIndexesVPack(tmpInner, [withHidden, flags](arangodb::Index const* idx, decltype(flags)& indexFlags) {
      if (withHidden || !idx->isHidden()) {
        indexFlags = flags;
        return true;
      }

      return false;
    });

    tmp.openArray();
    for (VPackSlice const& s : VPackArrayIterator(tmpInner.slice())) {
      auto id = arangodb::velocypack::StringRef(s.get(StaticStrings::IndexId));
      auto found = std::find_if(estimates.begin(), estimates.end(),
                                [&id](std::pair<std::string, double> const& v) {
                                  return id == v.first;
                                });
      if (found == estimates.end()) {
        tmp.add(s);  // just copy
      } else {
        tmp.openObject();
        tmp.add(VPackObjectIterator(s, true));
        tmp.add("selectivityEstimate", VPackValue(found->second));
        tmp.close();
      }
    }
    tmp.close();

  } else {
    std::shared_ptr<transaction::Methods> trx;
    if (inputTrx) {
      trx = std::shared_ptr<transaction::Methods>(inputTrx, [](transaction::Methods*) {});
    } else {
      trx = std::make_shared<SingleCollectionTransaction>(
          transaction::StandaloneContext::Create(collection->vocbase()),
          *collection, AccessMode::Type::READ);

      // we actually need this hint here, so that the collection is not
      // loaded if it has status unloaded.
      trx->addHint(transaction::Hints::Hint::NO_USAGE_LOCK);

      Result res = trx->begin();
      if (!res.ok()) {
        return res;
      }
    }

    // get list of indexes
    auto indexes = collection->getIndexes();

    tmp.openArray(true);
    for (std::shared_ptr<arangodb::Index> const& idx : indexes) {
      if (!withHidden && idx->isHidden()) {
        continue;
      }
      idx->toVelocyPack(tmp, flags);
    }
    tmp.close();

    if (!inputTrx) {
      Result res;
      res = trx->finish(res);
      if (res.fail()) {
        return res;
      }
    }
  }

  bool mergeEdgeIdxs = !ServerState::instance()->isDBServer();
  
  double selectivity = 0, memory = 0, cacheSize = 0, cacheUsage = 0,
         cacheLifeTimeHitRate = 0, cacheWindowedHitRate = 0;

  VPackArrayBuilder a(&result);
  for (VPackSlice const& index : VPackArrayIterator(tmp.slice())) {
    std::string id = collection->name() + TRI_INDEX_HANDLE_SEPARATOR_CHR +
                     index.get(arangodb::StaticStrings::IndexId).copyString();
    VPackBuilder merge;
    merge.openObject(true);
    merge.add(arangodb::StaticStrings::IndexId, arangodb::velocypack::Value(id));
    
    auto type = index.get(arangodb::StaticStrings::IndexType);
    if (mergeEdgeIdxs && Index::type(type.copyString()) == Index::TRI_IDX_TYPE_EDGE_INDEX) {
      VPackSlice fields = index.get(StaticStrings::IndexFields);
      TRI_ASSERT(fields.isArray() && fields.length() <= 2);
      
      if (fields.length() == 1) {  // merge indexes
        // read out relevant values
        VPackSlice val = index.get("selectivityEstimate");

        if (val.isNumber()) {
          selectivity += val.getNumber<double>();
        }

        bool useCache = false;
        VPackSlice figures = index.get("figures");

        if (figures.isObject() && !figures.isEmptyObject()) {
          if ((val = figures.get("cacheInUse")).isBool()) {
            useCache = val.getBool();
          }

          if ((val = figures.get("memory")).isNumber()) {
            memory += val.getNumber<double>();
          }

          if ((val = figures.get("cacheSize")).isNumber()) {
            cacheSize += val.getNumber<double>();
          }

          if ((val = figures.get("cacheUsage")).isNumber()) {
            cacheUsage += val.getNumber<double>();
          }

          if ((val = figures.get("cacheLifeTimeHitRate")).isNumber()) {
            cacheLifeTimeHitRate += val.getNumber<double>();
          }

          if ((val = figures.get("cacheWindowedHitRate")).isNumber()) {
            cacheWindowedHitRate += val.getNumber<double>();
          }
        }

        if (fields[0].compareString(StaticStrings::FromString) == 0) {
          continue;
        } else if (fields[0].compareString(StaticStrings::ToString) == 0) {
          merge.add(StaticStrings::IndexFields, VPackValue(VPackValueType::Array));
          merge.add(VPackValue(StaticStrings::FromString));
          merge.add(VPackValue(StaticStrings::ToString));
          merge.close();

          merge.add("selectivityEstimate", VPackValue(selectivity / 2));
          if (Index::hasFlag(flags, Index::Serialize::Figures)) {
            merge.add("figures", VPackValue(VPackValueType::Object));
            merge.add("memory", VPackValue(memory));
            if (useCache) {
              merge.add("cacheSize", VPackValue(cacheSize));
              merge.add("cacheUsage", VPackValue(cacheUsage));
              merge.add("cacheLifeTimeHitRate", VPackValue(cacheLifeTimeHitRate / 2));
              merge.add("cacheWindowedHitRate", VPackValue(cacheWindowedHitRate / 2));
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
                               VPackSlice const& definition, bool create,
                               VPackBuilder& output) {
  TRI_ASSERT(collection != nullptr);

  Result res;
  bool created = false;
  std::shared_ptr<arangodb::Index> idx;

  READ_LOCKER(readLocker, collection->vocbase()._inventoryLock);

  if (create) {
    try {
      idx = collection->createIndex(definition, created);
    } catch (arangodb::basics::Exception const& e) {
      return res.reset(e.code(), e.what());
    }
    TRI_ASSERT(idx != nullptr);
  } else {
    idx = collection->lookupIndex(definition);
    if (idx == nullptr) {
      // Index not found
      return res.reset(TRI_ERROR_ARANGO_INDEX_NOT_FOUND);
    }
  }

  readLocker.unlock();

  TRI_ASSERT(idx != nullptr);

  VPackBuilder tmp;
  try {
    idx->toVelocyPack(tmp, Index::makeFlags(Index::Serialize::Estimates));
  } catch (...) {
    return res.reset(TRI_ERROR_OUT_OF_MEMORY);
  }

  std::string iid = StringUtils::itoa(idx->id().id());
  VPackBuilder b;
  b.openObject();
  b.add("isNewlyCreated", VPackValue(created));
  b.add(StaticStrings::IndexId,
        VPackValue(collection->name() + TRI_INDEX_HANDLE_SEPARATOR_CHR + iid));
  b.close();
  output = VPackCollection::merge(tmp.slice(), b.slice(), false);
  return res;
}

Result Indexes::ensureIndexCoordinator(arangodb::LogicalCollection const* collection,
                                       VPackSlice const& indexDef, bool create,
                                       VPackBuilder& resultBuilder) {
  TRI_ASSERT(collection != nullptr);
  auto& cluster = collection->vocbase().server().getFeature<ClusterFeature>();

  return cluster.clusterInfo().ensureIndexCoordinator(  // create index
      *collection, indexDef, create, resultBuilder, cluster.indexCreationTimeout());
}

Result Indexes::ensureIndex(LogicalCollection* collection, VPackSlice const& input,
                            bool create, VPackBuilder& output) {
  // can read indexes with RO on db and collection. Modifications require RW/RW
  ExecContext const& exec = ExecContext::current();
  if (!exec.isSuperuser()) {
    auth::Level lvl = exec.databaseAuthLevel();
    bool canModify = exec.canUseCollection(collection->name(), auth::Level::RW);
    bool canRead = exec.canUseCollection(collection->name(), auth::Level::RO);
    if ((create && (lvl != auth::Level::RW || !canModify)) ||
        (lvl == auth::Level::NONE || !canRead)) {
      events::CreateIndex(collection->vocbase().name(), collection->name(),
                          input, TRI_ERROR_FORBIDDEN);
      return Result(TRI_ERROR_FORBIDDEN);
    }
  }

  TRI_ASSERT(collection);
  VPackBuilder normalized;
  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  auto res = engine->indexFactory().enhanceIndexDefinition(  // normalize definition
      input, normalized, create, collection->vocbase()       // args
  );

  if (res.fail()) {
    events::CreateIndex(collection->vocbase().name(), collection->name(), input,
                        res.errorNumber());
    return res;
  }

  VPackSlice indexDef = normalized.slice();

  if (ServerState::instance()->isCoordinator()) {
    TRI_ASSERT(indexDef.isObject());

    // check if there is an attempt to create a unique index on non-shard keys
    if (create) {
      Index::validateFields(indexDef);
      auto v = indexDef.get(arangodb::StaticStrings::IndexUnique);

      /* the following combinations of shardKeys and indexKeys are allowed/not
       allowed:

       shardKeys     indexKeys
       a             a            ok
       a             b        not ok
       a             a b          ok
       a b             a      not ok
       a b             b      not ok
       a b           a b          ok
       a b           a b c        ok
       a b c           a b    not ok
       a b c         a b c        ok
       */

      if (v.isBoolean() && v.getBoolean()) {
        // unique index, now check if fields and shard keys match
        auto flds = indexDef.get(arangodb::StaticStrings::IndexFields);

        if (flds.isArray() && collection->numberOfShards() > 1) {
          std::vector<std::string> const& shardKeys = collection->shardKeys();
          std::unordered_set<std::string> indexKeys;
          size_t n = static_cast<size_t>(flds.length());

          for (size_t i = 0; i < n; ++i) {
            VPackSlice f = flds.at(i);
            if (!f.isString()) {
              // index attributes must be strings
              events::CreateIndex(collection->vocbase().name(), collection->name(),
                                  indexDef, TRI_ERROR_INTERNAL);
              return Result(TRI_ERROR_INTERNAL,
                            "index field names should be strings");
            }
            indexKeys.emplace(f.copyString());
          }

          // all shard-keys must be covered by the index
          for (auto& it : shardKeys) {
            if (indexKeys.find(it) == indexKeys.end()) {
              events::CreateIndex(collection->vocbase().name(), collection->name(),
                                  indexDef, TRI_ERROR_CLUSTER_UNSUPPORTED);
              return Result(TRI_ERROR_CLUSTER_UNSUPPORTED,
                            "shard key '" + it +
                                "' must be present in unique index");
            }
          }
        }
      }
    }
  }

  TRI_ASSERT(!indexDef.isNone());
  events::CreateIndex(collection->vocbase().name(), collection->name(),
                      indexDef, TRI_ERROR_NO_ERROR);

  // ensure an index, coordinator case
  if (ServerState::instance()->isCoordinator()) {
    VPackBuilder tmp;
#ifdef USE_ENTERPRISE
    Result res = Indexes::ensureIndexCoordinatorEE(collection, indexDef, create, tmp);
#else
    Result res = Indexes::ensureIndexCoordinator(collection, indexDef, create, tmp);
#endif
    if (!res.ok()) {
      events::CreateIndex(collection->vocbase().name(), collection->name(),
                          indexDef, res.errorNumber());
      return res;
    } else if (tmp.slice().isNone()) {
      // did not find a suitable index
      int code = create ? TRI_ERROR_OUT_OF_MEMORY : TRI_ERROR_ARANGO_INDEX_NOT_FOUND;
      events::CreateIndex(collection->vocbase().name(), collection->name(), indexDef, code);
      return Result(code);
    }

    // flush estimates
    collection->flushClusterIndexEstimates();

    // the cluster won't set a proper id value
    std::string iid = tmp.slice().get(StaticStrings::IndexId).copyString();
    VPackBuilder b;
    b.openObject();
    b.add(StaticStrings::IndexId,
          VPackValue(collection->name() + TRI_INDEX_HANDLE_SEPARATOR_CHR + iid));
    b.close();
    output = VPackCollection::merge(tmp.slice(), b.slice(), false);
    events::CreateIndex(collection->vocbase().name(), collection->name(),
                        indexDef, res.errorNumber());
    return res;
  } else {
    Result res = EnsureIndexLocal(collection, indexDef, create, output);
    events::CreateIndex(collection->vocbase().name(), collection->name(),
                        indexDef, res.errorNumber());
    return res;
  }
}

arangodb::Result Indexes::createIndex(LogicalCollection* coll, Index::IndexType type,
                                      std::vector<std::string> const& fields,
                                      bool unique, bool sparse) {
  VPackBuilder props;

  props.openObject();
  props.add(arangodb::StaticStrings::IndexType,
            arangodb::velocypack::Value(Index::oldtypeName(type)));
  props.add(arangodb::StaticStrings::IndexFields,
            arangodb::velocypack::Value(VPackValueType::Array));

  for (std::string const& field : fields) {
    props.add(VPackValue(field));
  }

  props.close();
  props.add(arangodb::StaticStrings::IndexUnique, arangodb::velocypack::Value(unique));
  props.add(arangodb::StaticStrings::IndexSparse, arangodb::velocypack::Value(sparse));
  props.close();

  VPackBuilder ignored;
  return ensureIndex(coll, props.slice(), true, ignored);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if argument is an index identifier
////////////////////////////////////////////////////////////////////////////////

static bool ExtractIndexHandle(VPackSlice const& arg,
                               std::string& collectionName, IndexId& iid) {
  TRI_ASSERT(collectionName.empty());
  TRI_ASSERT(iid.empty());

  if (arg.isNumber()) {
    // numeric index id
    iid = (IndexId)arg.getUInt();
    return true;
  }

  if (!arg.isString()) {
    return false;
  }

  std::string str = arg.copyString();
  size_t split;
  if (arangodb::Index::validateHandle(str.data(), &split)) {
    collectionName = std::string(str.data(), split);
    iid = IndexId{StringUtils::uint64(str.data() + split + 1, str.length() - split - 1)};
    return true;
  }

  if (arangodb::Index::validateId(str.data())) {
    iid = IndexId{StringUtils::uint64(str)};
    return true;
  }
  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if argument is an index name
////////////////////////////////////////////////////////////////////////////////

static bool ExtractIndexName(VPackSlice const& arg, std::string& collectionName,
                             std::string& name) {
  TRI_ASSERT(collectionName.empty());
  TRI_ASSERT(name.empty());

  if (!arg.isString()) {
    return false;
  }

  std::string str = arg.copyString();
  size_t split;
  if (arangodb::Index::validateHandleName(str.data(), &split)) {
    collectionName = std::string(str.data(), split);
    name = std::string(str.data() + split + 1, str.length() - split - 1);
    return true;
  }

  if (arangodb::Index::validateName(str.data())) {
    name = str;
    return true;
  }
  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up an index identifier
////////////////////////////////////////////////////////////////////////////////

Result Indexes::extractHandle(arangodb::LogicalCollection const* collection,
                              arangodb::CollectionNameResolver const* resolver,
                              VPackSlice const& val, IndexId& iid, std::string& name) {
  // reset the collection identifier
  std::string collectionName;

  // assume we are already loaded
  TRI_ASSERT(collection != nullptr);

  // extract the index identifier from a string
  if (val.isString() || val.isNumber()) {
    if (!ExtractIndexHandle(val, collectionName, iid) &&
        !ExtractIndexName(val, collectionName, name)) {
      return Result(TRI_ERROR_ARANGO_INDEX_HANDLE_BAD);
    }
  }

  // extract the index identifier from an object
  else if (val.isObject()) {
    VPackSlice iidVal = val.get(StaticStrings::IndexId);
    if (!ExtractIndexHandle(iidVal, collectionName, iid)) {
      VPackSlice nameVal = val.get(StaticStrings::IndexName);
      if (!ExtractIndexName(nameVal, collectionName, name)) {
        return Result(TRI_ERROR_ARANGO_INDEX_HANDLE_BAD);
      }
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

arangodb::Result Indexes::drop(LogicalCollection* collection, VPackSlice const& indexArg) {
  TRI_ASSERT(collection);
  ExecContext const& exec = ExecContext::current();
  if (!exec.isSuperuser()) {
    if (exec.databaseAuthLevel() != auth::Level::RW ||
        !exec.canUseCollection(collection->name(), auth::Level::RW)) {
      events::DropIndex(collection->vocbase().name(), collection->name(), "", TRI_ERROR_FORBIDDEN);
      return TRI_ERROR_FORBIDDEN;
    }
  }

  IndexId iid = IndexId::none();
  std::string name;
  auto getHandle = [collection, &indexArg, &iid,
                    &name](CollectionNameResolver const* resolver,
                           transaction::Methods* trx = nullptr) -> Result {
    Result res = Indexes::extractHandle(collection, resolver, indexArg, iid, name);

    if (!res.ok()) {
      events::DropIndex(collection->vocbase().name(), collection->name(), "",
                        res.errorNumber());
      return res;
    }

    if (iid.empty() && !name.empty()) {
      VPackBuilder builder;
      res = methods::Indexes::getIndex(collection, indexArg, builder, trx);
      if (!res.ok()) {
        events::DropIndex(collection->vocbase().name(), collection->name(), "",
                          res.errorNumber());
        return res;
      }

      VPackSlice idSlice = builder.slice().get(StaticStrings::IndexId);
      Result res = Indexes::extractHandle(collection, resolver, idSlice, iid, name);

      if (!res.ok()) {
        events::DropIndex(collection->vocbase().name(), collection->name(), "",
                          res.errorNumber());
      }
    }

    return res;
  };

  if (ServerState::instance()->isCoordinator()) {
    CollectionNameResolver resolver(collection->vocbase());
    Result res = getHandle(&resolver);
    if (!res.ok()) {
      return res;
    }

    // flush estimates
    collection->flushClusterIndexEstimates();

#ifdef USE_ENTERPRISE
    res = Indexes::dropCoordinatorEE(collection, iid);
#else
    auto& ci = collection->vocbase().server().getFeature<ClusterFeature>().clusterInfo();
    res = ci.dropIndexCoordinator(  // drop index
        collection->vocbase().name(), std::to_string(collection->id()), iid, 0.0  // args
    );
#endif
    events::DropIndex(collection->vocbase().name(), collection->name(),
                      std::to_string(iid.id()), res.errorNumber());
    return res;
  } else {
    READ_LOCKER(readLocker, collection->vocbase()._inventoryLock);

    SingleCollectionTransaction trx(
        transaction::V8Context::CreateWhenRequired(collection->vocbase(), false),
        *collection, AccessMode::Type::EXCLUSIVE);
    Result res = trx.begin();

    if (!res.ok()) {
      events::DropIndex(collection->vocbase().name(), collection->name(), "",
                        res.errorNumber());
      return res;
    }

    LogicalCollection* col = trx.documentCollection();
    res = getHandle(trx.resolver(), &trx);
    if (!res.ok()) {
      return res;
    }

    std::shared_ptr<Index> idx = collection->lookupIndex(iid);
    if (!idx || idx->id().empty() || idx->id().isPrimary()) {
      events::DropIndex(collection->vocbase().name(), collection->name(),
                        std::to_string(iid.id()), TRI_ERROR_ARANGO_INDEX_NOT_FOUND);
      return Result(TRI_ERROR_ARANGO_INDEX_NOT_FOUND);
    }
    if (!idx->canBeDropped()) {
      events::DropIndex(collection->vocbase().name(), collection->name(),
                        std::to_string(iid.id()), TRI_ERROR_FORBIDDEN);
      return Result(TRI_ERROR_FORBIDDEN);
    }

    bool ok = col->dropIndex(idx->id());
    int code = ok ? TRI_ERROR_NO_ERROR : TRI_ERROR_FAILED;
    events::DropIndex(collection->vocbase().name(), collection->name(),
                      std::to_string(iid.id()), code);
    return Result(code);
  }
}
