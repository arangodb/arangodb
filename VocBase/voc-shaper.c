////////////////////////////////////////////////////////////////////////////////
/// @brief json shaper used to compute the shape of an json object
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
/// @author Martin Schoenert
/// @author Copyright 2006-2011, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "voc-shaper.h"

#include <BasicsC/associative.h>
#include <BasicsC/hashes.h>
#include <BasicsC/locks.h>
#include <BasicsC/logging.h>
#include <BasicsC/strings.h>

// -----------------------------------------------------------------------------
// --SECTION--                                                 private constants
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief datafile size
////////////////////////////////////////////////////////////////////////////////

static TRI_voc_size_t const SHAPER_DATAFILE_SIZE = (2 * 1024 * 1204);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                     private types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief persistent, collection-bases shaper
////////////////////////////////////////////////////////////////////////////////

typedef struct voc_shaper_s {
  TRI_shaper_t base;

  TRI_associative_synced_t _attributeNames;
  TRI_associative_synced_t _attributeIds;
  TRI_associative_synced_t _shapeDictionary;
  TRI_associative_synced_t _shapeIds;

  TRI_associative_pointer_t _accessors;

  TRI_vector_pointer_t _sortedAttributes;
  
  TRI_shape_aid_t _nextAid;
  TRI_shape_sid_t _nextSid;

  TRI_blob_collection_t* _collection;

  TRI_mutex_t _shapeLock;
  TRI_mutex_t _attributeLock;
  TRI_mutex_t _accessorLock;
}
voc_shaper_t;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

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

/*
static int sortedCompareFunction(const TRI_df_attribute_marker_t* leftItem, const TRI_df_attribute_marker_t* rightItem) {
  const char* leftString;
  const char* rightString;
  
  leftString  = (const char*)(leftItem)  + sizeof(TRI_df_attribute_marker_t);
  rightString = (const char*)(rightItem) + sizeof(TRI_df_attribute_marker_t);
  
  return (strcmp(leftString,rightString));
}
*/

/*
static int64_t sortedIndexOf(voc_shaper_t* shaper, TRI_df_attribute_marker_t* item) {
  int64_t leftPos;
  int64_t rightPos;
  int64_t midPos;
  int compareResult;
  
  leftPos  = 0;
  rightPos = (shaper->_sortedAttributes._length) - 1;
  
  while (leftPos <= rightPos)  {
    midPos = (leftPos + rightPos) / 2;
    compareResult = sortedCompareFunction((TRI_df_attribute_marker_t*)(shaper->_sortedAttributes._buffer[midPos]), item);
    if (compareResult < 0) {
      leftPos = midPos + 1;
    }    
    else if (compareResult > 0) {
      rightPos = midPos - 1;
    }
    else {
      // should never happen since we do not allow duplicates here
      return -1;
    }  
  }
  return leftPos; // insert it to the left of this position
}
*/
 
