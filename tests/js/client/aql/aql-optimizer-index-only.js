/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertTrue, assertFalse, assertEqual, AQL_EXPLAIN, AQL_EXECUTE */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for produces result
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
const normalize = require("@arangodb/aql-helper").normalizeProjections;
const disableSingleDocOp = { optimizer : { rules : [ "-optimize-cluster-single-document-operations"] } };

function optimizerIndexOnlyPrimaryTestSuite () {
  let c;

  return {
    setUpAll : function () {
      db._drop("UnitTestsCollection");
      c = db._create("UnitTestsCollection");

      for (var i = 0; i < 2000; ++i) {
        c.save({ _key: "test" + i, a: (i % 10), b: i });
      }
    },

    tearDownAll : function () {
      db._drop("UnitTestsCollection");
    },

    testNoProjectionsButIndex : function () {
      let queries = [
        `FOR doc IN ${c.name()} FILTER doc._key == "test123" RETURN doc`,
        `FOR doc IN ${c.name()} FILTER doc._key == "test123" SORT doc._key RETURN doc`,
        `FOR doc IN ${c.name()} FILTER doc._key == "test123" SORT doc RETURN doc.a`,
        `FOR doc IN ${c.name()} FILTER doc._key == "test123" SORT doc RETURN doc._key`,
        `FOR doc IN ${c.name()} FILTER doc._id == "${c.name()}/test123" RETURN doc`,
        `FOR doc IN ${c.name()} FILTER doc._id == "${c.name()}/test123" SORT doc RETURN doc._id`
      ];
     
      queries.forEach(function(query) {
        let plan = db._createStatement({query: query, bindVars:  {}, options:  disableSingleDocOp}).explain().plan;
        let nodes = plan.nodes.filter(function(n) { return n.type === 'IndexNode'; });
        assertEqual(1, nodes.length);
        assertEqual(normalize([]), normalize(nodes[0].projections), query);
        assertTrue(nodes[0].producesResult);
        assertFalse(nodes[0].indexCoversProjections);
      });
    },

    testNotCoveringProjection : function () {
      let queries = [
        [ `FOR doc IN ${c.name()} FILTER doc._key == "test123" RETURN doc.b`, ["b"] ],
        [ `FOR doc IN ${c.name()} FILTER doc._key == "test123" RETURN [ doc._key, doc.b ]`, ["_key", "b"] ],
        [ `FOR doc IN ${c.name()} FILTER doc._key == "test123" RETURN [ doc.a, doc.b ]`, ["a", "b"] ],
        [ `FOR doc IN ${c.name()} FILTER doc._key == "test123" RETURN [ doc.c, doc.d ]`, ["c", "d"] ],
        [ `FOR doc IN ${c.name()} FILTER doc._key == "test123" && doc.b == 1 RETURN doc.x`, ["x"] ],
        [ `FOR doc IN ${c.name()} FILTER doc._key == "test123" && doc.b == 1 RETURN CONCAT(doc._key, doc.u)`, ["_key", "u"] ],
        [ `FOR doc IN ${c.name()} FILTER doc._key == "test123" SORT doc.x RETURN doc.x`, ["x"] ]
      ];
    
      queries.forEach(function(query) { 
        let plan = db._createStatement({query: query[0], bindVars:  {}, options:  disableSingleDocOp}).explain().plan;
        let nodes = plan.nodes.filter(function(n) { return n.type === 'IndexNode'; });
        assertEqual(1, nodes.length);
        assertEqual(normalize(query[1]), normalize(nodes[0].projections), query);
        assertFalse(nodes[0].indexCoversProjections);
      });
    },
    
    testIndexCoveringProjection : function () {
      let query = `FOR doc IN ${c.name()} FILTER doc._key == "test123" RETURN doc._key`;
      let results = db._query(query, {}, disableSingleDocOp).toArray();
      assertEqual(1, results.length);
      assertEqual("test123", results[0]);
     
      let plan = db._createStatement({query: query, bindVars:  {}, options:  disableSingleDocOp}).explain().plan;
      let nodes = plan.nodes.filter(function(n) { return n.type === 'IndexNode'; });
      assertEqual(1, nodes.length);
      assertEqual(normalize(["_key"]), normalize(nodes[0].projections));
      assertTrue(nodes[0].indexCoversProjections);
    },
    
    testIndexCoveringInProjection : function () {
      let query = `FOR doc IN ${c.name()} FILTER doc._key IN ["test123", "test124", "test125"] RETURN doc._key`;
      let results = db._query(query, {}, disableSingleDocOp).toArray();
      assertEqual(3, results.length);
     
      let plan = db._createStatement({query: query, bindVars:  {}, options:  disableSingleDocOp}).explain().plan;
      let nodes = plan.nodes.filter(function(n) { return n.type === 'IndexNode'; });
      assertEqual(1, nodes.length);
      assertEqual(normalize(["_key"]), normalize(nodes[0].projections));
      assertTrue(nodes[0].indexCoversProjections);
    }

  };
}

