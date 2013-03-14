////////////////////////////////////////////////////////////////////////////////
/// @brief bitarray implementation
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
/// @author Dr. Oreste Costa-Panaia
/// @author Copyright 2006-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "BasicsC/common.h"
#include "BasicsC/logging.h"
#include "BasicsC/random.h"

#include "bitarray.h"
#include "VocBase/document-collection.h"

#if BITARRAY_MASTER_TABLE_BLOCKSIZE==8
typedef uint8_t bit_column_int_t;
#define BITARRAY_COLUMN_FREE_MARKER UINT8_MAX
#elif BITARRAY_MASTER_TABLE_BLOCKSIZE==16
typedef uint16_t bit_column_int_t;
#define BITARRAY_COLUMN_FREE_MARKER UINT16_MAX
#elif BITARRAY_MASTER_TABLE_BLOCKSIZE==32
typedef  uint32_t bit_column_int_t;
#define BITARRAY_COLUMN_FREE_MARKER UINT32_MAX
#elif BITARRAY_MASTER_TABLE_BLOCKSIZE==64
typedef  uint64_t bit_column_int_t;
#define BITARRAY_COLUMN_FREE_MARKER UINT64_MAX
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

static int  extendColumns         (TRI_bitarray_t*, size_t);
static void setBitarrayMask       (TRI_bitarray_t*, TRI_bitarray_mask_t*, TRI_master_table_position_t*);

