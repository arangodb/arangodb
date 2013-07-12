////////////////////////////////////////////////////////////////////////////////
/// @brief replication master info
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

#ifndef TRIAGENS_VOC_BASE_REPLICATION_MASTER_H
#define TRIAGENS_VOC_BASE_REPLICATION_MASTER_H 1

#include "BasicsC/common.h"

#include "VocBase/replication-common.h"
#include "VocBase/replication-logger.h"
#include "VocBase/server-id.h"

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                           REPLICATION MASTER INFO
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
  char*                          _endpoint;
  TRI_server_id_t                _serverId;
  int                            _majorVersion;
  int                            _minorVersion;
  TRI_replication_log_state_t    _state;
}
TRI_replication_master_info_t;

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
