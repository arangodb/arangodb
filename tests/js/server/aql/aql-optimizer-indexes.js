/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global AQL_EXPLAIN, AQL_EXECUTE */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for index usage
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

const jsunity = require('jsunity');
const assert = require('jsunity').jsUnity.assertions;
const {assertTrue, assertFalse, assertEqual, assertNotEqual} = assert;
const db = require('@arangodb').db;
const _ = require('lodash');

const opt = { optimizer: { rules: ["-reduce-extraction-to-projection"] } };

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function optimizerIndexesTestSuite () {
  var c;
  var idx = null;
  var idx1 = null;
  let deleteDefaultIdx = function() {
    db._dropIndex(idx);
    idx = null;
  };

  return {
    setUpAll: function() {
      db._drop("UnitTestsCollection");
      c = db._create("UnitTestsCollection");

      let docs = [];
      for (let i = 0; i < 2000; ++i) {
        docs.push({ _key: "test" + i, value: i });
      }
      c.insert(docs);
      
      idx = c.ensureSkiplist("value");
    },
    
    setUp: function() {
      if (idx === null) {
        idx = c.ensureSkiplist("value");
      }
    },

    tearDownAll: function() {
      db._drop("UnitTestsCollection");
    },

    tearDown: function() {
      if (idx1 !== null) {
        db._dropIndex(idx1);
        idx1 = null;
      }
    },
    
    testIndexUsedForExpansion1 : function () {
      let query = "LET test = NOOPT([{ value: 1 }, { value : 2 }]) FOR doc IN " + c.name() + " FILTER doc.value IN test[*].value SORT doc.value RETURN doc.value";

      let plan = AQL_EXPLAIN(query).plan;
      let nodeTypes = plan.nodes.map(function(node) {
        return node.type;
      });

      assertEqual("SingletonNode", nodeTypes[0], query);
      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);

      let results = AQL_EXECUTE(query);
      assertEqual([ 1, 2 ], results.json, query);
      assertEqual(0, results.stats.scannedFull);
      assertTrue(results.stats.scannedIndex > 0);
    },
    
    testIndexUsedForExpansion2 : function () {
      let query = "LET test = NOOPT([1, 2]) FOR doc IN " + c.name() + " FILTER doc.value IN test[*] SORT doc.value RETURN doc.value";

      let plan = AQL_EXPLAIN(query).plan;
      let nodeTypes = plan.nodes.map(function(node) {
        return node.type;
      });

      assertEqual("SingletonNode", nodeTypes[0], query);
      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);

      let results = AQL_EXECUTE(query);
      assertEqual([ 1, 2 ], results.json, query);
      assertEqual(0, results.stats.scannedFull);
      assertTrue(results.stats.scannedIndex > 0);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test same results for const access queries
////////////////////////////////////////////////////////////////////////////////

    testSameResultsConstAccess : function () {
      var bind = { doc : { key: "test1" } };
      var q1 = `RETURN (FOR item IN UnitTestsCollection FILTER (@doc.key == item._key) LIMIT 1 RETURN item)[0]`;
      var q2 = `LET doc = @doc RETURN (FOR item IN UnitTestsCollection FILTER (doc.key == item._key) LIMIT 1 RETURN item)[0]`;
      var q3 = `LET doc = { key: "test1" } RETURN (FOR item IN UnitTestsCollection FILTER (doc.key == item._key) LIMIT 1 RETURN item)[0]`;

      var results = AQL_EXECUTE(q1, bind);
      assertEqual(1, results.json.length);
      assertEqual("test1", results.json[0]._key);

      results = AQL_EXECUTE(q2, bind);
      assertEqual(1, results.json.length);
      assertEqual("test1", results.json[0]._key);

      results = AQL_EXECUTE(q3);
      assertEqual(1, results.json.length);
      assertEqual("test1", results.json[0]._key);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test _id
////////////////////////////////////////////////////////////////////////////////

    testUsePrimaryIdEq : function () {
      var query = "FOR i IN " + c.name() + " FILTER i._id == 'UnitTestsCollection/test22' RETURN i.value";

      var plan = AQL_EXPLAIN(query, {}, opt).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        return node.type;
      });

      assertEqual("SingletonNode", nodeTypes[0], query);
      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);

      var results = AQL_EXECUTE(query, {}, opt);
      assertEqual([ 22 ], results.json, query);
      assertEqual(0, results.stats.scannedFull);
      assertEqual(1, results.stats.scannedIndex);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test _id
////////////////////////////////////////////////////////////////////////////////

    testUsePrimaryIdEqNoMatches : function () {
      var query = "FOR i IN " + c.name() + " FILTER i._id == 'UnitTestsCollection/qux' RETURN i.value";

      var plan = AQL_EXPLAIN(query, {}, opt).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        return node.type;
      });

      assertEqual("SingletonNode", nodeTypes[0], query);
      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);

      var results = AQL_EXECUTE(query, {}, opt);
      assertEqual([ ], results.json, query);
      assertEqual(0, results.stats.scannedFull);
      assertEqual(0, results.stats.scannedIndex);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test _id
////////////////////////////////////////////////////////////////////////////////

    testUsePrimaryIdInAttributeAccess : function () {
      var values = [ "UnitTestsCollection/test1", "UnitTestsCollection/test2", "UnitTestsCollection/test21", "UnitTestsCollection/test30" ];
      var query = "LET data = NOOPT({ ids : " + JSON.stringify(values) + " }) FOR i IN " + c.name() + " FILTER i._id IN data.ids RETURN i.value";

      var plan = AQL_EXPLAIN(query, {}, opt).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        if (node.type === 'IndexNode') {
          assertTrue(node.producesResult);
        }
        return node.type;
      });

      assertEqual("SingletonNode", nodeTypes[0], query);
      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);

      var results = AQL_EXECUTE(query, {}, opt);
      assertEqual([ 1, 2, 21, 30 ], results.json.sort(), query);
      assertEqual(0, results.stats.scannedFull);
      assertEqual(4, results.stats.scannedIndex);
    },

    testUsePrimaryIdNoDocuments : function () {
      var values = [ "UnitTestsCollection/test1", "UnitTestsCollection/test2", "UnitTestsCollection/test21", "UnitTestsCollection/test30" ];
      var query = "FOR i IN " + c.name() + " FILTER i._id IN " + JSON.stringify(values) + " RETURN 1";

      var plan = AQL_EXPLAIN(query, {}, opt).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        if (node.type === 'IndexNode') {
          assertFalse(node.producesResult);
        }
        return node.type;
      });

      assertEqual("SingletonNode", nodeTypes[0], query);
      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);

      var results = AQL_EXECUTE(query, {}, opt);
      assertEqual([ 1, 1, 1, 1 ], results.json, query);
      assertEqual(0, results.stats.scannedFull);
      assertEqual(4, results.stats.scannedIndex);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test _id
////////////////////////////////////////////////////////////////////////////////

    testUsePrimaryId : function () {
      var values = [ "UnitTestsCollection/test1", "UnitTestsCollection/test2", "UnitTestsCollection/test21", "UnitTestsCollection/test30" ];
      var query = "FOR i IN " + c.name() + " FILTER i._id IN " + JSON.stringify(values) + " RETURN i.value";

      var plan = AQL_EXPLAIN(query, {}, opt).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        if (node.type === 'IndexNode') {
          assertTrue(node.producesResult);
        }
        return node.type;
      });

      assertEqual("SingletonNode", nodeTypes[0], query);
      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);

      var results = AQL_EXECUTE(query, {}, opt);
      assertEqual([ 1, 2, 21, 30 ], results.json.sort(), query);
      assertEqual(0, results.stats.scannedFull);
      assertEqual(4, results.stats.scannedIndex);
    },

    ////////////////////////////////////////////////////////////////////////////////
/// @brief test _id
////////////////////////////////////////////////////////////////////////////////

    testUsePrimaryIdNoMatches : function () {
      var values = [ "UnitTestsCollection/foo", "UnitTestsCollection/bar", "UnitTestsCollection/baz", "UnitTestsCollection/qux" ];
      var query = "FOR i IN " + c.name() + " FILTER i._id IN " + JSON.stringify(values) + " RETURN i.value";

      var plan = AQL_EXPLAIN(query, {}, opt).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        if (node.type === 'IndexNode') {
          assertTrue(node.producesResult);
        }
        return node.type;
      });

      assertEqual("SingletonNode", nodeTypes[0], query);
      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);

      var results = AQL_EXECUTE(query, {}, opt);
      assertEqual([ ], results.json, query);
      assertEqual(0, results.stats.scannedFull);
      assertEqual(0, results.stats.scannedIndex);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test _key
////////////////////////////////////////////////////////////////////////////////

    testUsePrimaryKeyEq : function () {
      var query = "FOR i IN " + c.name() + " FILTER i._key == 'test6' RETURN i.value";

      var plan = AQL_EXPLAIN(query, {}, opt).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        if (node.type === 'IndexNode') {
          assertTrue(node.producesResult);
        } else if (node.type === 'SingleRemoteOperationNode') {
          assertTrue(node.producesResult);
        }
        return node.type;
      });

      assertEqual("SingletonNode", nodeTypes[0], query);
      assertTrue(
        (
          ( nodeTypes.indexOf("IndexNode") !== -1) ||
          ( nodeTypes.indexOf("SingleRemoteOperationNode") !== -1)
        ), query);

      var results = AQL_EXECUTE(query, {}, opt);
      assertEqual([ 6 ], results.json, query);
      assertEqual(0, results.stats.scannedFull);
      assertEqual(1, results.stats.scannedIndex);
    },

    testUsePrimaryKeyEqNoDocuments : function () {
      var query = "FOR i IN " + c.name() + " FILTER i._key == 'test6' RETURN 1";

      var plan = AQL_EXPLAIN(query, {}, opt).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        if (node.type === 'IndexNode') {
          assertFalse(node.producesResult);
        }
        return node.type;
      });

      assertEqual("SingletonNode", nodeTypes[0], query);
      assertTrue(
        (
          ( nodeTypes.indexOf("IndexNode") !== -1) ||
          ( nodeTypes.indexOf("SingleRemoteOperationNode") !== -1)
        ), query);

      var results = AQL_EXECUTE(query, {}, opt);
      assertEqual([ 1 ], results.json, query);
      assertEqual(0, results.stats.scannedFull);
      assertEqual(1, results.stats.scannedIndex);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test _key
////////////////////////////////////////////////////////////////////////////////

    testUsePrimaryKeyEqNoMatches : function () {
      var query = "FOR i IN " + c.name() + " FILTER i._key == 'qux' RETURN i.value";

      var plan = AQL_EXPLAIN(query, {}, opt).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        return node.type;
      });

      assertEqual("SingletonNode", nodeTypes[0], query);
      assertTrue(
        (
          ( nodeTypes.indexOf("IndexNode") !== -1) ||
          ( nodeTypes.indexOf("SingleRemoteOperationNode") !== -1)
        ), query);

      var results = AQL_EXECUTE(query, {}, opt);
      assertEqual([ ], results.json, query);
      assertEqual(0, results.stats.scannedFull);
      assertEqual(0, results.stats.scannedIndex);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test _id
////////////////////////////////////////////////////////////////////////////////

    testUsePrimaryKeyInAttributeAccess : function () {
      var values = [ "test1", "test2", "test21", "test30" ];
      var query = "LET data = NOOPT({ ids : " + JSON.stringify(values) + " }) FOR i IN " + c.name() + " FILTER i._key IN data.ids RETURN i.value";

      var plan = AQL_EXPLAIN(query, {}, opt).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        return node.type;
      });

      assertEqual("SingletonNode", nodeTypes[0], query);
      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);

      var results = AQL_EXECUTE(query, {}, opt);
      assertEqual([ 1, 2, 21, 30 ], results.json.sort(), query);
      assertEqual(0, results.stats.scannedFull);
      assertEqual(4, results.stats.scannedIndex);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test _key
////////////////////////////////////////////////////////////////////////////////

    testUsePrimaryKey : function () {
      var values = [ "test1", "test2", "test21", "test30" ];
      var query = "FOR i IN " + c.name() + " FILTER i._key IN " + JSON.stringify(values) + " RETURN i.value";

      var plan = AQL_EXPLAIN(query, {}, opt).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        return node.type;
      });

      assertEqual("SingletonNode", nodeTypes[0], query);
      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);

      var results = AQL_EXECUTE(query, {}, opt);
      assertEqual([ 1, 2, 21, 30 ], results.json.sort(), query);
      assertEqual(0, results.stats.scannedFull);
      assertEqual(4, results.stats.scannedIndex);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test _key
////////////////////////////////////////////////////////////////////////////////

    testUsePrimaryKeyNoMatches : function () {
      var values = [ "foo", "bar", "baz", "qux" ];
      var query = "FOR i IN " + c.name() + " FILTER i._key IN " + JSON.stringify(values) + " RETURN i.value";

      var plan = AQL_EXPLAIN(query, {}, opt).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        return node.type;
      });

      assertEqual("SingletonNode", nodeTypes[0], query);
      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);

      var results = AQL_EXECUTE(query, {}, opt);
      assertEqual([ ], results.json, query);
      assertEqual(0, results.stats.scannedFull);
      assertEqual(0, results.stats.scannedIndex);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test fake _key
////////////////////////////////////////////////////////////////////////////////

    testFakeKey : function () {
      var query = "LET t = { _key: 'test12' } FOR i IN " + c.name() + " FILTER i._key == t._key RETURN i.value";

      var plan = AQL_EXPLAIN(query, {}, opt).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        return node.type;
      });

      assertEqual("SingletonNode", nodeTypes[0], query);
      assertTrue(
        (
          ( nodeTypes.indexOf("IndexNode") !== -1) ||
          ( nodeTypes.indexOf("SingleRemoteOperationNode") !== -1)
        ), query);

      var results = AQL_EXECUTE(query, {}, opt);
      assertEqual([ 12 ], results.json, query);
      assertEqual(0, results.stats.scannedFull);
      assertEqual(1, results.stats.scannedIndex);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test fake _key
////////////////////////////////////////////////////////////////////////////////

    testFakeKeyNonConst : function () {
      var query = "LET t = NOOPT({ _key: 'test12' }) FOR i IN " + c.name() + " FILTER i._key == t._key RETURN i.value";

      var plan = AQL_EXPLAIN(query, {}, opt).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        return node.type;
      });

      assertEqual("SingletonNode", nodeTypes[0], query);
      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);

      var results = AQL_EXECUTE(query, {}, opt);
      assertEqual([ 12 ], results.json, query);
      assertEqual(0, results.stats.scannedFull);
      assertEqual(1, results.stats.scannedIndex);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test fake _id
////////////////////////////////////////////////////////////////////////////////

    testFakeId : function () {
      var query = "LET t = { _id: 'test12' } FOR i IN " + c.name() + " FILTER i._key == t._id RETURN i.value";

      var plan = AQL_EXPLAIN(query, {}, opt).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        return node.type;
      });

      assertEqual("SingletonNode", nodeTypes[0], query);
      assertTrue(
        (
          ( nodeTypes.indexOf("IndexNode") !== -1) ||
          ( nodeTypes.indexOf("SingleRemoteOperationNode") !== -1)
        ), query);

      var results = AQL_EXECUTE(query, {}, opt);
      assertEqual([ 12 ], results.json, query);
      assertEqual(0, results.stats.scannedFull);
      assertEqual(1, results.stats.scannedIndex);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test fake _id
////////////////////////////////////////////////////////////////////////////////

    testFakeIdNonConst : function () {
      var query = "LET t = NOOPT({ _id: 'test12' }) FOR i IN " + c.name() + " FILTER i._key == t._id RETURN i.value";

      var plan = AQL_EXPLAIN(query, {}, opt).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        return node.type;
      });

      assertEqual("SingletonNode", nodeTypes[0], query);
      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);

      var results = AQL_EXECUTE(query, {}, opt);
      assertEqual([ 12 ], results.json, query);
      assertEqual(0, results.stats.scannedFull);
      assertEqual(1, results.stats.scannedIndex);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test fake _rev
////////////////////////////////////////////////////////////////////////////////

    testFakeRev : function () {
      var query = "LET t = { _rev: 'test12' } FOR i IN " + c.name() + " FILTER i._key == t._rev RETURN i.value";

      var plan = AQL_EXPLAIN(query, {}, opt).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        return node.type;
      });

      assertEqual("SingletonNode", nodeTypes[0], query);
      assertTrue(
        (
          ( nodeTypes.indexOf("IndexNode") !== -1) ||
          ( nodeTypes.indexOf("SingleRemoteOperationNode") !== -1)
        ), query);

      var results = AQL_EXECUTE(query, {}, opt);
      assertEqual([ 12 ], results.json, query);
      assertEqual(0, results.stats.scannedFull);
      assertEqual(1, results.stats.scannedIndex);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test fake _rev
////////////////////////////////////////////////////////////////////////////////

    testFakeRevNonConst : function () {
      var query = "LET t = NOOPT({ _rev: 'test12' }) FOR i IN " + c.name() + " FILTER i._key == t._rev RETURN i.value";

      var plan = AQL_EXPLAIN(query, {}, opt).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        return node.type;
      });

      assertEqual("SingletonNode", nodeTypes[0], query);
      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);

      var results = AQL_EXECUTE(query, {}, opt);
      assertEqual([ 12 ], results.json, query);
      assertEqual(0, results.stats.scannedFull);
      assertEqual(1, results.stats.scannedIndex);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test fake _from
////////////////////////////////////////////////////////////////////////////////

    testFakeFrom : function () {
      var query = "LET t = { _from: 'test12' } FOR i IN " + c.name() + " FILTER i._key == t._from RETURN i.value";

      var plan = AQL_EXPLAIN(query, {}, opt).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        return node.type;
      });

      assertEqual("SingletonNode", nodeTypes[0], query);
      assertTrue(
        (
          ( nodeTypes.indexOf("IndexNode") !== -1) ||
          ( nodeTypes.indexOf("SingleRemoteOperationNode") !== -1)
        ), query);

      var results = AQL_EXECUTE(query, {}, opt);
      assertEqual([ 12 ], results.json, query);
      assertEqual(0, results.stats.scannedFull);
      assertEqual(1, results.stats.scannedIndex);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test fake _from
////////////////////////////////////////////////////////////////////////////////

    testFakeFromNonConst : function () {
      var query = "LET t = NOOPT({ _from: 'test12' }) FOR i IN " + c.name() + " FILTER i._key == t._from RETURN i.value";

      var plan = AQL_EXPLAIN(query, {}, opt).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        return node.type;
      });

      assertEqual("SingletonNode", nodeTypes[0], query);
      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);

      var results = AQL_EXECUTE(query, {}, opt);
      assertEqual([ 12 ], results.json, query);
      assertEqual(0, results.stats.scannedFull);
      assertEqual(1, results.stats.scannedIndex);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test fake _to
