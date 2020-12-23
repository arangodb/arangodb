////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
///
/// Licensed under the Apache License, Version 2.0 (the "License")
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Tobias Gödderz
/// @author Copyright 2020, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

'use strict';

const internal = require("internal");
const db = internal.db;
const graphModule = require("@arangodb/smart-graph");
const pregel = require("@arangodb/pregel");
const _ = require("lodash");
const examplegraphs = require("@arangodb/air/pregel-example-graphs");
const testhelpers = require("@arangodb/air/test-helpers");
const accumulators = require("@arangodb/air/accumulators");


exports.push_relabel = push_relabel;
exports.push_relabel_program = push_relabel_program;
exports.test = test;

function push_relabel(graphName, s, t) {
  return pregel.start(
    "air",
    graphName,
    push_relabel_program(s, t)
  );
}

/*
 * Assumes that there are no duplicate (directed) edges. If there are two or
 * more edges between the same vertices (in the same direction), replace them
 * with a single edge that has the sum of the original edges capacities as
 * capacity.
 * Assumes every edge has a reverse edge. If that's not given, just add a
 * reverse edge with capacity 0 to each edge that doesn't have a reverse edge.
 */
function push_relabel_program() {
  return {
    resultField: "dbgOut",
    maxGSS: 1000,
    globalAccumulators: {
    },
    vertexAccumulators: {
      "flow": {
        // Holds a map "neighborId" -> {flow, capacity, edgeId}.
        // TODO Check/try if using the adjacent vertex's _id, rather than its
        //      stringified pregel id, wouldn't be a better choice for the
        //      key.
        // Note that we can't update an edge together with its reverse edge,
        // at least not without an additional Pregel step, as they are outbound
        // edges of different vertices.
        // Thus we cannot reduce opposite flows s.t. at least one vanishes.
        // Instead, if flow is sent from a vertex v via some edge (v, w), the
        // capacity on the reverse edge (w, e) is increased by that amount when
        // the message arrives at the neighboring vertex.
        // This means that both flow and capacity are ever-increasing. They can
        // be reduced afterwards in a post-processing step.
        //
        // Note that edges with flow = capacity must always be ignored, as they
        // are not part of the residual graph!
        "accumulatorType": "custom",
        "valueType": "slice",
        "customType": "flowAccumulator",
      },
      "label": {
        // TODO complete the label accumulator
        "valueType": "int",
      },
    },
    customAccumulators: {
      "flowAccumulator": {
        "updateProgram": ["let", [["senderId", ["to-json-string", ["attrib-ref", ["input-value"], "sender"]]]],
          //["print", "update vertex: ", ["attrib-ref", ["this-doc"], "name"]],
          ["print", "input-value: ", ["input-value"]],
          ["print", "accum value: ", ["attrib-ref-or-fail", ["current-value"], ["var-ref", "senderId"]]],
          ["print", "old accum: ", ["current-value"]],
          ["let",
            [["newAccum", ["attrib-set", ["current-value"], ["var-ref", "senderId"],
              ["let",
                [["accumEntry",
                  ["attrib-ref", ["current-value"], ["var-ref", "senderId"]]]],
                ["print", "accumEntry: ", ["var-ref", "accumEntry"]],
                ["let",
                  [["minFlow",
                    ["min", ["attrib-ref", ["var-ref", "accumEntry"], "flow"], ["attrib-ref", ["input-value"], "additionalFlow"]]]],
                  ["print", "minFlow: ", ["var-ref", "minFlow"]],
                  ["let",
                    [["newCapacity",
                      ["+", ["attrib-ref", ["var-ref", "accumEntry"], "capacity"],
                        ["attrib-ref", ["input-value"], "additionalFlow"]]]],
                    ["print", "newCapacity: ", ["var-ref", "newCapacity"]],
                    ["dict-merge", ["var-ref", "accumEntry"],
                      ["dict",
                        ["list", "flow", ["var-ref", "newFlow"]],
                        ["list", "capacity", ["var-ref", "newCapacity"]]]]]]]]]],
            ["print", "new accum: ", ["var-ref", "newAccum"]],
            ["this-set!", ["var-ref", "newAccum"]]]
        ],
        "clearProgram": ["this-set!", ["dict"]],
        "getProgram": ["current-value"],
        "setProgram": ["this-set!", ["input-value"]],
        // "finalizeProgram": ["current-value"],
        // Finalize as a map {edgeId -> flow}.
        "finalizeProgram": ["apply", "dict", ["map", ["lambda", ["list"], ["list", "i", "k"], ["quote",
            ["let", [["v", ["attrib-ref-or-fail", ["current-value"], ["var-ref", "k"]]]],
              ["list", ["attrib-ref-or-fail", ["var-ref", "v"], "id"], ["attrib-ref-or-fail", ["var-ref", "v"], "flow"]]]
          ]], ["dict-keys", ["current-value"]]]],
      }
    },
    // There currently are six phases. Maybe this count can be reduced, but this
    // way it seems most simple to describe.
    //
    // Note that edges with flow = capacity must never be considered, as they
    // are not part of the residual graph!
    //  * init-phase:
    //    runs an initProgram once, which initializes all accumulators, i.e.
    //    sets flow, capacity, and label. It then saturates all outgoing edges
    //    of the source vertex `s` and activates all vertices incident to `s`,
    //    and only those.
    //  * push-phase:
    //    runs an updateProgram that pushes all admissible edges from active
    //    vertices.
    //    if no push happened, continue with the fetch-label-phase.
    //    if no push happened, and no vertex is active, go to the finalize-phase.
    //  * fetch-label-phase:
    //    all active vertices send messages to their neighbors over all outgoing
    //    edges to ask for their label.
    //    always followed by a send-label-phase.
    //  * send-label-phase:
    //    all vertices that got messages in the fetch-label-phase send back
    //    their label to the respective inquirer(s)
    //    always followed by a relabel-phase.
    //  * relabel-phase:
    //    now exactly the vertices should be active again that sent messages in
    //    the fetch-label-phase. they all got responses to their label inquiries
    //    and can now relabel themselves.
    //    after that, we unconditionally continue with the push-phase. there is
    //    at least one admissible edge now.
    //  * finalize-phase:
    //    in the first superstep, all vertices must send over all outgoing edges
    //    how much flow they got.
    //    in the second superstep, for each message, the corresponding reverse
    //    edge's flow can be reduced by subtracting the minimum of both flows.
    //    after that, of each pair of reverse edges, at most one has positive
    //    flow.
    //    maybe this step can also be done in an AQL-post-processing instead.
    phases: [
      {
        name: "init-phase",
        initProgram: ["seq",
          ["print", "init vertex: ", ["attrib-ref", ["this-doc"], "name"], ", ", ["attrib-ref", ["this-doc"], "_id"], ", ", ["this-pregel-id"]],
          // Initialize flow accumulator
          ["accum-set!", "flow", ["apply", "dict",
            ["map",
              ["lambda",
                ["list"],
                ["list", "idx", "edge"],
                ["quote", ["list",
                  // edges are identified by their neighbor. this is necessary
                  // so we can find the matching edge when flow arrives over its
                  // reverse edge.
                  // only outgoing edges are administered here, so each edge is
                  // managed at exactly one vertex.
                  // we assume to have at most one edge per neighbor.
                  ["to-json-string", ["attrib-ref", ["var-ref", "edge"], "to-pregel-id"]],
                  ["dict",
                    ["list", "flow", 0],
                    ["list", "capacity", ["attrib-ref", ["var-ref", "edge"], ["list", "document", "capacity"]]],
                    ["list", "id", ["attrib-ref", ["var-ref", "edge"], ["list", "document", "_id"]]]],
                ]]],
              ["this-outbound-edges"]]
          ]],
          // TODO initialize label accumulator with `n` for vertex "s", and 0
          //      for all other vertices. `n` is available via ["vertex-count"]!
          ["print", "flow accumulator: ", ["accum-ref", "flow"]],
          // Saturate all outgoing edges of s (also in our flow accumulator).
          // TODO It'll be helpful to write a lambda "add-flow" or so, that both
          //      updates the accumulator and sends a message. However, maybe
          //      that will just be "push" - I think I'll better see later how
          //      to extract some common code.
          ["if",
            [["eq?", "s", ["attrib-ref", ["this-doc"], "name"]], ["seq",
              // saturate all edges on the source
              ["for-each", [["edge", ["this-outbound-edges"]]],
                ["seq",
                  // send flow to neighbors
                  ["send-to-accum", ["attrib-ref", ["var-ref", "edge"], "to-pregel-id"], "flow",
                    ["dict",
                      ["list", "sender", ["this-pregel-id"]],
                      ["list", "senderName", ["attrib-ref", ["this-doc"], "name"]],
                      ["list", "additionalFlow", ["attrib-ref", ["var-ref", "edge"], ["list", "document", "capacity"]]]]]
              ]],
              // set flow in our accumulator
              ["accum-set!", "flow", ["map", ["lambda", ["list"], ["list", "k", "v"], ["quote",
                ["attrib-set", ["var-ref", "v"], "flow", ["attrib-get", ["var-ref", "v"], "capacity"]]
              ]], ["accum-ref", "flow"]]]
          ]]],
          ["print", "flow accumulator after: ", ["accum-ref", "flow"]],
          // no vertex should be active but for the ones we just send messages to
          "vote-halt"
        ],
      },
      {
        name: "push-phase",
        // TODO
      },
      {
        name: "fetch-label-phase",
        // TODO
      },
      {
        name: "send-label-phase",
        // TODO
      },
      {
        name: "relabel-phase",
        // TODO
      },
      {
        name: "finalize-phase",
        // TODO
      },
    ],
  };
}

