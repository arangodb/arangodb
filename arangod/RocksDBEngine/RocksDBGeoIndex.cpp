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

#include "RocksDBGeoIndex.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Aql/Ast.h"
#include "Aql/AstNode.h"
#include "Aql/Function.h"
#include "Aql/SortCondition.h"
#include "Basics/StaticStrings.h"
#include "Basics/VelocyPackHelper.h"
#include "GeoIndex/Covering.h"
#include "GeoIndex/Near.h"
#include "Logger/Logger.h"
#include "Logger/LogMacros.h"
#include "RocksDBEngine/RocksDBColumnFamilyManager.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "RocksDBEngine/RocksDBTransactionMethods.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/PhysicalCollection.h"
#include "VocBase/LogicalCollection.h"

#include <rocksdb/db.h>
#include <s2/s2cell_id.h>

#include <velocypack/Iterator.h>

using namespace arangodb;

// Here are some explanations on how this whole geo index technology works
// together (reverse engineered from the code):
//
// The classes RDBNearIterator and RDBCoveringIterator organise the actual
// work of looking up things in the geo index. But before we talk about this,
// let's put this in a wider context and link to other places in the code
// base.
//
// A geo index is a specific type of index, which indexes one or two
// attributes in the documents of a collection for its "geo content".
// "Geo content" can be locations on earth (longitude/lattitude), or can
// be "geojson" objects like polygons. Simplified a lot, the index then allows
// to quickly find stuff which is "close to the indexed geo content" on earth.
//
// This works by configuring an "index factory" in
// `arangod/Indexes/IndexFactor.cpp` via the `IndexFactory::emplace` method.
// This is done in `arangod/RocksDBEngine/RocksDBIndexFactor.cpp` in the
// constructor of `RocksDBIndexFactory` for RocksDB and in
// `arangod/ClusterEngine/ClusterIndexFactor.cpp` in
// `ClusterIndexFactory::linkIndexFactories` for the cluster engine.
// These factories are implemented in the same file, for example
// as `GeoIndexFactory` for RocksDB. This index factory produces then
// an object of type `RocksDBGeoIndex` and this is responsible for
// storing stuff in RocksDB for the indexed data. The corresponding
// methods can be found in this file here. This is how we produce the
// indexed data.
//
// The `LogicalCollection` object knows about its indexes, and so the query
// optimizer for AQL can know about them.
//
// There are essentially three types of query:
//  (1) Find everything within a radius (assuming a geo index on the `geo`
//      attribute of our collection `coll`:
//      FOR d IN coll
//        FILTER GEO_DISTANCE(obj, d.geo) <= @radius
//        RETURN d
//      This might or might not be sorted by distance from the target. We
//      can also use `>=` or `<` or `>` or a combination to prescribe
//      the area of an annulus.
//  (2) Find everything in the database, which is contained in a given object:
//      FOR d IN coll
//        FILTER GEO_CONTAINS(obj, d.geo)
//        RETURN d
//  (3) Find everything in the database, which intersects a given object:
//      FOR d IN coll
//        FILTER GEO_INTERSECTS(obj, d.geo)
//        RETURN d
// In principle, there could also be:
//  (4) Find everything in the database, which contains a given object:
//      FOR d IN coll
//        FILTER GEO_CONTAINS(d.geo, obj)
//        RETURN d
//      but we do not support this. It will be executed by steam without
//      using the geo index.
//
// All of these can get a LIMIT clause and we can take advantage of this
// knowledge when the LIMIT is given in the QueryParams. Furthermore,
// they can get a lower and upper GEO_DISTANCE bound (centroid distance),
// which are detected by FILTER statements like:
//
//   FILTER GEO_DISTANCE(d.geo, obj) <= X
//
// and
//
//   FILTER GEO_DISTANCE(d.geo, obj) >= Y
//
// Finally, each such query can also observe a SORT clause like this:
//
//   SORT GEO_DISTANCE(d.geo, obj) ASC
//
// where ASC can also be DESC, and the ASC sorting is implicitly always
// present. Currently it does not seem to be possible to do an unsorted
// query, because ASC is implied if no sort is given.
//
// Note that (1) only uses the centroid of `obj`, which is a rather
// unconventional and unintuitive definition of distance. In this case,
// there could be an additional SORT clause to sort by distance.
//
// The query optimizer has to recognize all these possibilities. It does so
// by means of the optimizer rule `arangodb::aql::geoIndexRule` which can
// be found in `arangod/Aql/OptimizerRules.cpp`. It looks at the abstract
// syntax tree of the query and sees if any `EnumerateCollection` node
// can be optimized into an `IndexNode` which uses the geo index. At the
// end of the day, it puts together a `GeoIndexInfo` which is translated
// into options for the `IndexNode` and a "condition node" to specify
// the filtering and sorting conditions.
//
// When it comes to the execution of the query plan, the IndexBlock will call
// `iteratorForCondition` on the index object and hand in the condition
// for further processing here. Therefore, it is this method, which in the
// end organises a cursor for the index lookup.
//
// The algorithms can take into account one more piece of information, namely
// whether it is known that all objects indexed in the geo index are
// known to be points. In this case a number of optimizations are
// possible, which are in general not valid for the general GeoJSON
// case.
//
// Altogether, this amounts to a total of 60 possible combinations (12
// "near" query types, since they always have to have an upper bound for
// the GEO_DISTANCE, 24 "contains" query types and 24 "intersects" query types.
//
// Depending on the case, we either deploy a `RDBNearIterator` object or
// a `RDBCoveringIterator` object, both implemented in this file here.
// The latter is a simpler object, which only uses a covering of the search
// object. It can only be used if we are dealing with a "contains" or
// "intersects" query with no restrictions on the `GEO_DISTANCE`, and if
// no sorting by `GEO_DISTANCE` is needed.
//
// Both objects get told what to look for by the geo::QueryParams,
// and they get access to a (read) transaction trx, a logical
// collection and a geo index to use. Both objects are supposed to
// be an `IndexIterator`, this means, once the query is set up, it
// supports the next/nextDocument methods by implementing the
// nextImpl/nextDocumentImpl virtual methods. Furthermore,
// it needs to support skip and friends.
//
// For the `RDBNearIterator` object is templated on the sorting
// direction, which can be arangodb::geo_index::DocumentsAscending
// or `...::DocumentsDescending`, which means the sorting order by
// the distance to the query point/object. In case of an object, the
// distance to the centroid of the object is meant.
//
// The `RDBNearIterator` object does not do all the work on its own. Rather,
// it employs the help of a `geo_index::NearUtils` object. The `NearUtils`
// object is responsible for maintaining a priority queue `GeoDocumentsQueue`
// which is supposed to return the "closest" (in case of ascending) or
// "furthest" (in case of descending) solutions first. Furthermore, the
// `NearUtils` object can do some filtering with `contains` or `intersects`.
//
// The `NearUtils` object uses the following parameters:
//  - `minDistanceRad` and `maxDistanceRad` to limit search to a ring
//  - `origin` as the center of the search
//  - the information about ascending or descending search
//  - the flag `pointsOnly` which indicates that only points are indexed
//  - a filtering object and a filtering type (NONE, CONTAINS or INTERSECTS)
//
// The `NearUtils` object then has to organize the search either ascending
// or descending, and to this end produces a list of intervals to scan in
// the index. This is then done in the `RDBNearIterator` object in
// `performScan`. Whatever is found in the index is then reported back
// to the `NearUtils` object via `reportFound` and that calls the callback
// we got from the outside.
//
// Similarly, the `RDBCoveringIterator` object does not do all the work on
// its own. Rather, it employs the help of a `geo_index::CoveringUtils`
// object. The `CoveringUtils` object is responsible for maintaining a
// deque `GeoDocumentsQueue` which is supposed to return the objects which
// are found in the index. The documents come in any order, but are
// deduplicated. Sometimes the index finds too many objects, but the
// `CoveringUtils` do a final step to filter out wrong results.
//
// The `CoveringUtils` object uses the following parameters:
//  - the flag `pointsOnly` which indicates that only points are indexed
//  - a filtering object and a filtering type (CONTAINS or INTERSECTS).

