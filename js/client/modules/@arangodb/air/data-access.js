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

exports.write_vertex_program = data_access_write_vertex_program;
exports.write_vertex = write_vertex;
exports.test = test;

/* returns a program that writes {"someKeyA": "someValueA, "someKeyB": "someValueB"} into every vertex */
function data_access_write_vertex_program() {
  return {
    dataAccess: {
      writeVertex: [
        "attrib-set", ["attrib-set", ["dict"], "someKeyA", "someValueA"],
        "someKeyB", "someValueB"
      ]
    },
    maxGSS: 2,
    vertexAccumulators: {},
    phases: [{
      name: "main",
      initProgram: ["seq",
        false
      ],
      updateProgram: ["seq",
        false]
    }]
  };
}

function write_vertex(
  graphName) {
  return pregel.start(
    "air",
    graphName,
    data_access_write_vertex_program()
  );
}


/*
 * Write Vertex tests
 */
function exec_test_write_vertex_on_graph(graphSpec, amount) {
  testhelpers.wait_for_pregel("AIR write-vertex", write_vertex(graphSpec.name));

  let result = db._query(`
    FOR d IN @@V
      FILTER HAS(d, "someKeyA")
      FILTER HAS(d, "someKeyB")
      COLLECT WITH COUNT INTO length
    RETURN length`,
    {
      "@V": graphSpec.vname
    });

  let arrResult = result.toArray()[0];
  if (arrResult === amount) {
    internal.print("Test succeeded.");
    return true;
  } else {
    internal.print("Test failed.");
    return false;
  }
}

function exec_test_data_access() {
  exec_test_write_vertex_on_graph(examplegraphs.create_line_graph("LineGraph100", 100, 1), 100);
  exec_test_write_vertex_on_graph(examplegraphs.create_line_graph("LineGraph1000", 1000, 9), 1000);
  exec_test_write_vertex_on_graph(examplegraphs.create_line_graph("LineGraph10000", 10000, 18), 10000);
}

// run tests
function test() {
  exec_test_data_access();
}
