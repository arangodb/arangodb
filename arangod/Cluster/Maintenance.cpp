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

#include "ApplicationFeatures/ApplicationServer.h"
#include "Agency/AgencyStrings.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Logger/Logger.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/FollowerInfo.h"
#include "Cluster/Maintenance.h"
#include "Indexes/Index.h"
#include "Utils/DatabaseGuard.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/Methods/Databases.h"

#include <velocypack/Compare.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/Collection.h>
#include <velocypack/velocypack-aliases.h>

#include <algorithm>
#include <regex>

using namespace arangodb;
using namespace arangodb::consensus;
using namespace arangodb::basics;
using namespace arangodb::maintenance;
using namespace arangodb::methods;
using namespace arangodb::basics::StringUtils;

static std::vector<std::string> const cmp {
  JOURNAL_SIZE, WAIT_FOR_SYNC, DO_COMPACT, INDEX_BUCKETS};

static VPackValue const VP_DELETE("delete");
static VPackValue const VP_SET("set");

static int indexOf(VPackSlice const& slice, std::string const& val) {
  if (slice.isArray()) {
    int counter = 0;
    for (auto const& entry : VPackArrayIterator(slice)) {
      if (entry.isString()) {
        if (entry.copyString() == val) {
          return counter;
        }
      }
      counter++;
    }
  }
  return -1;
}

static std::shared_ptr<VPackBuilder> createProps(VPackSlice const& s) {
  TRI_ASSERT(s.isObject());
  return std::make_shared<VPackBuilder>(
    arangodb::velocypack::Collection::remove(s,
    std::unordered_set<std::string>({ID, NAME})));
}

static std::shared_ptr<VPackBuilder> compareRelevantProps (
  VPackSlice const& first, VPackSlice const& second) {
  auto result = std::make_shared<VPackBuilder>();
  { VPackObjectBuilder b(result.get());
    for (auto const& property : cmp) {
      auto const& planned = first.get(property);
      if (planned != second.get(property)) { // Register any change
        result->add(property,planned);
      }
    }
  }
  return result;
}


static VPackBuilder compareIndexes(
  std::string const& dbname, std::string const& collname,
  std::string const& shname,
  VPackSlice const& plan, VPackSlice const& local,
  MaintenanceFeature::errors_t const& errors,
  std::unordered_set<std::string>& indis) {

  VPackBuilder builder;
  { VPackArrayBuilder a(&builder);
    if (plan.isArray()) {
      for (auto const& pindex : VPackArrayIterator(plan)) {

        // Skip primary and edge indexes
        auto const& ptype   = pindex.get(StaticStrings::IndexType).copyString();
        if (ptype == PRIMARY || ptype == EDGE) {
          continue;
        }
        VPackSlice planId = pindex.get(ID);
        TRI_ASSERT(planId.isString());
        std::string planIdS = planId.copyString();
        std::string planIdWithColl = shname + "/" + planIdS;
        indis.emplace(planIdWithColl);

        // See, if we already have an index with the id given in the Plan:
        bool found = false;
        if (local.isArray()) {
          for (auto const& lindex : VPackArrayIterator(local)) {

            // Skip primary and edge indexes
            auto const& ltype   = lindex.get(StaticStrings::IndexType).copyString();
            if (ltype == PRIMARY || ltype == EDGE) {
              continue;
            }

            VPackSlice localId = lindex.get(ID);
            TRI_ASSERT(localId.isString());
            // The local ID has the form <collectionName>/<ID>, to compare,
            // we need to extract the local ID:
            std::string localIdS = localId.copyString();
            auto pos = localIdS.find('/');
            if (pos != std::string::npos) {
              localIdS = localIdS.substr(pos+1);
            }

            if (localIdS == planIdS) {
              // Already have this id, so abort search:
              found = true;
              // We should be done now, this index already exists, and since
              // one cannot legally change the properties of an index, we
              // should be fine. However, for robustness sake, we compare,
              // if the local index found actually has the right properties,
              // if not, we schedule a dropIndex action:
              if (!arangodb::Index::Compare(pindex, lindex)) {
                // To achieve this, we remove the long version of the ID
                // from the indis set. This way, the local index will be
                // dropped further down in handleLocalShard:
                indis.erase(planIdWithColl);
              }
              break;
            }
          }
        }
        if (!found) {
          // Finally check if we have an error for this index:
          bool haveError = false;
          std::string errorKey = dbname + "/" + collname + "/" + shname;
          auto it1 = errors.indexes.find(errorKey);
          if (it1 != errors.indexes.end()) {
            auto it2 = it1->second.find(planIdS);
            if (it2 != it1->second.end()) {
              haveError = true;
            }
          }
          if (!haveError) {
            builder.add(pindex);
          } else {
            LOG_TOPIC(DEBUG, Logger::MAINTENANCE)
              << "Previous failure exists for index " << planIdS << " on shard "
              << dbname << "/" << shname << " for central " << dbname << "/"
              << collname <<"- skipping";
          }
        }
      }
    }
  }

  return builder;
}


