////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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
#include "IResearch/IResearchRocksDBInvertedIndex.h"
#include "Logger/LogMacros.h"
#include "RocksDBEngine/RocksDBEdgeIndex.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "RocksDBEngine/RocksDBFulltextIndex.h"
#include "RocksDBEngine/RocksDBGeoIndex.h"
#include "RocksDBEngine/RocksDBHashIndex.h"
#include "RocksDBEngine/RocksDBMultiDimIndex.h"
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

using namespace arangodb;

namespace {

struct DefaultIndexFactory : public IndexTypeFactory {
  Index::IndexType const _type;

  explicit DefaultIndexFactory(ArangodServer& server, Index::IndexType type)
      : IndexTypeFactory(server), _type(type) {}

  bool equal(velocypack::Slice lhs, velocypack::Slice rhs,
             std::string const&) const override {
    return IndexTypeFactory::equal(_type, lhs, rhs, true);
  }
};

struct EdgeIndexFactory : public DefaultIndexFactory {
  explicit EdgeIndexFactory(ArangodServer& server)
      : DefaultIndexFactory(server, Index::TRI_IDX_TYPE_EDGE_INDEX) {}

  std::shared_ptr<Index> instantiate(LogicalCollection& collection,
                                     velocypack::Slice definition, IndexId id,
                                     bool isClusterConstructor) const override {
    if (!isClusterConstructor) {
      // this index type cannot be created directly
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                     "cannot create edge index");
    }

    auto fields = definition.get(StaticStrings::IndexFields);
    TRI_ASSERT(fields.isArray() && fields.length() == 1);
    auto direction = fields.at(0).copyString();
    TRI_ASSERT(direction == StaticStrings::FromString ||
               direction == StaticStrings::ToString);

    return std::make_shared<RocksDBEdgeIndex>(id, collection, definition,
                                              direction);
  }

  virtual Result normalize(velocypack::Builder& normalized,
                           velocypack::Slice /*definition*/, bool isCreation,
                           TRI_vocbase_t const& /*vocbase*/) const override {
    if (isCreation) {
      // creating these indexes yourself is forbidden
      return TRI_ERROR_FORBIDDEN;
    }

    TRI_ASSERT(normalized.isOpenObject());
    normalized.add(
        StaticStrings::IndexType,
        velocypack::Value(Index::oldtypeName(Index::TRI_IDX_TYPE_EDGE_INDEX)));

    return TRI_ERROR_INTERNAL;
  }
};

struct FulltextIndexFactory : public DefaultIndexFactory {
  explicit FulltextIndexFactory(ArangodServer& server)
      : DefaultIndexFactory(server, Index::TRI_IDX_TYPE_FULLTEXT_INDEX) {}

  std::shared_ptr<Index> instantiate(
      LogicalCollection& collection, velocypack::Slice definition, IndexId id,
      bool /*isClusterConstructor*/) const override {
    return std::make_shared<RocksDBFulltextIndex>(id, collection, definition);
  }

  virtual Result normalize(velocypack::Builder& normalized,
                           velocypack::Slice definition, bool isCreation,
                           TRI_vocbase_t const& /*vocbase*/) const override {
    TRI_ASSERT(normalized.isOpenObject());
    normalized.add(StaticStrings::IndexType,
                   velocypack::Value(
                       Index::oldtypeName(Index::TRI_IDX_TYPE_FULLTEXT_INDEX)));

    if (isCreation && !ServerState::instance()->isCoordinator() &&
        !definition.hasKey(StaticStrings::ObjectId)) {
      normalized.add(StaticStrings::ObjectId,
                     velocypack::Value(std::to_string(TRI_NewTickServer())));
    }

    return IndexFactory::enhanceJsonIndexFulltext(definition, normalized,
                                                  isCreation);
  }
};

struct GeoIndexFactory : public DefaultIndexFactory {
  explicit GeoIndexFactory(ArangodServer& server)
      : DefaultIndexFactory(server, Index::TRI_IDX_TYPE_GEO_INDEX) {}

  std::shared_ptr<Index> instantiate(
      LogicalCollection& collection, velocypack::Slice definition, IndexId id,
      bool /*isClusterConstructor*/) const override {
    return std::make_shared<RocksDBGeoIndex>(id, collection, definition, "geo");
  }

