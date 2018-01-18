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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "MMFilesGeoS2Index.h"

#include "Aql/Ast.h"
#include "Aql/AstNode.h"
#include "Aql/SortCondition.h"
#include "Basics/StringRef.h"
#include "Basics/VelocyPackHelper.h"
#include "Geo/GeoUtils.h"
#include "Geo/Near.h"
#include "Indexes/IndexIterator.h"
#include "Indexes/IndexResult.h"
#include "Logger/Logger.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ManagedDocumentResult.h"

using namespace arangodb;

template <typename CMP = geo::DocumentsAscending>
class NearIterator final : public IndexIterator {
 public:
  /// @brief Construct an RocksDBGeoIndexIterator based on Ast Conditions
  NearIterator(LogicalCollection* collection, transaction::Methods* trx,
               ManagedDocumentResult* mmdr, MMFilesGeoS2Index const* index,
               geo::QueryParams&& params)
      : IndexIterator(collection, trx, mmdr, index),
        _index(index),
        _near(std::move(params)) {
    estimateDensity();
  }

  char const* typeName() const override { return "s2-index-iterator"; }

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
            return false; // skip
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
              return false; // skip
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
            TRI_ASSERT(filter.type() != geo::ShapeContainer::Type::EMPTY);
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
    MMFilesGeoS2Index::IndexTree const& tree = _index->tree();

    // list of sorted intervals to scan
    std::vector<geo::Interval> const scan = _near.intervals();
    //LOG_TOPIC(INFO, Logger::FIXME) << "# intervals: " << scan.size();
    //size_t seeks = 0;

    auto it = tree.begin();
    for (size_t i = 0; i < scan.size(); i++) {
      geo::Interval const& interval = scan[i];
      TRI_ASSERT(interval.min <= interval.max);

      // intervals are sorted and likely consecutive, try to avoid seeks
      // by checking whether we are in the range already
      bool seek = true;
      if (i > 0) {
        TRI_ASSERT(scan[i - 1].max < interval.min);
        if (it == tree.end()) { // no more valid keys after this
          break;
        } else if (it->first > interval.max) {
          continue; // beyond range already
        } else if (interval.min <= it->first) {
          seek = false; // already in range: min <= key <= max
          TRI_ASSERT(it->first <= interval.max);
        }
      }

      if (seek) { // try to avoid seeking at all cost
        it = tree.lower_bound(interval.min); //seeks++;
      }
      
      while (it != tree.end() && it->first <= interval.max) {
        _near.reportFound(it->second.documentId, it->second.centroid);
        it++;
      }
    }
    //LOG_TOPIC(INFO, Logger::FIXME) << "# seeks: " << seeks;
  }

  /// find the first indexed entry to estimate the # of entries
  /// around our target coordinates
  void estimateDensity() {
    MMFilesGeoS2Index::IndexTree const& tree = _index->tree();
    if (!tree.empty()) {
      S2CellId cell = S2CellId::FromPoint(_near.origin());
      auto it = tree.upper_bound(cell);
      if (it == tree.end()) {
        it = tree.lower_bound(cell);
      }
      if (it != tree.end()) {
        _near.estimateDensity(it->second.centroid);
      }
    }
  }

 private:
  MMFilesGeoS2Index const* _index;
  geo::NearUtils<CMP> _near;
};


MMFilesGeoS2Index::MMFilesGeoS2Index(TRI_idx_iid_t iid,
                                     LogicalCollection* collection,
                                     VPackSlice const& info)
: MMFilesIndex(iid, collection, info), geo::Index(info) {
  TRI_ASSERT(iid != 0);
  _unique = false;
  _sparse = true;
  geo::Index::initalize(info, _fields); // initalize mixin fields
  TRI_ASSERT(_variant != geo::Index::Variant::NONE);
}

size_t MMFilesGeoS2Index::memory() const { return _tree.bytes_used(); }

/// @brief return a JSON representation of the index
void MMFilesGeoS2Index::toVelocyPack(VPackBuilder& builder, bool withFigures,
                                     bool forPersistence) const {
  TRI_ASSERT(_variant != geo::Index::Variant::NONE);
  builder.openObject();
  // Basic index
  // RocksDBIndex::toVelocyPack(builder, withFigures, forPersistence);
  MMFilesIndex::toVelocyPack(builder, withFigures, forPersistence);
  _coverParams.toVelocyPack(builder);
  builder.add("geoJson",
              VPackValue(_variant == geo::Index::Variant::COMBINED_GEOJSON));
  // geo indexes are always non-unique
  // geo indexes are always sparse.
  builder.add("unique", VPackValue(false));
  builder.add("sparse", VPackValue(true));
  builder.close();
}

