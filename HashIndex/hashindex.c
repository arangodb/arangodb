////////////////////////////////////////////////////////////////////////////////
/// @brief geo index
///
/// @file
///
/// DISCLAIMER
///
/// Copyright by triAGENS GmbH - All rights reserved.
///
/// The Programs (which include both the software and documentation)
/// contain proprietary information of triAGENS GmbH; they are
/// provided under a license agreement containing restrictions on use and
/// disclosure and are also protected by copyright, patent and other
/// intellectual and industrial property laws. Reverse engineering,
/// disassembly or decompilation of the Programs, except to the extent
/// required to obtain interoperability with other independently created
/// software or as specified by law, is prohibited.
///
/// The Programs are not intended for use in any nuclear, aviation, mass
/// transit, medical, or other inherently dangerous applications. It shall
/// be the licensee's responsibility to take all appropriate fail-safe,
/// backup, redundancy, and other measures to ensure the safe use of such
/// applications if the Programs are used for such purposes, and triAGENS
/// GmbH disclaims liability for any damages caused by such use of
/// the Programs.
///
/// This software is the confidential and proprietary information of
/// triAGENS GmbH. You shall not disclose such confidential and
/// proprietary information and shall use it only in accordance with the
/// terms of the license agreement you entered into with triAGENS GmbH.
///
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Dr. O
/// @author Copyright 2011, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "hashindex.h"

/* unused
static bool isEqualJsonJson (const TRI_json_t* left, const TRI_json_t* right) {
  size_t j;

  if (left == NULL && right == NULL) {
    return true;
  }

  if (left == NULL && right != NULL) {
    return false;
  }

  if (left != NULL && right == NULL) {
    return false;
  }
  
  if (left->_type != right->_type) {
    return false;
  }

  switch (left->_type) {

    case TRI_JSON_UNUSED: {
      return true;
    }  
  
    case TRI_JSON_NULL: {
      return true;
    }  
    
    case TRI_JSON_BOOLEAN: {
      return ( left->_value._boolean == right->_value._boolean ); 
    }  
    
    case TRI_JSON_NUMBER: {
      return ( left->_value._number == right->_value._number );
    }  
    
    
    case TRI_JSON_STRING: {
      if (left->_value._string.length == right->_value._string.length) {
        return (memcmp(left->_value._string.data, right->_value._string.data,
                       left->_value._string.length) == 0);
      }  
      return false;
    }      
    
    
    case TRI_JSON_ARRAY: {
      // order not defined here -- need special case here
      if (left->_value._objects._length != right->_value._objects._length) {
        return false;
      }  
      
      for (j = 0; j < left->_value._objects._length; ++j) {
        if (!isEqualJsonJson( (TRI_json_t*)(TRI_AtVector(&(left->_value._objects),j)), 
                               (TRI_json_t*)(TRI_AtVector(&(right->_value._objects),j)) 
                             )
            ) {
          return false;
        }           
      }
      return true;
    }
    
    case TRI_JSON_LIST: {
    
      if (left->_value._objects._length != right->_value._objects._length) {
        return false;
      }  
      
      for (j = 0; j < left->_value._objects._length; ++j) {
        if (!isEqualJsonJson( (TRI_json_t*)(TRI_AtVector(&(left->_value._objects),j)), 
                               (TRI_json_t*)(TRI_AtVector(&(right->_value._objects),j)) 
                             )
            ) {
          return false;
        }           
      }
      return true;
    }
  } // end of switch statement
  assert(false);  
  return false;
}  // end of function isEqualJsonJson
*/

static bool isEqualShapedJsonShapedJson (const TRI_shaped_json_t* left, const TRI_shaped_json_t* right) {
  if (left == NULL && right == NULL) {
    return true;
  }

  if (left == NULL && right != NULL) {
    return false;
  }

  if (left != NULL && right == NULL) {
    return false;
  }

  if (left->_data.length != right->_data.length) {
    return false;
  }

  return ( memcmp(left->_data.data,right->_data.data, left->_data.length) == 0);   
}  // end of function isEqualShapedJsonShapedJson

