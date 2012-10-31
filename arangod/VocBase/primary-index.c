////////////////////////////////////////////////////////////////////////////////
/// @brief primary index
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2011 triagens GmbH, Cologne, Germany
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
/// @author Jan Steemann
/// @author Copyright 2012, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "primary-index.h"

#include "BasicsC/hashes.h"

#define INITIAL_SIZE (128)

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief return the local id of the transaction that created a document
////////////////////////////////////////////////////////////////////////////////

static TRI_transaction_local_id_t DocumentTransaction (const TRI_transaction_doc_mptr_t* const doc) {
  return doc->_validFrom;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check whether a specific transaction is currently running
/// (not yet implemented)
////////////////////////////////////////////////////////////////////////////////

static bool InWriteTransactionsTable (const TRI_transaction_t* const trx, 
                                      const TRI_transaction_local_id_t id) {
  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hashes the document key
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashKey (const char* const key) {
  return TRI_FnvHashString(key);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check constraints on previous documnent revision found
////////////////////////////////////////////////////////////////////////////////
     
static int CheckConstraint (TRI_revision_constraint_t* const constraint, 
                            const TRI_transaction_doc_mptr_t* const doc) {
  if (doc == NULL) {
    // document not found
    return TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND;
  }
  
  if (constraint != NULL) {
    // return revision id found
    constraint->_previousRevision = doc->_validFrom;
  
    if (constraint->_policy == TRI_DOC_UPDATE_ERROR && constraint->_expectedRevision != doc->_validFrom) {
      // revision ids do not match
      return TRI_ERROR_ARANGO_CONFLICT;
    }
    else if (constraint->_policy == TRI_DOC_UPDATE_CONFLICT) {
      return TRI_ERROR_NOT_IMPLEMENTED;
    }
    else if (constraint->_policy == TRI_DOC_UPDATE_ILLEGAL) {
      return TRI_ERROR_INTERNAL;
    }
  }

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compares a document key and a document
////////////////////////////////////////////////////////////////////////////////

static bool IsVisible (const TRI_transaction_t* const trx,
                       const char* const key, 
                       const TRI_transaction_doc_mptr_t* const element) {
  const TRI_transaction_local_id_t ownId = trx->_id._localId;

  return (element->_validFrom < ownId) &&
         (! InWriteTransactionsTable(trx, element->_validFrom)) &&
         (element->_validTo == 0 || element->_validTo > ownId || InWriteTransactionsTable(trx, element->_validTo)) &&
         (strcmp(key, element->_key) == 0);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief resizes the index
////////////////////////////////////////////////////////////////////////////////

static bool Resize (TRI_primary_index_t* const idx) {
  TRI_transaction_doc_mptr_t** oldTable;
  uint64_t oldAlloc;
  uint64_t j;

  oldTable = idx->_table;
  oldAlloc = idx->_nrAlloc;

  idx->_nrAlloc = 2 * idx->_nrAlloc + 1;
  idx->_table = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, idx->_nrAlloc * sizeof(TRI_transaction_doc_mptr_t*), true);

  if (idx->_table == NULL) {
    // out of memory
    idx->_nrAlloc = oldAlloc;
    idx->_table = oldTable;

    return false;
  }
  
  // reposition old elements into new index
  for (j = 0; j < oldAlloc; ++j) {
    if (oldTable[j] != NULL) {
      const uint64_t hash = HashKey(oldTable[j]->_key);
      uint64_t i;
      
      i = hash % idx->_nrAlloc;

      while (idx->_table[i] != NULL) {
        i = (i + 1) % idx->_nrAlloc;
      }

      idx->_table[i] = oldTable[j]; 
    }
  }

  TRI_Free(TRI_UNKNOWN_MEM_ZONE, oldTable);
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up a document in the index 
////////////////////////////////////////////////////////////////////////////////
 
static TRI_transaction_doc_mptr_t* Lookup (TRI_primary_index_t* const idx,
                                           TRI_transaction_t* const trx,
                                           const char* const key) {
  const uint64_t hash = HashKey(key);
  uint64_t i;
  
  i = hash % idx->_nrAlloc;

  // search the table
  while (idx->_table[i] != NULL) {
    if (IsVisible(trx, key, idx->_table[i])) {
      return idx->_table[i];
    }

    i = (i + 1) % idx->_nrAlloc;
  }

  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief insert a document into the index and return the previous revision
////////////////////////////////////////////////////////////////////////////////
 
static TRI_transaction_doc_mptr_t* Insert (TRI_primary_index_t* const idx,
                                           TRI_transaction_t* const trx,
                                           TRI_transaction_doc_mptr_t* const doc) {
  const uint64_t hash = HashKey(doc->_key);
  uint64_t i;
  TRI_transaction_doc_mptr_t* old;
  
  if (idx->_nrAlloc == idx->_nrUsed) {
    return NULL;
  }

  i = hash % idx->_nrAlloc;

  // search the table
  while (idx->_table[i] != NULL && ! IsVisible(trx, doc->_key, idx->_table[i])) {
    i = (i + 1) % idx->_nrAlloc;
  }

  // get previous element
  old = idx->_table[i];
  if (old != NULL) {
    return old;
  }

  idx->_table[i] = doc;
  idx->_nrUsed++;

  // if we were adding and the table is more than half full, extend it
  if (idx->_nrAlloc < 2 * idx->_nrUsed) {
    Resize(idx);
  }

  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief create the primary index
////////////////////////////////////////////////////////////////////////////////

TRI_primary_index_t* TRI_CreatePrimaryIndex () {
  TRI_primary_index_t* idx;

  idx = (TRI_primary_index_t*) TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_primary_index_t), false);
  if (idx == NULL) {
    return NULL;
  }
  
  TRI_InitReadWriteLock(&idx->_lock);

  idx->_table = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_transaction_doc_mptr_t*) * INITIAL_SIZE, true);
  if (idx->_table == NULL) {
    // out of memory
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, idx);

    return NULL;
  }

  idx->_nrUsed = 0;
  idx->_nrAlloc = INITIAL_SIZE;

  return idx;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free the primary index
////////////////////////////////////////////////////////////////////////////////

void TRI_FreePrimaryIndex (TRI_primary_index_t* const idx) {
  assert(idx);

  TRI_Free(TRI_UNKNOWN_MEM_ZONE, &idx->_table);
  TRI_DestroyReadWriteLock(&idx->_lock);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, idx);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

TRI_transaction_doc_mptr_t* TRI_LookupPrimaryIndex (TRI_primary_index_t* const idx, 
                                                    TRI_transaction_t* const trx,
                                                    const char* const key) {
  TRI_transaction_doc_mptr_t* old; 

  // start critical section -----------------------------
  TRI_ReadLockReadWriteLock(&idx->_lock); 

  old = Lookup(idx, trx, key);

  // end critical section -----------------------------
  TRI_ReadUnlockReadWriteLock(&idx->_lock); 
  
  return old;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an element to the primary index
////////////////////////////////////////////////////////////////////////////////

int TRI_InsertPrimaryIndex (TRI_primary_index_t* const idx,
                            TRI_transaction_t* const trx,
                            TRI_transaction_doc_mptr_t* const doc) {
  TRI_transaction_doc_mptr_t* old;
  const TRI_transaction_local_id_t id = TRI_LocalIdTransaction(trx);

  assert(id == DocumentTransaction(doc));
  assert(doc->_validTo == 0);
  assert(doc->_data != NULL);

  // start critical section -----------------------------
  TRI_WriteLockReadWriteLock(&idx->_lock); 

  old = Insert(idx, trx, doc);
  if (old != NULL) {
    // duplicate key error
    TRI_WriteUnlockReadWriteLock(&idx->_lock); 

    return TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED;
  }
  
  // end critical section -----------------------------
  TRI_WriteUnlockReadWriteLock(&idx->_lock); 

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief updates an element in the primary index
////////////////////////////////////////////////////////////////////////////////

int TRI_UpdatePrimaryIndex (TRI_primary_index_t* const idx,
                            TRI_transaction_t* const trx,
                            TRI_transaction_doc_mptr_t* const doc,
                            TRI_revision_constraint_t* const constraint) {
  TRI_transaction_doc_mptr_t* old;
  const TRI_transaction_local_id_t id = TRI_LocalIdTransaction(trx);
  int result;

  assert(id == DocumentTransaction(doc));
  assert(doc->_validTo == 0);
  assert(doc->_data != NULL);
  
  // start critical section -----------------------------
  TRI_WriteLockReadWriteLock(&idx->_lock); 
 
  old = Lookup(idx, trx, doc->_key);

  result = CheckConstraint(constraint, old);
  if (result != TRI_ERROR_NO_ERROR) {
    // an error occurred
    // end critical section -----------------------------
    TRI_WriteUnlockReadWriteLock(&idx->_lock); 

    return result;
  }
  
  // update previous revision
  assert(old);
  old->_validTo = id;

  // end critical section -----------------------------
  TRI_WriteUnlockReadWriteLock(&idx->_lock); 

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an element from the primary index
////////////////////////////////////////////////////////////////////////////////

int TRI_DeletePrimaryIndex (TRI_primary_index_t* const idx,
                            TRI_transaction_t* const trx,
                            TRI_transaction_doc_mptr_t* const doc,
                            TRI_revision_constraint_t* const constraint) {
  TRI_transaction_doc_mptr_t* old;
  const TRI_transaction_local_id_t id = TRI_LocalIdTransaction(trx);
  int result;

  assert(id == DocumentTransaction(doc));
  assert(doc->_validTo == 0);
  assert(doc->_data == NULL);
  
  // start critical section -----------------------------
  TRI_WriteLockReadWriteLock(&idx->_lock); 
 
  old = Lookup(idx, trx, doc->_key);

  result = CheckConstraint(constraint, old);
  if (result != TRI_ERROR_NO_ERROR) {
    // an error occurred
    // end critical section -----------------------------
    TRI_WriteUnlockReadWriteLock(&idx->_lock); 

    return result;
  }

  // update previous revision
  assert(old);
  old->_validTo = id;

  // end critical section -----------------------------
  TRI_WriteUnlockReadWriteLock(&idx->_lock); 

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
