////////////////////////////////////////////////////////////////////////////////
/// @brief tests for query language, hash indexes
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

function aqlHashIndexTestSuite () {
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
      collection = db.UnitTestsHashIndex;

      if (collection.count() == 0) {
        collection.save( { "id" : 1, "name" : "fox", "age" : 19 } );
        collection.save( { "id" : 2, "name" : "brown", "age" : 25 } );
        collection.save( { "id" : 5, "name" : "peter", "age" : 12 } );
        collection.save( { "id" : 6, "name" : "hulk", "age" : 9 } );
        collection.save( { "id" : 9, "name" : "fred", "age" : 17 } );
      }

      collection.ensureHashIndex('name', 'age');
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test const access on index
////////////////////////////////////////////////////////////////////////////////

    testHashIndexConstAccess1 : function () {
      assertTrue(collection.count() > 0);

      // invalid values
      var result = getQueryResults("SELECT p1 FROM " + collection.name() + " p1 WHERE p1.name == 'jane'");
      assertEqual(0, result.length);
      
      var result = getQueryResults("SELECT p1 FROM " + collection.name() + " p1 WHERE p1.name < 'brown'");
      assertEqual(0, result.length);

      var result = getQueryResults("SELECT p1 FROM " + collection.name() + " p1 WHERE p1.name > 'peter'");
      assertEqual(0, result.length);

      var result = getQueryResults("SELECT p1 FROM " + collection.name() + " p1 WHERE p1.name == 'fox' && p1.age == 18");
      assertEqual(0, result.length);

      var result = getQueryResults("SELECT p1 FROM " + collection.name() + " p1 WHERE p1.name == 'fox' && p1.age == 20");
      assertEqual(0, result.length);
      
      var result = getQueryResults("SELECT p1 FROM " + collection.name() + " p1 WHERE p1.name == 'fox' && p1.age > 19");
      assertEqual(0, result.length);

      var result = getQueryResults("SELECT p1 FROM " + collection.name() + " p1 WHERE p1.name == 'fox' && p1.age < 19");
      assertEqual(0, result.length);
      
      var result = getQueryResults("SELECT p1 FROM " + collection.name() + " p1 WHERE p1.name == 'fox' && p1.age <= 18");
      assertEqual(0, result.length);
      
      var result = getQueryResults("SELECT p1 FROM " + collection.name() + " p1 WHERE p1.name == 'fox' && p1.age >= 20");
      assertEqual(0, result.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test const access on index
////////////////////////////////////////////////////////////////////////////////

    testHashIndexConstAccess2 : function () {
      assertTrue(collection.count() > 0);

      var result = getQueryResults("SELECT p1 FROM " + collection.name() + " p1 WHERE p1.name == 'peter'");
      assertEqual(1, result.length);
      assertEqual('peter', result[0].name);
      assertEqual(12, result[0].age);
      
      var result = getQueryResults("SELECT p1 FROM " + collection.name() + " p1 WHERE p1.name >= 'fox' ORDER BY p1.name");
      assertEqual(4, result.length);
      assertEqual('fox', result[0].name);
      assertEqual('fred', result[1].name);
      assertEqual('hulk', result[2].name);
      assertEqual('peter', result[3].name);
      
      var result = getQueryResults("SELECT p1 FROM " + collection.name() + " p1 WHERE p1.name == 'fox' && p1.age == 19");
      assertEqual(1, result.length);
      assertEqual('fox', result[0].name);
      assertEqual(19, result[0].age);
      
      var result = getQueryResults("SELECT p1 FROM " + collection.name() + " p1 WHERE p1.name == 'fox' && p1.age >= 19");
      assertEqual(1, result.length);
      assertEqual('fox', result[0].name);
      assertEqual(19, result[0].age);
      
      var result = getQueryResults("SELECT p1 FROM " + collection.name() + " p1 WHERE p1.name == 'fox' && p1.age <= 19");
      assertEqual(1, result.length);
      assertEqual('fox', result[0].name);
      assertEqual(19, result[0].age);
      
      var result = getQueryResults("SELECT p1 FROM " + collection.name() + " p1 WHERE p1.name >= 'fox' && p1.age > 12 ORDER BY p1.name");
      assertEqual(2, result.length);
      assertEqual('fox', result[0].name);
      assertEqual('fred', result[1].name);
      
      var result = getQueryResults("SELECT p1 FROM " + collection.name() + " p1 WHERE p1.name >= 'fox' && p1.age >= 17 ORDER BY p1.name");
      assertEqual(2, result.length);
      assertEqual('fox', result[0].name);
      assertEqual('fred', result[1].name);
      
      var result = getQueryResults("SELECT p1 FROM " + collection.name() + " p1 WHERE p1.name >= 'fox' && p1.age != 19 ORDER BY p1.name");
      assertEqual(3, result.length);
      assertEqual('fred', result[0].name);
      assertEqual('hulk', result[1].name);
      assertEqual('peter', result[2].name);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test ref access on index
////////////////////////////////////////////////////////////////////////////////

    testHashIndexRefAccess1 : function () {
      assertTrue(collection.count() > 0);

      var result = getQueryResults("SELECT { p1 : p1, p2: p2 } FROM " + collection.name() + " p1 INNER JOIN " + collection.name() + " p2 ON (p1.name == p2.name)");
      assertEqual(collection.count(), result.length);
      for (i = 0; i < result.length; i++) {
        var row = result[i];
        assertEqual(row['p1'].name, row['p2'].name);
        assertEqual(row['p1'].age, row['p2'].age);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test ref access on index
////////////////////////////////////////////////////////////////////////////////

    testHashIndexRefAccess2 : function () {
      assertTrue(collection.count() > 0);

      var result = getQueryResults("SELECT { p1 : p1, p2: p2 } FROM " + collection.name() + " p1 INNER JOIN " + collection.name() + " p2 ON (p1.name == p2.name && p1.age == p2.age)");
      assertEqual(collection.count(), result.length);
      for (i = 0; i < result.length; i++) {
        var row = result[i];
        assertEqual(row['p1'].name, row['p2'].name);
        assertEqual(row['p1'].age, row['p2'].age);
      }
    }

  };

}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(aqlHashIndexTestSuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
