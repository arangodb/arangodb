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
#include "Pregel/Algorithm.h"

namespace arangodb::pregel::algos {

/// LineRank from "Centralities in Large Networks: Algorithms and Observations"
/// 2011:
/// Given a directed graph G, the LINERANK score of a node v ∈ G is computed by
/// aggregating the stationary probabilities of its incident edges on the line
/// graph L(G).
/// Implementation based on
/// github.com/JananiC/NetworkCentralities/blob/master/src/main/java/linerank/LineRank.java

/**
 * Following the paper "Centralities in Large Networks: Algorithms and
 * Observations", linerank should be the following.
 *
 * Given a directed graph G = (V, E), compute the directed graph L(G) = (V_L,
 * E_L) as V_L = E (the new vertices are the old edges) and E_L = {((a,b),
 * (b,c)) : (a,b), (b,c) in E}, i.e. there is an edge from vertex (a,b) in V_L
 * to vertex (b,c) in V_L if there are edges (a,b) and (b,c) in the given graph.
 *
 * Now in L(G), we compute almost the pagerank, the only exception being that
 * when normalizing outgoing messages from a vertex, we divide by the total
 * number of edges in the graph (in G, not in L(G)) rather than by the number of
 * edges leaving the vertex (i.e., by the out-degree).
 *
 * After the values on each vertex converge, each vertex in L(G) and thus each
 * edge in G has a value, a rank. The final value of a vertex in G is the sum
 * of the values of all its in- and outgoing edges.
 *
 * In our implementation, we compute the normal pagerank except for the
 * following.
 *
 * (1) We initialize every vertex with 1/|E| (rather than with 1/|V|).
 * (2) We normalize outgoing messages from a vertex as for linerank (divide by
 *     the total number of edges in the graph, not by the out-degree of the
 *     vertex).
 * (3) In the last iteration, the value of a vertex is updated in another way:
 *     the new value is: (old value) * |E| + (sum of the incoming values).
 *
 * It seems that the values computed by both algorithms have not much to do with
 * each other.
 */

struct LineRank : public SimpleAlgorithm<float, float, float> {
 public:
  explicit LineRank(application_features::ApplicationServer& server,
                    arangodb::velocypack::Slice params);

  GraphFormat<float, float>* inputFormat() const override {
    return new VertexGraphFormat<float, float>(_server, _resultField, 0);
  }
  MessageFormat<float>* messageFormat() const override {
    return new NumberMessageFormat<float>();
  }

  MessageCombiner<float>* messageCombiner() const override {
    return new SumCombiner<float>();
  }

  WorkerContext* workerContext(velocypack::Slice params) const override;
  MasterContext* masterContext(velocypack::Slice) const override;

  VertexComputation<float, float, float>* createComputation(
      WorkerConfig const*) const override;

  IAggregator* aggregator(std::string const& name) const override;
};
}  // namespace arangodb::pregel::algos
