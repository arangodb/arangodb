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

#include "MMFilesGeoIndex.h"

#include "Aql/Ast.h"
#include "Aql/AstNode.h"
#include "Aql/SortCondition.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringRef.h"
#include "Basics/VelocyPackHelper.h"
#include "Geo/GeoUtils.h"
#include "GeoIndex/Near.h"
#include "Indexes/IndexIterator.h"
#include "Indexes/IndexResult.h"
#include "Logger/Logger.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ManagedDocumentResult.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;

template <typename CMP = geo_index::DocumentsAscending>
struct NearIterator final : public IndexIterator {
  /// @brief Construct an RocksDBGeoIndexIterator based on Ast Conditions
  NearIterator(LogicalCollection* collection, transaction::Methods* trx,
               ManagedDocumentResult* mmdr, MMFilesGeoIndex const* index,
               geo::QueryParams&& params)
      : IndexIterator(collection, trx, index),
        _index(index),
        _near(std::move(params)),
        _mmdr(mmdr) {
    if (!params.fullRange) {
      estimateDensity();
    }
  }

  ~NearIterator() {}

  char const* typeName() const override { return "s2-index-iterator"; }

  /// internal retrieval loop
  inline bool nextToken(
      std::function<bool(geo_index::Document const& gdoc)>&& cb, size_t limit) {
    if (_near.isDone()) {
      // we already know that no further results will be returned by the index
      TRI_ASSERT(!_near.hasNearest());
      return false;
    }

    while (limit > 0 && !_near.isDone()) {
      while (limit > 0 && _near.hasNearest()) {
        if (cb(_near.nearest())) {
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
        [this, &cb](geo_index::Document const& gdoc) -> bool {
          if (!_collection->readDocument(_trx, gdoc.token, *_mmdr)) {
            return false;  // skip
          }
          VPackSlice doc(_mmdr->vpack());
          geo::FilterType const ft = _near.filterType();
          if (ft != geo::FilterType::NONE) {  // expensive test
            geo::ShapeContainer const& filter = _near.filterShape();
            TRI_ASSERT(!filter.empty());
            geo::ShapeContainer test;
            Result res = _index->shape(doc, test);
            TRI_ASSERT(res.ok() &&
                       !test.empty());  // this should never fail here
            if (res.fail() ||
                (ft == geo::FilterType::CONTAINS && !filter.contains(&test)) ||
                (ft == geo::FilterType::INTERSECTS &&
                 !filter.intersects(&test))) {
              return false;  // skip
            }
          }
          cb(gdoc.token, doc);  // return result
          return true;
        },
        limit);
  }

  bool next(LocalDocumentIdCallback const& cb, size_t limit) override {
    return nextToken(
        [this, &cb](geo_index::Document const& gdoc) -> bool {
          geo::FilterType const ft = _near.filterType();
          if (ft != geo::FilterType::NONE) {
            geo::ShapeContainer const& filter = _near.filterShape();
            TRI_ASSERT(!filter.empty());
            if (!_collection->readDocument(_trx, gdoc.token, *_mmdr)) {
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
          cb(gdoc.token);  // return result
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
    MMFilesGeoIndex::IndexTree const& tree = _index->tree();
    // list of sorted intervals to scan
    std::vector<geo::Interval> const scan = _near.intervals();

    auto it = tree.begin();
    for (size_t i = 0; i < scan.size(); i++) {
      geo::Interval const& interval = scan[i];
      TRI_ASSERT(interval.min <= interval.max);

      // intervals are sorted and likely consecutive, try to avoid seeks
      // by checking whether we are in the range already
      bool seek = true;
      if (i > 0) {
        TRI_ASSERT(scan[i - 1].max < interval.min);
        if (it == tree.end()) {  // no more valid keys after this
          break;
        } else if (it->first > interval.max) {
          continue;  // beyond range already
        } else if (interval.min <= it->first) {
          seek = false;  // already in range: min <= key <= max
          TRI_ASSERT(it->first <= interval.max);
        }
      }

      if (seek) {  // try to avoid seeking at all cost
        it = tree.lower_bound(interval.min);
      }

      while (it != tree.end() && it->first <= interval.max) {
        _near.reportFound(it->second.documentId, it->second.centroid);
        it++;
      }
    }

    _near.didScanIntervals();  // calculate next bounds
  }

  /// find the first indexed entry to estimate the # of entries
  /// around our target coordinates
  void estimateDensity() {
    MMFilesGeoIndex::IndexTree const& tree = _index->tree();
    if (!tree.empty()) {
      S2CellId cell = S2CellId(_near.origin());
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
  MMFilesGeoIndex const* _index;
  geo_index::NearUtils<CMP> _near;
  ManagedDocumentResult* _mmdr;
};
typedef NearIterator<geo_index::DocumentsAscending> LegacyIterator;

MMFilesGeoIndex::MMFilesGeoIndex(TRI_idx_iid_t iid,
                                 LogicalCollection* collection,
                                 VPackSlice const& info,
                                 std::string const& typeName)
    : MMFilesIndex(iid, collection, info),
      geo_index::Index(info, _fields),
      _typeName(typeName) {
  TRI_ASSERT(iid != 0);
  _unique = false;
  _sparse = true;
  TRI_ASSERT(_variant != geo_index::Index::Variant::NONE);
}

size_t MMFilesGeoIndex::memory() const { return _tree.bytes_used(); }

/// @brief return a JSON representation of the index
void MMFilesGeoIndex::toVelocyPack(VPackBuilder& builder, bool withFigures,
                                   bool forPersistence) const {
  TRI_ASSERT(_variant != geo_index::Index::Variant::NONE);
  builder.openObject();
  // Basic index
  // RocksDBIndex::toVelocyPack(builder, withFigures, forPersistence);
  MMFilesIndex::toVelocyPack(builder, withFigures, forPersistence);
  _coverParams.toVelocyPack(builder);
  builder.add("geoJson",
              VPackValue(_variant == geo_index::Index::Variant::GEOJSON));
  // geo indexes are always non-unique
  builder.add(
    arangodb::StaticStrings::IndexUnique,
    arangodb::velocypack::Value(false)
  );
  // geo indexes are always sparse.
  builder.add(
    arangodb::StaticStrings::IndexSparse,
    arangodb::velocypack::Value(true)
  );
  builder.close();
}

/// @brief Test if this index matches the definition
bool MMFilesGeoIndex::matchesDefinition(VPackSlice const& info) const {
  TRI_ASSERT(_variant != geo_index::Index::Variant::NONE);
  TRI_ASSERT(info.isObject());
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  auto typeSlice = info.get(arangodb::StaticStrings::IndexType);
  TRI_ASSERT(typeSlice.isString());
  StringRef typeStr(typeSlice);
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
    StringRef idRef(value);
    return idRef == std::to_string(_iid);
  }

  if (_unique != basics::VelocyPackHelper::getBooleanValue(
                   info, arangodb::StaticStrings::IndexUnique.c_str(), false
                 )
     ) {
    return false;
  }

  if (_sparse != basics::VelocyPackHelper::getBooleanValue(
                   info, arangodb::StaticStrings::IndexSparse.c_str(), true
                 )
     ) {
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
    bool geoJson1 =
        basics::VelocyPackHelper::getBooleanValue(info, "geoJson", false);
    bool geoJson2 = _variant == geo_index::Index::Variant::GEOJSON;
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

Result MMFilesGeoIndex::insert(transaction::Methods*,
                               LocalDocumentId const& documentId,
                               VPackSlice const& doc, OperationMode mode) {
  // covering and centroid of coordinate / polygon / ...
  size_t reserve = _variant == Variant::GEOJSON ? 8 : 1;
  std::vector<S2CellId> cells;
  cells.reserve(reserve);
  S2Point centroid;
  Result res = geo_index::Index::indexCells(doc, cells, centroid);

  if (res.fail()) {
    // Invalid, no insert. Index is sparse
    return res.is(TRI_ERROR_BAD_PARAMETER) ? IndexResult() : res;
  }
  // LOG_TOPIC(ERR, Logger::FIXME) << "Inserting #cells " << cells.size() << "
  // doc: " << doc.toJson() << " center: " << centroid.toString();
  TRI_ASSERT(!cells.empty());
  TRI_ASSERT(S2::IsUnitLength(centroid));

  IndexValue value(documentId, std::move(centroid));
  for (S2CellId cell : cells) {
    _tree.insert(std::make_pair(cell, value));
  }

  return IndexResult();
}

Result MMFilesGeoIndex::remove(transaction::Methods*,
                               LocalDocumentId const& documentId,
                               VPackSlice const& doc, OperationMode mode) {
  // covering and centroid of coordinate / polygon / ...
  size_t reserve = _variant == Variant::GEOJSON ? 8 : 1;
  std::vector<S2CellId> cells(reserve);
  S2Point centroid;
  Result res = geo_index::Index::indexCells(doc, cells, centroid);

  if (res.fail()) {  // might occur if insert is rolled back
    // Invalid, no insert. Index is sparse
    return res.is(TRI_ERROR_BAD_PARAMETER) ? IndexResult() : res;
  }
  // LOG_TOPIC(ERR, Logger::FIXME) << "Removing #cells " << cells.size() << "
  // doc: " << doc.toJson();
  TRI_ASSERT(!cells.empty());

  for (S2CellId cell : cells) {
    auto it = _tree.lower_bound(cell);
    while (it != _tree.end() && it->first == cell) {
      if (it->second.documentId == documentId) {
        it = _tree.erase(it);
      } else {
        ++it;
      }
    }
  }
  return IndexResult();
}

/// @brief creates an IndexIterator for the given Condition
IndexIterator* MMFilesGeoIndex::iteratorForCondition(
    transaction::Methods* trx, ManagedDocumentResult* mmdr,
    arangodb::aql::AstNode const* node,
    arangodb::aql::Variable const* reference,
    IndexIteratorOptions const& opts) {
  TRI_ASSERT(!isSorted() || opts.sorted);
  TRI_ASSERT(node != nullptr);

  geo::QueryParams params;
  params.sorted = opts.sorted;
  params.ascending = opts.ascending;
  params.pointsOnly = pointsOnly();
  params.fullRange = opts.fullRange;
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

  // why does this have to be shit?
  if (params.ascending) {
    return new NearIterator<geo_index::DocumentsAscending>(
        _collection, trx, mmdr, this, std::move(params));
  } else {
    return new NearIterator<geo_index::DocumentsDescending>(
        _collection, trx, mmdr, this, std::move(params));
  }
}

void MMFilesGeoIndex::unload() {
  _tree.clear();  // TODO: do we need load?
}

namespace {
void retrieveNear(MMFilesGeoIndex const& index, transaction::Methods* trx,
                  double lat, double lon, double radius, size_t count,
                  std::string const& attributeName, VPackBuilder& builder) {
  geo::QueryParams params;
  params.origin = S2LatLng::FromDegrees(lat, lon);
  params.sorted = true;
  if (radius > 0.0) {
    params.maxDistance = radius;
    params.fullRange = true;
  }
  params.pointsOnly = index.pointsOnly();
  params.limit = count;
  size_t limit = (count > 0) ? count : SIZE_MAX;

  ManagedDocumentResult mmdr;
  LogicalCollection* collection = index.collection();
  LegacyIterator iter(collection, trx, &mmdr, &index, std::move(params));
  auto fetchDoc = [&](geo_index::Document const& gdoc) -> bool {
    bool read = collection->readDocument(trx, gdoc.token, mmdr);
    if (!read) {
      TRI_ASSERT(false);
      return false;
    }
    VPackSlice doc(mmdr.vpack());
    // add to builder results
    if (!attributeName.empty()) {
      double distance = gdoc.distAngle.radians() * geo::kEarthRadiusInMeters;
      // We have to copy the entire document
      VPackObjectBuilder docGuard(&builder);
      builder.add(attributeName, VPackValue(distance));
      for (auto const& entry : VPackObjectIterator(doc, true)) {
        std::string key = entry.key.copyString();
        if (key != attributeName) {
          builder.add(key, entry.value);
        }
      }
    } else {
      mmdr.addToBuilder(builder, true);
    }

    return true;
  };

  bool more = iter.nextToken(fetchDoc, limit);
  TRI_ASSERT(count > 0 || !more);
}
}  // namespace

/// @brief looks up all points within a given radius
void MMFilesGeoIndex::withinQuery(transaction::Methods* trx, double lat,
                                  double lon, double radius,
                                  std::string const& attributeName,
                                  VPackBuilder& builder) const {
  ::retrieveNear(*this, trx, lat, lon, radius, 0, attributeName, builder);
}

void MMFilesGeoIndex::nearQuery(transaction::Methods* trx, double lat,
                                double lon, size_t count,
                                std::string const& attributeName,
                                VPackBuilder& builder) const {
  ::retrieveNear(*this, trx, lat, lon, -1.0, count, attributeName, builder);
}
