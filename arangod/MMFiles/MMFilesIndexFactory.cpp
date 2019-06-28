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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "MMFilesIndexFactory.h"
#include "Basics/Exceptions.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ServerState.h"
#include "Indexes/Index.h"
#include "MMFiles/MMFilesEdgeIndex.h"
#include "MMFiles/MMFilesFulltextIndex.h"
#include "MMFiles/MMFilesGeoIndex.h"
#include "MMFiles/MMFilesHashIndex.h"
#include "MMFiles/MMFilesPersistentIndex.h"
#include "MMFiles/MMFilesPrimaryIndex.h"
#include "MMFiles/MMFilesSkiplistIndex.h"
#include "MMFiles/MMFilesTtlIndex.h"
#include "MMFiles/mmfiles-fulltext-index.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/voc-types.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/StringRef.h>
#include <velocypack/velocypack-aliases.h>

#include "IResearch/IResearchMMFilesLink.h"

using namespace arangodb;

namespace {

struct DefaultIndexFactory : public arangodb::IndexTypeFactory {
  std::string const _type;

  explicit DefaultIndexFactory(std::string const& type) : _type(type) {}

  bool equal(arangodb::velocypack::Slice const& lhs,
             arangodb::velocypack::Slice const& rhs) const override {
    return arangodb::IndexTypeFactory::equal(Index::type(_type), lhs, rhs, attributeOrderMatters());
  }
};

struct EdgeIndexFactory : public DefaultIndexFactory {
  explicit EdgeIndexFactory(std::string const& type) : DefaultIndexFactory(type) {}

  std::shared_ptr<arangodb::Index> instantiate(arangodb::LogicalCollection& collection,
                                               arangodb::velocypack::Slice const& definition,
                                               TRI_idx_iid_t id,
                                               bool isClusterConstructor) const override {
    if (!isClusterConstructor) {
      // this index type cannot be created directly
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "cannot create edge index");
    }

    return std::make_shared<arangodb::MMFilesEdgeIndex>(id, collection);
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
                   arangodb::velocypack::Value(Index::oldtypeName(Index::TRI_IDX_TYPE_EDGE_INDEX)));

    return TRI_ERROR_INTERNAL;
  }
};

struct FulltextIndexFactory : public DefaultIndexFactory {
  explicit FulltextIndexFactory(std::string const& type) : DefaultIndexFactory(type) {}

  std::shared_ptr<arangodb::Index> instantiate(arangodb::LogicalCollection& collection,
                                               arangodb::velocypack::Slice const& definition,
                                               TRI_idx_iid_t id,
                                               bool isClusterConstructor) const override {
    return std::make_shared<arangodb::MMFilesFulltextIndex>(id, collection, definition);
  }

  virtual arangodb::Result normalize( // normalize definition
      arangodb::velocypack::Builder& normalized, // normalized definition (out-param)
      arangodb::velocypack::Slice definition, // source definition
      bool isCreation, // definition for index creation
      TRI_vocbase_t const& vocbase // index vocbase
  ) const override {
    TRI_ASSERT(normalized.isOpenObject());
    normalized.add(arangodb::StaticStrings::IndexType,
                   arangodb::velocypack::Value(
                       Index::oldtypeName(Index::TRI_IDX_TYPE_FULLTEXT_INDEX)));

    return IndexFactory::enhanceJsonIndexFulltext(definition, normalized, isCreation);
  }
};

struct GeoIndexFactory : public DefaultIndexFactory {
  explicit GeoIndexFactory(std::string const& type) : DefaultIndexFactory(type) {}

  std::shared_ptr<arangodb::Index> instantiate(arangodb::LogicalCollection& collection,
                                               arangodb::velocypack::Slice const& definition,
                                               TRI_idx_iid_t id,
                                               bool isClusterConstructor) const override {
    return std::make_shared<arangodb::MMFilesGeoIndex>(id, collection, definition, "geo");
  }

  virtual arangodb::Result normalize( // normalize definition
      arangodb::velocypack::Builder& normalized, // normalized definition (out-param)
      arangodb::velocypack::Slice definition, // source definition
      bool isCreation, // definition for index creation
      TRI_vocbase_t const& vocbase // index vocbase
  ) const override {
    TRI_ASSERT(normalized.isOpenObject());
    normalized.add(arangodb::StaticStrings::IndexType,
                   arangodb::velocypack::Value(Index::oldtypeName(Index::TRI_IDX_TYPE_GEO_INDEX)));

    return IndexFactory::enhanceJsonIndexGeo(definition, normalized, isCreation, 1, 2);
  }
};

struct Geo1IndexFactory : public DefaultIndexFactory {
  explicit Geo1IndexFactory(std::string const& type) : DefaultIndexFactory(type) {}

