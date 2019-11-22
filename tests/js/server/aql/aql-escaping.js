/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, AQL_PARSE */

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
var helper = require("@arangodb/aql-helper");
var getQueryResults = helper.getQueryResults;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ahuacatlEscapingTestSuite () {
  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief test comment length
////////////////////////////////////////////////////////////////////////////////
    
    testLongComment : function () {
      var value = "";
      for (var i = 0; i < 10000; ++i) {
        value += "blablabla ";
      }
      assertEqual(100000, value.length);

      // multi-line comment
      var actual = getQueryResults("LET foo = /* " + value + " */ 'bar' RETURN /* " + value + " */ foo");
      assertEqual([ "bar" ], actual);

      // single-line comment
      actual = getQueryResults("LET foo = // " + value + " \n 'bar' RETURN // " + value + "\n foo");
      assertEqual([ "bar" ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test identifier length
////////////////////////////////////////////////////////////////////////////////
    
    testLongIdentifierLength : function () {
      var value = "";
      for (var i = 0; i < 10000; ++i) {
        value += "variable";
      }
      assertEqual(80000, value.length);

      var actual = getQueryResults("LET " + value + " = 12345 RETURN " + value);
      assertEqual([ 12345 ], actual);
      // backticks
      actual = getQueryResults("LET `" + value + "` = 12345 RETURN `" + value + "`");
      assertEqual([ 12345 ], actual);
      // forward ticks
      actual = getQueryResults("LET ´" + value + "´ = 12345 RETURN ´" + value + "´");
      assertEqual([ 12345 ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test string length
////////////////////////////////////////////////////////////////////////////////
    
    testLongReturnValue : function () {
      var value = "";
      for (var i = 0; i < 10000; ++i) {
        value += "testvalue!";
      }
      assertEqual(100000, value.length);

      var actual = getQueryResults("RETURN " + JSON.stringify(value));
      assertEqual([ value ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test bind parameter length
////////////////////////////////////////////////////////////////////////////////
    
    testLongBindParameterValue : function () {
      var value = "";
      for (var i = 0; i < 10000; ++i) {
        value += "testvalue!";
      }
      assertEqual(100000, value.length);

      var actual = getQueryResults("RETURN @what", { what: value });
      assertEqual([ value ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test token processing
////////////////////////////////////////////////////////////////////////////////
    
    testTokens : function () {
      var queries = [
        "LET `foo` = [ '1', \"2\", \"3\" ] RETURN ´foo´",
        "LET ´foo´ = [ \"1\", '2', '3' ] RETURN `foo`",
        "   \n\nLET \n\n\r\n´foo´\r\n\t =\t [\t \"1\"\r, \n'2',\t \"3\"\r\n ] \t RETURN\n\n `foo`  \r\n", 
        "   \n\nLET \n\n\r\nfoo\r\n\t =\t [\t '1'  \r, \n\"2\"  , \t '3'\r\n ] \tRETURN  \n\nfoo \r\n" 
      ];

      queries.forEach(function(query) {
        var actual = AQL_PARSE(query);
        var nodes = actual.ast[0].subNodes;
        assertEqual("let", nodes[0].type);
        assertEqual("variable", nodes[0].subNodes[0].type);
        assertEqual("foo", nodes[0].subNodes[0].name);

        assertEqual("array", nodes[0].subNodes[1].type);
        assertEqual("value", nodes[0].subNodes[1].subNodes[0].type);
        assertEqual("1", nodes[0].subNodes[1].subNodes[0].value);
        assertEqual("string", typeof nodes[0].subNodes[1].subNodes[0].value);
        assertEqual("value", nodes[0].subNodes[1].subNodes[1].type);
        assertEqual("2", nodes[0].subNodes[1].subNodes[1].value);
        assertEqual("string", typeof nodes[0].subNodes[1].subNodes[1].value);
        assertEqual("value", nodes[0].subNodes[1].subNodes[2].type);
        assertEqual("3", nodes[0].subNodes[1].subNodes[2].value);
        assertEqual("string", typeof nodes[0].subNodes[1].subNodes[2].value);

        assertEqual("return", nodes[1].type);
        assertEqual("reference", nodes[1].subNodes[0].type);
        assertEqual("foo", nodes[1].subNodes[0].name);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test simple name handling
////////////////////////////////////////////////////////////////////////////////

    testSimpleName : function () {
      var expected = [ 1, 2, 3 ];
      // backticks
      var actual = getQueryResults("FOR `x` IN [ 1, 2, 3 ] RETURN `x`");
      assertEqual(expected, actual);

      // forward ticks
      actual = getQueryResults("FOR ´x´ IN [ 1, 2, 3 ] RETURN ´x´");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test reserved name handling
////////////////////////////////////////////////////////////////////////////////
    
    testReservedNames1 : function () {
      var expected = [ 1, 2, 3 ];
      var names = [ "for", "let", "return", "sort", "collect", "limit", "filter", "asc", "desc", "in", "into" ];

      for (var i in names) {
        if (names.hasOwnProperty(i)) {
          // backticks
          var actual = getQueryResults("FOR `" + names[i] + "` IN [ 1, 2, 3 ] RETURN `" + names[i] + "`");
          assertEqual(expected, actual);

          // forward ticks
          actual = getQueryResults("FOR ´" + names[i] + "´ IN [ 1, 2, 3 ] RETURN ´" + names[i] + "´");
          assertEqual(expected, actual);
        }
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test reserved names handling
////////////////////////////////////////////////////////////////////////////////
    
    testReservedNames2 : function () {
      var expected = [ { "let" : 1 }, { "collect" : 2 }, { "sort" : 3 } ];
      // backticks
      var actual = getQueryResults("FOR `for` IN [ { \"let\" : 1 }, { \"collect\" : 2 }, { \"sort\" : 3 } ] RETURN `for`");
      assertEqual(expected, actual);

      // forward ticks
      actual = getQueryResults("FOR ´for´ IN [ { \"let\" : 1 }, { \"collect\" : 2 }, { \"sort\" : 3 } ] RETURN `for`");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test punctuation names escaping
////////////////////////////////////////////////////////////////////////////////
    
    testPunctuationBackticks1 : function () {
      var expected = [ 1, 2, 3 ];
      var actual = getQueryResults("FOR `brown_fox` IN [ 1, 2, 3 ] RETURN `brown_fox`");
      assertEqual(expected, actual);
      
      actual = getQueryResults("FOR ´brown_fox´ IN [ 1, 2, 3 ] RETURN ´brown_fox´");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test punctuation names escaping
////////////////////////////////////////////////////////////////////////////////
    
    testPunctuationBackticks2 : function () {
      var expected = [ 1, 2, 3 ];
      var actual = getQueryResults("FOR `brown_fox__1234_` IN [ 1, 2, 3 ] RETURN `brown_fox__1234_`");
      assertEqual(expected, actual);
      
      actual = getQueryResults("FOR ´brown_fox__1234_´ IN [ 1, 2, 3 ] RETURN ´brown_fox__1234_´");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test punctuation names escaping
////////////////////////////////////////////////////////////////////////////////
    
    testPunctuationBackticks3 : function () {
      var expected = [ 1, 2, 3 ];
      var actual = getQueryResults("FOR `brown fox  1234_` IN [ 1, 2, 3 ] RETURN `brown fox  1234_`");
      assertEqual(expected, actual);
      
      actual = getQueryResults("FOR ´brown fox  1234_´ IN [ 1, 2, 3 ] RETURN ´brown fox  1234_´");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test punctuation names escaping
////////////////////////////////////////////////////////////////////////////////
    
    testPunctuationBackticks4 : function () {
      var expected = [ 1, 3 ];
      var actual = getQueryResults("FOR r IN [ { \"a\" : 1, \"b\" : 1 }, { \"a\" : 2, \"b\" : 2 }, { \"a\" : 1, \"b\" : 3 } ] FILTER r.`a` == 1 RETURN r.`b`");
      assertEqual(expected, actual);
      
      actual = getQueryResults("FOR r IN [ { \"a\" : 1, \"b\" : 1 }, { \"a\" : 2, \"b\" : 2 }, { \"a\" : 1, \"b\" : 3 } ] FILTER r.´a´ == 1 RETURN r.´b´");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test punctuation names escaping
////////////////////////////////////////////////////////////////////////////////
    
    testPunctuationBackticks5 : function () {
      var expected = [ 1, 3 ];
      var actual = getQueryResults("FOR r IN [ { \"a fox\" : 1, \"b fox\" : 1 }, { \"a fox\" : 2, \"b fox\" : 2 }, { \"a fox\" : 1, \"b fox\" : 3 } ] FILTER r.`a fox` == 1 RETURN r.`b fox`");
      assertEqual(expected, actual);
      
      actual = getQueryResults("FOR r IN [ { \"a fox\" : 1, \"b fox\" : 1 }, { \"a fox\" : 2, \"b fox\" : 2 }, { \"a fox\" : 1, \"b fox\" : 3 } ] FILTER r.´a fox´ == 1 RETURN r.´b fox´");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test backticks with line breaks
////////////////////////////////////////////////////////////////////////////////
    
    testPunctuationBackticks6 : function () {
      var expected = { "a\r\nfox" : "jumped\nover\nit" };
      var actual = getQueryResults("FOR r IN [ " + JSON.stringify(expected) + " ] FILTER r.`a\r\nfox` == 'jumped\nover\nit' RETURN r"); 
      assertEqual([ expected ], actual);
      
      actual = getQueryResults("FOR r IN [ " + JSON.stringify(expected) + " ] FILTER r.´a\r\nfox´ == 'jumped\nover\nit' RETURN r"); 
      assertEqual([ expected ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test backticks with line breaks
////////////////////////////////////////////////////////////////////////////////
    
    testPunctuationBackticks7 : function () {
      var expected = { "a\r\nfox" : "jumped\nover\nit" };
      var actual = getQueryResults("FOR r IN [ " + JSON.stringify(expected) + " ] FILTER r.`a\r\nfox` == \"jumped\nover\nit\" RETURN r"); 
      assertEqual([ expected ], actual);
      
      actual = getQueryResults("FOR r IN [ " + JSON.stringify(expected) + " ] FILTER r.´a\r\nfox´ == \"jumped\nover\nit\" RETURN r"); 
      assertEqual([ expected ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test backticks with line breaks
////////////////////////////////////////////////////////////////////////////////
    
    testPunctuationBackticks8 : function () {
      var expected = { "a\\r\\nfox" : "jumped\\nover\\nit" };
      var actual = getQueryResults("FOR r IN [ " + JSON.stringify(expected) + " ] FILTER r.`a\\\\r\\\\nfox` == 'jumped\\\\nover\\\\nit' RETURN r"); 
      assertEqual([ expected ], actual);
      
      actual = getQueryResults("FOR r IN [ " + JSON.stringify(expected) + " ] FILTER r.´a\\\\r\\\\nfox´ == 'jumped\\\\nover\\\\nit' RETURN r"); 
      assertEqual([ expected ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test backticks with line breaks
////////////////////////////////////////////////////////////////////////////////
    
    testPunctuationBackticks9 : function () {
      var expected = { "a\\r\\nfox" : "jumped\\nover\\nit" };
      var actual = getQueryResults("FOR r IN [ " + JSON.stringify(expected) + " ] FILTER r.`a\\\\r\\\\nfox` == \"jumped\\\\nover\\\\nit\" RETURN r"); 
      assertEqual([ expected ], actual);
      
      actual = getQueryResults("FOR r IN [ " + JSON.stringify(expected) + " ] FILTER r.´a\\\\r\\\\nfox´ == \"jumped\\\\nover\\\\nit\" RETURN r"); 
      assertEqual([ expected ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test backticks with line breaks
////////////////////////////////////////////////////////////////////////////////
    
    testPunctuationBackticks10 : function () {
      var expected = { "a\\r\\nfox" : "jumped\\nover\\nit" };
      var actual = getQueryResults("FOR r IN [ @var1 ] FILTER r.`a\\\\r\\\\nfox` == @var2 RETURN r", { var1: expected, var2: expected["a\\r\\nfox"] }); 
      assertEqual([ expected ], actual);
      
      actual = getQueryResults("FOR r IN [ @var1 ] FILTER r.´a\\\\r\\\\nfox´ == @var2 RETURN r", { var1: expected, var2: expected["a\\r\\nfox"] }); 
      assertEqual([ expected ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test backticks with line breaks
////////////////////////////////////////////////////////////////////////////////
    
    testPunctuationBackticks11 : function () {
      var expected = { "a\r\nfox" : "jumped\nover\nit" };
      var actual = getQueryResults("FOR r IN [ @var1 ] FILTER r.`a\\r\\nfox` == @var2 RETURN r", { var1: expected, var2: expected["a\r\nfox"] }); 
      assertEqual([ expected ], actual);
      
      actual = getQueryResults("FOR r IN [ @var1 ] FILTER r.´a\\r\\nfox´ == @var2 RETURN r", { var1: expected, var2: expected["a\r\nfox"] }); 
      assertEqual([ expected ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test punctuation names escaping
////////////////////////////////////////////////////////////////////////////////
    
    testPunctuationDoubleQuotes1 : function () {
      var expected = [ { '"a"' : 1 }, { '"a"' : '"b"' } ];
      var actual = getQueryResults("FOR r IN [ { '\"a\"' : 1 }, { '\"a\"' : '\"b\"' } ] RETURN r");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test punctuation names escaping
////////////////////////////////////////////////////////////////////////////////
    
    testPunctuationDoubleQuotes2 : function () {
      var expected = [ { '"a fox"' : 1 }, { '"a fox "' : '"b fox"' } ];
      var actual = getQueryResults("FOR r IN [ { '\"a fox\"' : 1 }, { '\"a fox \"' : '\"b fox\"' } ] RETURN r");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test punctuation names escaping
////////////////////////////////////////////////////////////////////////////////
    
    testPunctuationDoubleQuotes3 : function () {
      var expected = [ { '"a" \\ " fox"' : 1 }, { '"a" \\ " fox "' : '"b fox"' } ];
      var actual = getQueryResults("FOR r IN [ { '\"a\" \\\\ \\\" fox\"' : 1 }, { '\"a\" \\\\ \\\" fox \"' : '\"b fox\"' } ] RETURN r");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test punctuation names escaping
////////////////////////////////////////////////////////////////////////////////
    
    testPunctuationSingleQuotes1 : function () {
      var expected = [ { "'a'" : 1 }, { "'a'" : "'b'" } ];
      var actual = getQueryResults("FOR r IN [ { \"'a'\" : 1 }, { \"'a'\" : \"'b'\" } ] RETURN r");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test punctuation names escaping
////////////////////////////////////////////////////////////////////////////////
    
    testPunctuationSingleQuotes2 : function () {
      var expected = [ { "'a fox'" : 1 }, { "'a fox'" : "'b fox'" } ];
      var actual = getQueryResults("FOR r IN [ { \"'a fox'\" : 1 }, { \"'a fox'\" : \"'b fox'\" } ] RETURN r");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test punctuation names escaping
////////////////////////////////////////////////////////////////////////////////
    
    testPunctuationSingleQuotes3 : function () {
      var expected = [ { "'a \\ ' fox'" : 1 }, { "'a \\ ' fox'" : "'b fox'" } ];
      var actual = getQueryResults("FOR r IN [ { \"'a \\\\ \\' fox'\" : 1 }, { \"'a \\\\ \\' fox'\" : \"'b fox'\" } ] RETURN r");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test punctuation unquoted names handling
////////////////////////////////////////////////////////////////////////////////
    
    testPunctuationNoQuotes1 : function () {
      var expected = [ { "r" : 1 }, { "r" : 2 }, { "r" : 3 } ];
      var actual = getQueryResults("FOR r IN [ 1, 2, 3 ] return { r : r }");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test punctuation unquoted names handling
////////////////////////////////////////////////////////////////////////////////
    
    testPunctuationNoQuotes2 : function () {
      var expected = [ { "alias" : 1 }, { "alias" : 2 }, { "alias" : 3 } ];
      var actual = getQueryResults("FOR r IN [ 1, 2, 3 ] return { alias : r }");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test punctuation unquoted names handling
////////////////////////////////////////////////////////////////////////////////
    
    testPunctuationNoQuotes3 : function () {
      var expected = [ { "v" : { "value" : 1 } }, { "v" : { "value" : 2 } } ];
      var actual = getQueryResults("FOR r IN [ 1, 2 ] LET fx = { value : r } RETURN { v : fx }");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test punctuation unquoted names handling
////////////////////////////////////////////////////////////////////////////////
    
    testPunctuationNoQuotes4 : function () {
      var expected = [ { "fx" : [ { "someval" : 99 }, { "someval" : 1 } ] }, { "fx" : [ { "someval" : 99 }, { "someval" : 2 } ] } ];
      var actual = getQueryResults("FOR r IN [ 1, 2 ] LET fx = ( FOR someval IN [ 99, r ] RETURN { someval : someval } ) RETURN { fx : fx }");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test UTF8 names escaping
////////////////////////////////////////////////////////////////////////////////
    
    testUtf8Names1 : function () {
      var expected = [ "wälder", "hänsel", "grätel", "fraß", "kloß" ];
      var actual = getQueryResults("FOR r IN [ { \"äöüÄÖÜß\" : \"wälder\" }, { \"äöüÄÖÜß\" : \"hänsel\" }, { \"äöüÄÖÜß\" : \"grätel\" }, { \"äöüÄÖÜß\" : \"fraß\" }, { \"äöüÄÖÜß\" : \"kloß\" } ] RETURN r.`äöüÄÖÜß`");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test UTF8 names escaping
////////////////////////////////////////////////////////////////////////////////
    
    testUtf8Names2 : function () {
      var expected = [ "中央アメリカ", "熱帯", "亜熱帯" ];
      var actual = getQueryResults("FOR r IN [ { \"アボカド\" : \"中央アメリカ\" }, { \"アボカド\" : \"熱帯\" }, { \"アボカド\" : \"亜熱帯\" } ] RETURN r.`アボカド`");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief line breaks
////////////////////////////////////////////////////////////////////////////////
    
    testLineBreaks1 : function () {
      var expected = [ "the\nquick\nbrown\r\nfox\rjumped\n" ];
      var actual = getQueryResults("RETURN \"the\nquick\nbrown\r\nfox\rjumped\n\"");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief line breaks2
////////////////////////////////////////////////////////////////////////////////
    
    testLineBreaks2 : function () {
      var expected = [ { "the\nquick\nbrown\r\nfox\rjumped\n" : "over\nthis\r\nattribute" } ];
      var actual = getQueryResults("RETURN { \"the\nquick\nbrown\r\nfox\rjumped\n\" : \"over\nthis\r\nattribute\" }");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief line breaks3
////////////////////////////////////////////////////////////////////////////////
    
    testLineBreaks3 : function () {
      var expected = [ { "the\nquick\nbrown\r\nfox\rjumped\n" : "over\nthis\r\nattribute" } ];
      var actual = getQueryResults("RETURN { 'the\nquick\nbrown\r\nfox\rjumped\n' : 'over\nthis\r\nattribute' }");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test comments
////////////////////////////////////////////////////////////////////////////////
    
    testComments1 : function () {
      var expected = [ "jumped" ];
      var actual = getQueryResults("RETURN /* \"the quick fox\" */ \"jumped\"");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test comments
////////////////////////////////////////////////////////////////////////////////
    
    testComments2 : function () {
      var expected = [ 1 ];
      var actual = getQueryResults("RETURN /* \"the\n \" \\qui\\ck\\ \"\"\"\" \" \"\"f\\ox\" */ 1");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test comments
////////////////////////////////////////////////////////////////////////////////
    
    testComments3 : function () {
      var expected = [ 1 ];
      var actual = getQueryResults("RETURN /*'the ' \\qui\\ck\\ ''''' '' 'f\\ox'*/ 1");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test comments
////////////////////////////////////////////////////////////////////////////////
    
    testComments4 : function () {
      var expected = [ 1 ];
      var actual = getQueryResults("RETURN // the quick fox\n 1");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test comments
////////////////////////////////////////////////////////////////////////////////
    
    testComments5 : function () {
      var expected = [ 1 ];
      var actual = getQueryResults("RETURN 1 // the quick fox\n");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test comments
////////////////////////////////////////////////////////////////////////////////
    
    testComments6 : function () {
      var expected = [ 1 ];
      var actual = getQueryResults("RETURN // the quick fox\n1");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test comments
////////////////////////////////////////////////////////////////////////////////
    
    testComments7 : function () {
      var expected = [ 1 ];
      var actual = getQueryResults("RETURN // /* the quick fox */ * foo \n1");
      assertEqual(expected, actual);
    }
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ahuacatlEscapingTestSuite);

return jsunity.done();

