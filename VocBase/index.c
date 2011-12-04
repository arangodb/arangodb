////////////////////////////////////////////////////////////////////////////////
/// @brief index
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2011 triagens GmbH, Cologne, Germany
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
/// @author Copyright 2011, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "index.h"

#include <Basics/conversions.h>
#include <Basics/files.h>
#include <Basics/json.h>
#include <Basics/logging.h>
#include <Basics/strings.h>
#include <VocBase/simple-collection.h>

#include "ShapedJson/shaped-json.h"
#include "ShapedJson/shape-accessor.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                             INDEX
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an index file
////////////////////////////////////////////////////////////////////////////////

bool TRI_RemoveIndex (TRI_doc_collection_t* collection, TRI_index_t* idx) {
  char* filename;
  char* name;
  char* number;
  bool ok;

  // construct filename
  number = TRI_StringUInt64(idx->_iid);
  name = TRI_Concatenate3String("index-", number, ".json");
  filename = TRI_Concatenate2File(collection->base._directory, name);
  TRI_FreeString(name);
  TRI_FreeString(number);

  ok = TRI_UnlinkFile(filename);
  TRI_FreeString(filename);

  if (! ok) {
    LOG_ERROR("cannot remove index definition: %s", TRI_last_error());
    return false;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief saves an index
////////////////////////////////////////////////////////////////////////////////

bool TRI_SaveIndex (TRI_doc_collection_t* collection, TRI_index_t* idx) {
  TRI_json_t* json;
  char* filename;
  char* name;
  char* number;
  bool ok;

  // convert into JSON
  json = idx->json(idx, collection);

  if (json == NULL) {
    LOG_TRACE("cannot save index definition: index cannot be jsonfied");
    return false;
  }

  // construct filename
  number = TRI_StringUInt64(idx->_iid);
  name = TRI_Concatenate3String("index-", number, ".json");
  filename = TRI_Concatenate2File(collection->base._directory, name);
  TRI_FreeString(name);
  TRI_FreeString(number);

  // and save
  ok = TRI_SaveJson(filename, json);
  TRI_FreeString(filename);
  TRI_FreeJson(json);

  if (! ok) {
    LOG_ERROR("cannot save index definition: %s", TRI_last_error());
    return false;
  }

  return true;
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
/// @brief extracts a boolean from an array
////////////////////////////////////////////////////////////////////////////////

static bool ExtractDoubleArray (TRI_shaper_t* shaper,
                                TRI_shaped_json_t const* document,
                                TRI_shape_pid_t pid,
                                double* result) {
  TRI_shape_access_t* acc;
  TRI_shaped_json_t json;
  TRI_shape_sid_t sid;
  bool ok;

  sid = document->_sid;
  acc = TRI_ShapeAccessor(shaper, sid, pid);

  if (acc == NULL || acc->_shape == NULL) {
    return false;
  }

  if (acc->_shape->_sid != shaper->_sidNumber) {
    return false;
  }

  ok = TRI_ExecuteShapeAccessor(acc, document, &json);
  *result = * (double*) json._data.data;

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts a boolean from a list
////////////////////////////////////////////////////////////////////////////////

static bool ExtractDoubleList (TRI_shaper_t* shaper,
                               TRI_shaped_json_t const* document,
                               TRI_shape_pid_t pid,
                               double* latitude,
                               double* longitude) {
  TRI_shape_access_t* acc;
  TRI_shaped_json_t list;
  TRI_shaped_json_t entry;
  TRI_shape_sid_t sid;
  size_t len;
  bool ok;

  sid = document->_sid;
  acc = TRI_ShapeAccessor(shaper, sid, pid);

  if (acc == NULL || acc->_shape == NULL) {
    return false;
  }

  // in-homogenous list
  if (acc->_shape->_type == TRI_SHAPE_LIST) {
    ok = TRI_ExecuteShapeAccessor(acc, document, &list);
    len = TRI_LengthListShapedJson((TRI_list_shape_t*) acc->_shape, &list);

    if (len < 2) {
      return false;
    }

    // latitude
    ok = TRI_AtListShapedJson((TRI_list_shape_t*) acc->_shape, &list, 0, &entry);

    if (! ok || entry._sid != shaper->_sidNumber) {
      return false;
    }

    *latitude = * (double*) entry._data.data;

    // longitude
    ok = TRI_AtListShapedJson((TRI_list_shape_t*) acc->_shape, &list, 1, &entry);

    if (!ok || entry._sid != shaper->_sidNumber) {
      return false;
    }

    *longitude = * (double*) entry._data.data;

    return true;
  }

  // homogenous list
  else if (acc->_shape->_type == TRI_SHAPE_HOMOGENEOUS_LIST) {
    TRI_homogeneous_list_shape_t* hom;

    hom = (TRI_homogeneous_list_shape_t*) acc->_shape;

    if (hom->_sidEntry != shaper->_sidNumber) {
      return false;
    }

    ok = TRI_ExecuteShapeAccessor(acc, document, &list);

    if (! ok) {
      return false;
    }

    len = TRI_LengthHomogeneousListShapedJson((TRI_homogeneous_list_shape_t*) acc->_shape, &list);

    if (len < 2) {
      return false;
    }

    // latitude
    ok = TRI_AtHomogeneousListShapedJson((TRI_homogeneous_list_shape_t*) acc->_shape, &list, 0, &entry);

    if (! ok) {
      return false;
    }

    *latitude = * (double*) entry._data.data;

    // longitude
    ok = TRI_AtHomogeneousListShapedJson((TRI_homogeneous_list_shape_t*) acc->_shape, &list, 1, &entry);

    if (! ok) {
      return false;
    }

    *longitude = * (double*) entry._data.data;

    return true;
  }

  // homogeneous list
  else if (acc->_shape->_type == TRI_SHAPE_HOMOGENEOUS_SIZED_LIST) {
    TRI_homogeneous_sized_list_shape_t* hom;

    hom = (TRI_homogeneous_sized_list_shape_t*) acc->_shape;

    if (hom->_sidEntry != shaper->_sidNumber) {
      return false;
    }

    ok = TRI_ExecuteShapeAccessor(acc, document, &list);

    if (! ok) {
      return false;
    }

    len = TRI_LengthHomogeneousSizedListShapedJson((TRI_homogeneous_sized_list_shape_t*) acc->_shape, &list);

    if (len < 2) {
      return false;
    }

    // latitude
    ok = TRI_AtHomogeneousSizedListShapedJson((TRI_homogeneous_sized_list_shape_t*) acc->_shape, &list, 0, &entry);

    if (! ok) {
      return false;
    }

    *latitude = * (double*) entry._data.data;

    // longitude
    ok = TRI_AtHomogeneousSizedListShapedJson((TRI_homogeneous_sized_list_shape_t*) acc->_shape, &list, 1, &entry);

    if (! ok) {
      return false;
    }

    *longitude = * (double*) entry._data.data;

    return true;
  }

  // ups
  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief inserts a new document, location is a list
////////////////////////////////////////////////////////////////////////////////

static bool InsertGeoIndex (TRI_index_t* idx, TRI_doc_mptr_t const* doc) {
  GeoCoordinate gc;
  TRI_shaper_t* shaper;
  bool ok;
  double latitude;
  double longitude;
  TRI_geo_index_t* geo;
  int res;

  geo = (TRI_geo_index_t*) idx;
  shaper = geo->base._collection->_shaper;

  // lookup latitude and longitude
  if (geo->_location != 0) {
    if (geo->_geoJson) {
      ok = ExtractDoubleList(shaper, &doc->_document, geo->_location, &longitude, &latitude);
    }
    else {
      ok = ExtractDoubleList(shaper, &doc->_document, geo->_location, &latitude, &longitude);
    }
  }
  else {
    ok = ExtractDoubleArray(shaper, &doc->_document, geo->_latitude, &latitude);
    ok = ok && ExtractDoubleArray(shaper, &doc->_document, geo->_longitude, &longitude);
  }

  if (! ok) {
    return false;
  }

  // and insert into index
  gc.latitude = latitude;
  gc.longitude = longitude;
  gc.data = doc;

  res = GeoIndex_insert(geo->_geoIndex, &gc);

  if (res == -1) {
    LOG_WARNING("found duplicate entry in geo-index, should not happend");
  }
  else if (res == -2) {
    LOG_WARNING("out-of-memory in geo-index");
  }
  else if (res == -3) {
    LOG_DEBUG("illegal geo-coordinates, ignoring entry");
  }

  return res == 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief updates a document, location is a list
////////////////////////////////////////////////////////////////////////////////

static bool UpdateGeoIndex (TRI_index_t* idx, TRI_doc_mptr_t const* doc, TRI_shaped_json_t const* old) {
  GeoCoordinate gc;
  TRI_shaper_t* shaper;
  bool ok;
  double latitude;
  double longitude;
  TRI_geo_index_t* geo;
  int res;

  geo = (TRI_geo_index_t*) idx;
  shaper = geo->base._collection->_shaper;

  // lookup OLD latitude and longitude
  if (geo->_location != 0) {
    ok = ExtractDoubleList(shaper, old, geo->_location, &latitude, &longitude);
  }
  else {
    ok = ExtractDoubleArray(shaper, old, geo->_latitude, &latitude);
    ok = ok && ExtractDoubleArray(shaper, old, geo->_longitude, &longitude);
  }

  // and remove old entry
  if (ok) {
    gc.latitude = latitude;
    gc.longitude = longitude;
    gc.data = doc;

    res = GeoIndex_remove(geo->_geoIndex, &gc);

    if (res != 0) {
      LOG_WARNING("cannot remove old index entry: %d", res);
    }
  }

  // create new entry with new coordinates
  if (geo->_location != 0) {
    ok = ExtractDoubleList(shaper, &doc->_document, geo->_location, &latitude, &longitude);
  }
  else {
    ok = ExtractDoubleArray(shaper, &doc->_document, geo->_latitude, &latitude);
    ok = ok && ExtractDoubleArray(shaper, &doc->_document, geo->_longitude, &longitude);
  }

  if (! ok) {
    return false;
  }

  gc.latitude = latitude;
  gc.longitude = longitude;
  gc.data = doc;

  res = GeoIndex_insert(geo->_geoIndex, &gc);

  if (res == -1) {
    LOG_WARNING("found duplicate entry in geo-index, should not happend");
  }
  else if (res == -2) {
    LOG_WARNING("out-of-memory in geo-index");
  }
  else if (res == -3) {
    LOG_DEBUG("illegal geo-coordinates, ignoring entry");
  }

  return res == 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief erases a document, location is a list
////////////////////////////////////////////////////////////////////////////////

static bool RemoveGeoIndex (TRI_index_t* idx, TRI_doc_mptr_t const* doc) {
  GeoCoordinate gc;
  TRI_shaper_t* shaper;
  bool ok;
  double latitude;
  double longitude;
  TRI_geo_index_t* geo;
  int res;

  geo = (TRI_geo_index_t*) idx;
  shaper = geo->base._collection->_shaper;

  // lookup OLD latitude and longitude
  if (geo->_location != 0) {
    ok = ExtractDoubleList(shaper, &doc->_document, geo->_location, &latitude, &longitude);
  }
  else {
    ok = ExtractDoubleArray(shaper, &doc->_document, geo->_latitude, &latitude);
    ok = ok && ExtractDoubleArray(shaper, &doc->_document, geo->_longitude, &longitude);
  }

  // and remove old entry
  if (ok) {
    gc.latitude = latitude;
    gc.longitude = longitude;
    gc.data = doc;

    res = GeoIndex_remove(geo->_geoIndex, &gc);

    if (res != 0) {
      LOG_WARNING("cannot remove old index entry: %d", res);
    }

    return res == 0;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief JSON description of a geo index, location is a list
////////////////////////////////////////////////////////////////////////////////

static TRI_json_t* JsonGeoIndex (TRI_index_t* idx, TRI_doc_collection_t* collection) {
  TRI_json_t* json;
  TRI_shape_path_t const* path;
  char const* location;
  TRI_geo_index_t* geo;

  geo = (TRI_geo_index_t*) idx;

  // convert location to string
  path = collection->_shaper->lookupAttributePathByPid(collection->_shaper, geo->_location);

  if (path == 0) {
    return NULL;
  }

  location = ((char const*) path) + sizeof(TRI_shape_path_t) + (path->_aidLength * sizeof(TRI_shape_aid_t));

  // create json
  json = TRI_CreateArrayJson();

  TRI_Insert2ArrayJson(json, "iid", TRI_CreateNumberJson(idx->_iid));
  TRI_Insert2ArrayJson(json, "type", TRI_CreateStringCopyJson("geo"));
  TRI_Insert2ArrayJson(json, "location", TRI_CreateStringCopyJson(location));

  return json;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief saves the index to file, location is an array
////////////////////////////////////////////////////////////////////////////////

static TRI_json_t* JsonGeoIndex2 (TRI_index_t* idx, TRI_doc_collection_t* collection) {
  TRI_json_t* json;
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

  latitude = ((char const*) path) + sizeof(TRI_shape_path_t) + path->_aidLength * sizeof(TRI_shape_aid_t);

  // convert longitude to string
  path = collection->_shaper->lookupAttributePathByPid(collection->_shaper, geo->_longitude);

  if (path == 0) {
    return NULL;
  }

  longitude = ((char const*) path) + sizeof(TRI_shape_path_t) + path->_aidLength * sizeof(TRI_shape_aid_t);

  // create json
  json = TRI_CreateArrayJson();

  TRI_Insert2ArrayJson(json, "iid", TRI_CreateNumberJson(idx->_iid));
  TRI_Insert2ArrayJson(json, "type", TRI_CreateStringCopyJson("geo"));
  TRI_Insert2ArrayJson(json, "latitude", TRI_CreateStringCopyJson(latitude));
  TRI_Insert2ArrayJson(json, "longitude", TRI_CreateStringCopyJson(longitude));

  return json;
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

TRI_index_t* TRI_CreateGeoIndex (struct TRI_doc_collection_s* collection,
                                 TRI_shape_pid_t location,
                                 bool geoJson) {
  TRI_geo_index_t* geo;

  geo = TRI_Allocate(sizeof(TRI_geo_index_t));

  geo->base._iid = TRI_NewTickVocBase();
  geo->base._type = TRI_IDX_TYPE_GEO_INDEX;
  geo->base._collection = collection;

  geo->base.insert = InsertGeoIndex;
  geo->base.remove = RemoveGeoIndex;
  geo->base.update = UpdateGeoIndex;
  geo->base.json = JsonGeoIndex;

  geo->_geoIndex = GeoIndex_new();

  geo->_location = location;
  geo->_latitude = 0;
  geo->_longitude = 0;
  geo->_geoJson = geoJson;

  return &geo->base;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a geo-index for arrays
////////////////////////////////////////////////////////////////////////////////

TRI_index_t* TRI_CreateGeoIndex2 (struct TRI_doc_collection_s* collection,
                                  TRI_shape_pid_t latitude,
                                  TRI_shape_pid_t longitude) {
  TRI_geo_index_t* geo;

  geo = TRI_Allocate(sizeof(TRI_geo_index_t));

  geo->base._iid = TRI_NewTickVocBase();
  geo->base._type = TRI_IDX_TYPE_GEO_INDEX;
  geo->base._collection = collection;

  geo->base.insert = InsertGeoIndex;
  geo->base.remove = RemoveGeoIndex;
  geo->base.update = UpdateGeoIndex;
  geo->base.json = JsonGeoIndex2;

  geo->_geoIndex = GeoIndex_new();

  geo->_location = 0;
  geo->_latitude = latitude;
  geo->_longitude = longitude;

  return &geo->base;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the memory allocated, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyGeoIndex (TRI_index_t* idx) {
  TRI_geo_index_t* geo;

  geo = (TRI_geo_index_t*) idx;

  GeoIndex_free(geo->_geoIndex);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the memory allocated and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeGeoIndex (TRI_index_t* idx) {
  TRI_DestroyGeoIndex(idx);
  TRI_Free(idx);
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

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
