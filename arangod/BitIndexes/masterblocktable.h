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
  uint8_t _usedPointers;
  uint8_t _deletedPointers;
  bit_column_int_t _deleted;  
  
  MasterTableBlockData_t _tablePointers [BITARRAY_MASTER_TABLE_BLOCKSIZE];  
  // ............................................................................
  // to test for deleted value
  // Let 0 <= n <= (BITARRAY_MASTER_TABLE_BLOCKSIZE - 1). 
  // if ((1 << n) && deleted) == 0, then item has been removed, otherwise not.  
  // ............................................................................
} MasterTableBlock_t;



///////////////////////////////////////////////////////////////////////////////
// The actual structure for a Master Table
// A Master Table (MT) is a sequence of one or more 'blocks' (see above structure). 
// Currently the MT is a sequence of contiguous 'blocks' rather than a linked
// list of 'blocks'.
///////////////////////////////////////////////////////////////////////////////

typedef struct MasterTable_s {
  size_t _numBlocks;               // the number of blocks allocated in the memory zone for the Master Table
  size_t _usedBlocks;              // the number of used block (not necessarily completed) within the Master Table. 
  MasterTableBlock_t* _blocks;     // a list of blocks storing the document pointers
  TRI_memory_zone_t* _memoryZone;  // the memory zone where the blocks will reside NOT where the table structure will reside
  TRI_associative_array_t _tablePosition; // The associated array - which stores the table position of a document
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
static int  createMasterTable        (MasterTable_t**, TRI_memory_zone_t*); 
static void destroyMasterTable       (MasterTable_t* mt); 
static void freeMasterTable          (MasterTable_t* mt); 
static int  insertMasterTable        (MasterTable_t*, TRI_master_table_position_t*);
static int  removeElementMasterTable (MasterTable_t*, TRI_master_table_position_t*, bool*);
static int  storeElementMasterTable  (MasterTable_t*, TRI_vector_pointer_t*, TRI_master_table_position_t*);
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
// Implementation of master table functions
// ............................................................................



///////////////////////////////////////////////////////////////////////////////
// Creates a Master Table (MT). Failure will return an appropriate error number 
///////////////////////////////////////////////////////////////////////////////

static int createMasterTable(MasterTable_t** mt, TRI_memory_zone_t* memoryZone) {
  int j;
  MasterTableBlock_t* block;  

  // ..........................................................................  
  // If the MT has already been created, return, do nothing and report no error
  // ..........................................................................  
  
  if ( *mt != NULL) {
    return TRI_ERROR_NO_ERROR;
  }

  
  // ..........................................................................  
  // If the memoryZone is invalid, return internal error
  // ..........................................................................  
  
  if ( memoryZone == NULL) {
    return TRI_ERROR_INTERNAL;
  }

  // ..........................................................................  
  // Create the MT structure
  // ..........................................................................  
  
  *mt = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(MasterTable_t), false);
  if (*mt == NULL) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }  

  
  // ..........................................................................  
  // Create the blocks
  // ..........................................................................  
  
  (*mt)->_numBlocks  = BITARRAY_MASTER_TABLE_INITIAL_SIZE;  
  (*mt)->_usedBlocks = 0;
  (*mt)->_blocks     = TRI_Allocate(memoryZone, (sizeof(MasterTableBlock_t) * (*mt)->_numBlocks), false);
  (*mt)->_memoryZone = memoryZone;
  
  if ((*mt)->_blocks == NULL) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, *mt);
    return TRI_ERROR_OUT_OF_MEMORY;
  }  
  
  
  // ..........................................................................  
  // Clear all blocks in the table
  // ..........................................................................  
  
  for (j = 0; j < (*mt)->_numBlocks; ++j) {
    block = &((*mt)->_blocks[j]);
    block->_usedPointers    = 0;
    block->_deletedPointers = 0;
    memset(&(block->_tablePointers), 0, BITARRAY_MASTER_TABLE_BLOCKSIZE * sizeof(MasterTableBlockData_t) );
  }  
  
  
  // ............................................................................
  // The associative array for table position of a document
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
                           
  
  return TRI_ERROR_NO_ERROR;
}


static void destroyMasterTable(MasterTable_t* mt) {
  if (mt == NULL) {
    return;
  }
  freeMasterTable(mt);
  TRI_Free(mt->_memoryZone, mt);
}

///////////////////////////////////////////////////////////////////////////////
// extends a given MT. Error reported if failure results 
// The given position must be < mt->_numBlocks, otherwise the table is
// is extended to accomdate that position
///////////////////////////////////////////////////////////////////////////////

