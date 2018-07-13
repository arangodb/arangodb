/*jshint globalstrict:false, strict:false, maxlen: 850 */
/*global assertEqual */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for query language, simple collection-based queries
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
var internal = require("internal");
var helper = require("@arangodb/aql-helper");
var getQueryResults = helper.getQueryResults;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ahuacatlQueryCollectionTestSuite () {
  var users = null;
  var relations = null;

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      internal.db._drop("UnitTestsAhuacatlUsers");
      internal.db._drop("UnitTestsAhuacatlUserRelations");

      users = internal.db._create("UnitTestsAhuacatlUsers");
      relations = internal.db._create("UnitTestsAhuacatlUserRelations");

      users.save({ "id" : 100, "name" : "John", "age" : 37, "active" : true, "gender" : "m" });
      users.save({ "id" : 101, "name" : "Fred", "age" : 36, "active" : true, "gender" : "m" });
      users.save({ "id" : 102, "name" : "Jacob", "age" : 35, "active" : false, "gender" : "m" });
      users.save({ "id" : 103, "name" : "Ethan", "age" : 34, "active" : false, "gender" : "m" });
      users.save({ "id" : 104, "name" : "Michael", "age" : 33, "active" : true, "gender" : "m" });
      users.save({ "id" : 105, "name" : "Alexander", "age" : 32, "active" : true, "gender" : "m" });
      users.save({ "id" : 106, "name" : "Daniel", "age" : 31, "active" : true, "gender" : "m" });
      users.save({ "id" : 107, "name" : "Anthony", "age" : 30, "active" : true, "gender" : "m" });
      users.save({ "id" : 108, "name" : "Jim", "age" : 29, "active" : true, "gender" : "m" });
      users.save({ "id" : 109, "name" : "Diego", "age" : 28, "active" : true, "gender" : "m" });

      users.save({ "id" : 200, "name" : "Sophia", "age" : 37, "active" : true, "gender" : "f" });
      users.save({ "id" : 201, "name" : "Emma", "age" : 36,  "active" : true, "gender" : "f" });
      users.save({ "id" : 202, "name" : "Olivia", "age" : 35, "active" : false, "gender" : "f" });
      users.save({ "id" : 203, "name" : "Madison", "age" : 34, "active" : true, "gender": "f" });
      users.save({ "id" : 204, "name" : "Chloe", "age" : 33, "active" : true, "gender" : "f" });
      users.save({ "id" : 205, "name" : "Eva", "age" : 32, "active" : false, "gender" : "f" });
      users.save({ "id" : 206, "name" : "Abigail", "age" : 31, "active" : true, "gender" : "f" });
      users.save({ "id" : 207, "name" : "Isabella", "age" : 30, "active" : true, "gender" : "f" });
      users.save({ "id" : 208, "name" : "Mary", "age" : 29, "active" : true, "gender" : "f" });
      users.save({ "id" : 209, "name" : "Mariah", "age" : 28, "active" : true, "gender" : "f" });

      relations.save({ "from" : 209, "to" : 205, "type" : "friend" });
      relations.save({ "from" : 206, "to" : 108, "type" : "friend" });
      relations.save({ "from" : 202, "to" : 204, "type" : "friend" });
      relations.save({ "from" : 200, "to" : 100, "type" : "friend" });
      relations.save({ "from" : 205, "to" : 101, "type" : "friend" });
      relations.save({ "from" : 209, "to" : 203, "type" : "friend" });
      relations.save({ "from" : 200, "to" : 203, "type" : "friend" });
      relations.save({ "from" : 100, "to" : 208, "type" : "friend" });
      relations.save({ "from" : 101, "to" : 209, "type" : "friend" });
      relations.save({ "from" : 206, "to" : 102, "type" : "friend" });
      relations.save({ "from" : 104, "to" : 100, "type" : "friend" });
      relations.save({ "from" : 104, "to" : 108, "type" : "friend" });
      relations.save({ "from" : 108, "to" : 209, "type" : "friend" });
      relations.save({ "from" : 206, "to" : 106, "type" : "friend" });
      relations.save({ "from" : 204, "to" : 105, "type" : "friend" });
      relations.save({ "from" : 208, "to" : 207, "type" : "friend" });
      relations.save({ "from" : 102, "to" : 108, "type" : "friend" });
      relations.save({ "from" : 207, "to" : 203, "type" : "friend" });
      relations.save({ "from" : 203, "to" : 106, "type" : "friend" });
      relations.save({ "from" : 202, "to" : 108, "type" : "friend" });
      relations.save({ "from" : 201, "to" : 203, "type" : "friend" });
      relations.save({ "from" : 105, "to" : 100, "type" : "friend" });
      relations.save({ "from" : 100, "to" : 109, "type" : "friend" });
      relations.save({ "from" : 207, "to" : 109, "type" : "friend" });
      relations.save({ "from" : 103, "to" : 203, "type" : "friend" });
      relations.save({ "from" : 208, "to" : 104, "type" : "friend" });
      relations.save({ "from" : 105, "to" : 104, "type" : "friend" });
      relations.save({ "from" : 103, "to" : 208, "type" : "friend" });
      relations.save({ "from" : 203, "to" : 107, "type" : "boyfriend" });
      relations.save({ "from" : 107, "to" : 203, "type" : "girlfriend" });
      relations.save({ "from" : 208, "to" : 109, "type" : "boyfriend" });
      relations.save({ "from" : 109, "to" : 208, "type" : "girlfriend" });
      relations.save({ "from" : 106, "to" : 205, "type" : "girlfriend" });
      relations.save({ "from" : 205, "to" : 106, "type" : "boyfriend" });
      relations.save({ "from" : 103, "to" : 209, "type" : "girlfriend" });
      relations.save({ "from" : 209, "to" : 103, "type" : "boyfriend" });
      relations.save({ "from" : 201, "to" : 102, "type" : "boyfriend" });
      relations.save({ "from" : 102, "to" : 201, "type" : "girlfriend" });
      relations.save({ "from" : 206, "to" : 100, "type" : "boyfriend" });
      relations.save({ "from" : 100, "to" : 206, "type" : "girlfriend" });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      internal.db._drop("UnitTestsAhuacatlUsers");
      internal.db._drop("UnitTestsAhuacatlUserRelations");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief return a single value as part of a document
////////////////////////////////////////////////////////////////////////////////

    testFilterEmpty : function () {
      var expected = [ ];
      var actual = getQueryResults("FOR u in " + users.name() + " FILTER u.id < 0 SORT u.id RETURN { \"name\" : u.name }");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief return a single value as part of a document
////////////////////////////////////////////////////////////////////////////////

    testFilterSingleReturnDoc : function () {
      var expected = [ { "name" : "John" }, { "name" : "Fred" }, { "name" : "Jacob" }, { "name" : "Ethan" }, { "name" : "Michael" }, { "name" : "Alexander" }, { "name" : "Daniel" }, { "name" : "Anthony" }, { "name" : "Jim"} , { "name" : "Diego" } ];
      var actual = getQueryResults("FOR u in " + users.name() + " FILTER u.gender == \"m\" SORT u.id RETURN { \"name\" : u.name }");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief return a single value, standalone
////////////////////////////////////////////////////////////////////////////////

    testFilterSingleReturnNameAsc : function () {
      var expected = [ "John", "Fred", "Jacob", "Ethan", "Michael", "Alexander", "Daniel", "Anthony", "Jim", "Diego" ];

      var actual = getQueryResults("FOR u in " + users.name() + " FILTER u.gender == \"m\" SORT u.id RETURN u.name");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief return another single value, standalone
////////////////////////////////////////////////////////////////////////////////
    
    testFilterSingleReturnNameDesc : function () {
      var expected = [ "Mariah", "Mary", "Isabella", "Abigail", "Eva", "Chloe", "Madison", "Olivia", "Emma", "Sophia" ];

      var actual = getQueryResults("FOR u in " + users.name() + " FILTER u.gender == \"f\" SORT u.id DESC RETURN u.name");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief return two values as part of a document
////////////////////////////////////////////////////////////////////////////////
    
    testFilterSingleReturnNameAge : function () {
      var expected = [{ "age" : 28, "name" : "Mariah" }, { "age" : 29, "name" : "Mary" }, { "age" : 30, "name" : "Isabella" }, { "age" : 31, "name" : "Abigail" }, { "age" : 32, "name" : "Eva" }, { "age": 33, "name" : "Chloe" }, { "age" : 34, "name" : "Madison" }, { "age" : 35, "name" : "Olivia" }, { "age" : 36, "name" : "Emma" }, { "age" : 37, "name" : "Sophia" }];

      var actual = getQueryResults("FOR u in " + users.name() + " FILTER u.gender == \"f\" SORT u.id DESC RETURN { \"name\" : u.name, \"age\" : u.age }");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief return complete documents
////////////////////////////////////////////////////////////////////////////////
    
    testFilterSingleReturnNameAgeActive : function () {
      var expected = [{ "active" : true, "age" : 28, "gender" : "f", "id" : 209, "name" : "Mariah" }, { "active" : true, "age" : 29, "gender" : "f", "id" : 208, "name" : "Mary" }, { "active" : true, "age" : 30, "gender" : "f", "id" : 207, "name" : "Isabella" }, { "active" : true, "age" : 31, "gender" : "f", "id" : 206, "name" : "Abigail" }, { "active" : false, "age" : 32, "gender" : "f", "id" : 205, "name" : "Eva" }, { "active" : true, "age" : 33, "gender" : "f", "id" : 204, "name" : "Chloe" }, { "active" : true, "age" : 34, "gender" : "f", "id" : 203, "name" : "Madison" }, { "active" : false, "age" : 35, "gender" : "f", "id" : 202, "name" : "Olivia" }, { "active" : true, "age" : 36, "gender" : "f", "id" : 201, "name" : "Emma" }, { "active" : true, "age" : 37, "gender" : "f", "id" : 200, "name" : "Sophia" }];

      var actual = getQueryResults("FOR u in " + users.name() + " FILTER u.gender == \"f\" SORT u.id DESC RETURN u");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief filter by multiple values
////////////////////////////////////////////////////////////////////////////////
    
    testFilterMultipleReturnDocument : function () {
      var expected = [ { "active" : true, "age" : 37, "gender" : "m", "id" : 100, "name" : "John" }, { "active" : true, "age" : 36, "gender" : "m", "id" : 101, "name" : "Fred" }, { "active" : true, "age" : 37, "gender" : "f", "id" : 200, "name" : "Sophia" }, { "active" : true, "age" : 36, "gender" : "f", "id" : 201, "name" : "Emma" }, { "active" : true, "age" : 34, "gender" : "f", "id" : 203, "name" : "Madison" } ];

      var actual = getQueryResults("FOR u in " + users.name() + " FILTER u.age >= 34 && u.active == true SORT u.id ASC RETURN u");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief filter individual elements
////////////////////////////////////////////////////////////////////////////////
    
    testFilterIndividualReturnDocument : function () {
      var actual;

      actual = getQueryResults("FOR u in " + users.name() + " FILTER u.id == 200 RETURN u");
      assertEqual([ { "active" : true, "age" : 37, "gender" : "f", "id" : 200, "name" : "Sophia" } ], actual);
      
      actual = getQueryResults("FOR u in " + users.name() + " FILTER u.active == true && u.age == 35 RETURN u");
      assertEqual([ ], actual); 
     
      actual = getQueryResults("FOR u in " + users.name() + " FILTER u.active == true && u.age == 34 RETURN u");
      assertEqual([ { "active" : true, "age" : 34, "gender" : "f", "id" : 203, "name" : "Madison" } ], actual);
      
      actual = getQueryResults("FOR u in " + users.name() + " FILTER u.active != true && u.age == 34 RETURN u");
      assertEqual([ { "active" : false, "age" : 34, "gender" : "m", "id" : 103, "name" : "Ethan" } ], actual);
      
      actual = getQueryResults("FOR u in " + users.name() + " FILTER u.name == \"Mariah\" RETURN u");
      assertEqual([ { "active" : true, "age" : 28, "gender" : "f", "id" : 209, "name" : "Mariah" } ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief limit 0,x
////////////////////////////////////////////////////////////////////////////////
    
    testLimitZeroLong : function () {
      var actual;

      // limit 0,x
      actual = getQueryResults("FOR u in " + users.name() + " SORT u.id DESC LIMIT 0,0 RETURN u");
      assertEqual([ ], actual);

      actual = getQueryResults("FOR u in " + users.name() + " SORT u.id DESC LIMIT 0,1 RETURN u");
      assertEqual([ { "active" : true, "age" : 28, "gender" : "f", "id" : 209, "name" : "Mariah" } ], actual);
      
      actual = getQueryResults("FOR u in " + users.name() + " SORT u.id DESC LIMIT 0,2 RETURN u");
      assertEqual([ { "active" : true, "age" : 28, "gender" : "f", "id" : 209, "name" : "Mariah" }, { "active" : true, "age" : 29, "gender" : "f", "id" : 208, "name" : "Mary" } ], actual);
      
      actual = getQueryResults("FOR u in " + users.name() + " SORT u.id DESC LIMIT 0,3 RETURN u");
      assertEqual([ { "active" : true, "age" : 28, "gender" : "f", "id" : 209, "name" : "Mariah" }, { "active" : true, "age" : 29, "gender" : "f", "id" : 208, "name" : "Mary" }, { "active" : true, "age" : 30, "gender" : "f", "id" : 207, "name" : "Isabella" } ], actual);
      
      actual = getQueryResults("FOR u in " + users.name() + " SORT u.id DESC LIMIT 0,4 RETURN u");
      assertEqual([ { "active" : true, "age" : 28, "gender" : "f", "id" : 209, "name" : "Mariah" }, { "active" : true, "age" : 29, "gender" : "f", "id" : 208, "name" : "Mary" }, { "active" : true, "age" : 30, "gender" : "f", "id" : 207, "name" : "Isabella" }, { "active" : true, "age" : 31, "gender" : "f", "id" : 206, "name" : "Abigail" } ], actual);
    },
     
////////////////////////////////////////////////////////////////////////////////
/// @brief limit x
////////////////////////////////////////////////////////////////////////////////
    
    testLimitZeroShort : function () {
      var actual;

      // limit x (same as 0,x)
      actual = getQueryResults("FOR u in " + users.name() + " SORT u.id DESC LIMIT 0 RETURN u");
      assertEqual([ ], actual);

      actual = getQueryResults("FOR u in " + users.name() + " SORT u.id DESC LIMIT 1 RETURN u");
      assertEqual([ { "active" : true, "age" : 28, "gender" : "f", "id" : 209, "name" : "Mariah" } ], actual);
      
      actual = getQueryResults("FOR u in " + users.name() + " SORT u.id DESC LIMIT 2 RETURN u");
      assertEqual([ { "active" : true, "age" : 28, "gender" : "f", "id" : 209, "name" : "Mariah" }, { "active" : true, "age" : 29, "gender" : "f", "id" : 208, "name" : "Mary" } ], actual);
      
      actual = getQueryResults("FOR u in " + users.name() + " SORT u.id DESC LIMIT 3 RETURN u");
      assertEqual([ { "active" : true, "age" : 28, "gender" : "f", "id" : 209, "name" : "Mariah" }, { "active" : true, "age" : 29, "gender" : "f", "id" : 208, "name" : "Mary" }, { "active" : true, "age" : 30, "gender" : "f", "id" : 207, "name" : "Isabella" } ], actual);
      
      actual = getQueryResults("FOR u in " + users.name() + " SORT u.id DESC LIMIT 4 RETURN u");
      assertEqual([ { "active" : true, "age" : 28, "gender" : "f", "id" : 209, "name" : "Mariah" }, { "active" : true, "age" : 29, "gender" : "f", "id" : 208, "name" : "Mary" }, { "active" : true, "age" : 30, "gender" : "f", "id" : 207, "name" : "Isabella" }, { "active" : true, "age" : 31, "gender" : "f", "id" : 206, "name" : "Abigail" } ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief limit 1,x
////////////////////////////////////////////////////////////////////////////////
    
    testLimitOne : function () {
      var actual;

      // limit 1,x
      actual = getQueryResults("FOR u in " + users.name() + " SORT u.id DESC LIMIT 1,1 RETURN u");
      assertEqual([ { "active" : true, "age" : 29, "gender" : "f", "id" : 208, "name" : "Mary" } ], actual);
      
      actual = getQueryResults("FOR u in " + users.name() + " SORT u.id DESC LIMIT 1,2 RETURN u");
      assertEqual([ { "active" : true, "age" : 29, "gender" : "f", "id" : 208, "name" : "Mary" }, { "active" : true, "age" : 30, "gender" : "f", "id" : 207, "name" : "Isabella" } ], actual);
      
      actual = getQueryResults("FOR u in " + users.name() + " SORT u.id DESC LIMIT 1,3 RETURN u");
      assertEqual([ { "active" : true, "age" : 29, "gender" : "f", "id" : 208, "name" : "Mary" }, { "active" : true, "age" : 30, "gender" : "f", "id" : 207, "name" : "Isabella" }, { "active" : true, "age" : 31, "gender" : "f", "id" : 206, "name" : "Abigail" } ], actual);
      
      actual = getQueryResults("FOR u in " + users.name() + " SORT u.id DESC LIMIT 1,4 RETURN u");
      assertEqual([ { "active" : true, "age" : 29, "gender" : "f", "id" : 208, "name" : "Mary" }, { "active" : true, "age" : 30, "gender" : "f", "id" : 207, "name" : "Isabella" }, { "active" : true, "age" : 31, "gender" : "f", "id" : 206, "name" : "Abigail" }, { "active" : false, "age" : 32, "gender" : "f", "id" : 205, "name" : "Eva" } ], actual);
    },
    
////////////////////////////////////////////////////////////////////////////////
/// @brief limit 2,x
////////////////////////////////////////////////////////////////////////////////
    
    testLimitTwo : function () {
      var actual;
     
      // limit 2,x
      actual = getQueryResults("FOR u in " + users.name() + " SORT u.id DESC LIMIT 2,1 RETURN u");
      assertEqual([ { "active" : true, "age" : 30, "gender" : "f", "id" : 207, "name" : "Isabella" } ], actual);
      
      actual = getQueryResults("FOR u in " + users.name() + " SORT u.id DESC LIMIT 2,2 RETURN u");
      assertEqual([ { "active" : true, "age" : 30, "gender" : "f", "id" : 207, "name" : "Isabella" }, { "active" : true, "age" : 31, "gender" : "f", "id" : 206, "name" : "Abigail" } ], actual);

      actual = getQueryResults("FOR u in " + users.name() + " SORT u.id DESC LIMIT 2,3 RETURN u");
      assertEqual([ { "active" : true, "age" : 30, "gender" : "f", "id" : 207, "name" : "Isabella" }, { "active" : true, "age" : 31, "gender" : "f", "id" : 206, "name" : "Abigail" }, { "active" : false, "age" : 32, "gender" : "f", "id" : 205, "name" : "Eva" } ], actual);
      
      actual = getQueryResults("FOR u in " + users.name() + " SORT u.id DESC LIMIT 2,4 RETURN u");
      assertEqual([ { "active" : true, "age" : 30, "gender" : "f", "id" : 207, "name" : "Isabella" }, { "active" : true, "age" : 31, "gender" : "f", "id" : 206, "name" : "Abigail" }, { "active" : false, "age" : 32, "gender" : "f", "id" : 205, "name" : "Eva" }, { "active" : true, "age" : 33, "gender" : "f", "id" : 204, "name" : "Chloe" } ], actual);
    },
      
////////////////////////////////////////////////////////////////////////////////
/// @brief other limits
////////////////////////////////////////////////////////////////////////////////
    
    testLimitOther : function () {
      var actual;
     
      // limit x,x
      actual = getQueryResults("FOR u in " + users.name() + " SORT u.id DESC LIMIT 3,1 RETURN u");
      assertEqual([ { "active" : true, "age" : 31, "gender" : "f", "id" : 206, "name" : "Abigail" } ], actual);
      
      actual = getQueryResults("FOR u in " + users.name() + " SORT u.id DESC LIMIT 4,1 RETURN u");
      assertEqual([ { "active" : false, "age" : 32, "gender" : "f", "id" : 205, "name" : "Eva" } ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief limits at end
////////////////////////////////////////////////////////////////////////////////
    
    testLimitAtEnd : function () {
      var actual;
      
      // limit end - 2,x
      actual = getQueryResults("FOR u in " + users.name() + " SORT u.id DESC LIMIT 17,1 RETURN u");
      assertEqual([ { "active" : false, "age" : 35, "gender" : "m", "id" : 102, "name" : "Jacob" } ], actual);
     
      actual = getQueryResults("FOR u in " + users.name() + " SORT u.id DESC LIMIT 17,2 RETURN u");
      assertEqual([ { "active" : false, "age" : 35, "gender" : "m", "id" : 102, "name" : "Jacob" }, { "active" : true, "age" : 36, "gender" : "m", "id" : 101, "name" : "Fred" } ], actual);
      
      actual = getQueryResults("FOR u in " + users.name() + " SORT u.id DESC LIMIT 17,3 RETURN u");
      assertEqual([ { "active" : false, "age" : 35, "gender" : "m", "id" : 102, "name" : "Jacob" }, { "active" : true, "age" : 36, "gender" : "m", "id" : 101, "name" : "Fred" }, { "active" : true, "age" : 37, "gender" : "m", "id" : 100, "name" : "John" } ], actual);

      actual = getQueryResults("FOR u in " + users.name() + " SORT u.id DESC LIMIT 17,4 RETURN u");
      assertEqual([ { "active" : false, "age" : 35, "gender" : "m", "id" : 102, "name" : "Jacob" }, { "active" : true, "age" : 36, "gender" : "m", "id" : 101, "name" : "Fred" }, { "active" : true, "age" : 37, "gender" : "m", "id" : 100, "name" : "John" } ], actual);
      
      // limit end - 1,x
      actual = getQueryResults("FOR u in " + users.name() + " SORT u.id DESC LIMIT 18,1 RETURN u");
      assertEqual([ { "active" : true, "age" : 36, "gender" : "m", "id" : 101, "name" : "Fred" } ], actual);

      actual = getQueryResults("FOR u in " + users.name() + " SORT u.id DESC LIMIT 18,2 RETURN u");
      assertEqual([ { "active" : true, "age" : 36, "gender" : "m", "id" : 101, "name" : "Fred" }, { "active" : true, "age" : 37, "gender" : "m", "id" : 100, "name" : "John" } ], actual);
           
      actual = getQueryResults("FOR u in " + users.name() + " SORT u.id DESC LIMIT 18,3 RETURN u");
      assertEqual([ { "active" : true, "age" : 36, "gender" : "m", "id" : 101, "name" : "Fred" }, { "active" : true, "age" : 37, "gender" : "m", "id" : 100, "name" : "John" } ], actual);

      // limit end,x
      actual = getQueryResults("FOR u in " + users.name() + " SORT u.id DESC LIMIT 19,1 RETURN u");
      assertEqual([ { "active" : true, "age" : 37, "gender" : "m", "id" : 100, "name" : "John" } ], actual);
      
      actual = getQueryResults("FOR u in " + users.name() + " SORT u.id DESC LIMIT 19,2 RETURN u");
      assertEqual([ { "active" : true, "age" : 37, "gender" : "m", "id" : 100, "name" : "John" } ], actual);

      actual = getQueryResults("FOR u in " + users.name() + " SORT u.id DESC LIMIT 19,3 RETURN u");
      assertEqual([ { "active" : true, "age" : 37, "gender" : "m", "id" : 100, "name" : "John" } ], actual);
      
      actual = getQueryResults("FOR u in " + users.name() + " SORT u.id DESC LIMIT 19,100 RETURN u");
      assertEqual([ { "active" : true, "age" : 37, "gender" : "m", "id" : 100, "name" : "John" } ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief limit out of range 
////////////////////////////////////////////////////////////////////////////////
    
    testLimitOutOfRange : function () {
      var actual;

      // limit ouf of range
      actual = getQueryResults("FOR u in " + users.name() + " SORT u.id DESC LIMIT 20,1 RETURN u");
      assertEqual([ ], actual);

      actual = getQueryResults("FOR u in " + users.name() + " SORT u.id DESC LIMIT 100,1 RETURN u");
      assertEqual([ ], actual);
      
      actual = getQueryResults("FOR u in " + users.name() + " FILTER u.age >= 40 SORT u.id DESC LIMIT 0,1 RETURN u");
      assertEqual([ ], actual);
      
      actual = getQueryResults("FOR u in " + users.name() + " FILTER u.age >= 40 SORT u.id DESC LIMIT 1 RETURN u");
      assertEqual([ ], actual);
      
      actual = getQueryResults("FOR u in " + users.name() + " FILTER u.age >= 40 SORT u.id DESC LIMIT 1,1 RETURN u");
      assertEqual([ ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test equivalent filters
////////////////////////////////////////////////////////////////////////////////
    
    testFilterEquivalents : function () {
      var expected = [ { "active" : false, "age" : 34, "gender" : "m", "id" : 103, "name" : "Ethan" }, { "active" : false, "age" : 35, "gender" : "m", "id" : 102, "name" : "Jacob" } ];
      var actual;

      actual = getQueryResults("FOR u in " + users.name() + " FILTER u.age IN [ 34, 35 ] && u.gender == \"m\" SORT u.id DESC RETURN u");
      assertEqual(expected, actual);
      
      actual = getQueryResults("FOR u in " + users.name() + " FILTER u.gender == \"m\" && u.age IN [ 34, 35 ] SORT u.id DESC RETURN u");
      assertEqual(expected, actual);
      
      actual = getQueryResults("FOR u in " + users.name() + " FILTER u.gender == \"m\" FILTER u.age IN [ 34, 35 ] SORT u.id DESC RETURN u");
      assertEqual(expected, actual);

      actual = getQueryResults("FOR u in " + users.name() + " FILTER u.age IN [ 34, 35 ] FILTER u.gender == \"m\" SORT u.id DESC RETURN u");
      assertEqual(expected, actual);
      
      actual = getQueryResults("FOR u in " + users.name() + " FILTER u.age IN [ 34, 35 ] SORT u.id DESC FILTER u.gender == \"m\" RETURN u");
      assertEqual(expected, actual);
      
      actual = getQueryResults("FOR u in " + users.name() + " SORT u.id FILTER u.gender == \"m\" SORT u.id DESC FILTER u.age IN [ 34, 35 ] RETURN u");
      assertEqual(expected, actual);
      
      actual = getQueryResults("FOR u in " + users.name() + " SORT u.id DESC FILTER u.age IN [ 34, 35 ] FILTER u.gender == \"m\" RETURN u");
      assertEqual(expected, actual);
      
      actual = getQueryResults("FOR u in " + users.name() + " SORT u.id DESC FILTER u.gender == \"m\" FILTER u.age IN [ 34, 35 ] RETURN u");
      assertEqual(expected, actual);

      actual = getQueryResults("FOR u in " + users.name() + " SORT u.id DESC FILTER u.gender == \"m\" && u.age IN [ 34, 35 ] RETURN u");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test impossible filters
////////////////////////////////////////////////////////////////////////////////
    
    testFilterImpossible : function () {
      var actual;

      actual = getQueryResults("FOR u in " + users.name() + " FILTER u.age IN [ true, false, null, 26, 27, 38, 39, [ ], { } ] RETURN u");
      assertEqual([ ], actual);
      
      actual = getQueryResults("FOR u in " + users.name() + " FILTER u.gender == \"x\" RETURN u");
      assertEqual([ ], actual);
      
      actual = getQueryResults("FOR u in " + users.name() + " FILTER !(u.gender IN [ \"m\", \"f\" ]) RETURN u");
      assertEqual([ ], actual);
      
      actual = getQueryResults("FOR u in " + users.name() + " FILTER u.age < 28 || u.age > 37 RETURN u");
      assertEqual([ ], actual);
      
      actual = getQueryResults("FOR u in " + users.name() + " FILTER u.age <= 27 || u.age >= 38 RETURN u");
      assertEqual([ ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test collect
////////////////////////////////////////////////////////////////////////////////
    
    testCollectSimple : function () {
      var expected = [ { "gender" : "f", "numUsers" : 10 }, { "gender" : "m", "numUsers" : 10 } ];
      var actual = getQueryResults("FOR u in " + users.name() + " COLLECT gender = u.gender INTO g SORT gender ASC RETURN { \"gender\" : gender, \"numUsers\" : LENGTH(g) }");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test collect with filter
////////////////////////////////////////////////////////////////////////////////
      
    testCollectFiltered : function () {
      var expected = [ { "gender" : "m", "numUsers" : 8 }, { "gender" : "f", "numUsers" : 8 } ];
      var actual = getQueryResults("FOR u in " + users.name() + " FILTER u.active == true COLLECT gender = u.gender INTO g SORT gender DESC RETURN { \"gender\" : gender, \"numUsers\" : LENGTH(g) }");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple collect criteria
////////////////////////////////////////////////////////////////////////////////
      
    testCollectMultipleCriteria : function () {
      var expected = [ { "active" : false, "gender" : "m", "numUsers" : 2 }, { "active" : true, "gender" : "m", "numUsers" : 8 }, { "active" : false, "gender" : "f", "numUsers" : 2 }, { "active" : true, "gender" : "f", "numUsers" : 8 } ];
      var actual = getQueryResults("FOR u in " + users.name() + " COLLECT gender = u.gender, active = u.active INTO g SORT gender DESC, active ASC RETURN { \"gender\" : gender, \"active\" : active, \"numUsers\" : LENGTH(g) }");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test relations
////////////////////////////////////////////////////////////////////////////////
      
    testRelations1 : function () {
      var expected = [ { "name" : "Abigail", "numFriends" : 3 }, { "name" : "Alexander", "numFriends" : 2 }, { "name" : "Isabella", "numFriends" : 2 }, { "name" : "John", "numFriends" : 2 } ];

      var actual = getQueryResults("FOR u in " + users.name() + " FILTER u.active == true LET f = ((FOR r IN " + relations.name() + " FILTER r.from == u.id && r.type == \"friend\" RETURN r)) SORT LENGTH(f) DESC, u.name LIMIT 0,4 FILTER LENGTH(f) > 0 RETURN { \"name\" : u.name, \"numFriends\" : LENGTH(f) }");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test relations
////////////////////////////////////////////////////////////////////////////////

    testRelations2 : function () {
      var expected = [ { "friends" : [102, 106, 108], "name" : "Abigail" }, { "friends" : [100, 104], "name" : "Alexander" }, { "friends" : [109, 203], "name" : "Isabella" }, { "friends" : [109, 208], "name" : "John" } ];

      var actual = getQueryResults("FOR u in " + users.name() + " FILTER u.active == true LET f = ((FOR r IN " + relations.name() + " FILTER r.from == u.id && r.type == \"friend\" SORT r.to RETURN r)) SORT LENGTH(f) DESC, u.name LIMIT 0,4 FILTER LENGTH(f) > 0 RETURN { \"name\" : u.name, \"friends\" : f[*].to }");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test relations
////////////////////////////////////////////////////////////////////////////////
    
    testRelations3 : function () {
      var expected = [ { "friends" : ["Daniel", "Jacob", "Jim"], "name" : "Abigail" }, { "friends" : ["John", "Michael"], "name" : "Alexander" }, { "friends" : ["Diego", "Madison"], "name" : "Isabella" }, { "friends" : ["Diego", "Mary"], "name" : "John" } ];

      var actual = getQueryResults("FOR u in " + users.name() + " FILTER u.active == true LET f = ((FOR r IN " + relations.name() + " FILTER r.from == u.id && r.type == \"friend\" FOR u2 IN " + users.name() + " FILTER r.to == u2.id SORT u2.name RETURN u2.name)) SORT LENGTH(f) DESC, u.name LIMIT 0,4 FILTER LENGTH(f) > 0 RETURN { \"name\" : u.name, \"friends\" : f }");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test relations
////////////////////////////////////////////////////////////////////////////////
    
    testRelations4 : function () {
      var expected = [ { "friends" : ["Daniel", "Jacob", "Jim"], "name" : "Abigail" }, { "friends" : ["John", "Michael"], "name" : "Alexander" }, { "friends" : ["Diego", "Madison"], "name" : "Isabella" }, { "friends" : ["Diego", "Mary"], "name" : "John" } ];

      var actual = getQueryResults("FOR u in " + users.name() + " FILTER u.active == true LET f = ((FOR r IN " + relations.name() + " FILTER r.from == u.id && r.type == \"friend\" FOR u2 IN " + users.name() + " FILTER r.to == u2.id SORT u2.name RETURN u2)) SORT LENGTH(f) DESC, u.name LIMIT 0,4 FILTER LENGTH(f) > 0 RETURN { \"name\" : u.name, \"friends\" : f[*].name }");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test relations
////////////////////////////////////////////////////////////////////////////////
      
    testRelations5 : function () {
      var expected = [ { "friendIds" : [106, 102, 108], "friendNames" : ["Daniel", "Jacob", "Jim"], "name" : "Abigail" }, { "friendIds" : [100, 104], "friendNames" : ["John", "Michael"], "name" : "Alexander" }, { "friendIds" : [109, 203], "friendNames" : ["Diego", "Madison"], "name" : "Isabella" }, { "friendIds" : [109, 208], "friendNames" : ["Diego", "Mary"], "name" : "John" } ];

      var actual = getQueryResults("FOR u in " + users.name() + " FILTER u.active == true LET f = ((FOR r IN " + relations.name() + " FILTER r.from == u.id && r.type == \"friend\" FOR u2 IN " + users.name() + " FILTER r.to == u2.id SORT u2.name RETURN u2)) SORT LENGTH(f) DESC, u.name LIMIT 0,4 FILTER LENGTH(f) > 0 RETURN { \"name\" : u.name, \"friendNames\" : f[*].name, \"friendIds\" : f[*].id }");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test querying _from and _to of non-edges
////////////////////////////////////////////////////////////////////////////////
    
    testFromToOfNonEdge : function () {
      var expected = [ 
        {
          "from" : null,
          "name" : "Abigail",
          "to" : null
        },
        {
          "from" : null,
          "name" : "Alexander",
          "to" : null
        },
        {
          "from" : null,
          "name" : "Anthony",
          "to" : null
        },
        {
          "from" : null,
          "name" : "Chloe",
          "to" : null
        },
        {
          "from" : null,
          "name" : "Daniel",
          "to" : null
        },
        {
          "from" : null,
          "name" : "Diego",
          "to" : null
        },
        {
          "from" : null,
          "name" : "Emma",
          "to" : null
        },
        {
          "from" : null,
          "name" : "Ethan",
          "to" : null
        },
        {
          "from" : null,
          "name" : "Eva",
          "to" : null
        },
        {
          "from" : null,
          "name" : "Fred",
          "to" : null
        },
        {
          "from" : null,
          "name" : "Isabella",
          "to" : null
        },
        {
          "from" : null,
          "name" : "Jacob",
          "to" : null
        },
        {
          "from" : null,
          "name" : "Jim",
          "to" : null
        },
        {
          "from" : null,
          "name" : "John",
          "to" : null
        },
        {
          "from" : null,
          "name" : "Madison",
          "to" : null
        },
        {
          "from" : null,
          "name" : "Mariah",
          "to" : null
        },
        {
          "from" : null,
          "name" : "Mary",
          "to" : null
        },
        {
          "from" : null,
          "name" : "Michael",
          "to" : null
        },
        {
          "from" : null,
          "name" : "Olivia",
          "to" : null
        },
        {
          "from" : null,
          "name" : "Sophia",
          "to" : null
        }
      ];

      var actual = getQueryResults("FOR u in " + users.name() + " SORT u.name RETURN { name : u.name, from: u._from, to: u._to }");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test querying attributes
////////////////////////////////////////////////////////////////////////////////
    
    testAttributesQuery1 : function () {
      var expected = [ 
        [ 
          "Abigail", 
          "Abigail" 
        ], 
        [ 
          "Alexander", 
          "Alexander" 
        ], 
        [ 
          "Anthony", 
          "Anthony" 
        ], 
        [ 
          "Chloe", 
          "Chloe" 
        ], 
        [ 
          "Daniel", 
          "Daniel" 
        ], 
        [ 
          "Diego", 
          "Diego" 
        ], 
        [ 
          "Emma", 
          "Emma" 
        ], 
        [ 
          "Ethan", 
          "Ethan" 
        ], 
        [ 
          "Eva", 
          "Eva" 
        ], 
        [ 
          "Fred", 
          "Fred" 
        ], 
        [ 
          "Isabella", 
          "Isabella" 
        ], 
        [ 
          "Jacob", 
          "Jacob" 
        ], 
        [ 
          "Jim", 
          "Jim" 
        ], 
        [ 
          "John", 
          "John" 
        ], 
        [ 
          "Madison", 
          "Madison" 
        ], 
        [ 
          "Mariah", 
          "Mariah" 
        ], 
        [ 
          "Mary", 
          "Mary" 
        ], 
        [ 
          "Michael", 
          "Michael" 
        ], 
        [ 
          "Olivia", 
          "Olivia" 
        ], 
        [ 
          "Sophia", 
          "Sophia" 
        ] 
      ];

      var actual = getQueryResults("FOR u in " + users.name() + " SORT u.name RETURN [ u.name, u['name'] ]");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test querying attributes
////////////////////////////////////////////////////////////////////////////////
    
    testAttributesQuery2 : function () {
      var expected = [ 
        [ 
          "Abigail", 
          "Abigail" 
        ], 
        [ 
          "Alexander", 
          "Alexander" 
        ], 
        [ 
          "Anthony", 
          "Anthony" 
        ], 
        [ 
          "Chloe", 
          "Chloe" 
        ], 
        [ 
          "Daniel", 
          "Daniel" 
        ], 
        [ 
          "Diego", 
          "Diego" 
        ], 
        [ 
          "Emma", 
          "Emma" 
        ], 
        [ 
          "Ethan", 
          "Ethan" 
        ], 
        [ 
          "Eva", 
          "Eva" 
        ], 
        [ 
          "Fred", 
          "Fred" 
        ], 
        [ 
          "Isabella", 
          "Isabella" 
        ], 
        [ 
          "Jacob", 
          "Jacob" 
        ], 
        [ 
          "Jim", 
          "Jim" 
        ], 
        [ 
          "John", 
          "John" 
        ], 
        [ 
          "Madison", 
          "Madison" 
        ], 
        [ 
          "Mariah", 
          "Mariah" 
        ], 
        [ 
          "Mary", 
          "Mary" 
        ], 
        [ 
          "Michael", 
          "Michael" 
        ], 
        [ 
          "Olivia", 
          "Olivia" 
        ], 
        [ 
          "Sophia", 
          "Sophia" 
        ] 
      ];
      var actual = getQueryResults("FOR u in " + users.name() + " LET d = 'name' SORT u.name RETURN [ u.name, u[d] ]");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test querying attributes
////////////////////////////////////////////////////////////////////////////////
    
    testAttributesQuery3 : function () {
      users.save({ "hobbies" : [ "riding", "skating", "swimming" ] });

      var expected = [ [ "riding", "skating", "swimming", null, "swimming" ] ];
      var actual = getQueryResults("FOR u in " + users.name() + " FILTER HAS(u, 'hobbies') RETURN [ u.hobbies[0], u.hobbies[1], u.hobbies[2], u.hobbies[3], u.hobbies[-1] ]");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test hashing the documents
////////////////////////////////////////////////////////////////////////////////
    
    testHashes : function () {
      var expected = [
        438613548108819, 
        736577518779056, 
        1997321613449496, 
        1350231173529518, 
        1270199420722049, 
        317879879725550, 
        722866072919463, 
        1206291785257135, 
        1552225485422317, 
        961337730609982, 
        369117059618668, 
        1882233601916968, 
        528406878048694, 
        1954958029766941, 
        1081655675871352, 
        1244560085281213, 
        2210430228027913, 
        1550995730612121, 
        621907927620752, 
        957121224257198 
      ];
      var actual = getQueryResults("FOR u in " + users.name() + " SORT u.id RETURN HASH(UNSET(u, ['_key', '_rev', '_id']))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test hashing the documents
////////////////////////////////////////////////////////////////////////////////
    
    testHashSubquery : function () {
      var expected = [ 1815371496337334 ];
      var actual = getQueryResults("RETURN HASH(FOR u in " + users.name() + " SORT u.id RETURN UNSET(u, ['_key', '_rev', '_id']))");
      assertEqual(expected, actual);
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ahuacatlQueryCollectionTestSuite);

return jsunity.done();

