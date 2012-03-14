////////////////////////////////////////////////////////////////////////////////
/// @brief tests for bind parameters
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

function aqlBindParametersTestSuite () {
  var collection = null;

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

  function setUp () {
    this.collection = db.UnitTestsBind;

    if (this.collection.count() == 0) {
      for (var i = 0; i <= 10; i++) {
        for (var j = 0; j <= 10; j++) {
          this.collection.save({ "value1" : i, "value2" : j, "value3" : "test", "value4" : null, "value5" : -13.5});
        }
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

  function executeQuery (query, bindParameters) {
    var cursor = AQL_STATEMENT(db, query, bindParameters);
    assertFalse(cursor instanceof AvocadoQueryError);
    return cursor;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief execute a given query and return the results as an array
////////////////////////////////////////////////////////////////////////////////

  function getQueryResults (query, bindParameters) {
    var cursor = this.executeQuery(query, bindParameters);
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

  function testKeywordNamesWhereValid () {
    //this.collection.save({ "value1" : i, "value2" : j, "value3" : "test", "value4" : null, "value5" : -13.5});
    var result = this.getQueryResults("SELECT { value: @value1@ } FROM " + this.collection._name + " c WHERE c.value1 == @value1@", { "value1" : 0 });

    assertEqual(11, result.length);
  }

}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsUnity.run(aqlBindParametersTestSuite);

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