  virtual Result normalize(velocypack::Builder& normalized,
                           velocypack::Slice definition, bool isCreation,
                           TRI_vocbase_t const& /*vocbase*/) const override {
    TRI_ASSERT(normalized.isOpenObject());
    normalized.add(
        StaticStrings::IndexType,
        velocypack::Value(Index::oldtypeName(Index::TRI_IDX_TYPE_GEO_INDEX)));

    if (isCreation && !ServerState::instance()->isCoordinator() &&
        !definition.hasKey(StaticStrings::ObjectId)) {
      normalized.add(StaticStrings::ObjectId,
                     VPackValue(std::to_string(TRI_NewTickServer())));
    }

    return IndexFactory::enhanceJsonIndexGeo(definition, normalized, isCreation,
                                             1, 2);
  }
};

struct Geo1IndexFactory : public DefaultIndexFactory {
  explicit Geo1IndexFactory(ArangodServer& server)
      : DefaultIndexFactory(server, Index::TRI_IDX_TYPE_GEO_INDEX) {}

  std::shared_ptr<Index> instantiate(
      LogicalCollection& collection, velocypack::Slice definition, IndexId id,
      bool /*isClusterConstructor*/) const override {
    return std::make_shared<RocksDBGeoIndex>(id, collection, definition,
                                             "geo1");
  }

  virtual Result normalize(velocypack::Builder& normalized,
                           velocypack::Slice definition, bool isCreation,
                           TRI_vocbase_t const& /*vocbase*/) const override {
    TRI_ASSERT(normalized.isOpenObject());
    normalized.add(
        StaticStrings::IndexType,
        velocypack::Value(Index::oldtypeName(Index::TRI_IDX_TYPE_GEO_INDEX)));

    if (isCreation && !ServerState::instance()->isCoordinator() &&
        !definition.hasKey(StaticStrings::ObjectId)) {
      normalized.add(StaticStrings::ObjectId,
                     velocypack::Value(std::to_string(TRI_NewTickServer())));
    }

    return IndexFactory::enhanceJsonIndexGeo(definition, normalized, isCreation,
                                             1, 1);
  }
};

struct Geo2IndexFactory : public DefaultIndexFactory {
  explicit Geo2IndexFactory(ArangodServer& server)
      : DefaultIndexFactory(server, Index::TRI_IDX_TYPE_GEO_INDEX) {}

  std::shared_ptr<Index> instantiate(
      LogicalCollection& collection, velocypack::Slice definition, IndexId id,
      bool /*isClusterConstructor*/) const override {
    return std::make_shared<RocksDBGeoIndex>(id, collection, definition,
                                             "geo2");
  }

  virtual Result normalize(velocypack::Builder& normalized,
                           velocypack::Slice definition, bool isCreation,
                           TRI_vocbase_t const& /*vocbase*/) const override {
    TRI_ASSERT(normalized.isOpenObject());
    normalized.add(
        StaticStrings::IndexType,
        velocypack::Value(Index::oldtypeName(Index::TRI_IDX_TYPE_GEO_INDEX)));

    if (isCreation && !ServerState::instance()->isCoordinator() &&
        !definition.hasKey(StaticStrings::ObjectId)) {
      normalized.add(StaticStrings::ObjectId,
                     velocypack::Value(std::to_string(TRI_NewTickServer())));
    }

    return IndexFactory::enhanceJsonIndexGeo(definition, normalized, isCreation,
                                             1, 2);
  }
};

template<typename F, Index::IndexType type>
struct SecondaryIndexFactory : public DefaultIndexFactory {
  explicit SecondaryIndexFactory(ArangodServer& server)
      : DefaultIndexFactory(server, type) {}

  std::shared_ptr<Index> instantiate(
      LogicalCollection& collection, velocypack::Slice definition, IndexId id,
      bool /*isClusterConstructor*/) const override {
    return std::make_shared<F>(id, collection, definition);
  }

