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

#include "Basics/Exceptions.h"
#include "Basics/StringUtils.h"
#include "Rest/GeneralRequest.h"

#include <algorithm>

using namespace arangodb;

std::string SyncerId::toString() const { return std::to_string(value); }

SyncerId SyncerId::fromRequest(GeneralRequest const& request) {
  bool found;
  std::string const& idStr = request.value("syncerId", found);
  TRI_voc_tick_t id = 0;

  if (found) {
    if (idStr.empty()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER, "syncerId, if set, must not be empty");
    }
    if (!std::all_of(idStr.begin(), idStr.end(), [](char c) { 
      return c >= '0' && c <= '9';
    })) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER, "syncerId must be an integer");
    }
    if (idStr[0] == '0') {
      if (idStr.size() == 1) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER, "syncerId must be non-zero");
      } else {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER, "syncerId must not begin with zero");
      }
    }
    try {
      id = std::stoull(idStr, nullptr, 10);
      // stoull could also throw std::invalid_argument, but this shouldn't be
      // possible due to the checks before.
    } catch (std::out_of_range const& e) {
      THROW_ARANGO_EXCEPTION_FORMAT(TRI_ERROR_BAD_PARAMETER, "syncerId is too large: %s", e.what());
    }
    if (id == 0) {
      // id == 0 is reserved to mean "unset" and may not be set by the client.
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER, "syncerId must be non-zero");
    }
  }

  return SyncerId{id};
}
