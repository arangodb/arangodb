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

#ifndef ARANGODB_PREGEL_ALGOS_PAGERANK_H
#define ARANGODB_PREGEL_ALGOS_PAGERANK_H 1

#include <velocypack/Slice.h>
#include "Pregel/Algorithm.h"

namespace arangodb {
namespace pregel {
namespace algos {

/// PageRank
struct PageRank : public SimpleAlgorithm<double, float, double> {
  float _threshold;

 public:
  PageRank(arangodb::velocypack::Slice params);

  GraphFormat* inputFormat() const override {
    return new VertexGraphFormat<double, float>(_resultField, 0);
  }
  
  MessageFormat<double>* messageFormat() const override {
    return new DoubleMessageFormat();
  }
  
  MessageCombiner<double>* messageCombiner() const override {
    return new SumCombiner<double>();
  }
  
  VertexComputation<double, float, double>* createComputation(
      WorkerConfig const*) const override;
  IAggregator* aggregator(std::string const& name) const override;
};
}
}
}
#endif
