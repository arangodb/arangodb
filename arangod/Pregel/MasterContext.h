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

namespace arangodb {
namespace pregel {

class Conductor;

class MasterContext {
  friend class Conductor;

 public:
  uint64_t _globalSuperstep = 0;
  uint64_t _vertexCount = 0;
  uint64_t _edgeCount = 0;
  std::unique_ptr<AggregatorHandler> _aggregators;

  MasterContext(uint64_t vertexCount, uint64_t edgeCount,
                std::unique_ptr<AggregatorHandler> aggregators)
      : _vertexCount{vertexCount},
        _edgeCount{edgeCount},
        _aggregators{std::move(aggregators)} {}
  virtual ~MasterContext() = default;

  inline uint64_t globalSuperstep() const { return _globalSuperstep; }

  /// current global vertex count, might change after each gss
  inline uint64_t vertexCount() const { return _vertexCount; }

  /// current global edge count, might change after each gss
  inline uint64_t edgeCount() const { return _edgeCount; }

  template<typename T>
  inline void aggregate(std::string const& name, T const& value) {
    T const* ptr = &value;
    _aggregators->aggregate(name, ptr);
  }

  template<typename T>
  inline const T* getAggregatedValue(std::string const& name) {
    return (const T*)_aggregators->getAggregatedValue(name);
  }

  template<typename T>
  inline void setAggregatedValue(std::string const& name, T const& value) {
    // FIXME refactor the aggregators, this whole API is horrible
    arangodb::velocypack::Builder b;
    b.openObject();
    b.add("aggregators",
          arangodb::velocypack::Value(arangodb::velocypack::ValueType::Object));
    b.add(name, arangodb::velocypack::Value(value));
    b.close();
    b.close();
    _aggregators->setAggregatedValues(b.slice());
  }

  template<typename T>
  inline T* getAggregator(std::string const& name) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    return dynamic_cast<T*>(_aggregators->getAggregator(name));
#else
    return reinterpret_cast<T*>(_aggregators->getAggregator(name));
#endif
  }

  virtual void preApplication() {}

  /// @brief called before supersteps
  virtual void preGlobalSuperstep() {}
  /// @return true to continue the computation
  virtual bool preGlobalSuperstepWithResult() {
    preGlobalSuperstep();
    return true;
  }
  /// @brief called after supersteps
  /// @return true to continue the computation
  virtual bool postGlobalSuperstep() { return true; }

  virtual void postApplication() {}

  /// should indicate if compensation is supposed to start by returning true
  virtual bool preCompensation() { return true; }
  /// should indicate if compensation is finished, by returning false.
  /// otherwise workers will be called again with the aggregated values
  virtual bool postCompensation() { return false; }
};
}  // namespace pregel
}  // namespace arangodb
