/*jslint indent: 2, nomen: true, maxlen: 80 */
/*global require, assertEqual, assertTrue */

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
var ArangoStatement = require("org/arangodb/arango-statement").ArangoStatement;
var db = arangodb.db;

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
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test constructor
////////////////////////////////////////////////////////////////////////////////

    testConstructNoQuery : function () {
      try {
        var st = new ArangoStatement(db);
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
      var st = new ArangoStatement(db, { query: query });

      assertEqual(query, st.getQuery());
      assertEqual([ ], st.getBindVariables());
      assertEqual(false, st.getCount());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test constructor
////////////////////////////////////////////////////////////////////////////////

    testConstructWithBind : function () {
      var query = "for v in @values return v"; 
      var bind = { values: [ 1, 2, 3 ] };
      var st = new ArangoStatement(db, { query: query, bindVars: bind });

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
      var st = new ArangoStatement(db, { query: query, bindVars: bind, count: true });

      var result = st.execute().toArray();
      assertEqual(3, result.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test parse method
////////////////////////////////////////////////////////////////////////////////

    testParseError : function () {
      var st = new ArangoStatement(db, { query : "for u in" });

      try {
        st.parse();
      }
      catch (e) {
        assertEqual(1501, e.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test parse method
////////////////////////////////////////////////////////////////////////////////

    testParseOk1 : function () {
      var st = new ArangoStatement(db, { query : "for u in users return u" });
      var result = st.parse();

      assertEqual([ "users" ], result.collections);
      assertEqual([ ], result.bindVars);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test parse method, multiple collections
////////////////////////////////////////////////////////////////////////////////

    testParseOk2 : function () {
      var st = new ArangoStatement(db, { query : "for u in users for f in friends return u" });
      var result = st.parse();

      assertEqual([ "users", "friends" ], result.collections);
      assertEqual([ ], result.bindVars);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test parse method, bind variables
////////////////////////////////////////////////////////////////////////////////

    testParseBind1 : function () {
      var st = new ArangoStatement(db, { query : "for u in @@users filter u.name == @name return u" });
      var result = st.parse();

      assertEqual([ ], result.collections);
      assertEqual([ "name", "@users" ], result.bindVars);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test parse method, bind variables
////////////////////////////////////////////////////////////////////////////////

    testParseBind2 : function () {
      var st = new ArangoStatement(db, { query : "for u in @@users for f in friends filter u.name == @name && f.friendId == u._id return u" });
      var result = st.parse();

      assertEqual([ "friends" ], result.collections);
      assertEqual([ "name", "@users" ], result.bindVars);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test explain method
////////////////////////////////////////////////////////////////////////////////

    testExplainError : function () {
      var st = new ArangoStatement(db, { query : "for u in" });
      try {
        st.explain();
      }
      catch (e) {
        assertEqual(1501, e.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test explain method
////////////////////////////////////////////////////////////////////////////////

    testExplainNoBindError : function () {
      var st = new ArangoStatement(db, { query : "for i in [ 1 ] return @f" });
      try {
        st.explain();
      }
      catch (e) {
        assertEqual(1551, e.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test explain method
////////////////////////////////////////////////////////////////////////////////

    testExplainWithBind : function () {
      var st = new ArangoStatement(db, { query : "for i in [ 1 ] return @f", bindVars: { f : 1 } });
      var result = st.explain();

      assertEqual(2, result.length);

      assertEqual(1, result[0]["id"]);
      assertEqual(1, result[0]["loopLevel"]);
      assertEqual("for", result[0]["type"]);
      
      assertEqual(2, result[1]["id"]);
      assertEqual(1, result[1]["loopLevel"]);
      assertEqual("return", result[1]["type"]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test explain method
////////////////////////////////////////////////////////////////////////////////

    testExplainOk1 : function () {
      var st = new ArangoStatement(db, { query : "for u in [ 1, 2, 3 ] return 1" });
      var result = st.explain();

      assertEqual(2, result.length);

      assertEqual(1, result[0]["id"]);
      assertEqual(1, result[0]["loopLevel"]);
      assertEqual("for", result[0]["type"]);

      assertEqual(2, result[1]["id"]);
      assertEqual(1, result[1]["loopLevel"]);
      assertEqual("return", result[1]["type"]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test explain method
////////////////////////////////////////////////////////////////////////////////

    testExplainOk2 : function () {
      var st = new ArangoStatement(db, { query : "for u in [ 1, 2, 3 ] filter u != 1 for f in u return 1" });
      var result = st.explain();

      assertEqual(4, result.length);

      assertEqual(1, result[0]["id"]);
      assertEqual(1, result[0]["loopLevel"]);
      assertEqual("for", result[0]["type"]);

      assertEqual(2, result[1]["id"]);
      assertEqual(1, result[1]["loopLevel"]);
      assertEqual("filter", result[1]["type"]);
      
      assertEqual(3, result[2]["id"]);
      assertEqual(2, result[2]["loopLevel"]);
      assertEqual("for", result[2]["type"]);
      
      assertEqual(4, result[3]["id"]);
      assertEqual(2, result[3]["loopLevel"]);
      assertEqual("return", result[3]["type"]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test execute method
////////////////////////////////////////////////////////////////////////////////

    testExecuteError : function () {
      var st = new ArangoStatement(db, { query : "for u in" });
      try {
        result = st.execute();
      }
      catch (e) {
        assertEqual(1501, e.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test execute method
////////////////////////////////////////////////////////////////////////////////

    testExecuteOk1 : function () {
      var st = new ArangoStatement(db, { query : "for u in [ 1, 2, 3 ] return u" });
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
      var st = new ArangoStatement(db, { query : "return 1" });
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
/// @brief test bind method
////////////////////////////////////////////////////////////////////////////////

    testBind : function () {
      var st = new ArangoStatement(db, { query : "for u in @list return @value" });
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
      var st = new ArangoStatement(db, { query : "for u in @list return @value + @something" });
      var result = st.getBindVariables();

      assertEqual({ }, result);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test bind method
////////////////////////////////////////////////////////////////////////////////

    testBindVariables2 : function () {
      var st = new ArangoStatement(db, { query : "for u in @list return @value + @something" });
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
      var st = new ArangoStatement(db, { query : "for u in [ 1 ] return @value" });
      st.bind("list", [ 1, 2, 3 ]);
      try {
        var result = st.execute();
        assertFalse(true);
      }
      catch (e) {
        assertEqual(1551, e.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test bind method
////////////////////////////////////////////////////////////////////////////////

    testBindRedeclare : function () {
      var st = new ArangoStatement(db, { query : "for u in [ 1 ] return @value" });
      st.bind("value", 1);
      try {
        st.bind("value", 1);
        assertFalse(true);
      }
      catch (e) {
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test get/set query
////////////////////////////////////////////////////////////////////////////////

    testQuery : function () {
      var st = new ArangoStatement(db, { query : "for u in [ 1 ] return 1" });

      assertEqual("for u in [ 1 ] return 1", st.getQuery());

      st.setQuery("for u2 in users return 2");
      assertEqual("for u2 in users return 2", st.getQuery());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test get/set count
////////////////////////////////////////////////////////////////////////////////

    testCount : function () {
      var st = new ArangoStatement(db, { query : "for u in [ 1 ] return 1" });

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
      var st = new ArangoStatement(db, { query : "for u in [ 1 ] return 1" });

      assertEqual(null, st.getBatchSize());

      st.setBatchSize(1);
      assertEqual(1, st.getBatchSize());
      
      st.setBatchSize(100);
      assertEqual(100, st.getBatchSize());
      
      st.setBatchSize(10000);
      assertEqual(10000, st.getBatchSize());
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
