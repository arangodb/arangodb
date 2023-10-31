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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <velocypack/Slice.h>
#include "Basics/Common.h"
#include "Pregel/AggregatorHandler.h"
#include "Pregel/Utils.h"

namespace arangodb {
namespace pregel {

class WorkerContext {
  template<typename V, typename E, typename M>
  friend class Worker;

 public:
  WorkerContext(std::unique_ptr<AggregatorHandler> readAggregators,
                std::unique_ptr<AggregatorHandler> writeAggregators)
      : _readAggregators{std::move(readAggregators)},
        _writeAggregators{std::move(writeAggregators)} {};
  virtual ~WorkerContext() = default;

  inline uint64_t vertexCount() const { return _vertexCount; }

  inline uint64_t edgeCount() const { return _edgeCount; }

  template<typename T>
  inline void aggregate(std::string const& name, T const& value) {
    T const* ptr = &value;
    _writeAggregators->aggregate(name, ptr);
  }

  template<typename T>
  inline const T* getAggregatedValue(std::string const& name) {
    return (T*)_readAggregators->getAggregatedValue(name);
  }

  virtual void preApplication() {}
  virtual void preGlobalSuperstep(uint64_t gss) {}
  virtual void postGlobalSuperstep(uint64_t gss) {}

  uint64_t _vertexCount = 0, _edgeCount = 0;
  std::unique_ptr<AggregatorHandler> _readAggregators;
  std::unique_ptr<AggregatorHandler> _writeAggregators;
};
}  // namespace pregel
}  // namespace arangodb
