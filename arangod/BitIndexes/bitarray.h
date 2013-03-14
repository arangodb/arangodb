////////////////////////////////////////////////////////////////////////////////
/// @brief bit array implementation
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


#ifndef TRIAGENS_BIT_INDEXES_BITARRAY_H
#define TRIAGENS_BIT_INDEXES_BITARRAY_H 1

#include "BasicsC/common.h"
#include "BasicsC/locks.h"
#include "BasicsC/vector.h"
#include "BitIndexes/bitarrayIndex.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BITARRAY_MASTER_TABLE_BLOCKSIZE 8

#define BITARRAY_MASTER_TABLE_INITIAL_SIZE 1024
#define BITARRAY_MASTER_TABLE_GROW_FACTOR 1.2

#define BITARRAY_INITIAL_NUMBER_OF_COLUMN_BLOCKS_SIZE 10
#define BITARRAY_NUMBER_OF_COLUMN_BLOCKS_GROW_FACTOR 1.2

// -----------------------------------------------------------------------------
// --SECTION--                                             bitarray public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup bitarray
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Structure for the position for the of the document within the Master Table.
// The location of a document handle is given by 3 numbers:
//   (i)   the block within the master table array
//   (ii)  within the block the offset indicating the position within this block
//   (iii) if we store multiple document handles as a vector list, the offset
//         within the vector list
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_master_table_position_s {
  size_t _blockNum;   // the block within the Master Table
  uint8_t _bitNum;    // with the given block, an integer from 0..255 which indicates where within the block the document pointer resides
  size_t _vectorNum;  // vector list offset
  void* _docPointer;  // the document pointer stored in that position
} TRI_master_table_position_t;

////////////////////////////////////////////////////////////////////////////////
// The actual structure which will contain the bit arrays as a series of
// columns.
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_bitarray_s {
  size_t  _numColumns;         // number of bitarrays particpating in index
  char*   _columns;            // the actual bit array columns
  size_t  _numBlocksInColumn;  // the number of blocks (allocated not necessarily used) within a column
  //uint8_t _usedBitLength;      // the number of bits which have been used in the last block used.
  size_t  _lastBlockUsed;      // the number of the last block which contains active columns
  TRI_memory_zone_t* _memoryZone;
  void* _masterTable;
} TRI_bitarray_t;

////////////////////////////////////////////////////////////////////////////////
// When we add a document within the bit arrays we need a bit mask to determine
// which bits are 'on' and which are 'off'. To support the ternary values
// 'on', 'off' and 'ignore' we use a pair of integers rather one single integer.
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_bitarray_mask_s {
  uint64_t _ignoreMask;
  uint64_t _mask;
} TRI_bitarray_mask_t;

typedef struct TRI_bitarray_mask_set_s {
  TRI_bitarray_mask_t* _maskArray;
  size_t               _arraySize;
  size_t               _setSize;
} TRI_bitarray_mask_set_t;

////////////////////////////////////////////////////////////////////////////////
// A set of options (parameters) which are required when defining a group
// of bit array columns. Note, these options can be independent of any bit
// array index.
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_bitarray_params_s {
  int (*_clearBitMaskCallBack) (void*, void*, TRI_bitarray_mask_t*); // callback far function call
  int _clearBitMask; // static near function call
  int (*_setBitMaskCallBack) (void*, void*, TRI_bitarray_mask_t*); // callback far function call
  int _setBitMask; // static near function call
  /*
         other parameters to be definedfor optimization
         at a later time
  */
} TRI_bitarray_index_params_t;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

int TRI_InitBitarray (TRI_bitarray_t**, TRI_memory_zone_t*, size_t, void*);

int TRI_DestroyBitarray (TRI_bitarray_t*);

int TRI_FreeBitarray (TRI_bitarray_t*);

int TRI_InsertBitMaskElementBitarray (TRI_bitarray_t*,
                                      TRI_bitarray_mask_t*,
                                      struct TRI_doc_mptr_s*);

int TRI_LookupBitMaskSetBitarray (TRI_bitarray_t*,
                                  TRI_bitarray_mask_set_t*,
                                  struct TRI_index_iterator_s*);

int TRI_RemoveElementBitarray (TRI_bitarray_t*, struct TRI_doc_mptr_s*);

#ifdef __cplusplus
}
#endif

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
