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

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function aqlKeywordsTestSuite () {
  var collection = null;

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

  function setUp () {
    this.keywords = [ "select", "from", "where", "join", "inner", "left", "right", "on", "limit", "order", "by", "near", "within" ];

    this.keywordHash = { };
    for (var i in this.keywords) {
      if (!this.keywords.hasOwnProperty(i)) {
        continue;
      }
      this.keywordHash[this.keywords[i]] = this.keywords[i];
    }

    this.collection = db.UnitTestsKeywords;

    if (this.collection.count() == 0) {
      for (var i in this.keywords) {
        if (!this.keywords.hasOwnProperty(i)) {
          continue;
        }

        var k = this.keywords[i];
        var row = { };
        row[k] = k;

        this.collection.save(row);
      }
    }
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

  function tearDown () {
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief execute a given query
////////////////////////////////////////////////////////////////////////////////

  function executeQuery (query, expectError) {
    var cursor = AQL_STATEMENT(db, query, undefined);
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
    var cursor = this.executeQuery(query, expectError);
    if (cursor) {
      var results = [ ];
      while (cursor.hasNext()) {
        results.push(cursor.next());
      }
      return results;
    }
    return cursor;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief check keyword names in select
////////////////////////////////////////////////////////////////////////////////

  function testKeywordNamesSelectInvalid () {
    for (var i in this.keywords) {
      if (!this.keywords.hasOwnProperty(i)) {
        continue;
      }
     
      // this is expected to fail  
      this.getQueryResults("SELECT { " + this.keywords[i] + " : null } FROM " + this.collection._name + " c", true); 
    }
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief check keyword names in select
////////////////////////////////////////////////////////////////////////////////

  function testKeywordNamesSelectValid1 () {
    for (var i in this.keywords) {
      if (!this.keywords.hasOwnProperty(i)) {
        continue;
      }
     
      // this is expected to work 
      this.getQueryResults("SELECT { \"" + this.keywords[i] + "\" : null } FROM " + this.collection._name + " c", false); 
    }
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief check keyword names in select
////////////////////////////////////////////////////////////////////////////////

  function testKeywordNamesSelectValid2 () {
    for (var i in this.keywords) {
      if (!this.keywords.hasOwnProperty(i)) {
        continue;
      }
     
      // this is expected to work 
      this.getQueryResults("SELECT { value : \"" + this.keywords[i] + "\" } FROM " + this.collection._name + " c", false); 
    }
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief check keyword names in select
////////////////////////////////////////////////////////////////////////////////

  function testKeywordNamesSelectValid3 () {
    for (var i in this.keywords) {
      if (!this.keywords.hasOwnProperty(i)) {
        continue;
      }
     
      // this is expected to work 
      var result = this.getQueryResults("SELECT { value : c.`" + this.keywords[i] + "` } FROM " + this.collection._name + " c WHERE c.`" + this.keywords[i] + "` == '" + this.keywords[i] + "'", false); 
      for (var j in result) {
        if (!result.hasOwnProperty(j)) {
          continue;
        }
        assertEqual(this.keywords[i], result[j]["value"]);
      }
    }
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief check keyword names in from alias
////////////////////////////////////////////////////////////////////////////////

  function testKeywordNamesFromInvalid () {
    for (var i in this.keywords) {
      if (!this.keywords.hasOwnProperty(i)) {
        continue;
      }
     
      // this is expected to work 
      this.getQueryResults("SELECT { } FROM " + this.collection._name + " " + this.keywords[i], true); 
    }
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief check keyword names in from alias
////////////////////////////////////////////////////////////////////////////////

  function testKeywordNamesFromValid () {
    for (var i in this.keywords) {
      if (!this.keywords.hasOwnProperty(i)) {
        continue;
      }
     
      // this is expected to work 
      var result = this.getQueryResults("SELECT `" + this.keywords[i] + "` FROM " + this.collection._name + " `" + this.keywords[i] + "`", false); 
      assertEqual(result.length, this.keywords.length);
    }
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief check keyword names in where
////////////////////////////////////////////////////////////////////////////////

  function testKeywordNamesWhereInvalid () {
    for (var i in this.keywords) {
      if (!this.keywords.hasOwnProperty(i)) {
        continue;
      }
     
      // this is expected to work 
      this.getQueryResults("SELECT { } FROM " + this.collection._name + " c WHERE c." + this.keywords[i] + " == '" + this.keywords[i] + "'", true); 
    }
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief check keyword names in where
////////////////////////////////////////////////////////////////////////////////

  function testKeywordNamesWhereValid () {
    for (var i in this.keywords) {
      if (!this.keywords.hasOwnProperty(i)) {
        continue;
      }
     
      // this is expected to work 
      var result = this.getQueryResults("SELECT { \"" + this.keywords[i] + "\" : c.`" + this.keywords[i] + "` } FROM " + this.collection._name + " c WHERE '" + this.keywords[i] + "' == c.`" + this.keywords[i] + "`", false); 
      for (var j in result) {
        if (!result.hasOwnProperty(j)) {
          continue;
        }
        
        assertEqual(this.keywords[i], result[j][this.keywords[i]]);
      }
    }
  }

}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsUnity.run(aqlKeywordsTestSuite);

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
