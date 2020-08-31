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

const pregel = require("@arangodb/pregel");
const examplegraphs = require("@arangodb/air/pregel-example-graphs");
const testhelpers = require("@arangodb/air/test-helpers");
const accumulators = require("@arangodb/air/accumulators");
/*


*/
exports.strongly_connected_components_program = strongly_connected_components_program;
exports.strongly_connected_components = strongly_connected_components;
exports.test = test;

function strongly_connected_components_program(resultField) {
  return {
    resultField: resultField,
    // TODO: Karpott.
    maxGSS: 150,
    globalAccumulators: {
      // Converged is signalle by a vetex that found
      // that its min_f = min_b and hence determined its
      // SCC
      converged: {
        accumulatorType: "custom",
        valueType: "slice",
        customType: "or"
      },
    },
    vertexAccumulators: {
      forwardMin: {
        accumulatorType: "min",
        valueType: "ints",
      },
      backwardMin: {
        accumulatorType: "min",
        valueType: "ints",
      },
      isDisabled: {
        accumulatorType: "store",
        valueType: "bool",
      },
      mySCC: {
        accumulatorType: "store",
        valueType: "ints",
      },
      activeInbound: {
        accumulatorType: "list",
        valueType: "slice",
      },
    },
    customAccumulators: {
      or: accumulators.orAccumulator(),
    },
    phases: [
      {
        name: "init",
        initProgram: ["seq",
                      ["accum-set!", "isDisabled", false],
                      "vote-halt"],
        updateProgram: "vote-halt",
      },
      {
        // Active vertices broadcast their pregel id to all their neighbours
        name: "broadcast",
        initProgram: [ "seq",
                       ["accum-set!", "activeInbound", ["quote"]],
                       ["if",
                        [ ["not", ["accum-ref", "isDisabled"]],
                          ["send-to-all-neighbours", "activeInbound", ["this-pregel-id"]]],
                        [true, "vote-halt"]], // else
                       "vote-halt",
                     ],
        updateProgram: "vote-halt",
        onHalt: ["seq",
                 ["global-accum-clear!", "converged"],
                 ["goto-phase", "forward"]]
      },
      {
        name: "forward",
        initProgram: [ "seq",
                       ["if",
                        [["accum-ref", "isDisabled"], "vote-halt"],
                        [true, // else
                         ["seq",
                          ["accum-set!", "forwardMin", ["this-unique-id"]],
                          "vote-active"],
                        ],
                       ]],
        updateProgram: [
          "if",
          [["accum-ref", "isDisabled"], "vote-halt"],
          [true, // else
           ["seq",
            ["send-to-all-neighbours", "forwardMin", ["accum-ref", "forwardMin"]],
            "vote-halt"
           ]],
        ],
      },
      {
        name: "backward",
        // onHalt runs on the coordinator when all vertices have "vote-halt"-ed
        // in backwards propagation
        // TODO: make sure the global accumulator has been accumulated before this program
        //       runs
        onHalt: [
          "seq",
          ["if",
           [ ["not", ["global-accum-ref", "converged"]],
             ["seq",
              ["finish"] ] ],
           [true, ["seq",
                   ["goto-phase", "broadcast"]]],
          ],
        ],
        initProgram: ["seq",
          ["if",
           [["accum-ref", "isDisabled"], "vote-halt"],
           [["eq?", ["this-unique-id"], ["accum-ref", "forwardMin"]],
            ["seq",
             ["accum-set!", "backwardMin", ["accum-ref", "forwardMin"]],
             "vote-active"]],
           [true, ["seq", "vote-halt"]],
        ]],
        updateProgram: [
          "if",
          [["accum-ref", "isDisabled"], "vote-halt"],
          [["eq?", ["accum-ref", "backwardMin"], ["accum-ref", "forwardMin"]],
            ["seq",
             ["accum-set!", "isDisabled", true],
             ["accum-set!", "mySCC", ["accum-ref", "forwardMin"]],
             ["for-each",
              ["vertex", ["accum-ref", "activeInbound"]],
              ["seq", [
                "send-to-accum",
                ["var-ref", "vertex"],
                "backwardMin",
                ["accum-ref", "backwardMin"]]]],
             ["send-to-global-accum", "converged", true],
             "vote-halt",
            ],
          ],
          [true, ["seq",
                  "vote-halt"]],
        ],
      },
    ],
  };
}

function strongly_connected_components(graphName, resultField) {
  return pregel.start(
    "air",
    graphName,
    strongly_connected_components_program(resultField)
  );
}

function exec_test_scc_on_graph(graphSpec) {
  return testhelpers.wait_for_pregel(
    "Air Strongly Connected Components",
    strongly_connected_components(graphSpec.name, "SCC")
  );
}

function exec_test_scc() {
//    exec_test_scc_on_graph(examplegraphs.create_complete_graph("testComplete_5shard", 5));
  exec_test_scc_on_graph(examplegraphs.create_tadpole_graph("testTadpole_5shard", 5));
//  exec_test_scc_on_graph(examplegraphs.create_disjoint_circle_and_complete_graph("testCircleComplete_1shard", 1));
//  exec_test_scc_on_graph(examplegraphs.create_disjoint_circle_and_complete_graph("testCircleComplete_5shard", 5));
/*  exec_test_scc_on_graph(
    examplegraphs.create_line_graph("LineGraph100", 100, 1)
  ); */
}

function test() {
  exec_test_scc();
}
