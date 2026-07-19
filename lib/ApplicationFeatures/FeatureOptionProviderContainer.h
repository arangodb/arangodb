////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "Agency/AgencyOptionsProvider.h"
#include "Cluster/ClusterOptionsProvider.h"
#include "Cluster/ClusterUpgradeOptionsProvider.h"
#include "Cluster/MaintenanceOptionsProvider.h"
#include "Cluster/ReplicationTimeoutOptionsProvider.h"
#include "ProgramOptions/ProgramOptions.h"
#include "Replication/ReplicationOptionsProvider.h"
#include "Replication2/ReplicatedLog/ReplicatedLogOptionsProvider.h"
#include "RestServer/BootstrapOptionsProvider.h"
#include "RestServer/DatabasePathOptionsProvider.h"
#include "RestServer/DumpLimitsOptionsProvider.h"
#include "RestServer/FlushOptionsProvider.h"
#include "RestServer/FortuneOptionsProvider.h"
#include "RestServer/PrivilegeOptionsProvider.h"
#include "RestServer/TemporaryStorageOptionsProvider.h"
#include "RestServer/TtlOptionsProvider.h"
#include "RocksDBEngine/RocksDBEngineOptionsProvider.h"
#include "RocksDBEngine/RocksDBIndexCacheRefillOptionsProvider.h"
#include "RocksDBEngine/RocksDBOptionFeatureOptionsProvider.h"
#include "Statistics/StatisticsOptionsProvider.h"
#include "Transaction/ManagerOptionsProvider.h"

#include <tuple>

namespace arangodb::application_features {
class FeatureOptionProviderContainer final {
 public:
  void declareOptions(std::shared_ptr<options::ProgramOptions> programOptions);
  void validateOptions(std::shared_ptr<options::ProgramOptions> programOptions);

  template<typename ProviderType>
  auto& getOptions() const {
    return std::get<ProviderType>(_providers).options();
  }

 private:
  std::tuple<
      AgencyOptionsProvider, bootstrap::BootstrapOptionsProvider,
      ClusterOptionsProvider, upgrade::ClusterUpgradeOptionsProvider,
      DatabasePathOptionsProvider, DumpLimitsOptionsProvider,
      FlushOptionsProvider, fortune::FortuneOptionsProvider,
      MaintenanceOptionsProvider, replication2::ReplicatedLogOptionsProvider,
      ReplicationOptionsProvider, ReplicationTimeoutOptionsProvider,
      RocksDBEngineOptionsProvider, RocksDBIndexCacheRefillOptionsProvider,
      RocksDBOptionFeatureOptionsProvider, PrivilegeOptionsProvider,
      statistics::StatisticsOptionsProvider, TemporaryStorageOptionsProvider,
      transaction::ManagerOptionsProvider, TtlOptionsProvider>
      _providers{};
};
}  // namespace arangodb::application_features
