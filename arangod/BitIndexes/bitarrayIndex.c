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




static int generateBitMask (BitarrayIndex* baIndex, BitarrayIndexElement* element, TRI_bitarray_mash_t* mask);

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
// Public Methods Unique Skiplists
//------------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new bitarray index
////////////////////////////////////////////////////////////////////////////////

int BitarrayIndex_new(BitarrayIndex** baIndex, 
                      TRI_memory_zone_t* memoryZone, 
                      size_t cardinality,
                      bool addOtherColumn,
                      bool addUndefinedColumn,
                      void* context) {
  int     result;
  size_t  numArrays;
  
  if (*baIndex != NULL) {
    return TRI_ERROR_INTERNAL;
  }
  
  *baIndex = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(BitarrayIndex), true);
  if (*baIndex == NULL) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }
  
  numArrays = cardinality;
  
  if (addOtherColumn) {
    ++numArrays;
  }

  if (addUndefinedColumn) {
    ++numArrays;
  }    

  result = TRI_InitBitarray(&((*baIndex)->bitarray), memoryZone, numArrays, NULL);

  return result;
}



////////////////////////////////////////////////////////////////////////////////
/// @brief adds (inserts) a data element into one or more bit arrays
////////////////////////////////////////////////////////////////////////////////

int BitarrayIndex_add(BitarrayIndex* baIndex, BitarrayIndexElement* element) {
  int result;
  TRI_bitarray_mash_t mask;
  
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
  
  result = TRI_InsertBitMaskBitarray(baIndex->bitarray, &mask, element->data);  
  return result;
}


////////////////////////////////////////////////////////////////////////////////
/// @brief attempts to locate one or more documents which match an index operator
////////////////////////////////////////////////////////////////////////////////

int BitarrayIndex_find(BitarrayIndex* baIndex, TRI_sl_operator_t* slOperator, BitarrayIndexElements* elements) {
  results = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_skiplist_iterator_t), true);
  if (results == NULL) {
    return NULL; // calling procedure needs to care when the iterator is null
  }  
  results->_index = skiplistIndex;
  TRI_InitVector(&(results->_intervals), TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_skiplist_iterator_interval_t));
  results->_currentInterval = 0;
  results->_cursor          = NULL;
  results->_hasNext         = SkiplistHasNextIterationCallback;
  results->_next            = SkiplistNextIterationCallback;
  results->_nexts           = SkiplistNextsIterationCallback;
  results->_hasPrev         = SkiplistHasPrevIterationCallback;
  results->_prev            = SkiplistPrevIterationCallback;
  results->_prevs           = SkiplistPrevsIterationCallback;
  
  SkiplistIndex_findHelper(skiplistIndex, shapeList, slOperator, &(results->_intervals));
  
  return results;
}



//////////////////////////////////////////////////////////////////////////////////
/// @brief alias for add Index item
//////////////////////////////////////////////////////////////////////////////////

int BitarrayIndex_insert(BitarrayIndex* baIndex, BitarrayIndexElement* element) {
  return BitarrayIndex_add(ba,element);
} 



//////////////////////////////////////////////////////////////////////////////////
/// @brief removes an entry from the bit arrays and master table
//////////////////////////////////////////////////////////////////////////////////

int BitarrayIndex_remove(BitarrayIndex* baIndex, BitarrayIndexElement* element) {
  int result;
  result = TRI_RemoveElementBitarray(baIndex->bitarray, element, NULL); 
  return result;
}



//////////////////////////////////////////////////////////////////////////////////
/// @brief updates a bit array index  entry
//////////////////////////////////////////////////////////////////////////////////

bool BitarrayIndex_update(BitarrayIndex* baIndex, 
                          const BitarrayIndexElement* oldElement, 
                          const BitarrayIndexElement* newElement) {
  TRI_bitarray_mash_t oldMask;
  TRI_bitarray_mash_t newMask;
                          
  // ..........................................................................
  // updates an entry in the bit arrays and master table
  // essentially it replaces the bit mask with a new bit mask
  // ..........................................................................


  // ..........................................................................
  // Check that something has not gone terribly wrong
  // ..........................................................................
  
  if (oldElement->data != newElement->data) {
    return TRI_ERROR_INTERNAL; // todo: use appropriate error code
  }

  
  // ..........................................................................
  // generate bit masks
  // ..........................................................................
  
  result = generateBitMask (baIndex, oldElement, &oldMask);
  if (result != TRI_ERROR_NO_ERROR) {
    return result;
  }  

  result = generateBitMask (baIndex, newElement, &newMask);
  if (result != TRI_ERROR_NO_ERROR) {
    return result;
  }  

  
  // ..........................................................................
  // insert the bit mask into the bit array
  // ..........................................................................
  
  result = TRI_ReplaceBitMaskBitarray(baIndex->bitarray, oldMask, newMask, oldElement->data);  
  
  return result;
}






////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:


