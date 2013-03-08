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
#include <BasicsC/utf8-helper.h>

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

typedef struct attribute_weight_s {
    TRI_shape_aid_t _aid;
    int64_t _weight;
    char* _attribute;
    struct attribute_weight_s* _next;
} attribute_weight_t;


typedef struct attribute_weights_s {
  attribute_weight_t* _first;
  attribute_weight_t* _last;
  size_t _length;
} attribute_weights_t;

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

  TRI_vector_pointer_t      _sortedAttributes;
  TRI_associative_pointer_t _weightedAttributes;
  attribute_weights_t       _weights;
  
  TRI_shape_aid_t _nextAid;
  TRI_shape_sid_t _nextSid;

  TRI_shape_collection_t* _collection;

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
/// @brief compares two attribute strings stored in the attribute marker
/// 
/// returns 0 if the strings are equal
/// returns < 0 if the left string compares less than the right string
/// returns > 0 if the left string compares more than the right string
////////////////////////////////////////////////////////////////////////////////

static int attributeWeightCompareFunction(const void* leftItem, const void* rightItem) {
  const attribute_weight_t* l = (const attribute_weight_t*)(leftItem);
  const attribute_weight_t* r = (const attribute_weight_t*)(rightItem);

  assert(l);
  assert(r);

  return TRI_compare_utf8(l->_attribute, r->_attribute);
}

static int attributeWeightCompareFunctionPointer(const void* leftItem, const void* rightItem) {
  const attribute_weight_t* l = *((const attribute_weight_t**)(leftItem));
  const attribute_weight_t* r = *((const attribute_weight_t**)(rightItem));
  
  assert(l);
  assert(r);

  return TRI_compare_utf8(l->_attribute, r->_attribute);
}


////////////////////////////////////////////////////////////////////////////////
/// @brief performs a binary search on a list of attributes (attribute markers)
/// and returns the position where a given attribute would be inserted
////////////////////////////////////////////////////////////////////////////////

