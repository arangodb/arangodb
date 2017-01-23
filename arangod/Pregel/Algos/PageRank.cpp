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

#include "PageRank.h"
#include "Pregel/Aggregator.h"
#include "Pregel/GraphFormat.h"
#include "Pregel/Iterators.h"
#include "Pregel/MasterContext.h"
#include "Pregel/Utils.h"
#include "Pregel/VertexComputation.h"

#include "Cluster/ClusterInfo.h"
#include "Utils/OperationCursor.h"
#include "Utils/SingleCollectionTransaction.h"
#include "Utils/StandaloneTransactionContext.h"
#include "Utils/Transaction.h"
#include "VocBase/vocbase.h"

using namespace arangodb;
using namespace arangodb::pregel;
using namespace arangodb::pregel::algos;

static std::string const kConvergence = "convergence";

PageRank::PageRank(arangodb::velocypack::Slice params)
    : SimpleAlgorithm("PageRank", params) {
  VPackSlice t = params.get("convergenceThreshold");
  _threshold = t.isNumber() ? t.getNumber<double>() : 0.000002f;
}

struct PRComputation : public VertexComputation<double, float, double> {
  double _limit;
  PRComputation(double t) : _limit(t) {}
  void compute(MessageIterator<double> const& messages) override {
    double* ptr = mutableVertexData();
    double copy = *ptr;
    // TODO do initialization in GraphFormat?
    if (globalSuperstep() == 0) {
      *ptr = 1.0 / context()->vertexCount();
    } else if (globalSuperstep() > 0) {
      double sum = 0.0;
      for (const double* msg : messages) {
        sum += *msg;
      }
      *ptr = 0.85 * sum + 0.15 / context()->vertexCount();
    }
    double diff = fabs(copy - *ptr);
    aggregate(kConvergence, diff);
    
    if (diff > _limit) {
      RangeIterator<Edge<float>> edges = getEdges();
      double val = *ptr / edges.size();
      for (Edge<float>* edge : edges) {
        sendMessage(edge, val);
      }
    } else {
      voteHalt();
    }
  }
};

VertexComputation<double, float, double>* PageRank::createComputation(
    WorkerConfig const* config) const {
  return new PRComputation(_threshold);
}

IAggregator* PageRank::aggregator(std::string const& name) const {
  if (name == kConvergence) {
    return new MaxAggregator<double>(-1.0f, false);
  }
  return nullptr;
}