  virtual Result normalize(velocypack::Builder& normalized,
                           velocypack::Slice definition, bool isCreation,
                           TRI_vocbase_t const& /*vocbase*/) const override {
    TRI_ASSERT(normalized.isOpenObject());
    normalized.add(StaticStrings::IndexType,
                   velocypack::Value(Index::oldtypeName(type)));

    if (isCreation && !ServerState::instance()->isCoordinator() &&
        !definition.hasKey(StaticStrings::ObjectId)) {
      normalized.add(StaticStrings::ObjectId,
                     velocypack::Value(std::to_string(TRI_NewTickServer())));
    }
    if (isCreation) {
      bool est = basics::VelocyPackHelper::getBooleanValue(
          definition, StaticStrings::IndexEstimates, true);
      normalized.add(StaticStrings::IndexEstimates, velocypack::Value(est));
    }

    return IndexFactory::enhanceJsonIndexGeneric(definition, normalized,
                                                 isCreation);
  }
};

struct MdiIndexFactory : public DefaultIndexFactory {
  explicit MdiIndexFactory(ArangodServer& server, Index::IndexType type)
      : DefaultIndexFactory(server, type) {}

  std::shared_ptr<arangodb::Index> instantiate(
      arangodb::LogicalCollection& collection,
      arangodb::velocypack::Slice definition, IndexId id,
      bool /*isClusterConstructor*/) const override {
    if (auto isUnique = definition.get(StaticStrings::IndexUnique).isTrue();
        isUnique) {
      return std::make_shared<RocksDBUniqueMdiIndex>(id, collection,
                                                     definition);
    }

    return std::make_shared<RocksDBMdiIndex>(id, collection, definition);
  }

  virtual arangodb::Result normalize(
      velocypack::Builder& normalized, velocypack::Slice definition,
      bool isCreation, TRI_vocbase_t const& /*vocbase*/) const override {
    TRI_ASSERT(normalized.isOpenObject());
    normalized.add(StaticStrings::IndexType,
                   velocypack::Value(Index::oldtypeName(_type)));

    if (isCreation && !ServerState::instance()->isCoordinator() &&
        !definition.hasKey(StaticStrings::ObjectId)) {
      normalized.add(
          StaticStrings::ObjectId,
          arangodb::velocypack::Value(std::to_string(TRI_NewTickServer())));
    }

    if (definition.hasKey(StaticStrings::IndexPrefixFields)) {
      return Result(TRI_ERROR_BAD_PARAMETER,
                    "`mdi` index does not support prefixed fields. use "
                    "`mdi-prefixed` as type instead.");
    }
    // a mdi never uses index estimates
    normalized.add(StaticStrings::IndexEstimates, velocypack::Value(false));

    return IndexFactory::enhanceJsonIndexMdi(definition, normalized,
                                             isCreation);
  }
};

struct MdiPrefixedIndexFactory : public DefaultIndexFactory {
  explicit MdiPrefixedIndexFactory(ArangodServer& server)
      : DefaultIndexFactory(server, Index::TRI_IDX_TYPE_MDI_PREFIXED_INDEX) {}

  std::shared_ptr<arangodb::Index> instantiate(
      arangodb::LogicalCollection& collection,
      arangodb::velocypack::Slice definition, IndexId id,
      bool /*isClusterConstructor*/) const override {
    if (auto isUnique = definition.get(StaticStrings::IndexUnique).isTrue();
        isUnique) {
      return std::make_shared<RocksDBUniqueMdiIndex>(id, collection,
                                                     definition);
    }

    return std::make_shared<RocksDBMdiIndex>(id, collection, definition);
  }

  virtual arangodb::Result normalize(
      velocypack::Builder& normalized, velocypack::Slice definition,
      bool isCreation, TRI_vocbase_t const& /*vocbase*/) const override {
    TRI_ASSERT(normalized.isOpenObject());
    normalized.add(arangodb::StaticStrings::IndexType,
                   arangodb::velocypack::Value(arangodb::Index::oldtypeName(
                       Index::TRI_IDX_TYPE_MDI_PREFIXED_INDEX)));

    if (isCreation && !ServerState::instance()->isCoordinator() &&
        !definition.hasKey(StaticStrings::ObjectId)) {
      normalized.add(
          StaticStrings::ObjectId,
          arangodb::velocypack::Value(std::to_string(TRI_NewTickServer())));
    }
    if (isCreation) {
      bool est = basics::VelocyPackHelper::getBooleanValue(
          definition, StaticStrings::IndexEstimates, true);
      normalized.add(StaticStrings::IndexEstimates, velocypack::Value(est));
    }

    return IndexFactory::enhanceJsonIndexMdiPrefixed(definition, normalized,
                                                     isCreation);
  }
};

