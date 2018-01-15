////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2017 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_CLUSTER_MAINTENANCE_REST_HANDLER
#define ARANGOD_CLUSTER_MAINTENANCE_REST_HANDLER 1

#include "RestHandler/RestBaseHandler.h"

namespace arangodb {

////////////////////////////////////////////////////////////////////////////////
/// @brief Object that directs processing of one user maintenance requests
////////////////////////////////////////////////////////////////////////////////

class MaintenanceRestHandler : public RestBaseHandler {
 public:
  MaintenanceRestHandler(GeneralRequest*, GeneralResponse*);

 public:
  char const* name() const override { return "MaintenanceRestHandler"; }

  bool isDirect() const override;

  /// @brief Performs routing of request to appropriate subroutines
  RestStatus execute() override;

 protected:
  /// @brief PUT method adds an Action to the worklist (or executes action directly)
  void putAction();

};
}

#endif
