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

#include "Cluster/ActionRegistry.h"
#include "Cluster/Maintenance.h"

#include <algorithm>
#include <iostream>

using namespace arangodb;
using namespace arangodb::consensus;
using namespace arangodb::maintenance;


/// @brief calculate difference between plan and local for for databases
arangodb::Result arangodb::maintenance::diffPlanLocalForDatabases(
  Node const& plan, std::vector<std::string> const& local,
  std::vector<std::string>& toCreate, std::vector<std::string>& toDrop) {

  arangodb::Result result;
  
  std::vector<std::string> planv, localv = local;
  for (auto const i : plan.children()) {
    planv.emplace_back(i.first);
  }
  std::sort(planv.begin(), planv.end());
  std::sort(localv.begin(), localv.end());
    
  std::vector<std::string> isect;
  std::set_intersection(
    planv.begin(), planv.end(), localv.begin(), localv.end(),
    std::back_inserter(isect));

  // In plan but not in intersection => toCreate
  for (auto const i : planv) {
    if (std::find(isect.begin(), isect.end(), i) == isect.end()) {
      toCreate.emplace_back(i);
    }
  }

  // Local but not in intersection => toDrop
  for (auto const i : localv) {
    if (std::find(isect.begin(), isect.end(), i) == isect.end()) {
      toDrop.emplace_back(i);
    }
  }

  return result;
  
}

/// @brief handle plan for local databases
arangodb::Result arangodb::maintenance::executePlanForDatabases (
  Node plan, Node current, std::vector<std::string> local) {

  arangodb::Result result;
  ActionRegistry* actreg = ActionRegistry::instance();

  // build difference between plan and local
  std::vector<std::string> toCreate, toDrop;
  diffPlanLocalForDatabases(plan, local, toCreate, toDrop);

  // dispatch creations
  for (auto const& i : toCreate) {
    auto desc = ActionDescription({{"name", "CreateDatabase"}, {"database", i}});
    if (actreg->get(desc) == nullptr) {
      actreg->dispatch(desc);
    }
  }

  // dispatch creations
  for (auto const& i : toDrop) {
    auto desc = ActionDescription({{"name", "DropDatabase"}, {"database", i}});
    if (actreg->get(desc) == nullptr) {
      actreg->dispatch(desc);
    }
  }

  return result;
  
}
