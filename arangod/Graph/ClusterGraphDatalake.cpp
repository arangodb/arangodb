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

#include "ClusterGraphDatalake.h"

#include "Basics/debugging.h"

#include <velocypack/Buffer.h>
#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

#include <type_traits>

using namespace arangodb;
using namespace arangodb::graph;

template <typename T>
void ClusterGraphDatalake<T>::add(std::shared_ptr<T> data, size_t memoryUsage) {
  TRI_ASSERT(data != nullptr);

  if (_data.empty()) {
    // save initial reallocations
    _data.reserve(8);
  }
  
  memoryUsage += sizeof(typename decltype(_data)::value_type) + sizeof(T);
  
  {
    arangodb::ResourceUsageScope scope(_resourceMonitor, memoryUsage);

    _data.emplace_back(std::move(data));

    // we are now responsible for tracking the memory usage
    scope.steal();
  }
  _totalMemoryUsage += memoryUsage;
}
  
// partial specializations for different container types
template <>
arangodb::velocypack::Slice ClusterGraphDatalake<arangodb::velocypack::Builder>::operator[](size_t index) const noexcept {
  TRI_ASSERT(index < _data.size()); 
  return _data[index]->slice();
}

template <>
arangodb::velocypack::Slice ClusterGraphDatalake<arangodb::velocypack::Builder>::add(std::shared_ptr<arangodb::velocypack::Builder> data) {
  add(std::move(data), data->size());

  return _data.back()->slice();
}

template <>
arangodb::velocypack::Slice ClusterGraphDatalake<arangodb::velocypack::UInt8Buffer>::operator[](size_t index) const noexcept {
  TRI_ASSERT(index < _data.size()); 
  return arangodb::velocypack::Slice(_data[index]->data());
}

template <>
arangodb::velocypack::Slice ClusterGraphDatalake<arangodb::velocypack::UInt8Buffer>::add(std::shared_ptr<arangodb::velocypack::UInt8Buffer> data) {
  add(std::move(data), (data->usesLocalMemory() ? 0 : data->capacity()));

  return arangodb::velocypack::Slice(_data.back()->data());
}
