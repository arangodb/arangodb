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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/StaticStrings.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/AgencyCache.h"
#include "Cluster/ClusterFeature.h"
#include "ClusterEngine/ClusterEngine.h"
#include "ClusterIndex.h"
#include "Indexes/SimpleAttributeEqualityMatcher.h"
#include "Indexes/SortedIndexAttributeMatcher.h"
#include "RocksDBEngine/RocksDBMultiDimIndex.h"
#include "RocksDBEngine/RocksDBPrimaryIndex.h"
#include "RocksDBEngine/RocksDBVPackIndex.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ticks.h"

#include <velocypack/Collection.h>
#include <velocypack/Iterator.h>

using namespace arangodb;
using namespace arangodb::futures;

ClusterIndex::ClusterIndex(IndexId id, LogicalCollection& collection,
                           ClusterEngineType engineType, Index::IndexType itype,
                           velocypack::Slice info)
    : Index(id, collection, info),
      _engineType(engineType),
      _indexType(itype),
      _info(info),
      _estimates(true),
      _clusterSelectivity(/* default */ 0.1) {
  TRI_ASSERT(_info.slice().isObject());
  TRI_ASSERT(_info.isClosed());
  TRI_ASSERT(_engineType == ClusterEngineType::RocksDBEngine ||
             _engineType == ClusterEngineType::MockEngine);
#ifndef ARANGODB_USE_GOOGLE_TESTS
  TRI_ASSERT(_engineType == ClusterEngineType::RocksDBEngine);
#endif

  if (_engineType == ClusterEngineType::RocksDBEngine) {
    if (_indexType == TRI_IDX_TYPE_EDGE_INDEX) {
      // The Edge Index on RocksDB can serve _from and _to when being asked.
      std::string attr = "";
      TRI_AttributeNamesToString(_fields[0], attr);
      if (attr == StaticStrings::FromString) {
        _coveredFields = {
            {basics::AttributeName{StaticStrings::FromString, false}},
            {basics::AttributeName{StaticStrings::ToString, false}}};
      } else {
        TRI_ASSERT(attr == StaticStrings::ToString);
        _coveredFields = {
            {basics::AttributeName{StaticStrings::ToString, false}},
            {basics::AttributeName{StaticStrings::FromString, false}}};
      }
    } else if (_indexType == TRI_IDX_TYPE_PRIMARY_INDEX) {
      // The Primary Index on RocksDB can serve _key and _id when being asked.
      _coveredFields = {
          {basics::AttributeName(StaticStrings::KeyString, false)},
          {basics::AttributeName(StaticStrings::IdString, false)}};
    } else if (_indexType == TRI_IDX_TYPE_PERSISTENT_INDEX) {
      _coveredFields = Index::mergeFields(
          _fields,
          Index::parseFields(info.get(StaticStrings::IndexStoredValues),
                             /*allowEmpty*/ true, /*allowExpansion*/ false));
    } else if (_indexType == TRI_IDX_TYPE_MDI_INDEX ||
               _indexType == TRI_IDX_TYPE_ZKD_INDEX) {
      _coveredFields =
          Index::parseFields(info.get(StaticStrings::IndexStoredValues),
                             /*allowEmpty*/ true, /*allowExpansion*/ false);
    } else if (_indexType == TRI_IDX_TYPE_MDI_PREFIXED_INDEX) {
      _prefixFields =
          Index::parseFields(info.get(StaticStrings::IndexPrefixFields),
                             /*allowEmpty*/ true, /*allowExpansion*/ false);
      _coveredFields = Index::mergeFields(
          _prefixFields,
          Index::parseFields(info.get(StaticStrings::IndexStoredValues),
                             /*allowEmpty*/ true, /*allowExpansion*/ false));
    }

    // check for "estimates" attribute
    if (_unique) {
      _estimates = true;
    } else if (_indexType == TRI_IDX_TYPE_HASH_INDEX ||
               _indexType == TRI_IDX_TYPE_SKIPLIST_INDEX ||
               _indexType == TRI_IDX_TYPE_PERSISTENT_INDEX ||
               _indexType == TRI_IDX_TYPE_MDI_PREFIXED_INDEX) {
      if (VPackSlice s = info.get(StaticStrings::IndexEstimates);
          s.isBoolean()) {
        _estimates = s.getBoolean();
      }
    } else if (_indexType == TRI_IDX_TYPE_TTL_INDEX ||
               _indexType == TRI_IDX_TYPE_MDI_INDEX ||
               _indexType == TRI_IDX_TYPE_ZKD_INDEX) {
      _estimates = false;
    }
  }
}