struct TtlIndexFactory : public DefaultIndexFactory {
  TtlIndexFactory(ArangodServer& server, Index::IndexType type)
      : DefaultIndexFactory(server, type) {}

  std::shared_ptr<Index> instantiate(
      LogicalCollection& collection, velocypack::Slice definition, IndexId id,
      bool /*isClusterConstructor*/) const override {
    return std::make_shared<RocksDBTtlIndex>(id, collection, definition);
  }

  virtual Result normalize(velocypack::Builder& normalized,
                           velocypack::Slice definition, bool isCreation,
                           TRI_vocbase_t const& /*vocbase*/) const override {
    TRI_ASSERT(normalized.isOpenObject());
    normalized.add(StaticStrings::IndexType,
                   velocypack::Value(Index::oldtypeName(_type)));

    if (isCreation && !ServerState::instance()->isCoordinator() &&
        !definition.hasKey(StaticStrings::ObjectId)) {
      normalized.add(StaticStrings::ObjectId,
                     velocypack::Value(std::to_string(TRI_NewTickServer())));
    }
    // a TTL index never uses index estimates
    normalized.add(StaticStrings::IndexEstimates, velocypack::Value(false));

    return IndexFactory::enhanceJsonIndexTtl(definition, normalized,
                                             isCreation);
  }
};

struct PrimaryIndexFactory : public DefaultIndexFactory {
  explicit PrimaryIndexFactory(ArangodServer& server)
      : DefaultIndexFactory(server, Index::TRI_IDX_TYPE_PRIMARY_INDEX) {}

  std::shared_ptr<Index> instantiate(LogicalCollection& collection,
                                     velocypack::Slice definition,
                                     IndexId /*id*/,
                                     bool isClusterConstructor) const override {
    if (!isClusterConstructor) {
      // this index type cannot be created directly
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                     "cannot create primary index");
    }

    return std::make_shared<RocksDBPrimaryIndex>(collection, definition);
  }

  virtual Result normalize(velocypack::Builder& normalized,
                           velocypack::Slice /*definition*/, bool isCreation,
                           TRI_vocbase_t const& /*vocbase*/) const override {
    if (isCreation) {
      // creating these indexes yourself is forbidden
      return TRI_ERROR_FORBIDDEN;
    }

    TRI_ASSERT(normalized.isOpenObject());
    normalized.add(StaticStrings::IndexType,
                   velocypack::Value(
                       Index::oldtypeName(Index::TRI_IDX_TYPE_PRIMARY_INDEX)));

    return TRI_ERROR_INTERNAL;
  }
};

}  // namespace

RocksDBIndexFactory::RocksDBIndexFactory(ArangodServer& server)
    : IndexFactory(server) {
  static const EdgeIndexFactory edgeIndexFactory(server);
  static const FulltextIndexFactory fulltextIndexFactory(server);
  static const GeoIndexFactory geoIndexFactory(server);
  static const Geo1IndexFactory geo1IndexFactory(server);
  static const Geo2IndexFactory geo2IndexFactory(server);
  static const SecondaryIndexFactory<RocksDBHashIndex,
                                     Index::TRI_IDX_TYPE_HASH_INDEX>
      hashIndexFactory(server);
  static const SecondaryIndexFactory<RocksDBPersistentIndex,
                                     Index::TRI_IDX_TYPE_PERSISTENT_INDEX>
      persistentIndexFactory(server);
  static const SecondaryIndexFactory<RocksDBSkiplistIndex,
                                     Index::TRI_IDX_TYPE_SKIPLIST_INDEX>
      skiplistIndexFactory(server);
  static const TtlIndexFactory ttlIndexFactory(server,
                                               Index::TRI_IDX_TYPE_TTL_INDEX);
  static const PrimaryIndexFactory primaryIndexFactory(server);
  static const MdiIndexFactory zkdIndexFactory(server,
                                               Index::TRI_IDX_TYPE_ZKD_INDEX);
  static const MdiIndexFactory mdiIndexFactory(server,
                                               Index::TRI_IDX_TYPE_MDI_INDEX);
  static const iresearch::IResearchRocksDBInvertedIndexFactory
      iresearchInvertedIndexFactory(server);
  static const MdiPrefixedIndexFactory mdiPrefixedIndexFactory(server);

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
  emplace("zkd", zkdIndexFactory);
  emplace("mdi", mdiIndexFactory);
  emplace("mdi-prefixed", mdiPrefixedIndexFactory);
  emplace(arangodb::iresearch::IRESEARCH_INVERTED_INDEX_TYPE.data(),
          iresearchInvertedIndexFactory);
}

