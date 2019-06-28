////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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

#include "SyncerId.h"

#include <lib/Basics/StringUtils.h>
#include <lib/Logger/Logger.h>
#include <lib/Rest/GeneralRequest.h>

using namespace arangodb;

std::string SyncerId::toString() const { return std::to_string(value); }

SyncerId SyncerId::fromRequest(GeneralRequest const& request) {
  std::string const& idStr = request.value("syncerId");
  TRI_voc_tick_t id = basics::StringUtils::uint64(idStr);
  return SyncerId{id};
}