////////////////////////////////////////////////////////////////////////////////

    testFakeTo : function () {
      var query = "LET t = { _to: 'test12' } FOR i IN " + c.name() + " FILTER i._key == t._to RETURN i.value";

      var plan = AQL_EXPLAIN(query, {}, opt).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        return node.type;
      });

      assertEqual("SingletonNode", nodeTypes[0], query);
      assertTrue(
        (
          ( nodeTypes.indexOf("IndexNode") !== -1) ||
          ( nodeTypes.indexOf("SingleRemoteOperationNode") !== -1)
        ), query);

      var results = AQL_EXECUTE(query, {}, opt);
      assertEqual([ 12 ], results.json, query);
      assertEqual(0, results.stats.scannedFull);
      assertEqual(1, results.stats.scannedIndex);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test fake _to
////////////////////////////////////////////////////////////////////////////////

    testFakeToNonConst : function () {
      var query = "LET t = NOOPT({ _to: 'test12' }) FOR i IN " + c.name() + " FILTER i._key == t._to RETURN i.value";

      var plan = AQL_EXPLAIN(query, {}, opt).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        return node.type;
      });

      assertEqual("SingletonNode", nodeTypes[0], query);
      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);

      var results = AQL_EXECUTE(query, {}, opt);
      assertEqual([ 12 ], results.json, query);
      assertEqual(0, results.stats.scannedFull);
      assertEqual(1, results.stats.scannedIndex);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testValuePropagation : function () {
      var queries = [
        "FOR i IN " + c.name() + " FOR j IN " + c.name() + " FILTER i.value == 10 && i.value == j.value RETURN i.value",
        "FOR i IN " + c.name() + " FOR j IN " + c.name() + " FILTER i.value == 10 FiLTER i.value == j.value RETURN i.value",
        "FOR i IN " + c.name() + " FOR j IN " + c.name() + " FILTER i.value == 10 FiLTER j.value == i.value RETURN i.value",
        "FOR i IN " + c.name() + " FOR j IN " + c.name() + " FILTER i.value == j.value && i.value == 10 RETURN i.value",
        "FOR i IN " + c.name() + " FOR j IN " + c.name() + " FILTER i.value == j.value FILTER i.value == 10 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value == 10 FOR j IN " + c.name() + " FILTER i.value == j.value RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value == 10 FOR j IN " + c.name() + " FILTER j.value == i.value RETURN i.value",
        "FOR i IN " + c.name() + " FOR j IN " + c.name() + " FILTER 10 == i.value && i.value == j.value RETURN i.value",
        "FOR i IN " + c.name() + " FOR j IN " + c.name() + " FILTER 10 == i.value FiLTER i.value == j.value RETURN i.value",
        "FOR i IN " + c.name() + " FOR j IN " + c.name() + " FILTER 10 == i.value FiLTER j.value == i.value RETURN i.value",
        "FOR i IN " + c.name() + " FOR j IN " + c.name() + " FILTER i.value == j.value && 10 == i.value RETURN i.value",
        "FOR i IN " + c.name() + " FOR j IN " + c.name() + " FILTER i.value == j.value FILTER 10 == i.value RETURN i.value",
        "FOR i IN " + c.name() + " FILTER 10 == i.value FOR j IN " + c.name() + " FILTER i.value == j.value RETURN i.value",
        "FOR i IN " + c.name() + " FILTER 10 == i.value FOR j IN " + c.name() + " FILTER j.value == i.value RETURN i.value"
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query, {}, opt).plan;
        var indexNodes = 0;
        plan.nodes.map(function(node) {
          if (node.type === "IndexNode") {
            ++indexNodes;
          }
        });

        assertNotEqual(-1, plan.rules.indexOf("propagate-constant-attributes"));
        assertEqual(2, indexNodes);

        var results = AQL_EXECUTE(query, {}, opt);
        assertEqual([ 10 ], results.json, query);
        assertEqual(0, results.stats.scannedFull);
        assertTrue(results.stats.scannedIndex > 0);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testValuePropagationSubquery : function () {
      var query = "FOR i IN " + c.name() + " FILTER i.value == 10 " +
                  "LET sub1 = (FOR j IN " + c.name() + " FILTER j.value == i.value RETURN j.value) " +
                  "LET sub2 = (FOR j IN " + c.name() + " FILTER j.value == i.value RETURN j.value) " +
                  "LET sub3 = (FOR j IN " + c.name() + " FILTER j.value == i.value RETURN j.value) " +
                  "RETURN [ i.value, sub1, sub2, sub3 ]";

      var plan = AQL_EXPLAIN(query, {}, opt).plan;

      assertNotEqual(-1, plan.rules.indexOf("propagate-constant-attributes"));

      var results = AQL_EXECUTE(query, {}, opt);
      assertEqual([ [ 10, [ 10 ], [ 10 ], [ 10 ] ] ], results.json, query);
      assertEqual(0, results.stats.scannedFull);
      assertTrue(results.stats.scannedIndex > 0);
    },

    testUseIndexSimpleNoDocuments : function () {
      var query = "FOR i IN " + c.name() + " FILTER i.value >= 10 SORT i.value LIMIT 10 RETURN 1";

      var plan = AQL_EXPLAIN(query, {}, opt).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        if (node.type === 'IndexNode') {
          assertFalse(node.producesResult);
        }
        return node.type;
      });

      assertEqual("SingletonNode", nodeTypes[0], query);
      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);
      assertEqual(-1, nodeTypes.indexOf("SortNode"), query);
      assertEqual("ReturnNode", nodeTypes[nodeTypes.length - 1], query);

      var results = AQL_EXECUTE(query, {}, opt);
      assertEqual([ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 ], results.json, query);
      assertEqual(0, results.stats.scannedFull);
      assertTrue(results.stats.scannedIndex > 0);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testNoValuePropagationSubquery : function () {
      var query = "LET sub1 = (FOR j IN " + c.name() + " FILTER j.value == 10 RETURN j.value) " +
                  "LET sub2 = (FOR j IN " + c.name() + " FILTER j.value == 11 RETURN j.value) " +
                  "LET sub3 = (FOR j IN " + c.name() + " FILTER j.value == 12 RETURN j.value) " +
                  "RETURN [ sub1, sub2, sub3 ]";

      var plan = AQL_EXPLAIN(query, {}, opt).plan;

      assertEqual(-1, plan.rules.indexOf("propagate-constant-attributes"));

      var results = AQL_EXECUTE(query, {}, opt);
      assertEqual([ [ [ 10 ], [ 11 ], [ 12 ] ] ], results.json, query);
      assertEqual(0, results.stats.scannedFull);
      assertTrue(results.stats.scannedIndex > 0);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testUseIndexSimple : function () {
      var query = "FOR i IN " + c.name() + " FILTER i.value >= 10 SORT i.value LIMIT 10 RETURN i.value";

      var plan = AQL_EXPLAIN(query, {}, opt).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        if (node.type === 'IndexNode') {
          assertTrue(node.producesResult);
        }
        return node.type;
      });

      assertEqual("SingletonNode", nodeTypes[0], query);
      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);
      assertEqual(-1, nodeTypes.indexOf("SortNode"), query);
      assertEqual("ReturnNode", nodeTypes[nodeTypes.length - 1], query);

      var results = AQL_EXECUTE(query, {}, opt);
      assertEqual([ 10, 11, 12, 13, 14, 15, 16, 17, 18, 19 ], results.json, query);
      assertEqual(0, results.stats.scannedFull);
      assertTrue(results.stats.scannedIndex > 0);
    },

    ////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testUseIndexJoin : function () {
      var query = "FOR i IN " + c.name() + " FILTER i.value == 8 FOR j IN " + c.name() + " FILTER i.value == j.value RETURN i.value";

      var plan = AQL_EXPLAIN(query, {}, opt).plan;
      var indexes = 0;
      plan.nodes.map(function(node) {
        if (node.type === "IndexNode") {
          ++indexes;
        }
        return node.type;
      });
      assertEqual(2, indexes);

      var results = AQL_EXECUTE(query, {}, opt);
      assertEqual([ 8 ], results.json, query);
      assertEqual(0, results.stats.scannedFull);
      assertTrue(results.stats.scannedIndex > 0);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testUseIndexJoinJoin : function () {
      var query = "FOR i IN " + c.name() + " FILTER i.value == 8 FOR j IN " + c.name() + " FILTER i._key == j._key RETURN i.value";

      var plan = AQL_EXPLAIN(query, {}, opt).plan;
      var indexes = 0;
      plan.nodes.map(function(node) {
        if (node.type === "IndexNode") {
          ++indexes;
        }
        return node.type;
      });
      assertEqual(2, indexes);

      var results = AQL_EXECUTE(query, {}, opt);
      assertEqual([ 8 ], results.json, query);
      assertEqual(0, results.stats.scannedFull);
      assertTrue(results.stats.scannedIndex > 0);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testUseIndexJoinJoinJoin : function () {
      var query = "FOR i IN " + c.name() + " FILTER i.value == 8 FOR j IN " + c.name() + " FILTER i.value == j.value FOR k IN " + c.name() + " FILTER j.value == k.value RETURN i.value";

      var plan = AQL_EXPLAIN(query, {}, opt).plan;
      var indexes = 0;
      plan.nodes.map(function(node) {
        if (node.type === "IndexNode") {
          ++indexes;
        }
        return node.type;
      });
      assertEqual(3, indexes);

      var results = AQL_EXECUTE(query, {}, opt);
      assertEqual([ 8 ], results.json, query);
      assertEqual(0, results.stats.scannedFull);
      assertTrue(results.stats.scannedIndex > 0);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testUseIndexSubquery : function () {
      var query = "LET results = (FOR i IN " + c.name() + " FILTER i.value >= 10 SORT i.value LIMIT 10 RETURN i.value) RETURN results";

      var plan = AQL_EXPLAIN(query, {}, opt).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        return node.type;
      });

      assertEqual("SingletonNode", nodeTypes[0], query);
      assertEqual("SubqueryStartNode", nodeTypes[1], query);
      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);
      assertEqual(-1, nodeTypes.indexOf("SortNode"), query);
      assertEqual("ReturnNode", nodeTypes[nodeTypes.length - 1], query);

      var results = AQL_EXECUTE(query, {}, opt);
      assertEqual([ 10, 11, 12, 13, 14, 15, 16, 17, 18, 19 ], results.json[0], query);
      assertEqual(0, results.stats.scannedFull);
      assertTrue(results.stats.scannedIndex > 0);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testUseIndexSplicedSubSubquery : function () {
      const query = `
        FOR i IN ${c.name()}
          LIMIT 1
          RETURN (
            FOR j IN ${c.name()}
              FILTER j._key == i._key
              RETURN j.value
          )
      `;

      const plan = AQL_EXPLAIN(query, {}, opt).plan;
      const nodeTypes = plan.nodes.map(function(node) {
        return node.type;
      });

      assertEqual('SingletonNode', nodeTypes[0], query);
      const subqueryStartIdx = nodeTypes.indexOf('SubqueryStartNode');
      const subqueryEndIdx = nodeTypes.indexOf('SubqueryEndNode');
      const hasSplicedSubquery = subqueryStartIdx !== -1 && subqueryEndIdx !== -1;
      assertTrue(hasSplicedSubquery, JSON.stringify({
        subqueryStartIdx,
        subqueryEndIdx,
        query,
      }));

      const subNodeTypes = plan.nodes.slice(subqueryStartIdx+1, subqueryEndIdx).map(function(node) {
        return node.type;
      });
      assertNotEqual(-1, subNodeTypes.indexOf('IndexNode'), query);
      assertEqual(-1, subNodeTypes.indexOf('SortNode'), query);
      assertEqual('ReturnNode', nodeTypes[nodeTypes.length - 1], query);

      const results = AQL_EXECUTE(query, {}, opt);
      assertTrue(results.stats.scannedFull > 0); // for the outer query
      assertTrue(results.stats.scannedIndex > 0); // for the inner query
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testMultipleSubqueries : function () {
      var query = "LET a = (FOR x IN " + c.name() + " FILTER x._key == 'test1' RETURN x._key) " +
                  "LET b = (FOR x IN " + c.name() + " FILTER x._key == 'test2' RETURN x._key) " +
                  "LET c = (FOR x IN " + c.name() + " FILTER x._key == 'test3' RETURN x._key) " +
                  "LET d = (FOR x IN " + c.name() + " FILTER x._key == 'test4' RETURN x._key) " +
                  "LET e = (FOR x IN " + c.name() + " FILTER x._key == 'test5' RETURN x._key) " +
                  "LET f = (FOR x IN " + c.name() + " FILTER x._key == 'test6' RETURN x._key) " +
                  "LET g = (FOR x IN " + c.name() + " FILTER x._key == 'test7' RETURN x._key) " +
                  "LET h = (FOR x IN " + c.name() + " FILTER x._key == 'test8' RETURN x._key) " +
                  "LET i = (FOR x IN " + c.name() + " FILTER x._key == 'test9' RETURN x._key) " +
                  "LET j = (FOR x IN " + c.name() + " FILTER x._key == 'test10' RETURN x._key) " +
                  "RETURN [ a, b, c, d, e, f, g, h, i, j ]";


      var explain = AQL_EXPLAIN(query, {}, opt);
      var plan = explain.plan;

      var walker = function (nodes, func) {
        nodes.forEach(function(node) {
          func(node);
        });
      };

      var indexNodes = 0, collectionNodes = 0;
      walker(plan.nodes, function (node) {
        if (node.type === "IndexNode") {
          ++indexNodes;
        }
        else if (node.type === "EnumerateCollectionNode") {
          ++collectionNodes;
        }
      });

      assertEqual(0, collectionNodes);
      assertEqual(10, indexNodes);
      assertEqual(1, explain.stats.plansCreated);

      var results = AQL_EXECUTE(query, {}, opt);
      assertEqual(0, results.stats.scannedFull);
      assertNotEqual(0, results.stats.scannedIndex);
      assertEqual([ [ [ 'test1' ], [ 'test2' ], [ 'test3' ], [ 'test4' ], [ 'test5' ], [ 'test6' ], [ 'test7' ], [ 'test8' ], [ 'test9' ], [ 'test10' ] ] ], results.json);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testMultipleSubqueriesMultipleIndexes : function () {
      idx1 = c.ensureHashIndex("value"); // now we have a hash and a skiplist index
      var query = "LET a = (FOR x IN " + c.name() + " FILTER x.value == 1 RETURN x._key) " +
                  "LET b = (FOR x IN " + c.name() + " FILTER x.value == 2 RETURN x._key) " +
                  "LET c = (FOR x IN " + c.name() + " FILTER x.value == 3 RETURN x._key) " +
                  "LET d = (FOR x IN " + c.name() + " FILTER x.value == 4 RETURN x._key) " +
                  "LET e = (FOR x IN " + c.name() + " FILTER x.value == 5 RETURN x._key) " +
                  "LET f = (FOR x IN " + c.name() + " FILTER x.value == 6 RETURN x._key) " +
                  "LET g = (FOR x IN " + c.name() + " FILTER x.value == 7 RETURN x._key) " +
                  "LET h = (FOR x IN " + c.name() + " FILTER x.value == 8 RETURN x._key) " +
                  "LET i = (FOR x IN " + c.name() + " FILTER x.value == 9 RETURN x._key) " +
                  "LET j = (FOR x IN " + c.name() + " FILTER x.value == 10 RETURN x._key) " +
                  "RETURN [ a, b, c, d, e, f, g, h, i, j ]";


      var explain = AQL_EXPLAIN(query, {}, opt);
      var plan = explain.plan;

      var walker = function (nodes, func) {
        nodes.forEach(function(node) {
          func(node);
        });
      };

      var indexNodes = 0, collectionNodes = 0;
      walker(plan.nodes, function (node) {
        if (node.type === "IndexNode") {
          ++indexNodes;
          assertNotEqual(["hash", "skiplist", "persistent"].indexOf(node.indexes[0].type), -1);
        }
        else if (node.type === "EnumerateCollectionNode") {
          ++collectionNodes;
        }
      });

      assertEqual(0, collectionNodes);
      assertEqual(10, indexNodes);
      assertEqual(1, explain.stats.plansCreated);

      var results = AQL_EXECUTE(query, {}, opt);
      assertEqual(0, results.stats.scannedFull);
      assertNotEqual(0, results.stats.scannedIndex);
      assertEqual([ [ [ 'test1' ], [ 'test2' ], [ 'test3' ], [ 'test4' ], [ 'test5' ], [ 'test6' ], [ 'test7' ], [ 'test8' ], [ 'test9' ], [ 'test10' ] ] ], results.json);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testMultipleSubqueriesHashIndexes : function () {
      deleteDefaultIdx(); // drop skiplist index
      idx1 = c.ensureHashIndex("value");
      var query = "LET a = (FOR x IN " + c.name() + " FILTER x.value == 1 RETURN x._key) " +
                  "LET b = (FOR x IN " + c.name() + " FILTER x.value == 2 RETURN x._key) " +
                  "LET c = (FOR x IN " + c.name() + " FILTER x.value == 3 RETURN x._key) " +
                  "LET d = (FOR x IN " + c.name() + " FILTER x.value == 4 RETURN x._key) " +
                  "LET e = (FOR x IN " + c.name() + " FILTER x.value == 5 RETURN x._key) " +
                  "LET f = (FOR x IN " + c.name() + " FILTER x.value == 6 RETURN x._key) " +
                  "LET g = (FOR x IN " + c.name() + " FILTER x.value == 7 RETURN x._key) " +
                  "LET h = (FOR x IN " + c.name() + " FILTER x.value == 8 RETURN x._key) " +
                  "LET i = (FOR x IN " + c.name() + " FILTER x.value == 9 RETURN x._key) " +
                  "LET j = (FOR x IN " + c.name() + " FILTER x.value == 10 RETURN x._key) " +
                  "RETURN [ a, b, c, d, e, f, g, h, i, j ]";

      var explain = AQL_EXPLAIN(query, {}, opt);
      var plan = explain.plan;

      var walker = function (nodes, func) {
        nodes.forEach(function(node) {
          func(node);
        });
      };

      var indexNodes = 0, collectionNodes = 0;
      walker(plan.nodes, function (node) {
        if (node.type === "IndexNode") {
          ++indexNodes;
          assertEqual("hash", node.indexes[0].type);
        }
        else if (node.type === "EnumerateCollectionNode") {
          ++collectionNodes;
        }
      });

      assertEqual(0, collectionNodes);
      assertEqual(10, indexNodes);
      assertEqual(1, explain.stats.plansCreated);

      var results = AQL_EXECUTE(query, {}, opt);
      assertEqual(0, results.stats.scannedFull);
      assertNotEqual(0, results.stats.scannedIndex);
      assertEqual([ [ [ 'test1' ], [ 'test2' ], [ 'test3' ], [ 'test4' ], [ 'test5' ], [ 'test6' ], [ 'test7' ], [ 'test8' ], [ 'test9' ], [ 'test10' ] ] ], results.json);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testJoinMultipleIndexes : function () {
      idx1 = c.ensureHashIndex("value"); // now we have a hash and a skiplist index
      var query = "FOR i IN " + c.name() + " FILTER i.value < 10 FOR j IN " + c.name() + " FILTER j.value == i.value RETURN j._key";

      var explain = AQL_EXPLAIN(query, {}, opt);
      var plan = explain.plan;

      var collectionNodes = 0, indexNodes = 0;
      plan.nodes.forEach(function(node) {
        if (node.type === "IndexNode") {
          ++indexNodes;
          if (indexNodes === 1) {
            // skiplist must be used for the first FOR
            assertNotEqual(["hash", "skiplist", "persistent"].indexOf(node.indexes[0].type), -1);
            assertEqual("i", node.outVariable.name);
          }
          else {
            assertEqual("j", node.outVariable.name);
          }
        }
        else if (node.type === "EnumerateCollectionNode") {
          ++collectionNodes;
        }
      });

      assertEqual(0, collectionNodes);
      assertEqual(2, indexNodes);
      assertEqual(1, explain.stats.plansCreated);

      var results = AQL_EXECUTE(query, {}, opt);
      assertEqual(0, results.stats.scannedFull);
      assertNotEqual(0, results.stats.scannedIndex);
      assertEqual([ 'test0', 'test1', 'test2', 'test3', 'test4', 'test5', 'test6', 'test7', 'test8', 'test9' ], results.json);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testJoinRangesMultipleIndexes : function () {
      idx1 = c.ensureHashIndex("value"); // now we have a hash and a skiplist index
      var query = "FOR i IN " + c.name() + " FILTER i.value < 5 FOR j IN " + c.name() + " FILTER j.value < i.value RETURN j._key";

      var explain = AQL_EXPLAIN(query, {}, opt);
      var plan = explain.plan;

      var collectionNodes = 0, indexNodes = 0;
      plan.nodes.forEach(function(node) {
        if (node.type === "IndexNode") {
          ++indexNodes;
          // skiplist must be used for both FORs
          assertEqual("skiplist", node.indexes[0].type);
          if (indexNodes === 1) {
            assertEqual("i", node.outVariable.name);
          }
          else {
            assertEqual("j", node.outVariable.name);
          }
        }
        else if (node.type === "EnumerateCollectionNode") {
          ++collectionNodes;
        }
      });

      assertEqual(0, collectionNodes);
      assertEqual(2, indexNodes);
      assertEqual(1, explain.stats.plansCreated);

      var results = AQL_EXECUTE(query, {}, opt);
      assertEqual(0, results.stats.scannedFull);
      assertNotEqual(0, results.stats.scannedIndex);
      assertEqual([ 'test0', 'test0', 'test1', 'test0', 'test1', 'test2', 'test0', 'test1', 'test2', 'test3' ], results.json);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testTripleJoin : function () {
      idx1 = c.ensureHashIndex("value"); // now we have a hash and a skiplist index
      var query = "FOR i IN " + c.name() + " FILTER i.value == 4 FOR j IN " + c.name() + " FILTER j.value == i.value FOR k IN " + c.name() + " FILTER k.value < j.value RETURN k._key";

      var explain = AQL_EXPLAIN(query, {}, opt);
      var plan = explain.plan;

      var collectionNodes = 0, indexNodes = 0;
      plan.nodes.forEach(function(node) {
        if (node.type === "IndexNode") {
          ++indexNodes;
          if (indexNodes === 1) {
            assertNotEqual(["hash", "skiplist", "persistent"].indexOf(node.indexes[0].type), -1);
            assertEqual("i", node.outVariable.name);
          }
          else if (indexNodes === 2) {
            assertNotEqual(["hash", "skiplist", "persistent"].indexOf(node.indexes[0].type), -1);
            assertEqual("j", node.outVariable.name);
          }
          else {
            assertEqual("skiplist", node.indexes[0].type);
            assertEqual("k", node.outVariable.name);
          }
        }
        else if (node.type === "EnumerateCollectionNode") {
          ++collectionNodes;
        }
      });

      assertEqual(0, collectionNodes);
      assertEqual(3, indexNodes);
      assertEqual(1, explain.stats.plansCreated);

      var results = AQL_EXECUTE(query, {}, opt);
      assertEqual(0, results.stats.scannedFull);
      assertNotEqual(0, results.stats.scannedIndex);
      assertEqual([ 'test0', 'test1', 'test2', 'test3' ], results.json);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testSubqueryMadness : function () {
      idx1 = c.ensureHashIndex("value"); // now we have a hash and a skiplist index
      var query = "LET a = (FOR x IN " + c.name() + " FILTER x.value == 1 FOR y IN " + c.name() + " FILTER y.value == x.value RETURN x._key) " +
                  "LET b = (FOR x IN " + c.name() + " FILTER x.value == 2 FOR y IN " + c.name() + " FILTER y.value == x.value RETURN x._key) " +
                  "LET c = (FOR x IN " + c.name() + " FILTER x.value == 3 FOR y IN " + c.name() + " FILTER y.value == x.value RETURN x._key) " +
                  "LET d = (FOR x IN " + c.name() + " FILTER x.value == 4 FOR y IN " + c.name() + " FILTER y.value == x.value RETURN x._key) " +
                  "LET e = (FOR x IN " + c.name() + " FILTER x.value == 5 FOR y IN " + c.name() + " FILTER y.value == x.value RETURN x._key) " +
                  "LET f = (FOR x IN " + c.name() + " FILTER x.value == 6 FOR y IN " + c.name() + " FILTER y.value == x.value RETURN x._key) " +
                  "LET g = (FOR x IN " + c.name() + " FILTER x.value == 7 FOR y IN " + c.name() + " FILTER y.value == x.value RETURN x._key) " +
                  "LET h = (FOR x IN " + c.name() + " FILTER x.value == 8 FOR y IN " + c.name() + " FILTER y.value == x.value RETURN x._key) " +
                  "LET i = (FOR x IN " + c.name() + " FILTER x.value == 9 FOR y IN " + c.name() + " FILTER y.value == x.value RETURN x._key) " +
                  "LET j = (FOR x IN " + c.name() + " FILTER x.value == 10 FOR y IN " + c.name() + " FILTER y.value == x.value RETURN x._key) " +
                  "RETURN [ a, b, c, d, e, f, g, h, i, j ]";

      var explain = AQL_EXPLAIN(query, {}, opt);
      var plan = explain.plan;

      var walker = function (nodes, func) {
        nodes.forEach(function(node) {
          func(node);
        });
      };

      var indexNodes = 0, collectionNodes = 0;

      walker(plan.nodes, function (node) {
        if (node.type === "IndexNode") {
          ++indexNodes;
          assertNotEqual(["hash", "skiplist", "persistent"].indexOf(node.indexes[0].type), -1);
        }
        else if (node.type === "EnumerateCollectionNode") {
          ++collectionNodes;
        }
      });

      assertEqual(0, collectionNodes);
      assertEqual(20, indexNodes);
      assertEqual(1, explain.stats.plansCreated);

      var results = AQL_EXECUTE(query, {}, opt);
      assertEqual(0, results.stats.scannedFull);
      assertNotEqual(0, results.stats.scannedIndex);
      assertEqual([ [ [ 'test1' ], [ 'test2' ], [ 'test3' ], [ 'test4' ], [ 'test5' ], [ 'test6' ], [ 'test7' ], [ 'test8' ], [ 'test9' ], [ 'test10' ] ] ], results.json);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testIndexOrPrimary : function () {
      var query = "FOR i IN " + c.name() + " FILTER i._key == 'test1' || i._key == 'test9' RETURN i.value";

      var plan = AQL_EXPLAIN(query, {}, opt).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        if (node.type === "IndexNode") {
          assertEqual("primary", node.indexes[0].type);
        }
        return node.type;
      });

      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);

      var results = AQL_EXECUTE(query, {}, opt);
      assertEqual([ 1, 9 ], results.json.sort(), query);
      assertEqual(2, results.stats.scannedIndex);
      assertEqual(0, results.stats.scannedFull);
    },

    testIndexOrHashNoDocuments : function () {
      idx1 = c.ensureHashIndex("value");
      var query = "FOR i IN " + c.name() + " FILTER i.value == 1 || i.value == 9 RETURN 1";

      var plan = AQL_EXPLAIN(query, {}, opt).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        if (node.type === "IndexNode") {
          assertFalse(node.producesResult);
          assertNotEqual(["hash", "skiplist", "persistent"].indexOf(node.indexes[0].type), -1);
          assertFalse(node.indexes[0].unique);
        }
        return node.type;
      });

      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);

      var results = AQL_EXECUTE(query, {}, opt);
      assertEqual([ 1, 1 ], results.json, query);
      assertEqual(2, results.stats.scannedIndex);
      assertEqual(0, results.stats.scannedFull);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testPrimaryIndexDynamic : function () {
      var queries = [
        [ "LET a = NOOPT('test35') FOR i IN " + c.name() + " FILTER i._key IN [ a ] RETURN i.value", [ 35 ] ],
        [ "FOR i IN " + c.name() + " FILTER i._key IN [ NOOPT('test35') ] RETURN i.value", [ 35 ] ],
        [ "LET a = NOOPT('test35') FOR i IN " + c.name() + " FILTER i._key IN [ a, a, a ] RETURN i.value", [ 35 ] ],
        [ "LET a = NOOPT('test35') FOR i IN " + c.name() + " FILTER i._key IN [ 'test35', 'test36' ] RETURN i.value", [ 35, 36 ] ],
        [ "LET a = NOOPT('test35'), b = NOOPT('test36'), c = NOOPT('test-9') FOR i IN " + c.name() + " FILTER i._key IN [ b, b, a, b, c ] RETURN i.value", [ 35, 36 ] ],
        [ "LET a = NOOPT('test35'), b = NOOPT('test36'), c = NOOPT('test37') FOR i IN " + c.name() + " FILTER i._key IN [ a, b, c ] RETURN i.value", [ 35, 36, 37 ] ],
        [ "LET a = NOOPT('test35'), b = NOOPT('test36'), c = NOOPT('test37') FOR i IN " + c.name() + " FILTER i._key IN [ a ] || i._key IN [ b, c ] RETURN i.value", [ 35, 36, 37 ] ],
        [ "LET a = NOOPT('test35'), b = NOOPT('test36'), c = NOOPT('test37'), d = NOOPT('test35') FOR i IN " + c.name() + " FILTER i._key IN [ a, b, c, d ] || i._key IN [ a, b, c, d ] RETURN i.value", [ 35, 36, 37 ] ],
        [ "LET a = NOOPT('test35') FOR i IN " + c.name() + " FILTER i._key == a RETURN i.value", [ 35 ] ],
        [ "LET a = NOOPT('test35') FOR i IN " + c.name() + " FILTER i._key == a || i._key == a RETURN i.value", [ 35 ] ],
        [ "LET a = NOOPT('test35'), b = NOOPT('test36') FOR i IN " + c.name() + " FILTER i._key == a || i._key == b RETURN i.value", [ 35, 36 ] ],
        [ "LET a = NOOPT('test35'), b = NOOPT('test36'), c = NOOPT('test37'), d = NOOPT('test35') FOR i IN " + c.name() + " FILTER i._key == a || i._key == b || i._key == c || i._key == d RETURN i.value", [ 35, 36, 37 ] ]
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query[0]).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        // ensure the index is used
        assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);

        var results = AQL_EXECUTE(query[0]);
        results.json.forEach(function(value) {
          assertNotEqual(-1, query[1].indexOf(value), query);
        });

        assertTrue(results.stats.scannedIndex < 10);
        assertEqual(0, results.stats.scannedFull);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testIndexOrHash : function () {
      idx1 = c.ensureHashIndex("value");
      var query = "FOR i IN " + c.name() + " FILTER i.value == 1 || i.value == 9 RETURN i.value";

      var plan = AQL_EXPLAIN(query, {}, opt).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        if (node.type === "IndexNode") {
          assertTrue(node.producesResult);
          assertNotEqual(["hash", "skiplist", "persistent"].indexOf(node.indexes[0].type), -1);
          assertFalse(node.indexes[0].unique);
        }
        return node.type;
      });

      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);

      var results = AQL_EXECUTE(query, {}, opt);
      assertEqual([ 1, 9 ], results.json.sort(), query);
      assertEqual(2, results.stats.scannedIndex);
      assertEqual(0, results.stats.scannedFull);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testIndexOrUniqueHash : function () {
      idx1 = c.ensureUniqueConstraint("value");
      var query = "FOR i IN " + c.name() + " FILTER i.value == 1 || i.value == 9 RETURN i.value";

      var plan = AQL_EXPLAIN(query, {}, opt).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        if (node.type === "IndexNode") {
          assertEqual("hash", node.indexes[0].type);
          assertTrue(node.indexes[0].unique);
        }
        return node.type;
      });

      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);

      var results = AQL_EXECUTE(query, {}, opt);
      assertEqual([ 1, 9 ], results.json.sort(), query);
      assertEqual(2, results.stats.scannedIndex);
      assertEqual(0, results.stats.scannedFull);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testIndexOrMultiplySkiplist : function () {
      var query = "FOR i IN " + c.name() + " FILTER (i.value > 1 && i.value < 9) && (i.value2 == null || i.value3 == null) RETURN i.value";

      var plan = AQL_EXPLAIN(query, {}, opt).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        if (node.type === "IndexNode") {
          assertTrue(node.producesResult);
          assertEqual("skiplist", node.indexes[0].type);
          assertFalse(node.indexes[0].unique);
        }
        return node.type;
      });

      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);

      var results = AQL_EXECUTE(query, {}, opt);
      assertEqual([ 2, 3, 4, 5, 6, 7, 8 ], results.json.sort(), query);
      assertTrue(results.stats.scannedIndex > 0);
      assertEqual(0, results.stats.scannedFull);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testIndexOrSkiplist : function () {
      var query = "FOR i IN " + c.name() + " FILTER i.value == 1 || i.value == 9 RETURN i.value";

      var plan = AQL_EXPLAIN(query, {}, opt).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        if (node.type === "IndexNode") {
          assertTrue(node.producesResult);
          assertEqual("skiplist", node.indexes[0].type);
          assertFalse(node.indexes[0].unique);
        }
        return node.type;
      });

      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);

      var results = AQL_EXECUTE(query, {}, opt);
      assertEqual([ 1, 9 ], results.json.sort(), query);
      assertEqual(2, results.stats.scannedIndex);
      assertEqual(0, results.stats.scannedFull);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testIndexOrUniqueSkiplist : function () {
      idx1 = c.ensureUniqueSkiplist("value");
      var query = "FOR i IN " + c.name() + " FILTER i.value == 1 || i.value == 9 RETURN i.value";

      var plan = AQL_EXPLAIN(query, {}, opt).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        if (node.type === "IndexNode") {
          assertEqual("skiplist", node.indexes[0].type);
          assertTrue(node.indexes[0].unique);
        }
        return node.type;
      });

      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);

      var results = AQL_EXECUTE(query, {}, opt);
      assertEqual([ 1, 9 ], results.json.sort(), query);
      assertEqual(2, results.stats.scannedIndex);
      assertEqual(0, results.stats.scannedFull);
    },
