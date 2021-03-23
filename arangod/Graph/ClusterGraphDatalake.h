////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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

#include <velocypack/Buffer.h>

#include <memory>
#include <vector>

namespace arangodb {
struct ResourceMonitor;

namespace velocypack {
class Slice;
}

namespace graph {

class ClusterGraphDatalake {
 public:
  ClusterGraphDatalake(ClusterGraphDatalake const&) = delete;
  ClusterGraphDatalake& operator=(ClusterGraphDatalake const&) = delete;

  explicit ClusterGraphDatalake(arangodb::ResourceMonitor& resourceMonitor);
  ~ClusterGraphDatalake();

  size_t numEntries() const noexcept { return _data.size(); }
  
  void clear() noexcept {
    _data.clear();
    _resourceMonitor.decreaseMemoryUsage(_totalMemoryUsage);
    _totalMemoryUsage = 0;
  }
  
  arangodb::velocypack::Slice operator[](size_t index) const noexcept;

  arangodb::velocypack::Slice add(std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>> data);

 private:
  arangodb::ResourceMonitor& _resourceMonitor;
  size_t _totalMemoryUsage;
  std::vector<std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>>> _data;
};

}  // namespace graph
}  // namespace arangodb
#endif
