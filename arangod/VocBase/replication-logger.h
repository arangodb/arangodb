////////////////////////////////////////////////////////////////////////////////
/// @brief replication logger
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
/// @author Jan Steemann
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_VOC_BASE_REPLICATION_LOGGER_H
#define TRIAGENS_VOC_BASE_REPLICATION_LOGGER_H 1

#include "BasicsC/common.h"

#include "BasicsC/locks.h"
#include "BasicsC/vector.h"

#include "VocBase/replication-common.h"
#include "VocBase/vocbase.h"
#include "VocBase/voc-types.h"

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                              forward declarations
// -----------------------------------------------------------------------------

struct TRI_df_marker_s;
struct TRI_document_collection_s;
struct TRI_doc_mptr_s;
struct TRI_json_s;
struct TRI_transaction_s;
struct TRI_transaction_collection_s;
struct TRI_vocbase_col_s;
struct TRI_vocbase_s;

// -----------------------------------------------------------------------------
// --SECTION--                                                REPLICATION LOGGER
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief state information about replication logging
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_replication_log_state_s {
  TRI_voc_tick_t                       _lastLogTick;
  bool                                 _active;
}
TRI_replication_log_state_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief context information for replication logging
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_replication_logger_s {
  TRI_read_write_lock_t                _statusLock;
  TRI_spin_t                           _idLock;
  TRI_spin_t                           _bufferLock;
  TRI_vector_pointer_t                 _buffers;
  struct TRI_vocbase_s*                _vocbase;
  struct TRI_transaction_s*            _trx;
  struct TRI_transaction_collection_s* _trxCollection;

  TRI_replication_log_state_t          _state;

  char*                                _databaseName;
}
TRI_replication_logger_t;

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
/// @brief create a replication logger
////////////////////////////////////////////////////////////////////////////////

TRI_replication_logger_t* TRI_CreateReplicationLogger (struct TRI_vocbase_s*);

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy a replication logger
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyReplicationLogger (TRI_replication_logger_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief free a replication logger
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeReplicationLogger (TRI_replication_logger_t*);

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
/// @brief start the replication logger
////////////////////////////////////////////////////////////////////////////////

int TRI_StartReplicationLogger (TRI_replication_logger_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief stop the replication logger
////////////////////////////////////////////////////////////////////////////////

int TRI_StopReplicationLogger (TRI_replication_logger_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief get the current replication state
////////////////////////////////////////////////////////////////////////////////

int TRI_StateReplicationLogger (TRI_replication_logger_t*,
                                TRI_replication_log_state_t*);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                              public log functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief replicate a transaction
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_REPLICATION

int TRI_LogTransactionReplication (struct TRI_vocbase_s*,
                                   struct TRI_transaction_s const*);

#else

#define TRI_LogTransactionReplication(...)

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief replicate a "create collection" operation
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_REPLICATION

int TRI_LogCreateCollectionReplication (struct TRI_vocbase_s*,
                                        TRI_voc_cid_t, 
                                        struct TRI_json_s const*);

#else

#define TRI_LogCreateCollectionReplication(...)

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief replicate a "drop collection" operation
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_REPLICATION

int TRI_LogDropCollectionReplication (struct TRI_vocbase_s*,
                                      TRI_voc_cid_t);

#else

#define TRI_LogDropCollectionReplication(...)

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief replicate a "rename collection" operation
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_REPLICATION

int TRI_LogRenameCollectionReplication (struct TRI_vocbase_s*,
                                        TRI_voc_cid_t,
                                        char const*);

#else

#define TRI_LogRenameCollectionReplication(...)

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief replicate a "change collection properties" operation
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_REPLICATION

int TRI_LogChangePropertiesCollectionReplication (struct TRI_vocbase_s*,
                                                  TRI_voc_cid_t,
                                                  struct TRI_json_s const*);

#else

#define TRI_LogChangePropertiesCollectionReplication(...)

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief replicate a "create index" operation
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_REPLICATION

int TRI_LogCreateIndexReplication (struct TRI_vocbase_s*,
                                   TRI_voc_cid_t,
                                   TRI_idx_iid_t,
                                   struct TRI_json_s const*);

#else

#define TRI_LogCreateIndexReplication(...)

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief replicate a "drop index" operation
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_REPLICATION

int TRI_LogDropIndexReplication (struct TRI_vocbase_s*,
                                 TRI_voc_cid_t,
                                 TRI_idx_iid_t iid);

#else

#define TRI_LogDropIndexReplication(...)

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief replicate a document operation
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_REPLICATION

int TRI_LogDocumentReplication (struct TRI_vocbase_s*,
                                struct TRI_document_collection_s*,
                                TRI_voc_document_operation_e,
                                struct TRI_df_marker_s const*,
                                struct TRI_doc_mptr_s const*);

#else

#define TRI_LogDocumentReplication(...)

#endif

////////////////////////////////////////////////////////////////////////////////
/// @} 
////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
