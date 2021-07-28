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

    testProducePathsOutputs: function () {
      [
        [ "FOR v, e, p IN 1..1 OUTBOUND '" + vn + "/test0' GRAPH '" + gn + "' RETURN p", [ true, true, false ]], 
        [ "FOR v, e, p IN 1..1 OUTBOUND '" + vn + "/test0' GRAPH '" + gn + "' RETURN p[0]", [ true, true, false ]],
        [ "FOR v, e, p IN 1..1 OUTBOUND '" + vn + "/test0' GRAPH '" + gn + "' RETURN LENGTH(p)", [ true, true, false ]],
        [ "FOR v, e, p IN 1..1 OUTBOUND '" + vn + "/test0' GRAPH '" + gn + "' RETURN NOEVAL(p.edges)", [ false, true, false ]],
        [ "FOR v, e, p IN 1..1 OUTBOUND '" + vn + "/test0' GRAPH '" + gn + "' RETURN p[CONCAT('ver', 'tices')]", [ true, false, false ]],
        [ "FOR v, e, p IN 1..1 OUTBOUND '" + vn + "/test0' GRAPH '" + gn + "' RETURN NOEVAL(p['edges'])", [ false, true, false ]],
        [ "FOR v, e, p IN 1..1 OUTBOUND '" + vn + "/test0' GRAPH '" + gn + "' RETURN p[NOEVAL('edges')]", [ true, true, false ]],
        [ "FOR v, e, p IN 1..1 OUTBOUND '" + vn + "/test0' GRAPH '" + gn + "' RETURN p[NOEVAL(CONCAT('ver', 'tices'))]", [ true, true, false ]],
        [ "FOR v, e, p IN 1..1 OUTBOUND '" + vn + "/test0' GRAPH '" + gn + "' RETURN p.edges", [ false, true, false ]],
        [ "FOR v, e, p IN 1..1 OUTBOUND '" + vn + "/test0' GRAPH '" + gn + "' RETURN p.`edges`", [ false, true, false ]],
        [ "FOR v, e, p IN 1..1 OUTBOUND '" + vn + "/test0' GRAPH '" + gn + "' RETURN p['edges']", [ false, true, false ]],
        [ "FOR v, e, p IN 1..1 OUTBOUND '" + vn + "/test0' GRAPH '" + gn + "' RETURN p.edges[*]", [ false, true, false ]],
        [ "FOR v, e, p IN 1..1 OUTBOUND '" + vn + "/test0' GRAPH '" + gn + "' RETURN p.edges[*].testi", [ false, true, false ]],
        [ "FOR v, e, p IN 1..1 OUTBOUND '" + vn + "/test0' GRAPH '" + gn + "' RETURN p.edges[0]", [ false, true, false ]],
        [ "FOR v, e, p IN 1..1 OUTBOUND '" + vn + "/test0' GRAPH '" + gn + "' RETURN p.edges[0].testi", [ false, true, false ]],
        [ "FOR v, e, p IN 1..1 OUTBOUND '" + vn + "/test0' GRAPH '" + gn + "' RETURN p.vertices", [ true, false, false ]],
        [ "FOR v, e, p IN 1..1 OUTBOUND '" + vn + "/test0' GRAPH '" + gn + "' RETURN p.`vertices`", [ true,false, false ]],
        [ "FOR v, e, p IN 1..1 OUTBOUND '" + vn + "/test0' GRAPH '" + gn + "' RETURN p['vertices']", [ true, false, false ]],
        [ "FOR v, e, p IN 1..1 OUTBOUND '" + vn + "/test0' GRAPH '" + gn + "' RETURN p.vertices[*]", [ true, false, false ]],
        [ "FOR v, e, p IN 1..1 OUTBOUND '" + vn + "/test0' GRAPH '" + gn + "' RETURN p.vertices[*].testi", [ true, false, false ]],
        [ "FOR v, e, p IN 1..1 OUTBOUND '" + vn + "/test0' GRAPH '" + gn + "' RETURN p.vertices[0]", [ true, false, false ]],
        [ "FOR v, e, p IN 1..1 OUTBOUND '" + vn + "/test0' GRAPH '" + gn + "' RETURN p.vertices[0].testi", [ true, false, false ]],
        [ "FOR v, e, p IN 1..1 OUTBOUND '" + vn + "/test0' GRAPH '" + gn + "' RETURN [p.edges, p.vertices]", [ true, true, false ]],
        [ "FOR v, e, p IN 1..1 OUTBOUND '" + vn + "/test0' GRAPH '" + gn + "' RETURN [p.`edges`, p.`vertices`]", [ true, true, false ]],
        [ "FOR v, e, p IN 1..1 OUTBOUND '" + vn + "/test0' GRAPH '" + gn + "' RETURN [p['edges'], p['vertices']]", [ true, true, false ]],
        [ "FOR v, e, p IN 1..1 OUTBOUND '" + vn + "/test0' GRAPH '" + gn + "' RETURN [p.edges, p]", [ true, true, false ]],
        [ "FOR v, e, p IN 1..1 OUTBOUND '" + vn + "/test0' GRAPH '" + gn + "' RETURN [p['edges'], p]", [ true, true, false ]],
        [ "FOR v, e, p IN 1..1 OUTBOUND '" + vn + "/test0' GRAPH '" + gn + "' RETURN [p.vertices, p]", [ true, true, false ]],
        [ "FOR v, e, p IN 1..1 OUTBOUND '" + vn + "/test0' GRAPH '" + gn + "' RETURN [p['vertices'], p]", [ true, true, false ]],
        [ "FOR v, e, p IN 1..1 OUTBOUND '" + vn + "/test0' GRAPH '" + gn + "' RETURN [p.edges, p.vertices, p]", [ true, true, false ]],
        [ "FOR v, e, p IN 1..1 OUTBOUND '" + vn + "/test0' GRAPH '" + gn + "' RETURN [p['edges'], p['vertices'], p]", [ true, true, false ]],
        [ "FOR v, e, p IN 1..1 OUTBOUND '" + vn + "/test0' GRAPH '" + gn + "' RETURN p.testi", [ true, false, false ]],
        [ "FOR v, e, p IN 1..1 OUTBOUND '" + vn + "/test0' GRAPH '" + gn + "' RETURN p['testi']", [ true, false, false ]],
        [ "FOR v, e, p IN 1..1 OUTBOUND '" + vn + "/test0' GRAPH '" + gn + "' COLLECT pl = LENGTH(p) RETURN pl", [ true, true, false ]],
        [ "FOR v, e, p IN 1..1 OUTBOUND '" + vn + "/test0' GRAPH '" + gn + "' COLLECT key = v._key INTO g RETURN { key, other: g[*].p.edges }", [ true, true, false ]],
        [ "FOR v, e, p IN 1..1 OUTBOUND '" + vn + "/test0' GRAPH '" + gn + "' COLLECT key = v._key INTO g RETURN { key, other: g[*].p['edges'] }", [ true, true, false ]],
        [ "FOR v, e, p IN 1..1 OUTBOUND '" + vn + "/test0' GRAPH '" + gn + "' FILTER LENGTH(p) > 0 RETURN v", [ true, true, false ]],
        [ "FOR v, e, p IN 1..1 OUTBOUND '" + vn + "/test0' GRAPH '" + gn + "' SORT p.edges RETURN v", [ false, true, false ]],
        [ "FOR v, e, p IN 1..1 OUTBOUND '" + vn + "/test0' GRAPH '" + gn + "' SORT p['edges'] RETURN v", [ false, true, false ]],
        [ "FOR v, e, p IN 1..1 OUTBOUND '" + vn + "/test0' GRAPH '" + gn + "' SORT p.vertices RETURN v", [ true, false, false ]],
        [ "FOR v, e, p IN 1..1 OUTBOUND '" + vn + "/test0' GRAPH '" + gn + "' SORT p['vertices'] RETURN v", [ true, false, false ]],
        [ "FOR v, e, p IN 1..1 OUTBOUND '" + vn + "/test0' GRAPH '" + gn + "' SORT p.edges, p.vertices RETURN v", [ true, true, false ]],
        [ "FOR v, e, p IN 1..1 OUTBOUND '" + vn + "/test0' GRAPH '" + gn + "' SORT p['edges'], p['vertices'] RETURN v", [ true, true, false ]],
        [ "FOR v, e, p IN 1..1 OUTBOUND '" + vn + "/test0' GRAPH '" + gn + "' SORT LENGTH(p.edges) RETURN v", [ false, true, false ]],
        [ "FOR v, e, p IN 1..1 OUTBOUND '" + vn + "/test0' GRAPH '" + gn + "' SORT LENGTH(p['edges']) RETURN v", [ false, true, false ]],
        [ "FOR v, e, p IN 1..1 OUTBOUND '" + vn + "/test0' GRAPH '" + gn + "' SORT LENGTH(p.vertices) RETURN v", [ true, false, false ]],
        [ "FOR v, e, p IN 1..1 OUTBOUND '" + vn + "/test0' GRAPH '" + gn + "' SORT LENGTH(p['vertices']) RETURN v", [ true, false, false ]],
       
        // PRUNE
        [ "FOR v, e, p IN 1..1 OUTBOUND '" + vn + "/test0' GRAPH '" + gn + "' PRUNE v.testi == 'abc' RETURN p", [ true, true, false ]],
        [ "FOR v, e, p IN 1..1 OUTBOUND '" + vn + "/test0' GRAPH '" + gn + "' PRUNE v['testi'] == 'abc' RETURN p.edges", [ false, true, false ]],
        [ "FOR v, e, p IN 1..1 OUTBOUND '" + vn + "/test0' GRAPH '" + gn + "' PRUNE v.testi == 'abc' RETURN p.edges", [ false, true, false ]],
        [ "FOR v, e, p IN 1..1 OUTBOUND '" + vn + "/test0' GRAPH '" + gn + "' PRUNE v['testi'] == 'abc' RETURN p.vertices", [ true, false, false ]],
        [ "FOR v, e, p IN 1..1 OUTBOUND '" + vn + "/test0' GRAPH '" + gn + "' PRUNE v.testi == 'abc' RETURN p.vertices", [ true, false, false ]],
        [ "FOR v, e, p IN 1..1 OUTBOUND '" + vn + "/test0' GRAPH '" + gn + "' PRUNE v['testi'] == 'abc' RETURN p.vertices", [ true, false, false ]],
        [ "FOR v, e, p IN 1..1 OUTBOUND '" + vn + "/test0' GRAPH '" + gn + "' PRUNE p.vertices[-1].testi == 'abc' RETURN p.vertices", [ true, false, false ]],
        [ "FOR v, e, p IN 1..1 OUTBOUND '" + vn + "/test0' GRAPH '" + gn + "' PRUNE p.vertices[-1].testi == 'abc' RETURN p['vertices']", [ true, false, false ]],
        [ "FOR v, e, p IN 1..1 OUTBOUND '" + vn + "/test0' GRAPH '" + gn + "' PRUNE p['vertices'][-1].testi == 'abc' RETURN p.vertices", [ true, false, false ]],
        [ "FOR v, e, p IN 1..1 OUTBOUND '" + vn + "/test0' GRAPH '" + gn + "' PRUNE p['vertices'][-1].testi == 'abc' RETURN p['vertices']", [ true, false, false ]],
        [ "FOR v, e, p IN 1..1 OUTBOUND '" + vn + "/test0' GRAPH '" + gn + "' PRUNE p.vertices[-1].testi == 'abc' RETURN p.edges", [ false, true, false ]],
        [ "FOR v, e, p IN 1..1 OUTBOUND '" + vn + "/test0' GRAPH '" + gn + "' PRUNE p.vertices[-1].testi == 'abc' RETURN p['edges']", [ false, true, false ]],
        [ "FOR v, e, p IN 1..1 OUTBOUND '" + vn + "/test0' GRAPH '" + gn + "' PRUNE p['vertices'][-1].testi == 'abc' RETURN p.edges", [ false, true, false ]],
        [ "FOR v, e, p IN 1..1 OUTBOUND '" + vn + "/test0' GRAPH '" + gn + "' PRUNE p['vertices'][-1].testi == 'abc' RETURN p['edges']", [ false, true, false ]],
        [ "FOR v, e, p IN 1..1 OUTBOUND '" + vn + "/test0' GRAPH '" + gn + "' PRUNE p.edges[-1].testi == 'abc' RETURN p.vertices", [ true, false, false ]],
        [ "FOR v, e, p IN 1..1 OUTBOUND '" + vn + "/test0' GRAPH '" + gn + "' PRUNE p.edges[-1].testi == 'abc' RETURN p.vertices", [ true, false, false ]],
        [ "FOR v, e, p IN 1..1 OUTBOUND '" + vn + "/test0' GRAPH '" + gn + "' PRUNE p['edges'][-1].testi == 'abc' RETURN p.vertices", [ true, false, false ]],
        [ "FOR v, e, p IN 1..1 OUTBOUND '" + vn + "/test0' GRAPH '" + gn + "' PRUNE p['edges'][-1].testi == 'abc' RETURN p['vertices']", [ true, false, false ]],
        [ "FOR v, e, p IN 1..1 OUTBOUND '" + vn + "/test0' GRAPH '" + gn + "' PRUNE p.edges[-1].testi == 'abc' RETURN p.edges", [ false, true, false ]],
        [ "FOR v, e, p IN 1..1 OUTBOUND '" + vn + "/test0' GRAPH '" + gn + "' PRUNE p.edges[-1].testi == 'abc' RETURN p.edges", [ false, true, false ]],
        [ "FOR v, e, p IN 1..1 OUTBOUND '" + vn + "/test0' GRAPH '" + gn + "' PRUNE p['edges'][-1].testi == 'abc' RETURN p.edges", [ false, true, false ]],
        [ "FOR v, e, p IN 1..1 OUTBOUND '" + vn + "/test0' GRAPH '" + gn + "' PRUNE p['edges'][-1].testi == 'abc' RETURN p['edges']", [ false, true, false ]],

        // weighted traversals
        [ "FOR v, e, p IN 1..1 OUTBOUND '" + vn + "/test0' GRAPH '" + gn + "' OPTIONS { order: 'weighted', weightAttribute: 'testi' } RETURN p", [ true, true, true ]],
        [ "FOR v, e, p IN 1..1 OUTBOUND '" + vn + "/test0' GRAPH '" + gn + "' OPTIONS { order: 'weighted', weightAttribute: 'testi' } RETURN p[0]", [ true, true, true ]],
        [ "FOR v, e, p IN 1..1 OUTBOUND '" + vn + "/test0' GRAPH '" + gn + "' OPTIONS { order: 'weighted', weightAttribute: 'testi' } RETURN p.weights", [ false, false, true ]],
        [ "FOR v, e, p IN 1..1 OUTBOUND '" + vn + "/test0' GRAPH '" + gn + "' OPTIONS { order: 'weighted', weightAttribute: 'testi' } RETURN p.`weights`", [ false, false, true ]],
        [ "FOR v, e, p IN 1..1 OUTBOUND '" + vn + "/test0' GRAPH '" + gn + "' OPTIONS { order: 'weighted', weightAttribute: 'testi' } RETURN p['weights']", [ false, false, true ]],
        [ "FOR v, e, p IN 1..1 OUTBOUND '" + vn + "/test0' GRAPH '" + gn + "' OPTIONS { order: 'weighted', weightAttribute: 'testi' } RETURN p.edges", [ false, true, false ]],
        [ "FOR v, e, p IN 1..1 OUTBOUND '" + vn + "/test0' GRAPH '" + gn + "' OPTIONS { order: 'weighted', weightAttribute: 'testi' } RETURN p['edges']", [ false, true, false ]],
        [ "FOR v, e, p IN 1..1 OUTBOUND '" + vn + "/test0' GRAPH '" + gn + "' OPTIONS { order: 'weighted', weightAttribute: 'testi' } RETURN p.vertices", [ true, false, false ]],
        [ "FOR v, e, p IN 1..1 OUTBOUND '" + vn + "/test0' GRAPH '" + gn + "' OPTIONS { order: 'weighted', weightAttribute: 'testi' } RETURN p['vertices']", [ true, false, false ]],
        [ "FOR v, e, p IN 1..1 OUTBOUND '" + vn + "/test0' GRAPH '" + gn + "' OPTIONS { order: 'weighted', weightAttribute: 'testi' } RETURN [p.edges, p.vertices]", [ true, true, false ]],
        [ "FOR v, e, p IN 1..1 OUTBOUND '" + vn + "/test0' GRAPH '" + gn + "' OPTIONS { order: 'weighted', weightAttribute: 'testi' } RETURN [p['edges'], p['vertices']]", [ true, true, false ]],
        [ "FOR v, e, p IN 1..1 OUTBOUND '" + vn + "/test0' GRAPH '" + gn + "' OPTIONS { order: 'weighted', weightAttribute: 'testi' } RETURN [p.edges, p.weights]", [ false, true, true ]],
        [ "FOR v, e, p IN 1..1 OUTBOUND '" + vn + "/test0' GRAPH '" + gn + "' OPTIONS { order: 'weighted', weightAttribute: 'testi' } RETURN [p['edges'], p['weights']]", [ false, true, true ]],
        [ "FOR v, e, p IN 1..1 OUTBOUND '" + vn + "/test0' GRAPH '" + gn + "' OPTIONS { order: 'weighted', weightAttribute: 'testi' } RETURN p.testi", [ true, false, false ]],
        [ "FOR v, e, p IN 1..1 OUTBOUND '" + vn + "/test0' GRAPH '" + gn + "' OPTIONS { order: 'weighted', weightAttribute: 'testi' } RETURN p['testi']", [ true, false, false ]],
        [ "FOR v, e, p IN 1..1 OUTBOUND '" + vn + "/test0' GRAPH '" + gn + "' OPTIONS { order: 'weighted', weightAttribute: 'testi' } RETURN p.weights[*]", [ false, false, true ]],
        [ "FOR v, e, p IN 1..1 OUTBOUND '" + vn + "/test0' GRAPH '" + gn + "' OPTIONS { order: 'weighted', weightAttribute: 'testi' } RETURN p.weights[*].testi", [ false, false, true ]],
        [ "FOR v, e, p IN 1..1 OUTBOUND '" + vn + "/test0' GRAPH '" + gn + "' OPTIONS { order: 'weighted', weightAttribute: 'testi' } RETURN p.weights[0]", [ false, false, true ]],
        [ "FOR v, e, p IN 1..1 OUTBOUND '" + vn + "/test0' GRAPH '" + gn + "' OPTIONS { order: 'weighted', weightAttribute: 'testi' } RETURN p.weights[0].testi", [ false, false, true ]],

        // traversal filters
        [ "FOR v, e, p IN 1..1 OUTBOUND '" + vn + "/test0' GRAPH '" + gn + "' OPTIONS { order: 'weighted', weightAttribute: 'testi' } FILTER p.edges[*].weight ALL >= 3 RETURN p.vertices", [ true, false, false ]],
        [ "FOR v, e, p IN 1..1 OUTBOUND '" + vn + "/test0' GRAPH '" + gn + "' OPTIONS { order: 'weighted', weightAttribute: 'testi' } FILTER p.vertices[1].weight >= 3 RETURN p.vertices", [ true, false, false ]],
        [ "FOR v, e, p IN 1..1 OUTBOUND '" + vn + "/test0' GRAPH '" + gn + "' OPTIONS { order: 'weighted', weightAttribute: 'testi' } FILTER p.edges[0].weight >= 3 RETURN p.vertices", [ true, false, false ]],
        [ "FOR v, e, p IN 1..1 OUTBOUND '" + vn + "/test0' GRAPH '" + gn + "' OPTIONS { order: 'weighted', weightAttribute: 'testi' } FILTER p.vertices[*].weight ALL >= 3 RETURN p.edges", [ false, true, false ]],
      
      ].forEach((query) => {
        let nodes = AQL_EXPLAIN(query[0]).plan.nodes;
        let traversal = nodes.filter((node) => node.type === 'TraversalNode')[0];

        assertEqual("p", traversal.pathOutVariable.name);
        assertEqual(query[1][0], traversal.options.producePathsVertices, query);
        assertEqual(query[1][1], traversal.options.producePathsEdges, query);
        assertEqual(query[1][2], traversal.options.producePathsWeights, query);
       
        // execute the queries but don't check the results yet.
        // we execute them to ensure that there are no runtime crashses (e.g.
        // nullptr accesses due to optimized-away path variables etc.)
        let result = db._query(query[0]).toArray();
        assertTrue(Array.isArray(result));
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
