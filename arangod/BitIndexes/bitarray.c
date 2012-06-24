////////////////////////////////////////////////////////////////////////////////
/// @brief bitarray implementation
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2012 triagens GmbH, Cologne, Germany
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
/// @author Dr. O
/// @author Copyright 2006-2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "BasicsC/common.h"
#include <BasicsC/logging.h>
#include <BasicsC/random.h>

#include "bitarray.h"

#if BITARRAY_MASTER_TABLE_BLOCKSIZE==8  
typedef uint8_t bit_column_int_t;  
#elif BITARRAY_MASTER_TABLE_BLOCKSIZE==16  
typedef uint16_t bit_column_int_t;  
#elif BITARRAY_MASTER_TABLE_BLOCKSIZE==32  
typedef  uint32_t bit_column_int_t;  
#elif BITARRAY_MASTER_TABLE_BLOCKSIZE==64  
typedef  uint64_t bit_column_int_t;  
#else
#error unknown bitarray master table blocksize (BITARRAY_MASTER_TABLE_BLOCKSIZE)
#endif   

#include "masterblocktable.h"


// -----------------------------------------------------------------------------
// --SECTION--                                                          BITARRAY
// -----------------------------------------------------------------------------


////////////////////////////////////////////////////////////////////////////////
// A bit column is the actual bit array as a list of integers of the size
// table blocksize
////////////////////////////////////////////////////////////////////////////////



typedef struct BitColumn_s {
  bit_column_int_t* _column;  
} BitColumn_t;


// -----------------------------------------------------------------------------
// --SECTION--                                       STATIC FORWARD DECLARATIONS
// -----------------------------------------------------------------------------

static int extendColumns (TRI_bitarray_t*, size_t); 

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup bitarray
/// @{
////////////////////////////////////////////////////////////////////////////////



////////////////////////////////////////////////////////////////////////////////
/// @brief creates a sequence of bit arrays and their associated master table
////////////////////////////////////////////////////////////////////////////////

int TRI_InitBitarray(TRI_bitarray_t** bitArray, 
                     TRI_memory_zone_t* memoryZone,
                     size_t numArrays,
                     void* masterTableBlock) {
  MasterTable_t* mt;
  BitColumn_t* column;
  int result;
  int j;
  bool ok;
  
  
  // ...........................................................................
  // The memory for the bitArray is allocated here. We expect a NULL pointer to
  // be passed here.
  // ...........................................................................
  
  if (*bitArray != NULL) {
    return TRI_ERROR_INTERNAL;
  }  
  
  
  // ...........................................................................
  // Allocate the necessary memory to store the bitArray structure.
  // ...........................................................................
  
  *bitArray = TRI_Allocate(memoryZone, sizeof(TRI_bitarray_t), false);
  
  if (*bitArray == NULL) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

    
  // ...........................................................................
  // Store the number of columns in this set of bit arrays.
  // ...........................................................................
  
  (*bitArray)->_numColumns   = numArrays;

  
  // ...........................................................................
  // Attempt to allocate memory to create the column structures
  // ...........................................................................
  
  (*bitArray)->_memoryZone   = memoryZone;
  (*bitArray)->_columns      = TRI_Allocate(memoryZone, sizeof(BitColumn_t) * numArrays, true);    

  if ((*bitArray)->_columns == NULL) {
    TRI_Free(memoryZone,*bitArray);
    *bitArray = NULL;
    return TRI_ERROR_OUT_OF_MEMORY;
  }
  
  
  // ...........................................................................
  // For now each set of bit arrays will create and use its own MasterBlockTable.
  // Create the MasterBlockTable here.
  // ...........................................................................
  
  if (masterTableBlock != NULL) {
    assert(false);
  }  
  
  else {
    mt = NULL;
    result = createMasterTable(&mt, memoryZone); 
    if (result != TRI_ERROR_NO_ERROR) {
      TRI_Free(memoryZone, (*bitArray)->_columns);
      TRI_Free(memoryZone, *bitArray);
      *bitArray = NULL;
      return result;
    }  
    (*bitArray)->_masterTable = mt;
  }
  
  
  // ...........................................................................
  // Create the bitarrays (the columns which will contain the bits)
  // For each column we allocated the initialise size.
  // ...........................................................................

  ok = true;
  for (j = 0; j < (*bitArray)->_numColumns; ++j) {
    column = (BitColumn_t*)((*bitArray)->_columns + (sizeof(BitColumn_t) * j));
    column->_column = TRI_Allocate(memoryZone, sizeof(bit_column_int_t) * BITARRAY_INITIAL_SIZE, true);    
    if (column->_column == NULL) {
      ok = false;
      break;
    }  
  }
  
  
  if (! ok) { 
  
    // ...........................................................................
    // Oops -- memory failure. Return all allocated memory and make a quick escape.
    // ...........................................................................
    
    for (j = 0; j < (*bitArray)->_numColumns; ++j) {
      column = (BitColumn_t*)((*bitArray)->_columns + (sizeof(BitColumn_t) * j));
      if (column->_column != NULL) {
        TRI_Free(memoryZone,column->_column);
      }  
    }
    TRI_Free(memoryZone, (*bitArray)->_columns);
    TRI_Free(memoryZone, *bitArray);
    *bitArray = NULL;
    return TRI_ERROR_OUT_OF_MEMORY;
  }
  
  // ...........................................................................
  // Everything ok
  // ...........................................................................
  
  return TRI_ERROR_NO_ERROR;
}


