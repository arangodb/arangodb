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

#include "replication-master.h"

#include "BasicsC/logging.h"
#include "BasicsC/tri-strings.h"


#ifdef TRI_ENABLE_REPLICATION

// -----------------------------------------------------------------------------
// --SECTION--                                           REPLICATION MASTER INFO
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief initialise a master info struct
////////////////////////////////////////////////////////////////////////////////

void TRI_InitMasterInfoReplication (TRI_replication_master_info_t* info,
                                    const char* endpoint) {
  info->_endpoint            = TRI_DuplicateStringZ(TRI_CORE_MEM_ZONE, endpoint);
  info->_serverId            = 0;
  info->_majorVersion        = 0;
  info->_minorVersion        = 0;
  info->_state._lastLogTick  = 0;
  info->_state._active       = false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy a master info struct
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyMasterInfoReplication (TRI_replication_master_info_t* info) {
  if (info->_endpoint != NULL) {
    TRI_FreeString(TRI_CORE_MEM_ZONE, info->_endpoint);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief log information about the master state
////////////////////////////////////////////////////////////////////////////////
      
void TRI_LogMasterInfoReplication (TRI_replication_master_info_t const* info,
                                   const char* prefix) {
  LOG_INFO("%s master at %s, id %llu, version %d.%d, last log tick %llu", 
           prefix,
           info->_endpoint,
           (unsigned long long) info->_serverId,
           info->_majorVersion,
           info->_minorVersion,
           (unsigned long long) info->_state._lastLogTick);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
