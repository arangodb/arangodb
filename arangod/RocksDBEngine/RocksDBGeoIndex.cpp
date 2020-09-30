////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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

#include "RocksDBGeoIndex.h"

#include "Aql/Ast.h"
#include "Aql/AstNode.h"
#include "Aql/Function.h"
#include "Aql/SortCondition.h"
#include "Basics/VelocyPackHelper.h"
#include "GeoIndex/Near.h"
#include "Logger/Logger.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "RocksDBEngine/RocksDBMethods.h"
#include "VocBase/LogicalCollection.h"

#include <rocksdb/db.h>
#include <s2/s2cell_id.h>

#include <velocypack/Iterator.h>
#include <velocypack/StringRef.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;

template <typename CMP = geo_index::DocumentsAscending>
class RDBNearIterator final : public IndexIterator {
 public:
  /// @brief Construct an RocksDBGeoIndexIterator based on Ast Conditions
  RDBNearIterator(LogicalCollection* collection, transaction::Methods* trx,
                  RocksDBGeoIndex const* index, geo::QueryParams&& params)
      : IndexIterator(collection, trx), _index(index), _near(std::move(params)) {
    RocksDBMethods* mthds = RocksDBTransactionState::toMethods(trx);
    rocksdb::ReadOptions options = mthds->iteratorReadOptions();
    TRI_ASSERT(options.prefix_same_as_start);
    _iter = mthds->NewIterator(options, _index->columnFamily());
    TRI_ASSERT(_index->columnFamily()->GetID() == RocksDBColumnFamily::geo()->GetID());
    estimateDensity();
  }

  char const* typeName() const override { return "geo-index-iterator"; }

  /// internal retrieval loop
  template <typename F>
  inline bool nextToken(F&& cb, size_t limit) {
    if (_near.isDone()) {
      // we already know that no further results will be returned by the index
      TRI_ASSERT(!_near.hasNearest());
      return false;
    }

    while (limit > 0 && !_near.isDone()) {
      while (limit > 0 && _near.hasNearest()) {
        if (std::forward<F>(cb)(_near.nearest())) {
          limit--;
        }
        _near.popNearest();
      }
      // need to fetch more geo results
      if (limit > 0 && !_near.isDone()) {
        TRI_ASSERT(!_near.hasNearest());
        performScan();
      }
    }
    return !_near.isDone();
  }

  bool nextDocumentImpl(DocumentCallback const& cb, size_t limit) override {
    return nextToken(
        [this, &cb](geo_index::Document const& gdoc) -> bool {
          bool result = true;  // this is updated by the callback
          if (!_collection->readDocumentWithCallback(_trx, gdoc.token, [&](LocalDocumentId const&, VPackSlice doc) {
                geo::FilterType const ft = _near.filterType();
                if (ft != geo::FilterType::NONE) {  // expensive test
                  geo::ShapeContainer const& filter = _near.filterShape();
                  TRI_ASSERT(filter.type() != geo::ShapeContainer::Type::EMPTY);
                  geo::ShapeContainer test;
                  Result res = _index->shape(doc, test);
                  TRI_ASSERT(res.ok());  // this should never fail here
                  if (res.fail() ||
                      (ft == geo::FilterType::CONTAINS && !filter.contains(&test)) ||
                      (ft == geo::FilterType::INTERSECTS && !filter.intersects(&test))) {
                    result = false;
                    return false;
                  }
                }
                cb(gdoc.token, doc);  // return document
                result = true;
                return true;
              })) {
            return false;  // ignore document
          }
          return result;
        },
        limit);
  }

