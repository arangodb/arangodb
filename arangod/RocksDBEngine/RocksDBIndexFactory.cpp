////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;

namespace {

struct DefaultIndexFactory : public arangodb::IndexTypeFactory {
  arangodb::Index::IndexType const _type;

  explicit DefaultIndexFactory(arangodb::application_features::ApplicationServer& server,
                               arangodb::Index::IndexType type)
      : IndexTypeFactory(server), _type(type) {}

  bool equal(arangodb::velocypack::Slice const& lhs,
             arangodb::velocypack::Slice const& rhs,
             std::string const&) const override {
    return arangodb::IndexTypeFactory::equal(_type, lhs, rhs, true);
  }
};

struct EdgeIndexFactory : public DefaultIndexFactory {
  EdgeIndexFactory(arangodb::application_features::ApplicationServer& server)
      : DefaultIndexFactory(server, arangodb::Index::TRI_IDX_TYPE_EDGE_INDEX) {}

  std::shared_ptr<arangodb::Index> instantiate(arangodb::LogicalCollection& collection,
                                               arangodb::velocypack::Slice const& definition,
                                               IndexId id,
                                               bool isClusterConstructor) const override {
    if (!isClusterConstructor) {
      // this index type cannot be created directly
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "cannot create edge index");
    }

    auto fields = definition.get(arangodb::StaticStrings::IndexFields);
    TRI_ASSERT(fields.isArray() && fields.length() == 1);
    auto direction = fields.at(0).copyString();
    TRI_ASSERT(direction == StaticStrings::FromString || direction == StaticStrings::ToString);

    return std::make_shared<arangodb::RocksDBEdgeIndex>(id, collection, definition, direction);
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
  FulltextIndexFactory(arangodb::application_features::ApplicationServer& server)
      : DefaultIndexFactory(server, arangodb::Index::TRI_IDX_TYPE_FULLTEXT_INDEX) {}

