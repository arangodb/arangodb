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

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>
#include <cstdint>

#include "Aggregator.h"
#include "Basics/Common.h"
#include "GraphFormat.h"
#include "MessageCombiner.h"
#include "MessageFormat.h"

namespace arangodb {
namespace pregel {

template <typename V, typename E, typename M>
class VertexComputation;

// specify serialization, whatever
template <typename V, typename E, typename M>
struct Algorithm {
 public:
  virtual ~Algorithm() {}
  // virtual bool isFixpointAlgorithm() const {return false;}
  // virtual bool preserveTransactions() const { return false; }

  virtual size_t estimatedVertexSize() const { return sizeof(V); };
  virtual size_t estimatedEdgeSize() const { return sizeof(E); };
  virtual void aggregators(
      std::vector<std::unique_ptr<Aggregator>>& aggregators) {}

  virtual std::shared_ptr<GraphFormat<V, E>> inputFormat() const = 0;
  virtual std::shared_ptr<MessageFormat<M>> messageFormat() const = 0;
  virtual std::shared_ptr<MessageCombiner<M>> messageCombiner() const = 0;
  virtual std::shared_ptr<VertexComputation<V, E, M>> createComputation(uint64_t gss)
      const = 0;

  std::string const& getName() const { return _name; }

 protected:
  Algorithm(std::string const& name) : _name(name){};

 private:
  std::string _name;
};
}
}
#endif
