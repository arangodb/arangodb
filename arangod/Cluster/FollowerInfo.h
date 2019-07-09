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

namespace arangodb {

////////////////////////////////////////////////////////////////////////////////
/// @brief a class to track followers that are in sync for a shard
////////////////////////////////////////////////////////////////////////////////

class FollowerInfo {
  std::shared_ptr<std::vector<ServerID> const> _followers;

  // The agencyMutex is used to synchronise access to the agency.
  // the _dataLock is used to sync the access to local data.
  // The agencyMutex is always locked before the _dataLock is locked.
  mutable Mutex _agencyMutex;
  mutable arangodb::basics::ReadWriteLock _dataLock;
  arangodb::LogicalCollection* _docColl;
  std::string _theLeader;
  // if the latter is empty, then we are leading
  bool _theLeaderTouched;

 public:
  explicit FollowerInfo(arangodb::LogicalCollection* d)
      : _followers(std::make_shared<std::vector<ServerID>>()), _docColl(d), _theLeaderTouched(false) {}

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief get information about current followers of a shard.
  ////////////////////////////////////////////////////////////////////////////////

  std::shared_ptr<std::vector<ServerID> const> get() const {
    READ_LOCKER(readLocker, _dataLock);
    return _followers;
  }

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
};
}  // end namespace arangodb

#endif
