////////////////////////////////////////////////////////////////////////////////
/// @brief bitarray index
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

#include "bitarrayIndex.h"
#include "BasicsC/string-buffer.h"
#include "ShapedJson/json-shaper.h"
#include "ShapedJson/shaped-json.h"
#include "VocBase/document-collection.h"



// .............................................................................
// forward declaration of static functions used for iterator callbacks
// .............................................................................

static bool  BitarrayIndexHasNextIterationCallback (TRI_index_iterator_t*);
static void* BitarrayIndexNextIterationCallback    (TRI_index_iterator_t*);
static void* BitarrayIndexNextsIterationCallback   (TRI_index_iterator_t*, int64_t);
static bool  BitarrayIndexHasPrevIterationCallback (TRI_index_iterator_t*);
static void* BitarrayIndexPrevIterationCallback    (TRI_index_iterator_t*);
static void* BitarrayIndexPrevsIterationCallback   (TRI_index_iterator_t*, int64_t);
static void  BitarrayIndexDestroyIterator          (TRI_index_iterator_t*);
 
// .............................................................................
// forward declaration of static functions used here
// .............................................................................

static void BitarrayIndex_findHelper (BitarrayIndex*, TRI_vector_t*, TRI_index_operator_t*, TRI_index_iterator_t*);
static int  generateBitMask          (BitarrayIndex*, const BitarrayIndexElement*, TRI_bitarray_mask_t*);



// -----------------------------------------------------------------------------
// --SECTION--                           bitarrayIndex     common public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup bitarrayIndex
/// @{
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a bitarray index , but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void BitarrayIndex_destroy(BitarrayIndex* baIndex) {
  size_t j;
  if (baIndex == NULL) {
    return;
  } 
  for (j = 0;  j < baIndex->_values._length;  ++j) {
    TRI_DestroyJson(TRI_UNKNOWN_MEM_ZONE, (TRI_json_t*)(TRI_AtVector(&(baIndex->_values),j)));
  }
  TRI_DestroyVector(&baIndex->_values);
  
  TRI_FreeBitarray(baIndex->bitarray);
  baIndex->bitarray = NULL;
}


////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a bitarray index and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void BitarrayIndex_free(BitarrayIndex* baIndex) {
  if (baIndex == NULL) {
    return;
  }  
  BitarrayIndex_destroy(baIndex);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, baIndex);
}



////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////



//------------------------------------------------------------------------------
// Public Methods 
//------------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief Creates a new bitarray index. Failure will return an appropriate error code
////////////////////////////////////////////////////////////////////////////////

int BitarrayIndex_new(BitarrayIndex** baIndex, 
                      TRI_memory_zone_t* memoryZone, 
                      size_t cardinality,
                      TRI_vector_t* values,
                      bool supportUndef,
                      void* context) {
  int     result;
  size_t  numArrays;
  int j;
  
  
  // ...........................................................................  
  // Sime simple checks
  // ...........................................................................  
  
  if (baIndex == NULL) {
    assert(false);
    return TRI_ERROR_INTERNAL;
  }  
  
  
  
  // ...........................................................................  
  // If the bit array index has arealdy been created, return internal error
  // ...........................................................................  
  
  if (*baIndex != NULL) {
    return TRI_ERROR_INTERNAL;
  }


  // ...........................................................................  
  // If the memory zone is invalid, then return an internal error
  // ...........................................................................  
    
  if (memoryZone == NULL) {
    return TRI_ERROR_INTERNAL;
  }


  // ...........................................................................  
  // Create the bit array index structure
  // ...........................................................................  
  
  *baIndex = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(BitarrayIndex), true);
  if (*baIndex == NULL) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }
  

  // ...........................................................................  
  // Copy the values into this index
  // ...........................................................................  
 
   
  TRI_InitVector(&((*baIndex)->_values), TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_json_t));

  for (j = 0;  j < values->_length;  ++j) {
    TRI_json_t value;
    TRI_CopyToJson(TRI_UNKNOWN_MEM_ZONE, &value, (TRI_json_t*)(TRI_AtVector(values,j)));
    TRI_PushBackVector(&((*baIndex)->_values), &value);
  }
 
 
  // ...........................................................................  
  // Store whether or not the index supports 'undefined' documents (that is
  // documents with attributes which do not match those of the index
  // ...........................................................................  
  
  (*baIndex)->_supportUndef = supportUndef;
  
  // ...........................................................................  
  // Determine the number of bit columns which will comprise the bit array index.
  // ...........................................................................  
  
  numArrays = cardinality;
  
  // ...........................................................................  
  // Create the bit arrays
  // ...........................................................................  
  
  result = TRI_InitBitarray(&((*baIndex)->bitarray), memoryZone, numArrays, NULL);


  // ...........................................................................  
  // return the result of creating the bit  arrays
  // ...........................................................................  
  
  return result;
}



////////////////////////////////////////////////////////////////////////////////
/// @brief adds (inserts) a data element into one or more bit arrays
////////////////////////////////////////////////////////////////////////////////

