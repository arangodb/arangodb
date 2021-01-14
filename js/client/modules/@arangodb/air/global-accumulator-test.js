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
const internal = require("internal");

// ee/ce check + gm selection
const isEnterprise = require("internal").isEnterprise();
const graphModule = isEnterprise? require("@arangodb/smart-graph") : require("@arangodb/general-graph");

exports.global_accumulators_test_program = global_accumulators_test_program;
exports.global_accumulators_test = global_accumulators_test;
exports.test = test;

// TODO these tests have to be more detailed, but less icky to write...
function test_custom_global_accumulator_not_defined() {
  internal.print("Testing: custom global accumulator declared");

  let finalResult = false;
  const tgname = "LineGraph100";
  examplegraphs.create_line_graph(tgname, 100, 5);
  try {
    const res = testhelpers.wait_for_pregel(pregel.start(
      "ppa",
      tgname,
      {
        resultField: "",
        maxGSS: 5,
        globalAccumulators: {
          one: {
            accumulatorType: "custom",
            valueType: "any",
            customType: "sam"
          },
        },
        vertexAccumulators: { },
        customAccumulators: {
          sum: accumulators.sumAccumulator(),
        },
        phases: [ { name: "main", updateProgram: [], initProgram: [] } ]
      }));
  } catch(e) {
    if (e.errorNum === 10 && e.code === 400) {
      internal.print("\u001b[32mOK:   Error signaled when custom type used that is not declared\u001b[0m");
      finalResult = true;
    } else {
      internal.print("\u001b[31mFAIL: Expected errorNum == 10 and code == 400, received errorNum ", e.errorNum, " and code ", e.code, "\u001b[0m");
      finalResult = false;
    }
  }
  return finalResult;
}

function test_custom_vertex_accumulator_not_defined() {
  internal.print("Testing: custom vertex accumulators declared");
  let finalResult = false;

  const tgname = "LineGraph100";
  examplegraphs.create_line_graph(tgname, 100, 5);
  try {
    const res = testhelpers.wait_for_pregel(pregel.start(
      "ppa",
      tgname,
      {
        resultField: "",
        maxGSS: 5,
        globalAccumulators: { },
        vertexAccumulators: {
          one: {
            accumulatorType: "custom",
            valueType: "any",
            customType: "sam"
          },
        },
        customAccumulators: {
          sum: accumulators.sumAccumulator(),
        },
        phases: [ { name: "main", updateProgram: [], initProgram: [] } ]
      }));
  } catch(e) {
    if (e.errorNum === 10 && e.code === 400) {
      internal.print("\u001b[32mOK:   Error signaled when custom type used that is not declared\u001b[0m");
      finalResult = true;
    } else {
      internal.print("\u001b[31mFAIL: Expected errorNum == 10 and code == 400, received errorNum ", e.errorNum, " and code ", e.code, "\u001b[0m");
      finalResult = false;
    }
  }
  return finalResult;
}

/* returns a program that compputes the vertex degree of every vertex */
function global_accumulators_test_program(resultField) {
  return {
    resultField: resultField,
    maxGSS: 5000,
    globalAccumulators: {
      numberOfVertices: {
        accumulatorType: "custom",
        valueType: "any",
        customType: "my_sum"
      },
      minimalTest: {
        accumulatorType: "custom",
        valueType: "any",
        customType: "my_min"
      },
      maximalTest: {
        accumulatorType: "custom",
        valueType: "any",
        customType: "my_max"
      }
    },
    vertexAccumulators: {
      forward: {
        accumulatorType: "custom",
        valueType: "any",
        customType: "my_sum"
      },
    },
    customAccumulators: {
      my_sum: accumulators.sumAccumulator(),
      my_min: accumulators.minAccumulator(),
      my_max: accumulators.maxAccumulator()
    },
    phases: [
      {
        name: "main",
        initProgram: [
          "seq",
          ["send-to-global-accum", "numberOfVertices", 1],
          ["accum-set!", "forward", 0.1],
          "vote-active",
        ],
        updateProgram:
        ["seq",
         ["for-each", [["edge", ["this-outbound-edges"]]],
          ["seq",
            ["send-to-accum", "forward",
              ["attrib-ref", ["var-ref", "edge"], "to-pregel-id"],
              ["accum-ref", "forward"]]]],
         "vote-halt",
        ],
        onHalt: [
          "seq",
          ["goto-phase", "second"]
        ],
      },
      {
        name: "second",
        initProgram: [
          "seq",
          ["send-to-global-accum", "minimalTest", ["this-unique-id"]],
          "vote-halt",
        ],
        updateProgram: ["vote-halt"],
        onHalt: [
          "seq",
          ["goto-phase", "third"]
        ],
      },
      {
        name: "third",
        initProgram: [
          "seq",
          ["send-to-global-accum", "maximalTest", ["this-unique-id"]],
          "vote-halt",
        ],
        updateProgram: ["vote-halt"],
        onHalt: [
          "seq",
          ["finish"]
        ]
      }
    ],
  };
}

function global_accumulators_test(graphName, resultField) {
  return pregel.start(
    "ppa",
    graphName,
    global_accumulators_test_program(resultField)
  );
}

function checkVertexCount(graphSpec, presult) {
  const agg_numberOfVertices = presult.masterContext.globalAccumulatorValues.numberOfVertices;
  const exp_numberOfVertices = presult.vertexCount;

  if (agg_numberOfVertices !== exp_numberOfVertices) {
    throw "expected " + agg_numberOfVertices + " to be equal to " + exp_numberOfVertices;
  }
  return true;
}

function exec_test_global_accumulators_test_on_graph(graphSpec, checkProc) {
  const presult = testhelpers.wait_for_pregel(
    "AIR global-accumulators",
    global_accumulators_test(graphSpec.name, "globalAccumulators")
  );
  return checkProc(graphSpec, presult);
 }

function exec_test_vertex_degrees() {
  let results = [];
  let spec = examplegraphs.create_line_graph("LineGraph100", 100, 5);
  // YOLO, graph sometimes arrives too late.
  internal.print("waiting?");
  internal.wait(5);
  results.push(exec_test_global_accumulators_test_on_graph(
    spec,
    checkVertexCount
  ));
  try {
    graphModule._drop("LineGraph100", true);
  } catch (ignore) {
  }


  spec = examplegraphs.create_line_graph("LineGraph1000", 1000, 5);
  // YOLO, graph sometimes arrives too late.
  internal.print("waiting?");
  internal.wait(5);
  results.push(exec_test_global_accumulators_test_on_graph(
    spec,
    checkVertexCount
  ));
  try {
    graphModule._drop("LineGraph1000", true);
  } catch (ignore) {
  }

  spec = examplegraphs.create_wiki_vote_graph("WikiVote", 1);
  // YOLO, graph sometimes arrives too late.
  internal.print("waiting?");
  internal.wait(5);
  results.push(exec_test_global_accumulators_test_on_graph(
    spec,
    checkVertexCount
  ));
  try {
    graphModule._drop("WikiVote", true);
  } catch (ignore) {
  }

  if (results.includes(false)) {
    return false;
  }
  return true;
}

// run tests
function test() {
  if (!test_custom_global_accumulator_not_defined()) {
    internal.print('Test: "test_custom_global_accumulator_not_defined" failed!');
    return false;
  }

  if (!test_custom_vertex_accumulator_not_defined()) {
    internal.print('Test: "test_custom_vertex_accumulator_not_defined" failed!');
    return false;
  }

  if (!exec_test_vertex_degrees()) {
    internal.print('Test: "exec_test_vertex_degrees" failed!');
    return false;
  }

  return true;
}
