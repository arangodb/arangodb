/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, AQL_EXECUTE, AQL_EXPLAIN */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for primary index
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
var helper = require("@arangodb/aql-helper");
var db = require("@arangodb").db;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function explainSuite () {
  var cn = "UnitTestsAhuacatlPrimary";
  var c;

  var explain = function (query, params) {
    return helper.getCompactPlan(AQL_EXPLAIN(query, params, { optimizer: { rules: [ "-all", "+use-indexes" ] } })).map(function(node) { return node.type; });
  };

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUpAll : function () {
      db._drop(cn);
      c = db._create(cn);

      for (var i = 0; i < 100; ++i) {
        c.save({ _key: "testkey" + i, value: i });
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDownAll : function () {
      db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test lookup by non-existing _key
////////////////////////////////////////////////////////////////////////////////

    testKeyNotExisting : function () {
      var keys = [ 
        'foobar', 
        'baz', 
        '123', 
        '', 
        ' this key is not valid', 
        ' testkey1', 
        'testkey100', 
        'testkey1000', 
        1, 
        100, 
        false, 
        true, 
        null 
      ];
      var query = "FOR i IN " + cn + " FILTER i._key == @key RETURN i";

      keys.forEach(function(key) {
        assertEqual([ "SingletonNode", "IndexNode", "CalculationNode", "FilterNode", "ReturnNode" ], explain(query, { key: key }));
        assertEqual([ ], AQL_EXECUTE(query, { key: key }).json);
      });
    }, 

////////////////////////////////////////////////////////////////////////////////
/// @brief test lookup by existing _key
////////////////////////////////////////////////////////////////////////////////

    testKeyExisting : function () {
      var query = "FOR i IN " + cn + " FILTER i._key == @key RETURN i.value";
      for (var i = 0; i < 100; ++i) {
        assertEqual([ "SingletonNode", "IndexNode", "CalculationNode", "FilterNode", "CalculationNode", "ReturnNode" ], explain(query, { key: "testkey" + i }));
        assertEqual([ i ], AQL_EXECUTE(query, { key: "testkey" + i }).json);
      }
    }, 

////////////////////////////////////////////////////////////////////////////////
/// @brief test lookup by non-existing _id
////////////////////////////////////////////////////////////////////////////////

    testIdNotExisting : function () {
      var ids = [ 
        '_users/testkey10',
        ' ' + cn + '/testkey10',
        'othercollection/testkey1',
        'othercollection/testkey10',
        cn + '/testkey100',
        cn + '/testkey101',
        cn + '/testkey1000',
        cn + '/testkey-1',
        'foobar', 
        'baz', 
        '123', 
        '', 
        ' this key is not valid', 
        ' testkey1', 
        'testkey100', 
        'testkey1000', 
        1, 
        100, 
        false, 
        true, 
        null 
      ]; 
      var query = "FOR i IN " + cn + " FILTER i._id == @id RETURN i";

      ids.forEach(function(id) {
        assertEqual([ "SingletonNode", "IndexNode", "CalculationNode", "FilterNode", "ReturnNode" ], explain(query, { id: id }));
        assertEqual([ ], AQL_EXECUTE(query, { id: id }).json);
      });
    }, 

////////////////////////////////////////////////////////////////////////////////
/// @brief test lookup by existing _id
////////////////////////////////////////////////////////////////////////////////

    testIdExisting : function () {
      var query = "FOR i IN " + cn + " FILTER i._id == @id RETURN i.value";
      for (var i = 0; i < 100; ++i) {
        assertEqual([ "SingletonNode", "IndexNode", "CalculationNode", "FilterNode", "CalculationNode", "ReturnNode" ], explain(query, { id: cn + "/testkey" + i }));
        assertEqual([ i ], AQL_EXECUTE(query, { id: cn + "/testkey" + i }).json);
      }
    },

    testInvalidValuesinList : function () {
      var query = "FOR x IN @idList FOR i IN " + cn + " FILTER i._id == x SORT i.value RETURN i.value";
      var bindParams = {
        idList: [
          null,
          cn + "/testkey1", // Find this
          "blub/bla",
          "noKey",
          cn + "/testkey2", // And this
          123456,
          { "the": "foxx", "is": "wrapped", "in":"objects"},
          [15, "man", "on", "the", "dead", "mans", "chest"],
          cn + "/testkey3" // And this
        ]
      };
      assertEqual([ 1, 2, 3], AQL_EXECUTE(query, bindParams).json);
    },

    testInvalidValuesInCondition : function () {
      var query = "FOR i IN " + cn + " FILTER i._id IN @idList SORT i.value RETURN i.value";
      var bindParams = {
        idList: [
          null,
          cn + "/testkey1", // Find this
          "blub/bla",
          "noKey",
          cn + "/testkey2", // And this
          123456,
          { "the": "foxx", "is": "wrapped", "in":"objects"},
          [15, "man", "on", "the", "dead", "mans", "chest"],
          cn + "/testkey3" // And this
        ]
      };
      assertEqual([ 1, 2, 3], AQL_EXECUTE(query, bindParams).json);
    },

    testInIteratorJustNumeric : function () {
      let query = "FOR i IN 1..10 FOR doc IN " + cn + " FILTER doc._id IN [i, i + 1] RETURN doc._key";

      assertEqual([ ], AQL_EXECUTE(query).json);
    },
    
    testInIteratorString : function () {
      let query = "FOR i IN 1..5 LET key = CONCAT('testkey', i) FOR doc IN " + cn + " FILTER doc._key IN [key, 'foo'] RETURN doc._key";

      assertEqual(["testkey1", "testkey2", "testkey3", "testkey4", "testkey5"], AQL_EXECUTE(query).json);
    },
    
    testInIteratorStringId : function () {
      let query = "FOR i IN 1..5 LET key = CONCAT('" + cn + "/testkey', i) FOR doc IN " + cn + " FILTER doc._id IN [key, 'foo'] RETURN doc._key";

      assertEqual(["testkey1", "testkey2", "testkey3", "testkey4", "testkey5"], AQL_EXECUTE(query).json);
    },

    testInIteratorStringAndNumericReturnId : function () {
      let query = "FOR i IN 1..5 LET key = CONCAT('" + cn + "/testkey', i) FOR doc IN " + cn + " FILTER doc._id IN [key, i] RETURN doc._id";

      assertEqual([cn + "/testkey1", cn + "/testkey2", cn + "/testkey3", cn + "/testkey4", cn + "/testkey5"], AQL_EXECUTE(query).json);
    },

    testInIteratorStringAndNumeric : function () {
      let query = "FOR i IN 1..5 LET key = CONCAT('" + cn + "/testkey', i) FOR doc IN " + cn + " FILTER doc._id IN [key, i] RETURN doc._key";

      assertEqual(["testkey1", "testkey2", "testkey3", "testkey4", "testkey5"], AQL_EXECUTE(query).json);
    },

    testInIteratorStringDuplicate : function () {
      let query = "FOR i IN 1..5 LET key = CONCAT('" + cn + "/testkey', i) FOR doc IN " + cn + " FILTER doc._id IN [key, key] RETURN doc._id";

      assertEqual([cn + "/testkey1", cn + "/testkey2", cn + "/testkey3", cn + "/testkey4", cn + "/testkey5"], AQL_EXECUTE(query).json);
    },
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(explainSuite);

return jsunity.done();

