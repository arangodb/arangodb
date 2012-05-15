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

#include "BasicsC/conversions.h"
#include "BasicsC/files.h"
#include "BasicsC/json.h"
#include "BasicsC/linked-list.h"
#include "BasicsC/logging.h"
#include "BasicsC/string-buffer.h"
#include "BasicsC/strings.h"
#include "ShapedJson/shape-accessor.h"
#include "ShapedJson/shaped-json.h"
#include "VocBase/simple-collection.h"

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
/// @brief free an index
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeIndex (TRI_index_t* const idx) {
  assert(idx);

  LOG_TRACE("freeing index");

  switch (idx->_type) {
    case TRI_IDX_TYPE_GEO1_INDEX:
    case TRI_IDX_TYPE_GEO2_INDEX:
      TRI_FreeGeoIndex(idx);
      break;

    case TRI_IDX_TYPE_HASH_INDEX:
      TRI_FreeHashIndex(idx);
      break;

    case TRI_IDX_TYPE_SKIPLIST_INDEX:
      TRI_FreeSkiplistIndex(idx);
      break;

    case TRI_IDX_TYPE_CAP_CONSTRAINT:
      TRI_FreeCapConstraint(idx);
      break;

    case TRI_IDX_TYPE_PRIMARY_INDEX:
      TRI_FreePrimaryIndex(idx);
      break;

    default: 
      // no action necessary
      break;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an index file
////////////////////////////////////////////////////////////////////////////////

bool TRI_RemoveIndexFile (TRI_doc_collection_t* collection, TRI_index_t* idx) {
  char* filename;
  char* name;
  char* number;
  int res;

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

    TRI_FreeString(TRI_UNKNOWN_MEM_ZONE, number);
    LOG_ERROR("out of memory when creating index name");
    return false;
  }

  filename = TRI_Concatenate2File(collection->base._directory, name);

  if (filename == NULL) {
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);

    TRI_FreeString(TRI_UNKNOWN_MEM_ZONE, number);
    TRI_FreeString(TRI_UNKNOWN_MEM_ZONE, name);
    LOG_ERROR("out of memory when creating index filename");
    return false;
  }

  TRI_FreeString(TRI_UNKNOWN_MEM_ZONE, name);
  TRI_FreeString(TRI_UNKNOWN_MEM_ZONE, number);

  res = TRI_UnlinkFile(filename);
  TRI_FreeString(TRI_UNKNOWN_MEM_ZONE, filename);

  if (res != TRI_ERROR_NO_ERROR) {
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

  TRI_FreeString(TRI_UNKNOWN_MEM_ZONE, name);
  TRI_FreeString(TRI_UNKNOWN_MEM_ZONE, number);

  // and save
  ok = TRI_SaveJson(filename, json);

  TRI_FreeString(TRI_UNKNOWN_MEM_ZONE, filename);
  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);

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
    TRI_set_errno(TRI_ERROR_ARANGO_UNKNOWN_COLLECTION_TYPE);
    return NULL;
  }

  sim = (TRI_sim_collection_t*) collection;

  for (i = 0;  i < sim->_indexes._length;  ++i) {
    idx = sim->_indexes._buffer[i];

    if (idx->_iid == iid) {
      return idx;
    }
  }

  TRI_set_errno(TRI_ERROR_ARANGO_NO_INDEX);
  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief gets name of index type
////////////////////////////////////////////////////////////////////////////////

