/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, AQL_EXECUTE, AQL_EXPLAIN, AQL_EXECUTEJSON */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for dynamic attributes
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

function ahuacatlDynamicAttributesTestSuite () {

  function checkResult(query, expected) {
    let q = "RETURN NOOPT(" + query + ")";
    assertEqual(expected, AQL_EXECUTE(q).json[0]);
    assertEqual("simple", AQL_EXPLAIN(q).plan.nodes[1].expressionType);
    
    q = "RETURN " + query;
    assertEqual(expected, AQL_EXECUTE(q).json[0]);
    assertEqual("simple", AQL_EXPLAIN(q).plan.nodes[1].expressionType);
  }

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief test dynamic with non-string values
////////////////////////////////////////////////////////////////////////////////

    testDynamicAttributesNonStringNames : function () {
      var expected = { "" : 7, "true" : 12, "false" : 15, "123" : 21, "-42.5" : 99, "[]" : 117, "[1]" : 121, "[1,2,3,4]" : 131, "{\"a\":1}" : 149 };
      var query = "{ [ PASSTHRU(null) ] : 7, [ PASSTHRU(true) ] : 12, [ PASSTHRU(false) ] : 15, [ PASSTHRU(123) ] : 21, [ PASSTHRU(-42.5) ] : 99, [ PASSTHRU([ ]) ] : 117, [ PASSTHRU([ 1 ]) ] : 121, [ PASSTHRU([ 1, 2, 3, 4 ]) ] : 131, [ PASSTHRU({ a: 1 }) ] : 149 }";
      checkResult(query, expected);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test dynamic with keyword names
////////////////////////////////////////////////////////////////////////////////

    testDynamicAttributesReservedNames : function () {
      var expected = { LET: "LET", FILTER: "FILTER", SORT: "SORT", FOR: "FOR", "return": "RETURN" };
      var actual = AQL_EXECUTE("RETURN { [ UPPER('let') ] : 'LET', [ 'FILTER' ] : 'FILTER', [ PASSTHRU('SORT') ] : 'SORT', [ CONCAT('F', 'O', 'R') ] : 'FOR', [ PASSTHRU(LOWER('RETURN')) ] : 'RETURN' }");
      assertEqual(expected, actual.json[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test dynamic with special characters
////////////////////////////////////////////////////////////////////////////////

    testDynamicAttributesWhitespace : function () {
      var expected = { "1" : "one", " 1" : "one-1", "1 " : "1-one", " 1 " : "one-1-one" };
      var query = "{ [ '1' ] : 'one', [ ' 1' ] : 'one-1', [ '1 ' ] : '1-one', [ ' 1 ' ] : 'one-1-one' }";
      checkResult(query, expected);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test dynamic with special characters
////////////////////////////////////////////////////////////////////////////////

    testDynamicAttributesWhitespacePassthru : function () {
      var expected = { "1" : "one", " 1" : "one-1", "1 " : "1-one", " 1 " : "one-1-one" };
      var actual = AQL_EXECUTE("RETURN { [ PASSTHRU('1') ] : 'one', [ PASSTHRU(' 1') ] : 'one-1', [ PASSTHRU('1 ') ] : '1-one', [ PASSTHRU(' 1 ') ] : 'one-1-one' }");
      assertEqual(expected, actual.json[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test dynamic with special characters
////////////////////////////////////////////////////////////////////////////////

    testDynamicAttributesSpecialCharacters : function () {
      var expected = { "" : "empty", " " : "space", "  " : "two", "_" : "underscore", "\\" : "backslash", "\n" : "linebreak", "\t" : "tab!", "$" : "dollar", "1" : "one" };
      var actual = AQL_EXECUTE("RETURN { [ '' ] : 'empty', [ ' ' ] : 'space', [ '  ' ] : 'two', [ '_' ] : 'underscore', [ '\\\\'] : 'backslash', '\\n' : 'linebreak', '\\t' : 'tab!', '$' : 'dollar', '1' : 'one' }");
      assertEqual(expected, actual.json[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test dynamic with bind parameter-like names
////////////////////////////////////////////////////////////////////////////////

    testDynamicAttributesWithBindParameterLikeNames : function () {
      var expected = { "@abc": "abc", "@@a": "a", "@bc": "bc", "@ax": "ax" };
      var actual = AQL_EXECUTE("RETURN { [ '@abc' ] : 'abc', [ '@@a' ] : 'a', [ CONCAT('@', 'bc') ] : 'bc', [ PASSTHRU('@ax') ] : 'ax' }");
      assertEqual(expected, actual.json[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test dynamic with bind parameters
////////////////////////////////////////////////////////////////////////////////

    testDynamicAttributesWithBindParameters : function () {
      var expected = { "bind-abc": "xabc", "bind-a": "xa", "bind-bcde": "xbcde", "bind-ax": "xax" };
      var actual = AQL_EXECUTE("RETURN { [ @abc ] : 'xabc', [ @a ] : 'xa', [ CONCAT(@bc, 'de') ] : 'xbcde', [ PASSTHRU(@ax) ] : 'xax' }", { abc: "bind-abc", "a" : "bind-a", "bc" : "bind-bc", "ax" : "bind-ax" });
      assertEqual(expected, actual.json[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test dynamic with Unicode
////////////////////////////////////////////////////////////////////////////////

    testDynamicAttributesUmlaut : function () {
      var expected = { "KÖTÖR": "kötör", "trötör": "tröt!", "mÄgensÄftwürzgemösch": "möglich", "FöR": "FöR", "sörtierung": "sörtierung" };
      var actual = AQL_EXECUTE("RETURN { [ UPPER('kötör') ] : 'kötör', [ 'trötör' ] : 'tröt!', [ PASSTHRU('mÄgensÄftwürzgemösch') ] : 'möglich', [ CONCAT('F', 'ö', 'R') ] : 'FöR', [ PASSTHRU(LOWER('SÖRTIERUNG')) ] : 'sörtierung' }");
      assertEqual(expected, actual.json[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test dynamic with Unicode
////////////////////////////////////////////////////////////////////////////////

    testDynamicAttributesHangul : function () {
      var expected = { "코리아닷컴" : 1, "메일알리미" : 2 };
      var query = "{ [ PASSTHRU('코리아닷컴') ] : 1,  [ PASSTHRU('메일알리미') ] : 2 }";
      checkResult(query, expected);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test dynamic with Unicode
////////////////////////////////////////////////////////////////////////////////

    testDynamicAttributesChinese : function () {
      var expected = { "中华网以中国的市场为核心" : 23, "致力为当地用户提供流动增值服务" : 99 };
      var query = "{ [ PASSTHRU('中华网以中国的市场为核心') ] : 23, [ PASSTHRU('致力为当地用户提供流动增值服务') ] : 99 }";
      checkResult(query, expected);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test dynamic with Unicode
////////////////////////////////////////////////////////////////////////////////

    testDynamicAttributesRussian : function () {
      var expected = { "Голкипер" : 1, "мадридского" : 2, "Икер Касильяс призвал" : 3 };
      var query = "{ [ PASSTHRU('Голкипер') ] : 1, [ PASSTHRU('мадридского') ] : 2, [ PASSTHRU('Икер Касильяс призвал') ] : 3 }";
      checkResult(query, expected);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test dynamic with Unicode
////////////////////////////////////////////////////////////////////////////////

    testDynamicAttributesUnicodeMixed : function () {
      var expected = { "დახმარებისთვის" : 1, "töröö !" : 2, "ジャパン は、イギリスのニュー・ウェーヴバンド。デヴィッド" : 3 };
      var actual = AQL_EXECUTE("RETURN { [ PASSTHRU('დახმარებისთვის') ] : 1, [ PASSTHRU('töröö !') ] : 2, [ PASSTHRU('ジャパン は、イギリスのニュー・ウェーヴバンド。デヴィッド') ] : 3 }");
      assertEqual(expected, actual.json[0]);
    },
  
////////////////////////////////////////////////////////////////////////////////
/// @brief test dynamic with repeated attributes
////////////////////////////////////////////////////////////////////////////////

    testDynamicAttributesRepeated : function () {
      var expected = { FOO: 123, bar: 19 };
      var query = "{ [ 'FOO' ] : 123, [ PASSTHRU('FOO') ] : 42, [ 'bar' ] : 19, [ PASSTHRU('FOO') ] : 23 }";
      checkResult(query, expected);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test dynamic mixed with regular
////////////////////////////////////////////////////////////////////////////////

    testDynamicAttributesMixedWithRegular : function () {
      var expected = { foo: 1, FOO: 2, bar: 3, baR: 4, foobar: 6 };
      var query = "{ 'foo' : 1, [ 'FOO' ] : 2, 'bar' : 3, [ PASSTHRU('baR') ] : 4, [ PASSTHRU('bar') ] : 5, 'foobar' : 6 }";
      checkResult(query, expected);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test regular overwritten by dynamic
////////////////////////////////////////////////////////////////////////////////

    testDynamicAttributesOverwritingRegular : function () {
      var expected = { FOO: 2 };
      var query = "{ FOO : 2, [ PASSTHRU('FOO') ] : 1 }";
      checkResult(query, expected);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test dynamic overwritten by regular
////////////////////////////////////////////////////////////////////////////////

    testDynamicAttributesOverwrittenByRegular : function () {
      var expected = { FOO: 1 };
      var query = "{ [ PASSTHRU('FOO') ] : 1, FOO : 2 }";
      checkResult(query, expected);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test dynamic with for loop
////////////////////////////////////////////////////////////////////////////////

    testDynamicAttributesWithFor : function () {
      var expected = [ ];
      for (var i = 1; i <= 100; ++i) {
        var doc = { };
        doc["test-value-" + i] = i;
        expected.push(doc);
      }
      
      let actual = AQL_EXECUTE("FOR i IN 1..100 RETURN { [ CONCAT('test-value-', i) ] : i }");
      assertEqual(expected, actual.json);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test dynamic with for loop
////////////////////////////////////////////////////////////////////////////////

    testDynamicAttributesWithNestedFor : function () {
      var expected = [ ];
      for (var i = 1; i <= 10; ++i) {
        var doc = { };
        doc["one-" + i] = i;
        doc["two-23"] = 23;
        expected.push(doc);

        var doc2 = { };
        doc2["one-" + i] = i;
        doc2["two-42"] = 42;
        expected.push(doc2);
      }
      var actual = AQL_EXECUTE("FOR i IN 1..10 FOR j IN [ 23, 42 ] RETURN { [ CONCAT('one-', i) ] : i, [ CONCAT('two-', j) ] : j }");
      assertEqual(expected, actual.json);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test nested
////////////////////////////////////////////////////////////////////////////////

    testNestedObjectsWithDynamicAttributes : function () {
      var expected = {
        foo: "bar",
        bazbar: { 
          foo2: "foobar",
          bazbar: {
            2: 42,
            4: 23
          },
          BAZBAR: "foofoofoo"
        },
        lastone: "done"
      };

      var actual = AQL_EXECUTE("RETURN { [ 'foo' ] : 'bar', [ CONCAT('baz', 'bar') ] : { foo2: 'foobar', [ 'bazbar' ] : { [ 1 + 1 ] : 42, [ 2 + 2 ] : 23 }, [ UPPER('bazbar') ] : LOWER('FOOFOOFOO') }, [ CONCAT('last', 'one') ] : 'done' }");
      assertEqual(expected, actual.json[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test dynamic with conditional
////////////////////////////////////////////////////////////////////////////////

    testDynamicAttributesConditional : function () {
      var expected = [ ];
      for (var i = 1; i <= 100; ++i) {
        var doc = { };
        if (i < 50) {
          doc["whatever" + i] = i;
        }
        else {
          doc[i + "something-completely-different"] = i * 2;
        }
        expected.push(doc);
      }
      var actual = AQL_EXECUTE("FOR i IN 1..100 RETURN { [ i < 50 ? CONCAT('whatever', i) : CONCAT(i, 'something-completely-different') ] : i < 50 ? i : i * 2 }");
      assertEqual(expected, actual.json);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test dynamic explain
////////////////////////////////////////////////////////////////////////////////

    testDynamicExplain : function () {
      var expected = [ ];
      for (var i = 1; i <= 100; ++i) {
        var doc = { };
        if (i < 50) {
          doc["whatever" + i] = i;
        }
        else {
          doc[i + "something-completely-different"] = i * 2;
        }
        expected.push(doc);
      }
      var plan = AQL_EXPLAIN("FOR i IN 1..100 RETURN { [ i < 50 ? CONCAT('whatever', i) : CONCAT(i, 'something-completely-different') ] : i < 50 ? i : i * 2 }", { }, { verbosePlans: true }).plan;
      var actual = AQL_EXECUTEJSON(plan);
      assertEqual(expected, actual.json);
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ahuacatlDynamicAttributesTestSuite);

return jsunity.done();

