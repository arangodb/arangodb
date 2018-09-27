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

MMFilesIndexFactory::MMFilesIndexFactory() {
  emplaceFactory("edge", [](
    LogicalCollection& collection,
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

    return std::make_shared<MMFilesEdgeIndex>(id, collection);
  });

  emplaceFactory("fulltext", [](
    LogicalCollection& collection,
    velocypack::Slice const& definition,
    TRI_idx_iid_t id,
    bool isClusterConstructor
  )->std::shared_ptr<Index> {
    return std::make_shared<MMFilesFulltextIndex>(id, collection, definition);
  });

  emplaceFactory("geo1", [](
    LogicalCollection& collection,
    velocypack::Slice const& definition,
    TRI_idx_iid_t id,
    bool isClusterConstructor
  )->std::shared_ptr<Index> {
    return std::make_shared<MMFilesGeoIndex>(id, collection, definition, "geo1");
  });

  emplaceFactory("geo2", [](
    LogicalCollection& collection,
    velocypack::Slice const& definition,
    TRI_idx_iid_t id,
    bool isClusterConstructor
  )->std::shared_ptr<Index> {
    return std::make_shared<MMFilesGeoIndex>(id, collection, definition, "geo2");
  });

  emplaceFactory("geo", [](
    LogicalCollection& collection,
    velocypack::Slice const& definition,
    TRI_idx_iid_t id,
    bool isClusterConstructor
    )->std::shared_ptr<Index> {
    return std::make_shared<MMFilesGeoIndex>(id, collection, definition, "geo");
  });

  emplaceFactory("hash", [](
    LogicalCollection& collection,
    velocypack::Slice const& definition,
    TRI_idx_iid_t id,
    bool isClusterConstructor
  )->std::shared_ptr<Index> {
    return std::make_shared<MMFilesHashIndex>(id, collection, definition);
  });

  emplaceFactory("persistent", [](
    LogicalCollection& collection,
    velocypack::Slice const& definition,
    TRI_idx_iid_t id,
    bool isClusterConstructor
  )->std::shared_ptr<Index> {
    return std::make_shared<MMFilesPersistentIndex>(id, collection, definition);
  });

  emplaceFactory("primary", [](
    LogicalCollection& collection,
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

    return std::make_shared<MMFilesPrimaryIndex>(collection);
  });

  emplaceFactory("skiplist", [](
    LogicalCollection& collection,
    velocypack::Slice const& definition,
    TRI_idx_iid_t id,
    bool isClusterConstructor
  )->std::shared_ptr<Index> {
    return std::make_shared<MMFilesSkiplistIndex>(id, collection, definition);
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
    normalized.add(
      arangodb::StaticStrings::IndexType,
      arangodb::velocypack::Value(
        Index::oldtypeName(Index::TRI_IDX_TYPE_EDGE_INDEX)
      )
    );

    return TRI_ERROR_INTERNAL;
  });

  emplaceNormalizer("fulltext", [](
    velocypack::Builder& normalized,
    velocypack::Slice definition,
    bool isCreation
  )->arangodb::Result {
    TRI_ASSERT(normalized.isOpenObject());
    normalized.add(
      arangodb::StaticStrings::IndexType,
      arangodb::velocypack::Value(
        Index::oldtypeName(Index::TRI_IDX_TYPE_FULLTEXT_INDEX)
      )
    );

    return EnhanceJsonIndexFulltext(definition, normalized, isCreation);
  });

  emplaceNormalizer("geo", [](
    velocypack::Builder& normalized,
    velocypack::Slice definition,
    bool isCreation
  )->arangodb::Result {
    TRI_ASSERT(normalized.isOpenObject());
    normalized.add(
      arangodb::StaticStrings::IndexType,
      arangodb::velocypack::Value(
        Index::oldtypeName(Index::TRI_IDX_TYPE_GEO_INDEX)
      )
    );

    return EnhanceJsonIndexGeo(definition, normalized, isCreation);
  });

  emplaceNormalizer("geo1", [](
    velocypack::Builder& normalized,
    velocypack::Slice definition,
    bool isCreation
  )->arangodb::Result {
    TRI_ASSERT(normalized.isOpenObject());
    normalized.add(
      arangodb::StaticStrings::IndexType,
      arangodb::velocypack::Value(
        Index::oldtypeName(Index::TRI_IDX_TYPE_GEO_INDEX)
      )
    );

    return EnhanceJsonIndexGeo1(definition, normalized, isCreation);
  });

  emplaceNormalizer("geo2", [](
    velocypack::Builder& normalized,
    velocypack::Slice definition,
    bool isCreation
  )->arangodb::Result {
    TRI_ASSERT(normalized.isOpenObject());
    normalized.add(
      arangodb::StaticStrings::IndexType,
      arangodb::velocypack::Value(
        Index::oldtypeName(Index::TRI_IDX_TYPE_GEO_INDEX)
      )
    );

    return EnhanceJsonIndexGeo2(definition, normalized, isCreation);
  });

  emplaceNormalizer("hash", [](
    velocypack::Builder& normalized,
    velocypack::Slice definition,
    bool isCreation
  )->arangodb::Result {
    TRI_ASSERT(normalized.isOpenObject());
    normalized.add(
      arangodb::StaticStrings::IndexType,
      arangodb::velocypack::Value(
        Index::oldtypeName(Index::TRI_IDX_TYPE_HASH_INDEX)
      )
    );

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
    normalized.add(
      arangodb::StaticStrings::IndexType,
      arangodb::velocypack::Value(
        Index::oldtypeName(Index::TRI_IDX_TYPE_PRIMARY_INDEX)
      )
    );

    return TRI_ERROR_INTERNAL;
  });

  emplaceNormalizer("persistent", [](
    velocypack::Builder& normalized,
    velocypack::Slice definition,
    bool isCreation
  )->arangodb::Result {
    TRI_ASSERT(normalized.isOpenObject());
    normalized.add(
      arangodb::StaticStrings::IndexType,
      arangodb::velocypack::Value(
        Index::oldtypeName(Index::TRI_IDX_TYPE_PERSISTENT_INDEX)
      )
    );

    return EnhanceJsonIndexPersistent(definition, normalized, isCreation);
  });

  emplaceNormalizer("rocksdb", [](
    velocypack::Builder& normalized,
    velocypack::Slice definition,
    bool isCreation
  )->arangodb::Result {
    TRI_ASSERT(normalized.isOpenObject());
    normalized.add(
      arangodb::StaticStrings::IndexType,
      arangodb::velocypack::Value(
        Index::oldtypeName(Index::TRI_IDX_TYPE_PERSISTENT_INDEX)
      )
    );

    return EnhanceJsonIndexPersistent(definition, normalized, isCreation);
  });

  emplaceNormalizer("skiplist", [](
    velocypack::Builder& normalized,
    velocypack::Slice definition,
    bool isCreation
  )->arangodb::Result {
    TRI_ASSERT(normalized.isOpenObject());
    normalized.add(
      arangodb::StaticStrings::IndexType,
      arangodb::velocypack::Value(
        Index::oldtypeName(Index::TRI_IDX_TYPE_SKIPLIST_INDEX)
      )
    );

    return EnhanceJsonIndexSkiplist(definition, normalized, isCreation);
  });
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