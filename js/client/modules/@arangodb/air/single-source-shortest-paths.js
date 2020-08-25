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

const internal = require("internal");

exports.single_source_shortest_paths_program = single_source_shortest_paths_program;
exports.single_source_shortest_paths = single_source_shortest_paths;
exports.test = test;

function cmpAccumulator2(cmp) {
    return {
        updateProgram: ["if",
            [
                ["or",
                    ["not", ["attrib-get", "isSet", ["current-value"]]],
                    [cmp, ["input-value"], ["attrib-get", "value", ["current-value"]]]
                ],
                ["seq",
                    ["this-set!",
                        ["dict",
                            ["list", "isSet", true],
                            ["list", "value", ["input-value"]],
                            ["list", "sender", ["input-sender"]]
                        ]
                    ],
                    "hot"
                ]
            ],
            [true, "cold"]
        ],
        clearProgram: ["this-set!", {"isSet": false, "value": 0, sender: null}],
        getProgram: ["if",
            [
                ["attrib-get", "isSet", ["current-value"]],
                ["attrib-get", "value", ["current-value"]]
            ],
            [
                true,
                ["error", "accumulator undefined value"]
            ]
        ],
        setProgram: ["this-set!",
            ["dict",
                ["list", "isSet", true],
                ["list", "value", ["input-value"]],
                ["list", "sender", null]
            ]
        ],
        finalizeProgram: ["if",
            [
                ["attrib-get", "isSet", ["current-value"]],
                ["dict-x-tract",  ["current-value"], "value", "sender"]
            ],
            [true, null]
        ],
    }
}

const minAccumulator = cmpAccumulator2("lt?");
const maxAccumulator = cmpAccumulator2("gt?");

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
                valueType: "slice",
                customType: "my_min",
            },
        },
        customAccumulators: {
            "my_min": minAccumulator
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
                    [
                        "for-each",
                        ["edge", ["this-outbound-edges"]],
                        ["seq",
                            [
                                "send-to-accum",
                                ["attrib-ref", "to-pregel-id", ["var-ref", "edge"]],
                                "distance",
                                [
                                    "+",
                                    ["accum-ref", "distance"],
                                    1,//["attrib-ref", ["quote", "document", weightAttribute], ["var-ref", "edge"]],
                                ],
                            ],
                        ],
                    ],
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
        "air",
        graphName,
        single_source_shortest_paths_program(
            resultField,
            startVertexId
        )
    );
}

function exec_test_compare_sssp_impls(graphSpec) {
    // Find the ID of a vertex to start at.
    const some_vertex = db
        ._query(`FOR d IN @@V SORT RAND() LIMIT 1 RETURN d._id`,
            {"@V": graphSpec.vname})
        .toArray()[0];

    internal.print("using " + some_vertex + " as start vertex.");

    testhelpers.wait_for_pregel(
        "Air SSSP",
        single_source_shortest_paths(
            graphSpec.name,
            "SSSP",
            some_vertex,
        ));

    testhelpers.wait_for_pregel(
        "Native SSSP",
        pregel.start("sssp", graphSpec.name, {
            source: some_vertex,
            maxGSS: 10000,
        }));

    return testhelpers.compare_pregel(db._query(`FOR d IN @@V
               FILTER NOT ( d.result == d.SSSP.distance.value OR ( d.SSSP.distance.value == NULL AND d.result > 9999999 ) )
               RETURN d`, {"@V": graphSpec.vname}));
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
            LET path = p.vertices[*]._id
            COLLECT len = LENGTH(path) INTO paths = path
            LIMIT 1
            RETURN paths`, {"from": from_vertex, "to": to_vertex, "graph": graphSpec.name})
            .toArray();

        internal.print(" -- collecting Pregel SSSP Path to ", to_vertex);
        const found_path_result = db._query(`
            FOR v, e, p in 1..10000 INBOUND @to GRAPH @graph
                PRUNE v._id == @from || e._to == p.vertices[-1].SSSP.distance.sender
                FILTER v._id == @from
                RETURN REVERSE(p.vertices[*]._id)`,
            {"from": from_vertex, "to": to_vertex, "graph": graphSpec.name}).toArray();

        if ((found_path_result.length !== 0) !== (shortest_paths_result.length !== 0)) {
            internal.print("\u001b[31mFAIL: did not agree on the existance of a shortest path to ", to_vertex, "\u001b[0m");
            continue;
        } else if (found_path_result.length === 0) {
            internal.print("\u001b[32mOK  : no path found to ", to_vertex, ", as expected", "\u001b[0m");
            continue;
        }
        const shortest_paths = shortest_paths_result[0];
        const found_path = found_path_result[0];

        // now test if the shortest path found is actually in the list of shortest paths
        let found = false;
        for (let path of shortest_paths) {
            if (path.length === found_path.length) {
                let isSame = true;
                for (let i = 0; i < path.length; i++) {
                    if (path[i] !== found_path[i]) {
                        isSame = false;
                        break;
                    }
                }
                if (isSame) {
                    found = true;
                    break;
                }
            }
        }

        if (!found) {
            internal.print("\u001b[32mOK  : sssp path was not in the list of shortest paths, sssp: ", found_path, " all shortest paths: ", shortest_paths, "\u001b[0m");
        } else {
            internal.print("\u001b[32mOK  : shortest path is ok to ", to_vertex, "\u001b[0m");
        }
    }
}

function exec_test_compare_sssp() {
    exec_test_compare_sssp_impls(examplegraphs.create_line_graph("LineGraph100", 100, 1));
    exec_test_compare_sssp_impls(examplegraphs.create_line_graph("LineGraph1000", 1000, 9));
    exec_test_compare_sssp_impls(examplegraphs.create_line_graph("LineGraph10000", 10000, 18));

    exec_test_compare_sssp_impls(examplegraphs.create_wiki_vote_graph("WikiVote", 1));
    exec_test_compare_sssp_impls(examplegraphs.create_wiki_vote_graph("WikiVote", 9));
    exec_test_compare_sssp_impls(examplegraphs.create_wiki_vote_graph("WikiVote", 18));
}

function exec_test_shortest_path() {
    exec_test_shortest_path_impl(examplegraphs.create_line_graph("LineGraph100", 100, 1));
    exec_test_shortest_path_impl(examplegraphs.create_line_graph("LineGraph1000", 1000, 9));
    exec_test_shortest_path_impl(examplegraphs.create_line_graph("LineGraph10000", 10000, 18));

    exec_test_shortest_path_impl(examplegraphs.create_circle("Circle10", 10, 1));
    exec_test_shortest_path_impl(examplegraphs.create_circle("Circle100", 100, 6));
    exec_test_shortest_path_impl(examplegraphs.create_circle("Circle1000", 1000, 18));

    exec_test_shortest_path_impl(examplegraphs.create_wiki_vote_graph("WikiVote", 1));
    exec_test_shortest_path_impl(examplegraphs.create_wiki_vote_graph("WikiVote", 9));
    exec_test_shortest_path_impl(examplegraphs.create_wiki_vote_graph("WikiVote", 18));
}

function test() {
    exec_test_compare_sssp();
    exec_test_shortest_path();
}
