////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////
#include "Agency/TransactionBuilder.h"
#include "Basics/StringUtils.h"
#include "Cluster/Utils/EvenDistribution.h"
#include "CollectionGroupSupervision.h"
#include "Replication2/AgencyCollectionSpecification.h"
#include "Replication2/AgencyCollectionSpecificationInspectors.h"
#include "Replication2/ReplicatedLog/AgencyLogSpecification.h"
#include "Replication2/ReplicatedLog/AgencySpecificationInspectors.h"
#include "Replication2/ReplicatedLog/ParticipantsHealth.h"
#include "Replication2/StateMachines/Document/DocumentStateMachine.h"
#include <random>

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::document::supervision;
namespace ag = arangodb::replication2::agency;
namespace {

auto checkReplicatedLogConverged(ag::Log const& log) -> bool {
  if (not log.current or not log.current->supervision) {
    return false;
  }

  return log.current->supervision->targetVersion == log.target.version;
}

auto createLogConfigFromGroupAttributes(
    ag::CollectionGroupTargetSpecification::Attributes const& attributes) {
  return ag::LogTargetConfig{attributes.mutableAttributes.writeConcern,
                             attributes.mutableAttributes.replicationFactor,
                             attributes.mutableAttributes.waitForSync};
}

auto getHealthyParticipants(replicated_log::ParticipantsHealth const& health)
    -> std::vector<ParticipantId> {
  std::vector<ParticipantId> servers;

  for (auto const& [p, h] : health._health) {
    if (h.notIsFailed) {
      servers.push_back(p);
    }
  }
  return servers;
}

auto computeEvenDistributionForServers(
    std::size_t numberOfShards, std::size_t replicationFactor,
    replicated_log::ParticipantsHealth const& health) -> EvenDistribution {
  auto servers = getHealthyParticipants(health);
  {
    // TODO reuse random device?
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(servers.begin(), servers.end(), g);
  }

  EvenDistribution distribution{numberOfShards, replicationFactor, {}, false};
  std::unordered_set<ParticipantId> plannedServers;
  distribution.planShardsOnServers(servers, plannedServers);
  return distribution;
}

auto getReplicatedLogLeader(ag::Log const& log)
    -> std::optional<ParticipantId> {
  if (log.plan and log.plan->currentTerm and log.plan->currentTerm->leader) {
    return log.plan->currentTerm->leader->serverId;
  }
  return std::nullopt;
}

auto computeShardList(
    std::unordered_map<LogId, ag::Log> const& logs,
    std::vector<ag::CollectionGroupPlanSpecification::ShardSheaf> shardSheaves,
    std::vector<ShardID> const& shards) -> PlanShardToServerMapping {
  ADB_PROD_ASSERT(logs.size() == shards.size())
      << "logs.size = " << logs.size() << " shards.size = " << shards.size();
  PlanShardToServerMapping mapping;
  for (std::size_t i = 0; i < shards.size(); i++) {
    ResponsibleServerList servers;
    auto const& log = logs.at(shardSheaves[i].replicatedLog);
    for (auto const& [pid, flags] : log.target.participants) {
      servers.servers.push_back(pid);
    }

    auto leader = getReplicatedLogLeader(log);

    // sort by name, but leader in front
    std::sort(servers.servers.begin(), servers.servers.end(),
              [&](auto const& left, auto const& right) {
                // returns true if left < right
                if (left == leader) {
                  return true;
                }
                return left < right;
              });
    mapping.shards[shards[i]] = std::move(servers);
  }

  return mapping;
}

auto createCollectionPlanSpec(
    ag::CollectionGroupTargetSpecification const& target,
    std::vector<ag::CollectionGroupPlanSpecification::ShardSheaf> const&
        shardSheaves,
    ag::CollectionTargetSpecification const& collection,
    std::unordered_map<LogId, ag::Log> const& logs, UniqueIdProvider& uniqid)
    -> ag::CollectionPlanSpecification {
  std::vector<ShardID> shardList;
  std::generate_n(std::back_inserter(shardList),
                  target.attributes.immutableAttributes.numberOfShards, [&] {
                    return basics::StringUtils::concatT("s", uniqid.next());
                  });

  auto mapping = computeShardList(logs, shardSheaves, shardList);
  return ag::CollectionPlanSpecification{collection, std::move(shardList),
                                         std::move(mapping)};
}

auto createCollectionGroupTarget(
    DatabaseID const& database, CollectionGroup const& group,
    UniqueIdProvider& uniqid, replicated_log::ParticipantsHealth const& health)
    -> AddCollectionGroupToPlan {
  auto const& attributes = group.target.attributes;

  auto distribution = computeEvenDistributionForServers(
      attributes.immutableAttributes.numberOfShards,
      attributes.mutableAttributes.replicationFactor, health);

  std::size_t i = 0;
  std::unordered_map<LogId, ag::LogTarget> replicatedLogs;
  for (std::size_t k = 0;
       k < group.target.attributes.immutableAttributes.numberOfShards; k++) {
    // TODO fill core parameters here

    ag::LogTarget target;
    target.id = LogId{uniqid.next()};
    target.version = 1;
    target.config = createLogConfigFromGroupAttributes(group.target.attributes);
    target.properties.implementation.type = "document";

    auto participants = distribution.getServersForShardIndex(i++);
    target.leader = participants.getLeader();
    for (auto const& p : participants.servers) {
      target.participants.emplace(p, ParticipantFlags{});
    }

    replicatedLogs[target.id] = std::move(target);
  }

  ag::CollectionGroupPlanSpecification spec;
  spec.attributes = group.target.attributes;
  spec.id = group.target.id;

  std::transform(
      replicatedLogs.begin(), replicatedLogs.end(),
      std::back_inserter(spec.shardSheaves), [](auto const& target) {
        return ag::CollectionGroupPlanSpecification::ShardSheaf{target.first};
      });

  // TODO hack - remove this once we have more shards per collection group
  std::unordered_map<CollectionID, ag::CollectionPlanSpecification> collections;
  for (auto const& [cid, collection] : group.target.collections) {
    std::vector<ShardID> shardList;
    std::generate_n(std::back_inserter(shardList),
                    attributes.immutableAttributes.numberOfShards, [&] {
                      return basics::StringUtils::concatT("s", uniqid.next());
                    });

    collections[cid] = ag::CollectionPlanSpecification{
        group.targetCollections.at(cid), std::move(shardList), {}};
    spec.collections[cid];
  }
  int j = 0;
  for (auto& sheafId : spec.shardSheaves) {
    auto& log = replicatedLogs[sheafId.replicatedLog];
    replicated_state::document::DocumentCoreParameters parameters;
    parameters.databaseName = database;
    parameters.collectionId = collections.begin()->first;
    parameters.shardId = collections.begin()->second.shardList[j];

    j++;
    log.properties.implementation.parameters =
        velocypack::serialize(parameters);
  }

  return AddCollectionGroupToPlan{std::move(spec), std::move(replicatedLogs),
                                  std::move(collections)};
}

auto checkAssociatedReplicatedLogs(
    ag::CollectionGroupTargetSpecification const& target,
    ag::CollectionGroupPlanSpecification const& plan,
    std::unordered_map<LogId, ag::Log> const& logs,
    replicated_log::ParticipantsHealth const& health) -> Action {
  // check if replicated logs require an update
  ADB_PROD_ASSERT(plan.shardSheaves.size() ==
                  target.attributes.immutableAttributes.numberOfShards);
  for (auto const& sheaf : plan.shardSheaves) {
    auto const& log = logs.at(sheaf.replicatedLog);
    auto const& wantedConfig =
        createLogConfigFromGroupAttributes(target.attributes);

    if (log.target.config != wantedConfig) {
      // we have to update this replicated log
      return UpdateReplicatedLogConfig{sheaf.replicatedLog, wantedConfig};
    }

    auto expectedReplicationFactor =
        target.attributes.mutableAttributes.replicationFactor;
    auto currentReplicationFactor = log.target.participants.size();
    if (currentReplicationFactor < expectedReplicationFactor) {
      // add a new server to the replicated log
      // find a server not yet used

      // TODO account for CleanedOutServer, ToBeCleanedOutServers
      auto servers = getHealthyParticipants(health);
      std::erase_if(servers, [&](auto const& server) {
        return log.target.participants.contains(server);
      });

      // TODO pick random server
      if (not servers.empty()) {
        return AddParticipantToLog{log.target.id, servers.front()};
      } else {
        // TODO add report that no server is available
      }
    } else if (currentReplicationFactor > expectedReplicationFactor) {
      // TODO prefer an unhealthy participant over a healthy one
      //  do not pick the current leader
      ADB_PROD_ASSERT(log.target.participants.size() > 1);
      auto const& server = log.target.participants.begin()->first;
      return RemoveParticipantFromLog{log.target.id, server};
    }

    // TODO check add new server
    //  if server failed
  }

  return NoActionRequired{};
}

auto checkCollectionGroupConverged(CollectionGroup const& group) -> Action {
  if (not group.current or
      group.current->supervision.version != group.target.version) {
    // check is all replicated logs have converged
    for (auto const& [id, log] : group.logs) {
      if (not checkReplicatedLogConverged(log)) {
        return NoActionPossible{basics::StringUtils::concatT(
            "replicated log ", id, " not yet converged.")};
      }
    }

    return UpdateConvergedVersion{group.target.version};
  }

  return NoActionRequired{};
}
}  // namespace

