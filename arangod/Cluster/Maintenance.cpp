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
#include "Logger/Logger.h"
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
arangodb::Result arangodb::maintenance::diffPlanLocal (
  VPackSlice const& plan, VPackSlice const& local,
  std::vector<std::string>& toCreate, std::vector<std::string>& toDrop,
  std::vector<std::string>& toSync) {

  arangodb::Result result;
  /*
  ActionRegistry* ar = ActionRegistry::instance();
  
  auto const& pdbs =
    plan.get(std::vector<std::string>{"arango", "Plan", "Collections"});

  // Plan to local mismatch
  for (auto const& pdb : VPackObjectIterator(plan)) {
    auto const& dbname = pdb.key.copyString();
    if (local.hasKey(dbname)) {    // have database in both see to collections
      auto const& ldb = local.get(dbname);
      for (auto const& pcol : VPackObjectIterator(pdb.value)) {
        auto const& cname = pcol.key.copyString();
        auto const& cprops = pcol.value;
        std::map<std::string, std::string> props;
        auto const& shards = cprops.get("shards");
        for (auto const& shard : VPackObjectIterator(shards.value)) {
          auto const& shname = shard.key.copyString();
          size_t pos = 0;
          for (auto const& db : VPackArrayIterator(shard.value)) {

            if (db.copyString() == _id)  { // We have some responsibility
              
              if (props.empty()) { // Only do this once
                props = stdMapFromCollectionVPack(cprops);
                props["collection"] = shname;
              }
              
              if (ldb.hasKey(shname)) { // Have local collection with that name
                
                // see to properties / indexes
                LOG_TOPIC(WARN, Logger::MAINTENANCE) << "Props  " << shname;
                ar->dispatch(
                  ActionDescription({{"name", "AlterCollection"}, {"collection", shname}}));
              } else {                 // Create the sucker!
                LOG_TOPIC(ERR,  Logger::MAINTENANCE) << "Create " << shname;
                auto description (
                  {{"name", "CreateCollection"}, {"collection", shname}});
                for (auto const& i : VPackObjectIterator(props))
                ar->dispatch(desciption);
              }
              
            }
            ++pos;
          }
        }
      }
    } else {
      LOG_TOPIC(ERR, Logger::MAINTENANCE) << "Create  " << dbname;
      ar->dispatch(
        ActionDescription({{"name", "CreateDatabase"}, {"database", dbname}}));
    }

    
  }

    //
  for (auto const& db : VPackObjectIterator(local)) {
    auto const& dbname = db.key.copyString();

    if (pdbs.hasKey(dbname)) {
      LOG_TOPIC(WARN, Logger::MAINTENANCE) << "modify " << dbname;
      auto const& pdbs = 
      for (auto const& col : VPackObjectIterator(db.value)) {
        
      }
    } else {
      LOG_TOPIC(ERR, Logger::MAINTENANCE) << "database not existing yet " << name;
    }
  }
    */

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

