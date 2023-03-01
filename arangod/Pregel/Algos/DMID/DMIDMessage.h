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
#include "Pregel/GraphStore/VertexID.h"

namespace arangodb::pregel {

struct DMIDMessage {
  DMIDMessage() {}
  DMIDMessage(VertexID const& pid, float val) : senderId(pid), weight(val) {}

  DMIDMessage(VertexID const& sender, VertexID const& leader)
      : senderId(sender), leaderId(leader) {}

  VertexID senderId;
  VertexID leaderId;
  float weight = 0;
};

template<typename Inspector>
auto inspect(Inspector& f, DMIDMessage& v) {
  return f.object(v).fields(f.field("senderId", v.senderId),
                            f.field("leaderId", v.leaderId),
                            f.field("weight", v.weight));
}

}  // namespace arangodb::pregel
