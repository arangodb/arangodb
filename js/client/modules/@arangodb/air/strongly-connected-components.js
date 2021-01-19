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
const examplegraphs = require("@arangodb/air/pregel-example-graphs");
const testhelpers = require("@arangodb/air/test-helpers");
const {listAccumulator, orAccumulator, storeAccumulator, minAccumulator} = require("@arangodb/air/accumulators");
const _ = require("lodash");
const db = internal.db;

// ee/ce check + gm selection
const isEnterprise = require("internal").isEnterprise();
const graphModule = isEnterprise? require("@arangodb/smart-graph") : require("@arangodb/general-graph");

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
        valueType: "any",
        customType: "or"
      },
    },
    vertexAccumulators: {
      forwardMin: {
        accumulatorType: "custom",
        valueType: "any",
        customType: "min"
      },
      backwardMin: {
        accumulatorType: "custom",
        valueType: "any",
        customType: "min"
      },
      isDisabled: {
        accumulatorType: "custom",
        valueType: "any",
        customType: "store"
      },
      mySCC: {
        accumulatorType: "custom",
        valueType: "any",
        customType: "store"
      },
      activeInbound: {
        accumulatorType: "custom",
        valueType: "any",
        customType: "list"
      },
    },
    customAccumulators: {
      or: orAccumulator(),
      list: listAccumulator(),
      store: storeAccumulator(false),
      min: minAccumulator(),
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
        initProgram: ["seq",
          ["accum-set!", "activeInbound", ["quote", []]],
          ["if",
            [
              ["not", ["accum-ref", "isDisabled"]],
              ["send-to-all-neighbours", "activeInbound", ["this-pregel-id"]]
            ],
          ], // else
          "vote-halt",
        ],
        updateProgram: "vote-halt",
        onHalt: ["seq",
          ["global-accum-clear!", "converged"],
          ["goto-phase", "forward"]]
      },
      {
        name: "forward",
        initProgram: ["seq",
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
            [["not", ["global-accum-ref", "converged"]],
              ["seq",
                ["finish"]]],
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
                [["vertex", ["accum-ref", "activeInbound"]]],
                ["send-to-accum",
                  "backwardMin",
                  ["var-ref", "vertex"],
                  ["accum-ref", "backwardMin"]]],
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
    "ppa",
    graphName,
    strongly_connected_components_program(resultField)
  );
}

function exec_test_scc_on_graph(graphSpec, components = []) {
  let status = testhelpers.wait_for_pregel(
    "Air Strongly Connected Components",
    strongly_connected_components(graphSpec.name, "SCC")
  );

  // TODO: Verify SSC result
  if (status.state === "fatal error") {
    return false;
  } else {

    const found_components = db._query(`
      for doc in @@graph
        collect scc = doc.SCC.mySCC WITH COUNT INTO size
        sort size
        return size`,
      {"@graph": graphSpec.vname}).toArray();

    if (_.isEqual(components, found_components)) {
      internal.print("\u001b[32mOK  : found ", components.length, " components", "\u001b[0m");
      return true;
    } else {
      internal.print("\u001b[31mFAIL: scc's not as expected: ", found_components, " expected: ", components, "\u001b[0m");
      return false;
    }
  }
}

function exec_test_scc() {
  let results = [];
  results.push(exec_test_scc_on_graph(examplegraphs.create_complete_graph("testComplete_5shard", 5), [100]));
  try {
    graphModule._drop("testComplete_5shard", true);
  } catch (ignore) {
  }

  results.push(exec_test_scc_on_graph(examplegraphs.create_circle_graph("Circle10", 10, 3), [10]));
  try {
    graphModule._drop("Circle10", true);
  } catch (ignore) {
  }

  results.push(exec_test_scc_on_graph(examplegraphs.create_tadpole_graph("Tadpole", 10, 3), [1, 1, 1, 1, 6]));
  try {
    graphModule._drop("Tadpole", true);
  } catch (ignore) {
  }

  results.push(exec_test_scc_on_graph(examplegraphs.create_line_graph("LineGraph10", 10, 3), [1, 1, 1, 1, 1, 1, 1, 1, 1, 1]));
  try {
    graphModule._drop("LineGraph10", true);
  } catch (ignore) {
  }


  return !results.includes(false);
}

function test() {
  return exec_test_scc();
}
