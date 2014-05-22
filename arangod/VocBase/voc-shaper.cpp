////////////////////////////////////////////////////////////////////////////////
/// @brief json shaper used to compute the shape of an json object
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
/// @author Martin Schoenert
/// @author Copyright 2006-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "voc-shaper.h"

#include "BasicsC/associative.h"
#include "BasicsC/hashes.h"
#include "BasicsC/locks.h"
#include "BasicsC/logging.h"
#include "BasicsC/tri-strings.h"
#include "BasicsC/utf8-helper.h"
#include "VocBase/document-collection.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                     private types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief collection-based shaper
////////////////////////////////////////////////////////////////////////////////

typedef struct voc_shaper_s {
  TRI_shaper_t                 base;

  TRI_associative_synced_t     _attributeNames;
  TRI_associative_synced_t     _attributeIds;
  TRI_associative_synced_t     _shapeDictionary;
  TRI_associative_synced_t     _shapeIds;

  TRI_associative_pointer_t    _accessors;

  TRI_shape_aid_t              _nextAid;
  TRI_shape_sid_t              _nextSid;

  TRI_document_collection_t*   _collection;

  TRI_mutex_t                  _shapeLock;
  TRI_mutex_t                  _attributeLock;
  TRI_mutex_t                  _accessorLock;
}
voc_shaper_t;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief hashs the attribute name of a key
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashKeyAttributeName (TRI_associative_synced_t* array, void const* key) {
  char const* k = (char const*) key;

  return TRI_FnvHashString(k);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hashs the attribute name of an element
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashElementAttributeName (TRI_associative_synced_t* array, void const* element) {
  char const* e = (char const*) element;

  return TRI_FnvHashString(e + sizeof(TRI_df_attribute_marker_t));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compares an attribute name and an attribute
////////////////////////////////////////////////////////////////////////////////

static bool EqualKeyAttributeName (TRI_associative_synced_t* array, void const* key, void const* element) {
  char const* k = (char const*) key;
  char const* e = (char const*) element;

  return TRI_EqualString(k, e + sizeof(TRI_df_attribute_marker_t));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finds an attribute identifier by name
////////////////////////////////////////////////////////////////////////////////

static TRI_shape_aid_t LookupAttributeByName (TRI_shaper_t* shaper, 
                                              char const* name) {
  voc_shaper_t* s;
  void const* p;

  assert(name != NULL);

  s = (voc_shaper_t*) shaper;
  p = TRI_LookupByKeyAssociativeSynced(&s->_attributeNames, name);

  if (p != NULL) {
    return ((TRI_df_attribute_marker_t const*) p)->_aid;
  }

  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finds an attribute identifier by name
////////////////////////////////////////////////////////////////////////////////

static TRI_shape_aid_t FindOrCreateAttributeByName (TRI_shaper_t* shaper, 
                                                    char const* name,
                                                    bool isLocked) { 
  char* mem;
  TRI_df_attribute_marker_t* marker;
  TRI_df_marker_t* result;
  TRI_doc_datafile_info_t* dfi;
  TRI_voc_fid_t fid;
  int res;
  size_t n;
  voc_shaper_t* s;
  TRI_shape_aid_t aid;
  TRI_voc_size_t totalSize;
  void const* p;
  void* f;

  assert(name != nullptr);

  s = (voc_shaper_t*) shaper;
  p = TRI_LookupByKeyAssociativeSynced(&s->_attributeNames, name);

  if (p != nullptr) {
    return ((TRI_df_attribute_marker_t const*) p)->_aid;
  }

  // create a new attribute name
  n = strlen(name) + 1;
  
  totalSize = (TRI_voc_size_t) (sizeof(TRI_df_attribute_marker_t) + n);
  mem = (char*) TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, totalSize, false);

  if (mem == nullptr) {
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);

    return 0;
  }

  // marker points to mem, but has a different type
  marker = (TRI_df_attribute_marker_t*) mem;
  assert(marker != nullptr);

  // init attribute marker
  TRI_InitMarker(mem, TRI_DF_MARKER_ATTRIBUTE, totalSize);
  
  // copy attribute name into marker
  memcpy(mem + sizeof(TRI_df_attribute_marker_t), name, n);
  marker->_size = n;

  // lock the index and check that the element is still missing
  TRI_LockMutex(&s->_attributeLock);

  p = TRI_LookupByKeyAssociativeSynced(&s->_attributeNames, name);

  // if the element appeared, return the aid
  if (p != nullptr) {
    TRI_UnlockMutex(&s->_attributeLock);
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, mem);

    return ((TRI_df_attribute_marker_t const*) p)->_aid;
  }

  // get next attribute id and write into marker
  aid = s->_nextAid++;
  marker->_aid = aid;
  
  if (! isLocked) {
    // write-lock
    s->_collection->base.beginWrite(&s->_collection->base);
  }

  // write attribute into the collection
  res = TRI_WriteMarkerDocumentCollection(s->_collection, &marker->base, totalSize, &fid, &result, false);
  
  if (! isLocked) {
    s->_collection->base.endWrite(&s->_collection->base);
  }
 
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, mem);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_UnlockMutex(&s->_attributeLock);

    LOG_ERROR("an error occurred while writing attribute data into shapes collection: %s", 
              TRI_errno_string(res));
    return 0;
  }
  
  assert(result != nullptr);

  // update datafile info
  dfi = TRI_FindDatafileInfoPrimaryCollection(&s->_collection->base, fid, true);

  if (dfi != nullptr) {
    dfi->_numberAttributes++;
    dfi->_sizeAttributes += (int64_t) TRI_DF_ALIGN_BLOCK(totalSize);
  }
  
  f = TRI_InsertKeyAssociativeSynced(&s->_attributeIds, &aid, result, false);
  assert(f == nullptr);
  
  // enter into the dictionaries
  f = TRI_InsertKeyAssociativeSynced(&s->_attributeNames, name, result, false);
  assert(f == nullptr);

  // ...........................................................................
  // and release the lock
  // ...........................................................................

  TRI_UnlockMutex(&s->_attributeLock);

  return aid;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hashes the attribute id
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashKeyAttributeId (TRI_associative_synced_t* array, void const* key) {
  TRI_shape_aid_t const* k = static_cast<TRI_shape_aid_t const*>(key);

  return TRI_FnvHashPointer(k, sizeof(TRI_shape_aid_t));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hashes the attribute
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashElementAttributeId (TRI_associative_synced_t* array, void const* element) {
  TRI_df_attribute_marker_t const* e = static_cast<TRI_df_attribute_marker_t const*>(element);

  return TRI_FnvHashPointer(&e->_aid, sizeof(TRI_shape_aid_t));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compares an attribute name and an attribute
////////////////////////////////////////////////////////////////////////////////

static bool EqualKeyAttributeId (TRI_associative_synced_t* array, void const* key, void const* element) {
  TRI_shape_aid_t const* k = static_cast<TRI_shape_aid_t const*>(key);
  TRI_df_attribute_marker_t const* e = static_cast<TRI_df_attribute_marker_t const*>(element);

  return *k == e->_aid;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up an attribute name by identifier
////////////////////////////////////////////////////////////////////////////////

static char const* LookupAttributeId (TRI_shaper_t* shaper, 
                                      TRI_shape_aid_t aid) {
  voc_shaper_t* s = (voc_shaper_t*) shaper;

  void const* p = TRI_LookupByKeyAssociativeSynced(&s->_attributeIds, &aid);

  if (p == nullptr) {
    return nullptr;
  }

  return static_cast<char const*>(p) + sizeof(TRI_df_attribute_marker_t);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hashes the shapes
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashElementShape (TRI_associative_synced_t* array, 
                                  void const* element) {
  char const* e = static_cast<char const*>(element);
  TRI_shape_t const* ee = static_cast<TRI_shape_t const*>(element);

  return TRI_FnvHashPointer(e + + sizeof(TRI_shape_sid_t), ee->_size - sizeof(TRI_shape_sid_t));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compares shapes
////////////////////////////////////////////////////////////////////////////////

static bool EqualElementShape (TRI_associative_synced_t* array, 
                               void const* left, 
                               void const* right) {
  char const* l = static_cast<char const*>(left);
  char const* r = static_cast<char const*>(right);

  TRI_shape_t const* ll = static_cast<TRI_shape_t const*>(left);
  TRI_shape_t const* rr = static_cast<TRI_shape_t const*>(right);

  return (ll->_size == rr->_size)
    && memcmp(l + sizeof(TRI_shape_sid_t),
              r + sizeof(TRI_shape_sid_t),
              ll->_size - sizeof(TRI_shape_sid_t)) == 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finds a shape
/// if the function returns non-NULL, the return value is a pointer to an
/// already existing shape and the value must not be freed
/// if the function returns NULL, it has not found the shape and was not able
/// to create it. The value must then be freed by the caller
////////////////////////////////////////////////////////////////////////////////

static TRI_shape_t const* FindShape (TRI_shaper_t* shaper, 
                                     TRI_shape_t* shape,
                                     bool create,
                                     bool isLocked) {
  char* mem;
  TRI_df_marker_t* result;
  TRI_df_shape_marker_t* marker;
  TRI_doc_datafile_info_t* dfi;
  TRI_voc_fid_t fid;
  TRI_voc_size_t totalSize;
  TRI_shape_t const* found;
  TRI_shape_t* l;
  int res;
  voc_shaper_t* s;
  void* f;

  s = (voc_shaper_t*) shaper;

  found = TRI_LookupBasicShapeShaper(shape);

  if (found == NULL) {
    found = static_cast<TRI_shape_t const*>(TRI_LookupByElementAssociativeSynced(&s->_shapeDictionary, shape));
  }

  // shape found, free argument and return
  if (found != NULL) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, shape);

    return found;
  }

  // not found
  if (! create) {
    return 0;
  }

  // initialise a new shape marker
  totalSize = (TRI_voc_size_t) (sizeof(TRI_df_shape_marker_t) + shape->_size);
  mem = (char*) TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, totalSize, false);

  if (mem == NULL) {
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);

    return NULL;
  }

  marker = (TRI_df_shape_marker_t*) mem;
  assert(marker != NULL);

  TRI_InitMarker(mem, TRI_DF_MARKER_SHAPE, totalSize);
  
  // copy shape into the marker
  memcpy(mem + sizeof(TRI_df_shape_marker_t), shape, shape->_size);


  // lock the index and check the element is still missing
  TRI_LockMutex(&s->_shapeLock);

  found = static_cast<TRI_shape_t const*>(TRI_LookupByElementAssociativeSynced(&s->_shapeDictionary, shape));

  if (found != 0) {
    TRI_UnlockMutex(&s->_shapeLock);

    TRI_Free(TRI_UNKNOWN_MEM_ZONE, shape);
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, mem);

    return found;
  }

  // get next shape number and write into marker
  ((TRI_shape_t*) (mem + sizeof(TRI_df_shape_marker_t)))->_sid = s->_nextSid++;

  if (! isLocked) {
    // write-lock
    s->_collection->base.beginWrite(&s->_collection->base);
  }

  // write shape into the collection
  res = TRI_WriteMarkerDocumentCollection(s->_collection, &marker->base, totalSize, &fid, &result, false);

  if (! isLocked) {
    // write-unlock
    s->_collection->base.endWrite(&s->_collection->base);
  }

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_UnlockMutex(&s->_shapeLock);

    LOG_ERROR("an error occurred while writing shape data into shapes collection: %s", 
              TRI_errno_string(res));
  
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, mem);

    return NULL;
  }
  
  assert(result != NULL);
  
  // update datafile info
  dfi = TRI_FindDatafileInfoPrimaryCollection(&s->_collection->base, fid, true);

  if (dfi != NULL) {
    dfi->_numberShapes++;
    dfi->_sizeShapes += (int64_t) TRI_DF_ALIGN_BLOCK(totalSize);
  }
    
  // enter into the dictionaries
  l = (TRI_shape_t*) (((char*) result) + sizeof(TRI_df_shape_marker_t));
  
  f = TRI_InsertKeyAssociativeSynced(&s->_shapeIds, &l->_sid, l, false);
  assert(f == NULL);

  f = TRI_InsertElementAssociativeSynced(&s->_shapeDictionary, l, false);
  assert(f == NULL);

  TRI_UnlockMutex(&s->_shapeLock);
  
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, mem);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, shape);

  return l;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hashes the shape id
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashKeyShapeId (TRI_associative_synced_t* array, 
                                void const* key) {
  TRI_shape_sid_t const* k = static_cast<TRI_shape_sid_t const*>(key);

  return TRI_FnvHashPointer(k, sizeof(TRI_shape_sid_t));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hashes the shape
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashElementShapeId (TRI_associative_synced_t* array, 
                                    void const* element) {
  TRI_shape_t const* e = static_cast<TRI_shape_t const*>(element);

  return TRI_FnvHashPointer(&e->_sid, sizeof(TRI_shape_sid_t));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compares a shape id and a shape
////////////////////////////////////////////////////////////////////////////////

static bool EqualKeyShapeId (TRI_associative_synced_t* array, 
                             void const* key, 
                             void const* element) {
  TRI_shape_sid_t const* k = static_cast<TRI_shape_sid_t const*>(key);
  TRI_shape_t const* e = static_cast<TRI_shape_t const*>(element);

  return *k == e->_sid;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a shape by identifier
////////////////////////////////////////////////////////////////////////////////

static TRI_shape_t const* LookupShapeId (TRI_shaper_t* shaper, 
                                         TRI_shape_sid_t sid) {
  TRI_shape_t const* shape = TRI_LookupSidBasicShapeShaper(sid);

  if (shape == NULL) {
    voc_shaper_t* s = (voc_shaper_t*) shaper;
    shape = static_cast<TRI_shape_t const*>(TRI_LookupByKeyAssociativeSynced(&s->_shapeIds, &sid));
  }

  return shape;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hashes the accessor
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashElementAccessor (TRI_associative_pointer_t* array, void const* element) {
  TRI_shape_access_t const* ee = static_cast<TRI_shape_access_t const*>(element);
  uint64_t v[2];

  v[0] = ee->_sid;
  v[1] = ee->_pid;

  return TRI_FnvHashPointer(v, sizeof(v));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compares an accessor
////////////////////////////////////////////////////////////////////////////////

static bool EqualElementAccessor (TRI_associative_pointer_t* array, void const* left, void const* right) {
  TRI_shape_access_t const* ll = static_cast<TRI_shape_access_t const*>(left);
  TRI_shape_access_t const* rr = static_cast<TRI_shape_access_t const*>(right);

  return ll->_sid == rr->_sid && ll->_pid == rr->_pid;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises a shaper
////////////////////////////////////////////////////////////////////////////////

static int InitStep1VocShaper (voc_shaper_t* shaper) {
  int res;

  shaper->base.findOrCreateAttributeByName = FindOrCreateAttributeByName;
  shaper->base.lookupAttributeByName       = LookupAttributeByName;
  shaper->base.lookupAttributeId           = LookupAttributeId;
  shaper->base.findShape                   = FindShape;
  shaper->base.lookupShapeId               = LookupShapeId;

  res = TRI_InitAssociativeSynced(&shaper->_attributeNames,
                                  TRI_UNKNOWN_MEM_ZONE,
                                  HashKeyAttributeName,
                                  HashElementAttributeName,
                                  EqualKeyAttributeName,
                                  0);

  if (res != TRI_ERROR_NO_ERROR) {
    return res;
  }

  res = TRI_InitAssociativeSynced(&shaper->_attributeIds,
                                  TRI_UNKNOWN_MEM_ZONE,
                                  HashKeyAttributeId,
                                  HashElementAttributeId,
                                  EqualKeyAttributeId,
                                  0);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_DestroyAssociativeSynced(&shaper->_attributeNames);

    return res;
  }

  res = TRI_InitAssociativeSynced(&shaper->_shapeDictionary,
                                  TRI_UNKNOWN_MEM_ZONE,
                                  0,
                                  HashElementShape,
                                  0,
                                  EqualElementShape);
  
  if (res != TRI_ERROR_NO_ERROR) {
    TRI_DestroyAssociativeSynced(&shaper->_attributeIds);
    TRI_DestroyAssociativeSynced(&shaper->_attributeNames);

    return res;
  }

  res = TRI_InitAssociativeSynced(&shaper->_shapeIds,
                                  TRI_UNKNOWN_MEM_ZONE,
                                  HashKeyShapeId,
                                  HashElementShapeId,
                                  EqualKeyShapeId,
                                  0);
  
  if (res != TRI_ERROR_NO_ERROR) {
    TRI_DestroyAssociativeSynced(&shaper->_shapeDictionary);
    TRI_DestroyAssociativeSynced(&shaper->_attributeIds);
    TRI_DestroyAssociativeSynced(&shaper->_attributeNames);

    return res;
  }

  res = TRI_InitAssociativePointer(&shaper->_accessors,
                                   TRI_UNKNOWN_MEM_ZONE,
                                   0,
                                   HashElementAccessor,
                                   0,
                                   EqualElementAccessor);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_DestroyAssociativeSynced(&shaper->_shapeIds);
    TRI_DestroyAssociativeSynced(&shaper->_shapeDictionary);
    TRI_DestroyAssociativeSynced(&shaper->_attributeIds);
    TRI_DestroyAssociativeSynced(&shaper->_attributeNames);

    return res;
  }

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_DestroyAssociativePointer(&shaper->_accessors);
    TRI_DestroyAssociativeSynced(&shaper->_shapeIds);
    TRI_DestroyAssociativeSynced(&shaper->_shapeDictionary);
    TRI_DestroyAssociativeSynced(&shaper->_attributeIds);
    TRI_DestroyAssociativeSynced(&shaper->_attributeNames);

    return res;
  }
  
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises a shaper
////////////////////////////////////////////////////////////////////////////////

static int InitStep2VocShaper (voc_shaper_t* shaper) { 
  TRI_InitMutex(&shaper->_shapeLock);
  TRI_InitMutex(&shaper->_attributeLock);
  TRI_InitMutex(&shaper->_accessorLock);

  shaper->_nextAid = 1;                              // id of next attribute to hand out
  shaper->_nextSid = TRI_FirstCustomShapeIdShaper(); // id of next shape to hand out 

  return TRI_ERROR_NO_ERROR;
}

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a shaper
////////////////////////////////////////////////////////////////////////////////

TRI_shaper_t* TRI_CreateVocShaper (TRI_vocbase_t* vocbase,
                                   TRI_document_collection_t* document) {
  voc_shaper_t* shaper = static_cast<voc_shaper_t*>(TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(voc_shaper_t), false));

  if (shaper == NULL) {
    // out of memory
    return NULL;
  }

  shaper->_collection = document;

  int res = TRI_InitShaper(&shaper->base, TRI_UNKNOWN_MEM_ZONE);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, shaper);

    return NULL;
  }

  res = InitStep1VocShaper(shaper);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_FreeShaper(&shaper->base);

    return NULL;
  }

  res = InitStep2VocShaper(shaper);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_FreeVocShaper(&shaper->base);

    return NULL;
  }

  // and return
  return &shaper->base;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a shaper, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyVocShaper (TRI_shaper_t* s) {
  voc_shaper_t* shaper = (voc_shaper_t*) s;

  assert(shaper != NULL);

  TRI_DestroyAssociativeSynced(&shaper->_attributeNames);
  TRI_DestroyAssociativeSynced(&shaper->_attributeIds);
  TRI_DestroyAssociativeSynced(&shaper->_shapeDictionary);
  TRI_DestroyAssociativeSynced(&shaper->_shapeIds);

  for (size_t i = 0; i < shaper->_accessors._nrAlloc; ++i) {
    TRI_shape_access_t* accessor = (TRI_shape_access_t*) shaper->_accessors._table[i];

    if (accessor != NULL) {
      TRI_FreeShapeAccessor(accessor);
    }
  }
  TRI_DestroyAssociativePointer(&shaper->_accessors);

  TRI_DestroyMutex(&shaper->_shapeLock);
  TRI_DestroyMutex(&shaper->_attributeLock);

  TRI_DestroyShaper(s);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a shaper and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeVocShaper (TRI_shaper_t* shaper) {
  TRI_DestroyVocShaper(shaper);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, shaper);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief initialise the shaper
////////////////////////////////////////////////////////////////////////////////

int TRI_InitVocShaper (TRI_shaper_t* s) {
  // this is a no-op now as there are no attribute weights anymore
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief move a shape marker, called during compaction
////////////////////////////////////////////////////////////////////////////////

int TRI_MoveMarkerVocShaper (TRI_shaper_t* s, 
                             TRI_df_marker_t* marker) {
  voc_shaper_t* shaper = (voc_shaper_t*) s;

  if (marker->_type == TRI_DF_MARKER_SHAPE) {
    char* p = ((char*) marker) + sizeof(TRI_df_shape_marker_t);
    TRI_shape_t* l = (TRI_shape_t*) p;
    void* f;

    TRI_LockMutex(&shaper->_shapeLock);

    // remove the old marker
    // and re-insert the marker with the new pointer
    f = TRI_InsertKeyAssociativeSynced(&shaper->_shapeIds, &l->_sid, l, true);
    assert(f != NULL);
  
    // same for the shape dictionary
    // delete and re-insert 
    f = TRI_InsertElementAssociativeSynced(&shaper->_shapeDictionary, l, true);
    assert(f != NULL);
    
    TRI_UnlockMutex(&shaper->_shapeLock);
  }
  else if (marker->_type == TRI_DF_MARKER_ATTRIBUTE) {
    TRI_df_attribute_marker_t* m = (TRI_df_attribute_marker_t*) marker;
    char* p = ((char*) m) + sizeof(TRI_df_attribute_marker_t);
    void* f;
    
    TRI_LockMutex(&shaper->_attributeLock);
 
    // remove attribute by name (p points to new location of name, but names
    // are identical in old and new marker) 
    // and re-insert same attribute with adjusted pointer
    f = TRI_InsertKeyAssociativeSynced(&shaper->_attributeNames, p, m, true);
    assert(f != NULL);

    // same for attribute ids
    // delete and re-insert same attribute with adjusted pointer
    f = TRI_InsertKeyAssociativeSynced(&shaper->_attributeIds, &m->_aid, m, true);
    assert(f != NULL);
    
    TRI_UnlockMutex(&shaper->_attributeLock);
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief insert a shape, called when opening a collection
////////////////////////////////////////////////////////////////////////////////

int TRI_InsertShapeVocShaper (TRI_shaper_t* s,
                              TRI_df_marker_t const* marker) {
  voc_shaper_t* shaper = (voc_shaper_t*) s;
  char* p = ((char*) marker) + sizeof(TRI_df_shape_marker_t);
  TRI_shape_t* l = (TRI_shape_t*) p;
  void* f;

  LOG_TRACE("found shape %lu", (unsigned long) l->_sid);

  f = TRI_InsertElementAssociativeSynced(&shaper->_shapeDictionary, l, false);
  assert(f == NULL);

  f = TRI_InsertKeyAssociativeSynced(&shaper->_shapeIds, &l->_sid, l, false);
  assert(f == NULL);

  if (shaper->_nextSid <= l->_sid) {
    shaper->_nextSid = l->_sid + 1;
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief insert an attribute, called when opening a collection
////////////////////////////////////////////////////////////////////////////////

int TRI_InsertAttributeVocShaper (TRI_shaper_t* s,
                                  TRI_df_marker_t const* marker) {
  voc_shaper_t* shaper = (voc_shaper_t*) s;
  TRI_df_attribute_marker_t* m = (TRI_df_attribute_marker_t*) marker;
  char* p = ((char*) m) + sizeof(TRI_df_attribute_marker_t);
  void* f;

  LOG_TRACE("found attribute '%s', aid: %lu", p, (unsigned long) m->_aid);

  f = TRI_InsertKeyAssociativeSynced(&shaper->_attributeNames, p, m, false);

  if (f != NULL) {
    char const* name = shaper->_collection->base.base._info._name;

#ifdef TRI_ENABLE_MAINTAINER_MODE
    LOG_WARNING("found duplicate attribute name '%s' in collection '%s'", p, name);
#else
    LOG_TRACE("found duplicate attribute name '%s' in collection '%s'", p, name); 
#endif    
  }

  f = TRI_InsertKeyAssociativeSynced(&shaper->_attributeIds, &m->_aid, m, false);
  
  if (f != NULL) {
    char const* name = shaper->_collection->base.base._info._name;

#ifdef TRI_ENABLE_MAINTAINER_MODE
    LOG_WARNING("found duplicate attribute id '%llu' in collection '%s'", (unsigned long long) m->_aid, name);
#else
    LOG_TRACE("found duplicate attribute id '%llu' in collection '%s'", (unsigned long long) m->_aid, name);
#endif    
  }

  // no lock is necessary here as we are the only users of the shaper at this time
  if (shaper->_nextAid <= m->_aid) {
    shaper->_nextAid = m->_aid + 1;
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finds an accessor for a shaper
////////////////////////////////////////////////////////////////////////////////

TRI_shape_access_t const* TRI_FindAccessorVocShaper (TRI_shaper_t* s,
                                                     TRI_shape_sid_t sid,
                                                     TRI_shape_pid_t pid) {
  voc_shaper_t* shaper = (voc_shaper_t*) s;
  TRI_shape_access_t search;
  TRI_shape_access_t* accessor;

  search._sid = sid;
  search._pid = pid;

  TRI_LockMutex(&shaper->_accessorLock);
  TRI_shape_access_t const* found = static_cast<TRI_shape_access_t const*>(TRI_LookupByElementAssociativePointer(&shaper->_accessors, &search));

  if (found == NULL) {
    found = accessor = TRI_ShapeAccessor(&shaper->base, sid, pid);

    // TRI_ShapeAccessor can return a NULL pointer
    if (found != NULL) {
      TRI_InsertElementAssociativePointer(&shaper->_accessors, accessor, true);
    }
  }

  TRI_UnlockMutex(&shaper->_accessorLock);

  return found;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts a sub-shape
////////////////////////////////////////////////////////////////////////////////

bool TRI_ExtractShapedJsonVocShaper (TRI_shaper_t* shaper,
                                     TRI_shaped_json_t const* document,
                                     TRI_shape_sid_t sid,
                                     TRI_shape_pid_t pid,
                                     TRI_shaped_json_t* result,
                                     TRI_shape_t const** shape) {
  TRI_shape_access_t const* accessor;
  bool ok;

  accessor = TRI_FindAccessorVocShaper(shaper, document->_sid, pid);

  if (accessor == NULL) {
    LOG_TRACE("failed to get accessor for sid %lu and path %lu",
              (unsigned long) document->_sid,
              (unsigned long) pid);

    return false;
  }

  *shape = accessor->_shape;

  if (accessor->_shape == NULL) {
    LOG_TRACE("expecting any object for path %lu, got nothing",
              (unsigned long) pid);

    return sid == 0;
  }

  if (sid != 0 && sid != accessor->_shape->_sid) {
    LOG_TRACE("expecting sid %lu for path %lu, got sid %lu",
              (unsigned long) sid,
              (unsigned long) pid,
              (unsigned long) accessor->_shape->_sid);

    return false;
  }

  ok = TRI_ExecuteShapeAccessor(accessor, document, result);

  if (! ok) {
    LOG_TRACE("failed to get accessor for sid %lu and path %lu",
              (unsigned long) document->_sid,
              (unsigned long) pid);

    return false;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief temporary structure for attributes
////////////////////////////////////////////////////////////////////////////////

typedef struct attribute_entry_s {
  char*             _attribute;
  TRI_shaped_json_t _value;
}
attribute_entry_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief sort attribure names
////////////////////////////////////////////////////////////////////////////////
  
static int AttributeNameComparator (void const* lhs,
                                    void const* rhs) {
  attribute_entry_t const* l = static_cast<attribute_entry_t const*>(lhs);
  attribute_entry_t const* r = static_cast<attribute_entry_t const*>(rhs);

  if (l->_attribute == NULL || r->_attribute == NULL) {
    // error !
    return -1;
  }

  return TRI_compare_utf8(l->_attribute, r->_attribute);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a sorted vector of attributes
////////////////////////////////////////////////////////////////////////////////
          
static int FillAttributesVector (TRI_vector_t* vector,
                                 TRI_shaped_json_t const* shapedJson,
                                 TRI_shape_t const* shape,
                                 TRI_shaper_t const* shaper) {
  
  TRI_InitVector(vector, TRI_UNKNOWN_MEM_ZONE, sizeof(attribute_entry_t));

  // .............................................................................
  // Determine the number of fixed sized values
  // .............................................................................
  
  char const* charShape = (char const*) shape;

  charShape = charShape + sizeof(TRI_shape_t);           
  TRI_shape_size_t fixedEntries = *((TRI_shape_size_t*)(charShape));

  // .............................................................................
  // Determine the number of variable sized values
  // .............................................................................
  
  charShape = charShape + sizeof(TRI_shape_size_t);
  TRI_shape_size_t variableEntries = *((TRI_shape_size_t*)(charShape));

  // .............................................................................
  // It may happen that the shaped_json_array is 'empty {}'
  // .............................................................................
  
  if (fixedEntries + variableEntries == 0) {
    return TRI_ERROR_NO_ERROR;
  }  
  
  // .............................................................................
  // Determine the list of shape identifiers
  // .............................................................................
  
  charShape = charShape + sizeof(TRI_shape_size_t);
  TRI_shape_sid_t const* sids = (TRI_shape_sid_t const*) charShape;
  
  charShape = charShape + (sizeof(TRI_shape_sid_t) * (fixedEntries + variableEntries));
  TRI_shape_aid_t const* aids = (TRI_shape_aid_t const*) charShape;

  charShape = charShape + (sizeof(TRI_shape_aid_t) * (fixedEntries + variableEntries));
  TRI_shape_size_t const* offsets = (TRI_shape_size_t const*) charShape;
  
  for (TRI_shape_size_t i = 0; i < fixedEntries; ++i) {
    attribute_entry_t attribute;

    char const* a = shaper->lookupAttributeId((TRI_shaper_t*) shaper, aids[i]);

    if (a == NULL) {
      return TRI_ERROR_INTERNAL;
    }

    char* copy = TRI_DuplicateStringZ(TRI_UNKNOWN_MEM_ZONE, a);

    if (copy == NULL) {
      return TRI_ERROR_OUT_OF_MEMORY;
    }

    attribute._attribute          = copy;
    attribute._value._sid         = sids[i];
    attribute._value._data.data   = shapedJson->_data.data + offsets[i];
    attribute._value._data.length = (uint32_t) (offsets[i + 1] - offsets[i]);

    TRI_PushBackVector(vector, &attribute);
  }
  
  offsets = (TRI_shape_size_t const*) shapedJson->_data.data;

  for (TRI_shape_size_t i = 0; i < variableEntries; ++i) {
    attribute_entry_t attribute;
    
    char const* a = shaper->lookupAttributeId((TRI_shaper_t*) shaper, aids[i + fixedEntries]);

    if (a == NULL) {
      return TRI_ERROR_INTERNAL;
    }
    
    char* copy = TRI_DuplicateStringZ(TRI_UNKNOWN_MEM_ZONE, a);

    if (copy == NULL) {
      return TRI_ERROR_OUT_OF_MEMORY;
    }
    
    attribute._attribute          = copy;
    attribute._value._sid         = sids[i + fixedEntries];
    attribute._value._data.data   = shapedJson->_data.data + offsets[i];
    attribute._value._data.length = (uint32_t) (offsets[i + 1] - offsets[i]);

    TRI_PushBackVector(vector, &attribute);
  }
          
  // sort the attributes by attribute name
  qsort(vector->_buffer, TRI_LengthVector(vector), sizeof(attribute_entry_t), AttributeNameComparator);
          
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy a vector of attributes
////////////////////////////////////////////////////////////////////////////////

static void DestroyAttributesVector (TRI_vector_t* vector) {
  size_t const n = TRI_LengthVector(vector);

  for (size_t i = 0; i < n; ++i) {
    attribute_entry_t* entry = static_cast<attribute_entry_t*>(TRI_AtVector(vector, i));

    if (entry->_attribute != NULL) {
      TRI_Free(TRI_UNKNOWN_MEM_ZONE, entry->_attribute);
    }
  }

  TRI_DestroyVector(vector);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compares two shapes
///
/// You must either supply (leftDocument, leftObject) or leftShaped.
/// You must either supply (rightDocument, rightObject) or rightShaped.
////////////////////////////////////////////////////////////////////////////////

int TRI_CompareShapeTypes (TRI_doc_mptr_t* leftDocument,
                           TRI_shaped_sub_t* leftObject,
                           TRI_shaped_json_t const* leftShaped,
                           TRI_doc_mptr_t* rightDocument,
                           TRI_shaped_sub_t* rightObject,
                           TRI_shaped_json_t const* rightShaped,
                           TRI_shaper_t* leftShaper,
                           TRI_shaper_t* rightShaper) {
  
  TRI_shape_t const* leftShape;
  TRI_shape_t const* rightShape;
  TRI_shaped_json_t left;
  TRI_shape_type_t leftType;
  TRI_shape_type_t rightType;
  TRI_shaped_json_t leftElement;
  TRI_shaped_json_t right;
  TRI_shaped_json_t rightElement;
  char const* ptr;
  int result;
  
  // left is either a shaped json or a shaped sub object
  if (leftDocument != NULL) {
    ptr = (char const*) leftDocument->_data;

    left._sid = leftObject->_sid;
    left._data.length = (uint32_t) leftObject->_length;
    left._data.data = const_cast<char*>(ptr) + leftObject->_offset;
  }
  else {
    left = *leftShaped;
  }
    
  // right is either a shaped json or a shaped sub object
  if (rightDocument != NULL) {
    ptr = (char const*) rightDocument->_data;

    right._sid = rightObject->_sid;
    right._data.length = (uint32_t) rightObject->_length;
    right._data.data = const_cast<char*>(ptr) + rightObject->_offset;
  }
  else {
    right = *rightShaped;
  }
    
  // get shape and type
  if (leftShaper == rightShaper && left._sid == right._sid) {
    // identical collection and shape
    leftShape = rightShape = leftShaper->lookupShapeId(leftShaper, left._sid);
  }
  else {
    // different shapes
    leftShape  = leftShaper->lookupShapeId(leftShaper, left._sid);
    rightShape = rightShaper->lookupShapeId(rightShaper, right._sid);
  }

  if (leftShape == NULL || rightShape == NULL) {
    LOG_ERROR("shape not found");
    assert(false);
  }

  leftType   = leftShape->_type;
  rightType  = rightShape->_type;

  // .............................................................................
  // check ALL combinations of leftType and rightType
  // .............................................................................

  switch (leftType) {

    // .............................................................................
    // illegal type
    // .............................................................................

    case TRI_SHAPE_ILLEGAL: {
      switch (rightType) {
        case TRI_SHAPE_ILLEGAL: {
          return 0;
        }
        case TRI_SHAPE_NULL:
        case TRI_SHAPE_BOOLEAN:
        case TRI_SHAPE_NUMBER:
        case TRI_SHAPE_SHORT_STRING:
        case TRI_SHAPE_LONG_STRING:
        case TRI_SHAPE_ARRAY:
        case TRI_SHAPE_LIST:
        case TRI_SHAPE_HOMOGENEOUS_LIST:
        case TRI_SHAPE_HOMOGENEOUS_SIZED_LIST: {
          return -1;
        }
      } // end of switch (rightType) 
    } // end of case TRI_SHAPE_ILLEGAL

    // .............................................................................
    // NULL
    // .............................................................................

    case TRI_SHAPE_NULL: {
      switch (rightType) {
        case TRI_SHAPE_ILLEGAL: {
          return 1;
        }
        case TRI_SHAPE_NULL: {
          return 0;
        }
        case TRI_SHAPE_BOOLEAN:
        case TRI_SHAPE_NUMBER:
        case TRI_SHAPE_SHORT_STRING:
        case TRI_SHAPE_LONG_STRING:
        case TRI_SHAPE_ARRAY:
        case TRI_SHAPE_LIST:
        case TRI_SHAPE_HOMOGENEOUS_LIST:
        case TRI_SHAPE_HOMOGENEOUS_SIZED_LIST: {
          return -1;
        }
      } // end of switch (rightType) 
    } // end of case TRI_SHAPE_NULL

    // .............................................................................
    // BOOLEAN
    // .............................................................................

    case TRI_SHAPE_BOOLEAN: {
      switch (rightType) {
        case TRI_SHAPE_ILLEGAL: 
        case TRI_SHAPE_NULL: {
          return 1;
        }
        case TRI_SHAPE_BOOLEAN: {
          // check which is false and which is true!
          if ( *((TRI_shape_boolean_t*)(left._data.data)) == *((TRI_shape_boolean_t*)(right._data.data)) ) {
            return 0;          
          }  
          if ( *((TRI_shape_boolean_t*)(left._data.data)) < *((TRI_shape_boolean_t*)(right._data.data)) ) {
            return -1;          
          }  
          return 1;
        }
        case TRI_SHAPE_NUMBER:
        case TRI_SHAPE_SHORT_STRING:
        case TRI_SHAPE_LONG_STRING:
        case TRI_SHAPE_ARRAY:
        case TRI_SHAPE_LIST:
        case TRI_SHAPE_HOMOGENEOUS_LIST:
        case TRI_SHAPE_HOMOGENEOUS_SIZED_LIST: {
          return -1;
        }
      } // end of switch (rightType) 
    } // end of case TRI_SHAPE_BOOLEAN

    // .............................................................................
    // NUMBER
    // .............................................................................

    case TRI_SHAPE_NUMBER: {
      switch (rightType) {
        case TRI_SHAPE_ILLEGAL: 
        case TRI_SHAPE_NULL:
        case TRI_SHAPE_BOOLEAN: {
          return 1;
        }
        case TRI_SHAPE_NUMBER: {
          // compare the numbers
          if ( *((TRI_shape_number_t*)(left._data.data)) == *((TRI_shape_number_t*)(right._data.data)) ) {
            return 0;          
          }  
          if ( *((TRI_shape_number_t*)(left._data.data)) < *((TRI_shape_number_t*)(right._data.data)) ) {
            return -1;          
          }  
          return 1;
        }
        case TRI_SHAPE_SHORT_STRING:
        case TRI_SHAPE_LONG_STRING:
        case TRI_SHAPE_ARRAY:
        case TRI_SHAPE_LIST:
        case TRI_SHAPE_HOMOGENEOUS_LIST:
        case TRI_SHAPE_HOMOGENEOUS_SIZED_LIST: {
          return -1;
        }
      } // end of switch (rightType) 
    } // end of case TRI_SHAPE_NUMBER
    
    // .............................................................................
    // STRING
    // .............................................................................

    case TRI_SHAPE_SHORT_STRING: 
    case TRI_SHAPE_LONG_STRING: {
      switch (rightType) {
        case TRI_SHAPE_ILLEGAL: 
        case TRI_SHAPE_NULL:
        case TRI_SHAPE_BOOLEAN:
        case TRI_SHAPE_NUMBER: {
          return 1;
        }
        case TRI_SHAPE_SHORT_STRING:
        case TRI_SHAPE_LONG_STRING: {
          char* leftString;
          char* rightString;

          // compare strings
          // extract the strings
          if (leftType == TRI_SHAPE_SHORT_STRING) {
            leftString = (char*)(sizeof(TRI_shape_length_short_string_t) + left._data.data);
          }
          else {
            leftString = (char*)(sizeof(TRI_shape_length_long_string_t) + left._data.data);
          }          
          
          if (rightType == TRI_SHAPE_SHORT_STRING) {
            rightString = (char*)(sizeof(TRI_shape_length_short_string_t) + right._data.data);
          }
          else {
            rightString = (char*)(sizeof(TRI_shape_length_long_string_t) + right._data.data);
          }         
          
          return TRI_compare_utf8(leftString, rightString);
        }
        case TRI_SHAPE_ARRAY:
        case TRI_SHAPE_LIST:
        case TRI_SHAPE_HOMOGENEOUS_LIST:
        case TRI_SHAPE_HOMOGENEOUS_SIZED_LIST: {
          return -1;
        }
      } // end of switch (rightType) 
    } // end of case TRI_SHAPE_LONG/SHORT_STRING 

    
    // .............................................................................
    // HOMOGENEOUS LIST
    // .............................................................................

    case TRI_SHAPE_HOMOGENEOUS_LIST: 
    case TRI_SHAPE_HOMOGENEOUS_SIZED_LIST: 
    case TRI_SHAPE_LIST: {
      switch (rightType) {
        case TRI_SHAPE_ILLEGAL: 
        case TRI_SHAPE_NULL:
        case TRI_SHAPE_BOOLEAN:
        case TRI_SHAPE_NUMBER:
        case TRI_SHAPE_SHORT_STRING:
        case TRI_SHAPE_LONG_STRING: {
          return 1;
        }
        case TRI_SHAPE_HOMOGENEOUS_LIST:
        case TRI_SHAPE_HOMOGENEOUS_SIZED_LIST: 
        case TRI_SHAPE_LIST: {
          // unfortunately recursion: check the types of all the entries
          size_t leftListLength = *((TRI_shape_length_list_t*) left._data.data);
          size_t rightListLength = *((TRI_shape_length_list_t*) right._data.data);
          size_t listLength;
          
          // determine the smallest list
          if (leftListLength > rightListLength) {
            listLength = rightListLength;
          }
          else {
            listLength = leftListLength;
          }
          
          for (size_t j = 0; j < listLength; ++j) {
            if (leftType == TRI_SHAPE_HOMOGENEOUS_LIST) {
              TRI_AtHomogeneousListShapedJson((const TRI_homogeneous_list_shape_t*)(leftShape),
                                              &left,
                                              j,
                                              &leftElement);
            }            
            else if (leftType == TRI_SHAPE_HOMOGENEOUS_SIZED_LIST) {
              TRI_AtHomogeneousSizedListShapedJson((const TRI_homogeneous_sized_list_shape_t*)(leftShape),
                                                   &left,
                                                   j,
                                                   &leftElement);
            }
            else {
              TRI_AtListShapedJson((const TRI_list_shape_t*)(leftShape),
                                   &left,
                                   j,
                                   &leftElement);
            }
            
            
            if (rightType == TRI_SHAPE_HOMOGENEOUS_LIST) {
              TRI_AtHomogeneousListShapedJson((const TRI_homogeneous_list_shape_t*)(rightShape),
                                              &right,
                                              j,
                                              &rightElement);
            }            
            else if (rightType == TRI_SHAPE_HOMOGENEOUS_SIZED_LIST) {
              TRI_AtHomogeneousSizedListShapedJson((const TRI_homogeneous_sized_list_shape_t*)(rightShape),
                                                   &right,
                                                   j,
                                                   &rightElement);
            }
            else {
              TRI_AtListShapedJson((const TRI_list_shape_t*)(rightShape),
                                   &right,
                                   j,
                                   &rightElement);
            }
            
            result = TRI_CompareShapeTypes(NULL,
                                           NULL,
                                           &leftElement,
                                           NULL,
                                           NULL,
                                           &rightElement,
                                           leftShaper,
                                           rightShaper);

            if (result != 0) { 
              return result;
            }  
          }          
          
          // up to listLength everything matches
          if (leftListLength < rightListLength) {
            return -1;
          }
          else if (leftListLength > rightListLength) {
            return 1;
          }  
          return 0;
        }
        
        
        case TRI_SHAPE_ARRAY:
        {
          return -1;
        }
      } // end of switch (rightType) 
    } // end of case TRI_SHAPE_LIST ... 
    
    // .............................................................................
    // ARRAY
    // .............................................................................

    case TRI_SHAPE_ARRAY: {
      switch (rightType) {
        case TRI_SHAPE_ILLEGAL: 
        case TRI_SHAPE_NULL:
        case TRI_SHAPE_BOOLEAN:
        case TRI_SHAPE_NUMBER:
        case TRI_SHAPE_SHORT_STRING:
        case TRI_SHAPE_LONG_STRING:
        case TRI_SHAPE_HOMOGENEOUS_LIST:
        case TRI_SHAPE_HOMOGENEOUS_SIZED_LIST: 
        case TRI_SHAPE_LIST: {
          return 1;
        }
        
        case TRI_SHAPE_ARRAY: {
          // ............................................................................  
          // We are comparing a left JSON array with another JSON array on the right
          // ............................................................................  
          
          // ............................................................................
          // generate the left and right lists sorted by attribute names
          // ............................................................................

          TRI_vector_t leftSorted;
          TRI_vector_t rightSorted;

          bool error = false;
          if (FillAttributesVector(&leftSorted, &left, leftShape, leftShaper) != TRI_ERROR_NO_ERROR) {
            error = true;
          }

          if (FillAttributesVector(&rightSorted, &right, rightShape, rightShaper) != TRI_ERROR_NO_ERROR) {
            error = true;
          }

          size_t const leftLength  = TRI_LengthVector(&leftSorted);
          size_t const rightLength = TRI_LengthVector(&rightSorted);
          size_t const numElements = (leftLength < rightLength ? leftLength : rightLength);
          
          result = 0;

          for (size_t i = 0; i < numElements; ++i) {
            attribute_entry_t const* l = static_cast<attribute_entry_t const*>(TRI_AtVector(&leftSorted, i));
            attribute_entry_t const* r = static_cast<attribute_entry_t const*>(TRI_AtVector(&rightSorted, i));

            result = TRI_compare_utf8(l->_attribute, r->_attribute);

            if (result != 0) {
              break;
            }
            
            result = TRI_CompareShapeTypes(NULL,
                                           NULL,
                                           &l->_value,
                                           NULL,
                                           NULL,
                                           &r->_value,
                                           leftShaper,
                                           rightShaper);

            if (result != 0) { 
              break;
            }  
          }        
          
          if (result == 0) {
            // ............................................................................
            // The comparisions above indicate that the shaped_json_arrays are equal, 
            // however one more check to determine if the number of elements in the arrays
            // are equal.
            // ............................................................................
            if (leftLength < rightLength) {
              result = -1;
            }
            else if (leftLength > rightLength) {
              result = 1;
            }
          }

          // clean up
          DestroyAttributesVector(&leftSorted);
          DestroyAttributesVector(&rightSorted);

          if (error) {
            return -1;
          }

          return result;
        }
      } // end of switch (rightType) 
    } // end of case TRI_SHAPE_ARRAY
  } // end of switch (leftType)
  
  assert(false);
  return 0; //shut the vc++ up
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
