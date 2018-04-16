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
/// @brief process the fields list and add them to the json
////////////////////////////////////////////////////////////////////////////////

static int ProcessIndexFields(VPackSlice const definition,
                              VPackBuilder& builder, int numFields,
                              bool create) {
  TRI_ASSERT(builder.isOpenObject());
  std::unordered_set<StringRef> fields;

  VPackSlice fieldsSlice = definition.get("fields");
  builder.add(VPackValue("fields"));
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

  if (fields.empty() || (numFields > 0 && (int)fields.size() != numFields)) {
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
  bool unique =
      basics::VelocyPackHelper::getBooleanValue(definition, "unique", false);
  builder.add("unique", VPackValue(unique));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief process the sparse flag and add it to the json
////////////////////////////////////////////////////////////////////////////////

static void ProcessIndexSparseFlag(VPackSlice const definition,
                                   VPackBuilder& builder, bool create) {
  if (definition.hasKey("sparse")) {
    bool sparseBool =
        basics::VelocyPackHelper::getBooleanValue(definition, "sparse", false);
    builder.add("sparse", VPackValue(sparseBool));
  } else if (create) {
    // not set. now add a default value
    builder.add("sparse", VPackValue(false));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief process the deduplicate flag and add it to the json
////////////////////////////////////////////////////////////////////////////////

static void ProcessIndexDeduplicateFlag(VPackSlice const definition,
                                        VPackBuilder& builder) {
  bool dup =
      basics::VelocyPackHelper::getBooleanValue(definition, "deduplicate", true);
  builder.add("deduplicate", VPackValue(dup));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief enhances the json of a hash index
////////////////////////////////////////////////////////////////////////////////

static int EnhanceJsonIndexHash(VPackSlice const definition,
                                VPackBuilder& builder, bool create) {
  int res = ProcessIndexFields(definition, builder, 0, create);
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
  int res = ProcessIndexFields(definition, builder, 0, create);
  if (res == TRI_ERROR_NO_ERROR) {
    ProcessIndexSparseFlag(definition, builder, create);
    ProcessIndexUniqueFlag(definition, builder);
    ProcessIndexDeduplicateFlag(definition, builder);
  }
  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief enhances the json of a persistent index
////////////////////////////////////////////////////////////////////////////////

static int EnhanceJsonIndexPersistent(VPackSlice const definition,
                                      VPackBuilder& builder, bool create) {
  int res = ProcessIndexFields(definition, builder, 0, create);
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
  VPackSlice fieldsSlice = definition.get("fields");
  if (fieldsSlice.isArray() && fieldsSlice.length() == 1) {
    // only add geoJson for indexes with a single field (with needs to be an
    // array)
    bool geoJson =
        basics::VelocyPackHelper::getBooleanValue(definition, "geoJson", false);
    builder.add("geoJson", VPackValue(geoJson));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief enhances the json of a geo1 index
////////////////////////////////////////////////////////////////////////////////

static int EnhanceJsonIndexGeo1(VPackSlice const definition,
                                VPackBuilder& builder, bool create) {
  int res = ProcessIndexFields(definition, builder, 1, create);
  if (res == TRI_ERROR_NO_ERROR) {
    if (ServerState::instance()->isCoordinator()) {
      builder.add("ignoreNull", VPackValue(true));
      builder.add("constraint", VPackValue(false));
    }
    builder.add("sparse", VPackValue(true));
    builder.add("unique", VPackValue(false));
    ProcessIndexGeoJsonFlag(definition, builder);
  }
  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief enhances the json of a geo2 index
////////////////////////////////////////////////////////////////////////////////

static int EnhanceJsonIndexGeo2(VPackSlice const definition,
                                VPackBuilder& builder, bool create) {
  int res = ProcessIndexFields(definition, builder, 2, create);
  if (res == TRI_ERROR_NO_ERROR) {
    if (ServerState::instance()->isCoordinator()) {
      builder.add("ignoreNull", VPackValue(true));
      builder.add("constraint", VPackValue(false));
    }
    builder.add("sparse", VPackValue(true));
    builder.add("unique", VPackValue(false));
    ProcessIndexGeoJsonFlag(definition, builder);
  }
  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief enhances the json of a fulltext index
////////////////////////////////////////////////////////////////////////////////

static int EnhanceJsonIndexFulltext(VPackSlice const definition,
                                    VPackBuilder& builder, bool create) {
  int res = ProcessIndexFields(definition, builder, 1, create);
  if (res == TRI_ERROR_NO_ERROR) {
    // hard-coded defaults
    builder.add("sparse", VPackValue(true));
    builder.add("unique", VPackValue(false));

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

RocksDBIndexFactory::RocksDBIndexFactory() {
  emplaceFactory("edge", [](
    LogicalCollection* collection,
    velocypack::Slice const& definition,
    TRI_idx_iid_t id,
    bool isClusterConstructor
  )->std::shared_ptr<Index> {
    if (!isClusterConstructor) {
      // this indexes cannot be created directly
      THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL, "cannot create edge index"
      );
    }

    auto fields = definition.get("fields");
    TRI_ASSERT(fields.isArray() && fields.length() == 1);
    auto direction = fields.at(0).copyString();
    TRI_ASSERT(
      direction == StaticStrings::FromString
      || direction == StaticStrings::ToString
    );

    return std::make_shared<RocksDBEdgeIndex>(
      id, collection, definition, direction
    );
  });

  emplaceFactory("fulltext", [](
    LogicalCollection* collection,
    velocypack::Slice const& definition,
    TRI_idx_iid_t id,
    bool isClusterConstructor
  )->std::shared_ptr<Index> {
    return std::make_shared<RocksDBFulltextIndex>(id, collection, definition);
  });

  emplaceFactory("geo1", [](
    LogicalCollection* collection,
    velocypack::Slice const& definition,
    TRI_idx_iid_t id,
    bool isClusterConstructor
  )->std::shared_ptr<Index> {
    return std::make_shared<RocksDBGeoIndex>(id, collection, definition);
  });

  emplaceFactory("geo2", [](
    LogicalCollection* collection,
    velocypack::Slice const& definition,
    TRI_idx_iid_t id,
    bool isClusterConstructor
  )->std::shared_ptr<Index> {
    return std::make_shared<RocksDBGeoIndex>(id, collection, definition);
  });

  emplaceFactory("hash", [](
    LogicalCollection* collection,
    velocypack::Slice const& definition,
    TRI_idx_iid_t id,
    bool isClusterConstructor
  )->std::shared_ptr<Index> {
    return std::make_shared<RocksDBHashIndex>(id, collection, definition);
  });

  emplaceFactory("persistent", [](
    LogicalCollection* collection,
    velocypack::Slice const& definition,
    TRI_idx_iid_t id,
    bool isClusterConstructor
  )->std::shared_ptr<Index> {
    return std::make_shared<RocksDBPersistentIndex>(id, collection, definition);
  });

  emplaceFactory("primary", [](
    LogicalCollection* collection,
    velocypack::Slice const& definition,
    TRI_idx_iid_t id,
    bool isClusterConstructor
  )->std::shared_ptr<Index> {
    if (!isClusterConstructor) {
      // this indexes cannot be created directly
      THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL, "cannot create primary index"
      );
    }

    return std::make_shared<RocksDBPrimaryIndex>(collection, definition);
  });

  emplaceFactory("skiplist", [](
    LogicalCollection* collection,
    velocypack::Slice const& definition,
    TRI_idx_iid_t id,
    bool isClusterConstructor
  )->std::shared_ptr<Index> {
    return std::make_shared<RocksDBSkiplistIndex>(id, collection, definition);
  });


  emplaceNormalizer("edge", [](
    velocypack::Builder& normalized,
    velocypack::Slice definition,
    bool isCreation
  )->arangodb::Result {
    if (isCreation) {
      // creating these indexes yourself is forbidden
      return TRI_ERROR_FORBIDDEN;
    }

    TRI_ASSERT(normalized.isOpenObject());
    normalized.add("type", VPackValue(Index::oldtypeName(Index::TRI_IDX_TYPE_EDGE_INDEX)));

    if (isCreation && !ServerState::instance()->isCoordinator() && !definition.hasKey("objectId")) {
      normalized.add("objectId", velocypack::Value(std::to_string(TRI_NewTickServer())));
    }

    return TRI_ERROR_INTERNAL;
  });

  emplaceNormalizer("fulltext", [](
    velocypack::Builder& normalized,
    velocypack::Slice definition,
    bool isCreation
  )->arangodb::Result {
    TRI_ASSERT(normalized.isOpenObject());
    normalized.add("type", VPackValue(Index::oldtypeName(Index::TRI_IDX_TYPE_FULLTEXT_INDEX)));

    if (isCreation && !ServerState::instance()->isCoordinator() && !definition.hasKey("objectId")) {
      normalized.add("objectId", velocypack::Value(std::to_string(TRI_NewTickServer())));
    }

    return EnhanceJsonIndexFulltext(definition, normalized, isCreation);
  });

  emplaceNormalizer("geo", [](
    velocypack::Builder& normalized,
    velocypack::Slice definition,
    bool isCreation
  )->arangodb::Result {
    auto current = definition.get("fields");
    TRI_ASSERT(normalized.isOpenObject());

    if (isCreation && !ServerState::instance()->isCoordinator() && !definition.hasKey("objectId")) {
      normalized.add("objectId", velocypack::Value(std::to_string(TRI_NewTickServer())));
    }

    if (current.isArray() && current.length() == 2) {
      normalized.add("type", VPackValue(Index::oldtypeName(Index::TRI_IDX_TYPE_GEO2_INDEX)));

      return EnhanceJsonIndexGeo2(definition, normalized, isCreation);
    }

    normalized.add("type", VPackValue(Index::oldtypeName(Index::TRI_IDX_TYPE_GEO1_INDEX)));

    return EnhanceJsonIndexGeo1(definition, normalized, isCreation);
  });

  emplaceNormalizer("geo1", [](
    velocypack::Builder& normalized,
    velocypack::Slice definition,
    bool isCreation
  )->arangodb::Result {
    TRI_ASSERT(normalized.isOpenObject());
    normalized.add("type", VPackValue(Index::oldtypeName(Index::TRI_IDX_TYPE_GEO1_INDEX)));

    if (isCreation && !ServerState::instance()->isCoordinator() && !definition.hasKey("objectId")) {
      normalized.add("objectId", velocypack::Value(std::to_string(TRI_NewTickServer())));
    }

    return EnhanceJsonIndexGeo1(definition, normalized, isCreation);
  });

  emplaceNormalizer("geo2", [](
    velocypack::Builder& normalized,
    velocypack::Slice definition,
    bool isCreation
  )->arangodb::Result {
    TRI_ASSERT(normalized.isOpenObject());
    normalized.add("type", VPackValue(Index::oldtypeName(Index::TRI_IDX_TYPE_GEO2_INDEX)));

    if (isCreation && !ServerState::instance()->isCoordinator() && !definition.hasKey("objectId")) {
      normalized.add("objectId", velocypack::Value(std::to_string(TRI_NewTickServer())));
    }

    return EnhanceJsonIndexGeo2(definition, normalized, isCreation);
  });

  emplaceNormalizer("hash", [](
    velocypack::Builder& normalized,
    velocypack::Slice definition,
    bool isCreation
  )->arangodb::Result {
    TRI_ASSERT(normalized.isOpenObject());
    normalized.add("type", VPackValue(Index::oldtypeName(Index::TRI_IDX_TYPE_HASH_INDEX)));

    if (isCreation && !ServerState::instance()->isCoordinator() && !definition.hasKey("objectId")) {
      normalized.add("objectId", velocypack::Value(std::to_string(TRI_NewTickServer())));
    }

    return EnhanceJsonIndexHash(definition, normalized, isCreation);
  });

  emplaceNormalizer("primary", [](
    velocypack::Builder& normalized,
    velocypack::Slice definition,
    bool isCreation
  )->arangodb::Result {
    if (isCreation) {
      // creating these indexes yourself is forbidden
      return TRI_ERROR_FORBIDDEN;
    }

    TRI_ASSERT(normalized.isOpenObject());
    normalized.add("type", VPackValue(Index::oldtypeName(Index::TRI_IDX_TYPE_PRIMARY_INDEX)));

    if (isCreation && !ServerState::instance()->isCoordinator() && !definition.hasKey("objectId")) {
      normalized.add("objectId", velocypack::Value(std::to_string(TRI_NewTickServer())));
    }

    return TRI_ERROR_INTERNAL;
  });

  emplaceNormalizer("persistent", [](
    velocypack::Builder& normalized,
    velocypack::Slice definition,
    bool isCreation
  )->arangodb::Result {
    TRI_ASSERT(normalized.isOpenObject());
    normalized.add("type", VPackValue(Index::oldtypeName(Index::TRI_IDX_TYPE_PERSISTENT_INDEX)));

    if (isCreation && !ServerState::instance()->isCoordinator() && !definition.hasKey("objectId")) {
      normalized.add("objectId", velocypack::Value(std::to_string(TRI_NewTickServer())));
    }

    return EnhanceJsonIndexPersistent(definition, normalized, isCreation);
  });

  emplaceNormalizer("rocksdb", [](
    velocypack::Builder& normalized,
    velocypack::Slice definition,
    bool isCreation
  )->arangodb::Result {
    TRI_ASSERT(normalized.isOpenObject());
    normalized.add("type", VPackValue(Index::oldtypeName(Index::TRI_IDX_TYPE_PERSISTENT_INDEX)));

    if (isCreation && !ServerState::instance()->isCoordinator() && !definition.hasKey("objectId")) {
      normalized.add("objectId", velocypack::Value(std::to_string(TRI_NewTickServer())));
    }

    return EnhanceJsonIndexPersistent(definition, normalized, isCreation);
  });

  emplaceNormalizer("skiplist", [](
    velocypack::Builder& normalized,
    velocypack::Slice definition,
    bool isCreation
  )->arangodb::Result {
    TRI_ASSERT(normalized.isOpenObject());
    normalized.add("type", VPackValue(Index::oldtypeName(Index::TRI_IDX_TYPE_SKIPLIST_INDEX)));

    if (isCreation && !ServerState::instance()->isCoordinator() && !definition.hasKey("objectId")) {
      normalized.add("objectId", velocypack::Value(std::to_string(TRI_NewTickServer())));
    }

    return EnhanceJsonIndexSkiplist(definition, normalized, isCreation);
  });
}

void RocksDBIndexFactory::fillSystemIndexes(
    arangodb::LogicalCollection* col,
    std::vector<std::shared_ptr<arangodb::Index>>& systemIndexes) const {
  // create primary index
  VPackBuilder builder;
  builder.openObject();
  builder.close();

  systemIndexes.emplace_back(
      std::make_shared<arangodb::RocksDBPrimaryIndex>(col, builder.slice()));
  // create edges indexes
  if (col->type() == TRI_COL_TYPE_EDGE) {
    systemIndexes.emplace_back(std::make_shared<arangodb::RocksDBEdgeIndex>(
        1, col, builder.slice(), StaticStrings::FromString));
    systemIndexes.emplace_back(std::make_shared<arangodb::RocksDBEdgeIndex>(
        2, col, builder.slice(), StaticStrings::ToString));
  }
}

std::vector<std::string> RocksDBIndexFactory::supportedIndexes() const {
  return std::vector<std::string>{"primary",    "edge", "hash",    "skiplist",
                                  "persistent", "geo",  "fulltext"};
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------