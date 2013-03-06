////////////////////////////////////////////////////////////////////////////////
/// @brief master block table implementation
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


#ifndef TRIAGENS_BASICS_C_BITINDEXES_MASTER_BLOCK_TABLE_H
#define TRIAGENS_BASICS_C_BITINDEXES_MASTER_BLOCK_TABLE_H 1

#include "BasicsC/associative.h"
#include "BasicsC/common.h"
#include "BasicsC/hashes.h"
#include "BasicsC/locks.h"
#include "BasicsC/vector.h"

#include "IndexIterators/index-iterator.h"

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                            bitarray private types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup bitarray
/// @{
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
/// @brief private structure for the master table
////////////////////////////////////////////////////////////////////////////////

typedef struct MasterTableBlockData_s {
  void*  _tablePointer;  
  // later todo: if the same bit mask appears then rather than storing one element
  // we store a sequence of elements -- _numPointers below tells us how many elements
  // are stored here. However we need some overhead to array extension etc. do later or use vector
  size_t _numPointers; 
} MasterTableBlockData_t;


typedef struct MasterTableBlock_s {
  bit_column_int_t _free;  
  MasterTableBlockData_t _tablePointers [BITARRAY_MASTER_TABLE_BLOCKSIZE];  
} MasterTableBlock_t;



///////////////////////////////////////////////////////////////////////////////
// The actual structure for a Master Table
// A Master Table (MT) is a sequence of one or more 'blocks' (see above structure). 
// Currently the MT is a sequence of contiguous 'blocks' rather than a linked
// list of 'blocks'.
///////////////////////////////////////////////////////////////////////////////

typedef struct MasterTable_s {
  size_t _numBlocks;               // the number of blocks allocated in the memory zone for the Master Table
  MasterTableBlock_t* _blocks;     // a list of blocks storing the document pointers
  TRI_associative_array_t _tablePosition; // The associated array - which stores the table position of a document
  TRI_vector_t _freeBlockPosition; // a list of free blocks we can use
  bool _shared;                    // if true, then this table is shared between various bitarrays
  TRI_memory_zone_t* _memoryZone;  // the memory zone where the blocks will reside NOT where the table structure will reside
} MasterTable_t;

///////////////////////////////////////////////////////////////////////////////
// The input-output structure to the Master Table
///////////////////////////////////////////////////////////////////////////////



// ............................................................................
// The shared Master Table if we allow sharing
// ............................................................................
  
//static MasterTable_t* _masterTable_ = NULL;


// ............................................................................
// forward declared functions
// ............................................................................
static int  extendMasterTable        (MasterTable_t*);
static int  createMasterTable        (MasterTable_t**, TRI_memory_zone_t*, bool); 
static void destroyMasterTable       (MasterTable_t* mt); 
static void freeMasterTable          (MasterTable_t* mt); 
static int  insertMasterTable        (MasterTable_t*, TRI_master_table_position_t*);
static int  removeElementMasterTable (MasterTable_t*, TRI_master_table_position_t*);
static int  storeElementMasterTable  (MasterTable_t*, void*, TRI_master_table_position_t*);
// ............................................................................
// forward declared functions associative array
// ............................................................................
static uint64_t tablePositionHashKey               (TRI_associative_array_t*, void*);
static uint64_t tablePositionHashElement           (TRI_associative_array_t*, void*);
static void     tablePositionClearElement          (TRI_associative_array_t*, void*);
static bool     tablePositionIsEmptyElement        (TRI_associative_array_t*, void*);
static bool     tablePositionIsEqualKeyElement     (TRI_associative_array_t*, void*, void*);
static bool     tablePositionIsEqualElementElement (TRI_associative_array_t*, void*, void*);
// ............................................................................
// forward declared functions for vector
// ............................................................................
static int64_t compareIndexOf(MasterTable_t*, size_t, bool*);

// ............................................................................
// Implementation of master table functions
// ............................................................................



