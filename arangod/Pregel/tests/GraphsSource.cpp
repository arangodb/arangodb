////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Roman Rabinovich
////////////////////////////////////////////////////////////////////////////////

#include <Basics/VelocyPackStringLiteral.h>
#include "GraphsSource.h"
#include "Inspection/VPackPure.h"
#include "fmt/format.h"

using namespace arangodb::velocypack;
using namespace arangodb::slicegraph;

SharedSlice arangodb::slicegraph::setup2Path() {
  return
      R"({ "vertices": [ {"_key": "A", "value": 5},
                         {"_key": "B", "value": 10},
                         {"_key": "C", "value": 15} ],
           "numVertices": 3,
           "edges":    [ {"_key": "", "_from": "A", "_to": "B"},
                         {"_key": "", "_from": "B", "_to": "C"} ],
           "numEdges": 2 })"_vpack;
}

SharedSlice arangodb::slicegraph::setupUndirectedEdge() {
  return
      R"({ "vertices": [ {"_key": "A", "value": 5},
                         {"_key": "B", "value": 10} ],
           "numVertices": 2,
           "edges":    [ {"_key": "", "_from": "A", "_to": "B"},
                         {"_key": "", "_from": "B", "_to": "A"} ],
           "numEdges": 2 })"_vpack;
}

SharedSlice arangodb::slicegraph::setupThreeDisjointDirectedCycles() {
  return
      R"({ "vertices": [ {"_key": "a0", "value": 0},
                         {"_key": "a1", "value": 0},
                         {"_key": "a2", "value": 0},
                         {"_key": "b0", "value": 0},
                         {"_key": "b1", "value": 0},
                         {"_key": "b2", "value": 0},
                         {"_key": "b3", "value": 0},
                         {"_key": "c0", "value": 0},
                         {"_key": "c1", "value": 0},
                         {"_key": "c2", "value": 0},
                         {"_key": "c3", "value": 0},
                         {"_key": "c4", "value": 0}],
           "numVertices": 12,
           "edges":    [ {"_key": "", "_from": "a0", "_to": "a1"},
                         {"_key": "", "_from": "a1", "_to": "a2"},
                         {"_key": "", "_from": "a2", "_to": "a0"},
                         {"_key": "", "_from": "b0", "_to": "b1"},
                         {"_key": "", "_from": "b1", "_to": "b2"},
                         {"_key": "", "_from": "b2", "_to": "b3"},
                         {"_key": "", "_from": "b3", "_to": "b0"},
                         {"_key": "", "_from": "c0", "_to": "c1"},
                         {"_key": "", "_from": "c1", "_to": "c2"},
                         {"_key": "", "_from": "c2", "_to": "c3"},
                         {"_key": "", "_from": "c3", "_to": "c4"},
                         {"_key": "", "_from": "c4", "_to": "c0"} ],
           "numEdges": 12 })"_vpack;
}

SharedSlice arangodb::slicegraph::setupThreeDisjointAlternatingCycles() {
  return
      R"({ "vertices": [ {"_key": "a0", "value": 0},
                         {"_key": "a1", "value": 0},
                         {"_key": "a2", "value": 0},
                         {"_key": "b0", "value": 0},
                         {"_key": "b1", "value": 0},
                         {"_key": "b2", "value": 0},
                         {"_key": "b3", "value": 0},
                         {"_key": "c0", "value": 0},
                         {"_key": "c1", "value": 0},
                         {"_key": "c2", "value": 0},
                         {"_key": "c3", "value": 0},
                         {"_key": "c4", "value": 0}],
           "numVertices": 12,
           "edges":    [ {"_key": "", "_from": "a0", "_to": "a1"},
                         {"_key": "", "_from": "a2", "_to": "a1"},
                         {"_key": "", "_from": "a2", "_to": "a0"},
                         {"_key": "", "_from": "b0", "_to": "b1"},
                         {"_key": "", "_from": "b2", "_to": "b1"},
                         {"_key": "", "_from": "b2", "_to": "b3"},
                         {"_key": "", "_from": "b0", "_to": "b3"},
                         {"_key": "", "_from": "c0", "_to": "c1"},
                         {"_key": "", "_from": "c2", "_to": "c1"},
                         {"_key": "", "_from": "c2", "_to": "c3"},
                         {"_key": "", "_from": "c4", "_to": "c3"},
                         {"_key": "", "_from": "c0", "_to": "c4"} ],
           "numEdges": 12 })"_vpack;
}

SharedSlice arangodb::slicegraph::setup1SingleVertex() {
  return
      R"({ "vertices": [ {"_key": "A", "value": 0} ],
           "numVertices": 1,
           "edges":    [ ],
           "numEdges": 0 })"_vpack;
}

SharedSlice arangodb::slicegraph::setup2IsolatedVertices() {
  return
      R"({ "vertices": [ {"_key": "A", "value": 0},
                         {"_key": "B", "value": 0} ],
           "numVertices": 2,
           "edges":    [ ],
           "numEdges": 0 })"_vpack;
}

