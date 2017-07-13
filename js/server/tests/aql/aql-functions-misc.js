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

function ahuacatlMiscFunctionsTestSuite () {
  return {

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
          case 2:
          return `RETURN NOOPT(V8(PARSE_IDENTIFIER(${input})))`;
          default:
            assertTrue(false, "Undefined state");
        }
      };

      for (var i = 0; i < 3; ++i) {
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
          case 2:
          return `RETURN NOOPT(V8(PARSE_IDENTIFIER(${input})))`;
          default:
            assertTrue(false, "Undefined state");
        }
      };

      for (var i = 0; i < 3; ++i) {
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
          case 2:
          return `RETURN NOOPT(V8(PARSE_IDENTIFIER(${input})))`;
          default:
            assertTrue(false, "Undefined state");
        }
      };

      for (var i = 0; i < 3; ++i) {
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
          case 2:
            return "RETURN NOOPT(V8(" + q + "))";
          default:
            assertTrue(false, "Undefined state");
        }
      };

     for (var i = 0; i < 3; ++i) {
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
          case 2:
          return `RETURN NOOPT(V8(IS_SAME_COLLECTION(${cn}, ${input})))`;
          default:
            assertTrue(false, "Undefined state");
        }
      };

      for (var i = 0; i < 3; ++i) {
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
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN V8(IS_SAME_COLLECTION())");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN IS_SAME_COLLECTION('foo')");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN V8(IS_SAME_COLLECTION('foo'))");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN IS_SAME_COLLECTION('foo', 'bar', 'baz')");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN V8(IS_SAME_COLLECTION('foo', 'bar', 'baz'))");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN IS_SAME_COLLECTION('foo', null)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN IS_SAME_COLLECTION('foo', false)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN IS_SAME_COLLECTION('foo', true)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN IS_SAME_COLLECTION('foo', 3)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN IS_SAME_COLLECTION('foo', [ ])"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN IS_SAME_COLLECTION('foo', { })"); 

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN V8(IS_SAME_COLLECTION('foo', null))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN V8(IS_SAME_COLLECTION('foo', false))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN V8(IS_SAME_COLLECTION('foo', true))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN V8(IS_SAME_COLLECTION('foo', 3))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN V8(IS_SAME_COLLECTION('foo', [ ]))"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN V8(IS_SAME_COLLECTION('foo', { }))"); 
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
          case 2:
            return `RETURN NOOPT(V8(DOCUMENT(${input})))`;
          default:
            assertTrue(false, "Undefined state");
        }
      };

      for (var i = 0; i < 3; ++i) {
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

      var actual = getQueryResults("RETURN CURRENT_USER()");
      // there is no current user in the non-request context
      assertEqual([ expected ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test current database function
////////////////////////////////////////////////////////////////////////////////

    testCurrentDatabase : function () {
      var actual = getQueryResults("RETURN CURRENT_DATABASE()");
      assertEqual([ "_system" ], actual);

      actual = getQueryResults("RETURN NOOPT(CURRENT_DATABASE())");
      assertEqual([ "_system" ], actual);

      actual = getQueryResults("RETURN NOOPT(V8(CURRENT_DATABASE()))");
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
    }
  
  };
}

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

      for (var i = 1; i <= 1000; ++i) {
        c.insert({ _key: "test" + i });
      }
    },

    tearDown : function () {
      db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test LENGTH(collection) - non existing
////////////////////////////////////////////////////////////////////////////////

    testLengthNonExisting : function () {
      var cnot = cn + "DoesNotExist";

      var queries = [
        "RETURN LENGTH(" + cnot + ")",
        "RETURN V8(LENGTH(" + cnot + "))"
      ];

      queries.forEach(function(q) {
        try {
          AQL_EXECUTE(q);
          fail();
        }
        catch (err) {
          assertEqual(errors.ERROR_ARANGO_COLLECTION_NOT_FOUND.code, err.errorNum);
        }
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test LENGTH(collection)
////////////////////////////////////////////////////////////////////////////////

    testLength : function () {
      var queries = [
        "RETURN LENGTH(" + cn + ")",
        "RETURN V8(LENGTH(" + cn + "))"
      ];

      queries.forEach(function(q) {
        var actual = AQL_EXECUTE(q);
        assertEqual([ ], actual.warnings);
        assertEqual([ 1000 ], actual.json);
      });
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
    }
  
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ahuacatlMiscFunctionsTestSuite);
jsunity.run(ahuacatlCollectionCountTestSuite);

return jsunity.done();

