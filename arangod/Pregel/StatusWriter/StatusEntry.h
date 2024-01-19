////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "Pregel/DatabaseTypes.h"
#include "Pregel/ExecutionNumber.h"
#include "Pregel/Utils.h"

namespace arangodb::pregel {

struct PregelCollectionEntry {
  ServerID serverId;
  ExecutionNumber executionNumber;
};

template<typename Inspector>
auto inspect(Inspector& f, PregelCollectionEntry& x) {
  return f.object(x).fields(
      f.field("serverId", x.serverId),
      f.field(Utils::executionNumberKey, x.executionNumber));
}

}  // namespace arangodb::pregel