/// @brief index name aliases (e.g. "persistent" => "hash", "skiplist" =>
/// "hash") used to display storage engine capabilities
std::vector<std::pair<std::string_view, std::string_view>>
RocksDBIndexFactory::indexAliases() const {
  return {
      {"hash", "persistent"},
      {"skiplist", "persistent"},
      {"zkd", "mdi"},
  };
}

void RocksDBIndexFactory::fillSystemIndexes(
    LogicalCollection& col,
    std::vector<std::shared_ptr<Index>>& indexes) const {
  VPackSlice def = VPackSlice::emptyObjectSlice();

  // create primary index
  indexes.emplace_back(std::make_shared<RocksDBPrimaryIndex>(col, def));

  // create edges indexes
  if (TRI_COL_TYPE_EDGE == col.type()) {
    indexes.emplace_back(std::make_shared<RocksDBEdgeIndex>(
        IndexId::edgeFrom(), col, def, StaticStrings::FromString));
    indexes.emplace_back(std::make_shared<RocksDBEdgeIndex>(
        IndexId::edgeTo(), col, def, StaticStrings::ToString));
  }
}

/// @brief create indexes from a list of index definitions
void RocksDBIndexFactory::prepareIndexes(
    LogicalCollection& col, velocypack::Slice indexesSlice,
    std::vector<std::shared_ptr<Index>>& indexes) const {
  TRI_ASSERT(indexesSlice.isArray());

  bool splitEdgeIndex = false;
  IndexId last = IndexId::primary();

  for (VPackSlice v : VPackArrayIterator(indexesSlice)) {
    if (!validateFieldsDefinition(v, StaticStrings::IndexFields, 0, SIZE_MAX,
                                  /*allowSubAttributes*/ true,
                                  /*allowIdAttribute*/ false)
             .ok()) {
      continue;
    }

    // check for combined edge index from MMFiles; must split!
    auto typeSlice = v.get(StaticStrings::IndexType);
    if (typeSlice.isString()) {
      Index::IndexType const type = Index::type(typeSlice.stringView());

      if (type == Index::IndexType::TRI_IDX_TYPE_EDGE_INDEX) {
        VPackSlice fields = v.get(StaticStrings::IndexFields);

        if (fields.isArray() && fields.length() == 2) {
          VPackBuilder from;

          from.openObject();

          for (auto f : VPackObjectIterator(v)) {
            if (f.key.stringView() == StaticStrings::IndexFields) {
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
            if (f.key.stringView() == StaticStrings::IndexFields) {
              to.add(VPackValue(StaticStrings::IndexFields));
              to.openArray();
              to.add(VPackValue(StaticStrings::ToString));
              to.close();
            } else if (f.key.stringView() == StaticStrings::IndexId) {
              IndexId iid{basics::StringUtils::uint64(f.value.copyString()) +
                          1};
              last = iid;
              to.add(StaticStrings::IndexId,
                     VPackValue(std::to_string(iid.id())));
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
          if (f.key.stringView() == StaticStrings::IndexId) {
            last = IndexId{last.id() + 1};
            b.add(StaticStrings::IndexId,
                  VPackValue(std::to_string(last.id())));
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
      LOG_TOPIC("c455a", DEBUG, Logger::ENGINES)
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
      LOG_TOPIC("2885b", ERR, Logger::ENGINES)
          << "error creating index from definition '" << v.toString()
          << "': " << ex.what();
    }
  }
}
