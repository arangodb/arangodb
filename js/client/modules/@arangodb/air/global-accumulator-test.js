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

/*


*/
exports.global_accumulators_test_program = global_accumulators_test_program;
exports.global_accumulators_test = global_accumulators_test;
exports.test = test;

// TODO these tests have to be more detailed, but less icky to write...
function test_custom_global_accumulator_not_defined() {
  internal.print("Testing: custom global accumulator declared");

  const tgname = "LineGraph100";
  examplegraphs.create_line_graph(tgname, 100, 5);
  try {
    const res = testhelpers.wait_for_pregel(pregel.start(
      "air",
      tgname,
      {
        resultField: "",
        maxGSS: 5,
        globalAccumulators: {
          one: {
            accumulatorType: "custom",
            valueType: "slice",
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
      return;
    } else {
      internal.print("\u001b[31mFAIL: Expected errorNum == 10 and code == 400, received errorNum ", e.errorNum, " and code ", e.code, "\u001b[0m");
    }
  }
}

function test_custom_vertex_accumulator_not_defined() {
  internal.print("Testing: custom vertex accumulators declared");

  const tgname = "LineGraph100";
  examplegraphs.create_line_graph(tgname, 100, 5);
  try {
    const res = testhelpers.wait_for_pregel(pregel.start(
      "air",
      tgname,
      {
        resultField: "",
        maxGSS: 5,
        globalAccumulators: { },
        vertexAccumulators: {
          one: {
            accumulatorType: "custom",
            valueType: "slice",
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
      return;
    } else {
      internal.print("\u001b[31mFAIL: Expected errorNum == 10 and code == 400, received errorNum ", e.errorNum, " and code ", e.code, "\u001b[0m");
    }
  }
}

/* returns a program that compputes the vertex degree of every vertex */
function global_accumulators_test_program(resultField) {
  return {
    resultField: resultField,
    maxGSS: 5,
    globalAccumulators: {
      numberOfVertices: {
        accumulatorType: "custom",
        valueType: "slice",
        customType: "my_sum"
      },
      minimalTest: {
        accumulatorType: "custom",
        valueType: "slice",
        customType: "my_min"
      },
      maximalTest: {
        accumulatorType: "custom",
        valueType: "slice",
        customType: "my_max"
      }
    },
    vertexAccumulators: {
      forward: {
        accumulatorType: "custom",
        valueType: "slice",
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
         ["for-each", ["edge", ["this-outbound-edges"]],
          ["seq",
           ["send-to-accum",
            ["attrib-ref", "to-pregel-id", ["var-ref", "edge"]],
            "forward", ["accum-ref", "forward"]]]],
         "vote-halt",
        ],
        onHalt: [
          "seq",
          ["print", "global accum: ", ["global-accum-ref", "numberOfVertices"]],
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
          ["print", "global accum: ", ["global-accum-ref", "minimalTest"]],
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
          ["print", "global accum: ", ["global-accum-ref", "maximalTest"]],
          ["finish"]
        ]
      }
    ],
  };
}

function global_accumulators_test(graphName, resultField) {
  return pregel.start(
    "air",
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
}

function exec_test_global_accumulators_test_on_graph(graphSpec, checkProc) {
  const presult = testhelpers.wait_for_pregel(
    "AIR global-accumulators",
    global_accumulators_test(graphSpec.name, "globalAccumulators")
  );
  checkProc(graphSpec, presult);
 }

function exec_test_vertex_degrees() {
  exec_test_global_accumulators_test_on_graph(
    examplegraphs.create_line_graph("LineGraph100", 100, 5),
    checkVertexCount
  );

  exec_test_global_accumulators_test_on_graph(
    examplegraphs.create_line_graph("LineGraph1000", 1000, 5),
    checkVertexCount
  );

  exec_test_global_accumulators_test_on_graph(
    examplegraphs.create_wiki_vote_graph("WikiVote", 1),
    checkVertexCount
  );
}

// run tests
function test() {
  test_custom_global_accumulator_not_defined();
  test_custom_vertex_accumulator_not_defined();
  exec_test_vertex_degrees();
}
