/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertTrue, assertNotEqual, JSON */
'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / @brief Pregel Tests: graph generation
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2022 ArangoDB GmbH, Cologne, Germany
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
// / @author Roman Rabinovich
// //////////////////////////////////////////////////////////////////////////////


const db = require("@arangodb").db;
const graph_module = require("@arangodb/general-graph");

// TODO/FIXME: maybe we could just store vertices/edges in this closure and flush/store them
// to the underlying graph?
const communityGenerator = function (graphName, vColl, eColl, numberOfShards, replicationFactor) {
      const makeLabeledEdge = function (from, to, label) {
	  let r = {
              _from: `${vColl}/${from}`,
              _to: `${vColl}/${to}`,
              vertex: `${from}`,
          };
	  if (label !== undefined) {
	    r.label = label;
	  }
          return r;
      };
      const makeLabeledVertex = function (name, label) {
          let r = { _key: `${name}` };
	  if (label !== undefined) {
	    r.label = label;
	  }
	  return r;
      };

    return {
      graphName: graphName,
      vColl: vColl,
      eColl: eColl,
      setUp: function() {
	db._createDocumentCollection(vColl, {
          numberOfShards: numberOfShards,
	  replicationFactor: replicationFactor
	});
        db._createEdgeCollection(eColl, {
          numberOfShards: numberOfShards,
          replicationFactor: replicationFactor,
          shardKeys: ["vertex"],
          distributeShardsLike: vColl
        });
        graph_module._create(graphName, [graph_module._relation(eColl, vColl, vColl)], []);
      },

      tearDown: function() {
        graph_module._drop(graphName, true);
      },

      with_label: function(label) {
	return {
          makeEdge: function(from, to) { return makeLabeledEdge(from, to, label); },
          makeVertex: function(name) { return makeLabeledVertex(name, label); }
	};
      },

      store: function(graph) {
        let {vertices, edges} = graph;
        db[vColl].save(vertices);
        db[eColl].save(edges);
      }
    };
};

exports.communityGenerator = communityGenerator;
