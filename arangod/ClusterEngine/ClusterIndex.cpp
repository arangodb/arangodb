////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "ClusterIndex.h"
#include "Agency/AsyncAgencyComm.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/StaticStrings.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/AgencyCache.h"
#include "ClusterEngine/ClusterEngine.h"
#include "Cluster/ClusterFeature.h"
#include "Futures/Utilities.h"
#include "Indexes/SimpleAttributeEqualityMatcher.h"
#include "Indexes/SortedIndexAttributeMatcher.h"
#include "Logger/LogMacros.h"
#include "Network/NetworkFeature.h"
#include "RocksDBEngine/RocksDBZkdIndex.h"
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
          {basics::AttributeName(StaticStrings::IdString, false)},
          {basics::AttributeName(StaticStrings::KeyString, false)}};
    } else if (_indexType == TRI_IDX_TYPE_PERSISTENT_INDEX) {
      _coveredFields = Index::mergeFields(
          _fields,
          Index::parseFields(info.get(StaticStrings::IndexStoredValues),
                             /*allowEmpty*/ true, /*allowExpansion*/ false));
    }

    // check for "estimates" attribute
    if (_unique) {
      _estimates = true;
    } else if (_indexType == TRI_IDX_TYPE_HASH_INDEX ||
               _indexType == TRI_IDX_TYPE_SKIPLIST_INDEX ||
               _indexType == TRI_IDX_TYPE_PERSISTENT_INDEX) {
      if (VPackSlice s = info.get(StaticStrings::IndexEstimates);
          s.isBoolean()) {
        _estimates = s.getBoolean();
      }
    } else if (_indexType == TRI_IDX_TYPE_TTL_INDEX) {
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
      _indexType == Index::TRI_IDX_TYPE_PERSISTENT_INDEX) {
    builder.add(StaticStrings::IndexEstimates, VPackValue(_estimates));
  } else if (_indexType == Index::TRI_IDX_TYPE_TTL_INDEX) {
    // no estimates for the ttl index
    builder.add(StaticStrings::IndexEstimates, VPackValue(false));
  }

  if (inProgress()) {
    double progress = 0;
    double success = 0;
    auto const shards = _collection.shardIds();
    auto const body = VPackBuffer<uint8_t>();
    auto* pool =
        _collection.vocbase().server().getFeature<NetworkFeature>().pool();
    std::vector<Future<network::Response>> futures;
    futures.reserve(shards->size());
    std::string const prefix = "/_api/index/";
    network::RequestOptions reqOpts;
    reqOpts.param("withHidden", "true");
    // best effort. only displaying progress
    reqOpts.timeout = network::Timeout(10.0);
    for (auto const& shard : *shards) {
      std::string const url =
          prefix + shard.first + "/" + std::to_string(_iid.id());
      futures.emplace_back(
          network::sendRequestRetry(pool, "shard:" + shard.first,
                                    fuerte::RestVerb::Get, url, body, reqOpts));
    }
    for (Future<network::Response>& f : futures) {
      network::Response const& r = f.get();

      // Only best effort accounting. If something breaks here, we just
      // ignore the output. Account for what we can and move on.
      if (r.fail()) {
        LOG_TOPIC("afde4", INFO, Logger::CLUSTER)
            << "Communication error while collecting figures for collection "
            << _collection.name() << " from " << r.destination;
        continue;
      }
      VPackSlice resSlice = r.slice();
      if (!resSlice.isObject() ||
          !resSlice.get(StaticStrings::Error).isBoolean()) {
        LOG_TOPIC("aabe4", INFO, Logger::CLUSTER)
            << "Result of collecting figures for collection "
            << _collection.name() << " from " << r.destination << " is invalid";
        continue;
      }
      if (resSlice.get(StaticStrings::Error).getBoolean()) {
        LOG_TOPIC("a4bea", INFO, Logger::CLUSTER)
            << "Failed to collect figures for collection " << _collection.name()
            << " from " << r.destination;
        continue;
      }
      if (resSlice.get("progress").isNumber()) {
        progress += resSlice.get("progress").getNumber<double>();
        success++;
      } else {
        LOG_TOPIC("aeab4", INFO, Logger::CLUSTER)
            << "No progress entry on index " << _iid.id() << "  from "
            << r.destination << ": " << resSlice.toJson();
      }
    }
    if (success) {
      builder.add("progress", VPackValue(progress / success));
    }
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
           (_estimates && (_indexType == Index::TRI_IDX_TYPE_HASH_INDEX ||
                           _indexType == Index::TRI_IDX_TYPE_SKIPLIST_INDEX ||
                           _indexType == Index::TRI_IDX_TYPE_PERSISTENT_INDEX));
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
  return _clusterSelectivity;
}

void ClusterIndex::updateClusterSelectivityEstimate(double estimate) {
  _clusterSelectivity = estimate;
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
  auto& engine = _collection.vocbase()
                     .server()
                     .getFeature<EngineSelectorFeature>()
                     .engine();
  return Index::Compare(engine, _info.slice(), info,
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
          {basics::AttributeName(StaticStrings::IdString, false)},
          {basics::AttributeName(StaticStrings::KeyString, false)}};
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
      return zkd::supportsFilterCondition(this, allIndexes, node, reference,
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
      return zkd::specializeCondition(this, node, reference);

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
    case TRI_IDX_TYPE_ZKD_INDEX:
    case TRI_IDX_TYPE_IRESEARCH_LINK:
    case TRI_IDX_TYPE_NO_ACCESS_INDEX: {
      return Index::emptyCoveredFields;
    }
    default:
      return _fields;
  }
}

bool ClusterIndex::inProgress() const {
  // If agency entry "isBuilding".
  try {
    auto const& vocbase = _collection.vocbase();
    auto const& dbname = vocbase.name();
    auto const cid = std::to_string(_collection.id().id());
    auto const& agencyCache =
        vocbase.server().getFeature<ClusterFeature>().agencyCache();
    auto [acb, idx] =
        agencyCache.read(std::vector<std::string>{AgencyCommHelper::path(
            "Plan/Collections/" + basics::StringUtils::urlEncode(dbname) + "/" +
            cid + "/indexes")});
    auto slc = acb->slice()[0].get(std::vector<std::string>{
        "arango", "Plan", "Collections", vocbase.name()});
    if (slc.hasKey(std::vector<std::string>{cid, "indexes"})) {
      slc = slc.get(std::vector<std::string>{cid, "indexes"});
      for (auto const& index : VPackArrayIterator(slc)) {
        if (index.get("id").stringView() == std::to_string(_iid.id())) {
          return index.hasKey(StaticStrings::IndexIsBuilding);
        }
      }
    }
  } catch (...) {
  }
  return false;
}
