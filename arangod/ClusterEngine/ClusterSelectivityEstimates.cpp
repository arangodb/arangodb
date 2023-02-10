////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/system-functions.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterMethods.h"
#include "Indexes/Index.h"
#include "VocBase/LogicalCollection.h"

namespace arangodb {

ClusterSelectivityEstimates::ClusterSelectivityEstimates(
    LogicalCollection& collection)
    : _collection(collection) {}

void ClusterSelectivityEstimates::flush() {
  std::lock_guard guard{_update};
  std::atomic_store(&_data, std::shared_ptr<InternalData>());
}

IndexEstMap ClusterSelectivityEstimates::get(bool allowUpdate,
                                             TransactionId tid) {
  auto data =
      std::atomic_load<ClusterSelectivityEstimates::InternalData>(&_data);
  std::unique_lock guard{_update, std::defer_lock};
  if (allowUpdate) {
    double const now = TRI_microtime();
    bool useExpired = false;
    int tries = 0;
    do {
      if (data) {
        auto const& estimates = data->estimates;
        if (!estimates.empty() && (data->expireStamp > now || useExpired)) {
          // already have an estimate, and it is not yet expired
          // or, we have an expired estimate, and another thread is currently
          // updating it
          return estimates;
        }
      }
      if (!guard.try_lock()) {
        // only one thread is allowed to fetch the estimates from the DB servers
        // at any given time
        useExpired = true;
      } else {
        IndexEstMap estimates;  // must fetch estimates from coordinator
        if (selectivityEstimatesOnCoordinator(
                _collection.vocbase().server().getFeature<ClusterFeature>(),
                _collection.vocbase().name(), _collection.name(), estimates,
                tid)
                .ok()) {  // store the updated estimates and return them
          set(estimates);
          return estimates;
        }
        guard.unlock();
      }
      data = std::atomic_load<InternalData>(&_data);
    } while (++tries <= 3);  // give up!
  }

  if (data) {
    return data->estimates;  // we have got some estimates before
  }
  return {};  // return an empty map!
}

void ClusterSelectivityEstimates::set(IndexEstMap estimates) {
  // push new selectivity values into indexes' cache
  auto indexes = _collection.getIndexes();

  for (auto& idx : indexes) {
    auto it = estimates.find(std::to_string(idx->id().id()));
    if (it != estimates.end()) {
      idx->updateClusterSelectivityEstimate(it->second);
    }
  }

  double ttl = defaultTtl;
  // let selectivity estimates expire less often for system collections
  if (!_collection.name().empty() && _collection.name()[0] == '_') {
    ttl = systemCollectionTtl;
  }

  // finally update the cache
  std::atomic_store(&_data, std::make_shared<InternalData>(
                                std::move(estimates), TRI_microtime() + ttl));
}

}  // namespace arangodb
