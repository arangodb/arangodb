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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Basics/StaticStrings.h"
#include "Cluster/ClusterTypes.h"
#include "VocBase/Properties/UtilityInvariants.h"

#include <string>

namespace arangodb {
struct AgencyIsBuildingFlags {
  bool isBuilding = true;
  std::string coordinatorName{StaticStrings::Empty};
  RebootId rebootId{0};
};

template<class Inspector>
auto inspect(Inspector& f, AgencyIsBuildingFlags& props) {
  return f.object(props).fields(
      f.field("isBuilding", props.isBuilding).fallback(f.keep()),
      f.field("coordinator", props.coordinatorName)
          .fallback(f.keep())
          .invariant(UtilityInvariants::isNonEmpty),
      f.field("coordinatorRebootId", props.rebootId).fallback(f.keep()));
}

}  // namespace arangodb
