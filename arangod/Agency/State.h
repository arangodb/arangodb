////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2018 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_CONSENSUS_STATE_H
#define ARANGOD_CONSENSUS_STATE_H 1

#include "Agency/Store.h"
#include "AgencyCommon.h"
#include "Basics/MutexLocker.h"
#include "Cluster/ClusterComm.h"
#include "Utils/OperationOptions.h"

#include <velocypack/vpack.h>

#include <cstdint>
#include <deque>
#include <functional>
#include <map>

struct TRI_vocbase_t;

namespace arangodb {

namespace aql {
class QueryRegistry;
}

namespace consensus {

class Agent;

/**
 * @brief State replica
 */
class State {
 public:
  /// @brief Default constructor
  State();

  /// @brief Default Destructor
  virtual ~State();

  /// @brief Append log entry
  void append(query_t const& query);

  /// @brief Log entries (leader)
  std::vector<index_t> logLeaderMulti(query_t const& query,
                                      std::vector<apply_ret_t> const& indices, term_t term);

  /// @brief Single log entry (leader)
  index_t logLeaderSingle(velocypack::Slice const& slice, term_t term,
                          std::string const& clientId = std::string());

  /// @brief Log entries (followers)
  arangodb::consensus::index_t logFollower(query_t const&);

  /// @brief Find entry at index with term
  bool find(index_t index, term_t term);

  /// @brief Get complete log entries bound by lower and upper bounds.
  ///        Default: [first, last]
  std::vector<log_t> get(index_t = 0, index_t = (std::numeric_limits<uint64_t>::max)()) const;

 private:
  /// @brief Get complete log entries bound by lower and upper bounds.
  ///        Default: [first, last]
  log_t at(index_t) const;

  /// @brief non-locking version of at
  log_t atNoLock(index_t) const;

 public:
  /// @brief Check for a log entry, returns 0, if the log does not
  /// contain an entry with index `index`, 1, if it does contain one
  /// with term `term` and -1, if it does contain one with another term
  /// than `term`:
  int checkLog(index_t index, term_t term) const;

  /// @brief Has entry with index und term
  bool has(index_t, term_t) const;

  /// @brief Get log entries by client Id
  std::vector<index_t> inquire(query_t const&) const;

  /// @brief Get complete logged commands by lower and upper bounds.
  ///        Default: [first, last]
  arangodb::velocypack::Builder slices(index_t = 0,
                                       index_t = (std::numeric_limits<uint64_t>::max)()) const;

  /// @brief log entry at index i
  log_t operator[](index_t) const;

  /// @brief last log entry, copy entry because we do no longer have the lock
  /// after the return
  log_t lastLog() const;

  /// @brief index of last log entry
  index_t lastIndex() const;

  /// @brief index of first log entry
  index_t firstIndex() const;

  /// @brief Set endpoint
  bool configure(Agent* agent);

  /// @brief Load persisted data from above or start with empty log
  bool loadCollections(TRI_vocbase_t*, aql::QueryRegistry*, bool);

  /// @brief Pipe to ostream
  friend std::ostream& operator<<(std::ostream& os, State const& s) {
    VPackBuilder b;
    {
      VPackArrayBuilder a(&b);
      for (auto const& i : s._log) {
        VPackObjectBuilder bb(&b);
        b.add("index", VPackValue(i.index));
        b.add("term", VPackValue(i.term));
        b.add("item", VPackSlice(i.entry->data()));
      }
    }
    os << b.toJson();
    return os;
  }

  /// @brief compact state machine
  bool compact(arangodb::consensus::index_t cind, arangodb::consensus::index_t keep);

 private:
  /// @brief Remove RAFT conflicts. i.e. All indices, where higher term version
  ///        exists are overwritten, a snapshot in first position is ignored
  ///        as well, the flag gotSnapshot has to be true in this case.
  size_t removeConflicts(query_t const&, bool gotSnapshot);

 public:
  bool ready() const;

  /// @brief Persist active agency in pool, throws an exception in case of error
  void persistActiveAgents(query_t const& active, query_t const& pool);

  /// @brief Get everything from the state machine
  query_t allLogs() const;

