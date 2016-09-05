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

#include "GeoIndex.h"
#include "Logger/Logger.h"
#include "Basics/StringRef.h"
#include "Basics/VelocyPackHelper.h"
#include "VocBase/transaction.h"

using namespace arangodb;

////////////////////////////////////////////////////////////////////////////////
/// @brief create a new geo index, type "geo1"
///        Lat and Lon are stored in the same Array
////////////////////////////////////////////////////////////////////////////////

GeoIndex::GeoIndex(
    TRI_idx_iid_t iid, arangodb::LogicalCollection* collection,
    std::vector<std::vector<arangodb::basics::AttributeName>> const& fields,
    std::vector<std::string> const& path, bool geoJson)
    : Index(iid, collection, fields, false, true),
      _location(path),
      _variant(geoJson ? INDEX_GEO_COMBINED_LAT_LON
                       : INDEX_GEO_COMBINED_LON_LAT),
      _geoJson(geoJson),
      _geoIndex(nullptr) {
  TRI_ASSERT(iid != 0);

  _geoIndex = GeoIndex_new();

  if (_geoIndex == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a new geo index, type "geo2"
////////////////////////////////////////////////////////////////////////////////

GeoIndex::GeoIndex(
    TRI_idx_iid_t iid, arangodb::LogicalCollection* collection,
    std::vector<std::vector<arangodb::basics::AttributeName>> const& fields,
    std::vector<std::vector<std::string>> const& paths)
    : Index(iid, collection, fields, false, true),
      _latitude(paths[0]),
      _longitude(paths[1]),
      _variant(INDEX_GEO_INDIVIDUAL_LAT_LON),
      _geoJson(false),
      _geoIndex(nullptr) {
  TRI_ASSERT(iid != 0);

  _geoIndex = GeoIndex_new();

  if (_geoIndex == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }
}

/// @brief create a new geo index, type "geo2"

GeoIndex::GeoIndex(TRI_idx_iid_t iid, arangodb::LogicalCollection* collection,
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
        "GeoIndex can only be created with one or two fields.");
  }

  _geoIndex = GeoIndex_new();

  if (_geoIndex == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }
}

GeoIndex::~GeoIndex() {
  if (_geoIndex != nullptr) {
    GeoIndex_free(_geoIndex);
  }
}

size_t GeoIndex::memory() const { return GeoIndex_MemoryUsage(_geoIndex); }

////////////////////////////////////////////////////////////////////////////////
/// @brief return a JSON representation of the index
////////////////////////////////////////////////////////////////////////////////

void GeoIndex::toVelocyPack(VPackBuilder& builder, bool withFigures) const {
  // Basic index
  Index::toVelocyPack(builder, withFigures);

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
}

/// @brief Test if this index matches the definition
bool GeoIndex::matchesDefinition(VPackSlice const& info) const {
  TRI_ASSERT(info.isObject());
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  VPackSlice typeSlice = info.get("type");
  TRI_ASSERT(typeSlice.isString());
  StringRef typeStr(typeSlice);
  TRI_ASSERT(typeStr == typeName());
#endif
  auto value = info.get("id");
  if (!value.isNone()) {
    // We already have an id.
    if(!value.isString()) {
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



int GeoIndex::insert(arangodb::Transaction*, TRI_doc_mptr_t const* doc, bool) {
  TRI_ASSERT(doc != nullptr);
  TRI_ASSERT(doc->vpack() != nullptr);

  VPackSlice const slice(doc->vpack());

  double latitude;
  double longitude;

  if (_variant == INDEX_GEO_INDIVIDUAL_LAT_LON) {
    VPackSlice lat = slice.get(_latitude);
    if (!lat.isNumber()) {
      // Invalid, no insert. Index is sparse
      return TRI_ERROR_NO_ERROR;
    }

    VPackSlice lon = slice.get(_longitude);
    if (!lon.isNumber()) {
      // Invalid, no insert. Index is sparse
      return TRI_ERROR_NO_ERROR;
    }
    latitude = lat.getNumericValue<double>();
    longitude = lon.getNumericValue<double>();
  } else {
    VPackSlice loc = slice.get(_location);
    if (!loc.isArray() || loc.length() < 2) {
      // Invalid, no insert. Index is sparse
      return TRI_ERROR_NO_ERROR;
    }
    VPackSlice first = loc.at(0);
    if (!first.isNumber()) {
      // Invalid, no insert. Index is sparse
      return TRI_ERROR_NO_ERROR;
    }
    VPackSlice second = loc.at(1);
    if (!second.isNumber()) {
      // Invalid, no insert. Index is sparse
      return TRI_ERROR_NO_ERROR;
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
  gc.data = const_cast<void*>(static_cast<void const*>(doc));

  int res = GeoIndex_insert(_geoIndex, &gc);

  if (res == -1) {
    LOG(WARN) << "found duplicate entry in geo-index, should not happen";
    return TRI_set_errno(TRI_ERROR_INTERNAL);
  } else if (res == -2) {
    return TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
  } else if (res == -3) {
    LOG(DEBUG) << "illegal geo-coordinates, ignoring entry";
    return TRI_ERROR_NO_ERROR;
  } else if (res < 0) {
    return TRI_set_errno(TRI_ERROR_INTERNAL);
  }

  return TRI_ERROR_NO_ERROR;
}

int GeoIndex::remove(arangodb::Transaction*, TRI_doc_mptr_t const* doc, bool) {
  TRI_ASSERT(doc != nullptr);
  TRI_ASSERT(doc->vpack() != nullptr);

  VPackSlice const slice(doc->vpack());

  double latitude = 0.0;
  double longitude = 0.0;
  bool ok = true;

  if (_variant == INDEX_GEO_INDIVIDUAL_LAT_LON) {
    VPackSlice lat = slice.get(_latitude);
    VPackSlice lon = slice.get(_longitude);
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
    VPackSlice loc = slice.get(_location);
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
    return TRI_ERROR_NO_ERROR;
  }

  GeoCoordinate gc;
  gc.latitude = latitude;
  gc.longitude = longitude;
  gc.data = const_cast<void*>(static_cast<void const*>(doc));

  // ignore non-existing elements in geo-index
  GeoIndex_remove(_geoIndex, &gc);

  return TRI_ERROR_NO_ERROR;
}

int GeoIndex::unload() {
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

  return TRI_ERROR_NO_ERROR;
}

/// @brief looks up all points within a given radius
GeoCoordinates* GeoIndex::withinQuery(arangodb::Transaction* trx, double lat,
                                       double lon, double radius) const {
  GeoCoordinate gc;
  gc.latitude = lat;
  gc.longitude = lon;

  return GeoIndex_PointsWithinRadius(_geoIndex, &gc, radius);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up the nearest points
////////////////////////////////////////////////////////////////////////////////

GeoCoordinates* GeoIndex::nearQuery(arangodb::Transaction* trx, double lat,
                                     double lon, size_t count) const {
  GeoCoordinate gc;
  gc.latitude = lat;
  gc.longitude = lon;

  return GeoIndex_NearestCountPoints(_geoIndex, &gc, static_cast<int>(count));
}