void handlePlanShard(
  VPackSlice const& cprops, VPackSlice const& ldb,
  std::string const& dbname, std::string const& colname, std::string const& shname,
  std::string const& serverId, std::string const& leaderId,
  std::unordered_set<std::string>& commonShrds, std::unordered_set<std::string>& indis,
  MaintenanceFeature::errors_t& errors, MaintenanceFeature& feature,
  std::vector<ActionDescription>& actions) {

  bool shouldBeLeading = serverId == leaderId;

  commonShrds.emplace(shname);
  auto props = createProps(cprops); // Only once might need often!

  if (ldb.hasKey(shname)) {   // Have local collection with that name
    auto const lcol = ldb.get(shname);
    bool leading = lcol.get(THE_LEADER).copyString().empty();
    auto const properties = compareRelevantProps(cprops, lcol);

    auto fullShardLabel = dbname + "/" + colname + "/" + shname;

    // Check if there is some in-sync-follower which is no longer in the Plan:
    std::string followersToDropString;
    if (leading && shouldBeLeading) {
      VPackSlice shards = cprops.get("shards");
      if (shards.isObject()) {
        VPackSlice planServers = shards.get(shname);
        if (planServers.isArray()) {
          VPackSlice inSyncFollowers = lcol.get("servers");
          if (inSyncFollowers.isArray()) {
            // Now we have two server lists, we are looking for a server
            // which does not occur in the plan, but is in the followers
            // at an index > 0:
            std::unordered_set<std::string> followersToDrop;
            for (auto const& q : VPackArrayIterator(inSyncFollowers)) {
              followersToDrop.insert(q.copyString());
            }
            for (auto const& p : VPackArrayIterator(planServers)) {
              if (p.isString()) {
                followersToDrop.erase(p.copyString());
              }
            }
            // Everything remaining in followersToDrop is something we
            // need to act on
            for (auto const& r : followersToDrop) {
              if (!followersToDropString.empty()) {
                followersToDropString.push_back(',');
              }
              followersToDropString += r;
            }
          }
        }
      }
    }

    // If comparison has brought any updates
    if (properties->slice() != VPackSlice::emptyObjectSlice()
        || leading != shouldBeLeading || !followersToDropString.empty()) {

      if (errors.shards.find(fullShardLabel) ==
          errors.shards.end()) {
        actions.emplace_back(
          ActionDescription(
            std::map<std::string,std::string> {
              {NAME, UPDATE_COLLECTION}, {DATABASE, dbname}, {COLLECTION, colname},
              {SHARD, shname}, {THE_LEADER, shouldBeLeading ? std::string() : leaderId},
              {SERVER_ID, serverId}, {LOCAL_LEADER, lcol.get(THE_LEADER).copyString()},
              {FOLLOWERS_TO_DROP, followersToDropString}},
              properties));
      } else {
        LOG_TOPIC(DEBUG, Logger::MAINTENANCE)
          << "Previous failure exists for local shard " << dbname
          << "/" << shname << "for central " << dbname << "/" << colname
          <<"- skipping";
      }
    }

    // Indexes
    if (cprops.hasKey(INDEXES)) {
      auto const& pindexes = cprops.get(INDEXES);
      auto const& lindexes = lcol.get(INDEXES);
      auto difference = compareIndexes(dbname, colname, shname,
          pindexes, lindexes, errors, indis);

      // Index errors are checked in `compareIndexes`. THe loop below only
      // cares about those indexes that have no error.
      if (difference.slice().isArray()) {
        for (auto const& index : VPackArrayIterator(difference.slice())) {
          actions.emplace_back(
            ActionDescription({{NAME, "EnsureIndex"}, {DATABASE, dbname},
                {COLLECTION, colname}, {SHARD, shname},
                {StaticStrings::IndexType,
                    index.get(StaticStrings::IndexType).copyString()},
                {FIELDS, index.get(FIELDS).toJson()}, 
                {ID, index.get(ID).copyString()}},
              std::make_shared<VPackBuilder>(index)));
        }
      }
    }
  } else { // Create the sucker, if not a previous error stops us
    if (errors.shards.find(dbname + "/" + colname + "/" + shname) ==
        errors.shards.end()) {
      actions.emplace_back(
        ActionDescription(
          {{NAME, CREATE_COLLECTION}, {COLLECTION, colname}, {SHARD, shname},
           {DATABASE, dbname}, {SERVER_ID, serverId}, {THE_LEADER, shouldBeLeading ? std::string() : leaderId}},
          props));
    } else {
      LOG_TOPIC(DEBUG, Logger::MAINTENANCE)
        << "Previous failure exists for creating local shard " << dbname
        << "/" << shname << "for central " << dbname << "/" << colname
        <<"- skipping";
    }
  }
}