/* unused
static uint64_t hashJson (const size_t hash, const TRI_json_t* data) {
  size_t j;
  size_t newHash;
  
  if (data == NULL) {
    return hash;
  }

  switch (data->_type) {

    case TRI_JSON_UNUSED: {
      return hash;
    }  
  
    case TRI_JSON_NULL: {
      return hash;
    }  
    
    case TRI_JSON_BOOLEAN: {
      return TRI_FnvHashBlock(hash, (const char*)(&(data->_value._boolean)), sizeof(bool)); 
      // return TRI_BlockCrc32(hash, (const char*)(&(data->_value._boolean)), sizeof(bool));
    }  
    
    case TRI_JSON_NUMBER: {
      return TRI_FnvHashBlock(hash, (const char*)(&(data->_value._number)), sizeof(double)); 
      //return TRI_BlockCrc32(hash, (const char*)(&(data->_value._number)), sizeof(double));
    }  
       
    case TRI_JSON_STRING: {
      if (data->_value._string.length > 0) {
        return TRI_FnvHashBlock(hash, data->_value._string.data, data->_value._string.length);
        //return TRI_BlockCrc32(hash, data->_value._string.data, data->_value._string.length);
      }  
      return hash;
    }      
    
    
    case TRI_JSON_ARRAY: {
      // order not defined here -- need special case here
      if (data->_value._objects._length > 0) {
        newHash = hash;
        for (j = 0; j < data->_value._objects._length; ++j) {
          newHash = hashJson(newHash, (TRI_json_t*)(TRI_AtVector(&(data->_value._objects),j)) );
        }
        return newHash;
      }
      else {
        return hash;      
      }  
    }
    
    case TRI_JSON_LIST: {
      if (data->_value._objects._length > 0) {
        newHash = hash;
        for (j = 0; j < data->_value._objects._length; ++j) {
          newHash = hashJson(newHash, (TRI_json_t*)(TRI_AtVector(&(data->_value._objects),j)) );
        }
        return newHash;
      }
      else {
        return hash;      
      }  
    }
  } // end of switch statement
  assert(false);  
  return hash;
}  // end of function isEqualJsonJson
*/

static uint64_t hashShapedJson (const uint64_t hash, const TRI_shaped_json_t* shapedJson) {
  return TRI_FnvHashBlock(hash, shapedJson->_data.data, shapedJson->_data.length); 
}  // end of function hashShapedJson


// .............................................................................
// marks an element in the unique assoc array as being 'cleared' or 'empty'
// .............................................................................
static void clearElement(struct TRI_associative_array_s* associativeArray, void* element) {
  HashIndexElement* hElement = (HashIndexElement*)(element);
  if (element != NULL) {
    hElement->data = 0;
  }  
}


// .............................................................................
// returns true if an element in the unique assoc array is 'empty'
// .............................................................................
static bool isEmptyElement (struct TRI_associative_array_s* associativeArray, void* element) {
  HashIndexElement* hElement = (HashIndexElement*)(element);
  if (element != NULL) {
    if (hElement->data == 0) {
      return true;
    }
  }  
  return false;
}


// .............................................................................
// Determines if two elements of the unique assoc array are equal
// Two elements are 'equal' if the document pointer is the same. 
// .............................................................................
static bool isEqualElementElement (struct TRI_associative_array_s* associativeArray, 
                                   void* leftElement, void* rightElement) {
  HashIndexElement* hLeftElement  = (HashIndexElement*)(leftElement);
  HashIndexElement* hRightElement = (HashIndexElement*)(rightElement);
  
  if (leftElement == NULL || rightElement == NULL) {
    return false;
  }    
  
  if (hLeftElement->numFields != hRightElement->numFields) {
    return false; // should never happen
  }

  /*
  printf("%s:%u:%u:%u\n",__FILE__,__LINE__,
    (uint64_t)(hLeftElement->data),
    (uint64_t)(hRightElement->data)
  );
  */
  
  return (hLeftElement->data == hRightElement->data);
}