////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testIndexAndDespiteFunctions : function () {
      var queries = [
        "FOR i IN " + c.name() + " FILTER i.value == 1 && RAND() >= -1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value == 1 && RAND() >= -1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value == 1 && RAND() != -1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value == 1 && RAND() <= 10 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value == 1 && RAND() < 10 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value == 1 && NOOPT(true) == true RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value == 1 && 1 + NOOPT(2) == 3 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value == 1 && 1 + NOOPT(2) RETURN i.value",
        "FOR i IN " + c.name() + " FILTER RAND() >= -1 && i.value == 1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER RAND() >= -1 && i.value == 1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER RAND() != -1 && i.value == 1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER RAND() <= 10 && i.value == 1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER RAND() < 10 && i.value == 1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER 1 + NOOPT(2) == 3 && i.value == 1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER 1 + NOOPT(2) && i.value == 1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value IN [ 1 ] && RAND() >= -1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER RAND() >= -1 FILTER i.value IN [ 1 ] RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value == 1 FILTER RAND() >= -1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value == 1 FILTER RAND() >= -1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value == 1 FILTER RAND() != -1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value == 1 FILTER RAND() <= 10 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value == 1 FILTER RAND() < 10 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value == 1 FILTER NOOPT(true) == true RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value == 1 FILTER 1 + NOOPT(2) == 3 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value == 1 FILTER 1 + NOOPT(2) RETURN i.value",
        "FOR i IN " + c.name() + " FILTER RAND() >= -1 FILTER i.value == 1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER RAND() >= -1 FILTER i.value == 1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER RAND() != -1 FILTER i.value == 1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER RAND() <= 10 FILTER i.value == 1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER RAND() < 10 FILTER i.value == 1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER 1 + NOOPT(2) == 3 FILTER i.value == 1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER 1 + NOOPT(2) FILTER i.value == 1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value IN [ 1 ] FILTER RAND() >= -1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER RAND() >= -1 FILTER i.value IN [ 1 ] RETURN i.value"
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query, {}, opt).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);
        var results = AQL_EXECUTE(query, {}, opt);
        assertEqual(1, results.json.length);
        assertTrue(results.stats.scannedIndex > 0);
        assertEqual(0, results.stats.scannedFull);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testIndexAndDespiteReferences : function () {
      var queries = [
        "LET a = 1 FOR i IN " + c.name() + " FILTER i.value == 1 && a == 1 RETURN i.value",
        "LET a = 1 FOR i IN " + c.name() + " FILTER a == 1 && i.value == 1 RETURN i.value",
        "LET a = NOOPT(1) FOR i IN " + c.name() + " FILTER i.value == 1 && a == 1 RETURN i.value",
        "LET a = NOOPT(1) FOR i IN " + c.name() + " FILTER a == 1 && i.value == 1 RETURN i.value",
        "LET a = { value: 1 } FOR i IN " + c.name() + " FILTER i.value == 1 && a.value == 1 RETURN i.value",
        "LET a = { value: 1 } FOR i IN " + c.name() + " FILTER a.value == 1 && i.value == 1 RETURN i.value",
        "LET a = NOOPT({ value: 1 }) FOR i IN " + c.name() + " FILTER i.value == 1 && a.value == 1 RETURN i.value",
        "LET a = NOOPT({ value: 1 }) FOR i IN " + c.name() + " FILTER a.value == 1 && i.value == 1 RETURN i.value",
        "LET sub = (FOR x IN [ 1, 2, 3 ] RETURN x) FOR i IN " + c.name() + " FILTER i.value == 1 && (1 IN sub) RETURN i.value",
        "LET sub = (FOR x IN [ 1, 2, 3 ] RETURN x) FOR i IN " + c.name() + " FILTER (1 IN sub) && i.value == 1 RETURN i.value",
        "FOR j IN 1..1 FOR i IN " + c.name() + " FILTER i.value == 1 && j == 1 RETURN i.value",
        "FOR j IN 1..1 FOR i IN " + c.name() + " FILTER j == 1 && i.value == 1 RETURN i.value",
        "FOR j IN [ { value: 1 } ] FOR i IN " + c.name() + " FILTER i.value == 1 && j.value == 1 RETURN i.value",
        "FOR j IN [ { value: 1 } ] FOR i IN " + c.name() + " FILTER j.value == 1 && i.value == 1 RETURN i.value",
        "LET a = 1 FOR i IN " + c.name() + " FILTER i.value == 1 FILTER a == 1 RETURN i.value",
        "LET a = 1 FOR i IN " + c.name() + " FILTER a == 1 FILTER i.value == 1 RETURN i.value",
        "LET a = NOOPT(1) FOR i IN " + c.name() + " FILTER i.value == 1 FILTER a == 1 RETURN i.value",
        "LET a = NOOPT(1) FOR i IN " + c.name() + " FILTER a == 1 FILTER i.value == 1 RETURN i.value",
        "LET a = { value: 1 } FOR i IN " + c.name() + " FILTER i.value == 1 FILTER a.value == 1 RETURN i.value",
        "LET a = { value: 1 } FOR i IN " + c.name() + " FILTER a.value == 1 FILTER i.value == 1 RETURN i.value",
        "LET a = NOOPT({ value: 1 }) FOR i IN " + c.name() + " FILTER i.value == 1 FILTER a.value == 1 RETURN i.value",
        "LET a = NOOPT({ value: 1 }) FOR i IN " + c.name() + " FILTER a.value == 1 FILTER i.value == 1 RETURN i.value",
        "LET sub = (FOR x IN [ 1, 2, 3 ] RETURN x) FOR i IN " + c.name() + " FILTER i.value == 1 FILTER (1 IN sub) RETURN i.value",
        "LET sub = (FOR x IN [ 1, 2, 3 ] RETURN x) FOR i IN " + c.name() + " FILTER (1 IN sub) FILTER i.value == 1 RETURN i.value",
        "FOR j IN 1..1 FOR i IN " + c.name() + " FILTER i.value == 1 FILTER j == 1 RETURN i.value",
        "FOR j IN 1..1 FOR i IN " + c.name() + " FILTER j == 1 FILTER i.value == 1 RETURN i.value",
        "FOR j IN [ { value: 1 } ] FOR i IN " + c.name() + " FILTER i.value == 1 FILTER j.value == 1 RETURN i.value",
        "FOR j IN [ { value: 1 } ] FOR i IN " + c.name() + " FILTER j.value == 1 FILTER i.value == 1 RETURN i.value"
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query, {}, opt).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);
        var results = AQL_EXECUTE(query, {}, opt);
        assertEqual(1, results.json.length);
        assertTrue(results.stats.scannedIndex > 0);
        assertEqual(0, results.stats.scannedFull);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testIndexAndDespiteOtherCollection : function () {
      var queries = [
        "FOR j IN " + c.name() + " FOR i IN " + c.name() + " FILTER i.value == 1 && j.value == 1 LIMIT 2000 RETURN i.value",
        "FOR j IN " + c.name() + " FOR i IN " + c.name() + " FILTER j.value == 1 && i.value == 1 LIMIT 2000 RETURN i.value",
        "FOR j IN " + c.name() + " FOR i IN " + c.name() + " FILTER i.value == 1 FILTER j.value == 1 LIMIT 2000 RETURN i.value",
        "FOR j IN " + c.name() + " FOR i IN " + c.name() + " FILTER j.value == 1 FILTER i.value == 1 LIMIT 2000 RETURN i.value"
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query, {}, opt).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);
        var results = AQL_EXECUTE(query, {}, opt);
        assertEqual(1, results.json.length);
        assertTrue(results.stats.scannedIndex > 0);
        assertEqual(0, results.stats.scannedFull);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testIndexAndDespiteOperators : function () {
      var queries = [
        "LET a = NOOPT(1) FOR i IN " + c.name() + " FILTER i.value == 1 && a != 2 RETURN i.value",
        "LET a = NOOPT(1) FOR i IN " + c.name() + " FILTER a != 2 && i.value == 1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value == 1 && i.value != 2 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value != 2 && i.value == 1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value == 1 && i.value - 1 == 0 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value - 1 == 0 && i.value == 1 RETURN i.value",
        "LET a = NOOPT(0) FOR i IN " + c.name() + " FILTER i.value == 1 && a + 1 == 1 RETURN i.value",
        "LET a = NOOPT(0) FOR i IN " + c.name() + " FILTER a + 1 == 1 && i.value == 1 RETURN i.value",
        "LET a = NOOPT(0) FOR i IN " + c.name() + " FILTER i.value == 1 && a IN [ 0, 1 ] RETURN i.value",
        "LET a = NOOPT(0) FOR i IN " + c.name() + " FILTER a IN [ 0, 1 ] && i.value == 1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value == 1 && NOT (i.value != 1) RETURN i",
        "FOR i IN " + c.name() + " FILTER i.value == 1 || NOT NOT (i.value == 1) RETURN i",
        "LET a = NOOPT(1) FOR i IN " + c.name() + " FILTER i.value == 1 FILTER a != 2 RETURN i.value",
        "LET a = NOOPT(1) FOR i IN " + c.name() + " FILTER a != 2 FILTER i.value == 1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value == 1 FILTER i.value != 2 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value != 2 FILTER i.value == 1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value == 1 FILTER i.value - 1 == 0 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value - 1 == 0 FILTER i.value == 1 RETURN i.value",
        "LET a = NOOPT(0) FOR i IN " + c.name() + " FILTER i.value == 1 FILTER a + 1 == 1 RETURN i.value",
        "LET a = NOOPT(0) FOR i IN " + c.name() + " FILTER a + 1 == 1 FILTER i.value == 1 RETURN i.value",
        "LET a = NOOPT(0) FOR i IN " + c.name() + " FILTER i.value == 1 FILTER a IN [ 0, 1 ] RETURN i.value",
        "LET a = NOOPT(0) FOR i IN " + c.name() + " FILTER a IN [ 0, 1 ] FILTER i.value == 1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value == 3 && (i.value != 1 || i.value != 2) RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value == 3 FILTER (i.value != 1 || i.value != 2) RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value == 1 FILTER NOT (i.value != 1) RETURN i",
        "FOR i IN " + c.name() + " FILTER (i.value != 1 || i.value != 2) && i.value == 3 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER (i.value != 1 || i.value != 2) FILTER i.value == 3 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER (i.value2 != 1 || i.value2 != 2) && i.value == 3 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER (i.value2 != 1 || i.value2 != 2) FILTER i.value == 3 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value == 3 && (i.value2 != 1 || i.value2 != 2) RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value == 3 FILTER (i.value2 != 1 || i.value2 != 2) RETURN i.value"
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query, {}, opt).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);
        var results = AQL_EXECUTE(query, {}, opt);
        assertEqual(1, results.json.length);
        assertTrue(results.stats.scannedIndex > 0);
        assertEqual(0, results.stats.scannedFull);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testIndexAndDespiteOr : function () {
      AQL_EXECUTE("FOR i IN " + c.name() + " UPDATE i WITH { value2: i.value, value3: 1 } IN " + c.name());

      var queries = [
        "FOR i IN " + c.name() + " FILTER (i.value == 1 && i.value2 == 1) || (i.value == 2 && i.value2 == 2) RETURN i.value",
        "FOR i IN " + c.name() + " FILTER (i.value2 == 1 && i.value == 1) || (i.value2 == 2 && i.value == 2) RETURN i.value",
        "FOR i IN " + c.name() + " FILTER (i.value2 == 1 && i.value == 1) || (i.value3 != 2 && i.value == 2) RETURN i.value",
        "FOR i IN " + c.name() + " FILTER (i.value == 1 && i.value2 == 1) || (i.value == 2 && i.value3 != 2) RETURN i.value",
        "FOR i IN " + c.name() + " FILTER (i.value2 == 1 && i.value == 1) || (i.value3 == 1 && i.value == 2) RETURN i.value"
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query, {}, opt).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);
        var results = AQL_EXECUTE(query, {}, opt);
        assertEqual(2, results.json.length, query);
        assertTrue(results.stats.scannedIndex > 0);
        assertEqual(0, results.stats.scannedFull);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testIndexAndDespiteOperatorsEmpty : function () {
      var queries = [
        "LET a = NOOPT(1) FOR i IN " + c.name() + " FILTER i.value == 1 && a != 1 RETURN i.value",
        "LET a = NOOPT(1) FOR i IN " + c.name() + " FILTER a != 1 && i.value == 1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value == 1 && i.value != 1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value != 1 && i.value == 1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value == 1 && i.value - 1 == 1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value - 1 == 1 && i.value == 1 RETURN i.value",
        "LET a = NOOPT(0) FOR i IN " + c.name() + " FILTER i.value == 1 && a + 1 == 0 RETURN i.value",
        "LET a = NOOPT(0) FOR i IN " + c.name() + " FILTER a + 1 == 0 && i.value == 1 RETURN i.value",
        "LET a = NOOPT(0) FOR i IN " + c.name() + " FILTER i.value == 1 && a IN [ 1 ] RETURN i.value",
        "LET a = NOOPT(0) FOR i IN " + c.name() + " FILTER a IN [ 1 ] && i.value == 1 RETURN i.value",
        "LET a = NOOPT(1) FOR i IN " + c.name() + " FILTER i.value == 1 FILTER a != 1 RETURN i.value",
        "LET a = NOOPT(1) FOR i IN " + c.name() + " FILTER a != 1 FILTER i.value == 1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value == 1 FILTER i.value != 1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value != 1 FILTER i.value == 1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value == 1 FILTER i.value - 1 == 1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value - 1 == 1 FILTER i.value == 1 RETURN i.value",
        "LET a = NOOPT(0) FOR i IN " + c.name() + " FILTER i.value == 1 FILTER a + 1 == 0 RETURN i.value",
        "LET a = NOOPT(0) FOR i IN " + c.name() + " FILTER a + 1 == 0 FILTER i.value == 1 RETURN i.value",
        "LET a = NOOPT(0) FOR i IN " + c.name() + " FILTER i.value == 1 FILTER a IN [ 1 ] RETURN i.value",
        "LET a = NOOPT(0) FOR i IN " + c.name() + " FILTER a IN [ 1 ] FILTER i.value == 1 RETURN i.value"
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query, {}, opt).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        if ( nodeTypes.indexOf("NoResultsNode") !== -1) {
          // The condition is not impossible
          // We have to use an index
          assertEqual(-1, nodeTypes.indexOf("IndexNode"), query);
        }
        // The condition is impossible. We do not care for indexes.
        // assertNotEqual(-1, nodeTypes.indexOf("NoResultsNode"), query);
        var results = AQL_EXECUTE(query, {}, opt);
        assertEqual(0, results.json.length);
        assertTrue(results.stats.scannedIndex >= 0);
        assertEqual(0, results.stats.scannedFull);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testIndexOrNoIndexBecauseOfFunctions : function () {
      var queries = [
        "FOR i IN " + c.name() + " FILTER i.value == 1 || RAND() >= -1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value == 1 || RAND() >= -1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value == 1 || RAND() != -1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value == 1 || RAND() <= 10 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value == 1 || RAND() < 10 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value == 1 || NOOPT(true) == true RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value == 1 || 1 + NOOPT(2) == 3 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value == 1 || 1 + NOOPT(2) RETURN i.value",
        "FOR i IN " + c.name() + " FILTER RAND() >= -1 || i.value == 1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER RAND() >= -1 || i.value == 1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER RAND() != -1 || i.value == 1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER RAND() <= 10 || i.value == 1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER RAND() < 10 || i.value == 1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER 1 + NOOPT(2) == 3 || i.value == 1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER 1 + NOOPT(2) || i.value == 1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value IN [ 1, 2 ] || RAND() >= -1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER RAND() >= -1 || i.value IN [ 1, 2 ] RETURN i.value"
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query, {}, opt).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        assertEqual(-1, nodeTypes.indexOf("IndexNode"), query);
        var results = AQL_EXECUTE(query, {}, opt);
        assertEqual(2000, results.json.length);
        assertEqual(0, results.stats.scannedIndex);
        assertTrue(results.stats.scannedFull > 0);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testIndexOrNoIndexBecauseOfReferences : function () {
      var queries = [
        "LET a = 1 FOR i IN " + c.name() + " FILTER i.value == 1 || a == 1 RETURN i.value",
        "LET a = 1 FOR i IN " + c.name() + " FILTER a == 1 || i.value == 1 RETURN i.value",
        "LET a = NOOPT(1) FOR i IN " + c.name() + " FILTER i.value == 1 || a == 1 RETURN i.value",
        "LET a = NOOPT(1) FOR i IN " + c.name() + " FILTER a == 1 || i.value == 1 RETURN i.value",
        "LET a = { value: 1 } FOR i IN " + c.name() + " FILTER i.value == 1 || a.value == 1 RETURN i.value",
        "LET a = { value: 1 } FOR i IN " + c.name() + " FILTER a.value == 1 || i.value == 1 RETURN i.value",
        "LET a = NOOPT({ value: 1 }) FOR i IN " + c.name() + " FILTER i.value == 1 || a.value == 1 RETURN i.value",
        "LET a = NOOPT({ value: 1 }) FOR i IN " + c.name() + " FILTER a.value == 1 || i.value == 1 RETURN i.value",
        "LET sub = (FOR x IN [ 1, 2, 3 ] RETURN x) FOR i IN " + c.name() + " FILTER i.value == 1 || 1 IN sub RETURN i.value",
        "LET sub = (FOR x IN [ 1, 2, 3 ] RETURN x) FOR i IN " + c.name() + " FILTER 1 IN sub || i.value == 1 RETURN i.value",
        "FOR j IN 1..1 FOR i IN " + c.name() + " FILTER i.value == 1 || j == 1 RETURN i.value",
        "FOR j IN 1..1 FOR i IN " + c.name() + " FILTER j == 1 || i.value == 1 RETURN i.value",
        "FOR j IN [ { value: 1 } ] FOR i IN " + c.name() + " FILTER i.value == 1 || j.value == 1 RETURN i.value",
        "FOR j IN [ { value: 1 } ] FOR i IN " + c.name() + " FILTER j.value == 1 || i.value == 1 RETURN i.value"
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query, {}, opt).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        assertEqual(-1, nodeTypes.indexOf("IndexNode"), query);
        var results = AQL_EXECUTE(query, {}, opt);
        assertEqual(2000, results.json.length);
        assertEqual(0, results.stats.scannedIndex);
        assertTrue(results.stats.scannedFull > 0);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testIndexOrNoIndexBecauseOfOtherCollection : function () {
      var queries = [
        "FOR j IN " + c.name() + " FOR i IN " + c.name() + " FILTER i.value == 1 || j.value == 1 LIMIT 2000 RETURN i.value",
        "FOR j IN " + c.name() + " FOR i IN " + c.name() + " FILTER j.value == 1 || i.value == 1 LIMIT 2000 RETURN i.value"
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query, {}, opt).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        assertEqual(-1, nodeTypes.indexOf("IndexNode"), query);
        var results = AQL_EXECUTE(query, {}, opt);
        assertEqual(2000, results.json.length);
        assertEqual(0, results.stats.scannedIndex);
        assertTrue(results.stats.scannedFull > 0);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testIndexOrNoIndexBecauseOfOperators : function () {
      var queries = [
        "LET a = NOOPT(1) FOR i IN " + c.name() + " FILTER i.value == 1 || a != 2 RETURN i.value",
        "LET a = NOOPT(1) FOR i IN " + c.name() + " FILTER a != 2 || i.value == 1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value == 1 || i.value != -1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value != -1 || i.value == 1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value == 1 || i.value - 1 != 0 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value - 1 != 0 || i.value == 1 RETURN i.value",
        "LET a = NOOPT(0) FOR i IN " + c.name() + " FILTER i.value == 1 || a + 1 == 1 RETURN i.value",
        "LET a = NOOPT(0) FOR i IN " + c.name() + " FILTER a + 1 == 1 || i.value == 1 RETURN i.value",
        "LET a = NOOPT(0) FOR i IN " + c.name() + " FILTER i.value == 1 || a IN [ 0, 1 ] RETURN i.value",
        "LET a = NOOPT(0) FOR i IN " + c.name() + " FILTER a IN [ 0, 1 ] || i.value == 1 RETURN i.value"
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query, {}, opt).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        assertEqual(-1, nodeTypes.indexOf("IndexNode"), query);
        var results = AQL_EXECUTE(query, {}, opt);
        assertEqual(2000, results.json.length);
        assertEqual(0, results.stats.scannedIndex);
        assertTrue(results.stats.scannedFull > 0);
      });
    },
  };
}

