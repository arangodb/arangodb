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
exports.read_vertex_program = data_access_read_vertex_program;
exports.read_vertex = read_vertex;
exports.test = test;
exports.benchmark = benchmark;

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

/* returns a program that reads only particular data instead of copying all vertex data */
function data_access_read_vertex_program(expectedKeys) {
  return {
    dataAccess: {
      writeVertex: [
        "attrib-set", ["dict"], "availableKeys", ["accum-ref", "copiedDocumentKeys"]
      ],
      readVertex: expectedKeys
    },
    maxGSS: 2,
    vertexAccumulators: {
      copiedDocumentKeys: {
        accumulatorType: "list",
        valueType: "slice"
      }
    },
    phases: [{
      name: "main",
      // get all available keys and write into copiedDocumentKeys
      initProgram: [
        "seq",
        ["accum-set!", "copiedDocumentKeys", ["dict-keys", ["this-doc"]]]
      ],
      updateProgram: ["seq",
        false]
    }]
  };
}

function read_vertex(
  graphName, expectedKeys) {
  return pregel.start(
    "air",
    graphName,
    data_access_read_vertex_program(expectedKeys)
  );
}

/*
 * Write Vertex tests
 */
function exec_test_write_vertex_on_graph(graphSpec, amount) {
  // amount = expectation of writes that will happen
  let status = testhelpers.wait_for_pregel("AIR write-vertex", write_vertex(graphSpec.name));

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

/*
 * Read Vertex tests
 */
function exec_test_read_vertex_on_graph(graphSpec, expectedKeys) {
  let status = testhelpers.wait_for_pregel("AIR write-vertex", read_vertex(graphSpec.name, expectedKeys));

  let result = db._query(`
    FOR d IN @@V
      RETURN d["availableKeys"]`,
    {
      "@V": graphSpec.vname
    });

  let arrResult = result.toArray();
  let finalResult = false;

  for (let res of arrResult) {
    if ((res.indexOf(expectedKeys[0]) > -1) && (res.indexOf(expectedKeys[1]) > -1) && res.length === 2) {
      finalResult = true;
    } else {
      finalResult = false;
      break;
    }
  }

  if (finalResult) {
    internal.print("Test succeeded.");
    return true;
  } else {
    internal.print("Test failed.");
    return false;
  }
}

/*
 * Write Vertex Benchmark (AQL Comparison)
 */

function exec_benchmark_write_vertex_on_graph(graphSpec, runs) {
  // Pregel Run
  let statusOfRuns = [];
  let pregelTotalRuntime = 0;

  for (let i = 0; i < runs; i++) {
    statusOfRuns.push(testhelpers.wait_for_pregel("AIR write-vertex", write_vertex(graphSpec.name)));
  }

  statusOfRuns.forEach((status) => {
    pregelTotalRuntime += status.totalRuntime;
  });

  // AQL Run
  let statusOfAqlRuns = [];
  let aqlTotalRuntime = 0;
  for (let i = 0; i < runs; i++) {
    statusOfAqlRuns.push(db._query(`
    FOR d IN @@V
    UPDATE { _key: d._key, someKeyA: "someValueA", someKeyB: "someValueB" } IN @@V`,
      {
        "@V": graphSpec.vname
      }).getExtra());
  }
  statusOfAqlRuns.forEach((status) => {
    aqlTotalRuntime += status.stats.executionTime;
  });

  let avgPregel = (pregelTotalRuntime / runs).toFixed(4);
  let avgAql = (aqlTotalRuntime / runs).toFixed(4);
  internal.print("A run in Pregel took about: " + avgPregel + " seconds.");
  internal.print("A run in AQL took about: " + avgAql + " seconds.");
  if (avgPregel < avgAql) {
    internal.print("Pregel execution was faster, diff: " + (avgAql - avgPregel).toFixed(4));
  } else {
    internal.print("Pregel execution was slower, diff: " + (avgPregel - avgAql).toFixed(4));
  }
}

function exec_test_data_access() {
  // write vertex validation
  //exec_test_write_vertex_on_graph(examplegraphs.create_line_graph("LineGraph100", 100, 1), 100);
  //exec_test_write_vertex_on_graph(examplegraphs.create_line_graph("LineGraph1000", 1000, 9), 1000);
  //exec_test_write_vertex_on_graph(examplegraphs.create_line_graph("LineGraph10000", 10000, 18), 10000);

  // read vertex validation
  exec_test_read_vertex_on_graph(
    examplegraphs.create_line_graph("LineGraph100", 100, 1, ["a", "b", "c"]),
    ["a", "b"]
  );
}

function exec_benchmark_data_access() {
  // write vertex benchmark
  exec_benchmark_write_vertex_on_graph(examplegraphs.create_line_graph("LineGraph500000", 500000, 1), 5);
}

// run tests
function test() {
  exec_test_data_access();
}

function benchmark() {
  exec_benchmark_data_access();
}