// .............................................................................
// Determines if two elements of the unique assoc array are equal
// Two elements are 'equal' if the shaped json content is equal. 
// .............................................................................
static bool isEqualKeyElement (struct TRI_associative_array_s* associativeArray, 
                               void* leftElement, void* rightElement) {
  HashIndexElement* hLeftElement  = (HashIndexElement*)(leftElement);
  HashIndexElement* hRightElement = (HashIndexElement*)(rightElement);
  size_t j;
  
  if (leftElement == NULL || rightElement == NULL) {
    return false;
  }    
  
  if (hLeftElement->numFields != hRightElement->numFields) {
    return false; // should never happen
  }

  for (j = 0; j < hLeftElement->numFields; j++) {
    /*
    printf("%s:%u:%f:%f,%u:%u\n",__FILE__,__LINE__,
      *((double*)((j + hLeftElement->fields)->_data.data)),
      *((double*)((j + hRightElement->fields)->_data.data)),
      (uint64_t)(hLeftElement->data),
      (uint64_t)(hRightElement->data)
    );
    */
    if (!isEqualShapedJsonShapedJson((j + hLeftElement->fields), (j + hRightElement->fields))) {
      return false;
    }
  }
  
  return true;
}


// .............................................................................
// The actual hashing occurs here
// .............................................................................
static uint64_t hashElement (struct TRI_associative_array_s* associativeArray, void* element) {
  HashIndexElement* hElement = (HashIndexElement*)(element);
  uint64_t hash = TRI_FnvHashBlockInitial();
  size_t j;

  for (j = 0; j < hElement->numFields; j++) {
    hash = hashShapedJson(hash, (j + hElement->fields) );
  }
  return  hash;
}


static uint64_t hashKey (struct TRI_associative_array_s* associativeArray, void* element) {
  HashIndexElement* hElement = (HashIndexElement*)(element);
  uint64_t hash = TRI_FnvHashBlockInitial();
  size_t j;

  for (j = 0; j < hElement->numFields; j++) {
    hash = hashShapedJson(hash, (j + hElement->fields) );
  }
  return  hash;
}




// .............................................................................
// Creates a new associative array
// .............................................................................

HashIndex* HashIndex_new() {
  HashIndex* hashIndex;

  hashIndex = TRI_Allocate(sizeof(HashIndex));
  if (hashIndex == NULL) {
    return NULL;
  }

  hashIndex->unique = true;
  hashIndex->assocArray.uniqueArray = TRI_Allocate(sizeof(TRI_associative_array_t));
  if (hashIndex->assocArray.uniqueArray == NULL) {
    TRI_Free(hashIndex);
    return NULL;
  }    
    
  TRI_InitAssociativeArray(hashIndex->assocArray.uniqueArray,
                           sizeof(HashIndexElement),
                           hashKey,
                           hashElement,
                           clearElement,
                           isEmptyElement,
                           isEqualKeyElement,
                           isEqualElementElement);
  
  return hashIndex;
}


// ...............................................................................
// Adds (inserts) a data element into the associative array (hash index)
// ...............................................................................

int HashIndex_add(HashIndex* hashIndex, HashIndexElement* element) {
  bool result;
  result = TRI_InsertKeyAssociativeArray(hashIndex->assocArray.uniqueArray, element, element, false);
  return result ? TRI_ERROR_NO_ERROR : TRI_ERROR_AVOCADO_UNIQUE_CONSTRAINT_VIOLATED;
}


// ...............................................................................
// Locates an entry within the unique associative array
// ...............................................................................

HashIndexElements* HashIndex_find(HashIndex* hashIndex, HashIndexElement* element) {
  HashIndexElement* result;
  HashIndexElements* results;

  results = TRI_Allocate(sizeof(HashIndexElements));    
  /* FIXME: memory allocation might fail */
  
  result = (HashIndexElement*) (TRI_FindByElementAssociativeArray(hashIndex->assocArray.uniqueArray, element)); 
  
  if (result != NULL) {
    results->_elements    = TRI_Allocate(sizeof(HashIndexElement) * 1); // unique hash index maximum number is 1
    results->_elements[0] = *result;
    results->_numElements = 1;
  }
  else {
    results->_elements    = NULL;
    results->_numElements = 0;
  }
  return results;
}


