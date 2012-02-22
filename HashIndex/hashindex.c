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

static bool isEqualJsonJson (const TRI_json_t* left, const TRI_json_t* right) {

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
      
      for (size_t j = 0; j < left->_value._objects._length; ++j) {
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
      
      for (size_t j = 0; j < left->_value._objects._length; ++j) {
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



static size_t hashJson (const size_t hash, const TRI_json_t* data) {

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
      return TRI_BlockCrc32(hash, (const char*)(&(data->_value._boolean)), sizeof(bool));
    }  
    
    case TRI_JSON_NUMBER: {
      return TRI_BlockCrc32(hash, (const char*)(&(data->_value._number)), sizeof(double));
    }  
       
    case TRI_JSON_STRING: {
      if (data->_value._string.length > 0) {
        return TRI_BlockCrc32(hash, data->_value._string.data, data->_value._string.length);
      }  
      return hash;
    }      
    
    
    case TRI_JSON_ARRAY: {
      // order not defined here -- need special case here
      if (data->_value._objects._length > 0) {
        newHash = hash;
        for (size_t j = 0; j < data->_value._objects._length; ++j) {
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
        for (size_t j = 0; j < data->_value._objects._length; ++j) {
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

// .............................................................................
// .............................................................................
static void clearElement(struct TRI_associative_array_s* associativeArray, void* element) {
  HashIndexElement* hElement = (HashIndexElement*)(element);
  if (element != NULL) {
    hElement->data = 0;
  }  
}


// .............................................................................
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

  for (size_t j = 0; j < hLeftElement->numFields; j++) {
    TRI_json_t* left   = (j + hLeftElement->fields);
    TRI_json_t* right  = (j + hRightElement->fields);
    if (!isEqualJsonJson(left, right)) {
      return false;
    }
  }
  
  return true;
}


// .............................................................................
// .............................................................................
static bool isEqualKeyElement (struct TRI_associative_array_s* associativeArray, 
                               void* leftElement, void* rightElement) {
  HashIndexElement* hLeftElement  = (HashIndexElement*)(leftElement);
  HashIndexElement* hRightElement = (HashIndexElement*)(rightElement);
  
  if (leftElement == NULL || rightElement == NULL) {
    return false;
  }    
  
  if (hLeftElement->numFields != hRightElement->numFields) {
    return false; // should never happen
  }

  for (size_t j = 0; j < hLeftElement->numFields; j++) {
    TRI_json_t* left   = (j + hLeftElement->fields);
    TRI_json_t* right  = (j + hRightElement->fields);
    if (!isEqualJsonJson(left, right)) {
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
  size_t hash = TRI_InitialCrc32();
  
  for (size_t j = 0; j < hElement->numFields; j++) {
    hash = hashJson(hash, (j + hElement->fields) );
  }
  
  hash = TRI_FinalCrc32(hash);
  //printf("%s:%u:%u\n",__FILE__,__LINE__,hash);
  return  hash;
}


static uint64_t hashKey (struct TRI_associative_array_s* associativeArray, void* element) {
  HashIndexElement* hElement = (HashIndexElement*)(element);
  size_t hash = TRI_InitialCrc32();
  
  for (size_t j = 0; j < hElement->numFields; j++) {
    hash = hashJson(hash, (j + hElement->fields) );
  }
  
  hash = TRI_FinalCrc32(hash);
  return  hash;
}




// .............................................................................
// Creates a new associative array
// .............................................................................

HashIndex* HashIndex_new() {
  HashIndex* hashIndex;

  hashIndex = malloc(sizeof(HashIndex));
  if (hashIndex == NULL) {
    return NULL;
  }

  hashIndex->associativeArray = malloc(sizeof(TRI_associative_array_t));
  if (hashIndex->associativeArray == NULL) {
    free(hashIndex);
    return NULL;
  }    
    
  TRI_InitAssociativeArray(hashIndex->associativeArray,
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
  result = TRI_InsertElementAssociativeArray(hashIndex->associativeArray, element, false);
  if (result) {
    return 0;
  }    
  return -1;
}


// ...............................................................................
// Locates an entry within the associative array
// ...............................................................................

HashIndexElements* HashIndex_find(HashIndex* hashIndex, HashIndexElement* element) {
  HashIndexElement* result;
  HashIndexElements* elements;

  elements = TRI_Allocate(sizeof(HashIndexElements));    
  
  result = (HashIndexElement*) (TRI_FindByElementAssociativeArray(hashIndex->associativeArray, element)); 
  
  if (result != NULL) {
    elements->_elements    = TRI_Allocate(sizeof(HashIndexElement*) * 1); // unique hash index maximum number is 1
    elements->_elements[0] = result;
    elements->_numElements = 1;
  }
  else {
    elements->_elements    = NULL;
    elements->_numElements = 0;
  }
  return elements;
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

bool HashIndex_removeIndex(HashIndex* hashIndex, const HashIndexElement* element) {
  assert(false);
  return false;
}


// ...............................................................................
// updates and entry from the associative array, first removes beforeElement, 
//  then adds the afterElement
// ...............................................................................

bool HashIndex_update(HashIndex* hashIndex, const HashIndexElement* beforeElement, 
                      const HashIndexElement* afterElement) {
  assert(false);
  return false;                      
}
