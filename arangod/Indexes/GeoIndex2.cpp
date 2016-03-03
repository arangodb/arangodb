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

#include "GeoIndex2.h"
#include "Basics/Logger.h"
#include "VocBase/document-collection.h"
#include "VocBase/transaction.h"
#include "VocBase/VocShaper.h"

using namespace arangodb;

////////////////////////////////////////////////////////////////////////////////
/// @brief create a new geo index, type "geo1"
////////////////////////////////////////////////////////////////////////////////

GeoIndex2::GeoIndex2(
    TRI_idx_iid_t iid, TRI_document_collection_t* collection,
    std::vector<std::vector<arangodb::basics::AttributeName>> const& fields,
    std::vector<TRI_shape_pid_t> const& paths, bool geoJson)
    : Index(iid, collection, fields, false, true),
      _paths(paths),
      _location(paths[0]),
      _latitude(0),
      _longitude(0),
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

GeoIndex2::GeoIndex2(
    TRI_idx_iid_t iid, TRI_document_collection_t* collection,
    std::vector<std::vector<arangodb::basics::AttributeName>> const& fields,
    std::vector<TRI_shape_pid_t> const& paths)
    : Index(iid, collection, fields, false, true),
      _paths(paths),
      _location(0),
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

GeoIndex2::~GeoIndex2() {
  if (_geoIndex != nullptr) {
    GeoIndex_free(_geoIndex);
  }
}

size_t GeoIndex2::memory() const { return GeoIndex_MemoryUsage(_geoIndex); }

////////////////////////////////////////////////////////////////////////////////
/// @brief return a JSON representation of the index
////////////////////////////////////////////////////////////////////////////////

