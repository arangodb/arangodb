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

/* Execute the given pregel program */

/* TODO parametrise with bindings?  */
function execute_program(graphName, program) {
    return pregel.start("vertexaccumulators", graphName, program);
}

function test_bind_parameter_program(bindParameter, value) {
    return {
        resultField: "bindParameterTest",
        accumulatorsDeclaration: {
            distance: {
                accumulatorType: "min",
                valueType: "doubles",
                storeSender: true,
            },
        },
        bindings: {"bind-test-name": "Hello, world"},
        initProgram: ["seq", ["print", ["bind-ref", "bind-test-name"]]],
        updateProgram: ["seq", ["print", "hallo"]],
    };
}


/* Computes the vertex degree */

function vertex_degree_program(resultField) {
  return {
    resultField: resultField,
    maxGSS: 2,
    accumulatorsDeclaration: {
      inDegree: {
        accumulatorType: "sum",
        valueType: "ints",
        storeSender: false
      }
    },
    initProgram: [ "for",
                   "outbound",
                   ["quote", "edge"],
                   [
                     "quote",
                     "seq",
                     [ "update",
                       "inDegree",
                       ["attrib", "_to", ["var-ref", "edge"]], 1],
                   ],
                 ],
    updateProgram: [ false ]
  };
}

function vertex_degree(
  graphName,
  resultField) {
  return pregel.start(
    "vertexaccumulators",
    graphName,
    vertex_degree_program(resultField)
  );
}

/* Performs a single-source shortest path search (currently without path reconstruction)
   on all vertices in the graph starting from startVertex, using the cost stored
   in weightAttribute on each edge and storing the end result in resultField as an object
   containing the attribute distance */

/*

  (for this -: edge :-> that
    (update that.distance with this.distance + edge.weightAttribute))

*/
function single_source_shortest_path_program(
    resultField,
    startVertexId,
    weightAttribute
) {
    return {
        resultField: resultField,
        // TODO: Karpott.
        maxGSS: 10000,
        accumulatorsDeclaration: {
            distance: {
                accumulatorType: "min",
                valueType: "doubles",
                storeSender: true,
            },
        },
        initProgram: [
            "seq",
            [
                "if",
                [
                    ["eq?", ["this"], startVertexId],
                    ["seq", ["set", "distance", 0], true],
                ],
                [true, ["seq", ["set", "distance", 9223372036854776000], false]],
            ],
        ],
        updateProgram: [
            "seq",
            [
                "for",
                "outbound",
                ["quote", "edge"],
                [
                    "quote",
                    "seq",
                    [
                        "update",
                        "distance",
                        ["attrib", "_to", ["var-ref", "edge"]],
                        ["+", ["accum-ref", "distance"], ["attrib", weightAttribute, ["var-ref", "edge"]]],
                    ],
                ],
            ],
            false,
        ],
    };
}

function single_source_shortest_path(
    graphName,
    resultField,
    startVertexId,
    weightAttribute
) {
    return pregel.start(
        "vertexaccumulators",
        graphName,
        single_source_shortest_path_program(
            resultField,
            startVertexId,
            weightAttribute
        )
    );
}

