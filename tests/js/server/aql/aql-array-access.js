/*jshint globalstrict:false, strict:false, maxlen: 700 */
/*global assertEqual, AQL_EXECUTE */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for query language, array accesses
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

function arrayAccessTestSuite () {
  var persons = [
    { name: "sir alfred", age: 60, loves: [ "lettuce", "flowers" ] }, 
    { person: { name: "gadgetto", age: 50, loves: "gadgets" } }, 
    { name: "everybody", loves: "sunshine" }, 
    { name: "judge", loves: [ "order", "policing", "weapons" ] },
    "something"
  ];

  var continents = [
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
  
  var tags = [ 
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

  var arrayOfArrays = [
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
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(arrayAccessTestSuite);

return jsunity.done();

