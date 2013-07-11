////////////////////////////////////////////////////////////////////////////////
/// @brief replication applier
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

#include "replication-applier.h"

#include "BasicsC/conversions.h"
#include "BasicsC/files.h"
#include "BasicsC/json.h"
#include "BasicsC/logging.h"
#include "BasicsC/string-buffer.h"
#include "BasicsC/tri-strings.h"

#include "VocBase/collection.h"
#include "VocBase/datafile.h"
#include "VocBase/document-collection.h"
#include "VocBase/transaction.h"
#include "VocBase/vocbase.h"


#ifdef TRI_ENABLE_REPLICATION

// -----------------------------------------------------------------------------
// --SECTION--                                               REPLICATION APPLIER
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocBase
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief read a tick value from a JSON struct
////////////////////////////////////////////////////////////////////////////////
    
static int ReadTick (TRI_json_t const* json,
                     char const* attributeName,
                     TRI_voc_tick_t* dst) {
  TRI_json_t* tick;

  assert(json != NULL);
  assert(json->_type == TRI_JSON_ARRAY);
                                     
  tick = TRI_LookupArrayJson(json, attributeName);

  if (! TRI_IsStringJson(tick)) {
    return TRI_ERROR_REPLICATION_INVALID_APPLY_STATE;
  }

  *dst = (TRI_voc_tick_t) TRI_UInt64String2(tick->_value._string.data, tick->_value._string.length -1);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the filename of the replication application file
////////////////////////////////////////////////////////////////////////////////

static char* GetApplyStateFilename (TRI_vocbase_t* vocbase) {
  return TRI_Concatenate2File(vocbase->_path, "REPLICATION");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get a JSON representation of the replication apply state
////////////////////////////////////////////////////////////////////////////////

static TRI_json_t* ApplyStateToJson (TRI_replication_apply_state_t const* state) {
  TRI_json_t* json;
  char* serverId;
  char* lastProcessedContinuousTick;
  char* lastAppliedContinuousTick;
  char* lastAppliedInitialTick;

  json = TRI_CreateArray2Json(TRI_CORE_MEM_ZONE, 4);

  if (json == NULL) {
    return NULL;
  }

  lastProcessedContinuousTick = TRI_StringUInt64(state->_lastProcessedContinuousTick);
  lastAppliedContinuousTick   = TRI_StringUInt64(state->_lastAppliedContinuousTick);
  lastAppliedInitialTick      = TRI_StringUInt64(state->_lastAppliedInitialTick);
  serverId                    = TRI_StringUInt64(state->_serverId);

  TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, 
                       json, 
                       "serverId", 
                       TRI_CreateStringJson(TRI_CORE_MEM_ZONE, serverId));

  TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, 
                       json, 
                       "lastProcessedContinuousTick", 
                       TRI_CreateStringJson(TRI_CORE_MEM_ZONE, lastProcessedContinuousTick));

  TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, 
                       json, 
                       "lastAppliedContinuousTick", 
                       TRI_CreateStringJson(TRI_CORE_MEM_ZONE, lastAppliedContinuousTick));

  TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, 
                       json, 
                       "lastAppliedInitialTick", 
                       TRI_CreateStringJson(TRI_CORE_MEM_ZONE, lastAppliedInitialTick));

  return json;
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
/// @brief create a replication applier
////////////////////////////////////////////////////////////////////////////////

