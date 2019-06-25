////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ServerState.h"
#include "Indexes/Index.h"
#include "RocksDBEngine/RocksDBEdgeIndex.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "RocksDBEngine/RocksDBFulltextIndex.h"
#include "RocksDBEngine/RocksDBGeoIndex.h"
#include "RocksDBEngine/RocksDBHashIndex.h"
#include "RocksDBEngine/RocksDBPersistentIndex.h"
#include "RocksDBEngine/RocksDBPrimaryIndex.h"
#include "RocksDBEngine/RocksDBSkiplistIndex.h"
#include "RocksDBEngine/RocksDBTtlIndex.h"
#include "RocksDBIndexFactory.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ticks.h"
#include "VocBase/voc-types.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

#include "IResearch/IResearchRocksDBLink.h"

using namespace arangodb;

namespace {

struct DefaultIndexFactory : public arangodb::IndexTypeFactory {
  arangodb::Index::IndexType const _type;

  explicit DefaultIndexFactory(arangodb::Index::IndexType type) : _type(type) {}

  bool equal(arangodb::velocypack::Slice const& lhs,
             arangodb::velocypack::Slice const& rhs) const override {
    return arangodb::IndexTypeFactory::equal(_type, lhs, rhs, true);
  }
};

struct EdgeIndexFactory : public DefaultIndexFactory {
  EdgeIndexFactory()
      : DefaultIndexFactory(arangodb::Index::TRI_IDX_TYPE_EDGE_INDEX) {}

  arangodb::Result instantiate(std::shared_ptr<arangodb::Index>& index,
                               arangodb::LogicalCollection& collection,
                               arangodb::velocypack::Slice const& definition,
                               TRI_idx_iid_t id, bool isClusterConstructor) const override {
    if (!isClusterConstructor) {
      // this indexes cannot be created directly
      return arangodb::Result(TRI_ERROR_INTERNAL, "cannot create edge index");
    }

    auto fields = definition.get(arangodb::StaticStrings::IndexFields);
    TRI_ASSERT(fields.isArray() && fields.length() == 1);
    auto direction = fields.at(0).copyString();
    TRI_ASSERT(direction == StaticStrings::FromString || direction == StaticStrings::ToString);

    index = std::make_shared<arangodb::RocksDBEdgeIndex>(id, collection, definition, direction);

    return arangodb::Result();
  }

  virtual arangodb::Result normalize( // normalize definition
      arangodb::velocypack::Builder& normalized, // normalized definition (out-param)
      arangodb::velocypack::Slice definition, // source definition
      bool isCreation, // definition for index creation
      TRI_vocbase_t const& vocbase // index vocbase
  ) const override {
    if (isCreation) {
      // creating these indexes yourself is forbidden
      return TRI_ERROR_FORBIDDEN;
    }

    TRI_ASSERT(normalized.isOpenObject());
    normalized.add(arangodb::StaticStrings::IndexType,
                   arangodb::velocypack::Value(arangodb::Index::oldtypeName(
                       arangodb::Index::TRI_IDX_TYPE_EDGE_INDEX)));

    return TRI_ERROR_INTERNAL;
  }
};

struct FulltextIndexFactory : public DefaultIndexFactory {
  FulltextIndexFactory()
      : DefaultIndexFactory(arangodb::Index::TRI_IDX_TYPE_FULLTEXT_INDEX) {}

  arangodb::Result instantiate(std::shared_ptr<arangodb::Index>& index,
                               arangodb::LogicalCollection& collection,
                               arangodb::velocypack::Slice const& definition,
                               TRI_idx_iid_t id, bool isClusterConstructor) const override {
    index = std::make_shared<arangodb::RocksDBFulltextIndex>(id, collection, definition);

    return arangodb::Result();
  }

