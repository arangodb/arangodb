/*jshint globalstrict:false, strict:false, sub: true, maxlen: 500 */
/*global assertEqual, assertFalse, assertNull */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for query language, bind parameters
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

var db = require("@arangodb").db;
var jsunity = require("jsunity");
var helper = require("@arangodb/aql-helper");
var getQueryMultiplePlansAndExecutions = helper.getQueryMultiplePlansAndExecutions;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ahuacatlRemoveSuite () {
  var edge;
  var cn1 = "UnitTestsAhuacatlRemove1";
  var cn2 = "UnitTestsAhuacatlRemove2";
  var c1;
  var c2;

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      var i;
      db._drop(cn1);
      db._drop(cn2);
      c1 = db._create(cn1);
      c2 = db._create(cn2);

      for (i = 0; i < 100; ++i) {
        c1.save({ _key: "test" + i, value1: i, value2: "test" + i });
      }
      for (i = 0; i < 50; ++i) {
        c2.save({ _key: "test" + i, value1: i, value2: "test" + i });
      }
      edge = db._createEdgeCollection("UnitTestsAhuacatlEdge"); 

      for (i = 0; i < 100; ++i) {
        edge.save("UnitTestsAhuacatlRemove1/foo" + i, "UnitTestsAhuacatlRemove2/bar", { what: i, _key: "test" + i });
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      db._drop(cn1);
      db._drop(cn2);
      db._drop("UnitTestsAhuacatlEdge");
      c1 = null;
      c2 = null;
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove
////////////////////////////////////////////////////////////////////////////////

    testRemoveNothing : function () {
      var expected = { writesExecuted: 0, writesIgnored: 0 };
      var query = "FOR d IN " + cn1 + " FILTER d.value1 < 0 REMOVE d IN " + cn1;

      var allresults = getQueryMultiplePlansAndExecutions(query, {}, this);

      assertEqual(100, c1.count());
      for (var i = 0; i <  allresults.results.length; i++) {
        assertEqual(expected, allresults.results[i].stats,
                    "comparing " + i + " : "  + allresults.results[i].stats);
      }
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test remove
////////////////////////////////////////////////////////////////////////////////

    testRemoveNothingBind : function () {
      var expected = { writesExecuted: 0, writesIgnored: 0 };
      var query = "FOR d IN @@cn FILTER d.value1 < 0 REMOVE d IN @@cn";

      var allresults = getQueryMultiplePlansAndExecutions(query, { "@cn": cn1 }, this);

      assertEqual(100, c1.count());
      for (var i = 0; i <  allresults.results.length; i++) {
        assertEqual(expected, allresults.results[i].stats,
                    "comparing " + i + " : "  + allresults.results[i].stats);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove
////////////////////////////////////////////////////////////////////////////////

    testRemoveIgnore1 : function () {
      var expected = { writesExecuted: 0, writesIgnored: 100 };
      var query = "FOR d IN @@cn REMOVE 'foo' IN @@cn OPTIONS { ignoreErrors: true }";
      var allresults = getQueryMultiplePlansAndExecutions(query, { "@cn": cn1 }, this);

      assertEqual(100, c1.count());
      for (var i = 0; i <  allresults.results.length; i++) {
        assertEqual(expected, allresults.results[i].stats,
                    "comparing " + i + " : "  + allresults.results[i].stats);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove
////////////////////////////////////////////////////////////////////////////////

    testRemoveIgnore2 : function () {
      var expected = { writesExecuted: 100, writesIgnored: 1 };
      var query = "FOR i IN 0..100 REMOVE CONCAT('test', TO_STRING(i)) IN @@cn OPTIONS { ignoreErrors: true }";
      var allresults = getQueryMultiplePlansAndExecutions(query, { "@cn": cn1 }, this);

     assertEqual(0, c1.count());
      for (var i = 0; i <  allresults.results.length; i++) {
        assertEqual(expected, allresults.results[i].stats,
                    "comparing " + i + " : "  + allresults.results[i].stats);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove
////////////////////////////////////////////////////////////////////////////////

    testRemoveAll1 : function () {
      var expected = { writesExecuted: 100, writesIgnored: 0 };
      var query = "FOR d IN @@cn REMOVE d IN @@cn";
      var allresults = getQueryMultiplePlansAndExecutions(query, { "@cn": cn1 }, this);

      assertEqual(0, c1.count());
      for (var i = 0; i <  allresults.results.length; i++) {
        assertEqual(expected, allresults.results[i].stats,
                    "comparing " + i + " : "  + allresults.results[i].stats);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove
////////////////////////////////////////////////////////////////////////////////

    testRemoveAll2 : function () {
      var expected = { writesExecuted: 100, writesIgnored: 0 };
      var query = "FOR d IN @@cn REMOVE d._key IN @@cn";
      var allresults = getQueryMultiplePlansAndExecutions(query, { "@cn": cn1 }, this);

      assertEqual(0, c1.count());
      for (var i = 0; i <  allresults.results.length; i++) {
        assertEqual(expected, allresults.results[i].stats,
                    "comparing " + i + " : "  + allresults.results[i].stats);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove
////////////////////////////////////////////////////////////////////////////////

    testRemoveAll3 : function () {
      var expected = { writesExecuted: 100, writesIgnored: 0 };
      var query = "FOR d IN @@cn REMOVE { _key: d._key } IN @@cn";
      var allresults = getQueryMultiplePlansAndExecutions(query, { "@cn": cn1 }, this);

      assertEqual(0, c1.count());
      for (var i = 0; i <  allresults.results.length; i++) {
        assertEqual(expected, allresults.results[i].stats,
                    "comparing " + i + " : "  + allresults.results[i].stats);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove
////////////////////////////////////////////////////////////////////////////////

    testRemoveAll4 : function () {
      var expected = { writesExecuted: 100, writesIgnored: 0 };
      var query = "FOR i IN 0..99 REMOVE { _key: CONCAT('test', TO_STRING(i)) } IN @@cn";
      var allresults = getQueryMultiplePlansAndExecutions(query, { "@cn": cn1 }, this);

      assertEqual(0, c1.count());
      for (var i = 0; i <  allresults.results.length; i++) {
        assertEqual(expected, allresults.results[i].stats,
                    "comparing " + i + " : "  + allresults.results[i].stats);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove
////////////////////////////////////////////////////////////////////////////////

    testRemoveAll5 : function () {
      var expected = { writesExecuted: 100, writesIgnored: 0 };
      var query = "FOR d IN @@cn REMOVE d INTO @@cn";
      var allresults = getQueryMultiplePlansAndExecutions(query, { "@cn": cn1 }, this);

      assertEqual(0, c1.count());
      for (var i = 0; i <  allresults.results.length; i++) {
        assertEqual(expected, allresults.results[i].stats,
                    "comparing " + i + " : "  + allresults.results[i].stats);
      }
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test remove
////////////////////////////////////////////////////////////////////////////////

    testRemoveHalf : function () {
      var expected = { writesExecuted: 50, writesIgnored: 0 };
      var query = "FOR i IN 0..99 FILTER i % 2 == 0 REMOVE { _key: CONCAT('test', TO_STRING(i)) } IN @@cn";
      var allresults = getQueryMultiplePlansAndExecutions(query, { "@cn": cn1 }, this);

      assertEqual(50, c1.count());
      for (var i = 0; i <  allresults.results.length; i++) {
        assertEqual(expected, allresults.results[i].stats,
                    "comparing " + i + " : "  + allresults.results[i].stats);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove
////////////////////////////////////////////////////////////////////////////////

    testRemoveSingle : function () {
      var expected = { writesExecuted: 1, writesIgnored: 0 };
      var query = "REMOVE 'test0' IN @@cn";
      var allresults = getQueryMultiplePlansAndExecutions(query, { "@cn": cn1 }, this);

      assertEqual(99, c1.count());
      for (var i = 0; i <  allresults.results.length; i++) {
        assertEqual(expected, allresults.results[i].stats,
                    "comparing " + i + " : "  + allresults.results[i].stats);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove
////////////////////////////////////////////////////////////////////////////////

    testRemoveTwoCollectionsJoin1 : function () {
      var expected = { writesExecuted: 50, writesIgnored: 0 };
      var query = "FOR d IN @@cn1 FILTER d.value1 < 50 REMOVE { _key: d._key } IN @@cn2";
      var allresults = getQueryMultiplePlansAndExecutions(query, { "@cn1": cn1, "@cn2": cn2 }, this);

      assertEqual(100, c1.count());
      assertEqual(0, c2.count());
      for (var i = 0; i <  allresults.results.length; i++) {
        assertEqual(expected, allresults.results[i].stats,
                    "comparing " + i + " : "  + allresults.results[i].stats);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove
////////////////////////////////////////////////////////////////////////////////

    testRemoveTwoCollectionsJoin2 : function () {
      var expected = { writesExecuted: 48, writesIgnored: 0 };
      var query = "FOR d IN @@cn1 FILTER d.value1 >= 2 && d.value1 < 50 REMOVE { _key: d._key } IN @@cn2";
      var allresults = getQueryMultiplePlansAndExecutions(query, { "@cn1": cn1, "@cn2": cn2 }, this);

      assertEqual(100, c1.count());
      assertEqual(2, c2.count());
      for (var i = 0; i <  allresults.results.length; i++) {
        assertEqual(expected, allresults.results[i].stats,
                    "comparing " + i + " : "  + allresults.results[i].stats);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove
////////////////////////////////////////////////////////////////////////////////

    testRemoveTwoCollectionsIgnoreErrors1 : function () {
      var expected = { writesExecuted: 50, writesIgnored: 50 };
      var query = "FOR d IN @@cn1 REMOVE { _key: d._key } IN @@cn2 OPTIONS { ignoreErrors: true }";
      var allresults = getQueryMultiplePlansAndExecutions(query, { "@cn1": cn1, "@cn2": cn2 }, this);

      assertEqual(100, c1.count());
      assertEqual(0, c2.count());
      for (var i = 0; i <  allresults.results.length; i++) {
        assertEqual(expected, allresults.results[i].stats,
                    "comparing " + i + " : "  + allresults.results[i].stats);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove
////////////////////////////////////////////////////////////////////////////////

    testRemoveTwoCollectionsIgnoreErrors2 : function () {
      var expected = { writesExecuted: 0, writesIgnored: 100 };
      var query = "FOR d IN @@cn1 REMOVE { _key: CONCAT('foo', d._key) } IN @@cn2 OPTIONS { ignoreErrors: true }";
      var allresults = getQueryMultiplePlansAndExecutions(query, { "@cn1": cn1, "@cn2": cn2 }, this);

      assertEqual(100, c1.count());
      assertEqual(50, c2.count());
      for (var i = 0; i <  allresults.results.length; i++) {
        assertEqual(expected, allresults.results[i].stats,
                    "comparing " + i + " : "  + allresults.results[i].stats);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove
////////////////////////////////////////////////////////////////////////////////

    testRemoveWaitForSync : function () {
      var expected = { writesExecuted: 100, writesIgnored: 0 };
      var query = "FOR d IN @@cn REMOVE d IN @@cn OPTIONS { waitForSync: true }";
      var allresults = getQueryMultiplePlansAndExecutions(query, { "@cn": cn1 }, this);

      assertEqual(0, c1.count());
      for (var i = 0; i <  allresults.results.length; i++) {
        assertEqual(expected, allresults.results[i].stats,
                    "comparing " + i + " : "  + allresults.results[i].stats);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove
////////////////////////////////////////////////////////////////////////////////

    testRemoveEdge : function () {
      var expected = { writesExecuted: 10, writesIgnored: 0 };
      var query = "FOR i IN 0..9 REMOVE CONCAT('test', TO_STRING(i)) IN @@cn";
      var allresults = getQueryMultiplePlansAndExecutions(query, { "@cn": edge.name() }, this);

      assertEqual(100, c1.count());
      assertEqual(90, edge.count());
      for (var i = 0; i <  allresults.results.length; i++) {
        assertEqual(expected, allresults.results[i].stats,
                    "comparing " + i + " : "  + allresults.results[i].stats);
      }
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ahuacatlInsertSuite () {
  var cn1 = "UnitTestsAhuacatlInsert1";
  var cn2 = "UnitTestsAhuacatlInsert2";
  var c1;
  var c2;
  var edge;

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      db._drop(cn1);
      db._drop(cn2);
      c1 = db._create(cn1);
      c2 = db._create(cn2);

      for (var i = 0; i < 100; ++i) {
        c1.save({ _key: "test" + i, value1: i, value2: "test" + i });
      }
      for (i = 0; i < 50; ++i) {
        c2.save({ _key: "test" + i, value1: i, value2: "test" + i });
      }
      db._drop("UnitTestsAhuacatlEdge");
      edge = db._createEdgeCollection("UnitTestsAhuacatlEdge"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      db._drop("UnitTestsAhuacatlEdge");
      db._drop(cn1);
      db._drop(cn2);
      c1 = null;
      c2 = null;
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test insert
////////////////////////////////////////////////////////////////////////////////

    testInsertNothing : function () {
      var expected = { writesExecuted: 0, writesIgnored: 0 };
      var query = "FOR d IN " + cn1 + " FILTER d.value1 < 0 INSERT { foxx: true } IN " + cn1;
      var allresults = getQueryMultiplePlansAndExecutions(query, {}, this);
    
      assertEqual(100, c1.count());
      for (var i = 0; i < allresults.results.length; i++) {
        assertEqual(expected, allresults.results[i].stats,
                    "comparing " + i + " : "  + allresults.results[i].stats);
      }
    },
////////////////////////////////////////////////////////////////////////////////
/// @brief test insert
////////////////////////////////////////////////////////////////////////////////

    testInsertNothingBind : function () {
      var expected = { writesExecuted: 0, writesIgnored: 0 };
      var query = "FOR d IN @@cn FILTER d.value1 < 0 INSERT { foxx: true } IN @@cn";
      var allresults = getQueryMultiplePlansAndExecutions(query, { "@cn": cn1 }, this);

      assertEqual(100, c1.count());
      for (var i = 0; i < allresults.results.length; i++) {
        assertEqual(expected, allresults.results[i].stats,
                    "comparing " + i + " : "  + allresults.results[i].stats);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test insert
////////////////////////////////////////////////////////////////////////////////

    testInsertIgnore1 : function () {
      var expected = { writesExecuted: 0, writesIgnored: 100 };
      var query = "FOR d IN @@cn INSERT d IN @@cn OPTIONS { ignoreErrors: true }";
      var allresults = getQueryMultiplePlansAndExecutions(query, { "@cn": cn1 }, this);

      assertEqual(100, c1.count());
      for (var i = 0; i < allresults.results.length; i++) {
        assertEqual(expected, allresults.results[i].stats,
                    "comparing " + i + " : "  + allresults.results[i].stats);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test insert
////////////////////////////////////////////////////////////////////////////////

    testInsertIgnore2 : function () {
      var expected = { writesExecuted: 1, writesIgnored: 50 };
      var query = "FOR i IN 50..100 INSERT { _key: CONCAT('test', TO_STRING(i)) } IN @@cn OPTIONS { ignoreErrors: true }";
      var allresults = getQueryMultiplePlansAndExecutions(query, { "@cn": cn1 }, this);

      assertEqual(101, c1.count());
      for (var i = 0; i < allresults.results.length; i++) {
        assertEqual(expected, allresults.results[i].stats,
                    "comparing " + i + " : "  + allresults.results[i].stats);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test insert
////////////////////////////////////////////////////////////////////////////////

    testInsertIgnore3 : function () {
      var expected = { writesExecuted: 51, writesIgnored: 50 };
      var query = "FOR i IN 0..100 INSERT { _key: CONCAT('test', TO_STRING(i)) } IN @@cn OPTIONS { ignoreErrors: true }";
      var allresults = getQueryMultiplePlansAndExecutions(query, { "@cn": cn2 }, this);

      assertEqual(101, c2.count());
      for (var i = 0; i <  allresults.results.length; i++) {
        assertEqual(expected, allresults.results[i].stats,
                    "comparing " + i + " : "  + allresults.results[i].stats);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test insert
////////////////////////////////////////////////////////////////////////////////

    testInsertIgnore4 : function () {
      var expected = { writesExecuted: 0, writesIgnored: 100 };
      var query = "FOR i IN 0..99 INSERT { _key: CONCAT('test', TO_STRING(i)) } IN @@cn OPTIONS { ignoreErrors: true }";
      var allresults = getQueryMultiplePlansAndExecutions(query, { "@cn": cn1 }, this);

      assertEqual(100, c1.count());
      for (var i = 0; i <  allresults.results.length; i++) {
        assertEqual(expected, allresults.results[i].stats,
                    "comparing " + i + " : "  + allresults.results[i].stats);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test insert
////////////////////////////////////////////////////////////////////////////////

    testInsertIgnore5 : function () {
      var expected = { writesExecuted: 50, writesIgnored: 50 };
      var query = "FOR i IN 0..99 INSERT { _key: CONCAT('test', TO_STRING(i)) } IN @@cn OPTIONS { ignoreErrors: true }";
      var allresults = getQueryMultiplePlansAndExecutions(query, { "@cn": cn2 }, this);

      assertEqual(100, c2.count());
      for (var i = 0; i <  allresults.results.length; i++) {
        assertEqual(expected, allresults.results[i].stats,
                    "comparing " + i + " : "  + allresults.results[i].stats);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test insert
////////////////////////////////////////////////////////////////////////////////

    testInsertEmpty : function () {
      var expected = { writesExecuted: 100, writesIgnored: 0 };
      var query = "FOR d IN @@cn INSERT { } IN @@cn";
      var allresults = getQueryMultiplePlansAndExecutions(query, { "@cn": cn1 }, this);

      assertEqual(200, c1.count());
      for (var i = 0; i <  allresults.results.length; i++) {
        assertEqual(expected, allresults.results[i].stats,
                    "comparing " + i + " : "  + allresults.results[i].stats);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test insert
////////////////////////////////////////////////////////////////////////////////

    testInsertCopy : function () {
      var expected = { writesExecuted: 50, writesIgnored: 0 };
      var query = "FOR i IN 50..99 INSERT { _key: CONCAT('test', TO_STRING(i)) } IN @@cn";
      var allresults = getQueryMultiplePlansAndExecutions(query, { "@cn": cn2 }, this);

      assertEqual(100, c1.count());
      assertEqual(100, c2.count());
      for (var i = 0; i <  allresults.results.length; i++) {
        assertEqual(expected, allresults.results[i].stats,
                    "comparing " + i + " : "  + allresults.results[i].stats);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test insert
////////////////////////////////////////////////////////////////////////////////

    testSingleInsert : function () {
      var expected = { writesExecuted: 1, writesIgnored: 0 };
      var query = "INSERT { value: 'foobar', _key: 'test' } IN @@cn";
      var allresults = getQueryMultiplePlansAndExecutions(query, { "@cn": cn1 }, this);

      assertEqual(101, c1.count());
      assertEqual("foobar", c1.document("test").value);
      for (var i = 0; i <  allresults.results.length; i++) {
        assertEqual(expected, allresults.results[i].stats,
                    "comparing " + i + " : "  + allresults.results[i].stats);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test insert
////////////////////////////////////////////////////////////////////////////////

    testInsertWaitForSync : function () {
      var expected = { writesExecuted: 50, writesIgnored: 0 };
      var query = "FOR i IN 1..50 INSERT { value: i } INTO @@cn OPTIONS { waitForSync: true }";
      var allresults = getQueryMultiplePlansAndExecutions(query, { "@cn": cn2 }, this);

      assertEqual(100, c1.count());
      for (var i = 0; i <  allresults.results.length; i++) {
        assertEqual(expected, allresults.results[i].stats,
                    "comparing " + i + " : "  + allresults.results[i].stats);
      }
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test insert
////////////////////////////////////////////////////////////////////////////////

    testInsertEdge : function () {
      var i;
      var expected = { writesExecuted: 50, writesIgnored: 0 };
      var query = "FOR i IN 1..50 INSERT { _key: CONCAT('test', TO_STRING(i)), _from: CONCAT('UnitTestsAhuacatlInsert1/', TO_STRING(i)), _to: CONCAT('UnitTestsAhuacatlInsert2/', TO_STRING(i)), value: [ i ], sub: { foo: 'bar' } } INTO @@cn";
      var allresults = getQueryMultiplePlansAndExecutions(query, { "@cn": edge.name() }, this);

      for (i = 0; i < allresults.results.length; i++) {
        assertEqual(expected, allresults.results[i].stats,
                    "comparing " + i + " : "  + allresults.results[i].stats);
      }
      assertEqual(50, edge.count());

      for (i = 1; i <= 50; ++i) {
        var doc = edge.document("test" + i);
        assertEqual("UnitTestsAhuacatlInsert1/" + i, doc._from);
        assertEqual("UnitTestsAhuacatlInsert2/" + i, doc._to);
        assertEqual([ i ], doc.value);
        assertEqual({ foo: "bar" }, doc.sub);
      }
    }
   
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ahuacatlUpdateSuite () {
  var cn1 = "UnitTestsAhuacatlUpdate1";
  var cn2 = "UnitTestsAhuacatlUpdate2";
  var c1;
  var c2;

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      var i;
      db._drop(cn1);
      db._drop(cn2);
      c1 = db._create(cn1);
      c2 = db._create(cn2);

      for (i = 0; i < 100; ++i) {
        c1.save({ _key: "test" + i, value1: i, value2: "test" + i });
      }
      for (i = 0; i < 50; ++i) {
        c2.save({ _key: "test" + i, value1: i, value2: "test" + i });
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      db._drop(cn1);
      db._drop(cn2);
      c1 = null;
      c2 = null;
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test update
////////////////////////////////////////////////////////////////////////////////

    testUpdateNothing : function () {
      var expected = { writesExecuted: 0, writesIgnored: 0 };
      var query = "FOR d IN " + cn1 + " FILTER d.value1 < 0 UPDATE { foxx: true } IN " + cn1;
      var allresults = getQueryMultiplePlansAndExecutions(query, {}, this);

      for (var i = 0; i < allresults.results.length; i++) {
        assertEqual(expected, allresults.results[i].stats,
                    "comparing " + i + " : "  + allresults.results[i].stats);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test update
////////////////////////////////////////////////////////////////////////////////

    testUpdateNothingBind : function () {
      var expected = { writesExecuted: 0, writesIgnored: 0 };
      var query = "FOR d IN @@cn FILTER d.value1 < 0 UPDATE { foxx: true } IN @@cn";
      var allresults = getQueryMultiplePlansAndExecutions(query, { "@cn": cn1 });

      for (var i = 0; i < allresults.results.length; i++) {
        assertEqual(expected, allresults.results[i].stats,
                    "comparing " + i + " : "  + allresults.results[i].stats);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test update
////////////////////////////////////////////////////////////////////////////////

    testUpdateIgnore1 : function () {
      c1.ensureUniqueConstraint("value3", { sparse: true });
      var expected = { writesExecuted: 1, writesIgnored: 99 };
      var query = "FOR d IN @@cn UPDATE d WITH { value3: 1 } IN @@cn OPTIONS { ignoreErrors: true }";
      var allresults = getQueryMultiplePlansAndExecutions(query, { "@cn": cn1 });

      for (var i = 0; i < allresults.results.length; i++) {
        assertEqual(expected, allresults.results[i].stats,
                    "comparing " + i + " : "  + allresults.results[i].stats);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test update
////////////////////////////////////////////////////////////////////////////////

    testUpdateIgnore2 : function () {
      c1.ensureUniqueConstraint("value1", { sparse: true });
      var expected = { writesExecuted: 0, writesIgnored: 51 };
      var query = "FOR i IN 50..100 UPDATE { _key: CONCAT('test', TO_STRING(i)), value1: 1 } IN @@cn OPTIONS { ignoreErrors: true }";
      var allresults = getQueryMultiplePlansAndExecutions(query, { "@cn": cn1 });

      for (var i = 0; i < allresults.results.length; i++) {
        assertEqual(expected, allresults.results[i].stats,
                    "comparing " + i + " : "  + allresults.results[i].stats);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test update
////////////////////////////////////////////////////////////////////////////////

    testUpdateEmpty1 : function () {
      var i;
      var expected = { writesExecuted: 100, writesIgnored: 0 };
      var query = "FOR d IN @@cn UPDATE { _key: d._key } IN @@cn";
      var allresults = getQueryMultiplePlansAndExecutions(query, { "@cn": cn1 });

      for (i = 0; i < allresults.results.length; i++) {
        assertEqual(expected, allresults.results[i].stats,
                    "comparing " + i + " : "  + allresults.results[i].stats);
      }
      for (i = 0; i < 100; ++i) {
        var doc = c1.document("test" + i);
        assertEqual(i, doc.value1);
        assertEqual("test" + i, doc.value2);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test update
////////////////////////////////////////////////////////////////////////////////

    testUpdateEmpty2 : function () {
      var i;
      var expected = { writesExecuted: 100, writesIgnored: 0 };
      var query = "FOR d IN @@cn UPDATE d IN @@cn";
      var allresults = getQueryMultiplePlansAndExecutions(query, { "@cn": cn1 });

      for (i = 0; i < allresults.results.length; i++) {
        assertEqual(expected, allresults.results[i].stats,
                    "comparing " + i + " : "  + allresults.results[i].stats);
      }
      for (i = 0; i < 100; ++i) {
        var doc = c1.document("test" + i);
        assertEqual(i, doc.value1);
        assertEqual("test" + i, doc.value2);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test update
////////////////////////////////////////////////////////////////////////////////

    testSingleUpdate : function () {
      var expected = { writesExecuted: 1, writesIgnored: 0 };
      var query = "UPDATE { value: 'foobar', _key: 'test17' } IN @@cn";
      var allresults = getQueryMultiplePlansAndExecutions(query, { "@cn": cn1 });

      assertEqual("foobar", c1.document("test17").value);
      for (var i = 0; i < allresults.results.length; i++) {
        assertEqual(expected, allresults.results[i].stats,
                    "comparing " + i + " : "  + allresults.results[i].stats);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test update
////////////////////////////////////////////////////////////////////////////////

    testUpdateOldValue : function () {
      var i;
      var expected = { writesExecuted: 100, writesIgnored: 0 };
      var query = "FOR d IN @@cn UPDATE { _key: d._key, value1: d.value2, value2: d.value1, value3: d.value1 + 5 } IN @@cn";
      var allresults = getQueryMultiplePlansAndExecutions(query, { "@cn": cn1 }, this);
      for (i = 0; i < allresults.results.length; i++) {
        assertEqual(expected, allresults.results[i].stats,
                    "comparing " + i + " : "  + allresults.results[i].stats);
      }

      for (i = 0; i < 100; ++i) {
        var doc = c1.document("test" + i);
        assertEqual("test" + i, doc.value1);
        assertEqual(i, doc.value2);
        assertEqual(i + 5, doc.value3);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test update
////////////////////////////////////////////////////////////////////////////////

    testUpdateWaitForSync : function () {
      var i;
      var expected = { writesExecuted: 50, writesIgnored: 0 };
      var query = "FOR i IN 1..50 UPDATE { _key: CONCAT('test', TO_STRING(i)) } INTO @@cn OPTIONS { waitForSync: true }";
      var allresults = getQueryMultiplePlansAndExecutions(query, { "@cn": cn1 });
      for (i = 0; i < allresults.results.length; i++) {
        assertEqual(expected, allresults.results[i].stats,
                    "comparing " + i + " : "  + allresults.results[i].stats);
      }

      for (i = 0; i < 100; ++i) {
        var doc = c1.document("test" + i);
        assertEqual(i, doc.value1);
        assertEqual("test" + i, doc.value2);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test update
////////////////////////////////////////////////////////////////////////////////

    testUpdateKeepNullDefault : function () {
      var i;
      var expected = { writesExecuted: 100, writesIgnored: 0 };
      var query = "FOR d IN @@cn UPDATE d._key WITH { value1: null, value3: 'foobar', value9: null } INTO @@cn";
      var allresults = getQueryMultiplePlansAndExecutions(query, { "@cn": cn1 });
      for (i = 0; i < allresults.results.length; i++) {
        assertEqual(expected, allresults.results[i].stats,
                    "comparing " + i + " : "  + allresults.results[i].stats);
      }

      for (i = 0; i < 100; ++i) {
        var doc = c1.document("test" + i);
        assertNull(doc.value1);
        assertEqual("test" + i, doc.value2);
        assertEqual("foobar", doc.value3);
        assertNull(doc.value9);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test update
////////////////////////////////////////////////////////////////////////////////

    testUpdateKeepNullTrue : function () {
      var i;
      var expected = { writesExecuted: 100, writesIgnored: 0 };
      var query = "FOR d IN @@cn UPDATE d._key WITH { value1: null, value3: 'foobar', value9: null } INTO @@cn OPTIONS { keepNull: true }";
      var allresults = getQueryMultiplePlansAndExecutions(query, { "@cn": cn1 });
      for (i = 0; i < allresults.results.length; i++) {
        assertEqual(expected, allresults.results[i].stats,
                    "comparing " + i + " : "  + allresults.results[i].stats);
      }

      for (i = 0; i < 100; ++i) {
        var doc = c1.document("test" + i);
        assertNull(doc.value1);
        assertEqual("test" + i, doc.value2);
        assertEqual("foobar", doc.value3);
        assertNull(doc.value9);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test update
////////////////////////////////////////////////////////////////////////////////

    testUpdateKeepNullFalse : function () {
      var i;
      var expected = { writesExecuted: 100, writesIgnored: 0 };
      var query = "FOR d IN @@cn UPDATE d._key WITH { value1: null, value3: 'foobar', value9: null } INTO @@cn OPTIONS { keepNull: false }";
      var allresults = getQueryMultiplePlansAndExecutions(query, { "@cn": cn1 });
      for (i = 0; i < allresults.results.length; i++) {
        assertEqual(expected, allresults.results[i].stats,
                    "comparing " + i + " : "  + allresults.results[i].stats);
      }

      for (i = 0; i < 100; ++i) {
        var doc = c1.document("test" + i);
        assertFalse(doc.hasOwnProperty("value1"));
        assertEqual("test" + i, doc.value2);
        assertEqual("foobar", doc.value3);
        assertFalse(doc.hasOwnProperty("value9"));
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test update
////////////////////////////////////////////////////////////////////////////////

    testUpdateFilter : function () {
      var i;
      var expected = { writesExecuted: 50, writesIgnored: 0 };
      var query = "FOR d IN @@cn FILTER d.value1 % 2 == 0 UPDATE d._key WITH { value2: 100 } INTO @@cn";
      var allresults = getQueryMultiplePlansAndExecutions(query, { "@cn": cn1 }, this);

      for (i = 0; i < allresults.results.length; i++) {
        assertEqual(expected, allresults.results[i].stats,
                    "comparing " + i + " : "  + allresults.results[i].stats);
      }

      for (i = 0; i < 100; ++i) {
        var doc = c1.document("test" + i);
        if (i % 2 === 0) {
          assertEqual(100, doc.value2);
        }
        else {
          assertEqual("test" + i, doc.value2);
        }
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test update
////////////////////////////////////////////////////////////////////////////////

    testUpdateUpdate : function () {
      var i, j;
      var expected = { writesExecuted: 100, writesIgnored: 0 };
      var query = "FOR d IN @@cn UPDATE d._key WITH { counter: HAS(d, 'counter') ? d.counter + 1 : 1 } INTO @@cn";

      for (j = 0; j < 5; ++j) {

        var allresults = getQueryMultiplePlansAndExecutions(query, { "@cn": cn1 });
        for (i = 0; i < allresults.results.length; i++) {
          assertEqual(expected, allresults.results[i].stats,
                      "comparing " + i + " : "  + allresults.results[i].stats);
        }
      }

      for (i = 0; i < 100; ++i) {
        var doc = c1.document("test" + i);
        assertEqual(10, doc.counter);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test replace
////////////////////////////////////////////////////////////////////////////////

    testReplace1 : function () {
      var i, j;
      var expected = { writesExecuted: 100, writesIgnored: 0 };
      for (j = 0; j < 5; ++j) {
        var query = "FOR d IN @@cn REPLACE d._key WITH { value4: 12 } INTO @@cn";
        var allresults = getQueryMultiplePlansAndExecutions(query, { "@cn": cn1 });
        for (i = 0; i < allresults.results.length; i++) {
          assertEqual(expected, allresults.results[i].stats,
                      "comparing " + i + " : "  + allresults.results[i].stats);
        }
      }

      for (i = 0; i < 100; ++i) {
        var doc = c1.document("test" + i);
        assertFalse(doc.hasOwnProperty("value1"));
        assertFalse(doc.hasOwnProperty("value2"));
        assertFalse(doc.hasOwnProperty("value3"));
        assertEqual(12, doc.value4);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test replace
////////////////////////////////////////////////////////////////////////////////

    testReplace2 : function () {
      var i, j;
      var expected = { writesExecuted: 100, writesIgnored: 0 };
      for (j = 0; j < 5; ++j) {
        var query = "FOR d IN @@cn REPLACE { _key: d._key, value4: 13 } INTO @@cn";
        var allresults = getQueryMultiplePlansAndExecutions(query, { "@cn": cn1 });
        for (i = 0; i < allresults.results.length; i++) {
          assertEqual(expected, allresults.results[i].stats,
                      "comparing " + i + " : "  + allresults.results[i].stats);
        }
      }

      for (i = 0; i < 100; ++i) {
        var doc = c1.document("test" + i);
        assertFalse(doc.hasOwnProperty("value1"));
        assertFalse(doc.hasOwnProperty("value2"));
        assertFalse(doc.hasOwnProperty("value3"));
        assertEqual(13, doc.value4);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test replace
////////////////////////////////////////////////////////////////////////////////

    testReplaceReplace : function () {
      var i, j;
      var expected = { writesExecuted: 100, writesIgnored: 0 };
      for (j = 0; j < 5; ++j) {
        var query = "FOR d IN @@cn REPLACE d._key WITH { value1: d.value1 + 1 } INTO @@cn";
        var allresults = getQueryMultiplePlansAndExecutions(query, { "@cn": cn1 }, this);
        for (i = 0; i < allresults.results.length; i++) {
          assertEqual(expected, allresults.results[i].stats,
                      "comparing " + i + " : "  + allresults.results[i].stats);
        }
      }

      for (i = 0; i < 100; ++i) {
        var doc = c1.document("test" + i);
        assertEqual(i + 1, doc.value1);
        assertFalse(doc.hasOwnProperty("value2"));
      }
    }

  };

}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ahuacatlRemoveSuite);
jsunity.run(ahuacatlInsertSuite);
jsunity.run(ahuacatlUpdateSuite);

return jsunity.done();

