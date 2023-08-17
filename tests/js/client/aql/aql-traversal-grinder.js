/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global AQL_EXPLAIN, assertEqual, assertTrue */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for traversal optimization
///
/// DISCLAIMER
///
/// Copyright 2010-2014 triagens GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2020, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const db = require("@arangodb").db;
const { deriveTestSuite } = require('@arangodb/test-helper');
const isCoordinator = require('@arangodb/cluster').isCoordinator();
const isEnterprise = require("internal").isEnterprise();
  
const vn = "UnitTestsVertex";
const en = "UnitTestsEdges";
const gn = "UnitTestsGraph";

function BaseTestConfig() {
  return {

    testTraversalVariations: function () {
      [
        "FOR v, e, p IN @minDepth..@maxDepth OUTBOUND @start GRAPH '" + gn + "' RETURN p",
        "FOR v, e, p IN @minDepth..@maxDepth OUTBOUND @start GRAPH '" + gn + "' RETURN p[0]",
        "FOR v, e, p IN @minDepth..@maxDepth OUTBOUND @start GRAPH '" + gn + "' RETURN LENGTH(p)",
        "FOR v, e, p IN @minDepth..@maxDepth OUTBOUND @start GRAPH '" + gn + "' RETURN NOEVAL(p.edges)",
        "FOR v, e, p IN @minDepth..@maxDepth OUTBOUND @start GRAPH '" + gn + "' RETURN p[CONCAT('ver', 'tices')]",
        "FOR v, e, p IN @minDepth..@maxDepth OUTBOUND @start GRAPH '" + gn + "' RETURN NOEVAL(p['edges'])",
        "FOR v, e, p IN @minDepth..@maxDepth OUTBOUND @start GRAPH '" + gn + "' RETURN p[NOEVAL('edges')]",
        "FOR v, e, p IN @minDepth..@maxDepth OUTBOUND @start GRAPH '" + gn + "' RETURN p[NOEVAL(CONCAT('ver', 'tices'))]",
        "FOR v, e, p IN @minDepth..@maxDepth OUTBOUND @start GRAPH '" + gn + "' RETURN p.edges",
        "FOR v, e, p IN @minDepth..@maxDepth OUTBOUND @start GRAPH '" + gn + "' RETURN p.`edges`",
        "FOR v, e, p IN @minDepth..@maxDepth OUTBOUND @start GRAPH '" + gn + "' RETURN p['edges']",
        "FOR v, e, p IN @minDepth..@maxDepth OUTBOUND @start GRAPH '" + gn + "' RETURN p.edges[*]",
        "FOR v, e, p IN @minDepth..@maxDepth OUTBOUND @start GRAPH '" + gn + "' RETURN p.edges[*].testi",
        "FOR v, e, p IN @minDepth..@maxDepth OUTBOUND @start GRAPH '" + gn + "' RETURN p.edges[0]",
        "FOR v, e, p IN @minDepth..@maxDepth OUTBOUND @start GRAPH '" + gn + "' RETURN p.edges[0].testi",
        "FOR v, e, p IN @minDepth..@maxDepth OUTBOUND @start GRAPH '" + gn + "' RETURN p.vertices",
        "FOR v, e, p IN @minDepth..@maxDepth OUTBOUND @start GRAPH '" + gn + "' RETURN p.`vertices`",
        "FOR v, e, p IN @minDepth..@maxDepth OUTBOUND @start GRAPH '" + gn + "' RETURN p['vertices']",
        "FOR v, e, p IN @minDepth..@maxDepth OUTBOUND @start GRAPH '" + gn + "' RETURN p.vertices[*]",
        "FOR v, e, p IN @minDepth..@maxDepth OUTBOUND @start GRAPH '" + gn + "' RETURN p.vertices[*].testi",
        "FOR v, e, p IN @minDepth..@maxDepth OUTBOUND @start GRAPH '" + gn + "' RETURN p.vertices[0]",
        "FOR v, e, p IN @minDepth..@maxDepth OUTBOUND @start GRAPH '" + gn + "' RETURN p.vertices[0].testi",
        "FOR v, e, p IN @minDepth..@maxDepth OUTBOUND @start GRAPH '" + gn + "' RETURN [p.edges, p.vertices]",
        "FOR v, e, p IN @minDepth..@maxDepth OUTBOUND @start GRAPH '" + gn + "' RETURN [p.`edges`, p.`vertices`]",
        "FOR v, e, p IN @minDepth..@maxDepth OUTBOUND @start GRAPH '" + gn + "' RETURN [p['edges'], p['vertices']]",
        "FOR v, e, p IN @minDepth..@maxDepth OUTBOUND @start GRAPH '" + gn + "' RETURN [p.edges, p]",
        "FOR v, e, p IN @minDepth..@maxDepth OUTBOUND @start GRAPH '" + gn + "' RETURN [p['edges'], p]",
        "FOR v, e, p IN @minDepth..@maxDepth OUTBOUND @start GRAPH '" + gn + "' RETURN [p.vertices, p]",
        "FOR v, e, p IN @minDepth..@maxDepth OUTBOUND @start GRAPH '" + gn + "' RETURN [p['vertices'], p]",
        "FOR v, e, p IN @minDepth..@maxDepth OUTBOUND @start GRAPH '" + gn + "' RETURN [p.edges, p.vertices, p]",
        "FOR v, e, p IN @minDepth..@maxDepth OUTBOUND @start GRAPH '" + gn + "' RETURN [p['edges'], p['vertices'], p]",
        "FOR v, e, p IN @minDepth..@maxDepth OUTBOUND @start GRAPH '" + gn + "' RETURN p.testi",
        "FOR v, e, p IN @minDepth..@maxDepth OUTBOUND @start GRAPH '" + gn + "' RETURN p['testi']",
        "FOR v, e, p IN @minDepth..@maxDepth OUTBOUND @start GRAPH '" + gn + "' COLLECT pl = LENGTH(p) RETURN pl",
        "FOR v, e, p IN @minDepth..@maxDepth OUTBOUND @start GRAPH '" + gn + "' COLLECT key = v._key INTO g RETURN { key, other: g[*].p.edges }",
        "FOR v, e, p IN @minDepth..@maxDepth OUTBOUND @start GRAPH '" + gn + "' COLLECT key = v._key INTO g RETURN { key, other: g[*].p['edges'] }",
        "FOR v, e, p IN @minDepth..@maxDepth OUTBOUND @start GRAPH '" + gn + "' FILTER LENGTH(p) > 0 RETURN v",
        "FOR v, e, p IN @minDepth..@maxDepth OUTBOUND @start GRAPH '" + gn + "' SORT p.edges RETURN v",
        "FOR v, e, p IN @minDepth..@maxDepth OUTBOUND @start GRAPH '" + gn + "' SORT p['edges'] RETURN v",
        "FOR v, e, p IN @minDepth..@maxDepth OUTBOUND @start GRAPH '" + gn + "' SORT p.vertices RETURN v",
        "FOR v, e, p IN @minDepth..@maxDepth OUTBOUND @start GRAPH '" + gn + "' SORT p['vertices'] RETURN v",
        "FOR v, e, p IN @minDepth..@maxDepth OUTBOUND @start GRAPH '" + gn + "' SORT p.edges, p.vertices RETURN v",
        "FOR v, e, p IN @minDepth..@maxDepth OUTBOUND @start GRAPH '" + gn + "' SORT p['edges'], p['vertices'] RETURN v",
        "FOR v, e, p IN @minDepth..@maxDepth OUTBOUND @start GRAPH '" + gn + "' SORT LENGTH(p.edges) RETURN v",
        "FOR v, e, p IN @minDepth..@maxDepth OUTBOUND @start GRAPH '" + gn + "' SORT LENGTH(p['edges']) RETURN v",
        "FOR v, e, p IN @minDepth..@maxDepth OUTBOUND @start GRAPH '" + gn + "' SORT LENGTH(p.vertices) RETURN v",
        "FOR v, e, p IN @minDepth..@maxDepth OUTBOUND @start GRAPH '" + gn + "' SORT LENGTH(p['vertices']) RETURN v",
       
        // PRUNE
        "FOR v, e, p IN @minDepth..@maxDepth OUTBOUND @start GRAPH '" + gn + "' PRUNE v.testi == 'abc' RETURN p",
        "FOR v, e, p IN @minDepth..@maxDepth OUTBOUND @start GRAPH '" + gn + "' PRUNE v['testi'] == 'abc' RETURN p.edges",
        "FOR v, e, p IN @minDepth..@maxDepth OUTBOUND @start GRAPH '" + gn + "' PRUNE pruneCondition = v.testi == 'abc' FILTER pruneCondition RETURN p",
        "FOR v, e, p IN @minDepth..@maxDepth OUTBOUND @start GRAPH '" + gn + "' PRUNE pruneCondition = v['testi'] == 'abc' FILTER pruneCondition RETURN p.edges",
        "FOR v, e, p IN @minDepth..@maxDepth OUTBOUND @start GRAPH '" + gn + "' PRUNE pruneCondition = v.testi == 'abc' OPTIONS {bfs: true} FILTER pruneCondition RETURN p",
        "FOR v, e, p IN @minDepth..@maxDepth OUTBOUND @start GRAPH '" + gn + "' PRUNE pruneCondition = v['testi'] == 'abc' OPTIONS {bfs: true} FILTER pruneCondition RETURN p.edges",
        "FOR v, e, p IN @minDepth..@maxDepth OUTBOUND @start GRAPH '" + gn + "' PRUNE v.testi == 'abc' RETURN p.edges",
        "FOR v, e, p IN @minDepth..@maxDepth OUTBOUND @start GRAPH '" + gn + "' PRUNE v['testi'] == 'abc' RETURN p.vertices",
        "FOR v, e, p IN @minDepth..@maxDepth OUTBOUND @start GRAPH '" + gn + "' PRUNE v.testi == 'abc' RETURN p.vertices",
        "FOR v, e, p IN @minDepth..@maxDepth OUTBOUND @start GRAPH '" + gn + "' PRUNE v['testi'] == 'abc' RETURN p.vertices",
        "FOR v, e, p IN @minDepth..@maxDepth OUTBOUND @start GRAPH '" + gn + "' PRUNE p.vertices[-1].testi == 'abc' RETURN p.vertices",
        "FOR v, e, p IN @minDepth..@maxDepth OUTBOUND @start GRAPH '" + gn + "' PRUNE p.vertices[-1].testi == 'abc' RETURN p['vertices']",
        "FOR v, e, p IN @minDepth..@maxDepth OUTBOUND @start GRAPH '" + gn + "' PRUNE p['vertices'][-1].testi == 'abc' RETURN p.vertices",
        "FOR v, e, p IN @minDepth..@maxDepth OUTBOUND @start GRAPH '" + gn + "' PRUNE p['vertices'][-1].testi == 'abc' RETURN p['vertices']",
        "FOR v, e, p IN @minDepth..@maxDepth OUTBOUND @start GRAPH '" + gn + "' PRUNE p.vertices[-1].testi == 'abc' RETURN p.edges",
        "FOR v, e, p IN @minDepth..@maxDepth OUTBOUND @start GRAPH '" + gn + "' PRUNE p.vertices[-1].testi == 'abc' RETURN p['edges']",
        "FOR v, e, p IN @minDepth..@maxDepth OUTBOUND @start GRAPH '" + gn + "' PRUNE p['vertices'][-1].testi == 'abc' RETURN p.edges",
        "FOR v, e, p IN @minDepth..@maxDepth OUTBOUND @start GRAPH '" + gn + "' PRUNE p['vertices'][-1].testi == 'abc' RETURN p['edges']",
        "FOR v, e, p IN @minDepth..@maxDepth OUTBOUND @start GRAPH '" + gn + "' PRUNE p.edges[-1].testi == 'abc' RETURN p.vertices",
        "FOR v, e, p IN @minDepth..@maxDepth OUTBOUND @start GRAPH '" + gn + "' PRUNE p.edges[-1].testi == 'abc' RETURN p.vertices",
        "FOR v, e, p IN @minDepth..@maxDepth OUTBOUND @start GRAPH '" + gn + "' PRUNE p['edges'][-1].testi == 'abc' RETURN p.vertices",
        "FOR v, e, p IN @minDepth..@maxDepth OUTBOUND @start GRAPH '" + gn + "' PRUNE p['edges'][-1].testi == 'abc' RETURN p['vertices']",
        "FOR v, e, p IN @minDepth..@maxDepth OUTBOUND @start GRAPH '" + gn + "' PRUNE p.edges[-1].testi == 'abc' RETURN p.edges",
        "FOR v, e, p IN @minDepth..@maxDepth OUTBOUND @start GRAPH '" + gn + "' PRUNE p.edges[-1].testi == 'abc' RETURN p.edges",
        "FOR v, e, p IN @minDepth..@maxDepth OUTBOUND @start GRAPH '" + gn + "' PRUNE p['edges'][-1].testi == 'abc' RETURN p.edges",
        "FOR v, e, p IN @minDepth..@maxDepth OUTBOUND @start GRAPH '" + gn + "' PRUNE p['edges'][-1].testi == 'abc' RETURN p['edges']",

        // weighted traversals
        "FOR v, e, p IN @minDepth..@maxDepth OUTBOUND @start GRAPH '" + gn + "' OPTIONS { order: 'weighted', weightAttribute: 'testi' } RETURN p",
        "FOR v, e, p IN @minDepth..@maxDepth OUTBOUND @start GRAPH '" + gn + "' OPTIONS { order: 'weighted', weightAttribute: 'testi' } RETURN p[0]",
        "FOR v, e, p IN @minDepth..@maxDepth OUTBOUND @start GRAPH '" + gn + "' OPTIONS { order: 'weighted', weightAttribute: 'testi' } RETURN p.weights",
        "FOR v, e, p IN @minDepth..@maxDepth OUTBOUND @start GRAPH '" + gn + "' OPTIONS { order: 'weighted', weightAttribute: 'testi' } RETURN p.`weights`",
        "FOR v, e, p IN @minDepth..@maxDepth OUTBOUND @start GRAPH '" + gn + "' OPTIONS { order: 'weighted', weightAttribute: 'testi' } RETURN p['weights']",
        "FOR v, e, p IN @minDepth..@maxDepth OUTBOUND @start GRAPH '" + gn + "' OPTIONS { order: 'weighted', weightAttribute: 'testi' } RETURN p.edges",
        "FOR v, e, p IN @minDepth..@maxDepth OUTBOUND @start GRAPH '" + gn + "' OPTIONS { order: 'weighted', weightAttribute: 'testi' } RETURN p['edges']",
        "FOR v, e, p IN @minDepth..@maxDepth OUTBOUND @start GRAPH '" + gn + "' OPTIONS { order: 'weighted', weightAttribute: 'testi' } RETURN p.vertices",
        "FOR v, e, p IN @minDepth..@maxDepth OUTBOUND @start GRAPH '" + gn + "' OPTIONS { order: 'weighted', weightAttribute: 'testi' } RETURN p['vertices']",
        "FOR v, e, p IN @minDepth..@maxDepth OUTBOUND @start GRAPH '" + gn + "' OPTIONS { order: 'weighted', weightAttribute: 'testi' } RETURN [p.edges, p.vertices]",
        "FOR v, e, p IN @minDepth..@maxDepth OUTBOUND @start GRAPH '" + gn + "' OPTIONS { order: 'weighted', weightAttribute: 'testi' } RETURN [p['edges'], p['vertices']]",
        "FOR v, e, p IN @minDepth..@maxDepth OUTBOUND @start GRAPH '" + gn + "' OPTIONS { order: 'weighted', weightAttribute: 'testi' } RETURN [p.edges, p.weights]",
        "FOR v, e, p IN @minDepth..@maxDepth OUTBOUND @start GRAPH '" + gn + "' OPTIONS { order: 'weighted', weightAttribute: 'testi' } RETURN [p['edges'], p['weights']]",
        "FOR v, e, p IN @minDepth..@maxDepth OUTBOUND @start GRAPH '" + gn + "' OPTIONS { order: 'weighted', weightAttribute: 'testi' } RETURN p.testi",
        "FOR v, e, p IN @minDepth..@maxDepth OUTBOUND @start GRAPH '" + gn + "' OPTIONS { order: 'weighted', weightAttribute: 'testi' } RETURN p['testi']",
        "FOR v, e, p IN @minDepth..@maxDepth OUTBOUND @start GRAPH '" + gn + "' OPTIONS { order: 'weighted', weightAttribute: 'testi' } RETURN p.weights[*]",
        "FOR v, e, p IN @minDepth..@maxDepth OUTBOUND @start GRAPH '" + gn + "' OPTIONS { order: 'weighted', weightAttribute: 'testi' } RETURN p.weights[*].testi",
        "FOR v, e, p IN @minDepth..@maxDepth OUTBOUND @start GRAPH '" + gn + "' OPTIONS { order: 'weighted', weightAttribute: 'testi' } RETURN p.weights[0]",
        "FOR v, e, p IN @minDepth..@maxDepth OUTBOUND @start GRAPH '" + gn + "' OPTIONS { order: 'weighted', weightAttribute: 'testi' } RETURN p.weights[0].testi",

        // traversal filters
        "FOR v, e, p IN @minDepth..@maxDepth OUTBOUND @start GRAPH '" + gn + "' OPTIONS { order: 'weighted', weightAttribute: 'testi' } FILTER p.edges[*].weight ALL >= 3 RETURN p.vertices",
        "FOR v, e, p IN @minDepth..@maxDepth OUTBOUND @start GRAPH '" + gn + "' OPTIONS { order: 'weighted', weightAttribute: 'testi' } FILTER p.vertices[1].weight >= 3 RETURN p.vertices",
        "FOR v, e, p IN @minDepth..@maxDepth OUTBOUND @start GRAPH '" + gn + "' OPTIONS { order: 'weighted', weightAttribute: 'testi' } FILTER p.edges[0].weight >= 3 RETURN p.vertices",
        "FOR v, e, p IN @minDepth..@maxDepth OUTBOUND @start GRAPH '" + gn + "' OPTIONS { order: 'weighted', weightAttribute: 'testi' } FILTER p.vertices[*].weight ALL >= 3 RETURN p.edges",
      
      ].forEach((query) => {
        [vn + "/test0", vn + "/testmann", vn + "/test99", vn + "/fjgghghghghghfhdjfkd"].forEach((start) => {
          [0, 1].forEach((minDepth) => {
            [0, 1, 2].forEach((maxDepth) => {
              let bind = { start, minDepth, maxDepth: minDepth + maxDepth };
              let nodes = AQL_EXPLAIN(query, bind).plan.nodes;
              let traversal = nodes.filter((node) => node.type === 'TraversalNode')[0];

              assertEqual("p", traversal.pathOutVariable.name);
              // execute the queries but don't check the results.
              // we execute the queries here to ensure that there are no runtime crashses (e.g.
              // nullptr accesses due to optimized-away path variables etc.)
              let result = db._query(query, bind).toArray();
              assertTrue(Array.isArray(result));
            });
          });
        });
      });
    },

  };
}

