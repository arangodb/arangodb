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
const pregel = require("@arangodb/pregel");
const _ = require("lodash");
const examplegraphs = require("@arangodb/air/pregel-example-graphs");
const testhelpers = require("@arangodb/air/test-helpers");
const accumulators = require("@arangodb/air/accumulators");

// ee/ce check + gm selection
const isEnterprise = require("internal").isEnterprise();
const graphModule = isEnterprise? require("@arangodb/smart-graph") : require("@arangodb/general-graph");

exports.single_source_shortest_paths_program = single_source_shortest_paths_program;
exports.single_source_shortest_paths = single_source_shortest_paths;
exports.test = test;

/*

  `single_source_shortest_path_program` returns an AIR program that performs a
  single-source shortest path search, currently without path reconstruction, on
  all vertices in the graph starting from `startVertex`, using the cost stored
  in `weightAttribute` on each edge, and storing the end result in resultField
  as an object containing the attribute `distance`

*/
function single_source_shortest_paths_program(
  resultField,
  startVertexId
) {
  return {
    resultField: resultField,
    maxGSS: 10000,
    vertexAccumulators: {
      distance: {
        accumulatorType: "custom",
        valueType: "any",
        customType: "my_min",
      },
    },
    customAccumulators: {
      "my_min": accumulators.minAccumulator()
    },
    phases: [
      {
        name: "main",
        initProgram: [
          "seq",
          [
            "if",
            [
              ["eq?", ["this-vertex-id"], startVertexId],
              ["seq",
                ["accum-set!", "distance", 0],
                true],
            ],
            [true, ["seq",
              ["accum-clear!", "distance"],
              false]],
          ],
        ],
        updateProgram: [
          "seq",
          ["for-each",
            [["edge", ["this-outbound-edges"]]],
            ["send-to-accum",
              "distance",
              ["attrib-ref", ["var-ref", "edge"], "to-pregel-id"],
              ["+",
                ["accum-ref", "distance"],
                1,//["attrib-ref", ["quote", "document", weightAttribute], ["var-ref", "edge"]],
              ]]],
          false,
        ],
      },
    ],
  };
}

/* `single_source_shortest_path` executes the program
   returned by `single_source_shortest_path_program`
   on the graph identified by `graphName`. */
function single_source_shortest_paths(
  graphName,
  resultField,
  startVertexId
) {
  return pregel.start(
    "ppa",
    graphName,
    single_source_shortest_paths_program(
      resultField,
      startVertexId
    )
  );
}

function reconstruct_path(graphSpec, from, to) {

  let path = [to];

  while (from !== to) {
    let doc = db._collection(graphSpec.vname).document(to);
    if (doc === null || doc.SSSP.distance === null) {
      return [];
    }
    to = doc.SSSP.distance.sender;
    path.unshift(to);
  }

  return [path];
}

