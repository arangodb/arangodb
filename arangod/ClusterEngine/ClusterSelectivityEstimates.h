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

#ifndef ARANGOD_CLUSTER_ENGINE_CLUSTER_SELECTIVITY_ESTIMATES_H
#define ARANGOD_CLUSTER_ENGINE_CLUSTER_SELECTIVITY_ESTIMATES_H 1

#include "Basics/Common.h"
#include "Indexes/IndexIterator.h"
#include "VocBase/voc-types.h"

namespace arangodb {
class LogicalCollection;

///@brief basic cache for selectivity estimates in the cluster
class ClusterSelectivityEstimates {
 public:
  explicit ClusterSelectivityEstimates(LogicalCollection& collection);
  void flush();
  
  /// @brief fetch estimates from cache or server
  /// @param allowUpdate allow cluster communication
  /// @param tid specify ongoing transaction this is a part of
  IndexEstMap get(bool allowUpdating, TRI_voc_tick_t tid);
  void set(IndexEstMap const& estimates);

 private:
  struct InternalData {
    IndexEstMap estimates;
    double expireStamp;
    
    InternalData(IndexEstMap const& estimates, double expireStamp) 
        : estimates(estimates), expireStamp(expireStamp) {}
  };

  LogicalCollection& _collection;
  // the current estimates, only load and stored using atomic operations
  std::shared_ptr<InternalData> _data;
  // whether or not a thread is currently updating the estimates
  std::atomic<bool> _updating;

  static constexpr double defaultTtl = 90.0;
  static constexpr double systemCollectionTtl = 900.0;
};

}  // namespace arangodb

#endif
