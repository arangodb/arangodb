/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertNotEqual, assertNull, assertTrue, assertFalse, AQL_EXPLAIN, AQL_EXECUTE */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for optimizer rules
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2012 triagens GmbH, Cologne, Germany
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
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const db = require("@arangodb").db;
const graph = require("@arangodb/general-graph");
const cn = "UnitTestsCollection";
const isEnterprise = require("internal").isEnterprise();
const isCoordinator = require('@arangodb/cluster').isCoordinator();

function collectionTestSuite () {
  'use strict';
  return {
    setUpAll : function () {
      db._drop(cn + "v1");
      db._drop(cn + "e1");
     
      db._create(cn + "v1", { numberOfShards: 4 });
      db._createEdgeCollection(cn + "e1", { numberOfShards: 4 });

      for (let i = 0; i < 100; ++i) {
        db[cn + "v1"].insert({ _key: "test" + i });
      }

      for (let i = 0; i < 100; ++i) {
        db[cn + "e1"].insert({ _from: cn + "v1/test" + i, _to: cn + "v1/test" + ((i + 1) % 100) });
      }
    },

    tearDownAll : function () {
      db._drop(cn + "v1");
      db._drop(cn + "e1");
    },

    testCollectionRuleDisabled : function () {
      const queries = [ 
        "FOR v, e, p IN 1..3 OUTBOUND '" + cn + "v1/test1' " + cn + "e1 RETURN e",
        "FOR v, e, p IN 1..3 OUTBOUND '" + cn + "v1/test1' " + cn + "e1 RETURN v",
        "FOR v, e, p IN 1..3 OUTBOUND '" + cn + "v1/test1' " + cn + "e1 RETURN p",
        "FOR v, e, p IN 1..3 OUTBOUND '" + cn + "v1/test1' " + cn + "e1 RETURN 1",
      ];

      queries.forEach(function(query) {
        // expect that optimization does not kick in when we turn off the optimizer rule
        let result = AQL_EXPLAIN(query, {}, {optimizer: { rules: ["-optimize-traversals"] } });
        let traversal = result.plan.nodes.filter(function(n) { return n.type === 'TraversalNode'; })[0];
        assertTrue(traversal.produceVertices, query);
      });
    },
    
    testCollectionRuleNoEffect : function () {
      const queries = [ 
        "FOR v, e, p IN 1..3 OUTBOUND '" + cn + "v1/test1' " + cn + "e1 RETURN v",
        "FOR v, e, p IN 1..3 OUTBOUND '" + cn + "v1/test1' " + cn + "e1 RETURN [v, e]",
        "FOR v, e, p IN 1..3 OUTBOUND '" + cn + "v1/test1' " + cn + "e1 RETURN p",
        "FOR v, e, p IN 1..3 OUTBOUND '" + cn + "v1/test1' " + cn + "e1 FILTER v._key == '123' RETURN e",
        "FOR v, e, p IN 1..3 OUTBOUND '" + cn + "v1/test1' " + cn + "e1 FILTER p.vertices[0]._key == '123' RETURN e",
        "FOR v, e, p IN 1..3 OUTBOUND '" + cn + "v1/test1' " + cn + "e1 FILTER p.vertices[*]._key ALL == '123' RETURN e",
        "FOR v, e, p IN 1..3 OUTBOUND '" + cn + "v1/test1' " + cn + "e1 FILTER p.edges[0]._key == '123' RETURN e",
        "FOR v, e, p IN 1..3 OUTBOUND '" + cn + "v1/test1' " + cn + "e1 FILTER p.edges[*]._key ALL == '123' RETURN e",
      ];

      queries.forEach(function(query) {
        // expect that optimization does not kick in because it is not applicable
        let result = AQL_EXPLAIN(query, {});
        let traversal = result.plan.nodes.filter(function(n) { return n.type === 'TraversalNode'; })[0];
        assertTrue(traversal.produceVertices, query);
      });
    },

    testCollectionRuleHasEffect : function () {
      const queries = [ 
        "FOR v, e, p IN 1..3 OUTBOUND '" + cn + "v1/test1' " + cn + "e1 RETURN e",
        "FOR v, e, p IN 1..3 OUTBOUND '" + cn + "v1/test1' " + cn + "e1 RETURN 1",
        "FOR v, e, p IN 1..3 OUTBOUND '" + cn + "v1/test1' " + cn + "e1 RETURN e._key",
        "FOR v, e, p IN 1..3 OUTBOUND '" + cn + "v1/test1' " + cn + "e1 FILTER e._key == '123' RETURN e",
      ];

      queries.forEach(function(query) {
        // expect that optimization kicks in
        let result = AQL_EXPLAIN(query, {});
        let traversal = result.plan.nodes.filter(function(n) { return n.type === 'TraversalNode'; })[0];
        assertFalse(traversal.produceVertices, query);
      });
    },
    
    testCollectionResults : function () {
      const query = "FOR v, e, p IN 1..3 OUTBOUND '" + cn + "v1/test0' " + cn + "e1 RETURN e";
      // expect that optimization kicks in
      let result = AQL_EXPLAIN(query, {});
      let traversal = result.plan.nodes.filter(function(n) { return n.type === 'TraversalNode'; })[0];
      assertFalse(traversal.produceVertices, query);
      
      result = AQL_EXECUTE(query).json;
      assertEqual(3, result.length);
      assertEqual(cn + "v1/test0", result[0]._from);
      assertEqual(cn + "v1/test1", result[0]._to);
      
      assertEqual(cn + "v1/test1", result[1]._from);
      assertEqual(cn + "v1/test2", result[1]._to);
      
      assertEqual(cn + "v1/test2", result[2]._from);
      assertEqual(cn + "v1/test3", result[2]._to);
    },
    
    testCollectionResultsLevel0 : function () {
      const query = "FOR v, e, p IN 0..3 OUTBOUND '" + cn + "v1/test0' " + cn + "e1 RETURN e";
      // expect that optimization kicks in
      let result = AQL_EXPLAIN(query, {});
      let traversal = result.plan.nodes.filter(function(n) { return n.type === 'TraversalNode'; })[0];
      assertFalse(traversal.produceVertices, query);
      
      result = AQL_EXECUTE(query).json;
      assertEqual(4, result.length);
      assertNull(result[0]);
      
      assertEqual(cn + "v1/test0", result[1]._from);
      assertEqual(cn + "v1/test1", result[1]._to);
      
      assertEqual(cn + "v1/test1", result[2]._from);
      assertEqual(cn + "v1/test2", result[2]._to);
      
      assertEqual(cn + "v1/test2", result[3]._from);
      assertEqual(cn + "v1/test3", result[3]._to);
    },
  
  };
}

