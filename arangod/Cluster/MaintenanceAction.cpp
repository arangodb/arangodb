////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Kaveh Vahedipour
/// @author Matthew Von-Maszewski
////////////////////////////////////////////////////////////////////////////////

#include "MaintenanceAction.h"

#include "Cluster/ActionDescription.h"
#include "Cluster/MaintenanceFeature.h"

namespace arangodb {

namespace maintenance {


MaintenanceAction::MaintenanceAction(arangodb::MaintenanceFeature & feature,
                                     ActionDescription_t & description)
  : _feature(feature), _description(description), _state(READY) {

  _hash = ActionDescription::hash(description);
  _id = feature.nextActionId();
  return;

} // MaintenanceAction::MaintenanceAction


} // namespace maintenance

} // namespace arangodb
