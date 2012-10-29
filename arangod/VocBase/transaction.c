////////////////////////////////////////////////////////////////////////////////
/// @brief transaction subsystem
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

#include "transaction.h" 

#include "BasicsC/logging.h" 
#include "BasicsC/strings.h" 

// -----------------------------------------------------------------------------
// --SECTION--                                               TRANSACTION CONTEXT
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief generate a transaction id
////////////////////////////////////////////////////////////////////////////////

static TRI_transaction_local_id_t NextLocalTransactionId (TRI_transaction_context_t* const context) {
  TRI_transaction_local_id_t id;

  TRI_LockSpin(&context->_idLock); 
  id = ++context->_id._localId;
  TRI_UnlockSpin(&context->_idLock);

  return (TRI_transaction_local_id_t) id;
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
/// @brief create the global transaction context
////////////////////////////////////////////////////////////////////////////////

TRI_transaction_context_t* TRI_CreateTransactionContext (TRI_transaction_server_id_t serverId) {
  TRI_transaction_context_t* context;

  context = (TRI_transaction_context_t*) TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_transaction_context_t), false);
  if (context == NULL) {
    return context;
  }

  TRI_InitSpin(&context->_idLock);
  context->_id._localId  = 0;
  context->_id._serverId = serverId;

  return context;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free the global transaction context
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeTransactionContext (TRI_transaction_context_t* const context) {
  TRI_DestroySpin(&context->_idLock);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, context);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                       TRANSACTION
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief return the type of the transaction as a string
////////////////////////////////////////////////////////////////////////////////

static const char* TypeString (const TRI_transaction_type_e type) {
  switch (type) {
    case TRI_TRANSACTION_READ:
      return "read";
    case TRI_TRANSACTION_WRITE:
      return "write";
  }

  assert(false);
  return "unknown";
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the status of the transaction as a string
////////////////////////////////////////////////////////////////////////////////

static const char* StatusString (const TRI_transaction_status_e status) {
  switch (status) {
    case TRI_TRANSACTION_NONE:
      return "none";
    case TRI_TRANSACTION_INITIALISED:
      return "initialised";
    case TRI_TRANSACTION_RUNNING:
      return "running";
    case TRI_TRANSACTION_COMMITTED:
      return "committed";
    case TRI_TRANSACTION_ABORTED:
      return "aborted";
  }

  assert(false);
  return "unknown";
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a transaction collection container
////////////////////////////////////////////////////////////////////////////////

static TRI_transaction_collection_t* CreateCollection (const char* const name, 
                                                       const TRI_transaction_type_e type) {
  TRI_transaction_collection_t* collection;

  collection = (TRI_transaction_collection_t*) TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_transaction_collection_t), false);
  if (collection == NULL) {
    return NULL;
  }

  collection->_name = TRI_DuplicateStringZ(TRI_UNKNOWN_MEM_ZONE, name);
  if (collection->_name == NULL) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, collection);

    return NULL;
  }

  collection->_type = type;

  return collection;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free a transaction collection container
////////////////////////////////////////////////////////////////////////////////

