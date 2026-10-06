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
////////////////////////////////////////////////////////////////////////////////

#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/voc-errors.h"
#include "Cluster/ServerState.h"
#include "Indexes/IndexDefinitions.h"
#include "Indexes/IndexTypeCatalog.h"
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
#include "RocksDBEngine/RocksDBVectorIndex.h"
#include "VectorIndex/IVectorIndexProvider.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ticks.h"
#include "VocBase/voc-types.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

using namespace arangodb;

namespace {

struct EdgeIndexFactory : public DelegatingIndexFactory<EdgeIndexDefinition> {
  using DelegatingIndexFactory::DelegatingIndexFactory;

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
};

struct FulltextIndexFactory
    : public DelegatingIndexFactory<FulltextIndexDefinition> {
  using DelegatingIndexFactory::DelegatingIndexFactory;

  std::shared_ptr<Index> instantiate(
      LogicalCollection& collection, velocypack::Slice definition, IndexId id,
      bool /*isClusterConstructor*/) const override {
    return std::make_shared<RocksDBFulltextIndex>(id, collection, definition);
  }
};

struct GeoIndexFactory : public DelegatingIndexFactory<GeoIndexDefinition> {
  using DelegatingIndexFactory::DelegatingIndexFactory;

  std::shared_ptr<Index> instantiate(
      LogicalCollection& collection, velocypack::Slice definition, IndexId id,
      bool /*isClusterConstructor*/) const override {
    return std::make_shared<RocksDBGeoIndex>(id, collection, definition, "geo");
  }
};

struct Geo1IndexFactory : public DelegatingIndexFactory<Geo1IndexDefinition> {
  using DelegatingIndexFactory::DelegatingIndexFactory;

  std::shared_ptr<Index> instantiate(
      LogicalCollection& collection, velocypack::Slice definition, IndexId id,
      bool /*isClusterConstructor*/) const override {
    return std::make_shared<RocksDBGeoIndex>(id, collection, definition,
                                             "geo1");
  }
};

struct Geo2IndexFactory : public DelegatingIndexFactory<Geo2IndexDefinition> {
  using DelegatingIndexFactory::DelegatingIndexFactory;

  std::shared_ptr<Index> instantiate(
      LogicalCollection& collection, velocypack::Slice definition, IndexId id,
      bool /*isClusterConstructor*/) const override {
    return std::make_shared<RocksDBGeoIndex>(id, collection, definition,
                                             "geo2");
  }
};

template<typename F>
struct SecondaryIndexFactory
    : public DelegatingIndexFactory<SecondaryIndexDefinition> {
  using DelegatingIndexFactory::DelegatingIndexFactory;

  std::shared_ptr<Index> instantiate(
      LogicalCollection& collection, velocypack::Slice definition, IndexId id,
      bool /*isClusterConstructor*/) const override {
    return std::make_shared<F>(id, collection, definition);
  }
};

struct MdiIndexFactory : public DelegatingIndexFactory<MdiIndexDefinition> {
  using DelegatingIndexFactory::DelegatingIndexFactory;

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
};

struct MdiPrefixedIndexFactory
    : public DelegatingIndexFactory<MdiPrefixedIndexDefinition> {
  using DelegatingIndexFactory::DelegatingIndexFactory;

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
};

struct VectorIndexFactory
    : public DelegatingIndexFactory<VectorIndexDefinition> {
  using DelegatingIndexFactory::DelegatingIndexFactory;

  std::shared_ptr<arangodb::Index> instantiate(
      arangodb::LogicalCollection& collection,
      arangodb::velocypack::Slice definition, IndexId id,
      bool /*isClusterConstructor*/) const override {
    return std::make_shared<RocksDBVectorIndex>(id, collection, definition);
  }
};

struct TtlIndexFactory : public DelegatingIndexFactory<TtlIndexDefinition> {
  using DelegatingIndexFactory::DelegatingIndexFactory;

  std::shared_ptr<Index> instantiate(
      LogicalCollection& collection, velocypack::Slice definition, IndexId id,
      bool /*isClusterConstructor*/) const override {
    return std::make_shared<RocksDBTtlIndex>(id, collection, definition);
  }
};

struct PrimaryIndexFactory
    : public DelegatingIndexFactory<PrimaryIndexDefinition> {
  using DelegatingIndexFactory::DelegatingIndexFactory;

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
};

}  // namespace

RocksDBIndexFactory::RocksDBIndexFactory(
    application_features::ApplicationServer& server,
    IndexTypeCatalog const& catalog)
    : IndexFactory(server, catalog) {
  static const EdgeIndexFactory edgeIndexFactory(server, catalog.edge());
  static const FulltextIndexFactory fulltextIndexFactory(server,
                                                         catalog.fulltext());
  static const GeoIndexFactory geoIndexFactory(server, catalog.geo());
  static const Geo1IndexFactory geo1IndexFactory(server, catalog.geo1());
  static const Geo2IndexFactory geo2IndexFactory(server, catalog.geo2());
  static const SecondaryIndexFactory<RocksDBHashIndex> hashIndexFactory(
      server, catalog.hash());
  static const SecondaryIndexFactory<RocksDBPersistentIndex>
      persistentIndexFactory(server, catalog.persistent());
  static const SecondaryIndexFactory<RocksDBSkiplistIndex> skiplistIndexFactory(
      server, catalog.skiplist());
  static const TtlIndexFactory ttlIndexFactory(server, catalog.ttl());
  static const PrimaryIndexFactory primaryIndexFactory(server,
                                                       catalog.primary());
  static const MdiIndexFactory zkdIndexFactory(server, catalog.zkd());
  static const MdiIndexFactory mdiIndexFactory(server, catalog.mdi());
  static const VectorIndexFactory vectorIndexFactory(server, catalog.vector());
  static const iresearch::IResearchRocksDBInvertedIndexFactory
      iresearchInvertedIndexFactory(server, catalog.inverted());
  static const MdiPrefixedIndexFactory mdiPrefixedIndexFactory(
      server, catalog.mdiPrefixed());

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
  emplace("vector", vectorIndexFactory);
  emplace(arangodb::iresearch::IRESEARCH_INVERTED_INDEX_TYPE.data(),
          iresearchInvertedIndexFactory);
}

void RocksDBIndexFactory::finalizeDefinition(velocypack::Builder& normalized,
                                             velocypack::Slice definition,
                                             bool isCreation) const {
  if (isCreation && !definition.hasKey(StaticStrings::ObjectId)) {
    normalized.add(StaticStrings::ObjectId,
                   velocypack::Value(std::to_string(TRI_NewTickServer())));
  }
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
      IndexType const type = Index::type(typeSlice.stringView());

      if (type == IndexType::Edge) {
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
