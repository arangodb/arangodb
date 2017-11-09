////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2017 ArangoDB GmbH, Cologne, Germany
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

#include "RocksDBSphericalIndex.h"

#include "Aql/Ast.h"
#include "Aql/AstNode.h"
#include "Aql/SortCondition.h"
#include "Geo/GeoCover.h"
#include "Geo/GeoJsonParser.h"
#include "Geo/NearQuery.h"
#include "Basics/StringRef.h"
#include "Basics/VelocyPackHelper.h"
#include "Indexes/IndexResult.h"
#include "Logger/Logger.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "RocksDBEngine/RocksDBMethods.h"

#include <geometry/s2regioncoverer.h>
#include <rocksdb/db.h>

using namespace arangodb;
using namespace arangodb::rocksdbengine;

// Handle near queries, possibly with a radius forming an upper bound
class RocksDBSphericalIndexNearIterator final : public IndexIterator {
public:
  /// @brief Construct an RocksDBGeoIndexIterator based on Ast Conditions
  RocksDBSphericalIndexNearIterator(LogicalCollection* collection,
                         transaction::Methods* trx,
                         ManagedDocumentResult* mmdr,
                         RocksDBSphericalIndex const* index,
                         geo::NearQueryParams params)
  : IndexIterator(collection, trx, mmdr, index),
    _index(index),
    _nearQuery(params),
    _done(false) {}
  
  ~RocksDBSphericalIndexNearIterator() {}
  
  char const* typeName() const override { return "geospatial-index-iterator"; }
  
  bool next(LocalDocumentIdCallback const& cb, size_t limit) override {
    /*if (_done) {
      // we already know that no further results will be returned by the index
      TRI_ASSERT(_nearQuery. _queue.empty());
      return false;
    }*/
    
    /*TRI_ASSERT(limit > 0);
    if (limit > 0) {
      // only need to calculate distances for WITHIN queries, but not for NEAR
      // queries
      bool withDistances;
      double maxDistance;
      if (_near) {
        withDistances = false;
        maxDistance = -1.0;
      } else {
        withDistances = true;
        maxDistance = _radius;
      }
      
      size_t const length = coords ? coords->length : 0;
      
      if (length == 0) {
        // Nothing Found
        // TODO validate
        _done = true;
        return false;
      }
      
      size_t numDocs = findLastIndex(coords.get());
      if (numDocs == 0) {
        // we are done
        _done = true;
        return false;
      }
      
      for (size_t i = 0; i < numDocs; ++i) {
        cb(LocalDocumentId(coords->coordinates[i].data));
      }
      // If we return less then limit many docs we are done.
      _done = numDocs < limit;
    }*/
    return true;
  }
  
  void reset() override;

  
private:
  //size_t findLastIndex(arangodb::rocksdbengine::GeoCoordinates* coords) const;
  void createCursor(double lat, double lon);
  /*void evaluateCondition() {
    if (_condition) {
      auto numMembers = _condition->numMembers();
      
      TRI_ASSERT(numMembers == 1);  // should only be an FCALL
      auto fcall = _condition->getMember(0);
      TRI_ASSERT(fcall->type == arangodb::aql::NODE_TYPE_FCALL);
      TRI_ASSERT(fcall->numMembers() == 1);
      auto args = fcall->getMember(0);
      
      numMembers = args->numMembers();
      TRI_ASSERT(numMembers >= 3);
      
      _lat = args->getMember(1)->getDoubleValue();
      _lon = args->getMember(2)->getDoubleValue();
      
      if (numMembers == 3) {
        // NEAR
        _near = true;
      } else {
        // WITHIN
        TRI_ASSERT(numMembers == 5);
        _near = false;
        _radius = args->getMember(3)->getDoubleValue();
        _inclusive = args->getMember(4)->getBoolValue();
      }
    } else {
      LOG_TOPIC(ERR, arangodb::Logger::FIXME)
      << "No condition passed to RocksDBGeoIndexIterator constructor";
    }
  }*/
  
  RocksDBSphericalIndex const* _index;
  geo::NearQuery _nearQuery;
  bool _done = false;
};


