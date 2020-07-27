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

const pr = require("@arangodb/pregel");
const pp = require("@arangodb/pregel-programs");
const pe = require("@arangodb/pregel-example-graphs");

function exec_test_wiki_vote() {
  pe.create_wiki_vote_graph("WikiVoteGraph", 6);
  const some_vertex = db._query(`FOR d IN V FILTER d.id == "15" RETURN d._id`).toArray()[0];

  return pp.single_source_shortest_path("WikiVoteGraph", "SSSP", some_vertex, "cost");
}

function exec_test_line() {
  const collnames = pe.create_line_graph("LineGraph", 1000, 1);
  const some_vertex = db._query(`FOR d IN @@V FILTER d.id == "15" RETURN d._id`, {"@V": collnames.vname}).toArray()[0];

  require("internal").print("vertex: " + some_vertex);
  
  return pp.single_source_shortest_path("LineGraph", "SSSP", some_vertex, "cost");
}

/* TODO: Run the "native" SSSP, the VertexAccumulators SSSP, and AQL ShortestPath and compare the
   results */
function exec_sssp_test() {
  pe.create_wiki_vote_graph("WikiVoteGraph", 1);

  const some_vertex = db._query(`FOR d IN V FILTER d.id == "15" RETURN d._id`).toArray()[0];
  return pregel.start("sssp", "WikiVoteGraph", { "start": some_vertex });
}


exports.exec_test_wiki_vote = exec_test_wiki_vote;
exports.exec_test_line = exec_test_line;
