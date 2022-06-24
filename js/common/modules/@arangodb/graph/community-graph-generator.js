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

const communityGenerator = function (vColl, eColl, graphName, numberOfShards, replicationFactor) {
    return {
      setUp: function() {
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
      makeLabeledEdge: function (from, to, label) {
          return {
            _from: `${vColl}/${label}_${from}`,
            _to: `${vColl}/${label}_${to}`,
            vertex: `${label}_${from}`,
            label: `${label}`
          };
      },
      makeLabeledVertex: function (name, label) {
          return {
            _key: `${label}_${name}`,
            label: `${label}`
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