ClusterIndex::~ClusterIndex() = default;

void ClusterIndex::toVelocyPackFigures(VPackBuilder& builder) const {
  TRI_ASSERT(builder.isOpenObject());
  Index::toVelocyPackFigures(builder);
}

/// @brief return a VelocyPack representation of the index
void ClusterIndex::toVelocyPack(
    VPackBuilder& builder,
    std::underlying_type<Index::Serialize>::type flags) const {
  builder.openObject();
  Index::toVelocyPack(builder, flags);
  builder.add(StaticStrings::IndexUnique, VPackValue(_unique));
  builder.add(StaticStrings::IndexSparse, VPackValue(_sparse));

  if (_indexType == Index::TRI_IDX_TYPE_HASH_INDEX ||
      _indexType == Index::TRI_IDX_TYPE_SKIPLIST_INDEX ||
      _indexType == Index::TRI_IDX_TYPE_PERSISTENT_INDEX ||
      _indexType == Index::TRI_IDX_TYPE_MDI_PREFIXED_INDEX ||
      _indexType == Index::TRI_IDX_TYPE_MDI_INDEX ||
      _indexType == Index::TRI_IDX_TYPE_ZKD_INDEX) {
    TRI_ASSERT((_indexType != TRI_IDX_TYPE_MDI_INDEX &&
                _indexType != TRI_IDX_TYPE_ZKD_INDEX) ||
               !_estimates || _unique)
        << oldtypeName(_indexType) << std::boolalpha
        << " estimates = " << _estimates << " unique = " << _unique;
    builder.add(StaticStrings::IndexEstimates, VPackValue(_estimates));
  } else if (_indexType == Index::TRI_IDX_TYPE_TTL_INDEX) {
    // no estimates for the ttl index
    builder.add(StaticStrings::IndexEstimates, VPackValue(false));
  }

  if (_indexType == Index::TRI_IDX_TYPE_MDI_PREFIXED_INDEX) {
    builder.add(arangodb::velocypack::Value(StaticStrings::IndexPrefixFields));
    builder.openArray();

    for (auto const& field : _prefixFields) {
      std::string fieldString;
      TRI_AttributeNamesToString(field, fieldString);
      builder.add(VPackValue(fieldString));
    }

    builder.close();
  }

  for (auto pair : VPackObjectIterator(_info.slice())) {
    if (!pair.key.isEqualString(StaticStrings::IndexId) &&
        !pair.key.isEqualString(StaticStrings::IndexName) &&
        !pair.key.isEqualString(StaticStrings::IndexType) &&
        !pair.key.isEqualString(StaticStrings::IndexFields) &&
        !pair.key.isEqualString("selectivityEstimate") &&
        !pair.key.isEqualString("figures") &&
        !pair.key.isEqualString(StaticStrings::IndexUnique) &&
        !pair.key.isEqualString(StaticStrings::IndexSparse) &&
        !pair.key.isEqualString(StaticStrings::IndexEstimates)) {
      builder.add(pair.key);
      builder.add(pair.value);
    }
  }
  builder.close();
}

bool ClusterIndex::hasSelectivityEstimate() const {
  if (_engineType == ClusterEngineType::RocksDBEngine) {
    return _indexType == Index::TRI_IDX_TYPE_PRIMARY_INDEX ||
           _indexType == Index::TRI_IDX_TYPE_EDGE_INDEX ||
           _indexType == Index::TRI_IDX_TYPE_TTL_INDEX ||
           (_estimates &&
            (_indexType == Index::TRI_IDX_TYPE_HASH_INDEX ||
             _indexType == Index::TRI_IDX_TYPE_SKIPLIST_INDEX ||
             _indexType == Index::TRI_IDX_TYPE_PERSISTENT_INDEX ||
             _indexType == Index::TRI_IDX_TYPE_MDI_PREFIXED_INDEX ||
             ((_indexType == Index::TRI_IDX_TYPE_MDI_INDEX ||
               _indexType == Index::TRI_IDX_TYPE_ZKD_INDEX) &&
              _unique)));
#ifdef ARANGODB_USE_GOOGLE_TESTS
  } else if (_engineType == ClusterEngineType::MockEngine) {
    return false;
#endif
  }
  TRI_ASSERT(false);
  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                 "unsupported cluster storage engine");
}

