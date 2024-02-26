////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Andreas Streichardt
////////////////////////////////////////////////////////////////////////////////

#include "ClusterHelpers.h"

#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>

using namespace arangodb;

bool ClusterHelpers::compareServerLists(VPackSlice plan, VPackSlice current) {
  if (!plan.isArray() || !current.isArray()) {
    return false;
  }
  std::vector<std::string> p;
  for (VPackSlice srv : VPackArrayIterator(plan)) {
    if (srv.isString()) {
      p.emplace_back(srv.copyString());
    }
  }
  std::vector<std::string> c;
  for (VPackSlice srv : VPackArrayIterator(current)) {
    if (srv.isString()) {
      c.emplace_back(srv.copyString());
    }
  }
  return compareServerLists(std::move(p), std::move(c));
}

bool ClusterHelpers::compareServerLists(std::vector<std::string> planned,
                                        std::vector<std::string> current) {
  bool equalLeader = !planned.empty() && !current.empty() &&
                     planned.front() == current.front();
  if (!equalLeader) {
    return false;
  }
  std::sort(planned.begin(), planned.end());
  std::sort(current.begin(), current.end());
  return current == planned;
}

bool ClusterHelpers::isCoordinatorName(ServerID const& serverId) {
  return serverId.starts_with("CRDN-");
}

bool ClusterHelpers::isDBServerName(ServerID const& serverId) {
  return serverId.starts_with("PRMR-");
}
