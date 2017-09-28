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

using namespace arangodb;

////////////////////////////////////////////////////////////////////////////////
/// @brief process the fields list and add them to the json
////////////////////////////////////////////////////////////////////////////////

static int ProcessIndexFields(VPackSlice const definition,
                              VPackBuilder& builder, int numFields,
                              bool create) {
  TRI_ASSERT(builder.isOpenObject());
  std::unordered_set<StringRef> fields;

  try {
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
  } catch (std::bad_alloc const&) {
    return TRI_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return TRI_ERROR_INTERNAL;
  }
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
/// @brief enhances the json of a Persistent index
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
    // only add geoJson for indexes with a single field (with needs to be an array) 
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

////////////////////////////////////////////////////////////////////////////////
/// @brief enhances the json of an index
////////////////////////////////////////////////////////////////////////////////

int MMFilesIndexFactory::enhanceIndexDefinition(VPackSlice const definition,
    VPackBuilder& enhanced, bool create, bool isCoordinator) const {

  // extract index type
  Index::IndexType type = Index::TRI_IDX_TYPE_UNKNOWN;
  VPackSlice current = definition.get("type");
  if (current.isString()) {
    std::string t = current.copyString();

    // rewrite type "geo" into either "geo1" or "geo2", depending on the number
    // of fields
    if (t == "geo") {
      t = "geo1";
      current = definition.get("fields");
      if (current.isArray() && current.length() == 2) {
        t = "geo2";
      }
    }
    type = Index::type(t);
  }

  if (type == Index::TRI_IDX_TYPE_UNKNOWN) {
    return TRI_ERROR_BAD_PARAMETER;
  }

  if (create) {
    if (type == Index::TRI_IDX_TYPE_PRIMARY_INDEX ||
        type == Index::TRI_IDX_TYPE_EDGE_INDEX) {
      // creating these indexes yourself is forbidden
      return TRI_ERROR_FORBIDDEN;
    }
  }

  TRI_ASSERT(enhanced.isEmpty());
  int res = TRI_ERROR_INTERNAL;

  try {
    VPackObjectBuilder b(&enhanced);
    current = definition.get("id");
    uint64_t id = 0; 
    if (current.isNumber()) {
      id = current.getNumericValue<uint64_t>();
    } else if (current.isString()) {
      id = basics::StringUtils::uint64(current.copyString());
    }
    if (id > 0) {
      enhanced.add("id", VPackValue(std::to_string(id)));
    }

    
    enhanced.add("type", VPackValue(Index::oldtypeName(type)));

    switch (type) {
      case Index::TRI_IDX_TYPE_PRIMARY_INDEX:
      case Index::TRI_IDX_TYPE_EDGE_INDEX: {
        break;
      }

      case Index::TRI_IDX_TYPE_GEO1_INDEX:
        res = EnhanceJsonIndexGeo1(definition, enhanced, create);
        break;

      case Index::TRI_IDX_TYPE_GEO2_INDEX:
        res = EnhanceJsonIndexGeo2(definition, enhanced, create);
        break;

      case Index::TRI_IDX_TYPE_HASH_INDEX:
        res = EnhanceJsonIndexHash(definition, enhanced, create);
        break;

      case Index::TRI_IDX_TYPE_SKIPLIST_INDEX:
        res = EnhanceJsonIndexSkiplist(definition, enhanced, create);
        break;
      
      case Index::TRI_IDX_TYPE_PERSISTENT_INDEX:
        res = EnhanceJsonIndexPersistent(definition, enhanced, create);
        break;

      case Index::TRI_IDX_TYPE_FULLTEXT_INDEX:
        res = EnhanceJsonIndexFulltext(definition, enhanced, create);
        break;
      
      case Index::TRI_IDX_TYPE_UNKNOWN: 
      default: {
        res = TRI_ERROR_BAD_PARAMETER;
        break;
      }

    }
  } catch (basics::Exception const& ex) {
    return ex.code();
  } catch (std::exception const&) {
    return TRI_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return TRI_ERROR_INTERNAL;
  }

  return res;
}

// Creates an index object.
// It does not modify anything and does not insert things into
// the index.
// Is also save to use in cluster case.
std::shared_ptr<Index> MMFilesIndexFactory::prepareIndexFromSlice(
    VPackSlice info, bool generateKey, LogicalCollection* col,
    bool isClusterConstructor) const {
  if (!info.isObject()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
  }

  // extract type
  VPackSlice value = info.get("type");

  if (!value.isString()) {
    // Compatibility with old v8-vocindex.
    if (generateKey) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
    } else {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                     "invalid index type definition");
    }
  }

  std::string tmp = value.copyString();
  arangodb::Index::IndexType const type = arangodb::Index::type(tmp.c_str());

  std::shared_ptr<Index> newIdx;

  TRI_idx_iid_t iid = 0;
  value = info.get("id");
  if (value.isString()) {
    iid = basics::StringUtils::uint64(value.copyString());
  } else if (value.isNumber()) {
    iid =
        basics::VelocyPackHelper::getNumericValue<TRI_idx_iid_t>(info, "id", 0);
  } else if (!generateKey) {
    // In the restore case it is forbidden to NOT have id
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL, "cannot restore index without index identifier");
  }

  if (iid == 0 && !isClusterConstructor) {
    // Restore is not allowed to generate in id
    TRI_ASSERT(generateKey);
    iid = arangodb::Index::generateId();
  }

  switch (type) {
    case arangodb::Index::TRI_IDX_TYPE_PRIMARY_INDEX: {
      if (!isClusterConstructor) {
        // this indexes cannot be created directly
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                       "cannot create primary index");
      }
      newIdx.reset(new arangodb::MMFilesPrimaryIndex(col));
      break;
    }
    case arangodb::Index::TRI_IDX_TYPE_EDGE_INDEX: {
      if (!isClusterConstructor) {
        // this indexes cannot be created directly
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                       "cannot create edge index");
      }
      newIdx.reset(new arangodb::MMFilesEdgeIndex(iid, col));
      break;
    }
    case arangodb::Index::TRI_IDX_TYPE_GEO1_INDEX:
    case arangodb::Index::TRI_IDX_TYPE_GEO2_INDEX: {
      newIdx.reset(new arangodb::MMFilesGeoIndex(iid, col, info));
      break;
    }
    case arangodb::Index::TRI_IDX_TYPE_HASH_INDEX: {
      newIdx.reset(new arangodb::MMFilesHashIndex(iid, col, info));
      break;
    }
    case arangodb::Index::TRI_IDX_TYPE_SKIPLIST_INDEX: {
      newIdx.reset(new arangodb::MMFilesSkiplistIndex(iid, col, info));
      break;
    }
    case arangodb::Index::TRI_IDX_TYPE_PERSISTENT_INDEX: {
      newIdx.reset(new arangodb::MMFilesPersistentIndex(iid, col, info));
      break;
    }
    case arangodb::Index::TRI_IDX_TYPE_FULLTEXT_INDEX: {
      newIdx.reset(new arangodb::MMFilesFulltextIndex(iid, col, info));
      break;
    }
    
    case arangodb::Index::TRI_IDX_TYPE_UNKNOWN: 
    default: {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "invalid index type");
    }
  }

  TRI_ASSERT(newIdx != nullptr);
  return newIdx;
}

void MMFilesIndexFactory::fillSystemIndexes(
    arangodb::LogicalCollection* col,
    std::vector<std::shared_ptr<arangodb::Index>>& systemIndexes) const {
  // create primary index
  systemIndexes.emplace_back(
      std::make_shared<arangodb::MMFilesPrimaryIndex>(col));

  // create edges index
  if (col->type() == TRI_COL_TYPE_EDGE) {
    systemIndexes.emplace_back(
        std::make_shared<arangodb::MMFilesEdgeIndex>(1, col));
  }
}
  
std::vector<std::string> MMFilesIndexFactory::supportedIndexes() const {
  return std::vector<std::string>{ "primary", "edge", "hash", "skiplist", "persistent", "geo", "fulltext" };
}
