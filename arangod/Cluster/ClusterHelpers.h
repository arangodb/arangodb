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
/// @author Andreas Streichardt
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_CLUSTER_CLUSTER_HELPERS_H
#define ARANGOD_CLUSTER_CLUSTER_HELPERS_H 1

#include "Basics/Common.h"
#include "Cluster/ClusterTypes.h"

#include <string>
#include <vector>

namespace arangodb {
namespace velocypack {
class Slice;
}

class ClusterHelpers {
 public:
  static bool compareServerLists(arangodb::velocypack::Slice plan,
                                 arangodb::velocypack::Slice current);

  // values are passed by value intentionally, as they will be sorted inside the
  // function
  static bool compareServerLists(std::vector<std::string>, std::vector<std::string>);
  
  /// @brief whether or not the passed in name is a coordinator server name, i.e. "CRDN-..."
  static bool isCoordinatorName(ServerID const& serverId);
     
  /// @brief whether or not the passed in name is a DB server name, i.e. "PRMR-..."
  static bool isDBServerName(ServerID const& serverId);
  
};

}  // namespace arangodb

#endif
