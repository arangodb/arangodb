/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, AQL_EXECUTE, AQL_EXPLAIN */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for COLLECT w/ COUNT
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
var db = require("org/arangodb").db;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function optimizerCollectMethodsTestSuite () {
  var c;

  return {
    setUp : function () {
      db._drop("UnitTestsCollection");
      c = db._create("UnitTestsCollection");

      for (var i = 0; i < 1500; ++i) {
        c.save({ group: "test" + (i % 10), value: i });
      }
    },

    tearDown : function () {
      db._drop("UnitTestsCollection");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief expect hash COLLECT
////////////////////////////////////////////////////////////////////////////////

    testHashed : function () {
      var queries = [
        [ "FOR j IN " + c.name() + " COLLECT value = j RETURN value", 1500 ],
        [ "FOR j IN " + c.name() + " COLLECT value = j._key RETURN value", 1500 ],
        [ "FOR j IN " + c.name() + " COLLECT value = j.group RETURN value", 10 ],
        [ "FOR j IN " + c.name() + " COLLECT value1 = j.group, value2 = j.value RETURN [ value1, value2 ]", 1500 ],
        [ "FOR j IN " + c.name() + " COLLECT value = j.group WITH COUNT INTO l RETURN [ value, l ]", 10 ],
        [ "FOR j IN " + c.name() + " COLLECT value1 = j.group, value2 = j.value WITH COUNT INTO l RETURN [ value1, value2, l ]", 1500 ]
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query[0]).plan;

        var aggregateNodes = 0;
        plan.nodes.map(function(node) {
          if (node.type === "AggregateNode") {
            ++aggregateNodes;
            assertEqual("hash", node.method);
          }
        });

        var results = AQL_EXECUTE(query[0]);
        assertEqual(query[1], results.json.length);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief expect hash COLLECT
////////////////////////////////////////////////////////////////////////////////

    testHashedWithNonSortedIndex : function () {
      c.ensureIndex({ type: "hash", fields: [ "group" ] }); 
      c.ensureIndex({ type: "hash", fields: [ "group", "value" ] }); 

      var queries = [
        [ "FOR j IN " + c.name() + " COLLECT value = j RETURN value", 1500 ],
        [ "FOR j IN " + c.name() + " COLLECT value = j._key RETURN value", 1500 ],
        [ "FOR j IN " + c.name() + " COLLECT value = j.group RETURN value", 10 ],
        [ "FOR j IN " + c.name() + " COLLECT value1 = j.group, value2 = j.value RETURN [ value1, value2 ]", 1500 ],
        [ "FOR j IN " + c.name() + " COLLECT value = j.group WITH COUNT INTO l RETURN [ value, l ]", 10 ],
        [ "FOR j IN " + c.name() + " COLLECT value1 = j.group, value2 = j.value WITH COUNT INTO l RETURN [ value1, value2, l ]", 1500 ]
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query[0]).plan;

        var aggregateNodes = 0;
        plan.nodes.map(function(node) {
          if (node.type === "AggregateNode") {
            ++aggregateNodes;
            assertEqual("hash", node.method);
          }
        });

        var results = AQL_EXECUTE(query[0]);
        assertEqual(query[1], results.json.length);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief expect sorted COLLECT
////////////////////////////////////////////////////////////////////////////////

    testSortedIndex : function () {
      c.ensureIndex({ type: "skiplist", fields: [ "group" ] }); 
      c.ensureIndex({ type: "skiplist", fields: [ "group", "value" ] }); 

      var queries = [
        [ "FOR j IN " + c.name() + " COLLECT value = j.group RETURN value", 10 ],
        [ "FOR j IN " + c.name() + " COLLECT value1 = j.group, value2 = j.value RETURN [ value1, value2 ]", 1500 ],
        [ "FOR j IN " + c.name() + " COLLECT value = j.group WITH COUNT INTO l RETURN [ value, l ]", 10 ],
        [ "FOR j IN " + c.name() + " COLLECT value1 = j.group, value2 = j.value WITH COUNT INTO l RETURN [ value1, value2, l ]", 1500 ]
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query[0]).plan;

        var aggregateNodes = 0;
        plan.nodes.map(function(node) {
          if (node.type === "AggregateNode") {
            ++aggregateNodes;
            assertEqual("sorted", node.method);
          }
        });

        var results = AQL_EXECUTE(query[0]);
        assertEqual(query[1], results.json.length);
      });
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(optimizerCollectMethodsTestSuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
