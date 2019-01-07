////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Kaveh Vahedipour
/// @author Matthew Von-Maszewski
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_MAINTENANCE_NON_ACTION_H
#define ARANGODB_MAINTENANCE_NON_ACTION_H

#include "ActionBase.h"
#include "ActionDescription.h"

#include <chrono>

namespace arangodb {
namespace maintenance {

/**
 * @brief  Dummy action for failure to find action
 */
class NonAction : public ActionBase {
 public:
  NonAction(MaintenanceFeature&, ActionDescription const& d);

  virtual ~NonAction();

  virtual bool first() override final;
};

}  // namespace maintenance
}  // namespace arangodb

#endif
