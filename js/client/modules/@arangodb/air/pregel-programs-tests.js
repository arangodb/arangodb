/*jshint strict: false */

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
const db = internal.db;
const pr = require("@arangodb/pregel");
const pp = require("@arangodb/air/pregel-programs");
const pe = require("@arangodb/air/pregel-example-graphs");

/* Some helper functions */
function compare_pregel(aqlResult) {
  const res = aqlResult.toArray();
  if (res.length > 0) {
    internal.print("Test failed.");
    internal.print("Discrepancies in results: " + JSON.stringify(res));
    return false;
  }

  internal.print("Test succeeded.");
  return true;
}

function wait_for_pregel(name, pid) {
  internal.print("  Started: " + name + " - PID: " + pid);
  var waited = 0;
  while (true) {
    var status = pr.status(pid);

    if (status.state === "done") {
      internal.print("  " + name + " done, returned with status: ");
      internal.print(JSON.stringify(status, null, 4));
      return status;
    } else {
      waited++;
      if (waited % 10 === 0) {
        internal.print("waited" + waited * 10 + "seconds, not done yet, waiting some more");
      }
    }
    internal.sleep(1);
  }
}


function exec_test_page_rank() {
  let graphName = "PageRankGraph";
  let collectionNames = pe.create_page_rank_graph(graphName, 6);
  let vertexName = collectionNames.vname;

  wait_for_pregel(
    "Air Pagerank",
    pp.page_rank(
      "PageRankGraph",
      "pageRankResult",
      0.85
    ));

  wait_for_pregel(
    "Native Pagerank",
    pr.start("pagerank", graphName, {
      maxGSS: 5,
      resultField: "nativeRank"
    })
  );

  // Return results which differ too much (here currently 0.05)
  return compare_pregel(
    db._query(`FOR d IN @@V
               FILTER ABS(d.nativeRank - d.pageRankResult.rank) >= 0.05
               RETURN {
                 name: d.name,
                 native: d.nativeRank,
                 air: d.pageRankResult.rank
               }`, {"@V": vertexName})
  );
}

/* Run the "native" SSSP, the VertexAccumulators SSSP, and AQL ShortestPath and compare the
   results */
function exec_test_sssp() {
  // Import AIR programs
  const graphName = "LineGraph";
  const air = require("@arangodb/air/single-source-shortest-paths");

  // Create a line graph with 10000 vertices, 6 shards
  const collnames = pe.create_line_graph(graphName, 10000, 1);

  // Find the ID of a vertex
  const some_vertex = db
    ._query(`FOR d IN @@V FILTER d.id == "15" RETURN d._id`, {
      "@V": collnames.vname,
    })
    .toArray()[0];

  internal.print("Used start vertex: " + some_vertex + ")");

  wait_for_pregel(
    "Air SSSP",
    air.single_source_shortest_paths(
      graphName,
      "SSSP",
      some_vertex,
      "cost"
    ));

  wait_for_pregel(
    "Native SSSP",
    pr.start("sssp", graphName, {
    source: some_vertex,
    maxGSS: 10000,
  }));

  return compare_pregel(db._query(`FOR d IN @@V
               FILTER d.result != d.SSSP.distance
               RETURN d`, {"@V": collnames.vname}));
}

function exec_test_vertex_degrees() {
  const air = require("@arangodb/air/vertex-degrees");

  const graphName = "LineGraph";
  // Create a line graph with 10000 vertices, 6 shards
  const collnames = pe.create_line_graph(graphName, 10000, 1);
  wait_for_pregel("AIR vertex-degree", air.vertex_degrees(graphName, "vertexDegrees"));

  return compare_pregel(db._query(`
    FOR d IN @@V
      LET outDegree = LENGTH(FOR x IN LineGraph_E FILTER x._from == d._id RETURN x)
      LET inDegree = LENGTH(FOR x IN LineGraph_E FILTER x._to == d._id RETURN x)
      FILTER d.vertexDegrees.inDegree != inDegree || d.vertexDegrees.outDegree != outDegree
      RETURN { aql: { inDegree: inDegree, outDegree: outDegree },
               air: { inDegree: d.vertexDegrees.inDegree, outDegree: d.vertexDegrees.outDegree } }`,
                                  { "@V": collnames.vname}));
}

function exec_scc_test() {
  pe.create_circle("Circle", 5);
  // pe.create_line_graph("LineGraph", 5, 6);
  // return pp.strongly_connected_components("LineGraph", "scc");

  return pp.strongly_connected_components("Circle", "scc");
}

function exec_air_tests() {
  exec_scc_test();
}

exports.wait_for_pregel = wait_for_pregel;

exports.exec_test_page_rank = exec_test_page_rank;

exports.exec_scc_test = exec_scc_test;

exports.exec_test_vertex_degrees = exec_test_vertex_degrees;
exports.exec_test_sssp = exec_test_sssp;


exports.exec_air_tests = exec_air_tests;