/*size_t RocksDBSphericalIndexIterator::findLastIndex(GeoCoordinates* coords) const {
  TRI_ASSERT(coords != nullptr);

  // determine which documents to return...
  size_t numDocs = coords->length;

  if (!_near) {
    // WITHIN
    // only return those documents that are within the specified radius
    TRI_ASSERT(numDocs > 0);

    // linear scan for the first document outside the specified radius
    // scan backwards because documents with higher distances are more
    // interesting
    int iterations = 0;
    while ((_inclusive && coords->distances[numDocs - 1] > _radius) ||
           (!_inclusive && coords->distances[numDocs - 1] >= _radius)) {
      // document is outside the specified radius!
      --numDocs;

      if (numDocs == 0) {
        break;
      }

      if (++iterations == 8 && numDocs >= 10) {
        // switch to a binary search for documents inside/outside the specified
        // radius
        size_t l = 0;
        size_t r = numDocs - 1;

        while (true) {
          // determine midpoint
          size_t m = l + ((r - l) / 2);
          if ((_inclusive && coords->distances[m] > _radius) ||
              (!_inclusive && coords->distances[m] >= _radius)) {
            // document is outside the specified radius!
            if (m == 0) {
              numDocs = 0;
              break;
            }
            r = m - 1;
          } else {
            // still inside the radius
            numDocs = m + 1;
            l = m + 1;
          }

          if (r < l) {
            break;
          }
        }
        break;
      }
    }
  }
  return numDocs;
}*/

/// @brief creates an IndexIterator for the given Condition
IndexIterator* RocksDBSphericalIndex::iteratorForCondition(
    transaction::Methods* trx, ManagedDocumentResult* mmdr,
    arangodb::aql::AstNode const* node,
    arangodb::aql::Variable const* reference, bool reverse) {
  TRI_ASSERT(node != nullptr);
  
  double latitude, longitude;
  size_t numMembers = node->numMembers();
  
  TRI_ASSERT(numMembers == 1);  // should only be an FCALL
  auto fcall = node->getMember(0);
  TRI_ASSERT(fcall->type == arangodb::aql::NODE_TYPE_FCALL);
  TRI_ASSERT(fcall->numMembers() == 1);
  auto args = fcall->getMember(0);
  
  numMembers = args->numMembers();
  TRI_ASSERT(numMembers >= 3);
  
  latitude = args->getMember(1)->getDoubleValue();
  longitude = args->getMember(2)->getDoubleValue();
  
  geo::NearQueryParams params(latitude, longitude);
  if (numMembers == 5) {
    // WITHIN
    params.maxDistance = args->getMember(3)->getDoubleValue();
    params.maxInclusive = args->getMember(4)->getBoolValue();
  }
  
  return nullptr;
  //return new RocksDBSphericalIndexIterator(_collection, trx, mmdr, this, node,
  //                                   reference);
}


RocksDBSphericalIndex::RocksDBSphericalIndex(TRI_idx_iid_t iid,
                       LogicalCollection* collection,
                       VPackSlice const& info)
    : RocksDBIndex(iid, collection, info, RocksDBColumnFamily::geo(), false),
    _variant(IndexVariant::NONE) {
  TRI_ASSERT(iid != 0);
  _unique = false;
  _sparse = true;
      
  _coverParams.fromVelocyPack(info);
  if (_fields.size() == 1) {
    bool geoJson = basics::VelocyPackHelper::getBooleanValue(
                            info, "geoJson", true);
    // geojson means [<longitude>, <latitude>] or
    // json object {type:"<name>, coordinates:[]}.
    _variant = geoJson ? IndexVariant::COMBINED_GEOJSON :
               IndexVariant::COMBINED_LAT_LON;
    
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
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        "RocksDBGeoIndex can only be created with one or two fields.");
  }
}

RocksDBSphericalIndex::~RocksDBSphericalIndex() {
}

/// @brief return a JSON representation of the index
void RocksDBSphericalIndex::toVelocyPack(VPackBuilder& builder, bool withFigures,
                                   bool forPersistence) const {
  builder.openObject();
  // Basic index
  RocksDBIndex::toVelocyPack(builder, withFigures, forPersistence);

  if (_variant == IndexVariant::COMBINED_GEOJSON) {
    builder.add("geoJson", VPackValue(true));
  }

  // geo indexes are always non-unique
  // geo indexes are always sparse.
  // "ignoreNull" has the same meaning as "sparse" and is only returned for
  // backwards compatibility
  // the "constraint" attribute has no meaning since ArangoDB 2.5 and is only
  // returned for backwards compatibility
  builder.add("constraint", VPackValue(false));
  builder.add("unique", VPackValue(false));
  builder.add("ignoreNull", VPackValue(true));
  builder.add("sparse", VPackValue(true));
  builder.close();
}

