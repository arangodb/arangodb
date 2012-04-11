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
  var collection = null;

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
      collection = db.UnitTestsIndexes;

      if (collection.count() == 0) {
        collection.save( { "id" : 1, "name" : "fox", "age" : 19 } );
        collection.save( { "id" : 2, "name" : "brown", "age" : 25 } );
        collection.save( { "id" : 5, "name" : "peter", "age" : 12 } );
        collection.save( { "id" : 6, "name" : "hulk", "age" : 9 } );
        collection.save( { "id" : 9, "name" : "fred", "age" : 17 } );
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test const access on primary index
////////////////////////////////////////////////////////////////////////////////

    testPrimaryIndexConstAccess1 : function () {
      assertTrue(collection.count() > 0);

      var result = getQueryResults("SELECT p1 FROM " + collection.name() + " p1 WHERE p1._id == 'abc/def'");
      assertEqual(0, result.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test const access on primary index
////////////////////////////////////////////////////////////////////////////////

    testPrimaryIndexConstAccess2 : function () {
      assertTrue(collection.count() > 0);

      var id = db[collection.name()].all().next()._id;

      var result = getQueryResults("SELECT p1 FROM " + collection.name() + " p1 WHERE p1._id == '" + id + "'");
      assertEqual(1, result.length);
      assertEqual(id, result[0]._id);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test ref access on primary index
////////////////////////////////////////////////////////////////////////////////

    testPrimaryIndexRefAccess : function () {
      assertTrue(collection.count() > 0);

      var result = getQueryResults("SELECT { p1 : p1._id, p2: p2._id } FROM " + collection.name() + " p1 INNER JOIN " + collection.name() + " p2 ON (p1._id == p2._id)");
      assertEqual(collection.count(), result.length);
      for (i = 0; i < result.length; i++) {
        var row = result[i];
        assertEqual(row['p1'], row['p2']);
      }
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