char const* TRI_TypeNameIndex (const TRI_index_t* const idx) {
  switch (idx->_type) {
    case TRI_IDX_TYPE_HASH_INDEX: 
      return "hash";

    case TRI_IDX_TYPE_PRIORITY_QUEUE_INDEX:
      return "priorityqueue";

    case TRI_IDX_TYPE_SKIPLIST_INDEX:
      return "skiplist";

    case TRI_IDX_TYPE_GEO1_INDEX:
      return "geo1";

    case TRI_IDX_TYPE_GEO2_INDEX:
      return "geo2";

    case TRI_IDX_TYPE_CAP_CONSTRAINT:
      return "cap";

    case TRI_IDX_TYPE_PRIMARY_INDEX:
      return "primary";
  }

  return "unknown";
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return whether an index supports full coverage only
////////////////////////////////////////////////////////////////////////////////

bool TRI_NeedsFullCoverageIndex (const TRI_index_t* const idx) {
  // we'll use a switch here so the compiler warns if new index types are added elsewhere but not here
  switch (idx->_type) {
    case TRI_IDX_TYPE_GEO1_INDEX:
    case TRI_IDX_TYPE_GEO2_INDEX:
    case TRI_IDX_TYPE_PRIMARY_INDEX:
    case TRI_IDX_TYPE_HASH_INDEX:
    case TRI_IDX_TYPE_PRIORITY_QUEUE_INDEX:
    case TRI_IDX_TYPE_CAP_CONSTRAINT:
      return true;
    case TRI_IDX_TYPE_SKIPLIST_INDEX:
      return false;
  }

  assert(false);
  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                     PRIMARY INDEX
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a primary index, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyPrimaryIndex (TRI_index_t* idx) {
  LOG_TRACE("destroying primary index");

  TRI_DestroyVectorString(&idx->_fields);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a primary index and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreePrimaryIndex (TRI_index_t* idx) {
  TRI_DestroyPrimaryIndex(idx);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, idx); 
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    CAP CONSTRAINT
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief describes a cap constraint as a json object
////////////////////////////////////////////////////////////////////////////////

static TRI_json_t* JsonCapConstraint (TRI_index_t* idx, TRI_doc_collection_t const* collection) {
  TRI_json_t* json;
  TRI_cap_constraint_t* cap;
   
  // recast as a cap constraint
  cap = (TRI_cap_constraint_t*) idx;

  if (cap == NULL) {
    TRI_set_errno(TRI_ERROR_INTERNAL);
    return NULL;
  }
  
  // create json object and fill it
  json = TRI_CreateArrayJson(TRI_CORE_MEM_ZONE);

  TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, json, "id",   TRI_CreateNumberJson(TRI_CORE_MEM_ZONE, idx->_iid));
  TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, json, "type", TRI_CreateStringCopyJson(TRI_CORE_MEM_ZONE, "cap"));
  TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, json, "size",  TRI_CreateNumberJson(TRI_CORE_MEM_ZONE, cap->_size));

  return json;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes a cap constraint from collection
////////////////////////////////////////////////////////////////////////////////

static void RemoveIndexCapConstraint (TRI_index_t* idx, TRI_doc_collection_t* collection) {
  collection->_capConstraint = NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief inserts a document
////////////////////////////////////////////////////////////////////////////////

static int InsertCapConstraint (TRI_index_t* idx, TRI_doc_mptr_t const* doc) {
  TRI_cap_constraint_t* cap;

  cap = (TRI_cap_constraint_t*) idx;

  return TRI_AddLinkedArray(&cap->_array, doc);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief updates a document
////////////////////////////////////////////////////////////////////////////////

static int UpdateCapConstraint (TRI_index_t* idx,
                                const TRI_doc_mptr_t* newDoc, 
                                const TRI_shaped_json_t* oldDoc) {
  TRI_cap_constraint_t* cap;

  cap = (TRI_cap_constraint_t*) idx;
  TRI_MoveToBackLinkedArray(&cap->_array, newDoc);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes a document
////////////////////////////////////////////////////////////////////////////////

static int RemoveCapConstraint (TRI_index_t* idx, TRI_doc_mptr_t const* doc) {
  TRI_cap_constraint_t* cap;

  cap = (TRI_cap_constraint_t*) idx;
  TRI_RemoveLinkedArray(&cap->_array, doc);

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
/// @brief creates a cap constraint
////////////////////////////////////////////////////////////////////////////////

TRI_index_t* TRI_CreateCapConstraint (struct TRI_doc_collection_s* collection,
                                      size_t size) {
  TRI_cap_constraint_t* cap;

  cap = TRI_Allocate(TRI_CORE_MEM_ZONE, sizeof(TRI_cap_constraint_t), false);
  
  cap->base._iid = TRI_NewTickVocBase();
  cap->base._type = TRI_IDX_TYPE_CAP_CONSTRAINT;
  cap->base._collection = collection;
  cap->base._unique = false;

  cap->base.json = JsonCapConstraint;
  cap->base.removeIndex = RemoveIndexCapConstraint;

  cap->base.insert = InsertCapConstraint;
  cap->base.update = UpdateCapConstraint;
  cap->base.remove = RemoveCapConstraint;

  TRI_InitLinkedArray(&cap->_array, TRI_UNKNOWN_MEM_ZONE);

  cap->_size = size;
  
  return &cap->base;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the memory allocated, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyCapConstraint (TRI_index_t* idx) {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the memory allocated and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeCapConstraint (TRI_index_t* idx) {
  TRI_DestroyCapConstraint(idx);
  TRI_Free(TRI_CORE_MEM_ZONE, idx);
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
                                double* result,
                                bool* missing) {
  TRI_shape_access_t* acc;
  TRI_shaped_json_t json;
  TRI_shape_sid_t sid;
  bool ok;

  sid = document->_sid;
  acc = TRI_ShapeAccessor(shaper, sid, pid);
  *missing = false;

  // attribute not known or an internal error
  if (acc == NULL || acc->_shape == NULL) {
    if (acc != NULL) {
      TRI_FreeShapeAccessor(acc);
      *missing = true;
    }

    return false;
  }

  // number
  if (acc->_shape->_sid == shaper->_sidNumber) {
    ok = TRI_ExecuteShapeAccessor(acc, document, &json);

    if (ok) {
      *result = * (double*) json._data.data;
      TRI_FreeShapeAccessor(acc);
    }

    return ok;
  }

  // null
  else if (acc->_shape->_sid == shaper->_sidNull) {
    *missing = true;
    TRI_FreeShapeAccessor(acc);
    return false;
  }

  // everyting else
  else {
    TRI_FreeShapeAccessor(acc);
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
  TRI_shape_access_t* acc;
  TRI_shaped_json_t list;
  TRI_shaped_json_t entry;
  TRI_shape_sid_t sid;
  size_t len;
  bool ok;

  sid = document->_sid;
  acc = TRI_ShapeAccessor(shaper, sid, pid);
  *missing = false;

  // attribute not known or an internal error
  if (acc == NULL || acc->_shape == NULL) {
    if (acc != NULL) {
      TRI_FreeShapeAccessor(acc);
      *missing = true;
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

  // null
  else if (acc->_shape->_type == TRI_SHAPE_NULL) {
    *missing = true;
  }

  // ups
  TRI_FreeShapeAccessor(acc);
  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief inserts a new document
////////////////////////////////////////////////////////////////////////////////

static int InsertGeoIndex (TRI_index_t* idx, TRI_doc_mptr_t const* doc) {
  union { void* p; void const* c; } cnv;
  GeoCoordinate gc;
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
  if (geo->_location != 0) {
    if (geo->_geoJson) {
      ok = ExtractDoubleList(shaper, &doc->_document, geo->_location, &longitude, &latitude, &missing);
    }
    else {
      ok = ExtractDoubleList(shaper, &doc->_document, geo->_location, &latitude, &longitude, &missing);
    }
  }
  else {
    ok = ExtractDoubleArray(shaper, &doc->_document, geo->_latitude, &latitude, &missing);
    ok = ok && ExtractDoubleArray(shaper, &doc->_document, geo->_longitude, &longitude, &missing);
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
/// @brief updates a document
////////////////////////////////////////////////////////////////////////////////

static int  UpdateGeoIndex (TRI_index_t* idx, TRI_doc_mptr_t const* doc, TRI_shaped_json_t const* old) {
  union { void* p; void const* c; } cnv;
  GeoCoordinate gc;
  TRI_geo_index_t* geo;
  TRI_shaper_t* shaper;
  bool missing;
  bool ok;
  double latitude;
  double longitude;
  int res;

  geo = (TRI_geo_index_t*) idx;
  shaper = geo->base._collection->_shaper;

  // lookup OLD latitude and longitude
  if (geo->_location != 0) {
    ok = ExtractDoubleList(shaper, old, geo->_location, &latitude, &longitude, &missing);
  }
  else {
    ok = ExtractDoubleArray(shaper, old, geo->_latitude, &latitude, &missing);
    ok = ok && ExtractDoubleArray(shaper, old, geo->_longitude, &longitude, &missing);
  }

  // and remove old entry
  if (ok) {
    gc.latitude = latitude;
    gc.longitude = longitude;

    cnv.c = doc;
    gc.data = cnv.p;

    res = GeoIndex_remove(geo->_geoIndex, &gc);

    if (res != 0) {
      LOG_DEBUG("cannot remove old index entry: %d", res);
    }
  }

  // create new entry with new coordinates
  if (geo->_location != 0) {
    ok = ExtractDoubleList(shaper, &doc->_document, geo->_location, &latitude, &longitude, &missing);
  }
  else {
    ok = ExtractDoubleArray(shaper, &doc->_document, geo->_latitude, &latitude, &missing);
    ok = ok && ExtractDoubleArray(shaper, &doc->_document, geo->_longitude, &longitude, &missing);
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
  union { void* p; void const* c; } cnv;
  GeoCoordinate gc;
  TRI_geo_index_t* geo;
  TRI_shaper_t* shaper;
  bool missing;
  bool ok;
  double latitude;
  double longitude;
  int res;

  geo = (TRI_geo_index_t*) idx;
  shaper = geo->base._collection->_shaper;

  // lookup OLD latitude and longitude
  if (geo->_location != 0) {
    ok = ExtractDoubleList(shaper, &doc->_document, geo->_location, &latitude, &longitude, &missing);
  }
  else {
    ok = ExtractDoubleArray(shaper, &doc->_document, geo->_latitude, &latitude, &missing);
    ok = ok && ExtractDoubleArray(shaper, &doc->_document, geo->_longitude, &longitude, &missing);
  }

  // and remove old entry
  if (ok) {
    gc.latitude = latitude;
    gc.longitude = longitude;

    cnv.c = doc;
    gc.data = cnv.p;

    res = GeoIndex_remove(geo->_geoIndex, &gc);

    if (res != 0) {
      LOG_DEBUG("cannot remove old index entry: %d", res);
      return TRI_set_errno(TRI_ERROR_INTERNAL);
    }
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief JSON description of a geo index, location is a list
////////////////////////////////////////////////////////////////////////////////

static TRI_json_t* JsonGeo1Index (TRI_index_t* idx, TRI_doc_collection_t const* collection) {
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
  json = TRI_CreateArrayJson(TRI_UNKNOWN_MEM_ZONE);

  if (json == NULL) {
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
    return NULL;
  }

  fields = TRI_CreateListJson(TRI_UNKNOWN_MEM_ZONE);
  TRI_PushBack3ListJson(TRI_UNKNOWN_MEM_ZONE, fields, TRI_CreateStringCopyJson(TRI_UNKNOWN_MEM_ZONE, location));

  TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, json, "id", TRI_CreateNumberJson(TRI_UNKNOWN_MEM_ZONE, idx->_iid));
  TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, json, "type", TRI_CreateStringCopyJson(TRI_UNKNOWN_MEM_ZONE, "geo1"));
  TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, json, "geoJson", TRI_CreateBooleanJson(TRI_UNKNOWN_MEM_ZONE, geo->_geoJson));
  TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, json, "constraint", TRI_CreateBooleanJson(TRI_UNKNOWN_MEM_ZONE, geo->_constraint));

  if (geo->_constraint) {
    TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, json, "ignoreNull", TRI_CreateBooleanJson(TRI_UNKNOWN_MEM_ZONE, geo->base._ignoreNull));
  }

  TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, json, "fields", fields);

  return json;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief JSON description of a geo index, two attributes
////////////////////////////////////////////////////////////////////////////////

static TRI_json_t* JsonGeo2Index (TRI_index_t* idx, TRI_doc_collection_t const* collection) {
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
  json = TRI_CreateArrayJson(TRI_UNKNOWN_MEM_ZONE);

  if (json == NULL) {
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
    return NULL;
  }

  fields = TRI_CreateListJson(TRI_UNKNOWN_MEM_ZONE);
  TRI_PushBack3ListJson(TRI_UNKNOWN_MEM_ZONE, fields, TRI_CreateStringCopyJson(TRI_UNKNOWN_MEM_ZONE, latitude));
  TRI_PushBack3ListJson(TRI_UNKNOWN_MEM_ZONE, fields, TRI_CreateStringCopyJson(TRI_UNKNOWN_MEM_ZONE, longitude));

  TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, json, "id", TRI_CreateNumberJson(TRI_UNKNOWN_MEM_ZONE, idx->_iid));
  TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, json, "type", TRI_CreateStringCopyJson(TRI_UNKNOWN_MEM_ZONE, "geo2"));
  TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, json, "constraint", TRI_CreateBooleanJson(TRI_UNKNOWN_MEM_ZONE, geo->_constraint));

  if (geo->_constraint) {
    TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, json, "ignoreNull", TRI_CreateBooleanJson(TRI_UNKNOWN_MEM_ZONE, geo->base._ignoreNull));
  }

  TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, json, "fields", fields);

  return json;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes a json index from collection
////////////////////////////////////////////////////////////////////////////////

static void RemoveIndexGeoIndex (TRI_index_t* idx, TRI_doc_collection_t* collection) {
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

TRI_index_t* TRI_CreateGeo1Index (struct TRI_doc_collection_s* collection,
                                  char const* locationName,
                                  TRI_shape_pid_t location,
                                  bool geoJson,
                                  bool constraint,
                                  bool ignoreNull) {
  TRI_geo_index_t* geo;
  char* ln;

  geo = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_geo_index_t), false);

  if (geo == NULL) {
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
    return NULL;
  }

  ln = TRI_DuplicateString(locationName);

  TRI_InitVectorString(&geo->base._fields, TRI_UNKNOWN_MEM_ZONE);

  geo->base._iid = TRI_NewTickVocBase();
  geo->base._type = TRI_IDX_TYPE_GEO1_INDEX;
  geo->base._collection = collection;
  geo->base._unique = false;
  geo->base._ignoreNull = ignoreNull;

  geo->base.json = JsonGeo1Index;
  geo->base.removeIndex = RemoveIndexGeoIndex;

  geo->base.insert = InsertGeoIndex;
  geo->base.remove = RemoveGeoIndex;
  geo->base.update = UpdateGeoIndex;

  TRI_PushBackVectorString(&geo->base._fields, ln);

  geo->_constraint = constraint;
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

TRI_index_t* TRI_CreateGeo2Index (struct TRI_doc_collection_s* collection,
                                  char const* latitudeName,
                                  TRI_shape_pid_t latitude,
                                  char const* longitudeName,
                                  TRI_shape_pid_t longitude,
                                  bool constraint,
                                  bool ignoreNull) {
  TRI_geo_index_t* geo;
  char* lat;
  char* lon;

  geo = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_geo_index_t), false);

  if (geo == NULL) {
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
    return NULL;
  }

  lat = TRI_DuplicateString(latitudeName);
  lon = TRI_DuplicateString(longitudeName);

  TRI_InitVectorString(&geo->base._fields, TRI_UNKNOWN_MEM_ZONE);

  geo->base._iid = TRI_NewTickVocBase();
  geo->base._type = TRI_IDX_TYPE_GEO2_INDEX;
  geo->base._collection = collection;
  geo->base._unique = false;
  geo->base._ignoreNull = ignoreNull;

  geo->base.json = JsonGeo2Index;
  geo->base.removeIndex = RemoveIndexGeoIndex;

  geo->base.insert = InsertGeoIndex;
  geo->base.remove = RemoveGeoIndex;
  geo->base.update = UpdateGeoIndex;

  TRI_PushBackVectorString(&geo->base._fields, lat);
  TRI_PushBackVectorString(&geo->base._fields, lon);

  geo->_constraint = constraint;
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

  LOG_TRACE("destroying geo index");
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

static int HashIndexHelper (TRI_hash_index_t const* hashIndex, 
                            HashIndexElement* hashElement,
                            TRI_doc_mptr_t const* document,
                            TRI_shaped_json_t const* shapedDoc) {
  union { void* p; void const* c; } cnv;
  TRI_shape_access_t* acc;
  TRI_shaped_json_t shapedObject;
  TRI_shaper_t* shaper;
  int res;
  size_t j;
  
  shaper = hashIndex->base._collection->_shaper;

  // .............................................................................
  // Attempting to locate a hash entry using TRI_shaped_json_t object. Use this
  // when we wish to remove a hash entry and we only have the "keys" rather than
  // having the document (from which the keys would follow).
  // .............................................................................

  if (shapedDoc != NULL) {
    hashElement->data = NULL;
  }

  // .............................................................................
  // Assign the document to the HashIndexElement structure - so that it can
  // later be retreived.
  // .............................................................................

  else if (document != NULL) {
    cnv.c = document;
    hashElement->data = cnv.p;

    shapedDoc = &document->_document;
  }

  else {
    return TRI_ERROR_INTERNAL;
  }
  
  // .............................................................................
  // Extract the attribute values
  // .............................................................................

  res = TRI_ERROR_NO_ERROR;

  for (j = 0;  j < hashIndex->_paths._length;  ++j) {
    TRI_shape_pid_t shape = *((TRI_shape_pid_t*)(TRI_AtVector(&hashIndex->_paths, j)));
      
    // determine if document has that particular shape 
    acc = TRI_ShapeAccessor(shaper, shapedDoc->_sid, shape);

    if (acc == NULL || acc->_shape == NULL) {
      if (acc != NULL) {
        TRI_FreeShapeAccessor(acc);
      }

      shapedObject._sid = shaper->_sidNull;
      shapedObject._data.length = 0;
      shapedObject._data.data = NULL;

      res = TRI_WARNING_ARANGO_INDEX_HASH_DOCUMENT_ATTRIBUTE_MISSING;
    }
    else {
     
      // extract the field
      if (! TRI_ExecuteShapeAccessor(acc, shapedDoc, &shapedObject)) {
        TRI_FreeShapeAccessor(acc);
        // TRI_Free(hashElement->fields); memory deallocated in the calling procedure

        return TRI_ERROR_INTERNAL;
      }

      TRI_FreeShapeAccessor(acc);

      if (shapedObject._sid == shaper->_sidNull) {
        res = TRI_WARNING_ARANGO_INDEX_HASH_DOCUMENT_ATTRIBUTE_MISSING;
      }
    }
      
    // store the json shaped Object -- this is what will be hashed
    hashElement->fields[j] = shapedObject;
  }
  
  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief describes a hash index as a json object
////////////////////////////////////////////////////////////////////////////////

static TRI_json_t* JsonHashIndex (TRI_index_t* idx, TRI_doc_collection_t const* collection) {
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

  fieldList = TRI_Allocate( TRI_UNKNOWN_MEM_ZONE, (sizeof(char*) * hashIndex->_paths._length), false);

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
      TRI_Free(TRI_UNKNOWN_MEM_ZONE, fieldList);
      return NULL;
    }  

    fieldList[j] = ((const char*) path) + sizeof(TRI_shape_path_t) + path->_aidLength * sizeof(TRI_shape_aid_t);
  }

  // ..........................................................................  
  // create json object and fill it
  // ..........................................................................  

  json = TRI_CreateArrayJson(TRI_UNKNOWN_MEM_ZONE);

  if (json == NULL) {
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, fieldList);
    return NULL;
  }

  fields = TRI_CreateListJson(TRI_UNKNOWN_MEM_ZONE);

  for (j = 0; j < hashIndex->_paths._length; ++j) {
    TRI_PushBack3ListJson(TRI_UNKNOWN_MEM_ZONE, fields, TRI_CreateStringCopyJson(TRI_UNKNOWN_MEM_ZONE, fieldList[j]));
  }

  TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, json, "id", TRI_CreateNumberJson(TRI_UNKNOWN_MEM_ZONE, idx->_iid));
  TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, json, "unique", TRI_CreateBooleanJson(TRI_UNKNOWN_MEM_ZONE, hashIndex->base._unique));
  TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, json, "type", TRI_CreateStringCopyJson(TRI_UNKNOWN_MEM_ZONE, "hash"));
  TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, json, "fields", fields);

  TRI_Free(TRI_UNKNOWN_MEM_ZONE, fieldList);
    
  return json;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes a hash index from collection
////////////////////////////////////////////////////////////////////////////////

static void RemoveIndexHashIndex (TRI_index_t* idx, TRI_doc_collection_t* collection) {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief inserts a a document to a hash index
////////////////////////////////////////////////////////////////////////////////

static int InsertHashIndex (TRI_index_t* idx, TRI_doc_mptr_t const* doc) {
  HashIndexElement hashElement;
  TRI_hash_index_t* hashIndex;
  int res;

  // .............................................................................
  // Obtain the hash index structure
  // .............................................................................

  hashIndex = (TRI_hash_index_t*) idx;

  if (idx == NULL) {
    LOG_WARNING("internal error in InsertHashIndex");
    return TRI_set_errno(TRI_ERROR_INTERNAL);
  }

  // .............................................................................
  // Allocate storage to shaped json objects stored as a simple list.
  // These will be used for hashing.
  // .............................................................................
    
  hashElement.numFields = hashIndex->_paths._length;
  hashElement.fields    = TRI_Allocate(TRI_CORE_MEM_ZONE, sizeof(TRI_shaped_json_t) * hashElement.numFields, false);
         
  res = HashIndexHelper(hashIndex, &hashElement, doc, NULL);

  // .............................................................................
  // It is possible that this document does not have the necessary attributes
  // (keys) to participate in this index.
  //
  // If an error occurred in the called procedure HashIndexHelper, we must
  // now exit -- and deallocate memory assigned to hashElement.
  // .............................................................................
  
  if (res != TRI_ERROR_NO_ERROR) {
  
    // .............................................................................
    // It may happen that the document does not have the necessary attributes to 
    // be included within the hash index, in this case do not report back an error.
    // .............................................................................
    
    if (res == TRI_WARNING_ARANGO_INDEX_HASH_DOCUMENT_ATTRIBUTE_MISSING) { 
      if (hashIndex->base._unique) {
        TRI_Free(TRI_UNKNOWN_MEM_ZONE, hashElement.fields);
        return TRI_ERROR_NO_ERROR;
      }
    }
    else {
      TRI_Free(TRI_UNKNOWN_MEM_ZONE, hashElement.fields);
      return res;
    }
  }
  
  // .............................................................................
  // Fill the json field list from the document for unique or non-unique index
  // .............................................................................
  
  if (hashIndex->base._unique) {
    res = HashIndex_insert(hashIndex->_hashIndex, &hashElement);
  }
  else {
    res = MultiHashIndex_insert(hashIndex->_hashIndex, &hashElement);
  }

  // .............................................................................
  // Memory which has been allocated to hashElement.fields remains allocated
  // contents of which are stored in the hash array.
  // .............................................................................
      
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, hashElement.fields);
  
  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes a document from a hash index
////////////////////////////////////////////////////////////////////////////////

static int RemoveHashIndex (TRI_index_t* idx, TRI_doc_mptr_t const* doc) {
  HashIndexElement hashElement;
  TRI_hash_index_t* hashIndex;
  int res;
  
  // .............................................................................
  // Obtain the hash index structure
  // .............................................................................

  hashIndex = (TRI_hash_index_t*) idx;

  if (idx == NULL) {
    LOG_WARNING("internal error in RemoveHashIndex");
    return TRI_set_errno(TRI_ERROR_INTERNAL);
  } 

  // .............................................................................
  // Allocate some memory for the HashIndexElement structure
  // .............................................................................

  hashElement.numFields = hashIndex->_paths._length;
  hashElement.fields    = TRI_Allocate(TRI_CORE_MEM_ZONE, sizeof(TRI_shaped_json_t) * hashElement.numFields, false);

  // .............................................................................
  // Fill the json field list from the document
  // .............................................................................

  res = HashIndexHelper(hashIndex, &hashElement, doc, NULL);

  // .............................................................................
  // It may happen that the document does not have attributes which match
  // For now return internal error, there needs to be its own error number
  // and the appropriate action needs to be taken by the calling function in 
  // such cases.
  // .............................................................................
  
  if (res != TRI_ERROR_NO_ERROR) {
   
    // .............................................................................
    // It may happen that the document does not have the necessary attributes to
    // have particpated within the hash index. In this case, we do not report an
    // error to the calling procedure.
    //
    // TRI_WARNING_ARANGO_INDEX_HASH_DOCUMENT_ATTRIBUTE_MISSING from the called
    // procedure HashIndexHelper implies that we do not propagate the error to
    // the parent function. However for removal we advice the parent
    // function. TODO: return a proper error code.
    // .............................................................................
    
    if (res == TRI_WARNING_ARANGO_INDEX_HASH_DOCUMENT_ATTRIBUTE_MISSING) { 
      if (hashIndex->base._unique) {
        TRI_Free(TRI_UNKNOWN_MEM_ZONE, hashElement.fields);
        return TRI_ERROR_NO_ERROR;
      }
    }
    else {
      TRI_Free(TRI_UNKNOWN_MEM_ZONE, hashElement.fields);
      return res;
    }
  }
  
  // .............................................................................
  // Attempt the removal for unique or non-unique hash indexes
  // .............................................................................
  
  if (hashIndex->base._unique) {
    res = HashIndex_remove(hashIndex->_hashIndex, &hashElement);
  } 
  else {
    res = MultiHashIndex_remove(hashIndex->_hashIndex, &hashElement);
  }
  
  // .............................................................................
  // Deallocate memory allocated to hashElement.fields above
  // .............................................................................
    
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, hashElement.fields);
  
  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief updates a document from a hash index
////////////////////////////////////////////////////////////////////////////////

static int UpdateHashIndex (TRI_index_t* idx,
                            const TRI_doc_mptr_t* newDoc, 
                            const TRI_shaped_json_t* oldDoc) {
                             
  // .............................................................................
  // Note: The oldDoc is represented by the TRI_shaped_json_t rather than by a
  // TRI_doc_mptr_t object. However for non-unique indexes we must pass the
  // document shape to the hash remove function.
  // .............................................................................
  
  union { void* p; void const* c; } cnv;
  HashIndexElement hashElement;
  TRI_hash_index_t* hashIndex;
  int  res;  
  
  // .............................................................................
  // Obtain the hash index structure
  // .............................................................................
  
  hashIndex = (TRI_hash_index_t*) idx;

  if (idx == NULL) {
    LOG_WARNING("internal error in UpdateHashIndex");
    return TRI_ERROR_INTERNAL;
  }

  // .............................................................................
  // Allocate some memory for the HashIndexElement structure
  // .............................................................................

  hashElement.numFields = hashIndex->_paths._length;
  hashElement.fields    = TRI_Allocate(TRI_CORE_MEM_ZONE, sizeof(TRI_shaped_json_t) * hashElement.numFields, false);
  
  // .............................................................................
  // Update for unique hash index
  //
  // Fill in the fields with the values from oldDoc
  // .............................................................................
  
  assert(oldDoc != NULL);

  res = HashIndexHelper(hashIndex, &hashElement, NULL, oldDoc);
    
  if (res == TRI_ERROR_NO_ERROR) {
    
    // ............................................................................
    // We must fill the hashElement with the value of the document shape -- this
    // is necessary when we attempt to remove non-unique hash indexes.
    // ............................................................................

    cnv.c = newDoc; // we are assuming here that the doc ptr does not change
    hashElement.data = cnv.p;
      
    // ............................................................................
    // Remove the old hash index entry
    // ............................................................................

    if (hashIndex->base._unique) {
      res = HashIndex_remove(hashIndex->_hashIndex, &hashElement); 
    }
    else {
      res = MultiHashIndex_remove(hashIndex->_hashIndex, &hashElement);
    }

    // ..........................................................................
    // This error is common, when a document 'update' occurs, but fails
    // due to the fact that a duplicate entry already exists, when the 'rollback'
    // is applied, there is no document to remove -- so we get this error.
    // ..........................................................................
        
    if (res != TRI_ERROR_NO_ERROR) {
      LOG_DEBUG("could not remove existing document from hash index in UpdateHashIndex");
    }
  }    
    
  else if (res != TRI_WARNING_ARANGO_INDEX_HASH_DOCUMENT_ATTRIBUTE_MISSING) {
    LOG_WARNING("existing document was not removed from hash index in UpdateHashIndex");
  }
    
  // ............................................................................
  // Fill the json simple list from the document
  // ............................................................................

  res = HashIndexHelper(hashIndex, &hashElement, newDoc, NULL);

  // ............................................................................
  // Deal with any errors reported back.
  // ............................................................................

  if (res != TRI_ERROR_NO_ERROR) {
    
    // probably fields do not match. 
    if (res == TRI_WARNING_ARANGO_INDEX_HASH_DOCUMENT_ATTRIBUTE_MISSING) {
      if (hashIndex->base._unique) {
        TRI_Free(TRI_UNKNOWN_MEM_ZONE, hashElement.fields);
        return TRI_ERROR_NO_ERROR;
      }
    }
    else {
      TRI_Free(TRI_UNKNOWN_MEM_ZONE, hashElement.fields);
      return res;
    }
  }

  // ............................................................................
  // Attempt to add the hash entry from the new doc
  // ............................................................................

  if (hashIndex->base._unique) {
    res = HashIndex_insert(hashIndex->_hashIndex, &hashElement);
  }
  else {
    res = MultiHashIndex_insert(hashIndex->_hashIndex, &hashElement);
  }

  // ............................................................................
  // Deallocate memory given to hashElement.fields
  // ............................................................................
  
  TRI_Free(TRI_CORE_MEM_ZONE, hashElement.fields);
  
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

  hashIndex = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_hash_index_t), false);

  if (hashIndex == NULL) {
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
    return NULL;
  }
  
  hashIndex->base._iid = TRI_NewTickVocBase();
  hashIndex->base._type = TRI_IDX_TYPE_HASH_INDEX;
  hashIndex->base._collection = collection;
  hashIndex->base._unique = unique;

  hashIndex->base.json = JsonHashIndex;
  hashIndex->base.removeIndex = RemoveIndexHashIndex;

  hashIndex->base.insert = InsertHashIndex;
  hashIndex->base.remove = RemoveHashIndex;
  hashIndex->base.update = UpdateHashIndex;

  // ...........................................................................
  // Copy the contents of the path list vector into a new vector and store this
  // ...........................................................................  

  TRI_InitVector(&hashIndex->_paths, TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_shape_pid_t));

  for (j = 0;  j < paths->_length;  ++j) {
    TRI_shape_pid_t shape = *((TRI_shape_pid_t*)(TRI_AtVector(paths,j)));

    TRI_PushBackVector(&hashIndex->_paths, &shape);
  }
  
  TRI_InitVectorString(&hashIndex->base._fields, TRI_UNKNOWN_MEM_ZONE);

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
  
  if (hashIndex->_hashIndex == NULL) {
    TRI_DestroyVector(&hashIndex->_paths); 
    TRI_DestroyVectorString(&hashIndex->base._fields); 
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, hashIndex);
    return NULL;
  }
  
  return &hashIndex->base;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the memory allocated, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyHashIndex (TRI_index_t* idx) {
  TRI_hash_index_t* hashIndex;

  LOG_TRACE("destroying hash index");
  TRI_DestroyVectorString(&idx->_fields);

  hashIndex = (TRI_hash_index_t*) idx;

  TRI_DestroyVector(&hashIndex->_paths);

  HashIndex_free(hashIndex->_hashIndex);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the memory allocated and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeHashIndex (TRI_index_t* idx) {
  TRI_DestroyHashIndex(idx);
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
/// @brief free a result set returned by a hash index query
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeResultHashIndex (const TRI_index_t* const idx, 
                              TRI_hash_index_elements_t* const result) {
  TRI_hash_index_t* hashIndex = (TRI_hash_index_t*) idx;

  if (hashIndex->base._unique) {
    HashIndex_freeResult(result);
  }
  else {
    MultiHashIndex_freeResult(result);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief locates entries in the hash index given a JSON list
///
/// @warning who ever calls this function is responsible for destroying
/// TRI_hash_index_elements_t* results
////////////////////////////////////////////////////////////////////////////////

TRI_hash_index_elements_t* TRI_LookupJsonHashIndex (TRI_index_t* idx, TRI_json_t* values) {
  TRI_hash_index_t* hashIndex;
  TRI_hash_index_elements_t* result;
  HashIndexElement element;
  TRI_shaper_t* shaper;
  size_t j;
  
  element.numFields = values->_value._objects._length;
  element.fields    = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_shaped_json_t) * element.numFields, false);

  if (element.fields == NULL) {
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
    LOG_WARNING("out-of-memory in LookupJsonHashIndex");
    return NULL; 
  }  
    
  hashIndex = (TRI_hash_index_t*) idx;
  shaper = hashIndex->base._collection->_shaper;
    
  for (j = 0; j < element.numFields; ++j) {
    TRI_json_t* jsonObject = (TRI_json_t*) (TRI_AtVector(&(values->_value._objects), j));
    TRI_shaped_json_t* shapedObject = TRI_ShapedJsonJson(shaper, jsonObject);

    element.fields[j] = *shapedObject;
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, shapedObject);
  }
  
  if (hashIndex->base._unique) {
    result = HashIndex_find(hashIndex->_hashIndex, &element);
  }
  else {
    result = MultiHashIndex_find(hashIndex->_hashIndex, &element);
  }

  for (j = 0; j < element.numFields; ++j) {
    TRI_DestroyShapedJson(shaper, element.fields + j);
  }

  TRI_Free(TRI_UNKNOWN_MEM_ZONE, element.fields);
  
  return result;  
}

////////////////////////////////////////////////////////////////////////////////
/// @brief locates entries in the hash index given shaped json objects
////////////////////////////////////////////////////////////////////////////////

TRI_hash_index_elements_t* TRI_LookupShapedJsonHashIndex (TRI_index_t* idx, TRI_shaped_json_t** values) {
  TRI_hash_index_t* hashIndex;
  TRI_hash_index_elements_t* result;
  HashIndexElement element;
  size_t j;
  
  hashIndex = (TRI_hash_index_t*) idx;

  element.numFields = hashIndex->_paths._length;
  element.fields    = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_shaped_json_t) * element.numFields, false);

  if (element.fields == NULL) {
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
    LOG_WARNING("out-of-memory in LookupJsonHashIndex");
    return NULL; 
  }  

  for (j = 0; j < element.numFields; ++j) {
    element.fields[j] = *values[j];
  }
    
  if (hashIndex->base._unique) {
    result = HashIndex_find(hashIndex->_hashIndex, &element);
  }
  else {
    result = MultiHashIndex_find(hashIndex->_hashIndex, &element);
  }

  TRI_Free(TRI_UNKNOWN_MEM_ZONE, element.fields);
  
  return result;  
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                              PRIORITY QUEUE INDEX
// -----------------------------------------------------------------------------



// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief helper for priority queue index
////////////////////////////////////////////////////////////////////////////////

static int PriorityQueueIndexHelper (const TRI_priorityqueue_index_t* pqIndex, 
                                     PQIndexElement* pqElement,
                                     const TRI_doc_mptr_t* document,
                                     const TRI_shaped_json_t* shapedDoc) {
  union { void* p; void const* c; } cnv;
  TRI_shaped_json_t shapedObject;
  TRI_shape_access_t* acc;
  size_t j;
  
  // ............................................................................
  // TODO: allow documents to be indexed on other keys (attributes) besides
  // doubles. For now, documents which do have an attribute of double, will be
  // be skipped. We return -2 to indicate this.
  // ............................................................................
  
  if (shapedDoc != NULL) {

    // ..........................................................................
    // Attempting to locate a priority queue entry using TRI_shaped_json_t object. 
    // Use this when we wish to remove a priority queue entry and we only have 
    // the "keys" rather than having the document (from which the keys would follow).
    // ..........................................................................
    
    pqElement->data = NULL;

    

    for (j = 0; j < pqIndex->_paths._length; ++j) {
      TRI_shape_pid_t shape = *((TRI_shape_pid_t*)(TRI_AtVector(&pqIndex->_paths,j)));
      
      // ..........................................................................
      // Determine if document has that particular shape 
      // ..........................................................................

      acc = TRI_ShapeAccessor(pqIndex->base._collection->_shaper, shapedDoc->_sid, shape);

      if (acc == NULL || acc->_shape == NULL) {
        if (acc != NULL) {
          TRI_FreeShapeAccessor(acc);
        }

        TRI_Free(TRI_UNKNOWN_MEM_ZONE, pqElement->fields);
        // the attribute does not exist in the document
        return -1;
      }  
     
     
      // ..........................................................................
      // Determine if the attribute is of the type double -- if not for now
      // ignore this document
      // ..........................................................................

      if (acc->_shape->_type !=  TRI_SHAPE_NUMBER) {
        TRI_FreeShapeAccessor(acc);
        TRI_Free(TRI_UNKNOWN_MEM_ZONE, pqElement->fields);
        return -2;    
      }    
    
    
      // ..........................................................................
      // Extract the field
      // ..........................................................................    

      if (! TRI_ExecuteShapeAccessor(acc, shapedDoc, &shapedObject)) {
        TRI_FreeShapeAccessor(acc);
        TRI_Free(TRI_UNKNOWN_MEM_ZONE, pqElement->fields);

        return TRI_set_errno(TRI_ERROR_INTERNAL);
      }
      
      // ..........................................................................
      // Store the json shaped Object -- this is what will be hashed
      // ..........................................................................    

      pqElement->fields[j] = shapedObject;
      TRI_FreeShapeAccessor(acc);
    }  // end of for loop
  }
  
  else if (document != NULL) {
  
    // ..........................................................................
    // Assign the document to the PQIndexElement structure - so that it can later
    // be retreived.
    // ..........................................................................

    cnv.c = document;
    pqElement->data = cnv.p;
 
    for (j = 0; j < pqIndex->_paths._length; ++j) {
      TRI_shape_pid_t shape = *((TRI_shape_pid_t*)(TRI_AtVector(&pqIndex->_paths,j)));
      
      // ..........................................................................
      // Determine if document has that particular shape 
      // It is not an error if the document DOES NOT have the particular shape
      // ..........................................................................

      acc = TRI_ShapeAccessor(pqIndex->base._collection->_shaper, document->_document._sid, shape);

      if (acc == NULL || acc->_shape == NULL) {
        if (acc != NULL) {
          TRI_FreeShapeAccessor(acc);
        }

        TRI_Free(TRI_UNKNOWN_MEM_ZONE, pqElement->fields);

        return -1;
      }  
      
      
      // ..........................................................................
      // Determine if the attribute is of the type double -- if not for now
      // ignore this document
      // ..........................................................................

      if (acc->_shape->_type !=  TRI_SHAPE_NUMBER) {
        TRI_FreeShapeAccessor(acc);
        TRI_Free(TRI_UNKNOWN_MEM_ZONE, pqElement->fields);
        return -2;    
      }    
    
      
      // ..........................................................................
      // Extract the field
      // ..........................................................................    

      if (! TRI_ExecuteShapeAccessor(acc, &(document->_document), &shapedObject)) {
        TRI_FreeShapeAccessor(acc);
        TRI_Free(TRI_UNKNOWN_MEM_ZONE, pqElement->fields);

        return TRI_set_errno(TRI_ERROR_INTERNAL);
      }
      
      // ..........................................................................
      // Store the field
      // ..........................................................................    

      pqElement->fields[j] = shapedObject;

      TRI_FreeShapeAccessor(acc);
    }  // end of for loop
  }
  
  else {
    return TRI_set_errno(TRI_ERROR_INTERNAL);
  }
  
  return TRI_ERROR_NO_ERROR;
}