function NormalGraphSuite() {
  'use strict';
    
  const graphs = require("@arangodb/general-graph");

  let cleanup = function () {
    try {
      graphs._drop(gn, true);
    } catch (err) {}
    db._drop(vn);
    db._drop(en);
  };

  let suite = {
    setUpAll : function () {
      cleanup();
        
      db._create(vn, { numberOfShards: 4 });
      db._createEdgeCollection(en, { numberOfShards: 4 });

      graphs._create(gn, [graphs._relation(en, vn, vn)]);

      for (let i = 0; i < 100; ++i) {
        db[vn].insert({ _key: "test" + i });
      }
      for (let i = 0; i < 100; ++i) {
        db[en].insert({ _from: vn + "/test" + i, _to: vn + "/test" + ((i + 1) % 100) });
      }
      db[en].insert({ _from: vn + "/test0", _to: vn + "/testmann-does-not-exist" });
    },
    
    tearDownAll : function () {
      cleanup();
    },
  };
  
  deriveTestSuite(BaseTestConfig(), suite, '_NormalGraph');
  return suite;
}

function SmartGraphSuite() {
  'use strict';
    
  const graphs = require("@arangodb/smart-graph");

  let cleanup = function () {
    try {
      graphs._drop(gn, true);
    } catch (err) {}
    db._drop(vn);
    db._drop(en);
  };

  let suite = {
    setUpAll : function () {
      cleanup();
        
      graphs._create(gn, [graphs._relation(en, vn, vn)], null, { numberOfShards: 4, smartGraphAttribute: "testi" });

      for (let i = 0; i < 100; ++i) {
        db[vn].insert({ _key: "test" + (i % 10) + ":test" + i, testi: "test" + (i % 10) });
      }
      for (let i = 0; i < 100; ++i) {
        db[en].insert({ _from: vn + "/test" + i + ":test" + (i % 10), _to: vn + "/test" + ((i + 1) % 100) + ":test" + (i % 10), testi: (i % 10) });
      }
      db[en].insert({ _from: vn + "/test0:test0", _to: vn + "/testmann-does-not-exist:test0", testi: "test0" });
    },
    
    tearDownAll : function () {
      cleanup();
    },
  };
  
  deriveTestSuite(BaseTestConfig(), suite, '_SmartGraph');
  return suite;
}