static TRI_shape_aid_t FindAttributeName (TRI_shaper_t* shaper, char const* name) {
  TRI_df_attribute_marker_t marker;
  TRI_df_marker_t* result;
  int res;
  size_t n;
  voc_shaper_t* s;
  void const* p;
  void* f;
  
  s = (voc_shaper_t*) shaper;
  p = TRI_LookupByKeyAssociativeSynced(&s->_attributeNames, name);

  if (p != NULL) {
    return ((TRI_df_attribute_marker_t const*) p)->_aid;
  }

  // create a new attribute name
  n = strlen(name) + 1;

  // lock the index and check that the element is still missing
  TRI_LockMutex(&s->_attributeLock);

  p = TRI_LookupByKeyAssociativeSynced(&s->_attributeNames, name);

  // if the element appeared, return the aid
  if (p != NULL) {
    TRI_UnlockMutex(&s->_attributeLock);
    return ((TRI_df_attribute_marker_t const*) p)->_aid;
  }

  // create new attribute identifier
  memset(&marker, 0, sizeof(TRI_df_attribute_marker_t));

  marker.base._type = TRI_DF_MARKER_ATTRIBUTE;
  marker.base._size = sizeof(TRI_df_attribute_marker_t) + n;

  marker._aid = s->_nextAid++;
  marker._size = n;

  
  // write into the shape collection
  res = TRI_WriteBlobCollection(s->_collection, &marker.base, sizeof(TRI_df_attribute_marker_t), name, n, &result);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_UnlockMutex(&s->_attributeLock);
    return 0;
  }

    
  // enter into the dictionaries
  f = TRI_InsertKeyAssociativeSynced(&s->_attributeNames, name, result);
  assert(f == NULL);

  f = TRI_InsertKeyAssociativeSynced(&s->_attributeIds, &marker._aid, result);
  assert(f == NULL);

  /* sorted attributes (keys)
  // add the attribute to the sorted list
  searchResult = sortedIndexOf(s, (TRI_df_attribute_marker_t*)(result));
  if (searchResult >= 0 && searchResult <= s->_sortedAttributes._length) {
    TRI_InsertVectorPointer(&(s->_sortedAttributes),result,searchResult);      
    printf("%s:%u:##############:%d\n",__FILE__,__LINE__,searchResult);
  }
  else {
    assert(false);
  }
  */
  
  // and release the lock
  TRI_UnlockMutex(&s->_attributeLock);
  return marker._aid;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hashs the attribute id
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashKeyAttributeId (TRI_associative_synced_t* array, void const* key) {
  TRI_shape_aid_t const* k = key;

  return TRI_FnvHashPointer(k, sizeof(TRI_shape_aid_t));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hashs the attribute
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashElementAttributeId (TRI_associative_synced_t* array, void const* element) {
  TRI_df_attribute_marker_t const* e = element;

  return TRI_FnvHashPointer(&e->_aid, sizeof(TRI_shape_aid_t));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compares an attribute name and an attribute
////////////////////////////////////////////////////////////////////////////////

static bool EqualKeyAttributeId (TRI_associative_synced_t* array, void const* key, void const* element) {
  TRI_shape_aid_t const* k = key;
  TRI_df_attribute_marker_t const* e = element;

  return *k == e->_aid;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up an attribute name by identifier
////////////////////////////////////////////////////////////////////////////////

static char const* LookupAttributeId (TRI_shaper_t* shaper, TRI_shape_aid_t aid) {
  voc_shaper_t* s = (voc_shaper_t*) shaper;
  void const* p;

  p = TRI_LookupByKeyAssociativeSynced(&s->_attributeIds, &aid);

  if (p == NULL) {
    return NULL;
  }
  else {
    char const* a;

    a = p;
    return a + sizeof(TRI_df_attribute_marker_t);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hashs the shapes
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashElementShape (TRI_associative_synced_t* array, void const* element) {
  char const* e = element;
  TRI_shape_t const* ee = element;

  return TRI_FnvHashPointer(e + + sizeof(TRI_shape_sid_t), ee->_size - sizeof(TRI_shape_sid_t));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compares shapes
////////////////////////////////////////////////////////////////////////////////

static bool EqualElementShape (TRI_associative_synced_t* array, void const* left, void const* right) {
  char const* l = left;
  char const* r = right;

  TRI_shape_t const* ll = left;
  TRI_shape_t const* rr = right;

  return (ll->_size == rr->_size)
    && memcmp(l + sizeof(TRI_shape_sid_t),
              r + sizeof(TRI_shape_sid_t),
              ll->_size - sizeof(TRI_shape_sid_t)) == 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finds a shape
////////////////////////////////////////////////////////////////////////////////

static TRI_shape_t const* FindShape (TRI_shaper_t* shaper, TRI_shape_t* shape) {
  TRI_df_marker_t* result;
  TRI_df_shape_marker_t marker;
  TRI_shape_t const* found;
  TRI_shape_t* l;
  int res;
  voc_shaper_t* s;
  void* f;

  s = (voc_shaper_t*) shaper;
  found = TRI_LookupByElementAssociativeSynced(&s->_shapeDictionary, shape);

  // shape found, free argument and return
  if (found != 0) {
    TRI_Free(shape);
    return found;
  }

  // lock the index and check the element is still missing
  TRI_LockMutex(&s->_shapeLock);

  found = TRI_LookupByElementAssociativeSynced(&s->_shapeDictionary, shape);

  if (found != 0) {
    TRI_Free(shape);

    TRI_UnlockMutex(&s->_shapeLock);
    return found;
  }

  // create a new shape marker
  memset(&marker, 0, sizeof(TRI_df_shape_marker_t));

  marker.base._type = TRI_DF_MARKER_SHAPE;
  marker.base._size = sizeof(TRI_df_shape_marker_t) + shape->_size;

  shape->_sid = s->_nextSid++;

  // write into the shape collection
  res = TRI_WriteBlobCollection(s->_collection, &marker.base, sizeof(TRI_df_shape_marker_t), shape, shape->_size, &result);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_UnlockMutex(&s->_shapeLock);
    return NULL;
  }

  TRI_Free(shape);

  // enter into the dictionaries
  l = (TRI_shape_t*) (((char*) result) + sizeof(TRI_df_shape_marker_t));

  f = TRI_InsertElementAssociativeSynced(&s->_shapeDictionary, l);
  assert(f == NULL);

  f = TRI_InsertKeyAssociativeSynced(&s->_shapeIds, &l->_sid, l);
  assert(f == NULL);

  TRI_UnlockMutex(&s->_shapeLock);
  return l;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hashs the shape id
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashKeyShapeId (TRI_associative_synced_t* array, void const* key) {
  TRI_shape_sid_t const* k = key;

  return TRI_FnvHashPointer(k, sizeof(TRI_shape_sid_t));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hashs the shape
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashElementShapeId (TRI_associative_synced_t* array, void const* element) {
  TRI_shape_t const* e = element;

  return TRI_FnvHashPointer(&e->_sid, sizeof(TRI_shape_sid_t));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compares a shape id and a shape
////////////////////////////////////////////////////////////////////////////////

static bool EqualKeyShapeId (TRI_associative_synced_t* array, void const* key, void const* element) {
  TRI_shape_sid_t const* k = key;
  TRI_shape_t const* e = element;

  return *k == e->_sid;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a shape by identifier
////////////////////////////////////////////////////////////////////////////////

static TRI_shape_t const* LookupShapeId (TRI_shaper_t* shaper, TRI_shape_sid_t sid) {
  voc_shaper_t* s = (voc_shaper_t*) shaper;

  return TRI_LookupByKeyAssociativeSynced(&s->_shapeIds, &sid);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief iterator for open
////////////////////////////////////////////////////////////////////////////////

static bool OpenIterator (TRI_df_marker_t const* marker, void* data, TRI_datafile_t* datafile, bool journal) {
  voc_shaper_t* shaper = data;
  void* f;

  if (marker->_type == TRI_DF_MARKER_SHAPE) {
    char* p = ((char*) marker) + sizeof(TRI_df_shape_marker_t);
    TRI_shape_t* l = (TRI_shape_t*) p;

    LOG_TRACE("found shape %lu", (unsigned long) l->_sid);

    f = TRI_InsertElementAssociativeSynced(&shaper->_shapeDictionary, l);
    assert(f == NULL);

    f = TRI_InsertKeyAssociativeSynced(&shaper->_shapeIds, &l->_sid, l);
    assert(f == NULL);

    if (shaper->_nextSid <= l->_sid) {
      shaper->_nextSid = l->_sid + 1;
    }
  }
  else if (marker->_type == TRI_DF_MARKER_ATTRIBUTE) {
    TRI_df_attribute_marker_t* m = (TRI_df_attribute_marker_t*) marker;
    char* p = ((char*) m) + sizeof(TRI_df_attribute_marker_t);

    LOG_TRACE("found attribute %lu / '%s'", (unsigned long) m->_aid, p);

    f = TRI_InsertKeyAssociativeSynced(&shaper->_attributeNames, p, m);
    assert(f == NULL);

    f = TRI_InsertKeyAssociativeSynced(&shaper->_attributeIds, &m->_aid, m);
    assert(f == NULL);

    if (shaper->_nextAid <= m->_aid) {
      shaper->_nextAid = m->_aid + 1;
    }
  }
  else {
    LOG_TRACE("skipping marker %lu", (unsigned long) marker->_type);
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hashs the accessor
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashElementAccessor (TRI_associative_pointer_t* array, void const* element) {
  TRI_shape_access_t const* ee = element;
  uint64_t v[2];

  v[0] = ee->_sid;
  v[1] = ee->_pid;

  return TRI_FnvHashPointer(v, sizeof(v));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compares an accessor
////////////////////////////////////////////////////////////////////////////////

static bool EqualElementAccessor (TRI_associative_pointer_t* array, void const* left, void const* right) {
  TRI_shape_access_t const* ll = left;
  TRI_shape_access_t const* rr = right;

  return ll->_sid == rr->_sid && ll->_pid == rr->_pid;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises a persistent shaper
////////////////////////////////////////////////////////////////////////////////

static void InitVocShaper (voc_shaper_t* shaper, TRI_blob_collection_t* collection) {
  shaper->base.findAttributeName = FindAttributeName;
  shaper->base.lookupAttributeId = LookupAttributeId;
  shaper->base.findShape = FindShape;
  shaper->base.lookupShapeId = LookupShapeId;

  TRI_InitAssociativeSynced(&shaper->_attributeNames,
                            HashKeyAttributeName,
                            HashElementAttributeName,
                            EqualKeyAttributeName,
                            0);

  TRI_InitAssociativeSynced(&shaper->_attributeIds,
                            HashKeyAttributeId,
                            HashElementAttributeId,
                            EqualKeyAttributeId,
                            0);

  TRI_InitAssociativeSynced(&shaper->_shapeDictionary,
                            0,
                            HashElementShape,
                            0,
                            EqualElementShape);

  TRI_InitAssociativeSynced(&shaper->_shapeIds,
                            HashKeyShapeId,
                            HashElementShapeId,
                            EqualKeyShapeId,
                            0);

  TRI_InitAssociativePointer(&shaper->_accessors,
                             0,
                             HashElementAccessor,
                             0,
                             EqualElementAccessor);

  TRI_InitMutex(&shaper->_shapeLock);
  TRI_InitMutex(&shaper->_attributeLock);
  TRI_InitMutex(&shaper->_accessorLock);

  shaper->_nextAid = 1;
  shaper->_nextSid = 1;
  shaper->_collection = collection;
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
/// @brief creates persistent shaper
////////////////////////////////////////////////////////////////////////////////

TRI_shaper_t* TRI_CreateVocShaper (TRI_vocbase_t* vocbase,
                                   char const* path,
                                   char const* name) {
  voc_shaper_t* shaper;
  TRI_blob_collection_t* collection;
  TRI_col_parameter_t parameter;
  bool ok;

  TRI_InitParameterCollection(&parameter, name, SHAPER_DATAFILE_SIZE);

  collection = TRI_CreateBlobCollection(vocbase, path, &parameter);

  if (collection == NULL) {
    return NULL;
  }

  shaper = TRI_Allocate(sizeof(voc_shaper_t));
  TRI_InitShaper(&shaper->base);
  InitVocShaper(shaper, collection);

  // handle basics
  ok = TRI_InsertBasicTypesShaper(&shaper->base);

  if (! ok) {
    TRI_FreeVocShaper(&shaper->base);
    return NULL;
  }

  // and return
  return &shaper->base;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a persistent shaper, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyVocShaper (TRI_shaper_t* s) {
  voc_shaper_t* shaper = (voc_shaper_t*) s;
  
  assert(shaper);
  assert(shaper->_collection);

  TRI_FreeBlobCollection(shaper->_collection);

  TRI_DestroyAssociativeSynced(&shaper->_attributeNames);
  TRI_DestroyAssociativeSynced(&shaper->_attributeIds);
  TRI_DestroyAssociativeSynced(&shaper->_shapeDictionary);
  TRI_DestroyAssociativeSynced(&shaper->_shapeIds);
  TRI_DestroyAssociativePointer(&shaper->_accessors);

  TRI_DestroyMutex(&shaper->_shapeLock);
  TRI_DestroyMutex(&shaper->_attributeLock);

  TRI_DestroyShaper(s);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a persistent shaper and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeVocShaper (TRI_shaper_t* shaper) {
  TRI_DestroyVocShaper(shaper);
  TRI_Free(shaper);
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
/// @brief returns the underlying collection
////////////////////////////////////////////////////////////////////////////////

TRI_blob_collection_t* TRI_CollectionVocShaper (TRI_shaper_t* shaper) {
  return ((voc_shaper_t*) shaper)->_collection;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief opens a persistent shaper
////////////////////////////////////////////////////////////////////////////////

TRI_shaper_t* TRI_OpenVocShaper (TRI_vocbase_t* vocbase,
                                 char const* filename) {
  voc_shaper_t* shaper;
  TRI_blob_collection_t* collection;
  bool ok;

  collection = TRI_OpenBlobCollection(vocbase, filename);

  if (collection == NULL) {
    return NULL;
  }

  shaper = TRI_Allocate(sizeof(voc_shaper_t));
  TRI_InitShaper(&shaper->base);
  InitVocShaper(shaper, collection);

  // read all shapes and attributes
  TRI_IterateCollection(&collection->base, OpenIterator, shaper);

  // handle basics
  ok = TRI_InsertBasicTypesShaper(&shaper->base);

  if (! ok) {
    TRI_FreeVocShaper(&shaper->base);
    return NULL;
  }

  // and return
  return &shaper->base;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief closes a persistent shaper
////////////////////////////////////////////////////////////////////////////////

int TRI_CloseVocShaper (TRI_shaper_t* s) {
  voc_shaper_t* shaper = (voc_shaper_t*) s;
  int err;

  err = TRI_CloseBlobCollection(shaper->_collection);

  if (err != TRI_ERROR_NO_ERROR) {
    LOG_ERROR("cannot close blob collection of shaper, error %lu", (unsigned long) err);
  }

  return err;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief closes a persistent shaper
////////////////////////////////////////////////////////////////////////////////

TRI_shape_access_t const* TRI_FindAccessorVocShaper (TRI_shaper_t* s,
                                                     TRI_shape_sid_t sid,
                                                     TRI_shape_pid_t pid) {
  voc_shaper_t* shaper = (voc_shaper_t*) s;
  TRI_shape_access_t search;
  TRI_shape_access_t* accessor;
  TRI_shape_access_t const* found;

  TRI_LockMutex(&shaper->_accessorLock);

  search._sid = sid;
  search._pid = pid;

  found = TRI_LookupByElementAssociativePointer(&shaper->_accessors, &search);

  if (found == NULL) {
    found = accessor = TRI_ShapeAccessor(&shaper->base, sid, pid);
    TRI_InsertElementAssociativePointer(&shaper->_accessors, accessor, true);
  }

  TRI_UnlockMutex(&shaper->_accessorLock);

  return found;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
