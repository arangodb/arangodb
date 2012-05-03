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



// -----------------------------------------------------------------------------
// destructors public functions
// -----------------------------------------------------------------------------


////////////////////////////////////////////////////////////////////////////////
/// @brief destroys the hash index by calling the hash array's own Free function 
////////////////////////////////////////////////////////////////////////////////

void HashIndex_destroy(HashIndex* hashIndex) {
  if (hashIndex != NULL) {
    TRI_FreeHashArray(hashIndex->hashArray);
  }
}


////////////////////////////////////////////////////////////////////////////////
/// @brief destroys the hash index and frees the memory associated with the index structure
////////////////////////////////////////////////////////////////////////////////

void HashIndex_free(HashIndex* hashIndex) {
  if (hashIndex != NULL) {
    HashIndex_destroy(hashIndex);
    TRI_Free(hashIndex);  
  }
}




// -----------------------------------------------------------------------------
// constructors public functions
// -----------------------------------------------------------------------------


////////////////////////////////////////////////////////////////////////////////
/// @brief Creates a new hash array used for storage of elements in the hash index
////////////////////////////////////////////////////////////////////////////////


HashIndex* HashIndex_new() {
  HashIndex* hashIndex;

  hashIndex = TRI_Allocate(sizeof(HashIndex));
  if (hashIndex == NULL) {
    return NULL;
  }

  hashIndex->unique = true;
  hashIndex->hashArray = TRI_Allocate(sizeof(TRI_hasharray_t));
  if (hashIndex->hashArray == NULL) {
    TRI_Free(hashIndex);
    return NULL;
  }    
    
  if (! TRI_InitHashArray(hashIndex->hashArray, sizeof(HashIndexElement), NULL, NULL, NULL, NULL, NULL, NULL) ) {
    HashIndex_free(hashIndex);
    return NULL;    
  }
  
  return hashIndex;
}



// -----------------------------------------------------------------------------
// public functions : INSERT, REMOVE & LOOKUP
// -----------------------------------------------------------------------------


////////////////////////////////////////////////////////////////////////////////
/// @brief Adds (inserts) a data element into the hash array part of the hash index
////////////////////////////////////////////////////////////////////////////////

int HashIndex_add(HashIndex* hashIndex, HashIndexElement* element) {
  bool result;  

  // .............................................................................
  // Since we do not allow duplicates - we must compare using keys, rather than
  // documents.
  // .............................................................................
  
  result = TRI_InsertKeyHashArray(hashIndex->hashArray, element, element, false);
  return result ? TRI_ERROR_NO_ERROR : TRI_ERROR_AVOCADO_UNIQUE_CONSTRAINT_VIOLATED;
}


////////////////////////////////////////////////////////////////////////////////
/// @brief Locates an entry within the hash array part of the hash index
////////////////////////////////////////////////////////////////////////////////

HashIndexElements* HashIndex_find(HashIndex* hashIndex, HashIndexElement* element) {
  HashIndexElement* result;
  HashIndexElements* results;

  
  // .............................................................................
  // Create storage for the returned results if any. A return of NULL indicates
  // some sort of error.
  // .............................................................................
  
  results = TRI_Allocate(sizeof(HashIndexElements));    
  if (results == NULL) {
    return NULL;
  }  
  
  
  // .............................................................................
  // A find request means that a set of values for the "key" was sent. We need
  // to locate the hash array entry by key.
  // .............................................................................
  
  result = (HashIndexElement*) (TRI_FindByKeyHashArray(hashIndex->hashArray, element)); 
  
  if (result != NULL) {
    results->_elements = TRI_Allocate(sizeof(HashIndexElement) * 1); // unique hash index maximum number is 1
    if (results->_elements == NULL) {
      TRI_Free(results);
      return NULL;
    }  
    results->_elements[0] = *result;
    results->_numElements = 1;
  }
  else {
    results->_elements    = NULL;
    results->_numElements = 0;
  }
  return results;
}


////////////////////////////////////////////////////////////////////////////////
/// @brief An alias for HashIndex_add 
////////////////////////////////////////////////////////////////////////////////

