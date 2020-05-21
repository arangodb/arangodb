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
#include "Futures/Promise.h"
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

  /// @brief Get velocypack from node downward. AgencyCommHelper::path is prepended
  consensus::query_t const dump() const;

  /// @brief Get velocypack from node downward. AgencyCommHelper::path is prepended
  std::tuple <consensus::query_t, consensus::index_t> const get(
    std::string const& path = std::string("/")) const;

  /// @brief Get velocypack from node downward
  std::tuple <consensus::query_t, consensus::index_t> const read(
    std::vector<std::string> const& paths) const;

  /// @brief Get current commit index
  consensus::index_t index() const;

  /// @brief Register local callback
  bool registerCallback(std::string const& key, uint64_t const& id);

  /// @brief Register local callback
  bool unregisterCallback(std::string const& key, uint64_t const& id);

  /// @brief Wait to be notified, when a Raft index has arrived.
  futures::Future<Result> waitFor(consensus::index_t index);

  /// @brief Cache has these path? AgencyCommHelper::path is prepended
  bool has(std::string const& path) const;

  /// @brief Cache has these path? Paths are absolute
  std::vector<bool> has(std::vector<std::string> const& paths) const;

  /// @brief Used exclusively in unit tests
  consensus::check_ret_t set(VPackSlice const trx);

  /// @brief Used exclusively in unit tests
  consensus::Store& store();

private:

  /// @brief invoke all callbacks
  void invokeAllCallbacks() const;

  /// @brief invoke given callbacks
  void invokeCallbacks(std::vector<uint64_t> const&) const;

  /// @brief invoke given callbacks
  void invokeCallbackNoLock(uint64_t, std::string const& = std::string()) const;

  /// @brief handle callbacks for specific log document
  void handleCallbacksNoLock(VPackSlice, std::unordered_set<uint64_t>&, std::vector<uint64_t>&);

  /// @brief trigger all waiting call backs for index <= _commitIndex
  ///        caller must hold lock
  void triggerWaiting(consensus::index_t commitIndex);

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
  std::multimap<std::string, uint64_t> _callbacks;

  /// @brief Waiting room for indexes during office hours
  mutable std::mutex _waitLock;
  std::multimap<consensus::index_t, futures::Promise<arangodb::Result>> _waiting;
};

} // namespace

#endif
