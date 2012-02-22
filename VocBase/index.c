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

bool TRI_RemoveIndex (TRI_doc_collection_t* collection, TRI_index_t* idx) {
  char* filename;
  char* name;
  char* number;
  bool ok;

  // construct filename
  number = TRI_StringUInt64(idx->_iid);
  if (!number) {
    LOG_ERROR("out of memory when creating index number");
    return false;
  }
  name = TRI_Concatenate3String("index-", number, ".json");
  if (!name) {
    TRI_FreeString(number);
    LOG_ERROR("out of memory when creating index name");
    return false;
  }
  filename = TRI_Concatenate2File(collection->base._directory, name);
  if (!filename) {
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
  if (!number) {
    LOG_ERROR("out of memory when creating index number");
    TRI_FreeJson(json);
    return false;
  }

  name = TRI_Concatenate3String("index-", number, ".json");
  if (!name) {
    LOG_ERROR("out of memory when creating index name");
    TRI_FreeJson(json);
    TRI_FreeString(number);
    return false;
  }

  filename = TRI_Concatenate2File(collection->base._directory, name);
  if (!filename) {
    LOG_ERROR("out of memory when creating index filename");
    TRI_FreeJson(json);
    TRI_FreeString(number);
    TRI_FreeString(name);
    return false;
  }
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
/// @brief gets the definitions of all index files for a collection
////////////////////////////////////////////////////////////////////////////////

TRI_vector_pointer_t TRI_GetCollectionIndexes(const TRI_vocbase_t* vocbase,
                                              const char* collectionName) {
  TRI_vector_pointer_t indexes;
  TRI_index_definition_t* indexDefinition;
  TRI_vector_string_t indexFiles;
  TRI_json_t* json;
  TRI_json_t* strVal;
  TRI_json_t* numVal;
  char* error;
  char* temp1;
  char* temp2;
  char* temp3;
  TRI_idx_iid_t indexId;
  TRI_idx_type_e indexType;
  uint32_t numFields;
  size_t i;
  uint32_t j;
 
  assert(vocbase); 
  TRI_InitVectorPointer(&indexes);

  // add "pseudo" primary index
  indexDefinition = (TRI_index_definition_t*) TRI_Allocate(sizeof(TRI_index_definition_t));
  if (indexDefinition) {
    TRI_InitVectorString(&indexDefinition->_fields);
    indexDefinition->_iid      = 1; // what to do with this id?
    indexDefinition->_type     = TRI_IDX_TYPE_PRIMARY_INDEX;
    indexDefinition->_isUnique = true; 

    TRI_PushBackVectorString(&indexDefinition->_fields, "_id");
    TRI_PushBackVectorPointer(&indexes, indexDefinition);
  }

  // get all index filenames
  indexFiles = TRI_GetCollectionIndexFiles(vocbase, collectionName);
  
  for (i = 0;  i < indexFiles._length;  ++i) {
    // read JSON data from index file
    json = TRI_JsonFile(indexFiles._buffer[i], &error);
    if (!json) {
      continue;
    }
    if (error) {
      goto NEXT_INDEX;
    }

    if (json->_type != TRI_JSON_ARRAY) {
      goto NEXT_INDEX;
    }

    // index file contains a JSON array. fine.

    // read index id
    indexId = 0;
    numVal = TRI_LookupArrayJson(json, "iid");
    if (!numVal || numVal->_type != TRI_JSON_NUMBER) {
      goto NEXT_INDEX;
    }
    indexId = (uint64_t) numVal->_value._number;
    if (indexId == 0) {
      goto NEXT_INDEX;
    }

    // read index type
    strVal = TRI_LookupArrayJson(json, "type");
    if (!strVal || strVal->_type != TRI_JSON_STRING) {
      goto NEXT_INDEX;
    }
    if (strcmp(strVal->_value._string.data, "hash") == 0) {
      indexType = TRI_IDX_TYPE_HASH_INDEX;
    }
    else if (strcmp(strVal->_value._string.data, "geo") == 0) {
      indexType = TRI_IDX_TYPE_GEO_INDEX;
    }
    else {
      // unknown index type
      goto NEXT_INDEX;
    }

    // read number of fields
    numVal = TRI_LookupArrayJson(json, "field_count");
    if (!numVal && numVal->_type != TRI_JSON_NUMBER) {
      goto NEXT_INDEX;
    }
    numFields = (uint32_t) numVal->_value._number;
    if (numFields == 0) {
      goto NEXT_INDEX;
    }

    // create the index definition
    indexDefinition = (TRI_index_definition_t*) TRI_Allocate(sizeof(TRI_index_definition_t));
    if (indexDefinition) {
      TRI_InitVectorString(&indexDefinition->_fields);
      indexDefinition->_iid      = indexId;
      indexDefinition->_type     = indexType;
      indexDefinition->_isUnique = false; // currently hard-coded

      // read field names 
      for (j = 0; j < numFields ; j++) {
        temp1 = TRI_StringUInt32(j);
        if (temp1) {
          temp2 = TRI_Concatenate2String("field_", temp1); 
          if (temp2) {
            strVal = TRI_LookupArrayJson(json, temp2);
            if (strVal && strVal->_type == TRI_JSON_STRING) {
              temp3 = TRI_DuplicateString(strVal->_value._string.data);
              if (temp3) {
                TRI_PushBackVectorString(&indexDefinition->_fields, temp3);
              }
            }
            TRI_FreeString(temp2);
          }
          TRI_FreeString(temp1);
        }
      }

      if (indexDefinition->_fields._length == 0) {
        TRI_Free(indexDefinition);
      } 
      else {
        TRI_PushBackVectorPointer(&indexes, indexDefinition);
      }
    }

NEXT_INDEX:
    TRI_FreeJson(json);
  }
  TRI_DestroyVectorString(&indexFiles);

  return indexes;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief gets the names of all index files for a collection
////////////////////////////////////////////////////////////////////////////////

TRI_vector_string_t TRI_GetCollectionIndexFiles(const TRI_vocbase_t* vocbase, 
                                                const char* collectionName) {
  TRI_vector_string_t files;
  TRI_vector_string_t indexFiles;
  char *path;
  size_t i;
  size_t n;

  assert(vocbase); 
  TRI_InitVectorString(&indexFiles);

  path = TRI_Concatenate2File(vocbase->_path, collectionName);
  if (!path) {
    return indexFiles;
  }

  files = TRI_FilesDirectory(path);
  n = files._length;

  for (i = 0;  i < n;  ++i) {
    char* name;
    char* file;

    name = files._buffer[i];

    if (name[0] == '\0' || strncmp(name, "index-", 6) != 0) {
      continue;
    }

    file = TRI_Concatenate2File(path, name);
    if (!file) {
      continue;
    }

    if (TRI_IsDirectory(file)) {
      continue;
    }

    TRI_PushBackVectorString(&indexFiles, file);
  }

  TRI_DestroyVectorString(&files);
  TRI_FreeString(path);

  return indexFiles;
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
  if (!geo) {
    return NULL;
  }

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
  if (!geo) {
    return NULL;
  }

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
/// @brief attempts to locate an entry in the hash index
////////////////////////////////////////////////////////////////////////////////

HashIndexElements* TRI_LookupHashIndex(TRI_index_t* idx, TRI_json_t* parameterList) {
  TRI_hash_index_t* hashIndex;
  HashIndexElements* result;
  HashIndexElement  element;
  
  element.numFields = parameterList->_value._objects._length;
  element.fields    = TRI_Allocate( sizeof(TRI_json_t) * element.numFields);
  if (element.fields == NULL) {
    LOG_WARNING("out-of-memory in InsertHashIndex");
    return false;
  }  
    
    
  for (size_t j = 0; j < element.numFields; ++j) {
    TRI_json_t* object = (TRI_json_t*) (TRI_AtVector(&(parameterList->_value._objects),j));
    element.fields[j] = *object;
  }
  
  
  hashIndex = (TRI_hash_index_t*) idx;
  result = HashIndex_find(hashIndex->_hashIndex, &element);
  TRI_Free(element.fields);
  
  return result;  
}


////////////////////////////////////////////////////////////////////////////////
/// @brief hash indexes a document
////////////////////////////////////////////////////////////////////////////////

static bool InsertHashIndex (TRI_index_t* idx, TRI_doc_mptr_t const* doc) {
  union { void* p; void const* c; } cnv;
  HashIndexElement* hashElement;
  TRI_shaper_t* shaper;
  TRI_hash_index_t* hashIndex;
  int res;
  TRI_shape_sid_t sid;
  TRI_shape_access_t* acc;
  

  // ............................................................................
  // Obtain the hash index structure
  // ............................................................................
  hashIndex = (TRI_hash_index_t*) idx;
  if (idx == NULL) {
    LOG_WARNING("internal error in InsertHashIndex");
    return false;
  }
  
  // ............................................................................
  // Prepare document so that it can be stored as part of the hash index
  // ............................................................................
  hashElement = (HashIndexElement*)(TRI_Allocate( sizeof(HashIndexElement) ));
  if (hashElement == NULL) {
    LOG_WARNING("out-of-memory in InsertHashIndex");
    return false;
  }  
  cnv.c = doc;
  hashElement->data = cnv.p;


  // ............................................................................
  // Obtain the shaper structure for the collection
  // ............................................................................
  shaper = hashIndex->base._collection->_shaper;
  
  
  // ............................................................................
  // For each attribute obtain the data blob which forms part of the index
  // ............................................................................
  sid = doc->_document._sid;
  hashElement->numFields = hashIndex->_shapeList->_length;
  hashElement->fields    = TRI_Allocate( sizeof(TRI_json_t) * hashElement->numFields);
  
  if (hashElement->fields == NULL) {
    LOG_WARNING("out-of-memory in InsertHashIndex");
    return false;
  }  
    
    
  for (size_t j = 0; j < hashIndex->_shapeList->_length; ++j) {
    TRI_json_t* object;
    TRI_shaped_json_t shapedObject;
    
    TRI_shape_pid_t shape = *((TRI_shape_pid_t*)(TRI_AtVector(hashIndex->_shapeList,j)));
    
    // ..........................................................................
    // Determine if document has that particular shape 
    // ..........................................................................
    acc = TRI_ShapeAccessor(shaper, sid, shape);
    if (acc == NULL || acc->_shape == NULL) {
      TRI_Free(hashElement->fields);
      return false;
    }  
    
    
    // ..........................................................................
    // Extract the field
    // ..........................................................................    
    if (! TRI_ExecuteShapeAccessor(acc, &(doc->_document), &shapedObject)) {
      TRI_Free(hashElement->fields);
      return false;
    }
    // ..........................................................................
    // Store the field
    // ..........................................................................    
    else {
      object = TRI_JsonShapedJson(shaper, &shapedObject);
      hashElement->fields[j] = *object;
      TRI_Free(object);
      object = NULL;
    }

    /* oreste remove debug stuff     
    if (hashElement->fields[j]._type == TRI_JSON_NUMBER) {
      printf("%s:%u:number=%f\n",__FILE__,__LINE__,hashElement->fields[j]._value._number);  
    }
    else if (hashElement->fields[j]._type == TRI_JSON_STRING) {
      printf("%s:%u:string length=%u:%s\n",__FILE__,__LINE__,hashElement->fields[j]._value._string.length,hashElement->fields[j]._value._string.data);  
    }
    else if (false) {
    }
    */
  }


  res = HashIndex_insert(hashIndex->_hashIndex, hashElement);

  if (res == -1) {
    LOG_WARNING("found duplicate entry in hash-index, should not happen");
  }
  else if (res == -2) {
    LOG_WARNING("out-of-memory in hash-index");
  }
  else if (res == -99) {
    LOG_DEBUG("unknown error, ignoring entry");
  }

  return res == 0;
}


////////////////////////////////////////////////////////////////////////////////
/// @brief describes a hash index as a json object
////////////////////////////////////////////////////////////////////////////////

static TRI_json_t* JsonHashIndex (TRI_index_t* idx, TRI_doc_collection_t* collection) {

  TRI_json_t* json;
  const TRI_shape_path_t* path;
  TRI_hash_index_t* hashIndex;
  char const** fieldList;
  char* fieldCounter;
  
  // ..........................................................................  
  // Recast as a hash index
  // ..........................................................................  
  hashIndex = (TRI_hash_index_t*) idx;
  if (hashIndex == NULL) {
    return NULL;
  }

  
  // ..........................................................................  
  // Allocate sufficent memory for the field list
  // ..........................................................................  
  fieldList = TRI_Allocate( (sizeof(char*) * hashIndex->_shapeList->_length) );
  if (fieldList == NULL) {
    return NULL;
  }


  // ..........................................................................  
  // Convert the attributes (field list of the hash index) into strings
  // ..........................................................................  
  for (size_t j = 0; j < hashIndex->_shapeList->_length; ++j) {
    TRI_shape_pid_t shape = *((TRI_shape_pid_t*)(TRI_AtVector(hashIndex->_shapeList,j)));
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

  fieldCounter = TRI_Allocate(30);
  if (!fieldCounter) {
    TRI_Free(fieldList);
    TRI_FreeJson(json);
    return NULL;
  }

  TRI_Insert2ArrayJson(json, "iid", TRI_CreateNumberJson(idx->_iid));
  TRI_Insert2ArrayJson(json, "type", TRI_CreateStringCopyJson("hash"));
  TRI_Insert2ArrayJson(json, "field_count", TRI_CreateNumberJson(hashIndex->_shapeList->_length));
  for (size_t j = 0; j < hashIndex->_shapeList->_length; ++j) {
    sprintf(fieldCounter,"field_%lu",j);
    TRI_Insert2ArrayJson(json, fieldCounter, TRI_CreateStringCopyJson(fieldList[j]));
  }

  TRI_Free(fieldList);
  TRI_Free(fieldCounter);
    
  return json;
}

static bool RemoveHashIndex (TRI_index_t* idx, TRI_doc_mptr_t const* doc) {
  return false;
}

static bool UpdateHashIndex (TRI_index_t* idx, TRI_doc_mptr_t const* doc, TRI_shaped_json_t const* old) {
  return false;
}
  

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a hash index
////////////////////////////////////////////////////////////////////////////////

TRI_index_t* TRI_CreateHashIndex (struct TRI_doc_collection_s* collection,
                                  TRI_vector_t* shapeList) {
  TRI_hash_index_t* hashIndex;
  hashIndex = TRI_Allocate(sizeof(TRI_hash_index_t));
  if (!hashIndex) {
    return NULL;
  }
  
  hashIndex->base._iid = TRI_NewTickVocBase();
  hashIndex->base._type = TRI_IDX_TYPE_HASH_INDEX;
  hashIndex->base._collection = collection;

  hashIndex->base.insert = InsertHashIndex;
  hashIndex->base.json   = JsonHashIndex;
  hashIndex->base.remove = RemoveHashIndex;
  hashIndex->base.update = UpdateHashIndex;
  
  // ...........................................................................
  // Copy the contents of the shape list vector into a new vector and store this
  // ...........................................................................  
  hashIndex->_shapeList = TRI_Allocate(sizeof(TRI_vector_t));
  if (!hashIndex->_shapeList) {
    TRI_Free(hashIndex);
    return NULL;
  }

  TRI_InitVector(hashIndex->_shapeList, sizeof(TRI_shape_pid_t));
  for (size_t j = 0; j < shapeList->_length; ++j) {
    TRI_shape_pid_t shape = *((TRI_shape_pid_t*)(TRI_AtVector(shapeList,j)));
    TRI_PushBackVector(hashIndex->_shapeList,&shape);
  }

  hashIndex->_hashIndex = HashIndex_new();
  
  return &hashIndex->base;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