////////////////////////////////////////////////////////////////////////////////
/// @brief attempts to add a document to a priority queue index
////////////////////////////////////////////////////////////////////////////////

static int InsertPriorityQueueIndex (TRI_index_t* idx, TRI_doc_mptr_t const* doc) {
  PQIndexElement pqElement;
  TRI_priorityqueue_index_t* pqIndex;
  int res;

  // ............................................................................
  // Obtain the priority queue index structure
  // ............................................................................

  pqIndex = (TRI_priorityqueue_index_t*) idx;

  if (idx == NULL) {
    LOG_WARNING("internal error in InsertPriorityQueueIndex");
    return TRI_set_errno(TRI_ERROR_INTERNAL);
  }

  // ............................................................................
  // Allocate storage to shaped json objects stored as a simple list.
  // These will be used for adding the document to the priority queue
  // ............................................................................
    
  pqElement.numFields  = pqIndex->_paths._length;
  pqElement.fields     = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_shaped_json_t) * pqElement.numFields, false);
  pqElement.collection = pqIndex->base._collection;
  
  
  if (pqElement.fields == NULL) {
    LOG_WARNING("out-of-memory in InsertPriorityQueueIndex");
    return TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
  }  

  res = PriorityQueueIndexHelper(pqIndex, &pqElement, doc, NULL);
  

  // ............................................................................
  // It is possible that this document does not have the necessary attributes
  // (keys) to participate in this index. Skip this document for now.
  // ............................................................................
  
  if (res == -1) {
    return TRI_ERROR_NO_ERROR;
  } 

  
  // ............................................................................
  // It is possible that while we have the correct attribute name, the type is
  // not double. Skip this document for now.  
  // ............................................................................
  
  else if (res == -2) {
    return TRI_ERROR_NO_ERROR;
  } 

  
  // ............................................................................
  // Some other error has occurred - report this error to the calling function
  // ............................................................................
  
  else if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }    
  
  // ............................................................................
  // Attempt to insert document into priority queue index
  // ............................................................................
  
  res = PQIndex_insert(pqIndex->_pqIndex, &pqElement);
  
  if (res == TRI_ERROR_ARANGO_INDEX_PQ_INSERT_FAILED) {
    LOG_WARNING("priority queue insert failure");
  }  
  
  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief describes a priority queue index as a json object
