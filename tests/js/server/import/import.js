/*jshint globalstrict:false, strict:false */
/*global assertEqual */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for import
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

const internal = require("internal");
const jsunity = require("jsunity");
const ArangoError = require("@arangodb").ArangoError; 
const errors = internal.errors;

function importTestSuite () {
  'use strict';

  function getErrorCode (fn) {
    try {
      fn();
    } catch (e) {
      return e.errorNum;
    }
  }

  function executeQuery (query) {
    let statement = internal.db._createStatement({"query": query});
    if (statement instanceof ArangoError) {
      return statement;
    }

    return statement.execute();
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief execute a given query and return the results as an array
////////////////////////////////////////////////////////////////////////////////

  function getQueryResults (query, withKey) {
    var result = executeQuery(query);
    var results = [ ];

    while (result.hasNext()) {
      var keys = [ ];
      var row = result.next();
      var k;
      for (k in row) {
        if (row.hasOwnProperty(k) && k !== '_id' && k !== '_rev' && (k !== '_key' || withKey)) {
          keys.push(k);
        }
      }
       
      keys.sort();
      var resultRow = { };
      for (k in keys) {
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
/// @brief test csv import
////////////////////////////////////////////////////////////////////////////////
    
    testCsvImportSkip : function () {
      var expected = [ 
        { "a": "1", "b": 1, "c": "1.3", "e": -5, "id": 1 }, 
        { "b": "", "c": 3.1, "d": -2.5, "e": "ddd \" ' ffd", "id": 2 }, 
        { "a": "9999999999999999999999999999999999", "b": "test", "c" : -99999999, "d": true, "e": -888.4434, "id": 5 },
        { "a": 10e4, "b": 20.5, "c": -42, "d": " null ", "e": false, "id": 6 },
        { "a": -1.05e2, "b": 1.05e-2, "c": true, "d": false, "id": 7 }
      ];

      var actual = getQueryResults("FOR i IN UnitTestsImportCsvSkip SORT i.id RETURN i");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test json import
////////////////////////////////////////////////////////////////////////////////
    
    testJsonImport1 : function () {
      var expected = [ { "id": 1,
                         "one": 1,
                         "three": 3,
                         "two": 2 },
                       { "a": 1234,
                         "b": "the quick fox",
                         "id": 2,
                         "jumped":
                         "over the fox",
                         "null": null },
                       { "id": 3,
                         "not": "important",
                         "spacing": "is" },
                       { "  c  ": "h\"'ihi",
                         "a": true,
                         "b": false,
                         "d": "",
                         "id": 4 },
                       { "id": 5 } ];

      var actual = getQueryResults("FOR i IN UnitTestsImportJson1 SORT i.id RETURN i");
      assertEqual(expected, actual);
    },

    testJsonImport1Gz : function () {
      var expected = [ { "id": 1,
                         "one": 1,
                         "three": 3,
                         "two": 2 },
                       { "a": 1234,
                         "b": "the quick fox",
                         "id": 2,
                         "jumped":
                         "over the fox",
                         "null": null },
                       { "id": 3,
                         "not": "important",
                         "spacing": "is" },
                       { "  c  ": "h\"'ihi",
                         "a": true,
                         "b": false,
                         "d": "",
                         "id": 4 },
                       { "id": 5 } ];

      var actual = getQueryResults("FOR i IN UnitTestsImportJson1Gz SORT i.id RETURN i");
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
      var expected = [ 
        { "id": 1, "one": 1, "three": 3, "two": 2 }, 
        { "a": 1234, "b": "the quick fox", "id": 2, "jumped": "over the fox", "null": null }, 
        { "id": 3, "not": "important", "spacing": "is" }, 
        { "  c  ": "h\"'ihi", "a": true, "b": false, "d": "", "id": 4 }, 
        { "id": 5 } 
      ];
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

    testJsonImport4Gz : function () {
      var expected = [ ];
      for (var i = 0; i < 1000; ++i) {
        expected.push({ "active": true, "id": i, "value": "somerandomstuff" + i });
      }
      var actual = getQueryResults("FOR i IN UnitTestsImportJson4Gz SORT i.id RETURN i");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test json import
////////////////////////////////////////////////////////////////////////////////
    
    testJsonImport5 : function () {
      var expected = [ 
        { "active": true, "id": 0, "value": "something" },
        { "id": 1, "value": "foobar" },
        { "id": 2, "data": 99.5, "true": "a\t\r\nb  c " }
      ];

      var actual = getQueryResults("FOR i IN UnitTestsImportJson5 SORT i.id RETURN i");
      assertEqual(expected, actual);
    },
      
    testCsvImportNonoCreate : function () {
      assertEqual(errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code,
                  getErrorCode(function() { executeQuery("FOR i IN UnitTestsImportCsvNonoCreate RETURN i"); } ));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test csv import
////////////////////////////////////////////////////////////////////////////////
    
    testCsvImport1 : function () {
      var expected = [ 
        { "a": "1", "b": 1, "c": "1.3", "e": -5, "id": 1 }, 
        { "b": "", "c": 3.1, "d": -2.5, "e": "ddd \" ' ffd", "id": 2 }, 
        { "a": "9999999999999999999999999999999999", "b": "test", "c" : -99999999, "d": true, "e": -888.4434, "id": 5 },
        { "a": 10e4, "b": 20.5, "c": -42, "d": " null ", "e": false, "id": 6 },
        { "a": -1.05e2, "b": 1.05e-2, "c": true, "d": false, "id": 7 }
      ];

      var actual = getQueryResults("FOR i IN UnitTestsImportCsv1 SORT i.id RETURN i");
      assertEqual(expected, actual);
    },

    testCsvImport1Gz : function () {
      var expected = [
        { "a": "1", "b": 1, "c": "1.3", "e": -5, "id": 1 },
        { "b": "", "c": 3.1, "d": -2.5, "e": "ddd \" ' ffd", "id": 2 },
        { "a": "9999999999999999999999999999999999", "b": "test", "c" : -99999999, "d": true, "e": -888.4434, "id": 5 },
        { "a": 10e4, "b": 20.5, "c": -42, "d": " null ", "e": false, "id": 6 },
        { "a": -1.05e2, "b": 1.05e-2, "c": true, "d": false, "id": 7 }
      ];

      var actual = getQueryResults("FOR i IN UnitTestsImportCsv1Gz SORT i.id RETURN i");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test csv import
////////////////////////////////////////////////////////////////////////////////
    
    testCsvImport2 : function () {
      var actual = getQueryResults("FOR i IN UnitTestsImportCsv2 RETURN i");
      assertEqual([], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test csv import
////////////////////////////////////////////////////////////////////////////////
    
    testCsvImport3 : function () {
      var expected = [ 
        { name: "Bar", password: "wow!\nthis is a\nmultiline password!" },
        { name: "Bartholomew \"Bart\" Simpson", password: "Milhouse" },
        { name: "Foo", password: "r4\\nd\\\\om\"123!" }
      ];
      
      var actual = getQueryResults("FOR i IN UnitTestsImportCsv3 SORT i.name RETURN i");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test csv import
////////////////////////////////////////////////////////////////////////////////
    
    testCsvImport4 : function () {
      var expected = [ 
        { name: "Bar", password: "wow!\nthis is a\nmultiline password!" },
        { name: "Bartholomew \"Bart\" Simpson", password: "Milhouse" },
        { name: "Foo", password: "r4\\nd\\\\om\"123!" }
      ];
      
      var actual = getQueryResults("FOR i IN UnitTestsImportCsv4 SORT i.name RETURN i");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test csv import
////////////////////////////////////////////////////////////////////////////////
    
    testCsvImport5 : function () {
      var expected = [ 
        { name: "bar", password: "foo" },
        { name: "foo\t", password: "wow \t   \r\nthis is \t\ra multiline password!" }
      ];

      var actual = getQueryResults("FOR i IN UnitTestsImportCsv5 SORT i.name RETURN i");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test csv import
////////////////////////////////////////////////////////////////////////////////
    
    testCsvImport6 : function () {
      var expected = [ 
        { a: 1, b: 2, c: 3, d: 4, e: 5 },
        { a: 1, b: 2, c: 3, d: 4 },
        { a: 1, b: 2, c: 3 },
        { a: 1, b: 2 },
        { a: 1 }
      ];

      var actual = getQueryResults("FOR i IN UnitTestsImportCsv6 SORT TO_NUMBER(i._key) RETURN i");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test csv import
////////////////////////////////////////////////////////////////////////////////
    
    testCsvImportHeaders : function () {
     let expected = [ 
        { "a": "1", "b": 1, "c": "1.3", "e": -5, "id": 1 }, 
        { "b": "", "c": 3.1, "d": -2.5, "e": "ddd \" ' ffd", "id": 2 }, 
        { "a": "9999999999999999999999999999999999", "b": "test", "c" : -99999999, "d": true, "e": -888.4434, "id": 5 },
        { "a": 10e4, "b": 20.5, "c": -42, "d": " null ", "e": false, "id": 6 },
        { "a": -1.05e2, "b": 1.05e-2, "c": true, "d": false, "id": 7 }
      ];
      let actual = getQueryResults("FOR i IN UnitTestsImportCsvHeaders SORT i.id RETURN i");
      assertEqual(expected, actual);
    },
    
    testCsvImportBrokenHeaders : function () {
      let actual = getQueryResults("FOR i IN UnitTestsImportCsvBrokenHeaders RETURN i");
      assertEqual([], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test csv import with converting
////////////////////////////////////////////////////////////////////////////////
    
    testCsvImportConvert : function () {
      var expected = [ 
        { value1: 1 },
        { value1: 2, value2: "null" },
        { value1: 3, value2: false },
        { value1: 4, value2: "false" },
        { value1: 5, value2: true },
        { value1: 6, value2: "true" },
        { value1: 7, value2: 1 },
        { value1: 8, value2: 2 },
        { value1: 9, value2: "3" },
        { value1: 10, value2: "a" },
        { value1: 11, value2: "b" },
        { value1: 12, value2: "c" },
        { value1: 13, value2: " a" },
        { value1: 14, value2: " b" },
        { value1: 15, value2: -1 },
        { value1: 16, value2: "-2" },
        { value1: 17, value2: -0.5 },
        { value1: 18, value2: "-.7" },
        { value1: 19, value2: 0.8 },
        { value1: 20, value2: ".9" },
        { value1: 21, value2: 3.566 },
        { value1: 22, value2: "7.899" },
        { value1: 23, value2: 5000000 },
        { value1: 24, value2: "5e6" },
        { value1: 25, value2: 0 },
        { value1: 26, value2: "0" },
        { value1: 27 },
        { value1: 28, value2: "" },
        { value1: 29, value2: "       c" },
        { value1: 30, value2: "       d" },
        { value1: 31, value2: "       1" },
        { value1: 32, value2: "       2" },
        { value1: 33, value2: "e       " },
        { value1: 34, value2: "d       " },
        { value1: 35, value2: "       " },
        { value1: 36, value2: "       " },
        { value1: 37, value2: "\"true\"" }
      ];

      var actual = getQueryResults("FOR i IN UnitTestsImportCsvConvert SORT i.value1 RETURN i");
      assertEqual(JSON.stringify(expected), JSON.stringify(actual));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test csv import without converting
////////////////////////////////////////////////////////////////////////////////
    
    testCsvImportNoConvert : function () {
      var expected = [ 
        { value1: "1", value2: "null" },
        { value1: "2", value2: "null" },
        { value1: "3", value2: "false" },
        { value1: "4", value2: "false" },
        { value1: "5", value2: "true" },
        { value1: "6", value2: "true" },
        { value1: "7", value2: "1" },
        { value1: "8", value2: "2" },
        { value1: "9", value2: "3" },
        { value1: "10", value2: "a" },
        { value1: "11", value2: "b" },
        { value1: "12", value2: "c" },
        { value1: "13", value2: " a" },
        { value1: "14", value2: " b" },
        { value1: "15", value2: "-1" },
        { value1: "16", value2: "-2" },
        { value1: "17", value2: "-.5" },
        { value1: "18", value2: "-.7" },
        { value1: "19", value2: ".8" },
        { value1: "20", value2: ".9" },
        { value1: "21", value2: "3.566" },
        { value1: "22", value2: "7.899" },
        { value1: "23", value2: "5e6" },
        { value1: "24", value2: "5e6" },
        { value1: "25", value2: "0" },
        { value1: "26", value2: "0" },
        { value1: "27" },
        { value1: "28", value2: "" },
        { value1: "29", value2: "       c" },
        { value1: "30", value2: "       d" },
        { value1: "31", value2: "       1" },
        { value1: "32", value2: "       2" },
        { value1: "33", value2: "e       " },
        { value1: "34", value2: "d       " },
        { value1: "35", value2: "       " },
        { value1: "36", value2: "       " },
        { value1: "37", value2: "\"true\"" }
      ];

      var actual = getQueryResults("FOR i IN UnitTestsImportCsvNoConvert SORT TO_NUMBER(i.value1) RETURN i");
      assertEqual(JSON.stringify(expected), JSON.stringify(actual));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test csv import with fixed types
////////////////////////////////////////////////////////////////////////////////
    
    testCsvImportTypesBoolean : function () {
      let expected = [
        {
          "id" : "1",
          "value" : false
        },
        {
          "id" : "2",
          "value" : false
        },
        {
          "id" : "3",
          "value" : true
        },
        {
          "id" : "4",
          "value" : true
        },
        {
          "id" : "5",
          "value" : false
        },
        {
          "id" : "6",
          "value" : true
        },
        {
          "id" : "7",
          "value" : true
        },
        {
          "id" : "8",
          "value" : true
        },
        {
          "id" : "9",
          "value" : true
        },
        {
          "id" : "10",
          "value" : false
        },
        {
          "id" : "11",
          "value" : true
        },
        {
          "id" : "12",
          "value" : false
        },
        {
          "id" : "13",
          "value" : true
        },
        {
          "id" : "14",
          "value" : true
        },
        {
          "id" : "15",
          "value" : true
        },
        {
          "id" : "16",
          "value" : true
        },
        {
          "id" : "17",
          "value" : true
        },
        {
          "id" : "18",
          "value" : true
        },
        {
          "id" : "19",
          "value" : true
        },
        {
          "id" : "20",
          "value" : true
        },
        {
          "id" : "21",
          "value" : false
        },
        {
          "id" : "22",
          "value" : false
        },
        {
          "id" : "23",
          "value" : true
        }
      ];

      let actual = getQueryResults("FOR i IN UnitTestsImportCsvTypesBoolean SORT TO_NUMBER(i.id) RETURN i");
      assertEqual(JSON.stringify(expected), JSON.stringify(actual));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test csv import with fixed types
////////////////////////////////////////////////////////////////////////////////
    
    testCsvImportTypesNumber : function () {
      let expected = [ 
        { 
          "id" : "1", 
          "value" : 0 
        }, 
        { 
          "id" : "2", 
          "value" : 0 
        }, 
        { 
          "id" : "3", 
          "value" : 0 
        }, 
        { 
          "id" : "4", 
          "value" : -1 
        }, 
        { 
          "id" : "5", 
          "value" : 0 
        }, 
        { 
          "id" : "6", 
          "value" : 1 
        }, 
        { 
          "id" : "7", 
          "value" : 2 
        }, 
        { 
          "id" : "8", 
          "value" : 123456.43 
        }, 
        { 
          "id" : "9", 
          "value" : -13323.322 
        }, 
        { 
          "id" : "10", 
          "value" : 0 
        }, 
        { 
          "id" : "11", 
          "value" : -1 
        }, 
        { 
          "id" : "12", 
          "value" : 0 
        }, 
        { 
          "id" : "13", 
          "value" : 1 
        }, 
        { 
          "id" : "14", 
          "value" : 2 
        }, 
        { 
          "id" : "15", 
          "value" : 3 
        }, 
        { 
          "id" : "16", 
          "value" : -1443.4442 
        }, 
        { 
          "id" : "17", 
          "value" : 462.66425 
        }, 
        { 
          "id" : "18", 
          "value" : 0 
        }, 
        { 
          "id" : "20", 
          "value" : 0 
        }, 
        { 
          "id" : "21", 
          "value" : 0 
        }, 
        { 
          "id" : "22", 
          "value" : 0 
        }, 
        { 
          "id" : "23", 
          "value" : 0 
        } 
      ];

      let actual = getQueryResults("FOR i IN UnitTestsImportCsvTypesNumber SORT TO_NUMBER(i.id) RETURN i");
      assertEqual(JSON.stringify(expected), JSON.stringify(actual));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test csv import with fixed types
////////////////////////////////////////////////////////////////////////////////
    
    testCsvImportTypesString : function () {
      let expected = [
        { 
          "id" : "1", 
          "value" : "null" 
        }, 
        { 
          "id" : "2", 
          "value" : "false" 
        }, 
        { 
          "id" : "3", 
          "value" : "true" 
        }, 
        { 
          "id" : "4", 
          "value" : "-1" 
        }, 
        { 
          "id" : "5", 
          "value" : "0" 
        }, 
        { 
          "id" : "6", 
          "value" : "1" 
        }, 
        { 
          "id" : "7", 
          "value" : "2" 
        }, 
        { 
          "id" : "8", 
          "value" : "123456.43" 
        }, 
        { 
          "id" : "9", 
          "value" : "-13323.322" 
        }, 
        { 
          "id" : "10", 
          "value" : "null" 
        }, 
        { 
          "id" : "11", 
          "value" : "-1" 
        }, 
        { 
          "id" : "12", 
          "value" : "0" 
        }, 
        { 
          "id" : "13", 
          "value" : "1" 
        }, 
        { 
          "id" : "14", 
          "value" : "2" 
        }, 
        { 
          "id" : "15", 
          "value" : "3" 
        }, 
        { 
          "id" : "16", 
          "value" : "-1443.4442" 
        }, 
        { 
          "id" : "17", 
          "value" : "462.664250" 
        }, 
        { 
          "id" : "18", 
          "value" : "foo" 
        }, 
        { 
          "id" : "19", 
          "value" : "" 
        }, 
        { 
          "id" : "20", 
          "value" : " " 
        }, 
        { 
          "id" : "21", 
          "value" : "null" 
        }, 
        { 
          "id" : "22", 
          "value" : "false" 
        }, 
        { 
          "id" : "23", 
          "value" : "true" 
        } 
      ];

      let actual = getQueryResults("FOR i IN UnitTestsImportCsvTypesString SORT TO_NUMBER(i.id) RETURN i");
      assertEqual(JSON.stringify(expected), JSON.stringify(actual));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test csv import with fixed types, overriding --convert for value attr
////////////////////////////////////////////////////////////////////////////////
    
    testCsvImportTypesPrecedence : function () {
      let expected = [
        { 
          "id" : 1, 
          "value" : "null" 
        }, 
        { 
          "id" : 2, 
          "value" : "false" 
        }, 
        { 
          "id" : 3, 
          "value" : "true" 
        }, 
        { 
          "id" : 4, 
          "value" : "-1" 
        }, 
        { 
          "id" : 5, 
          "value" : "0" 
        }, 
        { 
          "id" : 6, 
          "value" : "1" 
        }, 
        { 
          "id" : 7, 
          "value" : "2" 
        }, 
        { 
          "id" : 8, 
          "value" : "123456.43" 
        }, 
        { 
          "id" : 9, 
          "value" : "-13323.322" 
        }, 
        { 
          "id" : 10, 
          "value" : "null" 
        }, 
        { 
          "id" : 11, 
          "value" : "-1" 
        }, 
        { 
          "id" : 12, 
          "value" : "0" 
        }, 
        { 
          "id" : 13, 
          "value" : "1" 
        }, 
        { 
          "id" : 14, 
          "value" : "2" 
        }, 
        { 
          "id" : 15, 
          "value" : "3" 
        }, 
        { 
          "id" : 16, 
          "value" : "-1443.4442" 
        }, 
        { 
          "id" : 17, 
          "value" : "462.664250" 
        }, 
        { 
          "id" : 18, 
          "value" : "foo" 
        }, 
        { 
          "id" : 19, 
          "value" : "" 
        }, 
        { 
          "id" : 20, 
          "value" : " " 
        }, 
        { 
          "id" : 21, 
          "value" : "null" 
        }, 
        { 
          "id" : 22, 
          "value" : "false" 
        }, 
        { 
          "id" : 23, 
          "value" : "true" 
        } 
      ];

      let actual = getQueryResults("FOR i IN UnitTestsImportCsvTypesPrecedence SORT TO_NUMBER(i.id) RETURN i");
      assertEqual(JSON.stringify(expected), JSON.stringify(actual));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test csv import without trailing eol
////////////////////////////////////////////////////////////////////////////////
    
    testCsvImportNoEol : function () {
      var expected = [ 
        { value1: "a", value2: "b" },
        { value1: "c", value2: "d" },
        { value1: "e", value2: "f" }
      ];

      var actual = getQueryResults("FOR i IN UnitTestsImportCsvNoEol SORT i.value1 RETURN i");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test tsv import
////////////////////////////////////////////////////////////////////////////////
    
    testTsvImport1 : function () {
      var expected = [ { "ab": "g", "cd": "h", "ef": "i" }, { "ab" : "j", "cd" : "k", "ef" : "l" } ];
      var actual = getQueryResults("FOR i IN UnitTestsImportTsv1 SORT i.ab RETURN i");

      assertEqual(expected, actual);
    },

    testTsvImport1Gz : function () {
      var expected = [ { "ab": "g", "cd": "h", "ef": "i" }, { "ab" : "j", "cd" : "k", "ef" : "l" } ];
      var actual = getQueryResults("FOR i IN UnitTestsImportTsv1Gz SORT i.ab RETURN i");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test tsv import
////////////////////////////////////////////////////////////////////////////////
    
    testTsvImport2 : function () {
      var expected = [ { "  fox" : "dog",
                         "brown  " : " the lazy",
                         "the quick": " jumped over" },
                       { "  fox" : " end",
                         "brown  " : "\"\"\"",
                         "the quick" : "\"'.,;" } ];

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
    },

    testJsonEdgesImportGz : function () {
      var expected = [
        { _from : "UnitTestsImportVertex/v1", _to: "UnitTestsImportVertex/v2", id: 1, what: "v1->v2" },
        { _from : "UnitTestsImportVertex/v2", _to: "UnitTestsImportVertex/v3", id: 2, what: "v2->v3" },
        { _from : "UnitTestsImportVertex/v9", _to: "UnitTestsImportVertex/v4", extra: "foo", id: 3, what: "v9->v4" },
        { _from : "UnitTestsImportVertex/v12", _to: "UnitTestsImportVertex/what", id: 4 }
      ];

      var actual = getQueryResults("FOR i IN UnitTestsImportEdgeGz SORT i.id RETURN i");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test unique constraint violations
////////////////////////////////////////////////////////////////////////////////
    
    testJsonIgnore : function () {
      var expected = [
        { "_key" : "test1", "value" : "abc" }, 
        { "_key" : "test3", "value" : "def" }, 
        { "_key" : "test4", "value" : "xyz" }, 
        { "_key" : "test6", "value" : "123" },
        { "_key" : "test7", "value" : "999" } 
      ];

      var actual = getQueryResults("FOR i IN UnitTestsImportIgnore SORT i._key RETURN i", true);
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test unique constraint violations
////////////////////////////////////////////////////////////////////////////////
    
    testJsonUniqueConstraints : function () {
      var expected = [ 
        { "_key" : "test1", "value" : "foo" }, 
        { "_key" : "test3", "value" : "def" }, 
        { "_key" : "test4", "value" : "xyz" }, 
        { "_key" : "test6", "value" : "234" }, 
        { "_key" : "test7", "value" : "999" }, 
        { "_key" : "test8", "value" : "aaa" } 
      ];

      var actual = getQueryResults("FOR i IN UnitTestsImportUniqueConstraints SORT i._key RETURN i", true);
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test csv import removing attribute
////////////////////////////////////////////////////////////////////////////////
        
    testCsvImportRemoveAttribute : function () {
      var expected = [ 
        { "b": 1, "c": "1.3", "e": -5, "id": 1 }, 
        { "b": "", "c": 3.1, "d": -2.5, "e": "ddd \" ' ffd", "id": 2 }, 
        { "b": "test", "c" : -99999999, "d": true, "e": -888.4434, "id": 5 },
        { "b": 20.5, "c": -42, "d": " null ", "e": false, "id": 6 },
        { "b": 1.05e-2, "c": true, "d": false, "id": 7 }
      ];

      var actual = getQueryResults("FOR i IN UnitTestsImportRemoveAttribute SORT i.id RETURN i");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test database creation
////////////////////////////////////////////////////////////////////////////////
    testCreateDatabase : function () {
      var db = require("@arangodb").db;
      try {
        var testdb = db._useDatabase("UnitTestImportCreateDatabase");
        var expected = [ { "id": 1,
                           "one": 1,
                           "three": 3,
                           "two": 2 },
                         { "a": 1234,
                           "b": "the quick fox",
                           "id": 2,
                           "jumped":
                           "over the fox",
                           "null": null },
                         { "id": 3,
                           "not": "important",
                           "spacing": "is" },
                         { "  c  ": "h\"'ihi",
                           "a": true,
                           "b": false,
                           "d": "",
                           "id": 4 },
                         { "id": 5 } ];

        var actual = getQueryResults("FOR i IN UnitTestsImportJson1 SORT i.id RETURN i");
        assertEqual(expected, actual);
      } finally {
        db._useDatabase("_system");
      }
    },
    
    testImportDatabaseUnicode : function () {
      let db = require("@arangodb").db;
      let dbs = ['maçã', '😀'];
      try {
        dbs.forEach((name) => {
          let testdb = db._useDatabase(name);
          let expected = [ { "id": 1,
                             "one": 1,
                             "three": 3,
                             "two": 2 },
                           { "a": 1234,
                             "b": "the quick fox",
                             "id": 2,
                             "jumped":
                             "over the fox",
                             "null": null },
                           { "id": 3,
                             "not": "important",
                             "spacing": "is" },
                           { "  c  ": "h\"'ihi",
                             "a": true,
                             "b": false,
                             "d": "",
                             "id": 4 },
                           { "id": 5 } ];

          let actual = getQueryResults("FOR i IN UnitTestsImportJson1 SORT i.id RETURN i");
          assertEqual(expected, actual);
        });
      } finally {
        db._useDatabase("_system");
      }
    },
    
    testCreateDatabaseUnicode : function () {
      let db = require("@arangodb").db;
      let dbs = ['ﻚﻠﺑ ﻞﻄﻴﻓ', 'abc mötor !" \' & <>'];
      try {
        dbs.forEach((name) => {
          let testdb = db._useDatabase(name);
          let expected = [ { "id": 1,
                             "one": 1,
                             "three": 3,
                             "two": 2 },
                           { "a": 1234,
                             "b": "the quick fox",
                             "id": 2,
                             "jumped":
                             "over the fox",
                             "null": null },
                           { "id": 3,
                             "not": "important",
                             "spacing": "is" },
                           { "  c  ": "h\"'ihi",
                             "a": true,
                             "b": false,
                             "d": "",
                             "id": 4 },
                           { "id": 5 } ];

          let actual = getQueryResults("FOR i IN UnitTestsImportJson1 SORT i.id RETURN i");
          assertEqual(expected, actual);
        });
      } finally {
        db._useDatabase("_system");
      }
    },

  };
}

jsunity.run(importTestSuite);
return jsunity.done();
