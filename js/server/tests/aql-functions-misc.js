/*jshint strict: false, maxlen: 500 */
/*global require, assertEqual, assertTrue */
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
var helper = require("org/arangodb/aql-helper");
var getQueryResults = helper.getQueryResults;
var getQueryResults = helper.getQueryResults;
var assertQueryError = helper.assertQueryError;
var assertQueryWarningAndNull = helper.assertQueryWarningAndNull;

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

      actual = getQueryResults("RETURN PARSE_IDENTIFIER('foo/bar')");
      assertEqual([ { collection: 'foo', key: 'bar' } ], actual);
      
      actual = getQueryResults("RETURN PARSE_IDENTIFIER('this-is-a-collection-name/and-this-is-an-id')");
      assertEqual([ { collection: 'this-is-a-collection-name', key: 'and-this-is-an-id' } ], actual);
      
      actual = getQueryResults("RETURN PARSE_IDENTIFIER('MY_COLLECTION/MY_DOC')");
      assertEqual([ { collection: 'MY_COLLECTION', key: 'MY_DOC' } ], actual);

      actual = getQueryResults("RETURN PARSE_IDENTIFIER('_users/AbC')");
      assertEqual([ { collection: '_users', key: 'AbC' } ], actual);
      
      actual = getQueryResults("RETURN PARSE_IDENTIFIER({ _id: 'foo/bar', value: 'baz' })");
      assertEqual([ { collection: 'foo', key: 'bar' } ], actual);

      actual = getQueryResults("RETURN PARSE_IDENTIFIER({ ignore: true, _id: '_system/VALUE', value: 'baz' })");
      assertEqual([ { collection: '_system', key: 'VALUE' } ], actual);

      actual = getQueryResults("RETURN PARSE_IDENTIFIER({ value: 123, _id: 'Some-Odd-Collection/THIS_IS_THE_KEY' })");
      assertEqual([ { collection: 'Some-Odd-Collection', key: 'THIS_IS_THE_KEY' } ], actual);
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
      
      expected = [ { collection: cn, key: "foobar" } ];
      actual = getQueryResults("RETURN PARSE_IDENTIFIER(DOCUMENT(CONCAT(@cn, '/', @key)))", { cn: cn, key: "foobar" });
      assertEqual(expected, actual);

      expected = [ { collection: cn, key: "foobar" } ];
      actual = getQueryResults("RETURN PARSE_IDENTIFIER(DOCUMENT(CONCAT(@cn, '/', @key)))", { cn: cn, key: "foobar" });
      assertEqual(expected, actual);

      expected = [ { collection: cn, key: "foobar" } ];
      actual = getQueryResults("RETURN PARSE_IDENTIFIER(DOCUMENT(CONCAT(@cn, '/', 'foobar')))", { cn: cn });
      assertEqual(expected, actual);

      expected = [ { collection: cn, key: "foobar" } ];
      actual = getQueryResults("RETURN PARSE_IDENTIFIER(DOCUMENT([ @key ])[0])", { key: "UnitTestsAhuacatlFunctions/foobar" });
      assertEqual(expected, actual);

      expected = [ { collection: cn, key: "so-this-is-it" } ];
      actual = getQueryResults("RETURN PARSE_IDENTIFIER(DOCUMENT([ 'UnitTestsAhuacatlFunctions/so-this-is-it' ])[0])");
      assertEqual(expected, actual);

      internal.db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test parse identifier function