  virtual arangodb::Result normalize( // normalize definition
      arangodb::velocypack::Builder& normalized, // normalized definition (out-param)
      arangodb::velocypack::Slice definition, // source definition
      bool isCreation, // definition for index creation
      TRI_vocbase_t const& vocbase // index vocbase
  ) const override {
    TRI_ASSERT(normalized.isOpenObject());
    normalized.add(arangodb::StaticStrings::IndexType,
                   arangodb::velocypack::Value(arangodb::Index::oldtypeName(
                       arangodb::Index::TRI_IDX_TYPE_FULLTEXT_INDEX)));

    if (isCreation && !ServerState::instance()->isCoordinator() &&
        !definition.hasKey("objectId")) {
      normalized.add("objectId",
                     arangodb::velocypack::Value(std::to_string(TRI_NewTickServer())));
    }

    return IndexFactory::enhanceJsonIndexFulltext(definition, normalized, isCreation);
  }
};

struct GeoIndexFactory : public DefaultIndexFactory {
  GeoIndexFactory()
      : DefaultIndexFactory(arangodb::Index::TRI_IDX_TYPE_GEO_INDEX) {}

  arangodb::Result instantiate(std::shared_ptr<arangodb::Index>& index,
                               arangodb::LogicalCollection& collection,
                               arangodb::velocypack::Slice const& definition,
                               TRI_idx_iid_t id, bool isClusterConstructor) const override {
    index = std::make_shared<arangodb::RocksDBGeoIndex>(id, collection,
                                                        definition, "geo");

    return arangodb::Result();
  }

  virtual arangodb::Result normalize( // normalize definition
      arangodb::velocypack::Builder& normalized, // normalized definition (out-param)
      arangodb::velocypack::Slice definition, // source definition
      bool isCreation, // definition for index creation
      TRI_vocbase_t const& vocbase // index vocbase
  ) const override {
    TRI_ASSERT(normalized.isOpenObject());
    normalized.add(arangodb::StaticStrings::IndexType,
                   arangodb::velocypack::Value(arangodb::Index::oldtypeName(
                       arangodb::Index::TRI_IDX_TYPE_GEO_INDEX)));

    if (isCreation && !ServerState::instance()->isCoordinator() &&
        !definition.hasKey("objectId")) {
      normalized.add("objectId", VPackValue(std::to_string(TRI_NewTickServer())));
    }

    return IndexFactory::enhanceJsonIndexGeo(definition, normalized, isCreation, 1, 2);
  }
};

struct Geo1IndexFactory : public DefaultIndexFactory {
  Geo1IndexFactory()
      : DefaultIndexFactory(arangodb::Index::TRI_IDX_TYPE_GEO_INDEX) {}

  arangodb::Result instantiate(std::shared_ptr<arangodb::Index>& index,
                               arangodb::LogicalCollection& collection,
                               arangodb::velocypack::Slice const& definition,
                               TRI_idx_iid_t id, bool isClusterConstructor) const override {
    index = std::make_shared<arangodb::RocksDBGeoIndex>(id, collection,
                                                        definition, "geo1");

    return arangodb::Result();
  }

  virtual arangodb::Result normalize( // normalize definition
      arangodb::velocypack::Builder& normalized, // normalized definition (out-param)
      arangodb::velocypack::Slice definition, // source definition
      bool isCreation, // definition for index creation
      TRI_vocbase_t const& vocbase // index vocbase
  ) const override {
    TRI_ASSERT(normalized.isOpenObject());
    normalized.add(arangodb::StaticStrings::IndexType,
                   arangodb::velocypack::Value(arangodb::Index::oldtypeName(
                       arangodb::Index::TRI_IDX_TYPE_GEO_INDEX)));

    if (isCreation && !ServerState::instance()->isCoordinator() &&
        !definition.hasKey("objectId")) {
      normalized.add("objectId",
                     arangodb::velocypack::Value(std::to_string(TRI_NewTickServer())));
    }

    return IndexFactory::enhanceJsonIndexGeo(definition, normalized, isCreation, 1, 1);
  }
};

struct Geo2IndexFactory : public DefaultIndexFactory {
  Geo2IndexFactory()
      : DefaultIndexFactory(arangodb::Index::TRI_IDX_TYPE_GEO_INDEX) {}

  arangodb::Result instantiate(std::shared_ptr<arangodb::Index>& index,
                               arangodb::LogicalCollection& collection,
                               arangodb::velocypack::Slice const& definition,
                               TRI_idx_iid_t id, bool isClusterConstructor) const override {
    index = std::make_shared<arangodb::RocksDBGeoIndex>(id, collection,
                                                        definition, "geo2");

    return arangodb::Result();
  }

