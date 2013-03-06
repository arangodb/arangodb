////////////////////////////////////////////////////////////////////////////////
/// @brief geo index
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Copyright 2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "geo-index.h"

#include "BasicsC/logging.h"
#include "BasicsC/strings.h"
#include "VocBase/document-collection.h"
#include "VocBase/voc-shaper.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

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
  else if (json._sid == shaper->_sidNumber) {
    *result = * (double*) json._data.data;
    return true;
  }
  else if (json._sid == shaper->_sidNull) {
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

    if (! ok || entry._sid != shaper->_sidNumber) {
      return false;
    }

    *latitude = * (double*) entry._data.data;

    // longitude
    ok = TRI_AtListShapedJson((const TRI_list_shape_t*) shape, &list, 1, &entry);

    if (! ok || entry._sid != shaper->_sidNumber) {
      return false;
    }

    *longitude = * (double*) entry._data.data;

    return true;
  }

  // homogenous list
  else if (shape->_type == TRI_SHAPE_HOMOGENEOUS_LIST) {
    const TRI_homogeneous_list_shape_t* hom;

    hom = (const TRI_homogeneous_list_shape_t*) shape;

    if (hom->_sidEntry != shaper->_sidNumber) {
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

    if (hom->_sidEntry != shaper->_sidNumber) {
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

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                         GEO INDEX
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief JSON description of a geo index, location is a list
////////////////////////////////////////////////////////////////////////////////

static TRI_json_t* JsonGeo1Index (TRI_index_t* idx,
                                  TRI_primary_collection_t const* collection) {
  TRI_json_t* json;
  TRI_json_t* fields;
  TRI_shape_path_t const* path;
  char const* location;
  TRI_geo_index_t* geo;

  geo = (TRI_geo_index_t*) idx;

  // convert location to string
  path = collection->_shaper->lookupAttributePathByPid(collection->_shaper, geo->_location);

  if (path == 0) {
    return NULL;
  }

  location = TRI_NAME_SHAPE_PATH(path);

  // create json
  json = TRI_CreateArrayJson(TRI_CORE_MEM_ZONE);
  fields = TRI_CreateListJson(TRI_CORE_MEM_ZONE);

  TRI_PushBack3ListJson(TRI_CORE_MEM_ZONE, fields, TRI_CreateStringCopyJson(TRI_CORE_MEM_ZONE, location));

  TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, json, "id", TRI_CreateNumberJson(TRI_CORE_MEM_ZONE, idx->_iid));
  TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, json, "type", TRI_CreateStringCopyJson(TRI_CORE_MEM_ZONE, "geo1"));
  TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, json, "geoJson", TRI_CreateBooleanJson(TRI_CORE_MEM_ZONE, geo->_geoJson));
  TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, json, "constraint", TRI_CreateBooleanJson(TRI_CORE_MEM_ZONE, geo->_constraint));

  if (geo->_constraint) {
    TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, json, "ignoreNull", TRI_CreateBooleanJson(TRI_CORE_MEM_ZONE, geo->base._ignoreNull));
  }

  TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, json, "fields", fields);

  return json;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief JSON description of a geo index, two attributes
////////////////////////////////////////////////////////////////////////////////

static TRI_json_t* JsonGeo2Index (TRI_index_t* idx,
                                  TRI_primary_collection_t const* collection) {
  TRI_json_t* json;
  TRI_json_t* fields;
  TRI_shape_path_t const* path;
  char const* latitude;
  char const* longitude;
  TRI_geo_index_t* geo;

  geo = (TRI_geo_index_t*) idx;

  // convert latitude to string
  path = collection->_shaper->lookupAttributePathByPid(collection->_shaper, geo->_latitude);

  if (path == 0) {
    return NULL;
  }

  latitude = TRI_NAME_SHAPE_PATH(path);

  // convert longitude to string
  path = collection->_shaper->lookupAttributePathByPid(collection->_shaper, geo->_longitude);

  if (path == 0) {
    return NULL;
  }

  longitude = TRI_NAME_SHAPE_PATH(path);

  // create json
  json = TRI_CreateArrayJson(TRI_CORE_MEM_ZONE);
  fields = TRI_CreateListJson(TRI_CORE_MEM_ZONE);

  TRI_PushBack3ListJson(TRI_CORE_MEM_ZONE, fields, TRI_CreateStringCopyJson(TRI_CORE_MEM_ZONE, latitude));
  TRI_PushBack3ListJson(TRI_CORE_MEM_ZONE, fields, TRI_CreateStringCopyJson(TRI_CORE_MEM_ZONE, longitude));

  TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, json, "id", TRI_CreateNumberJson(TRI_CORE_MEM_ZONE, idx->_iid));
  TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, json, "type", TRI_CreateStringCopyJson(TRI_CORE_MEM_ZONE, "geo2"));
  TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, json, "constraint", TRI_CreateBooleanJson(TRI_CORE_MEM_ZONE, geo->_constraint));

  if (geo->_constraint) {
    TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, json, "ignoreNull", TRI_CreateBooleanJson(TRI_CORE_MEM_ZONE, geo->base._ignoreNull));
  }

  TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, json, "fields", fields);

  return json;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes a json index from collection
////////////////////////////////////////////////////////////////////////////////

static void RemoveIndexGeoIndex (TRI_index_t* idx, TRI_primary_collection_t* collection) {
  // the index will later by destroyed, so do nothing here
}

////////////////////////////////////////////////////////////////////////////////
/// @brief inserts a new document
////////////////////////////////////////////////////////////////////////////////