static int64_t sortedIndexOf(voc_shaper_t* shaper, attribute_weight_t* item) {
  int64_t leftPos;
  int64_t rightPos;
  int64_t midPos;
  int compareResult;
  
  leftPos  = 0;
  rightPos = ((int64_t) shaper->_sortedAttributes._length) - 1;
  
  while (leftPos <= rightPos)  {
    midPos = (leftPos + rightPos) / 2;
    compareResult = attributeWeightCompareFunction(TRI_AtVectorPointer(&(shaper->_sortedAttributes), midPos), (void*)(item));
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
 
 
////////////////////////////////////////////////////////////////////////////////
/// @brief helper method to store a list of attributes and their corresponding
/// weights.
////////////////////////////////////////////////////////////////////////////////

static void setAttributeWeight(voc_shaper_t* shaper, attribute_weight_t* item,
                               int64_t* searchResult, bool* weighted) {
  uint64_t itemWeight;
  attribute_weight_t* leftItem;  // nearest neighbour items
  attribute_weight_t* rightItem; // nearest neighbour items
  const int64_t resolution = 100;
  void* result;
  
  *weighted = false;

  if (item == NULL && shaper == NULL) { // oops
    assert(false);
    return;
  }
  
  *searchResult = sortedIndexOf(shaper, item);
  
  if (*searchResult < 0 || *searchResult > shaper->_sortedAttributes._length) { // oops
    assert(false);
    return;
  }
  
  switch ( (shaper->_sortedAttributes)._length) {
  
    case 0: {
      item->_weight = 0;
      *weighted = true;
      break;
    }
    
    case 1: {
      if (*searchResult == 0) {
        item->_weight = 0;
        rightItem = (attribute_weight_t*)(TRI_AtVectorPointer(&(shaper->_sortedAttributes), 0));
        rightItem->_weight = resolution;
      }
      else {
        leftItem  = (attribute_weight_t*)(TRI_AtVectorPointer(&(shaper->_sortedAttributes), 0));
        leftItem->_weight = 0;
        item->_weight = resolution;
      }
      *weighted = true;
      break;
    }
    
    default: {
      if (*searchResult == 0) {
        rightItem = (attribute_weight_t*)(TRI_AtVectorPointer(&(shaper->_sortedAttributes), 0));
        item->_weight = rightItem->_weight - resolution;
        *weighted = true;
      }
      else if (*searchResult == (shaper->_sortedAttributes)._length) {
        leftItem  = (attribute_weight_t*)(TRI_AtVectorPointer(&(shaper->_sortedAttributes), (shaper->_sortedAttributes)._length - 1));
        item->_weight = leftItem->_weight + resolution;
        *weighted = true;
      }
      else {
        leftItem  = (attribute_weight_t*)(TRI_AtVectorPointer(&(shaper->_sortedAttributes), *searchResult - 1));
        rightItem = (attribute_weight_t*)(TRI_AtVectorPointer(&(shaper->_sortedAttributes), *searchResult));
        itemWeight = (rightItem->_weight + leftItem->_weight) / 2;
        if (leftItem->_weight != itemWeight && rightItem->_weight != itemWeight) {
          item->_weight = itemWeight;
          *weighted = true;
        }
      }
      break;
    }        
  } // end of switch statement
  
  result = TRI_InsertKeyAssociativePointer(&(shaper->_weightedAttributes), &(item->_aid), item, false);
  if (result != NULL) {
    LOG_ERROR("attribute weight could not be inserted into associative array");  
    *searchResult = -1;
    return;
  }
  
  // ...........................................................................
  // Obtain the pointer of the weighted attribute structure which is stored
  // in the associative array. We need to pass this pointer to the Vector Pointer
  // ...........................................................................
  
  item = TRI_LookupByKeyAssociativePointer(&(shaper->_weightedAttributes), &(item->_aid));
  
  if (item == NULL) {
    LOG_ERROR("attribute weight could not be located immediately after insert into associative array");  
    *searchResult = -1;
    return;
  }  
  
  TRI_InsertVectorPointer(&(shaper->_sortedAttributes), item, *searchResult);      
}
 
static void fullSetAttributeWeight(voc_shaper_t* shaper) {
  int64_t startWeight;
  attribute_weight_t* item;
  int j;

  startWeight = 0;
  for (j = 0; j < shaper->_sortedAttributes._length; ++j) {
    item = (attribute_weight_t*)(TRI_AtVectorPointer(&(shaper->_sortedAttributes), j));
    item->_weight = startWeight;
    startWeight += 100;
  }
}
 
////////////////////////////////////////////////////////////////////////////////
/// @brief finds an attribute identifier by name
////////////////////////////////////////////////////////////////////////////////
 
static TRI_shape_aid_t FindAttributeName (TRI_shaper_t* shaper, char const* name) {
  TRI_df_attribute_marker_t marker;
  TRI_df_marker_t* result;
  TRI_df_attribute_marker_t* markerResult;
  int res;
  size_t n;
  voc_shaper_t* s;
  void const* p;
  void* f;
  int64_t searchResult;
  bool weighted;
  attribute_weight_t* weightedAttribute; 
  
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
  res = TRI_WriteShapeCollection(s->_collection, &marker.base, sizeof(TRI_df_attribute_marker_t), name, n, &result);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_UnlockMutex(&s->_attributeLock);
    return 0;
  }

    
  // enter into the dictionaries
  f = TRI_InsertKeyAssociativeSynced(&s->_attributeNames, name, result);
  assert(f == NULL);

  f = TRI_InsertKeyAssociativeSynced(&s->_attributeIds, &marker._aid, result);
  assert(f == NULL);


  // ...........................................................................
  // Each attribute has an associated integer as a weight. This
  // weight corresponds to the natural ordering of the attribute strings
  // ...........................................................................
  
  markerResult = (TRI_df_attribute_marker_t*)(result);
  
  weightedAttribute = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(attribute_weight_t), false);
  
  if (weightedAttribute != NULL) {
    weightedAttribute->_aid       = markerResult->_aid;
    weightedAttribute->_weight    = TRI_VOC_UNDEFINED_ATTRIBUTE_WEIGHT;
    weightedAttribute->_attribute = (char*)(markerResult) + sizeof(TRI_df_attribute_marker_t);
    weightedAttribute->_next      = NULL;
    
    // ..........................................................................
    // Save the new attribute weight in the linked list.
    // ..........................................................................

    if ((s->_weights)._last == NULL) {
      (s->_weights)._first = weightedAttribute;  
    }
    else {
      ((s->_weights)._last)->_next = weightedAttribute;
    }
    (s->_weights)._last = weightedAttribute;  
    (s->_weights)._length += 1;
    
    setAttributeWeight(s, weightedAttribute, &searchResult, &weighted); 
    assert(searchResult > -1);
    if (! weighted) {  
      fullSetAttributeWeight(s);
    }    
  }
  
  else {
    LOG_WARNING("FindAttributeName could not allocate memory, attribute is NOT weighted");    
  }  

  // ...........................................................................
  // and release the lock
  // ...........................................................................

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
/// @brief looks up an attribute weight by identifier
////////////////////////////////////////////////////////////////////////////////

static int64_t LookupAttributeWeight (TRI_shaper_t* shaper, TRI_shape_aid_t aid) {
  voc_shaper_t* s = (voc_shaper_t*) shaper;
  const attribute_weight_t* item;
  
  item = (const attribute_weight_t*)(TRI_LookupByKeyAssociativePointer(&s->_weightedAttributes, &aid));

  if (item == NULL) {
    // ........................................................................
    // return -9223372036854775807L 2^63-1 to indicate that the attribute is 
    // weighted to be the lowest possible
    // ........................................................................
    LOG_WARNING("LookupAttributeWeight returned NULL weight");    
    return TRI_VOC_UNDEFINED_ATTRIBUTE_WEIGHT; 
  }
  
  if (item->_aid != aid) {
    // ........................................................................
    // return -9223372036854775807L 2^63-1 to indicate that the attribute is 
    // weighted to be the lowest possible
    // ........................................................................
    LOG_WARNING("LookupAttributeWeight returned an UNDEFINED weight");    
    return TRI_VOC_UNDEFINED_ATTRIBUTE_WEIGHT; 
  }
  
  return item->_weight;    
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hashes the shapes
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
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, shape);
    return found;
  }

  // lock the index and check the element is still missing
  TRI_LockMutex(&s->_shapeLock);

  found = TRI_LookupByElementAssociativeSynced(&s->_shapeDictionary, shape);

  if (found != 0) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, shape);

    TRI_UnlockMutex(&s->_shapeLock);
    return found;
  }

  // create a new shape marker
  memset(&marker, 0, sizeof(TRI_df_shape_marker_t));

  marker.base._type = TRI_DF_MARKER_SHAPE;
  marker.base._size = sizeof(TRI_df_shape_marker_t) + shape->_size;

  shape->_sid = s->_nextSid++;

  // write into the shape collection
  res = TRI_WriteShapeCollection(s->_collection, &marker.base, sizeof(TRI_df_shape_marker_t), shape, shape->_size, &result);

  if (res != TRI_ERROR_NO_ERROR) {
    TRI_UnlockMutex(&s->_shapeLock);
    return NULL;
  }

  TRI_Free(TRI_UNKNOWN_MEM_ZONE, shape);

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
/// @brief return the number of shapes
////////////////////////////////////////////////////////////////////////////////

