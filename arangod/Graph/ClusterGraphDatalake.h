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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_GRAPH_CLUSTER_GRAPH_DATA_LAKE_H
#define ARANGOD_GRAPH_CLUSTER_GRAPH_DATA_LAKE_H 1

#include "Basics/ResourceUsage.h"

#include <memory>
#include <vector>

namespace arangodb {
struct ResourceMonitor;

namespace velocypack {
class Slice;
}

namespace graph {

template <typename T>
class ClusterGraphDatalake {
 public:
  ClusterGraphDatalake(arangodb::ResourceMonitor& resourceMonitor)
      : _resourceMonitor(resourceMonitor),
        _totalMemoryUsage(0) {}

  ~ClusterGraphDatalake() {
    clear();
  }

  size_t numEntries() const noexcept { return _data.size(); }
  
  void clear() noexcept {
    _data.clear();
    _resourceMonitor.decreaseMemoryUsage(_totalMemoryUsage);
    _totalMemoryUsage = 0;
  }
  
  arangodb::velocypack::Slice operator[](size_t index) const noexcept;

  arangodb::velocypack::Slice add(std::shared_ptr<T> data);

 private:
  void add(std::shared_ptr<T> data, size_t memoryUsage);

 private:
  arangodb::ResourceMonitor& _resourceMonitor;
  size_t _totalMemoryUsage;
  std::vector<std::shared_ptr<T>> _data;
};

}  // namespace graph
}  // namespace arangodb
#endif
