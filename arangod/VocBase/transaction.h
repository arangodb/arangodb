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

#ifndef TRIAGENS_DURHAM_VOC_BASE_TRANSACTION_H
#define TRIAGENS_DURHAM_VOC_BASE_TRANSACTION_H 1

#include "BasicsC/common.h"

#include "BasicsC/associative.h"
#include "BasicsC/hashes.h"
#include "BasicsC/locks.h"
#include "BasicsC/vector.h"
#include "BasicsC/voc-errors.h"

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                              forward declarations
// -----------------------------------------------------------------------------

struct TRI_vocbase_s;
struct TRI_vocbase_col_s;

// -----------------------------------------------------------------------------
// --SECTION--                                                 TRANSACTION TYPES 
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief local transaction id typedef
////////////////////////////////////////////////////////////////////////////////

typedef uint64_t TRI_transaction_local_id_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief server identifier type
////////////////////////////////////////////////////////////////////////////////

typedef uint16_t TRI_transaction_server_id_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief transaction id typedef
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_transaction_id_s {
  TRI_transaction_local_id_t  _localId;
  TRI_transaction_server_id_t _serverId;
}
TRI_transaction_id_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief transaction isolation level
////////////////////////////////////////////////////////////////////////////////

typedef enum {
  TRI_TRANSACTION_READ_UNCOMMITED  = 1,
  TRI_TRANSACTION_READ_COMMITTED   = 2,
  TRI_TRANSACTION_READ_REPEATABLE  = 3
}
TRI_transaction_isolation_level_e;

////////////////////////////////////////////////////////////////////////////////
/// @brief transaction type
////////////////////////////////////////////////////////////////////////////////

typedef enum {
  TRI_TRANSACTION_READ  = 1,
  TRI_TRANSACTION_WRITE = 2
}
TRI_transaction_type_e;

////////////////////////////////////////////////////////////////////////////////
/// @brief transaction statuses
////////////////////////////////////////////////////////////////////////////////

typedef enum {
  TRI_TRANSACTION_UNDEFINED    = 0,
  TRI_TRANSACTION_CREATED      = 1,
  TRI_TRANSACTION_RUNNING      = 2,
  TRI_TRANSACTION_COMMITTED    = 3,
  TRI_TRANSACTION_ABORTED      = 4,
  TRI_TRANSACTION_FINISHED     = 5,
  TRI_TRANSACTION_FAILED       = 6
} 
TRI_transaction_status_e;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  TRANSACTION LIST 
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief an entry in the transactions list
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_transaction_list_entry_s {
  TRI_transaction_local_id_t _id;
  TRI_transaction_status_e   _status;
}
TRI_transaction_list_entry_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief transaction list typedef
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_transaction_list_s {
  TRI_vector_t _vector; // vector containing trx_list_entry_t
  size_t _numRunning;   // number of currently running trx
  size_t _numAborted;   // number of already aborted trx
}
TRI_transaction_list_t;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////


// -----------------------------------------------------------------------------
// --SECTION--                                               TRANSACTION CONTEXT
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief global transaction context typedef
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_transaction_context_s {
  TRI_transaction_id_t      _id;                // last transaction id assigned
  TRI_mutex_t               _lock;              // lock used to serialize starting/stopping transactions
  TRI_mutex_t               _collectionLock;    // lock used when accessing _collections
  TRI_mutex_t               _creatorLock;       // lock used when accessing _creators
  TRI_transaction_list_t    _readTransactions;  // global list of currently ongoing read transactions
  TRI_transaction_list_t    _writeTransactions; // global list of currently ongoing write transactions
  TRI_associative_pointer_t _collections;       // list of collections (TRI_transaction_collection_global_t)
  TRI_associative_pointer_t _creators;          // hash of transaction creator pointers (used to prevent nested transactions)
  struct TRI_vocbase_s*     _vocbase;           // pointer to vocbase
}
TRI_transaction_context_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief global instance of a collection in the transaction system
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_transaction_collection_global_s {
  const char*                _name;              // collection name
  TRI_transaction_list_t     _writeTransactions; // list of write-transactions currently going on for the collection
  TRI_mutex_t                _writeLock;         // write lock for the collection, used to serialize writes on the same collection
}
TRI_transaction_collection_global_t;

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

