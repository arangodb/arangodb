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

using namespace arangodb::velocypack;

SharedSlice setup2Path() {
  return
      R"({ "vertices": [ {"_key": "A", "value": 5},
                         {"_key": "B", "value": 10},
                         {"_key": "C", "value": 15} ],
           "edges":    [ {"_key": "", "_from": "A", "_to": "B"},
                         {"_key": "", "_from": "B", "_to": "C"} ] })"_vpack;
}

SharedSlice setupUndirectedEdge() {
  return
      R"({ "vertices": [ {"_key": "A", "value": 5},
                         {"_key": "B", "value": 10} ],
           "edges":    [ {"_key": "", "_from": "A", "_to": "B"},
                         {"_key": "", "_from": "B", "_to": "A"} ] })"_vpack;
}

SharedSlice setupThreeDisjointDirectedCycles() {
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
                         {"_key": "", "_from": "c4", "_to": "c0"}] })"_vpack;
}

SharedSlice setupThreeDisjointAlternatingCycles() {
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
                         {"_key": "", "_from": "c0", "_to": "c4"}] })"_vpack;
}

SharedSlice setup1SingleVertex() {
  return
      R"({ "vertices": [ {"_key": "A", "value": 0} ],
           "edges":    [ ] })"_vpack;
}

SharedSlice setup2IsolatedVertices() {
  return
      R"({ "vertices": [ {"_key": "A", "value": 0},
                         {"_key": "B", "value": 0} ],
           "edges":    [ ] })"_vpack;
}

SharedSlice setup1DirectedTree() {
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
                        {"_key": "", "_from": "a11", "_to": "a111"} ] })"_vpack;
}

SharedSlice setup1AlternatingTree() {
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
                         {"_key": "", "_from": "a11", "_to": "a111"} ] })"_vpack;
}

SharedSlice setup2CliquesConnectedByDirectedEdge() {
  return
      R"({ "vertices": [ {"_key": "a0", "value": 5},
                         {"_key": "a1", "value": 10},
                         {"_key": "a2", "value": 15},
                         {"_key": "b0", "value": 5},
                         {"_key": "b1", "value": 10},
                         {"_key": "b2", "value": 15}
 ],
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
                         {"_key": "", "_from": "a0", "_to": "b0"} ] })"_vpack;
}

SharedSlice setupDuplicateVertices() {
  return
      R"({ "vertices": [ {"_key": "A", "value": 5},
                         {"_key": "A", "value": 10} ],
           "edges":    [ ] })"_vpack;
}