////////////////////////////////////////////////////////////////////////////////

    testParseIdentifierInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN PARSE_IDENTIFIER()"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN PARSE_IDENTIFIER('foo', 'bar')");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN PARSE_IDENTIFIER(null)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN PARSE_IDENTIFIER(false)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN PARSE_IDENTIFIER(3)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN PARSE_IDENTIFIER(\"foo\")"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN PARSE_IDENTIFIER('foo bar')"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN PARSE_IDENTIFIER('foo/bar/baz')"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN PARSE_IDENTIFIER([ ])"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN PARSE_IDENTIFIER({ })"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN PARSE_IDENTIFIER({ foo: 'bar' })"); 
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

      // test with two parameters
      expected = [ { title: "123", value : 456 } ];
      actual = getQueryResults("RETURN DOCUMENT(" + cn + ", \"" + d1._id + "\")");
      assertEqual(expected, actual);
      
      actual = getQueryResults("RETURN DOCUMENT(" + cn + ", \"" + d1._key + "\")");
      assertEqual(expected, actual);
      
      expected = [ { title: "nada", value : 123 } ];
      actual = getQueryResults("RETURN DOCUMENT(" + cn + ", \"" + d2._id + "\")");
      assertEqual(expected, actual);
      
      actual = getQueryResults("RETURN DOCUMENT(" + cn + ", \"" + d2._key + "\")");
      assertEqual(expected, actual);
      
      // test with one parameter
      expected = [ { title: "nada", value : 123 } ];
      actual = getQueryResults("RETURN DOCUMENT(\"" + d2._id + "\")");
      assertEqual(expected, actual);
      
      // test with function result parameter
      actual = getQueryResults("RETURN DOCUMENT(CONCAT(\"foo\", \"bar\"))");
      assertEqual([ null ], actual);
      actual = getQueryResults("RETURN DOCUMENT(CONCAT(\"" + cn + "\", \"bart\"))");
      assertEqual([ null ], actual);

      cx.save({ _key: "foo", value: "bar" });
      expected = [ { value: "bar" } ];
      actual = getQueryResults("RETURN DOCUMENT(CONCAT(\"" + cn + "\", \"foo\"))");
      assertEqual([ null ], actual);
      actual = getQueryResults("RETURN DOCUMENT(CONCAT(@c, \"/\", @k))", { c: cn, k: "foo" });
      assertEqual(expected, actual);
      actual = getQueryResults("RETURN DOCUMENT(CONCAT(\"" + cn + "\", \"/\", @k))", { k: "foo" });
      assertEqual(expected, actual);
      
      // test with bind parameter
      expected = [ { title: "nada", value : 123 } ];
      actual = getQueryResults("RETURN DOCUMENT(@id)", { id: d2._id });
      assertEqual(expected, actual);
      
      // test dynamic parameter
      expected = [ { title: "nada", value : 123 }, { title: "123", value: 456 }, { value: "bar" } ];
      actual = getQueryResults("FOR d IN @@cn SORT d.value RETURN DOCUMENT(d._id)", { "@cn" : cn });
      assertEqual(expected, actual);
      
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
/// @brief test skiplist function
////////////////////////////////////////////////////////////////////////////////
    
    testSkiplist1 : function () {
      var cn = "UnitTestsAhuacatlFunctions";

      internal.db._drop(cn);
      var cx = internal.db._create(cn);
      cx.ensureSkiplist("created");

      var i;
      for (i = 0; i < 1000; ++i) {
        cx.save({ created: i });
      }
      
      var expected = [ { created: 0 } ];
      var actual = getQueryResults("FOR x IN SKIPLIST(" + cn + ", { created: [[ '>=', 0 ]] }, 0, 1) RETURN x"); 
      assertEqual(expected, actual);
      actual = getQueryResults("FOR x IN SKIPLIST(" + cn + ", { created: [[ '>=', 0 ], [ '<', 1 ]] }) RETURN x"); 
      assertEqual(expected, actual);
      
      expected = [ { created: 1 } ];
      actual = getQueryResults("FOR x IN SKIPLIST(" + cn + ", { created: [[ '>', 0 ]] }, 0, 1) RETURN x"); 
      assertEqual(expected, actual);
      actual = getQueryResults("FOR x IN SKIPLIST(" + cn + ", { created: [[ '>', 0 ], [ '<=', 1 ]] }) RETURN x"); 
      assertEqual(expected, actual);
      
      expected = [ { created: 0 }, { created: 1 }, { created: 2 } ];
      actual = getQueryResults("FOR x IN SKIPLIST(" + cn + ", { created: [[ '>=', 0 ]] }, 0, 3) RETURN x"); 
      assertEqual(expected, actual);
      
      expected = [ { created: 5 }, { created: 6 } ];
      actual = getQueryResults("FOR x IN SKIPLIST(" + cn + ", { created: [[ '>=', 0 ]] }, 5, 2) RETURN x"); 
      assertEqual(expected, actual);
      
      expected = [ { created: 5 }, { created: 6 } ];
      actual = getQueryResults("FOR x IN SKIPLIST(" + cn + ", { created: [[ '>=', 5 ], [ '<=', 6 ]] }, 0, 5) RETURN x"); 
      assertEqual(expected, actual);
      
      expected = [ { created: 5 } ];
      actual = getQueryResults("FOR x IN SKIPLIST(" + cn + ", { created: [[ '>=', 5 ], [ '<=', 6 ]] }, 0, 1) RETURN x"); 
      assertEqual(expected, actual);
      
      expected = [ { created: 2 }, { created: 3 } ];
      actual = getQueryResults("FOR x IN SKIPLIST(" + cn + ", { created: [[ '<', 4 ]] }, 2, 10) RETURN x"); 
      assertEqual(expected, actual);
      
      expected = [ ];
      actual = getQueryResults("FOR x IN SKIPLIST(" + cn + ", { created: [[ '>', 0 ]] }, 10000, 10) RETURN x"); 
      assertEqual(expected, actual);
      
      assertQueryError(errors.ERROR_ARANGO_NO_INDEX.code, "RETURN SKIPLIST(" + cn + ", { a: [[ '==', 1 ]] })"); 
      
      internal.db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test skiplist function
////////////////////////////////////////////////////////////////////////////////
    
    testSkiplist2 : function () {
      var cn = "UnitTestsAhuacatlFunctions";

      internal.db._drop(cn);
      var cx = internal.db._create(cn);
      
      assertQueryError(errors.ERROR_ARANGO_NO_INDEX.code, "RETURN SKIPLIST(" + cn + ", { a: [[ '==', 1 ]], b: [[ '==', 0 ]] })"); 

      cx.ensureSkiplist("a", "b");

      var i;
      for (i = 0; i < 1000; ++i) {
        cx.save({ a: i, b: i + 1 });
      }
      
      var expected = [ { a: 1, b: 2 } ];
      var actual = getQueryResults("FOR x IN SKIPLIST(" + cn + ", { a: [[ '==', 1 ]], b: [[ '>=', 2 ], [ '<=', 3 ]] }) RETURN x"); 
      assertEqual(expected, actual);
      
      expected = [ { a: 1, b: 2 } ];
      actual = getQueryResults("FOR x IN SKIPLIST(" + cn + ", { a: [[ '==', 1 ]], b: [[ '==', 2 ]] }) RETURN x"); 
      assertEqual(expected, actual);
      
      expected = [ ];
      actual = getQueryResults("FOR x IN SKIPLIST(" + cn + ", { a: [[ '==', 2 ]], b: [[ '==', 1 ]] }, 1, 1) RETURN x"); 
      assertEqual(expected, actual);
      actual = getQueryResults("FOR x IN SKIPLIST(" + cn + ", { a: [[ '==', 99 ]], b: [[ '==', 17 ]] }, 1, 1) RETURN x"); 
      assertEqual(expected, actual);
      
      assertQueryError(errors.ERROR_ARANGO_NO_INDEX.code, "RETURN SKIPLIST(" + cn + ", { a: [[ '==', 1 ]] })"); 
      assertQueryError(errors.ERROR_ARANGO_NO_INDEX.code, "RETURN SKIPLIST(" + cn + ", { b: [[ '==', 1 ]] })"); 
      assertQueryError(errors.ERROR_ARANGO_NO_INDEX.code, "RETURN SKIPLIST(" + cn + ", { c: [[ '==', 1 ]] })"); 
      
      internal.db._drop(cn);
    },
      
////////////////////////////////////////////////////////////////////////////////
/// @brief test current user function
////////////////////////////////////////////////////////////////////////////////

    testCurrentUser : function () {
      var actual = getQueryResults("RETURN CURRENT_USER()");
      // there is no current user in the non-request context
      assertEqual([ null ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test current database function
////////////////////////////////////////////////////////////////////////////////

    testCurrentDatabase : function () {
      var actual = getQueryResults("RETURN CURRENT_DATABASE()");
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

      // allow some tolerance for the time diff
      // TODO: we're currently sleeping twice because we're testing
      // old AQL against new AQL. when we're done comparing, remove the 2 *
      assertTrue(diff >= 1.8 && diff <= 2.5, "SLEEP(2) did not take between 1.8 and 2.5 seconds");
    }
  
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ahuacatlMiscFunctionsTestSuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
