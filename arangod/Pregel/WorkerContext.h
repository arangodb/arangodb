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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_PREGEL_WORKER_CONTEXT_H
#define ARANGODB_PREGEL_WORKER_CONTEXT_H 1

#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>
#include "Basics/Common.h"
#include "Pregel/AggregatorHandler.h"
#include "Pregel/Utils.h"
#include "Pregel/Reports.h"

namespace arangodb {
namespace pregel {

class WorkerContext {
  template <typename V, typename E, typename M>
  friend class Worker;

  uint64_t _vertexCount, _edgeCount;
  AggregatorHandler* _readAggregators;
  AggregatorHandler* _writeAggregators;
  ReportManager* _reports;

 protected:
  template <typename T>
  inline void aggregate(std::string const& name, T const& value) {
    T const* ptr = &value;
    _writeAggregators->aggregate(name, ptr);
  }

  template <typename T>
  inline const T* getAggregatedValue(std::string const& name) {
    return (T*)_readAggregators->getAggregatedValue(name);
  }

  AggregatorHandler& getWriteAggregators() {
    return *_writeAggregators;
  }

  virtual void preApplication() {}
  virtual void preGlobalSuperstep(uint64_t gss) {}
  virtual void preGlobalSuperstepMasterMessage(VPackSlice msg) {}
  virtual void postGlobalSuperstep(uint64_t gss) {}
  virtual void postGlobalSuperstepMasterMessage(VPackBuilder& msg) {}
  virtual void postApplication() {}

  ReportManager& getReportManager() const { return *_reports; }

 public:
  WorkerContext()
      : _vertexCount(0),
        _edgeCount(0),
        _readAggregators(nullptr),
        _writeAggregators(nullptr) {}
  virtual ~WorkerContext() = default;

  inline uint64_t vertexCount() const { return _vertexCount; }

  inline uint64_t edgeCount() const { return _edgeCount; }
};
}  // namespace pregel
}  // namespace arangodb
#endif
