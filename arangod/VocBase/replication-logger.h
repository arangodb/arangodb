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

#include "BasicsC/associative.h"
#include "BasicsC/locks.h"
#include "BasicsC/vector.h"

#include "VocBase/replication-common.h"
#include "VocBase/server-id.h"
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
struct TRI_vocbase_s;
struct TRI_vocbase_col_s;
struct TRI_vocbase_defaults_s;

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
/// @brief logger configuration
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_replication_logger_configuration_s {
  bool                                   _logRemoteChanges;
}
TRI_replication_logger_configuration_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief state information about replication logging
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_replication_logger_state_s {
  TRI_voc_tick_t                         _lastLogTick;
  bool                                   _active;
}
TRI_replication_logger_state_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief context information for replication logging
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_replication_logger_s {
  TRI_read_write_lock_t                  _statusLock;
  TRI_spin_t                             _idLock;
  TRI_spin_t                             _bufferLock;
  TRI_vector_pointer_t                   _buffers;
  TRI_read_write_lock_t                  _clientsLock;
  TRI_associative_pointer_t              _clients;

  struct TRI_vocbase_s*                  _vocbase;
  struct TRI_transaction_s*              _trx;
  struct TRI_transaction_collection_s*   _trxCollection;

  TRI_replication_logger_state_t         _state;
  TRI_replication_logger_configuration_t _configuration;
  TRI_server_id_t                        _localServerId;

  char*                                  _databaseName;
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

TRI_replication_logger_t* TRI_CreateReplicationLogger (struct TRI_vocbase_s*,
                                                       struct TRI_vocbase_defaults_s*);

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
/// @brief get a JSON representation of the replication logger configuration
////////////////////////////////////////////////////////////////////////////////

struct TRI_json_s* TRI_JsonConfigurationReplicationLogger (TRI_replication_logger_configuration_t const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief configure the replication logger
////////////////////////////////////////////////////////////////////////////////

int TRI_ConfigureReplicationLogger (TRI_replication_logger_t*,
                                    TRI_replication_logger_configuration_t const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief copy a logger configuration
////////////////////////////////////////////////////////////////////////////////

void TRI_CopyConfigurationReplicationLogger (TRI_replication_logger_configuration_t const*,
                                             TRI_replication_logger_configuration_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief return the list of clients as a JSON array
////////////////////////////////////////////////////////////////////////////////

struct TRI_json_s* TRI_JsonClientsReplicationLogger (TRI_replication_logger_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief insert an applier action into an action list
////////////////////////////////////////////////////////////////////////////////

void TRI_UpdateClientReplicationLogger (TRI_replication_logger_t*,
                                        TRI_server_id_t,
                                        char const*);

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
                                TRI_replication_logger_state_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief get a JSON representation of a logger state
////////////////////////////////////////////////////////////////////////////////
  
struct TRI_json_s* TRI_JsonStateReplicationLogger (TRI_replication_logger_state_t const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief return a JSON representation of the replication logger
////////////////////////////////////////////////////////////////////////////////

struct TRI_json_s* TRI_JsonReplicationLogger (TRI_replication_logger_t*);

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

int TRI_LogTransactionReplication (struct TRI_vocbase_s*,
                                   struct TRI_transaction_s const*,
                                   TRI_server_id_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief replicate a "create collection" operation
////////////////////////////////////////////////////////////////////////////////

int TRI_LogCreateCollectionReplication (struct TRI_vocbase_s*,
                                        TRI_voc_cid_t,
                                        char const*, 
                                        struct TRI_json_s const*,
                                        TRI_server_id_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief replicate a "drop collection" operation
////////////////////////////////////////////////////////////////////////////////

int TRI_LogDropCollectionReplication (struct TRI_vocbase_s*,
                                      TRI_voc_cid_t,
                                      char const*,
                                      TRI_server_id_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief replicate a "rename collection" operation
////////////////////////////////////////////////////////////////////////////////

int TRI_LogRenameCollectionReplication (struct TRI_vocbase_s*,
                                        TRI_voc_cid_t,
                                        char const*,
                                        TRI_server_id_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief replicate a "change collection properties" operation
////////////////////////////////////////////////////////////////////////////////

int TRI_LogChangePropertiesCollectionReplication (struct TRI_vocbase_s*,
                                                  TRI_voc_cid_t,
                                                  char const*,
                                                  struct TRI_json_s const*,
                                                  TRI_server_id_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief replicate a "create index" operation
////////////////////////////////////////////////////////////////////////////////

int TRI_LogCreateIndexReplication (struct TRI_vocbase_s*,
                                   TRI_voc_cid_t,
                                   char const*,
                                   TRI_idx_iid_t,
                                   struct TRI_json_s const*,
                                   TRI_server_id_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief replicate a "drop index" operation
////////////////////////////////////////////////////////////////////////////////

int TRI_LogDropIndexReplication (struct TRI_vocbase_s*,
                                 TRI_voc_cid_t,
                                 char const*,
                                 TRI_idx_iid_t iid,
                                 TRI_server_id_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief replicate a document operation
////////////////////////////////////////////////////////////////////////////////

int TRI_LogDocumentReplication (struct TRI_vocbase_s*,
                                struct TRI_document_collection_s*,
                                TRI_voc_document_operation_e,
                                struct TRI_df_marker_s const*,
                                struct TRI_doc_mptr_s const*,
                                TRI_server_id_t);

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
