/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, AQL_EXPLAIN */

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

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ahuacatlQueryOptimizerInTestSuite () {
  var c = null;
  var cn = "UnitTestsAhuacatlOptimizerIn";
  
  var explain = function (query, params) {
    return helper.getCompactPlan(AQL_EXPLAIN(query, params, { optimizer: { rules: [ "-all", "+use-indexes" ] } })).map(function(node) { return node.type; });
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
/// @brief check a ref access without any indexes
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

      assertEqual([ "SingletonNode", "CalculationNode", "ScatterNode", "RemoteNode", "IndexNode", "RemoteNode", "GatherNode", "CalculationNode", "FilterNode", "CalculationNode", "SortNode", "CalculationNode", "ReturnNode" ], explain(query));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check a ref access without any indexes
////////////////////////////////////////////////////////////////////////////////

    testInPrimaryDynamic : function () {
      c.save({ _key: "test0" });
      for (var i = 1; i < 100; ++i) {
        c.save({ _key: "test" + i, parent: "test" + (i - 1), parents: [ "test" + (i - 1) ] });
      }

      var expected = [ 'test5', 'test7' ];
      var query = "LET parents = (FOR c IN " + cn + " FILTER c._key IN [ 'test5', 'test7' ] RETURN c._key) FOR c IN " + cn + " FILTER c._key IN parents SORT c._key RETURN c._key";
      var actual = getQueryResults(query);
      assertEqual(expected, actual);

      assertEqual([ "SingletonNode", "SubqueryNode", "ScatterNode", "RemoteNode", "IndexNode", "RemoteNode", "GatherNode", "CalculationNode", "FilterNode", "CalculationNode", "SortNode", "CalculationNode", "ReturnNode" ], explain(query));
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
/// @brief check a ref access without any indexes
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
      
      assertEqual([ "SingletonNode", "CalculationNode", "ScatterNode", "RemoteNode", "IndexNode", "RemoteNode", "GatherNode", "CalculationNode", "FilterNode", "CalculationNode", "SortNode", "CalculationNode", "ReturnNode" ], explain(query));
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
      
      assertEqual([ "SingletonNode", "SubqueryNode", "ScatterNode", "RemoteNode", "IndexNode", "RemoteNode", "GatherNode", "CalculationNode", "FilterNode", "CalculationNode", "SortNode", "CalculationNode", "ReturnNode" ], explain(query));
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
      
      assertEqual([ "SingletonNode", "CalculationNode", "ScatterNode", "RemoteNode", "IndexNode", "RemoteNode", "GatherNode", "CalculationNode", "FilterNode", "CalculationNode", "SortNode", "CalculationNode", "ReturnNode" ], explain(query));
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
      
      assertEqual([ "SingletonNode", "SubqueryNode", "ScatterNode", "RemoteNode", "IndexNode", "RemoteNode", "GatherNode", "CalculationNode", "FilterNode", "CalculationNode", "SortNode", "CalculationNode", "ReturnNode" ], explain(query));
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
      
      assertEqual([ "SingletonNode", "CalculationNode", "ScatterNode", "RemoteNode", "IndexNode", "RemoteNode", "GatherNode", "CalculationNode", "FilterNode", "CalculationNode", "SortNode", "CalculationNode", "ReturnNode" ], explain(query));
      
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
      
      assertEqual([ "SingletonNode", "SubqueryNode", "ScatterNode", "RemoteNode", "IndexNode", "RemoteNode", "GatherNode", "CalculationNode", "FilterNode", "CalculationNode", "SortNode", "CalculationNode", "ReturnNode" ], explain(query));
      
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
      
      c.truncate();
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

  };

}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ahuacatlQueryOptimizerInTestSuite);

return jsunity.done();

