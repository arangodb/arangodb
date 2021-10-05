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
"use strict";
const internal = require("internal");
const db = internal.db;
const pregel = require("@arangodb/pregel");
const _ = require("lodash");
const examplegraphs = require("@arangodb/air/pregel-example-graphs");
const testhelpers = require("@arangodb/air/test-helpers");

// ee/ce check + gm selection
const isEnterprise = require("internal").isEnterprise();
const graphModule = isEnterprise ? require("@arangodb/smart-graph") : require("@arangodb/general-graph");

exports.random_walk_program = random_walk_program;
exports.random_walk = random_walk;
exports.test = test;

function random_walk_program(
    resultField
) {
    return {
        "resultField": resultField,
        "maxGSS": 1000,
        "vertexAccumulators": {
            "terminated_paths": {
                "accumulatorType": "list",
                "valueType": "any"
            },
            "active_paths": {
                "accumulatorType": "list",
                "valueType": "any"
            }
        },
        "phases": [
            {
                "name": "main",
                "initProgram": ["let",
                    [
                        ["number-of-paths", 5],
                        ["start-list", ["list",
                            [
                                "this-pregel-id"
                            ],
                            [
                                "this-vertex-id"
                            ]
                        ]]
                    ],
                    [
                        "if",
                        [
                            true,
                            ["seq",
                                ["accum-set!", "active_paths",
                                    ["list-repeat", ["var-ref", "start-list"], ["var-ref", "number-of-paths"]]
                                ],
                                "vote-active"
                            ]
                        ],
                        [
                            true,
                            "vote-halt"
                        ]
                    ]],
                "updateProgram": [
                    "let",
                    [
                        [
                            "max-path-length",
                            4
                        ],
                        [
                            "out-bound-edges",
                            [
                                "this-outbound-edges"
                            ]
                        ],
                        [
                            "terminate-path",
                            [
                                "lambda",
                                [
                                    "quote",
                                    []
                                ],
                                [
                                    "quote",
                                    [
                                        "path"
                                    ]
                                ],
                                [
                                    "quote",
                                    [
                                        "let",
                                        [
                                            [
                                                "start-vertex",
                                                [
                                                    "list-ref",
                                                    [
                                                        "var-ref",
                                                        "path"
                                                    ],
                                                    0
                                                ]
                                            ],
                                            [
                                                "filtered-path",
                                                [
                                                    "filter",
                                                    [
                                                        "lambda",
                                                        [
                                                            "quote",
                                                            []
                                                        ],
                                                        [
                                                            "quote",
                                                            [
                                                                "idx",
                                                                "v"
                                                            ]
                                                        ],
                                                        ["quote", [
                                                            "not",
                                                            [
                                                                "eq?",
                                                                0,
                                                                [
                                                                    "var-ref",
                                                                    "idx"
                                                                ]
                                                            ]
                                                        ]]
                                                    ],
                                                    [
                                                        "var-ref",
                                                        "path"
                                                    ]
                                                ]
                                            ]
                                        ],
                                        [
                                            "send-to-accum",
                                            "terminated_paths",
                                            [
                                                "var-ref",
                                                "start-vertex"
                                            ],
                                            [
                                                "var-ref",
                                                "filtered-path"
                                            ]
                                        ]
                                    ]
                                ]
                            ]
                        ]
                    ],
                    [
                        "for-each",
                        [
                            [
                                "path",
                                [
                                    "accum-ref",
                                    "active_paths"
                                ]
                            ]
                        ],
                        [
                            "if",
                            [
                                [
                                    "le?",
                                    ["+",
                                        ["*", 2, [
                                            "var-ref",
                                            "max-path-length"
                                        ]],
                                        1
                                    ],
                                    [
                                        "list-length",
                                        [
                                            "var-ref",
                                            "path"
                                        ]
                                    ]
                                ],
                                [
                                    [
                                        "var-ref",
                                        "terminate-path"
                                    ],
                                    [
                                        "var-ref",
                                        "path"
                                    ]
                                ]
                            ],
                            [
                                [
                                    "list-empty?",
                                    [
                                        "var-ref",
                                        "out-bound-edges"
                                    ]
                                ],
                                [
                                    [
                                        "var-ref",
                                        "terminate-path"
                                    ],
                                    [
                                        "var-ref",
                                        "path"
                                    ]
                                ]
                            ],
                            [
                                true,
                                [
                                    "let",
                                    [
                                        [
                                            "edge",
                                            ["list-ref",
                                                ["var-ref", "out-bound-edges"],
                                                ["floor",
                                                    ["rand-range",
                                                        0,
                                                        ["list-length",
                                                            ["var-ref", "out-bound-edges"]
                                                        ]
                                                    ]
                                                ]
                                            ]
                                        ]
                                    ],
                                    [
                                        "send-to-accum",
                                        "active_paths",
                                        [
                                            "attrib-ref",
                                            [
                                                "var-ref",
                                                "edge"
                                            ],
                                            "to-pregel-id"
                                        ],
                                        [
                                            "list-append",
                                            [
                                                "var-ref",
                                                "path"
                                            ],
                                            [
                                                "attrib-ref",
                                                ["var-ref", "edge"],
                                                ["list", "document", "_id"]
                                            ],
                                            [
                                                "attrib-ref",
                                                ["var-ref", "edge"],
                                                ["list", "document", "_to"]
                                            ]
                                        ]
                                    ]
                                ]
                            ]
                        ]

                    ],
                    ["accum-clear!", "active_paths"],
                    "vote-halt"
                ]
            }
        ]
    };
}

