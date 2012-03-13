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
/// @brief looks up an index identifier
////////////////////////////////////////////////////////////////////////////////

TRI_index_t* TRI_LookupIndex (TRI_doc_collection_t* collection, TRI_idx_iid_t iid) {
  TRI_sim_collection_t* sim;
  TRI_index_t* idx;
  size_t i;

  if (collection->base._type != TRI_COL_TYPE_SIMPLE_DOCUMENT) {
    return NULL;
  }

  sim = (TRI_sim_collection_t*) collection;

  for (i = 0;  i < sim->_indexes._length;  ++i) {
    idx = sim->_indexes._buffer[i];

    if (idx->_iid == iid) {
      return idx;
    }
  }

  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free an existing index definition
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeIndexDefinition (TRI_index_definition_t* definition) {
  assert(definition->_fields);
  TRI_FreeVectorString(definition->_fields);
  TRI_Free(definition);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free an existing index definitions vector
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeIndexDefinitions (TRI_vector_pointer_t* definitions) {
  TRI_index_definition_t* definition;
  size_t i;

  if (!definitions) {
    return;
  }

  for (i = 0; i < definitions->_length; i++) {
    definition = (TRI_index_definition_t*) definitions->_buffer[i];
    assert(definition);
    TRI_FreeIndexDefinition(definition);
  }

  TRI_FreeVectorPointer(definitions);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief read the fields of an index from a json structure and return them
////////////////////////////////////////////////////////////////////////////////

static TRI_vector_string_t* GetFieldsIndex (const TRI_idx_type_e indexType,
                                            TRI_json_t* json,
                                            TRI_index_geo_variant_e* geoVariant) {
  TRI_vector_string_t* fields;
  TRI_json_t* strVal;
  TRI_json_t* strVal2;
  char* temp1;
  char* temp2;
  char* temp3;
  uint32_t numFields;
  size_t i;

  *geoVariant = INDEX_GEO_NONE;
  fields = (TRI_vector_string_t*) TRI_Allocate(sizeof(TRI_vector_string_t));
  if (!fields) {
    return NULL;
  }

  TRI_InitVectorString(fields);
  if (indexType == TRI_IDX_TYPE_GEO_INDEX) {
    strVal = TRI_LookupArrayJson(json, "location");
    if (!strVal || strVal->_type != TRI_JSON_STRING) {
      strVal = TRI_LookupArrayJson(json, "latitude");
      if (!strVal || strVal->_type != TRI_JSON_STRING) {
        return fields;
      }
      strVal2 = TRI_LookupArrayJson(json, "longitude");
      if (!strVal2 || strVal2->_type != TRI_JSON_STRING) {
        return fields;
      }
      temp1 = TRI_DuplicateString(strVal->_value._string.data);
      if (!temp1) {
        return fields;
      }
      TRI_PushBackVectorString(fields, temp1);

      temp1 = TRI_DuplicateString(strVal2->_value._string.data);
      if (!temp1) {
        return fields;
      }
      TRI_PushBackVectorString(fields, temp1);
      *geoVariant = INDEX_GEO_INDIVIDUAL_LAT_LON;
    }
    else {
      *geoVariant = INDEX_GEO_COMBINED_LON_LAT;

      strVal2 = TRI_LookupArrayJson(json, "geoJson");
      if (strVal2 && strVal2->_type == TRI_JSON_BOOLEAN) {
        if (strVal2->_value._boolean) {
          *geoVariant = INDEX_GEO_COMBINED_LAT_LON;
        }
      }
      TRI_PushBackVectorString(fields, strVal->_value._string.data);
    }
  }
  else {
    // read number of fields
    strVal = TRI_LookupArrayJson(json, "fieldCount");
    if (!strVal || strVal->_type != TRI_JSON_NUMBER) {
      return fields;
    }
    
    numFields = (uint32_t) strVal->_value._number;
    if (numFields == 0) {
      return fields;
    }

    // read field names 
    for (i = 0; i < numFields ; i++) {
      temp1 = TRI_StringUInt32(i);
      if (temp1) {
        temp2 = TRI_Concatenate2String("field_", temp1); 
        if (temp2) {
          strVal = TRI_LookupArrayJson(json, temp2);
          if (strVal && strVal->_type == TRI_JSON_STRING) {
            temp3 = TRI_DuplicateString(strVal->_value._string.data);
            if (temp3) {
              TRI_PushBackVectorString(fields, temp3);
            }
          }
          TRI_FreeString(temp2);
        }
        TRI_FreeString(temp1);
      }
    }
  }

  return fields;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief gets the definitions of all index files for a collection
////////////////////////////////////////////////////////////////////////////////

TRI_vector_pointer_t* TRI_GetCollectionIndexes(const TRI_vocbase_t* vocbase,
                                               const char* collectionName) {
  TRI_vector_pointer_t* indexes;
  TRI_index_definition_t* indexDefinition;
  TRI_index_geo_variant_e geoVariant;
  TRI_vector_string_t indexFiles;
  TRI_vector_string_t* fields;
  TRI_json_t* json;
  TRI_json_t* strVal;
  TRI_json_t* numVal;
  char* error;
  char* temp;
  TRI_idx_iid_t indexId;
  TRI_idx_type_e indexType;
  bool indexUnique;
  size_t i;
 
  assert(vocbase); 

  indexes = (TRI_vector_pointer_t*) TRI_Allocate(sizeof(TRI_vector_pointer_t));
  if (!indexes) {
    return NULL;
  }
  TRI_InitVectorPointer(indexes);

  // add "pseudo" primary index
  indexDefinition = (TRI_index_definition_t*) TRI_Allocate(sizeof(TRI_index_definition_t));
  if (indexDefinition) {
    indexDefinition->_fields = TRI_Allocate(sizeof(TRI_vector_string_t));
    if (!indexDefinition->_fields) {
      TRI_Free(indexDefinition);
      return NULL;
    }

    TRI_InitVectorString(indexDefinition->_fields);
    indexDefinition->_iid      = 0; // note: this id is ignored
    indexDefinition->_type     = TRI_IDX_TYPE_PRIMARY_INDEX;
    indexDefinition->_isUnique = true; // primary index is always unique

    temp = TRI_DuplicateString("_id");
    if (temp) {
      TRI_PushBackVectorString(indexDefinition->_fields, temp); // name of field
    }
    TRI_PushBackVectorPointer(indexes, indexDefinition);
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

    // read uniqueness information
    strVal = TRI_LookupArrayJson(json, "unique");
    if (!strVal || strVal->_type != TRI_JSON_BOOLEAN) {
      indexUnique = false; // default is non-unique index
    }
    else {
      indexUnique = strVal->_value._boolean;
    }

    // read index type
    strVal = TRI_LookupArrayJson(json, "type");
    if (!strVal || strVal->_type != TRI_JSON_STRING) {
      goto NEXT_INDEX;
    }

    if (strcmp(strVal->_value._string.data, "hash") == 0) {
      indexType = TRI_IDX_TYPE_HASH_INDEX;
    }
    else if (strcmp(strVal->_value._string.data, "skiplist") == 0) {
      indexType = TRI_IDX_TYPE_SKIPLIST_INDEX;
    }
    else if (strcmp(strVal->_value._string.data, "geo") == 0) {
      indexType = TRI_IDX_TYPE_GEO_INDEX;
    }
    else {
      // unknown index type
      LOG_ERROR("found unknown index type '%s'", strVal->_value._string.data);
      goto NEXT_INDEX;
    }

    fields = GetFieldsIndex(indexType, json, &geoVariant);
    if (!fields) {
      goto NEXT_INDEX;
    }
    if (fields->_length == 0) {
      TRI_DestroyVectorString(fields);
      TRI_Free(fields);
      goto NEXT_INDEX;
    }

    // create the index definition
    indexDefinition = 
      (TRI_index_definition_t*) TRI_Allocate(sizeof(TRI_index_definition_t));
    if (!indexDefinition) {
      TRI_FreeVectorString(fields);
      goto NEXT_INDEX;
    }

    indexDefinition->_iid        = indexId;
    indexDefinition->_type       = indexType;
    indexDefinition->_isUnique   = indexUnique;
    indexDefinition->_fields     = fields;
    indexDefinition->_geoVariant = geoVariant;

    TRI_PushBackVectorPointer(indexes, indexDefinition);

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
  TRI_Insert2ArrayJson(json, "geoJson", TRI_CreateBooleanJson(geo->_geoJson));

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

static bool HashIndexHelper (const TRI_hash_index_t* hashIndex, 
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

    for (j = 0; j < hashIndex->_shapeList->_length; ++j) {
      
      TRI_shape_pid_t shape = *((TRI_shape_pid_t*)(TRI_AtVector(hashIndex->_shapeList,j)));
      
      // ..........................................................................
      // Determine if document has that particular shape 
      // ..........................................................................

      acc = TRI_ShapeAccessor(hashIndex->base._collection->_shaper, shapedDoc->_sid, shape);

      if (acc == NULL || acc->_shape == NULL) {
        if (acc != NULL) {
          TRI_FreeShapeAccessor(acc);
        }
        TRI_Free(hashElement->fields);
        return false;
      }  
     
      // ..........................................................................
      // Extract the field
      // ..........................................................................    

      if (! TRI_ExecuteShapeAccessor(acc, shapedDoc, &shapedObject)) {
        TRI_FreeShapeAccessor(acc);
        TRI_Free(hashElement->fields);

        return false;
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
 
    for (j = 0; j < hashIndex->_shapeList->_length; ++j) {
      TRI_shape_pid_t shape = *((TRI_shape_pid_t*)(TRI_AtVector(hashIndex->_shapeList,j)));
      
      // ..........................................................................
      // Determine if document has that particular shape 
      // ..........................................................................

      acc = TRI_ShapeAccessor(hashIndex->base._collection->_shaper, document->_document._sid, shape);

      if (acc == NULL || acc->_shape == NULL) {
        if (acc != NULL) {
          TRI_FreeShapeAccessor(acc);
        }

        TRI_Free(hashElement->fields);

        return false;
      }  
      
      // ..........................................................................
      // Extract the field
      // ..........................................................................    

      if (! TRI_ExecuteShapeAccessor(acc, &(document->_document), &shapedObject)) {
        TRI_FreeShapeAccessor(acc);
        TRI_Free(hashElement->fields);

        return false;
      }
      
#ifdef DEBUG_ORESTE
      TRI_json_t* object;
      TRI_string_buffer_t buffer;
      TRI_InitStringBuffer(&buffer);
      object = TRI_JsonShapedJson(hashIndex->base._collection->_shaper, &shapedObject);
      TRI_Stringify2Json(&buffer,object);
      printf("%s:%u:%s\n",__FILE__,__LINE__,buffer._buffer);
      printf("%s:%u:%f:\n",__FILE__,__LINE__,*((double*)(shapedObject._data.data)));
      TRI_DestroyStringBuffer(&buffer);
      TRI_Free(object);
      object = NULL;                      
#endif
      
      // ..........................................................................
      // Store the field
      // ..........................................................................    

      hashElement->fields[j] = shapedObject;

      TRI_FreeShapeAccessor(acc);
    }  // end of for loop
  }
  
  else {
    return false;
  }
  
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hash indexes a document
////////////////////////////////////////////////////////////////////////////////

static bool InsertHashIndex (TRI_index_t* idx, TRI_doc_mptr_t const* doc) {
  HashIndexElement hashElement;
  TRI_hash_index_t* hashIndex;
  int res;
  bool ok;

  // ............................................................................
  // Obtain the hash index structure
  // ............................................................................

  hashIndex = (TRI_hash_index_t*) idx;

  if (idx == NULL) {
    LOG_WARNING("internal error in InsertHashIndex");
    return false;
  }

  // ............................................................................
  // Allocate storage to shaped json objects stored as a simple list.
  // These will be used for hashing.
  // ............................................................................
    
  hashElement.numFields = hashIndex->_shapeList->_length;
  hashElement.fields    = TRI_Allocate( sizeof(TRI_shaped_json_t) * hashElement.numFields);

  if (hashElement.fields == NULL) {
    LOG_WARNING("out-of-memory in InsertHashIndex");
    return false;
  }  

  ok = HashIndexHelper(hashIndex, &hashElement, doc, NULL);
    
  if (!ok) {
    return false;
  }    
  
  // ............................................................................
  // Fill the json field list from the document for unique hash index
  // ............................................................................
  
  if (hashIndex->_unique) {
    res = HashIndex_insert(hashIndex->_hashIndex, &hashElement);
  }
  
  // ............................................................................
  // Fill the json field list from the document for non-unique hash index
  // ............................................................................
  
  else {
    res = MultiHashIndex_insert(hashIndex->_hashIndex, &hashElement);
  }
  
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
  size_t j;
   
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

  for (j = 0; j < hashIndex->_shapeList->_length; ++j) {
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
  TRI_Insert2ArrayJson(json, "unique", TRI_CreateBooleanJson(hashIndex->_unique));
  TRI_Insert2ArrayJson(json, "type", TRI_CreateStringCopyJson("hash"));
  TRI_Insert2ArrayJson(json, "fieldCount", TRI_CreateNumberJson(hashIndex->_shapeList->_length));

  for (j = 0; j < hashIndex->_shapeList->_length; ++j) {
    sprintf(fieldCounter,"field_%lu",j);
    TRI_Insert2ArrayJson(json, fieldCounter, TRI_CreateStringCopyJson(fieldList[j]));
  }

  TRI_Free(fieldList);
  TRI_Free(fieldCounter);
    
  return json;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes a document from a hash index
////////////////////////////////////////////////////////////////////////////////

static bool RemoveHashIndex (TRI_index_t* idx, TRI_doc_mptr_t const* doc) {
  HashIndexElement hashElement;
  TRI_hash_index_t* hashIndex;
  bool result;
  
  // ............................................................................
  // Obtain the hash index structure
  // ............................................................................

  hashIndex = (TRI_hash_index_t*) idx;

  if (idx == NULL) {
    LOG_WARNING("internal error in RemoveHashIndex");
    return false;
  }

  // ............................................................................
  // Allocate some memory for the HashIndexElement structure
  // ............................................................................

  hashElement.numFields = hashIndex->_shapeList->_length;
  hashElement.fields    = TRI_Allocate( sizeof(TRI_shaped_json_t) * hashElement.numFields);

  if (hashElement.fields == NULL) {
    LOG_WARNING("out-of-memory in InsertHashIndex");
    return false;
  }  
  
  // ..........................................................................
  // Fill the json field list from the document
  // ..........................................................................

  if (! HashIndexHelper(hashIndex, &hashElement, doc, NULL)) {
    return false;
  }    
  
  // ............................................................................
  // Attempt the removal for unique hash indexes
  // ............................................................................
  
  if (hashIndex->_unique) {
    result = HashIndex_remove(hashIndex->_hashIndex, &hashElement);
  } 
  
  // ............................................................................
  // Attempt the removal for non-unique hash indexes
  // ............................................................................
  
  else {
    result = MultiHashIndex_remove(hashIndex->_hashIndex, &hashElement);
  }
  
  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief updates a document from a hash index
////////////////////////////////////////////////////////////////////////////////

static bool UpdateHashIndex (TRI_index_t* idx,
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
    return false;
  }

  // ............................................................................
  // Allocate some memory for the HashIndexElement structure
  // ............................................................................

  hashElement.numFields = hashIndex->_shapeList->_length;
  hashElement.fields    = TRI_Allocate( sizeof(TRI_shaped_json_t) * hashElement.numFields);

  if (hashElement.fields == NULL) {
    LOG_WARNING("out-of-memory in UpdateHashIndex");
    return false;
  }  
  
  // ............................................................................
  // Update for unique hash index
  // ............................................................................
  
  // ............................................................................
  // Fill in the fields with the values from oldDoc
  // ............................................................................
  
  if (hashIndex->_unique) {
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

      if (!HashIndex_remove(hashIndex->_hashIndex, &hashElement)) {
        LOG_WARNING("could not remove old document from hash index in UpdateHashIndex");
      }
    }    

    // ............................................................................
    // Fill the json simple list from the document
    // ............................................................................

    if (! HashIndexHelper(hashIndex, &hashElement, newDoc, NULL)) {

      // ..........................................................................
      // probably fields do not match  
      // ..........................................................................
      return false;
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
    
    if (HashIndexHelper(hashIndex, &hashElement, NULL, oldDoc)) {

      // ............................................................................
      // We must fill the hashElement with the value of the document shape -- this
      // is necessary when we attempt to remove non-unique hash indexes.
      // ............................................................................

      cnv.c = newDoc;
      hashElement.data = cnv.p;
      
      // ............................................................................
      // Remove the hash index entry and return.
      // ............................................................................

      if (! MultiHashIndex_remove(hashIndex->_hashIndex, &hashElement)) {
        LOG_WARNING("could not remove old document from hash index in UpdateHashIndex");
      }
    }    

    // ............................................................................
    // Fill the shaped json simple list from the document
    // ............................................................................

    if (!HashIndexHelper(hashIndex, &hashElement, newDoc, NULL)) {

      // ..........................................................................
      // probably fields do not match  
      // ..........................................................................

      return false;
    }    

    // ............................................................................
    // Attempt to add the hash entry from the new doc
    // ............................................................................

    res = MultiHashIndex_insert(hashIndex->_hashIndex, &hashElement);
  }
  
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
                                  TRI_vector_t* shapeList,
                                  bool unique) {
  TRI_hash_index_t* hashIndex;
  size_t j;

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
  hashIndex->_unique     = unique;
  
  // ...........................................................................
  // Copy the contents of the shape list vector into a new vector and store this
  // ...........................................................................  

  hashIndex->_shapeList = TRI_Allocate(sizeof(TRI_vector_t));

  if (!hashIndex->_shapeList) {
    TRI_Free(hashIndex);
    return NULL;
  }

  TRI_InitVector(hashIndex->_shapeList, sizeof(TRI_shape_pid_t));

  for (j = 0; j < shapeList->_length; ++j) {
    TRI_shape_pid_t shape = *((TRI_shape_pid_t*)(TRI_AtVector(shapeList,j)));

    TRI_PushBackVector(hashIndex->_shapeList,&shape);
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
  LOG_ERROR("TRI_DestroyHashIndex not implemented");
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
  
  if (hashIndex->_unique) {
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
// For now return vector list of matches -- later return an iterator?
// Warning: who ever calls this function is responsible for destroying
// SkiplistIndexElements* results
// .............................................................................
SkiplistIndexElements* TRI_LookupSkiplistIndex(TRI_index_t* idx, TRI_json_t* parameterList) {

  TRI_skiplist_index_t* skiplistIndex;
  SkiplistIndexElements* result;
  SkiplistIndexElement  element;
  TRI_shaper_t*     shaper;
  size_t j;
  
  element.numFields = parameterList->_value._objects._length;
  element.fields    = TRI_Allocate( sizeof(TRI_json_t) * element.numFields);
  
  if (element.fields == NULL) {
    LOG_WARNING("out-of-memory in LookupSkiplistIndex");
    return NULL;
  }  
    
  skiplistIndex = (TRI_skiplist_index_t*) idx;
  shaper = skiplistIndex->base._collection->_shaper;
    
  for (j = 0; j < element.numFields; ++j) {
    TRI_json_t*        jsonObject   = (TRI_json_t*) (TRI_AtVector(&(parameterList->_value._objects),j));
    TRI_shaped_json_t* shapedObject = TRI_ShapedJsonJson(shaper, jsonObject);
    element.fields[j] = *shapedObject;
    TRI_Free(shapedObject);
  }
  
  element.collection = skiplistIndex->base._collection;
  
  if (skiplistIndex->_unique) {
    result = SkiplistIndex_find(skiplistIndex->_skiplistIndex, &element);
  }
  else {
    result = MultiSkiplistIndex_find(skiplistIndex->_skiplistIndex, &element);
  }

  for (j = 0; j < element.numFields; ++j) {
    TRI_DestroyShapedJson(element.fields + j);
  }
  TRI_Free(element.fields);
  
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
    
 
    for (j = 0; j < skiplistIndex->_shapeList->_length; ++j) {
      
      TRI_shape_pid_t shape = *((TRI_shape_pid_t*)(TRI_AtVector(skiplistIndex->_shapeList,j)));
      
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
    // Assign the document to the HashIndexElement structure - so that it can later
    // be retreived.
    // ..........................................................................
    cnv.c = document;
    skiplistElement->data = cnv.p;
 
    
    for (j = 0; j < skiplistIndex->_shapeList->_length; ++j) {
      
      TRI_shape_pid_t shape = *((TRI_shape_pid_t*)(TRI_AtVector(skiplistIndex->_shapeList,j)));
      
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

static bool InsertSkiplistIndex (TRI_index_t* idx, TRI_doc_mptr_t const* doc) {

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
    return false;
  }
  

  // ............................................................................
  // Allocate storage to shaped json objects stored as a simple list.
  // These will be used for comparisions
  // ............................................................................
    
  skiplistElement.numFields   = skiplistIndex->_shapeList->_length;
  skiplistElement.fields      = TRI_Allocate( sizeof(TRI_shaped_json_t) * skiplistElement.numFields);
  skiplistElement.collection  = skiplistIndex->base._collection;
  
  if (skiplistElement.fields == NULL) {
    LOG_WARNING("out-of-memory in InsertSkiplistIndex");
    return false;
  }  
  ok = SkiplistIndexHelper(skiplistIndex, &skiplistElement, doc, NULL);
    
  if (!ok) {
    return false;
  }    
  
  
  // ............................................................................
  // Fill the json field list from the document for unique skiplist index
  // ............................................................................
  
  if (skiplistIndex->_unique) {
    res = SkiplistIndex_insert(skiplistIndex->_skiplistIndex, &skiplistElement);
  }
  
  // ............................................................................
  // Fill the json field list from the document for non-unique skiplist index
  // ............................................................................
  
  else {
    res = MultiSkiplistIndex_insert(skiplistIndex->_skiplistIndex, &skiplistElement);
  }
  

  if (res == -1) {
    LOG_WARNING("found duplicate entry in skiplist-index, should not happen");
  }
  else if (res == -2) {
    LOG_WARNING("out-of-memory in skiplist-index");
  }
  else if (res == -99) {
    LOG_DEBUG("unknown error, ignoring entry");
  }

  return res == 0;
}


////////////////////////////////////////////////////////////////////////////////
/// @brief describes a skiplist  index as a json object
////////////////////////////////////////////////////////////////////////////////

static TRI_json_t* JsonSkiplistIndex (TRI_index_t* idx, TRI_doc_collection_t* collection) {

  TRI_json_t* json;
  const TRI_shape_path_t* path;
  TRI_skiplist_index_t* skiplistIndex;
  char const** fieldList;
  char* fieldCounter;
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
  fieldList = TRI_Allocate( (sizeof(char*) * skiplistIndex->_shapeList->_length) );
  if (fieldList == NULL) {
    return NULL;
  }


  // ..........................................................................  
  // Convert the attributes (field list of the skiplist index) into strings
  // ..........................................................................  
  for (j = 0; j < skiplistIndex->_shapeList->_length; ++j) {
    TRI_shape_pid_t shape = *((TRI_shape_pid_t*)(TRI_AtVector(skiplistIndex->_shapeList,j)));
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
  TRI_Insert2ArrayJson(json, "unique", TRI_CreateBooleanJson(skiplistIndex->_unique));
  TRI_Insert2ArrayJson(json, "type", TRI_CreateStringCopyJson("skiplist"));
  TRI_Insert2ArrayJson(json, "fieldCount", TRI_CreateNumberJson(skiplistIndex->_shapeList->_length));
  for (j = 0; j < skiplistIndex->_shapeList->_length; ++j) {
    sprintf(fieldCounter,"field_%lu",j);
    TRI_Insert2ArrayJson(json, fieldCounter, TRI_CreateStringCopyJson(fieldList[j]));
  }

  TRI_Free(fieldList);
  TRI_Free(fieldCounter);
    
  return json;
}


////////////////////////////////////////////////////////////////////////////////
/// @brief removes a document from a skiplist index
////////////////////////////////////////////////////////////////////////////////

static bool RemoveSkiplistIndex (TRI_index_t* idx, TRI_doc_mptr_t const* doc) {

  SkiplistIndexElement skiplistElement;
  TRI_skiplist_index_t* skiplistIndex;
  bool result;
  

  // ............................................................................
  // Obtain the skiplist index structure
  // ............................................................................
  skiplistIndex = (TRI_skiplist_index_t*) idx;
  if (idx == NULL) {
    LOG_WARNING("internal error in RemoveHashIndex");
    return false;
  }


  // ............................................................................
  // Allocate some memory for the SkiplistIndexElement structure
  // ............................................................................

  skiplistElement.numFields  = skiplistIndex->_shapeList->_length;
  skiplistElement.fields     = TRI_Allocate( sizeof(TRI_shaped_json_t) * skiplistElement.numFields);
  skiplistElement.collection = skiplistIndex->base._collection;
  
  if (skiplistElement.fields == NULL) {
    LOG_WARNING("out-of-memory in InsertSkiplistIndex");
    return false;
  }  
  
  // ..........................................................................
  // Fill the json field list from the document
  // ..........................................................................
  if (!SkiplistIndexHelper(skiplistIndex, &skiplistElement, doc, NULL)) {
    return false;
  }    

  
  // ............................................................................
  // Attempt the removal for unique skiplist indexes
  // ............................................................................
  
  if (skiplistIndex->_unique) {
    result = SkiplistIndex_remove(skiplistIndex->_skiplistIndex, &skiplistElement);
  } 
  
  // ............................................................................
  // Attempt the removal for non-unique skiplist indexes
  // ............................................................................
  
  else {
    result = MultiSkiplistIndex_remove(skiplistIndex->_skiplistIndex, &skiplistElement);
  }
  
  
  return result;
}


static bool UpdateSkiplistIndex (TRI_index_t* idx, const TRI_doc_mptr_t* newDoc, 
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
    return false;
  }


  // ............................................................................
  // Allocate some memory for the SkiplistIndexElement structure
  // ............................................................................

  skiplistElement.numFields  = skiplistIndex->_shapeList->_length;
  skiplistElement.fields     = TRI_Allocate( sizeof(TRI_shaped_json_t) * skiplistElement.numFields);
  skiplistElement.collection = skiplistIndex->base._collection;
  
  if (skiplistElement.fields == NULL) {
    LOG_WARNING("out-of-memory in UpdateHashIndex");
    return false;
  }  
      
  
  // ............................................................................
  // Update for unique skiplist index
  // ............................................................................
  
  
  // ............................................................................
  // Fill in the fields with the values from oldDoc
  // ............................................................................
  
  if (skiplistIndex->_unique) {
  
      
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
      return false;
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
      return false;
    }    


    // ............................................................................
    // Attempt to add the skiplist entry from the new doc
    // ............................................................................
    res = MultiSkiplistIndex_insert(skiplistIndex->_skiplistIndex, &skiplistElement);
    
  }
  
  if (res == -1) {
    LOG_WARNING("found duplicate entry in skiplist-index, should not happen");
  }
  else if (res == -2) {
    LOG_WARNING("out-of-memory in skiplist-index");
  }
  else if (res == -99) {
    LOG_DEBUG("unknown error, ignoring entry");
  }

  return res == 0;
}
  

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a skiplist index
////////////////////////////////////////////////////////////////////////////////

TRI_index_t* TRI_CreateSkiplistIndex (struct TRI_doc_collection_s* collection,
                                  TRI_vector_t* shapeList,
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

  skiplistIndex->base.insert = InsertSkiplistIndex;
  skiplistIndex->base.json   = JsonSkiplistIndex;
  skiplistIndex->base.remove = RemoveSkiplistIndex;
  skiplistIndex->base.update = UpdateSkiplistIndex;
  skiplistIndex->_unique     = unique;
  
  // ...........................................................................
  // Copy the contents of the shape list vector into a new vector and store this
  // ...........................................................................  
  skiplistIndex->_shapeList = TRI_Allocate(sizeof(TRI_vector_t));
  if (!skiplistIndex->_shapeList) {
    TRI_Free(skiplistIndex);
    return NULL;
  }

  TRI_InitVector(skiplistIndex->_shapeList, sizeof(TRI_shape_pid_t));
  for (j = 0; j < shapeList->_length; ++j) {
    TRI_shape_pid_t shape = *((TRI_shape_pid_t*)(TRI_AtVector(shapeList,j)));
    TRI_PushBackVector(skiplistIndex->_shapeList,&shape);
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
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