static size_t NumShapes (TRI_shaper_t* shaper) {
  voc_shaper_t* s;
  size_t n;

  s = (voc_shaper_t*) shaper;
  n = (size_t) TRI_GetLengthAssociativeSynced(&s->_shapeIds);

  return n;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hashes the shape id
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashKeyShapeId (TRI_associative_synced_t* array, void const* key) {
  TRI_shape_sid_t const* k = key;

  return TRI_FnvHashPointer(k, sizeof(TRI_shape_sid_t));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hashes the shape
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
  attribute_weight_t* weightedAttribute;  

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
    
    
    // .........................................................................
    // Add the attributes to the 'sorted' vector. These are in random order 
    // at the moment, however they will be sorted once all the attributes
    // have been loaded into memory.
    // .........................................................................

    weightedAttribute = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(attribute_weight_t), false);
  
    if (weightedAttribute != NULL) {
      attribute_weight_t* result;
    
      weightedAttribute->_aid       = m->_aid;
      weightedAttribute->_weight    = TRI_VOC_UNDEFINED_ATTRIBUTE_WEIGHT;
      weightedAttribute->_attribute = (char*)(m) + sizeof(TRI_df_attribute_marker_t);
      weightedAttribute->_next      = NULL;
      
      // ..........................................................................
      // Save the new attribute weight in the linked list.
      // ..........................................................................

      if ((shaper->_weights)._last == NULL) {
        (shaper->_weights)._first = weightedAttribute;  
      }
      else {
        ((shaper->_weights)._last)->_next = weightedAttribute;
      }
      
      (shaper->_weights)._last = weightedAttribute;  
      (shaper->_weights)._length += 1;
          
      result = (attribute_weight_t*)(TRI_InsertKeyAssociativePointer(&(shaper->_weightedAttributes), &(weightedAttribute->_aid), weightedAttribute, false));

      if (result == NULL) {
        attribute_weight_t* weightedItem;

        weightedItem = TRI_LookupByKeyAssociativePointer(&(shaper->_weightedAttributes), &(weightedAttribute->_aid));
        if (weightedItem == NULL) {
          LOG_ERROR("attribute weight could not be located immediately after insert into associative array");  
        }
        else if (weightedItem->_aid != weightedAttribute->_aid) {
          LOG_ERROR("attribute weight could not be located immediately after insert into associative array");  
        }      
        else {
          TRI_PushBackVectorPointer(&shaper->_sortedAttributes, weightedItem);
        }
      }
      else {
        LOG_WARNING("weighted attribute could not be inserted into associative array");    
      }
    }
    else {
      LOG_WARNING("OpenIterator could not allocate memory, attribute is NOT weighted");    
    }  
    
  }
  else {
    LOG_TRACE("skipping marker %lu", (unsigned long) marker->_type);
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hashes the accessor
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
/// @brief Hashes a weighted attribute
////////////////////////////////////////////////////////////////////////////////
                                 
static bool EqualKeyElementWeightedAttribute (TRI_associative_pointer_t* array, const void* key, const void* element) {
  TRI_shape_aid_t* aid = (TRI_shape_aid_t*)(key);
  attribute_weight_t* item = (attribute_weight_t*)(element);  
  return (*aid == item->_aid);
}

static uint64_t HashKeyWeightedAttribute (TRI_associative_pointer_t* array, const void* key) {
  TRI_shape_aid_t* aid = (TRI_shape_aid_t*)(key);
  return TRI_FnvHashBlock(TRI_FnvHashBlockInitial(), (char*)(aid), sizeof(TRI_shape_aid_t));
}

static uint64_t HashElementWeightedAttribute (TRI_associative_pointer_t* array, const void* element) {
  attribute_weight_t* item = (attribute_weight_t*)(element);  
  TRI_shape_aid_t* aid = &(item->_aid);
  return TRI_FnvHashBlock(TRI_FnvHashBlockInitial(), (char*)(aid), sizeof(TRI_shape_aid_t));
}



////////////////////////////////////////////////////////////////////////////////
/// @brief initialises a persistent shaper
////////////////////////////////////////////////////////////////////////////////

static void InitVocShaper (voc_shaper_t* shaper, TRI_shape_collection_t* collection) {
  shaper->base.findAttributeName = FindAttributeName;
  shaper->base.lookupAttributeId = LookupAttributeId;
  shaper->base.findShape = FindShape;
  shaper->base.lookupShapeId = LookupShapeId;
  shaper->base.numShapes = NumShapes;

  TRI_InitAssociativeSynced(&shaper->_attributeNames,
                            TRI_UNKNOWN_MEM_ZONE, 
                            HashKeyAttributeName,
                            HashElementAttributeName,
                            EqualKeyAttributeName,
                            0);

  TRI_InitAssociativeSynced(&shaper->_attributeIds,
                            TRI_UNKNOWN_MEM_ZONE, 
                            HashKeyAttributeId,
                            HashElementAttributeId,
                            EqualKeyAttributeId,
                            0);

  TRI_InitAssociativeSynced(&shaper->_shapeDictionary,
                            TRI_UNKNOWN_MEM_ZONE, 
                            0,
                            HashElementShape,
                            0,
                            EqualElementShape);

  TRI_InitAssociativeSynced(&shaper->_shapeIds,
                            TRI_UNKNOWN_MEM_ZONE, 
                            HashKeyShapeId,
                            HashElementShapeId,
                            EqualKeyShapeId,
                            0);

  TRI_InitAssociativePointer(&shaper->_accessors,
                             TRI_UNKNOWN_MEM_ZONE, 
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
  
  // ..........................................................................                             
  // Attribute weight 
  // ..........................................................................                             
  
  shaper->base.lookupAttributeWeight = LookupAttributeWeight;
  
  TRI_InitVectorPointer(&shaper->_sortedAttributes, TRI_UNKNOWN_MEM_ZONE);
  
  TRI_InitAssociativePointer(&shaper->_weightedAttributes, 
                             TRI_UNKNOWN_MEM_ZONE, 
                             HashKeyWeightedAttribute,
                             HashElementWeightedAttribute,
                             EqualKeyElementWeightedAttribute,
                             NULL);
                             
  // ..........................................................................                             
  // We require a place to store the weights -- a linked list is as good as 
  // anything for now. Later try and store in a smart 2D array. 
  // ..........................................................................                             
  
  (shaper->_weights)._first  = NULL;
  (shaper->_weights)._last   = NULL;
  (shaper->_weights)._length = 0;  
  
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
                                   char const* name, 
                                   const bool waitForSync,
                                   const bool isVolatile) {
  voc_shaper_t* shaper;
  TRI_shape_collection_t* collection;
  TRI_col_info_t parameter;
  int res;
  bool ok;

  TRI_InitCollectionInfo(vocbase, &parameter, name, TRI_COL_TYPE_SHAPE, SHAPER_DATAFILE_SIZE, 0);
  // set waitForSync and isVolatile for shapes collection
  parameter._isVolatile  = isVolatile;
  parameter._waitForSync = waitForSync;

  collection = TRI_CreateShapeCollection(vocbase, path, &parameter);

  if (collection == NULL) {
    return NULL;
  }

  shaper = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(voc_shaper_t), false);
  if (shaper == NULL) {
    // out of memory
    TRI_FreeShapeCollection(collection);

    return NULL;
  }

  TRI_InitShaper(&shaper->base, TRI_UNKNOWN_MEM_ZONE);
  InitVocShaper(shaper, collection);

  // handle basics
  ok = TRI_InsertBasicTypesShaper(&shaper->base);

  if (! ok) {
    TRI_FreeVocShaper(&shaper->base);
    return NULL;
  }
  
  res = TRI_SaveCollectionInfo(collection->base._directory, &parameter);
  if (res != TRI_ERROR_NO_ERROR) {
    LOG_ERROR("cannot save collection parameters in directory '%s': '%s'", collection->base._directory, TRI_last_error());
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
  size_t i;
  attribute_weight_t* weightedAttribute;
  attribute_weight_t* nextWeightedAttribute;
  
  assert(shaper);
  assert(shaper->_collection);

  TRI_FreeShapeCollection(shaper->_collection);

  TRI_DestroyAssociativeSynced(&shaper->_attributeNames);
  TRI_DestroyAssociativeSynced(&shaper->_attributeIds);
  TRI_DestroyAssociativeSynced(&shaper->_shapeDictionary);
  TRI_DestroyAssociativeSynced(&shaper->_shapeIds);

  for (i = 0; i < shaper->_accessors._nrAlloc; ++i) {
    TRI_shape_access_t* accessor = (TRI_shape_access_t*) shaper->_accessors._table[i];
    if (accessor != NULL) {
      TRI_FreeShapeAccessor(accessor);
    }
  }
  TRI_DestroyAssociativePointer(&shaper->_accessors);

  TRI_DestroyMutex(&shaper->_shapeLock);
  TRI_DestroyMutex(&shaper->_attributeLock);

  // ..........................................................................                             
  // Attribute weight 
  // ..........................................................................                             

  TRI_DestroyVectorPointer(&shaper->_sortedAttributes);
  TRI_DestroyAssociativePointer(&shaper->_weightedAttributes);
  
  weightedAttribute = (shaper->_weights)._first;
  while (weightedAttribute != NULL) {
    nextWeightedAttribute = weightedAttribute->_next;
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, weightedAttribute);
    weightedAttribute = nextWeightedAttribute;
  }
  
  TRI_DestroyShaper(s);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a persistent shaper and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeVocShaper (TRI_shaper_t* shaper) {
  TRI_DestroyVocShaper(shaper);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, shaper);
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

TRI_shape_collection_t* TRI_CollectionVocShaper (TRI_shaper_t* shaper) {
  return ((voc_shaper_t*) shaper)->_collection;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief opens a persistent shaper
////////////////////////////////////////////////////////////////////////////////

TRI_shaper_t* TRI_OpenVocShaper (TRI_vocbase_t* vocbase,
                                 char const* filename) {
  voc_shaper_t* shaper;
  TRI_shape_collection_t* collection;
  bool ok;

  collection = TRI_OpenShapeCollection(vocbase, filename);

  if (collection == NULL) {
    return NULL;
  }

  shaper = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(voc_shaper_t), false);
  if (shaper == NULL) {
    return NULL;
  }

  TRI_InitShaper(&shaper->base, TRI_UNKNOWN_MEM_ZONE);
  InitVocShaper(shaper, collection);

  // read all shapes and attributes
  TRI_IterateCollection(&collection->base, OpenIterator, shaper);


  // .............................................................................  
  // Sort all the attributes using the attribute string
  // .............................................................................
  
  qsort(shaper->_sortedAttributes._buffer, 
        shaper->_sortedAttributes._length, 
        sizeof(attribute_weight_t*), 
        attributeWeightCompareFunctionPointer);
  
  
  // .............................................................................
  // re-weigh all of the attributes
  // .............................................................................
  
  fullSetAttributeWeight(shaper);

  // handle basics
  ok = TRI_InsertBasicTypesShaper(&shaper->base);

  if (! ok) {
    TRI_FreeVocShaper(&shaper->base);
    return NULL;
  }

  return &shaper->base;
}


////////////////////////////////////////////////////////////////////////////////
/// @brief closes a persistent shaper
////////////////////////////////////////////////////////////////////////////////

int TRI_CloseVocShaper (TRI_shaper_t* s) {
  voc_shaper_t* shaper = (voc_shaper_t*) s;
  int err;

  err = TRI_CloseShapeCollection(shaper->_collection);

  if (err != TRI_ERROR_NO_ERROR) {
    LOG_ERROR("cannot close shape collection of shaper, error %d", (int) err);
  }

  // TODO free the accessors

  return err;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finds an accessor for a persistent shaper
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
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