function namedGraphTestSuite () {
  'use strict';
  return {
    setUpAll : function () {
      db._drop(cn + "v1");
      db._drop(cn + "e1");
      
      try {
        graph._drop(cn, true);
      } catch (ignore) {
      }
     
      const edgeDef = [graph._relation(cn + "e1", cn + "v1", cn + "v1")];
      graph._create(cn, edgeDef, null, { numberOfShards: 4 });
      
      for (let i = 0; i < 100; ++i) {
        db[cn + "v1"].insert({ _key: "test" + i });
      }

      for (let i = 0; i < 100; ++i) {
        db[cn + "e1"].insert({ _from: cn + "v1/test" + i, _to: cn + "v1/test" + ((i + 1) % 100) });
      }
    },

    tearDownAll : function () {
      try {
        graph._drop(cn, true);
      } catch (ignore) {
      }
      db._drop(cn + "v1");
      db._drop(cn + "e1");
    },

    testNamedGraphRuleDisabled : function () {
      const queries = [ 
        "FOR v, e, p IN 1..3 OUTBOUND '" + cn + "v1/test1' GRAPH '" + cn + "' RETURN e",
        "FOR v, e, p IN 1..3 OUTBOUND '" + cn + "v1/test1' GRAPH '" + cn + "' RETURN v",
        "FOR v, e, p IN 1..3 OUTBOUND '" + cn + "v1/test1' GRAPH '" + cn + "' RETURN p",
        "FOR v, e, p IN 1..3 OUTBOUND '" + cn + "v1/test1' GRAPH '" + cn + "' RETURN 1",
      ];

      queries.forEach(function(query) {
        // expect that optimization does not kick in when we turn off the optimizer rule
        let result = AQL_EXPLAIN(query, {}, {optimizer: { rules: ["-optimize-traversals"] } });
        let traversal = result.plan.nodes.filter(function(n) { return n.type === 'TraversalNode'; })[0];
        assertTrue(traversal.produceVertices, query);
      });
    },
    
    testNamedGraphRuleNoEffect : function () {
      const queries = [ 
        "FOR v, e, p IN 1..3 OUTBOUND '" + cn + "v1/test1' GRAPH '" + cn + "' RETURN v",
        "FOR v, e, p IN 1..3 OUTBOUND '" + cn + "v1/test1' GRAPH '" + cn + "' RETURN [v, e]",
        "FOR v, e, p IN 1..3 OUTBOUND '" + cn + "v1/test1' GRAPH '" + cn + "' RETURN p",
        "FOR v, e, p IN 1..3 OUTBOUND '" + cn + "v1/test1' GRAPH '" + cn + "' FILTER v._key == '123' RETURN e",
        "FOR v, e, p IN 1..3 OUTBOUND '" + cn + "v1/test1' GRAPH '" + cn + "' FILTER p.vertices[0]._key == '123' RETURN e",
        "FOR v, e, p IN 1..3 OUTBOUND '" + cn + "v1/test1' GRAPH '" + cn + "' FILTER p.vertices[*]._key ALL == '123' RETURN e",
        "FOR v, e, p IN 1..3 OUTBOUND '" + cn + "v1/test1' GRAPH '" + cn + "' FILTER p.edges[0]._key == '123' RETURN e",
        "FOR v, e, p IN 1..3 OUTBOUND '" + cn + "v1/test1' GRAPH '" + cn + "' FILTER p.edges[*]._key ALL == '123' RETURN e",
      ];

      queries.forEach(function(query) {
        // expect that optimization does not kick in because it is not applicable
        let result = AQL_EXPLAIN(query, {});
        let traversal = result.plan.nodes.filter(function(n) { return n.type === 'TraversalNode'; })[0];
        assertTrue(traversal.produceVertices, query);
      });
    },

    testNamedGraphRuleHasEffect : function () {
      const queries = [ 
        "FOR v, e, p IN 1..3 OUTBOUND '" + cn + "v1/test1' GRAPH '" + cn + "' RETURN e",
        "FOR v, e, p IN 1..3 OUTBOUND '" + cn + "v1/test1' GRAPH '" + cn + "' RETURN 1",
        "FOR v, e, p IN 1..3 OUTBOUND '" + cn + "v1/test1' GRAPH '" + cn + "' RETURN e._key",
        "FOR v, e, p IN 1..3 OUTBOUND '" + cn + "v1/test1' GRAPH '" + cn + "' FILTER e._key == '123' RETURN e",
      ];

      queries.forEach(function(query) {
        // expect that optimization kicks in
        let result = AQL_EXPLAIN(query, {});
        let traversal = result.plan.nodes.filter(function(n) { return n.type === 'TraversalNode'; })[0];
        assertFalse(traversal.produceVertices, query);
      });
    },
    
    testNamedGraphResults : function () {
      const query = "FOR v, e, p IN 1..3 OUTBOUND '" + cn + "v1/test0' GRAPH '" + cn + "' RETURN e";
      // expect that optimization kicks in
      let result = AQL_EXPLAIN(query, {});
      let traversal = result.plan.nodes.filter(function(n) { return n.type === 'TraversalNode'; })[0];
      assertFalse(traversal.produceVertices, query);
      
      result = AQL_EXECUTE(query).json;
      assertEqual(3, result.length);
      assertEqual(cn + "v1/test0", result[0]._from);
      assertEqual(cn + "v1/test1", result[0]._to);
      
      assertEqual(cn + "v1/test1", result[1]._from);
      assertEqual(cn + "v1/test2", result[1]._to);
      
      assertEqual(cn + "v1/test2", result[2]._from);
      assertEqual(cn + "v1/test3", result[2]._to);
    },
    
    testNamedGraphResultsLevel0 : function () {
      const query = "FOR v, e, p IN 0..3 OUTBOUND '" + cn + "v1/test0' GRAPH '" + cn + "' RETURN e";
      // expect that optimization kicks in
      let result = AQL_EXPLAIN(query, {});
      let traversal = result.plan.nodes.filter(function(n) { return n.type === 'TraversalNode'; })[0];
      assertFalse(traversal.produceVertices, query);
      
      result = AQL_EXECUTE(query).json;
      assertEqual(4, result.length);
      assertNull(result[0]);
      
      assertEqual(cn + "v1/test0", result[1]._from);
      assertEqual(cn + "v1/test1", result[1]._to);
      
      assertEqual(cn + "v1/test1", result[2]._from);
      assertEqual(cn + "v1/test2", result[2]._to);
      
      assertEqual(cn + "v1/test2", result[3]._from);
      assertEqual(cn + "v1/test3", result[3]._to);
    },
  
  };
}

