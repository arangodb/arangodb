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
#include "Geo/AqlUtils.h"
#include "Geo/GeoUtils.h"
#include "Geo/GeoJsonParser.h"
#include "Indexes/IndexResult.h"
#include "Logger/Logger.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "RocksDBEngine/RocksDBMethods.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ManagedDocumentResult.h"

#include <geometry/s2regioncoverer.h>
#include <rocksdb/db.h>

using namespace arangodb;

RocksDBGeoS2IndexIterator::RocksDBGeoS2IndexIterator(
    LogicalCollection* collection, transaction::Methods* trx,
    ManagedDocumentResult* mmdr, RocksDBGeoS2Index const* index)
    : IndexIterator(collection, trx, mmdr, index), _index(index) {
  RocksDBMethods* mthds = RocksDBTransactionState::toMethods(trx);
  rocksdb::ReadOptions options = mthds->readOptions();
  TRI_ASSERT(options.prefix_same_as_start);
  _iter = mthds->NewIterator(options, _index->columnFamily());
  TRI_ASSERT(_index->columnFamily()->GetID() ==
             RocksDBColumnFamily::geo()->GetID());
}
/*
class RegionIterator : public RocksDBGeoS2IndexIterator {
 public:
  RegionIterator(LogicalCollection* collection, transaction::Methods* trx,
                 ManagedDocumentResult* mmdr,
                 RocksDBGeoS2Index const* index,
                 std::unique_ptr<S2Region> region, geo::FilterType ft)
      : RocksDBGeoS2IndexIterator(collection, trx, mmdr, index),
        _region(region.release()),
        _filterType(ft) {}

  ~RegionIterator() { delete _region; }

  geo::FilterType filterType() const override { return _filterType; };

  bool nextDocument(DocumentCallback const& cb, size_t limit) override {}

  /// if you call this it's your own damn fault
  bool next(LocalDocumentIdCallback const& cb, size_t limit) override {}

  void reset() override {
    _seen.clear();
    // TODO
  }
  
private:
  

 private:
  S2Region* const _region;
  geo::FilterType const _filterType;
  std::unordered_set<TRI_voc_rid_t> _seen;
  std::vector<geo::GeoCover::Interval> _intervals;
};*/

class NearIterator final : public RocksDBGeoS2IndexIterator {
 public:
  /// @brief Construct an RocksDBGeoIndexIterator based on Ast Conditions
  NearIterator(LogicalCollection* collection, transaction::Methods* trx,
               ManagedDocumentResult* mmdr, RocksDBGeoS2Index const* index,
               geo::QueryParams&& params)
      : RocksDBGeoS2IndexIterator(collection, trx, mmdr, index),
        _near(std::move(params)) {
    estimateDensity();
  }
  
  bool nextDocument(DocumentCallback const& cb, size_t limit) override {
    if (_near.isDone()) {
      // we already know that no further results will be returned by the index
      TRI_ASSERT(!_near.hasNearest());
      return false;
    }
    geo::FilterType const ft = _near.filterType();
    
    while (limit > 0 && !_near.isDone()) {
      while (limit > 0 && _near.hasNearest()) {
        /*LOG_TOPIC(WARN, Logger::FIXME) << "[RETURN] " << _near.nearest().rid
         << "  d: " << ( _near.nearest().radians * geo::kEarthRadiusInMeters);*/
        
        LocalDocumentId token(_near.nearest().rid);
        _near.popNearest();
        limit--;
        
        if (_collection->readDocument(_trx, token, *_mmdr)) {
          VPackSlice doc(_mmdr->vpack());
          if (ft != geo::FilterType::NONE) { // expensive test
            geo::ShapeContainer const& filter = _near.filterShape();
            geo::ShapeContainer test;
            Result res = geo::GeoJsonParser::parseGeoJson(doc, test);
            if (res.fail()) { // this should never fail here
              THROW_ARANGO_EXCEPTION(res);
            }
            if ((ft == geo::FilterType::CONTAINS && !filter.contains(&test)) ||
                (ft == geo::FilterType::INTERSECTS && !filter.intersects(&test))) {
              continue;
            }
          }
          
          cb(token, doc);
        }
      }
      // need to fetch more geo results
      if (limit > 0 && !_near.isDone()) {
        TRI_ASSERT(!_near.hasNearest());
        performScan();
      }
    }
    return !_near.isDone();
  }

