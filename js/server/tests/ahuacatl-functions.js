////////////////////////////////////////////////////////////////////////////////
/// @brief tests for query language, functions
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

function ahuacatlFunctionsTestSuite () {

  ////////////////////////////////////////////////////////////////////////////////
/// @brief execute a given query
////////////////////////////////////////////////////////////////////////////////

  function executeQuery (query) {
    var cursor = AHUACATL_RUN(query, undefined);
    if (cursor instanceof AvocadoQueryError) {
      print(query, cursor.message);
    }
    assertFalse(cursor instanceof AvocadoQueryError);
    return cursor;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief execute a given query and return the results as an array
////////////////////////////////////////////////////////////////////////////////

  function getQueryResults (query, isFlat) {
    var result = executeQuery(query);
    var results = [ ];

    for (var i in result) {
      if (!result.hasOwnProperty(i)) {
        continue;
      }

      var row = result[i];
      if (isFlat) {
        results.push(row);
      } 
      else {
        var keys = [ ];
        for (var k in row) {
          if (row.hasOwnProperty(k) && k != '_id' && k != '_rev') {
            keys.push(k);
          }
        }
       
        keys.sort();
        var resultRow = { };
        for (var k in keys) {
          if (keys.hasOwnProperty(k)) {
            resultRow[keys[k]] = row[keys[k]];
          }
        }
        results.push(resultRow);
      }
    }

    return results;
  }


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
/// @brief test length function
////////////////////////////////////////////////////////////////////////////////

    testLength : function () {
      var expected = [ 4, 4, 4 ];
      var actual = getQueryResults("FOR year IN [ 2010, 2011, 2012 ] LET quarters = ((FOR q IN [ 1, 2, 3, 4 ] return q)) return LENGTH(quarters)", true);
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test concat function
////////////////////////////////////////////////////////////////////////////////
    
    testConcat : function () {
      var expected = [ "theQuickBrownFoxJumps" ];
      var actual = getQueryResults("FOR r IN [ 1 ] return CONCAT('the', 'Quick', null, 'Brown', null, 'Fox', 'Jumps')", true);
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test merge function
////////////////////////////////////////////////////////////////////////////////
    
    testMerge1 : function () {
      var expected = [ { "quarter" : 1, "year" : 2010 }, { "quarter" : 2, "year" : 2010 }, { "quarter" : 3, "year" : 2010 }, { "quarter" : 4, "year" : 2010 }, { "quarter" : 1, "year" : 2011 }, { "quarter" : 2, "year" : 2011 }, { "quarter" : 3, "year" : 2011 }, { "quarter" : 4, "year" : 2011 }, { "quarter" : 1, "year" : 2012 }, { "quarter" : 2, "year" : 2012 }, { "quarter" : 3, "year" : 2012 }, { "quarter" : 4, "year" : 2012 } ]; 
      var actual = getQueryResults("FOR year IN [ 2010, 2011, 2012 ] FOR quarter IN [ 1, 2, 3, 4 ] return MERGE({ \"year\" : year }, { \"quarter\" : quarter })", false);
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test merge function
////////////////////////////////////////////////////////////////////////////////
    
    testMerge2 : function () {
      var expected = [ { "age" : 15, "isAbove18" : false, "name" : "John" }, { "age" : 19, "isAbove18" : true, "name" : "Corey" } ];
      var actual = getQueryResults("FOR u IN [ { \"name\" : \"John\", \"age\" : 15 }, { \"name\" : \"Corey\", \"age\" : 19 } ] return MERGE(u, { \"isAbove18\" : u.age >= 18 })", false);
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test merge function
////////////////////////////////////////////////////////////////////////////////
    
    testMerge3 : function () {
      var expected = [ { "age" : 15, "id" : 9, "name" : "John" }, { "age" : 19, "id" : 9, "name" : "Corey" } ];
      var actual = getQueryResults("FOR u IN [ { \"id\" : 100, \"name\" : \"John\", \"age\" : 15 }, { \"id\" : 101, \"name\" : \"Corey\", \"age\" : 19 } ] return MERGE(u, { \"id\" : 9 })", false);
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test merge function
////////////////////////////////////////////////////////////////////////////////
    
    testMerge4 : function () {
      var expected = [ { "age" : 15, "id" : 33, "name" : "foo" }, { "age" : 19, "id" : 33, "name" : "foo" } ];
      var actual = getQueryResults("FOR u IN [ { \"id\" : 100, \"name\" : \"John\", \"age\" : 15 }, { \"id\" : 101, \"name\" : \"Corey\", \"age\" : 19 } ] return MERGE(u, { \"id\" : 9 }, { \"name\" : \"foo\", \"id\" : 33 })", false);
      assertEqual(expected, actual);
    },
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ahuacatlFunctionsTestSuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