///////////////////////////////////////////////////////////////////////////////
// Creates a Master Table (MT). Failure will return an appropriate error number 
///////////////////////////////////////////////////////////////////////////////

static int createMasterTable(MasterTable_t** mt, TRI_memory_zone_t* memoryZone, bool shared) {
  size_t j;
  
  // ..........................................................................  
  // If the MT has already been created, return, do nothing and report no error
  // ..........................................................................  
  
  if (*mt != NULL) {
    return TRI_ERROR_NO_ERROR;
  }

  
  // ..........................................................................  
  // If the memoryZone is invalid, return internal error
  // ..........................................................................  
  
  if (memoryZone == NULL) {
    return TRI_ERROR_INTERNAL;
  }

  // ..........................................................................  
  // Create the MT structure
  // ..........................................................................  
  
  *mt = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(MasterTable_t), true);
  if (*mt == NULL) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }  

  
  // ..........................................................................  
  // Create the blocks
  // ..........................................................................  
  
  (*mt)->_numBlocks  = BITARRAY_MASTER_TABLE_INITIAL_SIZE;  
  
  if ((*mt)->_numBlocks > 0) {
    (*mt)->_blocks = TRI_Allocate(memoryZone, (sizeof(MasterTableBlock_t) * (*mt)->_numBlocks), true); 
    if ((*mt)->_blocks == NULL) {
      TRI_Free(TRI_UNKNOWN_MEM_ZONE, *mt);
      return TRI_ERROR_OUT_OF_MEMORY;
    }  
  }
  else {
    (*mt)->_blocks = NULL;
  }
  
  (*mt)->_memoryZone = memoryZone;
  (*mt)->_shared     = shared;
  
  
  // ............................................................................
  // The associative array for table position of a document based upon the 
  // document handle
  // ............................................................................

  TRI_InitAssociativeArray(&((*mt)->_tablePosition), 
                           memoryZone, 
                           sizeof(TRI_master_table_position_t),
                           tablePositionHashKey,
                           tablePositionHashElement,
                           tablePositionClearElement,
                           tablePositionIsEmptyElement,
                           tablePositionIsEqualKeyElement,
                           tablePositionIsEqualElementElement);
                           
  
  // ............................................................................
  // Fill in the free list of blocks -- this is how we insert entries into the
  // master table. Entries are never deleted. We reuse the blocks whenever
  // possible. Note that the free list is for BLOCKS only not for arrays 
  // positions within a block. Once we have a free block we go to the block and
  // then we find the first free position within the block using the block itself.
  // ............................................................................
  
  TRI_InitVector(&((*mt)->_freeBlockPosition), memoryZone, sizeof(size_t)); 
  
  for (j = 0; j < (*mt)->_numBlocks; ++j) {
    MasterTableBlock_t* block = (*mt)->_blocks + j;    
    block->_free = BITARRAY_COLUMN_FREE_MARKER;
    TRI_PushBackVector(&((*mt)->_freeBlockPosition), &j);      
  }
  
  return TRI_ERROR_NO_ERROR;
}


static void destroyMasterTable(MasterTable_t* mt) {
  if (mt == NULL) {
    return;
  }
  TRI_DestroyAssociativeArray(&(mt->_tablePosition));
  TRI_DestroyVector(&(mt->_freeBlockPosition));
  if (mt->_blocks != 0) {
    TRI_Free(mt->_memoryZone, mt->_blocks);
  }  
}

///////////////////////////////////////////////////////////////////////////////
// extends a given MT. Error reported if failure results 
// The given position must be < mt->_numBlocks, otherwise the table is
// is extended to accomdate that position
///////////////////////////////////////////////////////////////////////////////