  bool next(LocalDocumentIdCallback const& cb, size_t limit) override {
    TRI_ASSERT(false); // I don't think this iterator should be used this way
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
    LOG_TOPIC(INFO, Logger::FIXME) << "# intervals: " << scan.size();
    size_t seeks = 0;

    for (size_t i = 0; i < scan.size(); i++) {
      geo::Interval const& it = scan[i];
      TRI_ASSERT(it.min <= it.max);
      RocksDBKeyBounds bds = RocksDBKeyBounds::S2Index(_index->objectId(),
                                                       it.min.id(), it.max.id());
      
      // intervals are sorted and likely consecutive, try to avoid seeks
      // by checking whether we are in the range already
      bool seek = true;
      if (i > 0) {
        TRI_ASSERT(scan[i - 1].max < it.min);
        if (!_iter->Valid()) { // no more valid keys after this
          LOG_TOPIC(INFO, Logger::FIXME) << "[Scan] reached end of index at: " << it.min;
          break;
        } else if (cmp->Compare(_iter->key(), bds.end()) > 0) {
          continue; // beyond range already
        } else if (cmp->Compare(bds.start(), _iter->key()) <= 0) {
          seek = false; // already in range: min <= key <= max
          TRI_ASSERT(cmp->Compare(_iter->key(), bds.end()) <= 0);
        } else { // cursor is positioned below min range key
          TRI_ASSERT(cmp->Compare(_iter->key(), bds.start()) < 0);
          int k = 10, cc = -1; // try to skip ahead a bit and catch the range
          do {
            _iter->Next();
            cc = cmp->Compare(_iter->key(), bds.start());
          } while (--k > 0 && _iter->Valid() && cc < 0);
          seek = !_iter->Valid() || cc < 0;
        }
      }

      if (seek) { // try to avoid seeking at all cost
        //LOG_TOPIC(INFO, Logger::FIXME) << "[Scan] seeking:" << it.min;
        _iter->Seek(bds.start());
        seeks++;
      }

      uint64_t cellId = it.min.id();
      while (_iter->Valid() && cmp->Compare(_iter->key(), bds.end()) <= 0) {
        cellId = RocksDBKey::S2Value(_iter->key());
        TRI_voc_rid_t rid = RocksDBKey::revisionId(
            RocksDBEntryType::S2IndexValue, _iter->key());
        geo::Coordinate cntrd = RocksDBValue::centroid(_iter->value());
        _near.reportFound(rid, cntrd);
        _iter->Next();
      }
      /*if (cellId != it.min.id()) {
        TRI_ASSERT(cellId <= it.max.id());
        LOG_TOPIC(INFO, Logger::FIXME) << "[Scan] ending:" << it.max;
      }*/
    }
    
    LOG_TOPIC(INFO, Logger::FIXME) << "# seeks: " << seeks;
  }

  /// find the first indexed entry to estimate the # of entries
  /// around our target coordinates
  void estimateDensity() {
    S2CellId cell = S2CellId::FromPoint(_near.centroid());

    RocksDBKeyLeaser key(_trx);
    key->constructS2IndexValue(_index->objectId(), cell.id(), 1);
    _iter->Seek(key->string());
    if (!_iter->Valid()) {
      _iter->SeekForPrev(key->string());
    }
    if (_iter->Valid()) {
      geo::Coordinate first = RocksDBValue::centroid(_iter->value());
      _near.estimateDensity(first);
    } else {
      LOG_TOPIC(INFO, Logger::ROCKSDB)
          << "Apparently the s2 index is empty";
      _near.invalidate();
    }
  }

 private:
  geo::NearUtils _near;
};

RocksDBGeoS2Index::RocksDBGeoS2Index(TRI_idx_iid_t iid,
                                             LogicalCollection* collection,
                                             VPackSlice const& info)
    : RocksDBIndex(iid, collection, info, RocksDBColumnFamily::geo(), false),
      _variant(IndexVariant::NONE) {
  TRI_ASSERT(iid != 0);
  _unique = false;
  _sparse = true;

  _coverParams.fromVelocyPack(info);
  if (_fields.size() == 1) {
    bool geoJson =
        basics::VelocyPackHelper::getBooleanValue(info, "geoJson", false);
    // geojson means [<longitude>, <latitude>] or
    // json object {type:"<name>, coordinates:[]}.
    _variant = geoJson ? IndexVariant::COMBINED_GEOJSON
                       : IndexVariant::COMBINED_LAT_LON;

    auto& loc = _fields[0];
    _location.reserve(loc.size());
    for (auto const& it : loc) {
      _location.emplace_back(it.name);
    }
  } else if (_fields.size() == 2) {
    _variant = IndexVariant::INDIVIDUAL_LAT_LON;
    auto& lat = _fields[0];
    _latitude.reserve(lat.size());
    for (auto const& it : lat) {
      _latitude.emplace_back(it.name);
    }
    auto& lon = _fields[1];
    _longitude.reserve(lon.size());
    for (auto const& it : lon) {
      _longitude.emplace_back(it.name);
    }
  } else {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
        "RocksDBGeoS2Index can only be created with one or two fields.");
  }
  TRI_ASSERT(_variant != IndexVariant::NONE);
}