/// @brief Test if this index matches the definition
bool RocksDBSphericalIndex::matchesDefinition(VPackSlice const& info) const {
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
  
  if (_unique != basics::VelocyPackHelper::getBooleanValue(
                     info, "unique", false)) {
    return false;
  }
  if (_sparse != basics::VelocyPackHelper::getBooleanValue(
                     info, "sparse", true)) {
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
    bool geoJson = basics::VelocyPackHelper::getBooleanValue(info, "geoJson", true);
    if (geoJson && _variant != IndexVariant::COMBINED_GEOJSON) {
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

/// internal insert function, set batch or trx before calling
Result RocksDBSphericalIndex::insertInternal(transaction::Methods* trx,
                                         RocksDBMethods* mthd,
                                         LocalDocumentId const& documentId,
                                         velocypack::Slice const& doc) {
  S2RegionCoverer coverer;
  _coverParams.configureS2RegionCoverer(&coverer);
  std::vector<S2CellId> cells;

  Result res;
  bool isGeoJson = _variant == IndexVariant::COMBINED_GEOJSON;
  if (isGeoJson || _variant == IndexVariant::COMBINED_LAT_LON) {
    VPackSlice loc = doc.get(_location);
    res = geo::GeoCover::generateCover(&coverer, loc, isGeoJson, cells);
  } else if (_variant == IndexVariant::INDIVIDUAL_LAT_LON) {
    VPackSlice lon = doc.get(_longitude);
    VPackSlice lat = doc.get(_latitude);
    if (!lon.isNumber() || !lat.isNumber()) {
      // Invalid, no insert. Index is sparse
      return IndexResult();
    }
    double latitude = lat.getNumericValue<double>();
    double longitude = lon.getNumericValue<double>();
    
    res = geo::GeoCover::generateCover(latitude, longitude, cells);
  }
  
  if (res.is(TRI_ERROR_BAD_PARAMETER)) {
    // Invalid, no insert. Index is sparse
    return IndexResult();
  } else if (res.fail()) {
    return res;
  }

  // FIXME: can we rely on the region coverer to return
  // the same cells everytime for the same parameters ?
  for (S2CellId cell : cells) {
    RocksDBKeyLeaser key(trx);
    key->constructSphericalIndexValue(_objectId, cell.id(), documentId.id());
    Result r = mthd->Put(RocksDBColumnFamily::geo(), key.ref(), rocksdb::Slice());
    if (r.fail()) {
      return r;
    }
  }
  
  return IndexResult();
}

/// internal remove function, set batch or trx before calling
Result RocksDBSphericalIndex::removeInternal(transaction::Methods* trx,
                                         RocksDBMethods* mthd,
                                         LocalDocumentId const& documentId,
                                         VPackSlice const& doc) {
  S2RegionCoverer coverer;
  _coverParams.configureS2RegionCoverer(&coverer);
  std::vector<S2CellId> cells;
  
  Result res;
  bool isGeoJson = _variant == IndexVariant::COMBINED_GEOJSON;
  if (isGeoJson || _variant == IndexVariant::COMBINED_LAT_LON) {
    VPackSlice loc = doc.get(_location);
    res = geo::GeoCover::generateCover(&coverer, loc, isGeoJson, cells);
  } else if (_variant == IndexVariant::INDIVIDUAL_LAT_LON) {
    VPackSlice lon = doc.get(_longitude);
    VPackSlice lat = doc.get(_latitude);
    if (!lon.isNumber() || !lat.isNumber()) {
      // Invalid, no insert. Index is sparse
      return IndexResult();
    }
    double latitude = lat.getNumericValue<double>();
    double longitude = lon.getNumericValue<double>();
    
    res = geo::GeoCover::generateCover(latitude, longitude, cells);
  }
  
  if (res.is(TRI_ERROR_BAD_PARAMETER)) {
    // Invalid, no insert. Index is sparse
    return IndexResult();
  } else if (res.fail()) {
    return res;
  }
  
  // FIXME: can we rely on the region coverer to return
  // the same cells everytime for the same parameters ?
  for (S2CellId cell : cells) {
    RocksDBKeyLeaser key(trx);
    key->constructSphericalIndexValue(_objectId, cell.id(), documentId.id());
    Result r = mthd->Delete(RocksDBColumnFamily::geo(), key.ref());
    if (r.fail()) {
      return r;
    }
  }
  
  return IndexResult();
}

void RocksDBSphericalIndex::truncate(transaction::Methods* trx) {
  RocksDBIndex::truncate(trx);
  //GeoIndex_reset(_geoIndex, RocksDBTransactionState::toMethods(trx));
}

