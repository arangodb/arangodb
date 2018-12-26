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
/// @author Jan Christoph Uhde
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "SelectivityEstimates.h"
#include "Basics/ReadLocker.h"
#include "Basics/WriteLocker.h"
#include "Cluster/ClusterMethods.h"
#include "VocBase/LogicalCollection.h"

using namespace arangodb;

ClusterSelectivityEstimates::ClusterSelectivityEstimates(LogicalCollection* collection)
    : _collection(collection), _expires(0.0), _updating(false) {}

ClusterSelectivityEstimates& ClusterSelectivityEstimates::operator=(ClusterSelectivityEstimates const& other) {
  if (this != &other) {
    WRITE_LOCKER(locker1, _lock);
    READ_LOCKER(locker2, other._lock);

    _collection = other._collection;
    _estimates = other._estimates;
    _expires = other._expires;
    _updating = false;
  }
  return *this;
}

ClusterSelectivityEstimates& ClusterSelectivityEstimates::operator=(ClusterSelectivityEstimates&& other) {
  if (this != &other) {
    WRITE_LOCKER(locker1, _lock);
    WRITE_LOCKER(locker2, other._lock);

    _collection = other._collection;
    _estimates = std::move(other._estimates);
    _expires = other._expires;
    _updating = false;
    other._expires = 0.0;
  }
  return *this;
}

ClusterSelectivityEstimates::~ClusterSelectivityEstimates() {}

bool ClusterSelectivityEstimates::defined() const {
  READ_LOCKER(locker, _lock);
  return _expires != 0.0;
}

ClusterSelectivityEstimates::ValueType ClusterSelectivityEstimates::fetch() {
  double now = TRI_microtime();
  {
    READ_LOCKER(locker, _lock);

    if (!_estimates.empty() && (now - _expires < defaultExpireTime)) {
      return _estimates;
    }

    // need to fetch an update of the estimates
  }

  while (true) {
    bool weAreUpdating = false;
    {
      WRITE_LOCKER(locker, _lock);

      // re-check update condition
      if (!_estimates.empty() && (now - _expires < defaultExpireTime)) {
        // another thread has updated the estimates
        return _estimates;
      }

      if (!_updating) {
        _updating = true;
        weAreUpdating = true;
      }
    }

    if (weAreUpdating) {
      ValueType estimates;
      selectivityEstimatesOnCoordinator(_collection->vocbase()->name(),
                                        _collection->name(), estimates);

      WRITE_LOCKER(locker, _lock);
      _estimates = estimates;
      _expires = now;
      _updating = false;
      return _estimates;
    }

    now = TRI_microtime();
    usleep(10000);
  }
}

ClusterSelectivityEstimates::ValueType ClusterSelectivityEstimates::get() const {
  READ_LOCKER(locker, _lock);
  return _estimates;
}