TRI_transaction_context_t* TRI_CreateTransactionContext (struct TRI_vocbase_s*,
                                                         TRI_transaction_server_id_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief free the global transaction context
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeTransactionContext (TRI_transaction_context_t*);

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
/// @brief free all data associated with a specific collection
/// this function must be called for all collections that are dropped
////////////////////////////////////////////////////////////////////////////////

void TRI_RemoveCollectionTransactionContext (TRI_transaction_context_t* const, 
                                             const char* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief dump transaction context data
////////////////////////////////////////////////////////////////////////////////

void TRI_DumpTransactionContext (TRI_transaction_context_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                       TRANSACTION
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief collection used in a transaction
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_transaction_collection_s {
  const char*                          _name;               // collection name
  TRI_transaction_type_e               _type;               // access type (read|write)
  TRI_transaction_list_t               _writeTransactions;  // private copy of other write transactions at transaction start
  struct TRI_vocbase_col_s*            _collection;         // vocbase collection pointer
  TRI_transaction_collection_global_t* _globalInstance;     // pointer to the global instance of the collection in the trx system
  bool                                 _locked;             // lock flag (used for write-transactions)
  bool                                 _externalLock;       // flag whether collection was locked externally
}
TRI_transaction_collection_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief transaction typedef
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_transaction_s {
  TRI_transaction_context_t*        _context;        // global context object
  TRI_transaction_id_t              _id;             // id of transaction
  TRI_transaction_type_e            _type;           // access type (read|write)
  TRI_transaction_status_e          _status;         // current status
  TRI_transaction_isolation_level_e _isolationLevel; // isolation level
  TRI_vector_pointer_t              _collections;    // list of participating collections
  bool                              _isSingleOperation; // trx only consists of one write operation
}
TRI_transaction_t;

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

TRI_transaction_t* TRI_CreateTransaction (TRI_transaction_context_t* const,
                                          const TRI_transaction_isolation_level_e,
                                          const bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief free a transaction container
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeTransaction (TRI_transaction_t* const);

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
/// @brief return whether the transaction consists only of a single operation
////////////////////////////////////////////////////////////////////////////////

bool TRI_IsSingleOperationTransaction (const TRI_transaction_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief return whether the transaction spans multiple write collections
////////////////////////////////////////////////////////////////////////////////

bool TRI_IsMultiCollectionWriteTransaction (const TRI_transaction_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief return the local id of a transaction
////////////////////////////////////////////////////////////////////////////////

TRI_transaction_local_id_t TRI_LocalIdTransaction (const TRI_transaction_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief dump information about a transaction
////////////////////////////////////////////////////////////////////////////////

void TRI_DumpTransaction (TRI_transaction_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief check if a collection is contained in a transaction and return it
////////////////////////////////////////////////////////////////////////////////

struct TRI_vocbase_col_s* TRI_CheckCollectionTransaction (TRI_transaction_t* const,
                                                          const char* const,
                                                          const TRI_transaction_type_e);

////////////////////////////////////////////////////////////////////////////////
/// @brief add a collection to a transaction
////////////////////////////////////////////////////////////////////////////////

int TRI_AddCollectionTransaction (TRI_transaction_t* const,
                                  const char* const, 
                                  const TRI_transaction_type_e,
                                  struct TRI_vocbase_col_s*);

////////////////////////////////////////////////////////////////////////////////
/// @brief start a transaction
////////////////////////////////////////////////////////////////////////////////

int TRI_StartTransaction (TRI_transaction_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief commit a transaction
////////////////////////////////////////////////////////////////////////////////

int TRI_CommitTransaction (TRI_transaction_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief abort a transaction
////////////////////////////////////////////////////////////////////////////////

int TRI_AbortTransaction (TRI_transaction_t* const);

////////////////////////////////////////////////////////////////////////////////
/// @brief finish a transaction
////////////////////////////////////////////////////////////////////////////////

int TRI_FinishTransaction (TRI_transaction_t* const);

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
