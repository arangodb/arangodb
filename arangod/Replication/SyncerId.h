////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_REPLICATION_SYNCERID_H
#define ARANGOD_REPLICATION_SYNCERID_H

#include "VocBase/voc-types.h"

namespace arangodb {

class GeneralRequest;

// Note that the value 0 is reserved and means unset.
struct SyncerId {
  TRI_voc_tick_t value;

  std::string toString() const;
  static SyncerId fromRequest(GeneralRequest const& request);

  inline bool operator==(SyncerId other) const noexcept {
    return value == other.value;
  }
};
}

#endif // ARANGOD_REPLICATION_SYNCERID_H
