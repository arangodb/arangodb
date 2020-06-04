/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertTrue, fail, AQL_EXECUTE */
////////////////////////////////////////////////////////////////////////////////
/// @brief tests for query language, functions
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

var internal = require("internal");
var errors = internal.errors;
var jsunity = require("jsunity");
var helper = require("@arangodb/aql-helper");
var getQueryResults = helper.getQueryResults;
var assertQueryError = helper.assertQueryError;
var assertQueryWarningAndNull = helper.assertQueryWarningAndNull;
var db = require("org/arangodb").db;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ahuacatlMiscFunctionsTestSuite () { return {

////////////////////////////////////////////////////////////////////////////////
/// @brief test parse identifier function
////////////////////////////////////////////////////////////////////////////////

    testParseIdentifier : function () {
      var actual;
      var buildQuery = function (nr, input) {
        switch (nr) {
          case 0:
            return `RETURN PARSE_IDENTIFIER(${input})`;
          case 1:
            return `RETURN NOOPT(PARSE_IDENTIFIER(${input}))`;
          default:
            assertTrue(false, "Undefined state");
        }
      };

      for (var i = 0; i < 2; ++i) {
        actual = getQueryResults(buildQuery(i, "'foo/bar'"));
        assertEqual([ { collection: 'foo', key: 'bar' } ], actual);

        actual = getQueryResults(buildQuery(i, "'this-is-a-collection-name/and-this-is-an-id'"));
        assertEqual([ { collection: 'this-is-a-collection-name', key: 'and-this-is-an-id' } ], actual);

        actual = getQueryResults(buildQuery(i, "'MY_COLLECTION/MY_DOC'"));
        assertEqual([ { collection: 'MY_COLLECTION', key: 'MY_DOC' } ], actual);

        actual = getQueryResults(buildQuery(i, "'_users/AbC'"));
        assertEqual([ { collection: '_users', key: 'AbC' } ], actual);

        actual = getQueryResults(buildQuery(i, "{ _id: 'foo/bar', value: 'baz' }"));
        assertEqual([ { collection: 'foo', key: 'bar' } ], actual);

        actual = getQueryResults(buildQuery(i, "{ ignore: true, _id: '_system/VALUE', value: 'baz' }"));
        assertEqual([ { collection: '_system', key: 'VALUE' } ], actual);

        actual = getQueryResults(buildQuery(i ,"{ value: 123, _id: 'Some-Odd-Collection/THIS_IS_THE_KEY' }"));
        assertEqual([ { collection: 'Some-Odd-Collection', key: 'THIS_IS_THE_KEY' } ], actual);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test parse identifier function
////////////////////////////////////////////////////////////////////////////////

    testParseIdentifierCollection : function () {
      var cn = "UnitTestsAhuacatlFunctions";

      internal.db._drop(cn);
      var cx = internal.db._create(cn);
      cx.save({ "title" : "123", "value" : 456, "_key" : "foobar" });
      cx.save({ "_key" : "so-this-is-it", "title" : "nada", "value" : 123 });

      var expected, actual;
      var buildQuery = function (nr, input) {
        switch (nr) {
          case 0:
            return `RETURN PARSE_IDENTIFIER(${input})`;
          case 1:
            return `RETURN NOOPT(PARSE_IDENTIFIER(${input}))`;
          default:
            assertTrue(false, "Undefined state");
        }
      };

      for (var i = 0; i < 2; ++i) {
        expected = [ { collection: cn, key: "foobar" } ];
        actual = getQueryResults(buildQuery(i, "DOCUMENT(CONCAT(@cn, '/', @key))"), { cn: cn, key: "foobar" });
        assertEqual(expected, actual);

        expected = [ { collection: cn, key: "foobar" } ];
        actual = getQueryResults(buildQuery(i, "DOCUMENT(CONCAT(@cn, '/', 'foobar'))"), { cn: cn });
        assertEqual(expected, actual);

        expected = [ { collection: cn, key: "foobar" } ];
        actual = getQueryResults(buildQuery(i, "DOCUMENT([ @key ])[0]"), { key: "UnitTestsAhuacatlFunctions/foobar" });
        assertEqual(expected, actual);

        expected = [ { collection: cn, key: "so-this-is-it" } ];
        actual = getQueryResults(buildQuery(i, "DOCUMENT([ 'UnitTestsAhuacatlFunctions/so-this-is-it' ])[0]"));
        assertEqual(expected, actual);
      }

      internal.db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test parse identifier function
////////////////////////////////////////////////////////////////////////////////

    testParseIdentifierInvalid : function () {
      var buildQuery = function (nr, input) {
        switch (nr) {
          case 0:
            return `RETURN PARSE_IDENTIFIER(${input})`;
          case 1:
            return `RETURN NOOPT(PARSE_IDENTIFIER(${input}))`;
          default:
            assertTrue(false, "Undefined state");
        }
      };

      for (var i = 0; i < 2; ++i) {
        assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, buildQuery(i, ""));
        assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, buildQuery(i, "'foo', 'bar'"));
        assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, buildQuery(i, "null"));
        assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, buildQuery(i, "false"));
        assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, buildQuery(i, "3"));
        assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, buildQuery(i, "\"foo\""));
        assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, buildQuery(i, "'foo bar'"));
        assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, buildQuery(i, "'foo/bar/baz'"));
        assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, buildQuery(i, "[ ]"));
        assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, buildQuery(i, "{ }"));
        assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, buildQuery(i, "{ foo: 'bar' }"));
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test is_same_collection function
////////////////////////////////////////////////////////////////////////////////

    testIsSameCollection : function () {
      var buildQuery = function (nr, collection, doc) {
        var q = "IS_SAME_COLLECTION(" + JSON.stringify(collection) + ", " + JSON.stringify(doc) + ")";
        switch (nr) {
          case 0:
            return "RETURN " + q;
         case 1:
            return "RETURN NOOPT(" + q + ")";
          default:
            assertTrue(false, "Undefined state");
        }
      };

     for (var i = 0; i < 2; ++i) {
        assertEqual([ true ], getQueryResults(buildQuery(i, "foo", "foo/bar")));
        assertEqual([ true ], getQueryResults(buildQuery(i, "foo", "foo/bark")));
        assertEqual([ false ], getQueryResults(buildQuery(i, "FOO", "foo/bark")));
        assertEqual([ false ], getQueryResults(buildQuery(i, "foo", "food/barz")));
        assertEqual([ false ], getQueryResults(buildQuery(i, "foo", "fooe/barz")));
        assertEqual([ false ], getQueryResults(buildQuery(i, "foo", " foo/barz")));

        assertEqual([ true ], getQueryResults(buildQuery(i, "foo", { _id: "foo/bark" })));
        assertEqual([ true ], getQueryResults(buildQuery(i, "foobar", { _id: "foobar/bark" })));
        assertEqual([ false ], getQueryResults(buildQuery(i, "FOOBAR", { _id: "foobar/bark" })));
        assertEqual([ false ], getQueryResults(buildQuery(i, "foobar2", { _id: "foobar/bark" })));
        assertEqual([ false ], getQueryResults(buildQuery(i, "foo", { _id: "food/bark" })));
        assertEqual([ false ], getQueryResults(buildQuery(i, "foo", { _id: "f/bark" })));
        assertEqual([ false ], getQueryResults(buildQuery(i, "foo", { _id: "f/bark" })));
        assertEqual([ false ], getQueryResults(buildQuery(i, "foo", { _id: "foobar/bark" })));
        assertEqual([ null ], getQueryResults(buildQuery(i, "foo", { id: "foobar/bark" })));
        assertEqual([ null ], getQueryResults(buildQuery(i, "foo", { _key: "foo/bark" })));
        assertEqual([ null ], getQueryResults(buildQuery(i, "foo", { _key: "foobar/bark" })));

        assertEqual([ null ], getQueryResults(buildQuery(i, "foo", null)));
        assertEqual([ null ], getQueryResults(buildQuery(i, "foo", true)));
        assertEqual([ null ], getQueryResults(buildQuery(i, "foo", false)));
        assertEqual([ null ], getQueryResults(buildQuery(i, "foo", 3.5)));
        assertEqual([ null ], getQueryResults(buildQuery(i, "foo", [ ])));
        assertEqual([ null ], getQueryResults(buildQuery(i, "foo", [ "foo/bar" ])));
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test is_same_collection function
////////////////////////////////////////////////////////////////////////////////

    testIsSameCollectionCollection : function () {
      var cn = "UnitTestsAhuacatlFunctions";

      internal.db._drop(cn);
      var cx = internal.db._create(cn);
      cx.save({ "title" : "123", "value" : 456, "_key" : "foobar" });
      cx.save({ "_key" : "so-this-is-it", "title" : "nada", "value" : 123 });

      var actual;
      var buildQuery = function (nr, input) {
        switch (nr) {
          case 0:
            return `RETURN IS_SAME_COLLECTION(${cn}, ${input})`;
          case 1:
            return `RETURN NOOPT(IS_SAME_COLLECTION(${cn}, ${input}))`;
          default:
            assertTrue(false, "Undefined state");
        }
      };

      for (var i = 0; i < 2; ++i) {
        actual = getQueryResults(buildQuery(i, "DOCUMENT(CONCAT(@cn, '/', @key))"), { cn: cn, key: "foobar" });
        assertTrue(actual[0]);

        actual = getQueryResults(buildQuery(i, "DOCUMENT(CONCAT(@cn, '/', 'foobar'))"), { cn: cn });
        assertTrue(actual[0]);

        actual = getQueryResults(buildQuery(i, "DOCUMENT([ @key ])[0]"), { key: "UnitTestsAhuacatlFunctions/foobar" });
        assertTrue(actual[0]);

        actual = getQueryResults(buildQuery(i, "DOCUMENT([ 'UnitTestsAhuacatlFunctions/so-this-is-it' ])[0]"));
        assertTrue(actual[0]);
      }

      internal.db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test is_same_collection function
////////////////////////////////////////////////////////////////////////////////

    testIsSameCollectionInvalid : function () {
     assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN IS_SAME_COLLECTION()");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN IS_SAME_COLLECTION('foo')");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN IS_SAME_COLLECTION('foo', 'bar', 'baz')");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN IS_SAME_COLLECTION('foo', null)");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN IS_SAME_COLLECTION('foo', false)");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN IS_SAME_COLLECTION('foo', true)");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN IS_SAME_COLLECTION('foo', 3)");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN IS_SAME_COLLECTION('foo', [ ])");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN IS_SAME_COLLECTION('foo', { })");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test check_document function
