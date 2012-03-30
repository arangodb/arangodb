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

#include <BasicsC/conversions.h>
#include <BasicsC/files.h>
#include <BasicsC/json.h>
#include <BasicsC/logging.h>
#include <BasicsC/string-buffer.h>
#include <BasicsC/strings.h>
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

bool TRI_RemoveIndexFile (TRI_doc_collection_t* collection, TRI_index_t* idx) {
  char* filename;
  char* name;
  char* number;
  bool ok;

  // construct filename
  number = TRI_StringUInt64(idx->_iid);

  if (number == NULL) {
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
    LOG_ERROR("out of memory when creating index number");
    return false;
  }

  name = TRI_Concatenate3String("index-", number, ".json");

  if (name == NULL) {
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);

    TRI_FreeString(number);
    LOG_ERROR("out of memory when creating index name");
    return false;
  }

  filename = TRI_Concatenate2File(collection->base._directory, name);

  if (filename == NULL) {
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);

    TRI_FreeString(number);
    TRI_FreeString(name);
    LOG_ERROR("out of memory when creating index filename");
    return false;
  }

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

int TRI_SaveIndex (TRI_doc_collection_t* collection, TRI_index_t* idx) {
  TRI_json_t* json;
  char* filename;
  char* name;
  char* number;
  bool ok;

  // convert into JSON
  json = idx->json(idx, collection);

  if (json == NULL) {
    LOG_TRACE("cannot save index definition: index cannot be jsonfied");
    return TRI_set_errno(TRI_ERROR_INTERNAL);
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
    return TRI_errno();
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up an index identifier
////////////////////////////////////////////////////////////////////////////////

TRI_index_t* TRI_LookupIndex (TRI_doc_collection_t* collection, TRI_idx_iid_t iid) {
  TRI_sim_collection_t* sim;
  TRI_index_t* idx;
  size_t i;

  if (collection->base._type != TRI_COL_TYPE_SIMPLE_DOCUMENT) {
    TRI_set_errno(TRI_ERROR_AVOCADO_UNKNOWN_COLLECTION_TYPE);
    return NULL;
  }

  sim = (TRI_sim_collection_t*) collection;

  for (i = 0;  i < sim->_indexes._length;  ++i) {
    idx = sim->_indexes._buffer[i];

    if (idx->_iid == iid) {
      return idx;
    }
  }

  TRI_set_errno(TRI_ERROR_AVOCADO_NO_INDEX);
  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief gets name of index type
////////////////////////////////////////////////////////////////////////////////

char const* TRI_TypeNameIndex (const TRI_index_t* const idx) {
  switch (idx->_type) {
    case TRI_IDX_TYPE_HASH_INDEX: 
      return "hash";

    case TRI_IDX_TYPE_SKIPLIST_INDEX:
      return "skiplist";

    case TRI_IDX_TYPE_GEO_INDEX:
      return "geo";

    case TRI_IDX_TYPE_PRIMARY_INDEX:
      return "primary";
  }

  return "unknown";
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
/// @brief extracts a double value from an array
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
    if (acc) {
      TRI_FreeShapeAccessor(acc);
    }
    return false;
  }

  if (acc->_shape->_sid != shaper->_sidNumber) {
    TRI_FreeShapeAccessor(acc);
    return false;
  }

  ok = TRI_ExecuteShapeAccessor(acc, document, &json);
  if (ok) {
    *result = * (double*) json._data.data;
    TRI_FreeShapeAccessor(acc);
  }
  return ok;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts a double value from a list
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
    if (acc) {
      TRI_FreeShapeAccessor(acc);
    }
    return false;
  }

  // in-homogenous list
  if (acc->_shape->_type == TRI_SHAPE_LIST) {
    ok = TRI_ExecuteShapeAccessor(acc, document, &list);
    len = TRI_LengthListShapedJson((const TRI_list_shape_t*) acc->_shape, &list);

    if (len < 2) {
      TRI_FreeShapeAccessor(acc);
      return false;
    }

    // latitude
    ok = TRI_AtListShapedJson((const TRI_list_shape_t*) acc->_shape, &list, 0, &entry);

    if (! ok || entry._sid != shaper->_sidNumber) {
      TRI_FreeShapeAccessor(acc);
      return false;
    }

    *latitude = * (double*) entry._data.data;

    // longitude
    ok = TRI_AtListShapedJson((const TRI_list_shape_t*) acc->_shape, &list, 1, &entry);

    if (!ok || entry._sid != shaper->_sidNumber) {
      TRI_FreeShapeAccessor(acc);
      return false;
    }

    *longitude = * (double*) entry._data.data;

    return true;
  }

  // homogenous list
  else if (acc->_shape->_type == TRI_SHAPE_HOMOGENEOUS_LIST) {
    const TRI_homogeneous_list_shape_t* hom;

    hom = (const TRI_homogeneous_list_shape_t*) acc->_shape;

    if (hom->_sidEntry != shaper->_sidNumber) {
      TRI_FreeShapeAccessor(acc);
      return false;
    }

    ok = TRI_ExecuteShapeAccessor(acc, document, &list);

    if (! ok) {
      TRI_FreeShapeAccessor(acc);
      return false;
    }

    len = TRI_LengthHomogeneousListShapedJson((const TRI_homogeneous_list_shape_t*) acc->_shape, &list);

    if (len < 2) {
      TRI_FreeShapeAccessor(acc);
      return false;
    }

    // latitude
    ok = TRI_AtHomogeneousListShapedJson((const TRI_homogeneous_list_shape_t*) acc->_shape, &list, 0, &entry);

    if (! ok) {
      TRI_FreeShapeAccessor(acc);
      return false;
    }

    *latitude = * (double*) entry._data.data;

    // longitude
    ok = TRI_AtHomogeneousListShapedJson((const TRI_homogeneous_list_shape_t*) acc->_shape, &list, 1, &entry);

    if (! ok) {
      TRI_FreeShapeAccessor(acc);
      return false;
    }

    *longitude = * (double*) entry._data.data;

    TRI_FreeShapeAccessor(acc);
    return true;
  }

  // homogeneous list
  else if (acc->_shape->_type == TRI_SHAPE_HOMOGENEOUS_SIZED_LIST) {
    const TRI_homogeneous_sized_list_shape_t* hom;

    hom = (const TRI_homogeneous_sized_list_shape_t*) acc->_shape;

    if (hom->_sidEntry != shaper->_sidNumber) {
      TRI_FreeShapeAccessor(acc);
      return false;
    }

    ok = TRI_ExecuteShapeAccessor(acc, document, &list);

    if (! ok) {
      TRI_FreeShapeAccessor(acc);
      return false;
    }

    len = TRI_LengthHomogeneousSizedListShapedJson((const TRI_homogeneous_sized_list_shape_t*) acc->_shape, &list);

    if (len < 2) {
      TRI_FreeShapeAccessor(acc);
      return false;
    }

    // latitude
    ok = TRI_AtHomogeneousSizedListShapedJson((const TRI_homogeneous_sized_list_shape_t*) acc->_shape, &list, 0, &entry);

    if (! ok) {
      TRI_FreeShapeAccessor(acc);
      return false;
    }

    *latitude = * (double*) entry._data.data;

    // longitude
    ok = TRI_AtHomogeneousSizedListShapedJson((const TRI_homogeneous_sized_list_shape_t*) acc->_shape, &list, 1, &entry);

    if (! ok) {
      TRI_FreeShapeAccessor(acc);
      return false;
    }

    *longitude = * (double*) entry._data.data;

    TRI_FreeShapeAccessor(acc);
    return true;
  }

  // ups
  TRI_FreeShapeAccessor(acc);
  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief inserts a new document, location is a list
////////////////////////////////////////////////////////////////////////////////

static int InsertGeoIndex (TRI_index_t* idx, TRI_doc_mptr_t const* doc) {
  union { void* p; void const* c; } cnv;
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

  cnv.c = doc;
  gc.data = cnv.p;

  res = GeoIndex_insert(geo->_geoIndex, &gc);

  if (res == -1) {
    LOG_WARNING("found duplicate entry in geo-index, should not happen");
    return TRI_set_errno(TRI_ERROR_INTERNAL);
  }
  else if (res == -2) {
    return TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
  }
  else if (res == -3) {
    LOG_DEBUG("illegal geo-coordinates, ignoring entry");
    return TRI_set_errno(TRI_ERROR_AVOCADO_GEO_INDEX_VIOLATED);
  }
  else if (res < 0) {
    return TRI_set_errno(TRI_ERROR_INTERNAL);
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief updates a document, location is a list
////////////////////////////////////////////////////////////////////////////////

static int  UpdateGeoIndex (TRI_index_t* idx, TRI_doc_mptr_t const* doc, TRI_shaped_json_t const* old) {
  union { void* p; void const* c; } cnv;
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

    cnv.c = doc;
    gc.data = cnv.p;

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

  cnv.c = doc;
  gc.data = cnv.p;

  res = GeoIndex_insert(geo->_geoIndex, &gc);

  if (res == -1) {
    LOG_WARNING("found duplicate entry in geo-index, should not happen");
    return TRI_set_errno(TRI_ERROR_INTERNAL);
  }
  else if (res == -2) {
    LOG_WARNING("out-of-memory in geo-index");
    return TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
  }
  else if (res == -3) {
    LOG_DEBUG("illegal geo-coordinates, ignoring entry");
    return TRI_set_errno(TRI_ERROR_AVOCADO_GEO_INDEX_VIOLATED);
  }
  else if (res < 0) {
    return TRI_set_errno(TRI_ERROR_INTERNAL);
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief erases a document, location is a list
////////////////////////////////////////////////////////////////////////////////

static int RemoveGeoIndex (TRI_index_t* idx, TRI_doc_mptr_t const* doc) {
  union { void* p; void const* c; } cnv;
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

    cnv.c = doc;
    gc.data = cnv.p;

    res = GeoIndex_remove(geo->_geoIndex, &gc);

    if (res != 0) {
      LOG_WARNING("cannot remove old index entry: %d", res);
      return TRI_set_errno(TRI_ERROR_INTERNAL);
    }
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief JSON description of a geo index, location is a list
////////////////////////////////////////////////////////////////////////////////

static TRI_json_t* JsonGeoIndex (TRI_index_t* idx, TRI_doc_collection_t* collection) {
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

  location = ((char const*) path) + sizeof(TRI_shape_path_t) + (path->_aidLength * sizeof(TRI_shape_aid_t));

  // create json
  json = TRI_CreateArrayJson();

  if (json == NULL) {
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
    return NULL;
  }

  fields = TRI_CreateListJson();
  TRI_PushBack3ListJson(fields, TRI_CreateStringCopyJson(location));

  TRI_Insert2ArrayJson(json, "id", TRI_CreateNumberJson(idx->_iid));
  TRI_Insert2ArrayJson(json, "type", TRI_CreateStringCopyJson("geo"));
  TRI_Insert2ArrayJson(json, "geoJson", TRI_CreateBooleanJson(geo->_geoJson));
  TRI_Insert2ArrayJson(json, "fields", fields);

  return json;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief saves the index to file, location is an array
////////////////////////////////////////////////////////////////////////////////

static TRI_json_t* JsonGeoIndex2 (TRI_index_t* idx, TRI_doc_collection_t* collection) {
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

  latitude = ((char const*) path) + sizeof(TRI_shape_path_t) + path->_aidLength * sizeof(TRI_shape_aid_t);

  // convert longitude to string
  path = collection->_shaper->lookupAttributePathByPid(collection->_shaper, geo->_longitude);

  if (path == 0) {
    return NULL;
  }

  longitude = ((char const*) path) + sizeof(TRI_shape_path_t) + path->_aidLength * sizeof(TRI_shape_aid_t);

  // create json
  json = TRI_CreateArrayJson();

  if (json == NULL) {
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
    return NULL;
  }

  fields = TRI_CreateListJson();
  TRI_PushBack3ListJson(fields, TRI_CreateStringCopyJson(latitude));
  TRI_PushBack3ListJson(fields, TRI_CreateStringCopyJson(longitude));

  TRI_Insert2ArrayJson(json, "id", TRI_CreateNumberJson(idx->_iid));
  TRI_Insert2ArrayJson(json, "type", TRI_CreateStringCopyJson("geo"));
  TRI_Insert2ArrayJson(json, "fields", fields);

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
                                 char const* locationName,
                                 TRI_shape_pid_t location,
                                 bool geoJson) {
  TRI_geo_index_t* geo;
  char* ln;

  geo = TRI_Allocate(sizeof(TRI_geo_index_t));
  ln = TRI_DuplicateString(locationName);

  if (geo == NULL || ln == NULL) {
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
    return NULL;
  }

  TRI_InitVectorString(&geo->base._fields);

  geo->base._iid = TRI_NewTickVocBase();
  geo->base._type = TRI_IDX_TYPE_GEO_INDEX;
  geo->base._collection = collection;
  geo->base._unique = false;

  geo->base.insert = InsertGeoIndex;
  geo->base.remove = RemoveGeoIndex;
  geo->base.update = UpdateGeoIndex;
  geo->base.json = JsonGeoIndex;

  TRI_PushBackVectorString(&geo->base._fields, ln);

  geo->_geoIndex = GeoIndex_new();

  geo->_variant = geoJson ? INDEX_GEO_COMBINED_LAT_LON : INDEX_GEO_COMBINED_LON_LAT;
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
                                  char const* latitudeName,
                                  TRI_shape_pid_t latitude,
                                  char const* longitudeName,
                                  TRI_shape_pid_t longitude) {
  TRI_geo_index_t* geo;
  char* lan;
  char* lon;

  geo = TRI_Allocate(sizeof(TRI_geo_index_t));
  lan = TRI_DuplicateString(latitudeName);
  lon = TRI_DuplicateString(longitudeName);

  if (geo == NULL || lan == NULL || lon == NULL) {
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
    return NULL;
  }

  TRI_InitVectorString(&geo->base._fields);

  geo->base._iid = TRI_NewTickVocBase();
  geo->base._type = TRI_IDX_TYPE_GEO_INDEX;
  geo->base._collection = collection;
  geo->base._unique = false;

  geo->base.insert = InsertGeoIndex;
  geo->base.remove = RemoveGeoIndex;
  geo->base.update = UpdateGeoIndex;
  geo->base.json = JsonGeoIndex2;

  TRI_PushBackVectorString(&geo->base._fields, lan);
  TRI_PushBackVectorString(&geo->base._fields, lon);

  geo->_geoIndex = GeoIndex_new();

  geo->_variant = INDEX_GEO_INDIVIDUAL_LAT_LON;
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

  TRI_DestroyVectorString(&idx->_fields);

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


// -----------------------------------------------------------------------------
// --SECTION--                                                        HASH INDEX
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief helper for hashing
////////////////////////////////////////////////////////////////////////////////

static int HashIndexHelper (const TRI_hash_index_t* hashIndex, 
                            HashIndexElement* hashElement,
                            const TRI_doc_mptr_t* document,
                            const TRI_shaped_json_t* shapedDoc) {
  union { void* p; void const* c; } cnv;
  TRI_shaped_json_t shapedObject;
  TRI_shape_access_t* acc;
  size_t j;
  
  if (shapedDoc != NULL) {
  
    // ..........................................................................
    // Attempting to locate a hash entry using TRI_shaped_json_t object. Use this
    // when we wish to remove a hash entry and we only have the "keys" rather than
    // having the document (from which the keys would follow).
    // ..........................................................................
    
    hashElement->data = NULL;

    for (j = 0; j < hashIndex->_paths._length; ++j) {
      TRI_shape_pid_t shape = *((TRI_shape_pid_t*)(TRI_AtVector(&hashIndex->_paths,j)));
      
      // ..........................................................................
      // Determine if document has that particular shape 
      // ..........................................................................

      acc = TRI_ShapeAccessor(hashIndex->base._collection->_shaper, shapedDoc->_sid, shape);

      if (acc == NULL || acc->_shape == NULL) {
        if (acc != NULL) {
          TRI_FreeShapeAccessor(acc);
        }

        TRI_Free(hashElement->fields);
        return TRI_set_errno(TRI_ERROR_INTERNAL);
      }  
     
      // ..........................................................................
      // Extract the field
      // ..........................................................................    

      if (! TRI_ExecuteShapeAccessor(acc, shapedDoc, &shapedObject)) {
        TRI_FreeShapeAccessor(acc);
        TRI_Free(hashElement->fields);

        return TRI_set_errno(TRI_ERROR_INTERNAL);
      }
      
      // ..........................................................................
      // Store the json shaped Object -- this is what will be hashed
      // ..........................................................................    

      hashElement->fields[j] = shapedObject;
      TRI_FreeShapeAccessor(acc);
    }  // end of for loop
  }
  
  else if (document != NULL) {
  
    // ..........................................................................
    // Assign the document to the HashIndexElement structure - so that it can later
    // be retreived.
    // ..........................................................................

    cnv.c = document;
    hashElement->data = cnv.p;
 
    for (j = 0; j < hashIndex->_paths._length; ++j) {
      TRI_shape_pid_t shape = *((TRI_shape_pid_t*)(TRI_AtVector(&hashIndex->_paths,j)));
      
      // ..........................................................................
      // Determine if document has that particular shape 
      // ..........................................................................

      acc = TRI_ShapeAccessor(hashIndex->base._collection->_shaper, document->_document._sid, shape);

      if (acc == NULL || acc->_shape == NULL) {
        if (acc != NULL) {
          TRI_FreeShapeAccessor(acc);
        }

        TRI_Free(hashElement->fields);

        return TRI_set_errno(TRI_ERROR_INTERNAL);
      }  
      
      // ..........................................................................
      // Extract the field
      // ..........................................................................    

      if (! TRI_ExecuteShapeAccessor(acc, &(document->_document), &shapedObject)) {
        TRI_FreeShapeAccessor(acc);
        TRI_Free(hashElement->fields);

        return TRI_set_errno(TRI_ERROR_INTERNAL);
      }
      
      // ..........................................................................
      // Store the field
      // ..........................................................................    

      hashElement->fields[j] = shapedObject;

      TRI_FreeShapeAccessor(acc);
    }  // end of for loop
  }
  
  else {
    return TRI_set_errno(TRI_ERROR_INTERNAL);
  }
  
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hash indexes a document
////////////////////////////////////////////////////////////////////////////////

static int InsertHashIndex (TRI_index_t* idx, TRI_doc_mptr_t const* doc) {
  HashIndexElement hashElement;
  TRI_hash_index_t* hashIndex;
  int res;

  // ............................................................................
  // Obtain the hash index structure
  // ............................................................................

  hashIndex = (TRI_hash_index_t*) idx;

  if (idx == NULL) {
    LOG_WARNING("internal error in InsertHashIndex");
    return TRI_set_errno(TRI_ERROR_INTERNAL);
  }

  // ............................................................................
  // Allocate storage to shaped json objects stored as a simple list.
  // These will be used for hashing.
  // ............................................................................
    
  hashElement.numFields = hashIndex->_paths._length;
  hashElement.fields    = TRI_Allocate(sizeof(TRI_shaped_json_t) * hashElement.numFields);

  if (hashElement.fields == NULL) {
    LOG_WARNING("out-of-memory in InsertHashIndex");
    return TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
  }  

  res = HashIndexHelper(hashIndex, &hashElement, doc, NULL);
    
  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }    
  
  // ............................................................................
  // Fill the json field list from the document for unique hash index
  // ............................................................................
  
  if (hashIndex->base._unique) {
    res = HashIndex_insert(hashIndex->_hashIndex, &hashElement);
  }
  
  // ............................................................................
  // Fill the json field list from the document for non-unique hash index
  // ............................................................................
  
  else {
    res = MultiHashIndex_insert(hashIndex->_hashIndex, &hashElement);
  }
  
  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief describes a hash index as a json object
////////////////////////////////////////////////////////////////////////////////

static TRI_json_t* JsonHashIndex (TRI_index_t* idx, TRI_doc_collection_t* collection) {
  TRI_json_t* json;
  TRI_json_t* fields;
  const TRI_shape_path_t* path;
  TRI_hash_index_t* hashIndex;
  char const** fieldList;
  size_t j;
   
  // ..........................................................................  
  // Recast as a hash index
  // ..........................................................................  

  hashIndex = (TRI_hash_index_t*) idx;

  if (hashIndex == NULL) {
    TRI_set_errno(TRI_ERROR_INTERNAL);
    return NULL;
  }
  
  // ..........................................................................  
  // Allocate sufficent memory for the field list
  // ..........................................................................  

  fieldList = TRI_Allocate( (sizeof(char*) * hashIndex->_paths._length) );

  if (fieldList == NULL) {
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
    return NULL;
  }

  // ..........................................................................  
  // Convert the attributes (field list of the hash index) into strings
  // ..........................................................................  

  for (j = 0; j < hashIndex->_paths._length; ++j) {
    TRI_shape_pid_t shape = *((TRI_shape_pid_t*)(TRI_AtVector(&hashIndex->_paths,j)));
    path = collection->_shaper->lookupAttributePathByPid(collection->_shaper, shape);

    if (path == NULL) {
      TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
      TRI_Free(fieldList);
      return NULL;
    }  

    fieldList[j] = ((const char*) path) + sizeof(TRI_shape_path_t) + path->_aidLength * sizeof(TRI_shape_aid_t);
  }

  // ..........................................................................  
  // create json object and fill it
  // ..........................................................................  

  json = TRI_CreateArrayJson();

  if (json == NULL) {
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
    TRI_Free(fieldList);
    return NULL;
  }

  fields = TRI_CreateListJson();

  for (j = 0; j < hashIndex->_paths._length; ++j) {
    TRI_PushBack3ListJson(fields, TRI_CreateStringCopyJson(fieldList[j]));
  }

  TRI_Insert3ArrayJson(json, "id", TRI_CreateNumberJson(idx->_iid));
  TRI_Insert3ArrayJson(json, "unique", TRI_CreateBooleanJson(hashIndex->base._unique));
  TRI_Insert3ArrayJson(json, "type", TRI_CreateStringCopyJson("hash"));
  TRI_Insert3ArrayJson(json, "fields", fields);

  TRI_Free(fieldList);
    
  return json;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes a document from a hash index
////////////////////////////////////////////////////////////////////////////////

static int RemoveHashIndex (TRI_index_t* idx, TRI_doc_mptr_t const* doc) {
  HashIndexElement hashElement;
  TRI_hash_index_t* hashIndex;
  int res;
  
  // ............................................................................
  // Obtain the hash index structure
  // ............................................................................

  hashIndex = (TRI_hash_index_t*) idx;

  if (idx == NULL) {
    LOG_WARNING("internal error in RemoveHashIndex");
    return TRI_set_errno(TRI_ERROR_INTERNAL);
  }

  // ............................................................................
  // Allocate some memory for the HashIndexElement structure
  // ............................................................................

  hashElement.numFields = hashIndex->_paths._length;
  hashElement.fields    = TRI_Allocate( sizeof(TRI_shaped_json_t) * hashElement.numFields);

  if (hashElement.fields == NULL) {
    LOG_WARNING("out-of-memory in InsertHashIndex");
    return TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
  }  
  
  // ..........................................................................
  // Fill the json field list from the document
  // ..........................................................................

  res = HashIndexHelper(hashIndex, &hashElement, doc, NULL);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }    
  
  // ............................................................................
  // Attempt the removal for unique hash indexes
  // ............................................................................
  
  if (hashIndex->base._unique) {
    res = HashIndex_remove(hashIndex->_hashIndex, &hashElement);
  } 
  
  // ............................................................................
  // Attempt the removal for non-unique hash indexes
  // ............................................................................
  
  else {
    res = MultiHashIndex_remove(hashIndex->_hashIndex, &hashElement);
  }
  
  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief updates a document from a hash index
////////////////////////////////////////////////////////////////////////////////

static int UpdateHashIndex (TRI_index_t* idx,
                            const TRI_doc_mptr_t* newDoc, 
                            const TRI_shaped_json_t* oldDoc) {
                             
  // ..........................................................................
  // Note: The oldDoc is represented by the TRI_shaped_json_t rather than by
  //       a TRI_doc_mptr_t object. However for non-unique indexes we must
  //       pass the document shape to the hash remove function.
  // ..........................................................................
  
  union { void* p; void const* c; } cnv;
  HashIndexElement hashElement;
  TRI_hash_index_t* hashIndex;
  int  res;  
  
  // ............................................................................
  // Obtain the hash index structure
  // ............................................................................
  
  hashIndex = (TRI_hash_index_t*) idx;

  if (idx == NULL) {
    LOG_WARNING("internal error in UpdateHashIndex");
    return TRI_ERROR_INTERNAL;
  }

  // ............................................................................
  // Allocate some memory for the HashIndexElement structure
  // ............................................................................

  hashElement.numFields = hashIndex->_paths._length;
  hashElement.fields    = TRI_Allocate(sizeof(TRI_shaped_json_t) * hashElement.numFields);

  if (hashElement.fields == NULL) {
    LOG_WARNING("out-of-memory in UpdateHashIndex");
    return TRI_ERROR_OUT_OF_MEMORY;
  }  
  
  // ............................................................................
  // Update for unique hash index
  // ............................................................................
  
  // ............................................................................
  // Fill in the fields with the values from oldDoc
  // ............................................................................
  
  if (hashIndex->base._unique) {
    if (HashIndexHelper(hashIndex, &hashElement, NULL, oldDoc)) {
    
      // ............................................................................
      // We must fill the hashElement with the value of the document shape -- this
      // is necessary when we attempt to remove non-unique hash indexes.
      // ............................................................................

      cnv.c = newDoc; // we are assuming here that the doc ptr does not change
      hashElement.data = cnv.p;
      
      // ............................................................................
      // Remove the hash index entry and return.
      // ............................................................................

      res = HashIndex_remove(hashIndex->_hashIndex, &hashElement); 

      if (res != TRI_ERROR_NO_ERROR) {
        LOG_WARNING("could not remove old document from hash index in UpdateHashIndex");
      }
    }    

    // ............................................................................
    // Fill the json simple list from the document
    // ............................................................................

    res = HashIndexHelper(hashIndex, &hashElement, newDoc, NULL);

    if (res != TRI_ERROR_NO_ERROR) {

      // ..........................................................................
      // probably fields do not match  
      // ..........................................................................

      return res;
    }    

    // ............................................................................
    // Attempt to add the hash entry from the new doc
    // ............................................................................

    res = HashIndex_insert(hashIndex->_hashIndex, &hashElement);
  }

  // ............................................................................
  // Update for non-unique hash index
  // ............................................................................

  else {
  
    // ............................................................................
    // Fill in the fields with the values from oldDoc
    // ............................................................................    
    
    res = HashIndexHelper(hashIndex, &hashElement, NULL, oldDoc);

    if (res != TRI_ERROR_NO_ERROR) {

      // ............................................................................
      // We must fill the hashElement with the value of the document shape -- this
      // is necessary when we attempt to remove non-unique hash indexes.
      // ............................................................................

      cnv.c = newDoc;
      hashElement.data = cnv.p;
      
      // ............................................................................
      // Remove the hash index entry and return.
      // ............................................................................

      res = MultiHashIndex_remove(hashIndex->_hashIndex, &hashElement);

      if (res != TRI_ERROR_NO_ERROR) {
        LOG_WARNING("could not remove old document from hash index in UpdateHashIndex");
      }
    }    

    // ............................................................................
    // Fill the shaped json simple list from the document
    // ............................................................................

    res = HashIndexHelper(hashIndex, &hashElement, newDoc, NULL); 

    if (res != TRI_ERROR_NO_ERROR) {

      // ..........................................................................
      // probably fields do not match  
      // ..........................................................................

      return res;
    }    

    // ............................................................................
    // Attempt to add the hash entry from the new doc
    // ............................................................................

    res = MultiHashIndex_insert(hashIndex->_hashIndex, &hashElement);
  }
  
  return res;
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
/// @brief creates a hash index
////////////////////////////////////////////////////////////////////////////////

TRI_index_t* TRI_CreateHashIndex (struct TRI_doc_collection_s* collection,
                                  TRI_vector_pointer_t* fields,
                                  TRI_vector_t* paths,
                                  bool unique) {
  TRI_hash_index_t* hashIndex;
  size_t j;

  hashIndex = TRI_Allocate(sizeof(TRI_hash_index_t));

  if (hashIndex == NULL) {
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
    return NULL;
  }
  
  hashIndex->base._iid = TRI_NewTickVocBase();
  hashIndex->base._type = TRI_IDX_TYPE_HASH_INDEX;
  hashIndex->base._collection = collection;
  hashIndex->base._unique = unique;

  hashIndex->base.insert = InsertHashIndex;
  hashIndex->base.json   = JsonHashIndex;
  hashIndex->base.remove = RemoveHashIndex;
  hashIndex->base.update = UpdateHashIndex;

  // ...........................................................................
  // Copy the contents of the path list vector into a new vector and store this
  // ...........................................................................  

  TRI_InitVector(&hashIndex->_paths, sizeof(TRI_shape_pid_t));

  for (j = 0;  j < paths->_length;  ++j) {
    TRI_shape_pid_t shape = *((TRI_shape_pid_t*)(TRI_AtVector(paths,j)));

    TRI_PushBackVector(&hashIndex->_paths, &shape);
  }
  
  TRI_InitVectorString(&hashIndex->base._fields);

  for (j = 0;  j < fields->_length;  ++j) {
    char const* name = fields->_buffer[j];

    TRI_PushBackVectorString(&hashIndex->base._fields, TRI_DuplicateString(name));
  }

  if (unique) {
    hashIndex->_hashIndex = HashIndex_new();
  }
  else {
    hashIndex->_hashIndex = MultiHashIndex_new();
  }  
  
  return &hashIndex->base;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the memory allocated, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyHashIndex (TRI_index_t* idx) {
  TRI_hash_index_t* hash;

  TRI_DestroyVectorString(&idx->_fields);

  hash = (TRI_hash_index_t*) idx;

  TRI_DestroyVector(&hash->_paths);

  LOG_ERROR("TRI_DestroyHashIndex not implemented TODO oreste");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the memory allocated and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeHashIndex (TRI_index_t* idx) {
  TRI_DestroyHashIndex(idx);
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
/// @brief attempts to locate an entry in the hash index
///
/// @warning who ever calls this function is responsible for destroying
/// HashIndexElements* results
////////////////////////////////////////////////////////////////////////////////

HashIndexElements* TRI_LookupHashIndex(TRI_index_t* idx, TRI_json_t* parameterList) {
  TRI_hash_index_t* hashIndex;
  HashIndexElements* result;
  HashIndexElement  element;
  TRI_shaper_t*     shaper;
  size_t j;
  
  element.numFields = parameterList->_value._objects._length;
  element.fields    = TRI_Allocate( sizeof(TRI_json_t) * element.numFields);

  if (element.fields == NULL) {
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
    LOG_WARNING("out-of-memory in LookupHashIndex");
    return NULL;
  }  
    
  hashIndex = (TRI_hash_index_t*) idx;
  shaper = hashIndex->base._collection->_shaper;
    
  for (j = 0; j < element.numFields; ++j) {
    TRI_json_t*        jsonObject   = (TRI_json_t*) (TRI_AtVector(&(parameterList->_value._objects),j));
    TRI_shaped_json_t* shapedObject = TRI_ShapedJsonJson(shaper, jsonObject);

    element.fields[j] = *shapedObject;
    TRI_Free(shapedObject);
  }
  
  if (hashIndex->base._unique) {
    result = HashIndex_find(hashIndex->_hashIndex, &element);
  }
  else {
    result = MultiHashIndex_find(hashIndex->_hashIndex, &element);
  }

  for (j = 0; j < element.numFields; ++j) {
    TRI_DestroyShapedJson(element.fields + j);
  }

  TRI_Free(element.fields);
  
  return result;  
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    SKIPLIST INDEX
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief attempts to locate an entry in the skip list index
////////////////////////////////////////////////////////////////////////////////

// .............................................................................
// Warning: who ever calls this function is responsible for destroying
// TRI_skiplist_iterator_t* results
// .............................................................................

static void FillLookupSLOperator(TRI_sl_operator_t* slOperator, TRI_doc_collection_t* collection) {
  TRI_json_t*                 jsonObject;
  TRI_shaped_json_t*          shapedObject;
  TRI_sl_relation_operator_t* relationOperator;
  TRI_sl_logical_operator_t*  logicalOperator;
  size_t j;
  
  if (slOperator == NULL) {
    return;
  }
  
  switch (slOperator->_type) {
    case TRI_SL_AND_OPERATOR: 
    case TRI_SL_NOT_OPERATOR:
    case TRI_SL_OR_OPERATOR: 
    {
      logicalOperator = (TRI_sl_logical_operator_t*)(slOperator);
      FillLookupSLOperator(logicalOperator->_left,collection);
      FillLookupSLOperator(logicalOperator->_right,collection);
      break;
    }
    
    case TRI_SL_EQ_OPERATOR: 
    case TRI_SL_GE_OPERATOR: 
    case TRI_SL_GT_OPERATOR: 
    case TRI_SL_NE_OPERATOR: 
    case TRI_SL_LE_OPERATOR: 
    case TRI_SL_LT_OPERATOR: 
    {
      relationOperator = (TRI_sl_relation_operator_t*)(slOperator);
      relationOperator->_numFields  = relationOperator->_parameters->_value._objects._length;
      relationOperator->_fields     = TRI_Allocate( sizeof(TRI_shaped_json_t) * relationOperator->_numFields);
      relationOperator->_collection = collection;
      
      for (j = 0; j < relationOperator->_numFields; ++j) {
        jsonObject   = (TRI_json_t*) (TRI_AtVector(&(relationOperator->_parameters->_value._objects),j));
        shapedObject = TRI_ShapedJsonJson(collection->_shaper, jsonObject);
        relationOperator->_fields[j] = *shapedObject; // shallow copy here is ok
        TRI_Free(shapedObject); // don't require storage anymore
      }
      break;
    }
  }
}

TRI_skiplist_iterator_t* TRI_LookupSkiplistIndex(TRI_index_t* idx, TRI_sl_operator_t* slOperator) {
  TRI_skiplist_index_t*    skiplistIndex;
  TRI_skiplist_iterator_t* result;
  
  skiplistIndex = (TRI_skiplist_index_t*)(idx);
  // .........................................................................
  // fill the relation operators which may be embedded in the slOperator with
  // additional information. Recall the slOperator is what information was
  // received from a user for query the skiplist.
  // .........................................................................
  
  FillLookupSLOperator(slOperator, skiplistIndex->base._collection); 
  
  if (skiplistIndex->base._unique) {
    result = SkiplistIndex_find(skiplistIndex->_skiplistIndex, &skiplistIndex->_paths, slOperator);
  }
  else {
    result = MultiSkiplistIndex_find(skiplistIndex->_skiplistIndex, &skiplistIndex->_paths, slOperator);
  }

  return result;  
}


////////////////////////////////////////////////////////////////////////////////
/// @brief helper for skiplist methods
////////////////////////////////////////////////////////////////////////////////

static bool SkiplistIndexHelper(const TRI_skiplist_index_t* skiplistIndex, 
                                SkiplistIndexElement* skiplistElement,
                                const TRI_doc_mptr_t* document,
                                const TRI_shaped_json_t* shapedDoc) {
  union { void* p; void const* c; } cnv;
  TRI_shaped_json_t shapedObject;
  TRI_shape_access_t* acc;
  size_t j;
  
  if (shapedDoc != NULL) {
  
    // ..........................................................................
    // Attempting to locate a entry using TRI_shaped_json_t object. Use this
    // when we wish to remove a entry and we only have the "keys" rather than
    // having the document (from which the keys would follow).
    // ..........................................................................
    
    skiplistElement->data = NULL;
    
 
    for (j = 0; j < skiplistIndex->_paths._length; ++j) {
      TRI_shape_pid_t shape = *((TRI_shape_pid_t*)(TRI_AtVector(&skiplistIndex->_paths,j)));
      
      // ..........................................................................
      // Determine if document has that particular shape 
      // ..........................................................................
      acc = TRI_ShapeAccessor(skiplistIndex->base._collection->_shaper, shapedDoc->_sid, shape);
      if (acc == NULL || acc->_shape == NULL) {
        if (acc != NULL) {
          TRI_FreeShapeAccessor(acc);
        }
        TRI_Free(skiplistElement->fields);
        return false;
      }  
      
      
      // ..........................................................................
      // Extract the field
      // ..........................................................................    
      if (! TRI_ExecuteShapeAccessor(acc, shapedDoc, &shapedObject)) {
        TRI_FreeShapeAccessor(acc);
        TRI_Free(skiplistElement->fields);
        return false;
      }
      
      
      // ..........................................................................
      // Store the json shaped Object -- this is what will be hashed
      // ..........................................................................    
      skiplistElement->fields[j] = shapedObject;
      TRI_FreeShapeAccessor(acc);
    }  // end of for loop
      
  }
  
  
  
  else if (document != NULL) {
  
    // ..........................................................................
    // Assign the document to the SkiplistIndexElement structure so that it can 
    // be retreived later.
    // ..........................................................................
    cnv.c = document;
    skiplistElement->data = cnv.p;
 
    
    for (j = 0; j < skiplistIndex->_paths._length; ++j) {
      TRI_shape_pid_t shape = *((TRI_shape_pid_t*)(TRI_AtVector(&skiplistIndex->_paths,j)));
      
      // ..........................................................................
      // Determine if document has that particular shape 
      // ..........................................................................
      acc = TRI_ShapeAccessor(skiplistIndex->base._collection->_shaper, document->_document._sid, shape);
      if (acc == NULL || acc->_shape == NULL) {
        if (acc != NULL) {
          TRI_FreeShapeAccessor(acc);
        }
        TRI_Free(skiplistElement->fields);
        return false;
      }  
      
      
      // ..........................................................................
      // Extract the field
      // ..........................................................................    
      if (! TRI_ExecuteShapeAccessor(acc, &(document->_document), &shapedObject)) {
        TRI_FreeShapeAccessor(acc);
        TRI_Free(skiplistElement->fields);
        return false;
      }
      

      // ..........................................................................
      // Store the field
      // ..........................................................................    
      skiplistElement->fields[j] = shapedObject;
      TRI_FreeShapeAccessor(acc);
      
    }  // end of for loop
  }
  
  else {
    return false;
  }
  
  return true;
  
} // end of static function SkiplistIndexHelper



////////////////////////////////////////////////////////////////////////////////
/// @brief inserts a document into a skip list index
////////////////////////////////////////////////////////////////////////////////

static int InsertSkiplistIndex (TRI_index_t* idx, TRI_doc_mptr_t const* doc) {

  SkiplistIndexElement skiplistElement;
  TRI_skiplist_index_t* skiplistIndex;
  int res;
  bool ok;

  
  // ............................................................................
  // Obtain the skip listindex structure
  // ............................................................................
  
  skiplistIndex = (TRI_skiplist_index_t*) idx;
  if (idx == NULL) {
    LOG_WARNING("internal error in InsertSkiplistIndex");
    return TRI_ERROR_INTERNAL;
  }
  

  // ............................................................................
  // Allocate storage to shaped json objects stored as a simple list.
  // These will be used for comparisions
  // ............................................................................
    
  skiplistElement.numFields   = skiplistIndex->_paths._length;
  skiplistElement.fields      = TRI_Allocate( sizeof(TRI_shaped_json_t) * skiplistElement.numFields);
  skiplistElement.collection  = skiplistIndex->base._collection;
  
  if (skiplistElement.fields == NULL) {
    LOG_WARNING("out-of-memory in InsertSkiplistIndex");
    return TRI_ERROR_OUT_OF_MEMORY;
  }  
  ok = SkiplistIndexHelper(skiplistIndex, &skiplistElement, doc, NULL);
    
  
  // ............................................................................
  // most likely the cause of this error is that the 'shape' of the document
  // does not match the 'shape' of the index structure -- so the document
  // is ignored. So not really an error at all.
  // ............................................................................
  
  if (!ok) {
    return TRI_ERROR_NO_ERROR;
  }    
  

  // ............................................................................
  // Fill the json field list from the document for unique skiplist index
  // ............................................................................
  
  if (skiplistIndex->base._unique) {
    res = SkiplistIndex_insert(skiplistIndex->_skiplistIndex, &skiplistElement);
  }
  
  // ............................................................................
  // Fill the json field list from the document for non-unique skiplist index
  // ............................................................................
  
  else {
    res = MultiSkiplistIndex_insert(skiplistIndex->_skiplistIndex, &skiplistElement);
  }

  
  // ............................................................................
  // Take the appropriate action which depends on what was returned by the insert
  // method of the index.
  // ............................................................................
  
  if (res == -1) {
    LOG_WARNING("found duplicate entry in skiplist-index, should not happen");
    return TRI_ERROR_INTERNAL;
  }
  else if (res == -2) {
    LOG_WARNING("out-of-memory in skiplist-index");
    return TRI_ERROR_OUT_OF_MEMORY;
  }
  else if (res < 0) {
    LOG_DEBUG("unknown error, ignoring entry");
    return TRI_ERROR_INTERNAL;
  }

  
  // ............................................................................
  // Everything OK
  // ............................................................................
  
  return TRI_ERROR_NO_ERROR;
}


////////////////////////////////////////////////////////////////////////////////
/// @brief describes a skiplist  index as a json object
////////////////////////////////////////////////////////////////////////////////

static TRI_json_t* JsonSkiplistIndex (TRI_index_t* idx, TRI_doc_collection_t* collection) {
  TRI_json_t* json;
  TRI_json_t* fields;
  const TRI_shape_path_t* path;
  TRI_skiplist_index_t* skiplistIndex;
  char const** fieldList;
  size_t j;
   
  // ..........................................................................  
  // Recast as a skiplist index
  // ..........................................................................  
  skiplistIndex = (TRI_skiplist_index_t*) idx;
  if (skiplistIndex == NULL) {
    return NULL;
  }

  
  // ..........................................................................  
  // Allocate sufficent memory for the field list
  // ..........................................................................  
  fieldList = TRI_Allocate( (sizeof(char*) * skiplistIndex->_paths._length) );
  if (fieldList == NULL) {
    return NULL;
  }


  // ..........................................................................  
  // Convert the attributes (field list of the skiplist index) into strings
  // ..........................................................................  
  for (j = 0; j < skiplistIndex->_paths._length; ++j) {
    TRI_shape_pid_t shape = *((TRI_shape_pid_t*)(TRI_AtVector(&skiplistIndex->_paths,j)));
    path = collection->_shaper->lookupAttributePathByPid(collection->_shaper, shape);
    if (path == NULL) {
      TRI_Free(fieldList);
      return NULL;
    }  
    fieldList[j] = ((const char*) path) + sizeof(TRI_shape_path_t) + path->_aidLength * sizeof(TRI_shape_aid_t);
  }
  

  // ..........................................................................  
  // create json object and fill it
  // ..........................................................................  
  json = TRI_CreateArrayJson();
  if (!json) {
    TRI_Free(fieldList);
    return NULL;
  }

  fields = TRI_CreateListJson();

  for (j = 0; j < skiplistIndex->_paths._length; ++j) {
    TRI_PushBack3ListJson(fields, TRI_CreateStringCopyJson(fieldList[j]));
  }

  TRI_Insert3ArrayJson(json, "id", TRI_CreateNumberJson(idx->_iid));
  TRI_Insert3ArrayJson(json, "unique", TRI_CreateBooleanJson(skiplistIndex->base._unique));
  TRI_Insert3ArrayJson(json, "type", TRI_CreateStringCopyJson("skiplist"));
  TRI_Insert3ArrayJson(json, "fields", fields);

  TRI_Free(fieldList);
    
  return json;
}


////////////////////////////////////////////////////////////////////////////////
/// @brief removes a document from a skiplist index
////////////////////////////////////////////////////////////////////////////////

static int RemoveSkiplistIndex (TRI_index_t* idx, TRI_doc_mptr_t const* doc) {

  SkiplistIndexElement skiplistElement;
  TRI_skiplist_index_t* skiplistIndex;
  bool result;
  

  // ............................................................................
  // Obtain the skiplist index structure
  // ............................................................................
  skiplistIndex = (TRI_skiplist_index_t*) idx;
  if (idx == NULL) {
    LOG_WARNING("internal error in RemoveHashIndex");
    return TRI_ERROR_INTERNAL;
  }


  // ............................................................................
  // Allocate some memory for the SkiplistIndexElement structure
  // ............................................................................

  skiplistElement.numFields  = skiplistIndex->_paths._length;
  skiplistElement.fields     = TRI_Allocate( sizeof(TRI_shaped_json_t) * skiplistElement.numFields);
  skiplistElement.collection = skiplistIndex->base._collection;
  
  if (skiplistElement.fields == NULL) {
    LOG_WARNING("out-of-memory in InsertSkiplistIndex");
    return TRI_ERROR_OUT_OF_MEMORY;
  }  
  
  // ..........................................................................
  // Fill the json field list from the document
  // ..........................................................................
  if (!SkiplistIndexHelper(skiplistIndex, &skiplistElement, doc, NULL)) {
    return TRI_ERROR_INTERNAL;
  }    

  
  // ............................................................................
  // Attempt the removal for unique skiplist indexes
  // ............................................................................
  
  if (skiplistIndex->base._unique) {
    result = SkiplistIndex_remove(skiplistIndex->_skiplistIndex, &skiplistElement);
  } 
  
  // ............................................................................
  // Attempt the removal for non-unique skiplist indexes
  // ............................................................................
  
  else {
    result = MultiSkiplistIndex_remove(skiplistIndex->_skiplistIndex, &skiplistElement);
  }
  
  
  return result ? TRI_ERROR_NO_ERROR : TRI_ERROR_INTERNAL;
}


static int UpdateSkiplistIndex (TRI_index_t* idx, const TRI_doc_mptr_t* newDoc, 
                                 const TRI_shaped_json_t* oldDoc) {
                             
  // ..........................................................................
  // Note: The oldDoc is represented by the TRI_shaped_json_t rather than by
  //       a TRI_doc_mptr_t object. However for non-unique indexes we must
  //       pass the document shape to the hash remove function.
  // ..........................................................................
  
  union { void* p; void const* c; } cnv;
  SkiplistIndexElement skiplistElement;
  TRI_skiplist_index_t* skiplistIndex;
  int  res;  

  
  // ............................................................................
  // Obtain the skiplist index structure
  // ............................................................................
  
  skiplistIndex = (TRI_skiplist_index_t*) idx;
  if (idx == NULL) {
    LOG_WARNING("internal error in UpdateSkiplistIndex");
    return TRI_ERROR_INTERNAL;
  }


  // ............................................................................
  // Allocate some memory for the SkiplistIndexElement structure
  // ............................................................................

  skiplistElement.numFields  = skiplistIndex->_paths._length;
  skiplistElement.fields     = TRI_Allocate( sizeof(TRI_shaped_json_t) * skiplistElement.numFields);
  skiplistElement.collection = skiplistIndex->base._collection;
  
  if (skiplistElement.fields == NULL) {
    LOG_WARNING("out-of-memory in UpdateHashIndex");
    return TRI_ERROR_OUT_OF_MEMORY;
  }  
      
  
  // ............................................................................
  // Update for unique skiplist index
  // ............................................................................
  
  
  // ............................................................................
  // Fill in the fields with the values from oldDoc
  // ............................................................................
  
  if (skiplistIndex->base._unique) {
  
      
    if (SkiplistIndexHelper(skiplistIndex, &skiplistElement, NULL, oldDoc)) {
    
      // ............................................................................
      // We must fill the skiplistElement with the value of the document shape -- this
      // is necessary when we attempt to remove non-unique skiplist indexes.
      // ............................................................................
      cnv.c = newDoc; // we are assuming here that the doc ptr does not change
      skiplistElement.data = cnv.p;
      
      
      // ............................................................................
      // Remove the skiplist index entry and return.
      // ............................................................................
      if (!SkiplistIndex_remove(skiplistIndex->_skiplistIndex, &skiplistElement)) {
        LOG_WARNING("could not remove old document from skiplist index in UpdateSkiplistIndex");
      }
      
    }    


    // ............................................................................
    // Fill the json simple list from the document
    // ............................................................................
    if (!SkiplistIndexHelper(skiplistIndex, &skiplistElement, newDoc, NULL)) {
      // ..........................................................................
      // probably fields do not match  
      // ..........................................................................
      return TRI_ERROR_INTERNAL;
    }    


    // ............................................................................
    // Attempt to add the skiplist entry from the new doc
    // ............................................................................
    res = SkiplistIndex_insert(skiplistIndex->_skiplistIndex, &skiplistElement);
  }

  
  // ............................................................................
  // Update for non-unique skiplist index
  // ............................................................................

  else {
  
    // ............................................................................
    // Fill in the fields with the values from oldDoc
    // ............................................................................    
    
    if (SkiplistIndexHelper(skiplistIndex, &skiplistElement, NULL, oldDoc)) {
      // ............................................................................
      // We must fill the skiplistElement with the value of the document shape -- this
      // is necessary when we attempt to remove non-unique skiplist indexes.
      // ............................................................................
      cnv.c = newDoc;
      skiplistElement.data = cnv.p;
      
      // ............................................................................
      // Remove the skiplist index entry and return.
      // ............................................................................
      
      if (!MultiSkiplistIndex_remove(skiplistIndex->_skiplistIndex, &skiplistElement)) {
        LOG_WARNING("could not remove old document from hash index in UpdateHashIndex");
      }
      
    }    


    // ............................................................................
    // Fill the shaped json simple list from the document
    // ............................................................................
    if (!SkiplistIndexHelper(skiplistIndex, &skiplistElement, newDoc, NULL)) {
      // ..........................................................................
      // probably fields do not match  
      // ..........................................................................
      return TRI_ERROR_INTERNAL;
    }    


    // ............................................................................
    // Attempt to add the skiplist entry from the new doc
    // ............................................................................
    res = MultiSkiplistIndex_insert(skiplistIndex->_skiplistIndex, &skiplistElement);
    
  }
  
  if (res == -1) {
    LOG_WARNING("found duplicate entry in skiplist-index, should not happen");
    return TRI_ERROR_INTERNAL;
  }
  else if (res == -2) {
    LOG_WARNING("out-of-memory in skiplist-index");
    return TRI_ERROR_OUT_OF_MEMORY;
  }
  else if (res < 0) {
    LOG_DEBUG("unknown error, ignoring entry");
    return TRI_ERROR_INTERNAL;
  }

  return TRI_ERROR_NO_ERROR;
}
  

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a skiplist index
////////////////////////////////////////////////////////////////////////////////

TRI_index_t* TRI_CreateSkiplistIndex (struct TRI_doc_collection_s* collection,
                                      TRI_vector_pointer_t* fields,
                                      TRI_vector_t* paths,
                                      bool unique) {
  TRI_skiplist_index_t* skiplistIndex;
  size_t j;

  skiplistIndex = TRI_Allocate(sizeof(TRI_skiplist_index_t));
  if (!skiplistIndex) {
    return NULL;
  }
  
  skiplistIndex->base._iid = TRI_NewTickVocBase();
  skiplistIndex->base._type = TRI_IDX_TYPE_SKIPLIST_INDEX;
  skiplistIndex->base._collection = collection;
  skiplistIndex->base._unique = unique;

  skiplistIndex->base.insert = InsertSkiplistIndex;
  skiplistIndex->base.json   = JsonSkiplistIndex;
  skiplistIndex->base.remove = RemoveSkiplistIndex;
  skiplistIndex->base.update = UpdateSkiplistIndex;
  
  // ...........................................................................
  // Copy the contents of the shape list vector into a new vector and store this
  // ...........................................................................  

  TRI_InitVector(&skiplistIndex->_paths, sizeof(TRI_shape_pid_t));

  for (j = 0;  j < paths->_length;  ++j) {
    TRI_shape_pid_t shape = *((TRI_shape_pid_t*)(TRI_AtVector(paths,j)));

    TRI_PushBackVector(&skiplistIndex->_paths, &shape);
  }

  TRI_InitVectorString(&skiplistIndex->base._fields);

  for (j = 0;  j < fields->_length;  ++j) {
    char const* name = fields->_buffer[j];
    TRI_PushBackVectorString(&skiplistIndex->base._fields, TRI_DuplicateString(name));
  }
  
  if (unique) {
    skiplistIndex->_skiplistIndex = SkiplistIndex_new();
  }
  else {
    skiplistIndex->_skiplistIndex = MultiSkiplistIndex_new();
  }  
  
  return &skiplistIndex->base;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the memory allocated, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroySkiplistIndex (TRI_index_t* idx) {
  TRI_skiplist_index_t* sl;

  if (idx == NULL) {
    return;
  }
  
  TRI_DestroyVectorString(&idx->_fields);

  sl = (TRI_skiplist_index_t*) idx;
  TRI_DestroyVector(&sl->_paths);

  SkiplistIndexFree(sl->_skiplistIndex);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the memory allocated and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeSkiplistIndex (TRI_index_t* idx) {
  if (idx == NULL) {
    return;
  }  
  TRI_DestroySkiplistIndex(idx);
  TRI_Free(idx);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