  virtual arangodb::Result normalize( // normalize definition
      arangodb::velocypack::Builder& normalized, // normalized definition (out-param)
      arangodb::velocypack::Slice definition, // source definition
      bool isCreation, // definition for index creation
      TRI_vocbase_t const& vocbase // index vocbase
  ) const override {
    TRI_ASSERT(normalized.isOpenObject());
    normalized.add(arangodb::StaticStrings::IndexType,
                   arangodb::velocypack::Value(arangodb::Index::oldtypeName(
                       arangodb::Index::TRI_IDX_TYPE_GEO_INDEX)));

    if (isCreation && !ServerState::instance()->isCoordinator() &&
        !definition.hasKey("objectId")) {
      normalized.add("objectId",
                     arangodb::velocypack::Value(std::to_string(TRI_NewTickServer())));
    }

    return IndexFactory::enhanceJsonIndexGeo(definition, normalized, isCreation, 1, 2);
  }
};

template <typename F, arangodb::Index::IndexType type>
struct SecondaryIndexFactory : public DefaultIndexFactory {
  SecondaryIndexFactory() : DefaultIndexFactory(type) {}

  arangodb::Result instantiate(std::shared_ptr<arangodb::Index>& index,
                               arangodb::LogicalCollection& collection,
                               arangodb::velocypack::Slice const& definition,
                               TRI_idx_iid_t id, bool isClusterConstructor) const override {
    index = std::make_shared<F>(id, collection, definition);
    return arangodb::Result();
  }

  virtual arangodb::Result normalize( // normalize definition
      arangodb::velocypack::Builder& normalized, // normalized definition (out-param)
      arangodb::velocypack::Slice definition, // source definition
      bool isCreation, // definition for index creation
      TRI_vocbase_t const& vocbase // index vocbase
  ) const override {
    TRI_ASSERT(normalized.isOpenObject());
    normalized.add(arangodb::StaticStrings::IndexType,
                   arangodb::velocypack::Value(arangodb::Index::oldtypeName(type)));

    if (isCreation && !ServerState::instance()->isCoordinator() &&
        !definition.hasKey("objectId")) {
      normalized.add("objectId",
                     arangodb::velocypack::Value(std::to_string(TRI_NewTickServer())));
    }

    return IndexFactory::enhanceJsonIndexGeneric(definition, normalized, isCreation);
  }
};

struct TtlIndexFactory : public DefaultIndexFactory {
  explicit TtlIndexFactory(arangodb::Index::IndexType type)
      : DefaultIndexFactory(type) {}

  arangodb::Result instantiate(std::shared_ptr<arangodb::Index>& index,
                               arangodb::LogicalCollection& collection,
                               arangodb::velocypack::Slice const& definition,
                               TRI_idx_iid_t id, bool isClusterConstructor) const override {
    index = std::make_shared<RocksDBTtlIndex>(id, collection, definition);
    return arangodb::Result();
  }

  virtual arangodb::Result normalize( // normalize definition
      arangodb::velocypack::Builder& normalized, // normalized definition (out-param)
      arangodb::velocypack::Slice definition, // source definition
      bool isCreation, // definition for index creation
      TRI_vocbase_t const& vocbase // index vocbase
  ) const override {
    TRI_ASSERT(normalized.isOpenObject());
    normalized.add(arangodb::StaticStrings::IndexType,
                   arangodb::velocypack::Value(arangodb::Index::oldtypeName(_type)));

    if (isCreation && !ServerState::instance()->isCoordinator() &&
        !definition.hasKey("objectId")) {
      normalized.add("objectId",
                     arangodb::velocypack::Value(std::to_string(TRI_NewTickServer())));
    }

    return IndexFactory::enhanceJsonIndexTtl(definition, normalized, isCreation);
  }
};

struct PrimaryIndexFactory : public DefaultIndexFactory {
  PrimaryIndexFactory()
      : DefaultIndexFactory(arangodb::Index::TRI_IDX_TYPE_PRIMARY_INDEX) {}

  arangodb::Result instantiate(std::shared_ptr<arangodb::Index>& index,
                               arangodb::LogicalCollection& collection,
                               arangodb::velocypack::Slice const& definition,
                               TRI_idx_iid_t id, bool isClusterConstructor) const override {
    if (!isClusterConstructor) {
      // this indexes cannot be created directly
      return arangodb::Result(TRI_ERROR_INTERNAL,
                              "cannot create primary index");
    }

    index = std::make_shared<arangodb::RocksDBPrimaryIndex>(collection, definition);

    return arangodb::Result();
  }