int BitarrayIndex_add(BitarrayIndex* baIndex, BitarrayIndexElement* element) {
  int result;
  TRI_bitarray_mask_t mask;

  // ..........................................................................
  // At the current time we have no way in which to store undefined documents
  // need some sort of parameter passed here.
  // ..........................................................................
  
  // ..........................................................................
  // generate bit mask
  // ..........................................................................
  
  result = generateBitMask (baIndex, element, &mask);
  if (result != TRI_ERROR_NO_ERROR) {
    return result;
  }  
  
  
  // ..........................................................................
  // insert the bit mask into the bit array
  // ..........................................................................
  
  result = TRI_InsertBitMaskElementBitarray(baIndex->bitarray, &mask, element->data);  
  return result;
}



////////////////////////////////////////////////////////////////////////////////
/// @brief attempts to locate one or more documents which match an index operator
////////////////////////////////////////////////////////////////////////////////

TRI_index_iterator_t* BitarrayIndex_find(BitarrayIndex* baIndex, 
                                         TRI_index_operator_t* indexOperator,
                                         TRI_vector_t* shapeList,
                                         bool (*filter) (TRI_index_iterator_t*) ) {

  TRI_index_iterator_t* iterator;
  
  // ...........................................................................  
  // Attempt to allocate memory for the index iterator which stores the results
  // if any of the lookup.
  // ...........................................................................  
  
  iterator = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_index_iterator_t), true);
  if (iterator == NULL) {
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
    return NULL; // calling procedure needs to care when the iterator is null
  }  
  
  
  // ...........................................................................  
  // We now initialise the index iterator with all the call back functions.
  // ...........................................................................  
  
  TRI_InitVector(&(iterator->_intervals), TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_index_iterator_interval_t));
  iterator->_index           = baIndex;  
  iterator->_currentInterval = 0;  
  iterator->_cursor          = NULL;
  
  iterator->_filter          = filter; 
  iterator->_hasNext         = BitarrayIndexHasNextIterationCallback;
  iterator->_next            = BitarrayIndexNextIterationCallback;
  iterator->_nexts           = BitarrayIndexNextsIterationCallback;
  iterator->_hasPrev         = BitarrayIndexHasPrevIterationCallback;
  iterator->_prev            = BitarrayIndexPrevIterationCallback;
  iterator->_prevs           = BitarrayIndexPrevsIterationCallback;
  iterator->_destroyIterator = BitarrayIndexDestroyIterator;
  
  BitarrayIndex_findHelper(baIndex, shapeList, indexOperator, iterator);
  
  return iterator;
}



//////////////////////////////////////////////////////////////////////////////////
/// @brief alias for add Index item
//////////////////////////////////////////////////////////////////////////////////

int BitarrayIndex_insert(BitarrayIndex* baIndex, BitarrayIndexElement* element) {
  return BitarrayIndex_add(baIndex,element);
} 



//////////////////////////////////////////////////////////////////////////////////
/// @brief removes an entry from the bit arrays and master table
//////////////////////////////////////////////////////////////////////////////////

int BitarrayIndex_remove(BitarrayIndex* baIndex, BitarrayIndexElement* element) {
  int result;
  result = TRI_RemoveElementBitarray(baIndex->bitarray, element->data); 
  return result;
}



//////////////////////////////////////////////////////////////////////////////////
/// @brief updates a bit array index  entry
//////////////////////////////////////////////////////////////////////////////////

int BitarrayIndex_update(BitarrayIndex* baIndex, 
                          const BitarrayIndexElement* oldElement, 
                          const BitarrayIndexElement* newElement) {
                          
  assert(false);                          
  return TRI_ERROR_INTERNAL;
}





// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// Implementation of static functions forward declared above
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------


// .............................................................................
// forward declaration of static functions used for iterator callbacks
// .............................................................................

bool  BitarrayIndexHasNextIterationCallback(TRI_index_iterator_t* iterator) {
  assert(0);
  return false;
}

  
void* BitarrayIndexNextIterationCallback(TRI_index_iterator_t* iterator) {
  assert(0);
  return NULL;
}


void* BitarrayIndexNextsIterationCallback(TRI_index_iterator_t* iterator, int64_t jumpSize) {
  assert(0);
  return NULL;
}

  
bool BitarrayIndexHasPrevIterationCallback(TRI_index_iterator_t* iterator) {
  assert(0);
  return false;
}

  
void* BitarrayIndexPrevIterationCallback(TRI_index_iterator_t* iterator) {
  assert(0);
  return NULL;
}

  
void* BitarrayIndexPrevsIterationCallback(TRI_index_iterator_t* iterator, int64_t jumpSize) {
  assert(0);
  return NULL;
}

  
void BitarrayIndexDestroyIterator(TRI_index_iterator_t* iterator) {
  assert(0);
}  
 
 
// .............................................................................
// forward declaration of static functions used internally here
// .............................................................................