  /// @brief load a compacted snapshot, returns true if successfull and false
  /// otherwise. In case of success store and index are modified. The store
  /// is reset to the state after log index `index` has been applied. Sets
  /// `index` to 0 if there is no compacted snapshot.
  bool loadLastCompactedSnapshot(Store& store, index_t& index, term_t& term);

  /// @brief lastCompactedAt
  index_t lastCompactionAt() const;

  /// @brief nextCompactionAfter
  index_t nextCompactionAfter() const { return _nextCompactionAfter; }

  /// @brief this method is intended for manual recovery only. It only looks
  /// at the persisted data structure and tries to recover the latest state.
  /// The returned builder has the complete state of the agency and index
  /// is set to the index of the last log entry.
  static std::shared_ptr<VPackBuilder> latestAgencyState(TRI_vocbase_t& vocbase,
                                                         index_t& index, term_t& term);

 private:
  /// @brief Persist a compaction snapshot
  bool persistCompactionSnapshot(arangodb::consensus::index_t cind,
                                 arangodb::consensus::term_t term,
                                 arangodb::consensus::Store& snapshot);

  /// @brief storeLogFromSnapshot, needed in the follower, this erases the
  /// complete log and persists the given snapshot. After this operation, the
  /// log is empty and something ought to be appended to it rather quickly.
  /// Therefore, this is only called from logFollower under the _logLock.
  bool storeLogFromSnapshot(arangodb::consensus::Store& snapshot,
                            arangodb::consensus::index_t index,
                            arangodb::consensus::term_t term);

  /// @brief Log single log entry. Must be guarded by caller.
  index_t logNonBlocking(index_t idx, velocypack::Slice const& slice,
                         term_t term, std::string const& clientId = std::string(),
                         bool leading = false, bool reconfiguration = false);

  /// @brief Save currentTerm, votedFor, log entries
  bool persist(index_t, term_t, arangodb::velocypack::Slice const&, std::string const&) const;

  /// @brief Save currentTerm, votedFor, log entries for reconfiguration
  bool persistconf(index_t, term_t, arangodb::velocypack::Slice const&,
                   std::string const&) const;

  bool saveCompacted();

  /// @brief Load collection from persistent store
  bool loadPersisted();
  bool loadCompacted();
  bool loadRemaining();
  bool loadOrPersistConfiguration();

  /// @brief Check collections
  bool checkCollections();

  /// @brief Check collection sanity
  bool checkCollection(std::string const& name);

  /// @brief Create collections
  bool createCollections();

  /// @brief Create collection
  bool createCollection(std::string const& name);

  /// @brief Compact persisted logs
  bool compactPersisted(arangodb::consensus::index_t cind, arangodb::consensus::index_t keep);

  /// @brief Compact RAM logs
  bool compactVolatile(arangodb::consensus::index_t cind, arangodb::consensus::index_t keep);

  /// @brief Remove obsolete logs
  bool removeObsolete(arangodb::consensus::index_t cind);

  /// @brief Our agent
  Agent* _agent;

  /// @brief Our vocbase
  TRI_vocbase_t* _vocbase;

  std::atomic<bool> _ready;

  /**< @brief Mutex for modifying
     _log & _cur
  */
  mutable arangodb::Mutex _logLock;
  std::deque<log_t> _log; /**< @brief  State entries */
  // Invariant: This has at least one entry at all times!
  bool _collectionsChecked; /**< @brief Collections checked */
  bool _collectionsLoaded;
  std::multimap<std::string, arangodb::consensus::index_t> _clientIdLookupTable;

  /// @brief compaction indexes
  std::atomic<index_t> _nextCompactionAfter;
  std::atomic<index_t> _lastCompactionAt;

  /// @brief Our query registry
  aql::QueryRegistry* _queryRegistry;

  /// @brief Current log offset, this is the index that is stored at position
  /// 0 in the deque _log.
  size_t _cur;

  /// @brief Operation options
  arangodb::OperationOptions _options;

  /// @brief Empty log entry;
  static log_t emptyLog;

  /// @brief Protect writing into configuration collection
  arangodb::Mutex _configurationWriteLock;
};

}  // namespace consensus
}  // namespace arangodb

#endif