  virtual arangodb::Result normalize( // normalize definition
      arangodb::velocypack::Builder& normalized, // normalized definition (out-param)
      arangodb::velocypack::Slice definition, // source definition
      bool isCreation, // definition for index creation
      TRI_vocbase_t const& vocbase // index vocbase
  ) const override {
    if (isCreation) {
      // creating these indexes yourself is forbidden
      return TRI_ERROR_FORBIDDEN;
    }

    TRI_ASSERT(normalized.isOpenObject());
    normalized.add(arangodb::StaticStrings::IndexType,
                   arangodb::velocypack::Value(arangodb::Index::oldtypeName(
                       arangodb::Index::TRI_IDX_TYPE_PRIMARY_INDEX)));

    return TRI_ERROR_INTERNAL;
  }
};

}  // namespace

RocksDBIndexFactory::RocksDBIndexFactory() {
  static const EdgeIndexFactory edgeIndexFactory;
  static const FulltextIndexFactory fulltextIndexFactory;
  static const GeoIndexFactory geoIndexFactory;
  static const Geo1IndexFactory geo1IndexFactory;
  static const Geo2IndexFactory geo2IndexFactory;
  static const SecondaryIndexFactory<arangodb::RocksDBHashIndex, arangodb::Index::TRI_IDX_TYPE_HASH_INDEX> hashIndexFactory;
  static const SecondaryIndexFactory<arangodb::RocksDBPersistentIndex, arangodb::Index::TRI_IDX_TYPE_PERSISTENT_INDEX> persistentIndexFactory;
  static const SecondaryIndexFactory<arangodb::RocksDBSkiplistIndex, arangodb::Index::TRI_IDX_TYPE_SKIPLIST_INDEX> skiplistIndexFactory;
  static const TtlIndexFactory ttlIndexFactory(arangodb::Index::TRI_IDX_TYPE_TTL_INDEX);
  static const PrimaryIndexFactory primaryIndexFactory;

  emplace("edge", edgeIndexFactory);
  emplace("fulltext", fulltextIndexFactory);
  emplace("geo", geoIndexFactory);
  emplace("geo1", geo1IndexFactory);
  emplace("geo2", geo2IndexFactory);
  emplace("hash", hashIndexFactory);
  emplace("persistent", persistentIndexFactory);
  emplace("primary", primaryIndexFactory);
  emplace("rocksdb", persistentIndexFactory);
  emplace("skiplist", skiplistIndexFactory);
  emplace("ttl", ttlIndexFactory);
}

/// @brief index name aliases (e.g. "persistent" => "hash", "skiplist" =>
/// "hash") used to display storage engine capabilities
std::unordered_map<std::string, std::string> RocksDBIndexFactory::indexAliases() const {
  return std::unordered_map<std::string, std::string>{
      {"hash", "persistent"},
      {"skiplist", "persistent"},
  };
}

void RocksDBIndexFactory::fillSystemIndexes(arangodb::LogicalCollection& col,
                                            std::vector<std::shared_ptr<arangodb::Index>>& indexes) const {
  VPackSlice def = VPackSlice::emptyObjectSlice();
  
  // create primary index
  indexes.emplace_back(std::make_shared<RocksDBPrimaryIndex>(col, def));

  // create edges indexes
  if (TRI_COL_TYPE_EDGE == col.type()) {
    indexes.emplace_back(
        std::make_shared<arangodb::RocksDBEdgeIndex>(1, col, def,
                                                     StaticStrings::FromString));
    indexes.emplace_back(
        std::make_shared<arangodb::RocksDBEdgeIndex>(2, col, def,
                                                     StaticStrings::ToString));
  }
}

