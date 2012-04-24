////////////////////////////////////////////////////////////////////////////////
/// @brief tests for query language, escaping
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

function ahuacatlEscapingTestSuite () {

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
/// @brief test simple name handling
////////////////////////////////////////////////////////////////////////////////

    testSimpleName : function () {
      var expected = [ 1, 2, 3 ];
      var actual = getQueryResults("FOR `x` IN [ 1, 2, 3 ] RETURN `x`", true);
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test reserved name handling
////////////////////////////////////////////////////////////////////////////////
    
    testReservedNames1 : function () {
      var expected = [ 1, 2, 3 ];
      var names = [ "for", "let", "return", "sort", "collect", "limit", "filter", "asc", "desc", "in", "into" ];

      for (var i in names) {
        if (!names.hasOwnProperty(i)) {
          continue;
        }

        var actual = getQueryResults("FOR `" + names[i] + "` IN [ 1, 2, 3 ] RETURN `" + names[i] + "`", true);
        assertEqual(expected, actual);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test reserved names handling
////////////////////////////////////////////////////////////////////////////////
    
    testReservedNames2 : function () {
      var expected = [ { "let" : 1 }, { "collect" : 2 }, { "sort" : 3 } ];
      var actual = getQueryResults("FOR `for` IN [ { \"let\" : 1 }, { \"collect\" : 2 }, { \"sort\" : 3 } ] RETURN `for`", false);
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test punctuation names escaping
////////////////////////////////////////////////////////////////////////////////
    
    testPunctuationName1 : function () {
      var expected = [ 1, 2, 3 ];
      var actual = getQueryResults("FOR `brown_fox` IN [ 1, 2, 3 ] RETURN `brown_fox`", true);
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test punctuation names escaping
////////////////////////////////////////////////////////////////////////////////
    
    testPunctuationName2 : function () {
      var expected = [ 1, 2, 3 ];
      var actual = getQueryResults("FOR `brown_fox__1234_` IN [ 1, 2, 3 ] RETURN `brown_fox__1234_`", true);
      assertEqual(expected, actual);
    },

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ahuacatlEscapingTestSuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
