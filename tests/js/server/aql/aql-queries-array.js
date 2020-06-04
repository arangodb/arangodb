/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertNotEqual, assertTrue, AQL_EXPLAIN, AQL_EXECUTE */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for array indexes in AQL
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2015 ArangoDB GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Michael Hackstein
/// @author Copyright 2015, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
var db = require("@arangodb").db;
var isCluster = require("@arangodb/cluster").isCluster();

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function arrayIndexSuite () {

  const cName = "UnitTestsArray";
  var col;
  const buildTags = function (i) {
    var tags = [];
    if (i % 10 === 0) {
      // Every tenth Array has the tag "tenth"
      tags.push("tenth");
    }
    if (i % 50 === 0) {
      // Every fifties Array has an explicit tag null
      tags.push(null);
    }
    if (i % 3 === 0) {
      // Every third Array has the tag duplicate multiple times
      tags.push("duplicate");
      tags.push("duplicate");
      tags.push("duplicate");
    }
    // Shuffle the array randomly
    return tags;
  };

  var checkIsOptimizedQuery = function (query, bindVars) {
    var plan = AQL_EXPLAIN(query, bindVars).plan;
    var nodeTypes = plan.nodes.map(function(node) {
      return node.type;
    });
    assertEqual("SingletonNode", nodeTypes[0], query);
    assertEqual(-1, nodeTypes.indexOf("EnumerateCollection"),
                "found EnumerateCollection node for:" + query);
    assertNotEqual(-1, nodeTypes.indexOf("IndexNode"),
                   "no index used for: " + query);
                 /*
    assertEqual(-1, nodeTypes.indexOf("FilterNode"),
                "found Filter node for:" + query);
              */
  };

  var validateIndexNotUsed = function (query, bindVars) {
    var plan = AQL_EXPLAIN(query, bindVars || {}).plan;
    var nodeTypes = plan.nodes.map(function(node) {
      return node.type;
    });
    assertEqual(-1, nodeTypes.indexOf("IndexNode"),
                "index used for: " + query);
  };

  var validateResults = function (query) {
    var bindVars = {};
    bindVars.tag = "tenth";

    checkIsOptimizedQuery(query, bindVars);
    // Assert the result
    var actual = AQL_EXECUTE(query, bindVars);
    assertTrue(actual.stats.scannedIndex > 0);
    assertEqual(actual.stats.scannedFull, 0);
    assertEqual(actual.json.length, 10);
    for (var i = 0; i < actual.json.length; ++i) {
      assertEqual(parseInt(actual.json[i]) % 10, 0, "A tenth _key is not divisble by ten: " + actual);
    }

    bindVars.tag = "duplicate";
    checkIsOptimizedQuery(query, bindVars);
    actual = AQL_EXECUTE(query, bindVars);
    assertTrue(actual.stats.scannedIndex > 0);
    assertEqual(actual.stats.scannedFull, 0);
    // No element is duplicated
    assertEqual(actual.json.length, 34);
    var last = -1;
    for (i = 0; i < actual.length; ++i) {
      assertEqual(parseInt(actual.json[i]) % 3, 0, "A duplicate _key is not divisble by 3: " + actual);
      assertTrue(last < parseInt(actual.json[i]));
      last = parseInt(actual.json[i]);
    }

    bindVars.tag = null;
    checkIsOptimizedQuery(query, bindVars);
    actual = AQL_EXECUTE(query, bindVars);
    // We check if we found the Arrays with NULL in it
    assertNotEqual(-1, actual.json.indexOf("0t"), "Did not find the null array");
    assertNotEqual(-1, actual.json.indexOf("50t"), "Did not find the null array");
    assertEqual(-1, actual.json.indexOf("empty"), "Did find the empty document");
    assertEqual(-1, actual.json.indexOf("emptyArray"), "Did find the empty array");
    assertEqual(-1, actual.json.indexOf("noArray"), "Did find the document with no array");
    assertEqual(-1, actual.json.indexOf("null"), "Did find the document set to null");
  };

  var validateResultsOr = function (query, sparse) {
    var bindVars = {};
    bindVars.tag1 = "tenth";
    bindVars.tag2 = "duplicate";

    checkIsOptimizedQuery(query, bindVars);
    // Assert the result
    var actual = AQL_EXECUTE(query, bindVars);
    assertTrue(actual.stats.scannedIndex > 0);
    assertEqual(actual.stats.scannedFull, 0);
    assertEqual(actual.json.length, 40); // 10 + 34 - 4
    var last = "0";
    for (var i = 0; i < actual.json.length; ++i) {
      var key = parseInt(actual.json[i]);
      if (key % 3 !== 0) {
        assertEqual(key % 10, 0, "A tenth _key is not divisble by ten: " + actual.json[i]);
      }
      assertTrue(last < actual.json[i], last + " < " + actual.json[i]);
      last = actual.json[i];
    }

    bindVars.tag1 = null;
    if (!sparse) {
      checkIsOptimizedQuery(query, bindVars);
    }
    else {
      validateIndexNotUsed(query, bindVars);
    }
    actual = AQL_EXECUTE(query, bindVars);
    // We check if we found the Arrays with NULL in it
    assertNotEqual(-1, actual.json.indexOf("0t"), "Did not find the null array");
    assertNotEqual(-1, actual.json.indexOf("50t"), "Did not find the null array");
    assertEqual(-1, actual.json.indexOf("empty"), "Did find the empty document");
    assertEqual(-1, actual.json.indexOf("emptyArray"), "Did find the empty array");
    assertEqual(-1, actual.json.indexOf("noArray"), "Did find the document with no array");
    assertEqual(-1, actual.json.indexOf("null"), "Did find the document set to null");
  };

  var validateCombinedResults = function (query) {
    var bindVars = {};
    bindVars.tag = "tenth";
    bindVars.list = [5, 10, 15, 20];

    checkIsOptimizedQuery(query, bindVars);
    // Assert the result
    var actual = AQL_EXECUTE(query, bindVars);
    assertTrue(actual.stats.scannedIndex > 0);
    assertEqual(actual.stats.scannedFull, 0);
    assertEqual(actual.json.length, 2);
    assertEqual(parseInt(actual.json[0]._key), 10);
    assertEqual(parseInt(actual.json[1]._key), 20);

    bindVars.tag = "duplicate";
    bindVars.list = [3, 4, 5, 6, 7, 8, 9];
    checkIsOptimizedQuery(query, bindVars);
    actual = AQL_EXECUTE(query, bindVars);
    assertTrue(actual.stats.scannedIndex > 0);
    assertEqual(actual.stats.scannedFull, 0);
    // No element is duplicated
    assertEqual(actual.json.length, 3);
    assertEqual(parseInt(actual.json[0]._key), 3);
    assertEqual(parseInt(actual.json[1]._key), 6);
    assertEqual(parseInt(actual.json[2]._key), 9);
  };

  return {

    setUp : function () {
      this.tearDown();
      col = db._create(cName);
    },

    tearDown : function () {
      db._drop(cName);
    },

    testMultipleFilters : function () {
      col.ensureHashIndex("a", "b[*]");
      col.save({ a: true, b: [1, 2, 3], c: [1, 2, 3] });

      let query = `FOR x IN ${cName} FILTER x.a == true && 2 IN x.b && 4 IN x.c RETURN x`;
      assertEqual(0, AQL_EXECUTE(query).json.length);
      
      query = `FOR x IN ${cName} FILTER x.a == true && NOOPT(2) IN x.b && NOOPT(4) IN x.c RETURN x`;
      assertEqual(0, AQL_EXECUTE(query).json.length);
      
      query = `FOR x IN ${cName} FILTER x.a == true && 2 IN x.b && 3 IN x.c RETURN x`;
      assertEqual(1, AQL_EXECUTE(query).json.length);
      
      query = `FOR x IN ${cName} FILTER x.a == true && NOOPT(2) IN x.b && NOOPT(3) IN x.c RETURN x`;
      assertEqual(1, AQL_EXECUTE(query).json.length);
    },
    
    testMultipleFiltersWithOuterLoop : function () {
      col.ensureHashIndex("a", "b[*]");
      col.save({ a: true, b: [1, 2, 3], c: [4, 5, 6] });

      let query = `FOR y IN [1, 2, 3] FOR x IN ${cName} FILTER x.a == true && y IN x.b && y IN x.c RETURN x`;
      assertEqual(0, AQL_EXECUTE(query).json.length);
      
      query = `RETURN LENGTH(FOR y IN [1, 2, 3] FOR x IN ${cName} FILTER x.a == true && y IN x.b && y IN x.c RETURN x)`;
      assertEqual(1, AQL_EXECUTE(query).json.length);
      assertEqual(0, AQL_EXECUTE(query).json[0]);
    },
    
    testMultipleFiltersWithOuterLoopAttributes : function () {
      col.ensureHashIndex("a", "b[*]");
      col.save({ a: true, b: [1, 2, 3], c: [4, 5, 6] });

      let query = `FOR y IN [{ val: 1 }, { val: 2 }, { val: 3 }] FOR x IN ${cName} FILTER x.a == true && y.val IN x.b && y.val IN x.c RETURN x`;
      assertEqual(0, AQL_EXECUTE(query).json.length);
      
      query = `FOR y IN [{ val: 1 }, { val: 2 }, { val: 3 }] RETURN LENGTH(FOR x IN ${cName} FILTER x.a == true && y.val IN x.b && y.val IN x.c RETURN x)`;
      assertEqual(3, AQL_EXECUTE(query).json.length);
      assertEqual(0, AQL_EXECUTE(query).json[0]);
      assertEqual(0, AQL_EXECUTE(query).json[1]);
      assertEqual(0, AQL_EXECUTE(query).json[2]);
    },
    
    testHashPrefixMultiExpansion : function () {
      col.ensureHashIndex("something", "a[*]", "b[*]");
      for (var i = 0; i < 100; ++i) {
        col.save({ _key: i + "t", something: 0, a: i % 10, b: i % 5 });
      }
      // this will use an index with the RocksDB engine
      const query = `FOR x IN ${cName} FILTER x.something == 0 RETURN x._key`;
      assertEqual(100, AQL_EXECUTE(query).json.length);
    },
    
    testSkiplistPrefixMultiExpansion : function () {
      col.ensureSkiplist("something", "a[*]", "b[*]");
      for (var i = 0; i < 100; ++i) {
        col.save({ _key: i + "t", something: 0, a: i % 10, b: i % 5 });
      }
      const query = `FOR x IN ${cName} FILTER x.something == 0 RETURN x._key`;
      assertEqual(100, AQL_EXECUTE(query).json.length);
    },
    
    testHashPrefixMultiExpansionSub1 : function () {
      col.ensureHashIndex("something", "a[*]", "b[*]");
      for (var i = 0; i < 100; ++i) {
        col.save({ _key: i + "t", something: 0, a: [ 1, 2, 3, 4 ], b: [ 1, 2, 3, 4, 5 ] });
      }
      // this will use an index with the RocksDB engine
      const query = `FOR x IN ${cName} FILTER x.something == 0 RETURN x._key`;
      assertEqual(100, AQL_EXECUTE(query).json.length);
    },
    
    testHashPrefixMultiExpansionSub2 : function () {
      col.ensureHashIndex("something", "a[*]", "b[*]");
      for (var i = 0; i < 100; ++i) {
        col.save({ _key: i + "t", something: 0, a: [ 1, 2, 3, 4 ], b: [ 1 ] });
      }
      // this will use an index with the RocksDB engine
      const query = `FOR x IN ${cName} FILTER x.something == 0 RETURN x._key`;
      assertEqual(100, AQL_EXECUTE(query).json.length);
    },
    
    testHashPrefixMultiExpansionSub3 : function () {
      col.ensureHashIndex("something", "a[*].a", "b[*].b");
      for (var i = 0; i < 100; ++i) {
        col.save({ _key: i + "t", something: 0, a: [ { a: 1 }, { a: 2 }, { a: 3 }, { a: 4 } ], b: [ { b: 1 }, { b: 2 } ] });
      }
      // this will use an index with the RocksDB engine
      const query = `FOR x IN ${cName} FILTER x.something == 0 RETURN x._key`;
      assertEqual(100, AQL_EXECUTE(query).json.length);
    },
    
    testSkiplistPrefixMultiExpansionSub1 : function () {
      col.ensureSkiplist("something", "a[*]", "b[*]");
      for (var i = 0; i < 100; ++i) {
        col.save({ _key: i + "t", something: 0, a: [ 1, 2, 3, 4 ], b: [ 1, 2, 3, 4, 5 ] });
      }
      const query = `FOR x IN ${cName} FILTER x.something == 0 RETURN x._key`;
      assertEqual(100, AQL_EXECUTE(query).json.length);
    },
    
    testSkiplistPrefixMultiExpansionSub2 : function () {
      col.ensureSkiplist("something", "a[*]", "b[*]");
      for (var i = 0; i < 100; ++i) {
        col.save({ _key: i + "t", something: 0, a: [ 1, 2, 3, 4 ], b: [ 1 ] });
      }
      const query = `FOR x IN ${cName} FILTER x.something == 0 RETURN x._key`;
      assertEqual(100, AQL_EXECUTE(query).json.length);
    },
    
    testSkiplistPrefixMultiExpansionSub3 : function () {
      col.ensureSkiplist("something", "a[*].a", "b[*].b");
      for (var i = 0; i < 100; ++i) {
        col.save({ _key: i + "t", something: 0, a: [ { a: 1 }, { a: 2 }, { a: 3 }, { a: 4 } ], b: [ { b: 1 }, { b: 2 } ] });
      }
      // this will use an index with the RocksDB engine
      const query = `FOR x IN ${cName} FILTER x.something == 0 RETURN x._key`;
      assertEqual(100, AQL_EXECUTE(query).json.length);
    },

    testHashPlainArray : function () {
      col.ensureHashIndex("a[*]");
      for (var i = 0; i < 100; ++i) {
        col.save({ _key: i + "t", a: buildTags(i)});
      }
      col.save({_key: "empty"});
      col.save({_key: "emptyArray", a: []});
      col.save({_key: "noArray", a: "NoArray"});
      col.save({_key: "null", a: null});
      const query = `FOR x IN ${cName} FILTER @tag IN x.a[*] SORT x._key RETURN x._key`;
      validateResults(query);
      const orQuery = `FOR x IN ${cName} FILTER @tag1 IN x.a[*] || @tag2 IN x.a[*] SORT x._key RETURN x._key`;
      validateResultsOr(orQuery);
    },

    testHashNestedArray : function () {
      col.ensureHashIndex("a.b[*]");
      for (var i = 0; i < 100; ++i) {
        col.save({ _key: i + "t", a: {b: buildTags(i)}});
      }
      col.save({_key: "empty"});
      col.save({_key: "emptyArray", a: {b:[]}});
      col.save({_key: "noArray", a: {b: "NoArray"}});
      col.save({_key: "null", a: {b: null}});
      const query = `FOR x IN ${cName} FILTER @tag IN x.a.b[*] SORT x._key RETURN x._key`;
      validateResults(query);
      const orQuery = `FOR x IN ${cName} FILTER @tag1 IN x.a.b[*] || @tag2 IN x.a.b[*] SORT x._key RETURN x._key`;
      validateResultsOr(orQuery);
    },

    testHashArraySubattributes : function () {
      col.ensureHashIndex("a[*].b");
      for (var i = 0; i < 100; ++i) {
        var tags = buildTags(i);
        var obj = [];
        for (var t = 0; t < tags.length; ++t) {
          obj.push({b: tags[t]});
        }
        col.save({ _key: i + "t", a: obj});
      }
      col.save({_key: "empty"});
      col.save({_key: "emptyArray", a: []});
      col.save({_key: "noArray", a: "NoArray"});
      col.save({_key: "null", a: null});
      const query = `FOR x IN ${cName} FILTER @tag IN x.a[*].b SORT x._key RETURN x._key`;
      validateResults(query);
      const orQuery = `FOR x IN ${cName} FILTER @tag1 IN x.a[*].b || @tag2 IN x.a[*].b SORT x._key RETURN x._key`;
      validateResultsOr(orQuery);
    },

    testHashArrayCombinedIndex : function () {
      col.ensureHashIndex("a[*]", "b");
      for (var i = 0; i < 100; ++i) {
        col.save({ _key: i + "t", a: buildTags(i), b: i});
      }
      col.save({_key: "empty"});
      col.save({_key: "emptyArray", a: [], b: 1});
      col.save({_key: "noArray", a: "NoArray", b: 1});
      col.save({_key: "null", a: null, b: 1});
      const query = `FOR x IN ${cName} FILTER @tag IN x.a[*] AND x.b IN @list SORT x._key RETURN x`;
      validateCombinedResults(query);
    },

    testHashArrayCombinedArrayIndex : function () {
      col.ensureHashIndex("a[*]", "b[*]");
      for (var i = 0; i < 100; ++i) {
        col.save({ _key: i + "t", a: buildTags(i), b: buildTags(i)});
      }
      col.save({_key: "empty"});
      col.save({_key: "emptyArray", a: [], b: []});
      col.save({_key: "noArray", a: "NoArray", b: "NoArray"});
      col.save({_key: "null", a: null, b: null});
      const query = `FOR x IN ${cName} FILTER @tag IN x.a[*] AND @tag IN x.b[*] SORT x._key RETURN x._key`;
      validateResults(query);
      const orQuery = `FOR x IN ${cName} FILTER @tag1 IN x.a[*] && @tag1 IN x.b[*] || @tag2 IN x.a[*] && @tag2 IN x.b[*] SORT x._key RETURN x._key`;
      validateResultsOr(orQuery);
    },

    testHashArrayNotUsed : function () {
      col.ensureHashIndex("a[*]");
      for (var i = 0; i < 100; ++i) {
        col.save({ _key: i + "t", a: buildTags(i)});
      }
      var operators = ["==", "<", ">", "<=", ">="];
      var elements = ["x.a", "x.a[*]"];
      operators.forEach(function(op) {
        elements.forEach(function(e) {
          var query = `FOR x IN ${cName} FILTER 1 ${op} ${e} RETURN x`;
          validateIndexNotUsed(query);
          query = `FOR x IN ${cName} FILTER ${e} ${op} 1 RETURN x`;
          validateIndexNotUsed(query);
        });
      });
      var query = `FOR x IN ${cName} FILTER x.a[*] IN [1] RETURN x`;
      validateIndexNotUsed(query);
      query = `FOR x IN ${cName} FILTER 1 IN x.a RETURN x`;
      checkIsOptimizedQuery(query);
      query = `FOR x IN ${cName} FILTER x.a IN [1] RETURN x`;
      validateIndexNotUsed(query);
    },

    testHashArraySparse : function () {
      col.ensureHashIndex("a[*]", {sparse: true});
      for (var i = 0; i < 100; ++i) {
        col.save({ _key: i + "t", a: buildTags(i)});
      }
      col.save({_key: "empty"});
      col.save({_key: "emptyArray", a: []});
      col.save({_key: "noArray", a: "NoArray"});
      col.save({_key: "null", a: null});
      const query = `FOR x IN ${cName} FILTER @tag IN x.a[*] SORT x._key RETURN x._key`;
      validateResults(query);
      const orQuery = `FOR x IN ${cName} FILTER @tag1 IN x.a[*] || @tag2 IN x.a[*] SORT x._key RETURN x._key`;
      validateResultsOr(orQuery);
    },

    testSkiplistPlainArray : function () {
      col.ensureSkiplist("a[*]");
      for (var i = 0; i < 100; ++i) {
        col.save({ _key: i + "t", a: buildTags(i)});
      }
      col.save({_key: "empty"});
      col.save({_key: "emptyArray", a: []});
      col.save({_key: "noArray", a: "NoArray"});
      col.save({_key: "null", a: null});
      const query = `FOR x IN ${cName} FILTER @tag IN x.a[*] SORT x._key RETURN x._key`;
      validateResults(query);
      const orQuery = `FOR x IN ${cName} FILTER @tag1 IN x.a[*] || @tag2 IN x.a[*] SORT x._key RETURN x._key`;
      validateResultsOr(orQuery);
    },

    testSkiplistNestedArray : function () {
      col.ensureSkiplist("a.b[*]");
      for (var i = 0; i < 100; ++i) {
        col.save({ _key: i + "t", a: {b: buildTags(i)}});
      }
      col.save({_key: "empty"});
      col.save({_key: "emptyArray", a: {b:[]}});
      col.save({_key: "noArray", a: {b: "NoArray"}});
      col.save({_key: "null", a: {b: null}});
      const query = `FOR x IN ${cName} FILTER @tag IN x.a.b[*] SORT x._key RETURN x._key`;
      validateResults(query);
      const orQuery = `FOR x IN ${cName} FILTER @tag1 IN x.a.b[*] || @tag2 IN x.a.b[*] SORT x._key RETURN x._key`;
      validateResultsOr(orQuery);
    },

    testSkiplistArraySubattributes : function () {
      col.ensureSkiplist("a[*].b");
      for (var i = 0; i < 100; ++i) {
        var tags = buildTags(i);
        var obj = [];
        for (var t = 0; t < tags.length; ++t) {
          obj.push({b: tags[t]});
        }
        col.save({ _key: i + "t", a: obj});
      }
      col.save({_key: "empty"});
      col.save({_key: "emptyArray", a: []});
      col.save({_key: "noArray", a: "NoArray"});
      col.save({_key: "null", a: null});
      const query = `FOR x IN ${cName} FILTER @tag IN x.a[*].b SORT x._key RETURN x._key`;
      validateResults(query);
      const orQuery = `FOR x IN ${cName} FILTER @tag1 IN x.a[*].b || @tag2 IN x.a[*].b SORT x._key RETURN x._key`;
      validateResultsOr(orQuery);
    },

    testSkiplistArrayCombinedIndex : function () {
      col.ensureSkiplist("a[*]", "b");
      for (var i = 0; i < 100; ++i) {
        col.save({ _key: i + "t", a: buildTags(i), b: i});
      }
      col.save({_key: "empty"});
      col.save({_key: "emptyArray", a: [], b: 1});
      col.save({_key: "noArray", a: "NoArray", b: 1});
      col.save({_key: "null", a: null, b: 1});
      const query = `FOR x IN ${cName} FILTER @tag IN x.a[*] AND x.b IN @list SORT x._key RETURN x`;
      validateCombinedResults(query);
    },

    testSkiplistArrayCombinedArrayIndex : function () {
      col.ensureSkiplist("a[*]", "b[*]");
      for (var i = 0; i < 100; ++i) {
        col.save({ _key: i + "t", a: buildTags(i), b: buildTags(i)});
      }
      col.save({_key: "empty"});
      const query = `FOR x IN ${cName} FILTER @tag IN x.a[*] AND @tag IN x.b[*] SORT x._key RETURN x._key`;
      validateResults(query);
      const orQuery = `FOR x IN ${cName} FILTER @tag1 IN x.a[*] && @tag1 IN x.b[*] || @tag2 IN x.a[*] && @tag2 IN x.b[*] SORT x._key RETURN x._key`;
      validateResultsOr(orQuery);
    },

    testSkiplistArrayNotUsed : function () {
      col.ensureHashIndex("a[*]");
      for (var i = 0; i < 100; ++i) {
        col.save({ _key: i + "t", a: buildTags(i)});
      }
      var operators = ["==", "<", ">", "<=", ">="];
      var elements = ["x.a", "x.a[*]"];
      operators.forEach(function(op) {
        elements.forEach(function(e) {
          var query = `FOR x IN ${cName} FILTER 1 ${op} ${e} RETURN x`;
          validateIndexNotUsed(query);
          query = `FOR x IN ${cName} FILTER ${e} ${op} 1 RETURN x`;
          validateIndexNotUsed(query);
        });
      });
      var query = `FOR x IN ${cName} FILTER x.a[*] IN [1] RETURN x`;
      validateIndexNotUsed(query);
      query = `FOR x IN ${cName} FILTER 1 IN x.a RETURN x`;
      checkIsOptimizedQuery(query);
      query = `FOR x IN ${cName} FILTER x.a IN [1] RETURN x`;
      validateIndexNotUsed(query);
    },

    testSkiplistArraySparse : function () {
      col.ensureSkiplist("a[*]", {sparse: true});
      for (var i = 0; i < 100; ++i) {
        col.save({ _key: i + "t", a: buildTags(i)});
      }
      col.save({_key: "empty"});
      col.save({_key: "emptyArray", a: []});
      col.save({_key: "noArray", a: "NoArray"});
      col.save({_key: "null", a: null});
      const query = `FOR x IN ${cName} FILTER @tag IN x.a[*] SORT x._key RETURN x._key`;
      validateResults(query);
      const orQuery = `FOR x IN ${cName} FILTER @tag1 IN x.a[*] || @tag2 IN x.a[*] SORT x._key RETURN x._key`;
      validateResultsOr(orQuery);
    }

  };

}

