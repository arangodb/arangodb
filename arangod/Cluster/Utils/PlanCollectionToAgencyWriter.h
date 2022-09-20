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

#pragma once

#include "Cluster/Utils/PlanCollectionEntry.h"

namespace arangodb {

class AgencyOperation;
struct PlanCollection;
struct ShardDistribution;

struct PlanCollectionToAgencyWriter {
  explicit PlanCollectionToAgencyWriter(PlanCollection col,
                                        ShardDistribution shardDistribution);

  [[nodiscard]] AgencyOperation prepareOperation(
      std::string const& databaseName) const;

 private:
  // Information required for the collection to write
  PlanCollectionEntry _entry;
};

}  // namespace arangodb
