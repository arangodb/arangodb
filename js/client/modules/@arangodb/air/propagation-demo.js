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
const db = internal.db;

/*


*/
exports.propagation_demo_program = demand_propagation_demo_program;
exports.create_demo_graph = function() { create_demo_propagation_graph("Propagation_Demo", 1); };
exports.test = test;
exports.demo = demo;

const graphModule = require("@arangodb/smart-graph");
const _ = require("lodash");

/* This demo graph is very simple for starters, it's only a binary tree
   but we can indeed propagate through any graph (even with cycles, see
   the pagerank example)

                        A
                      / | \
                     /  |  \
                    /   |   \
                   '    '    '
                   B    C    D
                       / \   / \
                      '   ' '   '
                      E   F G   H
                               / \
                              '   '
                              I   J
*/
function create_demo_propagation_graph(graphName, numberOfShards) {
  const vname = graphName + "_V";
  const ename = graphName + "_E";
  try {
    graphModule._drop(graphName, true);
  } catch (e) {}
  graphModule._create(
    graphName,
    [graphModule._relation(ename, vname, vname)],
    [],
    { smartGraphAttribute: "name", numberOfShards: numberOfShards }
  );

  var vdocs = db._collection(vname).save([
    { name: "A", demandBucket0:  0, demandBucket1: 0 },
    { name: "B", demandBucket0:  1, demandBucket1: 2 },
    { name: "C", demandBucket0:  0, demandBucket1: 0 },
    { name: "D", demandBucket0:  0, demandBucket1: 0 },
    { name: "E", demandBucket0:  7, demandBucket1: 1 },
    { name: "F", demandBucket0: 11, demandBucket1: 0 },
    { name: "G", demandBucket0: 10, demandBucket1: 9 },
    { name: "H", demandBucket0:  0, demandBucket1: 0 },
    { name: "I", demandBucket0:  2, demandBucket1: 7 },
    { name: "J", demandBucket0:  1, demandBucket1: 5 },
  ]);
  let [A, B, C, D, E, F, G, H, I, J, ...rest] = vdocs.map(x => x._id);

  var es = [];

  es.push({ _from: A, _to: B, cost: 1});
  es.push({ _from: A, _to: C, cost: 1});
  es.push({ _from: A, _to: D, cost: 1});

  es.push({ _from: C, _to: E, cost: 1});
  es.push({ _from: C, _to: F, cost: 1});

  es.push({ _from: D, _to: G, cost: 1});
  es.push({ _from: D, _to: H, cost: 1});

  es.push({ _from: H, _to: I, cost: 1});
  es.push({ _from: H, _to: J, cost: 1});

  db._collection(ename).save(es);

  return { name: graphName, vname: vname, ename: ename };
}


/* Todo: can we fold "update sums" into this accumulator (i.e. we keep an update and the sum,
   and fold these two things into one accumulator) */
function bucketizedSumAccumulator(nrBuckets) {
    let entry = null;
    return {
      updateProgram: [
                ["seq",
                 ["for-each", ["entry", [...Array(nrBuckets).keys()]],
                    ["this-set!",
                     ["array-set", entry,
                      ["+",
                       ["array-ref", ["current-value"], entry],
                       ["array-ref", ["input-value"], entry ]]]]],
                    "hot"
                ]
        // TODO: detect whether all entries are 0 and return "cold"? [true, "cold"]
            ],
      clearProgram: ["this-set!", Array(nrBuckets).fill(0)],
      getProgram: ["current-value"],
      setProgram: ["this-set!", ["input-value"]],
      finalizeProgram: ["current-value"],
    };
}

/*
 * This pregel program propagates demands from leaf nodes
 * "upwards"
 *
 */
function demand_propagation_demo_program(resultField) {
    return {
        /* The field into which we write the result (which is currently an object
         * containing all accumulator values)
         */
        "resultField": "demand_prop_result",
        /*
         * Maximal number of global supersteps (i.e. iterations);
         * ideally this is just a safety net to make sure the
         * algorithm doesn't run indefinitely
         *
         */
        "maxGSS": 1000,
        /* Global accumulators exist once per program and can
           receive values from all vertices */
        "globalAccumulators": {
            "converged": {
                "accumulatorType": "or",
                "valueType": "bool"
            }
        },
        /* Vertex accumulators are instanciated per vertex
           for each run of the algorithm */
        "vertexAccumulators": {
            "demandSumBuckets": {
                "accumulatorType": "custom",
                "customType": "bucketizedSum",
                "valueType": "any"
            },
            "finalDemandSumBuckets": {
                "accumulatorType": "custom",
                "customType": "bucketizedSum",
                "valueType": "any"
            },
        },
        "customAccumulators": {
            bucketizedSum: bucketizedSumAccumulator()
        },
        /* A pregel program is structured into *phases*,

           On entry into a phase, `initProgram` runs once on each vertex,
           then `updateProgram` runs until either all vertices have
           voted to halt, no updates have happened, or the maximum number
           of "global supersteps" has been reached.

           The execution of `initProgam` and `updateProgram` are lockstepped: *All*
           vertices have to finish running `initProgram` before `updateProgram` is started,
           and *all* vertices have to finish their i-th iteration of `updateProgram`
           before the next one is started.
        */
        "phases": [{
            "name": "init",
            "initProgram": ["seq",
                ["accum-set!", "demandSumBuckets", ["attrib-ref", "init_demandSumBuckets", ["this-doc"]]],
                ["send-to-all-neighbours", "predecessors", ["this-pregel-id"]],
                false],
            "updateProgram": ["seq",
                ["send-to-accum", ["this-pregel-id"], "finalDemandSumBuckets", ["accum-ref", "demandSumBuckets"]],
                ["for-each",
                    ["pred", ["accum-ref", "predecessors"]],
                    ["seq",
                        ["send-to-accum",
                            ["var-ref", "pred"],
                            "demandSumBuckets",
                            ["accum-ref", "demandSumBuckets"]],
                        ["accum-clear!", "demandSumBuckets"],
                        false]
                ]]
        }]
    };
}

/* As a possible surface syntax example:

   (seq
   (accum-set! "demandSumBuckets" (attrib-ref demandSumBuckets (this-doc)))
   (send-to-all-neighbors "predecessors" (this-pregel-id)))

   (seq
   (send-to-accum "finalDemandSumBuckets" (this-pregel-id) demandSumBuckets)
   (for-each (pred predecessors)
     (send-to-accum "demandSumBuckets" (attrib-ref "to-pregel-id" edge) demandSumBuckets)))
   (accum-clear! "demandSumBuckets"))


  or another (sneaky) possibility

  initProgram: function() {
    accum_set("demandSumBucket_0", attrib_ref(demandSumBucket_0, this_doc()));
    send_to_all_neighbors("predecessors", this_pregel_id());
    return false;
  }

  updateProgram: function() {
    send_to_accum("finalDemandSumBuckets", this_pregel_id(), demandSumBuckets);
    for(pred in predecessors) {
      send_to_accum("demandSumBuckets", edge.`to-pregel-id`, demandSumBuckets));
    }
    accum_clear("demandSumBuckets");
    return false;
  }
*/

function test() {}

function demo() {
  create_demo_propagation_graph("Propagation_Demo", 1);
  return testhelpers.wait_for_pregel(
    "propagation demo",
    pregel.start(
      "ppa",
      "Propagation_Demo",
      demand_propagation_demo_program("propagation")));
}
