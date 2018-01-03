////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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

#include "Action.h"

#include "Cluster/ActionBase.h"
#include "Cluster/CreateDatabase.h"
#include "Cluster/DropDatabase.h"

#include "Logger/Logger.h"

using namespace arangodb::maintenance;

Action::Action(ActionDescription const& d) : _action(nullptr) {
  TRI_ASSERT(!d.has("name"));
  std::string name = d.name();
  if (name == "CreateDatabase") {
    _action = new CreateDatabase(d);
  } else if (name == "DropDatabase") {
    _action = new DropDatabase(d);
  } else {
    // Should never get here
    LOG_TOPIC(ERR, Logger::CLUSTER) << "Unknown maintenance action" << name;
    TRI_ASSERT(false);
    
  }
}

ActionDescription Action::describe() const {
  TRI_ASSERT(_action != nullptr);
  return _action->describe();
}

arangodb::Result Action::run(
  std::chrono::duration<double> const& d, bool& f) {
  TRI_ASSERT(_action != nullptr);
  return _action->run(d, f);
}
  
arangodb::Result Action::kill(Signal const& signal) {
  TRI_ASSERT(_action != nullptr);
  return _action->kill(signal);
}
  
arangodb::Result Action::progress(double& p) {
  TRI_ASSERT(_action != nullptr);
  return _action->progress(p);
}

Action::~Action() {
  if(_action != nullptr)
    delete _action;
}


