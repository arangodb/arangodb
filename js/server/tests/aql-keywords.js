////////////////////////////////////////////////////////////////////////////////
/// @brief tests for query language, keywords
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

function aqlKeywordsTestSuite () {
  var collection = null;
  var keywords = [ ];
  var keywordHash = { };

  ////////////////////////////////////////////////////////////////////////////////
/// @brief execute a given query
////////////////////////////////////////////////////////////////////////////////

  function executeQuery (query, expectError) {
    var cursor = AQL_STATEMENT(query, undefined);
    if (expectError) {
      assertTrue(cursor instanceof AvocadoQueryError);
      return null;
    }
     
    assertFalse(cursor instanceof AvocadoQueryError);
    return cursor;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief execute a given query and return the results as an array
////////////////////////////////////////////////////////////////////////////////

  function getQueryResults (query, expectError) {
    var cursor = executeQuery(query, expectError);
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
      keywords = [ "select", "from", "where", "join", "inner", "left", "right", "on", "limit", "order", "by", "near", "within" ];

      keywordHash = { };
      for (var i in keywords) {
        if (!keywords.hasOwnProperty(i)) {
          continue;
        }
        keywordHash[keywords[i]] = keywords[i];
      }

      collection = db.UnitTestsKeywords;

      if (collection.count() == 0) {
        for (var i in keywords) {
          if (!keywords.hasOwnProperty(i)) {
            continue;
          }

          var k = keywords[i];
          var row = { };
          row[k] = k;
 
          collection.save(row);
        }
      }
    },  

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function() {
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check keyword names in select
////////////////////////////////////////////////////////////////////////////////

    testKeywordNamesSelectInvalid : function () {
      for (var i in keywords) {
        if (!keywords.hasOwnProperty(i)) {
          continue;
        }
     
        // this is expected to fail  
        getQueryResults("SELECT { " + keywords[i] + " : null } FROM " + collection.name() + " c", true); 
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check keyword names in select
////////////////////////////////////////////////////////////////////////////////

    testKeywordNamesSelectValid1 : function () {
      for (var i in keywords) {
        if (!keywords.hasOwnProperty(i)) {
          continue;
        }
     
        // this is expected to work 
        getQueryResults("SELECT { \"" + keywords[i] + "\" : null } FROM " + collection.name() + " c", false); 
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check keyword names in select
////////////////////////////////////////////////////////////////////////////////

    testKeywordNamesSelectValid2 : function () {
      for (var i in keywords) {
        if (!keywords.hasOwnProperty(i)) {
          continue;
        }
      
        // this is expected to work 
        getQueryResults("SELECT { value : \"" + keywords[i] + "\" } FROM " + collection.name() + " c", false); 
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check keyword names in select
////////////////////////////////////////////////////////////////////////////////

    testKeywordNamesSelectValid3 : function () {
      for (var i in keywords) {
        if (!keywords.hasOwnProperty(i)) {
          continue;
        }
      
        // this is expected to work 
        var result = getQueryResults("SELECT { value : c.`" + keywords[i] + "` } FROM " + collection.name() + " c WHERE c.`" + keywords[i] + "` == '" + keywords[i] + "'", false); 
        for (var j in result) {
          if (!result.hasOwnProperty(j)) {
            continue;
          }
          assertEqual(keywords[i], result[j]["value"]);
        }
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check keyword names in from alias
////////////////////////////////////////////////////////////////////////////////

    testKeywordNamesFromInvalid : function () {
      for (var i in keywords) {
        if (!keywords.hasOwnProperty(i)) {
          continue;
        }
      
        // this is expected to work 
        getQueryResults("SELECT { } FROM " + collection.name() + " " + keywords[i], true); 
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check keyword names in from alias
////////////////////////////////////////////////////////////////////////////////

    testKeywordNamesFromValid : function () {
      for (var i in keywords) {
        if (!keywords.hasOwnProperty(i)) {
          continue;
        }
      
        // this is expected to work 
        var result = getQueryResults("SELECT `" + keywords[i] + "` FROM " + collection.name() + " `" + keywords[i] + "`", false); 
        assertEqual(result.length, keywords.length);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check keyword names in where
////////////////////////////////////////////////////////////////////////////////

    testKeywordNamesWhereInvalid : function () {
      for (var i in keywords) {
        if (!keywords.hasOwnProperty(i)) {
          continue;
        }
      
        // this is expected to work 
        getQueryResults("SELECT { } FROM " + collection.name() + " c WHERE c." + keywords[i] + " == '" + keywords[i] + "'", true); 
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check keyword names in where
////////////////////////////////////////////////////////////////////////////////

    testKeywordNamesWhereValid : function () {
      for (var i in keywords) {
        if (!keywords.hasOwnProperty(i)) {
          continue;
        }
      
        // this is expected to work 
        var result = getQueryResults("SELECT { \"" + keywords[i] + "\" : c.`" + keywords[i] + "` } FROM " + collection.name() + " c WHERE '" + keywords[i] + "' == c.`" + keywords[i] + "`", false); 
        for (var j in result) {
          if (!result.hasOwnProperty(j)) {
            continue;
          }
          
          assertEqual(keywords[i], result[j][keywords[i]]);
        }
      }
    }

  };

}

// -----------------------------------------------------------------------------
// --SECTION--                                                              main
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(aqlKeywordsTestSuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
