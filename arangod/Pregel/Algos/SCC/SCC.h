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

#include "Pregel/Algorithm.h"
#include "Pregel/Algos/SCC/SCCValue.h"
#include "Pregel/SenderMessage.h"
#include "Pregel/SenderMessageFormat.h"

namespace arangodb::pregel::algos {

/** Finds strongly connected components of the graph.
 *
 * The algorithm is a simplification of the algorithm given in
 * Da Yan, James Cheng, Kai Xing, Yi Lu, Wilfred Ng, Yingyi Bu,
 * "Pregel Algorithms for Graph Connectivity Problems with Performance
 * Guarantees", Proceedings of the VLDB Endowment, Volume 7, Issue 14, October
 * 2014, pp. 1821–1832, http://www.vldb.org/pvldb/vol7/p1821-yan.pdf, see
 * Section 6.1.
 *
 * 1. Each vertex starts with its vertex id as its "color".
 * 2. Remove vertices which cannot be in an SCC (no incoming or no outgoing
 * edges).
 * 3. Propagate the color forward from each vertex, accept a predecessor's color
 *    if it's smaller than yours. For the propagation, a vertex sends its color
 *    to all its out-neighbors.
 *    When the fixed point is reached, vertices with the same color are exactly
 *    those reachable from the root of that color. Each vertex obtains the
 *    least color of a vertex from that it is reachable.
 * 4. Start at all vertices whose color did not change and propagate its color
 *    backwards as long as the color does not change. For the propagation, a
 *    vertex that received colors from its out-neighbors sends its color to all
 *    its in-neighbors and becomes inactive.

 *    When the fixed point is reached, every SCC that cannot be reached from a
 *    vertex with an Id smaller than all Ids in the SCC is identified: all its
 *    vertices are inactive (and will not become active any more) and they all
 *    have the same color. The colors of all other vertices will be reset in the
 *    next round and will never become the color of the SCC.
 *
 *    If there are SCCs that can be reached from a vertex with a smaller Id,
 *    their vertices are active and the computation is repeated only for the
 *    active vertices. For this, the algorithm goes to Step 1.
 *
 *    Otherwise, the algorithm terminates.
 *
 * The runtime measured in the number of elementary operations is in
 * O((n+m)^2), the number of global super steps is in O(n^2). The worst case
 * happens if in each iteration (from Step 1 to Step 4), the least Id is in the
 * SCC from that all other SCCs are reachable. If we assume that the
 * distribution of Ids is sufficiently random (at the moment they are set
 * vertex by vertex to the value of an counter), that the average SCC size
 * is k, and that the average size of the subgraph reachable from a vertex is a
 * fixed portion of the whole graph, the expected number of elementary
 * operations decreases to
 * O((n+m)^2 / (k * log(n+m)))
 * and the number of global super steps to
 * O(n^2 / (k * log(n))).
 *
 * (We find, in the first iteration, the SCC C_0 whose
 * least Id is minimum among all Ids in the graph and we find no SCCs reachable
 * from C_0. The reachable part is, on average, the half of the graph, actually,
 * a fixed portion of the graph. In the other half, we, recursively, would find
 * C_1, the SCC with the second least Id - among those not reachable from C_0 -,
 * and so on, having, on average, a depth of log(n) of those C_i. Thus in one
 * iteration we deactivate O(k * log(n)) vertices.)
 *
 * Possible improvements:
 * (1) Remove edges between found SCCs and the remainder (according to the paper
 *      mentioned above).
 * (2) Correctly randomize vertex Ids.
 * (3) Propagate also a color backwards (as suggested in the paper).
 */

struct SCC : public SimpleAlgorithm<SCCValue, int8_t, SenderMessage<uint64_t>> {
 public:
  explicit SCC(application_features::ApplicationServer& server,
               VPackSlice userParams)
      : SimpleAlgorithm<SCCValue, int8_t, SenderMessage<uint64_t>>(
            server, "scc", userParams) {}

  [[nodiscard]] GraphFormat<SCCValue, int8_t>* inputFormat() const override;
  [[nodiscard]] MessageFormat<SenderMessage<uint64_t>>* messageFormat()
      const override {
    return new SenderMessageFormat<uint64_t>();
  }

  VertexComputation<SCCValue, int8_t, SenderMessage<uint64_t>>*
  createComputation(WorkerConfig const*) const override;

  [[nodiscard]] MasterContext* masterContext(
      VPackSlice userParams) const override;

  [[nodiscard]] IAggregator* aggregator(std::string const& name) const override;
};
}  // namespace arangodb::pregel::algos