function strongly_connected_components_program(
    resultField,
) {
    return {
        resultField: resultField,
        // TODO: Karpott.
        maxGSS: 50,
        accumulatorsDeclaration: {
            forwardMin: {
                accumulatorType: "min",
                valueType: "ints",
            },
            backwardMin: {
                accumulatorType: "min",
                valueType: "ints",
            },
            isDisabled: {
                accumulatorType: "store",
                valueType: "bool",
            },
            mySCC: {
                accumulatorType: "store",
                valueType: "ints",
            },
            activeInbound: {
                accumulatorType: "list",
                valueType: "slice",
            },
        },
        phases: [
            {
                name: "init",
                initProgram: [
                    "seq",
                    ["set", "isDisabled", false],
                    false
                ],
                updateProgram: false,
            },
            {
                name: "broadcast",
                initProgram: [
                    "seq",
                    ["set", "activeInbound", ["quote"]],
                    //["print", "isDisabled =", ["accum-ref", "isDisabled"]],
                    [
                        "if",
                        [
                            ["not", ["accum-ref", "isDisabled"]],
                            [
                                "for",
                                "outbound",
                                ["quote", "edge"],
                                ["quote", "seq",
                                    //["print", ["vertex-unique-id"], "sending to vertex", ["attrib", "_to", ["var-ref", "edge"]], "with value", ["pregel-id"]],
                                    [
                                        "update",
                                        "activeInbound",
                                        ["attrib", "_to", ["var-ref", "edge"]],
                                        ["pregel-id"]
                                    ]
                                ]
                            ]
                        ]
                    ],
                    true
                ],
                updateProgram: false,
            },
            {
                name: "forward",
                initProgram: ["if",
                    [
                        ["accum-ref", "isDisabled"],
                        false
                    ],
                    [
                        true, // else
                        ["seq", ["set", "forwardMin", ["vertex-unique-id"]], true]
                    ]
                ],
                updateProgram: ["if",
                    [
                        ["accum-ref", "isDisabled"],
                        false
                    ],
                    [
                        true, // else
                        [
                            "seq",
                            [
                                "for",
                                "outbound",
                                ["quote", "edge"],
                                ["quote", "seq",
                                    //["print", ["vertex-unique-id"], "sending to vertex", ["attrib", "_to", ["var-ref", "edge"]], "with value", ["accum-ref", "forwardMin"]],
                                    [
                                        "update",
                                        "forwardMin",
                                        ["attrib", "_to", ["var-ref", "edge"]],
                                        ["accum-ref", "forwardMin"]
                                    ]
                                ]
                            ],
                            false
                        ]
                    ]
                ]
            },
            {
                name: "backward",
                onHalt: ["goto", "broadcast"],
                initProgram: ["if",
                    [
                        ["accum-ref", "isDisabled"],
                        false
                    ],
                    [
                        ["eq?", ["vertex-unique-id"], ["accum-ref", "forwardMin"]],
                        [
                            "seq",
                            ["print", ["vertex-unique-id"], "I am root of a SCC!"],
                            ["set", "backwardMin", ["accum-ref", "forwardMin"]],
                            true
                        ]
                    ],
                    [
                        true,
                        [
                            "seq",
                            ["print", ["vertex-unique-id"], "I am _not_ root of a SCC"],
                            ["set", "backwardMin", 99999],
                            false
                        ]
                    ]
                ],
                updateProgram: ["if",
                    [
                        ["accum-ref", "isDisabled"],
                        false
                    ],
                    [
                        ["eq?", ["accum-ref", "backwardMin"], ["accum-ref", "forwardMin"]],
                        [
                            "seq",
                            ["set", "isDisabled", true],
                            ["set", "mySCC", ["accum-ref", "forwardMin"]],
                            ["print", "I am done, my SCC id is", ["accum-ref", "forwardMin"]],
                            [
                                "for-each",
                                ["vertex", ["accum-ref", "activeInbound"]],
                                ["seq",
                                    //["print", ["vertex-unique-id"], "sending to vertex", ["var-ref", "vertex"], "with value", ["accum-ref", "backwardMin"]],
                                    [
                                        "update-by-id",
                                        "backwardMin",
                                        ["var-ref", "vertex"],
                                        ["accum-ref", "backwardMin"]
                                    ]
                                ]
                            ],
                            false,
                        ]
                    ],
                    [
                        true,
                        ["seq", ["print", "Was woken up, but min_f != min_b"], false]
                    ]
                ],
            }
        ]
    };
}


function strongly_connected_components(
    graphName,
    resultField
) {
    return pregel.start(
        "vertexaccumulators",
        graphName,
        strongly_connected_components_program(
            resultField
        )
    );
}

function page_rank_program(
    resultField,
    startVertexId,
    weightAttribute
) {
    return {
        resultField: resultField,
        // TODO: Karpott.
        maxGSS: 10000,
        accumulatorsDeclaration: {
            rank: {
                accumulatorType: "sum",
                valueType: "doubles",
                storeSender: false
            },
        },
        initProgram: [
            [
                "seq",
                [
                    "for",
                    "outbound",
                    ["quote", "edge"],
                    [
                        "quote",
                        "seq",
                        [
                            "update",
                            "rank",
                            ["attrib", "_to", ["var-ref", "edge"]],
                            ["/", ["/", 1, ["vertex-count"]], ["attrib", numOutgoingEdges, ["var-ref", "edge"]]],
                        ],
                    ],
                ],
                true
            ]
        ],
        updateProgram: [
            "seq",
            [
                "for",
                "outbound",
                ["quote", "edge"],
                [
                    "quote",
                    "seq",
                    [
                        "update",
                        "rank",
                        ["attrib", "_to", ["var-ref", "edge"]],
                        ["/", ["accum-ref", "rank"], ["attrib", numOutgoingEdges, ["var-ref", "edge"]]],
                    ],
                ],
            ],
            ["set", "rank", 0],
            true
        ],
    };
}

function page_rank(
    graphName,
    resultField
) {
    return pregel.start(
        "vertexaccumulators",
        graphName,
        page_rank_program(
            resultField
        )
    );
}

exports.vertex_degree = vertex_degree;

exports.single_source_shortest_path_program = single_source_shortest_path_program;
exports.single_source_shortest_path = single_source_shortest_path;
exports.strongly_connected_components_program = strongly_connected_components_program;
exports.strongly_connected_components = strongly_connected_components;
exports.page_rank_program = page_rank_program;
exports.page_rank = page_rank;
exports.test_bind_parameter_program = test_bind_parameter_program;

exports.execute = execute_program;