function random_walk(
    graphName,
    resultField
) {
    return pregel.start(
        "ppa",
        graphName,
        random_walk_program(
            resultField
        )
    );
}

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

function exec_test_random_walk_impl(graphSpec) {
    // Find the ID of a vertex to start at.

    internal.print(" -- computing random walk");

    testhelpers.wait_for_pregel(
        "Air Random Walk",
        random_walk(
            graphSpec.name,
            "random-walk"
        ));

    internal.print(" -- checking if paths are valid");

    return compare_pregel(db._query(`
        for v in @@V
            let paths = v["random-walk"].terminated_paths
            for p in paths
                filter length(p) != 1
                let bad_edges = (
                    for i in 0..(LENGTH(p) - 1)/2 - 1
                        let from = p[2 * i]
                        let to = p[2 * i + 2]
                        let edge_id = p[2 * i + 1]
                        let edge = DOCUMENT(@E, edge_id)
                        filter edge._to != to OR edge._from != from
                        return edge_id
                )
            filter length(bad_edges) != 0
            return {v, p}`, {
        "@V": graphSpec.vname,
        "E": graphSpec.ename
    }));
}

function exec_test_random_walk() {
    let results = [];

    results.push(exec_test_random_walk_impl(examplegraphs.create_line_graph("LineGraph10", 10, 1)));
    try {
        graphModule._drop("LineGraph10", true);
    } catch (ignore) {
    }

    results.push(exec_test_random_walk_impl(examplegraphs.create_line_graph("LineGraph100", 100, 9)));
    try {
        graphModule._drop("LineGraph100", true);
    } catch (ignore) {
    }

    results.push(exec_test_random_walk_impl(examplegraphs.create_line_graph("LineGraph1000", 1000, 18)));
    try {
        graphModule._drop("LineGraph1000", true);
    } catch (ignore) {
    }

    results.push(exec_test_random_walk_impl(examplegraphs.create_circle_graph("Circle10", 2, 1)));
    try {
        graphModule._drop("Circle10", true);
    } catch (ignore) {
    }

    results.push(exec_test_random_walk_impl(examplegraphs.create_circle_graph("Circle100", 4, 6)));
    try {
        graphModule._drop("Circle100", true);
    } catch (ignore) {
    }

    results.push(exec_test_random_walk_impl(examplegraphs.create_circle_graph("Circle1000", 8, 18)));
    try {
        graphModule._drop("Circle1000", true);
    } catch (ignore) {
    }

    results.push(exec_test_random_walk_impl(examplegraphs.create_complete_graph("Complete4", 10, 4)));
    try {
        graphModule._drop("Complete4", true);
    } catch (ignore) {
    }

    results.push(exec_test_random_walk_impl(examplegraphs.create_complete_graph("Complete10", 10, 10)));
    try {
        graphModule._drop("Complete10", true);
    } catch (ignore) {
    }

    results.push(exec_test_random_walk_impl(examplegraphs.create_complete_graph("Complete100", 10, 100)));
    try {
        graphModule._drop("Complete100", true);
    } catch (ignore) {
    }

    results.push(exec_test_random_walk_impl(examplegraphs.create_wiki_vote_graph("WikiVote", 1)));
    try {
        graphModule._drop("WikiVote", true);
    } catch (ignore) {
    }

    results.push(exec_test_random_walk_impl(examplegraphs.create_wiki_vote_graph("WikiVote", 9)));
    try {
        graphModule._drop("WikiVote", true);
    } catch (ignore) {
    }

    results.push(exec_test_random_walk_impl(examplegraphs.create_wiki_vote_graph("WikiVote", 18)));
    try {
        graphModule._drop("WikiVote", true);
    } catch (ignore) {
    }

    return !results.includes(false);
}

function test() {
    return exec_test_random_walk();
}