static int InsertGeoIndex (TRI_index_t* idx, TRI_doc_mptr_t const* doc) {
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
  shaper = geo->base._collection->_shaper;

  // lookup latitude and longitude
  TRI_EXTRACT_SHAPED_JSON_MARKER(shapedJson, doc->_data);

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
    if (geo->_constraint) {
      if (geo->base._ignoreNull && missing) {
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
    if (geo->_constraint) {
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

static int RemoveGeoIndex (TRI_index_t* idx, TRI_doc_mptr_t const* doc) {
  GeoCoordinate gc;
  TRI_shaped_json_t shapedJson;
  TRI_geo_index_t* geo;
  TRI_shaper_t* shaper;
  bool missing;
  bool ok;
  double latitude;
  double longitude;

  geo = (TRI_geo_index_t*) idx;
  shaper = geo->base._collection->_shaper;
  TRI_EXTRACT_SHAPED_JSON_MARKER(shapedJson, doc->_data);

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

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a geo-index for lists
////////////////////////////////////////////////////////////////////////////////

TRI_index_t* TRI_CreateGeo1Index (struct TRI_primary_collection_s* collection,
                                  char const* locationName,
                                  TRI_shape_pid_t location,
                                  bool geoJson,
                                  bool constraint,
                                  bool ignoreNull) {
  TRI_geo_index_t* geo;
  char* ln;

  geo = TRI_Allocate(TRI_CORE_MEM_ZONE, sizeof(TRI_geo_index_t), false);
  ln = TRI_DuplicateStringZ(TRI_CORE_MEM_ZONE, locationName);

  TRI_InitVectorString(&geo->base._fields, TRI_CORE_MEM_ZONE);

  TRI_InitIndex(&geo->base, TRI_IDX_TYPE_GEO1_INDEX, collection, false);
  geo->base._ignoreNull = ignoreNull;

  geo->base.json = JsonGeo1Index;
  geo->base.removeIndex = RemoveIndexGeoIndex;

  geo->base.insert = InsertGeoIndex;
  geo->base.remove = RemoveGeoIndex;

  TRI_PushBackVectorString(&geo->base._fields, ln);

  geo->_constraint = constraint;
  geo->_geoIndex = GeoIndex_new();

  // oops, out of memory?
  if (geo->_geoIndex == NULL) {
    TRI_DestroyVectorString(&geo->base._fields);
    TRI_Free(TRI_CORE_MEM_ZONE, geo);
    return NULL;
  }

  geo->_variant = geoJson ? INDEX_GEO_COMBINED_LAT_LON : INDEX_GEO_COMBINED_LON_LAT;
  geo->_location = location;
  geo->_latitude = 0;
  geo->_longitude = 0;
  geo->_geoJson = geoJson;

  GeoIndex_assignMethod(&(geo->base.indexQuery), TRI_INDEX_METHOD_ASSIGNMENT_QUERY);
  GeoIndex_assignMethod(&(geo->base.indexQueryFree), TRI_INDEX_METHOD_ASSIGNMENT_FREE);
  GeoIndex_assignMethod(&(geo->base.indexQueryResult), TRI_INDEX_METHOD_ASSIGNMENT_RESULT);

  return &geo->base;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a geo-index for arrays
////////////////////////////////////////////////////////////////////////////////

TRI_index_t* TRI_CreateGeo2Index (struct TRI_primary_collection_s* collection,
                                  char const* latitudeName,
                                  TRI_shape_pid_t latitude,
                                  char const* longitudeName,
                                  TRI_shape_pid_t longitude,
                                  bool constraint,
                                  bool ignoreNull) {
  TRI_geo_index_t* geo;
  char* lat;
  char* lon;

  geo = TRI_Allocate(TRI_CORE_MEM_ZONE, sizeof(TRI_geo_index_t), false);
  lat = TRI_DuplicateStringZ(TRI_CORE_MEM_ZONE, latitudeName);
  lon = TRI_DuplicateStringZ(TRI_CORE_MEM_ZONE, longitudeName);

  TRI_InitVectorString(&geo->base._fields, TRI_CORE_MEM_ZONE);

  TRI_InitIndex(&geo->base, TRI_IDX_TYPE_GEO2_INDEX, collection, false);
  geo->base._ignoreNull = ignoreNull;

  geo->base.json = JsonGeo2Index;
  geo->base.removeIndex = RemoveIndexGeoIndex;

  geo->base.insert = InsertGeoIndex;
  geo->base.remove = RemoveGeoIndex;

  TRI_PushBackVectorString(&geo->base._fields, lat);
  TRI_PushBackVectorString(&geo->base._fields, lon);

  geo->_constraint = constraint;
  geo->_geoIndex = GeoIndex_new();

  // oops, out of memory?
  if (geo->_geoIndex == NULL) {
    TRI_DestroyVectorString(&geo->base._fields);
    TRI_Free(TRI_CORE_MEM_ZONE, geo);
    return NULL;
  }

  geo->_variant = INDEX_GEO_INDIVIDUAL_LAT_LON;
  geo->_location = 0;
  geo->_latitude = latitude;
  geo->_longitude = longitude;

  GeoIndex_assignMethod(&(geo->base.indexQuery), TRI_INDEX_METHOD_ASSIGNMENT_QUERY);
  GeoIndex_assignMethod(&(geo->base.indexQueryFree), TRI_INDEX_METHOD_ASSIGNMENT_FREE);
  GeoIndex_assignMethod(&(geo->base.indexQueryResult), TRI_INDEX_METHOD_ASSIGNMENT_RESULT);

  return &geo->base;
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
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, idx);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

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
  TRI_geo_index_t* geo;
  GeoCoordinate gc;

  geo = (TRI_geo_index_t*) idx;
  gc.latitude = lat;
  gc.longitude = lon;

  return GeoIndex_NearestCountPoints(geo->_geoIndex, &gc, count);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
