////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "ClusterIndex.h"
#include "Basics/StaticStrings.h"
#include "Basics/VelocyPackHelper.h"
#include "ClusterEngine/ClusterEngine.h"
#include "Indexes/PersistentIndexAttributeMatcher.h"
#include "Indexes/SimpleAttributeEqualityMatcher.h"
#include "Indexes/SkiplistIndexAttributeMatcher.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ticks.h"

#include <velocypack/Collection.h>
#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using Helper = arangodb::basics::VelocyPackHelper;

namespace {
/// @brief hard-coded vector of the index attributes
/// note that the attribute names must be hard-coded here to avoid an init-order
/// fiasco with StaticStrings::FromString etc.

// The primary indexes do not have `_id` in the _fields instance variable
std::vector<std::vector<arangodb::basics::AttributeName>> const PrimaryIndexAttributes{
    {arangodb::basics::AttributeName("_id", false)},
    {arangodb::basics::AttributeName("_key", false)}};

};  // namespace

ClusterIndex::ClusterIndex(TRI_idx_iid_t id, LogicalCollection& collection,
                           ClusterEngineType engineType, Index::IndexType itype,
                           arangodb::velocypack::Slice const& info)
    : Index(id, collection, info),
      _engineType(engineType),
      _indexType(itype),
      _info(info),
      _clusterSelectivity(/* default */ 0.1) {
  TRI_ASSERT(_info.slice().isObject());
  TRI_ASSERT(_info.isClosed());

  // The Edge Index on RocksDB can serve _from and _to when beeing asked.
  if (_engineType == ClusterEngineType::RocksDBEngine && _indexType == TRI_IDX_TYPE_EDGE_INDEX) {
    std::string attr = "";
    TRI_AttributeNamesToString(_fields[0], attr);
    if (attr == StaticStrings::FromString) {
      _coveredFields = {{arangodb::basics::AttributeName{StaticStrings::FromString, false}},
                        {arangodb::basics::AttributeName{StaticStrings::ToString, false}}};
    } else {
      TRI_ASSERT(attr == StaticStrings::ToString);
      _coveredFields = {{arangodb::basics::AttributeName{StaticStrings::ToString, false}},
                        {arangodb::basics::AttributeName{StaticStrings::FromString, false}}};
    }
  }
}

ClusterIndex::~ClusterIndex() {}

void ClusterIndex::toVelocyPackFigures(VPackBuilder& builder) const {
  TRI_ASSERT(builder.isOpenObject());
  Index::toVelocyPackFigures(builder);
}

// ========== below is cluster schmutz ============

/// @brief return a VelocyPack representation of the index
void ClusterIndex::toVelocyPack(VPackBuilder& builder,
                                std::underlying_type<Index::Serialize>::type flags) const {
  builder.openObject();
  Index::toVelocyPack(builder, flags);
  builder.add(StaticStrings::IndexUnique, VPackValue(_unique));
  builder.add(StaticStrings::IndexSparse, VPackValue(_sparse));

  // static std::vector forbidden = {};
  for (auto pair : VPackObjectIterator(_info.slice())) {
    if (!pair.key.isEqualString(StaticStrings::IndexId) &&
        !pair.key.isEqualString(StaticStrings::IndexType) &&
        !pair.key.isEqualString(StaticStrings::IndexFields) &&
        !pair.key.isEqualString("selectivityEstimate") && !pair.key.isEqualString("figures") &&
        !pair.key.isEqualString(StaticStrings::IndexUnique) &&
        !pair.key.isEqualString(StaticStrings::IndexSparse)) {
      builder.add(pair.key);
      builder.add(pair.value);
    }
  }
  builder.close();
}

