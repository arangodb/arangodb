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
const sexptoair = require("@arangodb/air/sexpression-syntax");

const internal = require("internal");
const db = internal.db;
/*

  This file demos the translation of S-Expression syntax into AIR
  for slightly less awkward programming

*/
exports.vertex_degrees_program = vertex_degrees_program;
exports.vertex_degrees = vertex_degrees;
exports.test = test;

/* returns a program that compputes the vertex degree of every vertex */
function vertex_degrees_program(resultField) {
    return {
        resultField: resultField,
        maxGSS: 2,
        vertexAccumulators: {
          outDegree: {
            accumulatorType: "sum",
            valueType: "ints",
            storeSender: false
          },
          inDegree: {
            accumulatorType: "sum",
            valueType: "ints",
            storeSender: false
          }
        },
      phases: [ {
        name: "main",
        initProgram: sexptoair.t(
          `(seq
             (accum-set! outDegree (this-outbound-edges-count))
             (accum-set! inDegree 0)
             (send-to-all-neighbours inDegree 1))`),
        // Update program has to run once to accumulate the
        // inDegrees that have been sent out in initProgram
        updateProgram: [ false ]
      } ] };
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
      FILTER d.vertexDegrees.inDegree != inDegree || d.vertexDegrees.outDegree != outDegree
      RETURN { aql: { inDegree: inDegree, outDegree: outDegree },
               air: { inDegree: d.vertexDegrees.inDegree, outDegree: d.vertexDegrees.outDegree } }`,
                                  { "@V": graphSpec.vname,
                                    "@E": graphSpec.ename }));
}

function exec_test_vertex_degrees() {
  exec_test_vertex_degrees_on_graph(examplegraphs.create_line_graph("LineGraph100", 100, 1));
  exec_test_vertex_degrees_on_graph(examplegraphs.create_line_graph("LineGraph1000", 1000, 9));
  exec_test_vertex_degrees_on_graph(examplegraphs.create_line_graph("LineGraph10000", 10000, 18));

  exec_test_vertex_degrees_on_graph(examplegraphs.create_wiki_vote_graph("WikiVote", 1));
  exec_test_vertex_degrees_on_graph(examplegraphs.create_wiki_vote_graph("WikiVote", 9));
  exec_test_vertex_degrees_on_graph(examplegraphs.create_wiki_vote_graph("WikiVote", 18));

  // TODO: random graph
  // TODO: structurally generated graph
}

// run tests
function test() {
  exec_test_vertex_degrees();
}
