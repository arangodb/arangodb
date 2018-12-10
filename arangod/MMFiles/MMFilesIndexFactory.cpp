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
#include "Basics/StringRef.h"
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
#include "MMFiles/mmfiles-fulltext-index.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/voc-types.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

#ifdef USE_IRESEARCH
#include "IResearch/IResearchMMFilesLink.h"
#endif

using namespace arangodb;

////////////////////////////////////////////////////////////////////////////////
/// @brief process the fields list deduplicate and add them to the json
////////////////////////////////////////////////////////////////////////////////

static int ProcessIndexFields(VPackSlice const definition, VPackBuilder& builder,
                              size_t minFields, size_t maxField, bool create) {
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
  bool dup = true;
  if (definition.hasKey("deduplicate")) {
    dup = basics::VelocyPackHelper::getBooleanValue(definition, "deduplicate", true);
  }
  builder.add("deduplicate", VPackValue(dup));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief enhances the json of a hash index
////////////////////////////////////////////////////////////////////////////////

static int EnhanceJsonIndexHash(VPackSlice const definition,
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
/// @brief enhances the json of a skiplist index
////////////////////////////////////////////////////////////////////////////////

static int EnhanceJsonIndexSkiplist(VPackSlice const definition,
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
/// @brief enhances the json of a Persistent index
////////////////////////////////////////////////////////////////////////////////

static int EnhanceJsonIndexPersistent(VPackSlice const definition,
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
/// @brief enhances the json of a s2 index
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

    index = std::make_shared<arangodb::MMFilesEdgeIndex>(id, collection);

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
        Index::oldtypeName(Index::TRI_IDX_TYPE_EDGE_INDEX)
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
    index = std::make_shared<arangodb::MMFilesFulltextIndex>(
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
        Index::oldtypeName(Index::TRI_IDX_TYPE_FULLTEXT_INDEX)
      )
    );

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
    index = std::make_shared<arangodb::MMFilesGeoIndex>(
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
        Index::oldtypeName(Index::TRI_IDX_TYPE_GEO_INDEX)
      )
    );

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
    index = std::make_shared<arangodb::MMFilesGeoIndex>(
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
        Index::oldtypeName(Index::TRI_IDX_TYPE_GEO_INDEX)
      )
    );

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
    index = std::make_shared<arangodb::MMFilesGeoIndex>(
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
        Index::oldtypeName(Index::TRI_IDX_TYPE_GEO_INDEX)
      )
    );

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
    index = std::make_shared<arangodb::MMFilesHashIndex>(
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
        Index::oldtypeName(Index::TRI_IDX_TYPE_HASH_INDEX)
      )
    );

    return EnhanceJsonIndexHash(definition, normalized, isCreation);
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
    index = std::make_shared<arangodb::MMFilesPersistentIndex>(
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
        Index::oldtypeName(Index::TRI_IDX_TYPE_PERSISTENT_INDEX)
      )
    );

    return EnhanceJsonIndexPersistent(definition, normalized, isCreation);
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

    index = std::make_shared<MMFilesPrimaryIndex>(collection);

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
        Index::oldtypeName(Index::TRI_IDX_TYPE_PRIMARY_INDEX)
      )
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
    index = std::make_shared<arangodb::MMFilesSkiplistIndex>(
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
        Index::oldtypeName(Index::TRI_IDX_TYPE_SKIPLIST_INDEX)
      )
    );

    return EnhanceJsonIndexSkiplist(definition, normalized, isCreation);
  }
};

}

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

void MMFilesIndexFactory::fillSystemIndexes(
    arangodb::LogicalCollection& col,
    std::vector<std::shared_ptr<arangodb::Index>>& systemIndexes
) const {
    // create primary index
    systemIndexes.emplace_back(std::make_shared<arangodb::MMFilesPrimaryIndex>(col));
    
    // create edges index
    if (TRI_COL_TYPE_EDGE == col.type()) {
      systemIndexes.emplace_back(std::make_shared<arangodb::MMFilesEdgeIndex>(1, col));
    }
  }

void MMFilesIndexFactory::prepareIndexes(
    LogicalCollection& col,
    arangodb::velocypack::Slice const& indexesSlice,
    std::vector<std::shared_ptr<arangodb::Index>>& indexes
) const {
  for (auto const& v : VPackArrayIterator(indexesSlice)) {
    if (basics::VelocyPackHelper::getBooleanValue(v, "error", false)) {
      // We have an error here.
      // Do not add index.
      continue;
    }

    auto idx = prepareIndexFromSlice(v, false, col, true);

    if (!idx) {
      LOG_TOPIC(ERR, arangodb::Logger::ENGINES)
      << "error creating index from definition '" << v.toString() << "'";

      continue;
    }

    indexes.emplace_back(std::move(idx));
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------