void handleLocalShard(
  std::string const& dbname, std::string const& colname, VPackSlice const& cprops,
  VPackSlice const& shardMap, std::unordered_set<std::string>& commonShrds,
  std::unordered_set<std::string>& indis, std::string const &serverId,
  std::vector<ActionDescription>& actions) {

  std::unordered_set<std::string>::const_iterator it;

  std::string plannedLeader;
  if (shardMap.hasKey(colname) && shardMap.get(colname).isArray()) {
    plannedLeader = shardMap.get(colname)[0].copyString();
  }
  bool localLeader = cprops.get(THE_LEADER).copyString().empty();
  if (plannedLeader == UNDERSCORE + serverId && localLeader) {
    actions.emplace_back(
      ActionDescription (
        {{NAME, "ResignShardLeadership"}, {DATABASE, dbname}, {SHARD, colname}}));
  } else {
    bool drop = false;
    // check if shard is in plan, if not drop it
    if (commonShrds.empty()) {
      drop = true;
    } else {
      it = std::find(commonShrds.begin(), commonShrds.end(), colname);
      if (it == commonShrds.end()) {
        drop = true;
      }
    }

    if (drop) {
      actions.emplace_back(
        ActionDescription({{NAME, DROP_COLLECTION},
            {DATABASE, dbname}, {COLLECTION, colname}}));
    } else {
      // The shard exists in both Plan and Local
      commonShrds.erase(it);  // it not a common shard?

      // We only drop indexes, when collection is not being dropped already
      if (cprops.hasKey(INDEXES)) {
        if (cprops.get(INDEXES).isArray()) {
          for (auto const& index :
                 VPackArrayIterator(cprops.get(INDEXES))) {
            auto const& type = index.get(StaticStrings::IndexType).copyString();
            if (type != PRIMARY && type != EDGE) {
              std::string const id = index.get(ID).copyString();

              // check if index is in plan
              if (indis.find(colname + "/" + id) != indis.end() ||
                  indis.find(id)                 != indis.end()) {
                indis.erase(id);
              } else {
                actions.emplace_back(
                  ActionDescription(
                  {{NAME, "DropIndex"}, {DATABASE, dbname}, {COLLECTION, colname}, {"index", id}}));
              }
            }
          }
        }
      }
    }
  }
}

/// @brief Get a map shardName -> servers
VPackBuilder getShardMap (VPackSlice const& plan) {
  VPackBuilder shardMap;
  { VPackObjectBuilder o(&shardMap);
    for (auto database : VPackObjectIterator(plan)) {
      for (auto collection : VPackObjectIterator(database.value)) {
        for (auto shard : VPackObjectIterator(collection.value.get(SHARDS))) {
          std::string const shName = shard.key.copyString();
          shardMap.add(shName, shard.value);
        }
      }
    }}
  return shardMap;
}


struct NotEmpty {
  bool operator()(const std::string& s) { return !s.empty(); }
};

/// @brief calculate difference between plan and local for for databases
arangodb::Result arangodb::maintenance::diffPlanLocal (
  VPackSlice const& plan, VPackSlice const& local, std::string const& serverId,
  MaintenanceFeature::errors_t& errors, MaintenanceFeature& feature,
  std::vector<ActionDescription>& actions) {

  arangodb::Result result;
  std::unordered_set<std::string> commonShrds; // Intersection collections plan&local
  std::unordered_set<std::string> indis; // Intersection indexes plan&local

  // Plan to local mismatch ----------------------------------------------------
  // Create or modify if local databases are affected
  auto pdbs = plan.get(DATABASES);
  for (auto const& pdb : VPackObjectIterator(pdbs)) {
    auto const& dbname = pdb.key.copyString();
    if (!local.hasKey(dbname)) {
      if (errors.databases.find(dbname) == errors.databases.end()) {
        actions.emplace_back(ActionDescription({{std::string(NAME), std::string(CREATE_DATABASE)}, {std::string(DATABASE), std::string(dbname)}}));
      } else {
        LOG_TOPIC(DEBUG, Logger::MAINTENANCE)
          << "Previous failure exists for creating database " << dbname
          << "skipping";
      }
    }
  }

  // Drop databases, which are no longer in plan
  for (auto const& ldb : VPackObjectIterator(local)) {
    auto const& dbname = ldb.key.copyString();
    if (!plan.hasKey(std::vector<std::string> {DATABASES, dbname})) {
      actions.emplace_back(ActionDescription({{std::string(NAME), std::string(DROP_DATABASE)}, {std::string(DATABASE), std::string(dbname)}}));
    }
  }

  // Check errors for databases, which are no longer in plan and remove from errors
  for (auto& database : errors.databases) {
    if (!plan.hasKey(std::vector<std::string>{DATABASES, database.first})) {
      database.second.reset();
    }
  }

  // Create or modify if local collections are affected
  pdbs = plan.get(COLLECTIONS);
  for (auto const& pdb : VPackObjectIterator(pdbs)) { // for each db in Plan
    auto const& dbname = pdb.key.copyString();
    if (local.hasKey(dbname)) {    // have database in both
      auto const& ldb = local.get(dbname);
      for (auto const& pcol : VPackObjectIterator(pdb.value)) { // for each plan collection
        auto const& cprops = pcol.value;
        for (auto const& shard : VPackObjectIterator(cprops.get(SHARDS))) { // for each shard in plan collection
          if (shard.value.isArray()) {
            for (auto const& dbs : VPackArrayIterator(shard.value)) { // for each db server with that shard
              // We only care for shards, where we find us as "serverId" or "_serverId"
              if (dbs.isEqualString(serverId) ||
                  dbs.isEqualString(UNDERSCORE+serverId)) {
                // at this point a shard is in plan, we have the db for it
                handlePlanShard(
                  cprops, ldb, dbname, pcol.key.copyString(),
                  shard.key.copyString(), serverId, shard.value[0].copyString(),
                  commonShrds, indis, errors, feature, actions);
                break ;
              }
            }
          } // else if(!shard.value.isArray()) - intentionally do nothing
        }
      }
    }
  }

  // At this point commonShrds contains all shards that eventually reside on
  // this server, are in Plan and their database is present

  // Compare local to plan -----------------------------------------------------
  auto const shardMap = getShardMap(pdbs);  // plan shards -> servers
  for (auto const& db : VPackObjectIterator(local)) { // for each local databases
    auto const& dbname = db.key.copyString();
    if (pdbs.hasKey(dbname)) {                        // if in plan
      for (auto const& sh : VPackObjectIterator(db.value)) { // for each local shard
        std::string shName = sh.key.copyString();
        handleLocalShard(dbname, shName, sh.value, shardMap.slice(), commonShrds,
                         indis, serverId, actions);
      }
    }
  }



  // See if shard errors can be thrown out:
  for (auto& shard : errors.shards) {
    std::vector<std::string> path = split(shard.first, '/');
    path.pop_back(); // Get rid of shard
    if (!pdbs.hasKey(path)) { // we can drop the local error
      shard.second.reset();
    }
  }

  // See if index errors can be thrown out:
  for (auto& shard : errors.indexes) {
    std::vector<std::string> path = split(shard.first, '/');  // dbname, collection, shardid
    path.pop_back();    // dbname, collection
    path.emplace_back(INDEXES); // dbname, collection, indexes
    VPackSlice indexes = pdbs.get(path);
    if (!indexes.isArray()) { // collection gone, can drop errors
      for (auto& index : shard.second) {
        index.second.reset();
      }
    } else {   // need to look at individual errors and indexes:
      for (auto& p : shard.second) {
        std::string const& id = p.first;
        bool found = false;
        for (auto const& ind : VPackArrayIterator(indexes)) {
          if (ind.get(ID).copyString() == id) {
            found = true;
            break;
          }
        }
        if (!found) {
          p.second.reset();
        }
      }
    }
  }

  return result;

}