static int extendMasterTable(MasterTable_t* mt) {
  MasterTableBlock_t* newBlocks;
  size_t newNumBlocks;
  size_t* freeBlock;
  size_t j;
  
  // ..........................................................................
  // Obtain the first block which is free
  // ..........................................................................
  
  freeBlock = TRI_AtVector(&(mt->_freeBlockPosition),0);
  
  if (freeBlock != NULL) {
  
    // ........................................................................
    // something is terribly wrong
    // ........................................................................
    assert(false);
    return TRI_ERROR_INTERNAL;
  }

  
  newNumBlocks = (mt->_numBlocks * BITARRAY_MASTER_TABLE_GROW_FACTOR) + 1;  
  newBlocks    = TRI_Allocate(mt->_memoryZone, (sizeof(MasterTableBlock_t) * newNumBlocks), true);
  
  if (newBlocks == NULL) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }  

  
  if (mt->_blocks != NULL) {
    memcpy(newBlocks, mt->_blocks, mt->_numBlocks * sizeof(MasterTableBlock_t));
    TRI_Free(mt->_memoryZone, mt->_blocks);
  }
  
  mt->_blocks = newBlocks;
  
  for (j = mt->_numBlocks; j < newNumBlocks; ++j) {
    MasterTableBlock_t* block = mt->_blocks + j;     
    // TODO: negative integer implicitly converted to unsigned type [-Wsign-conversion]
    block->_free = ~((bit_column_int_t)(0));
    TRI_PushBackVector(&(mt->_freeBlockPosition), &j);      
  }
  
  mt->_numBlocks = newNumBlocks;  
  
  return TRI_ERROR_NO_ERROR;
}


///////////////////////////////////////////////////////////////////////////////
//  
///////////////////////////////////////////////////////////////////////////////


static void freeMasterTable(MasterTable_t* mt) {
  if (mt == NULL) {
    return;
  }
  destroyMasterTable(mt);
  TRI_Free(mt->_memoryZone, mt);
}


///////////////////////////////////////////////////////////////////////////////
// Always insert first, then apply the generated position 
// Not optimised as yet for document pointers/handles with the same bit masks
///////////////////////////////////////////////////////////////////////////////

static int insertMasterTable(MasterTable_t* mt, TRI_master_table_position_t* tableEntry) {
  int result;
  MasterTableBlock_t* block;  
  bit_column_int_t blockEntryNum;
  size_t* freeBlock;
  
  
  START: // I love goto it makes me nostalgic for the good old days of FORTRAN 
  
  // ..........................................................................
  // Obtain the first block which is free
  // ..........................................................................
  
  freeBlock = TRI_AtVector(&(mt->_freeBlockPosition),0);
  
  if (freeBlock == NULL) {
    // ........................................................................
    // It appears that we have run out of remove in our master table. Extend
    // the master table and try again.
    // ........................................................................

    result = extendMasterTable(mt);

    if (result != TRI_ERROR_NO_ERROR) {
      return result;
    }
    
    goto START;
  }
  

  // ..........................................................................
  // Store the number of the block within the position structure
  // ..........................................................................
  
  tableEntry->_blockNum  = *freeBlock;
  
  
  // ..........................................................................
  // Extract the block in which we are going to operate on
  // ..........................................................................
  
  block = &(mt->_blocks[tableEntry->_blockNum]);


  // ..........................................................................
  // Locate the first entry which is free within this block
  // Note that if all entries within this block are occupied we have to try
  // again.
  // ..........................................................................
  
  if (block->_free == 0) {
  
    // ........................................................................
    // this block is not free, remove it from the free list and try again
    // ........................................................................

    TRI_RemoveVector(&(mt->_freeBlockPosition),0);
    goto START;    
  }
  
  
  blockEntryNum = 0;
  while (true) {
    bit_column_int_t tempInt = (bit_column_int_t)(1) << blockEntryNum;
    if ((block->_free & tempInt)) {
      block->_free = (block->_free & (~tempInt));
      break;
    }
    ++blockEntryNum;
    if (blockEntryNum == BITARRAY_MASTER_TABLE_BLOCKSIZE) {
      assert(false);
      return TRI_ERROR_INTERNAL;
    }
  }
  
  tableEntry->_blockNum  = *freeBlock;
  tableEntry->_bitNum    = blockEntryNum;
  tableEntry->_vectorNum = 0; // not currently used in this revision
  
  block->_tablePointers[blockEntryNum]._numPointers  = 1;
  block->_tablePointers[blockEntryNum]._tablePointer = tableEntry->_docPointer;
  
  
  // ..........................................................................
  // Insert the tableEntry into the associative array with the key being 
  // the document handle.
  // ..........................................................................
  
  if (!TRI_InsertKeyAssociativeArray(&(mt->_tablePosition), tableEntry->_docPointer, tableEntry, false)) {
    return TRI_ERROR_INTERNAL;
  }  
  
  
  return TRI_ERROR_NO_ERROR;
}