auto document::supervision::checkCollectionGroup(
    DatabaseID const& database, CollectionGroup const& group,
    UniqueIdProvider& uniqid, replicated_log::ParticipantsHealth const& health)
    -> Action {
  if (not group.plan.has_value()) {
    // create collection group in plan
    return createCollectionGroupTarget(database, group, uniqid, health);
  }

  // check replicated logs
  if (auto action = checkAssociatedReplicatedLogs(group.target, *group.plan,
                                                  group.logs, health);
      not std::holds_alternative<NoActionRequired>(action)) {
    return action;
  }

  // check that every collection in target is in plan
  for (auto const& [cid, collection] : group.targetCollections) {
    ADB_PROD_ASSERT(group.target.collections.contains(cid));

    if (not group.planCollections.contains(cid)) {
      ADB_PROD_ASSERT(not group.plan->collections.contains(cid));
      auto spec =
          createCollectionPlanSpec(group.target, group.plan->shardSheaves,
                                   collection, group.logs, uniqid);
      return AddCollectionToPlan{cid, std::move(spec)};
    }
  }

  // check that every collection in plan is in target
  for (auto const& [cid, collection] : group.planCollections) {
    ADB_PROD_ASSERT(group.plan->collections.contains(cid));

    auto iter = group.targetCollections.find(cid);
    if (iter == group.targetCollections.end()) {
      return DropCollectionPlan{cid};
    }

    // TODO compare mutable properties and update if necessary

    // TODO remove deprecatedShardMap comparison
    auto expectedShardMap = computeShardList(
        group.logs, group.plan->shardSheaves, collection.shardList);
    if (collection.deprecatedShardMap.shards != expectedShardMap.shards) {
      return UpdateCollectionShardMap{cid, expectedShardMap};
    }
  }

  if (auto action = checkCollectionGroupConverged(group);
      not std::holds_alternative<NoActionRequired>(action)) {
    return action;
  }

  return NoActionRequired{};
}

