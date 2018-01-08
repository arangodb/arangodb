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

#include "ActionRegistry.h"

#include "Cluster/ActionRegistry.h"
#include "Cluster/Action.h"
#include "Basics/ReadLocker.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/WriteLocker.h"

using namespace arangodb::maintenance;

/// @brief let us begin
ActionRegistry* ActionRegistry::_instance = nullptr;

/// @brief consturuct
ActionRegistry::ActionRegistry() {}

/// @brief clean up
ActionRegistry::~ActionRegistry() {}

/// @brief public access to registry
ActionRegistry* ActionRegistry::instance() {
  if (_instance == nullptr) {
    _instance = new ActionRegistry();
  }
  return _instance;
}

/// @brief proposal entry for dispatching new actions through registry
arangodb::Result ActionRegistry::dispatch (ActionDescription const& desc) {
  arangodb::Result result;
  {
    WRITE_LOCKER(guard, _registryLock);
    if (_registry.find(desc) == _registry.end()) {
      _registry.emplace(desc,std::make_shared<Action>(desc));
    } else {
      result.reset(TRI_ERROR_ACTION_ALREADY_REGISTERED);
    }
  }
  return result;
}

/// @brief get action through its description
std::shared_ptr<Action> ActionRegistry::get (ActionDescription const& desc) {
  READ_LOCKER(guard, _registryLock);
  return (_registry.find(desc) != _registry.end()) ?
    _registry.at(desc) : nullptr;
}

/// @brief size of the registry, i.e. number of active jobs
std::size_t ActionRegistry::size () const {
  READ_LOCKER(guard, _registryLock);
  return _registry.size();
}


/// @brief print to ostream
VPackBuilder ActionRegistry::toVelocyPack() const {
  VPackBuilder b;
  { VPackArrayBuilder bb(&b);
    for (auto const i : _registry) {
      b.add(i.first.toVelocyPack().slice());
    }}
  return b;
}

