////////////////////////////////////////////////////////////////////////////////
/// @brief tests for query language, indexes
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

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function aqlIndexesTestSuite () {
  var indexes = null;

////////////////////////////////////////////////////////////////////////////////
/// @brief execute a given query
////////////////////////////////////////////////////////////////////////////////

  function executeQuery (query) {
    var cursor = AQL_STATEMENT(query, undefined);
    assertFalse(cursor instanceof AvocadoQueryError);
    return cursor;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief execute a given query and return the results as an array
////////////////////////////////////////////////////////////////////////////////

  function getQueryResults (query) {
    var cursor = executeQuery(query);
    if (cursor) {
      var results = [ ];
      while (cursor.hasNext()) {
        results.push(cursor.next());
      }
      return results;
    }
    return cursor;
  }

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      indexes = db.UnitTestsIndexes;

      if (indexes.count() == 0) {
        indexes.save( { "id" : 1, "name" : "fox", "age" : 19 } );
        indexes.save( { "id" : 2, "name" : "brown", "age" : 25 } );
        indexes.save( { "id" : 5, "name" : "peter", "age" : 12 } );
        indexes.save( { "id" : 6, "name" : "hulk", "age" : 9 } );
        indexes.save( { "id" : 9, "name" : "fred", "age" : 17 } );
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test inner join results
////////////////////////////////////////////////////////////////////////////////

    testPrimaryIndexJoin : function () {
      assertTrue(indexes.count() > 0);

//      var result = getQueryResults("SELECT p1 FROM " + collection.name() + " p1 INNER JOIN " + collection.name() + " p2 ON (p1._id == p2._id)");
//      assertEqual(collection.count(), result.length);
    }

  };

}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(aqlIndexesTestSuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
