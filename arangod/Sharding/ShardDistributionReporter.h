////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_CLUSTER_SHARD_DISTRIBUTED_REPORTER_H
#define ARANGOD_CLUSTER_SHARD_DISTRIBUTED_REPORTER_H 1

#include "Basics/Common.h"
#include "Network/Methods.h"

#include <queue>

namespace arangodb {

class ClusterInfo;

namespace velocypack {
class Builder;
}

class LogicalCollection;

namespace cluster {

class ShardDistributionReporter {
 public:
  ShardDistributionReporter(ClusterInfo* ci, network::Sender sender);

  ~ShardDistributionReporter();
  
  /// @brief helper function to create an instance of the ShardDistributionReporter
  static std::shared_ptr<ShardDistributionReporter> instance(application_features::ApplicationServer&);


  /// @brief fetch distribution for a single collections in db
  void getCollectionDistributionForDatabase(std::string const& dbName,
                                            std::string const& colName,
                                            arangodb::velocypack::Builder& result);

  /// @brief fetch distributions for all collections in db
  void getDistributionForDatabase(std::string const& dbName,
                                  arangodb::velocypack::Builder& result);

 private:
  /// @brief internal helper function to fetch distributions
  void getCollectionDistribution(
      std::string const& dbName, 
      std::vector<std::shared_ptr<LogicalCollection>> const& cols, 
      arangodb::velocypack::Builder& result,
      bool progress);

  bool testAllShardsInSync(std::string const& dbName, LogicalCollection const* col,
                           std::unordered_map<std::string, std::vector<std::string>> const* allShards);

  void helperDistributionForDatabase(
      std::string const& dbName, arangodb::velocypack::Builder& result,
      std::queue<std::shared_ptr<LogicalCollection>>& todoSyncStateCheck, double endtime,
      std::unordered_map<std::string, std::string>& aliases, bool progress);

 private:
  ClusterInfo* _ci;
  network::Sender _send;
};
}  // namespace cluster
}  // namespace arangodb

#endif
