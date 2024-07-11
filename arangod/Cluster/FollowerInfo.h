////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
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
///
/// @author Max Neunhoeffer
/// @author Andreas Streichardt
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Basics/ReadWriteLock.h"
#include "Cluster/ClusterTypes.h"
#include "Metrics/InstrumentedBool.h"

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace arangodb {

namespace velocypack {
class Slice;
}

class LogicalCollection;

////////////////////////////////////////////////////////////////////////////////
/// @brief a class to track followers that are in sync for a shard
////////////////////////////////////////////////////////////////////////////////

class FollowerInfo {
  // This is the list of real local followers
  std::shared_ptr<std::vector<ServerID>> _followers;
  // This is the list of followers that have been insync BEFORE we
  // triggered a failover to this server.
  // The list is filled only temporarily, and will be deleted as
  // soon as we can guarantee at least so many followers locally.
  std::shared_ptr<std::vector<ServerID>> _failoverCandidates;

  // The following map holds a random number for each follower, this
  // random number is given and sent to the follower when it gets in sync
  // (actually, when it acquires the exclusive lock to get in sync), and is
  // then subsequently sent alongside every synchronous replication
  // request. If the number does not match, the follower will refuse the
  // replication request. This is to ensure that replication requests cannot
  // be delayed into the "next" leader/follower relationship.
  // And here is the proof that this works: The exclusive lock divides the
  // write operations between "before" and "after". The id is changed
  // when the exclusive lock is established. Therefore, it is OK for the
  // replication requests to send along the "current" id, directly
  // from this map.
  std::unordered_map<ServerID, uint64_t> _followingTermId;

  // The agencyMutex is used to synchronise access to the agency.
  // the _dataLock is used to sync the access to local data.
  // The _canWriteLock is used to protect flag if we do have enough followers
  // The locking ordering to avoid dead locks has to be as follows:
  // 1.) _agencyMutex
  // 2.) _canWriteLock
  // 3.) _dataLock
  mutable std::mutex _agencyMutex;
  mutable basics::ReadWriteLock _canWriteLock;
  mutable basics::ReadWriteLock _dataLock;

  LogicalCollection* _docColl;
  // if the latter is empty, then we are leading
  std::string _theLeader;
  bool _theLeaderTouched;
  // flag if we have enough insnc followers and can pass through writes
  bool _canWrite;
  metrics::InstrumentedBool _writeConcernReached;

 public:
  explicit FollowerInfo(LogicalCollection* d);

  enum class WriteState { ALLOWED = 0, FORBIDDEN, STARTUP, UNAVAILABLE };

  /// @brief get information about current followers of a shard.
  std::shared_ptr<std::vector<ServerID> const> get() const;

  /// @brief get a copy of the information about current followers of a shard.
  std::vector<ServerID> getCopy() const;

  /// @brief get information about current followers of a shard.
  std::shared_ptr<std::vector<ServerID> const> getFailoverCandidates() const;

  /// @brief Take over leadership for this shard.
  ///        Also inject information of a insync followers that we knew about
  ///        before a failover to this server has happened
  ///        The second parameter may be nullptr. It is an additional list
  ///        of declared to be insync followers. If it is nullptr the follower
  ///        list is initialized empty.
  void takeOverLeadership(
      std::vector<ServerID> const& previousInsyncFollowers,
      std::shared_ptr<std::vector<ServerID>> realInsyncFollowers);

  /// @brief add a follower to a shard, this is only done by the server side
  /// of the "get-in-sync" capabilities. This reports to the agency under
  /// `/Current` but in asynchronous "fire-and-forget" way. The method
  /// fails silently, if the follower information has since been dropped
  /// (see `dropFollowerInfo` below).
  Result add(ServerID const& s);

  /// @brief remove a follower from a shard, this is only done by the
  /// server if a synchronous replication request fails. This reports to
  /// the agency under `/Current` but in an asynchronous "fire-and-forget"
  /// way.
  Result remove(ServerID const& s);

  /// @brief explicitly set the following term id for a follower.
  /// this should only be used for special cases during upgrading or testing.
  void setFollowingTermId(ServerID const& s, uint64_t value);

  /// @brief for each run of the "get-in-sync" protocol we generate a
  /// random number to identify this "following term". This is created
  /// when the follower fetches the exclusive lock to finally get in sync
  /// and is stored in _followingTermId, so that it can be forwarded with
  /// each synchronous replication request. The follower can then decline
  /// the replication in case it is not "in the same term".
  uint64_t newFollowingTermId(ServerID const& s) noexcept;

  /// @brief for each run of the "get-in-sync" protocol we generate a
  /// random number to identify this "following term". This is created
  /// when the follower fetches the exclusive lock to finally get in sync
  /// and is stored in _followingTermId, so that it can be forwarded with
  /// each synchronous replication request. The follower can then decline
  /// the replication in case it is not "in the same term".
  uint64_t getFollowingTermId(ServerID const& s) const noexcept;

  /// @brief clear follower list, no changes in agency necesary
  void clear();

  /// @brief check whether the given server is a follower
  bool contains(ServerID const& s) const;

  /// @brief set leadership
  void setTheLeader(std::string const& who);

  // conditionally change the leader, in case the current leader is still the
  // same as expected. in this case, return true and change the leader to
  // actual. otherwise, don't change anything and return false
  bool setTheLeaderConditional(std::string const& expected,
                               std::string const& actual);

  /// @brief get the leader
  std::string getLeader() const;

  /// @brief see if leader was explicitly set
  bool getLeaderTouched() const;

  WriteState allowedToWrite();

  /// @brief Inject the information about followers into the builder.
  ///        Builder needs to be an open object and is not allowed to contain
  ///        the keys "servers" and "failoverCandidates".
  std::pair<size_t, size_t> injectFollowerInfo(
      velocypack::Builder& builder) const;

 private:
  /// @brief inject the information about "servers" and "failoverCandidates".
  /// must be called with _dataLock locked.
  void injectFollowerInfoInternal(velocypack::Builder& builder) const;

  bool updateFailoverCandidates();

  Result persistInAgency(bool isRemove, bool acquireDataLock) const;

  velocypack::Builder newShardEntry(velocypack::Slice oldValue) const;
};
}  // end namespace arangodb
