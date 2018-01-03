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
#include "Basics/WriteLocker.h"

using namespace arangodb::maintenance;

/// @brief let us begin
ActionRegistry* ActionRegistry::_instance = nullptr;

/// @brief consturuct
ActionRegistry::ActionRegistry() {}

/// @brief clean up
ActionRegistry::~ActionRegistry() {}

/// @brief public access to registry
ActionRegistry* ActionRegistry::Instance() {
  if (_instance == nullptr) {
    _instance = new ActionRegistry();
  }
  return _instance;
}

/// @brief proposal entry for dispatching new actions through registry
arangodb::Result ActionRegistry::dispatch (
  ActionDescription const& desc, std::shared_ptr<Action>& ptr) {
  
  arangodb::Result result;
  
  {
    WRITE_LOCKER(guard, _registryLock);
    
    if (_registry.find(desc) == _registry.end()) {
      ptr = std::make_shared<Action>(desc);
      _registry.emplace(desc, ptr);
    }
  }
  
  return result;
  
}

/// @brief get 
std::shared_ptr<Action> ActionRegistry::get (ActionDescription const& desc) {
  READ_LOCKER(guard, _registryLock);
  return _registry.at(desc);
}
