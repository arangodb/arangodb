////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "replication-master.h"

#include "Basics/Logger.h"
#include "Basics/tri-strings.h"

////////////////////////////////////////////////////////////////////////////////
/// @brief initialize a master info struct
////////////////////////////////////////////////////////////////////////////////

void TRI_InitMasterInfoReplication(TRI_replication_master_info_t* info,
                                   char const* endpoint) {
  TRI_ASSERT(endpoint != nullptr);

  info->_endpoint = TRI_DuplicateString(TRI_CORE_MEM_ZONE, endpoint);
  info->_serverId = 0;
  info->_majorVersion = 0;
  info->_minorVersion = 0;
  info->_lastLogTick = 0;
  info->_active = false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy a master info struct
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyMasterInfoReplication(TRI_replication_master_info_t* info) {
  if (info->_endpoint != nullptr) {
    TRI_FreeString(TRI_CORE_MEM_ZONE, info->_endpoint);
    info->_endpoint = nullptr;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief log information about the master state
////////////////////////////////////////////////////////////////////////////////

void TRI_LogMasterInfoReplication(TRI_replication_master_info_t const* info,
                                  char const* prefix) {
  TRI_ASSERT(info->_endpoint != nullptr);

  LOG(INFO) << "" << prefix << " master at " << info->_endpoint << ", id " << info->_serverId << ", version " << info->_majorVersion << "." << info->_minorVersion << ", last log tick " << info->_lastLogTick;
}
