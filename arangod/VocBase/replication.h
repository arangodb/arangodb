////////////////////////////////////////////////////////////////////////////////
/// @brief replication functions
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

#ifndef TRIAGENS_VOC_BASE_REPLICATION_H
#define TRIAGENS_VOC_BASE_REPLICATION_H 1

#include "BasicsC/common.h"

#include "BasicsC/locks.h"
#include "ShapedJson/shaped-json.h"

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
struct TRI_shape_s;
struct TRI_string_buffer_s;
struct TRI_transaction_s;
struct TRI_transaction_collection_s;
struct TRI_vocbase_col_s;
struct TRI_vocbase_s;

// -----------------------------------------------------------------------------
// --SECTION--                                                REPLICATION LOGGER
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                    public defines
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief default size for each log file
////////////////////////////////////////////////////////////////////////////////

#define TRI_REPLICATION_DEFAULT_LOG_SIZE (64 * 1024 * 1024)

////////////////////////////////////////////////////////////////////////////////
/// @brief HTTP response header for "check for more data?"
////////////////////////////////////////////////////////////////////////////////

#define TRI_REPLICATION_HEADER_CHECKMORE "x-arango-replication-checkmore"

////////////////////////////////////////////////////////////////////////////////
/// @brief HTTP response header for "last found tick"
////////////////////////////////////////////////////////////////////////////////

#define TRI_REPLICATION_HEADER_LASTFOUND "x-arango-replication-lastfound"

////////////////////////////////////////////////////////////////////////////////
/// @brief HTTP response header for "replication active"
////////////////////////////////////////////////////////////////////////////////

#define TRI_REPLICATION_HEADER_ACTIVE    "x-arango-replication-active"

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief replication dump container
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_replication_dump_s {
  struct TRI_string_buffer_s*  _buffer;
  TRI_voc_tick_t               _lastFoundTick;
  TRI_shape_sid_t              _lastSid;
  struct TRI_shape_s const*    _lastShape;
  bool                         _failed;
  bool                         _hasMore;
  bool                         _bufferFull;
}
TRI_replication_dump_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief state information about replication logging
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_replication_log_state_s {
  TRI_voc_tick_t  _firstTick;
  TRI_voc_tick_t  _lastTick;
  bool            _active;
}
TRI_replication_log_state_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief context information for replication logging
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_replication_logger_s {
  TRI_read_write_lock_t                _statusLock;
  TRI_spin_t                           _idLock;
  struct TRI_vocbase_s*                _vocbase;
  struct TRI_transaction_s*            _trx;
  struct TRI_transaction_collection_s* _trxCollection;

  TRI_replication_log_state_t          _state;

  bool                                 _waitForSync;
  int64_t                              _logSize;
  char*                                _databaseName;
}
TRI_replication_logger_t;

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
// --SECTION--                                                       REPLICATION
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                     log functions
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

// -----------------------------------------------------------------------------
// --SECTION--                                                    dump functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief dump data from a single collection
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_REPLICATION

int TRI_DumpCollectionReplication (TRI_replication_dump_t*,
                                   struct TRI_vocbase_col_s*,
                                   TRI_voc_tick_t,
                                   TRI_voc_tick_t,
                                   uint64_t);

#else

#define TRI_DumpCollectionReplication(...)

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief dump data from the replication log
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_REPLICATION

int TRI_DumpLogReplication (struct TRI_vocbase_s*, 
                            TRI_replication_dump_t*,
                            TRI_voc_tick_t,
                            TRI_voc_tick_t,
                            uint64_t);

#else

#define TRI_DumpLogReplication(...)

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief initialise a replication dump container
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_REPLICATION

void TRI_InitDumpReplication (TRI_replication_dump_t*);

#else

#define TRI_InitDumpReplication(...)

#endif

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                           REPLICATION APPLICATION
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief state information about replication master
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_replication_master_info_s {
  char*                        _endpoint;
  TRI_server_id_t              _serverId;
  int                          _majorVersion;
  int                          _minorVersion;
  TRI_replication_log_state_t  _state;
}
TRI_replication_master_info_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief state information about replication application
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_replication_apply_state_s {
  TRI_voc_tick_t    _lastTick;
  TRI_server_id_t   _serverId;
}
TRI_replication_apply_state_t;

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
/// @brief initialise a master info struct
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_REPLICATION

void TRI_InitMasterInfoReplication (TRI_replication_master_info_t*,
                                    const char*);

#else

#define TRI_InitMasterInfoReplication(...)

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy a master info struct
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_REPLICATION

void TRI_DestroyMasterInfoReplication (TRI_replication_master_info_t*);

#else

#define TRI_DestroyMasterInfoReplication(...)

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief log information about the master state
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_REPLICATION
      
void TRI_LogMasterInfoReplication (TRI_replication_master_info_t const*,
                                   const char*);

#else

#define TRI_LogMasterInfoReplication(...)

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief initialise an apply state struct
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_REPLICATION

void TRI_InitApplyStateReplication (TRI_replication_apply_state_t*);

#else

#define TRI_InitApplyStateReplication(...)

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief save the replication application state to a file
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_REPLICATION

int TRI_SaveApplyStateReplication (struct TRI_vocbase_s*,
                                   TRI_replication_apply_state_t const*,
                                   bool);

#else

#define TRI_SaveApplyStateReplication(...)

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief load the replication application state from a file
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_REPLICATION

int TRI_LoadApplyStateReplication (struct TRI_vocbase_s*,
                                   TRI_replication_apply_state_t*);

#else

#define TRI_LoadApplyStateReplication(...)

#endif

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  HELPER FUNCTIONS
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief determine whether a collection should be included in replication
////////////////////////////////////////////////////////////////////////////////

bool TRI_ExcludeCollectionReplication (const char*);

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
