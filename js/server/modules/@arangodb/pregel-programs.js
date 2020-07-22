// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// /
// / Licensed under the Apache License, Version 2.0 (the "License")
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     http://www.apache.org/licenses/LICENSE-2.0
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
// / @author Heiko Kernbach
// / @author Lars Maier
// / @author Markus Pfeiffer
// / @author Copyright 2020, ArangoDB GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const internal = require("internal");
const pregel = require("@arangodb/pregel");

/* Performs a single-source shortest path search (currently without path reconstruction)
   on all vertices in the graph starting from startVertex, using the cost stored
   in weightAttribute on each edge and storing the end result in resultField as an object
   containing the attribute distance */
const single_source_shortest_path_program = function (
  resultField,
  startVertexId,
  weightAttribute
) {
  return {
    resultField: resultField,
    accumulatorsDeclaration: {
      distance: {
        accumulatorType: "min",
        valueType: "ints",
        storeSender: true,
      },
    },
    initProgram: [
      "seq",
      ["print", " this = ", ["this"]],
      [
        "if",
        [
          ["eq?", ["this"], startVertex],
          ["seq", ["set", "distance", 0], true],
        ],
        [true, ["seq", ["set", "distance", 999999], false]],
      ],
    ],
    updateProgram: [
      "for",
      "outbound",
      ["quote", "edge"],
      [
        "quote",
        "seq",
        [
          "update",
          "distance",
          ["attrib", "_to", ["var-ref", "edge"]],
          ["attrib", weightAttribute, ["var-ref", "edge"]],
        ],
      ],
    ],
  };
};

const single_source_shortest_path = function (
  graphName,
  resultField,
  startVertexId,
  weightAttribute
) {
  return pregel.start(
    "vertexaccumulators",
    graphName,
    single_source_shortest_path_program(
      resultField,
      startVertexId,
      weightAttribute
    )
  );
};

exports.single_source_shortest_path_program;