bool ClusterIndex::hasSelectivityEstimate() const {
  if (_engineType == ClusterEngineType::MMFilesEngine) {
    return _indexType == Index::TRI_IDX_TYPE_PRIMARY_INDEX ||
           _indexType == Index::TRI_IDX_TYPE_EDGE_INDEX ||
           _indexType == Index::TRI_IDX_TYPE_HASH_INDEX;
  } else if (_engineType == ClusterEngineType::RocksDBEngine) {
    return _indexType == Index::TRI_IDX_TYPE_PRIMARY_INDEX ||
           _indexType == Index::TRI_IDX_TYPE_EDGE_INDEX ||
           _indexType == Index::TRI_IDX_TYPE_HASH_INDEX ||
           _indexType == Index::TRI_IDX_TYPE_SKIPLIST_INDEX ||
           _indexType == Index::TRI_IDX_TYPE_PERSISTENT_INDEX;
  } else if (_engineType == ClusterEngineType::MockEngine) {
    return false;
  }
  TRI_ASSERT(false);
  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                 "unsupported cluster storage engine");
  return true;
}

/// @brief default implementation for selectivityEstimate
double ClusterIndex::selectivityEstimate(StringRef const&) const {
  TRI_ASSERT(hasSelectivityEstimate());
  if (_unique) {
    return 1.0;
  }

  // floating-point tolerance
  TRI_ASSERT(_clusterSelectivity >= 0.0 && _clusterSelectivity <= 1.00001);
  return _clusterSelectivity;
}

void ClusterIndex::updateClusterSelectivityEstimate(double estimate) {
  _clusterSelectivity = estimate;
}

bool ClusterIndex::isSorted() const {
  if (_engineType == ClusterEngineType::MMFilesEngine) {
    return _indexType == Index::TRI_IDX_TYPE_SKIPLIST_INDEX ||
           _indexType == Index::TRI_IDX_TYPE_PERSISTENT_INDEX;
  } else if (_engineType == ClusterEngineType::RocksDBEngine) {
    return _indexType == Index::TRI_IDX_TYPE_EDGE_INDEX ||
           _indexType == Index::TRI_IDX_TYPE_HASH_INDEX ||
           _indexType == Index::TRI_IDX_TYPE_SKIPLIST_INDEX ||
           _indexType == Index::TRI_IDX_TYPE_PERSISTENT_INDEX ||
           _indexType == Index::TRI_IDX_TYPE_FULLTEXT_INDEX;
  } else if (_engineType == ClusterEngineType::MockEngine) {
    return false;
  }
  TRI_ASSERT(false);
  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                 "unsupported cluster storage engine");
}

void ClusterIndex::updateProperties(velocypack::Slice const& slice) {
  VPackBuilder merge;
  merge.openObject();

  if (_engineType == ClusterEngineType::MMFilesEngine) {
    // nothing to update here
  } else if (_engineType == ClusterEngineType::RocksDBEngine) {
    merge.add("cacheEnabled",
              VPackValue(Helper::readBooleanValue(slice, "cacheEnabled", false)));

  } else {
    TRI_ASSERT(false);
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "unsupported cluster storage engine");
  }

  merge.close();
  TRI_ASSERT(merge.slice().isObject());
  TRI_ASSERT(_info.slice().isObject());
  VPackBuilder tmp = VPackCollection::merge(_info.slice(), merge.slice(), true);
  _info = std::move(tmp);
  TRI_ASSERT(_info.slice().isObject());
  TRI_ASSERT(_info.isClosed());
}

bool ClusterIndex::hasCoveringIterator() const {
  if (_engineType == ClusterEngineType::RocksDBEngine) {
    return _indexType == Index::TRI_IDX_TYPE_PRIMARY_INDEX ||
           _indexType == Index::TRI_IDX_TYPE_EDGE_INDEX ||
           _indexType == Index::TRI_IDX_TYPE_HASH_INDEX ||
           _indexType == Index::TRI_IDX_TYPE_SKIPLIST_INDEX ||
           _indexType == Index::TRI_IDX_TYPE_PERSISTENT_INDEX;
  }
  return false;
}

bool ClusterIndex::matchesDefinition(VPackSlice const& info) const {
  // TODO implement faster version of this
  return Index::Compare(_info.slice(), info);
}