function optimizerIndexesModifyTestSuite () {
  var c;
  var idx = null;
  var idx1 = null;
  var idx2 = null;

  return {
    setUp: function() {
      db._drop("UnitTestsCollection");
      c = db._create("UnitTestsCollection");

      let docs = [];
      for (var i = 0; i < 2000; ++i) {
        docs.push({ _key: "test" + i, value: i });
      }
      c.insert(docs);
      
      idx = c.ensureSkiplist("value");
    },
    
    tearDown: function() {
      db._drop("UnitTestsCollection");
    },

    testMultiIndexesOrCondition : function () {
      let values = [ 
        [ "abc", true, true ],
        [ "abc", false, true ],
        [ "abc", true, false ]
      ];

      values.forEach(function(v) {
        c.update("test2", { test1: v[0], test2: v[1], test3: v[2] });
        let q = "FOR doc IN UnitTestsCollection FILTER doc.value == 2 && doc.test1 == 'abc' && (doc.test2 || doc.test3) RETURN doc";
        var results = AQL_EXECUTE(q);
        assertEqual(1, results.json.length);
        assertEqual(2, results.json[0].value);
        assertEqual(v[0], results.json[0].test1);
        assertEqual(v[1], results.json[0].test2);
        assertEqual(v[2], results.json[0].test3);

        let plan = AQL_EXPLAIN(q).plan;
        let nodes = plan.nodes;
        let nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });
        let indexNode = nodes[nodeTypes.indexOf("IndexNode")];
        assertEqual("n-ary or", indexNode.condition.type);
        assertEqual(2, indexNode.condition.subNodes.length);
        assertEqual("n-ary and", indexNode.condition.subNodes[0].type);
        assertEqual("compare ==", indexNode.condition.subNodes[0].subNodes[0].type);
        assertEqual("n-ary and", indexNode.condition.subNodes[1].type);
        assertEqual("compare ==", indexNode.condition.subNodes[1].subNodes[0].type);
      });
    },