  bool nextImpl(LocalDocumentIdCallback const& cb, size_t limit) override {
    return nextToken(
        [this, &cb](geo_index::Document const& gdoc) -> bool {
          geo::FilterType const ft = _near.filterType();
          if (ft != geo::FilterType::NONE) {
            geo::ShapeContainer const& filter = _near.filterShape();
            TRI_ASSERT(!filter.empty());
            bool result = true;  // this is updated by the callback
            if (!_collection->readDocumentWithCallback(_trx, gdoc.token, [&](LocalDocumentId const&, VPackSlice doc) {
                  geo::ShapeContainer test;
                  Result res = _index->shape(doc, test);
                  TRI_ASSERT(res.ok());  // this should never fail here
                  if (res.fail() ||
                      (ft == geo::FilterType::CONTAINS && !filter.contains(&test)) ||
                      (ft == geo::FilterType::INTERSECTS && !filter.intersects(&test))) {
                    result = false;
                    return false;
                  }
                  return true;
                })) {
              return false;
            }
            if (!result) {
              return false;
            }
          }

          cb(gdoc.token);  // return result
          return true;
        },
        limit);
  }

  void resetImpl() override {
    _near.reset();
    estimateDensity();
  }

 private:
  // we need to get intervals representing areas in a ring (annulus)
  // around our target point. We need to fetch them ALL and then sort
  // found results in a priority list according to their distance
  void performScan() {
    rocksdb::Comparator const* cmp = _index->comparator();
    // list of sorted intervals to scan
    std::vector<geo::Interval> const scan = _near.intervals();
    // LOG_TOPIC("b1eea", INFO, Logger::ENGINES) << "# intervals: " << scan.size();
    // size_t seeks = 0;

    for (size_t i = 0; i < scan.size(); i++) {
      geo::Interval const& it = scan[i];
      TRI_ASSERT(it.range_min <= it.range_max);
      RocksDBKeyBounds bds =
          RocksDBKeyBounds::GeoIndex(_index->objectId(), it.range_min.id(),
                                     it.range_max.id());
      
      // intervals are sorted and likely consecutive, try to avoid seeks
      // by checking whether we are in the range already
      bool seek = true;
      if (i > 0) {
        TRI_ASSERT(scan[i - 1].range_max < it.range_min);
        if (!_iter->Valid()) {  // no more valid keys after this
          break;
        } else if (cmp->Compare(_iter->key(), bds.end()) > 0) {
          continue;  // beyond range already
        } else if (cmp->Compare(bds.start(), _iter->key()) <= 0) {
          seek = false;  // already in range: min <= key <= max
          TRI_ASSERT(cmp->Compare(_iter->key(), bds.end()) <= 0);
        } else {  // cursor is positioned below min range key
          TRI_ASSERT(cmp->Compare(_iter->key(), bds.start()) < 0);
          int k = 10;  // try to catch the range
          while (k > 0 && _iter->Valid() && cmp->Compare(_iter->key(), bds.start()) < 0) {
            _iter->Next();
            --k;
          }
          seek = !_iter->Valid() || (cmp->Compare(_iter->key(), bds.start()) < 0);
        }
      }

      if (seek) {  // try to avoid seeking at all cost
        // LOG_TOPIC("0afaa", INFO, Logger::ENGINES) << "[Scan] seeking:" << it.min;
        // seeks++;
        _iter->Seek(bds.start());
      }

      while (_iter->Valid() && cmp->Compare(_iter->key(), bds.end()) <= 0) {
        _near.reportFound(RocksDBKey::indexDocumentId(_iter->key()),
                          RocksDBValue::centroid(_iter->value()));
        _iter->Next();
      }
    
      // validate that Iterator is in a good shape and hasn't failed
      arangodb::rocksutils::checkIteratorStatus(_iter.get());
    }

    _near.didScanIntervals();  // calculate next bounds
    // LOG_TOPIC("e82ee", INFO, Logger::ENGINES) << "# seeks: " << seeks;
  }

  /// find the first indexed entry to estimate the # of entries
  /// around our target coordinates
  void estimateDensity() {
    S2CellId cell = S2CellId(_near.origin());

    RocksDBKeyLeaser key(_trx);
    key->constructGeoIndexValue(_index->objectId(), cell.id(), LocalDocumentId(1));
    _iter->Seek(key->string());
    if (!_iter->Valid()) {
      _iter->SeekForPrev(key->string());
    }
    if (_iter->Valid()) {
      _near.estimateDensity(RocksDBValue::centroid(_iter->value()));
    }
  }

