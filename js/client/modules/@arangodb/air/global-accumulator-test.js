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

/*


*/
exports.global_accumulators_test_program = global_accumulators_test_program;
exports.global_accumulators_test = global_accumulators_test;
exports.test = test;

/* returns a program that compputes the vertex degree of every vertex */
function global_accumulators_test_program(resultField) {
  return {
    resultField: resultField,
    maxGSS: 5,
    globalAccumulators: {
      numberOfVertices: {
        accumulatorType: "sum",
        valueType: "ints",
        storeSender: false,
      },
    },
    vertexAccumulators: {
      forward: {
        accumulatorType: "sum",
        valueType: "ints",
        storeSender: false,
      },
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
          ["finish"]
        ],
      },
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
  const agg_numberOfVertices = presult.aggregators["[global]-numberOfVertices"];
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
  exec_test_vertex_degrees();
}
