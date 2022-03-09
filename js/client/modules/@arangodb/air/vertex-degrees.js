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

const _ = require("lodash");
const internal = require("internal");
const db = internal.db;

// ee/ce check + gm selection
const isEnterprise = require("internal").isEnterprise();
const graphModule = isEnterprise? require("@arangodb/smart-graph") : require("@arangodb/general-graph");

exports.vertex_degrees_program = vertex_degrees_program;
exports.vertex_degrees = vertex_degrees;
exports.test = test;

/* returns a program that computes the vertex degree of every vertex */
function vertex_degrees_program() {
  return {
    dataAccess: {
      writeVertex: [
        "attrib-set", ["attrib-set", ["dict"], "inDegree", ["accum-ref", "inDegree"]],
        "outDegree", ["accum-ref", "outDegree"]
      ]
    },
    maxGSS: 2,
    vertexAccumulators: {
      outDegree: {
        accumulatorType: "store",
        valueType: "int"
      },
      inDegree: {
        accumulatorType: "sum",
        valueType: "int"
      }
    },
    phases: [{
      name: "main",
      initProgram: ["seq",
        // Set our out degree
        ["accum-set!", "outDegree", ["this-outbound-edges-count"]],
        // Init in degree to 0
        ["accum-set!", "inDegree", 0],
        ["send-to-all-neighbours", "inDegree", 1]
      ],
      // Update program has to run once to accumulate the
      // inDegrees that have been sent out in initProgram
      updateProgram: ["seq",
        false]
    }]
  };
}

function vertex_degrees(
  graphName,
  resultField) {
  return pregel.start(
    "ppa",
    graphName,
    vertex_degrees_program(resultField)
  );
}


/*
 * Vertex Degree tests
 */
function exec_test_vertex_degrees_on_graph(graphSpec) {
  testhelpers.wait_for_pregel("AIR vertex-degree", vertex_degrees(graphSpec.name, "vertexDegrees"));

  return testhelpers.compare_pregel(db._query(`
    FOR d IN @@V
      LET outDegree = LENGTH(FOR x IN @@E FILTER x._from == d._id RETURN x)
      LET inDegree = LENGTH(FOR x IN @@E FILTER x._to == d._id RETURN x)
      FILTER d.inDegree != inDegree || d.outDegree != outDegree
      RETURN { aql: { inDegree: inDegree, outDegree: outDegree },
               air: { inDegree: d.inDegree, outDegree: d.outDegree } }`,
    {
      "@V": graphSpec.vname,
      "@E": graphSpec.ename
    }));
}

function exec_test_vertex_degrees() {
  let results = [];
  results.push(exec_test_vertex_degrees_on_graph(examplegraphs.create_line_graph("LineGraph100", 100, 1)));
  try {
    graphModule._drop("LineGraph100", true);
  } catch (ignore) {
  }

  results.push(exec_test_vertex_degrees_on_graph(examplegraphs.create_line_graph("LineGraph1000", 1000, 9)));
  try {
    graphModule._drop("LineGraph1000", true);
  } catch (ignore) {
  }

  results.push(exec_test_vertex_degrees_on_graph(examplegraphs.create_line_graph("LineGraph10000", 10000, 18)));
  try {
    graphModule._drop("LineGraph10000", true);
  } catch (ignore) {
  }

  results.push(exec_test_vertex_degrees_on_graph(examplegraphs.create_wiki_vote_graph("WikiVote", 1)));
  try {
    graphModule._drop("WikiVote", true);
  } catch (ignore) {
  }

  results.push(exec_test_vertex_degrees_on_graph(examplegraphs.create_wiki_vote_graph("WikiVote", 9)));
  try {
    graphModule._drop("WikiVote", true);
  } catch (ignore) {
  }

  results.push(exec_test_vertex_degrees_on_graph(examplegraphs.create_wiki_vote_graph("WikiVote", 18)));
  try {
    graphModule._drop("WikiVote", true);
  } catch (ignore) {
  }

  // TODO: random graph
  // TODO: structurally generated graph
  return !results.includes(false);
}

// run tests
function test() {
  return exec_test_vertex_degrees();
}
