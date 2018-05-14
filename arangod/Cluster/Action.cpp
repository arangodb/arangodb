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

#include "Cluster/CreateDatabase.h"
#include "Cluster/DropDatabase.h"
#include "Cluster/UpdateCollection.h"

#include "Logger/Logger.h"

using namespace arangodb::maintenance;

Action::Action(
  MaintenanceFeature& feature, ActionDescription const& d)
  : _action(nullptr) {
  TRI_ASSERT(d.has("name"));
  std::string name = d.name();
  if (name == "CreateDatabase") {
    _action.reset(new CreateDatabase(feature, d));
  } else if (name == "DropDatabase") {
    _action.reset(new DropDatabase(feature, d));
  } else if (name == "UpdateCollection") {
    _action.reset(new UpdateCollection(feature, d));
  } else {
    // We should never get here
    LOG_TOPIC(ERR, Logger::MAINTENANCE) << "Unknown maintenance action" << name;
    TRI_ASSERT(false);
  }
}

Action::Action(
  MaintenanceFeature& feature,
  std::shared_ptr<ActionDescription> const desc)
  : _action(nullptr) {
  auto const& d = *desc;
  TRI_ASSERT(d.has("name"));
  std::string name = d.name();
  if (name == "CreateDatabase") {
    _action.reset(new CreateDatabase(feature, d));
  } else if (name == "DropDatabase") {
    _action.reset(new DropDatabase(feature, d));
  } else if (name == "UpdateCollection") {
    _action.reset(new UpdateCollection(feature, d));
  } else {
    // We should never get here
    LOG_TOPIC(ERR, Logger::MAINTENANCE) << "Unknown maintenance action" << name;
    TRI_ASSERT(false);
  }
}


Action::~Action() {}

ActionDescription const& Action::describe() const {
  TRI_ASSERT(_action != nullptr);
  return _action->describe();
}

std::shared_ptr<VPackBuilder> const Action::properties() const {
  return describe().properties();
}

arangodb::Result Action::first() {
  TRI_ASSERT(_action != nullptr);
  return _action->first();
}

arangodb::Result Action::next() {
  TRI_ASSERT(_action != nullptr);
  return _action->next();
}

arangodb::Result Action::result() {
  TRI_ASSERT(_action != nullptr);
  return _action->result();
}

arangodb::Result Action::kill(Signal const& signal) {
  TRI_ASSERT(_action != nullptr);
  return _action->kill(signal);
}

arangodb::Result Action::progress(double& p) {
  TRI_ASSERT(_action != nullptr);
  return _action->progress(p);
}

ActionState Action::getState() const {
  return _action->getState();
}

void Action::startStats() {
  _action->startStats();
}

void Action::incStats() {
  _action->incStats();
}

void Action::endStats() {
  _action->incStats();
}

arangodb::Result Action::run(
  std::chrono::duration<double> const& duration, bool& finished) {
  TRI_ASSERT(_action != nullptr);
  return _action->run(duration, finished);
}

void Action::toVelocyPack(arangodb::velocypack::Builder& builder) const {
  TRI_ASSERT(_action != nullptr);
  _action->toVelocyPack(builder);
}