bool ClusterIndex::supportsFilterCondition(
    std::vector<std::shared_ptr<arangodb::Index>> const& allIndexes,
    arangodb::aql::AstNode const* node, arangodb::aql::Variable const* reference,
    size_t itemsInIndex, size_t& estimatedItems, double& estimatedCost) const {
  switch (_indexType) {
    case TRI_IDX_TYPE_PRIMARY_INDEX: {
      SimpleAttributeEqualityMatcher matcher(PrimaryIndexAttributes);
      return matcher.matchOne(this, node, reference, itemsInIndex, estimatedItems, estimatedCost);
    }
    case TRI_IDX_TYPE_GEO_INDEX:
    case TRI_IDX_TYPE_GEO1_INDEX:
    case TRI_IDX_TYPE_GEO2_INDEX:
    case TRI_IDX_TYPE_FULLTEXT_INDEX:
#ifdef USE_IRESEARCH
    case TRI_IDX_TYPE_IRESEARCH_LINK:
#endif
    case TRI_IDX_TYPE_NO_ACCESS_INDEX: {
      // should not be called for these indexes
      return Index::supportsFilterCondition(allIndexes, node, reference, itemsInIndex,
                                            estimatedItems, estimatedCost);
    }
    case TRI_IDX_TYPE_HASH_INDEX: {
      if (_engineType == ClusterEngineType::MMFilesEngine) {
        SimpleAttributeEqualityMatcher matcher(this->_fields);
        return matcher.matchAll(this, node, reference, itemsInIndex,
                                estimatedItems, estimatedCost);
      } else if (_engineType == ClusterEngineType::RocksDBEngine) {
        return SkiplistIndexAttributeMatcher::supportsFilterCondition(
            allIndexes, this, node, reference, itemsInIndex, estimatedItems, estimatedCost);
      }
      break;
    }
    case TRI_IDX_TYPE_EDGE_INDEX: {
      // same for both engines
      SimpleAttributeEqualityMatcher matcher(this->_fields);
      return matcher.matchOne(this, node, reference, itemsInIndex, estimatedItems, estimatedCost);
    }

    case TRI_IDX_TYPE_SKIPLIST_INDEX: {
      if (_engineType == ClusterEngineType::MMFilesEngine) {
        return SkiplistIndexAttributeMatcher::supportsFilterCondition(
            allIndexes, this, node, reference, itemsInIndex, estimatedItems, estimatedCost);
      } else if (_engineType == ClusterEngineType::RocksDBEngine) {
        return SkiplistIndexAttributeMatcher::supportsFilterCondition(
            allIndexes, this, node, reference, itemsInIndex, estimatedItems, estimatedCost);
      }
      break;
    }
    case TRI_IDX_TYPE_PERSISTENT_INDEX: {
      // same for both engines
      return SkiplistIndexAttributeMatcher::supportsFilterCondition(
          allIndexes, this, node, reference, itemsInIndex, estimatedItems, estimatedCost);
    }

    case TRI_IDX_TYPE_UNKNOWN:
      break;
  }

  if (_engineType == ClusterEngineType::MockEngine) {
    return false;
  }
  TRI_ASSERT(false);
  return false;
}

bool ClusterIndex::supportsSortCondition(arangodb::aql::SortCondition const* sortCondition,
                                         arangodb::aql::Variable const* reference,
                                         size_t itemsInIndex, double& estimatedCost,
                                         size_t& coveredAttributes) const {
  switch (_indexType) {
    case TRI_IDX_TYPE_PRIMARY_INDEX:
    case TRI_IDX_TYPE_GEO_INDEX:
    case TRI_IDX_TYPE_GEO1_INDEX:
    case TRI_IDX_TYPE_GEO2_INDEX:
    case TRI_IDX_TYPE_FULLTEXT_INDEX:
#ifdef USE_IRESEARCH
    case TRI_IDX_TYPE_IRESEARCH_LINK:
#endif
    case TRI_IDX_TYPE_NO_ACCESS_INDEX: {
      return Index::supportsSortCondition(sortCondition, reference, itemsInIndex,
                                          estimatedCost, coveredAttributes);
    }
    case TRI_IDX_TYPE_HASH_INDEX: {
      if (_engineType == ClusterEngineType::MMFilesEngine) {
        // does not support sorting
        return Index::supportsSortCondition(sortCondition, reference, itemsInIndex,
                                            estimatedCost, coveredAttributes);
      } else if (_engineType == ClusterEngineType::RocksDBEngine) {
        return PersistentIndexAttributeMatcher::supportsSortCondition(
            this, sortCondition, reference, itemsInIndex, estimatedCost, coveredAttributes);
      }
      break;
    }
    case TRI_IDX_TYPE_EDGE_INDEX: {
      return Index::supportsSortCondition(sortCondition, reference, itemsInIndex,
                                          estimatedCost, coveredAttributes);
    }

    case TRI_IDX_TYPE_SKIPLIST_INDEX: {
      if (_engineType == ClusterEngineType::MMFilesEngine) {
        return SkiplistIndexAttributeMatcher::supportsSortCondition(
            this, sortCondition, reference, itemsInIndex, estimatedCost, coveredAttributes);
      } else if (_engineType == ClusterEngineType::RocksDBEngine) {
        return PersistentIndexAttributeMatcher::supportsSortCondition(
            this, sortCondition, reference, itemsInIndex, estimatedCost, coveredAttributes);
      }
      break;
    }
    case TRI_IDX_TYPE_PERSISTENT_INDEX: {
      // same for both indexes
      return PersistentIndexAttributeMatcher::supportsSortCondition(
          this, sortCondition, reference, itemsInIndex, estimatedCost, coveredAttributes);
    }

    case TRI_IDX_TYPE_UNKNOWN:
      break;
  }

  if (_engineType == ClusterEngineType::MockEngine) {
    return false;
  }
  TRI_ASSERT(false);
  return false;
}