SharedSlice arangodb::slicegraph::setup1DirectedTree() {
  return
      R"({ "vertices": [ {"_key": "a", "value": 5},
                         {"_key": "a0", "value": 10},
                         {"_key": "a1", "value": 15},
                         {"_key": "a00", "value": 10},
                         {"_key": "a01", "value": 15},
                         {"_key": "a10", "value": 10},
                         {"_key": "a11", "value": 15},
                         {"_key": "a000", "value": 10},
                         {"_key": "a001", "value": 15},
                         {"_key": "a010", "value": 10},
                         {"_key": "a011", "value": 15},
                         {"_key": "a100", "value": 10},
                         {"_key": "a101", "value": 15},
                         {"_key": "a110", "value": 10},
                         {"_key": "a111", "value": 15} ],
           "numVertices": 15,
           "edges":   [ {"_key": "", "_from": "a", "_to": "a0"},
                        {"_key": "", "_from": "a", "_to": "a1"},
                        {"_key": "", "_from": "a0", "_to": "a00"},
                        {"_key": "", "_from": "a0", "_to": "a01"},
                        {"_key": "", "_from": "a1", "_to": "a10"},
                        {"_key": "", "_from": "a1", "_to": "a11"},
                        {"_key": "", "_from": "a00", "_to": "a000"},
                        {"_key": "", "_from": "a00", "_to": "a001"},
                        {"_key": "", "_from": "a01", "_to": "a010"},
                        {"_key": "", "_from": "a01", "_to": "a011"},
                        {"_key": "", "_from": "a10", "_to": "a100"},
                        {"_key": "", "_from": "a10", "_to": "a101"},
                        {"_key": "", "_from": "a11", "_to": "a110"},
                        {"_key": "", "_from": "a11", "_to": "a111"} ],
           "numEdges": 14 })"_vpack;
}

SharedSlice arangodb::slicegraph::setup1AlternatingTree() {
  return
      R"({ "vertices": [ {"_key": "a", "value": 5},
                         {"_key": "a0", "value": 10},
                         {"_key": "a1", "value": 15},
                         {"_key": "a00", "value": 10},
                         {"_key": "a01", "value": 15},
                         {"_key": "a10", "value": 10},
                         {"_key": "a11", "value": 15},
                         {"_key": "a000", "value": 10},
                         {"_key": "a001", "value": 15},
                         {"_key": "a010", "value": 10},
                         {"_key": "a011", "value": 15},
                         {"_key": "a100", "value": 10},
                         {"_key": "a101", "value": 15},
                         {"_key": "a110", "value": 10},
                         {"_key": "a111", "value": 15} ],
           "numVertices": 15,
           "edges":    [ {"_key": "", "_from": "a", "_to": "a0"},
                         {"_key": "", "_from": "a", "_to": "a1"},
                         {"_key": "", "_from": "a00", "_to": "a0"},
                         {"_key": "", "_from": "a01", "_to": "a0"},
                         {"_key": "", "_from": "a10", "_to": "a1"},
                         {"_key": "", "_from": "a11", "_to": "a1"},
                         {"_key": "", "_from": "a00", "_to": "a000"},
                         {"_key": "", "_from": "a00", "_to": "a001"},
                         {"_key": "", "_from": "a01", "_to": "a010"},
                         {"_key": "", "_from": "a01", "_to": "a011"},
                         {"_key": "", "_from": "a10", "_to": "a100"},
                         {"_key": "", "_from": "a10", "_to": "a101"},
                         {"_key": "", "_from": "a11", "_to": "a110"},
                         {"_key": "", "_from": "a11", "_to": "a111"} ],
           "numEdges": 14 })"_vpack;
}

SharedSlice arangodb::slicegraph::setup2CliquesConnectedByDirectedEdge() {
  return
      R"({ "vertices": [ {"_key": "a0", "value": 5},
                         {"_key": "a1", "value": 10},
                         {"_key": "a2", "value": 15},
                         {"_key": "b0", "value": 5},
                         {"_key": "b1", "value": 10},
                         {"_key": "b2", "value": 15} ],
           "numVertices": 6,
           "edges":    [ {"_key": "", "_from": "a0", "_to": "a1"},
                         {"_key": "", "_from": "a1", "_to": "a0"},
                         {"_key": "", "_from": "a0", "_to": "a2"},
                         {"_key": "", "_from": "a2", "_to": "a0"},
                         {"_key": "", "_from": "a1", "_to": "a2"},
                         {"_key": "", "_from": "a2", "_to": "a1"},
                         {"_key": "", "_from": "b0", "_to": "b1"},
                         {"_key": "", "_from": "b1", "_to": "b0"},
                         {"_key": "", "_from": "b0", "_to": "b2"},
                         {"_key": "", "_from": "b2", "_to": "b0"},
                         {"_key": "", "_from": "b1", "_to": "b2"},
                         {"_key": "", "_from": "b2", "_to": "b1"},
                         {"_key": "", "_from": "a0", "_to": "b0"} ],
           "numEdges": 13 })"_vpack;
}

SharedSlice arangodb::slicegraph::setupDuplicateVertices() {
  return
      R"({ "vertices": [ {"_key": "A", "value": 5},
                         {"_key": "A", "value": 10} ],
           "numVertices": 2,
           "edges":    [ ],
           "numEdges": 0 })"_vpack;
}

auto GraphSliceHelper::getNumVertices(SharedSlice const& graphSlice) -> size_t {
  size_t expectedNumVertices;
  {
    auto result = deserializeWithStatus<size_t>(
        graphSlice.slice()["numVertices"], expectedNumVertices);
    if (not result.ok()) {
      throw std::runtime_error(
          fmt::format("Could not get numVertices from the graph slice {}",
                      graphSlice.toString()));
    }
  }
  return expectedNumVertices;
}

auto GraphSliceHelper::getNumEdges(SharedSlice const& graphSlice) -> size_t {
  size_t expectedNumEdges;
  {
    auto result = deserializeWithStatus<size_t>(
        graphSlice.slice()["numEdges"], expectedNumEdges);
    if (not result.ok()) {
      throw std::runtime_error(
          fmt::format("Could not get numEdges from the graph slice {}",
                      graphSlice.toString()));
    }
  }
  return expectedNumEdges;
}