////////////////////////////////////////////////////////////////////////////////

    testCheckDocument : function () {
      var cn = "UnitTestsAhuacatlFunctions";

      internal.db._drop(cn);
      let c = internal.db._create(cn);

      c.insert({ _key: "test1", a: 1, b: 2, c: 3 });
      c.insert({ _key: "test2", a: 1, b: 2, c: 3, sub: { a: 1, b: 2, c: 3 }});
      c.insert({ _key: "test3", a: 1, b: 2, c: 3, sub: [{ a: 1 }, { b: 2 }, { c: 3 }, { a: 1 }]});
      
      assertEqual([ true, true, true ], getQueryResults("FOR doc IN " + cn + " RETURN CHECK_DOCUMENT(doc)"));

      assertEqual([ false ], getQueryResults("RETURN CHECK_DOCUMENT(null)"));
      assertEqual([ false ], getQueryResults("RETURN CHECK_DOCUMENT(true)"));
      assertEqual([ false ], getQueryResults("RETURN CHECK_DOCUMENT(false)"));
      assertEqual([ false ], getQueryResults("RETURN CHECK_DOCUMENT(-1)"));
      assertEqual([ false ], getQueryResults("RETURN CHECK_DOCUMENT(0)"));
      assertEqual([ false ], getQueryResults("RETURN CHECK_DOCUMENT(1)"));
      assertEqual([ false ], getQueryResults("RETURN CHECK_DOCUMENT('foo')"));
      assertEqual([ false ], getQueryResults("RETURN CHECK_DOCUMENT([])"));
      assertEqual([ false ], getQueryResults("RETURN CHECK_DOCUMENT([1, 2, 3])"));
      assertEqual([ true ], getQueryResults("RETURN CHECK_DOCUMENT({})"));
      assertEqual([ true ], getQueryResults("RETURN CHECK_DOCUMENT({a: 1})"));
      assertEqual([ true ], getQueryResults("RETURN CHECK_DOCUMENT({a: 1, b: 2})"));
      assertEqual([ true ], getQueryResults("RETURN CHECK_DOCUMENT({a: 1, sub: [1, 2, 3]})"));
      assertEqual([ true ], getQueryResults("RETURN CHECK_DOCUMENT({a: 1, sub: {a: 1, b: 2 }})"));

      internal.db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test document function
////////////////////////////////////////////////////////////////////////////////

    testDocument1 : function () {
      var cn = "UnitTestsAhuacatlFunctions";

      internal.db._drop(cn);
      var cx = internal.db._create(cn);
      var d1 = cx.save({ "title" : "123", "value" : 456 });
      var d2 = cx.save({ "title" : "nada", "value" : 123 });

      var expected, actual;

      var buildQuery = function (nr, input) {
        switch (nr) {
          case 0:
            return `RETURN DOCUMENT(${input})`;
          case 1:
            return `RETURN NOOPT(DOCUMENT(${input}))`;
          default:
            assertTrue(false, "Undefined state");
        }
      };

      for (var i = 0; i < 2; ++i) {
        // test with two parameters
        expected = [ { title: "123", value : 456 } ];
        actual = getQueryResults(buildQuery(i, cn + ", \"" + d1._id + "\""));
        assertEqual(expected, actual);

        actual = getQueryResults(buildQuery(i, cn + ", \"" + d1._key + "\""));
        assertEqual(expected, actual);

        expected = [ { title: "nada", value : 123 } ];
        actual = getQueryResults(buildQuery(i, cn + ", \"" + d2._id + "\""));
        assertEqual(expected, actual);

        actual = getQueryResults(buildQuery(i, cn + ", \"" + d2._key + "\""));
        assertEqual(expected, actual);

        // test with one parameter
        expected = [ { title: "nada", value : 123 } ];
        actual = getQueryResults(buildQuery(i, "\"" + d2._id + "\""));
        assertEqual(expected, actual);

        // test with function result parameter
        actual = getQueryResults(buildQuery(i, "CONCAT(\"foo\", \"bar\")"));
        assertEqual([ null ], actual);
        actual = getQueryResults(buildQuery(i, "CONCAT(\"" + cn + "\", \"bart\")"));
        assertEqual([ null ], actual);

        cx.save({ _key: "foo", value: "bar" });
        expected = [ { value: "bar" } ];
        actual = getQueryResults(buildQuery(i, "CONCAT(\"" + cn + "\", \"foo\")"));
        assertEqual([ null ], actual);
        actual = getQueryResults(buildQuery(i, "CONCAT(@c, \"/\", @k)"), { c: cn, k: "foo" });
        assertEqual(expected, actual);
        actual = getQueryResults(buildQuery(i, "CONCAT(\"" + cn + "\", \"/\", @k)"), { k: "foo" });
        assertEqual(expected, actual);

        // test with bind parameter
        expected = [ { title: "nada", value : 123 } ];
        actual = getQueryResults(buildQuery(i, "@id"), { id: d2._id });
        assertEqual(expected, actual);

        // test dynamic parameter
        expected = [ { title: "nada", value : 123 }, { title: "123", value: 456 }, { value: "bar" } ];
        actual = getQueryResults("FOR d IN @@cn SORT d.value " + buildQuery(i, "d._id"), { "@cn" : cn });
        assertEqual(expected, actual);
        cx.remove("foo");
      }

      internal.db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test document function
////////////////////////////////////////////////////////////////////////////////

    testDocument2 : function () {
      var cn = "UnitTestsAhuacatlFunctions";

      internal.db._drop(cn);
      var cx = internal.db._create(cn);
      var d1 = cx.save({ "title" : "123", "value" : 456, "zxy" : 1 });
      var d2 = cx.save({ "title" : "nada", "value" : 123, "zzzz" : false });

      var expected, actual;

      // test with two parameters
      expected = [ { title: "123", value : 456, zxy: 1 } ];
      actual = getQueryResults("RETURN DOCUMENT(@@cn, @id)", { "@cn" : cn, "id" : d1._id });
      assertEqual(expected, actual);

      actual = getQueryResults("RETURN DOCUMENT(@@cn, @id)", { "@cn" : cn, "id" : d1._key });
      assertEqual(expected, actual);

      expected = [ { title: "nada", value : 123, zzzz : false } ];
      actual = getQueryResults("RETURN DOCUMENT(@@cn, @id)", { "@cn" : cn, "id" : d2._id });
      assertEqual(expected, actual);

      actual = getQueryResults("RETURN DOCUMENT(@@cn, @id)", { "@cn" : cn, "id" : d2._key });
      assertEqual(expected, actual);

      // test with one parameter
      expected = [ { title: "123", value : 456, zxy: 1 } ];
      actual = getQueryResults("RETURN DOCUMENT(@id)", { "id" : d1._id });
      assertEqual(expected, actual);

      expected = [ { title: "nada", value : 123, zzzz : false } ];
      actual = getQueryResults("RETURN DOCUMENT(@id)", { "id" : d2._id });
      assertEqual(expected, actual);

      internal.db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test document function
////////////////////////////////////////////////////////////////////////////////

    testDocumentMulti1 : function () {
      var cn = "UnitTestsAhuacatlFunctions";

      internal.db._drop(cn);
      var cx = internal.db._create(cn);
      var d1 = cx.save({ "title" : "123", "value" : 456, "zxy" : 1 });
      var d2 = cx.save({ "title" : "nada", "value" : 123, "zzzz" : false });
      var d3 = cx.save({ "title" : "boom", "value" : 3321, "zzzz" : null });

      var expected, actual;

      // test with two parameters
      expected = [ [
        { title: "123", value : 456, zxy : 1 },
        { title: "nada", value : 123, zzzz : false },
        { title: "boom", value : 3321, zzzz : null }
      ] ];

      actual = getQueryResults("RETURN DOCUMENT(@@cn, @id)", { "@cn" : cn, "id" : [ d1._id, d2._id, d3._id ] }, true);
      assertEqual(expected, actual);

      expected = [ [ { title: "nada", value : 123, zzzz : false } ] ];
      actual = getQueryResults("RETURN DOCUMENT(@@cn, @id)", { "@cn" : cn, "id" : [ d2._id ] }, true);
      assertEqual(expected, actual);

      // test with one parameter
      expected = [ [ { title: "nada", value : 123, zzzz : false } ] ];
      actual = getQueryResults("RETURN DOCUMENT(@id)", { "id" : [ d2._id ] }, true);
      assertEqual(expected, actual);


      cx.remove(d3);

      expected = [ [ { title: "nada", value : 123, zzzz : false } ] ];
      actual = getQueryResults("RETURN DOCUMENT(@@cn, @id)", { "@cn" : cn, "id" : [ d2._id, d3._id, "abc/def" ] }, true);
      assertEqual(expected, actual);

      expected = [ [ { title: "nada", value : 123, zzzz : false } ] ];
      actual = getQueryResults("RETURN DOCUMENT(@id)", { "id" : [ d2._id, d3._id, "abc/def" ] }, true);
      assertEqual(expected, actual);

      internal.db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test document function
////////////////////////////////////////////////////////////////////////////////

    testDocumentInvalid : function () {
      var cn = "UnitTestsAhuacatlFunctions";

      internal.db._drop(cn);
      internal.db._create(cn);

      var expected, actual;

      // test with non-existing document
      expected = [ null ];
      actual = getQueryResults("RETURN DOCUMENT(" + cn + ", \"" + cn + "/99999999999\")");
      assertEqual(expected, actual);

      actual = getQueryResults("RETURN DOCUMENT(" + cn + ", \"thefoxdoesnotexist/99999999999\")");
      assertEqual(expected, actual);

      actual = getQueryResults("RETURN DOCUMENT({}, 'foo')");
      assertEqual(expected, actual);

      actual = getQueryResults("RETURN DOCUMENT({}, {})");
      assertEqual(expected, actual);

      actual = getQueryResults("RETURN DOCUMENT(true, 'foo')");
      assertEqual(expected, actual);

      actual = getQueryResults("RETURN DOCUMENT('foo', {})");
      assertEqual(expected, actual);

      internal.db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test document indexed access function
////////////////////////////////////////////////////////////////////////////////

    testDocumentIndexedAccess : function () {
      var cn = "UnitTestsAhuacatlFunctions";

      internal.db._drop(cn);
      var cx = internal.db._create(cn);
      var d1 = cx.save({ "title" : "123", "value" : 456 });

      var expected, actual;

      expected = [ "123" ];
      actual = getQueryResults("RETURN DOCUMENT(" + cn + ", [ \"" + d1._id + "\" ])[0].title");
      assertEqual(expected, actual);

      internal.db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test current user function
////////////////////////////////////////////////////////////////////////////////

    testCurrentUser : function () {
      var expected = null;
      if (internal.getCurrentRequest) {
        var req = internal.getCurrentRequest();

        if (typeof req === 'object') {
          expected = req.user;
        }
      }

       // there is no current user in the non-request context
       assertEqual([ expected ], getQueryResults("RETURN NOOPT(CURRENT_USER())"));
       assertEqual([ expected ], getQueryResults("RETURN NOOPT(V8(CURRENT_USER()))"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test current database function
////////////////////////////////////////////////////////////////////////////////

    testCurrentDatabase : function () {
      var actual = getQueryResults("RETURN CURRENT_DATABASE()");
      assertEqual([ "_system" ], actual);

      actual = getQueryResults("RETURN NOOPT(CURRENT_DATABASE())");
      assertEqual([ "_system" ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test sleep function
////////////////////////////////////////////////////////////////////////////////

    testSleep : function () {
      var start = require("internal").time();
      var actual = getQueryResults("LET a = SLEEP(2) RETURN 1");

      var diff = Math.round(require("internal").time() - start, 1);

      assertEqual([ 1 ], actual);

      // allow some tolerance for the time diff (because of busy servers and Valgrind)
      assertTrue(diff >= 1.8 && diff <= 20, "SLEEP(2) did not take between 1.8 and 20 seconds");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test ASSERT function
////////////////////////////////////////////////////////////////////////////////

    testAssert : function () {
      var cn = "UnitTestsAhuacatlFunctions";

      internal.db._drop(cn);
      var c = internal.db._create(cn);
      c.insert({"foo": 0});
      c.insert({"foo": 1});
      c.insert({"foo": 2});

      var result = db._query("FOR doc in @@cn FILTER ASSERT(doc.foo < 4,'USER MESSAGE: doc.foo not less than 4') RETURN doc", { "@cn" : cn });
      assertEqual(result.toArray().length,3);

      try {
        db._query("FOR doc in @@cn FILTER ASSERT(doc.foo < 2,'USER MESSAGE: doc.foo not less than 2') RETURN doc", { "@cn" : cn });
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_QUERY_USER_ASSERT.code, err.errorNum);
      } finally {
        internal.db._drop(cn);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test WARN function
////////////////////////////////////////////////////////////////////////////////

    testWarn : function () {
      var cn = "UnitTestsAhuacatlFunctions";

      internal.db._drop(cn);
      var c = internal.db._create(cn);
      c.insert({"foo": 0});
      c.insert({"foo": 1});
      c.insert({"foo": 2});


      var result = db._query("FOR doc in @@cn FILTER WARN(doc.foo < 4,'USER MESSAGE: doc.foo not less than 4') RETURN doc", { "@cn" : cn });
      assertEqual(result.toArray().length,3);

      result = db._query("FOR doc in @@cn FILTER WARN(doc.foo < 2,'USER MESSAGE: doc.foo not less than 2') RETURN doc", { "@cn" : cn });
      assertEqual(result.toArray().length,2);

      try {
        db._query("FOR doc in @@cn FILTER WARN(doc.foo < 2,'USER MESSAGE: doc.foo not less than 2') RETURN doc", { "@cn" : cn }, {"failOnWarning": true});
      } catch (err) {
        assertEqual(errors.ERROR_QUERY_USER_WARN.code, err.errorNum);
      } finally {
        internal.db._drop(cn);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test DECODE_REV
////////////////////////////////////////////////////////////////////////////////

    testDecodeRevInvalid : function () {
      [ 
        null, 
        false, 
        true, 
        -1000, 
        -1, 
        -0.1, 
        0, 
        0.1, 
        10, 
        10000, 
        9945854.354, 
        "", 
        " ", 
        [],
        {} 
      ].forEach(function(value) {
        assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DECODE_REV(@value)", { value });
      });
    },
    
    testDecodeRev : function () {
      [
        [ "_YSlmbS6---", "2019-03-04T18:12:45.007Z", 0 ], 
        [ "_YUwHwWy---", "2019-03-11T11:36:03.213Z", 0 ], 
        [ "_YaE0fyK---", "2019-03-28T00:32:38.723Z", 0 ], 
        [ "_YSmDflm---", "2019-03-04T18:44:29.946Z", 0 ], 
        [ "_YaE0Way---", "2019-03-28T00:32:29.133Z", 0 ], 
        [ "_YWeadNG---", "2019-03-16T20:06:02.226Z", 0 ], 
        [ "_YVORUcq---", "2019-03-12T22:43:39.115Z", 0 ], 
        [ "_YSl0nrO---", "2019-03-04T18:28:15.188Z", 0 ], 
        [ "_YY7gl3K--A", "2019-03-24T11:07:50.035Z", 2 ], 
        [ "_YaE4H3q---", "2019-03-28T00:36:36.379Z", 0 ], 
        [ "_YY7N6p2--A", "2019-03-24T10:47:26.142Z", 2 ], 
        [ "_YSlq-Ny---", "2019-03-04T18:16:37.373Z", 0 ], 
        [ "_YYykPIe---", "2019-03-24T00:42:40.168Z", 0 ], 
        [ "_YWf_RSG--B", "2019-03-16T20:46:14.850Z", 3 ], 
        [ "_YSl93J----", "2019-03-04T18:38:20.848Z", 0 ], 
        [ "_YY0Gml6---", "2019-03-24T02:30:06.719Z", 0 ], 
        [ "_YSmdMmW---", "2019-03-04T19:12:34.438Z", 0 ], 
        [ "_YY00EdG---", "2019-03-24T03:19:46.418Z", 0 ], 
        [ "_YYy1OBu---", "2019-03-24T01:01:13.148Z", 0 ], 
        [ "_YY1Bzgu---", "2019-03-24T03:34:46.572Z", 0 ], 
        [ "_YYyPBYK---", "2019-03-24T00:19:29.827Z", 0 ], 
        [ "_YaFcLFe--_", "2019-03-28T01:15:58.968Z", 1 ], 
        [ "_YWexcTu---", "2019-03-16T20:31:08.636Z", 0 ], 
        [ "_YU0HOEG---", "2019-03-11T16:15:05.314Z", 0 ], 
        [ "_YaE1DTC--A", "2019-03-28T00:33:15.089Z", 2 ], 
        [ "_YSmRdx6---", "2019-03-04T18:59:45.599Z", 0 ], 
        [ "_YaE2Q4S---", "2019-03-28T00:34:34.533Z", 0 ], 
        [ "_YSmTlb2---", "2019-03-04T19:02:04.510Z", 0 ], 
        [ "_YYzAiEe---", "2019-03-24T01:13:34.568Z", 0 ], 
        [ "_YVN12He---", "2019-03-12T22:13:38.584Z", 0 ], 
        [ "_YYyVku----", "2019-03-24T00:26:39.232Z", 0 ], 
        [ "_YaFQc26---", "2019-03-28T01:03:10.735Z", 0 ], 
        [ "_YYytrHa---", "2019-03-24T00:52:58.647Z", 0 ], 
        [ "_YaFVoZC---", "2019-03-28T01:08:50.225Z", 0 ], 
        [ "_YWezNX6--A", "2019-03-16T20:33:04.415Z", 2 ], 
        [ "_YaE3rfi---", "2019-03-28T00:36:07.321Z", 0 ], 
        [ "_YUw_E-e---", "2019-03-11T11:26:33.480Z", 0 ], 
        [ "_YYzEieO---", "2019-03-24T01:17:57.124Z", 0 ], 
        [ "_YVOfDlu---", "2019-03-12T22:58:39.356Z", 0 ], 
        [ "_YYyRtVy---", "2019-03-24T00:22:25.917Z", 0 ], 
        [ "_YaFvsf2---", "2019-03-28T01:37:18.366Z", 0 ], 
        [ "_YWfiar----", "2019-03-16T21:24:38.224Z", 0 ], 
        [ "_YSmHzya---", "2019-03-04T18:49:12.775Z", 0 ], 
        [ "_YY0uxp2---", "2019-03-24T03:13:59.486Z", 0 ], 
        [ "_YZZIMQy---", "2019-03-25T21:38:20.077Z", 0 ], 
        [ "_YaE353K---", "2019-03-28T00:36:22.035Z", 0 ], 
      ].forEach(function(parts){
        let value = parts[0];
        let result = AQL_EXECUTE("RETURN DECODE_REV(@value)", { value }).json;
        assertEqual({ date: parts[1], count: parts[2] }, result[0], parts);
      });
    },
};} // ahuacatlMiscFunctionsTestSuite

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ahuacatlCollectionCountTestSuite () {
  var c;
  var cn = "UnitTestsCollectionCount";

  return {

    setUp : function () {
      db._drop(cn);
      c = db._create(cn, { numberOfShards: 4 });
      let docs = [];

      for (var i = 1; i <= 1000; ++i) {
        docs.push({ _key: "test" + i });
      }
      c.insert(docs);
    },

    tearDown : function () {
      db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test LENGTH(collection) - non existing
////////////////////////////////////////////////////////////////////////////////

    testLengthNonExisting : function () {
      var cnot = cn + "DoesNotExist";

      try {
        AQL_EXECUTE("RETURN LENGTH(" + cnot + ")");
        fail();
      }
      catch (err) {
        assertEqual(errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test LENGTH(collection)
////////////////////////////////////////////////////////////////////////////////

    testLength : function () {
      var actual = AQL_EXECUTE("RETURN LENGTH(" + cn + ")");
      assertEqual([ ], actual.warnings);
      assertEqual([ 1000 ], actual.json);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test LENGTH(collection)
////////////////////////////////////////////////////////////////////////////////

    testLengthUseInLoop : function () {
      var actual = AQL_EXECUTE("FOR i IN 1..LENGTH(" + cn + ") REMOVE CONCAT('test', i) IN " + cn);
      assertEqual([ ], actual.warnings);
      assertEqual([ ], actual.json);
      assertEqual(0, c.count());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test LENGTH(collection)
////////////////////////////////////////////////////////////////////////////////

    testLengthUseInModification : function () {
      try {
        AQL_EXECUTE("FOR doc IN " + cn + " REMOVE doc IN " + cn + " RETURN LENGTH(" + cn + ")");
        fail();
      }
      catch (err) {
        assertEqual(errors.ERROR_QUERY_ACCESS_AFTER_MODIFICATION.code, err.errorNum);
      }
      assertEqual(1000, c.count());
   },

////////////////////////////////////////////////////////////////////////////////
/// @brief test COLLECTIONS()
////////////////////////////////////////////////////////////////////////////////

    testCollections : function () {
      assertEqual(db._collections().map((col) => {return {name:col.name(),_id:col._id};}), getQueryResults('RETURN NOOPT(COLLECTIONS())')[0]);
    }
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ahuacatlMiscFunctionsTestSuite);
jsunity.run(ahuacatlCollectionCountTestSuite);

return jsunity.done();
