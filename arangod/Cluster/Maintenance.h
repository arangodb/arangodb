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

namespace arangodb {

class LogicalCollection;

namespace maintenance {

using Transactions = std::vector<std::pair<VPackBuilder,VPackBuilder>>;

arangodb::Result diffPlanLocalForDatabases(
  VPackSlice const&, std::vector<std::string> const&,
  std::vector<std::string>&, std::vector<std::string>&);

/// @brief calculate difference between plan and local for dbs/cols
///        come up with list of actions, which need to be performed
arangodb::Result diffPlanLocal(
  VPackSlice const&, VPackSlice const&, std::string const& serverId,
  std::vector<ActionDescription>&);

arangodb::Result executePlan (
  VPackSlice const&, VPackSlice const&, VPackSlice const&, std::string const&);

arangodb::Result diffLocalCurrent (
  VPackSlice const& local, VPackSlice const& current,
  std::string const& serverId, Transactions& report);

arangodb::Result synchroniseShards (
  VPackSlice const&, VPackSlice const&, VPackSlice const&);

arangodb::Result phaseOne (
  VPackSlice const& plan, VPackSlice const& cur, VPackSlice const& local,
  std::string const&);

arangodb::Result phaseTwo (
  VPackSlice const& plan, VPackSlice const& cur);

}}

#endif

