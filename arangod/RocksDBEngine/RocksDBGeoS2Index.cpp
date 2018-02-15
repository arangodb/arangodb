////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2017 ArangoDB GmbH, Cologne, Germany
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

#include "RocksDBGeoS2Index.h"

#include "Aql/Ast.h"
#include "Aql/AstNode.h"
#include "Aql/Function.h"
#include "Aql/SortCondition.h"
#include "Basics/StringRef.h"
#include "Basics/VelocyPackHelper.h"
#include "GeoIndex/Near.h"
#include "Indexes/IndexResult.h"
#include "Logger/Logger.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "RocksDBEngine/RocksDBMethods.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ManagedDocumentResult.h"

#include <rocksdb/db.h>

using namespace arangodb;

template <typename CMP = geo_index::DocumentsAscending>
class RDBNearIterator final : public IndexIterator {
 public:
  /// @brief Construct an RocksDBGeoIndexIterator based on Ast Conditions
  RDBNearIterator(LogicalCollection* collection, transaction::Methods* trx,
                  ManagedDocumentResult* mmdr, RocksDBGeoS2Index const* index,
                  geo::QueryParams&& params)
      : IndexIterator(collection, trx, mmdr, index),
        _index(index),
        _near(std::move(params),
              index->variant() == geo_index::Index::Variant::GEOJSON) {
    RocksDBMethods* mthds = RocksDBTransactionState::toMethods(trx);
    rocksdb::ReadOptions options = mthds->readOptions();
    TRI_ASSERT(options.prefix_same_as_start);
    _iter = mthds->NewIterator(options, _index->columnFamily());
    TRI_ASSERT(_index->columnFamily()->GetID() ==
               RocksDBColumnFamily::geo()->GetID());
    estimateDensity();
  }

  constexpr char const* typeName() const override {
    return "s2-index-iterator";
  }

