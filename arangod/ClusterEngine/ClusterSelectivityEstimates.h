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
#include "Basics/ReadWriteLock.h"
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
  std::unordered_map<std::string, double> get(bool allowUpdate, TRI_voc_tick_t tid) const;
  void set(std::unordered_map<std::string, double>&& estimates);

 private:
  LogicalCollection& _collection;
  mutable basics::ReadWriteLock _lock;
  mutable std::unordered_map<std::string, double> _estimates;
  mutable double _expireStamp;

  static constexpr double defaultTtl = 60.0;
};

}  // namespace arangodb

#endif
