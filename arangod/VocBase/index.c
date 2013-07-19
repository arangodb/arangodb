////////////////////////////////////////////////////////////////////////////////
/// @brief index
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
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "index.h"

#include "BasicsC/conversions.h"
#include "BasicsC/files.h"
#include "BasicsC/json.h"
#include "BasicsC/linked-list.h"
#include "BasicsC/logging.h"
#include "BasicsC/string-buffer.h"
#include "BasicsC/tri-strings.h"
#include "BasicsC/utf8-helper.h"
#include "CapConstraint/cap-constraint.h"
#include "GeoIndex/geo-index.h"
#include "FulltextIndex/fulltext-index.h"
#include "FulltextIndex/fulltext-wordlist.h"
#include "GeoIndex/geo-index.h"
#include "HashIndex/hash-index.h"
#include "ShapedJson/shape-accessor.h"
#include "ShapedJson/shaped-json.h"
#include "VocBase/document-collection.h"
#include "VocBase/edge-collection.h"
#include "VocBase/replication-logger.h"
#include "VocBase/voc-shaper.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                             INDEX
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief initialise basic index properties
////////////////////////////////////////////////////////////////////////////////

void TRI_InitIndex (TRI_index_t* idx, 
                    const TRI_idx_type_e type, 
                    struct TRI_primary_collection_s* primary,
                    bool unique,
                    bool needsFullCoverage) {
  assert(idx != NULL);

  idx->_iid               = TRI_NewTickVocBase();
  idx->_type              = type;
  idx->_collection        = primary;
  idx->_unique            = unique;
  idx->_needsFullCoverage = needsFullCoverage;
  
  // init common functions
  idx->removeIndex       = NULL;
  idx->cleanup           = NULL;

  idx->postInsert        = NULL;

  idx->beginTransaction  = NULL;
  idx->abortTransaction  = NULL;
  idx->commitTransaction = NULL;

  LOG_TRACE("initialising index of type %s", idx->typeName(idx));
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

    case TRI_IDX_TYPE_BITARRAY_INDEX:
      TRI_FreeBitarrayIndex(idx);
      break;

    case TRI_IDX_TYPE_HASH_INDEX:
      TRI_FreeHashIndex(idx);
      break;

    case TRI_IDX_TYPE_EDGE_INDEX:
      TRI_FreeEdgeIndex(idx);
      break;

    case TRI_IDX_TYPE_SKIPLIST_INDEX:
      TRI_FreeSkiplistIndex(idx);
      break;

    case TRI_IDX_TYPE_FULLTEXT_INDEX:
      TRI_FreeFulltextIndex(idx);
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

bool TRI_RemoveIndexFile (TRI_primary_collection_t* collection, TRI_index_t* idx) {
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

    TRI_FreeString(TRI_CORE_MEM_ZONE, number);
    LOG_ERROR("out of memory when creating index name");
    return false;
  }

  filename = TRI_Concatenate2File(collection->base._directory, name);

  if (filename == NULL) {
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);

    TRI_FreeString(TRI_CORE_MEM_ZONE, number);
    TRI_FreeString(TRI_CORE_MEM_ZONE, name);
    LOG_ERROR("out of memory when creating index filename");
    return false;
  }

  TRI_FreeString(TRI_CORE_MEM_ZONE, name);
  TRI_FreeString(TRI_CORE_MEM_ZONE, number);

  res = TRI_UnlinkFile(filename);
  TRI_FreeString(TRI_CORE_MEM_ZONE, filename);

  if (res != TRI_ERROR_NO_ERROR) {
    LOG_ERROR("cannot remove index definition: %s", TRI_last_error());
    return false;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief saves an index
////////////////////////////////////////////////////////////////////////////////

int TRI_SaveIndex (TRI_primary_collection_t* collection, 
                   TRI_index_t* idx) {
  TRI_json_t* json;
  char* filename;
  char* name;
  char* number;
  bool ok;

  // convert into JSON
  json = idx->json(idx, collection);

  if (json == NULL) {
    LOG_TRACE("cannot save index definition: index cannot be jsonified");
    return TRI_set_errno(TRI_ERROR_INTERNAL);
  }

  // construct filename
  number   = TRI_StringUInt64(idx->_iid);
  name     = TRI_Concatenate3String("index-", number, ".json");
  filename = TRI_Concatenate2File(collection->base._directory, name);

  TRI_FreeString(TRI_CORE_MEM_ZONE, name);
  TRI_FreeString(TRI_CORE_MEM_ZONE, number);

  // and save
  ok = TRI_SaveJson(filename, json, collection->base._vocbase->_forceSyncProperties);
  
  TRI_FreeString(TRI_CORE_MEM_ZONE, filename);

  if (! ok) {
    LOG_ERROR("cannot save index definition: %s", TRI_last_error());
    TRI_FreeJson(TRI_CORE_MEM_ZONE, json);

    return TRI_errno();
  }

#ifdef TRI_ENABLE_REPLICATION
  TRI_LogCreateIndexReplication(collection->base._vocbase, collection->base._info._cid, idx->_iid, json);
#endif

  TRI_FreeJson(TRI_CORE_MEM_ZONE, json);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up an index identifier
////////////////////////////////////////////////////////////////////////////////

TRI_index_t* TRI_LookupIndex (TRI_primary_collection_t* collection, TRI_idx_iid_t iid) {
  TRI_document_collection_t* doc;
  TRI_index_t* idx;
  size_t i;

  if (! TRI_IS_DOCUMENT_COLLECTION(collection->base._info._type)) {
    TRI_set_errno(TRI_ERROR_ARANGO_UNKNOWN_COLLECTION_TYPE);
    return NULL;
  }

  doc = (TRI_document_collection_t*) collection;

  for (i = 0;  i < doc->_allIndexes._length;  ++i) {
    idx = doc->_allIndexes._buffer[i];

    if (idx->_iid == iid) {
      return idx;
    }
  }

  TRI_set_errno(TRI_ERROR_ARANGO_NO_INDEX);

  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a basic index description as JSON
/// this only contains the common index fields and needs to be extended by the
/// specialised index 
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* TRI_JsonIndex (TRI_memory_zone_t* zone, TRI_index_t* idx) {
  TRI_json_t* json;

  json = TRI_CreateArrayJson(zone);

  if (json != NULL) {
    char* number;
    
    number = TRI_StringUInt64(idx->_iid);
    TRI_Insert3ArrayJson(zone, json, "id", TRI_CreateStringCopyJson(zone, number));
    TRI_Insert3ArrayJson(zone, json, "type", TRI_CreateStringCopyJson(zone, idx->typeName(idx)));
    TRI_Insert3ArrayJson(zone, json, "unique", TRI_CreateBooleanJson(zone, idx->_unique));

    TRI_FreeString(TRI_CORE_MEM_ZONE, number);
  }

  return json;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a result set returned by a hash index query
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyIndexResult (TRI_index_result_t* result) {
  if (result->_documents != NULL) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, result->_documents);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief copies a path vector
////////////////////////////////////////////////////////////////////////////////

void TRI_CopyPathVector (TRI_vector_t* dst, TRI_vector_t* src) {
  size_t j;

  TRI_InitVector(dst, TRI_CORE_MEM_ZONE, sizeof(TRI_shape_pid_t));

  for (j = 0;  j < src->_length;  ++j) {
    TRI_shape_pid_t shape = *((TRI_shape_pid_t*)(TRI_AtVector(src,j)));

    TRI_PushBackVector(dst, &shape);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief copies all pointers from a vector
////////////////////////////////////////////////////////////////////////////////

void TRI_CopyFieldsVector (TRI_vector_string_t* dst, TRI_vector_pointer_t* src) {
  TRI_InitVectorString(dst, TRI_CORE_MEM_ZONE);

  TRI_ClearVectorString(dst);

  if (0 < src->_length) {
    char** qtr;
    void** ptr;
    void** end;

    TRI_ResizeVectorString (dst, src->_length);

    ptr = src->_buffer;
    end = src->_buffer + src->_length;
    qtr = dst->_buffer;

    for (;  ptr < end;  ++ptr, ++qtr) {
      *qtr = TRI_DuplicateString(*ptr);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a path vector into a field list
///
/// Note that you must free the field list itself, but not the fields. The
/// belong to the shaper.
////////////////////////////////////////////////////////////////////////////////

char const** TRI_FieldListByPathList (TRI_shaper_t* shaper,
                                      TRI_vector_t* paths) {
  char const** fieldList;
  size_t j;

  // .............................................................................
  // Allocate sufficent memory for the field list
  // .............................................................................

  fieldList = TRI_Allocate(TRI_CORE_MEM_ZONE, (sizeof(char const*) * paths->_length), false);

  // ..........................................................................  
  // Convert the attributes (field list of the hash index) into strings
  // ..........................................................................  

  for (j = 0;  j < paths->_length;  ++j) {
    TRI_shape_pid_t shape = *((TRI_shape_pid_t*)(TRI_AtVector(paths, j)));
    TRI_shape_path_t const* path = shaper->lookupAttributePathByPid(shaper, shape);

    if (path == NULL) {
      TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
      TRI_Free(TRI_CORE_MEM_ZONE, fieldList);
      return NULL;
    }

    fieldList[j] = ((const char*) path) + sizeof(TRI_shape_path_t) + path->_aidLength * sizeof(TRI_shape_aid_t);
  }

  return fieldList;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                     PRIMARY INDEX
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief return the index type name
////////////////////////////////////////////////////////////////////////////////

static const char* TypeNamePrimary (TRI_index_t const* idx) {
  return "primary";
}

////////////////////////////////////////////////////////////////////////////////
/// @brief insert methods does nothing
////////////////////////////////////////////////////////////////////////////////

static int InsertPrimary (TRI_index_t* idx, 
                          TRI_doc_mptr_t const* doc, 
                          const bool isRollback) {
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief remove methods does nothing
////////////////////////////////////////////////////////////////////////////////

static int RemovePrimary (TRI_index_t* idx, 
                          TRI_doc_mptr_t const* doc,
                          const bool isRollback) {
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief JSON description of a primary index
////////////////////////////////////////////////////////////////////////////////

static TRI_json_t* JsonPrimary (TRI_index_t* idx, TRI_primary_collection_t const* collection) {
  TRI_json_t* json;
  TRI_json_t* fields;

  json = TRI_JsonIndex(TRI_CORE_MEM_ZONE, idx);

  fields = TRI_CreateListJson(TRI_CORE_MEM_ZONE);
  TRI_PushBack3ListJson(TRI_CORE_MEM_ZONE, fields, TRI_CreateStringCopyJson(TRI_CORE_MEM_ZONE, "_id"));
  TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, json, "fields", fields);

  return json;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief create the primary index
////////////////////////////////////////////////////////////////////////////////

TRI_index_t* TRI_CreatePrimaryIndex (struct TRI_primary_collection_s* primary) {
  TRI_index_t* idx;
  char* id;

  // create primary index
  idx = TRI_Allocate(TRI_CORE_MEM_ZONE, sizeof(TRI_index_t), false);

  id = TRI_DuplicateStringZ(TRI_CORE_MEM_ZONE, "_id");
  TRI_InitVectorString(&idx->_fields, TRI_CORE_MEM_ZONE);
  TRI_PushBackVectorString(&idx->_fields, id);

  idx->typeName = TypeNamePrimary;
  TRI_InitIndex(idx, TRI_IDX_TYPE_PRIMARY_INDEX, primary, true, true);

  // override iid
  idx->_iid     = 0;

  idx->json     = JsonPrimary;
  idx->insert   = InsertPrimary;
  idx->remove   = RemovePrimary;

  return idx;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a primary index, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyPrimaryIndex (TRI_index_t* idx) {
  LOG_TRACE("destroying primary index");

  TRI_DestroyVectorString(&idx->_fields);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free a primary index
////////////////////////////////////////////////////////////////////////////////

void TRI_FreePrimaryIndex (TRI_index_t* idx) {
 TRI_DestroyPrimaryIndex(idx);
 TRI_Free(TRI_CORE_MEM_ZONE, idx);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                        EDGE INDEX
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief hashes an edge header
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashElementEdge (TRI_multi_pointer_t* array, void const* data) {
  TRI_edge_header_t const* h;
  uint64_t hash[3];
  char* key;

  h = data;

  if (h->_mptr != NULL) {
    key = ((char*) h->_mptr->_data) + h->_searchKey._offsetKey;
  }
  else {
    key = h->_searchKey._key;
  }

  // only include directional bits for hashing, exclude special bits
  hash[0] = (uint64_t) (h->_flags & TRI_EDGE_BITS_DIRECTION);
  hash[1] = h->_cid;
  hash[2] = TRI_FnvHashString(key);

  return TRI_FnvHashPointer(hash, sizeof(hash));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if key and element match
////////////////////////////////////////////////////////////////////////////////

static bool IsEqualKeyEdge (TRI_multi_pointer_t* array,
                            void const* left,
                            void const* right) {
  TRI_edge_header_t const* l;
  TRI_edge_header_t const* r;
  const char* lKey;
  const char* rKey;

  l = left;
  r = right;

  if (l->_mptr != NULL) {
    lKey = ((char*) ((TRI_doc_edge_key_marker_t const*) l->_mptr->_data)) + l->_searchKey._offsetKey;
  }
  else {
    lKey = l->_searchKey._key;
  }

  if (r->_mptr != NULL) {
    rKey = ((char*) ((TRI_doc_edge_key_marker_t const*) r->_mptr->_data)) + r->_searchKey._offsetKey;
  }
  else {
    rKey = r->_searchKey._key;
  }

  // only include directional flags, exclude special bits
  return ((l->_flags & TRI_EDGE_BITS_DIRECTION) == (r->_flags & TRI_EDGE_BITS_DIRECTION)) &&
         (l->_cid == r->_cid) &&
         (strcmp(lKey, rKey) == 0);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks for elements are equal
////////////////////////////////////////////////////////////////////////////////

static bool IsEqualElementEdge (TRI_multi_pointer_t* array,
                                void const* left,
                                void const* right) {
  TRI_edge_header_t const* l;
  TRI_edge_header_t const* r;
  const char* lKey;
  const char* rKey;

  l = left;
  r = right;

  if (l->_mptr != NULL) {
    lKey = ((char*) ((TRI_doc_edge_key_marker_t const*) l->_mptr->_data)) + l->_searchKey._offsetKey;
  }
  else {
    lKey = l->_searchKey._key;
  }

  if (r->_mptr != NULL) {
    rKey = ((char*) ((TRI_doc_edge_key_marker_t const*) r->_mptr->_data)) + r->_searchKey._offsetKey;
  }
  else {
    rKey = r->_searchKey._key;
  }

  // only include directional flags, exclude special bits
  return (l->_mptr == r->_mptr) &&
         ((l->_flags & TRI_EDGE_BITS_DIRECTION) == (r->_flags & TRI_EDGE_BITS_DIRECTION)) &&
         (l->_cid == r->_cid) &&
         (strcmp(lKey, rKey) == 0);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the index type name
////////////////////////////////////////////////////////////////////////////////

static const char* TypeNameEdge (TRI_index_t const* idx) {
  return "edge";
}

////////////////////////////////////////////////////////////////////////////////
/// @brief insert method for edges
////////////////////////////////////////////////////////////////////////////////

static int InsertEdge (TRI_index_t* idx, 
                       TRI_doc_mptr_t const* mptr,
                       const bool isRollback) {
  TRI_edge_header_t* entryIn;
  TRI_edge_header_t* entryOut;
  TRI_doc_edge_key_marker_t const* edge;
  bool isReflexive;

  TRI_multi_pointer_t* edgesIndex = &(((TRI_edge_index_t*) idx)->_edges);

  edge = mptr->_data;

  // is the edge self-reflexive (_from & _to are identical)?
  isReflexive = (edge->_toCid == edge->_fromCid && strcmp(((char*) edge) + edge->_offsetToKey, ((char*) edge) + edge->_offsetFromKey) == 0);

  // allocate all edge headers and return early if memory allocation fails

  entryIn = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_edge_header_t), false);

  if (entryIn == NULL) {
    return TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
  }

  entryOut = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_edge_header_t), false);

  if (entryOut == NULL) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, entryIn);
    return TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
  }

  // first slot: IN
  entryIn->_mptr = mptr;
  entryIn->_flags = TRI_FlagsEdge(TRI_EDGE_IN, isReflexive);
  entryIn->_cid = edge->_toCid;
  entryIn->_searchKey._offsetKey = edge->_offsetToKey;
  TRI_InsertElementMultiPointer(edgesIndex, entryIn, true, isRollback);

  // second slot: OUT
  entryOut->_mptr = mptr;
  entryOut->_flags = TRI_FlagsEdge(TRI_EDGE_OUT, isReflexive);
  entryOut->_cid = edge->_fromCid;
  entryOut->_searchKey._offsetKey = edge->_offsetFromKey;
  TRI_InsertElementMultiPointer(edgesIndex, entryOut, true, isRollback);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief remove an edge
////////////////////////////////////////////////////////////////////////////////

static int RemoveEdge (TRI_index_t* idx, 
                       TRI_doc_mptr_t const* doc,
                       const bool isRollback) {
  TRI_edge_header_t entry;
  TRI_edge_header_t* old;
  TRI_doc_edge_key_marker_t const* edge;
  TRI_multi_pointer_t* edgesIndex = &(((TRI_edge_index_t*) idx)->_edges);

  edge = doc->_data;

  entry._mptr = doc;

  // OUT
  // we do not need to free the OUT element
  entry._flags = TRI_LookupFlagsEdge(TRI_EDGE_OUT);
  entry._cid = edge->_fromCid;
  entry._searchKey._offsetKey = edge->_offsetFromKey;
  old = TRI_RemoveElementMultiPointer(edgesIndex, &entry);

  // the pointer to the OUT element is also the memory pointer we need to free
  if (old != NULL) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, old);
  }

  // IN
  entry._flags = TRI_LookupFlagsEdge(TRI_EDGE_IN);
  entry._cid = edge->_toCid;
  entry._searchKey._offsetKey = edge->_offsetToKey;
  old = TRI_RemoveElementMultiPointer(edgesIndex, &entry);

  // the pointer to the IN element is also the memory pointer we need to free
  if (old != NULL) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, old);
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief JSON description of edge index
////////////////////////////////////////////////////////////////////////////////

static TRI_json_t* JsonEdge (TRI_index_t* idx, TRI_primary_collection_t const* primary) {
  TRI_json_t* json;
  TRI_json_t* fields;

  json = TRI_JsonIndex(TRI_CORE_MEM_ZONE, idx);

  fields = TRI_CreateListJson(TRI_CORE_MEM_ZONE);
  TRI_PushBack3ListJson(TRI_CORE_MEM_ZONE, fields, TRI_CreateStringCopyJson(TRI_CORE_MEM_ZONE, TRI_VOC_ATTRIBUTE_FROM));
  TRI_PushBack3ListJson(TRI_CORE_MEM_ZONE, fields, TRI_CreateStringCopyJson(TRI_CORE_MEM_ZONE, TRI_VOC_ATTRIBUTE_TO));
  TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, json, "fields", fields);

  return json;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief create the edge index
////////////////////////////////////////////////////////////////////////////////

TRI_index_t* TRI_CreateEdgeIndex (struct TRI_primary_collection_s* primary) {
  TRI_edge_index_t* edgeIndex;
  TRI_index_t* idx;
  char* id;
  int res;

  // create index
  edgeIndex = TRI_Allocate(TRI_CORE_MEM_ZONE, sizeof(TRI_edge_index_t), false);

  if (edgeIndex == NULL) {
    return NULL;
  }
  
  res = TRI_InitMultiPointer(&edgeIndex->_edges,
                             TRI_UNKNOWN_MEM_ZONE,
                             HashElementEdge,
                             HashElementEdge,
                             IsEqualKeyEdge,
                             IsEqualElementEdge);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_Free(TRI_CORE_MEM_ZONE, edgeIndex);

    return NULL;
  }


  idx = &edgeIndex->base;

  TRI_InitVectorString(&idx->_fields, TRI_CORE_MEM_ZONE);
  id = TRI_DuplicateStringZ(TRI_CORE_MEM_ZONE, TRI_VOC_ATTRIBUTE_FROM);
  TRI_PushBackVectorString(&idx->_fields, id);
 
  idx->typeName = TypeNameEdge;
  TRI_InitIndex(idx, TRI_IDX_TYPE_EDGE_INDEX, primary, false, true); 

  idx->json     = JsonEdge;
  idx->insert   = InsertEdge;
  idx->remove   = RemoveEdge;

  return idx;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys the edge index, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyEdgeIndex (TRI_index_t* idx) {
  TRI_edge_index_t* edgesIndex;
  size_t i, n;

  edgesIndex = (TRI_edge_index_t*) idx;

  LOG_TRACE("destroying edge index");

  // free all elements in the edges index
  n = edgesIndex->_edges._nrAlloc;

  for (i = 0; i < n; ++i) {
    TRI_edge_header_t* element = edgesIndex->_edges._table[i];
    if (element != NULL) {
      TRI_Free(TRI_UNKNOWN_MEM_ZONE, element);
    }
  }

  TRI_DestroyMultiPointer(&edgesIndex->_edges);

  TRI_DestroyVectorString(&idx->_fields);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the edge index
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeEdgeIndex (TRI_index_t* idx) {
  TRI_DestroyEdgeIndex(idx);
  TRI_Free(TRI_CORE_MEM_ZONE, idx);
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
                                     TRI_pq_index_element_t* pqElement,
                                     const TRI_doc_mptr_t* document) {
  TRI_shaped_json_t shapedObject;
  TRI_shape_access_t const* acc;
  size_t j;
  
  // ..........................................................................
  // Assign the document to the TRI_pq_index_element_t structure - so that it can later
  // be retreived.
  // ..........................................................................

  pqElement->_document = CONST_CAST(document);
 
  for (j = 0; j < pqIndex->_paths._length; ++j) {
    TRI_shaped_json_t shapedJson;
    TRI_shape_pid_t shape = *((TRI_shape_pid_t*)(TRI_AtVector(&pqIndex->_paths,j)));
      
    // ..........................................................................
    // Determine if document has that particular shape 
    // It is not an error if the document DOES NOT have the particular shape
    // ..........................................................................

    TRI_EXTRACT_SHAPED_JSON_MARKER(shapedJson, document->_data);
      
    acc = TRI_FindAccessorVocShaper(pqIndex->base._collection->_shaper, shapedJson._sid, shape);

    if (acc == NULL || acc->_shape == NULL) {
      TRI_Free(TRI_UNKNOWN_MEM_ZONE, pqElement->_subObjects);
      return -1;
    }  
      
    // ..........................................................................
    // Extract the field
    // ..........................................................................    

    if (! TRI_ExecuteShapeAccessor(acc, &shapedJson, &shapedObject)) {
      TRI_Free(TRI_UNKNOWN_MEM_ZONE, pqElement->_subObjects);
      return TRI_set_errno(TRI_ERROR_INTERNAL);
    }
      
    // ..........................................................................
    // Store the field
    // ..........................................................................    

    pqElement->_subObjects[j]._sid = shapedObject._sid;
    pqElement->_subObjects[j]._length = shapedObject._data.length;
    pqElement->_subObjects[j]._offset = ((char const*) shapedObject._data.data) - ((char const*) document->_data);
  }
  
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief attempts to add a document to a priority queue index
////////////////////////////////////////////////////////////////////////////////

static int InsertPriorityQueueIndex (TRI_index_t* idx, 
                                     TRI_doc_mptr_t const* doc,
                                     const bool isRollback) {
  TRI_pq_index_element_t pqElement;
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
    
  pqElement.numFields   = pqIndex->_paths._length;
  pqElement._subObjects = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_shaped_sub_t) * pqElement.numFields, false);
  pqElement.collection  = pqIndex->base._collection;
  
  
  if (pqElement._subObjects == NULL) {
    LOG_WARNING("out-of-memory in InsertPriorityQueueIndex");
    return TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
  }  

  res = PriorityQueueIndexHelper(pqIndex, &pqElement, doc);
  

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
/// @brief return the index type name
////////////////////////////////////////////////////////////////////////////////

static const char* TypeNamePriorityQueueIndex (TRI_index_t const* idx) {
  return "priorityqueue";
}

////////////////////////////////////////////////////////////////////////////////
/// @brief describes a priority queue index as a json object
////////////////////////////////////////////////////////////////////////////////

static TRI_json_t* JsonPriorityQueueIndex (TRI_index_t* idx, TRI_primary_collection_t const* primary) {
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

  fieldList = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, (sizeof(char*) * pqIndex->_paths._length) , false);

  if (fieldList == NULL) {
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
    return NULL;
  }

  // ..........................................................................  
  // Convert the attributes (field list of the hash index) into strings
  // ..........................................................................  

  for (j = 0; j < pqIndex->_paths._length; ++j) {
    TRI_shape_pid_t shape = *((TRI_shape_pid_t*)(TRI_AtVector(&pqIndex->_paths,j)));
    path = primary->_shaper->lookupAttributePathByPid(primary->_shaper, shape);

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

  json = TRI_JsonIndex(TRI_CORE_MEM_ZONE, idx);

  fields = TRI_CreateListJson(TRI_CORE_MEM_ZONE);
  for (j = 0; j < pqIndex->_paths._length; ++j) {
    TRI_PushBack3ListJson(TRI_CORE_MEM_ZONE, fields, TRI_CreateStringCopyJson(TRI_CORE_MEM_ZONE, fieldList[j]));
  }
  TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, json, "fields", fields);

  TRI_Free(TRI_UNKNOWN_MEM_ZONE, fieldList);

  return json;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes a document from a priority queue index
////////////////////////////////////////////////////////////////////////////////

static int RemovePriorityQueueIndex (TRI_index_t* idx, 
                                     TRI_doc_mptr_t const* doc,
                                     const bool isRollback) {
  TRI_priorityqueue_index_t* pqIndex;
  
  // ............................................................................
  // Obtain the priority queue index structure
  // ............................................................................

  pqIndex = (TRI_priorityqueue_index_t*) idx;

  if (idx == NULL) {
    LOG_WARNING("internal error in RemovePriorityQueueIndex");
    return TRI_set_errno(TRI_ERROR_INTERNAL);
  }

  return PQIndex_remove(pqIndex->_pqIndex, doc);
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

TRI_index_t* TRI_CreatePriorityQueueIndex (struct TRI_primary_collection_s* primary,
                                           TRI_vector_pointer_t* fields,
                                           TRI_vector_t* paths,
                                           bool unique) {
  TRI_priorityqueue_index_t* pqIndex;
  TRI_index_t* idx;
  size_t j;
  
  if (unique) {
    LOG_ERROR("non-unique priority queue indexes are unsupported");
    return NULL;
  }

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

  pqIndex = TRI_Allocate(TRI_CORE_MEM_ZONE, sizeof(TRI_priorityqueue_index_t), false);
  idx = &pqIndex->base;
  
  idx->typeName = TypeNamePriorityQueueIndex;
  TRI_InitIndex(idx, TRI_IDX_TYPE_PRIORITY_QUEUE_INDEX, primary, unique, true);
   
  idx->json     = JsonPriorityQueueIndex;
  idx->insert   = InsertPriorityQueueIndex;
  idx->remove   = RemovePriorityQueueIndex;

  // ...........................................................................
  // Copy the contents of the path list vector into a new vector and store this
  // ...........................................................................

  TRI_InitVector(&pqIndex->_paths, TRI_CORE_MEM_ZONE, sizeof(TRI_shape_pid_t));

  for (j = 0;  j < paths->_length;  ++j) {
    TRI_shape_pid_t shape = *((TRI_shape_pid_t*)(TRI_AtVector(paths,j)));

    TRI_PushBackVector(&pqIndex->_paths, &shape);
  }

  TRI_InitVectorString(&idx->_fields, TRI_CORE_MEM_ZONE);

  for (j = 0;  j < fields->_length;  ++j) {
    char const* name = fields->_buffer[j];
    char* copy = TRI_DuplicateStringZ(TRI_CORE_MEM_ZONE, name);

    TRI_PushBackVectorString(&idx->_fields, copy);
  }

  pqIndex->_pqIndex = PQueueIndex_new();

  return idx;
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
  TRI_Free(TRI_CORE_MEM_ZONE, idx);
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

PQIndexElements* TRI_LookupPriorityQueueIndex (TRI_index_t* idx,
                                               size_t n) {
  TRI_priorityqueue_index_t* pqIndex;
  
  if (idx == NULL) {
    return NULL;
  }

  pqIndex = (TRI_priorityqueue_index_t*) idx;
  
  return PQIndex_top(pqIndex->_pqIndex, n);
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


static int FillLookupSLOperator(TRI_index_operator_t* slOperator, TRI_primary_collection_t* collection) {
  TRI_json_t*                    jsonObject;
  TRI_shaped_json_t*             shapedObject;
  TRI_relation_index_operator_t* relationOperator;
  TRI_logical_index_operator_t*  logicalOperator;
  size_t j;
  int result;

  if (slOperator == NULL) {
    return TRI_ERROR_INTERNAL;
  }

  switch (slOperator->_type) {
    case TRI_AND_INDEX_OPERATOR:
    case TRI_NOT_INDEX_OPERATOR:
    case TRI_OR_INDEX_OPERATOR: {

      logicalOperator = (TRI_logical_index_operator_t*)(slOperator);
      result = FillLookupSLOperator(logicalOperator->_left,collection);
      if (result == TRI_ERROR_NO_ERROR) {
        result = FillLookupSLOperator(logicalOperator->_right,collection);
      }
      if (result != TRI_ERROR_NO_ERROR) {
        return result;
      }
      break;
    }

    case TRI_EQ_INDEX_OPERATOR:
    case TRI_GE_INDEX_OPERATOR:
    case TRI_GT_INDEX_OPERATOR:
    case TRI_NE_INDEX_OPERATOR:
    case TRI_LE_INDEX_OPERATOR:
    case TRI_LT_INDEX_OPERATOR: {

      relationOperator = (TRI_relation_index_operator_t*)(slOperator);
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

    // .........................................................................
    // This index operator is special
    // The parameters are given to us as a list of json objects for EQ(...),
    // however for the IN(...) operator each parameter in the parameters list
    // is itself a list. For skiplists, the number of parameters is a
    // decreasing sequence. That is, for a skiplist with 3 attributes,
    // the parameters [ ["a","b","c","d"],["x","y"],[0] ] are allowed, whereas
    // the parameters [ ["a","b","c"], ["x","y"], [0,1,2] ] are not allowed.
    // .........................................................................

    case TRI_IN_INDEX_OPERATOR: {
      int maxEntries;

      relationOperator = (TRI_relation_index_operator_t*)(slOperator);
      relationOperator->_numFields  = 0;
      relationOperator->_collection = collection;
      relationOperator->_fields     = NULL;

      // .......................................................................
      // check that the parameters field is not null
      // .......................................................................

      if (relationOperator->_parameters == NULL) {
        LOG_WARNING("No parameters given when using Skiplist lookup index");
        return TRI_ERROR_INTERNAL;
      }


      // .......................................................................
      // check that the parameters json object is of the type list
      // .......................................................................

      if (relationOperator->_parameters->_type != TRI_JSON_LIST) {
        LOG_WARNING("Format of parameters given when using Skiplist lookup index are invalid (a)");
        return TRI_ERROR_INTERNAL;
      }

      // .......................................................................
      // Each entry in the list is itself a list
      // .......................................................................

      relationOperator->_numFields  = relationOperator->_parameters->_value._objects._length;
      relationOperator->_fields     = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_shaped_json_t) * relationOperator->_numFields, false);
      if (relationOperator->_fields == NULL) {
        relationOperator->_numFields = 0; // out of memory?
        return TRI_ERROR_OUT_OF_MEMORY;
      }

      result     = 0;
      maxEntries = -1;

      for (j = 0; j < relationOperator->_numFields; ++j) {
        jsonObject   = (TRI_json_t*) (TRI_AtVector(&(relationOperator->_parameters->_value._objects),j));

        if (jsonObject == NULL) {
          result = -1;
          break;
        }

        if (jsonObject->_type != TRI_JSON_LIST) {
          result = -2;
          break;
        }

        // check and see that entries are non-increasing
        if ((int) jsonObject->_value._objects._length > maxEntries) {
          if (maxEntries > 0) {
            result = -3;
            break;
          }
          maxEntries = jsonObject->_value._objects._length;
        }

        // convert json to shaped json
        shapedObject = TRI_ShapedJsonJson(collection->_shaper, jsonObject);
        if (shapedObject == NULL) {
          result = -4;
          break;
        }

        // store shaped json list
        relationOperator->_fields[j] = *shapedObject; // shallow copy here is ok
        TRI_Free(TRI_UNKNOWN_MEM_ZONE, shapedObject); // don't require storage anymore
      }

      if (result != 0) {
        TRI_Free(TRI_UNKNOWN_MEM_ZONE,relationOperator->_fields);
        relationOperator->_fields = NULL;
        relationOperator->_numFields = 0;
        LOG_WARNING("Format of parameters given when using Skiplist lookup index are invalid (b)");
        return TRI_ERROR_INTERNAL;
      }

    }


  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief attempts to locate an entry in the skip list index
////////////////////////////////////////////////////////////////////////////////

// .............................................................................
// Note: this function will destroy the passed slOperator before it returns
// Warning: who ever calls this function is responsible for destroying
// TRI_skiplist_iterator_t* results
// .............................................................................


TRI_skiplist_iterator_t* TRI_LookupSkiplistIndex(TRI_index_t* idx, TRI_index_operator_t* slOperator) {
  TRI_skiplist_index_t*    skiplistIndex;
  TRI_skiplist_iterator_t* iteratorResult;
  int                      errorResult;

  skiplistIndex = (TRI_skiplist_index_t*)(idx);

  // .........................................................................
  // fill the relation operators which may be embedded in the slOperator with
  // additional information. Recall the slOperator is what information was
  // received from a user for query the skiplist.
  // .........................................................................

  errorResult = FillLookupSLOperator(slOperator, skiplistIndex->base._collection);
  if (errorResult != TRI_ERROR_NO_ERROR) {
    return NULL;
  }

  if (skiplistIndex->base._unique) {
    iteratorResult = SkiplistIndex_find(skiplistIndex->_skiplistIndex, &skiplistIndex->_paths, slOperator);
  }
  else {
    iteratorResult = MultiSkiplistIndex_find(skiplistIndex->_skiplistIndex, &skiplistIndex->_paths, slOperator);
  }

  // .........................................................................
  // we must deallocate any memory we allocated in FillLookupSLOperator
  // .........................................................................

  TRI_FreeIndexOperator(slOperator);

  return iteratorResult;
}

/*
TRI_skiplistEx_iterator_t* TRI_LookupSkiplistExIndex(TRI_index_t* idx, TRI_index_operator_t* slOperator) {
  TRI_skiplistEx_index_t*    skiplistExIndex;
  TRI_skiplistEx_iterator_t* iteratorResult;
  int                        errorResult;

  skiplistExIndex = (TRI_skiplistEx_index_t*)(idx);

  // .........................................................................
  // fill the relation operators which may be embedded in the slOperator with
  // additional information. Recall the slOperator is what information was
  // received from a user for query the skiplist.
  // .........................................................................

  errorResult = FillLookupSLOperator(slOperator, skiplistExIndex->base._collection);
  if (errorResult != TRI_ERROR_NO_ERROR) {
    return NULL;
  }

  if (skiplistExIndex->base._unique) {
    iteratorResult = SkiplistExIndex_find(skiplistExIndex->_skiplistExIndex, &skiplistExIndex->_paths, slOperator);
  }
  else {
    iteratorResult = MultiSkiplistExIndex_find(skiplistExIndex->_skiplistExIndex, &skiplistExIndex->_paths, slOperator);
  }

  // .........................................................................
  // we must deallocate any memory we allocated in FillLookupSLOperator
  // .........................................................................

  TRI_FreeIndexOperator(slOperator);

  return iteratorResult;
}
*/

////////////////////////////////////////////////////////////////////////////////
/// @brief helper for skiplist methods
////////////////////////////////////////////////////////////////////////////////

static int SkiplistIndexHelper(const TRI_skiplist_index_t* skiplistIndex,
                               TRI_skiplist_index_element_t* skiplistElement,
                               const TRI_doc_mptr_t* document) {
  TRI_shaped_json_t shapedObject;
  TRI_shaped_json_t shapedJson;
  TRI_shape_access_t const* acc;
  char const* ptr;
  size_t j;
  
  // ..........................................................................
  // Assign the document to the SkiplistIndexElement structure so that it can 
  // be retrieved later.
  // ..........................................................................
    
  assert(document != NULL); 
  assert(document->_data != NULL); 
    
  TRI_EXTRACT_SHAPED_JSON_MARKER(shapedJson, document->_data);

  if (shapedJson._sid == 0) {
    LOG_WARNING("encountered invalid marker with shape id 0");
    
    return TRI_ERROR_INTERNAL;
  }

  skiplistElement->_document = CONST_CAST(document);
  ptr = (char const*) skiplistElement->_document->_data;
    
  for (j = 0; j < skiplistIndex->_paths._length; ++j) {
    TRI_shape_pid_t shape = *((TRI_shape_pid_t*)(TRI_AtVector(&skiplistIndex->_paths,j)));

    // ..........................................................................
    // Determine if document has that particular shape 
    // ..........................................................................

    acc = TRI_FindAccessorVocShaper(skiplistIndex->base._collection->_shaper, shapedJson._sid, shape);

    if (acc == NULL || acc->_shape == NULL) {
      // TRI_Free(skiplistElement->fields); memory deallocated in the calling procedure
      return TRI_WARNING_ARANGO_INDEX_SKIPLIST_DOCUMENT_ATTRIBUTE_MISSING;
    }  
      
      
    // ..........................................................................
    // Extract the field
    // ..........................................................................    

    if (! TRI_ExecuteShapeAccessor(acc, &shapedJson, &shapedObject)) {
      // TRI_Free(skiplistElement->fields); memory deallocated in the calling procedure
      return TRI_ERROR_INTERNAL;
    }

    // ..........................................................................
    // Store the field
    // ..........................................................................    

    skiplistElement->_subObjects[j]._sid = shapedObject._sid;
    skiplistElement->_subObjects[j]._length = shapedObject._data.length;
    skiplistElement->_subObjects[j]._offset = ((char const*) shapedObject._data.data) - ptr;
  }

  return TRI_ERROR_NO_ERROR;
} // end of static function SkiplistIndexHelper



////////////////////////////////////////////////////////////////////////////////
/// @brief inserts a document into a skip list index
////////////////////////////////////////////////////////////////////////////////

static int InsertSkiplistIndex (TRI_index_t* idx, 
                                TRI_doc_mptr_t const* doc,
                                const bool isRollback) {
  TRI_skiplist_index_element_t skiplistElement;
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
  skiplistElement._subObjects = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_shaped_sub_t) * skiplistElement.numFields, false);
  skiplistElement.collection  = skiplistIndex->base._collection;
  
  if (skiplistElement._subObjects == NULL) {
    LOG_WARNING("out-of-memory in InsertSkiplistIndex");
    return TRI_ERROR_OUT_OF_MEMORY;
  }  
  
  res = SkiplistIndexHelper(skiplistIndex, &skiplistElement, doc);
  // ............................................................................
  // most likely the cause of this error is that the 'shape' of the document
  // does not match the 'shape' of the index structure -- so the document
  // is ignored. So not really an error at all.
  // ............................................................................

  if (res != TRI_ERROR_NO_ERROR) {

    // ..........................................................................
    // Deallocated the memory already allocated to skiplistElement.fields
    // ..........................................................................
      
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, skiplistElement._subObjects);
    
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
      
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, skiplistElement._subObjects);
  
  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the index type name
////////////////////////////////////////////////////////////////////////////////

static const char* TypeNameSkiplistIndex (TRI_index_t const* idx) {
  return "skiplist";
}

////////////////////////////////////////////////////////////////////////////////
/// @brief describes a skiplist index as a json object
////////////////////////////////////////////////////////////////////////////////

static TRI_json_t* JsonSkiplistIndex (TRI_index_t* idx, TRI_primary_collection_t const* collection) {
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
  fieldList = TRI_Allocate( TRI_CORE_MEM_ZONE, (sizeof(char*) * skiplistIndex->_paths._length) , false);

  // ..........................................................................
  // Convert the attributes (field list of the skiplist index) into strings
  // ..........................................................................

  for (j = 0; j < skiplistIndex->_paths._length; ++j) {
    TRI_shape_pid_t shape = *((TRI_shape_pid_t*)(TRI_AtVector(&skiplistIndex->_paths,j)));
    path = collection->_shaper->lookupAttributePathByPid(collection->_shaper, shape);
    if (path == NULL) {
      TRI_Free(TRI_CORE_MEM_ZONE, fieldList);

      return NULL;
    }
    fieldList[j] = ((const char*) path) + sizeof(TRI_shape_path_t) + path->_aidLength * sizeof(TRI_shape_aid_t);
  }

  // ..........................................................................
  // create json object and fill it
  // ..........................................................................

  json = TRI_JsonIndex(TRI_CORE_MEM_ZONE, idx);

  fields = TRI_CreateListJson(TRI_CORE_MEM_ZONE);
  for (j = 0; j < skiplistIndex->_paths._length; ++j) {
    TRI_PushBack3ListJson(TRI_CORE_MEM_ZONE, fields, TRI_CreateStringCopyJson(TRI_CORE_MEM_ZONE, fieldList[j]));
  }
  TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, json, "fields", fields);

  TRI_Free(TRI_CORE_MEM_ZONE, fieldList);

  return json;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes a document from a skiplist index
////////////////////////////////////////////////////////////////////////////////

static int RemoveSkiplistIndex (TRI_index_t* idx, 
                                TRI_doc_mptr_t const* doc,
                                const bool isRollback) {
  TRI_skiplist_index_element_t skiplistElement;
  TRI_skiplist_index_t* skiplistIndex;
  int res;

  // ............................................................................
  // Obtain the skiplist index structure
  // ............................................................................

  skiplistIndex = (TRI_skiplist_index_t*) idx;

  // ............................................................................
  // Allocate some memory for the SkiplistIndexElement structure
  // ............................................................................

  skiplistElement.numFields   = skiplistIndex->_paths._length;
  skiplistElement._subObjects = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_shaped_json_t) * skiplistElement.numFields, false);
  skiplistElement.collection  = skiplistIndex->base._collection;
  
  if (skiplistElement._subObjects == NULL) {
    LOG_WARNING("out-of-memory in InsertSkiplistIndex");
    return TRI_ERROR_OUT_OF_MEMORY;
  }  
  
  // ..........................................................................
  // Fill the json field list from the document
  // ..........................................................................
  
  res = SkiplistIndexHelper(skiplistIndex, &skiplistElement, doc);
  
  // ..........................................................................
  // Error returned generally implies that the document never was part of the 
  // skiplist index
  // ..........................................................................
  
  if (res != TRI_ERROR_NO_ERROR) {
  
    // ........................................................................
    // Deallocate memory allocated to skiplistElement.fields above
    // ........................................................................
    
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, skiplistElement._subObjects);
    
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
    
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, skiplistElement._subObjects);
  
  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a skiplist index
////////////////////////////////////////////////////////////////////////////////

TRI_index_t* TRI_CreateSkiplistIndex (struct TRI_primary_collection_s* primary,
                                      TRI_vector_pointer_t* fields,
                                      TRI_vector_t* paths,
                                      bool unique) {
  TRI_skiplist_index_t* skiplistIndex;
  TRI_index_t* idx;
  int result;
  size_t j;

  skiplistIndex = TRI_Allocate(TRI_CORE_MEM_ZONE, sizeof(TRI_skiplist_index_t), false);
  idx = &skiplistIndex->base;

  idx->typeName = TypeNameSkiplistIndex;
  TRI_InitIndex(idx, TRI_IDX_TYPE_SKIPLIST_INDEX, primary, unique, true);

  idx->json     = JsonSkiplistIndex;
  idx->insert   = InsertSkiplistIndex;
  idx->remove   = RemoveSkiplistIndex;
  
  // ...........................................................................
  // Copy the contents of the shape list vector into a new vector and store this
  // ...........................................................................

  TRI_InitVector(&skiplistIndex->_paths, TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_shape_pid_t));

  for (j = 0;  j < paths->_length;  ++j) {
    TRI_shape_pid_t shape = *((TRI_shape_pid_t*)(TRI_AtVector(paths,j)));

    TRI_PushBackVector(&skiplistIndex->_paths, &shape);
  }

  TRI_InitVectorString(&idx->_fields, TRI_CORE_MEM_ZONE);

  for (j = 0;  j < fields->_length;  ++j) {
    char const* name = fields->_buffer[j];
    char* copy = TRI_DuplicateStringZ(TRI_CORE_MEM_ZONE, name);
    TRI_PushBackVectorString(&idx->_fields, copy);
  }

  if (unique) {
    skiplistIndex->_skiplistIndex = SkiplistIndex_new();
  }
  else {
    skiplistIndex->_skiplistIndex = MultiSkiplistIndex_new();
  }

  if (skiplistIndex->_skiplistIndex == NULL) {
    TRI_DestroyVector(&skiplistIndex->_paths);
    TRI_DestroyVectorString(&idx->_fields);
    TRI_Free(TRI_CORE_MEM_ZONE, skiplistIndex);
    LOG_WARNING("skiplist index creation failed -- internal error when creating skiplist structure");
    return NULL;
  }

  // ...........................................................................
  // Assign the function calls used by the query engine
  // ...........................................................................

  result = SkiplistIndex_assignMethod(&(idx->indexQuery), TRI_INDEX_METHOD_ASSIGNMENT_QUERY);
  result = result || SkiplistIndex_assignMethod(&(idx->indexQueryFree), TRI_INDEX_METHOD_ASSIGNMENT_FREE);
  result = result || SkiplistIndex_assignMethod(&(idx->indexQueryResult), TRI_INDEX_METHOD_ASSIGNMENT_RESULT);

  if (result != TRI_ERROR_NO_ERROR) {
    TRI_DestroyVector(&skiplistIndex->_paths);
    TRI_DestroyVectorString(&idx->_fields);
    SkiplistIndex_free(skiplistIndex->_skiplistIndex);
    TRI_Free(TRI_CORE_MEM_ZONE, skiplistIndex);
    LOG_WARNING("skiplist index creation failed -- internal error when assigning function calls");
    return NULL;
  }

  return idx;
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
  TRI_Free(TRI_CORE_MEM_ZONE, idx);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    FULLTEXT INDEX
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief callback function called by the fulltext index to determine the
/// words to index for a specific document
////////////////////////////////////////////////////////////////////////////////

static TRI_fulltext_wordlist_t* GetWordlist (TRI_index_t* idx,
                                             const TRI_doc_mptr_t* const document) {
  TRI_fulltext_index_t* fulltextIndex;
  TRI_fulltext_wordlist_t* wordlist;
  TRI_shaped_json_t shaped;
  TRI_shaped_json_t shapedJson;
  TRI_shape_t const* shape;
  TRI_doc_mptr_t* doc;
  char* text;
  size_t textLength;
  TRI_vector_string_t* words;
  bool ok;

  fulltextIndex = (TRI_fulltext_index_t*) idx;
  doc = (TRI_doc_mptr_t*) ((uintptr_t) document);

  // extract the shape
  TRI_EXTRACT_SHAPED_JSON_MARKER(shaped, doc->_data);
  ok = TRI_ExtractShapedJsonVocShaper(fulltextIndex->base._collection->_shaper, &shaped, 0, fulltextIndex->_attribute, &shapedJson, &shape);

  if (! ok || shape == NULL) {
    return NULL;
  }

  // extract the string value for the indexed attribute
  ok = TRI_StringValueShapedJson(shape, shapedJson._data.data, &text, &textLength);

  if (! ok) {
    return NULL;
  }

  // parse the document text
  words = TRI_get_words(text, textLength, (size_t) fulltextIndex->_minWordLength, (size_t) TRI_FULLTEXT_MAX_WORD_LENGTH, true);
  if (words == NULL) {
    return NULL;
  }

  wordlist = TRI_CreateWordlistFulltextIndex(words->_buffer, words->_length);
  if (wordlist == NULL) {
    TRI_FreeVectorString(TRI_UNKNOWN_MEM_ZONE, words);
    return NULL;
  }

  // this really is a hack, but it works well:
  // make the word list vector think it's empty and free it
  // this does not free the word list, that we have already over the result
  words->_length = 0;
  words->_buffer = NULL;
  TRI_FreeVectorString(TRI_UNKNOWN_MEM_ZONE, words);

  return wordlist;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief inserts a document into the fulltext index
////////////////////////////////////////////////////////////////////////////////

static int InsertFulltextIndex (TRI_index_t* idx, 
                                TRI_doc_mptr_t const* doc,
                                const bool isRollback) {
  TRI_fulltext_index_t* fulltextIndex;
  TRI_fulltext_wordlist_t* wordlist;
  int res;

  fulltextIndex = (TRI_fulltext_index_t*) idx;
  if (idx == NULL) {
    LOG_WARNING("internal error in InsertFulltextIndex");
    return TRI_ERROR_INTERNAL;
  }

  res = TRI_ERROR_NO_ERROR;

  wordlist = GetWordlist(idx, doc);
  if (wordlist == NULL) {
    // TODO: distinguish the cases "empty wordlist" and "out of memory"
    // LOG_WARNING("could not build wordlist");
    return res;
  }

  if (wordlist->_numWords > 0) {
    // TODO: use status codes
    if (! TRI_InsertWordsFulltextIndex(fulltextIndex->_fulltextIndex, (TRI_fulltext_doc_t) ((uintptr_t) doc), wordlist)) {
      LOG_ERROR("adding document to fulltext index failed");
      res = TRI_ERROR_INTERNAL;
    }
  }

  TRI_FreeWordlistFulltextIndex(wordlist);

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the index type name
////////////////////////////////////////////////////////////////////////////////

static const char* TypeNameFulltextIndex (TRI_index_t const* idx) {
  return "fulltext";
}

////////////////////////////////////////////////////////////////////////////////
/// @brief describes a fulltext index as a json object
////////////////////////////////////////////////////////////////////////////////

static TRI_json_t* JsonFulltextIndex (TRI_index_t* idx, TRI_primary_collection_t const* collection) {
  TRI_json_t* json;
  TRI_json_t* fields;
  TRI_fulltext_index_t* fulltextIndex;
  TRI_shape_path_t const* path;
  char const* attributeName;

  fulltextIndex = (TRI_fulltext_index_t*) idx;
  if (fulltextIndex == NULL) {
    return NULL;
  }

  // convert attribute to string
  path = collection->_shaper->lookupAttributePathByPid(collection->_shaper, fulltextIndex->_attribute);
  if (path == 0) {
    return NULL;
  }

  attributeName = ((char const*) path) + sizeof(TRI_shape_path_t) + (path->_aidLength * sizeof(TRI_shape_aid_t));

  json = TRI_JsonIndex(TRI_CORE_MEM_ZONE, idx);

  TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, json, "minLength", TRI_CreateNumberJson(TRI_CORE_MEM_ZONE, (double) fulltextIndex->_minWordLength));

  fields = TRI_CreateListJson(TRI_CORE_MEM_ZONE);
  TRI_PushBack3ListJson(TRI_CORE_MEM_ZONE, fields, TRI_CreateStringCopyJson(TRI_CORE_MEM_ZONE, attributeName));
  TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, json, "fields", fields);

  return json;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes a document from a fulltext index
////////////////////////////////////////////////////////////////////////////////

static int RemoveFulltextIndex (TRI_index_t* idx, 
                                TRI_doc_mptr_t const* doc,
                                const bool isRollback) {
  TRI_fulltext_index_t* fulltextIndex;

  fulltextIndex = (TRI_fulltext_index_t*) idx;

  TRI_DeleteDocumentFulltextIndex(fulltextIndex->_fulltextIndex, (TRI_fulltext_doc_t) ((uintptr_t) doc));

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief cleanup function for the fulltext index
///
/// This will incrementally clean the index by removing document/word pairs
/// for deleted documents
////////////////////////////////////////////////////////////////////////////////

static int CleanupFulltextIndex (TRI_index_t* idx) {
  TRI_fulltext_index_t* fulltextIndex;
  int res;

  LOG_TRACE("fulltext cleanup called");

  fulltextIndex = (TRI_fulltext_index_t*) idx;
  res = TRI_ERROR_NO_ERROR;

  // check whether we should do a cleanup at all
  if (! TRI_CompactFulltextIndex(fulltextIndex->_fulltextIndex)) {
    res = TRI_ERROR_INTERNAL;
  }

  return res;
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
/// @brief creates a fulltext index
////////////////////////////////////////////////////////////////////////////////

TRI_index_t* TRI_CreateFulltextIndex (struct TRI_primary_collection_s* primary,
                                      const char* attributeName,
                                      const bool indexSubstrings,
                                      int minWordLength) {
  TRI_fulltext_index_t* fulltextIndex;
  TRI_index_t* idx;
  TRI_fts_index_t* fts;
  TRI_shaper_t* shaper;
  char* copy;
  TRI_shape_pid_t attribute;

  // look up the attribute
  shaper = primary->_shaper;
  attribute = shaper->findAttributePathByName(shaper, attributeName);

  if (attribute == 0) {
    return NULL;
  }

  copy = TRI_DuplicateStringZ(TRI_CORE_MEM_ZONE, attributeName);
  fulltextIndex = TRI_Allocate(TRI_CORE_MEM_ZONE, sizeof(TRI_fulltext_index_t), false);

  fts = TRI_CreateFtsIndex(2048, 1, 1);
  if (fts == NULL) {
    TRI_Free(TRI_CORE_MEM_ZONE, fulltextIndex);
    return NULL;
  }

  idx = &fulltextIndex->base;

  idx->typeName = TypeNameFulltextIndex;
  TRI_InitIndex(idx, TRI_IDX_TYPE_FULLTEXT_INDEX, primary, false, true); 

  idx->json     = JsonFulltextIndex;
  idx->insert   = InsertFulltextIndex;
  idx->remove   = RemoveFulltextIndex;
  idx->cleanup  = CleanupFulltextIndex;

  fulltextIndex->_fulltextIndex   = fts;
  fulltextIndex->_indexSubstrings = indexSubstrings;
  fulltextIndex->_attribute       = attribute;
  fulltextIndex->_minWordLength   = (minWordLength > 0 ? minWordLength : 1);

  TRI_InitVectorString(&idx->_fields, TRI_CORE_MEM_ZONE);
  TRI_PushBackVectorString(&idx->_fields, copy);

  return idx;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the memory allocated, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyFulltextIndex (TRI_index_t* idx) {
  TRI_fulltext_index_t* fulltextIndex;

  if (idx == NULL) {
    return;
  }

  fulltextIndex = (TRI_fulltext_index_t*) idx;

  TRI_DestroyVectorString(&idx->_fields);

  LOG_TRACE("destroying fulltext index");

  TRI_FreeFtsIndex(fulltextIndex->_fulltextIndex);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the memory allocated and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeFulltextIndex (TRI_index_t* idx) {
  if (idx == NULL) {
    return;
  }

  TRI_DestroyFulltextIndex(idx);
  TRI_Free(TRI_CORE_MEM_ZONE, idx);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    BITARRAY INDEX
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

// .............................................................................
// Helper function for TRI_LookupBitarrayIndex
// .............................................................................

static int FillLookupBitarrayOperator(TRI_index_operator_t* indexOperator, TRI_primary_collection_t* collection) {
  TRI_relation_index_operator_t* relationOperator;
  TRI_logical_index_operator_t*  logicalOperator;

  if (indexOperator == NULL) {
    return TRI_ERROR_INTERNAL;
  }

  switch (indexOperator->_type) {
    case TRI_AND_INDEX_OPERATOR:
    case TRI_NOT_INDEX_OPERATOR:
    case TRI_OR_INDEX_OPERATOR: {

      logicalOperator = (TRI_logical_index_operator_t*)(indexOperator);
      FillLookupBitarrayOperator(logicalOperator->_left,collection);
      FillLookupBitarrayOperator(logicalOperator->_right,collection);
      break;
    }

    case TRI_EQ_INDEX_OPERATOR:
    case TRI_GE_INDEX_OPERATOR:
    case TRI_GT_INDEX_OPERATOR:
    case TRI_NE_INDEX_OPERATOR:
    case TRI_LE_INDEX_OPERATOR:
    case TRI_LT_INDEX_OPERATOR: {

      relationOperator = (TRI_relation_index_operator_t*)(indexOperator);
      relationOperator->_numFields  = relationOperator->_parameters->_value._objects._length;
      relationOperator->_collection = collection;
      relationOperator->_fields     = NULL; // bitarray indexes need only the json representation of values

      // even tough we use the json representation of the values sent by the client
      // for a bitarray index, we still require the shaped_json values for later
      // if we intend to force a bitarray index to return a result set irrespective
      // of whether the index can do this efficiently, then we will require the shaped json
      // representation of the values to apply any filter condition. Note that
      // for skiplist indexes, we DO NOT use the json representation, rather the shaped json
      // representation of the values is used since for skiplists we are ALWAYS required to
      // go to the document and make comparisons with the document values and the client values


      // when you are ready to use the shaped json values -- uncomment the follow
      /*
      relationOperator->_fields     = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_shaped_json_t) * relationOperator->_numFields, false);
      if (relationOperator->_fields != NULL) {
        int j;
        TRI_json_t* jsonObject;
        TRI_shaped_json_t* shapedObject;
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
      */

      break;

    }


    // .........................................................................
    // This index operator is special
    // .........................................................................

    case TRI_IN_INDEX_OPERATOR: {
      assert(false);
    }
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief attempts to locate an entry in the bitarray index
////////////////////////////////////////////////////////////////////////////////

// .............................................................................
// Note: this function will destroy the passed index operator before it returns
// Warning: who ever calls this function is responsible for destroying
// TRI_index_iterator_t* results
// .............................................................................


TRI_index_iterator_t* TRI_LookupBitarrayIndex(TRI_index_t* idx,
                                              TRI_index_operator_t* indexOperator,
                                              bool (*filter) (TRI_index_iterator_t*)) {
  TRI_bitarray_index_t* baIndex;
  TRI_index_iterator_t* iteratorResult;
  int                   errorResult;

  baIndex = (TRI_bitarray_index_t*)(idx);

  // .........................................................................
  // fill the relation operators which may be embedded in the indexOperator
  // with additional information. Recall the indexOperator is what information
  // was received from a client for querying the bitarray index.
  // .........................................................................
  
  errorResult = FillLookupBitarrayOperator(indexOperator, baIndex->base._collection); 

  if (errorResult != TRI_ERROR_NO_ERROR) {
    return NULL;
  }  
  
  iteratorResult = BitarrayIndex_find(baIndex->_bitarrayIndex,
                                      indexOperator,
                                      &baIndex->_paths,
                                      baIndex,
                                      NULL);

  TRI_FreeIndexOperator(indexOperator);

  return iteratorResult;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief helper for bitarray methods
////////////////////////////////////////////////////////////////////////////////

static int BitarrayIndexHelper(const TRI_bitarray_index_t* baIndex, 
                               TRI_bitarray_index_key_t* element,
                               const TRI_doc_mptr_t* document,
                               const TRI_shaped_json_t* shapedDoc) {

  TRI_shaped_json_t shapedObject;
  TRI_shape_access_t const* acc;
  size_t j;

  // ...........................................................................
  // For the structure element->fields, memory will have been allocated for this
  // by the calling procedure -- DO NOT deallocate the memory here -- it is the
  // responsibility of the calling procedure.
  // ...........................................................................


  if (shapedDoc != NULL) {

    // ..........................................................................
    // Attempting to locate a entry using TRI_shaped_json_t object. Use this
    // when we wish to remove a entry and we only have the "keys" rather than
    // having the document (from which the keys would follow).
    // ..........................................................................

    element->data = NULL;


    for (j = 0; j < baIndex->_paths._length; ++j) {
      TRI_shape_pid_t shape = *((TRI_shape_pid_t*)(TRI_AtVector(&baIndex->_paths,j)));

      // ..........................................................................
      // Determine if document has that particular shape
      // ..........................................................................

      acc = TRI_FindAccessorVocShaper(baIndex->base._collection->_shaper, shapedDoc->_sid, shape);

      if (acc == NULL || acc->_shape == NULL) {
        return TRI_WARNING_ARANGO_INDEX_BITARRAY_UPDATE_ATTRIBUTE_MISSING;
      }


      // ..........................................................................
      // Extract the field
      // ..........................................................................

      if (! TRI_ExecuteShapeAccessor(acc, shapedDoc, &shapedObject)) {
        return TRI_ERROR_INTERNAL;
      }

      // ..........................................................................
      // Store the json shaped Object -- this is what will be used by index to
      // whatever it requires to be determined.
      // ..........................................................................

      element->fields[j] = shapedObject;
    }
  }

  else if (document != NULL) {

    // ..........................................................................
    // Assign the document to the element structure so that it can
    // be retreived later.
    // ..........................................................................

    element->data = CONST_CAST(document);

    for (j = 0; j < baIndex->_paths._length; ++j) {
      TRI_shaped_json_t shapedJson;
      TRI_shape_pid_t shape = *((TRI_shape_pid_t*)(TRI_AtVector(&baIndex->_paths,j)));

      // ..........................................................................
      // Determine if document has that particular shape
      // ..........................................................................

      TRI_EXTRACT_SHAPED_JSON_MARKER(shapedJson, document->_data);

      acc = TRI_FindAccessorVocShaper(baIndex->base._collection->_shaper, shapedJson._sid, shape);

      if (acc == NULL || acc->_shape == NULL) {
        return TRI_WARNING_ARANGO_INDEX_BITARRAY_DOCUMENT_ATTRIBUTE_MISSING;
      }

      // ..........................................................................
      // Extract the field
      // ..........................................................................

      if (! TRI_ExecuteShapeAccessor(acc, &shapedJson, &shapedObject)) {
        return TRI_ERROR_INTERNAL;
      }

      // ..........................................................................
      // Store the field
      // ..........................................................................

      element->fields[j] = shapedObject;
    }
  }

  else {
    return TRI_ERROR_INTERNAL;
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief inserts a document into a bitarray list index
////////////////////////////////////////////////////////////////////////////////

static int InsertBitarrayIndex (TRI_index_t* idx, 
                                TRI_doc_mptr_t const* doc,
                                const bool isRollback) {
  TRI_bitarray_index_key_t element;
  TRI_bitarray_index_t* baIndex;
  int result;

  // ............................................................................
  // Obtain the bitarray index structure
  // ............................................................................

  baIndex = (TRI_bitarray_index_t*) idx;
  if (idx == NULL) {
    LOG_WARNING("internal error in InsertBitarrayIndex");
    return TRI_ERROR_INTERNAL;
  }



  // ............................................................................
  // Allocate storage to shaped json objects stored as a simple list.
  // These will be used for comparisions
  // ............................................................................

  element.numFields   = baIndex->_paths._length;
  element.fields      = TRI_Allocate( TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_shaped_json_t) * element.numFields, false);
  element.collection  = baIndex->base._collection;

  if (element.fields == NULL) {
    LOG_WARNING("out-of-memory in InsertBitarrayIndex");
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  // ............................................................................
  // For each attribute we have defined in the index obtain its corresponding
  // value.
  // ............................................................................

  result = BitarrayIndexHelper(baIndex, &element, doc, NULL);

  // ............................................................................
  // most likely the cause of this error is that the 'shape' of the document
  // does not match the 'shape' of the index structure -- so the document
  // is ignored.
  // ............................................................................

  if (result != TRI_ERROR_NO_ERROR) {

    // ..........................................................................
    // Deallocated the memory already allocated to element.fields
    // ..........................................................................

    TRI_Free(TRI_UNKNOWN_MEM_ZONE, element.fields);
    element.numFields = 0;


    // ..........................................................................
    // It may happen that the document does not have the necessary attributes to
    // be included within the bitarray index, in this case do not report back an error.
    // ..........................................................................

    if (result == TRI_WARNING_ARANGO_INDEX_BITARRAY_DOCUMENT_ATTRIBUTE_MISSING) {
      if (! baIndex->_supportUndef) {
        return TRI_ERROR_NO_ERROR;
      }


      // ........................................................................
      // This insert means that the document does NOT have the index attributes
      // defined, however, we still insert it into aspecial 'undefined' column
      // ........................................................................

      result = BitarrayIndex_insert(baIndex->_bitarrayIndex, &element);
    }

    return result;
  }


  // ............................................................................
  // This insert means that the document has ALL attributes which have been defined
  // in the index. However, it may happen that one or more attribute VALUES are
  // unsupported within the index -- in this case the function below will return
  // an error and insertion of the document is rolled back.
  // ............................................................................

  result = BitarrayIndex_insert(baIndex->_bitarrayIndex, &element);

  // ............................................................................
  // Since we have allocated memory to element.fields above, we have to deallocate
  // this here.
  // ............................................................................

  TRI_Free(TRI_UNKNOWN_MEM_ZONE, element.fields);

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the index type name
////////////////////////////////////////////////////////////////////////////////

static const char* TypeNameBitarrayIndex (TRI_index_t const* idx) {
  return "bitarray";
}

////////////////////////////////////////////////////////////////////////////////
/// @brief describes a bitarray index as a json object
////////////////////////////////////////////////////////////////////////////////

static TRI_json_t* JsonBitarrayIndex (TRI_index_t* idx, TRI_primary_collection_t const* collection) {
  TRI_json_t* json;      // the json object we return describing the index
  TRI_json_t* keyValues; // a list of attributes and their associated values
  const TRI_shape_path_t* path;
  TRI_bitarray_index_t* baIndex;
  char const** fieldList;
  size_t j;

  // ..........................................................................
  // Recast index as bitarray index
  // ..........................................................................

  baIndex = (TRI_bitarray_index_t*) idx;
  if (baIndex == NULL) {
    return NULL;
  }


  // ..........................................................................
  // Allocate sufficent memory for the field list
  // ..........................................................................

  fieldList = TRI_Allocate( TRI_CORE_MEM_ZONE, (sizeof(char*) * baIndex->_paths._length) , false);

  // ..........................................................................
  // Convert the attributes (field list of the bitarray index) into strings
  // ..........................................................................

  for (j = 0; j < baIndex->_paths._length; ++j) {
    TRI_shape_pid_t shape = *((TRI_shape_pid_t*)(TRI_AtVector(&baIndex->_paths,j)));
    path = collection->_shaper->lookupAttributePathByPid(collection->_shaper, shape);
    if (path == NULL) {
      TRI_Free(TRI_CORE_MEM_ZONE, fieldList);
      return NULL;
    }
    fieldList[j] = ((const char*) path) + sizeof(TRI_shape_path_t) + path->_aidLength * sizeof(TRI_shape_aid_t);
  }


  // ..........................................................................
  // create the json object representing the index and proceed to fill it
  // ..........................................................................

  json = TRI_JsonIndex(TRI_CORE_MEM_ZONE, idx);

  // ..........................................................................
  // Create json list which will hold the key value pairs. Assuming that the
  // index is constructed with 3 fields "a","b" % "c", these pairs are stored as follows:
  // [ ["a", [value-a1,...,value-aN]], ["b", [value-b1,...,value-bN]], ["c", [value-c1,...,value-cN]] ]
  // ..........................................................................


  // ..........................................................................
  // Create the key value list
  // ..........................................................................

  keyValues = TRI_CreateListJson(TRI_CORE_MEM_ZONE);

  for (j = 0; j < baIndex->_paths._length; ++j) {
    TRI_json_t* keyValue;
    TRI_json_t* key;
    TRI_json_t* value;

    // ........................................................................
    // Create the list to store the pairs
    // ........................................................................

    keyValue = TRI_CreateListJson(TRI_CORE_MEM_ZONE);


    // ........................................................................
    // Create the key json object (copy the string)
    // ........................................................................

    key = TRI_CreateStringCopyJson(TRI_CORE_MEM_ZONE, fieldList[j]);


    // ........................................................................
    // Create the list of values and fill it from the values stored in the
    // bit array index structure
    // ........................................................................

    value = TRI_CreateListJson(TRI_CORE_MEM_ZONE);

    if (keyValue == NULL || key == NULL || value == NULL) {
      TRI_Free(TRI_CORE_MEM_ZONE, fieldList);
      return NULL;
    }

    TRI_CopyToJson(TRI_CORE_MEM_ZONE, value, (TRI_json_t*)(TRI_AtVector(&baIndex->_values,j)));


    // ........................................................................
    // insert the key first followed by the list of values
    // ........................................................................

    TRI_PushBack3ListJson(TRI_CORE_MEM_ZONE, keyValue, key);
    TRI_PushBack3ListJson(TRI_CORE_MEM_ZONE, keyValue, value);


    // ........................................................................
    // insert the key value pair into the list of such pairs
    // ........................................................................

    TRI_PushBack3ListJson(TRI_CORE_MEM_ZONE, keyValues, keyValue);
  }

  TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, json, "fields", keyValues);
  TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, json, "undefined", TRI_CreateBooleanJson(TRI_CORE_MEM_ZONE, baIndex->_supportUndef));

  TRI_Free(TRI_CORE_MEM_ZONE, fieldList);

  return json;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes a document from a bitarray index
////////////////////////////////////////////////////////////////////////////////

static int RemoveBitarrayIndex (TRI_index_t* idx, 
                                TRI_doc_mptr_t const* doc,
                                const bool isRollback) {
  TRI_bitarray_index_key_t element;
  TRI_bitarray_index_t* baIndex;
  int result;

  // ............................................................................
  // Obtain the bitarray index structure
  // ............................................................................

  baIndex = (TRI_bitarray_index_t*) idx;

  // ............................................................................
  // Allocate some memory for the element structure
  // ............................................................................

  element.numFields  = baIndex->_paths._length;
  element.fields     = TRI_Allocate(TRI_CORE_MEM_ZONE, sizeof(TRI_shaped_json_t) * element.numFields, false);
  element.collection = baIndex->base._collection;
  
  // ..........................................................................
  // Fill the json field list with values from the document
  // ..........................................................................

  result = BitarrayIndexHelper(baIndex, &element, doc, NULL);

  // ..........................................................................
  // Error returned generally implies that the document never was part of the
  // index -- however for a bitarray index we support docs which do not have
  // such an index key(s).
  // ..........................................................................

  if (result != TRI_ERROR_NO_ERROR) {

    // ........................................................................
    // Check what type of error we received. If 'bad' error, then return
    // ........................................................................

    if (result != TRI_WARNING_ARANGO_INDEX_BITARRAY_DOCUMENT_ATTRIBUTE_MISSING) {

      // ......................................................................
      // Deallocate memory allocated to element.fields above
      // ......................................................................
    
      TRI_Free(TRI_CORE_MEM_ZONE, element.fields);
      return result;    
    }    

    // ........................................................................
    // If we support undefined documents in the index, then pass this on,
    // otherwise return an error. Note that, eventually it may be slightly
    // more efficient to simply pass these undefined documents straight to
    // the index without using the BitarrayIndexHelper function above.
    // ........................................................................

    if (! baIndex->_supportUndef) {

      // ......................................................................
      // Deallocate memory allocated to element.fields above
      // ......................................................................
    
      TRI_Free(TRI_CORE_MEM_ZONE, element.fields);
    
      return TRI_ERROR_NO_ERROR;    
    }
  }
  
  // ............................................................................
  // Attempt to remove document from index
  // ............................................................................
  
  result = BitarrayIndex_remove(baIndex->_bitarrayIndex, &element);

  // ............................................................................
  // Deallocate memory allocated to element.fields above
  // ............................................................................
    
  TRI_Free(TRI_CORE_MEM_ZONE, element.fields);
  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a bitarray index
////////////////////////////////////////////////////////////////////////////////

TRI_index_t* TRI_CreateBitarrayIndex (struct TRI_primary_collection_s* primary,
                                      TRI_vector_pointer_t* fields,
                                      TRI_vector_t* paths,
                                      TRI_vector_pointer_t* values,
                                      bool supportUndef, 
                                      int* errorNum, 
                                      char** errorStr) {
  TRI_bitarray_index_t* baIndex;
  TRI_index_t* idx;
  size_t i,j,k;
  int result;
  void* createContext;
  int cardinality;


  // ...........................................................................
  // Before we start moving things about, ensure that the attributes have
  // not been repeated
  // ...........................................................................

  for (j = 0;  j < paths->_length;  ++j) {
    TRI_shape_pid_t* leftShape = (TRI_shape_pid_t*)(TRI_AtVector(paths,j));
    for (i = j + 1; i < paths->_length;  ++i) {
      TRI_shape_pid_t* rightShape = (TRI_shape_pid_t*)(TRI_AtVector(paths,i));
      if (*leftShape == *rightShape) {
        LOG_WARNING("bitarray index creation failed -- duplicate keys in index");
        *errorNum = TRI_ERROR_ARANGO_INDEX_BITARRAY_CREATION_FAILURE_DUPLICATE_ATTRIBUTES;
        *errorStr = TRI_DuplicateString("bitarray index creation failed -- duplicate keys in index");
        return NULL;
      }
    }
  }

  // ...........................................................................
  // For each key (attribute) ensure that the list of supported values are
  // unique
  // ...........................................................................

  for (k = 0;  k < paths->_length;  ++k) {
    TRI_json_t* valueList = (TRI_json_t*)(TRI_AtVectorPointer(values,k));

    if (valueList == NULL || valueList->_type != TRI_JSON_LIST) {
      LOG_WARNING("bitarray index creation failed -- list of values for index undefined");
      *errorNum = TRI_ERROR_BAD_PARAMETER;
      *errorStr = TRI_DuplicateString("bitarray index creation failed -- list of values for index undefined");
      return NULL;
    }

    for (j = 0; j < valueList->_value._objects._length; ++j) {
      TRI_json_t* leftValue = (TRI_json_t*)(TRI_AtVector(&(valueList->_value._objects), j));
      for (i = j + 1; i < valueList->_value._objects._length; ++i) {
        TRI_json_t* rightValue = (TRI_json_t*)(TRI_AtVector(&(valueList->_value._objects), i));
        if (TRI_EqualJsonJson(leftValue, rightValue)) {
          LOG_WARNING("bitarray index creation failed -- duplicate values in value list for an attribute");
          *errorNum = TRI_ERROR_ARANGO_INDEX_BITARRAY_CREATION_FAILURE_DUPLICATE_VALUES;
          *errorStr = TRI_DuplicateString("bitarray index creation failed -- duplicate values in value list for an attribute");
          return NULL;
        }
      }
    }
  }

  // ...........................................................................
  // attempt to allocate memory for the bit array index structure
  // ...........................................................................

  baIndex = TRI_Allocate(TRI_CORE_MEM_ZONE, sizeof(TRI_bitarray_index_t), false);
  idx = &baIndex->base;

  idx->typeName = TypeNameBitarrayIndex;
  TRI_InitIndex(idx, TRI_IDX_TYPE_BITARRAY_INDEX, primary, false, false);

  idx->json     = JsonBitarrayIndex;
  idx->insert   = InsertBitarrayIndex;
  idx->remove   = RemoveBitarrayIndex;
  
  baIndex->_supportUndef  = supportUndef;
  baIndex->_bitarrayIndex = NULL;
  
  // ...........................................................................
  // Copy the contents of the shape list vector into a new vector and store this
  // Do the same for the values associated with the attributes
  // ...........................................................................

  TRI_InitVector(&baIndex->_paths, TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_shape_pid_t));
  TRI_InitVector(&baIndex->_values, TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_json_t));

  for (j = 0;  j < paths->_length;  ++j) {
    TRI_json_t value;
    TRI_shape_pid_t shape = *((TRI_shape_pid_t*)(TRI_AtVector(paths,j)));
    TRI_PushBackVector(&baIndex->_paths, &shape);
    TRI_CopyToJson(TRI_UNKNOWN_MEM_ZONE, &value, (TRI_json_t*)(TRI_AtVectorPointer(values,j)));
    TRI_PushBackVector(&baIndex->_values, &value);
  }

  // ...........................................................................
  // Store the list of fields (attributes based on the paths above) as simple
  // c strings - saves us looking these up at a latter stage
  // ...........................................................................

  TRI_InitVectorString(&idx->_fields, TRI_CORE_MEM_ZONE);

  for (j = 0;  j < fields->_length;  ++j) {
    char const* name = fields->_buffer[j];
    char* copy = TRI_DuplicateStringZ(TRI_CORE_MEM_ZONE, name);

    TRI_PushBackVectorString(&idx->_fields, copy);
  }

  // ...........................................................................
  // Currently there is no creation context -- todo later
  // ...........................................................................

  createContext = NULL;

  // ...........................................................................
  // Check that the attributes have not been repeated
  // ...........................................................................

  // ...........................................................................
  // Determine the cardinality of the Bitarray index (that is, the number of
  // columns which constitute the index)
  // ...........................................................................

  cardinality = 0;

  for (j = 0;  j < paths->_length;  ++j) {
    TRI_json_t* value = (TRI_json_t*) TRI_AtVector(&baIndex->_values,j);
    size_t numValues;

    if (value == NULL) {
      TRI_DestroyVector(&baIndex->_paths);
      TRI_DestroyVector(&baIndex->_values);
      TRI_Free(TRI_CORE_MEM_ZONE, baIndex);
      LOG_WARNING("bitarray index creation failed -- list of values for index undefined");
      return NULL;
    }

    numValues = value->_value._objects._length;


    // .........................................................................
    // value is a list json type -- the number of entries informs us how many
    // different possible values there are
    // .........................................................................

    cardinality += numValues;
  }

  // ...........................................................................
  // for the moment we restrict the cardinality to 64
  // ...........................................................................

  if (cardinality > 64) {
    TRI_DestroyVector(&baIndex->_paths);
    TRI_DestroyVector(&baIndex->_values);
    TRI_Free(TRI_CORE_MEM_ZONE, baIndex);
    LOG_WARNING("bitarray index creation failed -- more than 64 possible values");
    return NULL;
  }


  if (cardinality < 1 ) {
    TRI_DestroyVector(&baIndex->_paths);
    TRI_DestroyVector(&baIndex->_values);
    TRI_Free(TRI_CORE_MEM_ZONE, baIndex);
    LOG_WARNING("bitarray index creation failed -- no index values defined");
    return NULL;
  }

  // ...........................................................................
  // Assign the function calls used by the query engine
  // ...........................................................................

  result = BittarrayIndex_assignMethod(&(idx->indexQuery), TRI_INDEX_METHOD_ASSIGNMENT_QUERY);
  result = result || BittarrayIndex_assignMethod(&(idx->indexQueryFree), TRI_INDEX_METHOD_ASSIGNMENT_FREE);
  result = result || BittarrayIndex_assignMethod(&(idx->indexQueryResult), TRI_INDEX_METHOD_ASSIGNMENT_RESULT);

  if (result != TRI_ERROR_NO_ERROR) {
    TRI_DestroyVector(&baIndex->_paths);
    TRI_DestroyVector(&baIndex->_values);
    TRI_Free(TRI_CORE_MEM_ZONE, baIndex);
    LOG_WARNING("bitarray index creation failed -- internal error when assigning function calls");
    return NULL;
  }

  // ...........................................................................
  // attempt to create a new bitarray index
  // ...........................................................................

  result = BitarrayIndex_new(&(baIndex->_bitarrayIndex), TRI_UNKNOWN_MEM_ZONE, (size_t) cardinality,
                             &baIndex->_values, supportUndef, createContext);
  if (result != TRI_ERROR_NO_ERROR) {
    TRI_DestroyVector(&baIndex->_paths);
    TRI_DestroyVector(&baIndex->_values);
    TRI_FreeBitarrayIndex(idx);
    LOG_WARNING("bitarray index creation failed -- your guess as good as mine");
    return NULL;
  }

  return idx;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the memory allocated, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyBitarrayIndex (TRI_index_t* idx) {
  TRI_bitarray_index_t* baIndex;
  size_t j;

  if (idx == NULL) {
    return;
  }

  LOG_TRACE("destroying bitarray index");
  TRI_DestroyVectorString(&idx->_fields);

  baIndex = (TRI_bitarray_index_t*) idx;
  for (j = 0;  j < baIndex->_values._length;  ++j) {
    TRI_DestroyJson(TRI_UNKNOWN_MEM_ZONE, (TRI_json_t*)(TRI_AtVector(&(baIndex->_values),j)));
  }
  TRI_DestroyVector(&baIndex->_paths);
  TRI_DestroyVector(&baIndex->_values);
  BitarrayIndex_free(baIndex->_bitarrayIndex);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief frees the memory allocated and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeBitarrayIndex (TRI_index_t* idx) {
  if (idx == NULL) {
    return;
  }
  TRI_DestroyBitarrayIndex(idx);
  TRI_Free(TRI_CORE_MEM_ZONE, idx);
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
