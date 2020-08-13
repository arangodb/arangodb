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
/*


*/
exports.strongly_connected_components_program = strongly_connected_components_program;
exports.strongly_connected_components = strongly_connected_components;

function strongly_connected_components_program(resultField) {
  return {
    resultField: resultField,
    // TODO: Karpott.
    maxGSS: 5000,
    globalAccumulators: {
      converged: {
        accumulatorType: "or",
        valueType: "bool",
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
    phases: [
      {
        name: "init",
        initProgram: ["seq", ["set", "isDisabled", false], false],
        onHalt: ["seq", ["set", "converged", false], ["goto", "broadcast"]],
        updateProgram: false,
      },
      {
        name: "broadcast",
        initProgram: [
          "seq",
          ["set", "activeInbound", ["quote"]],
          //["print", "isDisabled =", ["accum-ref", "isDisabled"]],
          [
            "if",
            [
              ["not", ["accum-ref", "isDisabled"]],
              [
                "for",
                "outbound",
                ["quote", "edge"],
                [
                  "quote",
                  "seq",
                  //["print", ["vertex-unique-id"], "sending to vertex", ["attrib", "_to", ["var-ref", "edge"]], "with value", ["pregel-id"]],
                  [
                    "update",
                    "activeInbound",
                    ["attrib", "_to", ["var-ref", "edge"]],
                    [
                      "dict",
                      ["list", "pid", ["pregel-id"]],
                      ["list", "unique-id", ["vertex-unique-id"]],
                    ],
                  ],
                ],
              ],
            ],
          ],
          false,
        ],
        updateProgram: false,
      },
      {
        name: "forward",
        initProgram: [
          "if",
          [["accum-ref", "isDisabled"], false],
          [
            true, // else
            ["seq", ["set", "forwardMin", ["vertex-unique-id"]], true],
          ],
        ],
        updateProgram: [
          "if",
          [["accum-ref", "isDisabled"], false],
          [
            true, // else
            [
              "seq",
              [
                "for",
                "outbound",
                ["quote", "edge"],
                [
                  "quote",
                  "seq",
                  //["print", ["vertex-unique-id"], "sending to vertex", ["attrib", "_to", ["var-ref", "edge"]], "with value", ["accum-ref", "forwardMin"]],
                  [
                    "update",
                    "forwardMin",
                    ["attrib", "_to", ["var-ref", "edge"]],
                    ["accum-ref", "forwardMin"],
                  ],
                ],
              ],
              false,
            ],
          ],
        ],
      },
      {
        name: "backward",
        onHalt: [
          "seq",
          ["print", "converged has value", ["accum-ref", "converged"]],
          [
            "if",
            [
              ["accum-ref", "converged"],
              ["seq", ["set", "converged", false], ["goto", "broadcast"]],
            ],
            [true, ["finish"]],
          ],
        ],
        initProgram: [
          "if",
          [
            ["accum-ref", "isDisabled"],
            ["seq", ["print", ["vertex-unique-id"], "is disabled"], false],
          ],
          [
            ["eq?", ["vertex-unique-id"], ["accum-ref", "forwardMin"]],
            [
              "seq",
              ["print", ["vertex-unique-id"], "I am root of a SCC!"],
              ["set", "backwardMin", ["accum-ref", "forwardMin"]],
              true,
            ],
          ],
          [
            true,
            [
              "seq",
              ["print", ["vertex-unique-id"], "I am _not_ root of a SCC"],
              ["set", "backwardMin", 99999],
              false,
            ],
          ],
        ],
        updateProgram: [
          "if",
          [
            ["accum-ref", "isDisabled"],
            ["seq", ["print", "I am disabled"], false],
          ],
          [
            ["eq?", ["accum-ref", "backwardMin"], ["accum-ref", "forwardMin"]],
            [
              "seq",
              ["set", "isDisabled", true],
              ["update", "converged", "", true],
              ["set", "mySCC", ["accum-ref", "forwardMin"]],
              ["print", "I am done, my SCC id is", ["accum-ref", "forwardMin"]],
              [
                "for-each",
                ["vertex", ["accum-ref", "activeInbound"]],
                [
                  "seq",
                  [
                    "print",
                    ["vertex-unique-id"],
                    "sending to vertex",
                    ["var-ref", "vertex"],
                    "with value",
                    ["accum-ref", "backwardMin"],
                  ],
                  [
                    "update-by-id",
                    "backwardMin",
                    ["attrib", "pid", ["var-ref", "vertex"]],
                    ["accum-ref", "backwardMin"],
                  ],
                ],
              ],
              false,
            ],
          ],
          [true, ["seq", ["print", "Was woken up, but min_f != min_b"], false]],
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
  wait_for_pregel(
    "Air Strongly Connected Components",
    strongly_connected_components(
      graphSpec.name,
      "SCC"
    ));


  return pp.strongly_connected_components("Circle", "scc");
}

function exec_test_scc() {
  exec_test_scc_on_graph(examplegraphs.create_circle("Circle", 5));
  exec_test_scc_on_graph(examplegraphs.create_line_graph("LineGraph100", 100, 1));
}

function test() {
  exec_test_scc();
}
