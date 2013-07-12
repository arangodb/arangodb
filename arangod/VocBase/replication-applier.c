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
/// @brief stringify an apply phase name
////////////////////////////////////////////////////////////////////////////////

static const char* StringifyPhase (TRI_replication_apply_phase_e phase) {
  switch (phase) {
    case PHASE_NONE:
      return "stopped";
    case PHASE_INIT:
      return "initial dump - validating";
    case PHASE_DROP:
      return "initial dump - dropping collections";
    case PHASE_CREATE:
      return "initial dump - creating collections";
    case PHASE_DUMP:
      return "initial dump - dumping data";
    case PHASE_FOLLOW:
      return "continuous dump";
  }

  assert(false);
  return NULL;
}

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

  assert(state->_endpoint != NULL);

  json = TRI_CreateArray2Json(TRI_CORE_MEM_ZONE, 5);

  if (json == NULL) {
    return NULL;
  }

  lastProcessedContinuousTick = TRI_StringUInt64(state->_lastProcessedContinuousTick);
  lastAppliedContinuousTick   = TRI_StringUInt64(state->_lastAppliedContinuousTick);
  lastAppliedInitialTick      = TRI_StringUInt64(state->_lastAppliedInitialTick);
  serverId                    = TRI_StringUInt64(state->_serverId);
 
  TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, 
                       json, 
                       "endpoint", 
                       TRI_CreateStringCopyJson(TRI_CORE_MEM_ZONE, state->_endpoint));

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
/// @brief register an applier error, without locking
////////////////////////////////////////////////////////////////////////////////

