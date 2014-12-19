////////////////////////////////////////////////////////////////////////////////
/// @brief geo index
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "geo-index.h"

#include "Basics/logging.h"
#include "Basics/tri-strings.h"
#include "VocBase/document-collection.h"
#include "VocBase/voc-shaper.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts a double value from an array
////////////////////////////////////////////////////////////////////////////////

static bool ExtractDoubleArray (TRI_shaper_t* shaper,
                                TRI_shaped_json_t const* document,
                                TRI_shape_pid_t pid,
                                double* result,
                                bool* missing) {
  TRI_shape_t const* shape;
  TRI_shaped_json_t json;
  bool ok;

  *missing = false;

  ok = TRI_ExtractShapedJsonVocShaper(shaper, document, 0, pid, &json, &shape);

  if (! ok) {
    return false;
  }

  if (shape == NULL) {
    *missing = true;
    return false;
  }
  else if (json._sid == TRI_LookupBasicSidShaper(TRI_SHAPE_NUMBER)) {
    *result = * (double*) json._data.data;
    return true;
  }
  else if (json._sid == TRI_LookupBasicSidShaper(TRI_SHAPE_NULL)) {
    *missing = true;
    return false;
  }
  else {
    return false;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts a double value from a list
////////////////////////////////////////////////////////////////////////////////

static bool ExtractDoubleList (TRI_shaper_t* shaper,
                               TRI_shaped_json_t const* document,
                               TRI_shape_pid_t pid,
                               double* latitude,
                               double* longitude,
                               bool* missing) {
  TRI_shape_t const* shape;
  TRI_shaped_json_t entry;
  TRI_shaped_json_t list;
  bool ok;
  size_t len;

  *missing = false;

  ok = TRI_ExtractShapedJsonVocShaper(shaper, document, 0, pid, &list, &shape);

  if (! ok) {
    return false;
  }

  if (shape == NULL) {
    *missing = true;
    return false;
  }

  // in-homogenous list
  if (shape->_type == TRI_SHAPE_LIST) {
    len = TRI_LengthListShapedJson((const TRI_list_shape_t*) shape, &list);

    if (len < 2) {
      return false;
    }

    // latitude
    ok = TRI_AtListShapedJson((const TRI_list_shape_t*) shape, &list, 0, &entry);

    if (! ok || entry._sid != TRI_LookupBasicSidShaper(TRI_SHAPE_NUMBER)) {
      return false;
    }

    *latitude = * (double*) entry._data.data;

    // longitude
    ok = TRI_AtListShapedJson((const TRI_list_shape_t*) shape, &list, 1, &entry);

    if (! ok || entry._sid != TRI_LookupBasicSidShaper(TRI_SHAPE_NUMBER)) {
      return false;
    }

    *longitude = * (double*) entry._data.data;

    return true;
  }

  // homogenous list
  else if (shape->_type == TRI_SHAPE_HOMOGENEOUS_LIST) {
    const TRI_homogeneous_list_shape_t* hom;

    hom = (const TRI_homogeneous_list_shape_t*) shape;

    if (hom->_sidEntry != TRI_LookupBasicSidShaper(TRI_SHAPE_NUMBER)) {
      return false;
    }

    len = TRI_LengthHomogeneousListShapedJson((const TRI_homogeneous_list_shape_t*) shape, &list);

    if (len < 2) {
      return false;
    }

    // latitude
    ok = TRI_AtHomogeneousListShapedJson((const TRI_homogeneous_list_shape_t*) shape, &list, 0, &entry);

    if (! ok) {
      return false;
    }

    *latitude = * (double*) entry._data.data;

    // longitude
    ok = TRI_AtHomogeneousListShapedJson((const TRI_homogeneous_list_shape_t*) shape, &list, 1, &entry);

    if (! ok) {
      return false;
    }

    *longitude = * (double*) entry._data.data;

    return true;
  }

  // homogeneous list
  else if (shape->_type == TRI_SHAPE_HOMOGENEOUS_SIZED_LIST) {
    const TRI_homogeneous_sized_list_shape_t* hom;

    hom = (const TRI_homogeneous_sized_list_shape_t*) shape;

    if (hom->_sidEntry != TRI_LookupBasicSidShaper(TRI_SHAPE_NUMBER)) {
      return false;
    }

    len = TRI_LengthHomogeneousSizedListShapedJson((const TRI_homogeneous_sized_list_shape_t*) shape, &list);

    if (len < 2) {
      return false;
    }

    // latitude
    ok = TRI_AtHomogeneousSizedListShapedJson((const TRI_homogeneous_sized_list_shape_t*) shape, &list, 0, &entry);

    if (! ok) {
      return false;
    }

    *latitude = * (double*) entry._data.data;

    // longitude
    ok = TRI_AtHomogeneousSizedListShapedJson((const TRI_homogeneous_sized_list_shape_t*) shape, &list, 1, &entry);

    if (! ok) {
      return false;
    }

    *longitude = * (double*) entry._data.data;

    return true;
  }

  // null
  else if (shape->_type == TRI_SHAPE_NULL) {
    *missing = true;
  }

  // ups
  return false;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                         GEO INDEX
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief return the memory used by the index
////////////////////////////////////////////////////////////////////////////////

static size_t MemoryGeoIndex (TRI_index_t const* idx) {
  TRI_geo_index_t const* geo = (TRI_geo_index_t const*) idx;

  return GeoIndex_MemoryUsage(geo->_geoIndex);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief JSON description of a geo index, location is a list
////////////////////////////////////////////////////////////////////////////////

static TRI_json_t* JsonGeo1Index (TRI_index_t const* idx) {
  TRI_json_t* json;
  TRI_json_t* fields;
  TRI_shape_path_t const* path;
  char const* location;

  TRI_geo_index_t const* geo = (TRI_geo_index_t const*) idx;
  TRI_document_collection_t* document = idx->_collection;

  // convert location to string
  path = document->getShaper()->lookupAttributePathByPid(document->getShaper(), geo->_location);  // ONLY IN INDEX, PROTECTED by RUNTIME

  if (path == 0) {
    return nullptr;
  }

  location = TRI_NAME_SHAPE_PATH(path);

  // create json
  json = TRI_JsonIndex(TRI_CORE_MEM_ZONE, idx);

  if (json == nullptr) {
    return nullptr;
  }

  TRI_Insert3ObjectJson(TRI_CORE_MEM_ZONE, json, "geoJson", TRI_CreateBooleanJson(TRI_CORE_MEM_ZONE, geo->_geoJson));

  // "constraint" and "unique" are identical for geo indexes.
  // we return "constraint" just for downwards-compatibility
  TRI_Insert3ObjectJson(TRI_CORE_MEM_ZONE, json, "constraint", TRI_CreateBooleanJson(TRI_CORE_MEM_ZONE, idx->_unique));

  TRI_Insert3ObjectJson(TRI_CORE_MEM_ZONE, json, "ignoreNull", TRI_CreateBooleanJson(TRI_CORE_MEM_ZONE, idx->_ignoreNull));

  fields = TRI_CreateArrayJson(TRI_CORE_MEM_ZONE);
  TRI_PushBack3ArrayJson(TRI_CORE_MEM_ZONE, fields, TRI_CreateStringCopyJson(TRI_CORE_MEM_ZONE, location));
  TRI_Insert3ObjectJson(TRI_CORE_MEM_ZONE, json, "fields", fields);

  return json;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief JSON description of a geo index, two attributes
////////////////////////////////////////////////////////////////////////////////

static TRI_json_t* JsonGeo2Index (TRI_index_t const* idx) {
  TRI_json_t* json;
  TRI_json_t* fields;
  TRI_shape_path_t const* path;
  char const* latitude;
  char const* longitude;

  TRI_geo_index_t const* geo = (TRI_geo_index_t const*) idx;
  TRI_document_collection_t* document = idx->_collection;

  // convert latitude to string
  path = document->getShaper()->lookupAttributePathByPid(document->getShaper(), geo->_latitude);  // ONLY IN INDEX, PROTECTED by RUNTIME

  if (path == 0) {
    return nullptr;
  }

  latitude = TRI_NAME_SHAPE_PATH(path);

  // convert longitude to string
  path = document->getShaper()->lookupAttributePathByPid(document->getShaper(), geo->_longitude);  // ONLY IN INDEX, PROTECTED by RUNTIME

  if (path == 0) {
    return nullptr;
  }

  longitude = TRI_NAME_SHAPE_PATH(path);

  // create json
  json = TRI_JsonIndex(TRI_CORE_MEM_ZONE, idx);

  if (json == nullptr) {
    return nullptr;
  }

  // "constraint" and "unique" are identical for geo indexes.
  // we return "constraint" just for downwards-compatibility
  TRI_Insert3ObjectJson(TRI_CORE_MEM_ZONE, json, "constraint", TRI_CreateBooleanJson(TRI_CORE_MEM_ZONE, idx->_unique));

  TRI_Insert3ObjectJson(TRI_CORE_MEM_ZONE, json, "ignoreNull", TRI_CreateBooleanJson(TRI_CORE_MEM_ZONE, geo->base._ignoreNull));

  fields = TRI_CreateArrayJson(TRI_CORE_MEM_ZONE);
  TRI_PushBack3ArrayJson(TRI_CORE_MEM_ZONE, fields, TRI_CreateStringCopyJson(TRI_CORE_MEM_ZONE, latitude));
  TRI_PushBack3ArrayJson(TRI_CORE_MEM_ZONE, fields, TRI_CreateStringCopyJson(TRI_CORE_MEM_ZONE, longitude));
  TRI_Insert3ObjectJson(TRI_CORE_MEM_ZONE, json, "fields", fields);

  return json;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief inserts a new document
////////////////////////////////////////////////////////////////////////////////

static int InsertGeoIndex (TRI_index_t* idx,
                           TRI_doc_mptr_t const* doc,
                           bool isRollback) {
  GeoCoordinate gc;
  TRI_shaped_json_t shapedJson;
  TRI_geo_index_t* geo;
  TRI_shaper_t* shaper;
  bool missing;
  bool ok;
  double latitude;
  double longitude;
  int res;

  geo = (TRI_geo_index_t*) idx;
  shaper = geo->base._collection->getShaper();  // ONLY IN INDEX, PROTECTED by RUNTIME

  // lookup latitude and longitude
  TRI_EXTRACT_SHAPED_JSON_MARKER(shapedJson, doc->getDataPtr());  // ONLY IN INDEX, PROTECTED by RUNTIME

  if (geo->_location != 0) {
    if (geo->_geoJson) {
      ok = ExtractDoubleList(shaper, &shapedJson, geo->_location, &longitude, &latitude, &missing);
    }
    else {
      ok = ExtractDoubleList(shaper, &shapedJson, geo->_location, &latitude, &longitude, &missing);
    }
  }
  else {
    ok = ExtractDoubleArray(shaper, &shapedJson, geo->_latitude, &latitude, &missing);
    ok = ok && ExtractDoubleArray(shaper, &shapedJson, geo->_longitude, &longitude, &missing);
  }

  if (! ok) {
    if (idx->_unique) {
      if (idx->_ignoreNull && missing) {
        return TRI_ERROR_NO_ERROR;
      }
      else {
        return TRI_set_errno(TRI_ERROR_ARANGO_GEO_INDEX_VIOLATED);
      }
    }
    else {
      return TRI_ERROR_NO_ERROR;
    }
  }

  // and insert into index
  gc.latitude = latitude;
  gc.longitude = longitude;

  gc.data = CONST_CAST(doc);

  res = GeoIndex_insert(geo->_geoIndex, &gc);

  if (res == -1) {
    LOG_WARNING("found duplicate entry in geo-index, should not happen");
    return TRI_set_errno(TRI_ERROR_INTERNAL);
  }
  else if (res == -2) {
    return TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
  }
  else if (res == -3) {
    if (idx->_unique) {
      LOG_DEBUG("illegal geo-coordinates, ignoring entry");
      return TRI_set_errno(TRI_ERROR_ARANGO_GEO_INDEX_VIOLATED);
    }
    else {
      return TRI_ERROR_NO_ERROR;
    }
  }
  else if (res < 0) {
    return TRI_set_errno(TRI_ERROR_INTERNAL);
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief erases a document
////////////////////////////////////////////////////////////////////////////////

static int RemoveGeoIndex (TRI_index_t* idx,
                           TRI_doc_mptr_t const* doc,
                           bool isRollback) {
  GeoCoordinate gc;
  TRI_shaped_json_t shapedJson;
  TRI_geo_index_t* geo;
  TRI_shaper_t* shaper;
  bool missing;
  bool ok;
  double latitude;
  double longitude;

  geo = (TRI_geo_index_t*) idx;
  shaper = geo->base._collection->getShaper();  // ONLY IN INDEX, PROTECTED by RUNTIME
  TRI_EXTRACT_SHAPED_JSON_MARKER(shapedJson, doc->getDataPtr());  // ONLY IN INDEX, PROTECTED by RUNTIME

  // lookup OLD latitude and longitude
  if (geo->_location != 0) {
    ok = ExtractDoubleList(shaper, &shapedJson, geo->_location, &latitude, &longitude, &missing);
  }
  else {
    ok = ExtractDoubleArray(shaper, &shapedJson, geo->_latitude, &latitude, &missing);
    ok = ok && ExtractDoubleArray(shaper, &shapedJson, geo->_longitude, &longitude, &missing);
  }

  // and remove old entry
  if (ok) {
    gc.latitude = latitude;
    gc.longitude = longitude;

    gc.data = CONST_CAST(doc);

    // ignore non-existing elements in geo-index
    GeoIndex_remove(geo->_geoIndex, &gc);
  }

  return TRI_ERROR_NO_ERROR;
}

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a geo-index for lists
////////////////////////////////////////////////////////////////////////////////

TRI_index_t* TRI_CreateGeo1Index (TRI_document_collection_t* document,
                                  TRI_idx_iid_t iid,
                                  char const* locationName,
                                  TRI_shape_pid_t location,
                                  bool geoJson,
                                  bool unique,
                                  bool ignoreNull) {
  char* ln;

  TRI_geo_index_t* geo = static_cast<TRI_geo_index_t*>(TRI_Allocate(TRI_CORE_MEM_ZONE, sizeof(TRI_geo_index_t), false));
  TRI_index_t* idx = &geo->base;

  TRI_InitVectorString(&idx->_fields, TRI_CORE_MEM_ZONE);

  TRI_InitIndex(idx, iid, TRI_IDX_TYPE_GEO1_INDEX, document, unique, false);

  idx->_ignoreNull = ignoreNull;

  idx->memory   = MemoryGeoIndex;
  idx->json     = JsonGeo1Index;
  idx->insert   = InsertGeoIndex;
  idx->remove   = RemoveGeoIndex;

  ln = TRI_DuplicateStringZ(TRI_CORE_MEM_ZONE, locationName);
  TRI_PushBackVectorString(&idx->_fields, ln);

  geo->_geoIndex = GeoIndex_new();

  // oops, out of memory?
  if (geo->_geoIndex == NULL) {
    TRI_DestroyVectorString(&idx->_fields);
    TRI_Free(TRI_CORE_MEM_ZONE, geo);
    return NULL;
  }

  geo->_variant    = geoJson ? INDEX_GEO_COMBINED_LAT_LON : INDEX_GEO_COMBINED_LON_LAT;
  geo->_location   = location;
  geo->_latitude   = 0;
  geo->_longitude  = 0;
  geo->_geoJson    = geoJson;

  return idx;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a geo-index for arrays
////////////////////////////////////////////////////////////////////////////////

TRI_index_t* TRI_CreateGeo2Index (TRI_document_collection_t* document,
                                  TRI_idx_iid_t iid,
                                  char const* latitudeName,
                                  TRI_shape_pid_t latitude,
                                  char const* longitudeName,
                                  TRI_shape_pid_t longitude,
                                  bool unique,
                                  bool ignoreNull) {
  char* lat;
  char* lon;

  TRI_geo_index_t* geo = static_cast<TRI_geo_index_t*>(TRI_Allocate(TRI_CORE_MEM_ZONE, sizeof(TRI_geo_index_t), false));
  TRI_index_t* idx = &geo->base;

  TRI_InitVectorString(&idx->_fields, TRI_CORE_MEM_ZONE);

  TRI_InitIndex(idx, iid, TRI_IDX_TYPE_GEO2_INDEX, document, unique, false); 
  idx->_ignoreNull = ignoreNull;

  idx->memory   = MemoryGeoIndex;
  idx->json     = JsonGeo2Index;
  idx->insert   = InsertGeoIndex;
  idx->remove   = RemoveGeoIndex;

  lat = TRI_DuplicateStringZ(TRI_CORE_MEM_ZONE, latitudeName);
  lon = TRI_DuplicateStringZ(TRI_CORE_MEM_ZONE, longitudeName);
  TRI_PushBackVectorString(&idx->_fields, lat);
  TRI_PushBackVectorString(&idx->_fields, lon);

  geo->_geoIndex = GeoIndex_new();

  // oops, out of memory?
  if (geo->_geoIndex == NULL) {
    TRI_DestroyVectorString(&idx->_fields);
    TRI_Free(TRI_CORE_MEM_ZONE, geo);
    return NULL;
  }

  geo->_variant    = INDEX_GEO_INDIVIDUAL_LAT_LON;
  geo->_location   = 0;
  geo->_latitude   = latitude;
  geo->_longitude  = longitude;

  return idx;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the memory allocated, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyGeoIndex (TRI_index_t* idx) {
  TRI_geo_index_t* geo;

  TRI_DestroyVectorString(&idx->_fields);

  geo = (TRI_geo_index_t*) idx;

  GeoIndex_free(geo->_geoIndex);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the memory allocated and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeGeoIndex (TRI_index_t* idx) {
  TRI_DestroyGeoIndex(idx);
  TRI_Free(TRI_CORE_MEM_ZONE, idx);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up all points within a given radius
////////////////////////////////////////////////////////////////////////////////

GeoCoordinates* TRI_WithinGeoIndex (TRI_index_t* idx,
                                    double lat,
                                    double lon,
                                    double radius) {
  TRI_geo_index_t* geo;
  GeoCoordinate gc;

  geo = (TRI_geo_index_t*) idx;
  gc.latitude = lat;
  gc.longitude = lon;

  return GeoIndex_PointsWithinRadius(geo->_geoIndex, &gc, radius);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up the nearest points
////////////////////////////////////////////////////////////////////////////////

GeoCoordinates* TRI_NearestGeoIndex (TRI_index_t* idx,
                                     double lat,
                                     double lon,
                                     size_t count) {
  GeoCoordinate gc;

  TRI_geo_index_t* geo = (TRI_geo_index_t*) idx;
  gc.latitude = lat;
  gc.longitude = lon;

  return GeoIndex_NearestCountPoints(geo->_geoIndex, &gc, (int) count);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
