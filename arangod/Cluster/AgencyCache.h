////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
#include <shared_mutex>

namespace arangodb {

class AgencyCache final : public arangodb::Thread {

public:

  typedef std::unordered_map<std::string, consensus::query_t> databases_t;

  struct change_set_t {
    consensus::index_t ind;  // Raft index
    uint64_t version;        // Plan / Current version
    databases_t dbs; // touched databases
    consensus::query_t rest; // Plan / Current rest
    change_set_t (consensus::index_t const& i, uint64_t const& v, databases_t const& d,
                  consensus::query_t const& r) :
      ind(i), version(v), dbs(d), rest(r) {}
    change_set_t (consensus::index_t&& i, uint64_t&& v, databases_t&& d, consensus::query_t&& r) :
      ind(std::move(i)), version(std::move(v)), dbs(std::move(d)), rest(std::move(r)) {}
  };

  /// @brief start off with our server
  explicit AgencyCache(
    application_features::ApplicationServer& server,
    AgencyCallbackRegistry& callbackRegistry);

  /// @brief Clean up
  virtual ~AgencyCache();

  // whether or not the thread is allowed to start during prepare
  bool isSystem() const override;

  /// @brief 1. Long poll from agency's Raft log
  ///        2. Entertain local cache of agency's read db
  void run() override;

  /// @brief Start thread
  bool start();

  /// @brief Start orderly shutdown of threads
  // cppcheck-suppress virtualCallInConstructor
  void beginShutdown() override;

  /// @brief Get velocypack from node downward. AgencyCommHelper::path is prepended
  consensus::query_t dump() const;

  /// @brief Get velocypack from node downward. AgencyCommHelper::path is prepended
  consensus::index_t get(arangodb::velocypack::Builder& result, std::string const& path = "/") const;

  /// @brief Get velocypack from node downward. AgencyCommHelper::path is prepended
  std::tuple<consensus::query_t, consensus::index_t> get(std::string const& path = "/") const;

  /// @brief Get velocypack from node downward
  std::tuple<consensus::query_t, consensus::index_t> read(std::vector<std::string> const& paths) const;

  /// @brief Get current commit index
  consensus::index_t index() const;

  /// @brief Register local callback
  bool registerCallback(std::string const& key, uint64_t const& id);

  /// @brief Unregister local callback
  void unregisterCallback(std::string const& key, uint64_t const& id);

  /// @brief Wait to be notified, when a Raft index has arrived.
  futures::Future<Result> waitFor(consensus::index_t index);

  /// @brief Cache has these path? AgencyCommHelper::path is prepended
  bool has(std::string const& path) const;

  /// @brief Cache has these path? Paths are absolute
  std::vector<bool> has(std::vector<std::string> const& paths) const;

  /// @brief Used exclusively in unit tests!
  ///        Do not use for production code under any circumstances
  std::pair<std::vector<consensus::apply_ret_t>, consensus::index_t> applyTestTransaction (
    consensus::query_t const& trx);

  /// @brief Used exclusively in unit tests
  consensus::Store& store();

  /**
   * @brief         Get a list of planned/current  changes and other
   *                databases and the corresponding RAFT index
   *
   * @param section Plan/Current
   * @param last    Last index known to the caller
   *
   * @return        The currently last noted RAFT index and  a velocypack
   *                representation of planned and other desired databases
   */
  change_set_t changedSince(
    std::string const& section, consensus::index_t const& last) const;
  
private:

  /// @brief invoke all callbacks
  void invokeAllCallbacks() const;

  /// @brief invoke given callbacks
  void invokeCallbacks(std::vector<uint64_t> const&) const;

  /// @brief invoke given callbacks
  void invokeCallbackNoLock(uint64_t, std::string const& = std::string()) const;

  /// @brief reinitialize all databases, after a snapshot or after a hotbackup restore
  /// Must hold storeLock to call!
  std::unordered_set<std::string> reInitPlan();

  /// @brief handle callbacks for specific log document
  void handleCallbacksNoLock(
    VPackSlice, std::unordered_set<uint64_t>&, std::vector<uint64_t>&,
    std::unordered_set<std::string>& plannedChanges, std::unordered_set<std::string>& currentChanges);

  /// @brief trigger all waiting call backs for index <= _commitIndex
  ///        caller must hold lock
  void triggerWaiting(consensus::index_t commitIndex);

  /// @brief Guard for _readDB
  mutable std::shared_mutex _storeLock;

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

  /// @ brief changes of index to plan and current 
  std::multimap<consensus::index_t, std::string> _planChanges;
  std::multimap<consensus::index_t, std::string> _currentChanges;

  /// @brief snapshot note for client 
  consensus::index_t _lastSnapshot;
  
};

} // namespace

namespace std {
ostream& operator<<(ostream& o, arangodb::AgencyCache::change_set_t const& c);
ostream& operator<<(ostream& o, arangodb::AgencyCache::databases_t const& d);
}


#endif