  std::shared_ptr<arangodb::Index> instantiate(arangodb::LogicalCollection& collection,
                                               arangodb::velocypack::Slice const& definition,
                                               IndexId id,
                                               bool isClusterConstructor) const override {
    return std::make_shared<arangodb::RocksDBFulltextIndex>(id, collection, definition);
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
  GeoIndexFactory(arangodb::application_features::ApplicationServer& server)
      : DefaultIndexFactory(server, arangodb::Index::TRI_IDX_TYPE_GEO_INDEX) {}

  std::shared_ptr<arangodb::Index> instantiate(arangodb::LogicalCollection& collection,
                                               arangodb::velocypack::Slice const& definition,
                                               IndexId id,
                                               bool isClusterConstructor) const override {
    return std::make_shared<arangodb::RocksDBGeoIndex>(id, collection, definition, "geo");
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
  Geo1IndexFactory(arangodb::application_features::ApplicationServer& server)
      : DefaultIndexFactory(server, arangodb::Index::TRI_IDX_TYPE_GEO_INDEX) {}

  std::shared_ptr<arangodb::Index> instantiate(arangodb::LogicalCollection& collection,
                                               arangodb::velocypack::Slice const& definition,
                                               IndexId id,
                                               bool isClusterConstructor) const override {
    return std::make_shared<arangodb::RocksDBGeoIndex>(id, collection, definition, "geo1");
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
  Geo2IndexFactory(arangodb::application_features::ApplicationServer& server)
      : DefaultIndexFactory(server, arangodb::Index::TRI_IDX_TYPE_GEO_INDEX) {}

  std::shared_ptr<arangodb::Index> instantiate(arangodb::LogicalCollection& collection,
                                               arangodb::velocypack::Slice const& definition,
                                               IndexId id,
                                               bool isClusterConstructor) const override {
    return std::make_shared<arangodb::RocksDBGeoIndex>(id, collection, definition, "geo2");
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
  SecondaryIndexFactory(arangodb::application_features::ApplicationServer& server)
      : DefaultIndexFactory(server, type) {}

  std::shared_ptr<arangodb::Index> instantiate(arangodb::LogicalCollection& collection,
                                               arangodb::velocypack::Slice const& definition,
                                               IndexId id,
                                               bool isClusterConstructor) const override {
    return std::make_shared<F>(id, collection, definition);
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
    if (isCreation) { 
      bool est = basics::VelocyPackHelper::getBooleanValue(definition, StaticStrings::IndexEstimates, true);
      normalized.add(StaticStrings::IndexEstimates, arangodb::velocypack::Value(est));
    }

    return IndexFactory::enhanceJsonIndexGeneric(definition, normalized, isCreation);
  }
};

struct TtlIndexFactory : public DefaultIndexFactory {
  explicit TtlIndexFactory(arangodb::application_features::ApplicationServer& server,
                           arangodb::Index::IndexType type)
      : DefaultIndexFactory(server, type) {}

  std::shared_ptr<arangodb::Index> instantiate(arangodb::LogicalCollection& collection,
                                               arangodb::velocypack::Slice const& definition,
                                               IndexId id,
                                               bool isClusterConstructor) const override {
    return std::make_shared<RocksDBTtlIndex>(id, collection, definition);
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
    // a TTL index never uses index estimates
    normalized.add(StaticStrings::IndexEstimates, arangodb::velocypack::Value(false));

    return IndexFactory::enhanceJsonIndexTtl(definition, normalized, isCreation);
  }
};

struct PrimaryIndexFactory : public DefaultIndexFactory {
  PrimaryIndexFactory(arangodb::application_features::ApplicationServer& server)
      : DefaultIndexFactory(server, arangodb::Index::TRI_IDX_TYPE_PRIMARY_INDEX) {}

  std::shared_ptr<arangodb::Index> instantiate(arangodb::LogicalCollection& collection,
                                               arangodb::velocypack::Slice const& definition,
                                               IndexId id,
                                               bool isClusterConstructor) const override {
    if (!isClusterConstructor) {
      // this index type cannot be created directly
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "cannot create primary index");
    }

    return std::make_shared<arangodb::RocksDBPrimaryIndex>(collection, definition);
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

RocksDBIndexFactory::RocksDBIndexFactory(application_features::ApplicationServer& server)
    : IndexFactory(server) {
  static const EdgeIndexFactory edgeIndexFactory(server);
  static const FulltextIndexFactory fulltextIndexFactory(server);
  static const GeoIndexFactory geoIndexFactory(server);
  static const Geo1IndexFactory geo1IndexFactory(server);
  static const Geo2IndexFactory geo2IndexFactory(server);
  static const SecondaryIndexFactory<arangodb::RocksDBHashIndex, arangodb::Index::TRI_IDX_TYPE_HASH_INDEX> hashIndexFactory(
      server);
  static const SecondaryIndexFactory<arangodb::RocksDBPersistentIndex, arangodb::Index::TRI_IDX_TYPE_PERSISTENT_INDEX> persistentIndexFactory(
      server);
  static const SecondaryIndexFactory<arangodb::RocksDBSkiplistIndex, arangodb::Index::TRI_IDX_TYPE_SKIPLIST_INDEX> skiplistIndexFactory(
      server);
  static const TtlIndexFactory ttlIndexFactory(server, arangodb::Index::TRI_IDX_TYPE_TTL_INDEX);
  static const PrimaryIndexFactory primaryIndexFactory(server);

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
        std::make_shared<arangodb::RocksDBEdgeIndex>(IndexId::edgeFrom(), col, def,
                                                     StaticStrings::FromString));
    indexes.emplace_back(
        std::make_shared<arangodb::RocksDBEdgeIndex>(IndexId::edgeTo(), col, def,
                                                     StaticStrings::ToString));
  }
}

/// @brief create indexes from a list of index definitions
void RocksDBIndexFactory::prepareIndexes(
    LogicalCollection& col, arangodb::velocypack::Slice const& indexesSlice,
    std::vector<std::shared_ptr<arangodb::Index>>& indexes) const {
  TRI_ASSERT(indexesSlice.isArray());

  bool splitEdgeIndex = false;
  IndexId last = IndexId::primary();

  for (VPackSlice v : VPackArrayIterator(indexesSlice)) {
    if (!validateFieldsDefinition(v, 0, SIZE_MAX).ok()) {
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

          for (auto f : VPackObjectIterator(v)) {
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
          for (auto f : VPackObjectIterator(v)) {
            if (arangodb::velocypack::StringRef(f.key) == StaticStrings::IndexFields) {
              to.add(VPackValue(StaticStrings::IndexFields));
              to.openArray();
              to.add(VPackValue(StaticStrings::ToString));
              to.close();
            } else if (arangodb::velocypack::StringRef(f.key) == StaticStrings::IndexId) {
              IndexId iid{basics::StringUtils::uint64(f.value.copyString()) + 1};
              last = iid;
              to.add(StaticStrings::IndexId, VPackValue(std::to_string(iid.id())));
            } else {
              to.add(f.key);
              to.add(f.value);
            }
          }

          to.close();

          auto idxFrom = prepareIndexFromSlice(from.slice(), false, col, true);
          auto idxTo = prepareIndexFromSlice(to.slice(), false, col, true);

          TRI_ASSERT(idxFrom != nullptr);
          TRI_ASSERT(idxTo != nullptr);

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
            last = IndexId{last.id() + 1};
            b.add(StaticStrings::IndexId, VPackValue(std::to_string(last.id())));
          } else {
            b.add(f.key);
            b.add(f.value);
          }
        }

        b.close();

        auto idx = prepareIndexFromSlice(b.slice(), false, col, true);
        TRI_ASSERT(idx != nullptr);
        indexes.emplace_back(std::move(idx));
        continue;
      }
    }

    try {
      auto idx = prepareIndexFromSlice(v, false, col, true);
      TRI_ASSERT(idx != nullptr);
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
      LOG_TOPIC("c455a", DEBUG, arangodb::Logger::ENGINES)
          << "created index '" << idx->id().id() << "' from definition '"
          << v.toJson() << "'";
#endif

      if (basics::VelocyPackHelper::getBooleanValue(v, "_inprogress", false)) {
        LOG_TOPIC("66770", WARN, Logger::ENGINES)
            << "dropping failed index '" << idx->id().id() << "'";
        idx->drop();
        continue;
      }

      indexes.emplace_back(std::move(idx));
    } catch (std::exception const& ex) {
      LOG_TOPIC("2885b", ERR, arangodb::Logger::ENGINES)
          << "error creating index from definition '" << v.toString() << "': " << ex.what();
    }
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