/// @brief handle plan for local databases
arangodb::Result arangodb::maintenance::executePlan (
  VPackSlice const& plan, VPackSlice const& local,
  std::string const& serverId, MaintenanceFeature& feature,
  VPackBuilder& report) {

  arangodb::Result result;

  // Errors from maintenance feature
  MaintenanceFeature::errors_t errors;
  result = feature.copyAllErrors(errors);
  if (!result.ok()) {
    LOG_TOPIC(ERR, Logger::MAINTENANCE) <<
      "phaseOne: failed to acquire copy of errors from maintenance feature.";
    return result;
  }

  // build difference between plan and local
  std::vector<ActionDescription> actions;
  report.add(VPackValue(AGENCY));
  { VPackArrayBuilder a(&report);
    diffPlanLocal(plan, local, serverId, errors, feature, actions); }

  for (auto const& i : errors.databases) {
    if (i.second == nullptr) {
      feature.removeDBError(i.first);
    }
  }
  for (auto const& i : errors.shards) {
    if (i.second == nullptr) {
      feature.removeShardError(i.first);
    }
  }
  for (auto const& i : errors.indexes) {
    std::unordered_set<std::string> tmp;
    for (auto const& index : i.second) {
      if (index.second == nullptr) {
        tmp.emplace(index.first);
      }
    }
    if (!tmp.empty()) {
      feature.removeIndexErrors(i.first, tmp);
    }
  }

  TRI_ASSERT(report.isOpenObject());
  report.add(VPackValue(ACTIONS));
  { VPackArrayBuilder a(&report);
    // enact all
    for (auto const& action : actions) {
      LOG_TOPIC(DEBUG, Logger::MAINTENANCE)
        << "adding action " << action << " to feature ";
      { VPackObjectBuilder b(&report);
        action.toVelocyPack(report);
      }
      feature.addAction(std::make_shared<ActionDescription>(action), false);
    }
  }

  return result;
}

/// @brief add new database to current
void addDatabaseToTransactions(
  std::string const& name, Transactions& transactions) {

  // [ {"dbPath":{}}, {"dbPath":{"oldEmpty":true}} ]

  std::string dbPath = CURRENT_COLLECTIONS + name;
  VPackBuilder operation; // create database in current
  { VPackObjectBuilder b(&operation);
    operation.add(dbPath, VPackSlice::emptyObjectSlice()); }
  VPackBuilder precondition;
  { VPackObjectBuilder b(&precondition);
    precondition.add(VPackValue(dbPath));
    { VPackObjectBuilder bb(&precondition);
      precondition.add("oldEmpty", VPackValue(true)); }}
  transactions.push_back({operation, precondition});

}

