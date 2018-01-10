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

#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ActionRegistry.h"
#include "Cluster/Maintenance.h"
#include "VocBase/LogicalCollection.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

#include <algorithm>

using namespace arangodb::basics;
using namespace arangodb::consensus;
using namespace arangodb::maintenance;

/// @brief calculate difference between plan and local for for databases
arangodb::Result arangodb::maintenance::diffPlanLocalForDatabases(
  VPackSlice const& plan, std::vector<std::string> const& local,
  std::vector<std::string>& toCreate, std::vector<std::string>& toDrop) {

  arangodb::Result result;
  
  std::vector<std::string> planv, isect;
  VPackSlice pdbs = plan.get(
    std::vector<std::string>{"arango","Plan","Databases"});
  for (auto const& i : VPackObjectIterator(pdbs)) {
    planv.emplace_back(i.key.copyString());
  }
  std::sort(planv.begin(), planv.end());

  // Build intersection
  std::set_intersection(
    planv.begin(), planv.end(), local.begin(), local.end(),
    std::back_inserter(isect));

  // In plan but not in intersection => toCreate
  for (auto const i : planv) {
    if (std::find(isect.begin(), isect.end(), i) == isect.end()) {
      toCreate.emplace_back(i);
    }
  }

  // Local but not in intersection => toDrop
  for (auto const i : local) {
    if (std::find(isect.begin(), isect.end(), i) == isect.end()) {
      toDrop.emplace_back(i);
    }
  }

  return result;
  
}


/// @brief calculate difference between plan and local for for databases
arangodb::Result arangodb::maintenance::diffPlanLocalForCollections(
  VPackSlice const& plan, VPackSlice const& local,
  std::vector<std::string>& toCreate, std::vector<std::string>& toDrop,
  std::vector<std::string>& toSync) {
  
  arangodb::Result result;

  for (auto const& l : VPackObjectIterator(local)) {

    // sorted list of collections (local)
    //std::string const& database = l.first;
/*    auto collections = l.value;
    std::sort(
      collections.begin(), collections.end(),
      [](arangodb::LogicalCollection* l, arangodb::LogicalCollection* r) -> bool {
        return StringUtils::tolower(l->name())<StringUtils::tolower(r->name()); });
*/  
  }

  return result;
  
}

/*
arangodb::Result arangodb::maintenance::diffLocalCurrentForDatabases(
  VPackSlice const& local, VPackSlice const& Current,
  VPackBuilder& agencyTransaction) {
  
  arangodb::Result result;
  std::vector<std::string> localv, current;
  
  { VPackObjectBuilder o(&agencyTransaction);
  }

  return result;
  
  }*/


/// @brief handle plan for local databases
arangodb::Result arangodb::maintenance::executePlanForDatabases (
  VPackSlice const& plan, VPackSlice const& current, VPackSlice const& local) {

  arangodb::Result result;
  ActionRegistry* actreg = ActionRegistry::instance();

  // build difference between plan and local
  std::vector<std::string> toCreate, toDrop;
  std::vector<std::string> localv;
  for (auto const& i : VPackObjectIterator(local)) {
    localv.emplace_back(i.key.copyString());
  }
  std::sort(localv.begin(), localv.end());
  diffPlanLocalForDatabases(plan, localv, toCreate, toDrop);

  // dispatch creations
  for (auto const& i : toCreate) {
    auto desc = ActionDescription({{"name", "CreateDatabase"}, {"database", i}});
    if (actreg->get(desc) == nullptr) {
      actreg->dispatch(desc);
    }
  }

  // dispatch drops
  for (auto const& i : toDrop) {
    auto desc = ActionDescription({{"name", "DropDatabase"}, {"database", i}});
    if (actreg->get(desc) == nullptr) {
      actreg->dispatch(desc);
    }
  }

  return result;
  
}


/// @brief Phase one: Compare plan and local and create descriptions
arangodb::Result arangodb::maintenance::phaseOne (
  VPackSlice const& plan, VPackSlice const& cur, VPackSlice const& local) {

  arangodb::Result result;

  // Execute database changes
  result = executePlanForDatabases(plan, cur, local);
  if (result.fail()) {
    return result;
  }
  
  // Execute collection changes
  result = executePlanForCollections(plan, cur, local);
  if (result.fail()) {
    return result;
  }
  
  // Synchronise shards
  result = synchroniseShards(plan, cur, local);
  
  return result;
}


/// @brief Phase two: See, what we can report to the agency
arangodb::Result arangodb::maintenance::phaseTwo (
  VPackSlice const& plan, VPackSlice const& cur) {
  arangodb::Result result;
  return result;
}


arangodb::Result arangodb::maintenance::executePlanForCollections (
  VPackSlice const& plan, VPackSlice const& current, VPackSlice const& local) {
  arangodb::Result result;
  return result;
}

arangodb::Result arangodb::maintenance::synchroniseShards (
  VPackSlice const&, VPackSlice const&, VPackSlice const&) {
  arangodb::Result result;
  return result;
}