////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testUseIndexMultipleFiltersWithLimit : function () {
      c.insert([{ value: "one" },
                { value: "two" },
                { value: "one" },
                { value: "two" }]);
      var query = "FOR i IN " + c.name() + " FILTER i.value IN ['one', 'two'] SORT i.value DESC LIMIT 0, 2 FILTER i.value == 'one' RETURN i.value";

      var plan = AQL_EXPLAIN(query, {}, opt).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        return node.type;
      });

      assertEqual("SingletonNode", nodeTypes[0], query);
      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);

      var results = AQL_EXECUTE(query, {}, opt);
      assertEqual([ ], results.json, query);
      assertEqual(0, results.stats.scannedFull);
      assertTrue(results.stats.scannedIndex > 0);

      // retry without index
      var idx = c.lookupIndex({ type: "skiplist", fields: [ "value" ] });
      c.dropIndex(idx);
      idx = null;

      results = AQL_EXECUTE(query, {}, opt);
      assertEqual([ ], results.json, query);
      assertTrue(results.stats.scannedFull > 0);
      assertEqual(0, results.stats.scannedIndex);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testUseIndexMultipleFilters1 : function () {
      c.insert([{ value: "one" },
                { value: "one" },
                { value: "two" },
                { value: "two" }]);
      var query = "FOR i IN " + c.name() + " FILTER i.value IN ['one', 'two'] FILTER i.value == 'one' RETURN i.value";

      var plan = AQL_EXPLAIN(query, {}, opt).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        return node.type;
      });

      assertEqual("SingletonNode", nodeTypes[0], query);
      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);

      var results = AQL_EXECUTE(query, {}, opt);
      assertEqual([ 'one', 'one' ], results.json, query);
      assertEqual(0, results.stats.scannedFull);
      assertTrue(results.stats.scannedIndex > 0);

      // retry without index
      var idx = c.lookupIndex({ type: "skiplist", fields: [ "value" ] });
      c.dropIndex(idx);
      idx = null;

      results = AQL_EXECUTE(query, {}, opt);
      assertEqual([ 'one', 'one' ], results.json, query);
      assertTrue(results.stats.scannedFull > 0);
      assertEqual(0, results.stats.scannedIndex);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testUseIndexMultipleFilters2 : function () {
      c.insert([{ value: "one" },
                { value: "one" },
                { value: "two" },
                { value: "two" }]);
      var query = "FOR i IN " + c.name() + " FILTER i.value IN ['one', 'two'] LIMIT 0, 4 FILTER i.value == 'one' RETURN i.value";

      var plan = AQL_EXPLAIN(query, {}, opt).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        return node.type;
      });

      assertEqual("SingletonNode", nodeTypes[0], query);
      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);
      assertNotEqual(-1, nodeTypes.indexOf("LimitNode"), query);

      var results = AQL_EXECUTE(query, {}, opt);
      assertEqual([ 'one', 'one' ], results.json, query);
      assertEqual(0, results.stats.scannedFull);
      assertTrue(results.stats.scannedIndex > 0);

      // retry without index
      var idx = c.lookupIndex({ type: "skiplist", fields: [ "value" ] });
      c.dropIndex(idx);
      idx = null;

      results = AQL_EXECUTE(query, {}, opt);
      assertEqual([ 'one', 'one' ], results.json, query);
      assertTrue(results.stats.scannedFull > 0);
      assertEqual(0, results.stats.scannedIndex);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testUseIndexMultipleFiltersInvalid1 : function () {
      c.insert([{ value: "one" },
                { value: "one" },
                { value: "two" },
                { value: "two" }]);
      var query = "FOR i IN " + c.name() + " FILTER i.value IN ['one', 'two'] FILTER i.value == 'three' RETURN i.value";

      var plan = AQL_EXPLAIN(query, {}, opt).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        return node.type;
      });

      assertEqual("SingletonNode", nodeTypes[0], query);
      assertEqual(-1, nodeTypes.indexOf("IndexNode"), query);
      assertNotEqual(-1, nodeTypes.indexOf("NoResultsNode"), query);

      var results = AQL_EXECUTE(query, {}, opt);
      assertEqual([ ], results.json, query);
      assertEqual(0, results.stats.scannedFull);
      assertEqual(0, results.stats.scannedIndex);

      // retry without index
      var idx = c.lookupIndex({ type: "skiplist", fields: [ "value" ] });
      c.dropIndex(idx);
      idx = null;

      results = AQL_EXECUTE(query, {}, opt);
      assertEqual([ ], results.json, query);
      assertEqual(0, results.stats.scannedFull);
      assertEqual(0, results.stats.scannedIndex);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testUseIndexMultipleFiltersInvalid2 : function () {
      c.insert([{ value: "one" },
                { value: "one" },
                { value: "two" },
                { value: "two" }]);
      var query = "FOR i IN " + c.name() + " FILTER i.value IN ['one', 'two'] LIMIT 0, 4 FILTER i.value == 'three' RETURN i.value";

      var plan = AQL_EXPLAIN(query, {}, opt).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        if (node.type === 'IndexNode') {
          assertTrue(node.producesResult);
        }
        return node.type;
      });

      assertEqual("SingletonNode", nodeTypes[0], query);
      assertEqual(1, nodeTypes.indexOf("IndexNode"), query);

      var results = AQL_EXECUTE(query, {}, opt);
      assertEqual([ ], results.json, query);
      assertEqual(0, results.stats.scannedFull);
      assertTrue(results.stats.scannedIndex > 0);

      // retry without index
      var idx = c.lookupIndex({ type: "skiplist", fields: [ "value" ] });
      c.dropIndex(idx);
      idx = null;

      results = AQL_EXECUTE(query, {}, opt);
      assertEqual([ ], results.json, query);
      assertTrue(results.stats.scannedFull > 0);
      assertEqual(0, results.stats.scannedIndex);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testIndexSkiplistMultiple : function () {
      c.truncate({ compact: false });
      let docs = [];
      for (var i = 0; i < 10; ++i) {
        docs.push({ value1: i, value2: i });
      }
      c.insert(docs);
      c.ensureIndex({ type: "skiplist", fields: [ "value1", "value2" ] });

      var query = "FOR i IN " + c.name() + " FILTER (i.value2 > 1 && i.value2 < 9) && (i.value1 == 3) RETURN i.value1";

      var plan = AQL_EXPLAIN(query, {}, opt).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        if (node.type === "IndexNode") {
          assertEqual("skiplist", node.indexes[0].type);
          assertFalse(node.indexes[0].unique);
        }
        return node.type;
      });

      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);
      assertEqual(-1, nodeTypes.indexOf("FilterNode"), query);

      var results = AQL_EXECUTE(query, {}, opt);
      assertEqual([ 3 ], results.json.sort(), query);
      assertEqual(1, results.stats.scannedIndex);
      assertEqual(0, results.stats.scannedFull);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testIndexSkiplistMultiple2 : function () {
      c.truncate({ compact: false });
      let docs = [];
      for (var i = 0; i < 10; ++i) {
        docs.push({ value1: i, value2: i });
      }
      c.insert(docs);
      c.ensureIndex({ type: "skiplist", fields: [ "value1", "value2" ] });

      var query = "FOR i IN " + c.name() + " FILTER (i.value2 > 1 && i.value2 < 9) && (i.value1 == 2 || i.value1 == 3) RETURN i.value1";

      var plan = AQL_EXPLAIN(query, {}, opt).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        if (node.type === "IndexNode") {
          assertEqual("skiplist", node.indexes[0].type);
          assertFalse(node.indexes[0].unique);
        }
        return node.type;
      });

      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);

      var results = AQL_EXECUTE(query, {}, opt);
      assertEqual([ 2, 3 ], results.json.sort(), query);
      assertEqual(2, results.stats.scannedIndex);
      assertEqual(0, results.stats.scannedFull);
    },

    testIndexOrSkiplistNoDocuments : function () {
      var query = "FOR i IN " + c.name() + " FILTER i.value == 1 || i.value == 9 RETURN 1";

      var plan = AQL_EXPLAIN(query, {}, opt).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        if (node.type === "IndexNode") {
          assertFalse(node.producesResult);
          assertEqual("skiplist", node.indexes[0].type);
          assertFalse(node.indexes[0].unique);
        }
        return node.type;
      });

      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);

      var results = AQL_EXECUTE(query, {}, opt);
      assertEqual([ 1, 1 ], results.json.sort(), query);
      assertEqual(2, results.stats.scannedIndex);
      assertEqual(0, results.stats.scannedFull);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testIndexOrNoIndex : function () {
      c.ensureSkiplist("value2");
      AQL_EXECUTE("FOR i IN " + c.name() + " UPDATE i WITH { value2: i.value } IN " + c.name());

      var query = "FOR i IN " + c.name() + " FILTER i.value == 1 || i.value2 == 1 RETURN i.value2";

      var plan = AQL_EXPLAIN(query, {}, opt).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        return node.type;
      });

      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);

      var results = AQL_EXECUTE(query, {}, opt);
      assertEqual([ 1 ], results.json, query);
      assertTrue(results.stats.scannedIndex > 0);
      assertEqual(0, results.stats.scannedFull);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testIndexOrNoIndexComplexConditionEq : function () {
      c.ensureSkiplist("value2");
      AQL_EXECUTE("FOR i IN " + c.name() + " UPDATE i WITH { value2: [ i.value ] } IN " + c.name());

      var query = "FOR i IN " + c.name() + " FILTER i.value == -1 || [ i.value ] == i.value2 RETURN i.value2";

      var plan = AQL_EXPLAIN(query, {}, opt).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        return node.type;
      });

      assertEqual(-1, nodeTypes.indexOf("IndexNode"), query);

      var results = AQL_EXECUTE(query, {}, opt);
      assertEqual(2000, results.json.length);
      assertEqual(0, results.stats.scannedIndex);
      assertTrue(results.stats.scannedFull > 0);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testIndexOrNoIndexComplexConditionIn : function () {
      c.ensureSkiplist("value2");
      AQL_EXECUTE("FOR i IN " + c.name() + " UPDATE i WITH { value2: [ i.value ] } IN " + c.name());

      var query = "FOR i IN " + c.name() + " FILTER i.value == -1 || i.value IN i.value2 RETURN i.value2";

      var plan = AQL_EXPLAIN(query, {}, opt).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        return node.type;
      });

      assertEqual(-1, nodeTypes.indexOf("IndexNode"), query);

      var results = AQL_EXECUTE(query, {}, opt);
      assertEqual(2000, results.json.length);
      assertEqual(0, results.stats.scannedIndex);
      assertTrue(results.stats.scannedFull > 0);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testIndexOrNoIndexComplexConditionLt1 : function () {
      c.ensureSkiplist("value2");
      AQL_EXECUTE("FOR i IN " + c.name() + " UPDATE i WITH { value2: i.value - 1 } IN " + c.name());

      var query = "FOR i IN " + c.name() + " FILTER i.value == -1 || i.value2 < i.value RETURN i.value2";

      var plan = AQL_EXPLAIN(query, {}, opt).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        return node.type;
      });

      assertEqual(-1, nodeTypes.indexOf("IndexNode"), query);

      var results = AQL_EXECUTE(query, {}, opt);
      assertEqual(2000, results.json.length);
      assertEqual(0, results.stats.scannedIndex);
      assertTrue(results.stats.scannedFull > 0);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testIndexOrNoIndexComplexConditionLt2 : function () {
      c.ensureSkiplist("value2");
      AQL_EXECUTE("FOR i IN " + c.name() + " UPDATE i WITH { value2: i.value - 1 } IN " + c.name());

      var query = "FOR i IN " + c.name() + " FILTER i.value == -1 || i.value2 < NOOPT(i.value) RETURN i.value2";

      var plan = AQL_EXPLAIN(query, {}, opt).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        return node.type;
      });

      assertEqual(-1, nodeTypes.indexOf("IndexNode"), query);

      var results = AQL_EXECUTE(query, {}, opt);
      assertEqual(2000, results.json.length);
      assertEqual(0, results.stats.scannedIndex);
      assertTrue(results.stats.scannedFull > 0);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testIndexOrNoIndexComplexConditionGt : function () {
      c.ensureSkiplist("value2");
      AQL_EXECUTE("FOR i IN " + c.name() + " UPDATE i WITH { value2: i.value + 1 } IN " + c.name());

      var query = "FOR i IN " + c.name() + " FILTER i.value == -1 || i.value2 > i.value RETURN i.value2";

      var plan = AQL_EXPLAIN(query, {}, opt).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        return node.type;
      });

      assertEqual(-1, nodeTypes.indexOf("IndexNode"), query);

      var results = AQL_EXECUTE(query, {}, opt);
      assertEqual(2000, results.json.length);
      assertEqual(0, results.stats.scannedIndex);
      assertTrue(results.stats.scannedFull > 0);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testIndexOrHashMultipleRanges : function () {
      c.ensureHashIndex("value2", "value3");
      AQL_EXECUTE("FOR i IN " + c.name() + " UPDATE i WITH { value2: i.value, value3: i.value } IN " + c.name());

      var query = "FOR i IN " + c.name() + " FILTER (i.value2 == 2 && i.value3 == 2) || (i.value2 == 3 && i.value3 == 3) RETURN i.value2";

      var plan = AQL_EXPLAIN(query, {}, opt).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        if (node.type === "IndexNode") {
          assertEqual("hash", node.indexes[0].type);
          assertFalse(node.indexes[0].unique);
          assertEqual([ "value2", "value3" ], node.indexes[0].fields);
        }
        return node.type;
      });

      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);

      var results = AQL_EXECUTE(query, {}, opt);
      assertEqual([ 2, 3 ], results.json.sort(), query);
      assertEqual(2, results.stats.scannedIndex);
      assertEqual(0, results.stats.scannedFull);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testIndexOrSkiplistMultipleRanges : function () {
      c.ensureSkiplist("value2", "value3");
      AQL_EXECUTE("FOR i IN " + c.name() + " UPDATE i WITH { value2: i.value, value3: i.value } IN " + c.name());

      var query = "FOR i IN " + c.name() + " FILTER (i.value2 == 2 && i.value3 == 2) || (i.value2 == 3 && i.value3 == 3) RETURN i.value2";

      var plan = AQL_EXPLAIN(query, {}, opt).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        if (node.type === "IndexNode") {
          assertEqual("skiplist", node.indexes[0].type);
          assertFalse(node.indexes[0].unique);
          assertEqual([ "value2", "value3" ], node.indexes[0].fields);
        }
        return node.type;
      });

      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);

      var results = AQL_EXECUTE(query, {}, opt);
      assertEqual([ 2, 3 ], results.json.sort(), query);
      assertEqual(2, results.stats.scannedIndex);
      assertEqual(0, results.stats.scannedFull);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testIndexOrHashMultipleRangesPartialNoIndex1 : function () {
      c.ensureHashIndex("value2", "value3");
      AQL_EXECUTE("FOR i IN " + c.name() + " UPDATE i WITH { value2: i.value, value3: i.value } IN " + c.name());

      var query = "FOR i IN " + c.name() + " FILTER (i.value2 == 1 && i.value3 == 1) || (i.value2 == 2) RETURN i.value2";

      var plan = AQL_EXPLAIN(query, {}, opt).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        return node.type;
      });

      var results = AQL_EXECUTE(query, {}, opt);
      assertEqual([ 1, 2 ], results.json.sort(), query);
      assertEqual(2, results.stats.scannedIndex);
      assertEqual(0, results.stats.scannedFull);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testIndexOrHashMultipleRangesPartialNoIndex2 : function () {
      c.ensureHashIndex("value2", "value3");
      AQL_EXECUTE("FOR i IN " + c.name() + " UPDATE i WITH { value2: i.value, value3: i.value } IN " + c.name());

      var query = "FOR i IN " + c.name() + " FILTER (i.value2 == 1 && i.value3 == 1) || (i.value3 == 2) RETURN i.value2";

      var plan = AQL_EXPLAIN(query, {}, opt).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        return node.type;
      });
      assertEqual(-1, nodeTypes.indexOf("IndexNode"), query);

      var results = AQL_EXECUTE(query, {}, opt);
      assertEqual([ 1, 2 ], results.json.sort(), query);
      assertEqual(0, results.stats.scannedIndex);
      assertTrue(results.stats.scannedFull > 0);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testIndexOrSkiplistMultipleRangesPartialIndex : function () {
      c.ensureSkiplist("value2", "value3");
      AQL_EXECUTE("FOR i IN " + c.name() + " UPDATE i WITH { value2: i.value, value3: i.value } IN " + c.name());

      var query = "FOR i IN " + c.name() + " FILTER (i.value2 == 1 && i.value3 == 1) || (i.value2 == 2) RETURN i.value2";

      var plan = AQL_EXPLAIN(query, {}, opt).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        if (node.type === "IndexNode") {
          assertEqual("skiplist", node.indexes[0].type);
          assertFalse(node.indexes[0].unique);
          assertEqual([ "value2", "value3" ], node.indexes[0].fields);
        }
        return node.type;
      });
      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);

      var results = AQL_EXECUTE(query, {}, opt);
      assertEqual([ 1, 2 ], results.json.sort(), query);
      assertEqual(2, results.stats.scannedIndex);
      assertEqual(0, results.stats.scannedFull);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testIndexOrSkiplistMultipleRangesPartialNoIndex : function () {
      c.ensureSkiplist("value2", "value3");
      AQL_EXECUTE("FOR i IN " + c.name() + " UPDATE i WITH { value2: i.value, value3: i.value } IN " + c.name());

      var query = "FOR i IN " + c.name() + " FILTER (i.value2 == 1 && i.value3 == 1) || (i.value3 == 2) RETURN i.value2";

      var plan = AQL_EXPLAIN(query, {}, opt).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        return node.type;
      });
      assertEqual(-1, nodeTypes.indexOf("IndexNode"), query);

      var results = AQL_EXECUTE(query, {}, opt);
      assertEqual([ 1, 2 ], results.json.sort(), query);
      assertEqual(0, results.stats.scannedIndex);
      assertTrue(results.stats.scannedFull > 0);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testIndexAndUseIndex : function () {
      c.ensureSkiplist("value2");
      AQL_EXECUTE("FOR i IN " + c.name() + " UPDATE i WITH { value2: i.value } IN " + c.name());

      var query = "FOR i IN " + c.name() + " FILTER i.value == 1 && i.value2 == 1 RETURN i.value2";

      var plan = AQL_EXPLAIN(query, {}, opt).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        return node.type;
      });

      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);

      var results = AQL_EXECUTE(query, {}, opt);
      assertEqual([ 1 ], results.json, query);
      assertEqual(0, results.stats.scannedFull);
      assertTrue(results.stats.scannedIndex > 0);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testIndexOrNoIndexBecauseOfDifferentAttributes : function () {
      AQL_EXECUTE("FOR i IN " + c.name() + " UPDATE i WITH { value2: i.value, value3: 1 } IN " + c.name());

      var queries = [
        "FOR i IN " + c.name() + " FILTER i.value == 1 || i.value2 == 2 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 || i.value == 1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER (i.value == 1 || i.value == 2) || i.value2 == 2 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 || (i.value == 1 || i.value == 2) RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value == 1 || (i.value == 2 || i.value2 == 2) RETURN i.value",
        "FOR i IN " + c.name() + " FILTER (i.value2 == 2 || i.value == 1) || i.value == 2 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER (i.value == 1 && i.value2 == 1) || (i.value == 2 || i.value3 != 1) RETURN i.value",
        "FOR i IN " + c.name() + " FILTER (i.value2 == 1 && i.value == 1) || (i.value3 != 1 || i.value == 2) RETURN i.value",
        "FOR i IN " + c.name() + " FILTER (i.value == 1 && i.value2 == 1) || (i.value == 2 || i.value3 == 0) RETURN i.value",
        "FOR i IN " + c.name() + " FILTER (i.value2 == 1 && i.value == 1) || (i.value3 == 0 || i.value == 2) RETURN i.value"
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query, {}, opt).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        assertEqual(-1, nodeTypes.indexOf("IndexNode"), query);
        var results = AQL_EXECUTE(query, {}, opt);
        assertEqual(2, results.json.length);
        assertTrue(results.stats.scannedFull > 0);
        assertEqual(0, results.stats.scannedIndex);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testIndexOrNoIndexBecauseOfOperator : function () {
      AQL_EXECUTE("FOR i IN " + c.name() + " UPDATE i WITH { value2: i.value, value3: 1 } IN " + c.name());

      var queries = [
        "FOR i IN " + c.name() + " FILTER i.value == 1 || i.value2 != i.value RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value2 != i.value || i.value == 1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value != 1 && NOOPT(i.value2) == 2 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value != 1 FILTER NOOPT(i.value2) == 2 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER NOOPT(i.value2) == 2 && i.value != 1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER NOOPT(i.value2) == 2 FILTER i.value != 1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value2 != 2 && NOOPT(i.value) == 1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value2 != 2 FILTER NOOPT(i.value) == 1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER NOOPT(i.value) == 1 && i.value2 != 2 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER NOOPT(i.value) == 1 FILTER i.value2 != 2 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value == 1 || i.value3 != 1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value3 != 1 || i.value == 1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value3 != 1 || i.value2 == 2 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 || i.value3 != 1 RETURN i.value"
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query, {}, opt).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        assertEqual(-1, nodeTypes.indexOf("IndexNode"), query);
        var results = AQL_EXECUTE(query, {}, opt);
        assertEqual(1, results.json.length, query);
        assertTrue(results.stats.scannedFull > 0);
        assertEqual(0, results.stats.scannedIndex);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testIndexAndSkiplistIndexedNonIndexed : function () {
      // value2 is not indexed
      AQL_EXECUTE("FOR i IN " + c.name() + " UPDATE i WITH { value2: i.value } IN " + c.name());

      var queries = [
        "FOR i IN " + c.name() + " FILTER i.value == 2 FILTER i.value2 == 2 RETURN i",
        "FOR i IN " + c.name() + " FILTER i.value == 2 && i.value2 == 2 RETURN i",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 FILTER i.value == 2 RETURN i",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 && i.value == 2 RETURN i",
        "FOR i IN " + c.name() + " FILTER i.value == 2 FILTER i.value2 != 1 RETURN i",
        "FOR i IN " + c.name() + " FILTER i.value == 2 && i.value2 != 1 RETURN i",
        "FOR i IN " + c.name() + " FILTER i.value2 != 1 FILTER i.value == 2 RETURN i",
        "FOR i IN " + c.name() + " FILTER i.value2 != 1 && i.value == 2 RETURN i"
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query, {}, opt).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);
        var results = AQL_EXECUTE(query, {}, opt);
        assertEqual(1, results.json.length);
        assertEqual(2, results.json[0].value);
        assertTrue(results.stats.scannedIndex > 0);
        assertEqual(0, results.stats.scannedFull);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testIndexAndHashIndexedNonIndexed : function () {
      // drop the skiplist index
      c.dropIndex(c.getIndexes()[1]);
      c.ensureHashIndex("value");
      // value2 is not indexed
      AQL_EXECUTE("FOR i IN " + c.name() + " UPDATE i WITH { value2: i.value } IN " + c.name());

      var queries = [
        "FOR i IN " + c.name() + " FILTER i.value == 2 FILTER i.value2 == 2 RETURN i",
        "FOR i IN " + c.name() + " FILTER i.value == 2 && i.value2 == 2 RETURN i",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 FILTER i.value == 2 RETURN i",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 && i.value == 2 RETURN i",
        "FOR i IN " + c.name() + " FILTER i.value == 2 FILTER i.value2 != 1 RETURN i",
        "FOR i IN " + c.name() + " FILTER i.value == 2 && i.value2 != 1 RETURN i",
        "FOR i IN " + c.name() + " FILTER i.value2 != 1 FILTER i.value == 2 RETURN i",
        "FOR i IN " + c.name() + " FILTER i.value2 != 1 && i.value == 2 RETURN i"
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query, {}, opt).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);
        var results = AQL_EXECUTE(query, {}, opt);
        assertEqual(1, results.json.length);
        assertEqual(2, results.json[0].value);
        assertTrue(results.stats.scannedIndex > 0);
        assertEqual(0, results.stats.scannedFull);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testIndexAndAlternativeHashAndSkiplistIndexedNonIndexed : function () {
      // create a competitor index
      c.ensureHashIndex("value");

      // value2 is not indexed
      AQL_EXECUTE("FOR i IN " + c.name() + " UPDATE i WITH { value2: i.value } IN " + c.name());

      var queries = [
        "FOR i IN " + c.name() + " FILTER i.value == 2 FILTER i.value2 == 2 RETURN i",
        "FOR i IN " + c.name() + " FILTER i.value == 2 && i.value2 == 2 RETURN i",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 FILTER i.value == 2 RETURN i",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 && i.value == 2 RETURN i",
        "FOR i IN " + c.name() + " FILTER i.value == 2 FILTER i.value2 != 1 RETURN i",
        "FOR i IN " + c.name() + " FILTER i.value == 2 && i.value2 != 1 RETURN i",
        "FOR i IN " + c.name() + " FILTER i.value2 != 1 FILTER i.value == 2 RETURN i",
        "FOR i IN " + c.name() + " FILTER i.value2 != 1 && i.value == 2 RETURN i"
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query, {}, opt).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);
        var results = AQL_EXECUTE(query, {}, opt);
        assertEqual(1, results.json.length);
        assertEqual(2, results.json[0].value);
        assertTrue(results.stats.scannedIndex > 0);
        assertEqual(0, results.stats.scannedFull);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testIndexAndSkiplistPartialIndexedNonIndexed : function () {
      // drop the skiplist index
      c.dropIndex(c.getIndexes()[1]);

      // create an alternative index
      c.ensureSkiplist("value", "value3");

      // value2 is not indexed
      AQL_EXECUTE("FOR i IN " + c.name() + " UPDATE i WITH { value2: i.value } IN " + c.name());

      var queries = [
        "FOR i IN " + c.name() + " FILTER i.value == 2 FILTER i.value2 == 2 RETURN i",
        "FOR i IN " + c.name() + " FILTER i.value == 2 && i.value2 == 2 RETURN i",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 FILTER i.value == 2 RETURN i",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 && i.value == 2 RETURN i",
        "FOR i IN " + c.name() + " FILTER i.value == 2 FILTER i.value2 != 1 RETURN i",
        "FOR i IN " + c.name() + " FILTER i.value == 2 && i.value2 != 1 RETURN i",
        "FOR i IN " + c.name() + " FILTER i.value2 != 1 FILTER i.value == 2 RETURN i",
        "FOR i IN " + c.name() + " FILTER i.value2 != 1 && i.value == 2 RETURN i"
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query, {}, opt).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);
        var results = AQL_EXECUTE(query, {}, opt);
        assertEqual(1, results.json.length);
        assertEqual(2, results.json[0].value);
        assertTrue(results.stats.scannedIndex > 0);
        assertEqual(0, results.stats.scannedFull);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testIndexAndAlternativeSkiplistPartialIndexedNonIndexed : function () {
      // create a competitor index
      c.ensureSkiplist("value", "value3");

      // value2 is not indexed
      AQL_EXECUTE("FOR i IN " + c.name() + " UPDATE i WITH { value2: i.value } IN " + c.name());

      var queries = [
        "FOR i IN " + c.name() + " FILTER i.value == 2 FILTER i.value2 == 2 RETURN i",
        "FOR i IN " + c.name() + " FILTER i.value == 2 && i.value2 == 2 RETURN i",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 FILTER i.value == 2 RETURN i",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 && i.value == 2 RETURN i",
        "FOR i IN " + c.name() + " FILTER i.value == 2 FILTER i.value2 != 1 RETURN i",
        "FOR i IN " + c.name() + " FILTER i.value == 2 && i.value2 != 1 RETURN i",
        "FOR i IN " + c.name() + " FILTER i.value2 != 1 FILTER i.value == 2 RETURN i",
        "FOR i IN " + c.name() + " FILTER i.value2 != 1 && i.value == 2 RETURN i"
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query, {}, opt).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);
        var results = AQL_EXECUTE(query, {}, opt);
        assertEqual(1, results.json.length);
        assertEqual(2, results.json[0].value);
        assertTrue(results.stats.scannedIndex > 0);
        assertEqual(0, results.stats.scannedFull);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testIndexNotNoIndex : function () {
      c.ensureSkiplist("value2");
      AQL_EXECUTE("FOR i IN " + c.name() + " UPDATE i WITH { value2: i.value } IN " + c.name());

      var queries = [
        [ "FOR i IN " + c.name() + " FILTER NOT (i.value == 1 || i.value == 2) RETURN i", 1998 ],
        [ "FOR i IN " + c.name() + " FILTER NOT (i.value == 1 && i.value == 2) RETURN i", 2000 ],
        [ "FOR i IN " + c.name() + " FILTER i.value == 1 || NOT (i.value == -1) RETURN i", 2000 ]
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query[0], {}, opt).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        assertEqual(-1, nodeTypes.indexOf("IndexNode"), query);
        var results = AQL_EXECUTE(query[0]);
        assertEqual(query[1], results.json.length);
        assertTrue(results.stats.scannedFull > 0);
        assertEqual(0, results.stats.scannedIndex);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testIndexNotUseIndex : function () {
      c.ensureSkiplist("value2");
      AQL_EXECUTE("FOR i IN " + c.name() + " UPDATE i WITH { value2: i.value } IN " + c.name());

      var queries = [
        [ "FOR i IN " + c.name() + " FILTER NOT (i.value == 2) && i.value == 1 RETURN i", 1 ],
        [ "FOR i IN " + c.name() + " FILTER i.value == 1 && NOT (i.value == 2) RETURN i", 1 ],
        [ "FOR i IN " + c.name() + " FILTER i.value == 1 && NOT (i.value != 1) RETURN i", 1 ],
        [ "FOR i IN " + c.name() + " FILTER NOT (i.value != 1) && i.value == 1 RETURN i", 1 ],
        [ "FOR i IN " + c.name() + " FILTER i.value == 1 || NOT (i.value != -1) RETURN i", 1 ],
        [ "FOR i IN " + c.name() + " FILTER NOT (i.value == 2) FILTER i.value == 1 RETURN i", 1 ],
        [ "FOR i IN " + c.name() + " FILTER i.value == 1 FILTER NOT (i.value == 2) RETURN i", 1 ],
        [ "FOR i IN " + c.name() + " FILTER i.value == 1 FILTER NOT (i.value != 1) RETURN i", 1 ],
        [ "FOR i IN " + c.name() + " FILTER NOT (i.value != 1) FILTER i.value == 1 RETURN i", 1 ]
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query[0], {}, opt).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);
        var results = AQL_EXECUTE(query[0]);
        assertEqual(query[1], results.json.length);
        assertTrue(results.stats.scannedIndex > 0);
        assertEqual(0, results.stats.scannedFull);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testIndexUsageIn : function () {
      c.ensureHashIndex("value2");
      c.ensureSkiplist("value3");

      AQL_EXECUTE("FOR i IN " + c.name() + " UPDATE i WITH { value2: i.value, value3: i.value } IN " + c.name());

      var queries = [
        [ "FOR i IN " + c.name() + " FILTER i.value2 IN NOOPT([]) RETURN i.value2", [ ] ],
        [ "FOR i IN " + c.name() + " FILTER i.value2 IN NOOPT([23]) RETURN i.value2", [ 23 ] ],
        [ "FOR i IN " + c.name() + " FILTER i.value2 IN NOOPT([23, 42]) RETURN i.value2", [ 23, 42 ] ],
        [ "LET a = NOOPT(42) FOR i IN " + c.name() + " FILTER i.value2 IN APPEND([ 23 ], a) RETURN i.value2", [ 23, 42 ] ],
        [ "LET a = NOOPT(23) FOR i IN " + c.name() + " FILTER i.value2 IN APPEND([ 23 ], a) RETURN i.value2", [ 23 ] ],
        [ "LET a = NOOPT(23) FOR i IN " + c.name() + " FILTER i.value2 IN APPEND([ a ], a) RETURN i.value2", [ 23 ] ],
        [ "LET a = NOOPT(23), b = NOOPT(42) FOR i IN " + c.name() + " FILTER i.value2 IN [ a, b ] RETURN i.value2", [ 23, 42 ] ],
        [ "FOR i IN " + c.name() + " FILTER i.value2 IN [23] RETURN i.value2", [ 23 ] ],
        [ "FOR i IN " + c.name() + " FILTER i.value2 IN [23, 42] RETURN i.value2", [ 23, 42 ] ],
        [ "LET a = NOOPT([]) FOR i IN " + c.name() + " FILTER i.value2 IN a RETURN i.value2", [ ] ],
        [ "LET a = NOOPT([23]) FOR i IN " + c.name() + " FILTER i.value2 IN a RETURN i.value2", [ 23 ] ],
        [ "LET a = NOOPT([23, 42]) FOR i IN " + c.name() + " FILTER i.value2 IN a RETURN i.value2", [ 23, 42 ] ],
        [ "LET a = NOOPT(42) FOR i IN " + c.name() + " FILTER i.value3 IN APPEND([ 23 ], a) RETURN i.value2", [ 23, 42 ] ],
        [ "LET a = NOOPT(23) FOR i IN " + c.name() + " FILTER i.value3 IN APPEND([ 23 ], a) RETURN i.value2", [ 23 ] ],
        [ "LET a = NOOPT(23) FOR i IN " + c.name() + " FILTER i.value3 IN APPEND([ a ], a) RETURN i.value2", [ 23 ] ],
        [ "LET a = NOOPT(23), b = NOOPT(42) FOR i IN " + c.name() + " FILTER i.value3 IN [ a, b ] RETURN i.value2", [ 23, 42 ] ],
        [ "FOR i IN " + c.name() + " FILTER i.value3 IN NOOPT([]) RETURN i.value2", [ ] ],
        [ "FOR i IN " + c.name() + " FILTER i.value3 IN NOOPT([23]) RETURN i.value2", [ 23 ] ],
        [ "FOR i IN " + c.name() + " FILTER i.value3 IN NOOPT([23, 42]) RETURN i.value2", [ 23, 42 ] ],
        [ "FOR i IN " + c.name() + " FILTER i.value3 IN [23] RETURN i.value2", [ 23 ] ],
        [ "FOR i IN " + c.name() + " FILTER i.value3 IN [23, 42] RETURN i.value2", [ 23, 42 ] ],
        [ "LET a = NOOPT([]) FOR i IN " + c.name() + " FILTER i.value3 IN a RETURN i.value2", [ ] ],
        [ "LET a = NOOPT([23]) FOR i IN " + c.name() + " FILTER i.value3 IN a RETURN i.value2", [ 23 ] ],
        [ "LET a = NOOPT([23, 42]) FOR i IN " + c.name() + " FILTER i.value3 IN a RETURN i.value2", [ 23, 42 ] ],
        [ "LET a = NOOPT('test42') FOR i IN " + c.name() + " FILTER i._key IN APPEND([ 'test23' ], a) RETURN i._key", [ 'test23', 'test42' ] ],
        [ "LET a = NOOPT('test23') FOR i IN " + c.name() + " FILTER i._key IN APPEND([ 'test23' ], a) RETURN i._key", [ 'test23' ] ],
        [ "LET a = NOOPT('test23') FOR i IN " + c.name() + " FILTER i._key IN APPEND([ a ], a) RETURN i._key", [ 'test23' ] ],
        [ "LET a = NOOPT('test23'), b = NOOPT('test42') FOR i IN " + c.name() + " FILTER i._key IN [ a, b ] RETURN i._key", [ 'test23', 'test42' ] ],
        [ "FOR i IN " + c.name() + " FILTER i._key IN NOOPT([]) RETURN i._key", [ ] ],
        [ "FOR i IN " + c.name() + " FILTER i._key IN NOOPT(['test23']) RETURN i._key", [ 'test23' ] ],
        [ "FOR i IN " + c.name() + " FILTER i._key IN NOOPT(['test23', 'test42']) RETURN i._key", [ 'test23', 'test42' ] ],
        [ "FOR i IN " + c.name() + " FILTER i._key IN ['test23'] RETURN i._key", [ 'test23' ] ],
        [ "FOR i IN " + c.name() + " FILTER i._key IN ['test23', 'test42'] RETURN i._key", [ 'test23', 'test42' ] ],
        [ "LET a = NOOPT([]) FOR i IN " + c.name() + " FILTER i._key IN a RETURN i._key", [ ] ],
        [ "LET a = NOOPT(['test23']) FOR i IN " + c.name() + " FILTER i._key IN a RETURN i._key", [ 'test23' ] ],
        [ "LET a = NOOPT(['test23', 'test42']) FOR i IN " + c.name() + " FILTER i._key IN a RETURN i._key", [ 'test23', 'test42' ] ],
        [ "LET a = NOOPT({ ids: ['test23', 'test42'] }) FOR i IN " + c.name() + " FILTER i._key IN a.ids RETURN i._key", [ 'test23', 'test42' ] ],
        [ "LET a = NOOPT({ ids: [23, 42] }) FOR i IN " + c.name() + " FILTER i.value2 IN a.ids RETURN i.value2", [ 23, 42 ] ],
        [ "LET a = NOOPT({ ids: [23, 42] }) FOR i IN " + c.name() + " FILTER i.value3 IN a.ids RETURN i.value2", [ 23, 42 ] ],

        // non-arrays. should not fail but return no results
        [ "LET a = NOOPT({}) FOR i IN " + c.name() + " FILTER i._key IN a RETURN i._key", [ ] ],
        [ "LET a = NOOPT({}) FOR i IN " + c.name() + " FILTER i.value2 IN a RETURN i.value2", [ ] ],
        [ "LET a = NOOPT({}) FOR i IN " + c.name() + " FILTER i.value3 IN a RETURN i.value2", [ ] ]
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query[0], {}, opt).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        assertTrue(
          (
            ( nodeTypes.indexOf("IndexNode") !== -1) ||
            ( nodeTypes.indexOf("SingleRemoteOperationNode") !== -1)
          ), query);

        var results = AQL_EXECUTE(query[0]);
        assertEqual(query[1].length, results.json.length, query);
        assertEqual(query[1].sort(), results.json.sort(), query);
        assertTrue(results.stats.scannedIndex >= 0);
        assertEqual(0, results.stats.scannedFull);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testIndexUsageInNoIndex : function () {
      c.ensureHashIndex("value2");
      c.ensureSkiplist("value3");

      AQL_EXECUTE("FOR i IN " + c.name() + " UPDATE i WITH { value2: i.value, value3: i.value } IN " + c.name());

      var queries = [
        [ "FOR i IN " + c.name() + " FILTER i.value2 IN [] RETURN i.value2", [ ] ],
        [ "FOR i IN " + c.name() + " FILTER i.value3 IN [] RETURN i.value2", [ ] ],
        [ "FOR i IN " + c.name() + " FILTER i._key IN [] RETURN i.value2", [ ] ]
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query[0], {}, opt).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        assertEqual(-1, nodeTypes.indexOf("IndexNode"), query);
        assertNotEqual(-1, nodeTypes.indexOf("NoResultsNode"), query);

        var results = AQL_EXECUTE(query[0]);
        assertEqual(0, results.json.length);
        assertEqual(0, results.stats.scannedIndex);
        assertEqual(0, results.stats.scannedFull);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test sparse indexes
////////////////////////////////////////////////////////////////////////////////

    testSparseHash : function () {
      AQL_EXECUTE("FOR i IN " + c.name() + " UPDATE i WITH { value2: i.value } IN " + c.name());

      c.ensureHashIndex("value2", { sparse: true });
      var query = "FOR i IN " + c.name() + " FILTER i.value2 == 2 RETURN i.value";

      var plan = AQL_EXPLAIN(query, {}, opt).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        if (node.type === "IndexNode") {
          assertEqual(1, node.indexes.length);
          assertEqual("hash", node.indexes[0].type);
          assertFalse(node.indexes[0].unique);
          assertTrue(node.indexes[0].sparse);
        }
        return node.type;
      });

      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);

      var results = AQL_EXECUTE(query, {}, opt);
      assertEqual([ 2 ], results.json, query);
      assertEqual(1, results.stats.scannedIndex);
      assertEqual(0, results.stats.scannedFull);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test sparse indexes
////////////////////////////////////////////////////////////////////////////////

    testSparseUniqueHash : function () {
      AQL_EXECUTE("FOR i IN " + c.name() + " UPDATE i WITH { value2: i.value } IN " + c.name());

      c.ensureUniqueConstraint("value2", { sparse: true });
      var query = "FOR i IN " + c.name() + " FILTER i.value2 == 2 RETURN i.value";

      var plan = AQL_EXPLAIN(query, {}, opt).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        if (node.type === "IndexNode") {
          assertEqual(1, node.indexes.length);
          assertEqual("hash", node.indexes[0].type);
          assertTrue(node.indexes[0].unique);
          assertTrue(node.indexes[0].sparse);
        }
        return node.type;
      });

      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);

      var results = AQL_EXECUTE(query, {}, opt);
      assertEqual([ 2 ], results.json, query);
      assertEqual(1, results.stats.scannedIndex);
      assertEqual(0, results.stats.scannedFull);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test sparse indexes
////////////////////////////////////////////////////////////////////////////////

    testSparseHashCollisions : function () {
      AQL_EXECUTE("FOR i IN " + c.name() + " UPDATE i WITH { value2: i.value % 100 } IN " + c.name());

      c.ensureHashIndex("value2", { sparse: true });
      var query = "FOR i IN " + c.name() + " FILTER i.value2 == 2 RETURN i.value";

      var plan = AQL_EXPLAIN(query, {}, opt).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        if (node.type === "IndexNode") {
          assertEqual(1, node.indexes.length);
          assertEqual("hash", node.indexes[0].type);
          assertFalse(node.indexes[0].unique);
          assertTrue(node.indexes[0].sparse);
        }
        return node.type;
      });

      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);

      var results = AQL_EXECUTE(query, {}, opt);
      assertEqual(20, results.json.length);
      assertEqual(20, results.stats.scannedIndex);
      assertEqual(0, results.stats.scannedFull);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test sparse indexes