function arrayIndexNonArraySuite () {

  const cName = "UnitTestsArray";
  var col;

  const checkElementsInIndex = function (count) {
    var allIndexes = col.getIndexes(true);
    assertEqual(allIndexes.length, 2, "We have more than one index!");
  };

  return {

    setUp: function () {
      this.tearDown();
      col = db._create(cName);
    },

    tearDown: function () {
      db._drop(cName);
    },

    testHashSingleAttribute : function () {
      col.ensureHashIndex("a[*]");
      col.save({}); // a is not set
      checkElementsInIndex(0);
      col.save({ a: "This is no array" });
      checkElementsInIndex(0);
      col.save({ a: {b: [42] } });
      checkElementsInIndex(0);
      col.save({ a: [] }); // Empty Array nothing to insert here
      checkElementsInIndex(0);

      col.save({ a: [1, 2, 3, 3, 2, 1] });
      checkElementsInIndex(3); // 1, 2, 3

      col.save({_key: "null", a: [null, "a", "b", "c", "b", "a", null] });
      checkElementsInIndex(7); // 1, 2, 3, a, b, c, null

      const query = `FOR x IN ${cName} FILTER @tag IN x.a[*] SORT x._key RETURN x._key`;
      var actual = AQL_EXECUTE(query, { tag : null }).json;
      // We expect that we can only find the Array that has stored exactly the null value
      assertEqual(actual.length, 1);
      assertEqual(actual[0], "null");
    },

    testHashMultipleAttributeFirstArray : function () {
      var inserted = 0;
      col.ensureHashIndex("a[*]", "b");
      col.save({}); // a is not set
      checkElementsInIndex(inserted);
      col.save({b: 1}); // a is not set
      checkElementsInIndex(inserted);
      col.save({ a: "This is no array" });
      checkElementsInIndex(inserted);
      col.save({ a: "This is no array", b: 1 });
      checkElementsInIndex(inserted);
      col.save({ a: {b: [42] } });
      checkElementsInIndex(inserted);
      col.save({ a: {b: [42] }, b: 1 });
      checkElementsInIndex(inserted);
      col.save({ a: [] }); // Empty Array nothing to insert here
      checkElementsInIndex(inserted);
      col.save({ a: [], b: 1 }); // Empty Array nothing to insert here
      checkElementsInIndex(inserted);

      col.save({ a: [1, 2, 3, 3, 2, 1] });
      inserted += 3;
      checkElementsInIndex(inserted); // 1, 2, 3

      col.save({ a: [1, 2, 3, 3, 2, 1], b: 1 });
      inserted += 3;
      checkElementsInIndex(inserted); // 1, 2, 3 :: 1

      col.save({a: [null, "a", "b", "c", "b", "a", null] });
      inserted += 4;
      checkElementsInIndex(inserted); // 1, 2, 3 :: 1

      col.save({_key: "null", a: [null, "a", "b", "c", "b", "a", null], b: 1 });
      inserted += 4;
      checkElementsInIndex(inserted); // 1, 2, 3 :: 1

      const query = `FOR x IN ${cName} FILTER @tag IN x.a[*] && 1 == x.b SORT x._key RETURN x._key`;
      var actual = AQL_EXECUTE(query, { tag : null }).json;
      // We expect that we can only find the Array that has stored exactly the null value
      assertEqual(actual.length, 1);
      assertEqual(actual[0], "null");
    },

    testHashMultipleAttributeMiddleArray : function () {
      var inserted = 0;
      col.ensureHashIndex("b", "a[*]", "c");
      col.save({}); // a is not set
      checkElementsInIndex(inserted);
      col.save({b: 1}); // a is not set
      checkElementsInIndex(inserted);
      col.save({b: 1, c: 1}); // a is not set
      checkElementsInIndex(inserted);
      col.save({ a: "This is no array" });
      checkElementsInIndex(inserted);
      col.save({ a: "This is no array", b: 1 });
      checkElementsInIndex(inserted);
      col.save({ a: "This is no array", b: 1, c: 1});
      checkElementsInIndex(inserted);
      col.save({ a: {b: [42] } });
      checkElementsInIndex(inserted);
      col.save({ a: {b: [42] }, b: 1 });
      checkElementsInIndex(inserted);
      col.save({ a: {b: [42] }, b: 1, c: 1 });
      checkElementsInIndex(inserted);
      col.save({ a: [] }); // Empty Array nothing to insert here
      checkElementsInIndex(inserted);
      col.save({ a: [], b: 1 }); // Empty Array nothing to insert here
      checkElementsInIndex(inserted);
      col.save({ a: [], b: 1, c: 1 }); // Empty Array nothing to insert here
      checkElementsInIndex(inserted);

      col.save({ a: [1, 2, 3, 3, 2, 1] });
      inserted += 3;
      checkElementsInIndex(inserted); // 1, 2, 3

      col.save({ a: [1, 2, 3, 3, 2, 1], b: 1 });
      inserted += 3;
      checkElementsInIndex(inserted); // 1, 2, 3 :: 1

      col.save({_key: "null1", a: [null, "a", "b", "c", "b", "a", null] });
      inserted += 4;
      checkElementsInIndex(inserted); // 1, 2, 3 :: 1

      col.save({_key: "null2", a: [null, "a", "b", "c", "b", "a", null], b: 1 });
      inserted += 4;
      checkElementsInIndex(inserted); // 1, 2, 3 :: 1

      const query = `FOR x IN ${cName} FILTER @tag IN x.a[*] && 1 == x.b SORT x._key RETURN x._key`;
      var actual = AQL_EXECUTE(query, { tag : null }).json;
      // We expect that we can only find the Array that has stored exactly the null value
      assertEqual(actual.length, 1);
      assertEqual(actual[0], "null2");
    },

    testHashIndexMultipleAttributeMiddleArraySub : function () {
      var inserted = 0;
      col.ensureHashIndex("b", "a[*].d", "c");
      col.save({}); // a is not set, no indexing
      checkElementsInIndex(inserted);
      col.save({b: 1}); // a is not set, no indexing
      checkElementsInIndex(inserted);
      col.save({b: 1, c: 1}); // a is not set, no indexing
      checkElementsInIndex(inserted);
      col.save({ a: "This is no array" }); // a is illegal, no indexing
      checkElementsInIndex(inserted);
      col.save({ a: "This is no array", b: 1 }); // no indexing
      checkElementsInIndex(inserted);
      col.save({ a: "This is no array", b: 1, c: 1}); // no indexing
      checkElementsInIndex(inserted);
      col.save({ a: {b: [42] } }); // no indexing
      checkElementsInIndex(inserted);
      col.save({ a: {b: [42] }, b: 1 }); // no indexing
      checkElementsInIndex(inserted);
      col.save({ a: {b: [42] }, b: 1, c: 1 }); // no indexing
      checkElementsInIndex(inserted);
      col.save({ a: [] }); // Empty Array. no indexing
      checkElementsInIndex(inserted);
      col.save({ a: [], b: 1 }); // Empty Array. no indexing
      checkElementsInIndex(inserted);
      col.save({ a: [], b: 1, c: 1 });  // Empty Array. no indexing
      checkElementsInIndex(inserted);

      col.save({ a: [1, 2, 3, 3, 2, 1] }); // a does not have any nested value. Index as one null
      inserted += 1;
      checkElementsInIndex(inserted);

      col.save({ a: [1, 2, 3, 3, 2, 1], b: 1 }); // a does not have any nested value. Index as one null
      inserted += 1;
      checkElementsInIndex(inserted);

      col.save({_key: "null1", a: [1, 2, 3, 3, 2, 1], b: 1, c: 1 }); // a does not have any nested value. Index as one null
      inserted += 1;
      checkElementsInIndex(inserted);

      col.save({ a: [{d: 1}, {d: 2}, {d: 3}, {d: 3}, {d: 2}, {d: 1}] });
      inserted += 3; // We index b: null and 3 values for a[*].d
      checkElementsInIndex(inserted);

      col.save({ a: [{d: 1}, {d: 2}, {d: 3}, {d: 3}, {d: 2}, {d: 1}], b: 1 });
      inserted += 3; // We index b: 1 and 3 values for a[*].d
      checkElementsInIndex(inserted);

      col.save({ a: [{d: 1}, {d: 2}, {d: 3}, {d: 3}, {d: 2}, {d: 1}], b: 1, c: 1 });
      inserted += 3; // We index b: 1, c: 1 and 3 values for a[*].d
      checkElementsInIndex(inserted);

      col.save({_key: "null2", a: [{d: null}, {d: "a"}, {d: "b"}, {d: "c"}, {d: "b"}, {d: "a"}, {d: null}] });
      inserted += 4;
      checkElementsInIndex(inserted); // b: null a: "a", "b", "c", null c:null

      col.save({_key: "null3", a: [{d: null}, {d: "a"}, {d: "b"}, {d: "c"}, {d: "b"}, {d: "a"}, {d: null}], b: 1 });
      inserted += 4;
      checkElementsInIndex(inserted);

      col.save({_key: "null4", a: [{d: null}, {d: "a"}, {d: "b"}, {d: "c"}, {d: "b"}, {d: "a"}, {d: null}], b: 1, c: 1 });
      inserted += 4;
      checkElementsInIndex(inserted);

      const query = `FOR x IN ${cName} FILTER @tag IN x.a[*].d && 1 == x.b && 1 == x.c SORT x._key RETURN x._key`;
      var actual = AQL_EXECUTE(query, { tag : null }).json;
      // We expect that we can only find the Array that has stored exactly the null value
      assertEqual(actual.length, 2);
      assertEqual(actual[0], "null1");
      assertEqual(actual[1], "null4");
    },

    testHashIndexSubAttributeArray : function () {
      col.ensureHashIndex("b", "a.d[*]", "c");
      col.save({b: 1, a:[ {d: [1, 2]}, {d: 3} ], c: 1});
      checkElementsInIndex(0);

      const query = `FOR x IN ${cName} FILTER @tag IN x.a[*].d && 1 == x.b SORT x._key RETURN x._key`;
      var actual = AQL_EXECUTE(query, { tag : null }).json;
      assertEqual(actual.length, 0);
      // Do not find anything
    },

    testHashIndexMultiArray : function () {
      col.ensureHashIndex("a[*]", "b[*]");

      col.save({a: [1, 2, 3]}); // Do not index
      checkElementsInIndex(0);

      col.save({a: [1, 2, 3], b: null}); // Do not index
      checkElementsInIndex(0);

      col.save({a: [1, 2, 3], b: "this is no array"}); // Do not index
      checkElementsInIndex(0);

      col.save({a: "this is no array", b: ["a", "b", "c"]}); // Do not index
      checkElementsInIndex(0);

      col.save({a: [1, 2, null, null, 2, 1], b: ["a", "b", null, "b", "a"]});
      checkElementsInIndex(9); // 3*3 many combinations

      const query = `FOR x IN ${cName} FILTER @tag IN x.a[*] && @tag IN x.b[*] SORT x._key RETURN x._key`;
      var actual = AQL_EXECUTE(query, { tag : null }).json;
      assertEqual(actual.length, 1);
    },

    testHashIndexArraySparse : function () {
      col.ensureHashIndex("a[*]", "b", {sparse: true});
      var inserted = 0;

      col.save({a: [1, 2, 3]}); // Do not index, b is not set
      checkElementsInIndex(inserted);

      col.save({a: [1, 2, 3], b: null}); // Do not index, b is null
      checkElementsInIndex(inserted);

      col.save({a: [1, 2, 3], b: 1}); // Do index
      inserted += 3;
      checkElementsInIndex(inserted);

      col.save({a: [null, 4], b: 1}); // Do index
      inserted += 2;
      checkElementsInIndex(inserted);

      const query = `FOR x IN ${cName} FILTER null IN x.a[*] && 1 == x.b SORT x._key RETURN x._key`;
      // We can use the index for null in SPARSE
      var actual = AQL_EXECUTE(query).json;
      assertEqual(actual.length, 1);
      var plan = AQL_EXPLAIN(query).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        return node.type;
      });
      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"));

      const query2 = `FOR x IN ${cName} FILTER null IN x.a[*] SORT x._rev RETURN x._key`;
      plan = AQL_EXPLAIN(query2).plan;
      nodeTypes = plan.nodes.map(function(node) {
        return node.type;
      });
      // Cannot use the index for sub attribute a
      assertEqual(-1, nodeTypes.indexOf("IndexNode"));
    },

    testSkiplistSingleAttribute : function () {
      col.ensureSkiplist("a[*]");
      col.save({}); // a is not set
      checkElementsInIndex(0);
      col.save({ a: "This is no array" });
      checkElementsInIndex(0);
      col.save({ a: {b: [42] } });
      checkElementsInIndex(0);
      col.save({ a: [] }); // Empty Array nothing to insert here
      checkElementsInIndex(0);

      col.save({ a: [1, 2, 3, 3, 2, 1] });
      checkElementsInIndex(3); // 1, 2, 3

      col.save({_key: "null", a: [null, "a", "b", "c", "b", "a", null] });
      checkElementsInIndex(7); // 1, 2, 3, a, b, c, null

      const query = `FOR x IN ${cName} FILTER @tag IN x.a[*] SORT x._key RETURN x._key`;
      var actual = AQL_EXECUTE(query, { tag : null }).json;
      // We expect that we can only find the Array that has stored exactly the null value
      assertEqual(actual.length, 1);
      assertEqual(actual[0], "null");
    },

    testSkiplistMultipleAttributeFirstArray : function () {
      var inserted = 0;
      col.ensureSkiplist("a[*]", "b");
      col.save({}); // a is not set
      checkElementsInIndex(inserted);
      col.save({b: 1}); // a is not set
      checkElementsInIndex(inserted);
      col.save({ a: "This is no array" });
      checkElementsInIndex(inserted);
      col.save({ a: "This is no array", b: 1 });
      checkElementsInIndex(inserted);
      col.save({ a: {b: [42] } });
      checkElementsInIndex(inserted);
      col.save({ a: {b: [42] }, b: 1 });
      checkElementsInIndex(inserted);
      col.save({ a: [] }); // Empty Array nothing to insert here
      checkElementsInIndex(inserted);
      col.save({ a: [], b: 1 }); // Empty Array nothing to insert here
      checkElementsInIndex(inserted);

      col.save({ a: [1, 2, 3, 3, 2, 1] });
      inserted += 3;
      checkElementsInIndex(inserted); // 1, 2, 3

      col.save({ a: [1, 2, 3, 3, 2, 1], b: 1 });
      inserted += 3;
      checkElementsInIndex(inserted); // 1, 2, 3 :: 1

      col.save({a: [null, "a", "b", "c", "b", "a", null] });
      inserted += 4;
      checkElementsInIndex(inserted); // 1, 2, 3 :: 1

      col.save({_key: "null", a: [null, "a", "b", "c", "b", "a", null], b: 1 });
      inserted += 4;
      checkElementsInIndex(inserted); // 1, 2, 3, a, b, c, null :: 1

      const query = `FOR x IN ${cName} FILTER @tag IN x.a[*] && 1 == x.b SORT x._key RETURN x._key`;
      var actual = AQL_EXECUTE(query, { tag : null }).json;
      // We expect that we can only find the Array that has stored exactly the null value
      assertEqual(actual.length, 1);
      assertEqual(actual[0], "null");
    },

    testSkiplistMultipleAttributeMiddleArray : function () {
      var inserted = 0;
      var insertedB = 0;
      col.ensureSkiplist("b", "a[*]", "c");
      col.save({}); // a is not set, we index b: null
      inserted += 1;
      checkElementsInIndex(inserted);
      col.save({b: 1}); // a is not set, but we can index b only
      inserted += 1;
      insertedB += 1;
      checkElementsInIndex(inserted);
      col.save({b: 1, c: 1}); // a is not set, but we can index b only
      inserted += 1;
      insertedB += 1;
      checkElementsInIndex(inserted);
      col.save({ a: "This is no array" }); // a is illegal. But we can index b only
      inserted += 1;
      checkElementsInIndex(inserted);
      col.save({ a: "This is no array", b: 1 }); // We can index b only
      inserted += 1;
      insertedB += 1;
      checkElementsInIndex(inserted);
      col.save({ a: "This is no array", b: 1, c: 1}); // We can index b only
      inserted += 1;
      insertedB += 1;
      checkElementsInIndex(inserted);
      col.save({ a: {b: [42] } });
      inserted += 1;
      checkElementsInIndex(inserted);
      col.save({ a: {b: [42] }, b: 1 }); // We can index b only
      inserted += 1;
      insertedB += 1;
      checkElementsInIndex(inserted);
      col.save({ a: {b: [42] }, b: 1, c: 1 }); // We can index b only
      inserted += 1;
      insertedB += 1;
      checkElementsInIndex(inserted);
      col.save({ a: [] }); // Empty Array. Nothing for a but index b
      inserted += 1;
      checkElementsInIndex(inserted);
      col.save({ a: [], b: 1 }); // Empty Array. Nothing for a but index b
      inserted += 1;
      insertedB += 1;
      checkElementsInIndex(inserted);
      col.save({ a: [], b: 1, c: 1 });  // Empty Array. Nothing for a but index b
      inserted += 1;
      insertedB += 1;
      checkElementsInIndex(inserted);

      col.save({ a: [1, 2, 3, 3, 2, 1] });
      inserted += 3; // We index b: null a: 1,2,3 c: null
      checkElementsInIndex(inserted);

      col.save({ a: [1, 2, 3, 3, 2, 1], b: 1 }); // b: 1 a: 1,2,3 c: null
      inserted += 3;
      insertedB += 1;
      checkElementsInIndex(inserted);

      col.save({ a: [1, 2, 3, 3, 2, 1], b: 1, c: 1 });
      inserted += 3;
      insertedB += 1;
      checkElementsInIndex(inserted); // 1, 2, 3 :: 1 :: 1

      col.save({_key: "null1", a: [null, "a", "b", "c", "b", "a", null] });
      inserted += 4;
      checkElementsInIndex(inserted); // b: null a: "a", "b", "c", null c:null

      col.save({_key: "null2", a: [null, "a", "b", "c", "b", "a", null], b: 1 });
      inserted += 4;
      insertedB += 1;
      checkElementsInIndex(inserted);

      col.save({_key: "null3", a: [null, "a", "b", "c", "b", "a", null], b: 1, c: 1});
      inserted += 4;
      insertedB += 1;
      checkElementsInIndex(inserted);

      const query = `FOR x IN ${cName} FILTER @tag IN x.a[*] && 1 == x.b SORT x._key RETURN x._key`;
      var actual = AQL_EXECUTE(query, { tag : null }).json;
      // We expect that we can only find the Array that has stored exactly the null value
      assertEqual(actual.length, 2);
      assertEqual(actual[0], "null2");
      assertEqual(actual[1], "null3");

      const query2 = `FOR x IN ${cName} FILTER 1 == x.b RETURN x._key`;
      actual = AQL_EXECUTE(query2).json;
      assertEqual(actual.length, insertedB);
      const query3 = `FOR x IN ${cName} FILTER null == x.b RETURN x._key`;
      actual = AQL_EXECUTE(query3).json;
      assertEqual(actual.length, col.count() - insertedB);
    },

    testSkiplistMultipleAttributeMiddleArraySub : function () {
      var inserted = 0;
      var insertedB = 0;
      col.ensureSkiplist("b", "a[*].d", "c");
      col.save({}); // a is not set, we index b: null
      inserted += 1;
      checkElementsInIndex(inserted);
      col.save({b: 1}); // a is not set, but we can index b only
      inserted += 1;
      insertedB += 1;
      checkElementsInIndex(inserted);
      col.save({b: 1, c: 1}); // a is not set, but we can index b only
      inserted += 1;
      insertedB += 1;
      checkElementsInIndex(inserted);
      col.save({ a: "This is no array" }); // a is illegal. But we can index b only
      inserted += 1;
      checkElementsInIndex(inserted);
      col.save({ a: "This is no array", b: 1 }); // We can index b only
      inserted += 1;
      insertedB += 1;
      checkElementsInIndex(inserted);
      col.save({ a: "This is no array", b: 1, c: 1}); // We can index b only
      inserted += 1;
      insertedB += 1;
      checkElementsInIndex(inserted);
      col.save({ a: {b: [42] } }); // We can index b only
      inserted += 1;
      checkElementsInIndex(inserted);
      col.save({ a: {b: [42] }, b: 1 }); // We can index b only
      inserted += 1;
      insertedB += 1;
      checkElementsInIndex(inserted);
      col.save({ a: {b: [42] }, b: 1, c: 1 }); // We can index b only
      inserted += 1;
      insertedB += 1;
      checkElementsInIndex(inserted);
      col.save({ a: [] }); // Empty Array. Nothing for a but index b
      inserted += 1;
      checkElementsInIndex(inserted);
      col.save({ a: [], b: 1 }); // Empty Array. Nothing for a but index b
      inserted += 1;
      insertedB += 1;
      checkElementsInIndex(inserted);
      col.save({ a: [], b: 1, c: 1 });  // Empty Array. Nothing for a but index b
      inserted += 1;
      insertedB += 1;
      checkElementsInIndex(inserted);

      col.save({ a: [1, 2, 3, 3, 2, 1] });
      inserted += 1; // We index b: null But a does not have any nested value. Index one for null
      checkElementsInIndex(inserted);

      col.save({ _key: "null1", a: [1, 2, 3, 3, 2, 1], b: 1 }); // b: 1 a: 1,2,3 c: null
      inserted += 1; // We index b: 1 But a does not have any nested value. Index one for null
      insertedB += 1;
      checkElementsInIndex(inserted);

      col.save({ _key: "null2", a: [1, 2, 3, 3, 2, 1], b: 1, c: 1 });
      inserted += 1; // We index b: 1, c: 1 But a does not have any nested value. Index one for null
      insertedB += 1;
      checkElementsInIndex(inserted);

      col.save({ a: [{d: 1}, {d: 2}, {d: 3}, {d: 3}, {d: 2}, {d: 1}] });
      inserted += 3; // We index b: null and 3 values for a[*].d
      checkElementsInIndex(inserted);

      col.save({ a: [{d: 1}, {d: 2}, {d: 3}, {d: 3}, {d: 2}, {d: 1}], b: 1 });
      inserted += 3; // We index b: 1 and 3 values for a[*].d
      insertedB += 1;
      checkElementsInIndex(inserted);

      col.save({ a: [{d: 1}, {d: 2}, {d: 3}, {d: 3}, {d: 2}, {d: 1}], b: 1, c: 1 });
      inserted += 3; // We index b: 1, c: 1 and 3 values for a[*].d
      insertedB += 1;
      checkElementsInIndex(inserted);

      col.save({_key: "null3", a: [{d: null}, {d: "a"}, {d: "b"}, {d: "c"}, {d: "b"}, {d: "a"}, {d: null}] });
      inserted += 4;
      checkElementsInIndex(inserted); // b: null a: "a", "b", "c", null c:null

      col.save({_key: "null4", a: [{d: null}, {d: "a"}, {d: "b"}, {d: "c"}, {d: "b"}, {d: "a"}, {d: null}], b: 1 });
      inserted += 4;
      insertedB += 1;
      checkElementsInIndex(inserted);

      col.save({_key: "null5", a: [{d: null}, {d: "a"}, {d: "b"}, {d: "c"}, {d: "b"}, {d: "a"}, {d: null}], b: 1, c: 1 });
      inserted += 4;
      insertedB += 1;
      checkElementsInIndex(inserted);

      const query = `FOR x IN ${cName} FILTER @tag IN x.a[*].d && 1 == x.b SORT x._key RETURN x._key`;
      var actual = AQL_EXECUTE(query, { tag : null }).json;
      // We expect that we can only find the array that stores exactly the null value
      // And the arrays that do not have the sub attribute.
      assertEqual(actual.length, 4);
      assertEqual(actual[0], "null1");
      assertEqual(actual[1], "null2");
      assertEqual(actual[2], "null4");
      assertEqual(actual[3], "null5");

      const query2 = `FOR x IN ${cName} FILTER 1 == x.b RETURN x._key`;
      actual = AQL_EXECUTE(query2).json;
      assertEqual(actual.length, insertedB);
      const query3 = `FOR x IN ${cName} FILTER null == x.b RETURN x._key`;
      actual = AQL_EXECUTE(query3).json;
      assertEqual(actual.length, col.count() - insertedB);
    },

    testSkiplistIndexSubAttributeArray : function () {
      col.ensureSkiplist("b", "a.d[*]", "c");
      col.save({b: 1, a:[ {d: [1, 2]}, {d: 3} ], c: 1});
      // We do only index b: 1 here
      checkElementsInIndex(1);

      const query = `FOR x IN ${cName} FILTER @tag IN x.a[*].d && 1 == x.b SORT x._key RETURN x._key`;
      var actual = AQL_EXECUTE(query, { tag : null }).json;
      assertEqual(actual.length, 0);
      // Do not find anything
    },

    testSkiplistIndexArraySparse : function () {
      col.ensureSkiplist("a[*]", "b", {sparse: true});
      var inserted = 0;

      col.save({a: [1, 2, 3]}); // Do not index, b is not set
      checkElementsInIndex(inserted);

      col.save({a: [1, 2, 3], b: null}); // Do not index, b is null
      checkElementsInIndex(inserted);

      col.save({a: [1, 2, 3], b: 1}); // Do index
      inserted += 3;
      checkElementsInIndex(inserted);

      col.save({a: [null, 4], b: 1}); // Do index
      inserted += 2;
      checkElementsInIndex(inserted);

      const query = `FOR x IN ${cName} FILTER null IN x.a[*] && 1 == x.b SORT x._key RETURN x._key`;
      // We can use the index for null in SPARSE
      var actual = AQL_EXECUTE(query).json;
      assertEqual(actual.length, 1);
      var plan = AQL_EXPLAIN(query).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        return node.type;
      });
      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"));

      const query2 = `FOR x IN ${cName} FILTER null IN x.a[*] SORT x._rev RETURN x._key`;
      plan = AQL_EXPLAIN(query2).plan;
      nodeTypes = plan.nodes.map(function(node) {
        return node.type;
      });
      // Cannot use the index for sub attribute a
      assertEqual(-1, nodeTypes.indexOf("IndexNode"));
    },


  };

}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(arrayIndexSuite);
jsunity.run(arrayIndexNonArraySuite);

return jsunity.done();