// ...............................................................................
// An alias for addIndex 
// ...............................................................................

int HashIndex_insert(HashIndex* hashIndex, HashIndexElement* element) {
  return HashIndex_add(hashIndex,element);
} 


// ...............................................................................
// Removes an entry from the associative array
// ...............................................................................

int HashIndex_remove(HashIndex* hashIndex, HashIndexElement* element) {
  bool result;

  result = TRI_RemoveElementAssociativeArray(hashIndex->assocArray.uniqueArray, element, NULL); 
  
  return result ? TRI_ERROR_NO_ERROR : TRI_ERROR_INTERNAL;
}


// ...............................................................................
// updates and entry from the associative array, first removes beforeElement, 
//  then adds the afterElement
// ...............................................................................

int HashIndex_update(HashIndex* hashIndex, const HashIndexElement* beforeElement, 
                      const HashIndexElement* afterElement) {
  assert(false);
  return TRI_ERROR_INTERNAL;                      
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Multi-hash non-unique hash indexes
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------


//------------------------------------------------------------------------------
// Private methods
//------------------------------------------------------------------------------


// .............................................................................
// Marks an entry in the non-unique associative array as being 'cleared'. 
// .............................................................................
static void multiClearElement(struct TRI_multi_array_s* multiArray, void* element) {
  HashIndexElement* hElement = (HashIndexElement*)(element);
  if (element != NULL) {
    hElement->data = 0;
  }  
}


// .............................................................................
// Determines if an entry in the associative array is marked as being empty (cleared)
// .............................................................................
static bool isMultiEmptyElement (struct TRI_multi_array_s* multiArray, void* element) {
  HashIndexElement* hElement = (HashIndexElement*)(element);
  if (element != NULL) {
    if (hElement->data == 0) {
      return true;
    }
  }  
  return false;
}


// .............................................................................
// Returns true if document pointers are the same, otherwise returns false
// .............................................................................
static bool isMultiEqualElementElement (struct TRI_multi_array_s* multiArray, 
                                        void* leftElement, void* rightElement) {
  HashIndexElement* hLeftElement  = (HashIndexElement*)(leftElement);
  HashIndexElement* hRightElement = (HashIndexElement*)(rightElement);
  
  if (leftElement == NULL || rightElement == NULL) {
    return false;
  }    
  
  return (hLeftElement->data == hRightElement->data); 
}


// .............................................................................
// Returns true if the "key" matches that of the element
// .............................................................................
static bool isMultiEqualKeyElement (struct TRI_multi_array_s* multiArray, 
                                    void* leftElement, void* rightElement) {
  HashIndexElement* hLeftElement  = (HashIndexElement*)(leftElement);
  HashIndexElement* hRightElement = (HashIndexElement*)(rightElement);
  size_t j;
  
  if (leftElement == NULL || rightElement == NULL) {
    return false;
  }    
  
  if (hLeftElement->numFields != hRightElement->numFields) {
    return false; // should never happen
  }

  for (j = 0; j < hLeftElement->numFields; j++) {
    TRI_shaped_json_t* left   = (j + hLeftElement->fields);
    TRI_shaped_json_t* right  = (j + hRightElement->fields);
    if (!isEqualShapedJsonShapedJson(left, right)) {
      return false;
    }
  }
  
  return true;
}


// .............................................................................
// The actual hashing occurs here
// .............................................................................
static uint64_t multiHashElement (struct TRI_multi_array_s* multiArray, void* element) {
  HashIndexElement* hElement = (HashIndexElement*)(element);
  uint64_t hash = TRI_FnvHashBlockInitial();
  size_t j;

  for (j = 0; j < hElement->numFields; j++) {
    hash = hashShapedJson(hash, (j + hElement->fields) );
  }
  return  hash;
}


static uint64_t multiHashKey (struct TRI_multi_array_s* multiArray, void* element) {
  HashIndexElement* hElement = (HashIndexElement*)(element);
  uint64_t hash = TRI_FnvHashBlockInitial();
  size_t j;

  for (j = 0; j < hElement->numFields; j++) {
    hash = hashShapedJson(hash, (j + hElement->fields) );
  }
  return  hash;
}




// .............................................................................
// Creates a new multi associative array
// .............................................................................

HashIndex* MultiHashIndex_new() {
  HashIndex* hashIndex;

  hashIndex = TRI_Allocate(sizeof(HashIndex));
  if (hashIndex == NULL) {
    return NULL;
  }

  hashIndex->unique = false;
  hashIndex->assocArray.nonUniqueArray = TRI_Allocate(sizeof(TRI_multi_array_t));
  if (hashIndex->assocArray.nonUniqueArray == NULL) {
    TRI_Free(hashIndex);
    return NULL;
  }    
    
  TRI_InitMultiArray(hashIndex->assocArray.nonUniqueArray,
                     sizeof(HashIndexElement),
                     multiHashKey,
                     multiHashElement,
                     multiClearElement,
                     isMultiEmptyElement,
                     isMultiEqualKeyElement,
                     isMultiEqualElementElement);
  
  return hashIndex;
}




// ...............................................................................
// Adds (inserts) a data element into the associative array (hash index)
// ...............................................................................

int MultiHashIndex_add(HashIndex* hashIndex, HashIndexElement* element) {
  bool result;
  result = TRI_InsertElementMultiArray(hashIndex->assocArray.nonUniqueArray, element, false);
  if (result) {
    return 0;
  }    
  return -1;
}


// ...............................................................................
// Locates an entry within the associative array
// ...............................................................................

HashIndexElements* MultiHashIndex_find(HashIndex* hashIndex, HashIndexElement* element) {
  TRI_vector_pointer_t result;
  HashIndexElements* results;
  size_t j;
  
  results = TRI_Allocate(sizeof(HashIndexElements));    
  /* FIXME: memory allocation might fail */
  
  // .............................................................................
  // We can only use the LookupByKey method for non-unique hash indexes, since
  // we want more than one result returned!
  // .............................................................................
  result  = TRI_LookupByKeyMultiArray (hashIndex->assocArray.nonUniqueArray, element); 

  
  if (result._length == 0) {
    results->_elements    = NULL;
    results->_numElements = 0;
  }
  else {  
    results->_numElements = result._length;
    results->_elements = TRI_Allocate(sizeof(HashIndexElement) * result._length); 
    /* FIXME: memory allocation might fail */
    for (j = 0; j < result._length; ++j) {  
      results->_elements[j] = *((HashIndexElement*)(result._buffer[j]));
    }  
  }
  TRI_DestroyVectorPointer(&result);
  return results;
}


// ...............................................................................
// An alias for addIndex 
// ...............................................................................

int MultiHashIndex_insert(HashIndex* hashIndex, HashIndexElement* element) {
  return MultiHashIndex_add(hashIndex,element);
} 


// ...............................................................................
// Removes an entry from the associative array
// ...............................................................................

int MultiHashIndex_remove(HashIndex* hashIndex, HashIndexElement* element) {
  bool result;
  result = TRI_RemoveElementMultiArray(hashIndex->assocArray.nonUniqueArray, element, NULL); 
  return result ? TRI_ERROR_NO_ERROR : TRI_ERROR_INTERNAL;
}


// ...............................................................................
// updates and entry from the associative array, first removes beforeElement, 
//  then adds the afterElement
// ...............................................................................

int MultiHashIndex_update(HashIndex* hashIndex, HashIndexElement* beforeElement, 
                           HashIndexElement* afterElement) {
  assert(false);
  return TRI_ERROR_INTERNAL;
}