////////////////////////////////////////////////////////////////////////////////

    testSparseHashCollisionsOr : function () {
      AQL_EXECUTE("FOR i IN " + c.name() + " UPDATE i WITH { value2: i.value % 100 } IN " + c.name());

      c.ensureHashIndex("value2", { sparse: true });
      var query = "FOR i IN " + c.name() + " FILTER i.value2 == 2 || i.value2 == 9 RETURN i.value";

      var plan = AQL_EXPLAIN(query, {}, opt).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        if (node.type === "IndexNode") {
          assertEqual(1, node.indexes.length);
          assertEqual("hash", node.indexes[0].type);
          assertFalse(node.indexes[0].unique);
          assertTrue(node.indexes[0].sparse);
        }
        return node.type;
      });

      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);

      var results = AQL_EXECUTE(query, {}, opt);
      assertEqual(40, results.json.length);
      assertEqual(40, results.stats.scannedIndex);
      assertEqual(0, results.stats.scannedFull);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test sparse indexes
////////////////////////////////////////////////////////////////////////////////

    testSparseHashDynamic : function () {
      AQL_EXECUTE("FOR i IN " + c.name() + " FILTER i.value >= 1 UPDATE i WITH { value2: i.value % 100 } IN " + c.name());

      c.ensureHashIndex("value2", { sparse: true });
      var queries = [
        "LET a = NOOPT(null) FOR i IN " + c.name() + " FILTER i.value2 == a RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value2 == NOOPT(null) RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value2 == NOOPT(null) || i.value2 == NOOPT(null) RETURN i.value"
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query, {}, opt).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        // must not use an index
        assertEqual(-1, nodeTypes.indexOf("IndexNode"), query);

        var results = AQL_EXECUTE(query, {}, opt);
        assertEqual(1, results.json.length);
        assertEqual(0, results.stats.scannedIndex);
        assertTrue(results.stats.scannedFull > 0);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test sparse indexes
////////////////////////////////////////////////////////////////////////////////

    testSparseHashNull : function () {
      AQL_EXECUTE("FOR i IN " + c.name() + " FILTER i.value >= 1 UPDATE i WITH { value2: i.value % 100 } IN " + c.name());

      c.ensureHashIndex("value2", { sparse: true });
      var query = "FOR i IN " + c.name() + " FILTER i.value2 == null RETURN i.value";

      var plan = AQL_EXPLAIN(query, {}, opt).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        return node.type;
      });

      // must not use an index
      assertEqual(-1, nodeTypes.indexOf("IndexNode"), query);

      var results = AQL_EXECUTE(query, {}, opt);
      assertEqual(1, results.json.length);
      assertEqual(0, results.stats.scannedIndex);
      assertTrue(results.stats.scannedFull > 0);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test sparse indexes