static int SetError (TRI_replication_applier_t* applier,
                     int errorCode,
                     char const* msg) {
  TRI_replication_apply_state_t* state;
  char const* realMsg;

  if (msg == NULL) {
    realMsg = TRI_errno_string(errorCode);
  }
  else {
    realMsg = msg;
  }

  // log error message
  if (errorCode != TRI_ERROR_REPLICATION_NO_RESPONSE) {
    LOG_WARNING("replication error: %s", realMsg);
  }

  state = &applier->_state;
  state->_lastError._code = errorCode;

  if (state->_lastError._msg != NULL) {
    TRI_FreeString(TRI_CORE_MEM_ZONE, state->_lastError._msg);
  }

  state->_lastError._msg = TRI_DuplicateStringZ(TRI_CORE_MEM_ZONE, realMsg);

  return errorCode;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief applier main loop
////////////////////////////////////////////////////////////////////////////////

void ApplyLoop (void* data) {
  TRI_replication_applier_t* applier = (TRI_replication_applier_t*) data;

  while (1) {
    bool isActive;

    TRI_LockSpin(&applier->_threadLock);

    if (applier->_terminateThread) {
      TRI_UnlockSpin(&applier->_threadLock);

      return;
    }
      
    TRI_UnlockSpin(&applier->_threadLock);

    TRI_ReadLockReadWriteLock(&applier->_statusLock);
    isActive = applier->_state._active;
    TRI_ReadUnlockReadWriteLock(&applier->_statusLock);

    if (isActive) {
      int res;

      res = TRI_RunFetcherReplication(applier->_fetcher, false, 0);

      if (res != TRI_ERROR_NO_ERROR) {
        return;
      }
    }

    sleep(2);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief start the replication applier
/// note: must hold the lock when calling this
////////////////////////////////////////////////////////////////////////////////

static int StartApplier (TRI_replication_applier_t* applier) {
  TRI_replication_apply_state_t* state;

  state = &applier->_state;

  if (state->_active) {
    return TRI_ERROR_INTERNAL;
  }

  if (state->_endpoint == NULL) {
    return SetError(applier, TRI_ERROR_REPLICATION_INVALID_APPLY_STATE, NULL);
  }

  applier->_fetcher = (void*) TRI_CreateFetcherReplication(applier->_vocbase, state->_endpoint, 600.0);

  state->_active = true;
  
  LOG_INFO("started replication applier for database '%s'",
           applier->_databaseName);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stop the replication applier
/// note: must hold the lock when calling this
////////////////////////////////////////////////////////////////////////////////

static int StopApplier (TRI_replication_applier_t* applier) {
  TRI_replication_apply_state_t* state;

  state = &applier->_state;

  if (! state->_active) {
    return TRI_ERROR_INTERNAL;
  }

  state->_active = false;
  state->_phase  = PHASE_NONE;

  if (state->_progress != NULL) {
    TRI_FreeString(TRI_CORE_MEM_ZONE, state->_progress);
  }

  state->_progress = TRI_DuplicateStringZ(TRI_CORE_MEM_ZONE, "applier stopped");
  
  if (state->_lastError._msg != NULL) {
    TRI_FreeString(TRI_CORE_MEM_ZONE, state->_lastError._msg);
    state->_lastError._msg = NULL;
  }

  state->_lastError._code = TRI_ERROR_NO_ERROR;

  LOG_INFO("stopped replication applier for database '%s'",
           applier->_databaseName);

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
/// @brief create a replication applier
////////////////////////////////////////////////////////////////////////////////

TRI_replication_applier_t* TRI_CreateReplicationApplier (TRI_vocbase_t* vocbase) {
  TRI_replication_applier_t* applier;
  int res;

  applier = TRI_Allocate(TRI_CORE_MEM_ZONE, sizeof(TRI_replication_applier_t), false);

  if (applier == NULL) {
    return NULL;
  }
  
  res = TRI_LoadStateFileReplicationApplier(vocbase, &applier->_state);

  if (res != TRI_ERROR_NO_ERROR && 
      res != TRI_ERROR_FILE_NOT_FOUND) {
    TRI_set_errno(res);
    TRI_DestroyApplyStateReplicationApplier(&applier->_state);
    TRI_Free(TRI_CORE_MEM_ZONE, applier);

    return NULL;
  }
  
  TRI_InitReadWriteLock(&applier->_statusLock);
  TRI_InitSpin(&applier->_threadLock);

  applier->_terminateThread = false;
  applier->_vocbase         = vocbase;
  applier->_databaseName    = TRI_DuplicateStringZ(TRI_CORE_MEM_ZONE, vocbase->_name);

  assert(applier->_databaseName != NULL);
  
  TRI_InitThread(&applier->_thread);

  if (! TRI_StartThread(&applier->_thread, "[applier]", ApplyLoop, applier)) {
    return NULL;
  }
  
  return applier;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy a replication applier
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyReplicationApplier (TRI_replication_applier_t* applier) {
  TRI_StopReplicationApplier(applier);
  
  TRI_LockSpin(&applier->_threadLock);
  applier->_terminateThread = true;
  TRI_UnlockSpin(&applier->_threadLock);

  TRI_JoinThread(&applier->_thread);

  TRI_DestroyApplyStateReplicationApplier(&applier->_state);
  TRI_FreeString(TRI_CORE_MEM_ZONE, applier->_databaseName);
  TRI_DestroySpin(&applier->_threadLock);
  TRI_DestroyReadWriteLock(&applier->_statusLock);
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
  
  TRI_WriteLockReadWriteLock(&applier->_statusLock);

  if (! applier->_state._active) {
    res = StartApplier(applier);
  }
  
  TRI_WriteUnlockReadWriteLock(&applier->_statusLock);

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stop the replication applier
////////////////////////////////////////////////////////////////////////////////

int TRI_StopReplicationApplier (TRI_replication_applier_t* applier) {
  int res;

  res = TRI_ERROR_NO_ERROR;
 
  TRI_WriteLockReadWriteLock(&applier->_statusLock);

  if (applier->_state._active) {
    res = StopApplier(applier);
  }

  TRI_WriteUnlockReadWriteLock(&applier->_statusLock);
  
  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief configure the replication applier
////////////////////////////////////////////////////////////////////////////////

int TRI_ConfigureReplicationApplier (TRI_replication_applier_t* applier,
                                     char const* endpoint,
                                     double timeout,
                                     bool fullSync,
                                     uint64_t ignoreCount) {
  int res;

  res = TRI_ERROR_NO_ERROR;

  TRI_WriteLockReadWriteLock(&applier->_statusLock);

  if (applier->_state._active) {
    res = StopApplier(applier);
  }

  if (res == TRI_ERROR_NO_ERROR) {
    if (applier->_state._endpoint != NULL) {
      TRI_FreeString(TRI_CORE_MEM_ZONE, applier->_state._endpoint);
    }

    if (fullSync) {
      // re-init
      TRI_InitApplyStateReplicationApplier(&applier->_state);
    }

    applier->_state._endpoint = TRI_DuplicateStringZ(TRI_CORE_MEM_ZONE, endpoint);
      
    TRI_SaveStateFileReplicationApplier(applier->_vocbase, &applier->_state, true);

    res = StartApplier(applier);
  }

  TRI_WriteUnlockReadWriteLock(&applier->_statusLock);

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the current replication apply state
////////////////////////////////////////////////////////////////////////////////

int TRI_StateReplicationApplier (TRI_replication_applier_t* applier,
                                 TRI_replication_apply_state_t* state) {
  TRI_InitApplyStateReplicationApplier(state);

  TRI_ReadLockReadWriteLock(&applier->_statusLock);
  
  state->_active                    = applier->_state._active;
  state->_lastAppliedContinuousTick = applier->_state._lastAppliedContinuousTick;
  state->_lastAppliedInitialTick    = applier->_state._lastAppliedInitialTick;
  state->_serverId                  = applier->_state._serverId;
  state->_phase                     = applier->_state._phase;
  state->_lastError._code           = applier->_state._lastError._code;

  if (applier->_state._endpoint != NULL) {
    state->_endpoint = TRI_DuplicateStringZ(TRI_CORE_MEM_ZONE, applier->_state._endpoint);
  }
  
  if (applier->_state._progress != NULL) {
    state->_progress = TRI_DuplicateStringZ(TRI_CORE_MEM_ZONE, applier->_state._progress);
  }
  
  if (applier->_state._lastError._msg != NULL) {
    state->_lastError._msg          = TRI_DuplicateStringZ(TRI_CORE_MEM_ZONE, applier->_state._lastError._msg);
  }

  TRI_ReadUnlockReadWriteLock(&applier->_statusLock);

  return TRI_ERROR_NO_ERROR;
} 

////////////////////////////////////////////////////////////////////////////////
/// @brief get a JSON representation of an applier state
////////////////////////////////////////////////////////////////////////////////
  
TRI_json_t* TRI_JsonStateReplicationApplier (TRI_replication_apply_state_t const* state) {
  TRI_json_t* json;
  TRI_json_t* error;
  char* lastString;
  
  json = TRI_CreateArray2Json(TRI_CORE_MEM_ZONE, 2);

  // add replication state
  TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, json, "running", TRI_CreateBooleanJson(TRI_CORE_MEM_ZONE, state->_active));
  
  if (state->_endpoint != NULL) {
    TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, json, "endpoint", TRI_CreateStringCopyJson(TRI_CORE_MEM_ZONE, state->_endpoint));
  }
  
  lastString = TRI_StringUInt64(state->_lastAppliedContinuousTick);
  TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, json, "lastAppliedContinuousTick", TRI_CreateStringJson(TRI_CORE_MEM_ZONE, lastString));

  lastString = TRI_StringUInt64(state->_lastAppliedInitialTick);
  TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, json, "lastAppliedInitialTick", TRI_CreateStringJson(TRI_CORE_MEM_ZONE, lastString));
  
  TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, json, "phase", TRI_CreateStringCopyJson(TRI_CORE_MEM_ZONE, StringifyPhase(state->_phase)));

  if (state->_progress != NULL) {
    TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, json, "progress", TRI_CreateStringCopyJson(TRI_CORE_MEM_ZONE, state->_progress));
  }

  error = TRI_CreateArrayJson(TRI_CORE_MEM_ZONE);
  TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, error, "errorNum", TRI_CreateNumberJson(TRI_CORE_MEM_ZONE, (double) state->_lastError._code));

  if (state->_lastError._msg != NULL) {
    TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, error, "errorMessage", TRI_CreateStringCopyJson(TRI_CORE_MEM_ZONE, state->_lastError._msg));
  }

  TRI_Insert3ArrayJson(TRI_CORE_MEM_ZONE, json, "lastError", error);
  
  return json;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief register an applier error
////////////////////////////////////////////////////////////////////////////////

int TRI_SetErrorReplicationApplier (TRI_replication_applier_t* applier,
                                    int errorCode,
                                    char const* msg) {
  TRI_WriteLockReadWriteLock(&applier->_statusLock);

  SetError(applier, errorCode, msg);

  TRI_WriteUnlockReadWriteLock(&applier->_statusLock);

  return errorCode;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set the current phase
////////////////////////////////////////////////////////////////////////////////

void TRI_SetPhaseReplicationApplier (TRI_replication_applier_t* applier,
                                     TRI_replication_apply_phase_e phase) {
  TRI_WriteLockReadWriteLock(&applier->_statusLock);

  applier->_state._phase = phase;

  TRI_WriteUnlockReadWriteLock(&applier->_statusLock);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set the progress
////////////////////////////////////////////////////////////////////////////////

void TRI_SetProgressReplicationApplier (TRI_replication_applier_t* applier,
                                        char const* msg) {
  char* copy = TRI_DuplicateStringZ(TRI_CORE_MEM_ZONE, msg);

  if (copy == NULL) {
    return;
  }

  TRI_WriteLockReadWriteLock(&applier->_statusLock);

  if (applier->_state._progress != NULL) {
    TRI_FreeString(TRI_CORE_MEM_ZONE, applier->_state._progress);
  }
  applier->_state._progress = copy;

  TRI_WriteUnlockReadWriteLock(&applier->_statusLock);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initialise an apply state struct
////////////////////////////////////////////////////////////////////////////////

void TRI_InitApplyStateReplicationApplier (TRI_replication_apply_state_t* state) {
  memset(state, 0, sizeof(TRI_replication_apply_state_t));

  state->_active          = false;
  state->_lastError._code = TRI_ERROR_NO_ERROR;
  state->_phase           = PHASE_NONE;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy an apply state struct
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyApplyStateReplicationApplier (TRI_replication_apply_state_t* state) {
  if (state->_progress != NULL) {
    TRI_FreeString(TRI_CORE_MEM_ZONE, state->_progress);
  }

  if (state->_lastError._msg != NULL) {
    TRI_FreeString(TRI_CORE_MEM_ZONE, state->_lastError._msg);
  }

  if (state->_endpoint != NULL) {
    TRI_FreeString(TRI_CORE_MEM_ZONE, state->_endpoint);
    state->_endpoint = NULL;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief remove the replication application state file
////////////////////////////////////////////////////////////////////////////////

int TRI_RemoveStateFileReplicationApplier (TRI_vocbase_t* vocbase) {
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

int TRI_SaveStateFileReplicationApplier (TRI_vocbase_t* vocbase,
                                         TRI_replication_apply_state_t const* state,
                                         bool sync) {
  TRI_json_t* json;
  char* filename;
  int res;

  if (state->_endpoint == NULL) {
    return TRI_ERROR_REPLICATION_INVALID_APPLY_STATE;
  }

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

int TRI_LoadStateFileReplicationApplier (TRI_vocbase_t* vocbase,
                                         TRI_replication_apply_state_t* state) {
  TRI_json_t* json;
  TRI_json_t* serverId;
  TRI_json_t* endpoint;
  char* filename;
  char* error;
  int res;
   
  TRI_InitApplyStateReplicationApplier(state);
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
    state->_serverId = TRI_UInt64String2(serverId->_value._string.data, 
                                         serverId->_value._string.length - 1);
  }

  endpoint = TRI_LookupArrayJson(json, "endpoint");

  if (! TRI_IsStringJson(endpoint)) {
    res = TRI_ERROR_REPLICATION_INVALID_APPLY_STATE;
  }
  else {
    state->_endpoint = TRI_DuplicateString2Z(TRI_CORE_MEM_ZONE, 
                                             endpoint->_value._string.data, 
                                             endpoint->_value._string.length - 1);
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
/// @brief initialise an apply configuration
////////////////////////////////////////////////////////////////////////////////

void TRI_InitApplyConfigurationReplicationApplier (TRI_replication_apply_configuration_t* config,
                                                   char* endpoint,
                                                   double timeout,
                                                   uint64_t ignoreErrors,
                                                   int maxConnectRetries) {
  assert(endpoint != NULL);

  config->_endpoint          = TRI_DuplicateStringZ(TRI_CORE_MEM_ZONE, endpoint);
  config->_timeout           = timeout;
  config->_ignoreErrors      = ignoreErrors;
  config->_maxConnectRetries = maxConnectRetries;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy an apply configuration
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyApplyConfigurationReplicationApplier (TRI_replication_apply_configuration_t* config) {
  if (config->_endpoint != NULL) {
    TRI_FreeString(TRI_CORE_MEM_ZONE, config->_endpoint);
    config->_endpoint = NULL;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