function exec_test_shortest_path_impl(graphSpec) {
  // Find the ID of a vertex to start at.
  const [from_vertex, ...to_vertexes] = db
    ._query(`FOR d IN @@V SORT RAND() LIMIT 10 RETURN d._id`,
      {"@V": graphSpec.vname})
    .toArray();

  internal.print(" -- computing sssp " + from_vertex + " -> ", to_vertexes.join(", "));

  testhelpers.wait_for_pregel(
    "Air SSSP",
    single_source_shortest_paths(
      graphSpec.name,
      "SSSP",
      from_vertex,
    ));

  for (let to_vertex of to_vertexes) {

    internal.print(" -- computing shortest paths using kShortestPath AQL to ", to_vertex);
    const shortest_paths_result = db._query(`
            FOR p IN OUTBOUND K_SHORTEST_PATHS @from TO @to
            GRAPH @graph
            LIMIT 1
            RETURN p`, {"from": from_vertex, "to": to_vertex, "graph": graphSpec.name})
      .toArray();

    internal.print(" -- collecting Pregel SSSP Path to ", to_vertex);
    /*const found_path_result = db._query(`
            FOR v, e, p in 0..10000 INBOUND @to GRAPH @graph
                PRUNE v._id == @from || (e != null && e._from != p.vertices[-2].SSSP.distance.sender)
                OPTIONS {uniqueVertices: "path"}
                FILTER v._id == @from
                LIMIT 1
                RETURN REVERSE(p.vertices[*]._id)`,
      {"from": from_vertex, "to": to_vertex, "graph": graphSpec.name}).toArray();*/
    const found_path_result = reconstruct_path(graphSpec, from_vertex, to_vertex);

    if ((found_path_result.length !== 0) !== (shortest_paths_result.length !== 0)) {
      internal.print("\u001b[31mFAIL: did not agree on the existance of a shortest path to ", to_vertex, "\u001b[0m");
      return false;
    } else if (found_path_result.length === 0) {
      internal.print("\u001b[32mOK  : no path found to ", to_vertex, ", as expected", "\u001b[0m");
      continue;
    }
    const shortest_paths = shortest_paths_result[0];
    const found_path = found_path_result[0];

    if (shortest_paths.vertices.length !== found_path.length) {
      internal.print("\u001b[32mOK  : sssp path was not a shortest path, sssp: ", found_path.length, " all shortest paths: ", shortest_paths.vertices.length, "\u001b[0m");
      internal.print(found_path, shortest_paths);
      return false;
    } else {
      internal.print("\u001b[32mOK  : shortest path is ok to ", to_vertex, "\u001b[0m");
    }
  }
  return true;
}

function exec_test_shortest_path() {
  let results = [];

  results.push(exec_test_shortest_path_impl(examplegraphs.create_line_graph("LineGraph10", 10, 1)));
  try {
    graphModule._drop("LineGraph10", true);
  } catch (ignore) {
  }

  results.push(exec_test_shortest_path_impl(examplegraphs.create_line_graph("LineGraph100", 100, 9)));
  try {
    graphModule._drop("LineGraph100", true);
  } catch (ignore) {
  }

  results.push(exec_test_shortest_path_impl(examplegraphs.create_line_graph("LineGraph1000", 1000, 18)));
  try {
    graphModule._drop("LineGraph1000", true);
  } catch (ignore) {
  }

  results.push(exec_test_shortest_path_impl(examplegraphs.create_circle_graph("Circle10", 2, 1)));
  try {
    graphModule._drop("Circle10", true);
  } catch (ignore) {
  }

  results.push(exec_test_shortest_path_impl(examplegraphs.create_circle_graph("Circle100", 4, 6)));
  try {
    graphModule._drop("Circle100", true);
  } catch (ignore) {
  }

  results.push(exec_test_shortest_path_impl(examplegraphs.create_circle_graph("Circle1000", 8, 18)));
  try {
    graphModule._drop("Circle1000", true);
  } catch (ignore) {
  }

  results.push(exec_test_shortest_path_impl(examplegraphs.create_complete_graph("Complete4", 10, 4)));
  try {
    graphModule._drop("Complete4", true);
  } catch (ignore) {
  }

  results.push(exec_test_shortest_path_impl(examplegraphs.create_complete_graph("Complete10", 10, 10)));
  try {
    graphModule._drop("Complete10", true);
  } catch (ignore) {
  }

  results.push(exec_test_shortest_path_impl(examplegraphs.create_complete_graph("Complete100", 10, 100)));
  try {
    graphModule._drop("Complete100", true);
  } catch (ignore) {
  }

  results.push(exec_test_shortest_path_impl(examplegraphs.create_wiki_vote_graph("WikiVote", 1)));
  try {
    graphModule._drop("WikiVote", true);
  } catch (ignore) {
  }

  results.push(exec_test_shortest_path_impl(examplegraphs.create_wiki_vote_graph("WikiVote", 9)));
  try {
    graphModule._drop("WikiVote", true);
  } catch (ignore) {
  }

  results.push(exec_test_shortest_path_impl(examplegraphs.create_wiki_vote_graph("WikiVote", 18)));
  try {
    graphModule._drop("WikiVote", true);
  } catch (ignore) {
  }

  return !results.includes(false);
}

function test() {
  return exec_test_shortest_path();
}