////////////////////////////////////////////////////////////////////////////////

static TRI_json_t* JsonPriorityQueueIndex (TRI_index_t* idx, TRI_doc_collection_t const* collection) {
  TRI_json_t* json;
  TRI_json_t* fields;
  const TRI_shape_path_t* path;
  TRI_priorityqueue_index_t* pqIndex;
  char const** fieldList;
  size_t j;
   
  // ..........................................................................  
  // Recast as a priority queue index
  // ..........................................................................  

  pqIndex = (TRI_priorityqueue_index_t*) idx;

  if (pqIndex == NULL) {
    TRI_set_errno(TRI_ERROR_INTERNAL);
    return NULL;
  }
  
  // ..........................................................................  
  // Allocate sufficent memory for the field list
  // ..........................................................................  

  fieldList = TRI_Allocate( TRI_UNKNOWN_MEM_ZONE, (sizeof(char*) * pqIndex->_paths._length) , false);

  if (fieldList == NULL) {
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
    return NULL;
  }

  // ..........................................................................  
  // Convert the attributes (field list of the hash index) into strings
  // ..........................................................................  

  for (j = 0; j < pqIndex->_paths._length; ++j) {
    TRI_shape_pid_t shape = *((TRI_shape_pid_t*)(TRI_AtVector(&pqIndex->_paths,j)));
    path = collection->_shaper->lookupAttributePathByPid(collection->_shaper, shape);

    if (path == NULL) {
      TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
      TRI_Free(TRI_UNKNOWN_MEM_ZONE, fieldList);
      return NULL;
    }  

    fieldList[j] = ((const char*) path) + sizeof(TRI_shape_path_t) + path->_aidLength * sizeof(TRI_shape_aid_t);
  }

  // ..........................................................................  
  // create json object and fill it
  // ..........................................................................  

  json = TRI_CreateArrayJson(TRI_UNKNOWN_MEM_ZONE);

  if (json == NULL) {
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, fieldList);
    return NULL;
  }

  fields = TRI_CreateListJson(TRI_UNKNOWN_MEM_ZONE);

  for (j = 0; j < pqIndex->_paths._length; ++j) {
    TRI_PushBack3ListJson(TRI_UNKNOWN_MEM_ZONE, fields, TRI_CreateStringCopyJson(TRI_UNKNOWN_MEM_ZONE, fieldList[j]));
  }

  TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, json, "id", TRI_CreateNumberJson(TRI_UNKNOWN_MEM_ZONE, idx->_iid));
  TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, json, "unique", TRI_CreateBooleanJson(TRI_UNKNOWN_MEM_ZONE, pqIndex->base._unique));
  TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, json, "type", TRI_CreateStringCopyJson(TRI_UNKNOWN_MEM_ZONE, TRI_TypeNameIndex(idx)));
  TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, json, "fields", fields);

  TRI_Free(TRI_UNKNOWN_MEM_ZONE, fieldList);
    
  return json;
}


