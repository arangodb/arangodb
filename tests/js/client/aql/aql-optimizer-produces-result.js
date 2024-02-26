/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertTrue, assertFalse, assertEqual */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
// / Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
// /
// / Licensed under the Business Source License 1.1 (the "License");
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     https://github.com/arangodb/arangodb/blob/devel/LICENSE
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
/// @author Jan Steemann
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const db = require("@arangodb").db;

function optimizerProducesResultTestSuite () {
  let c;

  return {
    setUpAll : function () {
      db._drop("UnitTestsCollection");
      c = db._create("UnitTestsCollection");

      let docs = [];
      for (let i = 0; i < 2000; ++i) {
        docs.push({ _key: "test" + i, a: (i % 10), b: i, c: i, x: (i % 10), y: i });
      }
      c.insert(docs);

      c.ensureIndex({ type: "hash", fields: ["a", "b"] });
      c.ensureIndex({ type: "hash", fields: ["c"] });
      c.ensureIndex({ type: "skiplist", fields: ["x"] });
    },

    tearDownAll : function () {
      db._drop("UnitTestsCollection");
    },

    testFullScan : function () {
      let query = `FOR doc IN ${c.name()} RETURN 1`;
      let results = db._query(query).toArray();
      assertEqual(2000, results.length);
      for (let i = 0; i < results.length; ++i) {
        assertEqual(1, results[i]);
      }
      
      let plan = db._createStatement(query).explain().plan;
      let nodes = plan.nodes.filter(function(n) { return n.type === 'EnumerateCollectionNode'; });
      if (nodes.length === 0) {
        // rocksdb
        nodes = plan.nodes.filter(function(n) { return n.type === 'IndexNode'; });
      }
      assertEqual(1, nodes.length);
      assertFalse(nodes[0].producesResult);
    },
    
    testFiltered : function () {
      let query = `FOR doc IN ${c.name()} FILTER doc.c == 2 RETURN 1`;
      let results = db._query(query).toArray();
      assertEqual(1, results.length);
      assertEqual(1, results[0]);
      
      let plan = db._createStatement(query).explain().plan;
      let nodes = plan.nodes.filter(function(n) { return n.type === 'IndexNode'; });
      assertEqual(1, nodes.length);
      assertFalse(nodes[0].producesResult);
    },
    
    
    testIndexed : function () {
      let query = `FOR doc IN ${c.name()} FILTER doc.c == 5 RETURN 1`;
      let results = db._query(query).toArray();
      assertEqual(1, results.length);
      assertEqual(1, results[0]);
      
      let plan = db._createStatement(query).explain().plan;
      let nodes = plan.nodes.filter(function(n) { return n.type === 'IndexNode'; });
      assertEqual(1, nodes.length);
      assertFalse(nodes[0].producesResult);
    },
    
    testIndexedBoth : function () {
      let query = `FOR doc IN ${c.name()} FILTER doc.a == 1 && doc.b == 1 RETURN 1`;
      let results = db._query(query).toArray();
      assertEqual(1, results.length);
      assertEqual(1, results[0]);
      
      let plan = db._createStatement(query).explain().plan;
      let nodes = plan.nodes.filter(function(n) { return n.type === 'IndexNode'; });
      assertEqual(1, nodes.length);
      assertFalse(nodes[0].producesResult);
    },
    
    testIndexedRange : function () {
      let query = `FOR doc IN ${c.name()} FILTER doc.x >= 8 RETURN 1`;
      let results = db._query(query).toArray();
      assertEqual(400, results.length);
      for (let i = 0; i < results.length; ++i) {
        assertEqual(1, results[i]);
      }
      
      let plan = db._createStatement(query).explain().plan;
      let nodes = plan.nodes.filter(function(n) { return n.type === 'IndexNode'; });
      assertEqual(1, nodes.length);
      assertFalse(nodes[0].producesResult);
    },
    
    testDisabled : function () {
      let queries = [
        `FOR doc IN ${c.name()} FILTER doc.whatever != 2 RETURN 1`,
        `FOR doc IN ${c.name()} FILTER doc.whatever != 2 RETURN doc`,
        `FOR doc IN ${c.name()} FILTER doc.c == 9 RETURN doc.c`,
        `FOR doc IN ${c.name()} FILTER doc.x >= 8 RETURN doc.x`
      ];

      queries.forEach(function(query) {
        let plan = db._createStatement(query).explain().plan;
        let nodes = plan.nodes.filter(function(n) { return n.type === 'IndexNode' || n.type === 'EnumerateCollectionNode'; });
        assertEqual(1, nodes.length);
        assertTrue(nodes[0].producesResult);
      });
    }

  };
}

jsunity.run(optimizerProducesResultTestSuite);

return jsunity.done();

