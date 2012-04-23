////////////////////////////////////////////////////////////////////////////////
/// @brief tests for query language, joins
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

function aqlJoinsTestSuite () {
  var persons;
  var friends;
  var locations;

////////////////////////////////////////////////////////////////////////////////
/// @brief sanitize a result row, recursively
////////////////////////////////////////////////////////////////////////////////

  function sanitizeRow (row) {
    var copy;

    if (row instanceof Array) {
      copy = [ ];
    } 
    else {
      copy = { };
    }

    for (var i in row) {
      if (i === "_id" || i === "_rev" || !row.hasOwnProperty(i)) {
        continue;
      }
      if (row[i] instanceof Array || row[i] instanceof Object) {
        copy[i] = sanitizeRow(row[i]);
      }
      else {
        copy[i] = row[i];
      }
    }
    return copy;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief sort callback function for result documents
////////////////////////////////////////////////////////////////////////////////

  function sortResult(l, r) {
    var left = JSON.stringify(l);
    var right = JSON.stringify(r);

    if (left < right) {
      return -1;
    }
    if (left > right) {
      return 1;
    }
    return 0;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief normalize result set by removing _id and _rev and by sorting
////////////////////////////////////////////////////////////////////////////////

  function normalizeResult (result, sortKeys) {
    var copy = [ ];

    for (var i = 0; i < result.length; i++) {
      var row = sanitizeRow(result[i]);
     
      if (sortKeys instanceof Array) { 
        for (var key in sortKeys) {
          var sortKey = sortKeys[key];
          if (row[sortKey] instanceof Array) {
            row[sortKey] = row[sortKey].sort(sortResult);
          }
        }
      }

      copy.push(row);
    }

    return copy;
  }

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
      persons = db.UnitTestsPersons;
      friends = db.UnitTestsFriends;
      locations = db.UnitTestsLocations;

      if (persons.count() == 0) {
        persons.save( { "id" : 1, "name" : "fox" } );
        persons.save( { "id" : 2, "name" : "brown" } );
        persons.save( { "id" : 5, "name" : "peter" } );
        persons.save( { "id" : 6, "name" : "hulk" } );
        persons.save( { "id" : 9, "name" : "fred" } );
      }
      
      if (friends.count() == 0) {
        friends.save( { "person1" : 1, "person2" : 2 } );
        friends.save( { "person1" : 9, "person2" : 1 } );
        friends.save( { "person1" : 1, "person2" : 9 } );
        friends.save( { "person1" : 5, "person2" : 6 } );
        friends.save( { "person1" : 6, "person2" : 9 } );
      }
      
      if (locations.count() == 0) {
        locations.save( { "person" : 1, "x" : 1, "y": 5 } );
        locations.save( { "person" : 1, "x" : 3, "y": 4 } );
        locations.save( { "person" : 1, "x" : -2, "y": 3 } );
        locations.save( { "person" : 2, "x" : 3, "y": -2 } );
        locations.save( { "person" : 2, "x" : 2, "y": 1 } );
        locations.save( { "person" : 5, "x" : 4, "y": -1 } );
        locations.save( { "person" : 5, "x" : 3, "y": -2 } );
        locations.save( { "person" : 5, "x" : 2, "y": -2 } );
        locations.save( { "person" : 5, "x" : 5, "y": -5 } );
        locations.save( { "person" : 5, "x" : 6, "y": -5 } );
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

    testInnerJoin1 : function () {
      var expected = [{ "id" : 1, "name" : "fox"}, { "id" : 1, "name" : "fox"}, { "id" : 1, "name" : "fox"}, { "id" : 2, "name" : "brown"}, { "id" : 2, "name" : "brown"}, { "id" : 5, "name" : "peter"}, { "id" : 5, "name" : "peter"}, { "id" : 5, "name" : "peter"}, { "id" : 5, "name" : "peter"}, { "id" : 5, "name" : "peter"}];

      var result = getQueryResults("SELECT p FROM " + persons.name() + " p INNER JOIN " + locations.name() + " l ON (p.id == l.person) ORDER BY p.id, l.x, l.y");
      result = normalizeResult(result);
      assertEqual(10, result.length);
      assertEqual(expected, result);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test inner join results
////////////////////////////////////////////////////////////////////////////////

    testInnerJoin2 : function () {
      var expected = [{ "p" : { "id" : 1, "name" : "fox"}, "l" : { "person" : 1, "x" : -2, "y" : 3}}, { "p" : { "id" : 1, "name" : "fox"}, "l" : { "person" : 1, "x" : 1, "y" : 5}}, { "p" : { "id" : 1, "name" : "fox"}, "l" : { "person" : 1, "x" : 3, "y" : 4}}, { "p" : { "id" : 2, "name" : "brown"}, "l" : { "person" : 2, "x" : 2, "y" : 1}}, { "p" : { "id" : 2, "name" : "brown"}, "l" : { "person" : 2, "x" : 3, "y" : -2}}, { "p" : { "id" : 5, "name" : "peter"}, "l" : { "person" : 5, "x" : 2, "y" : -2}}, { "p" : { "id" : 5, "name" : "peter"}, "l" : { "person" : 5, "x" : 3, "y" : -2}}, { "p" : { "id" : 5, "name" : "peter"}, "l" : { "person" : 5, "x" : 4, "y" : -1}}, { "p" : { "id" : 5, "name" : "peter"}, "l" : { "person" : 5, "x" : 5, "y" : -5}}, { "p" : { "id" : 5, "name" : "peter"}, "l" : { "person" : 5, "x" : 6, "y" : -5}}];

      var result = getQueryResults("SELECT { p : p, l : l } FROM " + persons.name() + " p INNER JOIN " + locations.name() + " l ON (p.id == l.person) ORDER BY p.id, l.x, l.y");
      result = normalizeResult(result);
      assertEqual(10, result.length);
      assertEqual(expected, result);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test inner join results with limit
////////////////////////////////////////////////////////////////////////////////

    testInnerJoinLimit1 : function () {
      var expected = [{ "id" : 1, "name" : "fox"}, { "id" : 1, "name" : "fox"}, { "id" : 2, "name" : "brown"}];

      var result = getQueryResults("SELECT p FROM " + persons.name() + " p INNER JOIN " + locations.name() + " l ON (p.id == l.person) ORDER BY p.id, l.x, l.y LIMIT 1, 3");
      result = normalizeResult(result);
      assertEqual(3, result.length);
      assertEqual(expected, result);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test left join results
////////////////////////////////////////////////////////////////////////////////

    testLeftJoin1 : function () {
      var expected = [{ "id" : 1, "name" : "fox"}, { "id" : 1, "name" : "fox"}, { "id" : 1, "name" : "fox"}, { "id" : 2, "name" : "brown"}, { "id" : 2, "name" : "brown"}, { "id" : 5, "name" : "peter"}, { "id" : 5, "name" : "peter"}, { "id" : 5, "name" : "peter"}, { "id" : 5, "name" : "peter"}, { "id" : 5, "name" : "peter"}, { "id" : 6, "name" : "hulk"}, { "id" : 9, "name" : "fred"}];

      var result = getQueryResults("SELECT p FROM " + persons.name() + " p LEFT JOIN " + locations.name() + " l ON (p.id == l.person) ORDER BY p.id, l.x, l.y");
      result = normalizeResult(result);
      assertEqual(12, result.length);
      assertEqual(expected, result);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test left join results
////////////////////////////////////////////////////////////////////////////////

    testLeftJoin2 : function () {
      var expected = [{ "p" : { "id" : 1, "name" : "fox"}, "l" : { "person" : 1, "x" : -2, "y" : 3}}, { "p" : { "id" : 1, "name" : "fox"}, "l" : { "person" : 1, "x" : 1, "y" : 5}}, { "p" : { "id" : 1, "name" : "fox"}, "l" : { "person" : 1, "x" : 3, "y" : 4}}, { "p" : { "id" : 2, "name" : "brown"}, "l" : { "person" : 2, "x" : 2, "y" : 1}}, { "p" : { "id" : 2, "name" : "brown"}, "l" : { "person" : 2, "x" : 3, "y" : -2}}, { "p" : { "id" : 5, "name" : "peter"}, "l" : { "person" : 5, "x" : 2, "y" : -2}}, { "p" : { "id" : 5, "name" : "peter"}, "l" : { "person" : 5, "x" : 3, "y" : -2}}, { "p" : { "id" : 5, "name" : "peter"}, "l" : { "person" : 5, "x" : 4, "y" : -1}}, { "p" : { "id" : 5, "name" : "peter"}, "l" : { "person" : 5, "x" : 5, "y" : -5}}, { "p" : { "id" : 5, "name" : "peter"}, "l" : { "person" : 5, "x" : 6, "y" : -5}}, { "p" : { "id" : 6, "name" : "hulk"}, "l" : null}, { "p" : { "id" : 9, "name" : "fred"}, "l" : null}];

      var result = getQueryResults("SELECT { p : p, l : l } FROM " + persons.name() + " p LEFT JOIN " + locations.name() + " l ON (p.id == l.person) ORDER BY p.id, l.x, l.y");
      result = normalizeResult(result);
      assertEqual(12, result.length);
      assertEqual(expected, result);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test list join results
////////////////////////////////////////////////////////////////////////////////

    testListJoin1 : function () {
      var expected = [{ "p" : { "id" : 1, "name" : "fox"}, "l" : [{ "person" : 1, "x" : 1, "y" : 5}, { "person" : 1, "x" : 3, "y" : 4}, { "person" : 1, "x" : -2, "y" : 3}]}, { "p" : { "id" : 2, "name" : "brown"}, "l" : [{ "person" : 2, "x" : 2, "y" : 1}, { "person" : 2, "x" : 3, "y" : -2}]}, { "p" : { "id" : 5, "name" : "peter"}, "l" : [{ "person" : 5, "x" : 5, "y" : -5}, { "person" : 5, "x" : 3, "y" : -2}, { "person" : 5, "x" : 2, "y" : -2}, { "person" : 5, "x" : 6, "y" : -5}, { "person" : 5, "x" : 4, "y" : -1}]}, { "p" : { "id" : 6, "name" : "hulk"}, "l" : [ ]}, { "p" : { "id" : 9, "name" : "fred"}, "l" : [ ]}];
      expected = normalizeResult(expected, [ 'l' ]);

      var result = getQueryResults("SELECT { p : p, l : l } FROM " + persons.name() + " p LIST JOIN " + locations.name() + " l ON (p.id == l.person) ORDER BY p.id, l.x, l.y");
      result = normalizeResult(result, [ 'l' ]);
      assertEqual(5, result.length);
      assertEqual(expected, result);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test list join results
////////////////////////////////////////////////////////////////////////////////

    testListJoin2 : function () {
      var expected = [{ "p" : { "id" : 1, "name" : "fox"}, "f" : [{ "person1" : 1, "person2" : 9}, { "person1" : 1, "person2" : 2}]}, { "p" : { "id" : 2, "name" : "brown"}, "f" : [ ]}, { "p" : { "id" : 5, "name" : "peter"}, "f" : [{ "person1" : 5, "person2" : 6}]}, { "p" : { "id" : 6, "name" : "hulk"}, "f" : [{ "person1" : 6, "person2" : 9}]}, { "p" : { "id" : 9, "name" : "fred"}, "f" : [{ "person1" : 9, "person2" : 1}]}];
      expected = normalizeResult(expected, [ 'f' ]);

      var result = getQueryResults("SELECT { p : p, f : f } FROM " + persons.name() + " p LIST JOIN " + friends.name() + " f ON (p.id == f.person1) ORDER BY p.id");
      result = normalizeResult(result, [ 'f' ]);
      assertEqual(5, result.length);
      assertEqual(expected, result);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test list join results
////////////////////////////////////////////////////////////////////////////////

    testListJoin3 : function () {
      var expected = [{ "p" : { "id" : 1, "name" : "fox"}, "f" : [{ "person1" : 9, "person2" : 1}, { "person1" : 1, "person2" : 9}, { "person1" : 1, "person2" : 2}]}, { "p" : { "id" : 2, "name" : "brown"}, "f" : [{ "person1" : 1, "person2" : 2}]}, { "p" : { "id" : 5, "name" : "peter"}, "f" : [{ "person1" : 5, "person2" : 6}]}, { "p" : { "id" : 6, "name" : "hulk"}, "f" : [{ "person1" : 6, "person2" : 9}, { "person1" : 5, "person2" : 6}]}, { "p" : { "id" : 9, "name" : "fred"}, "f" : [{ "person1" : 9, "person2" : 1}, { "person1" : 1, "person2" : 9}, { "person1" : 6, "person2" : 9}]}];
      expected = normalizeResult(expected, [ 'f' ]);

      var result = getQueryResults("SELECT { p : p, f : f } FROM " + persons.name() + " p LIST JOIN " + friends.name() + " f ON (p.id == f.person1 || p.id == f.person2) ORDER BY p.id");
      result = normalizeResult(result, [ 'f' ]);
      assertEqual(5, result.length);
      assertEqual(expected, result);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test self join results
////////////////////////////////////////////////////////////////////////////////

    testSelfJoin : function () {
      var expected = [{ "id" : 1, "name" : "fox"}, { "id" : 2, "name" : "brown"}, { "id" : 5, "name" : "peter"}, { "id" : 6, "name" : "hulk"}, { "id" : 9, "name" : "fred"}];
      expected = normalizeResult(expected);

      var result = getQueryResults("SELECT p FROM " + persons.name() + " p INNER JOIN " + persons.name() + " p2 ON (p.id == p2.id) ORDER BY p.id");
      result = normalizeResult(result);
      assertEqual(5, result.length);
      assertEqual(expected, result);
    }

  };

}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(aqlJoinsTestSuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
