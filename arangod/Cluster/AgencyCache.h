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

#ifndef ARANGOD_CLUSTER_AGENCY_CACHE
#define ARANGOD_CLUSTER_AGENCY_CACHE 1

#include "Agency/Store.h"
#include "Basics/Thread.h"
#include "Cluster/AgencyCallbackRegistry.h"
#include "Cluster/ClusterFeature.h"
#include "GeneralServer/RestHandler.h"

#include <map>
#include <mutex>

namespace arangodb {

class AgencyCache final : public arangodb::Thread {

public:
  /// @brief start off with our server
  explicit AgencyCache(
    application_features::ApplicationServer& server,
    AgencyCallbackRegistry& callbackRegistry);

  /// @brief Clean up
  virtual ~AgencyCache();

  /// @brief 1. Long poll from agency's Raft log
  ///        2. Entertain local cache of agency's read db
  void run() override;

  /// @brief Start thread
  bool start();

  /// @brief Start orderly shutdown of threads
  // cppcheck-suppress virtualCallInConstructor
  void beginShutdown() override;

  /// @brief Read from local readDB cache at path
  arangodb::consensus::Node const& read(std::string const& path) const;

  /// @brief Get velocypack from node downward
  std::tuple <consensus::query_t, consensus::index_t> const get(
    std::string const& path = std::string("/")) const;

  /// @brief Get velocypack from node downward
  std::tuple <consensus::query_t, consensus::index_t> const get(
    std::vector<std::string> const& paths) const;

  /// @brief Get current commit index
  consensus::index_t index() const;

  /// @brief Register local callback
  bool registerCallback(std::string const& key, uint32_t const& id);

  /// @brief Register local callback
  bool unregisterCallback(std::string const& key, uint32_t const& id);

private:

  /// @brief Guard for _readDB
  mutable std::mutex _storeLock;

  /// @brief Commit index
  consensus::index_t _commitIndex;

  /// @brief Local copy of the read DB from the agency
  arangodb::consensus::Store _readDB;

  /// @brief Agency callback registry
  AgencyCallbackRegistry& _callbackRegistry;

  /// @brief Stored call backs key -> callback registry's id
  mutable std::mutex _callbacksLock;
  std::multimap<std::string, uint32_t> _callbacks;
};

} // namespace

#endif
