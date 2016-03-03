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

#ifndef ARANGOD_WAL_SYNC_REGION_H
#define ARANGOD_WAL_SYNC_REGION_H 1

#include "Basics/Common.h"
#include "Wal/Logfile.h"

namespace arangodb {
namespace wal {

struct SyncRegion {
  SyncRegion()
      : logfileId(0),
        logfile(nullptr),
        mem(nullptr),
        size(0),
        logfileStatus(Logfile::StatusType::UNKNOWN),
        firstSlotIndex(0),
        lastSlotIndex(0),
        waitForSync(false),
        checkMore(false),
        canSeal(false) {}

  ~SyncRegion() {}

  Logfile::IdType logfileId;
  Logfile* logfile;
  char* mem;
  uint32_t size;
  Logfile::StatusType logfileStatus;
  size_t firstSlotIndex;
  size_t lastSlotIndex;
  bool waitForSync;
  bool checkMore;
  bool canSeal;
};
}
}

#endif