TRI_replication_applier_t* TRI_CreateReplicationApplier (TRI_vocbase_t* vocbase) {
  TRI_replication_applier_t* applier;
  int res;

  applier = TRI_Allocate(TRI_CORE_MEM_ZONE, sizeof(TRI_replication_applier_t), false);

  if (applier == NULL) {
    return NULL;
  }
 /*
  TRI_InitReadWriteLock(&logger->_statusLock);
  TRI_InitSpin(&logger->_idLock);
  TRI_InitSpin(&logger->_bufferLock);

  logger->_vocbase             = vocbase;
  logger->_trx                 = NULL;
  logger->_trxCollection       = NULL;
  logger->_state._firstLogTick = 0;
  logger->_state._lastLogTick  = 0;
  logger->_state._active       = false;
  logger->_databaseName        = TRI_DuplicateStringZ(TRI_CORE_MEM_ZONE, vocbase->_name);

  assert(logger->_databaseName != NULL);
*/
  return applier;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy a replication applier
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyReplicationApplier (TRI_replication_applier_t* applier) {
  TRI_StopReplicationApplier(applier);
/*
  TRI_FreeString(TRI_CORE_MEM_ZONE, logger->_databaseName);
  TRI_DestroySpin(&logger->_bufferLock);
  TRI_DestroySpin(&logger->_idLock);
  TRI_DestroyReadWriteLock(&logger->_statusLock);
  */
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free a replication applier
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeReplicationApplier (TRI_replication_applier_t* applier) {
  TRI_DestroyReplicationApplier(applier);
  TRI_Free(TRI_CORE_MEM_ZONE, applier);
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
/// @brief start the replication applier
////////////////////////////////////////////////////////////////////////////////

int TRI_StartReplicationApplier (TRI_replication_applier_t* applier) {
  int res;
  
  res = TRI_ERROR_NO_ERROR;
/*
  TRI_WriteLockReadWriteLock(&logger->_statusLock);

  if (! logger->_state._active) {
    res = StartReplicationLogger(logger);
  }

  TRI_WriteUnlockReadWriteLock(&logger->_statusLock);
*/
  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stop the replication applier
////////////////////////////////////////////////////////////////////////////////

int TRI_StopReplicationApplier (TRI_replication_applier_t* applier) {
  int res;

  res = TRI_ERROR_NO_ERROR;
 /* 
  TRI_WriteLockReadWriteLock(&logger->_statusLock);

  if (logger->_state._active) {
    res = StopReplicationLogger(logger);
  }

  TRI_WriteUnlockReadWriteLock(&logger->_statusLock);
*/
  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the current replication apply state
////////////////////////////////////////////////////////////////////////////////

int TRI_StateReplicationApplier (TRI_replication_applier_t* applier,
                                 TRI_replication_apply_state_t* state) {
  TRI_vocbase_t* vocbase;
  int res;

  res = TRI_ERROR_NO_ERROR;
/*
  vocbase = logger->_vocbase;

  TRI_WriteLockReadWriteLock(&applier->_statusLock);

  if (logger->_state._active) {
    // use state from logger
    res = GetStateReplicationLogger(logger, state);
  }
  else {
    // read first/last directly from collection
    res = GetStateInactive(logger->_vocbase, state);
  }

  TRI_WriteUnlockReadWriteLock(&applier->_statusLock);
*/
  return res;
} 

////////////////////////////////////////////////////////////////////////////////
/// @brief initialise an apply state struct
////////////////////////////////////////////////////////////////////////////////

void TRI_InitApplyStateReplication (TRI_replication_apply_state_t* state) {
  memset(state, 0, sizeof(TRI_replication_apply_state_t));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief remove the replication application state file
////////////////////////////////////////////////////////////////////////////////

int TRI_RemoveApplyStateReplication (TRI_vocbase_t* vocbase) {
  char* filename;
  int res;

  filename = GetApplyStateFilename(vocbase);

  if (filename == NULL) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  if (TRI_ExistsFile(filename)) {
    res = TRI_UnlinkFile(filename);
  }
  else {
    res = TRI_ERROR_NO_ERROR;
  }

  TRI_FreeString(TRI_CORE_MEM_ZONE, filename);

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief save the replication application state to a file
////////////////////////////////////////////////////////////////////////////////

int TRI_SaveApplyStateReplication (TRI_vocbase_t* vocbase,
                                   TRI_replication_apply_state_t const* state,
                                   bool sync) {
  TRI_json_t* json;
  char* filename;
  int res;

  json = ApplyStateToJson(state);

  if (json == NULL) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  filename = GetApplyStateFilename(vocbase);

  if (! TRI_SaveJson(filename, json, sync)) {
    res = TRI_ERROR_INTERNAL;
  }
  else {
    res = TRI_ERROR_NO_ERROR;
  }

  TRI_FreeString(TRI_CORE_MEM_ZONE, filename);
  TRI_FreeJson(TRI_CORE_MEM_ZONE, json);

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief load the replication application state from a file
////////////////////////////////////////////////////////////////////////////////

int TRI_LoadApplyStateReplication (TRI_vocbase_t* vocbase,
                                   TRI_replication_apply_state_t* state) {
  TRI_json_t* json;
  TRI_json_t* serverId;
  char* filename;
  char* error;
  int res;
   
  filename = GetApplyStateFilename(vocbase);

  if (! TRI_ExistsFile(filename)) {
    TRI_FreeString(TRI_CORE_MEM_ZONE, filename);

    return TRI_ERROR_FILE_NOT_FOUND;
  }

  error = NULL;
  json  = TRI_JsonFile(TRI_CORE_MEM_ZONE, filename, &error);

  if (json == NULL || json->_type != TRI_JSON_ARRAY) {
    if (error != NULL) {
      TRI_Free(TRI_CORE_MEM_ZONE, error);
    }

    return TRI_ERROR_REPLICATION_INVALID_APPLY_STATE;
  }

  res = TRI_ERROR_NO_ERROR;

  // read the server id
  serverId = TRI_LookupArrayJson(json, "serverId");

  if (! TRI_IsStringJson(serverId)) {
    res = TRI_ERROR_REPLICATION_INVALID_APPLY_STATE;
  }
  else {
    state->_serverId = TRI_UInt64String2(serverId->_value._string.data, serverId->_value._string.length - 1);
  }

  if (res == TRI_ERROR_NO_ERROR) {
    // read the ticks
    res |= ReadTick(json, "lastAppliedContinuousTick", &state->_lastAppliedContinuousTick); 
    res |= ReadTick(json, "lastAppliedInitialTick", &state->_lastAppliedInitialTick); 

    // set processed = applied
    state->_lastProcessedContinuousTick = state->_lastAppliedContinuousTick;
  }

  TRI_Free(TRI_CORE_MEM_ZONE, json);

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
