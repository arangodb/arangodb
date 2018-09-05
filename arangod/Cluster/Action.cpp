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

#include "Action.h"

#include "Cluster/CreateCollection.h"
#include "Cluster/CreateDatabase.h"
#include "Cluster/DropCollection.h"
#include "Cluster/DropDatabase.h"
#include "Cluster/DropIndex.h"
#include "Cluster/EnsureIndex.h"
#include "Cluster/MaintenanceStrings.h"
#include "Cluster/NonAction.h"
#include "Cluster/ResignShardLeadership.h"
#include "Cluster/SynchronizeShard.h"
#include "Cluster/UpdateCollection.h"

#include "Logger/Logger.h"

using namespace arangodb;
using namespace arangodb::maintenance;

Action::Action(
  MaintenanceFeature& feature,
  ActionDescription const& description) : _action(nullptr) {
  TRI_ASSERT(description.has(NAME));
  create(feature, description);
}

Action::Action(
  MaintenanceFeature& feature,
  ActionDescription&& description) : _action(nullptr) {
  TRI_ASSERT(description.has(NAME));
  create(feature, std::move(description));
}

Action::Action(
  MaintenanceFeature& feature,
  std::shared_ptr<ActionDescription> const &description)
  : _action(nullptr) {
  TRI_ASSERT(description->has(NAME));
  create(feature, *description);
}

Action::Action(std::unique_ptr<ActionBase> action)
  : _action(std::move(action)) {}

Action::~Action() {}

void Action::create(
  MaintenanceFeature& feature, ActionDescription const& description) {
  std::string name = description.name();
  if (name == CREATE_COLLECTION) {
    _action.reset(new CreateCollection(feature, description));
  } else if (name == CREATE_DATABASE) {
    _action.reset(new CreateDatabase(feature, description));
  } else if (name == DROP_COLLECTION) {
    _action.reset(new DropCollection(feature, description));
  } else if (name == DROP_DATABASE) {
    _action.reset(new DropDatabase(feature, description));
  } else if (name == DROP_INDEX) {
    _action.reset(new DropIndex(feature, description));
  } else if (name == ENSURE_INDEX) {
    _action.reset(new EnsureIndex(feature, description));
  } else if (name == RESIGN_SHARD_LEADERSHIP) {
    _action.reset(new ResignShardLeadership(feature, description));
  } else if (name == SYNCHRONIZE_SHARD) {
    _action.reset(new SynchronizeShard(feature, description));
  } else if (name == UPDATE_COLLECTION) {
    _action.reset(new UpdateCollection(feature, description));
  } else {
    _action.reset(new NonAction(feature, description));
  }
}

ActionDescription const& Action::describe() const {
  TRI_ASSERT(_action != nullptr);
  return _action->describe();
}

arangodb::MaintenanceFeature& Action::feature() const {
  TRI_ASSERT(_action != nullptr);
  return _action->feature();
}

std::shared_ptr<VPackBuilder> const Action::properties() const {
  return describe().properties();
}

bool Action::first() {
  TRI_ASSERT(_action != nullptr);
  return _action->first();
}

bool Action::next() {
  TRI_ASSERT(_action != nullptr);
  return _action->next();
}

bool Action::matches(std::unordered_set<std::string> const& labels) const {
  TRI_ASSERT(_action != nullptr);
  return _action->matches(labels);
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
  _action->endStats();
}

void Action::toVelocyPack(arangodb::velocypack::Builder& builder) const {
  TRI_ASSERT(_action != nullptr);
  _action->toVelocyPack(builder);
}

namespace std {
ostream& operator<< (
  ostream& out, arangodb::maintenance::Action const& d) {
  out << d.toVelocyPack().toJson();
  return out;
}}