/// @brief default implementation for selectivityEstimate
double ClusterIndex::selectivityEstimate(std::string_view) const {
  TRI_ASSERT(hasSelectivityEstimate());
  if (_unique) {
    return 1.0;
  }
  if (!_estimates) {
    return 0.0;
  }

  // floating-point tolerance
  TRI_ASSERT(_clusterSelectivity >= 0.0 && _clusterSelectivity <= 1.00001);
  return _clusterSelectivity.load(std::memory_order_relaxed);
}

void ClusterIndex::updateClusterSelectivityEstimate(double estimate) {
  _clusterSelectivity.store(estimate, std::memory_order_relaxed);
}

bool ClusterIndex::isSorted() const {
  if (_engineType == ClusterEngineType::RocksDBEngine) {
    return _indexType == Index::TRI_IDX_TYPE_PRIMARY_INDEX ||
           _indexType == Index::TRI_IDX_TYPE_HASH_INDEX ||
           _indexType == Index::TRI_IDX_TYPE_SKIPLIST_INDEX ||
           _indexType == Index::TRI_IDX_TYPE_PERSISTENT_INDEX ||
           _indexType == Index::TRI_IDX_TYPE_TTL_INDEX;
#ifdef ARANGODB_USE_GOOGLE_TESTS
  } else if (_engineType == ClusterEngineType::MockEngine) {
    return false;
#endif
  }
  TRI_ASSERT(false);
  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                 "unsupported cluster storage engine");
}

