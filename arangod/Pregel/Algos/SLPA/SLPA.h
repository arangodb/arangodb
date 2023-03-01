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

#include <cmath>
#include "Pregel/Algorithm.h"
#include "Pregel/Algos/SLPA/SLPAValue.h"

namespace arangodb::pregel::algos {

/// SLPA algorithm:
/// Overlap is one of the characteristics of social
/// networks, in which a person may belong to more than one social
/// group. For this reason, discovering overlapping structure
/// is  necessary  for  realistic  social  analysis.  In the SLPA algorithm
/// nodes  exchange  labels  according  to  dynamic
/// interaction  rules.  It has excellent performance in identifying both
/// overlapping
/// nodes and  overlapping  communities  with  different  degrees  of diversity.

/**
 * From SLPA: Jierui Xie, Boleslaw K. Szymanski, Xiaoming Liu. Uncovering
 * Overlapping Communities in Social Networks via A Speaker-listener Interaction
 * Dynamic Process, 2011.
 *
 * The algorithm should actually be the following. In the paper, it is kept
 * much more general. Our intention of the implementation of the places
 * described generally in the paper is based on the ArangoDB's documentation.
 *
 * The algorithm is performed in a series of iterations during that vertices
 * send vertex IDs (natural numbers) to each other. Each vertex contains all
 * vertex IDs it has received and chosen since the beginning together with the
 * information how often an ID has been received and chosen. If na vertexId was
 * received and chosen, we say, it was saved. At the beginning, a vertex
 * contains only its own ID that was saved once. In an iteration, the vertices
 * are processed in a random order, which is chosen for each iteration
 * separately. A vertex v gets from each of its in-neighbors one of their saved
 * IDs. Which of the saved IDs is sent by an in-neighbor w is chosen randomly
 * with the probability
 *
 * <number of times the ID was saved by w> / <number of times any ID
 * was saved by w>.
 *
 * Vertex v chooses one of the received IDs to save in the
 * iteration and discards the others: it chooses the least ID out of those that
 * arrived most often. For example, if v received (3, 3, 2, 4, 4), it chooses 3.
 *
 * The number of iterations is an input parameter. Another input parameter is a
 * real number R from the interval (0,1]. Also let D be the number of all IDs
 * received by all vertices in all iterations (which is the number of all sent
 * IDs in total). After all iterations are done, each vertex returns all its
 * saved IDs filtered as follows. If an ID was received by the vertex X times
 * and X / D >= R, the ID is returned, otherwise not.
 *
 * Our implementation is quite different:
 *
 * (1) In an iteration, a vertex obtains messages not necessarily but with a
 * certain probability: (rnd() + this.ID) % 2 == gss % 2. This seems to be just
 * the probability of 1/2 but, in addition, whether the outcome is yes or no
 * correlates between different vertices. (Btw, the messages are sent anyway,
 * but if the outcome is no, they are not processed.)
 *
 * (2) A vertex always sends its own ID (why?!) and sometimes a saved ID that is
 * chosen as follows. Generate a random number r between 0 and the number of
 * times the vertex received any IDs (i.e., the sum over all IDs of the numbers
 * of times an ID was received). Now iterate (always in the same order) over the
 * saved IDs of the vertex and trace the sum of times they were saved. When
 * this accumulated sum reaches r (the randomly generated number), choose the
 * current ID and send it.
 */
struct SLPA : public SimpleAlgorithm<SLPAValue, int8_t, uint64_t> {
  double _threshold = 0.15;
  unsigned _maxCommunities = 1;

 public:
  explicit SLPA(application_features::ApplicationServer& server,
                VPackSlice userParams)
      : SimpleAlgorithm<SLPAValue, int8_t, uint64_t>(server, "slpa",
                                                     userParams) {
    arangodb::velocypack::Slice val = userParams.get("threshold");
    if (val.isNumber()) {
      _threshold = std::min(1.0, std::max(val.getDouble(), 0.0));
    }
    val = userParams.get("maxCommunities");
    if (val.isInteger()) {
      _maxCommunities = (unsigned)std::min(
          (uint64_t)32, std::max(val.getUInt(), (uint64_t)0));
    }
  }

  GraphFormat<SLPAValue, int8_t>* inputFormat() const override;
  MessageFormat<uint64_t>* messageFormat() const override {
    return new NumberMessageFormat<uint64_t>();
  }

  VertexComputation<SLPAValue, int8_t, uint64_t>* createComputation(
      WorkerConfig const*) const override;
  WorkerContext* workerContext(velocypack::Slice userParams) const override;
};
}  // namespace arangodb::pregel::algos
