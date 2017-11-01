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

#include "S2GeoIndex.h"

#include "Aql/Ast.h"
#include "Aql/AstNode.h"
#include "Aql/SortCondition.h"
#include "Geo/GeoHelper.h"
#include "Geo/GeoJsonParser.h"
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
class S2GeoIndexNearIterator final : public IndexIterator {
public:
  /// @brief Construct an RocksDBGeoIndexIterator based on Ast Conditions
  S2GeoIndexNearIterator(LogicalCollection* collection,
                         transaction::Methods* trx,
                         ManagedDocumentResult* mmdr,
                         S2GeoIndex const* index,
                         arangodb::aql::AstNode const* cond,
                         arangodb::aql::Variable const*)
  : IndexIterator(collection, trx, mmdr, index),
  _index(index),
  _condition(cond),
  _lat(0.0),
  _lon(0.0),
  _inclusive(false),
  _done(false),
  _radius(0.0) {
    evaluateCondition();
  }
  
  ~S2GeoIndexNearIterator() {}
  
  char const* typeName() const override { return "geospatial-index-iterator"; }
  
  bool next(LocalDocumentIdCallback const& cb, size_t limit) override {
    
    if (_done) {
      // we already know that no further results will be returned by the index
      return false;
    }
    
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
  void evaluateCondition() {
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
  }
  
  
  S2GeoIndex const* _index;
  arangodb::aql::AstNode const* _condition;
  double _lat;
  double _lon;
  bool _near;
  bool _inclusive;
  bool _done;
  double _radius;
};


/*size_t S2GeoIndexIterator::findLastIndex(GeoCoordinates* coords) const {
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
IndexIterator* S2GeoIndex::iteratorForCondition(
    transaction::Methods* trx, ManagedDocumentResult* mmdr,
    arangodb::aql::AstNode const* node,
    arangodb::aql::Variable const* reference, bool reverse) {
  TRI_IF_FAILURE("GeoIndex::noIterator") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
  return nullptr;
  //return new S2GeoIndexIterator(_collection, trx, mmdr, this, node,
  //                                   reference);
}


S2GeoIndex::S2GeoIndex(TRI_idx_iid_t iid,
                       LogicalCollection* collection,
                       VPackSlice const& info)
    : RocksDBIndex(iid, collection, info, RocksDBColumnFamily::geo(), false),
    _variant(IndexVariant::NONE) {
  TRI_ASSERT(iid != 0);
  _unique = false;
  _sparse = true;

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

S2GeoIndex::~S2GeoIndex() {
}

/// @brief return a JSON representation of the index
void S2GeoIndex::toVelocyPack(VPackBuilder& builder, bool withFigures,
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
bool S2GeoIndex::matchesDefinition(VPackSlice const& info) const {
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
  
  if (_unique != arangodb::basics::VelocyPackHelper::getBooleanValue(
                     info, "unique", false)) {
    return false;
  }
  if (_sparse != arangodb::basics::VelocyPackHelper::getBooleanValue(
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
Result S2GeoIndex::insertInternal(transaction::Methods* trx,
                                       RocksDBMethods* mthd,
                                       LocalDocumentId const& documentId,
                                       velocypack::Slice const& doc) {
  // GeoIndex is always exclusively write-locked with rocksdb
  
  S2RegionCoverer coverer;
  std::vector<S2CellId> cells;

  Result res;
  bool isGeoJson = _variant == IndexVariant::COMBINED_GEOJSON;
  if (isGeoJson || _variant == IndexVariant::COMBINED_LAT_LON) {
    VPackSlice loc = doc.get(_location);
    res = geo::GeoHelper::generateS2CellIds(&coverer, loc, isGeoJson, cells);
  } else if (_variant == IndexVariant::INDIVIDUAL_LAT_LON) {
    VPackSlice lat = doc.get(_latitude);
    if (!lat.isNumber()) {
      // Invalid, no insert. Index is sparse
      return IndexResult();
    }
    
    VPackSlice lon = doc.get(_longitude);
    if (!lon.isNumber()) {
      // Invalid, no insert. Index is sparse
      return IndexResult();
    }
    double latitude = lat.getNumericValue<double>();
    double longitude = lon.getNumericValue<double>();
    
    res = geo::GeoHelper::generateS2CellIdFromLatLng(latitude, longitude, cells);
  }
  
  if (res.is(TRI_ERROR_BAD_PARAMETER)) {
    // Invalid, no insert. Index is sparse
    return IndexResult();
  } else if (res.fail()) {
    return res;
  }

  

  // and insert into index
  /*GeoCoordinate gc;
  gc.latitude = latitude;
  gc.longitude = longitude;
  gc.data = static_cast<uint64_t>(documentId.id());

  int res = GeoIndex_insert(_geoIndex, mthd, &gc);
  if (res == -1) {
    LOG_TOPIC(WARN, arangodb::Logger::FIXME)
        << "found duplicate entry in geo-index, should not happen";
    return IndexResult(TRI_ERROR_INTERNAL, this);
  } else if (res == -2) {
    return IndexResult(TRI_ERROR_OUT_OF_MEMORY, this);
  } else if (res == -3) {
    LOG_TOPIC(DEBUG, arangodb::Logger::FIXME)
        << "illegal geo-coordinates, ignoring entry";
  } else if (res < 0) {
    return IndexResult(TRI_ERROR_INTERNAL, this);
  }*/
  return IndexResult();
}

/// internal remove function, set batch or trx before calling
Result S2GeoIndex::removeInternal(transaction::Methods* trx,
                                       RocksDBMethods* mthd,
                                       LocalDocumentId const& documentId,
                                       arangodb::velocypack::Slice const& doc) {
  // GeoIndex is always exclusively write-locked with rocksdb
  /*double latitude = 0.0;
  double longitude = 0.0;
  bool ok = true;

  if (_variant == INDEX_GEO_INDIVIDUAL_LAT_LON) {
    VPackSlice lat = doc.get(_latitude);
    VPackSlice lon = doc.get(_longitude);
    if (!lat.isNumber()) {
      ok = false;
    } else {
      latitude = lat.getNumericValue<double>();
    }
    if (!lon.isNumber()) {
      ok = false;
    } else {
      longitude = lon.getNumericValue<double>();
    }
  } else {
    VPackSlice loc = doc.get(_location);
    if (!loc.isArray() || loc.length() < 2) {
      ok = false;
    } else {
      VPackSlice first = loc.at(0);
      if (!first.isNumber()) {
        ok = false;
      }
      VPackSlice second = loc.at(1);
      if (!second.isNumber()) {
        ok = false;
      }
      if (ok) {
        if (_geoJson) {
          longitude = first.getNumericValue<double>();
          latitude = second.getNumericValue<double>();
        } else {
          latitude = first.getNumericValue<double>();
          longitude = second.getNumericValue<double>();
        }
      }
    }
  }

  if (ok) {
    GeoCoordinate gc;
    gc.latitude = latitude;
    gc.longitude = longitude;
    gc.data = static_cast<uint64_t>(documentId.id());
    // ignore non-existing elements in geo-index
    GeoIndex_remove(_geoIndex, RocksDBTransactionState::toMethods(trx), &gc);
  }*/

  return IndexResult();
}

void S2GeoIndex::truncate(transaction::Methods* trx) {
  RocksDBIndex::truncate(trx);
  //GeoIndex_reset(_geoIndex, RocksDBTransactionState::toMethods(trx));
}