 private:
  RocksDBGeoIndex const* _index;
  geo_index::NearUtils<CMP> _near;
  std::unique_ptr<rocksdb::Iterator> _iter;
};

RocksDBGeoIndex::RocksDBGeoIndex(IndexId iid, LogicalCollection& collection,
                                 arangodb::velocypack::Slice const& info,
                                 std::string const& typeName)
    : RocksDBIndex(iid, collection, info, RocksDBColumnFamily::geo(), false),
      geo_index::Index(info, _fields),
      _typeName(typeName) {
  TRI_ASSERT(iid.isSet());
  _unique = false;
  _sparse = true;
  TRI_ASSERT(_variant != geo_index::Index::Variant::NONE);
}

/// @brief return a JSON representation of the index
void RocksDBGeoIndex::toVelocyPack(VPackBuilder& builder,
                                   std::underlying_type<arangodb::Index::Serialize>::type flags) const {
  TRI_ASSERT(_variant != geo_index::Index::Variant::NONE);
  builder.openObject();
  RocksDBIndex::toVelocyPack(builder, flags);
  _coverParams.toVelocyPack(builder);
  builder.add("geoJson", VPackValue(_variant == geo_index::Index::Variant::GEOJSON));
  builder.close();
}

/// @brief Test if this index matches the definition
bool RocksDBGeoIndex::matchesDefinition(VPackSlice const& info) const {
  TRI_ASSERT(_variant != geo_index::Index::Variant::NONE);
  TRI_ASSERT(info.isObject());
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  auto typeSlice = info.get(arangodb::StaticStrings::IndexType);
  TRI_ASSERT(typeSlice.isString());
  arangodb::velocypack::StringRef typeStr(typeSlice);
  TRI_ASSERT(typeStr == oldtypeName());
#endif
  auto value = info.get(arangodb::StaticStrings::IndexId);

  if (!value.isNone()) {
    // We already have an id.
    if (!value.isString()) {
      // Invalid ID
      return false;
    }

    // Short circuit. If id is correct the index is identical.
    arangodb::velocypack::StringRef idRef(value);
    return idRef == std::to_string(_iid.id());
  }

  if (_unique != basics::VelocyPackHelper::getBooleanValue(info, arangodb::StaticStrings::IndexUnique,
                                                           false)) {
    return false;
  }

  if (_sparse != basics::VelocyPackHelper::getBooleanValue(info, arangodb::StaticStrings::IndexSparse,
                                                           true)) {
    return false;
  }

  value = info.get(arangodb::StaticStrings::IndexFields);

  if (!value.isArray()) {
    return false;
  }

  size_t const n = static_cast<size_t>(value.length());
  if (n != _fields.size()) {
    return false;
  }

  if (n == 1) {
    bool gj1 = basics::VelocyPackHelper::getBooleanValue(info, "geoJson", false);
    bool gj2 = _variant == geo_index::Index::Variant::GEOJSON;
    if (gj1 != gj2) {
      return false;
    }
  }

  // This check takes ordering of attributes into account.
  std::vector<arangodb::basics::AttributeName> translate;
  for (size_t i = 0; i < n; ++i) {
    translate.clear();
    VPackSlice f = value.at(i);
    if (!f.isString()) {
      // Invalid field definition!
      return false;
    }
    arangodb::velocypack::StringRef in(f);
    TRI_ParseAttributeString(in, translate, true);
    if (!arangodb::basics::AttributeName::isIdentical(_fields[i], translate, false)) {
      return false;
    }
  }
  return true;
}

