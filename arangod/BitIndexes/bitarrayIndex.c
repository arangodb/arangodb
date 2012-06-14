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
  if (baIndex == NULL) {
    return;
  }   
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
                      bool addOtherColumn,
                      bool addUndefinedColumn,
                      void* context) {
  int     result;
  size_t  numArrays;

  
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
  // Determine the number of bit columns which will comprise the bit array index.
  // ...........................................................................  
  
  numArrays = cardinality;
  
  if (addOtherColumn) {
    ++numArrays;
  }

  if (addUndefinedColumn) {
    ++numArrays;
  }    

  
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

bool BitarrayIndex_update(BitarrayIndex* baIndex, 
                          const BitarrayIndexElement* oldElement, 
                          const BitarrayIndexElement* newElement) {
                          
  int result;                          
  TRI_bitarray_mask_t oldMask;
  TRI_bitarray_mask_t newMask;
                          
  // ..........................................................................
  // updates an entry in the bit arrays and master table
  // essentially it replaces the bit mask with a new bit mask
  // ..........................................................................


  // ..........................................................................
  // Check that something has not gone terribly wrong
  // ..........................................................................
  
  if (oldElement->data != newElement->data) {
    TRI_set_errno(TRI_ERROR_INTERNAL);
    return false;
  }

  
  // ..........................................................................
  // generate bit masks
  // ..........................................................................
  
  result = generateBitMask (baIndex, oldElement, &oldMask);
  if (result != TRI_ERROR_NO_ERROR) {
    TRI_set_errno(result);
    return false;
  }  

  result = generateBitMask (baIndex, newElement, &newMask);
  if (result != TRI_ERROR_NO_ERROR) {
    TRI_set_errno(result);
    return false;
  }  

  
  // ..........................................................................
  // replace the old mask with the new mask into the bit array
  // ..........................................................................
  
  result = TRI_ReplaceBitMaskElementBitarray(baIndex->bitarray, &oldMask, &newMask, oldElement->data);  
  if (result != TRI_ERROR_NO_ERROR) {
    TRI_set_errno(result);
    return false;
  }  
  
  return true;
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
            

            
int generateBitMask(BitarrayIndex* baIndex, const BitarrayIndexElement* element, TRI_bitarray_mask_t* mask) {
  assert(0);
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:


