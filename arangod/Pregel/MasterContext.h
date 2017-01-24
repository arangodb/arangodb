////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGODB_PREGEL_MASTER_CONTEXT_H
#define ARANGODB_PREGEL_MASTER_CONTEXT_H 1

#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>
#include "Basics/Common.h"
#include "Pregel/AggregatorHandler.h"

namespace arangodb {
namespace pregel {

class MasterContext {
  friend class Conductor;

  uint64_t _globalSuperstep = 0;
  uint64_t _vertexCount = 0;
  uint64_t _edgeCount = 0;
  AggregatorHandler* _aggregators;

 public:
  
  MasterContext(){};
  
  inline uint64_t globalSuperstep() const { return _globalSuperstep; }
  
  /// current global vertex count, might change after each gss
  inline uint64_t vertexCount() const { return _vertexCount; }
  
  /// current global edge count, might change after each gss
  inline uint64_t edgeCount() const { return _edgeCount; }
  
  template <typename T>
  inline void aggregate(std::string const& name, T const& valuePtr) {
    _aggregators->aggregate(name, &valuePtr);
  }

  template <typename T>
  inline const T* getAggregatedValue(std::string const& name) {
    return (const T*)_aggregators->getAggregatedValue(name);
  }

  virtual void preApplication(){};

  /// @brief called before supersteps
  /// @return true to continue the computation
  virtual void preGlobalSuperstep() {};
  /// @brief called after supersteps
  /// @return true to continue the computation
  virtual bool postGlobalSuperstep() { return true; };
  virtual void postApplication(){};

  /// should indicate if compensation is supposed to start by returning true
  virtual bool preCompensation() { return true; }
  /// should indicate if compensation is finished, by returning false.
  /// otherwise workers will be called again with the aggregated values
  virtual bool postCompensation() { return false; }

};
}
}
#endif
