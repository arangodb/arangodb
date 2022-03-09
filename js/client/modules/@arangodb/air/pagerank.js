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

const arangodb = require("@arangodb");
const db = arangodb.db;
const pregel = require("@arangodb/pregel");
const examplegraphs = require("@arangodb/air/pregel-example-graphs");
const testhelpers = require("@arangodb/air/test-helpers");
const {sumAccumulator, storeAccumulator, maxAccumulator} = require("./accumulators");
const _ = require("lodash");

// ee/ce check + gm selection
const isEnterprise = require("internal").isEnterprise();
const graphModule = isEnterprise? require("@arangodb/smart-graph") : require("@arangodb/general-graph");

exports.pagerank_program = pagerank_program;
exports.pagerank = pagerank;
exports.test = test;

function pagerank_program(resultField, dampingFactor, traceVertex) {
  return {
    resultField: resultField,
    // TODO: Karpott.
    maxGSS: 1000,
    globalAccumulators: {
      delta: {
        accumulatorType: "custom",
        valueType: "any",
        customType: "maxAccumulator",
      }
    },
    vertexAccumulators: {
      rank: {
        accumulatorType: "custom",
        valueType: "any",
        customType: "storeAccumulator",
      },
      receiver: {
        accumulatorType: "custom",
        valueType: "any",
        customType: "sumAccumulator",
      }
    },
    customAccumulators: {
      sumAccumulator: sumAccumulator(),
      storeAccumulator: storeAccumulator(),
      maxAccumulator: maxAccumulator(),
    },
    phases: [
      {
        name: "main",
        initProgram: [
          "seq",
          ["accum-set!", "rank", ["/", 1, ["vertex-count"]]],
          ["accum-set!", "receiver", 0],
          ["gt?", ["this-outbound-edges-count"], 0]
        ],
        onPreStep: ["global-accum-set!", "delta", 0],
        onPostStep: ["if",
          [
            ["and",
              ["lt?", ["global-accum-ref", "delta"], 0.001],
              ["ne?", ["global-superstep"], 0]
            ],
            ["finish"]
          ]
        ],
        updateProgram: [
          "seq",
          ["let",
            [
              [
                "new-rank",
                [
                  "+",
                  ["/", ["-", 1, dampingFactor], ["vertex-count"]],
                  ["*", dampingFactor, ["accum-ref", "receiver"]],
                ]
              ]
            ],
            ["send-to-global-accum", "delta",
              ["abs", ["-",
                ["accum-ref", "rank"],
                ["var-ref", "new-rank"]
              ]]
            ],
            [
              "accum-set!",
              "rank",
              ["var-ref", "new-rank"]
            ],
          ],
          ["accum-set!", "receiver", 0],
          ["if",
            [
              ["gt?", ["this-outbound-edges-count"], 0],
              [
                "send-to-all-neighbours",
                "receiver",
                ["/", ["accum-ref", "rank"], ["this-outbound-edges-count"]],
              ]
            ]
          ],
          true,
        ],
      },
    ],
  };
}

function pagerank(graphName, resultField, dampingFactor, vertex) {
  return pregel.start(
    "ppa",
    graphName,
    pagerank_program(resultField, dampingFactor, vertex)
  );
}

function exec_test_pagerank_on_graph(graphSpec, vertex) {
  testhelpers.wait_for_pregel(
    "Air Pagerank",
    pagerank(
      graphSpec.name,
      "pageRankResult",
      0.85,
      vertex,
    ));

  testhelpers.wait_for_pregel(
    "Native Pagerank",
    pregel.start("pagerank", graphSpec.name, {
      maxGSS: 5,
      resultField: "nativeRank"
    })
  );

  // Return results which differ too much (here currently 0.05)
  return testhelpers.compare_pregel(
    db._query(`FOR d IN @@V
               FILTER ABS(d.nativeRank - d.pageRankResult.rank) >= 0.05
               RETURN {
                 name: d.name,
                 native: d.nativeRank,
                 air: d.pageRankResult.rank
               }`, {"@V": graphSpec.vname})
  );
}

function exec_test_pagerank(vertex) {
  let results = [];

  results.push(exec_test_pagerank_on_graph(examplegraphs.create_pagerank_graph("PageRankGraph1", 1), vertex));
  try {
    graphModule._drop("PageRankGraph1", true);
  } catch (ignore) {
  }

  results.push(exec_test_pagerank_on_graph(examplegraphs.create_pagerank_graph("PageRankGraph9", 9), vertex));
  try {
    graphModule._drop("PageRankGraph9", true);
  } catch (ignore) {
  }

  results.push(exec_test_pagerank_on_graph(examplegraphs.create_pagerank_graph("PageRankGraph18", 18), vertex));
  try {
    graphModule._drop("PageRankGraph18", true);
  } catch (ignore) {
  }

  results.push(exec_test_pagerank_on_graph(examplegraphs.create_wiki_vote_graph("WikiVoteGraph1", 1)));
  try {
    graphModule._drop("WikiVoteGraph1", true);
  } catch (ignore) {
  }

  results.push(exec_test_pagerank_on_graph(examplegraphs.create_wiki_vote_graph("WikiVoteGraph9", 9)));
  try {
    graphModule._drop("WikiVoteGraph9", true);
  } catch (ignore) {
  }

  results.push(exec_test_pagerank_on_graph(examplegraphs.create_wiki_vote_graph("WikiVoteGraph18", 18)));
  try {
    graphModule._drop("WikiVoteGraph18", true);
  } catch (ignore) {
  }

  if (results.includes(false)) {
    return false;
  }
  return true;
}

function test(vertex = "") {
  return exec_test_pagerank(vertex);
}
