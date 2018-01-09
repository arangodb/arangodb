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
#include "Agency/Node.h"

namespace arangodb {

class LogicalCollection;

namespace maintenance {

using LocalState = std::map<std::string,std::vector<LogicalCollection*>>;
using AgencyState = arangodb::consensus::Node;


static std::string const PLANNED_DATABASES("/arango/Plan/Databases");

arangodb::Result diffPlanLocalForDatabases(
  AgencyState const&, std::vector<std::string> const&,
  std::vector<std::string>&, std::vector<std::string>&);

arangodb::Result executePlanForDatabases (
  AgencyState const&, AgencyState const&, LocalState const&);

arangodb::Result diffPlanLocalForCollections(
  AgencyState const&, LocalState const&,
  std::vector<std::string>&, std::vector<std::string>&,
  std::vector<std::string>&);

arangodb::Result executePlanForCollections (
  AgencyState const&, AgencyState const&, LocalState const&);

arangodb::Result synchroniseShards (
  AgencyState const&, AgencyState const&, LocalState const&);

arangodb::Result reportLocalForDatabases (
  AgencyState plan, AgencyState current,
  std::vector<std::string>);

arangodb::Result phaseOne (
  AgencyState const& plan, AgencyState const& cur, LocalState const& local);

arangodb::Result phaseTwo (
  AgencyState const& plan, AgencyState const& cur);

}}

#endif

