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
const pr = require("@arangodb/pregel");
const pp = require("@arangodb/pregel-programs");
const pe = require("@arangodb/pregel-example-graphs");

function exec_test_wiki_vote() {
  pe.create_wiki_vote_graph("WikiVoteGraph", 6);
  const some_vertex = db
    ._query(`FOR d IN V FILTER d.id == "15" RETURN d._id`)
    .toArray()[0];

  return pp.single_source_shortest_path(
    "WikiVoteGraph",
    "SSSP",
    some_vertex,
    "cost"
  );
}

function exec_test_line() {
  const collnames = pe.create_line_graph("LineGraph", 10000, 6);
  const some_vertex = db
    ._query(`FOR d IN @@V FILTER d.id == "15" RETURN d._id`, {
      "@V": collnames.vname,
    })
    .toArray()[0];

  require("internal").print("vertex: " + some_vertex);

  return pp.single_source_shortest_path(
    "LineGraph",
    "SSSP",
    some_vertex,
    "cost"
  );
}

function exec_test_page_rank() {
  pe.create_page_rank_graph("PageRankGraph", 6);

  let native_pid = pp.page_rank(
      "PageRankGraph",
      "pageRankResult",
      0.85
  );
  const native_status = wait_for_pregel(native_pid);
  internal.print("  done, returned with status: ");
  internal.print(JSON.stringify(native_status, null, 4));
}

function wait_for_pregel(pid) {
  var waited = 0;
  while(true) {
    var status = pr.status(pid);

    if (status.state === "done") {
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

/* TODO: Run the "native" SSSP, the VertexAccumulators SSSP, and AQL ShortestPath and compare the
   results */
function exec_sssp_test() {
  // Create a line graph with 10000 vertices, 6 shards
  const collnames = pe.create_line_graph("LineGraph", 10000, 6);

  // Find the ID of a vertex
  const some_vertex = db
    ._query(`FOR d IN @@V FILTER d.id == "15" RETURN d._id`, {
      "@V": collnames.vname,
    })
    .toArray()[0];

  internal.print("Running SSSP with start vertex " + some_vertex);

  internal.print("  Native Pregel");
  const native_pid = pr.start("sssp", "LineGraph", {
    source: some_vertex,
    maxGSS: 10000,
  });
  const native_status = wait_for_pregel(native_pid);
  internal.print("  done, returned with status: ");
  internal.print(JSON.stringify(native_status, null, 4));

  internal.print("  AIR Pregel");
  const air_pid = pp.single_source_shortest_path(
          "LineGraph",
          "SSSP",
          some_vertex,
          "cost"
        );
  const air_status = wait_for_pregel(air_pid);
  internal.print("  done, returned with status: ");
  internal.print(JSON.stringify(air_status, null, 4));

  const res = db._query(`FOR d IN @@V
               FILTER d.result != d.SSSP.distance.value
               RETURN d`, {"@V": collnames.vname });

  internal.print("Discrepancies in results: " + JSON.stringify(res.toArray()));
}

function exec_scc_test() {
  pe.create_circle("Circle", 5);
  pe.create_line_graph("LineGraph", 5, 6);

  return pp.strongly_connected_components("LineGraph", "scc");
}

function exec_air_tests() {
  exec_scc_test();
}

exports.wait_for_pregel = wait_for_pregel;

exports.exec_test_wiki_vote = exec_test_wiki_vote;
exports.exec_test_line = exec_test_line;
exports.exec_test_page_rank = exec_test_page_rank;
exports.exec_scc_test = exec_scc_test;
exports.exec_sssp_test = exec_sssp_test;


exports.exec_air_tests = exec_air_tests;
