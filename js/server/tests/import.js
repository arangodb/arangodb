////////////////////////////////////////////////////////////////////////////////
/// @brief tests for query language, arithmetic operators
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

var internal = require("internal");
var jsunity = require("jsunity");
var ArangoError = require("org/arangodb/arango-error").ArangoError; 

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function importTestSuite () {
  var errors = internal.errors;

////////////////////////////////////////////////////////////////////////////////
/// @brief return the error code from a result
////////////////////////////////////////////////////////////////////////////////

  function getErrorCode (fn) {
    try {
      fn();
    }
    catch (e) {
      return e.errorNum;
    }
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief execute a given query
////////////////////////////////////////////////////////////////////////////////

  function executeQuery (query) {
    var statement = internal.db._createStatement({"query": query});
    if (statement instanceof ArangoError) {
      return statement;
    }

    var cursor = statement.execute();
    return cursor;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief execute a given query and return the results as an array
////////////////////////////////////////////////////////////////////////////////

  function getQueryResults (query) {
    var result = executeQuery(query);
    var results = [ ];

    while (result.hasNext()) {
      var keys = [ ];
      var row = result.next();
      for (var k in row) {
        if (row.hasOwnProperty(k) && k != '_id' && k != '_rev' && k != '_key') {
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
/// @brief test json import
////////////////////////////////////////////////////////////////////////////////
    
    testJsonImport1 : function () {
      var expected = [ { "id": 1, "one": 1, "three": 3, "two": 2 }, { "a": 1234, "b": "the quick fox", "id": 2, "jumped": "over the fox", "null": null }, { "id": 3, "not": "important", "spacing": "is" }, { "  c  ": "h\"'ihi", "a": true, "b": false, "d": "", "id": 4 }, { "id": 5 } ];
      var actual = getQueryResults("FOR i IN UnitTestsImportJson1 SORT i.id RETURN i");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test json import
////////////////////////////////////////////////////////////////////////////////
    
    testJsonImport2 : function () {
      var expected = [ { "id": 1, "value": -445.4 }, { "id": 2, "value": 34000 }, { "id": 3, "value": null } ];
      var actual = getQueryResults("FOR i IN UnitTestsImportJson2 SORT i.id RETURN i");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test json import
////////////////////////////////////////////////////////////////////////////////
    
    testJsonImport3 : function () {
      var expected = [ { "id": 1, "one": 1, "three": 3, "two": 2 }, { "a": 1234, "b": "the quick fox", "id": 2, "jumped": "over the fox", "null": null }, { "id": 3, "not": "important", "spacing": "is" }, { "  c  ": "h\"'ihi", "a": true, "b": false, "d": "", "id": 4 }, { "id": 5 } ];
      var actual = getQueryResults("FOR i IN UnitTestsImportJson3 SORT i.id RETURN i");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test json import
////////////////////////////////////////////////////////////////////////////////
    
    testJsonImport4 : function () {
      var expected = [ ];
      for (var i = 0; i < 1000; ++i) {
        expected.push({ "active": true, "id": i, "value": "somerandomstuff" + i });
      }
      var actual = getQueryResults("FOR i IN UnitTestsImportJson4 SORT i.id RETURN i");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test csv import
////////////////////////////////////////////////////////////////////////////////
    
    testCsvImport1 : function () {
      var expected = [ { "a": "1", "b": 1, "c": "1.3", "d": null, "e": -5, "id": 1 }, { "a": null, "b": "", "c": 3.1, "d": -2.5, "e": "ddd \" ' ffd", "id": 2 } ];
      var actual = getQueryResults("FOR i IN UnitTestsImportCsv1 SORT i.id RETURN i");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test csv import
////////////////////////////////////////////////////////////////////////////////
    
    testCsvImport2 : function () {
      assertEqual(errors.ERROR_ARANGO_COLLECTION_NOT_FOUND.code, getErrorCode(function() { executeQuery("FOR i IN UnitTestsImportCsv2 SORT i.id RETURN i"); } ));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test tsv import
////////////////////////////////////////////////////////////////////////////////
    
    testTsvImport1 : function () {
      var expected = [ { "ab": "g", "cd": "h", "ef": "i" }, { "ab" : "j", "cd" : "k", "ef" : "l" } ];
      var actual = getQueryResults("FOR i IN UnitTestsImportTsv1 SORT i.ab RETURN i");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test tsv import
////////////////////////////////////////////////////////////////////////////////
    
    testTsvImport2 : function () {
      var expected = [ { "  fox" : "dog", "brown  " : " the lazy", "the quick": " jumped over" }, { "  fox" : " end", "brown  " : "\"\"\"", "the quick" : "\"'.,;" } ];
      var actual = getQueryResults("FOR i IN UnitTestsImportTsv2 SORT i.`  fox` DESC RETURN i");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test json edge import
////////////////////////////////////////////////////////////////////////////////
    
    testJsonEdgesImport : function () {
      var expected = [ 
        { _from : "UnitTestsImportVertex/v1", _to: "UnitTestsImportVertex/v2", id: 1, what: "v1->v2" },
        { _from : "UnitTestsImportVertex/v2", _to: "UnitTestsImportVertex/v3", id: 2, what: "v2->v3" },
        { _from : "UnitTestsImportVertex/v9", _to: "UnitTestsImportVertex/v4", extra: "foo", id: 3, what: "v9->v4" },
        { _from : "UnitTestsImportVertex/v12", _to: "UnitTestsImportVertex/what", id: 4 }
      ];

      var actual = getQueryResults("FOR i IN UnitTestsImportEdge SORT i.id RETURN i");
      assertEqual(expected, actual);
    }

  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(importTestSuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