int HashIndex_insert(HashIndex* hashIndex, HashIndexElement* element) {
  return HashIndex_add(hashIndex,element);
} 


////////////////////////////////////////////////////////////////////////////////
/// @brief Removes an entry from the hash array part of the hash index
////////////////////////////////////////////////////////////////////////////////

int HashIndex_remove(HashIndex* hashIndex, HashIndexElement* element) {
  bool result;

  result = TRI_RemoveElementHashArray(hashIndex->hashArray, element); 
  return result ? TRI_ERROR_NO_ERROR : TRI_ERROR_INTERNAL;
}


////////////////////////////////////////////////////////////////////////////////
/// @brief updates and entry from the associative array, first removes beforeElement, 
/// @brief then adds the afterElement
////////////////////////////////////////////////////////////////////////////////

int HashIndex_update(HashIndex* hashIndex, const HashIndexElement* beforeElement, 
                      const HashIndexElement* afterElement) {
  // ...........................................................................
  // This function is not currently implemented and must not be called.
  // It's purpose would be to remove the existing beforeElement and replace it
  // with the afterElement.
  // ...........................................................................
  
  assert(false);
  return TRI_ERROR_INTERNAL;                      
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Multi-hash non-unique hash indexes
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------



// -----------------------------------------------------------------------------
// destructors public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys the hash index by calling the hash array's own Free function 
////////////////////////////////////////////////////////////////////////////////

void MultiHashIndex_destroy(HashIndex* hashIndex) {
  HashIndex_destroy(hashIndex);
}


////////////////////////////////////////////////////////////////////////////////
/// @brief destroys the hash index and frees the memory associated with the index structure
////////////////////////////////////////////////////////////////////////////////

void MultiHashIndex_free(HashIndex* hashIndex) {
  HashIndex_free(hashIndex);
}


// -----------------------------------------------------------------------------
// constructors public functions
// -----------------------------------------------------------------------------


////////////////////////////////////////////////////////////////////////////////
/// @brief Creates a new multi (non-unique) hash index
////////////////////////////////////////////////////////////////////////////////

HashIndex* MultiHashIndex_new() {
  HashIndex* hashIndex;

  hashIndex = TRI_Allocate(sizeof(HashIndex));
  if (hashIndex == NULL) {
    return NULL;
  }

  hashIndex->unique = false;
  hashIndex->hashArray = TRI_Allocate(sizeof(TRI_hasharray_t));
  if (hashIndex->hashArray == NULL) {
    TRI_Free(hashIndex);
    return NULL;
  }    
    
  if (! TRI_InitHashArray(hashIndex->hashArray, sizeof(HashIndexElement), NULL, NULL, NULL, NULL, NULL, NULL) ) {
    HashIndex_free(hashIndex);
    return NULL;    
  }

  return hashIndex;
}


// -----------------------------------------------------------------------------
// public functions : INSERT, REMOVE & LOOKUP
// -----------------------------------------------------------------------------


////////////////////////////////////////////////////////////////////////////////
/// @brief Adds (inserts) a data element into the hash array (hash index)
////////////////////////////////////////////////////////////////////////////////

int MultiHashIndex_add(HashIndex* hashIndex, HashIndexElement* element) {
  bool result;
  result = TRI_InsertElementHashArrayMulti(hashIndex->hashArray, element, false);
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
  if (results == NULL) {
    return NULL;
  }
  
  
  // .............................................................................
  // We can only use the LookupByKey method for non-unique hash indexes, since
  // we want more than one result returned!
  // .............................................................................
  
  result = TRI_LookupByKeyHashArrayMulti (hashIndex->hashArray, element); 

  
  if (result._length == 0) {
    results->_elements    = NULL;
    results->_numElements = 0;
  }
  else {  
    results->_numElements = result._length;
    results->_elements = TRI_Allocate(sizeof(HashIndexElement) * result._length); 
    if (results->_elements == NULL) {
      TRI_Free(results);
      return NULL;
    }  
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
  result = TRI_RemoveElementHashArrayMulti(hashIndex->hashArray, element); 
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
