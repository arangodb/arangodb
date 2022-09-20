////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2022-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "PlanCollectionToAgencyWriter.h"
#include "Agency/AgencyComm.h"
#include "Agency/AgencyPaths.h"
#include "Inspection/VPack.h"
#include "VocBase/Properties/PlanCollection.h"

#include "Logger/LogMacros.h"

using namespace arangodb;

PlanCollectionToAgencyWriter::PlanCollectionToAgencyWriter(
    PlanCollection col, ShardDistribution shardDistribution)
    : _entry(std::move(col), std::move(shardDistribution)) {}

[[nodiscard]] AgencyOperation PlanCollectionToAgencyWriter::prepareOperation(
    std::string const& databaseName) const {
  auto const collectionPath = cluster::paths::root()
                                  ->arango()
                                  ->plan()
                                  ->collections()
                                  ->database(databaseName)
                                  ->collection(_entry.getCID());
  // TODO: This is temporary
  auto builder = std::make_shared<VPackBuilder>(_entry.toVPackDeprecated());
  // velocypack::serialize(*builder, _entry);
  return AgencyOperation{collectionPath, AgencyValueOperationType::SET,
                         std::move(builder)};
}