////////////////////////////////////////////////////////////////////////////////

    testSparseHashesNull : function () {
      AQL_EXECUTE("FOR i IN " + c.name() + " FILTER i.value >= 1 UPDATE i WITH { value2: i.value % 100 } IN " + c.name());

      c.ensureHashIndex("value2", { sparse: true });
      c.ensureHashIndex("value2", { sparse: false });
      var query = "FOR i IN " + c.name() + " FILTER i.value2 == null RETURN i.value";

      var plan = AQL_EXPLAIN(query, {}, opt).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        if (node.type === "IndexNode") {
          assertEqual(1, node.indexes.length);
          assertEqual("hash", node.indexes[0].type);
          assertFalse(node.indexes[0].unique);
          assertFalse(node.indexes[0].sparse);
        }
        return node.type;
      });

      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);

      var results = AQL_EXECUTE(query, {}, opt);
      assertEqual(1, results.json.length);
      assertEqual(1, results.stats.scannedIndex);
      assertEqual(0, results.stats.scannedFull);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test sparse indexes
////////////////////////////////////////////////////////////////////////////////

    testSparseHashMulti : function () {
      AQL_EXECUTE("FOR i IN " + c.name() + " UPDATE i WITH { value2: i.value, value3: i.value } IN " + c.name());

      c.ensureHashIndex("value2", "value3", { sparse: true });

      var queries = [
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 && i.value3 == 2 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value3 == 2 && i.value2 == 2 RETURN i.value"
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query, {}, opt).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          if (node.type === "IndexNode") {
            assertEqual(1, node.indexes.length);
            assertEqual("hash", node.indexes[0].type);
            assertFalse(node.indexes[0].unique);
            assertTrue(node.indexes[0].sparse);
          }
          return node.type;
        });

        assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);

        var results = AQL_EXECUTE(query, {}, opt);
        assertEqual([ 2 ], results.json, query);
        assertEqual(1, results.stats.scannedIndex);
        assertEqual(0, results.stats.scannedFull);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test sparse indexes
////////////////////////////////////////////////////////////////////////////////

    testSparseHashMultiMissing : function () {
      AQL_EXECUTE("FOR i IN " + c.name() + " UPDATE i WITH { value2: i.value, value3: i.value } IN " + c.name());

      c.ensureHashIndex("value2", "value3", { sparse: true });

      var queries = [
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value3 == 2 RETURN i.value"
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query, {}, opt).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        assertEqual(-1, nodeTypes.indexOf("IndexNode"), query);

        var results = AQL_EXECUTE(query, {}, opt);
        assertEqual([ 2 ], results.json, query);
        assertEqual(0, results.stats.scannedIndex);
        assertTrue(results.stats.scannedFull > 0);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test sparse indexes
////////////////////////////////////////////////////////////////////////////////

    testSparseHashMultiNull : function () {
      AQL_EXECUTE("FOR i IN " + c.name() + " FILTER i.value >= 1 UPDATE i WITH { value2: i.value, value3: i.value } IN " + c.name());

      c.ensureHashIndex("value2", "value3", { sparse: true });

      var queries = [
        "FOR i IN " + c.name() + " FILTER i.value2 == null && i.value3 == null RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value2 == null RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value3 == null RETURN i.value"
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query, {}, opt).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        assertEqual(-1, nodeTypes.indexOf("IndexNode"), query);

        var results = AQL_EXECUTE(query, {}, opt);
        assertEqual([ 0 ], results.json, query);
        assertEqual(0, results.stats.scannedIndex);
        assertTrue(results.stats.scannedFull > 0);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test sparse indexes
////////////////////////////////////////////////////////////////////////////////

    testSparseHashesMultiNull : function () {
      AQL_EXECUTE("FOR i IN " + c.name() + " FILTER i.value >= 1 UPDATE i WITH { value2: i.value, value3: i.value } IN " + c.name());

      c.ensureHashIndex("value2", "value3", { sparse: true });
      c.ensureHashIndex("value2", "value3", { sparse: false });

      var query = "FOR i IN " + c.name() + " FILTER i.value2 == null && i.value3 == null RETURN i.value";

      var plan = AQL_EXPLAIN(query, {}, opt).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        if (node.type === "IndexNode") {
          assertEqual(1, node.indexes.length);
          assertEqual("hash", node.indexes[0].type);
          assertFalse(node.indexes[0].unique);
          assertFalse(node.indexes[0].sparse);
        }
        return node.type;
      });

      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);

      var results = AQL_EXECUTE(query, {}, opt);
      assertEqual([ 0 ], results.json, query);
      assertEqual(1, results.stats.scannedIndex);
      assertEqual(0, results.stats.scannedFull);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test sparse indexes
////////////////////////////////////////////////////////////////////////////////

    testSparseSkiplist : function () {
      AQL_EXECUTE("FOR i IN " + c.name() + " UPDATE i WITH { value2: i.value } IN " + c.name());

      c.ensureSkiplist("value2", { sparse: true });
      var query = "FOR i IN " + c.name() + " FILTER i.value2 == 2 RETURN i.value";

      var plan = AQL_EXPLAIN(query, {}, opt).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        if (node.type === "IndexNode") {
          assertEqual(1, node.indexes.length);
          assertEqual("skiplist", node.indexes[0].type);
          assertFalse(node.indexes[0].unique);
          assertTrue(node.indexes[0].sparse);
        }
        return node.type;
      });

      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);

      var results = AQL_EXECUTE(query, {}, opt);
      assertEqual([ 2 ], results.json, query);
      assertEqual(1, results.stats.scannedIndex);
      assertEqual(0, results.stats.scannedFull);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test sparse indexes
////////////////////////////////////////////////////////////////////////////////

    testSparseUniqueSkiplist : function () {
      AQL_EXECUTE("FOR i IN " + c.name() + " UPDATE i WITH { value2: i.value } IN " + c.name());

      c.ensureUniqueSkiplist("value2", { sparse: true });
      var query = "FOR i IN " + c.name() + " FILTER i.value2 == 2 RETURN i.value";

      var plan = AQL_EXPLAIN(query, {}, opt).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        if (node.type === "IndexNode") {
          assertEqual(1, node.indexes.length);
          assertEqual("skiplist", node.indexes[0].type);
          assertTrue(node.indexes[0].unique);
          assertTrue(node.indexes[0].sparse);
        }
        return node.type;
      });

      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);

      var results = AQL_EXECUTE(query, {}, opt);
      assertEqual([ 2 ], results.json, query);
      assertEqual(1, results.stats.scannedIndex);
      assertEqual(0, results.stats.scannedFull);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test sparse indexes
////////////////////////////////////////////////////////////////////////////////

    testSparseSkiplistCollisions : function () {
      AQL_EXECUTE("FOR i IN " + c.name() + " UPDATE i WITH { value2: i.value % 100 } IN " + c.name());

      c.ensureSkiplist("value2", { sparse: true });
      var query = "FOR i IN " + c.name() + " FILTER i.value2 == 2 RETURN i.value";

      var plan = AQL_EXPLAIN(query, {}, opt).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        if (node.type === "IndexNode") {
          assertEqual(1, node.indexes.length);
          assertEqual("skiplist", node.indexes[0].type);
          assertFalse(node.indexes[0].unique);
          assertTrue(node.indexes[0].sparse);
        }
        return node.type;
      });

      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);

      var results = AQL_EXECUTE(query, {}, opt);
      assertEqual(20, results.json.length);
      assertEqual(20, results.stats.scannedIndex);
      assertEqual(0, results.stats.scannedFull);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test sparse indexes
////////////////////////////////////////////////////////////////////////////////

    testSparseSkiplistCollisionsOr : function () {
      AQL_EXECUTE("FOR i IN " + c.name() + " UPDATE i WITH { value2: i.value % 100 } IN " + c.name());

      c.ensureSkiplist("value2", { sparse: true });
      var query = "FOR i IN " + c.name() + " FILTER i.value2 == 2 || i.value2 == 9 RETURN i.value";

      var plan = AQL_EXPLAIN(query, {}, opt).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        if (node.type === "IndexNode") {
          assertEqual(1, node.indexes.length);
          assertEqual("skiplist", node.indexes[0].type);
          assertFalse(node.indexes[0].unique);
          assertTrue(node.indexes[0].sparse);
        }
        return node.type;
      });

      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);

      var results = AQL_EXECUTE(query, {}, opt);
      assertEqual(40, results.json.length);
      assertEqual(40, results.stats.scannedIndex);
      assertEqual(0, results.stats.scannedFull);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test sparse indexes
////////////////////////////////////////////////////////////////////////////////

    testSparseSkiplistDynamic : function () {
      AQL_EXECUTE("FOR i IN " + c.name() + " FILTER i.value >= 1 UPDATE i WITH { value2: i.value % 100 } IN " + c.name());

      c.ensureSkiplist("value2", { sparse: true });
      var queries = [
        "LET a = NOOPT(null) FOR i IN " + c.name() + " FILTER i.value2 == a RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value2 == NOOPT(null) RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value2 == NOOPT(null) || i.value2 == NOOPT(null) RETURN i.value"
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query, {}, opt).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        // must not use an index
        assertEqual(-1, nodeTypes.indexOf("IndexNode"), query);

        var results = AQL_EXECUTE(query, {}, opt);
        assertEqual(1, results.json.length);
        assertEqual(0, results.stats.scannedIndex);
        assertTrue(results.stats.scannedFull > 0);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test sparse indexes
////////////////////////////////////////////////////////////////////////////////

    testSparseSkiplistNull : function () {
      AQL_EXECUTE("FOR i IN " + c.name() + " FILTER i.value >= 1 UPDATE i WITH { value2: i.value % 100 } IN " + c.name());

      c.ensureSkiplist("value2", { sparse: true });
      var query = "FOR i IN " + c.name() + " FILTER i.value2 == null RETURN i.value";

      var plan = AQL_EXPLAIN(query, {}, opt).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        return node.type;
      });

      // must not use an index
      assertEqual(-1, nodeTypes.indexOf("IndexNode"), query);

      var results = AQL_EXECUTE(query, {}, opt);
      assertEqual(1, results.json.length);
      assertEqual(0, results.stats.scannedIndex);
      assertTrue(results.stats.scannedFull > 0);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test sparse indexes
////////////////////////////////////////////////////////////////////////////////

    testSparseSkiplistsNull : function () {
      AQL_EXECUTE("FOR i IN " + c.name() + " FILTER i.value >= 1 UPDATE i WITH { value2: i.value % 100 } IN " + c.name());

      c.ensureSkiplist("value2", { sparse: true });
      c.ensureSkiplist("value2", { sparse: false });
      var query = "FOR i IN " + c.name() + " FILTER i.value2 == null RETURN i.value";

      var plan = AQL_EXPLAIN(query, {}, opt).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        if (node.type === "IndexNode") {
          assertEqual(1, node.indexes.length);
          assertEqual("skiplist", node.indexes[0].type);
          assertFalse(node.indexes[0].unique);
          assertFalse(node.indexes[0].sparse);
        }
        return node.type;
      });

      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);

      var results = AQL_EXECUTE(query, {}, opt);
      assertEqual(1, results.json.length);
      assertEqual(1, results.stats.scannedIndex);
      assertEqual(0, results.stats.scannedFull);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test sparse indexes
////////////////////////////////////////////////////////////////////////////////

    testSparseSkiplistMulti : function () {
      AQL_EXECUTE("FOR i IN " + c.name() + " UPDATE i WITH { value2: i.value, value3: i.value } IN " + c.name());

      c.ensureSkiplist("value2", "value3", { sparse: true });

      var queries = [
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 && i.value3 == 2 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value3 == 2 && i.value2 == 2 RETURN i.value"
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query, {}, opt).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          if (node.type === "IndexNode") {
            assertEqual(1, node.indexes.length);
            assertEqual("skiplist", node.indexes[0].type);
            assertFalse(node.indexes[0].unique);
            assertTrue(node.indexes[0].sparse);
          }
          return node.type;
        });

        assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);

        var results = AQL_EXECUTE(query, {}, opt);
        assertEqual([ 2 ], results.json, query);
        assertEqual(1, results.stats.scannedIndex);
        assertEqual(0, results.stats.scannedFull);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test sparse indexes
////////////////////////////////////////////////////////////////////////////////

    testSparseSkiplistMultiMissing : function () {
      AQL_EXECUTE("FOR i IN " + c.name() + " UPDATE i WITH { value2: i.value, value3: i.value } IN " + c.name());

      c.ensureSkiplist("value2", "value3", { sparse: true });

      var queries = [
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value3 == 2 RETURN i.value"
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query, {}, opt).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        // Cannot use Index
        assertEqual(-1, nodeTypes.indexOf("IndexNode"), query);
        var results = AQL_EXECUTE(query, {}, opt);
        assertEqual([ 2 ], results.json, query);
        assertTrue(results.stats.scannedFull > 0);
        assertEqual(0, results.stats.scannedIndex);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test sparse indexes
////////////////////////////////////////////////////////////////////////////////

    testSparseSkiplistMultiNull : function () {
      AQL_EXECUTE("FOR i IN " + c.name() + " FILTER i.value >= 1 UPDATE i WITH { value2: i.value, value3: i.value } IN " + c.name());

      c.ensureSkiplist("value2", "value3", { sparse: true });

      var queries = [
        "FOR i IN " + c.name() + " FILTER i.value2 == null && i.value3 == null RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value2 == null RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value3 == null RETURN i.value"
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query, {}, opt).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        assertEqual(-1, nodeTypes.indexOf("IndexNode"), query);

        var results = AQL_EXECUTE(query, {}, opt);
        assertEqual([ 0 ], results.json, query);
        assertEqual(0, results.stats.scannedIndex);
        assertTrue(results.stats.scannedFull > 0);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test sparse indexes
////////////////////////////////////////////////////////////////////////////////

    testSparseSkiplistsMultiNull : function () {
      AQL_EXECUTE("FOR i IN " + c.name() + " FILTER i.value >= 1 UPDATE i WITH { value2: i.value, value3: i.value } IN " + c.name());

      c.ensureSkiplist("value2", "value3", { sparse: true });
      c.ensureSkiplist("value2", "value3", { sparse: false });

      var query = "FOR i IN " + c.name() + " FILTER i.value2 == null && i.value3 == null RETURN i.value";

      var plan = AQL_EXPLAIN(query, {}, opt).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        if (node.type === "IndexNode") {
          assertEqual(1, node.indexes.length);
          assertEqual("skiplist", node.indexes[0].type);
          assertFalse(node.indexes[0].unique);
          assertFalse(node.indexes[0].sparse);
        }
        return node.type;
      });

      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);

      var results = AQL_EXECUTE(query, {}, opt);
      assertEqual([ 0 ], results.json, query);
      assertEqual(1, results.stats.scannedIndex);
      assertEqual(0, results.stats.scannedFull);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test sparse indexes