/// @brief report local to current
arangodb::Result arangodb::maintenance::diffLocalCurrent (
  VPackSlice const& local, VPackSlice const& current,
  std::string const& serverId, Transactions& transactions) {

  arangodb::Result result;
  auto const& cdbs = current;

  // Iterate over local databases
  for (auto const& ldbo : VPackObjectIterator(local)) {
    std::string dbname = ldbo.key.copyString();
    //VPackSlice ldb = ldbo.value;

    // Current has this database
    if (cdbs.hasKey(dbname)) {

    } else {
      // Create new database in current
      addDatabaseToTransactions(dbname, transactions);
    }
  }

  return result;
}


/// @brief Phase one: Compare plan and local and create descriptions
arangodb::Result arangodb::maintenance::phaseOne (
  VPackSlice const& plan, VPackSlice const& local, std::string const& serverId,
  MaintenanceFeature& feature, VPackBuilder& report) {

  arangodb::Result result;

  report.add(VPackValue(PHASE_ONE));
  { VPackObjectBuilder por(&report);

    // Execute database changes
    try {
      result = executePlan(plan, local, serverId, feature, report);
    } catch (std::exception const& e) {
      LOG_TOPIC(ERR, Logger::MAINTENANCE)
        << "Error executing plan: " << e.what()
        << ". " << __FILE__ << ":" << __LINE__;
    }}

  report.add(VPackValue(PLAN));
  { VPackObjectBuilder p(&report);
    report.add("Version", plan.get("Version"));}

  return result;

}

static VPackBuilder removeSelectivityEstimate(VPackSlice const& index) {
  TRI_ASSERT(index.isObject());
  return arangodb::velocypack::Collection::remove(index,
      std::unordered_set<std::string>({SELECTIVITY_ESTIMATE}));
}

static VPackBuilder assembleLocalCollectionInfo(
  VPackSlice const& info, VPackSlice const& planServers,
  std::string const& database, std::string const& shard,
  std::string const& ourselves, MaintenanceFeature::errors_t const& allErrors) {

  VPackBuilder ret;

  try {
    DatabaseGuard guard(database);
    auto vocbase = &guard.database();

    auto collection = vocbase->lookupCollection(shard);
    if (collection == nullptr) {
      std::string errorMsg(
        "Maintenance::assembleLocalCollectionInfo: Failed to lookup collection ");
      errorMsg += shard;
      LOG_TOPIC(DEBUG, Logger::MAINTENANCE) << errorMsg;
      { VPackObjectBuilder o(&ret); }
      return ret;
    }

    std::string errorKey
      = database + "/" + std::to_string(collection->planId()) + "/" + shard;
    { VPackObjectBuilder r(&ret);
      auto it = allErrors.shards.find(errorKey);
      if (it == allErrors.shards.end()) {
        ret.add(StaticStrings::Error, VPackValue(false));
        ret.add(StaticStrings::ErrorMessage, VPackValue(std::string()));
        ret.add(StaticStrings::ErrorNum, VPackValue(0));
      } else {
        VPackSlice errs(static_cast<uint8_t const*>(it->second->data()));
        ret.add(StaticStrings::Error, errs.get(StaticStrings::Error));
        ret.add(StaticStrings::ErrorNum, errs.get(StaticStrings::ErrorNum));
        ret.add(StaticStrings::ErrorMessage, errs.get(StaticStrings::ErrorMessage));
      }
      ret.add(VPackValue(INDEXES));
      { VPackArrayBuilder ixs(&ret);
        if (info.get(INDEXES).isArray()) {
          auto it1 = allErrors.indexes.find(errorKey);
          std::unordered_set<std::string> indexesDone;
          // First the indexes as they are in Local, potentially replaced
          // by an error:
          for (auto const& index : VPackArrayIterator(info.get(INDEXES))) {
            std::string id = index.get(ID).copyString();
            indexesDone.insert(id);
            if (it1 != allErrors.indexes.end()) {
              auto it2 = it1->second.find(id);
              if (it2 != it1->second.end()) {
                // Add the error instead:
                ret.add(VPackSlice(
                    static_cast<uint8_t const*>(it2->second->data())));
                continue;
              }
            }
            ret.add(removeSelectivityEstimate(index).slice());
          }
          // Now all the errors for this shard, for which there is no index:
          if (it1 != allErrors.indexes.end()) {
            for (auto const& p : it1->second) {
              if (indexesDone.find(p.first) == indexesDone.end()) {
                ret.add(VPackSlice(
                      static_cast<uint8_t const*>(p.second->data())));
              }
            }
          }
        }
      }
      ret.add(VPackValue(SERVERS));
      { VPackArrayBuilder a(&ret);
        ret.add(VPackValue(ourselves));
        // planServers may be `none` in the case that the shard is not contained
        // in Plan, but in local.
        if (planServers.isArray()) {
          std::shared_ptr<std::vector<std::string> const> current
            = collection->followers()->get();
          for (auto const& server : *current) {
            ret.add(VPackValue(server));
          }
        }
      }
    }
    return ret;
  } catch (std::exception const& e) {
    std::string errorMsg(
      "Maintenance::assembleLocalCollectionInfo: Failed to lookup database ");
    errorMsg += database;
    errorMsg += ", exception: ";
    errorMsg += e.what();
    LOG_TOPIC(WARN, Logger::MAINTENANCE) << errorMsg;
    { VPackObjectBuilder o(&ret); }
    return ret;
  }
}