/*
static void debugPrintBitarray    (TRI_bitarray_t*);
static void debugPrintMaskFooter  (const char*);
static void debugPrintMaskHeader  (const char*);
static void debugPrintMask        (TRI_bitarray_t*, uint64_t);
*/

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
                     void* masterTable) {
  MasterTable_t* mt;
  int result;
  size_t j;
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
  // For now each set of bit arrays will create and use its own MasteTable.
  // ...........................................................................

  if (masterTable != NULL) {
    assert(false);
    mt = (MasterTable_t*)(masterTable);
  }

  // ...........................................................................
  // Create the new master table here
  // ...........................................................................

  else {
    mt = NULL;
    result = createMasterTable(&mt, memoryZone, false);
    if (result != TRI_ERROR_NO_ERROR) {
      TRI_Free(memoryZone, *bitArray);
      *bitArray = NULL;
      return result;
    }
  }

  // ...........................................................................
  // Check that we have a valid master table
  // ...........................................................................

  if (mt == NULL) {
    if (*bitArray != NULL) {
      TRI_Free(memoryZone, *bitArray);
    }
    *bitArray = NULL;
    return TRI_ERROR_INTERNAL;
  }

  (*bitArray)->_masterTable = mt;

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
  // Create the bitarrays (the columns which will contain the bits)
  // For each column we allocated the initialise size.
  // ...........................................................................

  ok = true;
  for (j = 0; j < (*bitArray)->_numColumns; ++j) {
    BitColumn_t* column;

    column = (BitColumn_t*)((*bitArray)->_columns + (sizeof(BitColumn_t) * j));
    column->_column = TRI_Allocate(memoryZone, sizeof(bit_column_int_t) * BITARRAY_INITIAL_NUMBER_OF_COLUMN_BLOCKS_SIZE, true);
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
      BitColumn_t* column;
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

  (*bitArray)->_numBlocksInColumn = BITARRAY_INITIAL_NUMBER_OF_COLUMN_BLOCKS_SIZE;
  //(*bitArray)->_usedBitLength     = 0;
  (*bitArray)->_lastBlockUsed = 0;

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
  size_t j;

  // ..........................................................................
  // if the master table is NOT shared, then delete it as well
  // ..........................................................................

  if (!mt->_shared) {
    freeMasterTable(mt);
  }

  // ..........................................................................
  // destroy the array of columns
  // ..........................................................................

  for (j = 0; j < ba->_numColumns; ++j) {
    BitColumn_t* column;
    column = (BitColumn_t*)(ba->_columns + (sizeof(BitColumn_t) * j));
    if (column->_column != NULL) {
      TRI_Free(ba->_memoryZone,column->_column);
    }
  }
  TRI_Free(ba->_memoryZone, ba->_columns);

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

int TRI_InsertBitMaskElementBitarray (TRI_bitarray_t* ba,
                                      TRI_bitarray_mask_t* mask,
                                      TRI_doc_mptr_t* element) {
  MasterTable_t* mt;
  TRI_master_table_position_t position;
  int result;

  if (ba == NULL || mask == NULL || element == NULL) {
    return TRI_ERROR_INTERNAL;
  }

  mt = (MasterTable_t*)(ba->_masterTable);

  if (mt == NULL) {
    assert(NULL);
    return TRI_ERROR_INTERNAL;
  }

  // ..........................................................................
  // Ensure that the element we are going to insert into the MasterBlockTable
  // and the bitarrays is not already there.
  // ..........................................................................

  if (TRI_FindByKeyAssociativeArray(&(mt->_tablePosition),element) != NULL) {
    assert(false);
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

  if (position._blockNum >= ba->_numBlocksInColumn) {
    result = extendColumns(ba, position._blockNum + 1);

    if (result != TRI_ERROR_NO_ERROR) {
      return result;
    }
  }

  // ...........................................................................
  // Use the mask to set the bits in each column to 0 or 1
  // ...........................................................................

  setBitarrayMask(ba, mask, &position);
  //debugPrintMask(ba,mask->_mask);

  // ...........................................................................
  // update the last block which is in use -- a small amount of help so that
  // we do not keep scanning indefinitely down the columns
  // ...........................................................................

  if (ba->_lastBlockUsed < position._blockNum) {
    ba->_lastBlockUsed = position._blockNum;
  }

  //debugPrintBitarray(ba);

  return TRI_ERROR_NO_ERROR;
}



////////////////////////////////////////////////////////////////////////////////
/// @brief Given a bit mask returns a list of document pointers
////////////////////////////////////////////////////////////////////////////////

int TRI_LookupBitMaskBitarray (TRI_bitarray_t* ba, TRI_bitarray_mask_t* mask, void* resultStorage) {
  // int result;
  uint8_t numBits;
  int i_blockNum,j_bitNum,k_colNum;
  TRI_master_table_position_t position;
  uint64_t compareMask = mask->_mask | mask->_ignoreMask;

  // ...........................................................................
  // TODO: we need to add an 'undefined' special column. If this column is NOT
  // set, then we do not bother checking the rest of columns.
  // ...........................................................................

  numBits = BITARRAY_MASTER_TABLE_BLOCKSIZE;

  // ...........................................................................
  // scan down the blocks
  // ...........................................................................

  for (i_blockNum = 0; i_blockNum < (ba->_lastBlockUsed + 1); ++i_blockNum) {

    // .........................................................................
    // within each block scan down the bit integer
    // .........................................................................

    for (j_bitNum = 0; j_bitNum < numBits; ++j_bitNum) {
      uint64_t bitValues = 0;

      // .......................................................................
      // Within each bit in the integer scan across the columns, this will
      // generate a 64 bit integer. Use this integer to compare to the bit
      // mask (eventually masks) sent here.
      // .......................................................................

      for (k_colNum = 0; k_colNum < ba->_numColumns; ++k_colNum) {

        BitColumn_t*     column;
        bit_column_int_t bitInteger;
        uint64_t         tempInteger;
        // .......................................................................
        // Extract the particular column
        // .......................................................................

        column = (BitColumn_t*)(ba->_columns + (sizeof(BitColumn_t) * k_colNum));


        // .......................................................................
        // Obtain the integer representation of this block
        // .......................................................................

        bitInteger  = *(column->_column + i_blockNum);

        tempInteger = (uint64_t)((bitInteger >> j_bitNum) & (1)) << k_colNum;

        bitValues  = bitValues | tempInteger;

      }

      // ..........................................................................
      // TODO: eventually we might have more than one bitmask to process
      // when we have AND/OR operators.
      // ..........................................................................

      bitValues = bitValues | mask->_ignoreMask;

      if (mask->_mask == 0 && bitValues != 0) {
        continue;
      }

      if (mask->_mask != 0 && bitValues == 0) {
        continue;
      }

      if (bitValues == compareMask) { // add to the list of things
        //debugPrintMaskHeader("bitValues/mask");
        //debugPrintMask(ba,bitValues);
        //debugPrintMask(ba,mask->_mask);
        //debugPrintMaskFooter("");
        position._blockNum = i_blockNum;
        position._bitNum   = j_bitNum;
        /* result = */ storeElementMasterTable(ba->_masterTable, resultStorage, &position);
      }

    }
  }

  // debugPrintBitarray(ba);

  return TRI_ERROR_NO_ERROR;
}


////////////////////////////////////////////////////////////////////////////////
/// @brief Given a bit mask set returns a list of document pointers
////////////////////////////////////////////////////////////////////////////////

int TRI_LookupBitMaskSetBitarray (TRI_bitarray_t* ba,
                                  TRI_bitarray_mask_set_t* maskSet,
                                  TRI_index_iterator_t* resultStorage) {
  // int result;
  uint8_t numBits;
  int i_blockNum,j_bitNum,k_colNum,m_mask;
  TRI_master_table_position_t position;

  // ...........................................................................
  // TODO: we need to add an 'undefined' special column. If this column is NOT
  // set, then we do not bother checking the rest of columns.
  // ...........................................................................

  numBits = BITARRAY_MASTER_TABLE_BLOCKSIZE;

  // ...........................................................................
  // scan down the blocks
  // ...........................................................................

  for (i_blockNum = 0; i_blockNum < (ba->_lastBlockUsed + 1); ++i_blockNum) {

    // .........................................................................
    // within each block scan down the bit integer
    // .........................................................................

    for (j_bitNum = 0; j_bitNum < numBits; ++j_bitNum) {
      uint64_t bitValues = 0;

      // .......................................................................
      // Within each bit in the integer scan across the columns, this will
      // generate a 64 bit integer. Use this integer to compare to the bit
      // mask (eventually masks) sent here.
      // .......................................................................

      for (k_colNum = 0; k_colNum < ba->_numColumns; ++k_colNum) {
        BitColumn_t*     column;
        bit_column_int_t bitInteger;
        uint64_t         tempInteger;

        // .......................................................................
        // Extract the particular column
        // .......................................................................

        column = (BitColumn_t*)(ba->_columns + (sizeof(BitColumn_t) * k_colNum));

        // .......................................................................
        // Obtain the integer representation of this block
        // .......................................................................

        bitInteger  = *(column->_column + i_blockNum);
        tempInteger = (uint64_t)((bitInteger >> j_bitNum) & (1)) << k_colNum;
        bitValues  = bitValues | tempInteger;
      }

      // ..........................................................................
      // Iterate over all the masks in the mask set
      // ..........................................................................

      for (m_mask = 0; m_mask < maskSet->_setSize; ++m_mask) {
        TRI_bitarray_mask_t* mask = maskSet->_maskArray + m_mask;
        uint64_t compareMask      = mask->_mask | mask->_ignoreMask;

        bitValues = bitValues | mask->_ignoreMask;

        if (mask->_mask == 0 && bitValues != 0) {
          continue;
        }

        if (mask->_mask != 0 && bitValues == 0) {
          continue;
        }

        if (bitValues == compareMask) { // add to the list of things
          //debugPrintMaskHeader("bitValues/mask");
          //debugPrintMask(ba,bitValues);
          //debugPrintMask(ba,mask->_mask);
          //debugPrintMaskFooter("");
          position._blockNum = i_blockNum;
          position._bitNum   = j_bitNum;
          /* result = */ storeElementMasterTable(ba->_masterTable, resultStorage, &position);
        }
      }
    }
  }

  // debugPrintBitarray(ba);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Remove an entry from a sequence of bitarray columns
////////////////////////////////////////////////////////////////////////////////

int TRI_RemoveElementBitarray (TRI_bitarray_t* ba, TRI_doc_mptr_t* element) {
  TRI_master_table_position_t* position;
  TRI_bitarray_mask_t mask;
  MasterTable_t* mt;
  MasterTableBlock_t* block;
  int result;
  bool ok;

  // ..........................................................................
  // Attempt to locate the position of the element within the Master Block table
  // ..........................................................................

  mt = (MasterTable_t*)(ba->_masterTable);
  position = (TRI_master_table_position_t*)(TRI_FindByKeyAssociativeArray(&(mt->_tablePosition),element));

  if (position == NULL) {
    return TRI_WARNING_ARANGO_INDEX_BITARRAY_REMOVE_ITEM_MISSING;
  }

  // ..........................................................................
  // Observe that we are NOT removing any entries from the actual bit arrays.
  // All we are 'removing' are entries in the master block table.
  // ..........................................................................

  result = removeElementMasterTable(mt, position);

  // ...........................................................................
  // It may happen that the block is completely free, moreover it may happen
  // that we are fortunate and it is the last used block.
  // ...........................................................................
  block = &(mt->_blocks[position->_blockNum]);

  // TODO: comparison is always false due to limited range of data type [-Wtype-limits]
  if (block->_free == BITARRAY_COLUMN_FREE_MARKER) {
    if (ba->_lastBlockUsed == position->_blockNum) {
      --ba->_lastBlockUsed;
    }
  }

  if (result != TRI_ERROR_NO_ERROR) {
    return result;
  }

  mask._mask = 0;
  mask._ignoreMask = 0;
  setBitarrayMask(ba, &mask, position);


  // ..........................................................................
  // Remove the entry from associative array
  // ..........................................................................

  ok = TRI_RemoveKeyAssociativeArray(&(mt->_tablePosition), element, NULL);
  if (!ok) {
    return TRI_ERROR_INTERNAL;
  }

  // debugPrintBitarray(ba);

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
/// @brief extends the columns which comprise the bitarray
////////////////////////////////////////////////////////////////////////////////

int extendColumns (TRI_bitarray_t* ba, size_t newBlocks) {
  bool ok = true;
  size_t j;
  char* newColumns;


  // ............................................................................
  // allocate memory for the new columns
  // ............................................................................

  newColumns = TRI_Allocate(ba->_memoryZone, sizeof(BitColumn_t) * ba->_numColumns, true);
  if (newColumns == NULL) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  // ............................................................................
  // allocate space for each column
  // ............................................................................

  for (j = 0; j < ba->_numColumns; ++j) {
    BitColumn_t* newColumn;
    newColumn = (BitColumn_t*)(newColumns + (sizeof(BitColumn_t) * j));
    newColumn->_column = TRI_Allocate(ba->_memoryZone, sizeof(bit_column_int_t) * newBlocks, true);
    if (newColumn->_column == NULL) {
      ok = false;
      break;
    }
  }


  // ............................................................................
  // if memory allocation failed, undo allocation and return
  // ............................................................................

  if (!ok) { // memory allocation failure
    for (j = 0; j < ba->_numColumns; ++j) {
      BitColumn_t* newColumn;
      newColumn = (BitColumn_t*)(newColumns + (sizeof(BitColumn_t) * j));
      if (newColumn->_column != NULL) {
        TRI_Free(ba->_memoryZone,newColumn->_column);
      }
    }
    TRI_Free(ba->_memoryZone, newColumns);
    return TRI_ERROR_OUT_OF_MEMORY;
  }


  // ............................................................................
  // copy the old columns into the new and free that column
  // ............................................................................

  for (j = 0; j < ba->_numColumns; ++j) {
    BitColumn_t* oldColumn;
    BitColumn_t* newColumn;

    oldColumn = (BitColumn_t*)(ba->_columns + (sizeof(BitColumn_t) * j));
    newColumn = (BitColumn_t*)(newColumns + (sizeof(BitColumn_t) * j));
    if (oldColumn->_column == NULL) {
      assert(false);
      continue;
    }
    memcpy(newColumn->_column, oldColumn->_column, sizeof(bit_column_int_t) * ba->_numBlocksInColumn);
    TRI_Free(ba->_memoryZone,oldColumn->_column);
  }


  TRI_Free(ba->_memoryZone, ba->_columns);


  ba->_columns = newColumns;
  ba->_numBlocksInColumn = newBlocks;

  return TRI_ERROR_NO_ERROR;
}


////////////////////////////////////////////////////////////////////////////////
/// @brief inserts a bitmask into a bit array
////////////////////////////////////////////////////////////////////////////////

static void setBitarrayMask(TRI_bitarray_t* ba, TRI_bitarray_mask_t* mask, TRI_master_table_position_t* position) {
  size_t j;
  // ...........................................................................
  // Use the mask to set the bits in each column to 0 or 1
  // ...........................................................................

  for (j = 0; j < ba->_numColumns; ++j) {
    BitColumn_t*      column;
    bit_column_int_t* bitInteger;

    // .........................................................................
    // Extract the particular column
    // .........................................................................

    column = (BitColumn_t*)(ba->_columns + (sizeof(BitColumn_t) * j));


    // .........................................................................
    // Extract the integer which represents a block
    // .........................................................................

    bitInteger = column->_column + position->_blockNum;


    // .........................................................................
    // Determine if the jth bit in the bit mask is 0 or 1
    // .........................................................................

    if ( (((uint64_t)(1) << j) & mask->_mask) == 0 ) {

      // .......................................................................
      // For the jth column we have determined that a '0' is to be stored here.
      // .......................................................................

      *bitInteger = (*bitInteger) & (~((bit_column_int_t)(1) << position->_bitNum));
    }

    else {

      // .......................................................................
      // For the jth column we have determined that a '0' is to be stored here.
      // .......................................................................

      *bitInteger = (*bitInteger) | ((bit_column_int_t)(1) << position->_bitNum);
    }

  }
}


////////////////////////////////////////////////////////////////////////////////
// Implementation of forward declared debug functions
////////////////////////////////////////////////////////////////////////////////

/*
void debugPrintMaskFooter(const char* footer) {
  printf("%s\n\n",footer);
}

void debugPrintMaskHeader(const char* header) {
  printf("------------------- %s --------------------------\n",header);
}


void debugPrintMask(TRI_bitarray_t* ba, uint64_t mask) {
  int j;

  for (j = 0; j < ba->_numColumns; ++j) {
    if ((mask & ((uint64_t)(1) << j)) == 0) {
      printf("0");
    }
    else {
      printf("1");
    }
  }
  printf("\n");
}


void debugPrintBitarray(TRI_bitarray_t* ba) {
  int j;
  uint64_t bb;
  bit_column_int_t oo;


  // ...........................................................................
  // Determine the number of blocks -- remember to add one more if there is
  // an started and unfinished block.
  // ...........................................................................

  printf("\n");
  printf("THERE ARE %lu COLUMNS\n",ba->_numColumns);
  printf("THE NUMBER OF ALLOCATED BLOCKS IN EACH COLUMN IS %lu\n",ba->_numBlocksInColumn);
  printf("THE NUMBER OF THE LAST BLOCK USED IS %lu\n",ba->_lastBlockUsed);
  printf("\n");


  printf("-------------------------------------------------------------------------------------------------\n");
  for (bb = 0; bb <= ba->_lastBlockUsed ; ++bb) {
    if (bb != ba->_lastBlockUsed) {
      //continue;
    }
    printf("==\n");
    for (oo = 0; oo < BITARRAY_MASTER_TABLE_BLOCKSIZE; ++oo) {
      printf("ROW %lu: ", ((bb * BITARRAY_MASTER_TABLE_BLOCKSIZE) + oo) );
      for (j = 0; j < ba->_numColumns; ++j) {
        BitColumn_t* column;
        bit_column_int_t* bitInteger;

        column = (BitColumn_t*)(ba->_columns + (sizeof(BitColumn_t) * j));
        bitInteger = column->_column + bb;

        if ( (*bitInteger & ((bit_column_int_t)(1) << oo)) == 0) {
          printf(" 0 ");
        }
        else {
          printf(" 1 ");
        }
      }
      printf("\n");
    }
  }
}

*/

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////


// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