function exec_test_push_relabel_on_graph(graphSpec) {
  internal.print(" -- computing push-relabel " + graphSpec.source + " -> " + graphSpec.target);

  const status = testhelpers.wait_for_pregel(
    "Air Push-Relabel",
    push_relabel(graphSpec.name, graphSpec.source, graphSpec.target)
  );

  const reports = status.reports;
  status.reports = reports.length;

  console.warn(status);

  /**
   * @type {{level: string, msg: string}} report
   */
  for (let report of reports) {
    const log = (() => {
      switch (report.level) {
        case 'fatal':
          return console.fatal;
        case 'error':
          return console.error;
        case 'warn':
          return console.warn;
        default:
          return console.info;
      }
    })();
    log(`[${report.level}]`, report.annotations);
    internal.print(report.msg);
  }

  console.log(Object.fromEntries(db[graphSpec.vname].toArray().map(v => [v.name, v.dbgOut])));

}

function exec_test_push_relabel() {
  let results = [];

  results.push(exec_test_push_relabel_on_graph(examplegraphs.create_network_flow_graph("SimpleNetworkFlowGraph")));
  try {
    graphModule._drop("SimpleNetworkFlowGraph", true);
  } catch (ignore) {
  }
}

function test() {
  return exec_test_push_relabel();
}
