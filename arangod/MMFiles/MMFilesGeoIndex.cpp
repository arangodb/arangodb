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
#include "Basics/StringRef.h"
#include "Basics/VelocyPackHelper.h"
#include "Indexes/IndexResult.h"
#include "Logger/Logger.h"
#include "MMFiles/MMFilesToken.h"
#include "StorageEngine/TransactionState.h"

using namespace arangodb;

MMFilesGeoIndexIterator::MMFilesGeoIndexIterator(
    LogicalCollection* collection, transaction::Methods* trx,
    ManagedDocumentResult* mmdr, MMFilesGeoIndex const* index,
    arangodb::aql::AstNode const* cond, arangodb::aql::Variable const* var)
    : IndexIterator(collection, trx, mmdr, index),
      _index(index),
      _cursor(nullptr),
      _coor(),
      _condition(cond),
      _lat(0.0),
      _lon(0.0),
      _near(true),
      _inclusive(false),
      _done(false),
      _radius(0.0) {
  evaluateCondition();
}

void MMFilesGeoIndexIterator::evaluateCondition() {
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
        << "No condition passed to MMFilesGeoIndexIterator constructor";
  }
}

size_t MMFilesGeoIndexIterator::findLastIndex(GeoCoordinates* coords) const {
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
}

bool MMFilesGeoIndexIterator::next(TokenCallback const& cb, size_t limit) {
  if (!_cursor) {
    createCursor(_lat, _lon);

    if (!_cursor) {
      // actually validate that we got a valid cursor
      THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
    }
  }

  TRI_ASSERT(_cursor != nullptr);

  if (_done) {
    // we already know that no further results will be returned by the index
    return false;
  }

  TRI_ASSERT(limit > 0);
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
    auto coords = std::unique_ptr<GeoCoordinates>(::GeoIndex_ReadCursor(
        _cursor, static_cast<int>(limit), withDistances, maxDistance));

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
      cb(::MMFilesGeoIndex::toDocumentIdentifierToken(
          coords->coordinates[i].data));
    }
    // If we return less then limit many docs we are done.
    _done = numDocs < limit;
  }
  return true;
}

void MMFilesGeoIndexIterator::replaceCursor(::GeoCursor* c) {
  if (_cursor) {
    ::GeoIndex_CursorFree(_cursor);
  }
  _cursor = c;
  _done = false;
}

void MMFilesGeoIndexIterator::createCursor(double lat, double lon) {
  _coor = GeoCoordinate{lat, lon, 0};
  replaceCursor(::GeoIndex_NewCursor(_index->_geoIndex, &_coor));
}

uint64_t MMFilesGeoIndex::fromDocumentIdentifierToken(
    DocumentIdentifierToken const& token) {
  auto tkn = static_cast<MMFilesToken const*>(&token);
  return static_cast<uint64_t>(tkn->revisionId());
}

DocumentIdentifierToken MMFilesGeoIndex::toDocumentIdentifierToken(
    uint64_t internal) {
  return MMFilesToken{internal};
}

/// @brief creates an IndexIterator for the given Condition
IndexIterator* MMFilesGeoIndex::iteratorForCondition(
    transaction::Methods* trx, ManagedDocumentResult* mmdr,
    arangodb::aql::AstNode const* node,
    arangodb::aql::Variable const* reference, bool) {
  TRI_IF_FAILURE("GeoIndex::noIterator") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
  return new MMFilesGeoIndexIterator(_collection, trx, mmdr, this, node,
                                     reference);
}

void MMFilesGeoIndexIterator::reset() { replaceCursor(nullptr); }

MMFilesGeoIndex::MMFilesGeoIndex(TRI_idx_iid_t iid,
                                 arangodb::LogicalCollection* collection,
                                 VPackSlice const& info)
    : Index(iid, collection, info),
      _variant(INDEX_GEO_INDIVIDUAL_LAT_LON),
      _geoJson(false),
      _geoIndex(nullptr) {
  TRI_ASSERT(iid != 0);
  _unique = false;
  _sparse = true;

  if (_fields.size() == 1) {
    _geoJson = arangodb::basics::VelocyPackHelper::getBooleanValue(
        info, "geoJson", false);
    auto& loc = _fields[0];
    _location.reserve(loc.size());
    for (auto const& it : loc) {
      _location.emplace_back(it.name);
    }
    _variant =
        _geoJson ? INDEX_GEO_COMBINED_LAT_LON : INDEX_GEO_COMBINED_LON_LAT;
  } else if (_fields.size() == 2) {
    _variant = INDEX_GEO_INDIVIDUAL_LAT_LON;
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
        "MMFilesGeoIndex can only be created with one or two fields.");
  }

  _geoIndex = GeoIndex_new();

  if (_geoIndex == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }
}

MMFilesGeoIndex::~MMFilesGeoIndex() {
  if (_geoIndex != nullptr) {
    GeoIndex_free(_geoIndex);
  }
}

size_t MMFilesGeoIndex::memory() const {
  return GeoIndex_MemoryUsage(_geoIndex);
}

