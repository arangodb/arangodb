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

/* Execute the given pregel program */
/* TODO parametrise with bindings?  */
function execute_program(graphName, program) {
  return pregel.start("vertexaccumulators", graphName, program);
}

function test_bind_parameter_program(bindParameter, value) {
  return {
    resultField: "bindParameterTest",
    accumulatorsDeclaration: {
      distance: {
        accumulatorType: "min",
        valueType: "doubles",
        storeSender: true,
      },
    },
    bindings: { "bind-test-name": "Hello, world" },
    initProgram: ["seq", ["print", ["bind-ref", "bind-test-name"]]],
    updateProgram: ["seq", ["print", "hallo"]],
  };
}

/* Performs a single-source shortest path search (currently without path reconstruction)
   on all vertices in the graph starting from startVertex, using the cost stored
   in weightAttribute on each edge and storing the end result in resultField as an object
   containing the attribute distance */

/*

  (for this -: edge :-> that
    (update that.distance with this.distance + edge.weightAttribute))

*/
function single_source_shortest_path_program(
  resultField,
  startVertexId,
  weightAttribute
) {
  return {
    resultField: resultField,
    // TODO: Karpott.
    maxGSS: 10000,
    accumulatorsDeclaration: {
      distance: {
        accumulatorType: "min",
        valueType: "doubles",
        storeSender: true,
      },
    },
    initProgram: [
      "seq",
      [
        "if",
        [
          ["eq?", ["this"], startVertexId],
          ["seq", ["set", "distance", 0], true],
        ],
        [true, ["seq", ["set", "distance", 9223372036854776000], false]],
      ],
    ],
    updateProgram: [
      "seq",
      [
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
            ["+", ["accum-ref", "distance"], ["attrib", weightAttribute, ["var-ref", "edge"]]],
          ],
        ],
      ],
      false,
    ],
  };
}

function single_source_shortest_path(
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
}

exports.single_source_shortest_path_program = single_source_shortest_path_program;
exports.single_source_shortest_path = single_source_shortest_path;
exports.test_bind_parameter_program = test_bind_parameter_program;

exports.execute = execute_program;
