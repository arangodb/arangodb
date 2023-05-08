/*jshint globalstrict:false, strict:false, maxlen: 700 */
/*global assertEqual, assertNotEqual, AQL_EXECUTE, AQL_EXECUTEJSON */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for query language, array accesses
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

const jsunity = require("jsunity");

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
      var result = AQL_EXECUTE("RETURN { foo: 'bar' }[0]").json;
      assertEqual([ null ], result);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test non-array access
////////////////////////////////////////////////////////////////////////////////

    testNonArray2 : function () {
      var result = AQL_EXECUTE("RETURN { foo: 'bar' }[99]").json;
      assertEqual([ null ], result);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test non-array access
////////////////////////////////////////////////////////////////////////////////

    testNonArray3 : function () {
      var result = AQL_EXECUTE("RETURN { foo: 'bar' }[-1]").json;
      assertEqual([ null ], result);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test non-array access
////////////////////////////////////////////////////////////////////////////////

    testNonArray4 : function () {
      var result = AQL_EXECUTE("RETURN NOOPT(PASSTHRU({ foo: 'bar' }))[-1]").json;
      assertEqual([ null ], result);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test non-array access
////////////////////////////////////////////////////////////////////////////////

    testNonArray5 : function () {
      var result = AQL_EXECUTE("RETURN NOOPT(PASSTHRU(null))[-1]").json;
      assertEqual([ null ], result);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test non-array access
////////////////////////////////////////////////////////////////////////////////

    testNonArray6 : function () {
      var result = AQL_EXECUTE("RETURN NOOPT(PASSTHRU('foobar'))[2]").json;
      assertEqual([ null ], result);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test non-array access
////////////////////////////////////////////////////////////////////////////////

    testNonArrayRange1 : function () {
      var result = AQL_EXECUTE("RETURN { foo: 'bar' }[0..1]").json;
      assertEqual([ null ], result);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test non-array access
////////////////////////////////////////////////////////////////////////////////

    testNonArrayRange2 : function () {
      var result = AQL_EXECUTE("RETURN { foo: 'bar' }[-2..-1]").json;
      assertEqual([ null ], result);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test subquery result
////////////////////////////////////////////////////////////////////////////////

    testSubqueryResult : function () {
      for (var i = 0; i < persons.length; ++i) {
        var result = AQL_EXECUTE("RETURN (FOR value IN @persons RETURN value)[" + i + "]", { persons: persons }).json;
        assertEqual([ persons[i] ], result);
      }
    },

    testQuestionMarkEmptyArray : function() {
      var expected = [ false ];
      var result = AQL_EXECUTE("RETURN [][?]").json;
      assertEqual(expected, result);
    },

    testMulitpleQuestionMarkEmptyArray : function() {
      var expected = [ false ];
      var result = AQL_EXECUTE("RETURN [][????]").json;
      assertEqual(expected, result);
    },

    testQuestionMarkNonEmptyArray : function() {
      var expected = [ true ];
      var result = AQL_EXECUTE("RETURN [42][?]").json;
      assertEqual(expected, result);
    },

    testQuestionMarkWithFilter: function() {
      var expected = [ true ];
      var result = AQL_EXECUTE("RETURN @value[? FILTER CURRENT.name == 'Europe']", { value : continents }).json;
      assertEqual(expected, result);
    },

    testMultipleQuestionMarkWithFilter: function() {
      var expected = [ true ];
      var result = AQL_EXECUTE("RETURN @value[???????????? FILTER CURRENT.name == 'Europe']", { value : continents }).json;
      assertEqual(expected, result);
    },

    testQuestionMarkWithNonMatchingFilter: function() {
      var expected = [ false ];
      var result = AQL_EXECUTE("RETURN @value[? FILTER CURRENT.name == 'foobar']", { value : continents }).json;
      assertEqual(expected, result);
    },
    
    testQuestionMarkWithFilterAndQuantifier: function() {
      const values = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 ];
      let result;
  
      // range quantifiers
      result = AQL_EXECUTE("LET values = @values RETURN values[? 1..1 FILTER CURRENT == 3]", { values }).json;
      assertEqual([ true ], result);
      
      result = AQL_EXECUTE("LET values = @values RETURN values[? 1..2 FILTER CURRENT == 3]", { values }).json;
      assertEqual([ true ], result);
      
      result = AQL_EXECUTE("LET values = @values RETURN values[? 1..10 FILTER CURRENT == 3]", { values }).json;
      assertEqual([ true ], result);
      
      result = AQL_EXECUTE("LET values = @values RETURN values[? 2..2 FILTER CURRENT == 3]", { values }).json;
      assertEqual([ false ], result);
      
      result = AQL_EXECUTE("LET values = @values RETURN values[? 2..5 FILTER CURRENT == 3]", { values }).json;
      assertEqual([ false ], result);

      result = AQL_EXECUTE("LET values = @values RETURN values[? 1..1 FILTER CURRENT >= 3]", { values }).json;
      assertEqual([ false ], result);
      
      result = AQL_EXECUTE("LET values = @values RETURN values[? 1..2 FILTER CURRENT >= 3]", { values }).json;
      assertEqual([ false ], result);
      
      result = AQL_EXECUTE("LET values = @values RETURN values[? 1..10 FILTER CURRENT >= 3]", { values }).json;
      assertEqual([ true ], result);
      
      result = AQL_EXECUTE("LET values = @values RETURN values[? 2..2 FILTER CURRENT >= 3]", { values }).json;
      assertEqual([ false ], result);
      
      result = AQL_EXECUTE("LET values = @values RETURN values[? 2..5 FILTER CURRENT >= 3]", { values }).json;
      assertEqual([ false ], result);
      
      result = AQL_EXECUTE("LET values = @values RETURN values[? 9..9 FILTER CURRENT >= 3]", { values }).json;
      assertEqual([ true ], result);
      
      result = AQL_EXECUTE("LET values = @values RETURN values[? 8..9 FILTER CURRENT >= 3]", { values }).json;
      assertEqual([ true ], result);
      
      result = AQL_EXECUTE("LET values = @values RETURN values[? 8..10 FILTER CURRENT >= 3]", { values }).json;
      assertEqual([ true ], result);
      
      result = AQL_EXECUTE("LET values = @values RETURN values[? 9..11 FILTER CURRENT >= 3]", { values }).json;
      assertEqual([ true ], result);
      
      result = AQL_EXECUTE("LET values = @values RETURN values[? 10..11 FILTER CURRENT >= 3]", { values }).json;
      assertEqual([ false ], result);
      
      // range quantifiers with bind parameters
      result = AQL_EXECUTE("LET values = @values RETURN values[? @lower..@upper FILTER CURRENT >= 3]", { values, lower: 9, upper: 11 }).json;
      assertEqual([ true ], result);
      
      result = AQL_EXECUTE("LET values = @values RETURN values[? @lower..@upper FILTER CURRENT >= 3]", { values, lower: 1, upper: 15 }).json;
      assertEqual([ true ], result);
      
      result = AQL_EXECUTE("LET values = @values RETURN values[? @lower..@upper FILTER CURRENT <= 3]", { values, lower: -1, upper: 4 }).json;
      assertEqual([ true ], result);
      
      result = AQL_EXECUTE("LET values = @values RETURN values[? @lower..@upper FILTER CURRENT >= 3]", { values, lower: 9, upper: 10 }).json;
      assertEqual([ true ], result);
      
      result = AQL_EXECUTE("LET values = @values RETURN values[? @lower..@upper FILTER CURRENT >= 3]", { values, lower: -3, upper: 10 }).json;
      assertEqual([ true ], result);
      
      result = AQL_EXECUTE("LET values = @values RETURN values[? @lower..@upper FILTER CURRENT >= 3]", { values, lower: "piff", upper: 10 }).json;
      assertEqual([ true ], result);
      
      // range quantifiers with expressions
      result = AQL_EXECUTE("LET values = @values RETURN values[? NOOPT(4)..NOOPT(9) FILTER CURRENT >= 3]", { values }).json;
      assertEqual([ true ], result);
      
      result = AQL_EXECUTE("FOR i IN 1..5 LET values = @values RETURN values[? i..(i + 2) FILTER CURRENT >= 6]", { values }).json;
      assertEqual([ false, false, false, true, true ], result);

      result = AQL_EXECUTE("LET values = @values, x = NOOPT(1), y = NOOPT(5) RETURN values[? x..y FILTER CURRENT >= 3]", { values }).json;
      assertEqual([ false ], result);

      result = AQL_EXECUTE("LET values = @values, x = 1, y = NOOPT(5) RETURN values[? x..y FILTER CURRENT >= 3]", { values }).json;
      assertEqual([ false ], result);

      result = AQL_EXECUTE("LET values = @values, x = NOOPT(1), y = 5 RETURN values[? x..y FILTER CURRENT >= 3]", { values }).json;
      assertEqual([ false ], result);

      result = AQL_EXECUTE("LET values = @values LET bound = ROUND(RAND() * 1000) % 10 RETURN values[? 1..bound FILTER CURRENT <= 0]", { values }).json;
      assertEqual([ false ], result);
      
      result = AQL_EXECUTE("LET values = [1,2,3,4,5,6,7,8,9,10,11] LET x = 1..5 RETURN values[? x FILTER CURRENT >= 3]").json;
      assertEqual([ false ], result);

      // numeric quantifiers
      result = AQL_EXECUTE("LET values = @values RETURN values[? 1 FILTER CURRENT == 3]", { values }).json;
      assertEqual([ true ], result);
      
      result = AQL_EXECUTE("LET values = @values RETURN values[? 1 FILTER CURRENT == 4]", { values }).json;
      assertEqual([ true ], result);
      
      result = AQL_EXECUTE("LET values = @values RETURN values[? 1 FILTER CURRENT != 3]", { values }).json;
      assertEqual([ false ], result);
      
      result = AQL_EXECUTE("LET values = @values RETURN values[? 1 FILTER CURRENT == 12]", { values }).json;
      assertEqual([ false ], result);
      
      result = AQL_EXECUTE("LET values = @values RETURN values[? 1 FILTER CURRENT == 0]", { values }).json;
      assertEqual([ false ], result);
      
      result = AQL_EXECUTE("LET values = @values RETURN values[? 2 FILTER CURRENT == 1]", { values }).json;
      assertEqual([ false ], result);
      
      result = AQL_EXECUTE("LET values = @values RETURN values[? 0 FILTER CURRENT == 1]", { values }).json;
      assertEqual([ false ], result);

      // numeric quantifiers with bind parameters
      result = AQL_EXECUTE("LET values = @values RETURN values[? @value FILTER CURRENT >= 3]", { values, value: 8 }).json;
      assertEqual([ false ], result);
      
      result = AQL_EXECUTE("LET values = @values RETURN values[? @value FILTER CURRENT >= 3]", { values, value: 9 }).json;
      assertEqual([ true ], result);
      
      result = AQL_EXECUTE("LET values = @values RETURN values[? @value FILTER CURRENT <= 3]", { values, value: 3 }).json;
      assertEqual([ true ], result);
      
      result = AQL_EXECUTE("LET values = @values RETURN values[? @value FILTER CURRENT <= 3]", { values, value: 4 }).json;
      assertEqual([ false ], result);
      
      // numeric quantifiers with expressions
      result = AQL_EXECUTE("LET values = @values RETURN values[? NOOPT(4) FILTER CURRENT >= 3]", { values }).json;
      assertEqual([ false ], result);
      
      result = AQL_EXECUTE("LET values = @values RETURN values[? NOOPT(4) FILTER CURRENT >= 8]", { values }).json;
      assertEqual([ true ], result);
      
      result = AQL_EXECUTE("FOR i IN 1..7 LET values = @values RETURN values[? i FILTER CURRENT >= 6]", { values }).json;
      assertEqual([ false, false, false, false, false, true, false ], result);
     
      // ALL|ANY|NONE
      result = AQL_EXECUTE("LET values = @values RETURN values[? ALL FILTER CURRENT == 1]", { values: [] }).json;
      assertEqual([ true ], result);

      result = AQL_EXECUTE("LET values = @values RETURN values[? ALL FILTER CURRENT == 1]", { values }).json;
      assertEqual([ false ], result);
      
      result = AQL_EXECUTE("LET values = @values RETURN values[? ALL FILTER CURRENT <= 11]", { values }).json;
      assertEqual([ true ], result);
      
      result = AQL_EXECUTE("LET values = @values RETURN values[? ALL FILTER CURRENT >= 1 && CURRENT <= 11]", { values }).json;
      assertEqual([ true ], result);
      
      result = AQL_EXECUTE("LET values = @values RETURN values[? ALL FILTER CURRENT >= 2 && CURRENT <= 11]", { values }).json;
      assertEqual([ false ], result);
      
      result = AQL_EXECUTE("LET values = @values RETURN values[? ANY FILTER CURRENT == 1]", { values: [] }).json;
      assertEqual([ false ], result);
      
      result = AQL_EXECUTE("LET values = @values RETURN values[? ANY FILTER CURRENT == 1]", { values }).json;
      assertEqual([ true ], result);
      
      result = AQL_EXECUTE("LET values = @values RETURN values[? ANY FILTER CURRENT == 10]", { values }).json;
      assertEqual([ true ], result);
      
      result = AQL_EXECUTE("LET values = @values RETURN values[? ANY FILTER CURRENT == 12]", { values }).json;
      assertEqual([ false ], result);
      
      result = AQL_EXECUTE("LET values = @values RETURN values[? ANY FILTER CURRENT >= 5]", { values }).json;
      assertEqual([ true ], result);
      
      result = AQL_EXECUTE("LET values = @values RETURN values[? NONE FILTER CURRENT == 1]", { values: [] }).json;
      assertEqual([ true ], result);
      
      result = AQL_EXECUTE("LET values = @values RETURN values[? NONE FILTER CURRENT == 1]", { values }).json;
      assertEqual([ false ], result);
      
      result = AQL_EXECUTE("LET values = @values RETURN values[? NONE FILTER CURRENT == 11]", { values }).json;
      assertEqual([ false ], result);
      
      result = AQL_EXECUTE("LET values = @values RETURN values[? NONE FILTER CURRENT == 12]", { values }).json;
      assertEqual([ true ], result);
      
      result = AQL_EXECUTE("LET values = @values RETURN values[? NONE FILTER CURRENT <= 11]", { values }).json;
      assertEqual([ false ], result);
      
      result = AQL_EXECUTE("LET values = @values RETURN values[? NONE FILTER CURRENT >= 12]", { values }).json;
      assertEqual([ true ], result);
      
      result = AQL_EXECUTE("LET values = @values RETURN values[? AT LEAST (0) FILTER CURRENT == 1]", { values }).json;
      assertEqual([ true ], result);
      
      result = AQL_EXECUTE("LET values = @values RETURN values[? AT LEAST (1) FILTER CURRENT == 1]", { values }).json;
      assertEqual([ true ], result);
      
      result = AQL_EXECUTE("LET values = @values RETURN values[? AT LEAST (2) FILTER CURRENT == 1]", { values }).json;
      assertEqual([ false ], result);
      
      result = AQL_EXECUTE("LET values = @values RETURN values[? AT LEAST (1) FILTER CURRENT == 12]", { values }).json;
      assertEqual([ false ], result);
      
      result = AQL_EXECUTE("LET values = @values RETURN values[? AT LEAST (1) FILTER CURRENT != 1]", { values }).json;
      assertEqual([ true ], result);
      
      result = AQL_EXECUTE("LET values = @values RETURN values[? AT LEAST (10) FILTER CURRENT != 1]", { values }).json;
      assertEqual([ true ], result);
      
      result = AQL_EXECUTE("LET values = @values RETURN values[? AT LEAST (15) FILTER CURRENT != 1]", { values }).json;
      assertEqual([ false ], result);
      
      result = AQL_EXECUTE("LET values = @values RETURN values[? AT LEAST (5) FILTER CURRENT < 10]", { values }).json;
      assertEqual([ true ], result);
      
      result = AQL_EXECUTE("LET values = @values RETURN values[? AT LEAST (5) FILTER CURRENT < 4]", { values }).json;
      assertEqual([ false ], result);
      
      result = AQL_EXECUTE("LET values = @values RETURN values[? AT LEAST (5) FILTER CURRENT <= 10]", { values }).json;
      assertEqual([ true ], result);
      
      result = AQL_EXECUTE("LET values = @values RETURN values[? AT LEAST (5) FILTER CURRENT <= 4]", { values }).json;
      assertEqual([ false ], result);
      
      result = AQL_EXECUTE("LET values = @values RETURN values[? AT LEAST (3) FILTER CURRENT >= 8]", { values }).json;
      assertEqual([ true ], result);
      
      result = AQL_EXECUTE("LET values = @values RETURN values[? AT LEAST (5) FILTER CURRENT >= 8]", { values }).json;
      assertEqual([ false ], result);
      
      result = AQL_EXECUTE("LET values = @values RETURN values[? AT LEAST (3) FILTER CURRENT > 8]", { values }).json;
      assertEqual([ true ], result);
      
      result = AQL_EXECUTE("LET values = @values RETURN values[? AT LEAST (5) FILTER CURRENT > 8]", { values }).json;
      assertEqual([ false ], result);
    },

    testStarExtractScalar : function () {
      var expected = [ "Europe", "Asia" ];
      var result = AQL_EXECUTE("RETURN @value[*].name", { value : continents }).json;
      assertEqual([ expected ], result);
    },

    testStarExtractScalarUndefinedAttribute : function () {
      var expected = [ null, null ];
      var result = AQL_EXECUTE("RETURN @value[*].foobar", { value : continents }).json;
      assertEqual([ expected ], result);
    },

    testStarExtractScalarWithFilter : function () {
      var expected = [ "Europe" ];
      var result = AQL_EXECUTE("RETURN @value[* FILTER CURRENT.name == 'Europe'].name", { value : continents }).json;
      assertEqual([ expected ], result);
    },

    testStarExtractScalarWithNonMatchingFilter : function () {
      var expected = [ ];
      var result = AQL_EXECUTE("RETURN @value[* FILTER CURRENT.name == 'foobar'].name", { value : continents }).json;
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
 
      var result = AQL_EXECUTE("RETURN @value[*].countries", { value : continents }).json;
      assertEqual([ expected ], result);
    },

    testStarExtractArrayWithFilter : function () {
      var expected = [ continents[0].countries ];
      var result = AQL_EXECUTE("RETURN @value[* FILTER CURRENT.name == 'Europe'].countries", { value : continents }).json;
      assertEqual([ expected ], result);
    },

    testStarExtractSubArrayIndexed1 : function () {
      var expected = [ 
        { id: "UK", capital: "London" }, 
        { id: "JP", capital: "Tokyo" } 
      ];
      var result = AQL_EXECUTE("RETURN @value[*].countries[0]", { value : continents }).json;
      assertEqual([ expected ], result);
    },

    testStarExtractSubArrayIndexed2 : function () {
      var expected = [ 
        { id: "IT", capital: "Rome" }, 
        { id: "IN", capital: "Delhi" }
      ];
      var result = AQL_EXECUTE("RETURN @value[*].countries[-1]", { value : continents }).json;
      assertEqual([ expected ], result);
    },

    testStarExtractSubArrayIndexed3 : function () {
      var expected = [ 
        null,
        null
      ];
      var result = AQL_EXECUTE("RETURN @value[*].countries[99]", { value : continents }).json;
      assertEqual([ expected ], result);
    },

    testStarExtractSubArrayIndexedTotal1 : function () {
      var expected = [ 
        { id: "UK", capital: "London" }, 
        { id: "FR", capital: "Paris" }, 
        { id: "IT", capital: "Rome" } 
      ];
      var result = AQL_EXECUTE("RETURN (@value[*].countries)[0]", { value : continents }).json;
      assertEqual([ expected ], result);
    },

    testStarExtractSubArrayIndexedTotal2 : function () {
      var expected = [ 
        { id: "JP", capital: "Tokyo" }, 
        { id: "CN", capital: "Beijing" }, 
        { id: "KR", capital: "Seoul" }, 
        { id: "IN", capital: "Delhi" }
      ];
      var result = AQL_EXECUTE("RETURN (@value[*].countries)[-1]", { value : continents }).json;
      assertEqual([ expected ], result);
    },

    testStarExtractSubAttribute1 : function () {
      var expected = [ 
        [ "UK", "FR", "IT" ],
        [ "JP", "CN", "KR", "IN" ]
      ];
      var result = AQL_EXECUTE("RETURN @value[*].countries[*].id", { value : continents }).json;
      assertEqual([ expected ], result);
    },

    testStarExtractSubAttribute2 : function () {
      var expected = [ 
        [ "London", "Paris", "Rome" ],
        [ "Tokyo", "Beijing", "Seoul", "Delhi" ]
      ];
      var result = AQL_EXECUTE("RETURN @value[*].countries[*].capital", { value : continents }).json;
      assertEqual([ expected ], result);
    },

    testStarExtractSubAttributeExtraOperator1 : function () {
      var expected = [ 
        [ "UK", "FR", "IT" ],
        [ "JP", "CN", "KR", "IN" ]
      ];
      var result = AQL_EXECUTE("RETURN @value[*].countries[*].id[*]", { value : continents }).json;
      assertEqual([ expected ], result);
    },

    testStarExtractSubAttributeExtraOperator2 : function () {
      var expected = [ 
        [ "London", "Paris", "Rome" ],
        [ "Tokyo", "Beijing", "Seoul", "Delhi" ]
      ];
      var result = AQL_EXECUTE("RETURN @value[*].countries[*].capital[*]", { value : continents }).json;
      assertEqual([ expected ], result);
    },

    testStarExtractSubAttribute1Combine : function () {
      var expected = [ 
        "UK", "FR", "IT", "JP", "CN", "KR", "IN"
      ];
      var result = AQL_EXECUTE("RETURN @value[*].countries[**].id", { value : continents }).json;
      assertEqual([ expected ], result);
    },

    testStarExtractSubAttribute2Combine : function () {
      var expected = [ 
        "London", "Paris", "Rome", "Tokyo", "Beijing", "Seoul", "Delhi"
      ];
      var result = AQL_EXECUTE("RETURN @value[*].countries[**].capital", { value : continents }).json;
      assertEqual([ expected ], result);
    },

    testStarExtractSubAttribute3Combine : function () {
      var expected = [ 
        "London", "Paris", "Rome", "Tokyo", "Beijing", "Seoul", "Delhi"
      ];
      // more than 2 asterisks won't change the result
      var result = AQL_EXECUTE("RETURN @value[*].countries[****].capital", { value : continents }).json;
      assertEqual([ expected ], result);
    },

    testStarExtractSubAttributeNonExisting : function () {
      var expected = [ 
        [ null, null, null ],
        [ null, null, null, null ]
      ];
      var result = AQL_EXECUTE("RETURN @value[*].countries[*].id.foobar", { value : continents }).json;
      assertEqual([ expected ], result);
    },

    testStarExtractSubAttributeIndexed1 : function () {
      var expected = [ 
        [ null, null, null ],
        [ null, null, null, null ]
      ];
      var result = AQL_EXECUTE("RETURN @value[*].countries[*].id[0]", { value : continents }).json;
      assertEqual([ expected ], result);
    },

    testStarExtractSubAttributeIndexed2 : function () {
      var expected = [ 
        [ null, null, null ],
        [ null, null, null, null ]
      ];
      var result = AQL_EXECUTE("RETURN @value[*].countries[*].id[1]", { value : continents }).json;
      assertEqual([ expected ], result);
    },

    testStarExtractSubAttributeIndexedTotal1 : function () {
      var expected = [ 
        "UK", "FR", "IT"
      ];
      var result = AQL_EXECUTE("RETURN (@value[*].countries[*].id)[0]", { value : continents }).json;
      assertEqual([ expected ], result);
    },

    testStarExtractSubAttributeIndexedTotal2 : function () {
      var expected = [ 
        "JP", "CN", "KR", "IN"
      ];
      var result = AQL_EXECUTE("RETURN (@value[*].countries[*].id)[1]", { value : continents }).json;
      assertEqual([ expected ], result);
    },

    testStarExtractFiltered1 : function () {
      var expected = [ 
        "javascript", "php"
      ];
      var result = AQL_EXECUTE("RETURN @value[0].values[* FILTER CURRENT.count == 2].name", { value : tags }).json;
      assertEqual([ expected ], result);
    },

    testStarExtractFiltered2 : function () {
      var expected = [ 
        "cpp", "ruby", "python"
      ];
      var result = AQL_EXECUTE("RETURN @value[0].values[* FILTER CURRENT.count != 2].name", { value : tags }).json;
      assertEqual([ expected ], result);
    },

    testStarExtractFiltered3 : function () {
      var expected = [ 
        "ruby"
      ];
      var result = AQL_EXECUTE("RETURN @value[0].values[* FILTER CURRENT.count != 2 && CURRENT.name == 'ruby'].name", { value : tags }).json;
      assertEqual([ expected ], result);
    },

    testStarExtractFiltered4 : function () {
      var expected = [ 
        3
      ];
      var result = AQL_EXECUTE("RETURN @value[0].values[* FILTER LENGTH(CURRENT.payload) == 4].count", { value : tags }).json;
      assertEqual([ expected ], result);
    },

    testStarExtractFiltered5 : function () {
      var expected = [ 
        "php", "ruby", "python"
      ];
      var result = AQL_EXECUTE("RETURN @value[0].values[* FILTER CURRENT.name IN [ 'php', 'python', 'ruby' ]].name", { value : tags }).json;
      assertEqual([ expected ], result);
    },

    testStarExtractFiltered6 : function () {
      var expected = [ 
        [ "javascript", "php" ]
      ];
      var result = AQL_EXECUTE("RETURN @value[*].values[* FILTER CURRENT.count == 2].name", { value : tags }).json;
      assertEqual([ expected ], result);
    },

    testStarExtractFiltered7 : function () {
      var expected = [ 
        [ "cpp", "ruby", "python" ]
      ];
      var result = AQL_EXECUTE("RETURN @value[*].values[* FILTER CURRENT.count != 2].name", { value : tags }).json;
      assertEqual([ expected ], result);
    },

    testStarExtractFiltered8 : function () {
      var expected = [ 
        [ "ruby" ]
      ];
      var result = AQL_EXECUTE("RETURN @value[*].values[* FILTER CURRENT.count != 2 && CURRENT.name == 'ruby'].name", { value : tags }).json;
      assertEqual([ expected ], result);
    },

    testStarExtractFiltered9 : function () {
      var expected = [ 
        [ 3 ]
      ];
      var result = AQL_EXECUTE("RETURN @value[*].values[* FILTER LENGTH(CURRENT.payload) == 4].count", { value : tags }).json;
      assertEqual([ expected ], result);
    },

    testStarExtractFiltered10 : function () {
      var expected = [ 
        [ "php", "ruby", "python" ]
      ];
      var result = AQL_EXECUTE("RETURN @value[*].values[* FILTER CURRENT.name IN [ 'php', 'python', 'ruby' ]].name", { value : tags }).json;
      assertEqual([ expected ], result);
    },

    testStarExtractProject1 : function () {
      var expected = [ 
        2, 2, 0, 2, 4
      ];
      var result = AQL_EXECUTE("RETURN @value[0].values[* RETURN LENGTH(CURRENT.payload)][**]", { value : tags }).json;
      assertEqual([ expected ], result);
    },

    testStarExtractProject2 : function () {
      var expected = [ 
        "x-javascript", "x-cpp", "x-php", "x-ruby", "x-python"
      ];
      var result = AQL_EXECUTE("RETURN @value[0].values[* RETURN CONCAT('x-', CURRENT.name)][**]", { value : tags }).json;
      assertEqual([ expected ], result);
    },

    testStarExtractProjectFilter : function () {
      var expected = [ 
        "x-cpp", "x-ruby", "x-python"
      ];
      var result = AQL_EXECUTE("RETURN @value[0].values[* FILTER CURRENT.count >= 3 RETURN CONCAT('x-', CURRENT.name)][**]", { value : tags }).json;
      assertEqual([ expected ], result);
    },

    testStarExtractLimitEmpty1 : function () {
      var expected = [ 
      ];
      var result = AQL_EXECUTE("RETURN @value[0].values[*].name[** LIMIT 0]", { value : tags }).json;
      assertEqual([ expected ], result);
    },

    testStarExtractLimitEmpty2 : function () {
      var expected = [ 
      ];
      var result = AQL_EXECUTE("RETURN @value[0].values[* LIMIT 0].name", { value : tags }).json;
      assertEqual([ expected ], result);
    },

    testStarExtractLimitEmpty3 : function () {
      var expected = [ 
      ];
      var result = AQL_EXECUTE("RETURN @value[0].values[* LIMIT -10, 1].name", { value : tags }).json;
      assertEqual([ expected ], result);
    },

    testStarExtractLimit0 : function () {
      var expected = [ 
        "javascript"
      ];
      var result = AQL_EXECUTE("RETURN @value[0].values[* LIMIT 1].name", { value : tags }).json;
      assertEqual([ expected ], result);
    },

    testStarExtractLimit1 : function () {
      var expected = [ 
        "javascript"
      ];
      var result = AQL_EXECUTE("RETURN @value[0].values[*].name[** LIMIT 1]", { value : tags }).json;
      assertEqual([ expected ], result);
    },

    testStarExtractLimit2 : function () {
      var expected = [ 
        "javascript"
      ];
      var result = AQL_EXECUTE("RETURN @value[0].values[*].name[** LIMIT 0, 1]", { value : tags }).json;
      assertEqual([ expected ], result);
    },

    testStarExtractLimit3 : function () {
      var expected = [ 
        "javascript", "cpp", "php", "ruby", "python"
      ];
      var result = AQL_EXECUTE("RETURN @value[0].values[*].name[** LIMIT 0, 10]", { value : tags }).json;
      assertEqual([ expected ], result);
    },

    testStarExtractLimit4 : function () {
      var expected = [ 
        "javascript", "cpp", "php", "ruby"
      ];
      var result = AQL_EXECUTE("RETURN @value[0].values[*].name[** LIMIT 0, 4]", { value : tags }).json;
      assertEqual([ expected ], result);
    },

    testStarExtractLimit5 : function () {
      var expected = [ 
        "javascript", "cpp"
      ];
      var result = AQL_EXECUTE("RETURN @value[0].values[*].name[** LIMIT 0, 2]", { value : tags }).json;
      assertEqual([ expected ], result);
    },

    testStarExtractLimit6 : function () {
      var expected = [ 
        "cpp", "php", "ruby", "python"
      ];
      var result = AQL_EXECUTE("RETURN @value[0].values[*].name[** LIMIT 1, 4]", { value : tags }).json;
      assertEqual([ expected ], result);
    },

    testStarExtractLimit7 : function () {
      var expected = [ 
        "ruby", "python"
      ];
      var result = AQL_EXECUTE("RETURN @value[0].values[*].name[** LIMIT 3, 4]", { value : tags }).json;
      assertEqual([ expected ], result);
    },

    testStarExtractLimit8 : function () {
      var expected = [ 
        "javascript", "cpp"
      ];
      var result = AQL_EXECUTE("RETURN @value[0].values[* FILTER CURRENT.name IN [ 'javascript', 'php', 'cpp' ] LIMIT 0, 2].name[**]", { value : tags }).json;
      assertEqual([ expected ], result);
    },

    testStarExtractLimitProject1 : function () {
      var expected = [ 
        "javascript-x", "cpp-x"
      ];
      var result = AQL_EXECUTE("RETURN @value[0].values[** LIMIT 0, 2 RETURN CONCAT(CURRENT.name, '-x')]", { value : tags }).json;
      assertEqual([ expected ], result);
    },

    testStarExtractLimitProject2 : function () {
      var expected = [ 
        "javascript", "cpp"
      ];
      var result = AQL_EXECUTE("RETURN @value[0].values[* FILTER CURRENT.name IN [ 'javascript', 'php', 'cpp' ] LIMIT 0, 2 RETURN CURRENT.name]", { value : tags }).json;
      assertEqual([ expected ], result);
    },

    testCollapse1 : function () {
      var expected = arrayOfArrays;
      var result = AQL_EXECUTE("RETURN @value[*]", { value : arrayOfArrays }).json;
      assertEqual([ expected ], result);
    },

    testCollapse2 : function () {
      var expected = [ [ "one", "two", "three" ], [ "four", "five" ], [ "six" ], [ "seven", "eight" ], [ "nine", "ten" ], [ "eleven", "twelve" ] ];
      var result = AQL_EXECUTE("RETURN @value[**]", { value : arrayOfArrays }).json;
      assertEqual([ expected ], result);
    },

    testCollapse3 : function () {
      var expected = [ "one", "two", "three", "four", "five", "six", "seven", "eight", "nine", "ten", "eleven", "twelve" ];
      var result = AQL_EXECUTE("RETURN @value[***]", { value : arrayOfArrays }).json;
      assertEqual([ expected ], result);
    },

    testCollapseUnmodified : function () {
      var data = [ [ [ [ 1, 2 ] ], [ 3, 4 ], 5, [ 6, 7 ] ], [ 8 ] ];
      var expected = data;
      var result = AQL_EXECUTE("RETURN @value", { value : data }).json;
      assertEqual([ expected ], result);
    },

    testCollapseMixed1 : function () {
      var data = [ [ [ [ 1, 2 ] ], [ 3, 4 ], 5, [ 6, 7 ] ], [ 8 ] ];
      var expected = data;
      var result = AQL_EXECUTE("RETURN @value[*]", { value : data }).json;
      assertEqual([ expected ], result);
    },

    testCollapseMixed2 : function () {
      var data = [ [ [ [ 1, 2 ] ], [ 3, 4 ], 5, [ 6, 7 ] ], [ 8 ] ];
      var expected = [ [ [ 1, 2 ] ], [ 3, 4 ], 5, [ 6, 7 ], 8 ];
      var result = AQL_EXECUTE("RETURN @value[**]", { value : data }).json;
      assertEqual([ expected ], result);
    },

    testCollapseMixed3 : function () {
      var data = [ [ [ [ 1, 2 ] ], [ 3, 4 ], 5, [ 6, 7 ] ], [ 8 ] ];
      var expected = [ [ 1, 2 ], 3, 4, 5, 6, 7, 8 ];
      var result = AQL_EXECUTE("RETURN @value[***]", { value : data }).json;
      assertEqual([ expected ], result);
    },

    testCollapseMixed4 : function () {
      var data = [ [ [ [ 1, 2 ] ], [ 3, 4 ], 5, [ 6, 7 ] ], [ 8 ] ];
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8 ];
      var result = AQL_EXECUTE("RETURN @value[****]", { value : data }).json;
      assertEqual([ expected ], result);
    },

    testCollapseMixed5 : function () {
      var data = [ [ [ [ 1, 2 ] ], [ 3, 4 ], 5, [ 6, 7 ] ], [ 8 ] ];
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8 ];
      var result = AQL_EXECUTE("RETURN @value[*****]", { value : data }).json;
      assertEqual([ expected ], result);
    },

    testCollapseFilter : function () {
      var data = [ 1, 2, 3, 4, 5, 6, 7, 8 ];
      var expected = [ 1, 3, 5, 7 ];
      var result = AQL_EXECUTE("RETURN @value[**** FILTER CURRENT % 2 != 0]", { value : data }).json;
      assertEqual([ expected ], result);
    },

    testCollapseProject : function () {
      var data = [ 1, 2, 3, 4, 5, 6, 7, 8 ];
      var expected = [ 2, 4, 6, 8, 10, 12, 14, 16 ];
      var result = AQL_EXECUTE("RETURN @value[**** RETURN CURRENT * 2]", { value : data }).json;
      assertEqual([ expected ], result);
    },

    testCollapseFilterProject : function () {
      var data = [ 1, 2, 3, 4, 5, 6, 7, 8 ];
      var expected = [ 4, 8, 12, 16 ];
      var result = AQL_EXECUTE("RETURN @value[**** FILTER CURRENT % 2 == 0 RETURN CURRENT * 2]", { value : data }).json;
      assertEqual([ expected ], result);
    },

    testCollapseFilterProjectLimit : function () {
      var data = [ 1, 2, 3, 4, 5, 6, 7, 8 ];
      var expected = [ 4, 8, 12 ];
      var result = AQL_EXECUTE("RETURN @value[**** FILTER CURRENT % 2 == 0 LIMIT 3 RETURN CURRENT * 2]", { value : data }).json;
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
      
      var result = AQL_EXECUTE("FOR c IN @value COLLECT st = c.st[* RETURN { id: CURRENT.id, date: CURRENT.last }], dx = c.st[*].dx[* RETURN { id: CURRENT.id, date: CURRENT.last }][**], docs = c.st[*].dx[*][**].docs[* RETURN { id: CURRENT.id, date: CURRENT.last }][**] RETURN { st , dx, docs }", { value : data }).json;
      assertEqual(expected, result);
    },

    testArrayFilterCompatibility : function () {
      // the following JSON was generated by the following code in 3.9:
      //   q = `RETURN [{Key: 'a', Value: 'a'}, {Key: 'a', Value: 'b'}, {Key: 'a', Value: 'd'}][* FILTER CURRENT.Key == 'a' && CURRENT.Value IN ['a', 'b', 'c']]`; 
      //   plan = AQL_EXPLAIN(q, null, { verbosePlans: true }).plan; 
      //   JSON.stringify(plan)
      const planFrom39 = {"nodes":[{"type":"SingletonNode","typeID":1,"dependencies":[],"id":1,"parents":[2],"estimatedCost":1,"estimatedNrItems":1,"depth":0,"varInfoList":[{"VariableId":3,"depth":0,"RegisterId":0}],"nrRegs":[1],"nrConstRegs":0,"regsToClear":[],"varsUsedLaterStack":[[{"id":3,"name":"2","isDataFromCollection":false}]],"regsToKeepStack":[[]],"varsValidStack":[[]],"isInSplicedSubquery":false,"isAsyncPrefetchEnabled":false,"isCallstackSplitEnabled":false},{"type":"CalculationNode","typeID":7,"dependencies":[1],"id":2,"parents":[3],"estimatedCost":2,"estimatedNrItems":1,"depth":0,"varInfoList":[{"VariableId":3,"depth":0,"RegisterId":0}],"nrRegs":[1],"nrConstRegs":0,"regsToClear":[],"varsUsedLaterStack":[[{"id":3,"name":"2","isDataFromCollection":false}]],"regsToKeepStack":[[]],"varsValidStack":[[{"id":3,"name":"2","isDataFromCollection":false}]],"isInSplicedSubquery":false,"isAsyncPrefetchEnabled":false,"isCallstackSplitEnabled":false,"expression":{"type":"expansion","typeID":38,"levels":1,"subNodes":[{"type":"iterator","typeID":39,"subNodes":[{"type":"variable","typeID":13,"name":"0_","id":1},{"type":"array","typeID":41,"subNodes":[{"type":"object","typeID":42,"subNodes":[{"type":"object element","typeID":43,"name":"Key","subNodes":[{"type":"value","typeID":40,"value":"a","vType":"string","vTypeID":4}]},{"type":"object element","typeID":43,"name":"Value","subNodes":[{"type":"value","typeID":40,"value":"a","vType":"string","vTypeID":4}]}]},{"type":"object","typeID":42,"subNodes":[{"type":"object element","typeID":43,"name":"Key","subNodes":[{"type":"value","typeID":40,"value":"a","vType":"string","vTypeID":4}]},{"type":"object element","typeID":43,"name":"Value","subNodes":[{"type":"value","typeID":40,"value":"b","vType":"string","vTypeID":4}]}]},{"type":"object","typeID":42,"subNodes":[{"type":"object element","typeID":43,"name":"Key","subNodes":[{"type":"value","typeID":40,"value":"a","vType":"string","vTypeID":4}]},{"type":"object element","typeID":43,"name":"Value","subNodes":[{"type":"value","typeID":40,"value":"d","vType":"string","vTypeID":4}]}]}]}]},{"type":"reference","typeID":45,"name":"0_","id":1},{"type":"logical and","typeID":18,"subNodes":[{"type":"compare ==","typeID":25,"excludesNull":false,"subNodes":[{"type":"attribute access","typeID":35,"name":"Key","subNodes":[{"type":"reference","typeID":45,"name":"0_","id":1}]},{"type":"value","typeID":40,"value":"a","vType":"string","vTypeID":4}]},{"type":"compare in","typeID":31,"sorted":false,"subNodes":[{"type":"attribute access","typeID":35,"name":"Value","subNodes":[{"type":"reference","typeID":45,"name":"0_","id":1}]},{"type":"array","typeID":41,"subNodes":[{"type":"value","typeID":40,"value":"a","vType":"string","vTypeID":4},{"type":"value","typeID":40,"value":"b","vType":"string","vTypeID":4},{"type":"value","typeID":40,"value":"c","vType":"string","vTypeID":4}]}]}]},{"type":"no-op","typeID":50},{"type":"no-op","typeID":50}]},"outVariable":{"id":3,"name":"2","isDataFromCollection":false},"canThrow":false,"expressionType":"simple","functions":[]},{"type":"ReturnNode","typeID":18,"dependencies":[2],"id":3,"parents":[],"estimatedCost":3,"estimatedNrItems":1,"depth":0,"varInfoList":[{"VariableId":3,"depth":0,"RegisterId":0}],"nrRegs":[1],"nrConstRegs":0,"regsToClear":[],"varsUsedLaterStack":[[]],"regsToKeepStack":[[]],"varsValidStack":[[{"id":3,"name":"2","isDataFromCollection":false}]],"isInSplicedSubquery":false,"isAsyncPrefetchEnabled":false,"isCallstackSplitEnabled":false,"inVariable":{"id":3,"name":"2","isDataFromCollection":false},"count":true}],"rules":[],"collections":[],"variables":[{"id":3,"name":"2","isDataFromCollection":false},{"id":1,"name":"0_","isDataFromCollection":false}],"estimatedCost":3,"estimatedNrItems":1,"isModificationQuery":false};

      let result = AQL_EXECUTEJSON(planFrom39).json;
      assertEqual([ [ { "Key" : "a", "Value" : "a" }, { "Key" : "a", "Value" : "b" } ] ], result);

      // plan for same query, but built by 3.10
      const planFrom310 = {"nodes":[{"type":"SingletonNode","typeID":1,"dependencies":[],"id":1,"parents":[2],"estimatedCost":1,"estimatedNrItems":1,"depth":0,"varInfoList":[{"VariableId":3,"depth":0,"RegisterId":0}],"nrRegs":[1],"nrConstRegs":0,"regsToClear":[],"varsUsedLaterStack":[[{"id":3,"name":"2","isDataFromCollection":false,"isFullDocumentFromCollection":false}]],"regsToKeepStack":[[]],"varsValidStack":[[]],"isInSplicedSubquery":false,"isAsyncPrefetchEnabled":false,"isCallstackSplitEnabled":false},{"type":"CalculationNode","typeID":7,"dependencies":[1],"id":2,"parents":[3],"estimatedCost":2,"estimatedNrItems":1,"depth":0,"varInfoList":[{"VariableId":3,"depth":0,"RegisterId":0}],"nrRegs":[1],"nrConstRegs":0,"regsToClear":[],"varsUsedLaterStack":[[{"id":3,"name":"2","isDataFromCollection":false,"isFullDocumentFromCollection":false}]],"regsToKeepStack":[[]],"varsValidStack":[[{"id":3,"name":"2","isDataFromCollection":false,"isFullDocumentFromCollection":false}]],"isInSplicedSubquery":false,"isAsyncPrefetchEnabled":false,"isCallstackSplitEnabled":false,"expression":{"type":"expansion","typeID":38,"levels":1,"booleanize":false,"subNodes":[{"type":"iterator","typeID":39,"subNodes":[{"type":"variable","typeID":13,"name":"0_","id":1},{"type":"array","typeID":41,"subNodes":[{"type":"object","typeID":42,"subNodes":[{"type":"object element","typeID":43,"name":"Key","subNodes":[{"type":"value","typeID":40,"value":"a","vType":"string","vTypeID":4}]},{"type":"object element","typeID":43,"name":"Value","subNodes":[{"type":"value","typeID":40,"value":"a","vType":"string","vTypeID":4}]}]},{"type":"object","typeID":42,"subNodes":[{"type":"object element","typeID":43,"name":"Key","subNodes":[{"type":"value","typeID":40,"value":"a","vType":"string","vTypeID":4}]},{"type":"object element","typeID":43,"name":"Value","subNodes":[{"type":"value","typeID":40,"value":"b","vType":"string","vTypeID":4}]}]},{"type":"object","typeID":42,"subNodes":[{"type":"object element","typeID":43,"name":"Key","subNodes":[{"type":"value","typeID":40,"value":"a","vType":"string","vTypeID":4}]},{"type":"object element","typeID":43,"name":"Value","subNodes":[{"type":"value","typeID":40,"value":"d","vType":"string","vTypeID":4}]}]}]}]},{"type":"reference","typeID":45,"name":"0_","id":1},{"type":"array filter","typeID":81,"subNodes":[{"type":"no-op","typeID":50},{"type":"logical and","typeID":18,"subNodes":[{"type":"compare ==","typeID":25,"excludesNull":false,"subNodes":[{"type":"attribute access","typeID":35,"name":"Key","subNodes":[{"type":"reference","typeID":45,"name":"0_","id":1}]},{"type":"value","typeID":40,"value":"a","vType":"string","vTypeID":4}]},{"type":"compare in","typeID":31,"sorted":false,"subNodes":[{"type":"attribute access","typeID":35,"name":"Value","subNodes":[{"type":"reference","typeID":45,"name":"0_","id":1}]},{"type":"array","typeID":41,"subNodes":[{"type":"value","typeID":40,"value":"a","vType":"string","vTypeID":4},{"type":"value","typeID":40,"value":"b","vType":"string","vTypeID":4},{"type":"value","typeID":40,"value":"c","vType":"string","vTypeID":4}]}]}]}]},{"type":"no-op","typeID":50},{"type":"no-op","typeID":50}]},"outVariable":{"id":3,"name":"2","isDataFromCollection":false,"isFullDocumentFromCollection":false},"canThrow":false,"expressionType":"simple","functions":[]},{"type":"ReturnNode","typeID":18,"dependencies":[2],"id":3,"parents":[],"estimatedCost":3,"estimatedNrItems":1,"depth":0,"varInfoList":[{"VariableId":3,"depth":0,"RegisterId":0}],"nrRegs":[1],"nrConstRegs":0,"regsToClear":[],"varsUsedLaterStack":[[]],"regsToKeepStack":[[]],"varsValidStack":[[{"id":3,"name":"2","isDataFromCollection":false,"isFullDocumentFromCollection":false}]],"isInSplicedSubquery":false,"isAsyncPrefetchEnabled":false,"isCallstackSplitEnabled":false,"inVariable":{"id":3,"name":"2","isDataFromCollection":false,"isFullDocumentFromCollection":false},"count":true}],"rules":[],"collections":[],"variables":[{"id":3,"name":"2","isDataFromCollection":false,"isFullDocumentFromCollection":false},{"id":1,"name":"0_","isDataFromCollection":false,"isFullDocumentFromCollection":false}],"estimatedCost":3,"estimatedNrItems":1,"isModificationQuery":false};
      
      result = AQL_EXECUTEJSON(planFrom310).json;
      assertEqual([ [ { "Key" : "a", "Value" : "a" }, { "Key" : "a", "Value" : "b" } ] ], result);
 
      // we expect 3.9 and 3.10 to generate different plans.
      // 3.10 is going to insert an ARRAY_FILTER for the array filter
      assertNotEqual(JSON.stringify(planFrom39), JSON.stringify(planFrom310));
    },

  };
}

jsunity.run(arrayAccessTestSuite);

return jsunity.done();
