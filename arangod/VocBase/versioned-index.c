////////////////////////////////////////////////////////////////////////////////
/// @brief versioned index
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

#include "versioned-index.h"

#include "BasicsC/hashes.h"

#include "VocBase/primary-collection.h"
#include "VocBase/transaction.h"

#define INITIAL_SIZE (128)

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief get the transaction id from a context
////////////////////////////////////////////////////////////////////////////////

static TRI_transaction_local_id_t GetTransactionId (const TRI_doc_operation_context_t* const context) {
  return TRI_LocalIdTransaction(context->_transaction);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check whether a specific transaction is currently running
/// (not yet implemented)
////////////////////////////////////////////////////////////////////////////////

static bool InWriteTransactionsTable (const TRI_doc_operation_context_t* const context,
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
/// @brief mark a document revision as obsolete
////////////////////////////////////////////////////////////////////////////////

static void MarkObsolete (TRI_doc_operation_context_t* const context,
                          TRI_doc_mptr_t* const old) {
  assert(old != NULL);
  
  old->_validTo = GetTransactionId(context);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compares a document key and a document
////////////////////////////////////////////////////////////////////////////////

static bool IsVisible (TRI_doc_operation_context_t* const context,
                       const char* const key, 
                       const TRI_doc_mptr_t* const element) {
  const TRI_transaction_local_id_t ownId = GetTransactionId(context);

  if (element->_validFrom > ownId || element->_validTo == ownId) {
    // element was created by a newer trx (invisible for us) or deleted by the current trx
    return false;
  }
  
  if (element->_validTo > ownId) {
    // element was deleted by a newer trx (invisible for us)
    return false;
  }
  
  if (strcmp(key, element->_key) != 0) {
    // element has a different key
    return false;
  }

  if (element->_validFrom < ownId && InWriteTransactionsTable(context, element->_validFrom)) {
    // element was created by an older, aborted/unfinished transaction
    return false;
  }

  if (element->_validTo != 0 && InWriteTransactionsTable(context, element->_validTo)) {
    // element was deleted by an older, aborted/unfinished transaction
    return false;
  }

  // element is visible
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief resizes the index
////////////////////////////////////////////////////////////////////////////////

static bool Resize (TRI_versioned_index_t* const idx) {
  TRI_doc_mptr_t** oldTable;
  uint64_t oldAlloc;
  uint64_t j;

  oldTable = idx->_table;
  oldAlloc = idx->_nrAlloc;

  idx->_nrAlloc = 2 * idx->_nrAlloc + 1;
  idx->_table = (TRI_doc_mptr_t**) TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, idx->_nrAlloc * sizeof(TRI_doc_mptr_t*), true);

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
/// @brief looks up a document in the index, using its key
////////////////////////////////////////////////////////////////////////////////
 
static TRI_doc_mptr_t* Lookup (const TRI_versioned_index_t* const idx,
                               TRI_doc_operation_context_t* const context,
                               const char* const key,
                               uint64_t* position) {
  const uint64_t hash = HashKey(key);
  uint64_t i;
  
  i = hash % idx->_nrAlloc;

  // search the table
  while (idx->_table[i] != NULL) {
    if (IsVisible(context, key, idx->_table[i])) {
      if (position != NULL) {
        *position = i;
      }
      return idx->_table[i];
    }

    i = (i + 1) % idx->_nrAlloc;
  }

  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief insert a document into the index and return the previous revision
////////////////////////////////////////////////////////////////////////////////
 
static TRI_doc_mptr_t* Insert (TRI_versioned_index_t* const idx,
                               TRI_doc_operation_context_t* const context,
                               TRI_doc_mptr_t* const doc) {
  const uint64_t hash = HashKey(doc->_key);
  uint64_t i;
  TRI_doc_mptr_t* old;
  
  if (idx->_nrAlloc == idx->_nrUsed) {
    return NULL;
  }

  i = hash % idx->_nrAlloc;

  // search the table
  while (idx->_table[i] != NULL && ! IsVisible(context, doc->_key, idx->_table[i])) {
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
/// @brief update a document in the index by either replacing the previous
/// revision or adding a new revision.
/// we can update in place if the document to update was created in the same
/// trx. if the document to update was created by a different trx, we must
/// create a new version because other trx might still see the old document
////////////////////////////////////////////////////////////////////////////////
 
static int Update (TRI_versioned_index_t* const idx,
                   TRI_doc_operation_context_t* const context,
                   const uint64_t position,
                   TRI_doc_mptr_t* const doc,
                   TRI_doc_mptr_t* const old) {
  const TRI_transaction_local_id_t ownId = GetTransactionId(context);
  int res;

  assert(doc);
  assert(old);

  res = TRI_ERROR_NO_ERROR;

  if (old->_validFrom == ownId) {
    // the document to update was created in our trx, no other trx can see it
    // we can update it in place
    idx->_table[position] = doc;
  }
  else {
    // the document to update was created by a different trx, must create a
    // new version of it and obsolete the old one
    MarkObsolete(context, old);

    if (Insert(idx, context, doc) != NULL) {
      // another visible revision was found in the index. this should never happen
      res = TRI_ERROR_INTERNAL;
    }
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief delete a document from the index by either removing the previous
/// revision or marking it as obsolete
/// we can remove the revision if the document to delete was created in the same
/// trx. if the document to delete was created by a different trx, we must
/// mark it is as obsolete because other trx might still see the old document
////////////////////////////////////////////////////////////////////////////////
 
static int Delete (TRI_versioned_index_t* const idx,
                   TRI_doc_operation_context_t* const context,
                   uint64_t position,
                   TRI_doc_mptr_t* const old) {
  const TRI_transaction_local_id_t ownId = GetTransactionId(context);

  assert(old);

  if (old->_validFrom == ownId) {
    uint64_t i;

    // the document to delete was created in our trx, no other trx can see it
    // we can directly remove it from the index
    idx->_table[position] = NULL;
    idx->_nrUsed--;
  
    // we must now reposition elements so that there are no gaps
    i = (position + 1) % idx->_nrAlloc;
    while (idx->_table[i] != NULL) {
      const uint64_t j = HashKey(idx->_table[i]->_key) % idx->_nrAlloc;

      if ((position < i && ! (position < j && j <= i)) || (i < position && ! (position < j || j <= i))) {
        idx->_table[position] = idx->_table[i];
        idx->_table[i] = NULL;
        position = i;
      }

      i = (i + 1) % idx->_nrAlloc;
    }
  }
  else {
    // the document to delete was created by a different trx
    // we must obsolete the old one
    MarkObsolete(context, old);
  }

  return TRI_ERROR_NO_ERROR;
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
/// @brief create the versioned index
////////////////////////////////////////////////////////////////////////////////

TRI_versioned_index_t* TRI_CreateVersionedIndex () {
  TRI_versioned_index_t* idx;

  idx = (TRI_versioned_index_t*) TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_versioned_index_t), false);
  if (idx == NULL) {
    return NULL;
  }
  
  TRI_InitReadWriteLock(&idx->_lock);

  idx->_table = (TRI_doc_mptr_t**) TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_doc_mptr_t*) * INITIAL_SIZE, true);
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
/// @brief free the versioned index
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeVersionedIndex (TRI_versioned_index_t* const idx) {
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

////////////////////////////////////////////////////////////////////////////////
/// @brief look up an element in the versioned index
////////////////////////////////////////////////////////////////////////////////

TRI_doc_mptr_t* TRI_LookupVersionedIndex (TRI_versioned_index_t* const idx,
                                          TRI_doc_operation_context_t* const context, 
                                          const char* const key) {
  TRI_doc_mptr_t* old; 

  TRI_ReadLockReadWriteLock(&idx->_lock); 
  old = Lookup(idx, context, key, NULL);
  TRI_ReadUnlockReadWriteLock(&idx->_lock); 
  
  return old;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an element to the versioned index
////////////////////////////////////////////////////////////////////////////////

int TRI_InsertVersionedIndex (TRI_versioned_index_t* const idx,
                              TRI_doc_operation_context_t* const context, 
                              TRI_doc_mptr_t* const doc) {
  TRI_doc_mptr_t* old;
  int res;

  assert(doc->_validTo == 0);
  assert(doc->_data != NULL);

  TRI_WriteLockReadWriteLock(&idx->_lock); 

  old = Insert(idx, context, doc);
  if (old != NULL) {
    // duplicate key error
    res = TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED;
  }
  else {
    res = TRI_ERROR_NO_ERROR;
  }
  
  TRI_WriteUnlockReadWriteLock(&idx->_lock); 

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief updates an element in the versioned index
////////////////////////////////////////////////////////////////////////////////

int TRI_UpdateVersionedIndex (TRI_versioned_index_t* const idx,
                              TRI_doc_operation_context_t* const context, 
                              TRI_doc_mptr_t* const doc) {
  TRI_doc_mptr_t* old;
  uint64_t position;
  int res;

  assert(doc->_validTo == 0);
  assert(doc->_data != NULL);
  
  TRI_WriteLockReadWriteLock(&idx->_lock); 
  old = Lookup(idx, context, doc->_key, &position);

  if (old == NULL) {
    res = TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND;
  }
  else {
    res = TRI_RevisionCheck(context, old->_rid);
    if (res == TRI_ERROR_NO_ERROR) {
      // update/replace previous revision
      Update(idx, context, position, old, doc);
    }
  }
  
  TRI_WriteUnlockReadWriteLock(&idx->_lock); 

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an element from the versioned index
////////////////////////////////////////////////////////////////////////////////

int TRI_DeleteVersionedIndex (TRI_versioned_index_t* const idx,
                              TRI_doc_operation_context_t* const context, 
                              TRI_doc_mptr_t* const doc) {
  TRI_doc_mptr_t* old;
  uint64_t position;
  int res;

  assert(doc->_validTo == 0);
  assert(doc->_data == NULL);
  
  TRI_WriteLockReadWriteLock(&idx->_lock); 
  old = Lookup(idx, context, doc->_key, &position);

  if (old == NULL) {
    res = TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND;
  }
  else {
    res = TRI_RevisionCheck(context, old->_rid);
    if (res == TRI_ERROR_NO_ERROR) {
      // update/replace previous revision
      Delete(idx, context, position, old);
    }
  }

  TRI_WriteUnlockReadWriteLock(&idx->_lock); 

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