/// @brief create indexes from a list of index definitions
void RocksDBIndexFactory::prepareIndexes(
    LogicalCollection& col, arangodb::velocypack::Slice const& indexesSlice,
    std::vector<std::shared_ptr<arangodb::Index>>& indexes) const {
  TRI_ASSERT(indexesSlice.isArray());

  bool splitEdgeIndex = false;
  TRI_idx_iid_t last = 0;

  for (VPackSlice v : VPackArrayIterator(indexesSlice)) {
    if (arangodb::basics::VelocyPackHelper::getBooleanValue(v, StaticStrings::Error, false)) {
      // We have an error here.
      // Do not add index.
      // TODO Handle Properly
      continue;
    }

    // check for combined edge index from MMFiles; must split!
    auto typeSlice = v.get(StaticStrings::IndexType);
    if (typeSlice.isString()) {
      VPackValueLength len;
      const char* tmp = typeSlice.getStringUnchecked(len);
      arangodb::Index::IndexType const type = arangodb::Index::type(tmp, len);

      if (type == Index::IndexType::TRI_IDX_TYPE_EDGE_INDEX) {
        VPackSlice fields = v.get(StaticStrings::IndexFields);

        if (fields.isArray() && fields.length() == 2) {
          VPackBuilder from;

          from.openObject();

          for (auto const& f : VPackObjectIterator(v)) {
            if (arangodb::velocypack::StringRef(f.key) == StaticStrings::IndexFields) {
              from.add(VPackValue(StaticStrings::IndexFields));
              from.openArray();
              from.add(VPackValue(StaticStrings::FromString));
              from.close();
            } else {
              from.add(f.key);
              from.add(f.value);
            }
          }

          from.close();

          VPackBuilder to;

          to.openObject();
          for (auto const& f : VPackObjectIterator(v)) {
            if (arangodb::velocypack::StringRef(f.key) == StaticStrings::IndexFields) {
              to.add(VPackValue(StaticStrings::IndexFields));
              to.openArray();
              to.add(VPackValue(StaticStrings::ToString));
              to.close();
            } else if (arangodb::velocypack::StringRef(f.key) == StaticStrings::IndexId) {
              auto iid = basics::StringUtils::uint64(f.value.copyString()) + 1;
              last = iid;
              to.add(StaticStrings::IndexId, VPackValue(std::to_string(iid)));
            } else {
              to.add(f.key);
              to.add(f.value);
            }
          }

          to.close();

          auto idxFrom = prepareIndexFromSlice(from.slice(), false, col, true);

          if (!idxFrom) {
            LOG_TOPIC("37c1b", ERR, arangodb::Logger::ENGINES)
                << "error creating index from definition '"
                << from.slice().toString() << "'";

            continue;
          }

          auto idxTo = prepareIndexFromSlice(to.slice(), false, col, true);

          if (!idxTo) {
            LOG_TOPIC("84c11", ERR, arangodb::Logger::ENGINES)
                << "error creating index from definition '"
                << to.slice().toString() << "'";

            continue;
          }

          indexes.emplace_back(std::move(idxFrom));
          indexes.emplace_back(std::move(idxTo));
          splitEdgeIndex = true;

          continue;
        }
      } else if (splitEdgeIndex) {
        VPackBuilder b;

        b.openObject();

        for (auto const& f : VPackObjectIterator(v)) {
          if (arangodb::velocypack::StringRef(f.key) == StaticStrings::IndexId) {
            last++;
            b.add(StaticStrings::IndexId, VPackValue(std::to_string(last)));
          } else {
            b.add(f.key);
            b.add(f.value);
          }
        }

        b.close();

        auto idx = prepareIndexFromSlice(b.slice(), false, col, true);

        if (!idx) {
          LOG_TOPIC("aa39e", ERR, arangodb::Logger::ENGINES)
              << "error creating index from definition '" << b.slice().toString() << "'";

          continue;
        }

        indexes.emplace_back(std::move(idx));

        continue;
      }
    }

    auto idx = prepareIndexFromSlice(v, false, col, true);
    if (!idx) {
      LOG_TOPIC("ddafd", ERR, arangodb::Logger::ENGINES)
          << "error creating index from definition '" << v.toString() << "'";

      continue;
    }
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    else {
      LOG_TOPIC("c455a", DEBUG, arangodb::Logger::ENGINES)
          << "created index '" << idx->id() << "' from definition '"
          << v.toJson() << "'";
    }
#endif

    if (basics::VelocyPackHelper::getBooleanValue(v, "_inprogress", false)) {
      LOG_TOPIC("66770", WARN, Logger::ENGINES) << "dropping failed index '" << idx->id() << "'";
      idx->drop();
      continue;
    }

    indexes.emplace_back(std::move(idx));
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