bool equivalent(VPackSlice const& local, VPackSlice const& current) {
  for (auto const& i : VPackObjectIterator(local)) {
    if (!VPackNormalizedCompare::equals(i.value,current.get(i.key.copyString()))) {
      return false;
    }
  }
  return true;
}

static VPackBuilder assembleLocalDatabaseInfo (std::string const& database,
    MaintenanceFeature::errors_t const& allErrors) {
  // This creates the VelocyPack that is put into
  // /Current/Databases/<dbname>/<serverID>  for a database.

  VPackBuilder ret;

  try {
    DatabaseGuard guard(database);
    auto vocbase = &guard.database();

    { VPackObjectBuilder o(&ret);
      auto it = allErrors.databases.find(database);
      if (it == allErrors.databases.end()) {
        ret.add(StaticStrings::Error, VPackValue(false));
        ret.add(StaticStrings::ErrorNum, VPackValue(0));
        ret.add(StaticStrings::ErrorMessage, VPackValue(""));
      } else {
        VPackSlice errs(static_cast<uint8_t const*>(it->second->data()));
        ret.add(StaticStrings::Error, errs.get(StaticStrings::Error));
        ret.add(StaticStrings::ErrorNum, errs.get(StaticStrings::ErrorNum));
        ret.add(StaticStrings::ErrorMessage, errs.get(StaticStrings::ErrorMessage));
      }
      ret.add(ID, VPackValue(std::to_string(vocbase->id())));
      ret.add("name", VPackValue(vocbase->name())); }

    return ret;
  } catch(std::exception const& e) {
    std::string errorMsg(
      "Maintenance::assembleLocalDatabaseInfo: Failed to lookup database ");
    errorMsg += database;
    errorMsg += ", exception: ";
    errorMsg += e.what();
    LOG_TOPIC(DEBUG, Logger::MAINTENANCE) << errorMsg;
    { VPackObjectBuilder o(&ret); }
    return ret;
  }
}


