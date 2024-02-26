/*jshint globalstrict:false, strict:false, maxlen: 700 */
/*global assertEqual, assertNotEqual, arango */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
// / Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
// /
// / Licensed under the Business Source License 1.1 (the "License");
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     https://github.com/arangodb/arangodb/blob/devel/LICENSE
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
/// @author Jan Steemann
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
var db = require("@arangodb").db;
const executeJson =  require("@arangodb/aql-helper").executeJson;

function arrayAccessTestSuite () {
  const persons = [
    { name: "sir alfred", age: 60, loves: [ "lettuce", "flowers" ] }, 
    { person: { name: "gadgetto", age: 50, loves: "gadgets" } }, 
    { name: "everybody", loves: "sunshine" }, 
    { name: "judge", loves: [ "order", "policing", "weapons" ] },
    "something"
  ];

  const continents = [
    { 
      name: "Europe", countries: [ 
        { id: "UK", capital: "London" }, 
        { id: "FR", capital: "Paris" }, 
        { id: "IT", capital: "Rome" } 
      ] 
    },
    { 
      name: "Asia", countries: [ 
        { id: "JP", capital: "Tokyo" }, 
        { id: "CN", capital: "Beijing" }, 
        { id: "KR", capital: "Seoul" }, 
        { id: "IN", capital: "Delhi" } 
      ] 
    }
  ];
  
  const tags = [ 
    { 
      values: [ 
        { name: "javascript", count: 2, payload: [ { name: "foo" }, { name: "bar" } ] }, 
        { name: "cpp", count: 4, payload: [ { name: "baz" }, { name: "qux" } ] }, 
        { name: "php", count: 2, payload: [ ] }, 
        { name: "ruby", count: 5, payload: [ { name: "test" }, { name: 23 } ] },
        { name: "python", count: 3, payload: [ { name: "one" }, { name: "two" }, { name: "three" }, { name: "four" } ] }
      ]
    }
  ]; 

  const arrayOfArrays = [
    [  
      [ "one", "two", "three" ],
      [ "four", "five" ],
      [ "six" ]
    ],
    [  
      [ "seven", "eight" ]
    ],
    [
      [ "nine", "ten" ],
      [ "eleven", "twelve" ]
    ]
  ];

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief test non-array access
////////////////////////////////////////////////////////////////////////////////

    testNonArray1 : function () {
      var result = db._query("RETURN { foo: 'bar' }[0]").toArray();
      assertEqual([ null ], result);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test non-array access
////////////////////////////////////////////////////////////////////////////////

    testNonArray2 : function () {
      var result = db._query("RETURN { foo: 'bar' }[99]").toArray();
      assertEqual([ null ], result);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test non-array access
////////////////////////////////////////////////////////////////////////////////

    testNonArray3 : function () {
      var result = db._query("RETURN { foo: 'bar' }[-1]").toArray();
      assertEqual([ null ], result);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test non-array access
////////////////////////////////////////////////////////////////////////////////

    testNonArray4 : function () {
      var result = db._query("RETURN NOOPT(PASSTHRU({ foo: 'bar' }))[-1]").toArray();
      assertEqual([ null ], result);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test non-array access
////////////////////////////////////////////////////////////////////////////////

    testNonArray5 : function () {
      var result = db._query("RETURN NOOPT(PASSTHRU(null))[-1]").toArray();
      assertEqual([ null ], result);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test non-array access
////////////////////////////////////////////////////////////////////////////////

    testNonArray6 : function () {
      var result = db._query("RETURN NOOPT(PASSTHRU('foobar'))[2]").toArray();
      assertEqual([ null ], result);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test non-array access
////////////////////////////////////////////////////////////////////////////////

    testNonArrayRange1 : function () {
      var result = db._query("RETURN { foo: 'bar' }[0..1]").toArray();
      assertEqual([ null ], result);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test non-array access
////////////////////////////////////////////////////////////////////////////////

    testNonArrayRange2 : function () {
      var result = db._query("RETURN { foo: 'bar' }[-2..-1]").toArray();
      assertEqual([ null ], result);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test subquery result
////////////////////////////////////////////////////////////////////////////////

    testSubqueryResult : function () {
      for (var i = 0; i < persons.length; ++i) {
        var result = db._query("RETURN (FOR value IN @persons RETURN value)[" + i + "]", { persons: persons }).toArray();
        assertEqual([ persons[i] ], result);
      }
    },

    testQuestionMarkEmptyArray : function() {
      var expected = [ false ];
      var result = db._query("RETURN [][?]").toArray();
      assertEqual(expected, result);
    },

    testMulitpleQuestionMarkEmptyArray : function() {
      var expected = [ false ];
      var result = db._query("RETURN [][????]").toArray();
      assertEqual(expected, result);
    },

    testQuestionMarkNonEmptyArray : function() {
      var expected = [ true ];
      var result = db._query("RETURN [42][?]").toArray();
      assertEqual(expected, result);
    },

    testQuestionMarkWithFilter: function() {
      var expected = [ true ];
      var result = db._query("RETURN @value[? FILTER CURRENT.name == 'Europe']", { value : continents }).toArray();
      assertEqual(expected, result);
    },

    testMultipleQuestionMarkWithFilter: function() {
      var expected = [ true ];
      var result = db._query("RETURN @value[???????????? FILTER CURRENT.name == 'Europe']", { value : continents }).toArray();
      assertEqual(expected, result);
    },

    testQuestionMarkWithNonMatchingFilter: function() {
      var expected = [ false ];
      var result = db._query("RETURN @value[? FILTER CURRENT.name == 'foobar']", { value : continents }).toArray();
      assertEqual(expected, result);
    },
    
    testQuestionMarkWithFilterAndQuantifier: function() {
      const values = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 ];
      let result;
  
      // range quantifiers
      result = db._query("LET values = @values RETURN values[? 1..1 FILTER CURRENT == 3]", { values }).toArray();
      assertEqual([ true ], result);
      
      result = db._query("LET values = @values RETURN values[? 1..2 FILTER CURRENT == 3]", { values }).toArray();
      assertEqual([ true ], result);
      
      result = db._query("LET values = @values RETURN values[? 1..10 FILTER CURRENT == 3]", { values }).toArray();
      assertEqual([ true ], result);
      
      result = db._query("LET values = @values RETURN values[? 2..2 FILTER CURRENT == 3]", { values }).toArray();
      assertEqual([ false ], result);
      
      result = db._query("LET values = @values RETURN values[? 2..5 FILTER CURRENT == 3]", { values }).toArray();
      assertEqual([ false ], result);

      result = db._query("LET values = @values RETURN values[? 1..1 FILTER CURRENT >= 3]", { values }).toArray();
      assertEqual([ false ], result);
      
      result = db._query("LET values = @values RETURN values[? 1..2 FILTER CURRENT >= 3]", { values }).toArray();
      assertEqual([ false ], result);
      
      result = db._query("LET values = @values RETURN values[? 1..10 FILTER CURRENT >= 3]", { values }).toArray();
      assertEqual([ true ], result);
      
      result = db._query("LET values = @values RETURN values[? 2..2 FILTER CURRENT >= 3]", { values }).toArray();
      assertEqual([ false ], result);
      
      result = db._query("LET values = @values RETURN values[? 2..5 FILTER CURRENT >= 3]", { values }).toArray();
      assertEqual([ false ], result);
      
      result = db._query("LET values = @values RETURN values[? 9..9 FILTER CURRENT >= 3]", { values }).toArray();
      assertEqual([ true ], result);
      
      result = db._query("LET values = @values RETURN values[? 8..9 FILTER CURRENT >= 3]", { values }).toArray();
      assertEqual([ true ], result);
      
      result = db._query("LET values = @values RETURN values[? 8..10 FILTER CURRENT >= 3]", { values }).toArray();
      assertEqual([ true ], result);
      
      result = db._query("LET values = @values RETURN values[? 9..11 FILTER CURRENT >= 3]", { values }).toArray();
      assertEqual([ true ], result);
      
      result = db._query("LET values = @values RETURN values[? 10..11 FILTER CURRENT >= 3]", { values }).toArray();
      assertEqual([ false ], result);
      
      // range quantifiers with bind parameters
      result = db._query("LET values = @values RETURN values[? @lower..@upper FILTER CURRENT >= 3]", { values, lower: 9, upper: 11 }).toArray();
      assertEqual([ true ], result);
      
      result = db._query("LET values = @values RETURN values[? @lower..@upper FILTER CURRENT >= 3]", { values, lower: 1, upper: 15 }).toArray();
      assertEqual([ true ], result);
      
      result = db._query("LET values = @values RETURN values[? @lower..@upper FILTER CURRENT <= 3]", { values, lower: -1, upper: 4 }).toArray();
      assertEqual([ true ], result);
      
      result = db._query("LET values = @values RETURN values[? @lower..@upper FILTER CURRENT >= 3]", { values, lower: 9, upper: 10 }).toArray();
      assertEqual([ true ], result);
      
      result = db._query("LET values = @values RETURN values[? @lower..@upper FILTER CURRENT >= 3]", { values, lower: -3, upper: 10 }).toArray();
      assertEqual([ true ], result);
      
      result = db._query("LET values = @values RETURN values[? @lower..@upper FILTER CURRENT >= 3]", { values, lower: "piff", upper: 10 }).toArray();
      assertEqual([ true ], result);
      
      // range quantifiers with expressions
      result = db._query("LET values = @values RETURN values[? NOOPT(4)..NOOPT(9) FILTER CURRENT >= 3]", { values }).toArray();
      assertEqual([ true ], result);
      
      result = db._query("FOR i IN 1..5 LET values = @values RETURN values[? i..(i + 2) FILTER CURRENT >= 6]", { values }).toArray();
      assertEqual([ false, false, false, true, true ], result);

      result = db._query("LET values = @values, x = NOOPT(1), y = NOOPT(5) RETURN values[? x..y FILTER CURRENT >= 3]", { values }).toArray();
      assertEqual([ false ], result);

      result = db._query("LET values = @values, x = 1, y = NOOPT(5) RETURN values[? x..y FILTER CURRENT >= 3]", { values }).toArray();
      assertEqual([ false ], result);

      result = db._query("LET values = @values, x = NOOPT(1), y = 5 RETURN values[? x..y FILTER CURRENT >= 3]", { values }).toArray();
      assertEqual([ false ], result);

      result = db._query("LET values = @values LET bound = ROUND(RAND() * 1000) % 10 RETURN values[? 1..bound FILTER CURRENT <= 0]", { values }).toArray();
      assertEqual([ false ], result);
      
      result = db._query("LET values = [1,2,3,4,5,6,7,8,9,10,11] LET x = 1..5 RETURN values[? x FILTER CURRENT >= 3]").toArray();
      assertEqual([ false ], result);

      // numeric quantifiers
      result = db._query("LET values = @values RETURN values[? 1 FILTER CURRENT == 3]", { values }).toArray();
      assertEqual([ true ], result);
      
      result = db._query("LET values = @values RETURN values[? 1 FILTER CURRENT == 4]", { values }).toArray();
      assertEqual([ true ], result);
      
      result = db._query("LET values = @values RETURN values[? 1 FILTER CURRENT != 3]", { values }).toArray();
      assertEqual([ false ], result);
      
      result = db._query("LET values = @values RETURN values[? 1 FILTER CURRENT == 12]", { values }).toArray();
      assertEqual([ false ], result);
      
      result = db._query("LET values = @values RETURN values[? 1 FILTER CURRENT == 0]", { values }).toArray();
      assertEqual([ false ], result);
      
      result = db._query("LET values = @values RETURN values[? 2 FILTER CURRENT == 1]", { values }).toArray();
      assertEqual([ false ], result);
      
      result = db._query("LET values = @values RETURN values[? 0 FILTER CURRENT == 1]", { values }).toArray();
      assertEqual([ false ], result);

      // numeric quantifiers with bind parameters
      result = db._query("LET values = @values RETURN values[? @value FILTER CURRENT >= 3]", { values, value: 8 }).toArray();
      assertEqual([ false ], result);
      
      result = db._query("LET values = @values RETURN values[? @value FILTER CURRENT >= 3]", { values, value: 9 }).toArray();
      assertEqual([ true ], result);
      
      result = db._query("LET values = @values RETURN values[? @value FILTER CURRENT <= 3]", { values, value: 3 }).toArray();
      assertEqual([ true ], result);
      
      result = db._query("LET values = @values RETURN values[? @value FILTER CURRENT <= 3]", { values, value: 4 }).toArray();
      assertEqual([ false ], result);
      
      // numeric quantifiers with expressions
      result = db._query("LET values = @values RETURN values[? NOOPT(4) FILTER CURRENT >= 3]", { values }).toArray();
      assertEqual([ false ], result);
      
      result = db._query("LET values = @values RETURN values[? NOOPT(4) FILTER CURRENT >= 8]", { values }).toArray();
      assertEqual([ true ], result);
      
      result = db._query("FOR i IN 1..7 LET values = @values RETURN values[? i FILTER CURRENT >= 6]", { values }).toArray();
      assertEqual([ false, false, false, false, false, true, false ], result);
     
      // ALL|ANY|NONE
      result = db._query("LET values = @values RETURN values[? ALL FILTER CURRENT == 1]", { values: [] }).toArray();
      assertEqual([ true ], result);

      result = db._query("LET values = @values RETURN values[? ALL FILTER CURRENT == 1]", { values }).toArray();
      assertEqual([ false ], result);
      
      result = db._query("LET values = @values RETURN values[? ALL FILTER CURRENT <= 11]", { values }).toArray();
      assertEqual([ true ], result);
      
      result = db._query("LET values = @values RETURN values[? ALL FILTER CURRENT >= 1 && CURRENT <= 11]", { values }).toArray();
      assertEqual([ true ], result);
      
      result = db._query("LET values = @values RETURN values[? ALL FILTER CURRENT >= 2 && CURRENT <= 11]", { values }).toArray();
      assertEqual([ false ], result);
      
      result = db._query("LET values = @values RETURN values[? ANY FILTER CURRENT == 1]", { values: [] }).toArray();
      assertEqual([ false ], result);
      
      result = db._query("LET values = @values RETURN values[? ANY FILTER CURRENT == 1]", { values }).toArray();
      assertEqual([ true ], result);
      
      result = db._query("LET values = @values RETURN values[? ANY FILTER CURRENT == 10]", { values }).toArray();
      assertEqual([ true ], result);
      
      result = db._query("LET values = @values RETURN values[? ANY FILTER CURRENT == 12]", { values }).toArray();
      assertEqual([ false ], result);
      
      result = db._query("LET values = @values RETURN values[? ANY FILTER CURRENT >= 5]", { values }).toArray();
      assertEqual([ true ], result);
      
      result = db._query("LET values = @values RETURN values[? NONE FILTER CURRENT == 1]", { values: [] }).toArray();
      assertEqual([ true ], result);
      
      result = db._query("LET values = @values RETURN values[? NONE FILTER CURRENT == 1]", { values }).toArray();
      assertEqual([ false ], result);
      
      result = db._query("LET values = @values RETURN values[? NONE FILTER CURRENT == 11]", { values }).toArray();
      assertEqual([ false ], result);
      
      result = db._query("LET values = @values RETURN values[? NONE FILTER CURRENT == 12]", { values }).toArray();
      assertEqual([ true ], result);
      
      result = db._query("LET values = @values RETURN values[? NONE FILTER CURRENT <= 11]", { values }).toArray();
      assertEqual([ false ], result);
      
      result = db._query("LET values = @values RETURN values[? NONE FILTER CURRENT >= 12]", { values }).toArray();
      assertEqual([ true ], result);
      
      result = db._query("LET values = @values RETURN values[? AT LEAST (0) FILTER CURRENT == 1]", { values }).toArray();
      assertEqual([ true ], result);
      
      result = db._query("LET values = @values RETURN values[? AT LEAST (1) FILTER CURRENT == 1]", { values }).toArray();
      assertEqual([ true ], result);
      
      result = db._query("LET values = @values RETURN values[? AT LEAST (2) FILTER CURRENT == 1]", { values }).toArray();
      assertEqual([ false ], result);
      
      result = db._query("LET values = @values RETURN values[? AT LEAST (1) FILTER CURRENT == 12]", { values }).toArray();
      assertEqual([ false ], result);
      
      result = db._query("LET values = @values RETURN values[? AT LEAST (1) FILTER CURRENT != 1]", { values }).toArray();
      assertEqual([ true ], result);
      
      result = db._query("LET values = @values RETURN values[? AT LEAST (10) FILTER CURRENT != 1]", { values }).toArray();
      assertEqual([ true ], result);
      
      result = db._query("LET values = @values RETURN values[? AT LEAST (15) FILTER CURRENT != 1]", { values }).toArray();
      assertEqual([ false ], result);
      
      result = db._query("LET values = @values RETURN values[? AT LEAST (5) FILTER CURRENT < 10]", { values }).toArray();
      assertEqual([ true ], result);
      
      result = db._query("LET values = @values RETURN values[? AT LEAST (5) FILTER CURRENT < 4]", { values }).toArray();
      assertEqual([ false ], result);
      
      result = db._query("LET values = @values RETURN values[? AT LEAST (5) FILTER CURRENT <= 10]", { values }).toArray();
      assertEqual([ true ], result);
      
      result = db._query("LET values = @values RETURN values[? AT LEAST (5) FILTER CURRENT <= 4]", { values }).toArray();
      assertEqual([ false ], result);
      
      result = db._query("LET values = @values RETURN values[? AT LEAST (3) FILTER CURRENT >= 8]", { values }).toArray();
      assertEqual([ true ], result);
      
      result = db._query("LET values = @values RETURN values[? AT LEAST (5) FILTER CURRENT >= 8]", { values }).toArray();
      assertEqual([ false ], result);
      
      result = db._query("LET values = @values RETURN values[? AT LEAST (3) FILTER CURRENT > 8]", { values }).toArray();
      assertEqual([ true ], result);
      
      result = db._query("LET values = @values RETURN values[? AT LEAST (5) FILTER CURRENT > 8]", { values }).toArray();
      assertEqual([ false ], result);
    },

    testStarExtractScalar : function () {
      var expected = [ "Europe", "Asia" ];
      var result = db._query("RETURN @value[*].name", { value : continents }).toArray();
      assertEqual([ expected ], result);
    },

    testStarExtractScalarUndefinedAttribute : function () {
      var expected = [ null, null ];
      var result = db._query("RETURN @value[*].foobar", { value : continents }).toArray();
      assertEqual([ expected ], result);
    },

    testStarExtractScalarWithFilter : function () {
      var expected = [ "Europe" ];
      var result = db._query("RETURN @value[* FILTER CURRENT.name == 'Europe'].name", { value : continents }).toArray();
      assertEqual([ expected ], result);
    },

    testStarExtractScalarWithNonMatchingFilter : function () {
      var expected = [ ];
      var result = db._query("RETURN @value[* FILTER CURRENT.name == 'foobar'].name", { value : continents }).toArray();
      assertEqual([ expected ], result);
    },
    
    testStarExtractArray : function () {
      var expected = [ 
        [
          { id: "UK", capital: "London" }, 
          { id: "FR", capital: "Paris" }, 
          { id: "IT", capital: "Rome" } 
        ],
        [
          { id: "JP", capital: "Tokyo" }, 
          { id: "CN", capital: "Beijing" }, 
          { id: "KR", capital: "Seoul" }, 
          { id: "IN", capital: "Delhi" }
        ]
      ];
 
      var result = db._query("RETURN @value[*].countries", { value : continents }).toArray();
      assertEqual([ expected ], result);
    },

    testStarExtractArrayWithFilter : function () {
      var expected = [ continents[0].countries ];
      var result = db._query("RETURN @value[* FILTER CURRENT.name == 'Europe'].countries", { value : continents }).toArray();
      assertEqual([ expected ], result);
    },

    testStarExtractSubArrayIndexed1 : function () {
      var expected = [ 
        { id: "UK", capital: "London" }, 
        { id: "JP", capital: "Tokyo" } 
      ];
      var result = db._query("RETURN @value[*].countries[0]", { value : continents }).toArray();
      assertEqual([ expected ], result);
    },

    testStarExtractSubArrayIndexed2 : function () {
      var expected = [ 
        { id: "IT", capital: "Rome" }, 
        { id: "IN", capital: "Delhi" }
      ];
      var result = db._query("RETURN @value[*].countries[-1]", { value : continents }).toArray();
      assertEqual([ expected ], result);
    },

    testStarExtractSubArrayIndexed3 : function () {
      var expected = [ 
        null,
        null
      ];
      var result = db._query("RETURN @value[*].countries[99]", { value : continents }).toArray();
      assertEqual([ expected ], result);
    },

    testStarExtractSubArrayIndexedTotal1 : function () {
      var expected = [ 
        { id: "UK", capital: "London" }, 
        { id: "FR", capital: "Paris" }, 
        { id: "IT", capital: "Rome" } 
      ];
      var result = db._query("RETURN (@value[*].countries)[0]", { value : continents }).toArray();
      assertEqual([ expected ], result);
    },

    testStarExtractSubArrayIndexedTotal2 : function () {
      var expected = [ 
        { id: "JP", capital: "Tokyo" }, 
        { id: "CN", capital: "Beijing" }, 
        { id: "KR", capital: "Seoul" }, 
        { id: "IN", capital: "Delhi" }
      ];
      var result = db._query("RETURN (@value[*].countries)[-1]", { value : continents }).toArray();
      assertEqual([ expected ], result);
    },

    testStarExtractSubAttribute1 : function () {
      var expected = [ 
        [ "UK", "FR", "IT" ],
        [ "JP", "CN", "KR", "IN" ]
      ];
      var result = db._query("RETURN @value[*].countries[*].id", { value : continents }).toArray();
      assertEqual([ expected ], result);
    },

    testStarExtractSubAttribute2 : function () {
      var expected = [ 
        [ "London", "Paris", "Rome" ],
        [ "Tokyo", "Beijing", "Seoul", "Delhi" ]
      ];
      var result = db._query("RETURN @value[*].countries[*].capital", { value : continents }).toArray();
      assertEqual([ expected ], result);
    },

    testStarExtractSubAttributeExtraOperator1 : function () {
      var expected = [ 
        [ "UK", "FR", "IT" ],
        [ "JP", "CN", "KR", "IN" ]
      ];
      var result = db._query("RETURN @value[*].countries[*].id[*]", { value : continents }).toArray();
      assertEqual([ expected ], result);
    },

    testStarExtractSubAttributeExtraOperator2 : function () {
      var expected = [ 
        [ "London", "Paris", "Rome" ],
        [ "Tokyo", "Beijing", "Seoul", "Delhi" ]
      ];
      var result = db._query("RETURN @value[*].countries[*].capital[*]", { value : continents }).toArray();
      assertEqual([ expected ], result);
    },

    testStarExtractSubAttribute1Combine : function () {
      var expected = [ 
        "UK", "FR", "IT", "JP", "CN", "KR", "IN"
      ];
      var result = db._query("RETURN @value[*].countries[**].id", { value : continents }).toArray();
      assertEqual([ expected ], result);
    },

    testStarExtractSubAttribute2Combine : function () {
      var expected = [ 
        "London", "Paris", "Rome", "Tokyo", "Beijing", "Seoul", "Delhi"
      ];
      var result = db._query("RETURN @value[*].countries[**].capital", { value : continents }).toArray();
      assertEqual([ expected ], result);
    },

    testStarExtractSubAttribute3Combine : function () {
      var expected = [ 
        "London", "Paris", "Rome", "Tokyo", "Beijing", "Seoul", "Delhi"
      ];
      // more than 2 asterisks won't change the result
      var result = db._query("RETURN @value[*].countries[****].capital", { value : continents }).toArray();
      assertEqual([ expected ], result);
    },

    testStarExtractSubAttributeNonExisting : function () {
      var expected = [ 
        [ null, null, null ],
        [ null, null, null, null ]
      ];
      var result = db._query("RETURN @value[*].countries[*].id.foobar", { value : continents }).toArray();
      assertEqual([ expected ], result);
    },

    testStarExtractSubAttributeIndexed1 : function () {
      var expected = [ 
        [ null, null, null ],
        [ null, null, null, null ]
      ];
      var result = db._query("RETURN @value[*].countries[*].id[0]", { value : continents }).toArray();
      assertEqual([ expected ], result);
    },

    testStarExtractSubAttributeIndexed2 : function () {
      var expected = [ 
        [ null, null, null ],
        [ null, null, null, null ]
      ];
      var result = db._query("RETURN @value[*].countries[*].id[1]", { value : continents }).toArray();
      assertEqual([ expected ], result);
    },

    testStarExtractSubAttributeIndexedTotal1 : function () {
      var expected = [ 
        "UK", "FR", "IT"
      ];
      var result = db._query("RETURN (@value[*].countries[*].id)[0]", { value : continents }).toArray();
      assertEqual([ expected ], result);
    },

    testStarExtractSubAttributeIndexedTotal2 : function () {
      var expected = [ 
        "JP", "CN", "KR", "IN"
      ];
      var result = db._query("RETURN (@value[*].countries[*].id)[1]", { value : continents }).toArray();
      assertEqual([ expected ], result);
    },

    testStarExtractFiltered1 : function () {
      var expected = [ 
        "javascript", "php"
      ];
      var result = db._query("RETURN @value[0].values[* FILTER CURRENT.count == 2].name", { value : tags }).toArray();
      assertEqual([ expected ], result);
    },

    testStarExtractFiltered2 : function () {
      var expected = [ 
        "cpp", "ruby", "python"
      ];
      var result = db._query("RETURN @value[0].values[* FILTER CURRENT.count != 2].name", { value : tags }).toArray();
      assertEqual([ expected ], result);
    },

    testStarExtractFiltered3 : function () {
      var expected = [ 
        "ruby"
      ];
      var result = db._query("RETURN @value[0].values[* FILTER CURRENT.count != 2 && CURRENT.name == 'ruby'].name", { value : tags }).toArray();
      assertEqual([ expected ], result);
    },

    testStarExtractFiltered4 : function () {
      var expected = [ 
        3
      ];
      var result = db._query("RETURN @value[0].values[* FILTER LENGTH(CURRENT.payload) == 4].count", { value : tags }).toArray();
      assertEqual([ expected ], result);
    },

    testStarExtractFiltered5 : function () {
      var expected = [ 
        "php", "ruby", "python"
      ];
      var result = db._query("RETURN @value[0].values[* FILTER CURRENT.name IN [ 'php', 'python', 'ruby' ]].name", { value : tags }).toArray();
      assertEqual([ expected ], result);
    },

    testStarExtractFiltered6 : function () {
      var expected = [ 
        [ "javascript", "php" ]
      ];
      var result = db._query("RETURN @value[*].values[* FILTER CURRENT.count == 2].name", { value : tags }).toArray();
      assertEqual([ expected ], result);
    },

    testStarExtractFiltered7 : function () {
      var expected = [ 
        [ "cpp", "ruby", "python" ]
      ];
      var result = db._query("RETURN @value[*].values[* FILTER CURRENT.count != 2].name", { value : tags }).toArray();
      assertEqual([ expected ], result);
    },

    testStarExtractFiltered8 : function () {
      var expected = [ 
        [ "ruby" ]
      ];
      var result = db._query("RETURN @value[*].values[* FILTER CURRENT.count != 2 && CURRENT.name == 'ruby'].name", { value : tags }).toArray();
      assertEqual([ expected ], result);
    },

    testStarExtractFiltered9 : function () {
      var expected = [ 
        [ 3 ]
      ];
      var result = db._query("RETURN @value[*].values[* FILTER LENGTH(CURRENT.payload) == 4].count", { value : tags }).toArray();
      assertEqual([ expected ], result);
    },

    testStarExtractFiltered10 : function () {
      var expected = [ 
        [ "php", "ruby", "python" ]
      ];
      var result = db._query("RETURN @value[*].values[* FILTER CURRENT.name IN [ 'php', 'python', 'ruby' ]].name", { value : tags }).toArray();
      assertEqual([ expected ], result);
    },

    testStarExtractProject1 : function () {
      var expected = [ 
        2, 2, 0, 2, 4
      ];
      var result = db._query("RETURN @value[0].values[* RETURN LENGTH(CURRENT.payload)][**]", { value : tags }).toArray();
      assertEqual([ expected ], result);
    },

    testStarExtractProject2 : function () {
      var expected = [ 
        "x-javascript", "x-cpp", "x-php", "x-ruby", "x-python"
      ];
      var result = db._query("RETURN @value[0].values[* RETURN CONCAT('x-', CURRENT.name)][**]", { value : tags }).toArray();
      assertEqual([ expected ], result);
    },

    testStarExtractProjectFilter : function () {
      var expected = [ 
        "x-cpp", "x-ruby", "x-python"
      ];
      var result = db._query("RETURN @value[0].values[* FILTER CURRENT.count >= 3 RETURN CONCAT('x-', CURRENT.name)][**]", { value : tags }).toArray();
      assertEqual([ expected ], result);
    },

    testStarExtractLimitEmpty1 : function () {
      var expected = [ 
      ];
      var result = db._query("RETURN @value[0].values[*].name[** LIMIT 0]", { value : tags }).toArray();
      assertEqual([ expected ], result);
    },

    testStarExtractLimitEmpty2 : function () {
      var expected = [ 
      ];
      var result = db._query("RETURN @value[0].values[* LIMIT 0].name", { value : tags }).toArray();
      assertEqual([ expected ], result);
    },

    testStarExtractLimitEmpty3 : function () {
      var expected = [ 
      ];
      var result = db._query("RETURN @value[0].values[* LIMIT -10, 1].name", { value : tags }).toArray();
      assertEqual([ expected ], result);
    },

    testStarExtractLimit0 : function () {
      var expected = [ 
        "javascript"
      ];
      var result = db._query("RETURN @value[0].values[* LIMIT 1].name", { value : tags }).toArray();
      assertEqual([ expected ], result);
    },

    testStarExtractLimit1 : function () {
      var expected = [ 
        "javascript"
      ];
      var result = db._query("RETURN @value[0].values[*].name[** LIMIT 1]", { value : tags }).toArray();
      assertEqual([ expected ], result);
    },

    testStarExtractLimit2 : function () {
      var expected = [ 
        "javascript"
      ];
      var result = db._query("RETURN @value[0].values[*].name[** LIMIT 0, 1]", { value : tags }).toArray();
      assertEqual([ expected ], result);
    },

    testStarExtractLimit3 : function () {
      var expected = [ 
        "javascript", "cpp", "php", "ruby", "python"
      ];
      var result = db._query("RETURN @value[0].values[*].name[** LIMIT 0, 10]", { value : tags }).toArray();
      assertEqual([ expected ], result);
    },

    testStarExtractLimit4 : function () {
      var expected = [ 
        "javascript", "cpp", "php", "ruby"
      ];
      var result = db._query("RETURN @value[0].values[*].name[** LIMIT 0, 4]", { value : tags }).toArray();
      assertEqual([ expected ], result);
    },

    testStarExtractLimit5 : function () {
      var expected = [ 
        "javascript", "cpp"
      ];
      var result = db._query("RETURN @value[0].values[*].name[** LIMIT 0, 2]", { value : tags }).toArray();
      assertEqual([ expected ], result);
    },

    testStarExtractLimit6 : function () {
      var expected = [ 
        "cpp", "php", "ruby", "python"
      ];
      var result = db._query("RETURN @value[0].values[*].name[** LIMIT 1, 4]", { value : tags }).toArray();
      assertEqual([ expected ], result);
    },

    testStarExtractLimit7 : function () {
      var expected = [ 
        "ruby", "python"
      ];
      var result = db._query("RETURN @value[0].values[*].name[** LIMIT 3, 4]", { value : tags }).toArray();
      assertEqual([ expected ], result);
    },

    testStarExtractLimit8 : function () {
      var expected = [ 
        "javascript", "cpp"
      ];
      var result = db._query("RETURN @value[0].values[* FILTER CURRENT.name IN [ 'javascript', 'php', 'cpp' ] LIMIT 0, 2].name[**]", { value : tags }).toArray();
      assertEqual([ expected ], result);
    },

    testStarExtractLimitProject1 : function () {
      var expected = [ 
        "javascript-x", "cpp-x"
      ];
      var result = db._query("RETURN @value[0].values[** LIMIT 0, 2 RETURN CONCAT(CURRENT.name, '-x')]", { value : tags }).toArray();
      assertEqual([ expected ], result);
    },

    testStarExtractLimitProject2 : function () {
      var expected = [ 
        "javascript", "cpp"
      ];
      var result = db._query("RETURN @value[0].values[* FILTER CURRENT.name IN [ 'javascript', 'php', 'cpp' ] LIMIT 0, 2 RETURN CURRENT.name]", { value : tags }).toArray();
      assertEqual([ expected ], result);
    },

    testCollapse1 : function () {
      var expected = arrayOfArrays;
      var result = db._query("RETURN @value[*]", { value : arrayOfArrays }).toArray();
      assertEqual([ expected ], result);
    },

    testCollapse2 : function () {
      var expected = [ [ "one", "two", "three" ], [ "four", "five" ], [ "six" ], [ "seven", "eight" ], [ "nine", "ten" ], [ "eleven", "twelve" ] ];
      var result = db._query("RETURN @value[**]", { value : arrayOfArrays }).toArray();
      assertEqual([ expected ], result);
    },

    testCollapse3 : function () {
      var expected = [ "one", "two", "three", "four", "five", "six", "seven", "eight", "nine", "ten", "eleven", "twelve" ];
      var result = db._query("RETURN @value[***]", { value : arrayOfArrays }).toArray();
      assertEqual([ expected ], result);
    },

    testCollapseUnmodified : function () {
      var data = [ [ [ [ 1, 2 ] ], [ 3, 4 ], 5, [ 6, 7 ] ], [ 8 ] ];
      var expected = data;
      var result = db._query("RETURN @value", { value : data }).toArray();
      assertEqual([ expected ], result);
    },

    testCollapseMixed1 : function () {
      var data = [ [ [ [ 1, 2 ] ], [ 3, 4 ], 5, [ 6, 7 ] ], [ 8 ] ];
      var expected = data;
      var result = db._query("RETURN @value[*]", { value : data }).toArray();
      assertEqual([ expected ], result);
    },

    testCollapseMixed2 : function () {
      var data = [ [ [ [ 1, 2 ] ], [ 3, 4 ], 5, [ 6, 7 ] ], [ 8 ] ];
      var expected = [ [ [ 1, 2 ] ], [ 3, 4 ], 5, [ 6, 7 ], 8 ];
      var result = db._query("RETURN @value[**]", { value : data }).toArray();
      assertEqual([ expected ], result);
    },

    testCollapseMixed3 : function () {
      var data = [ [ [ [ 1, 2 ] ], [ 3, 4 ], 5, [ 6, 7 ] ], [ 8 ] ];
      var expected = [ [ 1, 2 ], 3, 4, 5, 6, 7, 8 ];
      var result = db._query("RETURN @value[***]", { value : data }).toArray();
      assertEqual([ expected ], result);
    },

    testCollapseMixed4 : function () {
      var data = [ [ [ [ 1, 2 ] ], [ 3, 4 ], 5, [ 6, 7 ] ], [ 8 ] ];
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8 ];
      var result = db._query("RETURN @value[****]", { value : data }).toArray();
      assertEqual([ expected ], result);
    },

    testCollapseMixed5 : function () {
      var data = [ [ [ [ 1, 2 ] ], [ 3, 4 ], 5, [ 6, 7 ] ], [ 8 ] ];
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8 ];
      var result = db._query("RETURN @value[*****]", { value : data }).toArray();
      assertEqual([ expected ], result);
    },

    testCollapseFilter : function () {
      var data = [ 1, 2, 3, 4, 5, 6, 7, 8 ];
      var expected = [ 1, 3, 5, 7 ];
      var result = db._query("RETURN @value[**** FILTER CURRENT % 2 != 0]", { value : data }).toArray();
      assertEqual([ expected ], result);
    },

    testCollapseProject : function () {
      var data = [ 1, 2, 3, 4, 5, 6, 7, 8 ];
      var expected = [ 2, 4, 6, 8, 10, 12, 14, 16 ];
      var result = db._query("RETURN @value[**** RETURN CURRENT * 2]", { value : data }).toArray();
      assertEqual([ expected ], result);
    },

    testCollapseFilterProject : function () {
      var data = [ 1, 2, 3, 4, 5, 6, 7, 8 ];
      var expected = [ 4, 8, 12, 16 ];
      var result = db._query("RETURN @value[**** FILTER CURRENT % 2 == 0 RETURN CURRENT * 2]", { value : data }).toArray();
      assertEqual([ expected ], result);
    },

    testCollapseFilterProjectLimit : function () {
      var data = [ 1, 2, 3, 4, 5, 6, 7, 8 ];
      var expected = [ 4, 8, 12 ];
      var result = db._query("RETURN @value[**** FILTER CURRENT % 2 == 0 LIMIT 3 RETURN CURRENT * 2]", { value : data }).toArray();
      assertEqual([ expected ], result);
    },

    testCollapseWithData : function () {
      var data = [ {
        "last": "2016-07-12",
        "name": "a1",
        "st": [ {
          "last": "2016-07-12",
          "name": "a11",
          "id": "a2",
          "dx": [ {
            "last": "2016-07-12",
            "name": "a21",
            "id": "a3",
            "docs": []
          }, {
            "last": "2016-07-12",
            "name": "a22",
            "id": "a4",
            "docs": []
          } ]
        } ]
      } ];

      var expected = [  
        { 
          "st" : [ 
            { "id" : "a2", "date" : "2016-07-12" }
          ], 
          "dx" : [ 
            { "id" : "a3", "date" : "2016-07-12" }, 
            { "id" : "a4", "date" : "2016-07-12" }  
          ], 
          "docs" : [ ] 
        } 
      ];
      
      var result = db._query("FOR c IN @value COLLECT st = c.st[* RETURN { id: CURRENT.id, date: CURRENT.last }], dx = c.st[*].dx[* RETURN { id: CURRENT.id, date: CURRENT.last }][**], docs = c.st[*].dx[*][**].docs[* RETURN { id: CURRENT.id, date: CURRENT.last }][**] RETURN { st , dx, docs }", { value : data }).toArray();
      assertEqual(expected, result);
    },

    testArrayFilterCompatibility : function () {
      // the following JSON was generated by the following code in 3.9:
      //   q = `RETURN [{Key: 'a', Value: 'a'}, {Key: 'a', Value: 'b'}, {Key: 'a', Value: 'd'}][* FILTER CURRENT.Key == 'a' && CURRENT.Value IN ['a', 'b', 'c']]`; 
      //   plan = db._createStatement({query: q, null, bindVars:  { verbosePlans: true }}).explain().plan; 
      //   JSON.stringify(plan)
      const planFrom39 = {"nodes":[{"type":"SingletonNode","typeID":1,"dependencies":[],"id":1,"parents":[2],"estimatedCost":1,"estimatedNrItems":1,"depth":0,"varInfoList":[{"VariableId":3,"depth":0,"RegisterId":0}],"nrRegs":[1],"nrConstRegs":0,"regsToClear":[],"varsUsedLaterStack":[[{"id":3,"name":"2","isDataFromCollection":false}]],"regsToKeepStack":[[]],"varsValidStack":[[]],"isInSplicedSubquery":false,"isAsyncPrefetchEnabled":false,"isCallstackSplitEnabled":false},{"type":"CalculationNode","typeID":7,"dependencies":[1],"id":2,"parents":[3],"estimatedCost":2,"estimatedNrItems":1,"depth":0,"varInfoList":[{"VariableId":3,"depth":0,"RegisterId":0}],"nrRegs":[1],"nrConstRegs":0,"regsToClear":[],"varsUsedLaterStack":[[{"id":3,"name":"2","isDataFromCollection":false}]],"regsToKeepStack":[[]],"varsValidStack":[[{"id":3,"name":"2","isDataFromCollection":false}]],"isInSplicedSubquery":false,"isAsyncPrefetchEnabled":false,"isCallstackSplitEnabled":false,"expression":{"type":"expansion","typeID":38,"levels":1,"subNodes":[{"type":"iterator","typeID":39,"subNodes":[{"type":"variable","typeID":13,"name":"0_","id":1},{"type":"array","typeID":41,"subNodes":[{"type":"object","typeID":42,"subNodes":[{"type":"object element","typeID":43,"name":"Key","subNodes":[{"type":"value","typeID":40,"value":"a","vType":"string","vTypeID":4}]},{"type":"object element","typeID":43,"name":"Value","subNodes":[{"type":"value","typeID":40,"value":"a","vType":"string","vTypeID":4}]}]},{"type":"object","typeID":42,"subNodes":[{"type":"object element","typeID":43,"name":"Key","subNodes":[{"type":"value","typeID":40,"value":"a","vType":"string","vTypeID":4}]},{"type":"object element","typeID":43,"name":"Value","subNodes":[{"type":"value","typeID":40,"value":"b","vType":"string","vTypeID":4}]}]},{"type":"object","typeID":42,"subNodes":[{"type":"object element","typeID":43,"name":"Key","subNodes":[{"type":"value","typeID":40,"value":"a","vType":"string","vTypeID":4}]},{"type":"object element","typeID":43,"name":"Value","subNodes":[{"type":"value","typeID":40,"value":"d","vType":"string","vTypeID":4}]}]}]}]},{"type":"reference","typeID":45,"name":"0_","id":1},{"type":"logical and","typeID":18,"subNodes":[{"type":"compare ==","typeID":25,"excludesNull":false,"subNodes":[{"type":"attribute access","typeID":35,"name":"Key","subNodes":[{"type":"reference","typeID":45,"name":"0_","id":1}]},{"type":"value","typeID":40,"value":"a","vType":"string","vTypeID":4}]},{"type":"compare in","typeID":31,"sorted":false,"subNodes":[{"type":"attribute access","typeID":35,"name":"Value","subNodes":[{"type":"reference","typeID":45,"name":"0_","id":1}]},{"type":"array","typeID":41,"subNodes":[{"type":"value","typeID":40,"value":"a","vType":"string","vTypeID":4},{"type":"value","typeID":40,"value":"b","vType":"string","vTypeID":4},{"type":"value","typeID":40,"value":"c","vType":"string","vTypeID":4}]}]}]},{"type":"no-op","typeID":50},{"type":"no-op","typeID":50}]},"outVariable":{"id":3,"name":"2","isDataFromCollection":false},"canThrow":false,"expressionType":"simple","functions":[]},{"type":"ReturnNode","typeID":18,"dependencies":[2],"id":3,"parents":[],"estimatedCost":3,"estimatedNrItems":1,"depth":0,"varInfoList":[{"VariableId":3,"depth":0,"RegisterId":0}],"nrRegs":[1],"nrConstRegs":0,"regsToClear":[],"varsUsedLaterStack":[[]],"regsToKeepStack":[[]],"varsValidStack":[[{"id":3,"name":"2","isDataFromCollection":false}]],"isInSplicedSubquery":false,"isAsyncPrefetchEnabled":false,"isCallstackSplitEnabled":false,"inVariable":{"id":3,"name":"2","isDataFromCollection":false},"count":true}],"rules":[],"collections":[],"variables":[{"id":3,"name":"2","isDataFromCollection":false},{"id":1,"name":"0_","isDataFromCollection":false}],"estimatedCost":3,"estimatedNrItems":1,"isModificationQuery":false};

      let result39 = executeJson(planFrom39).json;

      assertEqual([ [ { "Key" : "a", "Value" : "a" }, { "Key" : "a", "Value" : "b" } ] ], result39);

      // plan for same query, but built by 3.10
      const planFrom310 = {"nodes":[{"type":"SingletonNode","typeID":1,"dependencies":[],"id":1,"parents":[2],"estimatedCost":1,"estimatedNrItems":1,"depth":0,"varInfoList":[{"VariableId":3,"depth":0,"RegisterId":0}],"nrRegs":[1],"nrConstRegs":0,"regsToClear":[],"varsUsedLaterStack":[[{"id":3,"name":"2","isDataFromCollection":false,"isFullDocumentFromCollection":false}]],"regsToKeepStack":[[]],"varsValidStack":[[]],"isInSplicedSubquery":false,"isAsyncPrefetchEnabled":false,"isCallstackSplitEnabled":false},{"type":"CalculationNode","typeID":7,"dependencies":[1],"id":2,"parents":[3],"estimatedCost":2,"estimatedNrItems":1,"depth":0,"varInfoList":[{"VariableId":3,"depth":0,"RegisterId":0}],"nrRegs":[1],"nrConstRegs":0,"regsToClear":[],"varsUsedLaterStack":[[{"id":3,"name":"2","isDataFromCollection":false,"isFullDocumentFromCollection":false}]],"regsToKeepStack":[[]],"varsValidStack":[[{"id":3,"name":"2","isDataFromCollection":false,"isFullDocumentFromCollection":false}]],"isInSplicedSubquery":false,"isAsyncPrefetchEnabled":false,"isCallstackSplitEnabled":false,"expression":{"type":"expansion","typeID":38,"levels":1,"booleanize":false,"subNodes":[{"type":"iterator","typeID":39,"subNodes":[{"type":"variable","typeID":13,"name":"0_","id":1},{"type":"array","typeID":41,"subNodes":[{"type":"object","typeID":42,"subNodes":[{"type":"object element","typeID":43,"name":"Key","subNodes":[{"type":"value","typeID":40,"value":"a","vType":"string","vTypeID":4}]},{"type":"object element","typeID":43,"name":"Value","subNodes":[{"type":"value","typeID":40,"value":"a","vType":"string","vTypeID":4}]}]},{"type":"object","typeID":42,"subNodes":[{"type":"object element","typeID":43,"name":"Key","subNodes":[{"type":"value","typeID":40,"value":"a","vType":"string","vTypeID":4}]},{"type":"object element","typeID":43,"name":"Value","subNodes":[{"type":"value","typeID":40,"value":"b","vType":"string","vTypeID":4}]}]},{"type":"object","typeID":42,"subNodes":[{"type":"object element","typeID":43,"name":"Key","subNodes":[{"type":"value","typeID":40,"value":"a","vType":"string","vTypeID":4}]},{"type":"object element","typeID":43,"name":"Value","subNodes":[{"type":"value","typeID":40,"value":"d","vType":"string","vTypeID":4}]}]}]}]},{"type":"reference","typeID":45,"name":"0_","id":1},{"type":"array filter","typeID":81,"subNodes":[{"type":"no-op","typeID":50},{"type":"logical and","typeID":18,"subNodes":[{"type":"compare ==","typeID":25,"excludesNull":false,"subNodes":[{"type":"attribute access","typeID":35,"name":"Key","subNodes":[{"type":"reference","typeID":45,"name":"0_","id":1}]},{"type":"value","typeID":40,"value":"a","vType":"string","vTypeID":4}]},{"type":"compare in","typeID":31,"sorted":false,"subNodes":[{"type":"attribute access","typeID":35,"name":"Value","subNodes":[{"type":"reference","typeID":45,"name":"0_","id":1}]},{"type":"array","typeID":41,"subNodes":[{"type":"value","typeID":40,"value":"a","vType":"string","vTypeID":4},{"type":"value","typeID":40,"value":"b","vType":"string","vTypeID":4},{"type":"value","typeID":40,"value":"c","vType":"string","vTypeID":4}]}]}]}]},{"type":"no-op","typeID":50},{"type":"no-op","typeID":50}]},"outVariable":{"id":3,"name":"2","isDataFromCollection":false,"isFullDocumentFromCollection":false},"canThrow":false,"expressionType":"simple","functions":[]},{"type":"ReturnNode","typeID":18,"dependencies":[2],"id":3,"parents":[],"estimatedCost":3,"estimatedNrItems":1,"depth":0,"varInfoList":[{"VariableId":3,"depth":0,"RegisterId":0}],"nrRegs":[1],"nrConstRegs":0,"regsToClear":[],"varsUsedLaterStack":[[]],"regsToKeepStack":[[]],"varsValidStack":[[{"id":3,"name":"2","isDataFromCollection":false,"isFullDocumentFromCollection":false}]],"isInSplicedSubquery":false,"isAsyncPrefetchEnabled":false,"isCallstackSplitEnabled":false,"inVariable":{"id":3,"name":"2","isDataFromCollection":false,"isFullDocumentFromCollection":false},"count":true}],"rules":[],"collections":[],"variables":[{"id":3,"name":"2","isDataFromCollection":false,"isFullDocumentFromCollection":false},{"id":1,"name":"0_","isDataFromCollection":false,"isFullDocumentFromCollection":false}],"estimatedCost":3,"estimatedNrItems":1,"isModificationQuery":false};

      let result310 = executeJson(planFrom310).json;
      assertEqual([ [ { "Key" : "a", "Value" : "a" }, { "Key" : "a", "Value" : "b" } ] ], result310);
 
      // we expect 3.9 and 3.10 to generate different plans.
      // 3.10 is going to insert an ARRAY_FILTER for the array filter
      assertNotEqual(JSON.stringify(planFrom39), JSON.stringify(planFrom310));
    },

  };
}

jsunity.run(arrayAccessTestSuite);

return jsunity.done();