////////////////////////////////////////////////////////////////////////////////
/// @brief removes a priority queue from collection
////////////////////////////////////////////////////////////////////////////////

static void RemoveIndexPriorityQueueIndex (TRI_index_t* idx, TRI_doc_collection_t* collection) {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes a document from a priority queue index
////////////////////////////////////////////////////////////////////////////////

static int RemovePriorityQueueIndex (TRI_index_t* idx, TRI_doc_mptr_t const* doc) {
  PQIndexElement pqElement;
  TRI_priorityqueue_index_t* pqIndex;
  int res;
  
  // ............................................................................
  // Obtain the priority queue index structure
  // ............................................................................

  pqIndex = (TRI_priorityqueue_index_t*) idx;

  if (idx == NULL) {
    LOG_WARNING("internal error in RemovePriorityQueueIndex");
    return TRI_set_errno(TRI_ERROR_INTERNAL);
  }

  // ............................................................................
  // Allocate some memory for the PQIndexElement structure
  // ............................................................................

  pqElement.numFields  = pqIndex->_paths._length;
  pqElement.fields     = TRI_Allocate( TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_shaped_json_t) * pqElement.numFields, false);
  pqElement.collection = pqIndex->base._collection;

  if (pqElement.fields == NULL) {
    LOG_WARNING("out-of-memory in RemovePriorityQueueIndex");
    return TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
  }  
  
  // ..........................................................................
  // Fill the json field list from the document
  // ..........................................................................

  res = PriorityQueueIndexHelper(pqIndex, &pqElement, doc, NULL);
  
  
  // ............................................................................
  // It is possible that this document does not have the necessary attributes
  // (keys) to participate in this index. For now report this as an error. Todo,
  // add its own unique error code so that the calling function can take the
  // appropriate action.
  // ............................................................................
  
  if (res == -1) {
    return TRI_set_errno(TRI_ERROR_INTERNAL);
  }
  
  // ............................................................................
  // It is possible that while we have the correct attribute name, the type is
  // not double. Skip this document for now.  
  // ............................................................................
  
  else if (res == -2) {
    return TRI_ERROR_NO_ERROR;
  } 

  
  else if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }    
  
  // ............................................................................
  // Attempt the removal for unique/non-unique priority queue indexes
  // ............................................................................
  
  res = PQIndex_remove(pqIndex->_pqIndex, &pqElement);
  
  return res;
}


////////////////////////////////////////////////////////////////////////////////
/// @brief updates a document from a priority queue index
////////////////////////////////////////////////////////////////////////////////

static int UpdatePriorityQueueIndex (TRI_index_t* idx,
                                     const TRI_doc_mptr_t* newDoc, 
                                     const TRI_shaped_json_t* oldDoc) {
                             
  // ..........................................................................
  // Note: The oldDoc is represented by the TRI_shaped_json_t rather than by
  //       a TRI_doc_mptr_t object. However for non-unique indexes (such as this
  //       one) we must pass the document shape to the hash remove function.
  // ..........................................................................
  
  union { void* p; void const* c; } cnv;
  PQIndexElement pqElement;
  TRI_priorityqueue_index_t* pqIndex;
  int  res;  
  
  // ............................................................................
  // Obtain the priority queue index structure
  // ............................................................................
  
  pqIndex = (TRI_priorityqueue_index_t*) idx;

  if (idx == NULL) {
    LOG_WARNING("internal error in UpdatePriorityQueueIndex");
    return TRI_ERROR_INTERNAL;
  }

  // ............................................................................
  // Allocate some memory for the HashIndexElement structure
  // ............................................................................

  pqElement.numFields  = pqIndex->_paths._length;
  pqElement.fields     = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_shaped_json_t) * pqElement.numFields, false);
  pqElement.collection = pqIndex->base._collection;

  if (pqElement.fields == NULL) {
    LOG_WARNING("out-of-memory in UpdatePriorityQueueIndex");
    return TRI_ERROR_OUT_OF_MEMORY;
  }  
  

  // ............................................................................
  // Fill in the fields with the values from oldDoc
  // ............................................................................
    
  res = PriorityQueueIndexHelper(pqIndex, &pqElement, NULL, oldDoc);

  if (res == TRI_ERROR_NO_ERROR) {

    cnv.c = newDoc;
    pqElement.data = cnv.p;
      
    // ............................................................................
    // Remove the priority queue index entry and return.
    // ............................................................................

    res = PQIndex_remove(pqIndex->_pqIndex, &pqElement);

    if (res != TRI_ERROR_NO_ERROR) {
      LOG_WARNING("could not remove old document from priority queue index in UpdatePriorityQueueIndex");
    }
  }    

  else {
    LOG_WARNING("could not remove old document from priority queue index in UpdatePriorityQueueIndex");
  }
  
  // ............................................................................
  // Fill the shaped json simple list from the document
  // ............................................................................

  res = PriorityQueueIndexHelper(pqIndex, &pqElement, newDoc, NULL); 

  if (res == -1) {

    // ..........................................................................
    // probably fields do not match  
    // ..........................................................................

    return TRI_ERROR_INTERNAL;
  }    
  
  // ............................................................................
  // It is possible that while we have the correct attribute name, the type is
  // not double. Skip this document for now.  
  // ............................................................................
  
  else if (res == -2) {
    return TRI_ERROR_NO_ERROR;
  } 

  
  else if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }  

  // ............................................................................
  // Attempt to add the priority queue entry from the new doc
  // ............................................................................

  res = PQIndex_insert(pqIndex->_pqIndex, &pqElement);
  
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
/// @brief creates a priority queue index
////////////////////////////////////////////////////////////////////////////////

TRI_index_t* TRI_CreatePriorityQueueIndex (struct TRI_doc_collection_s* collection,
                                           TRI_vector_pointer_t* fields,
                                           TRI_vector_t* paths,
                                           bool unique) {
  TRI_priorityqueue_index_t* pqIndex;
  size_t j;

  if (paths == NULL) {
    LOG_WARNING("Internal error in TRI_CreatePriorityQueueIndex. PriorityQueue index creation failed.");
    return NULL;
  }

  // ...........................................................................
  // TODO: Allow priority queue index to be indexed on more than one field. For
  // now report an error.
  // ...........................................................................
  
  if (paths->_length != 1) {
    LOG_WARNING("Currently only one attribute of the tye 'double' can be used for an index. PriorityQueue index creation failed.");
    return NULL;
  }
  
  pqIndex = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_priorityqueue_index_t), false);

  if (pqIndex == NULL) {
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
    return NULL;
  }
  
  pqIndex->base._iid = TRI_NewTickVocBase();
  pqIndex->base._type = TRI_IDX_TYPE_PRIORITY_QUEUE_INDEX;
  pqIndex->base._collection = collection;
  pqIndex->base._unique = unique;

  pqIndex->base.json = JsonPriorityQueueIndex;
  pqIndex->base.removeIndex = RemoveIndexPriorityQueueIndex;

  pqIndex->base.insert = InsertPriorityQueueIndex;
  pqIndex->base.remove = RemovePriorityQueueIndex;
  pqIndex->base.update = UpdatePriorityQueueIndex;

  // ...........................................................................
  // Copy the contents of the path list vector into a new vector and store this
  // ...........................................................................  

  TRI_InitVector(&pqIndex->_paths, TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_shape_pid_t));

  for (j = 0;  j < paths->_length;  ++j) {
    TRI_shape_pid_t shape = *((TRI_shape_pid_t*)(TRI_AtVector(paths,j)));

    TRI_PushBackVector(&pqIndex->_paths, &shape);
  }
  
  TRI_InitVectorString(&pqIndex->base._fields, TRI_UNKNOWN_MEM_ZONE);

  for (j = 0;  j < fields->_length;  ++j) {
    char const* name = fields->_buffer[j];

    TRI_PushBackVectorString(&pqIndex->base._fields, TRI_DuplicateString(name));
  }

  if (!unique) {
    pqIndex->_pqIndex = PQueueIndex_new();
  }
  else {
    assert(false);
  }  
  
  return &pqIndex->base;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the memory allocated, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyPriorityQueueIndex(TRI_index_t* idx) {
  TRI_priorityqueue_index_t* pqIndex;

  if (idx == NULL) {
    return;
  }
  
  TRI_DestroyVectorString(&idx->_fields);

  pqIndex = (TRI_priorityqueue_index_t*) idx;

  TRI_DestroyVector(&pqIndex->_paths);

  PQueueIndex_free(pqIndex->_pqIndex);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the memory allocated and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreePriorityQueueIndex(TRI_index_t* idx) {
  if (idx == NULL) {
    return;
  }  
  TRI_DestroyPriorityQueueIndex(idx);
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
/// @brief attempts to locate an entry in the priority queue index
/// A priority queue index lookup however only allows the 'top' most element
/// of the queue to be located.
///
/// @warning who ever calls this function is responsible for destroying
/// PQIndexElement* result (which could be null)
////////////////////////////////////////////////////////////////////////////////

PQIndexElements* TRI_LookupPriorityQueueIndex(TRI_index_t* idx, TRI_json_t* parameterList) {

  TRI_priorityqueue_index_t* pqIndex;
  PQIndexElements* result;
  size_t           j;
  uint64_t         numElements;
  TRI_json_t*      jsonObject;  
  
  
  if (idx == NULL) {
    return NULL;
  }

  pqIndex = (TRI_priorityqueue_index_t*) idx;
  
  
  // ..............................................................................
  // The parameter list should consist of exactly one parameter of the type number
  // which represents the first 'n' elements on the priority queue. If the 
  // parameter list is empty, then 'n' defaults to 1.
  // ..............................................................................

  if (parameterList == NULL) {  
    result = PQIndex_top(pqIndex->_pqIndex,1);
    return result;  
  }

  
  if (parameterList->_value._objects._length == 0) {
    result = PQIndex_top(pqIndex->_pqIndex,1);
    return result;  
  }
  
  if (parameterList->_value._objects._length != 1) {
    TRI_set_errno(TRI_ERROR_INTERNAL);
    LOG_WARNING("invalid parameter sent to LookupPriorityQueueIndex");
    return NULL;
  }  
  

  numElements = 0;  
    
  for (j = 0; j < parameterList->_value._objects._length; ++j) {
    jsonObject = (TRI_json_t*) (TRI_AtVector(&(parameterList->_value._objects),j));
    if (jsonObject->_type == TRI_JSON_NUMBER) {
      numElements = jsonObject->_value._number;
      break;
    }  
  }

  if (numElements == 0) {
    TRI_set_errno(TRI_ERROR_INTERNAL);
    LOG_WARNING("invalid parameter sent to LookupPriorityQueueIndex - request ignored");
    return NULL;
  }
  
  result = PQIndex_top(pqIndex->_pqIndex,numElements);
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


// .............................................................................
// Helper function for TRI_LookupSkiplistIndex
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
    case TRI_SL_OR_OPERATOR: {
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
    case TRI_SL_LT_OPERATOR: {
      relationOperator = (TRI_sl_relation_operator_t*)(slOperator);
      relationOperator->_numFields  = relationOperator->_parameters->_value._objects._length;
      relationOperator->_collection = collection;
      relationOperator->_fields     = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_shaped_json_t) * relationOperator->_numFields, false);
      if (relationOperator->_fields != NULL) {
        for (j = 0; j < relationOperator->_numFields; ++j) {
          jsonObject   = (TRI_json_t*) (TRI_AtVector(&(relationOperator->_parameters->_value._objects),j));
          shapedObject = TRI_ShapedJsonJson(collection->_shaper, jsonObject);
          if (shapedObject) {
            relationOperator->_fields[j] = *shapedObject; // shallow copy here is ok
            TRI_Free(TRI_UNKNOWN_MEM_ZONE, shapedObject); // don't require storage anymore
          }
        }
      }  
      else {
        relationOperator->_numFields = 0; // out of memory?
      }        
      break;
    }
  }
}