function optimizerIndexOnlyEdgeTestSuite () {
  let c;

  return {
    setUpAll : function () {
      db._drop("UnitTestsCollection");
      c = db._createEdgeCollection("UnitTestsCollection");

      for (var i = 0; i < 2000; ++i) {
        c.save({ _key: "test" + i, _from: "test/" + (i % 10), _to: "test/" + i });
      }
    },

    tearDownAll : function () {
      db._drop("UnitTestsCollection");
    },

    testEdgeNoProjectionsButIndex : function () {
      let queries = [
        `FOR doc IN ${c.name()} FILTER doc._from == "test/123" RETURN doc`,
        `FOR doc IN ${c.name()} FILTER doc._from == "test/123" SORT doc._from RETURN doc`,
        `FOR doc IN ${c.name()} FILTER doc._from == "test/123" SORT doc RETURN doc`,
        `FOR doc IN ${c.name()} FILTER doc._from == "test/123" SORT doc RETURN doc._to`,
        `FOR doc IN ${c.name()} FILTER doc._from == "test/123" SORT doc RETURN doc._a`,
        `FOR doc IN ${c.name()} FILTER doc._from == "test/123" RETURN doc`,
        `FOR doc IN ${c.name()} FILTER doc._to == "test/123" RETURN doc`,
        `FOR doc IN ${c.name()} FILTER doc._to == "test/123" SORT doc._from RETURN doc`,
        `FOR doc IN ${c.name()} FILTER doc._to == "test/123" SORT doc._to RETURN doc`,
        `FOR doc IN ${c.name()} FILTER doc._to == "test/123" SORT doc RETURN doc._to`,
        `FOR doc IN ${c.name()} FILTER doc._to == "test/123" SORT doc RETURN doc._from`,
        `FOR doc IN ${c.name()} FILTER doc._to == "test/123" SORT doc RETURN doc._a`,
        `FOR doc IN ${c.name()} FILTER doc._to == "test/123" RETURN doc`
      ];
     
      queries.forEach(function(query) {
        let plan = db._createStatement(query).explain().plan;
        let nodes = plan.nodes.filter(function(n) { return n.type === 'IndexNode'; });
        assertEqual(1, nodes.length);
        assertEqual(normalize([]), normalize(nodes[0].projections), query);
        assertTrue(nodes[0].producesResult);
        assertFalse(nodes[0].indexCoversProjections);
      });
    },

    testEdgeNotCoveringProjection : function () {
      let queries = [
        [ `FOR doc IN ${c.name()} FILTER doc._from == "test/123" RETURN doc.b`, ["b"] ],
        [ `FOR doc IN ${c.name()} FILTER doc._from == "test/123" RETURN [ doc._from, doc.b ]`, ["_from", "b"] ],
        [ `FOR doc IN ${c.name()} FILTER doc._from == "test/123" RETURN [ doc.a, doc.b ]`, ["a", "b"] ],
        [ `FOR doc IN ${c.name()} FILTER doc._from == "test/123" RETURN [ doc.c, doc.d ]`, ["c", "d"] ],
        [ `FOR doc IN ${c.name()} FILTER doc._from == "test/123" && doc.b == 1 RETURN doc.x`, ["x"] ],
        [ `FOR doc IN ${c.name()} FILTER doc._from == "test/123" && doc.b == 1 RETURN CONCAT(doc._from, doc.u)`, ["_from", "u"] ],
        [ `FOR doc IN ${c.name()} FILTER doc._from == "test/123" SORT doc.x RETURN doc.x`, ["x"] ],
        [ `FOR doc IN ${c.name()} FILTER doc._to == "test/123" RETURN doc.b`, ["b"] ],
        [ `FOR doc IN ${c.name()} FILTER doc._to == "test/123" RETURN [ doc._to, doc.b ]`, ["_to", "b"] ],
        [ `FOR doc IN ${c.name()} FILTER doc._to == "test/123" RETURN [ doc.a, doc.b ]`, ["a", "b"] ],
        [ `FOR doc IN ${c.name()} FILTER doc._to == "test/123" RETURN [ doc.c, doc.d ]`, ["c", "d"] ],
        [ `FOR doc IN ${c.name()} FILTER doc._to == "test/123" && doc.b == 1 RETURN doc.x`, ["x"] ],
        [ `FOR doc IN ${c.name()} FILTER doc._to == "test/123" && doc.b == 1 RETURN CONCAT(doc._to, doc.u)`, ["_to", "u"] ],
        [ `FOR doc IN ${c.name()} FILTER doc._to == "test/123" SORT doc.x RETURN doc.x`, ["x"] ]

      ];
    
      queries.forEach(function(query) { 
        let plan = db._createStatement(query[0]).explain().plan;
        let nodes = plan.nodes.filter(function(n) { return n.type === 'IndexNode'; });
        assertEqual(1, nodes.length);
        assertEqual(normalize(query[1]), normalize(nodes[0].projections), query);
        assertFalse(nodes[0].indexCoversProjections);
      });
    },

    testEdgeIndexFromCoveringProjection : function () {
      let queries = [
        [ `FOR doc IN ${c.name()} FILTER doc._from == "test/123" RETURN doc._from`, ["_from"] ],
        [ `FOR doc IN ${c.name()} FILTER doc._from == "test/123" SORT doc._from RETURN doc._to`, ["_to"] ],
        [ `FOR doc IN ${c.name()} FILTER doc._from == "test/123" RETURN doc._to`, ["_to"] ],
        [ `FOR doc IN ${c.name()} FILTER doc._to == "test/123" SORT doc._to RETURN doc._from`, ["_from"] ],
        [ `FOR doc IN ${c.name()} FILTER doc._to == "test/123" RETURN doc._from`, ["_from"] ]
      ];
     
      queries.forEach(function(query) { 
        let plan = db._createStatement(query[0]).explain().plan;
        let nodes = plan.nodes.filter(function(n) { return n.type === 'IndexNode'; });
        assertEqual(1, nodes.length);
        assertEqual(normalize(query[1]), normalize(nodes[0].projections), query);
        assertTrue(nodes[0].indexCoversProjections);
      });
    },
    
    testEdgeIndexFromCoveringInProjection : function () {
      let query = `FOR doc IN ${c.name()} FILTER doc._from IN ["test/123", "test/124", "test/125"] RETURN doc._from`;
     
      let plan = db._createStatement(query).explain().plan;
      let nodes = plan.nodes.filter(function(n) { return n.type === 'IndexNode'; });
      assertEqual(1, nodes.length);
      assertEqual(normalize(["_from"]), normalize(nodes[0].projections));
      assertTrue(nodes[0].indexCoversProjections);
    },
    
    testEdgeIndexToCoveringProjection : function () {
      let query = `FOR doc IN ${c.name()} FILTER doc._to == "test/123" RETURN doc._to`;
     
      let plan = db._createStatement(query).explain().plan;
      let nodes = plan.nodes.filter(function(n) { return n.type === 'IndexNode'; });
      assertEqual(1, nodes.length);
      assertEqual(normalize(["_to"]), normalize(nodes[0].projections));
      assertTrue(nodes[0].indexCoversProjections);
    },
    

    testEdgeIndexToCoveringInProjection : function () {
      let query = `FOR doc IN ${c.name()} FILTER doc._to IN ["test/123", "test/124", "test/125"] RETURN doc._to`;
     
      let plan = db._createStatement(query).explain().plan;
      let nodes = plan.nodes.filter(function(n) { return n.type === 'IndexNode'; });
      assertEqual(1, nodes.length);
      assertEqual(normalize(["_to"]), normalize(nodes[0].projections));
      assertTrue(nodes[0].indexCoversProjections);
    }

  };
}