void BitarrayIndex_findHelper(BitarrayIndex* baIndex, 
                              TRI_vector_t* shapeList, 
                              TRI_index_operator_t* indexOperator, 
                              TRI_index_iterator_t* iterator) {
  assert(0);                              
}
            


////////////////////////////////////////////////////////////////////////////////
// Given the index structure and the list of shaped json values which came from
// some document we generate a bit mask.
////////////////////////////////////////////////////////////////////////////////

static bool isEqualJson(TRI_json_t* left, TRI_json_t* right) {

  if (left == NULL && right == NULL) {
    return true;
  }

  if (left == NULL || right == NULL) {
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
      return (left->_value._boolean == right->_value._boolean);
    }    
    case TRI_JSON_NUMBER: {
      return (left->_value._number == right->_value._number);
    }    
    case TRI_JSON_STRING: {
      return (strcmp(left->_value._string.data, right->_value._string.data) == 0);
    }    
    case TRI_JSON_ARRAY: {
    }    
    case TRI_JSON_LIST: {
      int j;
      if (left->_value._objects._length != right->_value._objects._length) {
        return false;
      }
      for (j = 0; j < left->_value._objects._length; ++j) {
        TRI_json_t* subLeft;
        TRI_json_t* subRight;
        subLeft  = (TRI_json_t*)(TRI_AtVector(&(left->_value._objects),j));
        subRight = (TRI_json_t*)(TRI_AtVector(&(right->_value._objects),j));
        if (isEqualJson(subLeft, subRight)) {
          continue;
        }
        return false;
      }
    }    
    default: {
      assert(false);
    }  
  }        
}
            
int generateBitMask(BitarrayIndex* baIndex, const BitarrayIndexElement* element, TRI_bitarray_mask_t* mask) {

  TRI_shaper_t* shaper;
  int j;
  int shiftLeft;
  
  // ...........................................................................
  // some safety checks first
  // ...........................................................................
  
  if (baIndex == NULL || element == NULL) {
    return TRI_ERROR_INTERNAL;
  }

  if (element->collection == NULL) {
    return TRI_ERROR_INTERNAL;
  }


  // ...........................................................................
  // We could be trying to store an 'undefined' document into the bitarray
  // We determine this implicitly. If element->numFields b == 0, then we
  // assume that the document did not have any matching attributes, yet since
  // we are here we wish to store this fact.
  // ...........................................................................
  
  if (!baIndex->_supportUndef && (element->numFields == 0 || element->fields == NULL)) {
    return TRI_ERROR_INTERNAL;
  }

  
  if (baIndex->_supportUndef && element->numFields == 0) {
    mask->_mask       = 1;
    mask->_ignoreMask = 0;
    return TRI_ERROR_NO_ERROR;
  }  

  
  // ...........................................................................
  // attempt to convert the stored TRI_shaped_json_t into TRI_Json_t so that
  // we can make a comparison between what values the bitarray index requires
  // and what values the document has sent.
  // ...........................................................................
  
  shaper      = ((TRI_doc_collection_t*)(element->collection))->_shaper;
  mask->_mask = 0;
  shiftLeft   = 0;
  
  for (j = 0; j < baIndex->_values._length; ++j) {
    TRI_json_t* valueList;
    TRI_json_t* value;
    uint64_t    other;    
    int         i;
    uint64_t    tempMask;
    
    value      = TRI_JsonShapedJson(shaper, &(element->fields[j])); // from shaped json to simple json   
    valueList  = (TRI_json_t*)(TRI_AtVector(&(baIndex->_values),j));
    other      = 0;
    tempMask   = 0;
    
    for (i = 0; i < valueList->_value._objects._length; ++i) {
      TRI_json_t* listEntry = (TRI_json_t*)(TRI_AtVector(&(valueList->_value._objects), i));                 
      // .......................................................................
      // We need to take special care if the json object is a list
      // .......................................................................
      
      if (listEntry->_type == TRI_JSON_LIST) {
        // an empty list
        if (listEntry->_value._objects._length == 0) {
          other = (1 << i);
        }
        else {
          int k;
          for (k = 0; k < listEntry->_value._objects._length; k++) {
            TRI_json_t* subListEntry;
            subListEntry = (TRI_json_t*)(TRI_AtVector(&(listEntry->_value._objects), i));            
            if (isEqualJson(value, subListEntry)) {
              tempMask = tempMask | (1 << i);
            }  
          }          
        }
      }      
      
      else {
        if (isEqualJson(value, listEntry)) {
          tempMask = tempMask | (1 << i);
        }  
      }
  
  
      // ..........................................................................
      // remove the json entry created from the shaped json
      // ..........................................................................
    
      TRI_DestroyJson(shaper->_memoryZone, value);
      
    }
      
    if (tempMask == 0) {
      tempMask = other;
    }
    
    
    mask->_mask = mask->_mask | (tempMask << shiftLeft);
    
    shiftLeft += valueList->_value._objects._length;
  }  
  
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:


