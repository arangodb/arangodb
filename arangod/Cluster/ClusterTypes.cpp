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

#include "ClusterTypes.h"
#include "Basics/debugging.h"
#include "Basics/StaticStrings.h"
#include "velocypack/Builder.h"
#include "velocypack/velocypack-aliases.h"

std::ostream& operator<< (std::ostream& o, arangodb::RebootId const& r) {
  return r.print(o);
}

namespace arangodb {

arangodb::velocypack::Builder AnalyzersRevision::toVelocyPack() const {
  arangodb::velocypack::Builder builder;
  builder.openObject();
  builder.add(StaticStrings::AnalyzersRevision, VPackValue(_revision));
  builder.add(StaticStrings::AnalyzersBuildingRevision, VPackValue(_buildingRevision));
  TRI_ASSERT((_serverID.empty() && !_rebootID.initialized()) ||
             (!_serverID.empty() && _rebootID.initialized()));

  if (!_serverID.empty()) {
    builder.add(StaticStrings::AttrCoordinator, VPackValue(_serverID));
  }
  if (_rebootID.initialized()) {
    builder.add(StaticStrings::AttrCoordinatorRebootId, VPackValue(_rebootID.value()));
  }
  builder.close();

  return builder;
}
}  // namespace arangodb