template<typename CMP = geo_index::DocumentsAscending>
class RDBNearIterator final : public IndexIterator {
 public:
  /// @brief Construct an RocksDBGeoIndexIterator based on Ast Conditions
  RDBNearIterator(ResourceMonitor& monitor, LogicalCollection* collection,
                  transaction::Methods* trx, RocksDBGeoIndex const* index,
                  geo::QueryParams&& params)
      : IndexIterator(collection, trx, ReadOwnWrites::no),
        // geo index never needs to observe own writes since it cannot be used
        // for an UPSERT subquery
        _index(index),
        _near(std::move(params)) {
    RocksDBTransactionMethods* mthds =
        RocksDBTransactionState::toMethods(trx, _collection->id());
    _iter = mthds->NewIterator(_index->columnFamily(), {});
    TRI_ASSERT(_index->columnFamily()->GetID() ==
               RocksDBColumnFamilyManager::get(
                   RocksDBColumnFamilyManager::Family::GeoIndex)
                   ->GetID());
    estimateDensity();
  }

  std::string_view typeName() const noexcept final {
    return "geo-index-iterator";
  }

  /// internal retrieval loop
  template<typename F>
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

  bool nextDocumentImpl(DocumentCallback const& cb, uint64_t limit) override {
    return nextToken(
        [this, &cb](geo_index::Document const& gdoc) -> bool {
          bool result = true;  // this is updated by the callback
          auto callback = [&](LocalDocumentId, aql::DocumentData&& data,
                              VPackSlice doc) {
            geo::FilterType const ft = _near.filterType();
            if (ft != geo::FilterType::NONE) {  // expensive test
              geo::ShapeContainer const& filter = _near.filterShape();
              TRI_ASSERT(filter.type() != geo::ShapeContainer::Type::EMPTY);
              geo::ShapeContainer test;
              Result res = _index->shape(doc, test);
              TRI_ASSERT(res.ok());  // this should never fail here
              if (res.fail() ||
                  (ft == geo::FilterType::CONTAINS && !filter.contains(test)) ||
                  (ft == geo::FilterType::INTERSECTS &&
                   !filter.intersects(test))) {
                result = false;
                return false;
              }
            }
            cb(gdoc.token, std::move(data), doc);  // return document
            result = true;
            return true;
          };
          auto* physical = _collection->getPhysical();
          // geo index never needs to observe own writes
          if (!physical
                   ->lookup(_trx, gdoc.token, callback, {.countBytes = true})
                   .ok()) {
            return false;  // ignore document
          }
          return result;
        },
        limit);
  }