  std::shared_ptr<arangodb::Index> instantiate(arangodb::LogicalCollection& collection,
                                               arangodb::velocypack::Slice const& definition,
                                               TRI_idx_iid_t id,
                                               bool isClusterConstructor) const override {
    return std::make_shared<arangodb::MMFilesGeoIndex>(id, collection, definition, "geo1");
  }

  virtual arangodb::Result normalize( // normalize definition
      arangodb::velocypack::Builder& normalized, // normalized definition (out-param)
      arangodb::velocypack::Slice definition, // source definition
      bool isCreation, // definition for index creation
      TRI_vocbase_t const& vocbase // index vocbase
  ) const override {
    TRI_ASSERT(normalized.isOpenObject());
    normalized.add(arangodb::StaticStrings::IndexType,
                   arangodb::velocypack::Value(Index::oldtypeName(Index::TRI_IDX_TYPE_GEO_INDEX)));

    return IndexFactory::enhanceJsonIndexGeo(definition, normalized, isCreation, 1, 1);
  }
};

struct Geo2IndexFactory : public DefaultIndexFactory {
  explicit Geo2IndexFactory(std::string const& type) : DefaultIndexFactory(type) {}

  std::shared_ptr<arangodb::Index> instantiate(arangodb::LogicalCollection& collection,
                                               arangodb::velocypack::Slice const& definition,
                                               TRI_idx_iid_t id,
                                               bool isClusterConstructor) const override {
    return std::make_shared<arangodb::MMFilesGeoIndex>(id, collection, definition, "geo2");
  }

  virtual arangodb::Result normalize( // normalize definition
      arangodb::velocypack::Builder& normalized, // normalized definition (out-param)
      arangodb::velocypack::Slice definition, // source definition
      bool isCreation, // definition for index creation
      TRI_vocbase_t const& vocbase // index vocbase
  ) const override {
    TRI_ASSERT(normalized.isOpenObject());
    normalized.add(arangodb::StaticStrings::IndexType,
                   arangodb::velocypack::Value(Index::oldtypeName(Index::TRI_IDX_TYPE_GEO_INDEX)));

    return IndexFactory::enhanceJsonIndexGeo(definition, normalized, isCreation, 2, 2);
  }
};

struct HashIndexFactory : public DefaultIndexFactory {
  explicit HashIndexFactory(std::string const& type) : DefaultIndexFactory(type) {}

  std::shared_ptr<arangodb::Index> instantiate(arangodb::LogicalCollection& collection,
                                               arangodb::velocypack::Slice const& definition,
                                               TRI_idx_iid_t id,
                                               bool isClusterConstructor) const override {
    return std::make_shared<arangodb::MMFilesHashIndex>(id, collection, definition);
  }

  virtual arangodb::Result normalize( // normalize definition
      arangodb::velocypack::Builder& normalized, // normalized definition (out-param)
      arangodb::velocypack::Slice definition, // source definition
      bool isCreation, // definition for index creation
      TRI_vocbase_t const& vocbase // index vocbase
  ) const override {
    TRI_ASSERT(normalized.isOpenObject());
    normalized.add(arangodb::StaticStrings::IndexType,
                   arangodb::velocypack::Value(Index::oldtypeName(Index::TRI_IDX_TYPE_HASH_INDEX)));

    return IndexFactory::enhanceJsonIndexGeneric(definition, normalized, isCreation);
  }

  bool attributeOrderMatters() const override {
    // an index on ["a", "b"] is the same as an index on ["b", "a"]
    return false;
  }
};

struct PersistentIndexFactory : public DefaultIndexFactory {
  explicit PersistentIndexFactory(std::string const& type) : DefaultIndexFactory(type) {}

  std::shared_ptr<arangodb::Index> instantiate(arangodb::LogicalCollection& collection,
                                               arangodb::velocypack::Slice const& definition,
                                               TRI_idx_iid_t id,
                                               bool isClusterConstructor) const override {
    return std::make_shared<arangodb::MMFilesPersistentIndex>(id, collection, definition);
  }

  virtual arangodb::Result normalize( // normalize definition
      arangodb::velocypack::Builder& normalized, // normalized definition (out-param)
      arangodb::velocypack::Slice definition, // source definition
      bool isCreation, // definition for index creation
      TRI_vocbase_t const& vocbase // index vocbase
  ) const override {
    TRI_ASSERT(normalized.isOpenObject());
    normalized.add(arangodb::StaticStrings::IndexType,
                   arangodb::velocypack::Value(
                       Index::oldtypeName(Index::TRI_IDX_TYPE_PERSISTENT_INDEX)));

    return IndexFactory::enhanceJsonIndexGeneric(definition, normalized, isCreation);
  }
};

struct TtlIndexFactory : public DefaultIndexFactory {
  explicit TtlIndexFactory(std::string const& type) : DefaultIndexFactory(type) {}

  std::shared_ptr<arangodb::Index> instantiate(arangodb::LogicalCollection& collection,
                                               arangodb::velocypack::Slice const& definition,
                                               TRI_idx_iid_t id,
                                               bool isClusterConstructor) const override {
    return std::make_shared<arangodb::MMFilesTtlIndex>(id, collection, definition);
  }

