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

#include "RocksDBIndexFactory.h"
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
#include "StorageEngine/EngineSelectorFeature.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ticks.h"
#include "VocBase/voc-types.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

#ifdef USE_IRESEARCH
#include "IResearch/IResearchRocksDBLink.h"
#endif

using namespace arangodb;

////////////////////////////////////////////////////////////////////////////////
/// @brief process the fields list deduplicate and add them to the json
////////////////////////////////////////////////////////////////////////////////

static int ProcessIndexFields(VPackSlice const definition,
                              VPackBuilder& builder, size_t minFields,
                              size_t maxField, bool create) {
  TRI_ASSERT(builder.isOpenObject());
  std::unordered_set<StringRef> fields;
  auto fieldsSlice = definition.get(arangodb::StaticStrings::IndexFields);

  builder.add(
    arangodb::velocypack::Value(arangodb::StaticStrings::IndexFields)
  );
  builder.openArray();

  if (fieldsSlice.isArray()) {
    // "fields" is a list of fields
    for (auto const& it : VPackArrayIterator(fieldsSlice)) {
      if (!it.isString()) {
        return TRI_ERROR_BAD_PARAMETER;
      }

      StringRef f(it);

      if (f.empty() || (create && f == StaticStrings::IdString)) {
        // accessing internal attributes is disallowed
        return TRI_ERROR_BAD_PARAMETER;
      }

      if (fields.find(f) != fields.end()) {
        // duplicate attribute name
        return TRI_ERROR_BAD_PARAMETER;
      }

      fields.insert(f);
      builder.add(it);
    }
  }

  size_t cc = fields.size();
  if (cc == 0 || cc < minFields || cc > maxField) {
    return TRI_ERROR_BAD_PARAMETER;
  }

  builder.close();
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief process the unique flag and add it to the json
////////////////////////////////////////////////////////////////////////////////

static void ProcessIndexUniqueFlag(VPackSlice const definition,
                                   VPackBuilder& builder) {
  bool unique = basics::VelocyPackHelper::getBooleanValue(
    definition, arangodb::StaticStrings::IndexUnique.c_str(), false
  );

  builder.add(
    arangodb::StaticStrings::IndexUnique,
    arangodb::velocypack::Value(unique)
  );
}

////////////////////////////////////////////////////////////////////////////////
/// @brief process the sparse flag and add it to the json
////////////////////////////////////////////////////////////////////////////////

static void ProcessIndexSparseFlag(VPackSlice const definition,
                                   VPackBuilder& builder, bool create) {
  if (definition.hasKey(arangodb::StaticStrings::IndexSparse)) {
    bool sparseBool = basics::VelocyPackHelper::getBooleanValue(
      definition, arangodb::StaticStrings::IndexSparse.c_str(), false
    );

    builder.add(
      arangodb::StaticStrings::IndexSparse,
      arangodb::velocypack::Value(sparseBool)
    );
  } else if (create) {
    // not set. now add a default value
    builder.add(
      arangodb::StaticStrings::IndexSparse,
      arangodb::velocypack::Value(false)
    );
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief process the deduplicate flag and add it to the json
////////////////////////////////////////////////////////////////////////////////

static void ProcessIndexDeduplicateFlag(VPackSlice const definition,
                                        VPackBuilder& builder) {
  bool dup = basics::VelocyPackHelper::getBooleanValue(definition,
                                                       "deduplicate", true);
  builder.add("deduplicate", VPackValue(dup));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief enhances the json of a vpack index
////////////////////////////////////////////////////////////////////////////////

static int EnhanceJsonIndexVPack(VPackSlice const definition,
                                 VPackBuilder& builder, bool create) {
  int res = ProcessIndexFields(definition, builder, 1, INT_MAX, create);

  if (res == TRI_ERROR_NO_ERROR) {
    ProcessIndexSparseFlag(definition, builder, create);
    ProcessIndexUniqueFlag(definition, builder);
    ProcessIndexDeduplicateFlag(definition, builder);
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief process the geojson flag and add it to the json
////////////////////////////////////////////////////////////////////////////////

static void ProcessIndexGeoJsonFlag(VPackSlice const definition,
                                    VPackBuilder& builder) {
  auto fieldsSlice = definition.get(arangodb::StaticStrings::IndexFields);

  if (fieldsSlice.isArray() && fieldsSlice.length() == 1) {
    // only add geoJson for indexes with a single field (with needs to be an array)
    bool geoJson = basics::VelocyPackHelper::getBooleanValue(definition, "geoJson", false);

    builder.add("geoJson", VPackValue(geoJson));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief enhances the json of a geo1 index
////////////////////////////////////////////////////////////////////////////////

static int EnhanceJsonIndexGeo1(VPackSlice const definition,
                                VPackBuilder& builder, bool create) {
  int res = ProcessIndexFields(definition, builder, 1, 1, create);

  if (res == TRI_ERROR_NO_ERROR) {
    builder.add(
      arangodb::StaticStrings::IndexSparse,
      arangodb::velocypack::Value(true)
    );
    builder.add(
      arangodb::StaticStrings::IndexUnique,
      arangodb::velocypack::Value(false)
    );
    ProcessIndexGeoJsonFlag(definition, builder);
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief enhances the json of a geo2 index
////////////////////////////////////////////////////////////////////////////////

static int EnhanceJsonIndexGeo2(VPackSlice const definition,
                                VPackBuilder& builder, bool create) {
  int res = ProcessIndexFields(definition, builder, 2, 2, create);

  if (res == TRI_ERROR_NO_ERROR) {
    builder.add(
      arangodb::StaticStrings::IndexSparse,
      arangodb::velocypack::Value(true)
    );
    builder.add(
      arangodb::StaticStrings::IndexUnique,
      arangodb::velocypack::Value(false)
    );
    ProcessIndexGeoJsonFlag(definition, builder);
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief enhances the json of a geo index
////////////////////////////////////////////////////////////////////////////////

static int EnhanceJsonIndexGeo(VPackSlice const definition,
                               VPackBuilder& builder, bool create) {
  int res = ProcessIndexFields(definition, builder, 1, 2, create);

  if (res == TRI_ERROR_NO_ERROR) {
    builder.add(
      arangodb::StaticStrings::IndexSparse,
      arangodb::velocypack::Value(true)
    );
    builder.add(
      arangodb::StaticStrings::IndexUnique,
      arangodb::velocypack::Value(false)
    );
    ProcessIndexGeoJsonFlag(definition, builder);
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief enhances the json of a fulltext index
////////////////////////////////////////////////////////////////////////////////

static int EnhanceJsonIndexFulltext(VPackSlice const definition,
                                    VPackBuilder& builder, bool create) {
  int res = ProcessIndexFields(definition, builder, 1, 1, create);

  if (res == TRI_ERROR_NO_ERROR) {
    // hard-coded defaults
    builder.add(
      arangodb::StaticStrings::IndexSparse,
      arangodb::velocypack::Value(true)
    );
    builder.add(
      arangodb::StaticStrings::IndexUnique,
      arangodb::velocypack::Value(false)
    );

    // handle "minLength" attribute
    int minWordLength = TRI_FULLTEXT_MIN_WORD_LENGTH_DEFAULT;
    VPackSlice minLength = definition.get("minLength");

    if (minLength.isNumber()) {
      minWordLength = minLength.getNumericValue<int>();
    } else if (!minLength.isNull() && !minLength.isNone()) {
      return TRI_ERROR_BAD_PARAMETER;
    }

    builder.add("minLength", VPackValue(minWordLength));
  }

  return res;
}

namespace {

struct DefaultIndexFactory: public arangodb::IndexTypeFactory {
  std::string const _type;

  DefaultIndexFactory(std::string const& type): _type(type) {}

  virtual bool equal(
      arangodb::velocypack::Slice const& lhs,
      arangodb::velocypack::Slice const& rhs
  ) const override {
    // unique must be identical if present
    auto value = lhs.get(arangodb::StaticStrings::IndexUnique);

    if (value.isBoolean()) {
      if (arangodb::basics::VelocyPackHelper::compare(
            value, rhs.get(arangodb::StaticStrings::IndexUnique), false
          )) {
        return false;
      }
    }

    // sparse must be identical if present
    value = lhs.get(arangodb::StaticStrings::IndexSparse);

    if (value.isBoolean()) {
      if (arangodb::basics::VelocyPackHelper::compare(
            value, rhs.get(arangodb::StaticStrings::IndexSparse), false
          )) {
        return false;
      }
    }

    auto type = Index::type(_type);

    if (arangodb::Index::IndexType::TRI_IDX_TYPE_GEO1_INDEX == type||
        arangodb::Index::IndexType::TRI_IDX_TYPE_GEO_INDEX == type) {
      // geoJson must be identical if present
      value = lhs.get("geoJson");

      if (value.isBoolean()
          && arangodb::basics::VelocyPackHelper::compare(value, rhs.get("geoJson"), false)) {
        return false;
      }
    } else if (arangodb::Index::IndexType::TRI_IDX_TYPE_FULLTEXT_INDEX == type) {
      // minLength
      value = lhs.get("minLength");

      if (value.isNumber()
          && arangodb::basics::VelocyPackHelper::compare(value, rhs.get("minLength"), false)) {
        return false;
      }
    }

    // other index types: fields must be identical if present
    value = lhs.get(arangodb::StaticStrings::IndexFields);

    if (value.isArray()) {
      if (arangodb::Index::IndexType::TRI_IDX_TYPE_HASH_INDEX == type) {
        arangodb::velocypack::ValueLength const nv = value.length();

        // compare fields in arbitrary order
        auto r = rhs.get(arangodb::StaticStrings::IndexFields);

        if (!r.isArray() || nv != r.length()) {
          return false;
        }

        for (size_t i = 0; i < nv; ++i) {
          arangodb::velocypack::Slice const v = value.at(i);

          bool found = false;

          for (auto const& vr : VPackArrayIterator(r)) {
            if (arangodb::basics::VelocyPackHelper::compare(v, vr, false) == 0) {
              found = true;
              break;
            }
          }

          if (!found) {
            return false;
          }
        }
      } else {
        if (arangodb::basics::VelocyPackHelper::compare(
              value, rhs.get(arangodb::StaticStrings::IndexFields), false
            ) != 0) {
          return false;
        }
      }
    }

    return true;
  }
};

struct EdgeIndexFactory: public DefaultIndexFactory {
  EdgeIndexFactory(std::string const& type): DefaultIndexFactory(type) {}

  virtual arangodb::Result instantiate(
      std::shared_ptr<arangodb::Index>& index,
      arangodb::LogicalCollection& collection,
      arangodb::velocypack::Slice const& definition,
      TRI_idx_iid_t id,
      bool isClusterConstructor
  ) const override {
    if (!isClusterConstructor) {
      // this indexes cannot be created directly
      return arangodb::Result(TRI_ERROR_INTERNAL, "cannot create edge index");
    }

    auto fields = definition.get(arangodb::StaticStrings::IndexFields);
    TRI_ASSERT(fields.isArray() && fields.length() == 1);
    auto direction = fields.at(0).copyString();
    TRI_ASSERT(direction == StaticStrings::FromString ||
               direction == StaticStrings::ToString);

    index = std::make_shared<arangodb::RocksDBEdgeIndex>(
      id, collection, definition, direction
    );

    return arangodb::Result();
  }

  virtual arangodb::Result normalize(
      arangodb::velocypack::Builder& normalized,
      arangodb::velocypack::Slice definition,
      bool isCreation
  ) const override {
    if (isCreation) {
      // creating these indexes yourself is forbidden
      return TRI_ERROR_FORBIDDEN;
    }

    TRI_ASSERT(normalized.isOpenObject());
    normalized.add(
      arangodb::StaticStrings::IndexType,
      arangodb::velocypack::Value(
        arangodb::Index::oldtypeName(arangodb::Index::TRI_IDX_TYPE_EDGE_INDEX)
      )
    );

    return TRI_ERROR_INTERNAL;
  }
};

struct FulltextIndexFactory: public DefaultIndexFactory {
  FulltextIndexFactory(std::string const& type): DefaultIndexFactory(type) {}

  virtual arangodb::Result instantiate(
      std::shared_ptr<arangodb::Index>& index,
      arangodb::LogicalCollection& collection,
      arangodb::velocypack::Slice const& definition,
      TRI_idx_iid_t id,
      bool isClusterConstructor
  ) const override {
    index = std::make_shared<arangodb::RocksDBFulltextIndex>(
      id, collection, definition
    );

    return arangodb::Result();
  }

  virtual arangodb::Result normalize(
      arangodb::velocypack::Builder& normalized,
      arangodb::velocypack::Slice definition,
      bool isCreation
  ) const override {
    TRI_ASSERT(normalized.isOpenObject());
    normalized.add(
      arangodb::StaticStrings::IndexType,
      arangodb::velocypack::Value(arangodb::Index::oldtypeName(
        arangodb::Index::TRI_IDX_TYPE_FULLTEXT_INDEX
      ))
    );

    if (isCreation
        && !ServerState::instance()->isCoordinator()
        && !definition.hasKey("objectId")) {
      normalized.add(
        "objectId",
        arangodb::velocypack::Value(std::to_string(TRI_NewTickServer()))
      );
    }

    return EnhanceJsonIndexFulltext(definition, normalized, isCreation);
  }
};

struct GeoIndexFactory: public DefaultIndexFactory {
  GeoIndexFactory(std::string const& type): DefaultIndexFactory(type) {}

  virtual arangodb::Result instantiate(
      std::shared_ptr<arangodb::Index>& index,
      arangodb::LogicalCollection& collection,
      arangodb::velocypack::Slice const& definition,
      TRI_idx_iid_t id,
      bool isClusterConstructor
  ) const override {
    index = std::make_shared<arangodb::RocksDBGeoIndex>(
      id, collection, definition, "geo"
    );

    return arangodb::Result();
  }

  virtual arangodb::Result normalize(
      arangodb::velocypack::Builder& normalized,
      arangodb::velocypack::Slice definition,
      bool isCreation
  ) const override {
    TRI_ASSERT(normalized.isOpenObject());
    normalized.add(
      arangodb::StaticStrings::IndexType,
      arangodb::velocypack::Value(
        arangodb::Index::oldtypeName(arangodb::Index::TRI_IDX_TYPE_GEO_INDEX)
      )
    );

    if (isCreation
        && !ServerState::instance()->isCoordinator()
        && !definition.hasKey("objectId")) {
      normalized.add(
        "objectId",
        arangodb::velocypack::Value(std::to_string(TRI_NewTickServer()))
      );
    }

    return EnhanceJsonIndexGeo(definition, normalized, isCreation);
  }
};

struct Geo1IndexFactory: public DefaultIndexFactory {
  Geo1IndexFactory(std::string const& type): DefaultIndexFactory(type) {}

  virtual arangodb::Result instantiate(
      std::shared_ptr<arangodb::Index>& index,
      arangodb::LogicalCollection& collection,
      arangodb::velocypack::Slice const& definition,
      TRI_idx_iid_t id,
      bool isClusterConstructor
  ) const override {
    index = std::make_shared<arangodb::RocksDBGeoIndex>(
      id, collection, definition, "geo1"
    );

    return arangodb::Result();
  }

  virtual arangodb::Result normalize(
      arangodb::velocypack::Builder& normalized,
      arangodb::velocypack::Slice definition,
      bool isCreation
  ) const override {
    TRI_ASSERT(normalized.isOpenObject());
    normalized.add(
      arangodb::StaticStrings::IndexType,
      arangodb::velocypack::Value(
        arangodb::Index::oldtypeName(arangodb::Index::TRI_IDX_TYPE_GEO_INDEX)
      )
    );

    if (isCreation
        && !ServerState::instance()->isCoordinator()
        && !definition.hasKey("objectId")) {
      normalized.add(
        "objectId",
        arangodb::velocypack::Value(std::to_string(TRI_NewTickServer()))
      );
    }

    return EnhanceJsonIndexGeo1(definition, normalized, isCreation);
  }
};

struct Geo2IndexFactory: public DefaultIndexFactory {
  Geo2IndexFactory(std::string const& type): DefaultIndexFactory(type) {}

  virtual arangodb::Result instantiate(
      std::shared_ptr<arangodb::Index>& index,
      arangodb::LogicalCollection& collection,
      arangodb::velocypack::Slice const& definition,
      TRI_idx_iid_t id,
      bool isClusterConstructor
  ) const override {
    index = std::make_shared<arangodb::RocksDBGeoIndex>(
      id, collection, definition, "geo2"
    );

    return arangodb::Result();
  }

  virtual arangodb::Result normalize(
      arangodb::velocypack::Builder& normalized,
      arangodb::velocypack::Slice definition,
      bool isCreation
  ) const override {
    TRI_ASSERT(normalized.isOpenObject());
    normalized.add(
      arangodb::StaticStrings::IndexType,
      arangodb::velocypack::Value(
        arangodb::Index::oldtypeName(arangodb::Index::TRI_IDX_TYPE_GEO_INDEX)
      )
    );

    if (isCreation
        && !ServerState::instance()->isCoordinator()
        && !definition.hasKey("objectId")) {
      normalized.add(
        "objectId",
        arangodb::velocypack::Value(std::to_string(TRI_NewTickServer()))
      );
    }

    return EnhanceJsonIndexGeo2(definition, normalized, isCreation);
  }
};

struct HashIndexFactory: public DefaultIndexFactory {
  HashIndexFactory(std::string const& type): DefaultIndexFactory(type) {}

  virtual arangodb::Result instantiate(
      std::shared_ptr<arangodb::Index>& index,
      arangodb::LogicalCollection& collection,
      arangodb::velocypack::Slice const& definition,
      TRI_idx_iid_t id,
      bool isClusterConstructor
  ) const override {
    index = std::make_shared<arangodb::RocksDBHashIndex>(
      id, collection, definition
    );

    return arangodb::Result();
  }

  virtual arangodb::Result normalize(
      arangodb::velocypack::Builder& normalized,
      arangodb::velocypack::Slice definition,
      bool isCreation
  ) const override {
    TRI_ASSERT(normalized.isOpenObject());
    normalized.add(
      arangodb::StaticStrings::IndexType,
      arangodb::velocypack::Value(
        arangodb::Index::oldtypeName(arangodb::Index::TRI_IDX_TYPE_HASH_INDEX)
      )
    );

    if (isCreation
        && !ServerState::instance()->isCoordinator()
        && !definition.hasKey("objectId")) {
      normalized.add(
        "objectId",
        arangodb::velocypack::Value(std::to_string(TRI_NewTickServer()))
      );
    }

    return EnhanceJsonIndexVPack(definition, normalized, isCreation);
  }
};

struct PersistentIndexFactory: public DefaultIndexFactory {
  PersistentIndexFactory(std::string const& type): DefaultIndexFactory(type) {}

  virtual arangodb::Result instantiate(
      std::shared_ptr<arangodb::Index>& index,
      arangodb::LogicalCollection& collection,
      arangodb::velocypack::Slice const& definition,
      TRI_idx_iid_t id,
      bool isClusterConstructor
  ) const override {
    index = std::make_shared<arangodb::RocksDBPersistentIndex>(
      id, collection, definition
    );

    return arangodb::Result();
  }

  virtual arangodb::Result normalize(
      arangodb::velocypack::Builder& normalized,
      arangodb::velocypack::Slice definition,
      bool isCreation
  ) const override {
    TRI_ASSERT(normalized.isOpenObject());
    normalized.add(
      arangodb::StaticStrings::IndexType,
      arangodb::velocypack::Value(arangodb::Index::oldtypeName(
        arangodb::Index::TRI_IDX_TYPE_PERSISTENT_INDEX
      ))
    );

    if (isCreation
        && !ServerState::instance()->isCoordinator()
        && !definition.hasKey("objectId")) {
      normalized.add(
        "objectId",
        arangodb::velocypack::Value(std::to_string(TRI_NewTickServer()))
      );
    }

    return EnhanceJsonIndexVPack(definition, normalized, isCreation);
  }
};

struct PrimaryIndexFactory: public DefaultIndexFactory {
  PrimaryIndexFactory(std::string const& type): DefaultIndexFactory(type) {}

  virtual arangodb::Result instantiate(
      std::shared_ptr<arangodb::Index>& index,
      arangodb::LogicalCollection& collection,
      arangodb::velocypack::Slice const& definition,
      TRI_idx_iid_t id,
      bool isClusterConstructor
  ) const override {
    if (!isClusterConstructor) {
      // this indexes cannot be created directly
      return arangodb::Result(
        TRI_ERROR_INTERNAL,
        "cannot create primary index"
      );
    }

    index = std::make_shared<arangodb::RocksDBPrimaryIndex>(
      collection, definition
    );

    return arangodb::Result();
  }

  virtual arangodb::Result normalize(
      arangodb::velocypack::Builder& normalized,
      arangodb::velocypack::Slice definition,
      bool isCreation
  ) const override {
    if (isCreation) {
      // creating these indexes yourself is forbidden
      return TRI_ERROR_FORBIDDEN;
    }

    TRI_ASSERT(normalized.isOpenObject());
    normalized.add(
      arangodb::StaticStrings::IndexType,
      arangodb::velocypack::Value(arangodb::Index::oldtypeName(
        arangodb::Index::TRI_IDX_TYPE_PRIMARY_INDEX
      ))
    );

    return TRI_ERROR_INTERNAL;
  }
};

struct SkiplistIndexFactory: public DefaultIndexFactory {
  SkiplistIndexFactory(std::string const& type): DefaultIndexFactory(type) {}

  virtual arangodb::Result instantiate(
      std::shared_ptr<arangodb::Index>& index,
      arangodb::LogicalCollection& collection,
      arangodb::velocypack::Slice const& definition,
      TRI_idx_iid_t id,
      bool isClusterConstructor
  ) const override {
    index = std::make_shared<arangodb::RocksDBSkiplistIndex>(
      id, collection, definition
    );

    return arangodb::Result();
  }

  virtual arangodb::Result normalize(
      arangodb::velocypack::Builder& normalized,
      arangodb::velocypack::Slice definition,
      bool isCreation
  ) const override {
    TRI_ASSERT(normalized.isOpenObject());
    normalized.add(
      arangodb::StaticStrings::IndexType,
      arangodb::velocypack::Value(arangodb::Index::oldtypeName(
        arangodb::Index::TRI_IDX_TYPE_SKIPLIST_INDEX
      ))
    );

    if (isCreation
        && !ServerState::instance()->isCoordinator()
        && !definition.hasKey("objectId")) {
      normalized.add(
        "objectId",
        arangodb::velocypack::Value(std::to_string(TRI_NewTickServer()))
      );
    }

    return EnhanceJsonIndexVPack(definition, normalized, isCreation);
  }
};

}

RocksDBIndexFactory::RocksDBIndexFactory() {
  static const EdgeIndexFactory edgeIndexFactory("edge");
  static const FulltextIndexFactory fulltextIndexFactory("fulltext");
  static const GeoIndexFactory geoIndexFactory("geo");
  static const Geo1IndexFactory geo1IndexFactory("geo1");
  static const Geo2IndexFactory geo2IndexFactory("geo2");
  static const HashIndexFactory hashIndexFactory("hash");
  static const PersistentIndexFactory persistentIndexFactory("persistent");
  static const PrimaryIndexFactory primaryIndexFactory("primary");
  static const SkiplistIndexFactory skiplistIndexFactory("skiplist");

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
}


void RocksDBIndexFactory::fillSystemIndexes(
    arangodb::LogicalCollection& col,
    std::vector<std::shared_ptr<arangodb::Index>>& indexes
) const {
  // create primary index
  VPackBuilder builder;
  builder.openObject();
  builder.close();

  indexes.emplace_back(std::make_shared<RocksDBPrimaryIndex>(col, builder.slice()));

  // create edges indexes
  if (TRI_COL_TYPE_EDGE == col.type()) {
    indexes.emplace_back(std::make_shared<arangodb::RocksDBEdgeIndex>(1, col, builder.slice(), StaticStrings::FromString));
    indexes.emplace_back(std::make_shared<arangodb::RocksDBEdgeIndex>(2, col, builder.slice(), StaticStrings::ToString));
  }
}

/// @brief create indexes from a list of index definitions
void RocksDBIndexFactory::prepareIndexes(
    LogicalCollection& col,
    arangodb::velocypack::Slice const& indexesSlice,
    std::vector<std::shared_ptr<arangodb::Index>>& indexes
) const {
  TRI_ASSERT(indexesSlice.isArray());

  bool splitEdgeIndex = false;
  TRI_idx_iid_t last = 0;

  for (auto const& v : VPackArrayIterator(indexesSlice)) {
    if (arangodb::basics::VelocyPackHelper::getBooleanValue(v, "error",
                                                            false)) {
      // We have an error here.
      // Do not add index.
      // TODO Handle Properly
      continue;
    }

    // check for combined edge index from MMFiles; must split!
    auto value = v.get("type");

    if (value.isString()) {
      std::string tmp = value.copyString();
      arangodb::Index::IndexType const type =
      arangodb::Index::type(tmp.c_str());

      if (type == Index::IndexType::TRI_IDX_TYPE_EDGE_INDEX) {
        VPackSlice fields = v.get("fields");

        if (fields.isArray() && fields.length() == 2) {
          VPackBuilder from;

          from.openObject();

          for (auto const& f : VPackObjectIterator(v)) {
            if (arangodb::StringRef(f.key) == "fields") {
              from.add(VPackValue("fields"));
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
            if (arangodb::StringRef(f.key) == "fields") {
              to.add(VPackValue("fields"));
              to.openArray();
              to.add(VPackValue(StaticStrings::ToString));
              to.close();
            } else if (arangodb::StringRef(f.key) == "id") {
              auto iid = basics::StringUtils::uint64(f.value.copyString()) + 1;

              last = iid;
              to.add("id", VPackValue(std::to_string(iid)));
            } else {
              to.add(f.key);
              to.add(f.value);
            }
          }

          to.close();

          auto idxFrom = prepareIndexFromSlice(from.slice(), false, col, true);

          if (!idxFrom) {
            LOG_TOPIC(ERR, arangodb::Logger::ENGINES)
              << "error creating index from definition '" << from.slice().toString() << "'";

            continue;
          }

          auto idxTo = prepareIndexFromSlice(to.slice(), false, col, true);

          if (!idxTo) {
            LOG_TOPIC(ERR, arangodb::Logger::ENGINES)
              << "error creating index from definition '" << to.slice().toString() << "'";

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
          if (arangodb::StringRef(f.key) == "id") {
            last++;
            b.add("id", VPackValue(std::to_string(last)));
          } else {
            b.add(f.key);
            b.add(f.value);
          }
        }

        b.close();

        auto idx = prepareIndexFromSlice(b.slice(), false, col, true);

        if (!idx) {
          LOG_TOPIC(ERR, arangodb::Logger::ENGINES)
            << "error creating index from definition '" << b.slice().toString() << "'";

          continue;
        }

        indexes.emplace_back(std::move(idx));

        continue;
      }
    }

    auto idx = prepareIndexFromSlice(v, false, col, true);

    if (!idx) {
      LOG_TOPIC(ERR, arangodb::Logger::ENGINES)
        << "error creating index from definition '" << v.toString() << "'";

      continue;
    }
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    else {
      LOG_TOPIC(DEBUG, arangodb::Logger::ENGINES)
          << "created index '" << idx->id() << "' from definition '"
          << v.toJson() << "'";
    }
#endif

    indexes.emplace_back(std::move(idx));
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