static void UnFillLookupSLOperator(TRI_sl_operator_t* slOperator) {
  TRI_sl_relation_operator_t* relationOperator;
  TRI_sl_logical_operator_t*  logicalOperator;

  if (slOperator == NULL) {
    return;
  }
  
  switch (slOperator->_type) {
    case TRI_SL_AND_OPERATOR: 
    case TRI_SL_NOT_OPERATOR:
    case TRI_SL_OR_OPERATOR: {
      logicalOperator = (TRI_sl_logical_operator_t*)(slOperator);
      UnFillLookupSLOperator(logicalOperator->_left);
      UnFillLookupSLOperator(logicalOperator->_right);

      TRI_Free(TRI_UNKNOWN_MEM_ZONE, logicalOperator);
      break;
    }
    
    case TRI_SL_EQ_OPERATOR: 
    case TRI_SL_GE_OPERATOR: 
    case TRI_SL_GT_OPERATOR: 
    case TRI_SL_NE_OPERATOR: 
    case TRI_SL_LE_OPERATOR: 
    case TRI_SL_LT_OPERATOR: {
      relationOperator = (TRI_sl_relation_operator_t*)(slOperator);

      if (relationOperator->_parameters != NULL) {
        TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, relationOperator->_parameters);
      }

      if (relationOperator->_fields != NULL) {
        size_t j;

        for (j = 0; j < relationOperator->_numFields; ++j) {
          TRI_shaped_json_t* field = relationOperator->_fields + j;

          if (field->_data.data) {
            TRI_Free(TRI_UNKNOWN_MEM_ZONE, field->_data.data);
          }
        }

        TRI_Free(TRI_UNKNOWN_MEM_ZONE, relationOperator->_fields); // don't require storage anymore
        relationOperator->_fields = NULL;
      }
      relationOperator->_numFields = 0;
      
      TRI_Free(TRI_UNKNOWN_MEM_ZONE, relationOperator);
      break;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief attempts to locate an entry in the skip list index
////////////////////////////////////////////////////////////////////////////////

// .............................................................................
// Note: this function will destroy the passed slOperator before it returns
// Warning: who ever calls this function is responsible for destroying
// TRI_skiplist_iterator_t* results
// .............................................................................


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

  // .........................................................................
  // we must deallocate any memory we allocated in FillLookupSLOperator
  // .........................................................................
  
  UnFillLookupSLOperator(slOperator); 
  
  return result;  
}


////////////////////////////////////////////////////////////////////////////////
/// @brief helper for skiplist methods
////////////////////////////////////////////////////////////////////////////////

static int SkiplistIndexHelper(const TRI_skiplist_index_t* skiplistIndex, 
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
        // TRI_Free(skiplistElement->fields); memory deallocated in the calling procedure
        return TRI_WARNING_ARANGO_INDEX_SKIPLIST_UPDATE_ATTRIBUTE_MISSING;
      }  
      
      
      // ..........................................................................
      // Extract the field
      // ..........................................................................    
      if (! TRI_ExecuteShapeAccessor(acc, shapedDoc, &shapedObject)) {
        TRI_FreeShapeAccessor(acc);
        // TRI_Free(skiplistElement->fields); memory deallocated in the calling procedure
        
        return TRI_ERROR_INTERNAL;
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
        // TRI_Free(skiplistElement->fields); memory deallocated in the calling procedure
        return TRI_WARNING_ARANGO_INDEX_SKIPLIST_DOCUMENT_ATTRIBUTE_MISSING;
      }  
      
      
      // ..........................................................................
      // Extract the field
      // ..........................................................................    
      if (! TRI_ExecuteShapeAccessor(acc, &(document->_document), &shapedObject)) {
        TRI_FreeShapeAccessor(acc);
        // TRI_Free(skiplistElement->fields); memory deallocated in the calling procedure
        return TRI_ERROR_INTERNAL;
      }
      

      // ..........................................................................
      // Store the field
      // ..........................................................................    
      skiplistElement->fields[j] = shapedObject;
      TRI_FreeShapeAccessor(acc);
      
    }  // end of for loop
  }
  
  else {
    return TRI_ERROR_INTERNAL;
  }
  
  return TRI_ERROR_NO_ERROR;
  
} // end of static function SkiplistIndexHelper



////////////////////////////////////////////////////////////////////////////////
/// @brief inserts a document into a skip list index
////////////////////////////////////////////////////////////////////////////////

static int InsertSkiplistIndex (TRI_index_t* idx, TRI_doc_mptr_t const* doc) {

  SkiplistIndexElement skiplistElement;
  TRI_skiplist_index_t* skiplistIndex;
  int res;

  
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
  skiplistElement.fields      = TRI_Allocate( TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_shaped_json_t) * skiplistElement.numFields, false);
  skiplistElement.collection  = skiplistIndex->base._collection;
  
  if (skiplistElement.fields == NULL) {
    LOG_WARNING("out-of-memory in InsertSkiplistIndex");
    return TRI_ERROR_OUT_OF_MEMORY;
  }  
  
  res = SkiplistIndexHelper(skiplistIndex, &skiplistElement, doc, NULL);
    
  
  // ............................................................................
  // most likely the cause of this error is that the 'shape' of the document
  // does not match the 'shape' of the index structure -- so the document
  // is ignored. So not really an error at all.
  // ............................................................................
  
  if (res != TRI_ERROR_NO_ERROR) {
  
    // ..........................................................................
    // Deallocated the memory already allocated to skiplistElement.fields
    // ..........................................................................
      
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, skiplistElement.fields);
    
    
    // ..........................................................................
    // It may happen that the document does not have the necessary attributes to 
    // be included within the hash index, in this case do not report back an error.
    // ..........................................................................
    
    if (res == TRI_WARNING_ARANGO_INDEX_SKIPLIST_DOCUMENT_ATTRIBUTE_MISSING) { 
      return TRI_ERROR_NO_ERROR;
    }
    
    return res;
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
  // Memory which has been allocated to skiplistElement.fields remains allocated
  // contents of which are stored in the hash array.
  // ............................................................................
      
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, skiplistElement.fields);
  
  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief describes a skiplist index as a json object
////////////////////////////////////////////////////////////////////////////////

static TRI_json_t* JsonSkiplistIndex (TRI_index_t* idx, TRI_doc_collection_t const* collection) {
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
  fieldList = TRI_Allocate( TRI_UNKNOWN_MEM_ZONE, (sizeof(char*) * skiplistIndex->_paths._length) , false);
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
      TRI_Free(TRI_UNKNOWN_MEM_ZONE, fieldList);
      return NULL;
    }  
    fieldList[j] = ((const char*) path) + sizeof(TRI_shape_path_t) + path->_aidLength * sizeof(TRI_shape_aid_t);
  }
  

  // ..........................................................................  
  // create json object and fill it
  // ..........................................................................  
  json = TRI_CreateArrayJson(TRI_UNKNOWN_MEM_ZONE);
  if (!json) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, fieldList);
    return NULL;
  }

  fields = TRI_CreateListJson(TRI_UNKNOWN_MEM_ZONE);

  for (j = 0; j < skiplistIndex->_paths._length; ++j) {
    TRI_PushBack3ListJson(TRI_UNKNOWN_MEM_ZONE, fields, TRI_CreateStringCopyJson(TRI_UNKNOWN_MEM_ZONE, fieldList[j]));
  }

  TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, json, "id", TRI_CreateNumberJson(TRI_UNKNOWN_MEM_ZONE, idx->_iid));
  TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, json, "unique", TRI_CreateBooleanJson(TRI_UNKNOWN_MEM_ZONE, skiplistIndex->base._unique));
  TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, json, "type", TRI_CreateStringCopyJson(TRI_UNKNOWN_MEM_ZONE, "skiplist"));
  TRI_Insert3ArrayJson(TRI_UNKNOWN_MEM_ZONE, json, "fields", fields);

  TRI_Free(TRI_UNKNOWN_MEM_ZONE, fieldList);
    
  return json;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes a skip-list index from collection
