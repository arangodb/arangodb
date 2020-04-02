////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_CLUSTER_MAINTENANCE_FEATURE
#define ARANGOD_CLUSTER_MAINTENANCE_FEATURE 1

#include "Agency/Store.h"
#include "Cluster/ClusterFeature.h"
#include "GeneralServer/RestHandler.h"

namespace arangodb {

class AgencyCache : public arangodb::Thread,
                    public std::enable_shared_from_this<AgencyCache> {

public:
  /// @brief start off with our server
  explicit AgencyCache(application_features::ApplicationServer& server);

  /// @brief Clean up
  ~AgencyCache();

  /// @brief 1. Long poll from agency's Raft log
  ///        2. Entertain local cache of agency's read db
  void run() override final;

  /// @brief Start orderly shutdown of threads
  // cppcheck-suppress virtualCallInConstructor
  void beginShutdown() override final;

  /// @brief Read from local readDB cache at path
  arangodb::consensus::Node const& read(std::string const& path) const;

  /// @brief Get velocypack from node downward
  std::tuple <consensus::query_t, consensus::index_t> const get(
    std::string const& path = std::string("/")) const;

  /// @brief Get current commit index
  consensus::index_t index() const;

private:


  /// @brief Guard for _readDB
  mutable std::shared_mutex _lock;

  /// @brief Commit index
  consensus::index_t _commitIndex;

  /// @brief Local copy of the read DB from the agency
  arangodb::consensus::Store _readDB;

};

} // namespace

#endif
