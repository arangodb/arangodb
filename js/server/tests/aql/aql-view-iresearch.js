/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertUndefined, assertEqual, assertTrue, assertFalse, AQL_EXECUTE */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for iresearch usage
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
var db = require("@arangodb").db;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function iResearchAqlTestSuite () {
  var c;
  var v;

  return {
    setUpAll : function () {
      db._drop("UnitTestsCollection");
      c = db._create("UnitTestsCollection");

      db._dropView("UnitTestsView");
      v = db._createView("UnitTestsView", "iresearch", {});
      var meta = { links: { "UnitTestsCollection": { includeAllFields: true } } };
      v.properties(meta);

      for (let i = 0; i < 5; i++) {
        c.save({ a: "foo", b: "bar", c: i });
        c.save({ a: "foo", b: "baz", c: i });
        c.save({ a: "bar", b: "foo", c: i });
      }
      for (let i = 0; i < 4; i++) {
        c.save({ a: "baz", b: "foo", c: i });
      }

      // save last doc with waitForSync
      c.save({ a: "baz", b: "foo", c: 4 }, { waitForSync: true });
    },

    tearDownAll : function () {
      var meta = { links : { "UnitTestsCollection": null } };
      v.properties(meta);
      v.drop();
      db._drop("UnitTestsCollection");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test no fullcount
////////////////////////////////////////////////////////////////////////////////

    testAttributeEqualityFilter : function () {
      var result = AQL_EXECUTE("FOR doc IN VIEW UnitTestsView FILTER doc.a == 'foo' RETURN doc", null, { }).json;

      assertEqual(result.length, 10);
      result.forEach(function(res) {
        assertEqual(res.a, "foo");
      });
    },

    testMultipleAttributeEqualityFilter : function () {
      var result = AQL_EXECUTE("FOR doc IN VIEW UnitTestsView FILTER doc.a == 'foo' && doc.b == 'bar' RETURN doc", null, { }).json;

      assertEqual(result.length, 5);
      result.forEach(function(res) {
        assertEqual(res.a, "foo");
        assertEqual(res.b, "bar");
      });
    },

    testMultipleAttributeEqualityFilterSortAttribute : function () {
      var result = AQL_EXECUTE("FOR doc IN VIEW UnitTestsView FILTER doc.a == 'foo' && doc.b == 'bar' SORT doc.c RETURN doc", null, { }).json;

      assertEqual(result.length, 5);
      var last = -1;
      result.forEach(function(res) {
        assertEqual(res.a, "foo");
        assertEqual(res.b, "bar");
        assertEqual(res.c, last + 1);
        last = res.c;
      });
    },

    testMultipleAttributeEqualityFilterSortAttributeDesc : function () {
      var result = AQL_EXECUTE("FOR doc IN VIEW UnitTestsView FILTER doc.a == 'foo' AND doc.b == 'bar' SORT doc.c DESC RETURN doc", null, { }).json;

      assertEqual(result.length, 5);
      var last = 5;
      result.forEach(function(res) {
        assertEqual(res.a, "foo");
        assertEqual(res.b, "bar");
        assertEqual(res.c, last - 1);
        last = res.c;
      });
    },

    testAttributeLessFilter : function () {
      var result = AQL_EXECUTE("FOR doc IN VIEW UnitTestsView FILTER doc.c < 2 RETURN doc", null, { }).json;

      assertEqual(result.length, 8);
      result.forEach(function(res) {
        assertTrue(res.c < 2);
      });
    },

    testAttributeLeqFilter : function () {
      var result = AQL_EXECUTE("FOR doc IN VIEW UnitTestsView FILTER doc.c <= 2 RETURN doc", null, { }).json;

      assertEqual(result.length, 12);
      result.forEach(function(res) {
        assertTrue(res.c <= 2);
      });
    },

    testAttributeGeqFilter : function () {
      var result = AQL_EXECUTE("FOR doc IN VIEW UnitTestsView FILTER doc.c >= 2 RETURN doc", null, { }).json;

      assertEqual(result.length, 12);
      result.forEach(function(res) {
        assertTrue(res.c >= 2);
      });
    },

    testAttributeGreaterFilter : function () {
      var result = AQL_EXECUTE("FOR doc IN VIEW UnitTestsView FILTER doc.c > 2 RETURN doc", null, { }).json;

      assertEqual(result.length, 8);
      result.forEach(function(res) {
        assertTrue(res.c > 2);
      });
    },

    testAttributeOpenIntervalFilter : function () {
      var result = AQL_EXECUTE("FOR doc IN VIEW UnitTestsView FILTER doc.c > 1 AND doc.c < 3 RETURN doc", null, { }).json;

      assertEqual(result.length, 4);
      result.forEach(function(res) {
        assertTrue(res.c > 1 && res.c < 3);
      });
    },

    testAttributeClosedIntervalFilter : function () {
      var result = AQL_EXECUTE("FOR doc IN VIEW UnitTestsView FILTER doc.c >= 1 AND doc.c <= 3 RETURN doc", null, { }).json;

      assertEqual(result.length, 12);
      result.forEach(function(res) {
        assertTrue(res.c >= 1 && res.c <= 3);
      });
    },

    testAttributeIntervalExclusionFilter : function () {
      var result = AQL_EXECUTE("FOR doc IN VIEW UnitTestsView FILTER doc.c < 1 OR doc.c > 3 RETURN doc", null, { }).json;

      assertEqual(result.length, 8);
      result.forEach(function(res) {
        assertTrue(res.c < 1 || res.c > 3);
      });
    },

    testAttributeNeqFilter : function () {
      var result = AQL_EXECUTE("FOR doc IN VIEW UnitTestsView FILTER doc.a != 'foo'  RETURN doc", null, { }).json;

      assertEqual(result.length, 10);
      result.forEach(function(res) {
        assertFalse(res.a === 'foo');
      });
    },

    testStartsWithFilter : function () {
      var result = AQL_EXECUTE("FOR doc IN VIEW UnitTestsView FILTER STARTS_WITH(doc.a, 'fo') RETURN doc", null, { }).json;

      assertEqual(result.length, 10);
      result.forEach(function(res) {
        assertEqual(res.a, 'foo');
      });
    },

    testStartsWithFilter2 : function () {
      var result = AQL_EXECUTE("FOR doc IN VIEW UnitTestsView FILTER STARTS_WITH(doc.b, 'ba') RETURN doc", null, { }).json;

      assertEqual(result.length, 10);
      result.forEach(function(res) {
        assertTrue(res.b === 'bar' || res.b === 'baz');
      });
    },

    testStartsWithFilterSort : function () {
      var result = AQL_EXECUTE("FOR doc IN VIEW UnitTestsView FILTER STARTS_WITH(doc.b, 'ba') && doc.c == 0 SORT doc.b RETURN doc", null, { }).json;

      assertEqual(result.length, 2);
      assertEqual(result[0].b, 'bar');
      assertEqual(result[1].b, 'baz');
      assertEqual(result[0].c, 0);
      assertEqual(result[1].c, 0);
    },

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(iResearchAqlTestSuite);

return jsunity.done();