  bool nextImpl(LocalDocumentIdCallback const& cb, uint64_t limit) override {
    return nextToken(
        [this, &cb](geo_index::Document const& gdoc) -> bool {
          geo::FilterType const ft = _near.filterType();
          if (ft != geo::FilterType::NONE) {
            geo::ShapeContainer const& filter = _near.filterShape();
            TRI_ASSERT(!filter.empty());
            bool result = true;  // this is updated by the callback
            auto callback = [&](LocalDocumentId, aql::DocumentData&&,
                                VPackSlice doc) {
              geo::ShapeContainer test;
              Result res = _index->shape(doc, test);
              TRI_ASSERT(res.ok());  // this should never fail here
              if (res.fail() ||
                  (ft == geo::FilterType::CONTAINS && !filter.contains(test)) ||
                  (ft == geo::FilterType::INTERSECTS &&
                   !filter.intersects(test))) {
                result = false;
                return false;
              }
              return true;
            };
            auto* physical = _collection->getPhysical();
            // geo index never needs to observe own writes
            if (!physical
                     ->lookup(_trx, gdoc.token, callback, {.countBytes = true})
                     .ok()) {
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

  void resetImpl() final {
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
    // LOG_TOPIC("b1eea", INFO, Logger::ENGINES) << "# intervals: " <<
    // scan.size(); size_t seeks = 0;

    for (size_t i = 0; i < scan.size(); i++) {
      geo::Interval const& it = scan[i];
      TRI_ASSERT(it.range_min <= it.range_max);
      RocksDBKeyBounds bds = RocksDBKeyBounds::GeoIndex(
          _index->objectId(), it.range_min.id(), it.range_max.id());

      // intervals are sorted and likely consecutive, try to avoid seeks
      // by checking whether we are in the range already
      bool seek = true;
      if (i > 0) {
        TRI_ASSERT(scan[i - 1].range_max < it.range_min);
        if (!_iter->Valid()) {  // no more valid keys after this
          break;
        } else if (cmp->Compare(_iter->key(), bds.end()) > 0) {
          continue;  // beyond the range already
        } else if (cmp->Compare(bds.start(), _iter->key()) <= 0) {
          seek = false;  // already in range: min <= key <= max
          TRI_ASSERT(cmp->Compare(_iter->key(), bds.end()) <= 0);
        } else {  // cursor is positioned below min range key
          TRI_ASSERT(cmp->Compare(_iter->key(), bds.start()) < 0);
          int k = 10;  // try to catch the range
          while (k > 0 && _iter->Valid() &&
                 cmp->Compare(_iter->key(), bds.start()) < 0) {
            _iter->Next();
            --k;
          }
          seek =
              !_iter->Valid() || (cmp->Compare(_iter->key(), bds.start()) < 0);
        }
      }

      if (seek) {  // try to avoid seeking at all cost
        // LOG_TOPIC("0afaa", INFO, Logger::ENGINES) << "[Scan] seeking:" <<
        // it.min; seeks++;
        _iter->Seek(bds.start());
      }

      while (_iter->Valid() && cmp->Compare(_iter->key(), bds.end()) <= 0) {
        _near.reportFound(RocksDBKey::indexDocumentId(_iter->key()),
                          RocksDBValue::centroid(_iter->value()));
        _iter->Next();
      }

      // validate that Iterator is in a good shape and hasn't failed
      rocksutils::checkIteratorStatus(*_iter);
    }

    _near.didScanIntervals();  // calculate next bounds
    // LOG_TOPIC("e82ee", INFO, Logger::ENGINES) << "# seeks: " << seeks;
  }

  /// find the first indexed entry to estimate the # of entries
  /// around our target coordinates
  void estimateDensity() {
    S2CellId cell = S2CellId(_near.origin());

    RocksDBKeyLeaser key(_trx);
    key->constructGeoIndexValue(_index->objectId(), cell.id(),
                                LocalDocumentId(1));
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

class RDBCoveringIterator final : public IndexIterator {
 public:
  /// @brief Construct an RocksDBGeoIndexIterator based on Ast Conditions
  RDBCoveringIterator(ResourceMonitor& monitor, LogicalCollection* collection,
                      transaction::Methods* trx, RocksDBGeoIndex const* index,
                      geo::QueryParams&& params)
      : IndexIterator(collection, trx, ReadOwnWrites::no),
        // geo index never needs to observe own writes since it cannot be used
        // for an UPSERT subquery
        _index(index),
        _covering(std::move(params)) {
    RocksDBTransactionMethods* mthds =
        RocksDBTransactionState::toMethods(trx, _collection->id());
    _iter = mthds->NewIterator(_index->columnFamily(), {});
    TRI_ASSERT(_index->columnFamily()->GetID() ==
               RocksDBColumnFamilyManager::get(
                   RocksDBColumnFamilyManager::Family::GeoIndex)
                   ->GetID());
  }

  std::string_view typeName() const noexcept override {
    return "geo-index-covering-iterator";
  }

  /// internal retrieval loop
  template<typename F>
  inline bool nextToken(F&& cb, uint64_t limit) {
    if (_covering.isDone()) {
      // we already know that no further results will be returned by the index
      return false;
    }

    // We keep going, until either we have reached our limit or we have
    // scanned all intervals delivered by _covering:
    while (limit > 0 &&
           (!_covering.isDone() || _scanningInterval < _scan.size())) {
      if (!_covering.hasNext()) {
        performScan();
      }

      while (limit > 0 && _covering.hasNext()) {
        if (std::forward<F>(cb)(_covering.getNext())) {
          --limit;
        }
        _covering.next();
      }
    }
    return !_covering.isDone() || _scanningInterval < _scan.size();
  }

  bool nextDocumentImpl(DocumentCallback const& cb, uint64_t limit) override {
    return nextToken(
        [this, &cb](LocalDocumentId docid) -> bool {
          bool result = true;  // this is updated by the callback
          auto callback = [&](LocalDocumentId, aql::DocumentData&& data,
                              VPackSlice doc) {
            geo::FilterType const ft = _covering.filterType();
            geo::ShapeContainer const& filter = _covering.filterShape();
            TRI_ASSERT(filter.type() != geo::ShapeContainer::Type::EMPTY);
            geo::ShapeContainer test;
            Result res = _index->shape(doc, test);
            TRI_ASSERT(res.ok());  // this should never fail here
            if (res.fail() ||
                (ft == geo::FilterType::CONTAINS && !filter.contains(test)) ||
                (ft == geo::FilterType::INTERSECTS &&
                 !filter.intersects(test))) {
              result = false;
              return false;
            }
            cb(docid, std::move(data), doc);  // return document
            result = true;
            return true;
          };
          auto* physical = _collection->getPhysical();
          // geo index never needs to observe own writes
          if (!physical->lookup(_trx, docid, callback, {.countBytes = true})
                   .ok()) {
            return false;  // ignore document
          }
          return result;
        },
        limit);
  }

  bool nextImpl(LocalDocumentIdCallback const& cb, uint64_t limit) override {
    return nextToken(
        [this, &cb](LocalDocumentId docid) -> bool {
          geo::FilterType const ft = _covering.filterType();
          if (ft != geo::FilterType::NONE) {
            geo::ShapeContainer const& filter = _covering.filterShape();
            TRI_ASSERT(!filter.empty());
            bool result = true;  // this is updated by the callback
            auto callback = [&](LocalDocumentId, aql::DocumentData&&,
                                VPackSlice doc) {
              geo::ShapeContainer test;
              Result res = _index->shape(doc, test);
              TRI_ASSERT(res.ok());  // this should never fail here
              if (res.fail() ||
                  (ft == geo::FilterType::CONTAINS && !filter.contains(test)) ||
                  (ft == geo::FilterType::INTERSECTS &&
                   !filter.intersects(test))) {
                result = false;
                return false;
              }
              return true;
            };
            auto* physical = _collection->getPhysical();
            // geo index never needs to observe own writes
            if (!physical->lookup(_trx, docid, callback, {.countBytes = true})
                     .ok()) {
              return false;
            }
            if (!result) {
              return false;
            }
          }

          cb(docid);  // return result
          return true;
        },
        limit);
  }

  void resetImpl() final {
    _covering.reset();
    _gotIntervals = false;
  }

 private:
  void performScan() {
    rocksdb::Comparator const* cmp = _index->comparator();
    // list of sorted intervals to scan
    if (!_gotIntervals) {
      _scan = _covering.intervals();
      _gotIntervals = true;
      _scanningInterval = 0;
    }
    while (_scanningInterval < _scan.size()) {
      geo::Interval const& it = _scan[_scanningInterval];
      TRI_ASSERT(it.range_min <= it.range_max);
      RocksDBKeyBounds bds = RocksDBKeyBounds::GeoIndex(
          _index->objectId(), it.range_min.id(), it.range_max.id());

      // intervals are sorted and likely consecutive, try to avoid seeks
      // by checking whether we are in the range already
      bool seek = true;
      if (_scanningInterval > 0) {
        TRI_ASSERT(_scan[_scanningInterval - 1].range_max < it.range_min);
        if (!_iter->Valid()) {  // no more valid keys after this
          // Here is why we actually want to give up here:
          // Intervals come from cells, two cells either do not intersect,
          // or one is contained in the other, the same holds for the intervals.
          // The iterator has an implicit upper bound on the column family, if
          // we ever run past this for one interval I, then this means that
          // there is nothing of interest in the index past the end of the
          // interval I, and we have found everything we need in I.
          // However, any later interval J will have a beginning which is
          // greater or equal to the beginning of I, therefore nothing new
          // can be found from interval J. Therefore:
          _scanningInterval = _scan.size();
          // Besides, if we would not stop here we would have an endless loop.
          break;
        } else if (cmp->Compare(_iter->key(), bds.end()) > 0) {
          ++_scanningInterval;  // Move to the next interval, since we are
          continue;             // beyond range already
        } else if (cmp->Compare(bds.start(), _iter->key()) <= 0) {
          seek = false;  // already in range: min <= key <= max
          TRI_ASSERT(cmp->Compare(_iter->key(), bds.end()) <= 0);
        } else {  // cursor is positioned below min range key
          TRI_ASSERT(cmp->Compare(_iter->key(), bds.start()) < 0);
          int k = 10;  // try to catch the range
          while (k > 0 && _iter->Valid() &&
                 cmp->Compare(_iter->key(), bds.start()) < 0) {
            _iter->Next();
            --k;
          }
          seek =
              !_iter->Valid() || (cmp->Compare(_iter->key(), bds.start()) < 0);
        }
      }

      if (seek) {  // try to avoid seeking at all cost
        _iter->Seek(bds.start());
      }

      while (_iter->Valid() && cmp->Compare(_iter->key(), bds.end()) <= 0) {
        _covering.reportFound(RocksDBKey::indexDocumentId(_iter->key()),
                              RocksDBValue::centroid(_iter->value()));
        _iter->Next();
      }

      // validate that Iterator is in a good shape and hasn't failed
      rocksutils::checkIteratorStatus(*_iter);

      ++_scanningInterval;
      if (_covering.bufferSize() > 1024) {
        break;  // will be called later again
      }
    }
  }

 private:
  RocksDBGeoIndex const* _index;
  geo_index::CoveringUtils _covering;
  std::unique_ptr<rocksdb::Iterator> _iter;
  std::vector<geo::Interval> _scan;
  bool _gotIntervals = false;
  size_t _scanningInterval = 0;
};

RocksDBGeoIndex::RocksDBGeoIndex(IndexId iid, LogicalCollection& collection,
                                 arangodb::velocypack::Slice info,
                                 std::string const& typeName)
    : RocksDBIndex(iid, collection, info,
                   RocksDBColumnFamilyManager::get(
                       RocksDBColumnFamilyManager::Family::GeoIndex),
                   /*useCache*/ false,
                   /*cacheManager*/ nullptr,
                   /*engine*/
                   collection.vocbase().engine<RocksDBEngine>()),
      geo_index::Index(info, _fields),
      _typeName(typeName) {
  TRI_ASSERT(iid.isSet());
  _unique = false;
  _sparse = true;
  TRI_ASSERT(_variant != geo_index::Index::Variant::NONE);
}

/// @brief return a JSON representation of the index
void RocksDBGeoIndex::toVelocyPack(
    VPackBuilder& builder,
    std::underlying_type<arangodb::Index::Serialize>::type flags) const {
  TRI_ASSERT(_variant != geo_index::Index::Variant::NONE);
  builder.openObject();
  RocksDBIndex::toVelocyPack(builder, flags);
  _coverParams.toVelocyPack(builder);
  builder.add("geoJson",
              VPackValue(_variant == geo_index::Index::Variant::GEOJSON));
  builder.add(StaticStrings::IndexLegacyPolygons, VPackValue(_legacyPolygons));
  builder.close();
}

/// @brief Test if this index matches the definition
bool RocksDBGeoIndex::matchesDefinition(VPackSlice const& info) const {
  TRI_ASSERT(_variant != geo_index::Index::Variant::NONE);
  TRI_ASSERT(info.isObject());
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  auto typeSlice = info.get(arangodb::StaticStrings::IndexType);
  TRI_ASSERT(typeSlice.isString());
  TRI_ASSERT(typeSlice.stringView() == oldtypeName());
#endif
  auto value = info.get(arangodb::StaticStrings::IndexId);

  if (!value.isNone()) {
    // We already have an id.
    if (!value.isString()) {
      // Invalid ID
      return false;
    }

    // Short circuit. If id is correct the index is identical.
    return value.stringView() == std::to_string(_iid.id());
  }

  if (_unique != basics::VelocyPackHelper::getBooleanValue(
                     info, arangodb::StaticStrings::IndexUnique, false)) {
    return false;
  }

  if (_sparse != basics::VelocyPackHelper::getBooleanValue(
                     info, arangodb::StaticStrings::IndexSparse, true)) {
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
    bool gj1 =
        basics::VelocyPackHelper::getBooleanValue(info, "geoJson", false);
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
    TRI_ParseAttributeString(f.stringView(), translate, true);
    if (!arangodb::basics::AttributeName::isIdentical(_fields[i], translate,
                                                      false)) {
      return false;
    }
  }
  return true;
}

/// @brief creates an IndexIterator for the given Condition
std::unique_ptr<IndexIterator> RocksDBGeoIndex::iteratorForCondition(
    ResourceMonitor& monitor, transaction::Methods* trx,
    arangodb::aql::AstNode const* node,
    arangodb::aql::Variable const* reference, IndexIteratorOptions const& opts,
    ReadOwnWrites readOwnWrites, int) {
  TRI_ASSERT(!isSorted() || opts.sorted);
  TRI_ASSERT(node != nullptr);
  TRI_ASSERT(readOwnWrites ==
             ReadOwnWrites::no);  // geo index never needs to observe own writes

  geo::QueryParams params;
  params.sorted = opts.sorted;
  params.ascending = opts.ascending;
  params.pointsOnly = pointsOnly();
  params.limit = opts.limit;
  geo_index::Index::parseCondition(node, reference, params, _legacyPolygons);

  // First check if we can use the simpler method with a covering of the
  // target object:
  // If we have a `GEO_CONTAINS` or `GEO_INTERSECTS` clause but no
  // restriction on the `GEO_DISTANCE` and no sorting of results by
  // `GEO_DISTANCE`, we use the simpler method:
  if (!params.sorted &&
      (params.filterType == geo::FilterType::CONTAINS ||
       params.filterType == geo::FilterType::INTERSECTS) &&
      !params.distanceRestricted) {
    LOG_TOPIC("54612", DEBUG, Logger::AQL)
        << "Using RDBCoveringIterator for geo index query: "
        << params.toString();
    return std::make_unique<RDBCoveringIterator>(monitor, &_collection, trx,
                                                 this, std::move(params));
  }

  params.sorted = true;  // RDBNearIterator always works sorted!
  if (params.filterType == geo::FilterType::CONTAINS ||
      (params.filterType == geo::FilterType::INTERSECTS && params.pointsOnly)) {
    // This updates the maximal distance. We can only do this for a CONTAINS
    // query or for the INTERSECTS query, if the database contains only points.
    // Otherwise, there could be an object which intersects us but whose
    // centroid is not in the circumcircle of our bounding box.
    TRI_ASSERT(!params.filterShape.empty());
    params.filterShape.updateBounds(params);
  } else if (params.filterType == geo::FilterType::INTERSECTS) {
    // We need to set the origin still:
    S2LatLng ll(params.filterShape.centroid());
    params.origin = ll;
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

  LOG_TOPIC("54613", DEBUG, Logger::AQL)
      << "Using RDBNearIterator for geo index query: " << params.toString();

  if (params.ascending) {
    return std::make_unique<RDBNearIterator<geo_index::DocumentsAscending>>(
        monitor, &_collection, trx, this, std::move(params));
  } else {
    return std::make_unique<RDBNearIterator<geo_index::DocumentsDescending>>(
        monitor, &_collection, trx, this, std::move(params));
  }
}

/// internal insert function, set batch or trx before calling
Result RocksDBGeoIndex::insert(transaction::Methods& trx, RocksDBMethods* mthd,
                               LocalDocumentId documentId,
                               velocypack::Slice doc,
                               arangodb::OperationOptions const& /*options*/,
                               bool /*performChecks*/) {
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

    rocksdb::Status s =
        mthd->PutUntracked(RocksDBColumnFamilyManager::get(
                               RocksDBColumnFamilyManager::Family::GeoIndex),
                           key.ref(), val.string());

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
                               LocalDocumentId documentId,
                               velocypack::Slice doc,
                               OperationOptions const& /*options*/) {
  // covering and centroid of coordinate / polygon / ...
  std::vector<S2CellId> cells;
  S2Point centroid;

  Result res = geo_index::Index::indexCells(doc, cells, centroid);

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
    rocksdb::Status s =
        mthd->Delete(RocksDBColumnFamilyManager::get(
                         RocksDBColumnFamilyManager::Family::GeoIndex),
                     key.ref());
    if (!s.ok()) {
      res.reset(rocksutils::convertStatus(s, rocksutils::index));
      addErrorMsg(res);
      break;
    }
  }
  return res;
}

arangodb::Index::FilterCosts RocksDBGeoIndex::supportsFilterCondition(
    transaction::Methods& /*trx*/,
    std::vector<std::shared_ptr<arangodb::Index>> const& allIndexes,
    aql::AstNode const* node, aql::Variable const* reference,
    size_t itemsInIndex) const {
  arangodb::Index::FilterCosts costs =
      arangodb::Index::FilterCosts::defaultCosts(itemsInIndex, 1);
  costs.estimatedItems /= 100;
  costs.estimatedCosts = static_cast<double>(costs.estimatedItems);
  return costs;
}
