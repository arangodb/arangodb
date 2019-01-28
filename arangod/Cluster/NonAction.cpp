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

#include "NonAction.h"
#include "MaintenanceFeature.h"

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::maintenance;

NonAction::NonAction(MaintenanceFeature& feature, ActionDescription const& desc)
    : ActionBase(feature, desc) {
  std::string const error =
      std::string("Unknown maintenance action '") + desc.name() + "'";
  LOG_TOPIC(ERR, Logger::MAINTENANCE) << error;
  _result = arangodb::Result(TRI_ERROR_INTERNAL, error);
}

bool NonAction::first() {
  std::string const error =
      std::string("Unknown maintenance action '") + _description.name() + "'";
  LOG_TOPIC(ERR, Logger::MAINTENANCE) << error;
  _result = arangodb::Result(TRI_ERROR_INTERNAL, error);
  return false;
}

NonAction::~NonAction() {}