void GeoIndex2::toVelocyPack(VPackBuilder& builder, bool withFigures) const {
  std::vector<std::string> f;

  auto shaper = _collection->getShaper();

  if (_variant == INDEX_GEO_COMBINED_LAT_LON ||
      _variant == INDEX_GEO_COMBINED_LON_LAT) {
    // index has one field

    // convert location to string
    char const* location = shaper->attributeNameShapePid(_location);

    if (location != nullptr) {
      f.emplace_back(location);
    }
  } else {
    // index has two fields
    char const* latitude = shaper->attributeNameShapePid(_latitude);

    if (latitude != nullptr) {
      f.emplace_back(latitude);
    }

    char const* longitude = shaper->attributeNameShapePid(_longitude);

    if (longitude != nullptr) {
      f.emplace_back(longitude);
    }
  }

  if (f.empty()) {
    // No info to provide
    return;
  }

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

int GeoIndex2::insert(arangodb::Transaction*, TRI_doc_mptr_t const* doc, bool) {
  auto shaper =
      _collection->getShaper();  // ONLY IN INDEX, PROTECTED by RUNTIME

  // lookup latitude and longitude
  TRI_shaped_json_t shapedJson;
  TRI_EXTRACT_SHAPED_JSON_MARKER(
      shapedJson, doc->getDataPtr());  // ONLY IN INDEX, PROTECTED by RUNTIME

  bool ok;
  double latitude;
  double longitude;

  if (_location != 0) {
    if (_geoJson) {
      ok = extractDoubleArray(shaper, &shapedJson, &longitude, &latitude);
    } else {
      ok = extractDoubleArray(shaper, &shapedJson, &latitude, &longitude);
    }
  } else {
    ok = extractDoubleObject(shaper, &shapedJson, 0, &latitude);
    ok = ok && extractDoubleObject(shaper, &shapedJson, 1, &longitude);
  }

  if (!ok) {
    return TRI_ERROR_NO_ERROR;
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

int GeoIndex2::remove(arangodb::Transaction*, TRI_doc_mptr_t const* doc, bool) {
  TRI_shaped_json_t shapedJson;

  auto shaper =
      _collection->getShaper();  // ONLY IN INDEX, PROTECTED by RUNTIME
  TRI_EXTRACT_SHAPED_JSON_MARKER(
      shapedJson, doc->getDataPtr());  // ONLY IN INDEX, PROTECTED by RUNTIME

  // lookup OLD latitude and longitude
  bool ok;
  double latitude;
  double longitude;

  if (_location != 0) {
    ok = extractDoubleArray(shaper, &shapedJson, &latitude, &longitude);
  } else {
    ok = extractDoubleObject(shaper, &shapedJson, 0, &latitude);
    ok = ok && extractDoubleObject(shaper, &shapedJson, 1, &longitude);
  }

  // and remove old entry
  if (ok) {
    GeoCoordinate gc;
    gc.latitude = latitude;
    gc.longitude = longitude;
    gc.data = const_cast<void*>(static_cast<void const*>(doc));

    // ignore non-existing elements in geo-index
    GeoIndex_remove(_geoIndex, &gc);
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up all points within a given radius
////////////////////////////////////////////////////////////////////////////////

GeoCoordinates* GeoIndex2::withinQuery(arangodb::Transaction* trx, double lat,
                                       double lon, double radius) const {
  GeoCoordinate gc;
  gc.latitude = lat;
  gc.longitude = lon;

  return GeoIndex_PointsWithinRadius(_geoIndex, &gc, radius);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up the nearest points
////////////////////////////////////////////////////////////////////////////////

GeoCoordinates* GeoIndex2::nearQuery(arangodb::Transaction* trx, double lat,
                                     double lon, size_t count) const {
  GeoCoordinate gc;
  gc.latitude = lat;
  gc.longitude = lon;

  return GeoIndex_NearestCountPoints(_geoIndex, &gc, static_cast<int>(count));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts a double value from an object
////////////////////////////////////////////////////////////////////////////////

bool GeoIndex2::extractDoubleObject(VocShaper* shaper,
                                    TRI_shaped_json_t const* document,
                                    int which, double* result) {
  TRI_shape_pid_t const pid = (which == 0 ? _latitude : _longitude);

  TRI_shape_t const* shape;
  TRI_shaped_json_t json;

  if (!shaper->extractShapedJson(document, 0, pid, &json, &shape)) {
    return false;
  }

  if (shape == nullptr) {
    return false;
  } else if (json._sid == BasicShapes::TRI_SHAPE_SID_NUMBER) {
    *result = *(double*)json._data.data;
    return true;
  } else if (json._sid == BasicShapes::TRI_SHAPE_SID_NULL) {
    return false;
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts a double value from an array
////////////////////////////////////////////////////////////////////////////////

bool GeoIndex2::extractDoubleArray(VocShaper* shaper,
                                   TRI_shaped_json_t const* document,
                                   double* latitude, double* longitude) {
  TRI_shape_t const* shape;
  TRI_shaped_json_t list;

  bool ok = shaper->extractShapedJson(document, 0, _location, &list, &shape);

  if (!ok) {
    return false;
  }

  if (shape == nullptr) {
    return false;
  }

  // in-homogenous list
  if (shape->_type == TRI_SHAPE_LIST) {
    size_t len =
        TRI_LengthListShapedJson((const TRI_list_shape_t*)shape, &list);

    if (len < 2) {
      return false;
    }

    // latitude
    TRI_shaped_json_t entry;
    ok = TRI_AtListShapedJson((const TRI_list_shape_t*)shape, &list, 0, &entry);

    if (!ok || entry._sid != BasicShapes::TRI_SHAPE_SID_NUMBER) {
      return false;
    }

    *latitude = *(double*)entry._data.data;

    // longitude
    ok = TRI_AtListShapedJson((const TRI_list_shape_t*)shape, &list, 1, &entry);

    if (!ok || entry._sid != BasicShapes::TRI_SHAPE_SID_NUMBER) {
      return false;
    }

    *longitude = *(double*)entry._data.data;

    return true;
  }

  // homogenous list
  else if (shape->_type == TRI_SHAPE_HOMOGENEOUS_LIST) {
    const TRI_homogeneous_list_shape_t* hom;

    hom = (const TRI_homogeneous_list_shape_t*)shape;

    if (hom->_sidEntry != BasicShapes::TRI_SHAPE_SID_NUMBER) {
      return false;
    }

    size_t len = TRI_LengthHomogeneousListShapedJson(
        (const TRI_homogeneous_list_shape_t*)shape, &list);

    if (len < 2) {
      return false;
    }

    // latitude
    TRI_shaped_json_t entry;
    ok = TRI_AtHomogeneousListShapedJson(
        (const TRI_homogeneous_list_shape_t*)shape, &list, 0, &entry);

    if (!ok) {
      return false;
    }

    *latitude = *(double*)entry._data.data;

    // longitude
    ok = TRI_AtHomogeneousListShapedJson(
        (const TRI_homogeneous_list_shape_t*)shape, &list, 1, &entry);

    if (!ok) {
      return false;
    }

    *longitude = *(double*)entry._data.data;

    return true;
  }

  // homogeneous list
  else if (shape->_type == TRI_SHAPE_HOMOGENEOUS_SIZED_LIST) {
    const TRI_homogeneous_sized_list_shape_t* hom;

    hom = (const TRI_homogeneous_sized_list_shape_t*)shape;

    if (hom->_sidEntry != BasicShapes::TRI_SHAPE_SID_NUMBER) {
      return false;
    }

    size_t len = TRI_LengthHomogeneousSizedListShapedJson(
        (const TRI_homogeneous_sized_list_shape_t*)shape, &list);

    if (len < 2) {
      return false;
    }

    // latitude
    TRI_shaped_json_t entry;
    ok = TRI_AtHomogeneousSizedListShapedJson(
        (const TRI_homogeneous_sized_list_shape_t*)shape, &list, 0, &entry);

    if (!ok) {
      return false;
    }

    *latitude = *(double*)entry._data.data;

    // longitude
    ok = TRI_AtHomogeneousSizedListShapedJson(
        (const TRI_homogeneous_sized_list_shape_t*)shape, &list, 1, &entry);

    if (!ok) {
      return false;
    }

    *longitude = *(double*)entry._data.data;

    return true;
  }

  return false;
}