/// @brief return a JSON representation of the index
void MMFilesGeoIndex::toVelocyPack(VPackBuilder& builder, bool withFigures,
                                   bool forPersistence) const {
  builder.openObject();
  // Basic index
  Index::toVelocyPack(builder, withFigures, forPersistence);

  if (_variant == INDEX_GEO_COMBINED_LAT_LON ||
      _variant == INDEX_GEO_COMBINED_LON_LAT) {
    builder.add("geoJson", VPackValue(_geoJson));
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
bool MMFilesGeoIndex::matchesDefinition(VPackSlice const& info) const {
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
  value = info.get("fields");
  if (!value.isArray()) {
    return false;
  }

  size_t const n = static_cast<size_t>(value.length());
  if (n != _fields.size()) {
    return false;
  }
  if (_unique != arangodb::basics::VelocyPackHelper::getBooleanValue(
                     info, "unique", false)) {
    return false;
  }
  if (_sparse != arangodb::basics::VelocyPackHelper::getBooleanValue(
                     info, "sparse", true)) {
    return false;
  }

  if (n == 1) {
    if (_geoJson != arangodb::basics::VelocyPackHelper::getBooleanValue(
                        info, "geoJson", false)) {
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

Result MMFilesGeoIndex::insert(transaction::Methods*, TRI_voc_rid_t revisionId,
                               VPackSlice const& doc, bool isRollback) {
  double latitude;
  double longitude;

  if (_variant == INDEX_GEO_INDIVIDUAL_LAT_LON) {
    VPackSlice lat = doc.get(_latitude);
    if (!lat.isNumber()) {
      // Invalid, no insert. Index is sparse
      return Result(TRI_ERROR_NO_ERROR);
    }

    VPackSlice lon = doc.get(_longitude);
    if (!lon.isNumber()) {
      // Invalid, no insert. Index is sparse
      return Result(TRI_ERROR_NO_ERROR);
    }
    latitude = lat.getNumericValue<double>();
    longitude = lon.getNumericValue<double>();
  } else {
    VPackSlice loc = doc.get(_location);
    if (!loc.isArray() || loc.length() < 2) {
      // Invalid, no insert. Index is sparse
      return Result(TRI_ERROR_NO_ERROR);
    }
    VPackSlice first = loc.at(0);
    if (!first.isNumber()) {
      // Invalid, no insert. Index is sparse
      return Result(TRI_ERROR_NO_ERROR);
    }
    VPackSlice second = loc.at(1);
    if (!second.isNumber()) {
      // Invalid, no insert. Index is sparse
      return Result(TRI_ERROR_NO_ERROR);
    }
    if (_geoJson) {
      longitude = first.getNumericValue<double>();
      latitude = second.getNumericValue<double>();
    } else {
      latitude = first.getNumericValue<double>();
      longitude = second.getNumericValue<double>();
    }
  }

  // and insert into index
  GeoCoordinate gc;
  gc.latitude = latitude;
  gc.longitude = longitude;
  gc.data = static_cast<uint64_t>(revisionId);

  int res = GeoIndex_insert(_geoIndex, &gc);

  if (res == -1) {
    LOG_TOPIC(WARN, arangodb::Logger::FIXME)
        << "found duplicate entry in geo-index, should not happen";
    return IndexResult(TRI_set_errno(TRI_ERROR_INTERNAL), this);
  } else if (res == -2) {
    return IndexResult(TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY), this);
  } else if (res == -3) {
    LOG_TOPIC(DEBUG, arangodb::Logger::FIXME)
        << "illegal geo-coordinates, ignoring entry";
    return Result(TRI_ERROR_NO_ERROR);
  } else if (res < 0) {
    return IndexResult(TRI_set_errno(TRI_ERROR_INTERNAL), this);
  }

  return Result(TRI_ERROR_NO_ERROR);
}

Result MMFilesGeoIndex::remove(transaction::Methods*, TRI_voc_rid_t revisionId,
                               VPackSlice const& doc, bool isRollback) {
  double latitude = 0.0;
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

  if (!ok) {
    return Result(TRI_ERROR_NO_ERROR);
  }

  GeoCoordinate gc;
  gc.latitude = latitude;
  gc.longitude = longitude;
  gc.data = static_cast<uint64_t>(revisionId);

  // ignore non-existing elements in geo-index
  GeoIndex_remove(_geoIndex, &gc);

  return Result(TRI_ERROR_NO_ERROR);
}

void MMFilesGeoIndex::unload() {
  // create a new, empty index
  auto empty = GeoIndex_new();

  if (empty == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  // free the old one
  if (_geoIndex != nullptr) {
    GeoIndex_free(_geoIndex);
  }

  // and assign it
  _geoIndex = empty;
}

/// @brief looks up all points within a given radius
GeoCoordinates* MMFilesGeoIndex::withinQuery(transaction::Methods* trx,
                                             double lat, double lon,
                                             double radius) const {
  GeoCoordinate gc;
  gc.latitude = lat;
  gc.longitude = lon;

  return GeoIndex_PointsWithinRadius(_geoIndex, &gc, radius);
}

/// @brief looks up the nearest points
GeoCoordinates* MMFilesGeoIndex::nearQuery(transaction::Methods* trx,
                                           double lat, double lon,
                                           size_t count) const {
  GeoCoordinate gc;
  gc.latitude = lat;
  gc.longitude = lon;

  return GeoIndex_NearestCountPoints(_geoIndex, &gc, static_cast<int>(count));
}