function optimizerIndexOnlyVPackTestSuite () {
  let c;

  return {
    setUp : function () {
      db._drop("UnitTestsCollection");
      c = db._create("UnitTestsCollection");

      for (var i = 0; i < 2000; ++i) {
        c.save({ _key: "test" + i, a: (i % 10), b: i });
      }
    },

    tearDown : function () {
      db._drop("UnitTestsCollection");
    },

    testNoProjectionsNoIndex : function () {
      let queries = [
        `FOR doc IN ${c.name()} RETURN doc`,
        `FOR doc IN ${c.name()} FILTER doc.a == 1 && doc.b == 1 && doc.c == 1 && doc.d == 1 && doc.e == 1 RETURN doc`,
        `FOR doc IN ${c.name()} RETURN [doc.a, doc.b, doc.c, doc.d, doc.e, doc.f]`
      ];

      queries.forEach(function(query) {
        let plan = db._createStatement(query).explain().plan;
        let nodes = plan.nodes.filter(function(n) { return n.type === 'EnumerateCollectionNode'; });
        assertEqual(1, nodes.length);
        assertEqual([], nodes[0].projections);
        assertTrue(nodes[0].producesResult);
      });
    },
    
    testJustProjectionsNoIndex : function () {
      let queries = [
        [ `FOR doc IN ${c.name()} RETURN doc.a`, ["a"] ],
        [ `FOR doc IN ${c.name()} RETURN [ doc.a, doc.b ]`, ["a", "b"] ],
        [ `FOR doc IN ${c.name()} RETURN [ doc.a, doc.b, doc.c ]`, ["a", "b", "c"] ],
        [ `FOR doc IN ${c.name()} RETURN [ doc.a, doc.b, doc.c, doc.d ]`, ["a", "b", "c", "d"] ],
        [ `FOR doc IN ${c.name()} RETURN [ doc.a, doc.b, doc.c, doc.d, doc.e ]`, ["a", "b", "c", "d", "e"] ],
        [ `FOR doc IN ${c.name()} RETURN [ doc.a, doc.b, doc.c, doc.d, doc.e, 123 ]`, ["a", "b", "c", "d", "e"] ],
        [ `FOR doc IN ${c.name()} FILTER doc.y == 1 RETURN [ doc.a, doc.b ]`, ["a", "b"] ],
        [ `FOR doc IN ${c.name()} FILTER doc.x == 1 && doc.y == 1 RETURN [ doc.a, doc.b ]`, ["a", "b"] ],
        [ `FOR doc IN ${c.name()} SORT doc.x RETURN doc.a`, ["a", "x"] ],
        [ `FOR doc IN ${c.name()} SORT doc.x, doc.y RETURN doc.a`, ["a", "x", "y"] ]
      ];
     
      queries.forEach(function(query) {
        let plan = db._createStatement(query[0]).explain().plan;
        let nodes = plan.nodes.filter(function(n) { return n.type === 'EnumerateCollectionNode'; });
        assertEqual(1, nodes.length);
        assertEqual(normalize(query[1]), normalize(nodes[0].projections));
        assertTrue(nodes[0].producesResult);
      });
    },
    
    testVPackNoProjectionsButIndex : function () {
      c.ensureIndex({ type: "hash", fields: ["a"] });

      let queries = [
        `FOR doc IN ${c.name()} FILTER doc.a == 1 RETURN doc`,
        `FOR doc IN ${c.name()} FILTER doc.a == 1 SORT doc RETURN doc.a`,
        `FOR doc IN ${c.name()} FILTER doc.a > 1 RETURN doc`
      ];
     
      queries.forEach(function(query) {
        let plan = db._createStatement(query).explain().plan;
        let nodes = plan.nodes.filter(function(n) { return n.type === 'IndexNode'; });
        assertEqual(1, nodes.length);
        assertEqual([], nodes[0].projections);
        assertTrue(nodes[0].producesResult);
        assertFalse(nodes[0].indexCoversProjections);
      });
    },

    testVPackSingleFieldIndexNotCoveringProjection : function () {
      c.ensureIndex({ type: "hash", fields: ["a"] });
      
      let queries = [
        [ `FOR doc IN ${c.name()} FILTER doc.a >= 0 RETURN doc.b`, ["b"] ],
        [ `FOR doc IN ${c.name()} FILTER doc.a >= 0 RETURN [ doc.a, doc.b ]`, ["a", "b"] ],
        [ `FOR doc IN ${c.name()} FILTER doc.a >= 0 RETURN [ doc.c, doc.d ]`, ["c", "d"] ],
        [ `FOR doc IN ${c.name()} FILTER doc.a >= 0 && doc.b == 1 RETURN doc.x`, ["x"] ],
        [ `FOR doc IN ${c.name()} FILTER doc.a >= 0 && doc.b == 1 RETURN doc.a + doc.u`, ["a", "u"] ],
        [ `FOR doc IN ${c.name()} FILTER doc.a == 1 SORT doc.x RETURN doc.x`, ["x"] ]
      ];
    
      queries.forEach(function(query) { 
        let plan = db._createStatement(query[0]).explain().plan;
        let nodes = plan.nodes.filter(function(n) { return n.type === 'IndexNode'; });
        assertEqual(1, nodes.length);
        assertEqual(normalize(query[1]), normalize(nodes[0].projections));
        assertFalse(nodes[0].indexCoversProjections);
      });
    },
    
    testVPackSingleFieldIndexCoveringProjection : function () {
      c.ensureIndex({ type: "hash", fields: ["a"] });
      
      let query = `FOR doc IN ${c.name()} FILTER doc.a >= 0 RETURN doc.a`;
      let results = db._query(query).toArray();
      assertEqual(2000, results.length);
     
      let plan = db._createStatement(query).explain().plan;
      let nodes = plan.nodes.filter(function(n) { return n.type === 'IndexNode'; });
      assertEqual(1, nodes.length);
      assertEqual(normalize(["a"]), normalize(nodes[0].projections));
      assertTrue(nodes[0].indexCoversProjections);
    },
    
    testVPackSingleFieldIndexCoveringInProjection : function () {
      c.ensureIndex({ type: "hash", fields: ["a"] });
      
      let query = `FOR doc IN ${c.name()} FILTER doc.a IN [0, 1, 2, 3, 4, 5, 6, 7, 8, 9] RETURN doc.a`;
      let results = db._query(query).toArray();
      assertEqual(2000, results.length);
     
      let plan = db._createStatement(query).explain().plan;
      let nodes = plan.nodes.filter(function(n) { return n.type === 'IndexNode'; });
      assertEqual(1, nodes.length);
      assertEqual(normalize(["a"]), normalize(nodes[0].projections));
      assertTrue(nodes[0].indexCoversProjections);
    },
    
    testVPackSingleFieldUniqueIndexCoveringProjection : function () {
      c.ensureIndex({ type: "hash", fields: ["b"], unique: true });
      
      let query = `FOR doc IN ${c.name()} FILTER doc.b >= 0 RETURN doc.b`;
      let results = db._query(query).toArray();
      assertEqual(2000, results.length);
     
      let plan = db._createStatement(query).explain().plan;
      let nodes = plan.nodes.filter(function(n) { return n.type === 'IndexNode'; });
      assertEqual(1, nodes.length);
      assertEqual(normalize(["b"]), normalize(nodes[0].projections));
      assertTrue(nodes[0].indexCoversProjections);
    },
    
    testVPackMultipleFieldsIndexCoveringProjection : function () {
      c.ensureIndex({ type: "hash", fields: ["a", "b"] });
      
      let queries = [
        [ `FOR doc IN ${c.name()} FILTER doc.a >= 0 RETURN doc.a`, ["a"] ],
        [ `FOR doc IN ${c.name()} FILTER doc.a >= 0 SORT doc.a RETURN doc.a`, ["a"] ],
        [ `FOR doc IN ${c.name()} FILTER doc.a >= 0 SORT doc.a RETURN doc.b`, ["b"] ],
        [ `FOR doc IN ${c.name()} FILTER doc.a >= 0 SORT doc.a, doc.b RETURN doc.a`, ["a"] ],
        [ `FOR doc IN ${c.name()} FILTER doc.a >= 0 SORT doc.a, doc.b RETURN [ doc.a, doc.b ]`, ["a", "b"] ],
        [ `FOR doc IN ${c.name()} FILTER doc.a >= 0 RETURN [ doc.a, doc.b ]`, ["a", "b"] ],
        [ `FOR doc IN ${c.name()} FILTER doc.a >= 0 && doc.b >= 0 RETURN doc.a`, ["a"] ],
        [ `FOR doc IN ${c.name()} FILTER doc.a >= 0 && doc.b >= 0 RETURN [ doc.a, doc.b ]`, ["a", "b"] ],
        [ `FOR doc IN ${c.name()} FILTER doc.a >= 0 && doc.b >= 0 SORT doc.a RETURN [ doc.a, doc.b ]`, ["a", "b"] ],
        [ `FOR doc IN ${c.name()} FILTER doc.a >= 0 && doc.b >= 0 SORT doc.a, doc.b RETURN [ doc.a, doc.b ]`, ["a", "b"] ],
      ];

      queries.forEach(function(query) {
        let plan = db._createStatement(query[0]).explain().plan;
        let nodes = plan.nodes.filter(function(n) { return n.type === 'IndexNode'; });
        assertEqual(1, nodes.length);
        assertEqual(normalize(query[1]), normalize(nodes[0].projections), query);
        assertTrue(nodes[0].indexCoversProjections);
      });
    },

    testVPackMultipleFieldsUniqueIndexCoveringProjection : function () {
      c.ensureIndex({ type: "hash", fields: ["a", "b"], unique: true });
      
      let queries = [
        [ `FOR doc IN ${c.name()} FILTER doc.a >= 0 RETURN doc.a`, ["a"] ],
        [ `FOR doc IN ${c.name()} FILTER doc.a >= 0 SORT doc.a RETURN doc.a`, ["a"] ],
        [ `FOR doc IN ${c.name()} FILTER doc.a >= 0 SORT doc.a RETURN doc.b`, ["b"] ],
        [ `FOR doc IN ${c.name()} FILTER doc.a >= 0 SORT doc.a, doc.b RETURN doc.a`, ["a"] ],
        [ `FOR doc IN ${c.name()} FILTER doc.a >= 0 SORT doc.a, doc.b RETURN [ doc.a, doc.b ]`, ["a", "b"] ],
        [ `FOR doc IN ${c.name()} FILTER doc.a >= 0 RETURN [ doc.a, doc.b ]`, ["a", "b"] ],
        [ `FOR doc IN ${c.name()} FILTER doc.a >= 0 && doc.b >= 0 RETURN doc.a`, ["a"] ],
        [ `FOR doc IN ${c.name()} FILTER doc.a >= 0 && doc.b >= 0 RETURN [ doc.a, doc.b ]`, ["a", "b"] ],
        [ `FOR doc IN ${c.name()} FILTER doc.a >= 0 && doc.b >= 0 SORT doc.a RETURN [ doc.a, doc.b ]`, ["a", "b"] ],
        [ `FOR doc IN ${c.name()} FILTER doc.a >= 0 && doc.b >= 0 SORT doc.a, doc.b RETURN [ doc.a, doc.b ]`, ["a", "b"] ],
      ];

      queries.forEach(function(query) {
        let plan = db._createStatement(query[0]).explain().plan;
        let nodes = plan.nodes.filter(function(n) { return n.type === 'IndexNode'; });
        assertEqual(1, nodes.length);
        assertEqual(normalize(query[1]), normalize(nodes[0].projections), query);
        assertTrue(nodes[0].indexCoversProjections);
      });
    }

  };
}

jsunity.run(optimizerIndexOnlyPrimaryTestSuite);
jsunity.run(optimizerIndexOnlyEdgeTestSuite);
jsunity.run(optimizerIndexOnlyVPackTestSuite);

return jsunity.done();