// updateCurrentForCollections
// diff current and local and prepare agency transactions or whatever
// to update current. Will report the errors created locally to the agency
arangodb::Result arangodb::maintenance::reportInCurrent(
  VPackSlice const& plan, VPackSlice const& cur, VPackSlice const& local,
  MaintenanceFeature::errors_t const& allErrors,
  std::string const& serverId, VPackBuilder& report) {
  arangodb::Result result;

  auto shardMap = getShardMap(plan.get(COLLECTIONS));
  auto pdbs = plan.get(COLLECTIONS);

  for (auto const& database : VPackObjectIterator(local)) {
    auto const dbName = database.key.copyString();

    std::vector<std::string> const cdbpath {DATABASES, dbName, serverId};

    if (!cur.hasKey(cdbpath)) {
      auto const localDatabaseInfo = assembleLocalDatabaseInfo(dbName, allErrors);
      if (!localDatabaseInfo.slice().isEmptyObject()) {
        report.add(VPackValue(CURRENT_DATABASES + dbName + "/" + serverId));
        { VPackObjectBuilder o(&report);
          report.add(OP, VP_SET);
          report.add("payload", localDatabaseInfo.slice()); }
      }
    }


    for (auto const& shard : VPackObjectIterator(database.value)) {

      auto const shName = shard.key.copyString();
      auto const shSlice = shard.value;
      auto const colName = shSlice.get(StaticStrings::DataSourcePlanId).copyString();

      VPackBuilder error;
      if (shSlice.get(THE_LEADER).copyString().empty()) { // Leader

        auto const localCollectionInfo =
          assembleLocalCollectionInfo(
            shSlice, shardMap.slice().get(shName), dbName, shName, serverId,
            allErrors);
        // Collection no longer exists
        if (localCollectionInfo.slice().isEmptyObject()) {
          continue;
        }

        auto cp = std::vector<std::string> {COLLECTIONS, dbName, colName, shName};


        auto inCurrent = cur.hasKey(cp);
        if (!inCurrent || !equivalent(localCollectionInfo.slice(), cur.get(cp))) {


          report.add(
            VPackValue(CURRENT_COLLECTIONS+dbName+"/"+colName+"/"+shName));
            { VPackObjectBuilder o(&report);
              report.add(OP, VP_SET);
              report.add("payload", localCollectionInfo.slice()); }
        }
      } else {  // Follower

        auto servers = std::vector<std::string>
          {COLLECTIONS, dbName, colName, shName, SERVERS};
        if (cur.hasKey(servers)) {
          auto s = cur.get(servers);
          if (s.isArray() && cur.get(servers)[0].copyString() == serverId) {

            // We are in the situation after a restart, that we do not know
            // who the leader is because FollowerInfo is not updated yet.
            // Hence, in the case we are the Leader in Plan but do not
            // know it yet, do nothing here.
            if (shSlice.get("theLeaderTouched").isTrue()) {

              // we were previously leader and we are done resigning.
              // update current and let supervision handle the rest
              VPackBuilder ns;
              { VPackArrayBuilder a(&ns);
                if (s.isArray()) {
                  bool front = true;
                  for (auto const& i : VPackArrayIterator(s)) {
                    ns.add(VPackValue((!front) ? i.copyString() : UNDERSCORE + i.copyString()));
                    front = false;
                  }
                }}
              report.add(
                VPackValue(
                  CURRENT_COLLECTIONS + dbName + "/" + colName + "/" + shName
                  + "/" + SERVERS));

              { VPackObjectBuilder o(&report);
                report.add(OP, VP_SET);
                report.add("payload", ns.slice()); }
            }
          }
        }
      }
    }
  }

  // UpdateCurrentForDatabases
  auto cdbs = cur.get(DATABASES);
  for (auto const& database : VPackObjectIterator(cdbs)) {
    auto const dbName = database.key.copyString();
    if (!database.value.isObject()) {
      continue;
    }
    VPackSlice myEntry = database.value.get(serverId);
    if (!myEntry.isNone()) {

      // Database no longer in Plan and local
      if (!local.hasKey(dbName) && !pdbs.hasKey(dbName)) {
        // This covers the case that the database is neither in Local nor in Plan.
        // It remains to make sure an error is reported to Current if there is
        // a database in the Plan but not in Local
        report.add(VPackValue(CURRENT_DATABASES + dbName + "/" + serverId));
        { VPackObjectBuilder o(&report);
          report.add(OP, VP_DELETE); }
        // We delete all under /Current/Collections/<dbName>, it does not
        // hurt if every DBserver does this, since it is an idempotent
        // operation.
        report.add(VPackValue(CURRENT_COLLECTIONS + dbName));
        { VPackObjectBuilder o(&report);
          report.add(OP, VP_DELETE); }
      }
    }
  }

  // UpdateCurrentForCollections
  auto curcolls = cur.get(COLLECTIONS);
  for (auto const& database : VPackObjectIterator(curcolls)) {
    auto const dbName = database.key.copyString();

    // UpdateCurrentForCollections (Current/Collections/Collection)
    for (auto const& collection : VPackObjectIterator(database.value)) {
      auto const colName = collection.key.copyString();

      for (auto const& shard : VPackObjectIterator(collection.value)) {
        auto const shName = shard.key.copyString();

        // Shard in current and has servers
        if (shard.value.hasKey(SERVERS)) {
          auto servers = shard.value.get(SERVERS);

          if (servers.isArray() && servers.length() > 0    // servers in current
              && servers[0].copyString() == serverId       // we are leading
              && !local.hasKey(
                std::vector<std::string> {dbName, shName}) // no local collection
              && !shardMap.slice().hasKey(shName)) {               // no such shard in plan
            report.add(
              VPackValue(
                CURRENT_COLLECTIONS + dbName + "/" + colName + "/" + shName));
            { VPackObjectBuilder o(&report);
              report.add(OP, VP_DELETE); }
          }

        }
      }
    }
  }

  // Let's find database errors for databases which do not occur in Local
  // but in Plan:
  VPackSlice planDatabases = plan.get(DATABASES);
  VPackSlice curDatabases = cur.get(DATABASES);
  if (planDatabases.isObject() && curDatabases.isObject()) {
    for (auto const& p : allErrors.databases) {
      VPackSlice planDbEntry = planDatabases.get(p.first);
      VPackSlice curDbEntry = curDatabases.get(p.first);
      if (planDbEntry.isObject() && curDbEntry.isNone()) {
        // Need to create an error entry:
        report.add(VPackValue(CURRENT_DATABASES + p.first + "/" + serverId));
        { VPackObjectBuilder o(&report);
          report.add(OP, VP_SET);
          report.add(VPackSlice("payload"));
          { VPackObjectBuilder pp(&report);
            VPackSlice errs(static_cast<uint8_t const*>(p.second->data()));
            report.add(StaticStrings::Error, errs.get(StaticStrings::Error));
            report.add(StaticStrings::ErrorNum, errs.get(StaticStrings::ErrorNum));
            report.add(StaticStrings::ErrorMessage, errs.get(StaticStrings::ErrorMessage));
          }
        }
      }
    }
  }

  // Finally, let's find shard errors for shards which do not occur in
  // Local but in Plan, we need to make sure that these errors are reported
  // in Current:
  for (auto const& p : allErrors.shards) {
    // First split the key:
    std::string const& key = p.first;
    auto pos = key.find('/');
    TRI_ASSERT(pos != std::string::npos);
    std::string d = key.substr(0, pos);     // database
    auto pos2 = key.find('/', pos + 1);     // collection
    TRI_ASSERT(pos2 != std::string::npos);
    std::string c = key.substr(pos + 1, pos2);
    std::string s = key.substr(pos2 + 1);   // shard name

    // Now find out if the shard appears in the Plan but not in Local:
    VPackSlice inPlan = pdbs.get(std::vector<std::string>({d, c, "shards", s}));
    VPackSlice inLoc = local.get(std::vector<std::string>({d, s}));
    if (inPlan.isObject() && inLoc.isNone()) {
      VPackSlice inCur = curcolls.get(std::vector<std::string>({d, c, s}));
      VPackSlice theErr(static_cast<uint8_t const*>(p.second->data()));
      if (inCur.isNone() || !equivalent(theErr, inCur)) {
        report.add(
          VPackValue(CURRENT_COLLECTIONS + d + "/" + c + "/" + s));
            { VPackObjectBuilder o(&report);
              report.add(OP, VP_SET);
              report.add("payload", theErr); }
      }
    }
  }

  return result;

}