/// @brief return a JSON representation of the index
void RocksDBGeoS2Index::toVelocyPack(VPackBuilder& builder,
                                         bool withFigures,
                                         bool forPersistence) const {
  TRI_ASSERT(_variant != IndexVariant::NONE);
  builder.openObject();
  // Basic index
  RocksDBIndex::toVelocyPack(builder, withFigures, forPersistence);

  builder.add("geoJson",
              VPackValue(_variant == IndexVariant::COMBINED_GEOJSON));
  // geo indexes are always non-unique
  // geo indexes are always sparse.
  builder.add("unique", VPackValue(false));
  builder.add("sparse", VPackValue(true));
  builder.close();
}

/// @brief Test if this index matches the definition
bool RocksDBGeoS2Index::matchesDefinition(VPackSlice const& info) const {
  TRI_ASSERT(_variant != IndexVariant::NONE);
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
    bool geoJson1 =
        basics::VelocyPackHelper::getBooleanValue(info, "geoJson", false);
    bool geoJson2 = _variant == IndexVariant::COMBINED_GEOJSON;
    if (geoJson1 != geoJson2) {
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
  TRI_ASSERT(!opts.evaluateFCalls); // should not get here without
  TRI_ASSERT(node != nullptr);

  geo::QueryParams params;
  params.sorted = opts.sorted;
  params.ascending = opts.ascending;
  geo::AqlUtils::parseCondition(node, reference, params);
  TRI_ASSERT(!opts.sorted || params.centroid != geo::Coordinate::Invalid());

  // params.cover.worstIndexedLevel < _coverParams.worstIndexedLevel
  // is not necessary, > would be missing entries.
  params.cover.worstIndexedLevel = _coverParams.worstIndexedLevel;
  if (params.cover.bestIndexedLevel > _coverParams.bestIndexedLevel) {
    // it is unnessesary to use a better level than configured
    params.cover.bestIndexedLevel = _coverParams.bestIndexedLevel;
  }
  return new NearIterator(_collection, trx, mmdr, this, std::move(params));
}

Result RocksDBGeoS2Index::parse(VPackSlice const& doc,
                                std::vector<S2CellId>& cells,
                                geo::Coordinate& centroid) const {
  if (_variant == IndexVariant::COMBINED_GEOJSON) {
    VPackSlice loc = doc.get(_location);
    if (loc.isArray()) {
      return geo::GeoUtils::indexCellsLatLng(loc, true, cells, centroid);
    }
    S2RegionCoverer coverer;
    _coverParams.configureS2RegionCoverer(&coverer);
    return geo::GeoUtils::indexCellsGeoJson(&coverer, loc, cells, centroid);
  } else if (_variant == IndexVariant::COMBINED_LAT_LON) {
    VPackSlice loc = doc.get(_location);
    return geo::GeoUtils::indexCellsLatLng(loc, false, cells, centroid);
  } else if (_variant == IndexVariant::INDIVIDUAL_LAT_LON) {
    VPackSlice lon = doc.get(_longitude);
    VPackSlice lat = doc.get(_latitude);
    if (!lon.isNumber() || !lat.isNumber()) {
      return TRI_ERROR_BAD_PARAMETER;
    }
    centroid.latitude = lat.getNumericValue<double>();
    centroid.longitude = lon.getNumericValue<double>();
    return geo::GeoUtils::indexCells(centroid, cells);
  }
  return TRI_ERROR_INTERNAL;
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
  Result res = parse(doc, cells, centroid);
  if (res.fail()) {
    // Invalid, no insert. Index is sparse
    return res.is(TRI_ERROR_BAD_PARAMETER) ? IndexResult() : res;
  }
  TRI_ASSERT(!cells.empty() &&
             std::abs(centroid.latitude) <= 90.0 &&
             std::abs(centroid.longitude) <= 180.0);

  RocksDBValue val = RocksDBValue::S2Value(centroid);
  RocksDBKeyLeaser key(trx);
  // FIXME: can we rely on the region coverer to return
  // the same cells everytime for the same parameters ?
  for (S2CellId cell : cells) {
    //LOG_TOPIC(ERR, Logger::FIXME) << "[Insert] " << cell;
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

  Result res = parse(doc, cells, centroid);
  if (res.fail()) {
    // Invalid, no insert. Index is sparse
    return res.is(TRI_ERROR_BAD_PARAMETER) ? IndexResult() : res;
  }
  TRI_ASSERT(!cells.empty() &&
             std::abs(centroid.latitude) <= 90.0 &&
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

void RocksDBGeoS2Index::truncate(transaction::Methods* trx) {
  RocksDBIndex::truncate(trx);
  // GeoIndex_reset(_geoIndex, RocksDBTransactionState::toMethods(trx));
}