  /// internal retrieval loop
  inline bool nextToken(std::function<bool(LocalDocumentId token)>&& cb,
                        size_t limit) {
    if (_near.isDone()) {
      // we already know that no further results will be returned by the index
      TRI_ASSERT(!_near.hasNearest());
      return false;
    }

    while (limit > 0 && !_near.isDone()) {
      while (limit > 0 && _near.hasNearest()) {
        if (cb(_near.nearest().document)) {
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

  bool nextDocument(DocumentCallback const& cb, size_t limit) override {
    return nextToken(
        [this, &cb](LocalDocumentId const& token) -> bool {
          if (!_collection->readDocument(_trx, token, *_mmdr)) {
            return false;
          }
          VPackSlice doc(_mmdr->vpack());
          geo::FilterType const ft = _near.filterType();
          if (ft != geo::FilterType::NONE) {  // expensive test
            geo::ShapeContainer const& filter = _near.filterShape();
            TRI_ASSERT(filter.type() != geo::ShapeContainer::Type::EMPTY);
            geo::ShapeContainer test;
            Result res = _index->shape(doc, test);
            TRI_ASSERT(res.ok());  // this should never fail here
            if (res.fail() ||
                (ft == geo::FilterType::CONTAINS && !filter.contains(&test)) ||
                (ft == geo::FilterType::INTERSECTS &&
                 !filter.intersects(&test))) {
              return false;
            }
          }
          cb(token, doc);  // return result
          return true;
        },
        limit);
  }

  bool next(LocalDocumentIdCallback const& cb, size_t limit) override {
    return nextToken(
        [this, &cb](LocalDocumentId const& token) -> bool {
          geo::FilterType const ft = _near.filterType();
          if (ft != geo::FilterType::NONE) {
            geo::ShapeContainer const& filter = _near.filterShape();
            TRI_ASSERT(!filter.empty());
            if (!_collection->readDocument(_trx, token, *_mmdr)) {
              return false;
            }
            geo::ShapeContainer test;
            Result res = _index->shape(VPackSlice(_mmdr->vpack()), test);
            TRI_ASSERT(res.ok());  // this should never fail here
            if (res.fail() ||
                (ft == geo::FilterType::CONTAINS && !filter.contains(&test)) ||
                (ft == geo::FilterType::INTERSECTS &&
                 !filter.intersects(&test))) {
              return false;
            }
          }

          cb(token);  // return result
          return true;
        },
        limit);
  }

  void reset() override { _near.reset(); }

 private:
  // we need to get intervals representing areas in a ring (annulus)
  // around our target point. We need to fetch them ALL and then sort
  // found results in a priority list according to their distance
  void performScan() {
    rocksdb::Comparator const* cmp = _index->comparator();
    // list of sorted intervals to scan
    std::vector<geo::Interval> const scan = _near.intervals();
    //LOG_TOPIC(INFO, Logger::FIXME) << "# intervals: " << scan.size();
    //size_t seeks = 0;

    for (size_t i = 0; i < scan.size(); i++) {
      geo::Interval const& it = scan[i];
      TRI_ASSERT(it.min <= it.max);
      RocksDBKeyBounds bds = RocksDBKeyBounds::S2Index(
          _index->objectId(), it.min.id(), it.max.id());

      // intervals are sorted and likely consecutive, try to avoid seeks
      // by checking whether we are in the range already
      bool seek = true;
      if (i > 0) {
        TRI_ASSERT(scan[i - 1].max < it.min);
        if (!_iter->Valid()) {  // no more valid keys after this
          break;
        } else if (cmp->Compare(_iter->key(), bds.end()) > 0) {
          continue;  // beyond range already
        } else if (cmp->Compare(bds.start(), _iter->key()) <= 0) {
          seek = false;  // already in range: min <= key <= max
          TRI_ASSERT(cmp->Compare(_iter->key(), bds.end()) <= 0);
        } else {  // cursor is positioned below min range key
          TRI_ASSERT(cmp->Compare(_iter->key(), bds.start()) < 0);
          int k = 10, cc = -1;  // try to catch the range
          do {
            _iter->Next();
            cc = cmp->Compare(_iter->key(), bds.start());
          } while (--k > 0 && _iter->Valid() && cc < 0);
          seek = !_iter->Valid() || cc < 0;
        }
      }

      if (seek) {  // try to avoid seeking at all cost
        // LOG_TOPIC(INFO, Logger::FIXME) << "[Scan] seeking:" << it.min; seeks++;
        _iter->Seek(bds.start());
      }

      while (_iter->Valid() && cmp->Compare(_iter->key(), bds.end()) <= 0) {
        TRI_voc_rid_t rid = RocksDBKey::revisionId(
            RocksDBEntryType::S2IndexValue, _iter->key());
        _near.reportFound(LocalDocumentId(rid),
                          RocksDBValue::centroid(_iter->value()));
        _iter->Next();
      }
    }
    //LOG_TOPIC(INFO, Logger::FIXME) << "# seeks: " << seeks;
  }

  /// find the first indexed entry to estimate the # of entries
  /// around our target coordinates
  void estimateDensity() {
    S2CellId cell = S2CellId(_near.origin());

    RocksDBKeyLeaser key(_trx);
    key->constructS2IndexValue(_index->objectId(), cell.id(), 1);
    _iter->Seek(key->string());
    if (!_iter->Valid()) {
      _iter->SeekForPrev(key->string());
    }
    if (_iter->Valid()) {
      geo::Coordinate first = RocksDBValue::centroid(_iter->value());
      _near.estimateDensity(first);
    }
  }

 private:
  RocksDBGeoS2Index const* _index;
  geo_index::NearUtils<CMP> _near;
  std::unique_ptr<rocksdb::Iterator> _iter;
};

RocksDBGeoS2Index::RocksDBGeoS2Index(TRI_idx_iid_t iid,
                                     LogicalCollection* collection,
                                     VPackSlice const& info)
    : RocksDBIndex(iid, collection, info, RocksDBColumnFamily::geo(), false),
      geo_index::Index(info, _fields) {
  TRI_ASSERT(iid != 0);
  _unique = false;
  _sparse = true;
  TRI_ASSERT(_variant != geo_index::Index::Variant::NONE);
}

/// @brief return a JSON representation of the index
void RocksDBGeoS2Index::toVelocyPack(VPackBuilder& builder, bool withFigures,
                                     bool forPersistence) const {
  TRI_ASSERT(_variant != geo_index::Index::Variant::NONE);
  builder.openObject();
  RocksDBIndex::toVelocyPack(builder, withFigures, forPersistence);
  _coverParams.toVelocyPack(builder);
  builder.add("geoJson", VPackValue(_variant == geo_index::Index::Variant::GEOJSON));
  // geo indexes are always non-unique
  // geo indexes are always sparse.
  builder.add("unique", VPackValue(false));
  builder.add("sparse", VPackValue(true));
  builder.close();
}

/// @brief Test if this index matches the definition
bool RocksDBGeoS2Index::matchesDefinition(VPackSlice const& info) const {
  TRI_ASSERT(_variant != geo_index::Index::Variant::NONE);
  TRI_ASSERT(info.isObject());
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  VPackSlice typeSlice = info.get("type");
  TRI_ASSERT(typeSlice.isString());
  StringRef typeStr(typeSlice);
  TRI_ASSERT(typeStr == oldtypeName());
#endif
  auto value = info.get("id");
  if (!value.isNone()) {
    // We already have an id.
    if (!value.isString()) {
      // Invalid ID
      return false;
    }
    // Short circuit. If id is correct the index is identical.
    StringRef idRef(value);
    return idRef == std::to_string(_iid);
  }

  if (_unique !=
      basics::VelocyPackHelper::getBooleanValue(info, "unique", false)) {
    return false;
  }
  if (_sparse !=
      basics::VelocyPackHelper::getBooleanValue(info, "sparse", true)) {
    return false;
  }

  value = info.get("fields");
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
    arangodb::StringRef in(f);
    TRI_ParseAttributeString(in, translate, true);
    if (!arangodb::basics::AttributeName::isIdentical(_fields[i], translate,
                                                      false)) {
      return false;
    }
  }
  return true;
}

/// @brief creates an IndexIterator for the given Condition
IndexIterator* RocksDBGeoS2Index::iteratorForCondition(
    transaction::Methods* trx, ManagedDocumentResult* mmdr,
    arangodb::aql::AstNode const* node,
    arangodb::aql::Variable const* reference,
    IndexIteratorOptions const& opts) {
  TRI_ASSERT(!isSorted() || opts.sorted);
  TRI_ASSERT(!opts.evaluateFCalls);  // should not get here without
  TRI_ASSERT(node != nullptr);

  geo::QueryParams params;
  params.sorted = opts.sorted;
  params.ascending = opts.ascending;
  geo_index::Index::parseCondition(node, reference, params);

  // FIXME: <Optimize away>
  params.sorted = true;
  if (params.filterType != geo::FilterType::NONE) {
    TRI_ASSERT(!params.filterShape.empty());
    params.filterShape.updateBounds(params);
  }
  //        </Optimize away>

  TRI_ASSERT(!opts.sorted || params.origin.isValid());
  // params.cover.worstIndexedLevel < _coverParams.worstIndexedLevel
  // is not necessary, > would be missing entries.
  params.cover.worstIndexedLevel = _coverParams.worstIndexedLevel;
  if (params.cover.bestIndexedLevel > _coverParams.bestIndexedLevel) {
    // it is unnessesary to use a better level than configured
    params.cover.bestIndexedLevel = _coverParams.bestIndexedLevel;
  }
  if (params.ascending) {
    return new RDBNearIterator<geo_index::DocumentsAscending>(
        _collection, trx, mmdr, this, std::move(params));
  } else {
    return new RDBNearIterator<geo_index::DocumentsDescending>(
        _collection, trx, mmdr, this, std::move(params));
  }
}

/// internal insert function, set batch or trx before calling
Result RocksDBGeoS2Index::insertInternal(transaction::Methods* trx,
                                         RocksDBMethods* mthd,
                                         LocalDocumentId const& documentId,
                                         velocypack::Slice const& doc,
                                         OperationMode mode) {
  // covering and centroid of coordinate / polygon / ...
  std::vector<S2CellId> cells;
  geo::Coordinate centroid(-1.0, -1.0);
  Result res = geo_index::Index::indexCells(doc, cells, centroid);
  if (res.fail()) {
    // Invalid, no insert. Index is sparse
    return res.is(TRI_ERROR_BAD_PARAMETER) ? IndexResult() : res;
  }
  TRI_ASSERT(!cells.empty() && std::abs(centroid.latitude) <= 90.0 &&
             std::abs(centroid.longitude) <= 180.0);

  RocksDBValue val = RocksDBValue::S2Value(centroid);
  RocksDBKeyLeaser key(trx);
  // FIXME: can we rely on the region coverer to return
  // the same cells everytime for the same parameters ?
  for (S2CellId cell : cells) {
    // LOG_TOPIC(ERR, Logger::FIXME) << "[Insert] " << cell;
    key->constructS2IndexValue(_objectId, cell.id(), documentId.id());
    Result r = mthd->Put(RocksDBColumnFamily::geo(), key.ref(), val.string());
    if (r.fail()) {
      return r;
    }
  }

  return IndexResult();
}

/// internal remove function, set batch or trx before calling
Result RocksDBGeoS2Index::removeInternal(transaction::Methods* trx,
                                         RocksDBMethods* mthd,
                                         LocalDocumentId const& documentId,
                                         VPackSlice const& doc,
                                         OperationMode mode) {
  // covering and centroid of coordinate / polygon / ...
  std::vector<S2CellId> cells;
  geo::Coordinate centroid(-1, -1);
  Result res = geo_index::Index::indexCells(doc, cells, centroid);
  if (res.fail()) { // might occur if insert is rolled back
    // Invalid, no insert. Index is sparse
    return res.is(TRI_ERROR_BAD_PARAMETER) ? IndexResult() : res;
  }
  TRI_ASSERT(!cells.empty() && std::abs(centroid.latitude) <= 90.0 &&
             std::abs(centroid.longitude) <= 180.0);

  RocksDBKeyLeaser key(trx);
  // FIXME: can we rely on the region coverer to return
  // the same cells everytime for the same parameters ?
  for (S2CellId cell : cells) {
    // LOG_TOPIC(INFO, Logger::FIXME) << "[Remove] " << cell;
    key->constructS2IndexValue(_objectId, cell.id(), documentId.id());
    Result r = mthd->Delete(RocksDBColumnFamily::geo(), key.ref());
    if (r.fail()) {
      return r;
    }
  }
  return IndexResult();
}
