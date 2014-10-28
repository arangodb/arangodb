/*jshint strict: false, maxlen: 500 */
/*global require, assertEqual, AQL_EXECUTE, AQL_EXPLAIN */

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
var helper = require("org/arangodb/aql-helper");
var db = require("org/arangodb").db;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function explainSuite () {
  var cn = "UnitTestsAhuacatlPrimary";
  var c;

  var explain = function (query, params) {
    return helper.getCompactPlan(AQL_EXPLAIN(query, params, { optimizer: { rules: [ "-all", "+use-index-range" ] } })).map(function(node) { return node.type; });
  };

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      db._drop(cn);
      c = db._create(cn);

      for (var i = 0; i < 100; ++i) {
        c.save({ _key: "testkey" + i, value: i });
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
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
        assertEqual([ "SingletonNode", "ScatterNode", "RemoteNode", "IndexRangeNode", "CalculationNode", "FilterNode", "RemoteNode", "GatherNode", "ReturnNode" ], explain(query, { key: key }));
        assertEqual([ ], AQL_EXECUTE(query, { key: key }).json);
      });
    }, 

////////////////////////////////////////////////////////////////////////////////
/// @brief test lookup by existing _key
////////////////////////////////////////////////////////////////////////////////

    testKeyExisting : function () {
      var query = "FOR i IN " + cn + " FILTER i._key == @key RETURN i.value";
      for (var i = 0; i < 100; ++i) {
        assertEqual([ "SingletonNode", "ScatterNode", "RemoteNode", "IndexRangeNode", "CalculationNode", "FilterNode", "CalculationNode", "RemoteNode", "GatherNode", "ReturnNode" ], explain(query, { key: "testkey" + i }));
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
        assertEqual([ "SingletonNode", "ScatterNode", "RemoteNode", "IndexRangeNode", "CalculationNode", "FilterNode", "RemoteNode", "GatherNode", "ReturnNode" ], explain(query, { id: id }));
        assertEqual([ ], AQL_EXECUTE(query, { id: id }).json);
      });
    }, 

////////////////////////////////////////////////////////////////////////////////
/// @brief test lookup by existing _id
////////////////////////////////////////////////////////////////////////////////

    testIdExisting : function () {
      var query = "FOR i IN " + cn + " FILTER i._id == @id RETURN i.value";
      for (var i = 0; i < 100; ++i) {
        assertEqual([ "SingletonNode", "ScatterNode", "RemoteNode", "IndexRangeNode", "CalculationNode", "FilterNode", "CalculationNode", "RemoteNode", "GatherNode", "ReturnNode" ], explain(query, { id: cn + "/testkey" + i }));
        assertEqual([ i ], AQL_EXECUTE(query, { id: cn + "/testkey" + i }).json);
      }
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(explainSuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
