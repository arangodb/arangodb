/* jshint globalstrict:false, strict:false, maxlen: 500 */
/* global assertEqual, assertTrue, assertFalse, AQL_EXECUTE, AQL_EXPLAIN */

// //////////////////////////////////////////////////////////////////////////////
// / @brief tests for index usage
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2010-2012 triagens GmbH, Cologne, Germany
// /
// / Licensed under the Apache License, Version 2.0 (the "License");
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
// / Copyright holder is triAGENS GmbH, Cologne, Germany
// /
// / @author Jan Steemann
// / @author Copyright 2012, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

var jsunity = require('jsunity');
var db = require('@arangodb').db;
var internal = require('internal');

// //////////////////////////////////////////////////////////////////////////////
// / @brief test suite
// //////////////////////////////////////////////////////////////////////////////

function optimizerEdgeIndexTestSuite () {
  var e;

  return {
    setUpAll: function () {
      db._drop('UnitTestsCollection');
      db._drop('UnitTestsEdgeCollection');
      db._create('UnitTestsCollection');
      e = db._createEdgeCollection('UnitTestsEdgeCollection');

      let edocs = [];
      for (var i = 0; i < 2000; i += 100) {
        var j;

        for (j = 0; j < i; ++j) {
          edocs.push({'_from': 'UnitTestsCollection/from' + i, '_to': 'UnitTestsCollection/nono', value: i + '-' + j });
        }
        for (j = 0; j < i; ++j) {
          edocs.push({'_from': 'UnitTestsCollection/nono', '_to': 'UnitTestsCollection/to' + i, value: i + '-' + j });
        }
      }
      e.insert(edocs);
      internal.waitForEstimatorSync();
    },

    tearDownAll: function () {
      db._drop('UnitTestsCollection');
      db._drop('UnitTestsEdgeCollection');
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test continuous index scan
// //////////////////////////////////////////////////////////////////////////////

    testFindContinuous: function () {
      for (var i = 0; i < 20; ++i) {
        var query = 'FOR i IN ' + e.name() + ' FILTER i._from == "UnitTestsCollection/nono" && i._to == "UnitTestsCollection/to' + i + '" LIMIT 1 RETURN i._key';
        var results = AQL_EXECUTE(query);
        assertEqual(0, results.json.length, query);
      }
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test index usage
// //////////////////////////////////////////////////////////////////////////////

    testFindNone: function () {
      var queries = [
        'FOR i IN ' + e.name() + ' FILTER i._from == "" RETURN i._key',
        'FOR i IN ' + e.name() + ' FILTER i._from == "UnitTestsCollection/from0" RETURN i._key',
        'FOR i IN ' + e.name() + ' FILTER i._from == "UnitTestsCollection/from1" RETURN i._key',
        'FOR i IN ' + e.name() + ' FILTER i._from == "UnitTestsCollection/from2" RETURN i._key',
        'FOR i IN ' + e.name() + ' FILTER i._from == "UnitTestsCollection/" RETURN i._key',
        'FOR i IN ' + e.name() + ' FILTER i._from == "UnitTestsCollection" RETURN i._key',
        'FOR i IN ' + e.name() + ' FILTER i._from == "/" RETURN i._key',
        'FOR i IN ' + e.name() + ' FILTER i._from == "--foobar--" RETURN i._key',
        'FOR i IN ' + e.name() + ' FILTER i._to == "" RETURN i._key',
        'FOR i IN ' + e.name() + ' FILTER i._to == "UnitTestsCollection/from0" RETURN i._key',
        'FOR i IN ' + e.name() + ' FILTER i._to == "UnitTestsCollection/from1" RETURN i._key',
        'FOR i IN ' + e.name() + ' FILTER i._to == "UnitTestsCollection/from2" RETURN i._key',
        'FOR i IN ' + e.name() + ' FILTER i._to == "UnitTestsCollection/" RETURN i._key',
        'FOR i IN ' + e.name() + ' FILTER i._to == "UnitTestsCollection" RETURN i._key',
        'FOR i IN ' + e.name() + ' FILTER i._to == "/" RETURN i._key',
        'FOR i IN ' + e.name() + ' FILTER i._to == "--foobar--" RETURN i._key'
      ];

      queries.forEach(function (query) {
        var results = AQL_EXECUTE(query);
        assertEqual([ ], results.json, query);
        assertEqual(0, results.stats.scannedFull);
      });
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test index usage
// //////////////////////////////////////////////////////////////////////////////

    testFindFrom: function () {
      var queries = [
        [ 'FOR i IN ' + e.name() + ' FILTER i._from == "UnitTestsCollection/from100" RETURN i._key', 100 ],
        [ 'FOR i IN ' + e.name() + ' FILTER i._from == "UnitTestsCollection/from200" RETURN i._key', 200 ],
        [ 'FOR i IN ' + e.name() + ' FILTER i._from == "UnitTestsCollection/from1000" RETURN i._key', 1000 ],
        [ 'FOR i IN ' + e.name() + ' FILTER i._from == "UnitTestsCollection/from1100" RETURN i._key', 1100 ],
        [ 'FOR i IN ' + e.name() + ' FILTER i._from == "UnitTestsCollection/from1900" RETURN i._key', 1900 ]
      ];

      queries.forEach(function (query) {
        var results = AQL_EXECUTE(query[0]);
        assertEqual(query[1], results.json.length, query[0]);
        assertEqual(0, results.stats.scannedFull);
        assertEqual(query[1], results.stats.scannedIndex);
      });
    },
    
    testFindFromNoDocuments: function () {
      var queries = [
        [ 'FOR i IN ' + e.name() + ' FILTER i._from == "UnitTestsCollection/from100" RETURN 1', 100 ],
        [ 'FOR i IN ' + e.name() + ' FILTER i._from == "UnitTestsCollection/from200" RETURN 1', 200 ],
        [ 'FOR i IN ' + e.name() + ' FILTER i._from == "UnitTestsCollection/from1000" RETURN 1', 1000 ],
        [ 'FOR i IN ' + e.name() + ' FILTER i._from == "UnitTestsCollection/from1100" RETURN 1', 1100 ],
        [ 'FOR i IN ' + e.name() + ' FILTER i._from == "UnitTestsCollection/from1900" RETURN 1', 1900 ]
      ];

      queries.forEach(function (query) {
        var results = AQL_EXECUTE(query[0]);
        assertEqual(query[1], results.json.length, query[0]);
        assertEqual(0, results.stats.scannedFull);
        assertEqual(query[1], results.stats.scannedIndex);
        let nodes = AQL_EXPLAIN(query[0]).plan.nodes.filter(function(n) { return n.type === 'IndexNode'; });
        assertEqual(1, nodes.length);
        assertFalse(nodes[0].producesResult);
      });
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test index usage
// //////////////////////////////////////////////////////////////////////////////

    testFindTo: function () {
      var queries = [
        [ 'FOR i IN ' + e.name() + ' FILTER i._to == "UnitTestsCollection/to100" RETURN i._key', 100 ],
        [ 'FOR i IN ' + e.name() + ' FILTER i._to == "UnitTestsCollection/to200" RETURN i._key', 200 ],
        [ 'FOR i IN ' + e.name() + ' FILTER i._to == "UnitTestsCollection/to1000" RETURN i._key', 1000 ],
        [ 'FOR i IN ' + e.name() + ' FILTER i._to == "UnitTestsCollection/to1100" RETURN i._key', 1100 ],
        [ 'FOR i IN ' + e.name() + ' FILTER i._to == "UnitTestsCollection/to1900" RETURN i._key', 1900 ]
      ];

      queries.forEach(function (query) {
        var results = AQL_EXECUTE(query[0]);
        assertEqual(query[1], results.json.length, query[0]);
        assertEqual(0, results.stats.scannedFull);
        assertEqual(query[1], results.stats.scannedIndex);
      });
    },
    
    testFindToNoDocuments: function () {
      var queries = [
        [ 'FOR i IN ' + e.name() + ' FILTER i._to == "UnitTestsCollection/to100" RETURN 1', 100 ],
        [ 'FOR i IN ' + e.name() + ' FILTER i._to == "UnitTestsCollection/to200" RETURN 1', 200 ],
        [ 'FOR i IN ' + e.name() + ' FILTER i._to == "UnitTestsCollection/to1000" RETURN 1', 1000 ],
        [ 'FOR i IN ' + e.name() + ' FILTER i._to == "UnitTestsCollection/to1100" RETURN 1', 1100 ],
        [ 'FOR i IN ' + e.name() + ' FILTER i._to == "UnitTestsCollection/to1900" RETURN 1', 1900 ]
      ];

      queries.forEach(function (query) {
        var results = AQL_EXECUTE(query[0]);
        assertEqual(query[1], results.json.length, query[0]);
        assertEqual(0, results.stats.scannedFull);
        assertEqual(query[1], results.stats.scannedIndex);
        let nodes = AQL_EXPLAIN(query[0]).plan.nodes.filter(function(n) { return n.type === 'IndexNode'; });
        assertEqual(1, nodes.length);
        assertFalse(nodes[0].producesResult);
      });
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test index usage
// //////////////////////////////////////////////////////////////////////////////

    testFindFromTo: function () {
      var queries = [
        [ 'FOR i IN ' + e.name() + ' FILTER i._from == "UnitTestsCollection/from100" && i._to == "UnitTestsCollection/nono" RETURN i._key', 100 ],
        [ 'FOR i IN ' + e.name() + ' FILTER i._from == "UnitTestsCollection/from200" && i._to == "UnitTestsCollection/nono" RETURN i._key', 200 ],
        [ 'FOR i IN ' + e.name() + ' FILTER i._from == "UnitTestsCollection/from1000" && i._to == "UnitTestsCollection/nono" RETURN i._key', 1000 ],
        [ 'FOR i IN ' + e.name() + ' FILTER i._from == "UnitTestsCollection/from1100" && i._to == "UnitTestsCollection/nono" RETURN i._key', 1100 ],
        [ 'FOR i IN ' + e.name() + ' FILTER i._from == "UnitTestsCollection/from1900" && i._to == "UnitTestsCollection/nono" RETURN i._key', 1900 ]
      ];

      queries.forEach(function (query) {
        var results = AQL_EXECUTE(query[0]);
        assertEqual(query[1], results.json.length, query[0]);
        assertEqual(0, results.stats.scannedFull);
      });
    },

    testLookupOnFromSortOnToAttribute: function () {
      let query = "FOR doc IN " + e.name() + " FILTER doc._from == 'UnitTestsCollection/nono' COLLECT to = doc._to RETURN to";
      let results = AQL_EXECUTE(query);
      assertEqual(19, results.json.length);

      let node = AQL_EXPLAIN(query).plan.nodes.filter(function(n) { return n.type === 'SortNode'; });
      assertEqual(1, node.length);
      node = node[0];
      assertEqual(1, node.elements.length);
      assertEqual("to", node.elements[0].inVariable.name);
      assertTrue(node.elements[0].ascending);
    },
    
    testLookupOnFromSortOnFromAttribute: function () {
      let query = "FOR doc IN " + e.name() + " FILTER doc._from == 'UnitTestsCollection/nono' COLLECT from = doc._from RETURN from";
      let results = AQL_EXECUTE(query);
      assertEqual(1, results.json.length);

      let node = AQL_EXPLAIN(query).plan.nodes.filter(function(n) { return n.type === 'SortNode'; });
      assertEqual(0, node.length);
    },
    
    testLookupOnFromSortOnFromToAttribute: function () {
      let query = "FOR doc IN " + e.name() + " FILTER doc._from == 'UnitTestsCollection/nono' COLLECT from = doc._from, to = doc._to RETURN { from, to }";
      let results = AQL_EXECUTE(query);
      assertEqual(19, results.json.length);

      let node = AQL_EXPLAIN(query).plan.nodes.filter(function(n) { return n.type === 'SortNode'; });
      assertEqual(1, node.length);
      node = node[0];
      assertEqual(2, node.elements.length);
      assertEqual("from", node.elements[0].inVariable.name);
      assertEqual("to", node.elements[1].inVariable.name);
      assertTrue(node.elements[0].ascending);
      assertTrue(node.elements[1].ascending);
    },
    
    testLookupOnFromSortOnToFromAttribute: function () {
      let query = "FOR doc IN " + e.name() + " FILTER doc._from == 'UnitTestsCollection/nono' COLLECT to = doc._to, from = doc._from RETURN { from, to }";
      let results = AQL_EXECUTE(query);
      assertEqual(19, results.json.length);

      let node = AQL_EXPLAIN(query).plan.nodes.filter(function(n) { return n.type === 'SortNode'; });
      assertEqual(1, node.length);
      node = node[0];
      assertEqual(2, node.elements.length);
      assertEqual("to", node.elements[0].inVariable.name);
      assertEqual("from", node.elements[1].inVariable.name);
      assertTrue(node.elements[0].ascending);
      assertTrue(node.elements[1].ascending);
    },

    testLookupOnToSortOnFromAttribute: function () {
      let query = "FOR doc IN " + e.name() + " FILTER doc._to == 'UnitTestsCollection/nono' COLLECT from = doc._from RETURN from";
      let results = AQL_EXECUTE(query);
      assertEqual(19, results.json.length);

      let node = AQL_EXPLAIN(query).plan.nodes.filter(function(n) { return n.type === 'SortNode'; });
      assertEqual(1, node.length);
      node = node[0];
      assertEqual(1, node.elements.length);
      assertEqual("from", node.elements[0].inVariable.name);
      assertTrue(node.elements[0].ascending);
    },

    testLookupOnToSortOnToAttribute: function () {
      let query = "FOR doc IN " + e.name() + " FILTER doc._to == 'UnitTestsCollection/nono' COLLECT to = doc._to RETURN to";
      let results = AQL_EXECUTE(query);
      assertEqual(1, results.json.length);

      let node = AQL_EXPLAIN(query).plan.nodes.filter(function(n) { return n.type === 'SortNode'; });
      assertEqual(0, node.length);
    },

    testLookupOnToSortOnToFromAttribute: function () {
      let query = "FOR doc IN " + e.name() + " FILTER doc._to == 'UnitTestsCollection/nono' COLLECT to = doc._to, from = doc._from RETURN { from, to }";
      let results = AQL_EXECUTE(query);
      assertEqual(19, results.json.length);

      let node = AQL_EXPLAIN(query).plan.nodes.filter(function(n) { return n.type === 'SortNode'; });
      assertEqual(1, node.length);
      node = node[0];
      assertEqual(2, node.elements.length);
      assertEqual("to", node.elements[0].inVariable.name);
      assertEqual("from", node.elements[1].inVariable.name);
      assertTrue(node.elements[0].ascending);
      assertTrue(node.elements[1].ascending);
    },
    
    testLookupOnToSortOnFromToAttribute: function () {
      let query = "FOR doc IN " + e.name() + " FILTER doc._to == 'UnitTestsCollection/nono' COLLECT from = doc._from, to = doc._to RETURN { from, to }";
      let results = AQL_EXECUTE(query);
      assertEqual(19, results.json.length);

      let node = AQL_EXPLAIN(query).plan.nodes.filter(function(n) { return n.type === 'SortNode'; });
      assertEqual(1, node.length);
      node = node[0];
      assertEqual(2, node.elements.length);
      assertEqual("from", node.elements[0].inVariable.name);
      assertEqual("to", node.elements[1].inVariable.name);
      assertTrue(node.elements[0].ascending);
      assertTrue(node.elements[1].ascending);
    }

  };
}

jsunity.run(optimizerEdgeIndexTestSuite);

return jsunity.done();