function OneShardSuite() {
  'use strict';
    
  const graphs = require("@arangodb/general-graph");

  let cleanup = function () {
    try {
      graphs._drop(gn, true);
    } catch (err) {}
    db._drop(vn);
    db._drop(en);
  };

  let suite = {
    setUpAll : function () {
      cleanup();
      
      db._create(vn, { numberOfShards: 1 });
      db._createEdgeCollection(en, { distributeShardsLike: vn, numberOfShards: 1 });

      graphs._create(gn, [graphs._relation(en, vn, vn)]);

      for (let i = 0; i < 100; ++i) {
        db[vn].insert({ _key: "test" + i, testi: "test" + (i % 10) });
      }
      for (let i = 0; i < 100; ++i) {
        db[en].insert({ _from: vn + "/test" + i, _to: vn + "/test" + ((i + 1) % 100), testi: (i % 10) });
      }
      db[en].insert({ _from: vn + "/test0", _to: vn + "/testmann-does-not-exist" });
    },
    
    tearDownAll : function () {
      cleanup();
    },
  };
  
  deriveTestSuite(BaseTestConfig(), suite, '_OneShard');
  return suite;
}

jsunity.run(NormalGraphSuite);
if (isEnterprise && isCoordinator) {
  jsunity.run(SmartGraphSuite);
  jsunity.run(OneShardSuite);
}
return jsunity.done();
