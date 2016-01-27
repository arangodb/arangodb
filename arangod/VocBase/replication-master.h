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

#ifndef ARANGOD_VOC_BASE_REPLICATION_MASTER_H
#define ARANGOD_VOC_BASE_REPLICATION_MASTER_H 1

#include "Basics/Common.h"

#include "VocBase/replication-common.h"

////////////////////////////////////////////////////////////////////////////////
/// @brief state information about replication master
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_replication_master_info_s {
  char* _endpoint;
  TRI_server_id_t _serverId;
  int _majorVersion;
  int _minorVersion;
  TRI_voc_tick_t _lastLogTick;
  bool _active;
} TRI_replication_master_info_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief initialize a master info struct
////////////////////////////////////////////////////////////////////////////////

void TRI_InitMasterInfoReplication(TRI_replication_master_info_t*, char const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy a master info struct
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyMasterInfoReplication(TRI_replication_master_info_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief log information about the master state
////////////////////////////////////////////////////////////////////////////////

void TRI_LogMasterInfoReplication(TRI_replication_master_info_t const*,
                                  char const*);

#endif
