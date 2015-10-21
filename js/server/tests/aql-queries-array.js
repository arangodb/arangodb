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
var db = require("org/arangodb").db;

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
    return tags.sort(function() { return 0.5 - Math.random(); });
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

  var validateResults = function (query, sparse) {
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

    testHashPlainArray : function () {
      col.ensureHashIndex("a[*]");
      for (var i = 0; i < 100; ++i) {
        col.save({ _key: i + "t", a: buildTags(i)});
      }
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
      const query = `FOR x IN ${cName} FILTER @tag IN x.a[*] AND x.b IN @list SORT x._key RETURN x`;
      validateCombinedResults(query);
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
      validateIndexNotUsed(query);
      query = `FOR x IN ${cName} FILTER x.a IN [1] RETURN x`;
      validateIndexNotUsed(query);
    },

    testHashArraySparse : function () {
      col.ensureHashIndex("a[*]", {sparse: true});
      for (var i = 0; i < 100; ++i) {
        col.save({ _key: i + "t", a: buildTags(i)});
      }
      const query = `FOR x IN ${cName} FILTER @tag IN x.a[*] SORT x._key RETURN x._key`;
      validateResults(query, true);
      const orQuery = `FOR x IN ${cName} FILTER @tag1 IN x.a[*] || @tag2 IN x.a[*] SORT x._key RETURN x._key`;
      validateResultsOr(orQuery, true);
    },

    testSkiplistPlainArray : function () {
      col.ensureSkiplist("a[*]");
      for (var i = 0; i < 100; ++i) {
        col.save({ _key: i + "t", a: buildTags(i)});
      }
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
      const query = `FOR x IN ${cName} FILTER @tag IN x.a[*] AND x.b IN @list SORT x._key RETURN x`;
      validateCombinedResults(query);
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
      validateIndexNotUsed(query);
      query = `FOR x IN ${cName} FILTER x.a IN [1] RETURN x`;
      validateIndexNotUsed(query);
    },

    testSkiplistArraySparse : function () {
      col.ensureSkiplist("a[*]", {sparse: true});
      for (var i = 0; i < 100; ++i) {
        col.save({ _key: i + "t", a: buildTags(i)});
      }
      const query = `FOR x IN ${cName} FILTER @tag IN x.a[*] SORT x._key RETURN x._key`;
      validateResults(query, true);
      const orQuery = `FOR x IN ${cName} FILTER @tag1 IN x.a[*] || @tag2 IN x.a[*] SORT x._key RETURN x._key`;
      validateResultsOr(orQuery, true);
    }

  };

}

function arrayIndexCrazyQueriesSuite () {

  const cName = "UnitTestsArray";
  var col;

  return {

    setUp : function () {
      this.tearDown();
      col = db._create(cName);
    },

    tearDown : function () {
      db._drop(cName);
    },

    testLongChainediArrayHash : function () {
    }


  };

}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(arrayIndexSuite);
jsunity.run(arrayIndexCrazyQueriesSuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
