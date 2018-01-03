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

static std::string const PLANNED_DATABASES("Plan/Databases");

using namespace arangodb;
using namespace arangodb::consensus;
using namespace arangodb::maintenance;

arangodb::Result diffPlanLocalForDatabases(
  Node const& plan, std::vector<std::string> const& local,
  std::vector<std::string>& toAdd, std::vector<std::string>& toRemove) {

  arangodb::Result result;
  
  TRI_ASSERT(plan.has(PLANNED_DATABASES));
  Node const& plannedDatabases = plan(PLANNED_DATABASES);
  std::vector<std::string> planv;
  for (auto const i : plannedDatabases.children()) {
    planv.emplace_back(i.first);
  }
    
  std::vector<std::string> isect;
  std::set_intersection(
    planv.begin(), planv.end(), local.begin(), local.end(), isect.begin());

  // In plan but not in intersection => toAdd
  for (auto const i : planv) {
    if (std::find(isect.begin(), isect.end(), i) == isect.end()) {
      toAdd.emplace_back(i);
    }
  }

  // Local but not in intersection => toRemove
  for (auto const i : local) {
    if (std::find(isect.begin(), isect.end(), i) == isect.end()) {
      toRemove.emplace_back(i);
    }
  }

  return result;
  
}

/// @brief handle plan for local databases
arangodb::Result executePlanForDatabases (
  Node plan, Node current, std::vector<std::string> local) {

  arangodb::Result result;
  ActionRegistry* actreg = ActionRegistry::instance();

  // build difference between plan and local
  std::vector<std::string> toAdd, toRemove;
  diffPlanLocalForDatabases(plan, local, toAdd, toRemove);

  // dispatch creations
  for (auto const& i : toAdd) {
    actreg->dispatch(
      ActionDescription({{"name", "CreateDatabase"}, {"database", i}}));
  }

  // dispatch creations
  for (auto const& i : toRemove) {
    actreg->dispatch(
      ActionDescription({{"name", "DropDatabase"}, {"database", i}}));
  }

  return result;
  
}