function smartGraphTestSuite () {
  'use strict';
  
  let graph = require("@arangodb/smart-graph");
  let keys = [];

  return {
    setUpAll : function () {
      db._drop(cn + "v1");
      db._drop(cn + "e1");
      
      try {
        graph._drop(cn, true);
      } catch (ignore) {
      }
     
      const edgeDef = [graph._relation(cn + "e1", cn + "v1", cn + "v1")];
      graph._create(cn, edgeDef, null, { numberOfShards: 4, smartGraphAttribute: "aha" });
      
      for (let i = 0; i < 100; ++i) {
        keys.push(db[cn + "v1"].insert({ "aha" : "test" + i })._key);
      }

      for (let i = 0; i < 100; ++i) {
        db[cn + "e1"].insert({ _from: cn + "v1/" + keys[i], _to: cn + "v1/" + keys[(i + 1) % 100] });
      }
    },

    tearDownAll : function () {
      try {
        graph._drop(cn, true);
      } catch (ignore) {
      }
      db._drop(cn + "v1");
      db._drop(cn + "e1");
    },

    testSmartGraphRuleDisabled : function () {
      const queries = [ 
        "FOR v, e, p IN 1..3 OUTBOUND '" + cn + "v1/test1' GRAPH '" + cn + "' RETURN e",
        "FOR v, e, p IN 1..3 OUTBOUND '" + cn + "v1/test1' GRAPH '" + cn + "' RETURN v",
        "FOR v, e, p IN 1..3 OUTBOUND '" + cn + "v1/test1' GRAPH '" + cn + "' RETURN p",
        "FOR v, e, p IN 1..3 OUTBOUND '" + cn + "v1/test1' GRAPH '" + cn + "' RETURN 1",
      ];

      queries.forEach(function(query) {
        // expect that optimization does not kick in when we turn off the optimizer rule
        let result = AQL_EXPLAIN(query, {}, {optimizer: { rules: ["-optimize-traversals"] } });
        let traversal = result.plan.nodes.filter(function(n) { return n.type === 'TraversalNode'; })[0];
        assertTrue(traversal.produceVertices, query);
      });
    },
    
    testSmartGraphRuleNoEffect : function () {
      const queries = [ 
        "FOR v, e, p IN 1..3 OUTBOUND '" + cn + "v1/test1' GRAPH '" + cn + "' RETURN v",
        "FOR v, e, p IN 1..3 OUTBOUND '" + cn + "v1/test1' GRAPH '" + cn + "' RETURN [v, e]",
        "FOR v, e, p IN 1..3 OUTBOUND '" + cn + "v1/test1' GRAPH '" + cn + "' RETURN p",
        "FOR v, e, p IN 1..3 OUTBOUND '" + cn + "v1/test1' GRAPH '" + cn + "' FILTER v._key == '123' RETURN e",
        "FOR v, e, p IN 1..3 OUTBOUND '" + cn + "v1/test1' GRAPH '" + cn + "' FILTER p.vertices[0]._key == '123' RETURN e",
        "FOR v, e, p IN 1..3 OUTBOUND '" + cn + "v1/test1' GRAPH '" + cn + "' FILTER p.vertices[*]._key ALL == '123' RETURN e",
        "FOR v, e, p IN 1..3 OUTBOUND '" + cn + "v1/test1' GRAPH '" + cn + "' FILTER p.edges[0]._key == '123' RETURN e",
        "FOR v, e, p IN 1..3 OUTBOUND '" + cn + "v1/test1' GRAPH '" + cn + "' FILTER p.edges[*]._key ALL == '123' RETURN e",
      ];

      queries.forEach(function(query) {
        // expect that optimization does not kick in because it is not applicable
        let result = AQL_EXPLAIN(query, {});
        let traversal = result.plan.nodes.filter(function(n) { return n.type === 'TraversalNode'; })[0];
        assertTrue(traversal.produceVertices, query);
      });
    },

    testSmartGraphRuleHasEffect : function () {
      const queries = [ 
        "FOR v, e, p IN 1..3 OUTBOUND '" + cn + "v1/test1' GRAPH '" + cn + "' RETURN e",
        "FOR v, e, p IN 1..3 OUTBOUND '" + cn + "v1/test1' GRAPH '" + cn + "' RETURN 1",
        "FOR v, e, p IN 1..3 OUTBOUND '" + cn + "v1/test1' GRAPH '" + cn + "' RETURN e._key",
        "FOR v, e, p IN 1..3 OUTBOUND '" + cn + "v1/test1' GRAPH '" + cn + "' FILTER e._key == '123' RETURN e",
      ];

      queries.forEach(function(query) {
        // expect that optimization kicks in
        let result = AQL_EXPLAIN(query, {});
        let traversal = result.plan.nodes.filter(function(n) { return n.type === 'TraversalNode'; })[0];
        assertFalse(traversal.produceVertices, query);
      });
    },
    
    testSmartGraphResults : function () {
      const query = "FOR v, e, p IN 1..3 OUTBOUND '" + cn + "v1/" + keys[0] + "' GRAPH '" + cn + "' RETURN e";
      // expect that optimization kicks in
      let result = AQL_EXPLAIN(query, {});
      let traversal = result.plan.nodes.filter(function(n) { return n.type === 'TraversalNode'; })[0];
      assertFalse(traversal.produceVertices, query);
      
      result = AQL_EXECUTE(query).json;
      assertEqual(3, result.length);
      assertEqual(cn + "v1/" + keys[0], result[0]._from);
      assertEqual(cn + "v1/" + keys[1], result[0]._to);
      
      assertEqual(cn + "v1/" + keys[1], result[1]._from);
      assertEqual(cn + "v1/" + keys[2], result[1]._to);
      
      assertEqual(cn + "v1/" + keys[2], result[2]._from);
      assertEqual(cn + "v1/" + keys[3], result[2]._to);
    },
    
    testSmartGraphResultsLevel0 : function () {
      const query = "FOR v, e, p IN 0..3 OUTBOUND '" + cn + "v1/" + keys[0] + "' GRAPH '" + cn + "' RETURN e";
      // expect that optimization kicks in
      let result = AQL_EXPLAIN(query, {});
      let traversal = result.plan.nodes.filter(function(n) { return n.type === 'TraversalNode'; })[0];
      assertFalse(traversal.produceVertices, query);
      
      result = AQL_EXECUTE(query).json;
      assertEqual(4, result.length);
      assertNull(result[0]);
      
      assertEqual(cn + "v1/" + keys[0], result[1]._from);
      assertEqual(cn + "v1/" + keys[1], result[1]._to);
      
      assertEqual(cn + "v1/" + keys[1], result[2]._from);
      assertEqual(cn + "v1/" + keys[2], result[2]._to);
      
      assertEqual(cn + "v1/" + keys[2], result[3]._from);
      assertEqual(cn + "v1/" + keys[3], result[3]._to);
    },
  
  };
}

jsunity.run(collectionTestSuite);
jsunity.run(namedGraphTestSuite);
if (isEnterprise && isCoordinator) {
  jsunity.run(smartGraphTestSuite);
}

return jsunity.done();