static void FreeCollection (TRI_transaction_collection_t* collection) {
  assert(collection);
  assert(collection->_name);

  TRI_Free(TRI_UNKNOWN_MEM_ZONE, collection);
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
/// @brief create a new transaction container
////////////////////////////////////////////////////////////////////////////////

TRI_transaction_t* TRI_CreateTransaction (TRI_transaction_context_t* const context,
                                          const TRI_transaction_isolation_level_e isolationLevel) {
  TRI_transaction_t* trx;
  
  trx = (TRI_transaction_t*) TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(TRI_transaction_t), false);
  if (trx == NULL) {
    // out of memory
    return NULL;
  } 

  trx->_context        = context;
  trx->_id._serverId   = context->_id._serverId;
  trx->_id._localId    = NextLocalTransactionId(context);
  trx->_status         = TRI_TRANSACTION_NONE;  
  trx->_type           = TRI_TRANSACTION_READ;
  trx->_isolationLevel = isolationLevel;
  
  TRI_InitVectorPointer(&trx->_collections, TRI_UNKNOWN_MEM_ZONE);

  return trx;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free a transaction container
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeTransaction (TRI_transaction_t* const trx) {
  size_t i;

  assert(trx);

  if (trx->_status == TRI_TRANSACTION_RUNNING) {
    TRI_AbortTransaction(trx);
  }

  // free all collections
  i = trx->_collections._length;
  while (i-- > 0) {
    TRI_transaction_collection_t* collection = TRI_AtVectorPointer(&trx->_collections, i);

    FreeCollection(collection);
    if (i == 0) {
      break;
    }
  }

  TRI_DestroyVectorPointer(&trx->_collections);

  TRI_Free(TRI_UNKNOWN_MEM_ZONE, trx);
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
/// @brief return the local id of a transaction
////////////////////////////////////////////////////////////////////////////////

TRI_transaction_local_id_t TRI_LocalIdTransaction (const TRI_transaction_t* const trx) {
  return trx->_id._localId;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief dump information about a transaction
////////////////////////////////////////////////////////////////////////////////

void TRI_DumpTransaction (TRI_transaction_t* const trx) {
  size_t i, n;

  LOG_INFO("transaction id: %lu:%lu, type: %s, status: %s", 
           (unsigned long) trx->_id._serverId, 
           (unsigned long) trx->_id._localId, 
           TypeString(trx->_type), 
           StatusString(trx->_status));

  n = trx->_collections._length;
  for (i = 0; i < n; ++i) {
    TRI_transaction_collection_t* collection = TRI_AtVectorPointer(&trx->_collections, i);

    LOG_INFO("- collection: %s, type: %s", collection->_name, TypeString(collection->_type));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add a collection to a transaction
////////////////////////////////////////////////////////////////////////////////

bool TRI_AddCollectionTransaction (TRI_transaction_t* const trx,
                                   const char* const name,
                                   const TRI_transaction_type_e type) {
  TRI_transaction_collection_t* collection;
  size_t i, n;

  assert(trx->_status == TRI_TRANSACTION_NONE);
  assert(name);

  // upgrade transaction type if required
  if (type == TRI_TRANSACTION_WRITE && trx->_type == TRI_TRANSACTION_READ) {
    trx->_type = TRI_TRANSACTION_WRITE;
  }

  // check if we already have got this collection in the vector
  n = trx->_collections._length;
  for (i = 0; i < n; ++i) {
    int res;
    
    collection = TRI_AtVectorPointer(&trx->_collections, i);
    res = strcmp(name, collection->_name);

    if (res < 0) {
      collection = CreateCollection(name, type);
      if (collection == NULL) {
        // out of memory
        return false;
      }

      TRI_InsertVectorPointer(&trx->_collections, collection, i);
      return true;
    }

    if (res == 0) {
      // collection is already contained in vector

      // upgrade collection type if required
      if (type == TRI_TRANSACTION_WRITE && collection->_type == TRI_TRANSACTION_READ) {
        collection->_type = TRI_TRANSACTION_WRITE;
      }
      return true;
    }
  }
  
  // collection was not contained. now insert it
  collection = CreateCollection(name, type);
  if (collection == NULL) {
    // out of memory
    return false;
  }

  TRI_PushBackVectorPointer(&trx->_collections, collection);
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief start a transaction
////////////////////////////////////////////////////////////////////////////////

int TRI_StartTransaction (TRI_transaction_t* const trx) {
  assert(trx->_status == TRI_TRANSACTION_NONE);

  trx->_status = TRI_TRANSACTION_INITIALISED;
  trx->_status = TRI_TRANSACTION_RUNNING;

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief commit a transaction
////////////////////////////////////////////////////////////////////////////////

int TRI_CommitTransaction (TRI_transaction_t* const trx) {
  assert(trx->_status == TRI_TRANSACTION_RUNNING);
  
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief abort a transaction
////////////////////////////////////////////////////////////////////////////////

int TRI_AbortTransaction (TRI_transaction_t* const trx) {
  assert(trx->_status == TRI_TRANSACTION_INITIALISED ||
         trx->_status == TRI_TRANSACTION_RUNNING);
  
  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
