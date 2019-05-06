////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "ClusterSelectivityEstimates.h"

#include "Basics/ReadLocker.h"
#include "Basics/WriteLocker.h"
#include "Cluster/ClusterMethods.h"
#include "Indexes/Index.h"
#include "VocBase/LogicalCollection.h"

using namespace arangodb;

ClusterSelectivityEstimates::ClusterSelectivityEstimates(LogicalCollection& collection)
    : _collection(collection), _expireStamp(0.0) {}

void ClusterSelectivityEstimates::flush() {
  WRITE_LOCKER(lock, _lock);
  _estimates.clear();
  _expireStamp = 0.0;
}

IndexEstMap ClusterSelectivityEstimates::get(bool allowUpdate, TRI_voc_tid_t tid) const {
  double now;

  {
    READ_LOCKER(readLock, _lock);

    if (!allowUpdate) {
      // return whatever is there. may be empty as well
      return _estimates;
    }

    now = TRI_microtime();
    if (!_estimates.empty() && _expireStamp > now) {
      // already have an estimate, and it is not yet expired
      return _estimates;
    }
  }

  // have no estimate yet, or it is already expired
  // we have given up the read lock here
  // because we now need to modify the estimates

  int tries = 0;
  while (true) {
    decltype(_estimates) estimates;

    WRITE_LOCKER(writeLock, _lock);

    if (!_estimates.empty() && _expireStamp > now) {
      // some other thread has updated the estimates for us... just use them
      return _estimates;
    }

    int res = selectivityEstimatesOnCoordinator(_collection.vocbase().name(),
                                                _collection.name(), estimates, tid);

    if (res == TRI_ERROR_NO_ERROR) {
      _estimates = estimates;
      // let selectivity estimates expire less seldom for system collections
      _expireStamp = now + defaultTtl * (_collection.name()[0] == '_' ? 10.0 : 1.0);

      // give up the lock, and then update the selectivity values for each index
      writeLock.unlock();

      // push new selectivity values into indexes' cache
      auto indexes = _collection.getIndexes();

      for (std::shared_ptr<Index>& idx : indexes) {
        auto it = estimates.find(std::to_string(idx->id()));

        if (it != estimates.end()) {
          idx->updateClusterSelectivityEstimate(it->second);
        }
      }

      return estimates;
    }

    if (++tries == 3) {
      return _estimates;
    }
  }
}

void ClusterSelectivityEstimates::set(std::unordered_map<std::string, double>&& estimates) {
  double const now = TRI_microtime();

  // push new selectivity values into indexes' cache
  auto indexes = _collection.getIndexes();

  for (std::shared_ptr<Index>& idx : indexes) {
    auto it = estimates.find(std::to_string(idx->id()));

    if (it != estimates.end()) {
      idx->updateClusterSelectivityEstimate(it->second);
    }
  }

  // finally update the cache
  {
    WRITE_LOCKER(writelock, _lock);

    _estimates = std::move(estimates);
    // let selectivity estimates expire less seldom for system collections
    _expireStamp = now + defaultTtl * (_collection.name()[0] == '_' ? 10.0 : 1.0);
  }
}