static int extendMasterTable(MasterTable_t* mt) {
  MasterTableBlock_t* newBlocks;
  MasterTableBlock_t* block;  
  size_t newNumBlocks;
  int j;
  
  if (mt->_usedBlocks < mt->_numBlocks) {
    return TRI_ERROR_NO_ERROR;
  }

  newNumBlocks = (mt->_numBlocks * BITARRAY_MASTER_TABLE_GROW_FACTOR) + 1;  
  newBlocks    = TRI_Allocate(mt->_memoryZone, (sizeof(MasterTableBlock_t) * newNumBlocks), false);
  
  if (newBlocks == NULL) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }  

  
  for (j = mt->_usedBlocks; j < newNumBlocks; ++j) {
    block = &(newBlocks[j]);
    block->_usedPointers    = 0;
    block->_deletedPointers = 0;
    memset(&(block->_tablePointers), 0, BITARRAY_MASTER_TABLE_BLOCKSIZE * sizeof(MasterTableBlockData_t) );
  }  
  

  memcpy(newBlocks, mt->_blocks, mt->_usedBlocks * sizeof(MasterTableBlock_t));
  
  TRI_Free(mt->_memoryZone, mt->_blocks);
  mt->_blocks    = newBlocks;
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
  TRI_Free(mt->_memoryZone, mt->_blocks);
}


///////////////////////////////////////////////////////////////////////////////
// Always insert first, then apply the generated position 
// Not optimised as yet for document handles with the same bit masks
///////////////////////////////////////////////////////////////////////////////

static int insertMasterTable(MasterTable_t* mt, TRI_master_table_position_t* tableEntry) {
  int result;
  MasterTableBlock_t* block;  

  // ..........................................................................
  // Check whether or not we have enough room remaining in the master table 
  // array to store the new table entry.
  // ..........................................................................
  
  result = extendMasterTable(mt);

  if (result != TRI_ERROR_NO_ERROR) {
    return result;
  }
  
  
  // ..........................................................................
  // Extract the block in which we are going to operate on
  // ..........................................................................
  
  block = &(mt->_blocks[mt->_usedBlocks]);


  // ..........................................................................
  // Check that we do not need a new block if the currrent block is full
  // ..........................................................................

  if (block->_usedPointers == BITARRAY_MASTER_TABLE_BLOCKSIZE) {
    ++(mt->_usedBlocks);
    
    // ........................................................................
    // Need to check again if we have enough remove
    // ........................................................................

    result = extendMasterTable(mt);
    if (result != TRI_ERROR_NO_ERROR) {
      return result;
    }
    
    
    // ........................................................................
    // Extract the block again this time we know it is empty
    // ........................................................................
    
    block = &(mt->_blocks[mt->_usedBlocks]);
  }  

  
  
  // ..........................................................................
  // Set the document handle in the table
  // ..........................................................................
  
  ((block->_tablePointers)[block->_usedPointers])._tablePointer = tableEntry->_docPointer;
  ((block->_tablePointers)[block->_usedPointers])._numPointers  = 1;
  ++(block->_usedPointers);  
  

  // ..........................................................................
  // Set the position information so this can be passed back to the bitarray
  // ..........................................................................

  tableEntry->_blockNum  = mt->_usedBlocks;
  tableEntry->_bitNum    = block->_usedPointers;
  tableEntry->_vectorNum = 0; // not currently used in this revision
  

  // ..........................................................................
  // Insert the tableEntry into the associative array with the key being 
  // the document handle.
  // ..........................................................................
  
  if (!TRI_InsertKeyAssociativeArray(&(mt->_tablePosition), tableEntry->_docPointer, tableEntry, false)) {
    return TRI_ERROR_INTERNAL;
  }  
  
  
  return TRI_ERROR_NO_ERROR;
}



int removeElementMasterTable(MasterTable_t* mt, TRI_master_table_position_t* position, bool* compactColumns) {
  /*
  size_t _blockNum;   // the block within the Master Table
  uint8_t _offset;    // with the given block, an integer from 0..255 which indicates where within the block the document pointer resides
  void* _docPointer;  // the document pointer stored in that position 
  */
  return TRI_ERROR_NO_ERROR;
}

int storeElementMasterTable(MasterTable_t* mt, TRI_vector_pointer_t* results, TRI_master_table_position_t* position) {
  MasterTableBlock_t* tableBlock;  
  tableBlock = mt->_blocks + position->_blockNum;
  
  // ...........................................................................
  // Determine if the entry has been deleted
  // ...........................................................................
  
  if ((tableBlock->_deleted >> position->_bitNum) == 1) {
    return TRI_ERROR_NO_ERROR;
  }

  
  // ...........................................................................
  // entry not deleted so append it to the results vector
  // ...........................................................................
  
  TRI_PushBackVectorPointer(results,(tableBlock->_tablePointers[position->_bitNum])._tablePointer);
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