arangodb::Result arangodb::maintenance::syncReplicatedShardsWithLeaders(
  VPackSlice const& plan, VPackSlice const& current, VPackSlice const& local,
  std::string const& serverId, MaintenanceFeature& feature,
  std::vector<ActionDescription>& actions) {

  auto pdbs = plan.get(COLLECTIONS);
  auto cdbs = current.get(COLLECTIONS);

  for (auto const& pdb : VPackObjectIterator(pdbs)) {
    auto const& dbname = pdb.key.copyString();
    if (local.hasKey(dbname) && cdbs.hasKey(dbname)) {
      for (auto const& pcol : VPackObjectIterator(pdb.value)) {
        auto const& colname = pcol.key.copyString();
        if (cdbs.get(dbname).hasKey(colname)) {
          for (auto const& pshrd : VPackObjectIterator(pcol.value.get(SHARDS))) {
            auto const& shname = pshrd.key.copyString();

            // shard does not exist locally so nothing we can do at this point
            if (!local.hasKey(std::vector<std::string>{dbname, shname})) {
              continue;
            }

            // current stuff is created by the leader this one here will just
            // bring followers in sync so just continue here
            auto cpath = std::vector<std::string> {dbname, colname, shname};
            if (!cdbs.hasKey(cpath)) {
              LOG_TOPIC(DEBUG, Logger::MAINTENANCE) <<
                "Shard " << shname << " not in current yet. Rescheduling maintenance.";
              continue;
            }

            // Plan's servers
            auto ppath = std::vector<std::string> {dbname, colname, SHARDS, shname};
            if (!pdbs.hasKey(ppath)) {
              LOG_TOPIC(ERR, Logger::MAINTENANCE)
                << "Shard " << shname
                << " does not have servers substructure in 'Plan'";
              continue;
            }
            auto const& pservers = pdbs.get(ppath);

            // Current's servers
            cpath.push_back(SERVERS);
            if (!cdbs.hasKey(cpath)) {
              LOG_TOPIC(ERR, Logger::MAINTENANCE)
                << "Shard " << shname
                << " does not have servers substructure in 'Current'";
              continue;
            }
            auto const& cservers = cdbs.get(cpath);

            // we are not planned to be a follower
            if (indexOf(pservers, serverId) <= 0) {
              continue;
            }
            // if we are considered to be in sync there is nothing to do
            if (indexOf(cservers, serverId) > 0) {
              continue;
            }

            auto const leader = pservers[0].copyString();
            actions.emplace_back(
              ActionDescription(
                {{NAME, "SynchronizeShard"}, {DATABASE, dbname},
                 {COLLECTION, colname}, {SHARD, shname}, {THE_LEADER, leader},
                 {SHARD_VERSION, std::to_string(feature.shardVersion(shname))}}));

          }
        }
      }
    }
  }

  return Result();

}



/// @brief Phase two: See, what we can report to the agency
arangodb::Result arangodb::maintenance::phaseTwo (
  VPackSlice const& plan, VPackSlice const& cur, VPackSlice const& local,
  std::string const& serverId, MaintenanceFeature& feature, VPackBuilder& report) {

  MaintenanceFeature::errors_t allErrors;
  feature.copyAllErrors(allErrors);

  arangodb::Result result;

  report.add(VPackValue(PHASE_TWO));
  { VPackObjectBuilder p2(&report);

    // agency transactions
    report.add(VPackValue("agency"));
    { VPackObjectBuilder agency(&report);
    // Update Current
      try {
        result = reportInCurrent(plan, cur, local, allErrors, serverId, report);
      } catch (std::exception const& e) {
        LOG_TOPIC(ERR, Logger::MAINTENANCE)
          << "Error reporting in current: " << e.what() << ". "
          << __FILE__ << ":" << __LINE__;
      }}

    // maintenace actions
    report.add(VPackValue("actions"));
    { VPackObjectBuilder agency(&report);
      try {
        std::vector<ActionDescription> actions;
        result = syncReplicatedShardsWithLeaders(
          plan, cur, local, serverId, feature, actions);

        for (auto const& action : actions) {
          feature.addAction(std::make_shared<ActionDescription>(action), false);
        }
      } catch (std::exception const& e) {
        LOG_TOPIC(ERR, Logger::MAINTENANCE)
          << "Error scheduling shards: " << e.what() << ". "
          << __FILE__ << ":" << __LINE__;
      }}

  }

  report.add(VPackValue("Current"));
  { VPackObjectBuilder p(&report);
    report.add("Version", cur.get("Version")); }

  return result;

}

