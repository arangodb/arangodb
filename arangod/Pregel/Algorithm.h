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

#ifndef ARANGODB_PREGEL_ALGORITHM_H
#define ARANGODB_PREGEL_ALGORITHM_H 1

#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>
#include <cstdint>

#include "Aggregator.h"
#include "Basics/Common.h"
#include "GraphFormat.h"
#include "MessageCombiner.h"
#include "MessageFormat.h"
#include "WorkerContext.h"

namespace arangodb {
namespace pregel {

template <typename V, typename E, typename M>
class VertexComputation;

// specify serialization, whatever
template <typename V, typename E, typename M>
struct Algorithm : IAggregatorCreator {
 public:
  virtual ~Algorithm() {}
  // virtual bool isFixpointAlgorithm() const {return false;}
  // virtual bool preserveTransactions() const { return false; }
  virtual WorkerContext* workerContext(VPackSlice userParams) const {
    return new WorkerContext();
  }
  virtual GraphFormat<V, E>* inputFormat() const = 0;
  virtual Aggregator* aggregator(std::string const& name) const {
    return nullptr;
  }
  virtual MessageFormat<M>* messageFormat() const = 0;
  virtual MessageCombiner<M>* messageCombiner() const = 0;
  virtual VertexComputation<V, E, M>* createComputation(uint64_t gss) const = 0;

  std::string const& getName() const { return _name; }

 protected:
  Algorithm(std::string const& name) : _name(name){};

 private:
  std::string _name;
};

template <typename V, typename E, typename M>
class SimpleAlgorithm : public Algorithm<V, E, M> {
 protected:
  std::string _sourceField, _resultField;

  SimpleAlgorithm(std::string const& name, VPackSlice userParams)
      : Algorithm<V, E, M>(name) {
    VPackSlice field = userParams.get("sourceField");
    _sourceField = field.isString() ? field.copyString() : "value";
    field = userParams.get("resultField");
    _resultField = field.isString() ? field.copyString() : "result";
  }
};
}
}
#endif