namespace {
struct TransactionBuilder {
  ag::CollectionGroupId gid;
  DatabaseID const& database;
  arangodb::agency::envelope env;

  template<typename T>
  void operator()(T const&) {
    LOG_DEVEL << typeid(T).name() << " not handled";
  }

  // TODO do we need preconditions here?
  // TODO use agency paths

  void operator()(UpdateReplicatedLogConfig const& action) {
    env = env.write()
              .emplace_object(basics::StringUtils::concatT(
                                  "/arango/Target/ReplicatedLogs/", database,
                                  "/", action.logId, "/config"),
                              [&](VPackBuilder& builder) {
                                velocypack::serialize(builder, action.config);
                              })
              .end();
  }

  void operator()(UpdateConvergedVersion const& action) {
    env = env.write()
              .emplace_object(basics::StringUtils::concatT(
                                  "/arango/Current/CollectionGroups/", database,
                                  "/", gid.id(), "/supervision/targetVersion"),
                              [&](VPackBuilder& builder) {
                                velocypack::serialize(builder, action.version);
                              })
              .end();
  }

  void operator()(DropCollectionPlan const& action) {
    env = env.write()
              .remove(basics::StringUtils::concatT("/arango/Plan/Collections/",
                                                   database, "/", action.cid))
              .remove(basics::StringUtils::concatT(
                  "/arango/Plan/CollectionGroups/", database, "/", gid.id(),
                  "/collections/", action.cid))
              .inc("/arango/Plan/Version")
              .end();
  }