////////////////////////////////////////////////////////////////////////////////

    testSparseSkiplistRangeNoIndex : function () {
      AQL_EXECUTE("FOR i IN " + c.name() + " UPDATE i WITH { value2: i.value } IN " + c.name());

      c.ensureSkiplist("value2", { sparse: true });
      var query = "FOR i IN " + c.name() + " FILTER i.value2 < 10 SORT i.value2 RETURN i.value2";

      var plan = AQL_EXPLAIN(query, {}, opt).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        return node.type;
      });

      assertEqual(-1, nodeTypes.indexOf("IndexNode"), query);

      var results = AQL_EXECUTE(query, {}, opt);
      assertEqual([ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 ], results.json, query);
      assertEqual(0, results.stats.scannedIndex);
      assertEqual(2000, results.stats.scannedFull);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test sparse indexes
////////////////////////////////////////////////////////////////////////////////

    testSparseSkiplistRangeWithNullNoIndex : function () {
      AQL_EXECUTE("FOR i IN " + c.name() + " UPDATE i WITH { value2: i.value } IN " + c.name());

      c.ensureSkiplist("value2", { sparse: true });
      var query = "FOR i IN " + c.name() + " FILTER i.value2 < 10 && i.value2 >= null SORT i.value2 RETURN i.value2";

      var plan = AQL_EXPLAIN(query, {}, opt).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        return node.type;
      });

      assertEqual(-1, nodeTypes.indexOf("IndexNode"), query);

      var results = AQL_EXECUTE(query, {}, opt);
      assertEqual([ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 ], results.json, query);
      assertEqual(0, results.stats.scannedIndex);
      assertEqual(2000, results.stats.scannedFull);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test sparse indexes
////////////////////////////////////////////////////////////////////////////////

    testSparseSkiplistRangeWithIndex : function () {
      AQL_EXECUTE("FOR i IN " + c.name() + " UPDATE i WITH { value2: i.value } IN " + c.name());

      c.ensureSkiplist("value2", { sparse: true });
      var query = "FOR i IN " + c.name() + " FILTER i.value2 < 10 && i.value2 > null SORT i.value2 RETURN i.value2";

      var plan = AQL_EXPLAIN(query, {}, opt).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        return node.type;
      });

      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);
      assertEqual(-1, nodeTypes.indexOf("SortNode"), query);

      var results = AQL_EXECUTE(query, {}, opt);
      assertEqual([ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 ], results.json, query);
      assertEqual(10, results.stats.scannedIndex);
      assertEqual(0, results.stats.scannedFull);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test sparse indexes
////////////////////////////////////////////////////////////////////////////////

    testSparseSkiplistRangeWithIndexValue : function () {
      AQL_EXECUTE("FOR i IN " + c.name() + " UPDATE i WITH { value2: i.value } IN " + c.name());

      c.ensureSkiplist("value2", { sparse: true });
      var query = "FOR i IN " + c.name() + " FILTER i.value2 < 10 && i.value2 >= 2 SORT i.value2 RETURN i.value2";

      var plan = AQL_EXPLAIN(query, {}, opt).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        return node.type;
      });

      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);
      assertEqual(-1, nodeTypes.indexOf("SortNode"), query);

      var results = AQL_EXECUTE(query, {}, opt);
      assertEqual([ 2, 3, 4, 5, 6, 7, 8, 9 ], results.json, query);
      assertEqual(8, results.stats.scannedIndex);
      assertEqual(0, results.stats.scannedFull);
    }

  };
}





////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function optimizerIndexesMultiCollectionTestSuite () {
  var c1;
  var c2;

  return {
    setUp : function () {
      db._drop("UnitTestsCollection1");
      db._drop("UnitTestsCollection2");
      c1 = db._create("UnitTestsCollection1");
      c2 = db._create("UnitTestsCollection2");

      var i;
      for (i = 0; i < 200; ++i) {
        c1.save({ _key: "test" + i, value: i });
      }
      for (i = 0; i < 200; ++i) {
        c2.save({ _key: "test" + i, value: i, ref: "UnitTestsCollection1/test" + i });
      }
    },

    tearDown : function () {
      db._drop("UnitTestsCollection1");
      db._drop("UnitTestsCollection2");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testUseIndexAndSortInInner : function () {
      c2.ensureIndex({ type: "hash", fields: [ "value" ] });
      var query = "FOR i IN " + c1.name() + " LET res = (FOR j IN " + c2.name() + " FILTER j.value == i.value SORT j.ref LIMIT 1 RETURN j) SORT res[0] RETURN i";

      var plan = AQL_EXPLAIN(query, {}, opt).plan;
      let idx = -1;
      const nodeTypes = [];
      const subqueryTypes = [];
      {
        let inSubquery = false;
        for (const node of plan.nodes) {
          const n = node.type;
          if (n === "SubqueryStartNode" ) {
            nodeTypes.push(n);
            inSubquery = true;
          } else if (n === "SubqueryEndNode" ) {
            nodeTypes.push(n);
            inSubquery = false;
          } else if (inSubquery) {
            if (n === "IndexNode") {
              idx = node;
            }
            subqueryTypes.push(n);
          } else {
            nodeTypes.push(n);
          }
        }
      }

      assertEqual("SingletonNode", nodeTypes[0], query);
      assertEqual(-1, nodeTypes.indexOf("IndexNode"), query); // no index for outer query
      assertNotEqual(-1, nodeTypes.indexOf("SortNode"), query); // sort node for outer query

      var sub = nodeTypes.indexOf("SubqueryStartNode");
      assertNotEqual(-1, sub);
      assertNotEqual(-1, idx, query); // index used for inner query
      assertEqual("hash", idx.indexes[0].type);
      assertNotEqual(-1, subqueryTypes.indexOf("SortNode"), query); // must have sort node for inner query
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testUseIndexAndNoSortInInner : function () {
      c2.ensureIndex({ type: "hash", fields: [ "value" ] });
      var query = "FOR i IN " + c1.name() + " LET res = (FOR j IN " + c2.name() + " FILTER j.value == i.value SORT j.value LIMIT 1 RETURN j) SORT res[0] RETURN i";

      var plan = AQL_EXPLAIN(query, {}, opt).plan;
      const nodeTypes = [];
      const subqueryTypes = [];
      let idx = -1;
      {
        let inSubquery = false;
        for (const node of plan.nodes) {
          const n = node.type;
          if (n === "SubqueryStartNode" ) {
            nodeTypes.push(n);
            inSubquery = true;
          } else if (n === "SubqueryEndNode" ) {
            nodeTypes.push(n);
            inSubquery = false;
          } else if (inSubquery) {
            if (n === "IndexNode") {
              idx = node;
            }
            subqueryTypes.push(n);
          } else {
            nodeTypes.push(n);
          }
        }
      }
      assertEqual("SingletonNode", nodeTypes[0], query);
      assertEqual(-1, nodeTypes.indexOf("IndexNode"), query); // no index for outer query
      assertNotEqual(-1, nodeTypes.indexOf("SortNode"), query); // sort node for outer query

      var sub = nodeTypes.indexOf("SubqueryStartNode");
      assertNotEqual(-1, sub);

      assertNotEqual(-1, subqueryTypes.indexOf("IndexNode"), query); // index used for inner query
      assertEqual("hash", idx.indexes[0].type);
      assertEqual(-1, subqueryTypes.indexOf("SortNode"), query); // must not have sort node for inner query
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testUseRightIndexAndSortInInner : function () {
      c2.ensureIndex({ type: "hash", fields: [ "value" ] });
      c2.ensureIndex({ type: "skiplist", fields: [ "ref" ] });
      var query = "FOR i IN " + c1.name() + " LET res = (FOR j IN " + c2.name() + " FILTER j.value == i.value SORT j.ref LIMIT 1 RETURN j) SORT res[0] RETURN i";

      var plan = AQL_EXPLAIN(query, {}, opt).plan;
      const nodeTypes = [];
      const subqueryTypes = [];
      let idx = -1;
      {
        let inSubquery = false;
        for (const node of plan.nodes) {
          const n = node.type;
          if (n === "SubqueryStartNode" ) {
            nodeTypes.push(n);
            inSubquery = true;
          } else if (n === "SubqueryEndNode" ) {
            nodeTypes.push(n);
            inSubquery = false;
          } else if (inSubquery) {
            if (n === "IndexNode") {
              idx = node;
            }
            subqueryTypes.push(n);
          } else {
            nodeTypes.push(n);
          }
        }
      }

      assertEqual("SingletonNode", nodeTypes[0], query);
      assertEqual(-1, nodeTypes.indexOf("IndexNode"), query); // no index for outer query
      assertNotEqual(-1, nodeTypes.indexOf("SortNode"), query); // sort node for outer query

      var sub = nodeTypes.indexOf("SubqueryStartNode");
      assertNotEqual(-1, sub);
      assertNotEqual(-1, idx);
      assertNotEqual(-1, subqueryTypes.indexOf("IndexNode"), query); // index used for inner query
      assertEqual("hash", idx.indexes[0].type);
      assertNotEqual(-1, subqueryTypes.indexOf("SortNode"), query); // must have sort node for inner query
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testUseRightIndexAndSortInInner2 : function () {
      c2.ensureIndex({ type: "hash", fields: [ "value" ] });
      c2.ensureIndex({ type: "skiplist", fields: [ "ref" ] });
      var query = "FOR i IN " + c1.name() + " LET res = (FOR j IN " + c2.name() + " FILTER j.value == i.value SORT j.value LIMIT 1 RETURN j) SORT res[0] RETURN i";

      var plan = AQL_EXPLAIN(query, {}, opt).plan;
      const nodeTypes = [];
      const subqueryTypes = [];
      let idx = -1;
      {
        let inSubquery = false;
        for (const node of plan.nodes) {
          const n = node.type;
          if (n === "SubqueryStartNode" ) {
            nodeTypes.push(n);
            inSubquery = true;
          } else if (n === "SubqueryEndNode" ) {
            nodeTypes.push(n);
            inSubquery = false;
          } else if (inSubquery) {
            if (n === "IndexNode") {
              idx = node;
            }
            subqueryTypes.push(n);
          } else {
            nodeTypes.push(n);
          }
        }
      }

      assertEqual("SingletonNode", nodeTypes[0], query);
      assertEqual(-1, nodeTypes.indexOf("IndexNode"), query); // no index for outer query
      assertNotEqual(-1, nodeTypes.indexOf("SortNode"), query); // sort node for outer query

      assertNotEqual(-1, nodeTypes.indexOf("SubqueryStartNode"), query);

      assertNotEqual(-1, subqueryTypes.indexOf("IndexNode"), query);
      assertNotEqual(-1, idx, query); // index used for inner query
      assertEqual("hash", idx.indexes[0].type);
      assertEqual(-1, subqueryTypes.indexOf("SortNode"), query); // we're filtering on a constant, must not have sort node for inner query
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testUseRightIndexAndSortInInnerInner : function () {
      c2.ensureIndex({ type: "hash", fields: [ "value" ] });
      c2.ensureIndex({ type: "skiplist", fields: [ "ref" ] });
      var query = "FOR i IN " + c1.name() + " LET res = (FOR z IN 1..2 FOR j IN " + c2.name() + " FILTER j.value == i.value SORT j.value LIMIT 1 RETURN j) SORT res[0] RETURN i";

      var plan = AQL_EXPLAIN(query, {}, opt).plan;
      const nodeTypes = [];
      const subqueryTypes = [];
      let idx = -1;
      {
        let inSubquery = false;
        for (const node of plan.nodes) {
          const n = node.type;
          if (n === "SubqueryStartNode" ) {
            nodeTypes.push(n);
            inSubquery = true;
          } else if (n === "SubqueryEndNode" ) {
            nodeTypes.push(n);
            inSubquery = false;
          } else if (inSubquery) {
            if (n === "IndexNode") {
              idx = node;
            }
            subqueryTypes.push(n);
          } else {
            nodeTypes.push(n);
          }
        }
      }

      assertEqual("SingletonNode", nodeTypes[0], query);
      assertEqual(-1, nodeTypes.indexOf("IndexNode"), query); // no index for outer query
      assertNotEqual(-1, nodeTypes.indexOf("SortNode"), query); // sort node for outer query

      var sub = nodeTypes.indexOf("SubqueryStartNode");
      assertNotEqual(-1, sub, query);
      assertNotEqual(-1, subqueryTypes.indexOf("IndexNode"), query);
      assertNotEqual(-1, idx, query); // index used for inner query
      assertEqual("hash", idx.indexes[0].type);
      assertNotEqual(-1, subqueryTypes.indexOf("SortNode"), query); // we're filtering on a constant, but we're in an inner loop
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testUseRightIndexNoSortInInner : function () {
      c2.ensureIndex({ type: "hash", fields: [ "value" ] });
      c2.ensureIndex({ type: "skiplist", fields: [ "ref" ] });
      var query = "FOR i IN " + c1.name() + " LET res = (FOR j IN " + c2.name() + " FILTER j.ref == i.ref SORT j.ref LIMIT 1 RETURN j) SORT res[0] RETURN i";

      var plan = AQL_EXPLAIN(query, {}, opt).plan;
      const nodeTypes = [];
      const subqueryTypes = [];
      let idx = -1;
      {
        let inSubquery = false;
        for (const node of plan.nodes) {
          const n = node.type;
          if (n === "SubqueryStartNode" ) {
            nodeTypes.push(n);
            inSubquery = true;
          } else if (n === "SubqueryEndNode" ) {
            nodeTypes.push(n);
            inSubquery = false;
          } else if (inSubquery) {
            if (n === "IndexNode") {
              idx = node;
            }
            subqueryTypes.push(n);
          } else {
            nodeTypes.push(n);
          }
        }
      }

      assertEqual("SingletonNode", nodeTypes[0], query);
      assertEqual(-1, nodeTypes.indexOf("IndexNode"), query); // no index for outer query
      assertNotEqual(-1, nodeTypes.indexOf("SortNode"), query); // sort node for outer query
      assertNotEqual(-1, nodeTypes.indexOf("SubqueryStartNode"), query);

      assertNotEqual(-1, subqueryTypes.indexOf("IndexNode"), query);
      assertNotEqual(-1, idx, query); // index used for inner query
      assertEqual("skiplist", idx.indexes[0].type);
      assertEqual(-1, subqueryTypes.indexOf("SortNode"), query); // must not have sort node for inner query
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testPreventMoveFilterPastModify1 : function () {
      c1.ensureIndex({ type: "hash", fields: [ "value" ] });
      c2.ensureIndex({ type: "hash", fields: [ "value" ] });
      c2.ensureIndex({ type: "skiplist", fields: [ "ref" ] });
      var query = `
        FOR i IN ${c1.name()}
          FOR j IN ${c2.name()}
            UPDATE j WITH { tick: i } in ${c2.name()}
            FILTER i.value == 1
          RETURN [i, NEW]
      `;

      var plan = AQL_EXPLAIN(query, {}, opt).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        return node.type;
      });

      assertEqual("SingletonNode", nodeTypes[0], query);
      assertEqual(-1, nodeTypes.indexOf("IndexNode"), query); // no index
      assertNotEqual(-1, nodeTypes.indexOf("FilterNode"), query); // post filter
    },

    testPreventMoveFilterPastModify2 : function () {
      c1.ensureIndex({ type: "hash", fields: [ "value" ] });
      c2.ensureIndex({ type: "hash", fields: [ "value" ] });
      c2.ensureIndex({ type: "skiplist", fields: [ "ref" ] });
      var query = `
        FOR i IN ${c1.name()}
          FOR j IN ${c2.name()}
            UPDATE j WITH { tick: i } in ${c2.name()}
            FILTER j.value == 1
          RETURN [i, NEW]
      `;

      var plan = AQL_EXPLAIN(query, {}, opt).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        return node.type;
      });

      assertEqual("SingletonNode", nodeTypes[0], query);
      assertEqual(-1, nodeTypes.indexOf("IndexNode"), query); // no index
      assertNotEqual(-1, nodeTypes.indexOf("FilterNode"), query); // post filter
    },

    testPreventMoveSortPastModify1 : function () {
      c1.ensureIndex({ type: "hash", fields: [ "value" ] });
      c2.ensureIndex({ type: "hash", fields: [ "value" ] });
      c2.ensureIndex({ type: "skiplist", fields: [ "ref" ] });
      var query = `
        FOR i IN ${c1.name()}
          FOR j IN ${c2.name()}
            UPDATE j WITH { tick: i } in ${c2.name()}
            SORT i.value
          RETURN [i, NEW]
      `;

      var plan = AQL_EXPLAIN(query, {}, opt).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        return node.type;
      });

      assertEqual("SingletonNode", nodeTypes[0], query);
      assertEqual(-1, nodeTypes.indexOf("IndexNode"), query); // no index
      assertNotEqual(-1, nodeTypes.indexOf("SortNode"), query); // post filter
    },

    testPreventMoveSortPastModify2 : function () {
      c1.ensureIndex({ type: "hash", fields: [ "value" ] });
      c2.ensureIndex({ type: "hash", fields: [ "value" ] });
      c2.ensureIndex({ type: "skiplist", fields: [ "ref" ] });
      var query = `
        FOR i IN ${c1.name()}
          FOR j IN ${c2.name()}
            UPDATE j WITH { tick: i } in ${c2.name()}
            SORT NEW.value
          RETURN [i, NEW]
      `;

      var plan = AQL_EXPLAIN(query, {}, opt).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        return node.type;
      });

      assertEqual("SingletonNode", nodeTypes[0], query);
      assertEqual(-1, nodeTypes.indexOf("IndexNode"), query); // no index
      assertNotEqual(-1, nodeTypes.indexOf("SortNode"), query); // post filter
    },

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

jsunity.run(optimizerIndexesTestSuite);
jsunity.run(optimizerIndexesModifyTestSuite);
jsunity.run(optimizerIndexesMultiCollectionTestSuite);

return jsunity.done();