int removeElementMasterTable(MasterTable_t* mt, TRI_master_table_position_t* position) {  
  size_t vectorInsertPos;
  bool equality;
  MasterTableBlock_t* block = &(mt->_blocks[position->_blockNum]);
  bit_column_int_t tempInt = (bit_column_int_t)(1) << position->_bitNum;
  
  
  // ...........................................................................
  // determine if this block already exists in the free block list
  // ...........................................................................
  
  if (block->_free != 0) {
  
    // .........................................................................
    // simple removal since block already exists in the free block list
    // .........................................................................
    
    if ((block->_free & tempInt)) { // Catastrophic failure since the entry should NOT be free
      assert(false);
      return TRI_ERROR_INTERNAL;
    }
    
    block->_free = block->_free | tempInt;
    
    return TRI_ERROR_NO_ERROR;

  }
  
  // ...........................................................................
  // The block we have is not on the free list -- we have to add it
  // ...........................................................................

  block->_free = block->_free | tempInt;
  equality = false;
  vectorInsertPos = compareIndexOf(mt, position->_blockNum, &equality);
  if (!equality) {
    TRI_InsertVector(&(mt->_freeBlockPosition), &(position->_blockNum), vectorInsertPos);
  }
    
  return TRI_ERROR_NO_ERROR;
}

#if 0
static int localPushBackVector(TRI_vector_t* vector, void const* element) {
  //return TRI_ERROR_NO_ERROR;
  if (vector->_length == vector->_capacity) {
    char* newBuffer;
    size_t newSize = (size_t) (1 + (vector->_growthFactor * vector->_capacity));

    newBuffer = (char*) TRI_Reallocate(vector->_memoryZone, vector->_buffer, newSize * vector->_elementSize);

    if (newBuffer == NULL) {
      return TRI_ERROR_OUT_OF_MEMORY;
    }

    vector->_capacity = newSize;
    vector->_buffer = newBuffer;
  }

  //return TRI_ERROR_NO_ERROR;
  memcpy(vector->_buffer + (vector->_length * vector->_elementSize), element, vector->_elementSize);

  vector->_length++;

  return TRI_ERROR_NO_ERROR;
}
#endif

int storeElementMasterTable(MasterTable_t* mt, void* results, TRI_master_table_position_t* position) {
  // should we store doc pointers directly or indirectly via BitarrayIndexElement
  // need an Generic IndexElement structure to do this effectively
  MasterTableBlock_t* tableBlock;  
  TRI_index_iterator_interval_t interval;
  
  TRI_index_iterator_t* iterator = (TRI_index_iterator_t*)(results);
  
  if (results == NULL) {
    return TRI_ERROR_INTERNAL;
  }
  
  // ...........................................................................
  // Determine the block within the master table we are concentrating on
  // ...........................................................................
  
  tableBlock = mt->_blocks + position->_blockNum;
  
  
  // ...........................................................................
  // Within the block determine if the entry (array entry) is marked as free
  // if it is marked as free, then of course no reason to store the handle
  // ...........................................................................
  
  if (((tableBlock->_free >> position->_bitNum) & 1) == 1) {
    return TRI_ERROR_NO_ERROR;
  }
  
  // ...........................................................................
  // entry not deleted so append it to the results which is an iterator 
  // ...........................................................................
  
  interval._leftEndPoint = (tableBlock->_tablePointers[position->_bitNum])._tablePointer;
    
  TRI_PushBackVector(&(iterator->_intervals), &interval);
  return TRI_ERROR_NO_ERROR;
}



