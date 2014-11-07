/*global require, assertEqual, assertTrue, assertFalse, assertUndefined, fail */

////////////////////////////////////////////////////////////////////////////////
/// @brief test the statement class
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

var arangodb = require("org/arangodb");
var db = arangodb.db;
var ERRORS = arangodb.errors;

// -----------------------------------------------------------------------------
// --SECTION--                                           statement-related tests
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: statements
////////////////////////////////////////////////////////////////////////////////

function StatementSuite () {
  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      db._useDatabase("_system");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      try {
        db._dropDatabase("UnitTestsDatabase0");
      }
      catch (err) {
        // ignore this error
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test constructor
////////////////////////////////////////////////////////////////////////////////

    testConstructNoQuery : function () {
      try {
        db._createStatement();
        fail();
      }
      catch (e) {
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test constructor
////////////////////////////////////////////////////////////////////////////////

    testConstructQueryOnly : function () {
      var query = "for u in users return u";
      var st = db._createStatement({ query: query });

      assertEqual(query, st.getQuery());
      assertEqual([ ], st.getBindVariables());
      assertEqual(false, st.getCount());
      assertUndefined(st.getOptions());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test constructor
////////////////////////////////////////////////////////////////////////////////

    testConstructWithBind : function () {
      var query = "for v in @values return v";
      var bind = { values: [ 1, 2, 3 ] };
      var st = db._createStatement({ query: query, bindVars: bind });

      assertEqual(query, st.getQuery());
      assertEqual(bind, st.getBindVariables());
      assertEqual(false, st.getCount());
      assertEqual(null, st.getBatchSize());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test constructor
////////////////////////////////////////////////////////////////////////////////

    testConstructWithBindExecute : function () {
      var query = "for v in @values return v";
      var bind = { values: [ 1, 2, 3 ] };
      var st = db._createStatement({ query: query, bindVars: bind, count: true });

      var result = st.execute().toArray();
      assertEqual(3, result.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test parse method
////////////////////////////////////////////////////////////////////////////////

    testParseError : function () {
      var st = db._createStatement({ query : "for u in" });

      try {
        st.parse();
        fail();
      }
      catch (e) {
        assertEqual(ERRORS.ERROR_QUERY_PARSE.code, e.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test parse method
////////////////////////////////////////////////////////////////////////////////

    testParseOk1 : function () {
      var st = db._createStatement({ query : "for u in users return u" });
      var result = st.parse();

      assertEqual([ "users" ], result.collections);
      assertEqual([ ], result.bindVars);
      assertTrue(result.hasOwnProperty("ast"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test parse method, multiple collections
////////////////////////////////////////////////////////////////////////////////

    testParseOk2 : function () {
      var st = db._createStatement({ query : "for u in users for f in friends return u" });
      var result = st.parse();

      assertEqual([ "friends", "users" ], result.collections.sort());
      assertEqual([ ], result.bindVars);
      assertTrue(result.hasOwnProperty("ast"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test parse method, bind variables
////////////////////////////////////////////////////////////////////////////////

    testParseBind1 : function () {
      var st = db._createStatement({ query : "for u in @@users filter u.name == @name return u" });
      var result = st.parse();

      assertEqual([ ], result.collections);
      assertEqual([ "@users", "name" ], result.bindVars.sort());
      assertTrue(result.hasOwnProperty("ast"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test parse method, bind variables
////////////////////////////////////////////////////////////////////////////////

    testParseBind2 : function () {
      var st = db._createStatement({ query : "for u in @@users for f in friends filter u.name == @name && f.friendId == u._id return u" });
      var result = st.parse();

      assertEqual([ "friends" ], result.collections);
      assertEqual([ "@users", "name" ], result.bindVars.sort());
      assertTrue(result.hasOwnProperty("ast"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test explain method
////////////////////////////////////////////////////////////////////////////////

    testExplainError : function () {
      var st = db._createStatement({ query : "for u in" });

      try {
        st.explain();
        fail();
      }
      catch (e) {
        assertEqual(ERRORS.ERROR_QUERY_PARSE.code, e.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test explain method
////////////////////////////////////////////////////////////////////////////////

    testExplainOk : function () {
      var st = db._createStatement({ query : "FOR i IN 1..10 RETURN i" });
      var result = st.explain();

      assertEqual([ ], result.warnings);
      assertTrue(result.hasOwnProperty("plan"));
      assertFalse(result.hasOwnProperty("plans"));

      var plan = result.plan;
      assertTrue(plan.hasOwnProperty("estimatedCost"));
      assertTrue(plan.hasOwnProperty("rules"));
      assertEqual([ ], plan.rules);
      assertTrue(plan.hasOwnProperty("nodes"));
      assertTrue(plan.hasOwnProperty("collections"));
      assertEqual([ ], plan.collections);
      assertTrue(plan.hasOwnProperty("variables"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test explain method
////////////////////////////////////////////////////////////////////////////////

    testExplainAllPlans : function () {
      var st = db._createStatement({ query : "FOR i IN 1..10 RETURN i" });
      var result = st.explain({ allPlans: true });

      assertEqual([ ], result.warnings);
      assertFalse(result.hasOwnProperty("plan"));
      assertTrue(result.hasOwnProperty("plans"));

      assertEqual(1, result.plans.length);
      var plan = result.plans[0];
      assertTrue(plan.hasOwnProperty("estimatedCost"));
      assertTrue(plan.hasOwnProperty("rules"));
      assertEqual([ ], plan.rules);
      assertTrue(plan.hasOwnProperty("nodes"));
      assertTrue(plan.hasOwnProperty("collections"));
      assertEqual([ ], plan.collections);
      assertTrue(plan.hasOwnProperty("variables"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test explain method, bind variables
////////////////////////////////////////////////////////////////////////////////

    testExplainBindMissing : function () {
      var st = db._createStatement({ query : "FOR i IN @@list FILTER i == @value RETURN i" });
      try {
        st.explain();
      }
      catch (e) {
        assertEqual(ERRORS.ERROR_QUERY_BIND_PARAMETER_MISSING.code, e.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test explain method, bind variables
////////////////////////////////////////////////////////////////////////////////

    testExplainBindInvalidType : function () {
      var st = db._createStatement({ query : "FOR i IN @@list RETURN i" });
      st.bind("@list", [ 1, 2, 3 ]);

      try {
        st.explain();
      }
      catch (e) {
        assertEqual(ERRORS.ERROR_QUERY_BIND_PARAMETER_TYPE.code, e.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test explain method, bind variables
////////////////////////////////////////////////////////////////////////////////

    testExplainBindInvalid : function () {
      var st = db._createStatement({ query : "FOR i IN @list FILTER i == @value RETURN i" });
      st.bind("list", [ 1, 2, 3 ]);
      st.bind("value", 3);
      st.bind("foo", "bar");

      try {
        st.explain();
      }
      catch (e) {
        assertEqual(ERRORS.ERROR_QUERY_BIND_PARAMETER_UNDECLARED.code, e.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test bind method, bind variables
////////////////////////////////////////////////////////////////////////////////

    testExplainBind : function () {
      var st = db._createStatement({ query : "FOR i IN @list FILTER i == @value RETURN i" });
      st.bind("list", [ 1, 2, 3 ]);
      st.bind("value", 3);
      var result = st.explain();

      assertEqual([ ], result.warnings);
      assertTrue(result.hasOwnProperty("plan"));
      assertFalse(result.hasOwnProperty("plans"));

      var plan = result.plan;
      assertTrue(plan.hasOwnProperty("estimatedCost"));
      assertTrue(plan.hasOwnProperty("rules"));
      assertEqual([ ], plan.rules);
      assertTrue(plan.hasOwnProperty("nodes"));
      assertTrue(plan.hasOwnProperty("collections"));
      assertEqual([ ], plan.collections);
      assertTrue(plan.hasOwnProperty("variables"));
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test bind method, bind variables
////////////////////////////////////////////////////////////////////////////////

    testExplainBindWarnings : function () {
      var st = db._createStatement({ query : "FOR i IN 1..10 RETURN 1 / 0" });
      var result = st.explain();

      assertEqual(1, result.warnings.length);
      assertEqual(ERRORS.ERROR_QUERY_DIVISION_BY_ZERO.code, result.warnings[0].code);
      assertTrue(result.hasOwnProperty("plan"));
      assertFalse(result.hasOwnProperty("plans"));

      var plan = result.plan;
      assertTrue(plan.hasOwnProperty("estimatedCost"));
      assertTrue(plan.hasOwnProperty("rules"));
      assertTrue(plan.hasOwnProperty("nodes"));
      assertTrue(plan.hasOwnProperty("collections"));
      assertEqual([ ], plan.collections);
      assertTrue(plan.hasOwnProperty("variables"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test execute method
////////////////////////////////////////////////////////////////////////////////

    testExecuteError : function () {
      var st = db._createStatement({ query : "for u in" });
      try {
        var result = st.execute();
        result;
        fail();
      }
      catch (e) {
        assertEqual(ERRORS.ERROR_QUERY_PARSE.code, e.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test execute method
////////////////////////////////////////////////////////////////////////////////

    testExecuteOk1 : function () {
      var st = db._createStatement({ query : "for u in [ 1, 2, 3 ] return u" });
      var result = st.execute();

      var docs = [ ];
      while (result.hasNext()) {
        docs.push(result.next());
      }

      assertEqual([ 1, 2, 3 ], docs);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test execute method
////////////////////////////////////////////////////////////////////////////////

    testExecuteOk2 : function () {
      var st = db._createStatement({ query : "return 1" });
      st.setCount(true);
      st.setBatchSize(1);
      st.setQuery("for u in [ 1, 2, 3 ] return u");
      var result = st.execute();

      var docs = [ ];
      while (result.hasNext()) {
        docs.push(result.next());
      }

      assertEqual([ 1, 2, 3 ], docs);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test execute method, return extra
////////////////////////////////////////////////////////////////////////////////

    testExecuteExtra : function () {
      var st = db._createStatement({ query : "for i in 1..50 limit 1,2 return i", count: true, options: { fullCount: true } });
      var result = st.execute();

      assertEqual(2, result.count());
      var stats = result.getExtra().stats;
      assertTrue(stats.hasOwnProperty("scannedFull"));
      assertTrue(stats.hasOwnProperty("scannedIndex"));
      assertTrue(stats.hasOwnProperty("writesExecuted"));
      assertTrue(stats.hasOwnProperty("writesIgnored"));
      assertTrue(stats.hasOwnProperty("fullCount"));
      assertEqual(50, stats.fullCount);
      var docs = result.toArray();
      assertEqual(2, docs.length);

      assertEqual([ 2, 3 ], docs);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test execute method, return extra
////////////////////////////////////////////////////////////////////////////////

    testExecuteExtraFullCount : function () {
      var st = db._createStatement({ query : "for i in 1..12345 limit 4564, 2123 return i", count: true, options: { fullCount: true } });
      var result = st.execute();

      assertEqual(2123, result.count());
      var stats = result.getExtra().stats;
      assertTrue(stats.hasOwnProperty("scannedFull"));
      assertTrue(stats.hasOwnProperty("scannedIndex"));
      assertTrue(stats.hasOwnProperty("writesExecuted"));
      assertTrue(stats.hasOwnProperty("writesIgnored"));
      assertTrue(stats.hasOwnProperty("fullCount"));
      assertEqual(12345, stats.fullCount);
      var docs = result.toArray();
      assertEqual(2123, docs.length);

      var c = [ ];
      for (var i = 4565; i < 4565 + 2123; ++i) {
        c.push(i);
      }
      assertEqual(c, docs);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test execute method, return extra
////////////////////////////////////////////////////////////////////////////////

    testExecuteExtraFullCountLimit0 : function () {
      var st = db._createStatement({ query : "for i in 1..12345 limit 4564, 0 return i", count: true, options: { fullCount: true } });
      var result = st.execute();

      assertEqual(0, result.count());
      var stats = result.getExtra().stats;
      assertTrue(stats.hasOwnProperty("scannedFull"));
      assertTrue(stats.hasOwnProperty("scannedIndex"));
      assertTrue(stats.hasOwnProperty("writesExecuted"));
      assertTrue(stats.hasOwnProperty("writesIgnored"));
      assertTrue(stats.hasOwnProperty("fullCount"));
      assertEqual(12345, stats.fullCount);
      var docs = result.toArray();
      assertEqual(0, docs.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test execute method, return extra
////////////////////////////////////////////////////////////////////////////////

    testExecuteExtraNoFullCount : function () {
      var st = db._createStatement({ query : "for i in 1..10 return i", count: true, options: { fullCount: false } });
      var result = st.execute();

      assertEqual(10, result.count());
      var stats = result.getExtra().stats;
      assertTrue(stats.hasOwnProperty("scannedFull"));
      assertTrue(stats.hasOwnProperty("scannedIndex"));
      assertTrue(stats.hasOwnProperty("writesExecuted"));
      assertTrue(stats.hasOwnProperty("writesIgnored"));
      assertFalse(stats.hasOwnProperty("fullCount"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test bind method
////////////////////////////////////////////////////////////////////////////////

    testBind : function () {
      var st = db._createStatement({ query : "for u in @list return @value" });
      st.bind("list", [ 1, 2, 3 ]);
      st.bind("value", 25);
      var result = st.execute();

      var docs = [ ];
      while (result.hasNext()) {
        docs.push(result.next());
      }

      assertEqual([ 25, 25, 25 ], docs);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test bind method
////////////////////////////////////////////////////////////////////////////////

    testBindVariables1 : function () {
      var st = db._createStatement({ query : "for u in @list return @value + @something" });
      var result = st.getBindVariables();

      assertEqual({ }, result);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test bind method
////////////////////////////////////////////////////////////////////////////////

    testBindVariables2 : function () {
      var st = db._createStatement({ query : "for u in @list return @value + @something" });
      st.bind("list", [ 1, 2 ]);
      st.bind("value", "something");
      st.bind("something", "something else");
      st.bind("even more", "data goes here");
      var result = st.getBindVariables();

      assertEqual({ "list" : [ 1, 2 ], "value" : "something", "something" : "something else", "even more" : "data goes here" }, result);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test bind method
////////////////////////////////////////////////////////////////////////////////

    testBindInvalid : function () {
      var st = db._createStatement({ query : "for u in [ 1 ] return @value" });
      st.bind("list", [ 1, 2, 3 ]);
      try {
        st.execute();
        fail();
      }
      catch (e) {
        assertEqual(ERRORS.ERROR_QUERY_BIND_PARAMETER_MISSING.code, e.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test bind method
////////////////////////////////////////////////////////////////////////////////

    testBindRedeclare : function () {
      var st = db._createStatement({ query : "for u in [ 1 ] return @value" });
      st.bind("value", 1);
      try {
        st.bind("value", 1);
        fail();
      }
      catch (e) {
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test get/set query
////////////////////////////////////////////////////////////////////////////////

    testQuery : function () {
      var st = db._createStatement({ query : "for u in [ 1 ] return 1" });

      assertEqual("for u in [ 1 ] return 1", st.getQuery());

      st.setQuery("for u2 in users return 2");
      assertEqual("for u2 in users return 2", st.getQuery());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test get/set count
////////////////////////////////////////////////////////////////////////////////

    testCount : function () {
      var st = db._createStatement({ query : "for u in [ 1 ] return 1" });

      assertEqual(false, st.getCount());

      st.setCount(true);
      assertEqual(true, st.getCount());

      st.setCount(false);
      assertEqual(false, st.getCount());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test get/set batch size
////////////////////////////////////////////////////////////////////////////////

    testBatchSize: function () {
      var st = db._createStatement({ query : "for u in [ 1 ] return 1" });

      assertEqual(null, st.getBatchSize());

      st.setBatchSize(1);
      assertEqual(1, st.getBatchSize());

      st.setBatchSize(100);
      assertEqual(100, st.getBatchSize());

      st.setBatchSize(10000);
      assertEqual(10000, st.getBatchSize());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test options
////////////////////////////////////////////////////////////////////////////////

    testOptions : function () {
      var st = db._createStatement({ query : "for u in [ 1 ] return 1", options : { foo: 1, bar: 2 } });

      assertEqual({ foo: 1, bar: 2 }, st.getOptions());

      st.setOptions({ });
      assertEqual({ }, st.getOptions());

      st.setOptions({ baz: true });
      assertEqual({ baz: true }, st.getOptions());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test incremental fetch
////////////////////////////////////////////////////////////////////////////////

    testIncremental : function () {
      var st = db._createStatement({ query : "for i in 1..10 return i", batchSize : 1 });

      var c = st.execute();

      var result = [ ];
      while (c.hasNext()) {
        result.push(c.next());
      }

      assertEqual([ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ], result);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test dispose
////////////////////////////////////////////////////////////////////////////////

    testDispose1 : function () {
      var st = db._createStatement({ query : "for i in 1..10 return i", batchSize : 1 });

      var c = st.execute();

      c.dispose();

      try {
        // cursor does not exist anymore
        c.next();
      }
      catch (err) {
        require("internal").print(err);
        assertEqual(ERRORS.ERROR_ARANGO_DATABASE_NAME_INVALID.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test dispose
////////////////////////////////////////////////////////////////////////////////

    testDispose2 : function () {
      var st = db._createStatement({ query : "for i in 1..10 return i", batchSize : 1 });

      var c = st.execute();

      while (c.hasNext()) {
        c.next();
      }
      // this should have auto-disposed the cursor

      try {
        c.dispose();
      }
      catch (err) {
        require("internal").print(err);
        assertEqual(ERRORS.ERROR_ARANGO_DATABASE_NAME_INVALID.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test database change
////////////////////////////////////////////////////////////////////////////////

    testDatabaseChange : function () {
      assertEqual("_system", db._name());

      var st = db._createStatement({ query : "for i in 1..10 return i", batchSize : 1 });

      var c = st.execute();
      var result = [ ];

      try {
        db._dropDatabase("UnitTestsDatabase0");
      }
      catch (err) {
      }

      db._createDatabase("UnitTestsDatabase0");
      db._useDatabase("UnitTestsDatabase0");

      // now we have changed the database and should still be able to use the cursor from the
      // other...

      while (c.hasNext()) {
        result.push(c.next());
      }

      assertEqual([ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ], result);

      db._useDatabase("_system");
      db._dropDatabase("UnitTestsDatabase0");
    }

  };
}

// -----------------------------------------------------------------------------
// --SECTION--                                                              main
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(StatementSuite);
return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
