////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Julia Volmer
////////////////////////////////////////////////////////////////////////////////

#include "Pregel/Status/Status.h"
#include "Pregel/Utils.h"
#include <cstdint>
#include <string>

namespace arangodb::pregel {

struct StatusMessage {
  std::string senderId;
  uint64_t executionNumer;
  Status status;
};

template<typename Inspector>
auto inspect(Inspector& f, StatusMessage& x) {
  return f.object(x).fields(
      f.field(Utils::senderKey, x.senderId),
      f.field(Utils::executionNumberKey, x.executionNumer),
      f.field("status", x.status));
}

}  // namespace arangodb::pregel
