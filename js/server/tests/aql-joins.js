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

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function aqlJoinsTestSuite () {
  var collection = null;

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

  function setUp () {
    this.persons = db.UnitTestsPersons;
    this.friends = db.UnitTestsFriends;
    this.locations = db.UnitTestsLocations;

    if (this.persons.count() == 0) {
      this.persons.save( { "id" : 1, "name" : "fox" } );
      this.persons.save( { "id" : 2, "name" : "brown" } );
      this.persons.save( { "id" : 5, "name" : "peter" } );
      this.persons.save( { "id" : 6, "name" : "hulk" } );
      this.persons.save( { "id" : 9, "name" : "fred" } );
    }
    
    if (this.friends.count() == 0) {
      this.friends.save( { "person1" : 1, "person2" : 2 } );
      this.friends.save( { "person1" : 9, "person2" : 1 } );
      this.friends.save( { "person1" : 1, "person2" : 9 } );
      this.friends.save( { "person1" : 5, "person2" : 6 } );
      this.friends.save( { "person1" : 6, "person2" : 9 } );
    }
    
    if (this.locations.count() == 0) {
      this.locations.save( { "person" : 1, "x" : 1, "y": 5 } );
      this.locations.save( { "person" : 1, "x" : 3, "y": 4 } );
      this.locations.save( { "person" : 1, "x" : -2, "y": 3 } );
      this.locations.save( { "person" : 2, "x" : 3, "y": -2 } );
      this.locations.save( { "person" : 2, "x" : 2, "y": 1 } );
      this.locations.save( { "person" : 5, "x" : 4, "y": -1 } );
      this.locations.save( { "person" : 5, "x" : 3, "y": -2 } );
      this.locations.save( { "person" : 5, "x" : 2, "y": -2 } );
      this.locations.save( { "person" : 5, "x" : 5, "y": -5 } );
      this.locations.save( { "person" : 5, "x" : 6, "y": -5 } );
    }
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

  function tearDown () {
  }

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
      if (i === "_id" || !row.hasOwnProperty(i)) {
        continue;
      }
      if (row[i] instanceof Array || row[i] instanceof Object) {
        copy[i] = this.sanitizeRow(row[i]);
      }
      else {
        copy[i] = row[i];
      }
    }
    return copy;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief execute a given query
////////////////////////////////////////////////////////////////////////////////

  function executeQuery (query) {
    var cursor = AQL_STATEMENT(db, query, undefined);
    assertFalse(cursor instanceof AvocadoQueryError);
    return cursor;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief execute a given query and return the results as an array
////////////////////////////////////////////////////////////////////////////////

  function getQueryResults (query) {
    var cursor = this.executeQuery(query);
    if (cursor) {
      var results = [ ];
      while (cursor.hasNext()) {
        results.push(this.sanitizeRow(cursor.next()));
      }
      return results;
    }
    return cursor;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief test inner join results
////////////////////////////////////////////////////////////////////////////////

  function testInnerJoin1 () {
    var expected = [{ "id" : 1, "name" : "fox"}, { "id" : 1, "name" : "fox"}, { "id" : 1, "name" : "fox"}, { "id" : 2, "name" : "brown"}, { "id" : 2, "name" : "brown"}, { "id" : 5, "name" : "peter"}, { "id" : 5, "name" : "peter"}, { "id" : 5, "name" : "peter"}, { "id" : 5, "name" : "peter"}, { "id" : 5, "name" : "peter"}];

    var result = this.getQueryResults("SELECT p FROM " + this.persons._name + " p INNER JOIN " + this.locations._name + " l ON (p.id == l.person) ORDER BY p.id, l.x, l.y");
    assertEqual(10, result.length);
    assertEqual(expected, result);
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief test inner join results
////////////////////////////////////////////////////////////////////////////////

  function testInnerJoin2 () {
    var expected = [{ "p" : { "id" : 1, "name" : "fox"}, "l" : { "person" : 1, "x" : -2, "y" : 3}}, { "p" : { "id" : 1, "name" : "fox"}, "l" : { "person" : 1, "x" : 1, "y" : 5}}, { "p" : { "id" : 1, "name" : "fox"}, "l" : { "person" : 1, "x" : 3, "y" : 4}}, { "p" : { "id" : 2, "name" : "brown"}, "l" : { "person" : 2, "x" : 2, "y" : 1}}, { "p" : { "id" : 2, "name" : "brown"}, "l" : { "person" : 2, "x" : 3, "y" : -2}}, { "p" : { "id" : 5, "name" : "peter"}, "l" : { "person" : 5, "x" : 2, "y" : -2}}, { "p" : { "id" : 5, "name" : "peter"}, "l" : { "person" : 5, "x" : 3, "y" : -2}}, { "p" : { "id" : 5, "name" : "peter"}, "l" : { "person" : 5, "x" : 4, "y" : -1}}, { "p" : { "id" : 5, "name" : "peter"}, "l" : { "person" : 5, "x" : 5, "y" : -5}}, { "p" : { "id" : 5, "name" : "peter"}, "l" : { "person" : 5, "x" : 6, "y" : -5}}];

    var result = this.getQueryResults("SELECT { p : p, l : l } FROM " + this.persons._name + " p INNER JOIN " + this.locations._name + " l ON (p.id == l.person) ORDER BY p.id, l.x, l.y");
    assertEqual(10, result.length);
    assertEqual(expected, result);
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief test inner join results with limit
////////////////////////////////////////////////////////////////////////////////

  function testInnerJoinLimit1 () {
    var expected = [{ "id" : 1, "name" : "fox"}, { "id" : 1, "name" : "fox"}, { "id" : 2, "name" : "brown"}];

    var result = this.getQueryResults("SELECT p FROM " + this.persons._name + " p INNER JOIN " + this.locations._name + " l ON (p.id == l.person) ORDER BY p.id, l.x, l.y LIMIT 1, 3");
    assertEqual(3, result.length);
    assertEqual(expected, result);
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief test left join results
////////////////////////////////////////////////////////////////////////////////

  function testLeftJoin1 () {
    var expected = [{ "id" : 1, "name" : "fox"}, { "id" : 1, "name" : "fox"}, { "id" : 1, "name" : "fox"}, { "id" : 2, "name" : "brown"}, { "id" : 2, "name" : "brown"}, { "id" : 5, "name" : "peter"}, { "id" : 5, "name" : "peter"}, { "id" : 5, "name" : "peter"}, { "id" : 5, "name" : "peter"}, { "id" : 5, "name" : "peter"}, { "id" : 6, "name" : "hulk"}, { "id" : 9, "name" : "fred"}];

    var result = this.getQueryResults("SELECT p FROM " + this.persons._name + " p LEFT JOIN " + this.locations._name + " l ON (p.id == l.person) ORDER BY p.id, l.x, l.y");
    assertEqual(12, result.length);
    assertEqual(expected, result);
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief test left join results
////////////////////////////////////////////////////////////////////////////////

  function testLeftJoin2 () {
    var expected = [{ "p" : { "id" : 1, "name" : "fox"}, "l" : { "person" : 1, "x" : -2, "y" : 3}}, { "p" : { "id" : 1, "name" : "fox"}, "l" : { "person" : 1, "x" : 1, "y" : 5}}, { "p" : { "id" : 1, "name" : "fox"}, "l" : { "person" : 1, "x" : 3, "y" : 4}}, { "p" : { "id" : 2, "name" : "brown"}, "l" : { "person" : 2, "x" : 2, "y" : 1}}, { "p" : { "id" : 2, "name" : "brown"}, "l" : { "person" : 2, "x" : 3, "y" : -2}}, { "p" : { "id" : 5, "name" : "peter"}, "l" : { "person" : 5, "x" : 2, "y" : -2}}, { "p" : { "id" : 5, "name" : "peter"}, "l" : { "person" : 5, "x" : 3, "y" : -2}}, { "p" : { "id" : 5, "name" : "peter"}, "l" : { "person" : 5, "x" : 4, "y" : -1}}, { "p" : { "id" : 5, "name" : "peter"}, "l" : { "person" : 5, "x" : 5, "y" : -5}}, { "p" : { "id" : 5, "name" : "peter"}, "l" : { "person" : 5, "x" : 6, "y" : -5}}, { "p" : { "id" : 6, "name" : "hulk"}, "l" : null}, { "p" : { "id" : 9, "name" : "fred"}, "l" : null}];

    var result = this.getQueryResults("SELECT { p : p, l : l } FROM " + this.persons._name + " p LEFT JOIN " + this.locations._name + " l ON (p.id == l.person) ORDER BY p.id, l.x, l.y");
    assertEqual(12, result.length);
    assertEqual(expected, result);
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief test list join results
////////////////////////////////////////////////////////////////////////////////

  function testListJoin1 () {
    var expected = [{ "p" : { "id" : 1, "name" : "fox"}, "l" : [{ "person" : 1, "x" : 1, "y" : 5}, { "person" : 1, "x" : 3, "y" : 4}, { "person" : 1, "x" : -2, "y" : 3}]}, { "p" : { "id" : 2, "name" : "brown"}, "l" : [{ "person" : 2, "x" : 2, "y" : 1}, { "person" : 2, "x" : 3, "y" : -2}]}, { "p" : { "id" : 5, "name" : "peter"}, "l" : [{ "person" : 5, "x" : 5, "y" : -5}, { "person" : 5, "x" : 3, "y" : -2}, { "person" : 5, "x" : 2, "y" : -2}, { "person" : 5, "x" : 6, "y" : -5}, { "person" : 5, "x" : 4, "y" : -1}]}, { "p" : { "id" : 6, "name" : "hulk"}, "l" : [ ]}, { "p" : { "id" : 9, "name" : "fred"}, "l" : [ ]}];

    var result = this.getQueryResults("SELECT { p : p, l : l } FROM " + this.persons._name + " p LIST JOIN " + this.locations._name + " l ON (p.id == l.person) ORDER BY p.id, l.x, l.y");
    assertEqual(5, result.length);
    assertEqual(expected, result);
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief test list join results
////////////////////////////////////////////////////////////////////////////////

  function testListJoin2 () {
    var expected = [{ "p" : { "id" : 1, "name" : "fox"}, "f" : [{ "person1" : 1, "person2" : 9}, { "person1" : 1, "person2" : 2}]}, { "p" : { "id" : 2, "name" : "brown"}, "f" : [ ]}, { "p" : { "id" : 5, "name" : "peter"}, "f" : [{ "person1" : 5, "person2" : 6}]}, { "p" : { "id" : 6, "name" : "hulk"}, "f" : [{ "person1" : 6, "person2" : 9}]}, { "p" : { "id" : 9, "name" : "fred"}, "f" : [{ "person1" : 9, "person2" : 1}]}];

    var result = this.getQueryResults("SELECT { p : p, f : f } FROM " + this.persons._name + " p LIST JOIN " + this.friends._name + " f ON (p.id == f.person1) ORDER BY p.id");
    assertEqual(5, result.length);
    assertEqual(expected, result);
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief test list join results
////////////////////////////////////////////////////////////////////////////////

  function testListJoin3 () {
    var expected = [{ "p" : { "id" : 1, "name" : "fox"}, "f" : [{ "person1" : 9, "person2" : 1}, { "person1" : 1, "person2" : 9}, { "person1" : 1, "person2" : 2}]}, { "p" : { "id" : 2, "name" : "brown"}, "f" : [{ "person1" : 1, "person2" : 2}]}, { "p" : { "id" : 5, "name" : "peter"}, "f" : [{ "person1" : 5, "person2" : 6}]}, { "p" : { "id" : 6, "name" : "hulk"}, "f" : [{ "person1" : 6, "person2" : 9}, { "person1" : 5, "person2" : 6}]}, { "p" : { "id" : 9, "name" : "fred"}, "f" : [{ "person1" : 9, "person2" : 1}, { "person1" : 1, "person2" : 9}, { "person1" : 6, "person2" : 9}]}];

    var result = this.getQueryResults("SELECT { p : p, f : f } FROM " + this.persons._name + " p LIST JOIN " + this.friends._name + " f ON (p.id == f.person1 || p.id == f.person2) ORDER BY p.id");
    assertEqual(5, result.length);
    assertEqual(expected, result);
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief test self join results
////////////////////////////////////////////////////////////////////////////////

  function testSelfJoin () {
    var expected = [{ "id" : 1, "name" : "fox"}, { "id" : 2, "name" : "brown"}, { "id" : 5, "name" : "peter"}, { "id" : 6, "name" : "hulk"}, { "id" : 9, "name" : "fred"}];

    var result = this.getQueryResults("SELECT p FROM " + this.persons._name + " p INNER JOIN " + this.persons._name + " p2 ON (p.id == p2.id) ORDER BY p.id");
    assertEqual(5, result.length);
    assertEqual(expected, result);
  }

}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsUnity.run(aqlJoinsTestSuite);

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