/// @brief creates an IndexIterator for the given Condition
std::unique_ptr<IndexIterator> RocksDBGeoIndex::iteratorForCondition(
    transaction::Methods* trx, arangodb::aql::AstNode const* node,
    arangodb::aql::Variable const* reference, IndexIteratorOptions const& opts) {
  TRI_ASSERT(!isSorted() || opts.sorted);
  TRI_ASSERT(node != nullptr);

  geo::QueryParams params;
  params.sorted = opts.sorted;
  params.ascending = opts.ascending;
  params.pointsOnly = pointsOnly();
  params.limit = opts.limit;
  geo_index::Index::parseCondition(node, reference, params);

  // FIXME: <Optimize away>
  params.sorted = true;
  if (params.filterType != geo::FilterType::NONE) {
    TRI_ASSERT(!params.filterShape.empty());
    params.filterShape.updateBounds(params);
  }
  //        </Optimize away>

  TRI_ASSERT(!opts.sorted || params.origin.is_valid());
  // params.cover.worstIndexedLevel < _coverParams.worstIndexedLevel
  // is not necessary, > would be missing entries.
  params.cover.worstIndexedLevel = _coverParams.worstIndexedLevel;
  if (params.cover.bestIndexedLevel > _coverParams.bestIndexedLevel) {
    // it is unnessesary to use a better level than configured
    params.cover.bestIndexedLevel = _coverParams.bestIndexedLevel;
  }

  if (params.ascending) {
    return std::make_unique<RDBNearIterator<geo_index::DocumentsAscending>>(&_collection, trx, this, std::move(params));
  } else {
    return std::make_unique<RDBNearIterator<geo_index::DocumentsDescending>>(&_collection, trx, this, std::move(params));
  }
}

/// internal insert function, set batch or trx before calling
Result RocksDBGeoIndex::insert(transaction::Methods& trx, RocksDBMethods* mthd,
                               LocalDocumentId const& documentId,
                               velocypack::Slice const doc,
                               arangodb::OperationOptions& options) {
  // covering and centroid of coordinate / polygon / ...
  size_t reserve = _variant == Variant::GEOJSON ? 8 : 1;
  std::vector<S2CellId> cells;
  cells.reserve(reserve);

  S2Point centroid;

  Result res = geo_index::Index::indexCells(doc, cells, centroid);

  if (res.fail()) {
    if (res.is(TRI_ERROR_BAD_PARAMETER)) {
      res.reset();  // Invalid, no insert. Index is sparse
    }

    return res;
  }

  TRI_ASSERT(!cells.empty());
  TRI_ASSERT(S2::IsUnitLength(centroid));

  RocksDBValue val = RocksDBValue::S2Value(centroid);
  RocksDBKeyLeaser key(&trx);

  TRI_ASSERT(!_unique);

  for (S2CellId cell : cells) {
    key->constructGeoIndexValue(objectId(), cell.id(), documentId);
    TRI_ASSERT(key->containsLocalDocumentId(documentId));
 
    rocksdb::Status s = mthd->PutUntracked(RocksDBColumnFamily::geo(), key.ref(), val.string());

    if (!s.ok()) {
      res.reset(rocksutils::convertStatus(s, rocksutils::index));
      addErrorMsg(res);
      break;
    }
  }

  return res;
}

/// internal remove function, set batch or trx before calling
Result RocksDBGeoIndex::remove(transaction::Methods& trx, RocksDBMethods* mthd,
                               LocalDocumentId const& documentId,
                               velocypack::Slice const doc) {
  Result res;

  // covering and centroid of coordinate / polygon / ...
  std::vector<S2CellId> cells;
  S2Point centroid;

  res = geo_index::Index::indexCells(doc, cells, centroid);

  if (res.fail()) {  // might occur if insert is rolled back
    if (res.is(TRI_ERROR_BAD_PARAMETER)) {
      res.reset();  // Invalid, no insert. Index is sparse
    }

    return res;
  }

  TRI_ASSERT(!cells.empty());

  RocksDBKeyLeaser key(&trx);

  // FIXME: can we rely on the region coverer to return
  // the same cells everytime for the same parameters ?
  for (S2CellId cell : cells) {
    key->constructGeoIndexValue(objectId(), cell.id(), documentId);
    rocksdb::Status s = mthd->Delete(RocksDBColumnFamily::geo(), key.ref());
    if (!s.ok()) {
      res.reset(rocksutils::convertStatus(s, rocksutils::index));
      addErrorMsg(res);
      break;
    }
  }
  return res;
}