void ClusterIndex::updateProperties(velocypack::Slice slice) {
  VPackBuilder merge;
  merge.openObject();

  TRI_ASSERT(_engineType == ClusterEngineType::RocksDBEngine);

  if (_engineType == ClusterEngineType::RocksDBEngine) {
    merge.add(StaticStrings::CacheEnabled,
              VPackValue(basics::VelocyPackHelper::getBooleanValue(
                  slice, StaticStrings::CacheEnabled, false)));

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

bool ClusterIndex::matchesDefinition(VPackSlice const& info) const {
  // TODO implement faster version of this
  auto& engine = _collection.vocbase().engine();
  return Index::compare(engine, _info.slice(), info,
                        _collection.vocbase().name());
}

Index::FilterCosts ClusterIndex::supportsFilterCondition(
    transaction::Methods& trx,
    std::vector<std::shared_ptr<Index>> const& allIndexes,
    aql::AstNode const* node, aql::Variable const* reference,
    size_t itemsInIndex) const {
  switch (_indexType) {
    case TRI_IDX_TYPE_PRIMARY_INDEX: {
      if (_engineType == ClusterEngineType::RocksDBEngine) {
        return SortedIndexAttributeMatcher::supportsFilterCondition(
            allIndexes, this, node, reference, itemsInIndex);
      }
      // other...
      std::vector<std::vector<basics::AttributeName>> fields{
          {basics::AttributeName(StaticStrings::KeyString, false)},
          {basics::AttributeName(StaticStrings::IdString, false)}};
      SimpleAttributeEqualityMatcher matcher(fields);
      return matcher.matchOne(this, node, reference, itemsInIndex);
    }
    case TRI_IDX_TYPE_EDGE_INDEX: {
      if (_engineType == ClusterEngineType::RocksDBEngine) {
        SimpleAttributeEqualityMatcher matcher(this->_fields);
        return matcher.matchOne(this, node, reference, itemsInIndex);
      }
      // other...
      SimpleAttributeEqualityMatcher matcher(this->_fields);
      return matcher.matchOne(this, node, reference, itemsInIndex);
    }
    case TRI_IDX_TYPE_HASH_INDEX: {
      if (_engineType == ClusterEngineType::RocksDBEngine) {
        return SortedIndexAttributeMatcher::supportsFilterCondition(
            allIndexes, this, node, reference, itemsInIndex);
      }
      break;
    }

    case TRI_IDX_TYPE_SKIPLIST_INDEX:
    case TRI_IDX_TYPE_TTL_INDEX:
    case TRI_IDX_TYPE_PERSISTENT_INDEX: {
      // same for both engines
      return SortedIndexAttributeMatcher::supportsFilterCondition(
          allIndexes, this, node, reference, itemsInIndex);
    }
    case TRI_IDX_TYPE_GEO_INDEX:
    case TRI_IDX_TYPE_GEO1_INDEX:
    case TRI_IDX_TYPE_GEO2_INDEX:
    case TRI_IDX_TYPE_FULLTEXT_INDEX:
    case TRI_IDX_TYPE_INVERTED_INDEX:
    case TRI_IDX_TYPE_IRESEARCH_LINK:
    case TRI_IDX_TYPE_NO_ACCESS_INDEX: {
      // should not be called for these indexes
      return Index::supportsFilterCondition(trx, allIndexes, node, reference,
                                            itemsInIndex);
    }

    case TRI_IDX_TYPE_ZKD_INDEX:
    case TRI_IDX_TYPE_MDI_INDEX:
    case TRI_IDX_TYPE_MDI_PREFIXED_INDEX:
      return mdi::supportsFilterCondition(this, allIndexes, node, reference,
                                          itemsInIndex);

    case TRI_IDX_TYPE_UNKNOWN:
      break;
  }

  TRI_ASSERT(_engineType == ClusterEngineType::MockEngine);
  return Index::FilterCosts::defaultCosts(itemsInIndex);
}

Index::SortCosts ClusterIndex::supportsSortCondition(
    aql::SortCondition const* sortCondition, aql::Variable const* reference,
    size_t itemsInIndex) const {
  switch (_indexType) {
    case TRI_IDX_TYPE_PRIMARY_INDEX:
    case TRI_IDX_TYPE_HASH_INDEX: {
      if (_engineType == ClusterEngineType::RocksDBEngine) {
        return SortedIndexAttributeMatcher::supportsSortCondition(
            this, sortCondition, reference, itemsInIndex);
      }
      break;
    }
    case TRI_IDX_TYPE_GEO_INDEX:
    case TRI_IDX_TYPE_GEO1_INDEX:
    case TRI_IDX_TYPE_GEO2_INDEX:
    case TRI_IDX_TYPE_FULLTEXT_INDEX:
    case TRI_IDX_TYPE_INVERTED_INDEX:
    case TRI_IDX_TYPE_IRESEARCH_LINK:
    case TRI_IDX_TYPE_NO_ACCESS_INDEX:
    case TRI_IDX_TYPE_EDGE_INDEX: {
      return Index::supportsSortCondition(sortCondition, reference,
                                          itemsInIndex);
    }

    case TRI_IDX_TYPE_SKIPLIST_INDEX:
    case TRI_IDX_TYPE_TTL_INDEX:
    case TRI_IDX_TYPE_PERSISTENT_INDEX: {
      if (_engineType == ClusterEngineType::RocksDBEngine) {
        return SortedIndexAttributeMatcher::supportsSortCondition(
            this, sortCondition, reference, itemsInIndex);
      }
      break;
    }

    case TRI_IDX_TYPE_ZKD_INDEX:
    case TRI_IDX_TYPE_MDI_INDEX:
    case TRI_IDX_TYPE_MDI_PREFIXED_INDEX:
      // Sorting not supported
      return Index::SortCosts{};

    case TRI_IDX_TYPE_UNKNOWN:
      break;
  }

  TRI_ASSERT(_engineType == ClusterEngineType::MockEngine);
  return Index::SortCosts::defaultCosts(itemsInIndex);
}

/// @brief specializes the condition for use with the index
aql::AstNode* ClusterIndex::specializeCondition(
    transaction::Methods& trx, aql::AstNode* node,
    aql::Variable const* reference) const {
  switch (_indexType) {
    case TRI_IDX_TYPE_PRIMARY_INDEX: {
      if (_engineType == ClusterEngineType::RocksDBEngine) {
        return SortedIndexAttributeMatcher::specializeCondition(this, node,
                                                                reference);
      }
      return node;
    }
    // should not be called for these
    case TRI_IDX_TYPE_GEO_INDEX:
    case TRI_IDX_TYPE_GEO1_INDEX:
    case TRI_IDX_TYPE_GEO2_INDEX:
    case TRI_IDX_TYPE_FULLTEXT_INDEX:
    case TRI_IDX_TYPE_INVERTED_INDEX:
    case TRI_IDX_TYPE_IRESEARCH_LINK:
    case TRI_IDX_TYPE_NO_ACCESS_INDEX: {
      return Index::specializeCondition(trx, node, reference);  // unsupported
    }
    case TRI_IDX_TYPE_HASH_INDEX:
      if (_engineType == ClusterEngineType::RocksDBEngine) {
        return SortedIndexAttributeMatcher::specializeCondition(this, node,
                                                                reference);
      }
      break;

    case TRI_IDX_TYPE_EDGE_INDEX: {
      // same for both engines
      SimpleAttributeEqualityMatcher matcher(this->_fields);
      return matcher.specializeOne(this, node, reference);
    }

    case TRI_IDX_TYPE_SKIPLIST_INDEX:
    case TRI_IDX_TYPE_TTL_INDEX:
    case TRI_IDX_TYPE_PERSISTENT_INDEX: {
      return SortedIndexAttributeMatcher::specializeCondition(this, node,
                                                              reference);
    }

    case TRI_IDX_TYPE_ZKD_INDEX:
    case TRI_IDX_TYPE_MDI_INDEX:
    case TRI_IDX_TYPE_MDI_PREFIXED_INDEX:
      return mdi::specializeCondition(this, node, reference);

    case TRI_IDX_TYPE_UNKNOWN:
      break;
  }

#ifdef ARANGODB_USE_GOOGLE_TESTS
  if (_engineType == ClusterEngineType::MockEngine) {
    return node;
  }
#endif
  TRI_ASSERT(false);
  return node;
}

std::vector<std::vector<basics::AttributeName>> const&
ClusterIndex::coveredFields() const {
  if (!_coveredFields.empty()) {
    TRI_ASSERT(_engineType == ClusterEngineType::RocksDBEngine);
    return _coveredFields;
  }
  switch (_indexType) {
    case TRI_IDX_TYPE_GEO_INDEX:
    case TRI_IDX_TYPE_GEO1_INDEX:
    case TRI_IDX_TYPE_GEO2_INDEX:
    case TRI_IDX_TYPE_FULLTEXT_INDEX:
    case TRI_IDX_TYPE_TTL_INDEX:
    case TRI_IDX_TYPE_IRESEARCH_LINK:
    case TRI_IDX_TYPE_ZKD_INDEX:
    case TRI_IDX_TYPE_MDI_INDEX:
    case TRI_IDX_TYPE_MDI_PREFIXED_INDEX:
    case TRI_IDX_TYPE_NO_ACCESS_INDEX: {
      return Index::emptyCoveredFields;
    }
    default:
      return _fields;
  }
}

Index::StreamSupportResult ClusterIndex::supportsStreamInterface(
    IndexStreamOptions const& opts) const noexcept {
  switch (_indexType) {
    case Index::TRI_IDX_TYPE_PERSISTENT_INDEX: {
      if (_engineType == ClusterEngineType::RocksDBEngine) {
        return RocksDBVPackIndex::checkSupportsStreamInterface(
            _coveredFields, _fields, _unique, opts);
      }
      [[fallthrough]];
    }

    case Index::TRI_IDX_TYPE_PRIMARY_INDEX: {
      if (_engineType == ClusterEngineType::RocksDBEngine) {
        return RocksDBPrimaryIndex::checkSupportsStreamInterface(_coveredFields,
                                                                 opts);
      }
      [[fallthrough]];
    }

    default:
      break;
  }
  return Index::StreamSupportResult::makeUnsupported();
}