////////////////////////////////////////////////////////////////////////////////
// IMPLEMENTATION OF FORWARD DECLARED FUNCTIONS FOR ASSOCIATIVE ARRAY
////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// Given a doc pointer returns a hash
///////////////////////////////////////////////////////////////////////////////

uint64_t tablePositionHashKey(TRI_associative_array_t* aa, void* key) {
  uint64_t hash = TRI_FnvHashBlockInitial();
  hash = TRI_FnvHashBlock(hash, key, sizeof(void*)); 
  return  hash;
}

uint64_t tablePositionHashElement(TRI_associative_array_t* aa, void* element) { 
  TRI_master_table_position_t* mtp = (TRI_master_table_position_t*)(element);    
  uint64_t hash = TRI_FnvHashBlockInitial();
  hash = TRI_FnvHashBlock(hash, mtp->_docPointer, sizeof(void*)); 
  return hash;
}

void tablePositionClearElement(TRI_associative_array_t* aa, void* element) {
  TRI_master_table_position_t* mtp = (TRI_master_table_position_t*)(element);    
  mtp->_blockNum   = 0;
  mtp->_bitNum     = 0;
  mtp->_docPointer = NULL;
}

bool tablePositionIsEmptyElement(TRI_associative_array_t* aa, void* element) {
  TRI_master_table_position_t* mtp = (TRI_master_table_position_t*)(element);    
  if (mtp->_blockNum == 0 && mtp->_bitNum == 0 && mtp->_docPointer == NULL) {
    return true;
  }
  return false;  
}

bool tablePositionIsEqualKeyElement(TRI_associative_array_t* aa, void* key, void* element) {
  TRI_master_table_position_t* mtp = (TRI_master_table_position_t*)(element);    
  if (key == mtp->_docPointer) { 
    return true;
  }
  return false;  
}

bool tablePositionIsEqualElementElement(TRI_associative_array_t* aa, void* left, void* right) {
  TRI_master_table_position_t* left_mtp  = (TRI_master_table_position_t*)(left);    
  TRI_master_table_position_t* right_mtp = (TRI_master_table_position_t*)(right);    
  
  if (left_mtp->_blockNum   == right_mtp->_blockNum &&
      left_mtp->_bitNum     == right_mtp->_bitNum   &&
      left_mtp->_docPointer == right_mtp->_docPointer) {
    return true;
  }    
  return false;
}



  
////////////////////////////////////////////////////////////////////////////////
// IMPLEMENTATION OF FORWARD DECLARED FUNCTIONS FOR VECTOR
////////////////////////////////////////////////////////////////////////////////


static int64_t compareIndexOf(MasterTable_t* mt, size_t item, bool* equality) {
  int64_t leftPos;
  int64_t rightPos;
  int64_t midPos;
  
  leftPos  = 0;
  rightPos = ((int64_t) (mt->_freeBlockPosition)._length) - 1;
  
  while (leftPos <= rightPos)  {
    size_t* compareResult;
    
    midPos = (leftPos + rightPos) / 2;
    compareResult = TRI_AtVector(&(mt->_freeBlockPosition), midPos);
    if (*compareResult < item) {    
      leftPos = midPos + 1;
    }    
    else if (*compareResult > item) {
      rightPos = midPos - 1;
    }
    else {
      *equality = true;
      return midPos;
    }  
  }
  *equality = false;
  return leftPos; 
}
 

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////


#ifdef __cplusplus
}
#endif

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