  virtual arangodb::Result normalize( // normalize definition
      arangodb::velocypack::Builder& normalized, // normalized definition (out-param)
      arangodb::velocypack::Slice definition, // source definition
      bool isCreation, // definition for index creation
      TRI_vocbase_t const& vocbase // index vocbase
  ) const override {
    TRI_ASSERT(normalized.isOpenObject());
    normalized.add(arangodb::StaticStrings::IndexType,
                   arangodb::velocypack::Value(Index::oldtypeName(Index::TRI_IDX_TYPE_TTL_INDEX)));

    return IndexFactory::enhanceJsonIndexTtl(definition, normalized, isCreation);
  }
};

struct PrimaryIndexFactory : public DefaultIndexFactory {
  explicit PrimaryIndexFactory(std::string const& type) : DefaultIndexFactory(type) {}

  std::shared_ptr<arangodb::Index> instantiate(arangodb::LogicalCollection& collection,
                                               arangodb::velocypack::Slice const& definition,
                                               TRI_idx_iid_t id,
                                               bool isClusterConstructor) const override {
    if (!isClusterConstructor) {
      // this index type cannot be created directly
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "cannot create edge index");
    }

    return std::make_shared<MMFilesPrimaryIndex>(collection);
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
                   arangodb::velocypack::Value(
                       Index::oldtypeName(Index::TRI_IDX_TYPE_PRIMARY_INDEX)));

    return TRI_ERROR_INTERNAL;
  }
};

struct SkiplistIndexFactory : public DefaultIndexFactory {
  explicit SkiplistIndexFactory(std::string const& type) : DefaultIndexFactory(type) {}

  std::shared_ptr<arangodb::Index> instantiate(arangodb::LogicalCollection& collection,
                                               arangodb::velocypack::Slice const& definition,
                                               TRI_idx_iid_t id,
                                               bool isClusterConstructor) const override {
    return std::make_shared<arangodb::MMFilesSkiplistIndex>(id, collection, definition);
  }

  virtual arangodb::Result normalize( // normalize definition
      arangodb::velocypack::Builder& normalized, // normalized definition (out-param)
      arangodb::velocypack::Slice definition, // source definition
      bool isCreation, // definition for index creation
      TRI_vocbase_t const& vocbase // index vocbase
  ) const override {
    TRI_ASSERT(normalized.isOpenObject());
    normalized.add(arangodb::StaticStrings::IndexType,
                   arangodb::velocypack::Value(
                       Index::oldtypeName(Index::TRI_IDX_TYPE_SKIPLIST_INDEX)));

    return IndexFactory::enhanceJsonIndexGeneric(definition, normalized, isCreation);
  }
};

}  // namespace

MMFilesIndexFactory::MMFilesIndexFactory() {
  static const EdgeIndexFactory edgeIndexFactory("edge");
  static const FulltextIndexFactory fulltextIndexFactory("fulltext");
  static const GeoIndexFactory geoIndexFactory("geo");
  static const Geo1IndexFactory geo1IndexFactory("geo1");
  static const Geo2IndexFactory geo2IndexFactory("geo2");
  static const HashIndexFactory hashIndexFactory("hash");
  static const PersistentIndexFactory persistentIndexFactory("persistent");
  static const PrimaryIndexFactory primaryIndexFactory("primary");
  static const SkiplistIndexFactory skiplistIndexFactory("skiplist");
  static const TtlIndexFactory ttlIndexFactory("ttl");

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

void MMFilesIndexFactory::fillSystemIndexes(arangodb::LogicalCollection& col,
                                            std::vector<std::shared_ptr<arangodb::Index>>& systemIndexes) const {
  // create primary index
  systemIndexes.emplace_back(std::make_shared<arangodb::MMFilesPrimaryIndex>(col));

  // create edges index
  if (TRI_COL_TYPE_EDGE == col.type()) {
    systemIndexes.emplace_back(std::make_shared<arangodb::MMFilesEdgeIndex>(1, col));
  }
}

void MMFilesIndexFactory::prepareIndexes(
    LogicalCollection& col, arangodb::velocypack::Slice const& indexesSlice,
    std::vector<std::shared_ptr<arangodb::Index>>& indexes) const {
  for (auto const& v : VPackArrayIterator(indexesSlice)) {
    if (!validateFieldsDefinition(v, 0, SIZE_MAX).ok()) {
      continue;
    }
  
    try {
      auto idx = prepareIndexFromSlice(v, false, col, true);
      TRI_ASSERT(idx != nullptr);
      indexes.emplace_back(std::move(idx));
    } catch (std::exception const& ex) {
      LOG_TOPIC("dc878", ERR, arangodb::Logger::ENGINES)
          << "error creating index from definition '" << v.toString() << "'" << ex.what();

    }
  }
}