/// @brief specializes the condition for use with the index
aql::AstNode* ClusterIndex::specializeCondition(aql::AstNode* node,
                                                aql::Variable const* reference) const {
  switch (_indexType) {
    case TRI_IDX_TYPE_PRIMARY_INDEX: {
      SimpleAttributeEqualityMatcher matcher(PrimaryIndexAttributes);
      return matcher.specializeOne(this, node, reference);
    }
    // should not be called for these
    case TRI_IDX_TYPE_GEO_INDEX:
    case TRI_IDX_TYPE_GEO1_INDEX:
    case TRI_IDX_TYPE_GEO2_INDEX:
    case TRI_IDX_TYPE_FULLTEXT_INDEX:
#ifdef USE_IRESEARCH
    case TRI_IDX_TYPE_IRESEARCH_LINK:
#endif
    case TRI_IDX_TYPE_NO_ACCESS_INDEX: {
      return Index::specializeCondition(node, reference);  // unsupported
    }
    case TRI_IDX_TYPE_HASH_INDEX:
      if (_engineType == ClusterEngineType::MMFilesEngine) {
        SimpleAttributeEqualityMatcher matcher(this->_fields);
        return matcher.specializeAll(this, node, reference);
      } else if (_engineType == ClusterEngineType::RocksDBEngine) {
        return SkiplistIndexAttributeMatcher::specializeCondition(this, node, reference);
      }
      break;

    case TRI_IDX_TYPE_EDGE_INDEX: {
      // same for both engines
      SimpleAttributeEqualityMatcher matcher(this->_fields);
      return matcher.specializeOne(this, node, reference);
    }

    case TRI_IDX_TYPE_SKIPLIST_INDEX: {
      if (_engineType == ClusterEngineType::MMFilesEngine) {
        return SkiplistIndexAttributeMatcher::specializeCondition(this, node, reference);
      } else if (_engineType == ClusterEngineType::RocksDBEngine) {
        return SkiplistIndexAttributeMatcher::specializeCondition(this, node, reference);
      }
      break;
    }
    case TRI_IDX_TYPE_PERSISTENT_INDEX: {
      return SkiplistIndexAttributeMatcher::specializeCondition(this, node, reference);
    }

    case TRI_IDX_TYPE_UNKNOWN:
      break;
  }

  if (_engineType == ClusterEngineType::MockEngine) {
    return node;
  }
  TRI_ASSERT(false);
  return node;
}

std::vector<std::vector<arangodb::basics::AttributeName>> const& ClusterIndex::coveredFields() const {
  if (_engineType == ClusterEngineType::RocksDBEngine && _indexType == TRI_IDX_TYPE_EDGE_INDEX) {
    TRI_ASSERT(_coveredFields.size() == 2);
    return _coveredFields;
  }
  return _fields;
}