////////////////////////////////////////////////////////////////////////////////

static void RemoveIndexSkiplistIndex (TRI_index_t* idx, TRI_doc_collection_t* collection) {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes a document from a skiplist index
////////////////////////////////////////////////////////////////////////////////

static int RemoveSkiplistIndex (TRI_index_t* idx, TRI_doc_mptr_t const* doc) {

  SkiplistIndexElement skiplistElement;
  TRI_skiplist_index_t* skiplistIndex;
  int res;
  

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
  skiplistElement.fields     = TRI_Allocate( TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_shaped_json_t) * skiplistElement.numFields, false);
  skiplistElement.collection = skiplistIndex->base._collection;
  
  if (skiplistElement.fields == NULL) {
    LOG_WARNING("out-of-memory in InsertSkiplistIndex");
    return TRI_ERROR_OUT_OF_MEMORY;
  }  
  
  // ..........................................................................
  // Fill the json field list from the document
  // ..........................................................................
  
  
  res = SkiplistIndexHelper(skiplistIndex, &skiplistElement, doc, NULL);
  
  // ..........................................................................
  // Error returned generally implies that the document never was part of the 
  // skiplist index
  // ..........................................................................
  
  if (res != TRI_ERROR_NO_ERROR) {
  
    // ........................................................................
    // Deallocate memory allocated to skiplistElement.fields above
    // ........................................................................
    
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, skiplistElement.fields);
    
    
    // ........................................................................
    // It may happen that the document does not have the necessary attributes
    // to have particpated within the hash index. In this case, we do not
    // report an error to the calling procedure.
    // ........................................................................
    
    if (res == TRI_WARNING_ARANGO_INDEX_SKIPLIST_DOCUMENT_ATTRIBUTE_MISSING) { 
      return TRI_ERROR_NO_ERROR;
    }
    
    return res;  
  }
  
  
  // ............................................................................
  // Attempt the removal for unique skiplist indexes
  // ............................................................................
  
  if (skiplistIndex->base._unique) {
    res = SkiplistIndex_remove(skiplistIndex->_skiplistIndex, &skiplistElement);
  } 
  
  // ............................................................................
  // Attempt the removal for non-unique skiplist indexes
  // ............................................................................
  
  else {
    res = MultiSkiplistIndex_remove(skiplistIndex->_skiplistIndex, &skiplistElement);
  }
  
  // ............................................................................
  // Deallocate memory allocated to skiplistElement.fields above
  // ............................................................................
    
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, skiplistElement.fields);
  
  return res;
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
  skiplistElement.fields     = TRI_Allocate( TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_shaped_json_t) * skiplistElement.numFields, false);
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
  
    res = SkiplistIndexHelper(skiplistIndex, &skiplistElement, NULL, oldDoc);
      
    if (res == TRI_ERROR_NO_ERROR) {
    
      // ............................................................................
      // We must fill the skiplistElement with the value of the document shape -- this
      // is necessary when we attempt to remove non-unique skiplist indexes.
      // ............................................................................
      cnv.c = newDoc; // we are assuming here that the doc ptr does not change
      skiplistElement.data = cnv.p;
      
      
      // ............................................................................
      // Remove the skiplist index entry and return.
      // ............................................................................
      
      res = SkiplistIndex_remove(skiplistIndex->_skiplistIndex, &skiplistElement);
      
      if (res != TRI_ERROR_NO_ERROR) {
      
        // ..........................................................................
        // This error is common, when a document 'update' occurs, but fails
        // due to the fact that a duplicate entry already exists, when the 'rollback'
        // is applied, there is no document to remove -- so we get this error.
        // ..........................................................................
        
        LOG_WARNING("could not remove existing document from skiplist index in UpdateSkiplistIndex");
      }
      
    }    

    // ..............................................................................
    // Here we are assuming that the existing document could not be removed, because
    // the doc did not have the correct attributes. TODO: do not make this assumption.
    // ..............................................................................

    else if (res == TRI_WARNING_ARANGO_INDEX_SKIPLIST_UPDATE_ATTRIBUTE_MISSING) {
    }
    
    // ..............................................................................
    // some other error
    // ..............................................................................
    
    else {
      LOG_WARNING("existing document was not removed from skiplist index in UpdateSkiplistIndex");
    }
    
    
    // ............................................................................
    // Fill the json simple list from the document
    // ............................................................................
    
    res = SkiplistIndexHelper(skiplistIndex, &skiplistElement, newDoc, NULL);
    
    if (res != TRI_ERROR_NO_ERROR) {
    
      // ..........................................................................
      // Deallocated memory given to skiplistElement.fields
      // ..........................................................................
    
      TRI_Free(TRI_UNKNOWN_MEM_ZONE, skiplistElement.fields);
      
      if (res == TRI_WARNING_ARANGO_INDEX_SKIPLIST_DOCUMENT_ATTRIBUTE_MISSING) {
      
        // ........................................................................
        // probably fields do not match. 
        // ........................................................................

        return TRI_ERROR_NO_ERROR;
      }
    
      return res;
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

    res = SkiplistIndexHelper(skiplistIndex, &skiplistElement, NULL, oldDoc);
    
    if (res == TRI_ERROR_NO_ERROR) {
    
      // ............................................................................
      // We must fill the skiplistElement with the value of the document shape -- this
      // is necessary when we attempt to remove non-unique skiplist indexes.
      // ............................................................................
      
      cnv.c = newDoc;
      skiplistElement.data = cnv.p;
      
      
      // ............................................................................
      // Remove the skiplist index entry and return.
      // ............................................................................
      
      res = MultiSkiplistIndex_remove(skiplistIndex->_skiplistIndex, &skiplistElement);
      
      if (res != TRI_ERROR_NO_ERROR) {
        LOG_WARNING("could not remove old document from (non-unique) skiplist index  UpdateSkiplistIndex");
      }
      
    }    

    else if (res == TRI_WARNING_ARANGO_INDEX_SKIPLIST_UPDATE_ATTRIBUTE_MISSING) {
    }
    
    // ..............................................................................
    // some other error
    // ..............................................................................
    
    else {
      LOG_WARNING("existing document was not removed from (non-unique) skiplist index in UpdateSkiplistIndex");
    }


    
    // ............................................................................
    // Fill the shaped json simple list from the document
    // ............................................................................
    
    res = SkiplistIndexHelper(skiplistIndex, &skiplistElement, newDoc, NULL);
    
    if (res != TRI_ERROR_NO_ERROR) {
    
      // ..........................................................................
      // Deallocated memory given to skiplistElement.fields
      // ..........................................................................
    
      TRI_Free(TRI_UNKNOWN_MEM_ZONE, skiplistElement.fields);
      
      if (res == TRI_WARNING_ARANGO_INDEX_SKIPLIST_DOCUMENT_ATTRIBUTE_MISSING) {
      
        // ........................................................................
        // probably fields do not match. 
        // ........................................................................

        return TRI_ERROR_NO_ERROR;
      }
    
      return res;
    }    


    // ............................................................................
    // Attempt to add the skiplist entry from the new doc
    // ............................................................................
    res = MultiSkiplistIndex_insert(skiplistIndex->_skiplistIndex, &skiplistElement);
    
  }
  
  
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, skiplistElement.fields);  
  
  return res;
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

  skiplistIndex = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_skiplist_index_t), false);
  if (!skiplistIndex) {
    return NULL;
  }
  
  skiplistIndex->base._iid = TRI_NewTickVocBase();
  skiplistIndex->base._type = TRI_IDX_TYPE_SKIPLIST_INDEX;
  skiplistIndex->base._collection = collection;
  skiplistIndex->base._unique = unique;

  skiplistIndex->base.json = JsonSkiplistIndex;
  skiplistIndex->base.removeIndex = RemoveIndexSkiplistIndex;

  skiplistIndex->base.insert = InsertSkiplistIndex;
  skiplistIndex->base.remove = RemoveSkiplistIndex;
  skiplistIndex->base.update = UpdateSkiplistIndex;
  
  // ...........................................................................
  // Copy the contents of the shape list vector into a new vector and store this
  // ...........................................................................  

  TRI_InitVector(&skiplistIndex->_paths, TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_shape_pid_t));

  for (j = 0;  j < paths->_length;  ++j) {
    TRI_shape_pid_t shape = *((TRI_shape_pid_t*)(TRI_AtVector(paths,j)));

    TRI_PushBackVector(&skiplistIndex->_paths, &shape);
  }

  TRI_InitVectorString(&skiplistIndex->base._fields, TRI_UNKNOWN_MEM_ZONE);

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
  
  LOG_TRACE("destroying skiplist index");
  TRI_DestroyVectorString(&idx->_fields);

  sl = (TRI_skiplist_index_t*) idx;
  TRI_DestroyVector(&sl->_paths);

  SkiplistIndex_free(sl->_skiplistIndex);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the memory allocated and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeSkiplistIndex (TRI_index_t* idx) {
  if (idx == NULL) {
    return;
  }  
  TRI_DestroySkiplistIndex(idx);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, idx);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
