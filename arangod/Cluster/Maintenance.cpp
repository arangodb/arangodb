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

static std::vector<std::string> cmp {
  "journalSize", "waitForSync", "doCompact", "indexBuckets"};

VPackBuilder createProps(VPackSlice const& s) {
  VPackBuilder builder;
  TRI_ASSERT(s.isObject());
  { VPackObjectBuilder b(&builder);
    for (auto const& attr : VPackObjectIterator(s)) {
      std::string const key = attr.key.copyString();
      if (key == "id" || key == "name") {
        continue;
      }
      builder.add(key, attr.value);
    }}
  return builder;
}


VPackBuilder compareRelevantProps (
  VPackSlice const& first, VPackSlice const& second) {
  VPackBuilder result;
  { VPackObjectBuilder b(&result);
    for (auto property : cmp) {
      auto const& planned = first.get(property);
      if (planned != second.get(property)) { // Register any change
        result.add(property,planned);
      }
    }
  }
  return result;
}


/// @brief calculate difference between plan and local for for databases
arangodb::Result arangodb::maintenance::diffPlanLocal (
  VPackSlice const& plan, VPackSlice const& local, std::string const& serverId,
  std::vector<ActionDescription>& actions) {

  std::unordered_set<std::string> intersection;
  
  arangodb::Result result;
  auto const& pdbs =
    plan.get(std::vector<std::string>{"arango", "Plan", "Collections"});

  // Plan to local mismatch ----------------------------------------------------
  // Create or modify if local collections are affected
  
  for (auto const& pdb : VPackObjectIterator(pdbs)) {
    auto const& dbname = pdb.key.copyString();
    if (local.hasKey(dbname)) {    // have database in both see to collections
      auto const& ldb = local.get(dbname);
      for (auto const& pcol : VPackObjectIterator(pdb.value)) {
        auto const& cname = pcol.key.copyString();
        auto const& cprops = pcol.value;
        VPackBuilder props;
        auto const& shards = cprops.get("shards");
        for (auto const& shard : VPackObjectIterator(shards)) {
          auto const& shname = shard.key.copyString();
          size_t leader = true; // Want to understand leadership
          for (auto const& db : VPackArrayIterator(shard.value)) {

            // We only care for shards, where we find our own ID
            if (db.copyString() == serverId)  {

              intersection.emplace(shname);
              if (props.isEmpty()) {             // Must not have name or id 
                props = createProps(pcol.value); // Only once might need often!
              }

              if (ldb.hasKey(shname)) {   // Have local collection with that name
                auto const properties = 
                  compareRelevantProps(pcol.value, ldb.get(shname));
                
                // If comparison has brought any updates
                if (properties.slice() != VPackSlice::emptyObjectSlice()) {
                  actions.push_back(
                    ActionDescription(
                      {{"name", "AlterCollection"}, {"collection", shname}},
                      properties));
                }
                
                // Indexes
                if (cprops.has("indexes") && ) {
                  auto const& indexes = cprops.get("indexes");
                  TRI_ASSERT(indexes.isArray());
                  if (indexes.length() > 1) { // _key Index always there
                    for (auto const& index :
                           VPackArrayIterator(cprops.get("indexes"))) {
                      ActionDescription(
                        {{"name": "EnsureIndex"},{"collection": shname}},index});
                    }
                  }
                }
              } else {                   // Create the sucker!
                actions.push_back(
                  ActionDescription(
                    {{"name", "CreateCollection"}, {"collection", shname},
                     {"database", dbname}}, props));
              }

              
            }
            
            // Only first entry is leader
            leader = false;

            
            
          }
        }
      }
    } else {
      actions.push_back(
        ActionDescription({{"name", "CreateDatabase"}, {"database", dbname}}));
    }
  }
  

  // Compare local to plan -----------------------------------------------------
  
  for (auto const& db : VPackObjectIterator(local)) {
    auto const& dbname = db.key.copyString();
    if (pdbs.hasKey(dbname)) {
      for (auto const& col : VPackObjectIterator(db.value)) {
        bool drop = false;
        auto const& colname = col.key.copyString();
        std::unordered_set<std::string>::const_iterator it;
        if (intersection.empty()) {
          drop = true;
        } else {
          it = std::find(intersection.begin(), intersection.end(), colname);
          if (it == intersection.end()) {
            drop = true;
          }
        }
        if (drop) {
          actions.push_back(
            ActionDescription({{"name", "DropCollection"},
                {"database", dbname}, {"collection", colname}}));
        } else {
          intersection.erase(it);
        }
      }
    } else {
      actions.push_back(
        ActionDescription({{"name", "DropDatabase"}, {"database", dbname}}));
    }
  }
  
  return result;
  
}

/// @brief handle plan for local databases
arangodb::Result arangodb::maintenance::executePlan (
  VPackSlice const& plan, VPackSlice const& current, VPackSlice const& local,
  std::string const& serverId) {

  arangodb::Result result;
  ActionRegistry* registry = ActionRegistry::instance();

  // build difference between plan and local
  std::vector<std::string> toCreate, toDrop;
  std::vector<ActionDescription> actions;
  diffPlanLocal(plan, local, serverId, actions);

  for (auto const& action : actions) {
    registry->dispatch(action);
  }

  return result;
  
}

/// @brief Phase one: Compare plan and local and create descriptions
arangodb::Result arangodb::maintenance::phaseOne (
  VPackSlice const& plan, VPackSlice const& cur, VPackSlice const& local,
  std::string const& serverId) {

  arangodb::Result result;

  // Execute database changes
  result = executePlan(plan, cur, local, serverId);
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


arangodb::Result arangodb::maintenance::synchroniseShards (
  VPackSlice const&, VPackSlice const&, VPackSlice const&) {
  arangodb::Result result;
  return result;
}

