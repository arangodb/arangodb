/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertTrue, AQL_EXPLAIN, AQL_EXECUTE */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for query language, in
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

var jsunity = require("jsunity");
var internal = require("internal");
var helper = require("@arangodb/aql-helper");
var getQueryResults = helper.getQueryResults;
var db = internal.db;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ahuacatlQueryOptimizerInTestSuite () {
  var c = null;
  var cn = "UnitTestsAhuacatlOptimizerIn";

  var explain = function (query, params) {
    return helper.getCompactPlan(AQL_EXPLAIN(query, params, { optimizer: { rules: [ "-all", "+use-indexes" ] } })).map(function(node) { return node.type; });
  };

  var ruleIsUsed = function (query) {
   var result = AQL_EXPLAIN(query, {}, { optimizer: { rules: [ "-all", "+use-indexes" ] } });
   assertTrue(result.plan.rules.indexOf("use-indexes") !== -1, query);
  };

  var ruleIsNotUsed = function (query) {
   var result = AQL_EXPLAIN(query, {}, { optimizer: { rules: [ "-all", "+use-indexes" ] } });
   assertTrue(result.plan.rules.indexOf("use-indexes") === -1, query);
  };

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      internal.db._drop(cn);
      c = internal.db._create(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      internal.db._drop(cn);
    },

    testSortedArrayIn : function () {
      var values = [
        "WJItoWBuRBMBMajh", "WJIuWmBuRBMBMbdR",
        "WJIv_mBuRBMBMdgh", "WJIwOWBuRBMBMdzC",
        "WJIxaWBuRBMBMfRW", "WJIxyGBuRBMBMfvK",
        "WK5Mz2BuRBMBQT8y", "WKfgmGBuRBMBPgMR",
        "WMu6iHNG6AAGJ-gY", "WMu6jXNG6AAGJ-hd",
        "WMu7kXNG6AAGJ_Zc", "WMyn9nNG6AAGKN4Q",
        "WMynsHNG6AAGKNqu", "WMyo3HNG6AAGKOk5",
        "WMyoSXNG6AAGKOIY", "WMyohHNG6AAGKOTx"
      ];

      values.forEach(function(val) {
        c.insert({ val });
      });

      var query = "FOR doc IN " + cn + " FILTER doc.val IN " + JSON.stringify(values) + " RETURN 1";
      var actual = getQueryResults(query);
      assertEqual(16, actual.length);
    },

    testSortedArrayInStatic : function () {
      var values = [
        "WJItoWBuRBMBMajh", "WJIuWmBuRBMBMbdR",
        "WJIv_mBuRBMBMdgh", "WJIwOWBuRBMBMdzC",
        "WJIxaWBuRBMBMfRW", "WJIxyGBuRBMBMfvK",
        "WK5Mz2BuRBMBQT8y", "WKfgmGBuRBMBPgMR",
        "WMu6iHNG6AAGJ-gY", "WMu6jXNG6AAGJ-hd",
        "WMu7kXNG6AAGJ_Zc", "WMyn9nNG6AAGKN4Q",
        "WMynsHNG6AAGKNqu", "WMyo3HNG6AAGKOk5",
        "WMyoSXNG6AAGKOIY", "WMyohHNG6AAGKOTx"
      ];

      var query = "RETURN 'WMyohHNG6AAGKOTx' IN " + JSON.stringify(values);
      var actual = getQueryResults(query);
      assertEqual([ true ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check a ref access without any indexes
////////////////////////////////////////////////////////////////////////////////

    testInMergeOr : function () {
      c.save({ _key: "test0" });
      for (var i = 1; i < 100; ++i) {
        c.save({ _key: "test" + i, parent: "test" + (i - 1), parents: [ "test" + (i - 1) ] });
      }

      var expected = [ 'test1', 'test2', 'test5', 'test7' ];
      var actual = getQueryResults("LET parents = [ 'test5', 'test7' ] FOR c IN " + cn + " FILTER c._key IN parents || c._key IN [ 'test1' ] || c._key IN [ 'test2' ] || c._key IN parents SORT c._key RETURN c._key");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check a ref access without any indexes
////////////////////////////////////////////////////////////////////////////////

    testInMergeAnd : function () {
      c.save({ _key: "test0" });
      for (var i = 1; i < 100; ++i) {
        c.save({ _key: "test" + i, parent: "test" + (i - 1), parents: [ "test" + (i - 1) ] });
      }

      var expected = [ 'test5', 'test7' ];
      var actual = getQueryResults("LET parents = [ 'test5', 'test7' ] FOR c IN " + cn + " FILTER c._key IN parents && c._key IN [ 'test5', 'test7' ] && c._key IN [ 'test7', 'test5' ] && c._key IN parents SORT c._key RETURN c._key");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check a ref access without any indexes, not any more!!
////////////////////////////////////////////////////////////////////////////////

    testInPrimaryConst : function () {
      c.save({ _key: "test0" });
      for (var i = 1; i < 100; ++i) {
        c.save({ _key: "test" + i, parent: "test" + (i - 1), parents: [ "test" + (i - 1) ] });
      }

      var expected = [ 'test5', 'test7' ];
      var query = "LET parents = [ 'test5', 'test7' ] FOR c IN " + cn + " FILTER c._key IN parents SORT c._key RETURN c._key";
      var actual = getQueryResults(query);
      assertEqual(expected, actual);

      assertEqual([ "SingletonNode", "CalculationNode", "IndexNode", "CalculationNode", "FilterNode", "CalculationNode", "SortNode", "CalculationNode", "ReturnNode" ], explain(query));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check a ref access without any indexes
////////////////////////////////////////////////////////////////////////////////

    testInPrimaryDynamic : function () {
      c.save({ _key: "test0" });
      for (var i = 1; i < 100; ++i) {
        c.save({ _key: "test" + i, parent: "test" + (i - 1), parents: [ "test" + (i - 1) ] });
      }

      // ascending order - input ascending
      var expected = [ 'test4', 'test6' ];
      var query = "LET parents = (FOR c IN " + cn + " FILTER c._key IN [ 'test4', 'test6' ] RETURN c._key) FOR c IN " + cn + " FILTER c._key IN parents SORT c._key RETURN c._key";
      var actual = getQueryResults(query);
      assertEqual(expected, actual);
      assertEqual([ "SingletonNode", "SubqueryStartNode", "IndexNode", "CalculationNode", "FilterNode", "CalculationNode", "SubqueryEndNode", "IndexNode", "CalculationNode", "FilterNode", "CalculationNode", "SortNode", "CalculationNode", "ReturnNode" ], explain(query));

      // ascending order - input descending
      expected = [ 'test5', 'test7' ];
      query = "LET parents = (FOR c IN " + cn + " FILTER c._key IN [ 'test7', 'test5' ] RETURN c._key) FOR c IN " + cn + " FILTER c._key IN parents SORT c._key RETURN c._key";
      actual = getQueryResults(query);
      assertEqual(expected, actual);
      assertEqual([ "SingletonNode", "SubqueryStartNode", "IndexNode", "CalculationNode", "FilterNode", "CalculationNode", "SubqueryEndNode", "IndexNode", "CalculationNode", "FilterNode", "CalculationNode", "SortNode", "CalculationNode", "ReturnNode" ], explain(query));

      // descending order - input ascending
      expected = [ 'test8', 'test6' ];
      query = "LET parents = (FOR c IN " + cn + " FILTER c._key IN [ 'test6', 'test8' ] RETURN c._key) FOR c IN " + cn + " FILTER c._key IN parents SORT c._key DESC RETURN c._key";
      actual = getQueryResults(query);
      assertEqual(expected, actual);
      assertEqual([ "SingletonNode", "SubqueryStartNode", "IndexNode", "CalculationNode", "FilterNode", "CalculationNode", "SubqueryEndNode", "IndexNode", "CalculationNode", "FilterNode", "CalculationNode", "SortNode", "CalculationNode", "ReturnNode" ], explain(query));

      // descending order - input descending
      expected = [ 'test9', 'test7' ];
      query = "LET parents = (FOR c IN " + cn + " FILTER c._key IN [ 'test9', 'test7' ] RETURN c._key) FOR c IN " + cn + " FILTER c._key IN parents SORT c._key DESC RETURN c._key";
      actual = getQueryResults(query);
      assertEqual(expected, actual);
      assertEqual([ "SingletonNode", "SubqueryStartNode", "IndexNode", "CalculationNode", "FilterNode", "CalculationNode", "SubqueryEndNode", "IndexNode", "CalculationNode", "FilterNode", "CalculationNode", "SortNode", "CalculationNode", "ReturnNode" ], explain(query));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check a ref access without any indexes
////////////////////////////////////////////////////////////////////////////////

    testInPrimaryDynamicRef : function () {
      c.save({ _key: "test0" });
      for (var i = 1; i < 100; ++i) {
        c.save({ _key: "test" + i, parent: "test" + (i - 1), parents: [ "test" + (i - 1) ] });
      }

      var expected = [ { keys: [ 'test4' ] }, { keys: [ 'test6' ] } ];
      var actual = getQueryResults("FOR c IN " + cn + " FILTER c._key IN [ 'test5', 'test7' ] SORT c._key RETURN { keys: (FOR c2 IN " + cn + " FILTER c2._key IN [ c.parent ] RETURN c2._key) }");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check a ref access without any indexes
////////////////////////////////////////////////////////////////////////////////

    testInPrimaryRef : function () {
      c.save({ _key: "test0" });
      for (var i = 1; i < 100; ++i) {
        c.save({ _key: "test" + i, parent: "test" + (i - 1), parents: [ "test" + (i - 1) ] });
      }

      var expected = [ { keys: [ 'test4' ] }, { keys: [ 'test6' ] } ];
      var actual = getQueryResults("FOR c IN " + cn + " FILTER c._key IN [ 'test5', 'test7' ] SORT c._key RETURN { keys: (FOR c2 IN " + cn + " FILTER c2._key IN c.parents SORT c2._key RETURN c2._key) }");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check a ref access without any indexes, not any more
////////////////////////////////////////////////////////////////////////////////

    testInHashConst : function () {
      c.save({ code: "test0" });
      for (var i = 1; i < 100; ++i) {
        c.save({ code: "test" + i, parent: "test" + (i - 1), parents: [ "test" + (i - 1) ] });
      }
      c.ensureUniqueConstraint("code");

      var expected = [ 'test5', 'test7' ];
      var query = "LET parents = [ 'test5', 'test7' ] FOR c IN " + cn + " FILTER c.code IN parents SORT c.code RETURN c.code";
      var actual = getQueryResults(query);
      assertEqual(expected, actual);

      assertEqual([ "SingletonNode", "CalculationNode", "IndexNode", "CalculationNode", "FilterNode", "CalculationNode", "SortNode", "CalculationNode", "ReturnNode" ], explain(query));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check a ref access without any indexes
////////////////////////////////////////////////////////////////////////////////

    testInHashDynamic : function () {
      c.save({ code: "test0" });
      for (var i = 1; i < 100; ++i) {
        c.save({ code: "test" + i, parent: "test" + (i - 1), parents: [ "test" + (i - 1) ] });
      }
      c.ensureUniqueConstraint("code");

      var expected = [ 'test5', 'test7' ];
      var query = "LET parents = (FOR c IN " + cn + " FILTER c.code IN [ 'test5', 'test7' ] RETURN c.code) FOR c IN " + cn + " FILTER c.code IN parents SORT c.code RETURN c.code";
      var actual = getQueryResults(query);
      assertEqual(expected, actual);

      assertEqual([ "SingletonNode", "SubqueryStartNode", "IndexNode", "CalculationNode", "FilterNode", "CalculationNode", "SubqueryEndNode", "IndexNode", "CalculationNode", "FilterNode", "CalculationNode", "SortNode", "CalculationNode", "ReturnNode" ], explain(query));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check a ref access without any indexes
////////////////////////////////////////////////////////////////////////////////

    testInHashDynamicRef : function () {
      c.save({ code: "test0" });
      for (var i = 1; i < 100; ++i) {
        c.save({ code: "test" + i, parent: "test" + (i - 1), parents: [ "test" + (i - 1) ] });
      }
      c.ensureUniqueConstraint("code");

      var expected = [ { keys: [ 'test4' ] }, { keys: [ 'test6' ] } ];
      var actual = getQueryResults("FOR c IN " + cn + " FILTER c.code IN [ 'test5', 'test7' ] SORT c.code RETURN { keys: (FOR c2 IN " + cn + " FILTER c2.code IN [ c.parent ] RETURN c2.code) }");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check a ref access without any indexes
////////////////////////////////////////////////////////////////////////////////

    testInHashRef1 : function () {
      c.save({ _key: "test0" });
      for (var i = 1; i < 100; ++i) {
        c.save({ _key: "test" + i, parent: "test" + (i - 1), parents: [ "test" + (i - 1) ] });
      }
      c.ensureHashIndex("parent");

      var expected = [ 'test2' ];
      var query = "LET parents = [ DOCUMENT('" + cn + "/test2').parent ] FOR c IN " + cn + " FILTER c.parent IN parents RETURN c._key";
      var actual = getQueryResults(query);
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check a ref access without any indexes
////////////////////////////////////////////////////////////////////////////////

    testInHashRef2 : function () {
      c.save({ _key: "test0" });
      for (var i = 1; i < 100; ++i) {
        c.save({ _key: "test" + i, parent: "test" + (i - 1), parents: [ "test" + (i - 1) ] });
      }
      c.ensureHashIndex("parent");

      var expected = [ 'test2' ];
      var query = "LET parents = DOCUMENT('" + cn + "/test2').parent FOR c IN " + cn + " FILTER c.parent IN [ parents ] RETURN c._key";
      var actual = getQueryResults(query);
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check a ref access without any indexes
////////////////////////////////////////////////////////////////////////////////

    testInHashRef : function () {
      c.save({ code: "test0" });
      for (var i = 1; i < 100; ++i) {
        c.save({ code: "test" + i, parent: "test" + (i - 1), parents: [ "test" + (i - 1) ] });
      }
      c.ensureUniqueConstraint("code");

      var expected = [ { keys: [ 'test4' ] }, { keys: [ 'test6' ] } ];
      var actual = getQueryResults("FOR c IN " + cn + " FILTER c.code IN [ 'test5', 'test7' ] SORT c.code RETURN { keys: (FOR c2 IN " + cn + " FILTER c2.code IN c.parents SORT c2.code RETURN c2.code) }");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check a ref access without any indexes
////////////////////////////////////////////////////////////////////////////////

    testInSkipConst : function () {
      c.save({ code: "test0" });
      for (var i = 1; i < 100; ++i) {
        c.save({ code: "test" + i, parent: "test" + (i - 1), parents: [ "test" + (i - 1) ] });
      }
      c.ensureUniqueSkiplist("code");

      var expected = [ 'test5', 'test7' ];
      var query = "LET parents = [ 'test5', 'test7' ] FOR c IN " + cn + " FILTER c.code IN parents SORT c.code RETURN c.code";
      var actual = getQueryResults(query);
      assertEqual(expected, actual);

      assertEqual([ "SingletonNode", "CalculationNode", "IndexNode", "CalculationNode", "FilterNode", "CalculationNode", "SortNode", "CalculationNode", "ReturnNode" ], explain(query));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check a ref access without any indexes
////////////////////////////////////////////////////////////////////////////////

    testInSkipDynamic : function () {
      c.save({ code: "test0" });
      for (var i = 1; i < 100; ++i) {
        c.save({ code: "test" + i, parent: "test" + (i - 1), parents: [ "test" + (i - 1) ] });
      }
      c.ensureUniqueSkiplist("code");

      var expected = [ 'test5', 'test7' ];
      var query = "LET parents = (FOR c IN " + cn + " FILTER c.code IN [ 'test5', 'test7' ] RETURN c.code) FOR c IN " + cn + " FILTER c.code IN parents SORT c.code RETURN c.code";
      var actual = getQueryResults(query);
      assertEqual(expected, actual);

      assertEqual([ "SingletonNode", "SubqueryStartNode", "IndexNode", "CalculationNode", "FilterNode", "CalculationNode", "SubqueryEndNode", "IndexNode", "CalculationNode", "FilterNode", "CalculationNode", "SortNode", "CalculationNode", "ReturnNode" ], explain(query));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check a ref access without any indexes
////////////////////////////////////////////////////////////////////////////////

    testInSkipDynamicRef : function () {
      c.save({ code: "test0" });
      for (var i = 1; i < 100; ++i) {
        c.save({ code: "test" + i, parent: "test" + (i - 1), parents: [ "test" + (i - 1) ] });
      }
      c.ensureUniqueSkiplist("code");

      var expected = [ { keys: [ 'test4' ] }, { keys: [ 'test6' ] } ];
      var actual = getQueryResults("FOR c IN " + cn + " FILTER c.code IN [ 'test5', 'test7' ] SORT c.code RETURN { keys: (FOR c2 IN " + cn + " FILTER c2.code IN [ c.parent ] RETURN c2.code) }");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check a ref access without any indexes
////////////////////////////////////////////////////////////////////////////////

    testInSkipRef : function () {
      c.save({ code: "test0" });
      for (var i = 1; i < 100; ++i) {
        c.save({ code: "test" + i, parent: "test" + (i - 1), parents: [ "test" + (i - 1) ] });
      }
      c.ensureUniqueSkiplist("code");

      var expected = [ { keys: [ 'test4' ] }, { keys: [ 'test6' ] } ];
      var actual = getQueryResults("FOR c IN " + cn + " FILTER c.code IN [ 'test5', 'test7' ] SORT c.code RETURN { keys: (FOR c2 IN " + cn + " FILTER c2.code IN c.parents SORT c2.code RETURN c2.code) }");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check a ref access without any indexes
////////////////////////////////////////////////////////////////////////////////

    testInEdgeConst : function () {
      var i;
      c.save({ _key: "test0" });
      for (i = 1; i < 100; ++i) {
        c.save({ _key: "test" + i });
      }

      var en = cn + "Edge";
      internal.db._drop(en);
      var e = internal.db._createEdgeCollection(en);

      for (i = 1; i < 100; ++i) {
        e.save(cn + "/test" + i, cn + "/test" + (i - 1), { });
      }

      var expected = [ cn + '/test4', cn + '/test6' ];
      var query = "LET parents = [ '" + cn + "/test5', '" + cn + "/test7' ] FOR c IN " + en + " FILTER c._from IN parents SORT c._to RETURN c._to";
      var actual = getQueryResults(query);
      assertEqual(expected, actual);

      assertEqual([ "SingletonNode", "CalculationNode", "IndexNode", "CalculationNode", "FilterNode", "CalculationNode", "SortNode", "CalculationNode", "ReturnNode" ], explain(query));

      internal.db._drop(en);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check a ref access without any indexes
////////////////////////////////////////////////////////////////////////////////

    testInEdgeDynamic : function () {
      var i;
      c.save({ _key: "test0" });
      for (i = 1; i < 100; ++i) {
        c.save({ _key: "test" + i });
      }

      var en = cn + "Edge";
      internal.db._drop(en);
      var e = internal.db._createEdgeCollection(en);

      for (i = 1; i < 100; ++i) {
        e.save(cn + "/test" + i, cn + "/test" + (i - 1), { });
      }

      var expected = [ cn + '/test4', cn + '/test6' ];
      var query = "LET parents = (FOR c IN " + cn + " FILTER c._key IN [ 'test5', 'test7' ] RETURN c._id) FOR c IN " + en + " FILTER c._from IN parents SORT c._to RETURN c._to";
      var actual = getQueryResults(query);
      assertEqual(expected, actual);

      assertEqual([ "SingletonNode", "SubqueryStartNode", "IndexNode", "CalculationNode", "FilterNode", "CalculationNode", "SubqueryEndNode", "IndexNode", "CalculationNode", "FilterNode", "CalculationNode", "SortNode", "CalculationNode", "ReturnNode" ], explain(query));

      internal.db._drop(en);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check a ref access without any indexes
////////////////////////////////////////////////////////////////////////////////

    testInEdgeDynamicRef : function () {
      var i;
      c.save({ _key: "test0" });
      for (i = 1; i < 100; ++i) {
        c.save({ _key: "test" + i });
      }

      var en = cn + "Edge";
      internal.db._drop(en);
      var e = internal.db._createEdgeCollection(en);

      for (i = 1; i < 100; ++i) {
        e.save(cn + "/test" + i, cn + "/test" + (i - 1), { });
      }

      var expected = [ { keys: [ cn + '/test4' ] }, { keys: [ cn + '/test6' ] } ];
      var actual = getQueryResults("FOR c IN " + cn + " FILTER c._key IN [ 'test5', 'test7' ] SORT c._key RETURN { keys: (FOR c2 IN " + en + " FILTER c2._from IN [ c._id ] RETURN c2._to) }");
      assertEqual(expected, actual);

      internal.db._drop(en);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check a ref access without any indexes
////////////////////////////////////////////////////////////////////////////////

    testInEdgeRef : function () {
      var i;
      c.save({ _key: "test0" });
      for (i = 1; i < 100; ++i) {
        c.save({ _key: "test" + i, ids: [ cn + "/test" + i ] });
      }

      var en = cn + "Edge";
      internal.db._drop(en);
      var e = internal.db._createEdgeCollection(en);

      for (i = 1; i < 100; ++i) {
        e.save(cn + "/test" + i, cn + "/test" + (i - 1), { });
      }

      var expected = [ { keys: [ cn + '/test4' ] }, { keys: [ cn + '/test6' ] } ];
      var actual = getQueryResults("FOR c IN " + cn + " FILTER c._key IN [ 'test5', 'test7' ] SORT c._key RETURN { keys: (FOR c2 IN " + en + " FILTER c2._from IN c.ids RETURN c2._to) }");
      assertEqual(expected, actual);

      internal.db._drop(en);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check invalid values for IN
////////////////////////////////////////////////////////////////////////////////

    testInvalidIn : function () {
      assertEqual([ ], getQueryResults("FOR i IN [ 1, 2, 3 ] FILTER 1 IN null RETURN i"));
      assertEqual([ ], getQueryResults("FOR i IN [ 1, 2, 3 ] FILTER 1 IN false RETURN i"));
      assertEqual([ ], getQueryResults("FOR i IN [ 1, 2, 3 ] FILTER 1 IN 1.2 RETURN i"));
      assertEqual([ ], getQueryResults("FOR i IN [ 1, 2, 3 ] FILTER 1 IN '' RETURN i"));
      assertEqual([ ], getQueryResults("FOR i IN [ 1, 2, 3 ] FILTER 1 IN {} RETURN i"));
      assertEqual([ ], getQueryResults("FOR i IN [ 1, 2, 3 ] FILTER 1 IN @values RETURN i", { values: null }));
      assertEqual([ ], getQueryResults("FOR i IN [ 1, 2, 3 ] FILTER 1 IN @values RETURN i", { values: false }));
      assertEqual([ ], getQueryResults("FOR i IN [ 1, 2, 3 ] FILTER 1 IN @values RETURN i", { values: 1.2 }));
      assertEqual([ ], getQueryResults("FOR i IN [ 1, 2, 3 ] FILTER 1 IN @values RETURN i", { values: "" }));
      assertEqual([ ], getQueryResults("FOR i IN [ 1, 2, 3 ] FILTER 1 IN @values RETURN i", { values: { } }));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check invalid values for NOT IN
////////////////////////////////////////////////////////////////////////////////

    testInvalidNotIn : function () {
      assertEqual([ ], getQueryResults("FOR i IN [ 1, 2, 3 ] FILTER 1 NOT IN null RETURN i"));
      assertEqual([ ], getQueryResults("FOR i IN [ 1, 2, 3 ] FILTER 1 NOT IN false RETURN i"));
      assertEqual([ ], getQueryResults("FOR i IN [ 1, 2, 3 ] FILTER 1 NOT IN 1.2 RETURN i"));
      assertEqual([ ], getQueryResults("FOR i IN [ 1, 2, 3 ] FILTER 1 NOT IN '' RETURN i"));
      assertEqual([ ], getQueryResults("FOR i IN [ 1, 2, 3 ] FILTER 1 NOT IN {} RETURN i"));
      assertEqual([ ], getQueryResults("FOR i IN [ 1, 2, 3 ] FILTER 1 NOT IN @values RETURN i", { values: null }));
      assertEqual([ ], getQueryResults("FOR i IN [ 1, 2, 3 ] FILTER 1 NOT IN @values RETURN i", { values: false }));
      assertEqual([ ], getQueryResults("FOR i IN [ 1, 2, 3 ] FILTER 1 NOT IN @values RETURN i", { values: 1.2 }));
      assertEqual([ ], getQueryResults("FOR i IN [ 1, 2, 3 ] FILTER 1 NOT IN @values RETURN i", { values: "" }));
      assertEqual([ ], getQueryResults("FOR i IN [ 1, 2, 3 ] FILTER 1 NOT IN @values RETURN i", { values: { } }));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check invalid values for IN
////////////////////////////////////////////////////////////////////////////////

    testInvalidInCollection : function () {
      assertEqual([ ], getQueryResults("FOR i IN " + cn + " FILTER 1 IN null RETURN i"));
      assertEqual([ ], getQueryResults("FOR i IN " + cn + " FILTER 1 IN false RETURN i"));
      assertEqual([ ], getQueryResults("FOR i IN " + cn + " FILTER 1 IN 1.2 RETURN i"));
      assertEqual([ ], getQueryResults("FOR i IN " + cn + " FILTER 1 IN '' RETURN i"));
      assertEqual([ ], getQueryResults("FOR i IN " + cn + " FILTER 1 IN {} RETURN i"));
      assertEqual([ ], getQueryResults("FOR i IN " + cn + " FILTER 1 IN @values RETURN i", { values: null }));
      assertEqual([ ], getQueryResults("FOR i IN " + cn + " FILTER 1 IN @values RETURN i", { values: false }));
      assertEqual([ ], getQueryResults("FOR i IN " + cn + " FILTER 1 IN @values RETURN i", { values: 1.2 }));
      assertEqual([ ], getQueryResults("FOR i IN " + cn + " FILTER 1 IN @values RETURN i", { values: "" }));
      assertEqual([ ], getQueryResults("FOR i IN " + cn + " FILTER 1 IN @values RETURN i", { values: { } }));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check invalid values for NOT IN
////////////////////////////////////////////////////////////////////////////////

    testInvalidNotInCollection : function () {
      assertEqual([ ], getQueryResults("FOR i IN " + cn + " FILTER 1 NOT IN null RETURN i"));
      assertEqual([ ], getQueryResults("FOR i IN " + cn + " FILTER 1 NOT IN false RETURN i"));
      assertEqual([ ], getQueryResults("FOR i IN " + cn + " FILTER 1 NOT IN 1.2 RETURN i"));
      assertEqual([ ], getQueryResults("FOR i IN " + cn + " FILTER 1 NOT IN '' RETURN i"));
      assertEqual([ ], getQueryResults("FOR i IN " + cn + " FILTER 1 NOT IN {} RETURN i"));
      assertEqual([ ], getQueryResults("FOR i IN " + cn + " FILTER 1 NOT IN @values RETURN i", { values: null }));
      assertEqual([ ], getQueryResults("FOR i IN " + cn + " FILTER 1 NOT IN @values RETURN i", { values: false }));
      assertEqual([ ], getQueryResults("FOR i IN " + cn + " FILTER 1 NOT IN @values RETURN i", { values: 1.2 }));
      assertEqual([ ], getQueryResults("FOR i IN " + cn + " FILTER 1 NOT IN @values RETURN i", { values: "" }));
      assertEqual([ ], getQueryResults("FOR i IN " + cn + " FILTER 1 NOT IN @values RETURN i", { values: { } }));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check invalid values for IN
////////////////////////////////////////////////////////////////////////////////

    testInvalidInCollectionIndex : function () {
      assertEqual([ ], getQueryResults("FOR i IN " + cn + " FILTER i._id IN null RETURN i"));
      assertEqual([ ], getQueryResults("FOR i IN " + cn + " FILTER i._id IN false RETURN i"));
      assertEqual([ ], getQueryResults("FOR i IN " + cn + " FILTER i._id IN 1.2 RETURN i"));
      assertEqual([ ], getQueryResults("FOR i IN " + cn + " FILTER i._id IN '' RETURN i"));
      assertEqual([ ], getQueryResults("FOR i IN " + cn + " FILTER i._id IN {} RETURN i"));
      assertEqual([ ], getQueryResults("FOR i IN " + cn + " FILTER i._id IN @values RETURN i", { values: null }));
      assertEqual([ ], getQueryResults("FOR i IN " + cn + " FILTER i._id IN @values RETURN i", { values: false }));
      assertEqual([ ], getQueryResults("FOR i IN " + cn + " FILTER i._id IN @values RETURN i", { values: 1.2 }));
      assertEqual([ ], getQueryResults("FOR i IN " + cn + " FILTER i._id IN @values RETURN i", { values: "" }));
      assertEqual([ ], getQueryResults("FOR i IN " + cn + " FILTER i._id IN @values RETURN i", { values: { } }));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check valid IN queries
////////////////////////////////////////////////////////////////////////////////

    testValidIn : function () {
      c.save({ value: "red" });
      c.save({ value: "green" });
      c.save({ value: "blue" });
      c.save({ value: 12 });
      c.save({ value: false });
      c.save({ value: null });

      var actual = getQueryResults("FOR i IN " + cn + " FILTER i.value IN [ 'red', 'green' ] SORT i.value RETURN i.value");
      assertEqual([ "green", "red" ], actual);

      actual = getQueryResults("FOR i IN " + cn + " FILTER i.value NOT IN [ 'red', 'green' ] SORT i.value RETURN i.value");
      assertEqual([ null, false, 12, "blue" ], actual);

      actual = getQueryResults("FOR i IN " + cn + " FILTER i.value IN [ 'green', 'blue' ] SORT i.value RETURN i.value");
      assertEqual([ "blue", "green" ], actual);

      actual = getQueryResults("FOR i IN " + cn + " FILTER i.value NOT IN [ 'green', 'blue' ] SORT i.value RETURN i.value");
      assertEqual([ null, false, 12, "red" ], actual);

      actual = getQueryResults("FOR i IN " + cn + " FILTER i.value IN [ 'foo', 'bar' ] SORT i.value RETURN i.value");
      assertEqual([ ], actual);

      actual = getQueryResults("FOR i IN " + cn + " FILTER i.value NOT IN [ 'foo', 'bar' ] SORT i.value RETURN i.value");
      assertEqual([ null, false, 12, "blue", "green", "red" ], actual);

      actual = getQueryResults("FOR i IN " + cn + " FILTER i.value IN [ 12, false ] SORT i.value RETURN i.value");
      assertEqual([ false, 12 ], actual);

      actual = getQueryResults("FOR i IN " + cn + " FILTER i.value NOT IN [ 12, false ] SORT i.value RETURN i.value");
      assertEqual([ null, "blue", "green", "red" ], actual);

      actual = getQueryResults("FOR i IN " + cn + " FILTER i.value IN [ 23, 'black', 'red', null ] SORT i.value RETURN i.value");
      assertEqual([ null, 'red' ], actual);

      actual = getQueryResults("FOR i IN " + cn + " FILTER i.value NOT IN [ 23, 'black', 'red', null ] SORT i.value RETURN i.value");
      assertEqual([ false, 12, "blue", "green" ], actual);

      c.truncate({ compact: false });
      c.save({ value: [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, "red", "blue" ]});
      actual = getQueryResults("FOR i IN " + cn + " FILTER 12 IN i.value SORT i.value RETURN LENGTH(i.value)");
      assertEqual([ 14 ], actual);

      actual = getQueryResults("FOR i IN " + cn + " FILTER 99 NOT IN i.value SORT i.value RETURN LENGTH(i.value)");
      assertEqual([ 14 ], actual);

      actual = getQueryResults("FOR i IN " + cn + " FILTER 12 NOT IN i.value SORT i.value RETURN LENGTH(i.value)");
      assertEqual([ ], actual);

      actual = getQueryResults("FOR i IN " + cn + " FILTER 13 IN i.value SORT i.value RETURN LENGTH(i.value)");
      assertEqual([ ], actual);

      actual = getQueryResults("FOR i IN " + cn + " FILTER 13 NOT IN i.value SORT i.value RETURN LENGTH(i.value)");
      assertEqual([ 14 ], actual);

      actual = getQueryResults("FOR i IN " + cn + " FILTER 'red' IN i.value SORT i.value RETURN LENGTH(i.value)");
      assertEqual([ 14 ], actual);

      actual = getQueryResults("FOR i IN " + cn + " FILTER 'red' NOT IN i.value SORT i.value RETURN LENGTH(i.value)");
      assertEqual([ ], actual);

      actual = getQueryResults("FOR i IN " + cn + " FILTER 'fuchsia' NOT IN i.value SORT i.value RETURN LENGTH(i.value)");
      assertEqual([ 14 ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check IN
////////////////////////////////////////////////////////////////////////////////

    testInListHashIndex : function () {
      for (var i = 1; i < 100; ++i) {
        c.save({ value: i });
      }
      c.ensureHashIndex("value");
      var query = "FOR x IN " + cn + " FILTER x.value IN [3,35,90] RETURN x.value";
      var expected = [ 3, 35, 90 ];
      var actual = getQueryResults(query);
      assertEqual(expected, actual);
      ruleIsUsed(query);
    },

    testInListSkiplist : function () {
      for (var i = 1; i < 100; ++i) {
        c.save({ value: i });
      }
      c.ensureSkiplist("value");
      var query = "FOR x IN " + cn + " FILTER x.value IN [3,35,90] RETURN x.value";
      var expected = [ 3, 35, 90 ];
      var actual = getQueryResults(query);
      assertEqual(expected, actual);
      ruleIsUsed(query);
    },

    testInListPrimaryIndex : function () {
      for (var i = 1; i < 100; ++i) {
        c.save({ _key: "a" + i });
      }
      var query = "FOR x IN " + cn + " FILTER x._key IN ['a3', 'a35', 'a90'] RETURN x._key";
      var expected = [ "a3", "a35", "a90" ];
      var actual = getQueryResults(query);
      assertEqual(expected, actual);
      ruleIsUsed(query);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check overlapping IN
////////////////////////////////////////////////////////////////////////////////

    testOverlappingInListHashIndex1 : function () {
      for (var i = 1; i < 100; ++i) {
        c.save({ value: i });
      }
      c.ensureHashIndex("value");
      var query = "FOR x IN " + cn + " FILTER x.value IN [3,35,90] && x.value > 3 RETURN x.value";
      var expected = [ 35, 90 ];
      var actual = getQueryResults(query);
      assertEqual(expected, actual);
      ruleIsUsed(query);
    },

    testOverlappingInListSkiplist1 : function () {
      for (var i = 1; i < 100; ++i) {
        c.save({ value: i });
      }
      c.ensureSkiplist("value");
      var query = "FOR x IN " + cn + " FILTER x.value IN [3,35,90] && x.value > 3 RETURN x.value";
      var expected = [ 35, 90 ];
      var actual = getQueryResults(query);
      assertEqual(expected, actual);
      ruleIsUsed(query);
    },

    testOverlappingInListSkiplist1Rev : function () {
      for (var i = 1; i < 100; ++i) {
        c.save({ value: i });
      }
      c.ensureSkiplist("value");
      var query = "FOR x IN " + cn + " FILTER x.value IN [3,35,90] && x.value > 3 SORT x.value DESC RETURN x.value";
      var expected = [ 90, 35 ];
      var actual = getQueryResults(query);
      assertEqual(expected, actual);
      ruleIsUsed(query);
    },

    testOverlappingInListHashIndex2 : function () {
      for (var i = 1; i < 100; ++i) {
        c.save({ value: i });
      }
      c.ensureHashIndex("value");
      var query = "FOR x IN " + cn + " FILTER x.value > 3 &&  x.value IN [3,35,90] RETURN x.value";
      var expected = [ 35, 90 ];
      var actual = getQueryResults(query);
      assertEqual(expected, actual);
      ruleIsUsed(query);
    },

    testOverlappingInListSkiplist2 : function () {
      for (var i = 1; i < 100; ++i) {
        c.save({ value: i });
      }
      c.ensureSkiplist("value");
      var query = "FOR x IN " + cn + " FILTER x.value > 3 &&  x.value IN [3,35,90] RETURN x.value";
      var expected = [ 35, 90 ];
      var actual = getQueryResults(query);
      assertEqual(expected, actual);
      ruleIsUsed(query);
    },

    testOverlappingInListSkiplist2Rev : function () {
      for (var i = 1; i < 100; ++i) {
        c.save({ value: i });
      }
      c.ensureSkiplist("value");
      var query = "FOR x IN " + cn + " FILTER x.value > 3 &&  x.value IN [3,35,90] SORT x.value DESC RETURN x.value";
      var expected = [ 90, 35 ];
      var actual = getQueryResults(query);
      assertEqual(expected, actual);
      ruleIsUsed(query);
    },

    testOverlappingInListHashIndex3 : function () {
      for (var i = 1; i < 100; ++i) {
        c.save({ value: i });
      }
      c.ensureHashIndex("value");
      var query = "FOR x IN " + cn + " FILTER (x.value > 3 || x.value == 1) && x.value IN [1,3,35,90] SORT x.value RETURN x.value";
      var expected = [ 1, 35, 90 ];
      var actual = getQueryResults(query);
      assertEqual(expected, actual);
      ruleIsUsed(query);
    },

    testOverlappingInListSkiplist3 : function () {
      for (var i = 1; i < 100; ++i) {
        c.save({ value: i });
      }
      c.ensureSkiplist("value");
      var query = "FOR x IN " + cn + " FILTER (x.value > 3 || x.value == 1) && x.value IN [1,3,35,90] SORT x.value RETURN x.value";
      var expected = [ 1, 35, 90 ];
      var actual = getQueryResults(query);
      assertEqual(expected, actual);
      ruleIsUsed(query);
    },

    testOverlappingInListSkiplist3Rev : function () {
      for (var i = 1; i < 100; ++i) {
        c.save({ value: i });
      }
      c.ensureSkiplist("value");
      var query = "FOR x IN " + cn + " FILTER (x.value > 3 || x.value == 1) && x.value IN [1,3,35,90] SORT x.value DESC RETURN x.value";
      var expected = [ 90, 35, 1 ];
      var actual = getQueryResults(query);
      assertEqual(expected, actual);
      ruleIsUsed(query);
    },

    testOverlappingInListHashIndex4 : function () {
      for (var i = 1; i < 100; ++i) {
        c.save({ value: i });
      }
      c.ensureHashIndex("value");
      var query = "FOR x IN " + cn + " FILTER (x.value IN [3,35,90] || x.value IN [3, 90]) SORT x.value RETURN x.value";
      var expected = [ 3, 35, 90 ];
      var actual = getQueryResults(query);
      assertEqual(expected, actual);
      ruleIsUsed(query);
    },

    testOverlappingInListSkiplist4 : function () {
      for (var i = 1; i < 100; ++i) {
        c.save({ value: i });
      }
      c.ensureSkiplist("value");
      var query = "FOR x IN " + cn + " FILTER (x.value IN [3,35,90] || x.value IN [3, 90]) RETURN x.value";
      var expected = [ 3, 35, 90 ];
      var actual = getQueryResults(query);
      assertEqual(expected, actual);
      ruleIsUsed(query);
    },

    testOverlappingInListSkiplist4Rev : function () {
      for (var i = 1; i < 100; ++i) {
        c.save({ value: i });
      }
      c.ensureSkiplist("value");
      var query = "FOR x IN " + cn + " FILTER (x.value IN [3,35,90] || x.value IN [3, 90]) SORT x.value DESC RETURN x.value";
      var expected = [ 90, 35, 3 ];
      var actual = getQueryResults(query);
      assertEqual(expected, actual);
      ruleIsUsed(query);
    },

    testDuplicatesListHashIndex : function () {
      for (var i = 1; i < 100; ++i) {
        c.save({ value: i });
      }
      c.ensureHashIndex("value");
      var query = "FOR x IN " + cn + " FILTER x.value IN [3,3,3] RETURN x.value";
      var expected = [ 3 ];
      var actual = getQueryResults(query);
      assertEqual(expected, actual);
      ruleIsUsed(query);
    },

    testDuplicatesListSkiplist : function () {
      for (var i = 1; i < 100; ++i) {
        c.save({ value: i });
      }
      c.ensureSkiplist("value");
      var query = "FOR x IN " + cn + " FILTER x.value IN [3,3,3] RETURN x.value";
      var expected = [ 3 ];
      var actual = getQueryResults(query);
      assertEqual(expected, actual);
      ruleIsUsed(query);
    },

    testDuplicatesOrHashIndex : function () {
      for (var i = 1; i < 100; ++i) {
        c.save({ value: i });
      }
      c.ensureHashIndex("value");
      var query = "FOR x IN " + cn + " FILTER x.value == 3 || x.value == 3 || x.value == 3 RETURN x.value";
      var expected = [ 3 ];
      var actual = getQueryResults(query);
      assertEqual(expected, actual);
      ruleIsUsed(query);
    },

    testDuplicatesOrSkiplist : function () {
      for (var i = 1; i < 100; ++i) {
        c.save({ value: i });
      }
      c.ensureSkiplist("value");
      var query = "FOR x IN " + cn + " FILTER x.value == 3 || x.value == 3 || x.value == 3 RETURN x.value";
      var expected = [ 3 ];
      var actual = getQueryResults(query);
      assertEqual(expected, actual);
      ruleIsUsed(query);
    },

    testDuplicatesListHashIndexDynamic : function () {
      for (var i = 1; i < 100; ++i) {
        c.save({ value: i });
      }
      c.ensureHashIndex("value");
      var query = "FOR x IN " + cn + " FILTER x.value IN [3,3,PASSTHRU(3)] RETURN x.value";
      var expected = [ 3 ];
      var actual = getQueryResults(query);
      assertEqual(expected, actual);
      ruleIsUsed(query);
    },

    testDuplicatesListSkiplistDynamic : function () {
      for (var i = 1; i < 100; ++i) {
        c.save({ value: i });
      }
      c.ensureSkiplist("value");
      var query = "FOR x IN " + cn + " FILTER x.value IN [PASSTHRU(3),3,PASSTHRU(3)] RETURN x.value";
      var expected = [ 3 ];
      var actual = getQueryResults(query);
      assertEqual(expected, actual);
      ruleIsUsed(query);
    },

    testDuplicatesOrHashIndexDynamic : function () {
      for (var i = 1; i < 100; ++i) {
        c.save({ value: i });
      }
      c.ensureHashIndex("value");
      var query = "FOR x IN " + cn + " FILTER x.value == PASSTHRU(3) || x.value == PASSTHRU(3) || x.value == 3 RETURN x.value";
      var expected = [ 3 ];
      var actual = getQueryResults(query);
      assertEqual(expected, actual);
      ruleIsUsed(query);
    },

    testDuplicatesOrSkiplistDynamic : function () {
      for (var i = 1; i < 100; ++i) {
        c.save({ value: i });
      }
      c.ensureSkiplist("value");
      var query = "FOR x IN " + cn + " FILTER x.value == 3 || x.value == PASSTHRU(3) || x.value == 3 RETURN x.value";
      var expected = [ 3 ];
      var actual = getQueryResults(query);
      assertEqual(expected, actual);
      ruleIsUsed(query);
    },

    testOverlappingRangesListSkiplist1 : function () {
      for (var i = 1; i < 100; ++i) {
        c.save({ value: i });
      }
      c.ensureSkiplist("value");
      var query = "FOR x IN " + cn + " FILTER (x.value > 3 || x.value < 90) SORT x.value RETURN x.value";
      var expected = [ ];
      for (i = 1; i < 100; i++) {
        expected.push(i);
      }
      var actual = getQueryResults(query);
      assertEqual(expected, actual);
      ruleIsUsed(query);
    },

    testOverlappingRangesListHashIndex1 : function () {
      for (var i = 1; i < 100; ++i) {
        c.save({ value: i });
      }
      c.ensureHashIndex("value");
      var query = "FOR x IN " + cn + " FILTER (x.value > 3 || x.value < 90) RETURN x.value";
      ruleIsUsed(query);
    },

    testOverlappingRangesListSkiplist2 : function () {
      for (var i = 1; i < 100; ++i) {
        c.save({ value: i });
      }
      c.ensureSkiplist("value");
      var query = "FOR i IN " + cn + " FILTER i.value == 8 || i.value <= 7 SORT i.value DESC RETURN i.value";
      var expected = [ 8, 7, 6, 5, 4, 3, 2, 1 ];
      var actual = getQueryResults(query);
      assertEqual(expected, actual);
      ruleIsUsed(query);
    },

    testOverlappingRangesListSkiplist2Rev : function () {
      for (var i = 1; i < 100; ++i) {
        c.save({ value: i });
      }
      c.ensureSkiplist("value");
      var query = "FOR i IN " + cn + " FILTER i.value == 8 || i.value <= 7 SORT i.value RETURN i.value";
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8 ];
      var actual = getQueryResults(query);
      assertEqual(expected, actual);
      ruleIsUsed(query);
    },

    testOverlappingRangesListHashIndex2 : function () {
      for (var i = 1; i < 100; ++i) {
        c.save({ value: i });
      }
      c.ensureHashIndex("value");
      var query = "FOR i IN " + cn + " FILTER i.value == 8 || i.value <= 7 RETURN i.value";
      ruleIsUsed(query);
    },

    testNestedOrHashIndex : function () {
      for (var i = 1; i < 5; ++i) {
        c.save({ value: i });
      }
      c.ensureHashIndex("value");
      var query = "FOR j IN [1,2,3] FOR i IN " + cn + " FILTER i.value == j || i.value == j + 1 || i.value == j + 2 RETURN i.value";
      var expected = [ 1, 2, 3, 2, 3, 4, 3, 4 ];
      var actual = getQueryResults(query);
      assertEqual(expected.sort(), actual.sort());
      ruleIsUsed(query);
    },

    testNestedOrSkiplist : function () {
      for (var i = 1; i < 5; ++i) {
        c.save({ value: i });
      }
      c.ensureSkiplist("value");
      var query = "FOR j IN [1,2,3] FOR i IN " + cn + " FILTER i.value == j || i.value == j + 1 || i.value == j + 2 RETURN i.value";
      var expected = [ 1, 2, 3, 2, 3, 4, 3, 4 ];
      var actual = getQueryResults(query);
      assertEqual(expected, actual);
      ruleIsUsed(query);
    },

    testNestedOrSkiplistSorted : function () {
      for (var i = 1; i < 5; ++i) {
        c.save({ value: i });
      }
      c.ensureSkiplist("value");
      var query = "FOR j IN [1,2,3] FOR i IN " + cn + " FILTER i.value == j || i.value == j + 1 || i.value == j + 2 SORT i.value RETURN i.value";
      var expected = [ 1, 2, 2, 3, 3, 3, 4, 4 ];
      var actual = getQueryResults(query);
      assertEqual(expected, actual);
      ruleIsUsed(query);
    },

    testNestedOrSkiplistSortedDesc : function () {
      for (var i = 1; i < 5; ++i) {
        c.save({ value: i });
      }
      c.ensureSkiplist("value");
      var query = "FOR j IN [1,2,3] FOR i IN " + cn + " FILTER i.value == j || i.value == j + 1 || i.value == j + 2 SORT i.value DESC RETURN i.value";
      var expected = [ 4, 4, 3, 3, 3, 2, 2, 1 ];
      var actual = getQueryResults(query);
      assertEqual(expected, actual);
      ruleIsUsed(query);
    },

    testDudSkiplist : function () {
      for (var i = 1; i < 5; ++i) {
        c.save({ value1: i, value2: i + 5 });
      }
      c.ensureSkiplist("value1");
      var query = "FOR i IN " + cn + " FILTER i.value1 == 1 || i.value2 == 8 || i.value1 == 2 SORT i.value1 LIMIT 2 RETURN i.value1";
      var expected = [ 1, 2 ];
      var actual = getQueryResults(query);
      assertEqual(expected, actual);
      ruleIsUsed(query);
    },

    testOverlappingDynamicAndNonDynamic: function () {
      for (var i = 1; i <= 5; ++i) {
        c.save({ value1: i, value2: i + 5 });
      }
      c.ensureSkiplist("value1");
      var query = "FOR x IN " + cn + " FILTER x.value1 IN [PASSTHRU(3),PASSTHRU(3),PASSTHRU(3), 4] || x.value1 in [3,4,5,9] RETURN x.value1";
      var expected = [ 3, 4, 5 ];
      var actual = getQueryResults(query);
      assertEqual(expected, actual);
      ruleIsUsed(query);
    },

    testSkiplistMoreThanOne1 : function () {
      for (var i = 1; i <= 10; i++) {
        for (var j = 1; j <= 10; j++) {
          c.save({value1 : i, value2: j});
        }
      }
      c.ensureSkiplist("value1", "value2");

      var query = "FOR x in " + cn + " FILTER (x.value1 in [4,5] && x.value2 <= 2) || (x.value1 in [1,6] && x.value2 == 9) RETURN x.value1";
      var expected = [ 1, 4, 4, 5, 5, 6 ];
      var actual = getQueryResults(query);
      // Sorting is not guaranteed any more.
      // We sort the result ourself
      actual.sort(function (a, b) {
        return a - b;
      });
      assertEqual(expected, actual);
      ruleIsUsed(query);
    },

    testSkiplistMoreThanOne1Desc : function () {
      for (var i = 1; i <= 10; i++) {
        for (var j = 1; j <= 10; j++) {
          c.save({value1 : i, value2: j});
        }
      }
      c.ensureSkiplist("value1", "value2");

      var query = "FOR x in " + cn + " FILTER (x.value1 in [4,5] && x.value2 <= 2) || (x.value1 in [1,6] && x.value2 == 9) SORT x.value1 DESC RETURN x.value1";
      var expected = [ 6, 5, 5, 4, 4, 1 ];
      var actual = getQueryResults(query);
      assertEqual(expected, actual);
      ruleIsUsed(query);
    },

    testSkiplistMoreThanOne2 : function () {
      for (var i = 1; i <= 10; i++) {
        for (var j = 1; j <= 10; j++) {
          c.save({value1 : i, value2: j});
        }
      }
      c.ensureSkiplist("value1", "value2");

      var query = "FOR x in " + cn + " FILTER (x.value1 in [4,5] && x.value2 <= PASSTHRU(2)) || (x.value1 in [1,6] && x.value2 == 9) RETURN x.value1";
      var expected = [ 1, 4, 4, 5, 5, 6 ];
      var actual = getQueryResults(query);
      // Sorting is not guaranteed any more.
      // We sort the result ourself
      actual.sort(function (a, b) {
        return a - b;
      });
      assertEqual(expected, actual);
      ruleIsUsed(query);
    },

    testSkiplistMoreThanOne2Desc : function () {
      for (var i = 1; i <= 10; i++) {
        for (var j = 1; j <= 10; j++) {
          c.save({value1 : i, value2: j});
        }
      }
      c.ensureSkiplist("value1", "value2");

      var query = "FOR x in " + cn + " FILTER (x.value1 in [4,5] && x.value2 <= PASSTHRU(2)) || (x.value1 in [1,6] && x.value2 == 9) SORT x.value1 DESC RETURN x.value1";
      var expected = [ 6, 5, 5, 4, 4, 1 ];
      var actual = getQueryResults(query);
      assertEqual(expected, actual);
      ruleIsUsed(query);
    },

    testSkiplistMoreThanOne3 : function () {
      for (var i = 1; i <= 10; i++) {
        for (var j = 1; j <= 10; j++) {
          c.save({value1 : i, value2: j});
        }
      }
      c.ensureSkiplist("value1", "value2");

      var query = "FOR x in " + cn + " FILTER (x.value1 in [4,5] && x.value2 <= PASSTHRU(2)) || (x.value1 in [PASSTHRU(1),6] && x.value2 == 9) RETURN x.value1";
      var expected = [ 1, 4, 4, 5, 5, 6 ];
      var actual = getQueryResults(query);
      // Sorting is not guaranteed any more.
      // We sort the result ourself
      actual.sort(function (a, b) {
        return a - b;
      });
      assertEqual(expected, actual);
      ruleIsUsed(query);
    },

    testSkiplistMoreThanOne4 : function () {
      for (var i = 1;i <= 10;i++) {
        for (var j = 1; j <= 30; j++) {
          c.save({value1 : i, value2: j, value3: i + j,
            value4: 'somethings' + 2*j });
        }
      }

      c.ensureSkiplist("value1", "value2", "value3", "value4");

      var query = "FOR x IN " + cn + " FILTER (x.value1 IN [1, 2, 3] && x.value1 IN [2, 3, 4] && x.value2 == 10 && x.value3 <= 20) || (x.value1 == 1 && x.value2 == 2 && x.value3 >= 0 && x.value3 <= 6 && x.value4 in ['somethings2', 'somethings4'] ) RETURN [x.value1, x.value2, x.value3, x.value4]";
      var expected = [
                       [ 1, 2, 3, "somethings4" ],
                       [ 2, 10, 12, "somethings20" ],
                       [ 3, 10, 13, "somethings20" ],
                     ];
      var actual = getQueryResults(query);
      // Sorting is not guaranteed any more.
      // We sort the result ourself
      actual.sort(function (a, b) {
        if (a[0] !== b[0]) {
          return a[0] - b[0];
        }
        return a[2] - b[2];
      });
      assertEqual(expected, actual);
      ruleIsUsed(query);
    },

    testSkiplistMoreThanOne4Desc : function () {
      for (var i = 1;i <= 10;i++) {
        for (var j = 1; j <= 30; j++) {
          c.save({value1 : i, value2: j, value3: i + j,
            value4: 'somethings' + 2*j });
        }
      }

      c.ensureSkiplist("value1", "value2", "value3", "value4");

      var query = "FOR x IN " + cn + " FILTER (x.value1 IN [1, 2, 3] && x.value1 IN [2, 3, 4] && x.value2 == 10 && x.value3 <= 20) || (x.value1 == 1 && x.value2 == 2 && x.value3 >= 0 && x.value3 <= 6 && x.value4 in ['somethings2', 'somethings4'] ) SORT x.value1 DESC RETURN [x.value1, x.value2, x.value3, x.value4]";
      var expected = [
                       [ 3, 10, 13, "somethings20" ],
                       [ 2, 10, 12, "somethings20" ],
                       [ 1, 2, 3, "somethings4" ],
                     ];
      var actual = getQueryResults(query);
      assertEqual(expected, actual);
      ruleIsUsed(query);
    },

    testSkiplistMoreThanOne5 : function () {

      for (var i = 1;i <= 10;i++) {
        for (var j = 1; j <= 10; j++) {
          for (var k = 1; k <= 10; k++) {
            c.save({value1 : i, value2: j, value3: k,
              value4: 'somethings' + j * 2 });
          }
        }
      }

      c.ensureSkiplist("value1", "value2", "value3", "value4");

      var query = "FOR x IN " + cn + " FILTER (x.value1 IN [PASSTHRU(1), PASSTHRU(2), PASSTHRU(3)] && x.value1 IN [2, 3, 4] && x.value2 == PASSTHRU(10) && x.value3 <= 2) || (x.value1 == 1 && x.value2 == 2 && x.value3 >= 0 && x.value3 == PASSTHRU(6) && x.value4 in ['somethings2', PASSTHRU('somethings4')] ) RETURN [x.value1, x.value2, x.value3, x.value4]";
      var expected = [
                       [ 1, 2, 6, "somethings4" ]  ,
                       [ 2, 10, 1, "somethings20" ],
                       [ 2, 10, 2, "somethings20" ],
                       [ 3, 10, 1, "somethings20" ],
                       [ 3, 10, 2, "somethings20" ]
                     ];
      var actual = getQueryResults(query);
      // Sorting is not guaranteed any more.
      // We sort the result ourself
      actual.sort(function (a, b) {
        if (a[0] !== b[0]) {
          return a[0] - b[0];
        }
        return a[2] - b[2];
      });
      assertEqual(expected, actual);
      ruleIsUsed(query);
    },

    testSkiplistMoreThanOne5Desc : function () {

      for (var i = 1;i <= 10;i++) {
        for (var j = 1; j <= 10; j++) {
          for (var k = 1; k <= 10; k++) {
            c.save({value1 : i, value2: j, value3: k,
              value4: 'somethings' + 2*j });
          }
        }
      }

      c.ensureSkiplist("value1", "value2", "value3", "value4");

      var query = "FOR x IN " + cn + " FILTER (x.value1 IN [PASSTHRU(1), PASSTHRU(2), PASSTHRU(3)] && x.value1 IN [2, 3, 4] && x.value2 == PASSTHRU(10) && x.value3 <= 2) || (x.value1 == 1 && x.value2 == 2 && x.value3 >= 0 && x.value3 == PASSTHRU(6) && x.value4 in ['somethings2', PASSTHRU('somethings4')] ) SORT x.value1 DESC RETURN [x.value1, x.value2, x.value3, x.value4]";
      var expected = [
                       [ 3, 10, 2, "somethings20" ],
                       [ 3, 10, 1, "somethings20" ],
                       [ 2, 10, 2, "somethings20" ],
                       [ 2, 10, 1, "somethings20" ],
                       [ 1, 2, 6, "somethings4" ]  ,
                     ];
      var actual = getQueryResults(query);
      assertEqual(expected, actual);
      ruleIsUsed(query);
    },

    testSkiplistMoreThanOne6 : function () {

      for (var i = 1;i <= 10;i++) {
        for (var j = 1; j <= 10; j++) {
          for (var k = 1; k <= 10; k++) {
            c.save({value1 : i, value2: j, value3: k,
              value4: 'somethings' + 2*j });
          }
        }
      }

      c.ensureSkiplist("value1", "value2", "value3", "value4");

      var query = "FOR x IN " + cn + " FILTER (x.value1 IN [PASSTHRU(1), PASSTHRU(2), PASSTHRU(3)] && x.value1 IN PASSTHRU([2, 3, 4]) && x.value2 == PASSTHRU(10) && x.value3 <= 2) || (x.value1 == 1 && x.value2 == 2 && x.value3 >= 0 && x.value3 == PASSTHRU(6) && x.value4 in ['somethings2', PASSTHRU('somethings4')] ) RETURN [x.value1, x.value2, x.value3, x.value4]";
      var expected = [
                       [ 1, 2, 6, "somethings4" ]  ,
                       [ 2, 10, 1, "somethings20" ],
                       [ 2, 10, 2, "somethings20" ],
                       [ 3, 10, 1, "somethings20" ],
                       [ 3, 10, 2, "somethings20" ]
                     ];
      var actual = getQueryResults(query);
      // Sorting is not guaranteed any more.
      // We sort the result ourself
      actual.sort(function (a, b) {
        if (a[0] !== b[0]) {
          return a[0] - b[0];
        }
        return a[2] - b[2];
      });
      assertEqual(expected, actual);
      ruleIsUsed(query);
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ahuacatlQueryOptimizerInWithLongArraysTestSuite () {
  var c = null;
  var cn = "UnitTestsAhuacatlOptimizerInWithLongArrays";

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      internal.db._drop(cn);
      c = internal.db._create(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      internal.db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check long array with numbers
////////////////////////////////////////////////////////////////////////////////

    testInLongArrayWithNumbersForward : function () {
      var i, comp = [ ];
      for (i = 0; i < 1000; ++i) {
        c.insert({ value: i });
        comp.push(i);
      }

      var result;
      result = AQL_EXECUTE("FOR doc IN @@cn FILTER doc.value IN @comp RETURN 1", { "@cn": cn, comp: comp }).json;
      assertEqual(1000, result.length);

      result = AQL_EXECUTE("FOR doc IN @@cn FILTER doc.value NOT IN @comp RETURN 1", { "@cn": cn, comp: comp }).json;
      assertEqual(0, result.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check long array with numbers
////////////////////////////////////////////////////////////////////////////////

    testInLongArrayWithNumbersBackward : function () {
      var i, comp = [ ];
      for (i = 0; i < 1000; ++i) {
        c.insert({ value: i });
        comp.push(1000 - 1 - i);
      }

      var result;
      result = AQL_EXECUTE("FOR doc IN @@cn FILTER doc.value IN @comp RETURN 1", { "@cn": cn, comp: comp }).json;
      assertEqual(1000, result.length);

      result = AQL_EXECUTE("FOR doc IN @@cn FILTER doc.value NOT IN @comp RETURN 1", { "@cn": cn, comp: comp }).json;
      assertEqual(0, result.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check long array with strings
////////////////////////////////////////////////////////////////////////////////

    testInLongArrayWithStringsForward : function () {
      var i, comp = [ ];
      for (i = 0; i < 1000; ++i) {
        c.insert({ value: "test" + i });
        comp.push("test" + i);
      }

      var result;
      result = AQL_EXECUTE("FOR doc IN @@cn FILTER doc.value IN @comp RETURN 1", { "@cn": cn, comp: comp }).json;
      assertEqual(1000, result.length);

      result = AQL_EXECUTE("FOR doc IN @@cn FILTER doc.value NOT IN @comp RETURN 1", { "@cn": cn, comp: comp }).json;
      assertEqual(0, result.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check long array with strings
////////////////////////////////////////////////////////////////////////////////

    testInLongArrayWithStringsBackward : function () {
      var i, comp = [ ];
      for (i = 0; i < 1000; ++i) {
        c.insert({ value: "test" + i });
        comp.push("test" + (1000 - 1 - i));
      }

      var result;
      result = AQL_EXECUTE("FOR doc IN @@cn FILTER doc.value IN @comp RETURN 1", { "@cn": cn, comp: comp }).json;
      assertEqual(1000, result.length);

      result = AQL_EXECUTE("FOR doc IN @@cn FILTER doc.value NOT IN @comp RETURN 1", { "@cn": cn, comp: comp }).json;
      assertEqual(0, result.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check long array with objects
////////////////////////////////////////////////////////////////////////////////

    testInLongArrayWithObjectsForward1 : function () {
      var i, comp = [ ];
      for (i = 0; i < 1000; ++i) {
        c.insert({ value1: "test" + i, value2: i });
        comp.push({ value1: "test" + i, value2: i });
      }

      var result;
      result = AQL_EXECUTE("FOR doc IN @@cn FILTER { value1: doc.value1, value2: doc.value2 } IN @comp RETURN 1", { "@cn": cn, comp: comp }).json;
      assertEqual(1000, result.length);

      result = AQL_EXECUTE("FOR doc IN @@cn FILTER { value1: doc.value1, value2: doc.value2 } NOT IN @comp RETURN 1", { "@cn": cn, comp: comp }).json;
      assertEqual(0, result.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check long array with objects
////////////////////////////////////////////////////////////////////////////////

    testInLongArrayWithObjectsForward2 : function () {
      var i, comp = [ ];
      for (i = 0; i < 1000; ++i) {
        c.insert({ value1: "test" + i, value2: 1000 - 1 - i });
        comp.push({ value1: "test" + i, value2: 1000 - 1 - i });
      }

      var result;
      result = AQL_EXECUTE("FOR doc IN @@cn FILTER { value1: doc.value1, value2: doc.value2 } IN @comp RETURN 1", { "@cn": cn, comp: comp }).json;
      assertEqual(1000, result.length);

      result = AQL_EXECUTE("FOR doc IN @@cn FILTER { value1: doc.value1, value2: doc.value2 } NOT IN @comp RETURN 1", { "@cn": cn, comp: comp }).json;
      assertEqual(0, result.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check long array with objects
////////////////////////////////////////////////////////////////////////////////

    testInLongArrayWithObjectsBackward1 : function () {
      var i, comp = [ ];
      for (i = 0; i < 1000; ++i) {
        c.insert({ value1: "test" + (1000 - 1 - i), value2: 1000 - 1 - i });
        comp.push({ value1: "test" + (1000 - 1 - i), value2: 1000 - 1 - i });
      }

      var result;
      result = AQL_EXECUTE("FOR doc IN @@cn FILTER { value1: doc.value1, value2: doc.value2 } IN @comp RETURN 1", { "@cn": cn, comp: comp }).json;
      assertEqual(1000, result.length);

      result = AQL_EXECUTE("FOR doc IN @@cn FILTER { value1: doc.value1, value2: doc.value2 } NOT IN @comp RETURN 1", { "@cn": cn, comp: comp }).json;
      assertEqual(0, result.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check long array with objects
////////////////////////////////////////////////////////////////////////////////

    testInLongArrayWithObjectsBackward2 : function () {
      var i, comp = [ ];
      for (i = 0; i < 1000; ++i) {
        c.insert({ value1: "test" + (1000 - 1 - i), value2: i });
        comp.push({ value1: "test" + (1000 - 1 - i), value2: i });
      }

      var result;
      result = AQL_EXECUTE("FOR doc IN @@cn FILTER { value1: doc.value1, value2: doc.value2 } IN @comp RETURN 1", { "@cn": cn, comp: comp }).json;
      assertEqual(1000, result.length);

      result = AQL_EXECUTE("FOR doc IN @@cn FILTER { value1: doc.value1, value2: doc.value2 } NOT IN @comp RETURN 1", { "@cn": cn, comp: comp }).json;
      assertEqual(0, result.length);
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ahuacatlQueryOptimizerInTestSuite);
jsunity.run(ahuacatlQueryOptimizerInWithLongArraysTestSuite);

return jsunity.done();

