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
/// @author Max Neunhoeffer
/// @author Andreas Streichardt
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_CLUSTER_FOLLOWER_INFO_H
#define ARANGOD_CLUSTER_FOLLOWER_INFO_H 1

#include "ClusterInfo.h"

#include "Basics/Mutex.h"
#include "Basics/ReadWriteLock.h"
#include "Basics/Result.h"
#include "Basics/WriteLocker.h"
#include "VocBase/LogicalCollection.h"

namespace arangodb {

namespace velocypack {
class Slice;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief a class to track followers that are in sync for a shard
////////////////////////////////////////////////////////////////////////////////

class FollowerInfo {
  // This is the list of real local followers
  std::shared_ptr<std::vector<ServerID> const> _followers;
  // This is the list of followers that have been insync BEFORE we
  // triggered a failover to this server.
  // The list is filled only temporarily, and will be deleted as
  // soon as we can guarantee at least so many followers locally.
  std::shared_ptr<std::vector<ServerID> const> _failoverCandidates;

  // The agencyMutex is used to synchronise access to the agency.
  // the _dataLock is used to sync the access to local data.
  // The _canWriteLock is used to protect flag if we do have enough followers
  // The locking ordering to avoid dead locks has to be as follows:
  // 1.) _agencyMutex
  // 2.) _canWriteLock
  // 3.) _dataLock
  mutable Mutex _agencyMutex;
  mutable arangodb::basics::ReadWriteLock _canWriteLock;
  mutable arangodb::basics::ReadWriteLock _dataLock;

  arangodb::LogicalCollection* _docColl;
  // if the latter is empty, then we are leading
  std::string _theLeader;
  bool _theLeaderTouched;
  // flag if we have enough insnc followers and can pass through writes
  bool _canWrite;

 public:
  explicit FollowerInfo(arangodb::LogicalCollection* d)
      : _followers(std::make_shared<std::vector<ServerID>>()),
        _failoverCandidates(std::make_shared<std::vector<ServerID>>()),
        _docColl(d),
        _theLeader(""),
        _theLeaderTouched(false),
        _canWrite(_docColl->replicationFactor() <= 1) {
    // On replicationfactor 1 we do not have anything we can do.
    // This should also disable satellite tracking.
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief get information about current followers of a shard.
  ////////////////////////////////////////////////////////////////////////////////

  std::shared_ptr<std::vector<ServerID> const> get() const {
    READ_LOCKER(readLocker, _dataLock);
    return _followers;
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief get information about current followers of a shard.
  ////////////////////////////////////////////////////////////////////////////////

  std::shared_ptr<std::vector<ServerID> const> getFailoverCandidates() const {
    READ_LOCKER(readLocker, _dataLock);
    return _failoverCandidates;
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief Take over leadership for this shard.
  ///        Also inject information of a insync followers that we knew about
  ///        before a failover to this server has happened
  ////////////////////////////////////////////////////////////////////////////////

  void takeOverLeadership(std::vector<std::string> const& previousInsyncFollowers);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief add a follower to a shard, this is only done by the server side
  /// of the "get-in-sync" capabilities. This reports to the agency under
  /// `/Current` but in asynchronous "fire-and-forget" way. The method
  /// fails silently, if the follower information has since been dropped
  /// (see `dropFollowerInfo` below).
  //////////////////////////////////////////////////////////////////////////////

  Result add(ServerID const& s);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief remove a follower from a shard, this is only done by the
  /// server if a synchronous replication request fails. This reports to
  /// the agency under `/Current` but in an asynchronous "fire-and-forget"
  /// way.
  //////////////////////////////////////////////////////////////////////////////

  Result remove(ServerID const& s);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief clear follower list, no changes in agency necesary
  //////////////////////////////////////////////////////////////////////////////

  void clear();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief check whether the given server is a follower
  //////////////////////////////////////////////////////////////////////////////

  bool contains(ServerID const& s) const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief set leadership
  //////////////////////////////////////////////////////////////////////////////

  void setTheLeader(std::string const& who) {
    // Empty leader => we are now new leader.
    // This needs to be handled with takeOverLeadership
    TRI_ASSERT(!who.empty());
    WRITE_LOCKER(writeLocker, _dataLock);
    _theLeader = who;
    _theLeaderTouched = true;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief get the leader
  //////////////////////////////////////////////////////////////////////////////

  std::string getLeader() const {
    READ_LOCKER(readLocker, _dataLock);
    return _theLeader;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief see if leader was explicitly set
  //////////////////////////////////////////////////////////////////////////////

  bool getLeaderTouched() const {
    READ_LOCKER(readLocker, _dataLock);
    return _theLeaderTouched;
  }

  bool allowedToWrite() {
    {
      READ_LOCKER(readLocker, _canWriteLock);
      if (_canWrite) {
        // Someone has decided we can write, fastPath!

#ifdef ARANGODB_USE_MAINTAINER_MODE
        // Invariant, we can only WRITE if we do not have other failover candidates
        READ_LOCKER(readLockerData, _dataLock);
        TRI_ASSERT(_followers->size() == _failoverCandidates->size());
#endif
        return _canWrite;
      }
      READ_LOCKER(readLockerData, _dataLock);
      TRI_ASSERT(_docColl != nullptr);
      if (_followers->size() + 1 < _docColl->minReplicationFactor()) {
        // We know that we still do not have enough followers
        return false;
      }
    }
    return updateFailoverCandidates();
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Inject the information about followers into the builder.
  ///        Builder needs to be an open object and is not allowed to contain
  ///        the keys "servers" and "failoverCandidates".
  //////////////////////////////////////////////////////////////////////////////
  void injectFollowerInfo(arangodb::velocypack::Builder& builder) const {
    READ_LOCKER(readLockerData, _dataLock);
    injectFollowerInfoInternal(builder);
  }

 private:
  void injectFollowerInfoInternal(arangodb::velocypack::Builder& builder) const;

  bool updateFailoverCandidates();

  Result persistInAgency(bool isRemove) const;

  arangodb::velocypack::Builder newShardEntry(arangodb::velocypack::Slice oldValue) const;
};
}  // end namespace arangodb

#endif