  void operator()(AddCollectionToPlan const& action) {
    env = env.write()
              .emplace_object(
                  basics::StringUtils::concatT("/arango/Plan/Collections/",
                                               database, "/", action.cid),
                  [&](VPackBuilder& builder) {
                    velocypack::serialize(builder, action.spec);
                  })
              .key(basics::StringUtils::concatT(
                       "/arango/Plan/CollectionGroups/", database, "/",
                       gid.id(), "/collections/", action.cid),
                   VPackSlice::emptyObjectSlice())
              .inc("/arango/Plan/Version")
              .end();
  }

  void operator()(UpdateCollectionShardMap const& action) {
    env = env.write()
              .emplace_object(
                  basics::StringUtils::concatT("/arango/Plan/Collections/",
                                               database, "/", action.cid,
                                               "/shards"),
                  [&](VPackBuilder& builder) {
                    velocypack::serialize(builder, action.mapping.shards);
                  })
              .inc("/arango/Plan/Version")
              .end();
  }

  void operator()(AddParticipantToLog const& action) {
    env = env.write()
              .key(basics::StringUtils::concatT(
                       "/arango/Target/ReplicatedLogs/", database, "/",
                       action.logId, "/participants/", action.participant),
                   VPackSlice::emptyObjectSlice())
              .end();
  }

  void operator()(RemoveParticipantFromLog const& action) {
    env = env.write()
              .remove(basics::StringUtils::concatT(
                  "/arango/Target/ReplicatedLogs/", database, "/", action.logId,
                  "/participants/", action.participant))
              .end();
  }

  void operator()(AddCollectionGroupToPlan const& action) {
    auto write = env.write().emplace_object(
        basics::StringUtils::concatT("/arango/Plan/CollectionGroups/", database,
                                     "/", action.spec.id.id()),
        [&](VPackBuilder& builder) {
          velocypack::serialize(builder, action.spec);
        });

    for (auto const& [id, spec] : action.sheaves) {
      write = write.emplace_object(
          basics::StringUtils::concatT("/arango/Target/ReplicatedLogs/",
                                       database, "/", id),
          [&spec = spec](VPackBuilder& builder) {
            velocypack::serialize(builder, spec);
          });
    }

    for (auto const& [cid, spec] : action.collections) {
      write = write.emplace_object(
          basics::StringUtils::concatT("/arango/Plan/Collections/", database,
                                       "/", cid),
          [&spec = spec](VPackBuilder& builder) {
            velocypack::serialize(builder, spec);
          });
    }
    env = write.end();
  }

  void operator()(NoActionRequired const&) {}
  void operator()(NoActionPossible const&) {}
};
}  // namespace

auto document::supervision::executeCheckCollectionGroup(
    DatabaseID const& database, std::string const& logIdString,
    CollectionGroup const& group,
    replicated_log::ParticipantsHealth const& health, UniqueIdProvider& uniqid,
    arangodb::agency::envelope envelope) noexcept
    -> arangodb::agency::envelope {
  // TODO add agency reports, add last-time-updated

  auto action = checkCollectionGroup(database, group, uniqid, health);

  if (std::holds_alternative<NoActionRequired>(action)) {
    return envelope;
  } else if (std::holds_alternative<NoActionPossible>(action)) {
    // TODO remove logging later?
    LOG_TOPIC("33547", WARN, Logger::SUPERVISION)
        << "no progress possible for collection group " << database << "/"
        << group.target.id;
    return envelope;
  }

  TransactionBuilder builder{group.target.id, database, std::move(envelope)};
  std::visit(builder, action);
  return std::move(builder.env);
}