/// @brief Test if this index matches the definition
bool MMFilesGeoS2Index::matchesDefinition(VPackSlice const& info) const {
  TRI_ASSERT(_variant != geo::Index::Variant::NONE);
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
    bool geoJson2 = _variant == geo::Index::Variant::COMBINED_GEOJSON;
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
    if (!basics::AttributeName::isIdentical(_fields[i], translate, false)) {
      return false;
    }
  }
  return true;
}

Result MMFilesGeoS2Index::insert(transaction::Methods*,
                                 LocalDocumentId const& documentId,
                                 VPackSlice const& doc, OperationMode mode) {
  // covering and centroid of coordinate / polygon / ...
  std::vector<S2CellId> cells;
  geo::Coordinate centroid(-1.0, -1.0);
  Result res = geo::Index::indexCells(doc, cells, centroid);
  if (res.fail()) {
    // Invalid, no insert. Index is sparse
    return res.is(TRI_ERROR_BAD_PARAMETER) ? IndexResult() : res;
  }
  TRI_ASSERT(!cells.empty() && std::abs(centroid.latitude) <= 90.0 &&
             std::abs(centroid.longitude) <= 180.0);
  IndexValue value(documentId, std::move(centroid));

  // RocksDBValue val = RocksDBValue::S2Value(centroid);
  // RocksDBKeyLeaser key(trx);
  // FIXME: can we rely on the region coverer to return
  // the same cells everytime for the same parameters ?
  for (S2CellId cell : cells) {
    _tree.insert(std::make_pair(cell, value));
  }

  return IndexResult();
}

Result MMFilesGeoS2Index::remove(transaction::Methods*,
                                 LocalDocumentId const& documentId,
                                 VPackSlice const& doc, OperationMode mode) {
  // covering and centroid of coordinate / polygon / ...
  std::vector<S2CellId> cells;
  geo::Coordinate centroid(-1, -1);

  Result res = geo::Index::indexCells(doc, cells, centroid);
  if (res.fail()) {
    TRI_ASSERT(false);  // this should never fail here
    // Invalid, no insert. Index is sparse
    return res.is(TRI_ERROR_BAD_PARAMETER) ? IndexResult() : res;
  }
  TRI_ASSERT(!cells.empty() && std::abs(centroid.latitude) <= 90.0 &&
             std::abs(centroid.longitude) <= 180.0);

  // FIXME: can we rely on the region coverer to return
  // the same cells everytime for the same parameters ?
  for (S2CellId cell : cells) {
    auto it = _tree.lower_bound(cell);
    while (it != _tree.end() && it->first == cell) {
      if (it->second.documentId == documentId) {
        _tree.erase(it);
      }
      it++;
    }
  }
  return IndexResult();
}

/// @brief creates an IndexIterator for the given Condition
IndexIterator* MMFilesGeoS2Index::iteratorForCondition(
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
  geo::Index::parseCondition(node, reference, params);
  
  // FIXME: Optimize away
  if (!params.sorted) {
    TRI_ASSERT(params.filterType != geo::FilterType::NONE);
    TRI_ASSERT(params.filterShape.type() != geo::ShapeContainer::Type::EMPTY);
    params.sorted = true;
    params.origin = params.filterShape.centroid();
  }
  TRI_ASSERT(!opts.sorted || params.origin.isValid());

  // params.cover.worstIndexedLevel < _coverParams.worstIndexedLevel
  // is not necessary, > would be missing entries.
  params.cover.worstIndexedLevel = _coverParams.worstIndexedLevel;
  if (params.cover.bestIndexedLevel > _coverParams.bestIndexedLevel) {
    // it is unnessesary to use a better level than configured
    params.cover.bestIndexedLevel = _coverParams.bestIndexedLevel;
  }
  if (params.ascending) {
    return new NearIterator<geo::DocumentsAscending>(
        _collection, trx, mmdr, this, std::move(params));
  } else {
    return new NearIterator<geo::DocumentsDescending>(
        _collection, trx, mmdr, this, std::move(params));
  }
}

void MMFilesGeoS2Index::unload() {
  _tree.clear();  // TODO: do we need load?
}