////////////////////////////////////////////////////////////////////////////////
/// @brief Destroy all memory allocated within the bitarray structure 
////////////////////////////////////////////////////////////////////////////////

int TRI_DestroyBitarray(TRI_bitarray_t* ba) {
  MasterTable_t* mt = (MasterTable_t*)(ba->_masterTable);
  destroyMasterTable(mt);
  // destroy the columns etc
  
  return TRI_ERROR_NO_ERROR;
} 

int TRI_FreeBitarray(TRI_bitarray_t* ba) {
  int result;
  
  result = TRI_DestroyBitarray(ba);
  TRI_Free(ba->_memoryZone,ba);
  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Inserts a bit mask into the bit array columns
////////////////////////////////////////////////////////////////////////////////

int TRI_InsertBitMaskElementBitarray(TRI_bitarray_t* ba, TRI_bitarray_mask_t* mask, void* element) {
  MasterTable_t* mt = (MasterTable_t*)(ba->_masterTable);
  TRI_master_table_position_t position;
  int result;
  size_t newLength;
  int j;
  BitColumn_t* column;
  bit_column_int_t bitInteger;
  bit_column_int_t tempBitInteger;
  uint64_t bitMaskInteger;
  
  if (ba == NULL || mask == NULL || element == NULL) {
    return TRI_ERROR_INTERNAL;
  }
  
  // ..........................................................................  
  // Ensure that the element we are going to insert into the MasterBlockTable
  // and the bitarrays is not already there.
  // ..........................................................................  
  
  if (TRI_FindByKeyAssociativeArray(&(mt->_tablePosition),element) != NULL) {
    return TRI_ERROR_INTERNAL; // Todo: return correct error
  }  

  // ..........................................................................  
  // The insertion into the MasterBlockTable occurs first and has priority 
  // ..........................................................................  
  
  position._blockNum   = 0;
  position._bitNum     = 0;
  position._vectorNum  = 0;
  position._docPointer = element; // assign the document pointer 
  result = insertMasterTable(mt, &position); 

  if (result != TRI_ERROR_NO_ERROR) {
    return result;
  }

  
  // ...........................................................................
  // locate the position in the bitarrays, extend the bit arrays if necessary
  // ...........................................................................
  
  newLength = (BITARRAY_MASTER_TABLE_BLOCKSIZE * position._blockNum) + position._bitNum;

  if (ba->_columnLength <= newLength) {
    result = extendColumns(ba, newLength);
    if (result != TRI_ERROR_NO_ERROR) {
      return result;
    }      
  }

  
  // ...........................................................................
  // Use the mask to set the bits in each column to 0 or 1
  // ...........................................................................
  
  for (j = 0; j < ba->_numColumns; ++j) {
  
    // .........................................................................
    // Extract the particular column
    // .........................................................................
    
    column = (BitColumn_t*)(ba->_columns + (sizeof(BitColumn_t) * j));
    
    
    // .........................................................................
    // Extract the integer which represents a block 
    // .........................................................................
    
    bitInteger = *(column->_column + position._blockNum);

    
    // .........................................................................
    // Determine if the bit in the bit mask is 0 or 1
    // .........................................................................
    
    tempBitInteger = ((uint64_t)(1) << j);
    bitMaskInteger = (tempBitInteger & mask->_mask) >> j;
    
    if (bitMaskInteger == 0) {
      // we want a '0' in the offset position within this bit integer
      tempBitInteger = (bit_column_int_t)(1) << position._bitNum;
      tempBitInteger = ~tempBitInteger;
      bitInteger     = bitInteger & tempBitInteger;
    }
    
    else if (bitMaskInteger == 1) {
      // we want a '1' in the offset position within this bit integer
      tempBitInteger = (bit_column_int_t)(1) << position._bitNum;
      bitInteger     = bitInteger | tempBitInteger;
    }
    
    else {
      // should never occur
      assert(false);
    }
    
    // todo: remove  necessity of intermeidate bitinteger
    *(column->_column + position._blockNum) = bitInteger;
  }
  
  return TRI_ERROR_NO_ERROR;
}  



////////////////////////////////////////////////////////////////////////////////
/// @brief Given a bit mask returns a list of document pointers
////////////////////////////////////////////////////////////////////////////////

int TRI_LookupBitMaskBitarray(TRI_bitarray_t* ba, TRI_bitarray_mask_t* bm, TRI_vector_pointer_t* resultStorage ) {
  int result;
  BitColumn_t* column;
  size_t numBlocks;   
  uint8_t offsetLastBlock;
  uint8_t numBits;
  bit_column_int_t bitInteger;
  int i_blockNum,j_bitNum,k_colNum;
  TRI_master_table_position_t position;
  bit_column_int_t maskValue;
  
  // ...........................................................................
  // TODO: we need to add an 'undefined' special column. If this column is NOT 
  // set, then we do not bother checking the rest of columns.
  // ...........................................................................
  
  
  // ...........................................................................
  // Determine the number of blocks which we have to scan and the offset of
  // the last block
  // ...........................................................................
  
  numBlocks = (ba->_columnLength) / sizeof(bit_column_int_t);
  offsetLastBlock = (uint8_t)(ba->_columnLength - ((ba->_columnLength) * sizeof(bit_column_int_t)));
  if (offsetLastBlock != 0) {
    ++numBlocks;
  }
  
  
  // ...........................................................................
  // Check and see if the mask matches with row for a particular column
  // ...........................................................................

  numBits = sizeof(bit_column_int_t);

  for (i_blockNum = 0; i_blockNum < numBlocks; ++i_blockNum) {  

    if (i_blockNum == numBlocks - 1) {
      numBits = offsetLastBlock;
    }    
    
    // .........................................................................
    // scan down the bit integer
    // .........................................................................
    
    for (j_bitNum = 0; j_bitNum < numBits; ++j_bitNum) {
        
      for (k_colNum = 0; k_colNum < ba->_numColumns; ++k_colNum) {
      
        // .....................................................................
        // If the kth column is set to ignore, then ignore it
        // .....................................................................
      
        if ((bm->_ignoreMask >> k_colNum) == 1) {
          continue;
        }

        maskValue = (bit_column_int_t)(bm->_mask >> k_colNum);
        
        // .......................................................................
        // Extract the particular column
        // .......................................................................
    
        column = (BitColumn_t*)(ba->_columns + (sizeof(BitColumn_t) * k_colNum));
      
        // .......................................................................
        // Obtain the integer representation of this block
        // .......................................................................
    
        bitInteger = *(column->_column + i_blockNum);
      
        if ((bitInteger >> j_bitNum) != maskValue) {
          continue;
        }
        
        // .......................................................................
        // Ok, this column can not be ignored and matches, so we add it
        // .......................................................................
        position._blockNum = i_blockNum;
        position._bitNum   = j_bitNum;
        
        result = storeElementMasterTable(ba->_masterTable, resultStorage, &position);
        if (result != TRI_ERROR_NO_ERROR) {
          return result;
        }  
      }  
    }
  }

  return TRI_ERROR_NO_ERROR;
}
  
////////////////////////////////////////////////////////////////////////////////
/// @brief Remove an entry from a sequence of bitarray columns
////////////////////////////////////////////////////////////////////////////////

int TRI_RemoveElementBitarray(TRI_bitarray_t* ba, void* element) {
  TRI_master_table_position_t* position;
  bool compactColumns;
  MasterTable_t* mt;
  
  // ..........................................................................
  // Attempt to locate the position of the element within the Master Block table
  // ..........................................................................
 
  mt = (MasterTable_t*)(ba->_masterTable);
  position = (TRI_master_table_position_t*)(TRI_FindByKeyAssociativeArray(&(mt->_tablePosition),element));
 
  if (position == NULL) {
    return TRI_ERROR_INTERNAL; // Todo: return correct error
  }  

  removeElementMasterTable(mt, position, &compactColumns);  
  
  return TRI_ERROR_NO_ERROR;
}



////////////////////////////////////////////////////////////////////////////////
/// @brief For a data element replaces one bit mask with another
////////////////////////////////////////////////////////////////////////////////

int TRI_ReplaceBitMaskElementBitarray (TRI_bitarray_t* ba , 
                                       TRI_bitarray_mask_t* oldMask, 
                                       TRI_bitarray_mask_t* newMask, 
                                       void* element) {
  assert(0);
  return TRI_ERROR_NO_ERROR;
}                                       


////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
/// @addtogroup bitarray
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// IMPLEMENTATION OF STATIC FORWARD DECLARED FUNCTIONS
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
/// @brief inserts a bitmask into a bit array
////////////////////////////////////////////////////////////////////////////////

static int extendColumns(TRI_bitarray_t* ba, size_t newLength) {
  bool ok = true;
  int j;
  void* newColumns;
  BitColumn_t* column;
  size_t newIntegerSize;
  
  newColumns = TRI_Allocate(ba->_memoryZone, sizeof(BitColumn_t) * ba->_numColumns, true);    
  if (newColumns == NULL) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  if (newLength < (1.2 * ba->_columnLength)) {
    newLength = 1.2 * ba->_columnLength;
  }
  
  newIntegerSize = (newLength / sizeof(bit_column_int_t)) + 1;
  for (j = 0; j < ba->_numColumns; ++j) {
    column = (BitColumn_t*)(newColumns + (sizeof(BitColumn_t) * j));
    column->_column = TRI_Allocate(ba->_memoryZone, newIntegerSize, true);    
    if (column->_column == NULL) {
      ok = false;
      break;
    }  
  }

  if (! ok) { // memory allocation failure
    for (j = 0; j < ba->_numColumns; ++j) {
      column = (BitColumn_t*)(newColumns + (sizeof(BitColumn_t) * j));
      if (column->_column != NULL) {
        TRI_Free(ba->_memoryZone,column->_column);
      }  
    }
    TRI_Free(ba->_memoryZone, newColumns);
    return TRI_ERROR_OUT_OF_MEMORY;
  }
  
  for (j = 0; j < ba->_numColumns; ++j) {
    column = (BitColumn_t*)(ba->_columns + (sizeof(BitColumn_t) * j));
    if (column->_column != NULL) {
      TRI_Free(ba->_memoryZone,column->_column);
    }  
  }
  TRI_Free(ba->_memoryZone, ba->_columns);
  
  ba->_columns = newColumns;

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////


// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
