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

#ifndef ARANGODB_MAINTENANCE_MAINTENANCE_H
#define ARANGODB_MAINTENANCE_MAINTENANCE_H

#include "Basics/Result.h"
#include "Basics/VelocyPackHelper.h"
#include "Agency/Node.h"
#include "Cluster/MaintenanceFeature.h"

namespace arangodb {

class LogicalCollection;

namespace maintenance {

using Transactions = std::vector<std::pair<VPackBuilder,VPackBuilder>>;

arangodb::Result diffPlanLocalForDatabases(
  VPackSlice const&, std::vector<std::string> const&,
  std::vector<std::string>&, std::vector<std::string>&);


/**
 * @brief          Difference Plan and local for phase 1 of Maintenance run 
 *
 * @param plan     Snapshot of agency's planned state
 * @param local    Snapshot of local state
 * @param serverId This server's UUID
 * @param actions  Resulting actions from difference are packed in here
 *
 * @return         Result
 */
arangodb::Result diffPlanLocal(
  VPackSlice const& plan, VPackSlice const& local, std::string const& serverId,
  std::vector<ActionDescription>& actions);

/**
 * @brief          Difference Plan and local for phase 1 of Maintenance run 
 *
 * @param plan     Snapshot of agency's planned state
 * @param current  Snapshot of agency's current state
 * @param local    Snapshot of local state
 * @param serverId This server's UUID
 * @param feature  Maintenance featire
 *
 * @return         Result
 */
arangodb::Result executePlan (
  VPackSlice const& plan, VPackSlice const& current, VPackSlice const& local,
  std::string const& serverId, arangodb::MaintenanceFeature& feature);

/**
 * @brief          Difference local and current states for phase 2 of Maintenance 
 *
 * @param local    Snapshot of local state
 * @param current  Snapshot of agency's current state
 * @param serverId This server's UUID
 * @param report   Resulting agency transaction, which is to be sent
 *
 * @return         Result
 */
arangodb::Result diffLocalCurrent (
  VPackSlice const& local, VPackSlice const& current,
  std::string const& serverId, Transactions& report);

/**
 * @brief          Synchronise Shards
 *
 * @param local    Snapshot of local state
 * @param current  Snapshot of agency's current state
 * @param serverId This server's UUID
 * @param report   Resulting agency transaction, which is to be sent
 *
 * @return         Result
 */
arangodb::Result synchroniseShards (
  VPackSlice const& plan, VPackSlice const& current, VPackSlice const& local);


/**
 * @brief         Execute both phases and report
 *
 * @param report  Reference to report
 * @return        Success
 */
arangodb::Result handleChange(
  VPackSlice const& plan, VPackSlice const& cur, VPackSlice const& local,
  std::string const& serverId, MaintenanceFeature& feature, VPackBuilder& report);

/**
 * @brief          Phase one: Execute plan, shard replication startups
 *
 * @param plan     Snapshot of agency's planned state
 * @param current  Snapshot of agency's current state
 * @param local    Snapshot of local state
 * @param serverId This server's UUID
 * @param report   Resulting agency transaction, which is to be sent
 * @param feature  Maintenance feature
 *
 * @return         Result
 */
arangodb::Result phaseOne (
  VPackSlice const& plan, VPackSlice const& cur, VPackSlice const& local,
  std::string const& serverId, MaintenanceFeature& feature, VPackBuilder& report);

/**
 * @brief          Phase two: Report in agency
 *
 * @param plan     Snapshot of agency's planned state
 * @param current  Snapshot of agency's current state
 * @param local    Snapshot of local state
 * @param serverId This server's UUID
 * @param report   Report on what we did
 *
 * @return         Result
 */
arangodb::Result phaseTwo (
  VPackSlice const& plan, VPackSlice const& cur, VPackSlice const& local,
  std::string const& serverId, VPackBuilder& report);


/**
 * @brief          Report local changes to current
 *
 * @param plan     Snapshot of agency's planned state
 * @param current  Snapshot of agency's current state
 * @param local    Snapshot of local state
 * @param serverId This server's UUID
 * @param report   Report on what we did
 *
 * @return         Result
 */
arangodb::Result reportInCurrent(
  VPackSlice const& plan, VPackSlice const& cur, VPackSlice const& local,
  std::string const& serverId, VPackBuilder& report); 

}}

#endif

