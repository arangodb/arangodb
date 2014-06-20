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

var internal = require("internal");
var db = require("org/arangodb").db;
var jsunity = require("jsunity");
var helper = require("org/arangodb/aql-helper");
var getModifyQueryResults = helper.getModifyQueryResults;
var assertQueryError = helper.assertQueryError;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ahuacatlModifySuite () {
  var errors = internal.errors;
  var cn1 = "UnitTestsAhuacatlModify1";
  var cn2 = "UnitTestsAhuacatlModify2";
  var c1, c2;

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      db._drop(cn1);
      db._drop(cn2);
      c1 = db._create(cn1);
      c2 = db._create(cn2);

      c1.save({ _key: "foo", a: 1 });
      c2.save({ _key: "foo", b: 1 });
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
/// @brief test subquery
////////////////////////////////////////////////////////////////////////////////

    testRemoveInSubquery : function () {
      assertQueryError(errors.ERROR_QUERY_MODIFY_IN_SUBQUERY.code, "FOR d IN @@cn LET x = (REMOVE d.foobar IN @@cn) RETURN d", { "@cn": cn1 });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test subquery
////////////////////////////////////////////////////////////////////////////////

    testInsertInSubquery : function () {
      assertQueryError(errors.ERROR_QUERY_MODIFY_IN_SUBQUERY.code, "FOR d IN @@cn LET x = (INSERT { _key: 'test' } IN @@cn) RETURN d", { "@cn": cn1 });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test subquery
////////////////////////////////////////////////////////////////////////////////

    testUpdateInSubquery : function () {
      assertQueryError(errors.ERROR_QUERY_MODIFY_IN_SUBQUERY.code, "FOR d IN @@cn LET x = (UPDATE { _key: 'test' } IN @@cn) RETURN d", { "@cn": cn1 });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test subquery
////////////////////////////////////////////////////////////////////////////////

    testReplaceInSubquery : function () {
      assertQueryError(errors.ERROR_QUERY_MODIFY_IN_SUBQUERY.code, "FOR d IN @@cn LET x = (REPLACE { _key: 'test' } IN @@cn) RETURN d", { "@cn": cn1 });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test subquery
////////////////////////////////////////////////////////////////////////////////

    testReplaceInSubquery2 : function () {
      assertQueryError(errors.ERROR_QUERY_MODIFY_IN_SUBQUERY.code, "FOR d IN @@cn LET x = (FOR i IN 1..2 REPLACE { _key: 'test' } IN @@cn) RETURN d", { "@cn": cn1 });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test subquery
////////////////////////////////////////////////////////////////////////////////

    testMultiModify : function () {
      assertQueryError(errors.ERROR_QUERY_PARSE.code, "FOR d IN @@cn1 REMOVE d IN @@cn1 FOR e IN @@cn2 REMOVE e IN @@cn2", { "@cn1": cn1, "@cn2": cn2 });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test subquery
////////////////////////////////////////////////////////////////////////////////

    testMultiModify2 : function () {
      assertQueryError(errors.ERROR_QUERY_PARSE.code, "FOR d IN @@cn1 FOR e IN @@cn2 REMOVE d IN @@cn1 REMOVE e IN @@cn2", { "@cn1": cn1, "@cn2": cn2 });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test subquery
////////////////////////////////////////////////////////////////////////////////

    testDynamicOptions1 : function () {
      assertQueryError(errors.ERROR_QUERY_COMPILE_TIME_OPTIONS.code, "FOR d IN @@cn REMOVE d IN @@cn OPTIONS { foo: d }", { "@cn": cn1 });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test subquery
////////////////////////////////////////////////////////////////////////////////

    testDynamicOptions2 : function () {
      assertQueryError(errors.ERROR_QUERY_COMPILE_TIME_OPTIONS.code, "FOR d IN @@cn REMOVE d IN @@cn OPTIONS { foo: MERGE(d, { }) }", { "@cn": cn1 });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test subquery
////////////////////////////////////////////////////////////////////////////////

    testInvalidOptions : function () {
      assertQueryError(errors.ERROR_QUERY_PARSE.code, "FOR d IN @@cn REMOVE d IN @@cn OPTIONS 'foo'", { "@cn": cn1 });
    },
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ahuacatlRemoveSuite () {
  var errors = internal.errors;
  var cn1 = "UnitTestsAhuacatlRemove1";
  var cn2 = "UnitTestsAhuacatlRemove2";
  var c1;

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
      for (var i = 0; i < 50; ++i) {
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
/// @brief test remove
////////////////////////////////////////////////////////////////////////////////

    testRemoveNothing : function () {
      var expected = { executed: 0, ignored: 0 };
      var actual = getModifyQueryResults("FOR d IN " + cn1 + " FILTER d.value1 < 0 REMOVE d IN " + cn1);
    
      assertEqual(100, c1.count());
      assertEqual(expected, actual.operations);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove
////////////////////////////////////////////////////////////////////////////////

    testRemoveNothingBind : function () {
      var expected = { executed: 0, ignored: 0 };
      var actual = getModifyQueryResults("FOR d IN @@cn FILTER d.value1 < 0 REMOVE d IN @@cn", { "@cn": cn1 });

      assertEqual(100, c1.count());
      assertEqual(expected, actual.operations);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove
////////////////////////////////////////////////////////////////////////////////

    testRemoveInvalid1 : function () {
      assertQueryError(errors.ERROR_ARANGO_DOCUMENT_TYPE_INVALID.code, "FOR d IN @@cn REMOVE d.foobar IN @@cn", { "@cn": cn1 });
      assertEqual(100, c1.count());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove
////////////////////////////////////////////////////////////////////////////////

    testRemoveInvalid2 : function () {
      assertQueryError(errors.ERROR_ARANGO_DOCUMENT_KEY_MISSING.code, "FOR d IN @@cn REMOVE { } IN @@cn", { "@cn": cn1 });
      assertEqual(100, c1.count());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove
////////////////////////////////////////////////////////////////////////////////

    testRemoveInvalid3 : function () {
      assertQueryError(errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code, "FOR d IN @@cn REMOVE 'foobar' IN @@cn", { "@cn": cn1 });
      assertEqual(100, c1.count());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove
////////////////////////////////////////////////////////////////////////////////

    testRemoveInvalid4 : function () {
      assertQueryError(errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code, "FOR i iN 0..100 REMOVE CONCAT('test', TO_STRING(i)) IN @@cn", { "@cn": cn1 });
      assertEqual(100, c1.count());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove
////////////////////////////////////////////////////////////////////////////////

    testRemoveIgnore1 : function () {
      var expected = { executed: 0, ignored: 100 };
      var actual = getModifyQueryResults("FOR d IN @@cn REMOVE 'foo' IN @@cn OPTIONS { ignoreErrors: true }", { "@cn": cn1 });

      assertEqual(100, c1.count());
      assertEqual(expected, actual.operations);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove
////////////////////////////////////////////////////////////////////////////////

    testRemoveIgnore2 : function () {
      var expected = { executed: 100, ignored: 1 };
      var actual = getModifyQueryResults("FOR i IN 0..100 REMOVE CONCAT('test', TO_STRING(i)) IN @@cn OPTIONS { ignoreErrors: true }", { "@cn": cn1 });

      assertEqual(0, c1.count());
      assertEqual(expected, actual.operations);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove
////////////////////////////////////////////////////////////////////////////////

    testRemoveAll1 : function () {
      var expected = { executed: 100, ignored: 0 };
      var actual = getModifyQueryResults("FOR d IN @@cn REMOVE d IN @@cn", { "@cn": cn1 });

      assertEqual(0, c1.count());
      assertEqual(expected, actual.operations);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove
////////////////////////////////////////////////////////////////////////////////

    testRemoveAll2 : function () {
      var expected = { executed: 100, ignored: 0 };
      var actual = getModifyQueryResults("FOR d IN @@cn REMOVE d._key IN @@cn", { "@cn": cn1 });

      assertEqual(0, c1.count());
      assertEqual(expected, actual.operations);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove
////////////////////////////////////////////////////////////////////////////////

    testRemoveAll3 : function () {
      var expected = { executed: 100, ignored: 0 };
      var actual = getModifyQueryResults("FOR d IN @@cn REMOVE { _key: d._key } IN @@cn", { "@cn": cn1 });

      assertEqual(0, c1.count());
      assertEqual(expected, actual.operations);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove
////////////////////////////////////////////////////////////////////////////////

    testRemoveAll4 : function () {
      var expected = { executed: 100, ignored: 0 };
      var actual = getModifyQueryResults("FOR i IN 0..99 REMOVE { _key: CONCAT('test', TO_STRING(i)) } IN @@cn", { "@cn": cn1 });

      assertEqual(0, c1.count());
      assertEqual(expected, actual.operations);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove
////////////////////////////////////////////////////////////////////////////////

    testRemoveHalf : function () {
      var expected = { executed: 50, ignored: 0 };
      var actual = getModifyQueryResults("FOR i IN 0..99 FILTER i % 2 == 0 REMOVE { _key: CONCAT('test', TO_STRING(i)) } IN @@cn", { "@cn": cn1 });

      assertEqual(50, c1.count());
      assertEqual(expected, actual.operations);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove
////////////////////////////////////////////////////////////////////////////////

    testSingle : function () {
      var expected = { executed: 1, ignored: 0 };
      var actual = getModifyQueryResults("REMOVE 'test0' IN @@cn", { "@cn": cn1 });

      assertEqual(99, c1.count());
      assertEqual(expected, actual.operations);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove
////////////////////////////////////////////////////////////////////////////////

    testTwoCollectionsNotFound : function () {
      assertQueryError(errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code, "FOR d IN @@cn1 REMOVE { _key: d._key } IN @@cn2", { "@cn1": cn1, "@cn2": cn2 });

      assertEqual(100, c1.count());
      assertEqual(50, c2.count());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove
////////////////////////////////////////////////////////////////////////////////

    testTwoCollectionsJoin1 : function () {
      var expected = { executed: 50, ignored: 0 };
      var actual = getModifyQueryResults("FOR d IN @@cn1 FILTER d.value1 < 50 REMOVE { _key: d._key } IN @@cn2", { "@cn1": cn1, "@cn2": cn2 });

      assertEqual(100, c1.count());
      assertEqual(0, c2.count());
      assertEqual(expected, actual.operations);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove
////////////////////////////////////////////////////////////////////////////////

    testTwoCollectionsJoin2 : function () {
      var expected = { executed: 48, ignored: 0 };
      var actual = getModifyQueryResults("FOR d IN @@cn1 FILTER d.value1 >= 2 && d.value1 < 50 REMOVE { _key: d._key } IN @@cn2", { "@cn1": cn1, "@cn2": cn2 });

      assertEqual(100, c1.count());
      assertEqual(2, c2.count());
      assertEqual(expected, actual.operations);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove
////////////////////////////////////////////////////////////////////////////////

    testTwoCollectionsIgnoreErrors1 : function () {
      var expected = { executed: 50, ignored: 50 };
      var actual = getModifyQueryResults("FOR d IN @@cn1 REMOVE { _key: d._key } IN @@cn2 OPTIONS { ignoreErrors: true }", { "@cn1": cn1, "@cn2": cn2 });

      assertEqual(100, c1.count());
      assertEqual(0, c2.count());
      assertEqual(expected, actual.operations);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove
////////////////////////////////////////////////////////////////////////////////

    testTwoCollectionsIgnoreErrors2 : function () {
      var expected = { executed: 0, ignored: 100 };
      var actual = getModifyQueryResults("FOR d IN @@cn1 REMOVE { _key: CONCAT('foo', d._key) } IN @@cn2 OPTIONS { ignoreErrors: true }", { "@cn1": cn1, "@cn2": cn2 });

      assertEqual(100, c1.count());
      assertEqual(50, c2.count());
      assertEqual(expected, actual.operations);
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ahuacatlModifySuite);
jsunity.run(ahuacatlRemoveSuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
