/*jshint globalstrict:false, strict:false, maxlen: 7000 */
/*global assertEqual, assertTrue, assertMatch, fail, AQL_EXECUTE */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for query language, PARSE function
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
var helper = require("@arangodb/aql-helper");
var getParseResults = helper.getParseResults;
var assertParseError = helper.assertParseError;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ahuacatlParseTestSuite () {
  var errors = internal.errors;

////////////////////////////////////////////////////////////////////////////////
/// @brief return the collection names from the result
////////////////////////////////////////////////////////////////////////////////

  function getCollections (result) {
    var collections = result.collections;

    assertTrue(result.parsed);
   
    collections.sort();
    return collections;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief return the bind parameter names from the result
////////////////////////////////////////////////////////////////////////////////

  function getParameters (result) {
    var parameters = result.parameters;

    assertTrue(result.parsed);
   
    parameters.sort();
    return parameters;
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
/// @brief test empty query
////////////////////////////////////////////////////////////////////////////////

    testEmptyQuery : function () {
      assertParseError(errors.ERROR_QUERY_EMPTY.code, "");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test broken queries
////////////////////////////////////////////////////////////////////////////////

    testBrokenQueries : function () {
      assertParseError(errors.ERROR_QUERY_PARSE.code, " "); 
      assertParseError(errors.ERROR_QUERY_PARSE.code, "  "); 
      assertParseError(errors.ERROR_QUERY_PARSE.code, "for"); 
      assertParseError(errors.ERROR_QUERY_PARSE.code, "for "); 
      assertParseError(errors.ERROR_QUERY_PARSE.code, "for u"); 
      assertParseError(errors.ERROR_QUERY_PARSE.code, "for u "); 
      assertParseError(errors.ERROR_QUERY_PARSE.code, "for u in"); 
      assertParseError(errors.ERROR_QUERY_PARSE.code, "for u in "); 
      assertParseError(errors.ERROR_QUERY_PARSE.code, "for u in ["); 
      assertParseError(errors.ERROR_QUERY_PARSE.code, "for u in [1"); 
      assertParseError(errors.ERROR_QUERY_PARSE.code, "for u in [1]"); 
      assertParseError(errors.ERROR_QUERY_PARSE.code, "for u in [1] return"); 
      assertParseError(errors.ERROR_QUERY_PARSE.code, "for u in [1] return u;"); 
      assertParseError(errors.ERROR_QUERY_PARSE.code, ";"); 
      assertParseError(errors.ERROR_QUERY_PARSE.code, "1"); 
      assertParseError(errors.ERROR_QUERY_PARSE.code, "for @u in users return 1"); 
      assertParseError(errors.ERROR_QUERY_PARSE.code, "return"); 
      assertParseError(errors.ERROR_QUERY_PARSE.code, "return "); 
      assertParseError(errors.ERROR_QUERY_PARSE.code, "return 1;"); 
      assertParseError(errors.ERROR_QUERY_PARSE.code, "return 1 +"); 
      assertParseError(errors.ERROR_QUERY_PARSE.code, "return 1 + 1 +"); 
      assertParseError(errors.ERROR_QUERY_PARSE.code, "return (1"); 
      assertParseError(errors.ERROR_QUERY_PARSE.code, "for f1 in x1"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test bind parameter names
////////////////////////////////////////////////////////////////////////////////

    testParameterNames : function () {
      assertEqual([ ], getParameters(getParseResults("return 1")));
      assertEqual([ ], getParameters(getParseResults("for u in [ 1, 2, 3] return 1")));
      assertEqual([ "u" ], getParameters(getParseResults("for u in users return @u")));
      assertEqual([ "b" ], getParameters(getParseResults("for a in b return @b")));
      assertEqual([ "b", "c" ], getParameters(getParseResults("for a in @b return @c")));
      assertEqual([ "friends", "relations", "u", "users" ], getParameters(getParseResults("for u in @users for f in @friends for r in @relations return @u")));
      assertEqual([ "friends", "relations", "u", "users" ], getParameters(getParseResults("for r in @relations for f in @friends for u in @users return @u")));
      assertEqual([ "1", "hans", "r" ], getParameters(getParseResults("for r in (for x in @hans return @1) return @r")));
      assertEqual([ "1", "2", "hans" ], getParameters(getParseResults("for r in [ @1, @2 ] return @hans")));
      assertEqual([ "@users", "users" ], getParameters(getParseResults("for r in @@users return @users")));
      assertEqual([ "@users" ], getParameters(getParseResults("for r in @@users return @@users")));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test collection names
////////////////////////////////////////////////////////////////////////////////

    testCollectionNames : function () {
      assertEqual([ ], getCollections(getParseResults("return 1")));
      assertEqual([ ], getCollections(getParseResults("for u in [ 1, 2, 3] return 1")));
      assertEqual([ "users" ], getCollections(getParseResults("for u in users return u")));
      assertEqual([ "b" ], getCollections(getParseResults("for a in b return b")));
      assertEqual([ "friends", "relations", "users" ], getCollections(getParseResults("for u in users for f in friends for r in relations return u")));
      assertEqual([ "friends", "relations", "users" ], getCollections(getParseResults("for r in relations for f in friends for u in users return u")));
      assertEqual([ "hans" ], getCollections(getParseResults("for r in (for x in hans return 1) return r")));
      assertEqual([ "hans" ], getCollections(getParseResults("for r in [ 1, 2 ] return hans")));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test bind parameter names in comments
////////////////////////////////////////////////////////////////////////////////

    testComments : function () {
      assertEqual([ ], getParameters(getParseResults("return /* @nada */ 1")));
      assertEqual([ ], getParameters(getParseResults("return /* @@nada */ 1")));
      assertEqual([ ], getParameters(getParseResults("/*   @nada   */ return /* @@nada */ /*@@nada*/ 1 /*@nada*/")));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test string parsing
////////////////////////////////////////////////////////////////////////////////

    testStrings : function () {
      function getRoot(query) { return getParseResults(query).ast[0]; }

      var returnNode = getRoot("return 'abcdef'").subNodes[0];
      assertEqual("return", returnNode.type);
      assertEqual("value", returnNode.subNodes[0].type);
      assertEqual("abcdef", returnNode.subNodes[0].value);
      
      returnNode = getRoot("return 'abcdef ghi'").subNodes[0];
      assertEqual("return", returnNode.type);
      assertEqual("value", returnNode.subNodes[0].type);
      assertEqual("abcdef ghi", returnNode.subNodes[0].value);
      
      returnNode = getRoot("return 'abcd\"\\'ab\\nc'").subNodes[0];
      assertEqual("return", returnNode.type);
      assertEqual("value", returnNode.subNodes[0].type);
      assertEqual("abcd\"'ab\nc", returnNode.subNodes[0].value);
      
      returnNode = getRoot("return '\\'abcd\"\\'ab\nnc'").subNodes[0];
      assertEqual("return", returnNode.type);
      assertEqual("value", returnNode.subNodes[0].type);
      assertEqual("'abcd\"'ab\nnc", returnNode.subNodes[0].value);
      
      returnNode = getRoot("return \"abcdef\"").subNodes[0];
      assertEqual("return", returnNode.type);
      assertEqual("value", returnNode.subNodes[0].type);
      assertEqual("abcdef", returnNode.subNodes[0].value);
      
      returnNode = getRoot("return \"abcdef ghi\"").subNodes[0];
      assertEqual("return", returnNode.type);
      assertEqual("value", returnNode.subNodes[0].type);
      assertEqual("abcdef ghi", returnNode.subNodes[0].value);
      
      returnNode = getRoot("return \"abcd\\\"\\'ab\\nc\"").subNodes[0];
      assertEqual("return", returnNode.type);
      assertEqual("value", returnNode.subNodes[0].type);
      assertEqual("abcd\"'ab\nc", returnNode.subNodes[0].value);
      
      returnNode = getRoot("return \"\\'abcd\\\"\\'ab\nnc\"").subNodes[0];
      assertEqual("return", returnNode.type);
      assertEqual("value", returnNode.subNodes[0].type);
      assertEqual("'abcd\"'ab\nnc", returnNode.subNodes[0].value);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test string parsing
////////////////////////////////////////////////////////////////////////////////

    testStringsAfterNot : function () {
      function getRoot(query) { return getParseResults(query).ast[0]; }

      var returnNode = getRoot("return NOT ('abc' == 'def')").subNodes[0];
      assertEqual("return", returnNode.type);
      assertEqual("unary not", returnNode.subNodes[0].type);
      assertEqual("compare ==", returnNode.subNodes[0].subNodes[0].type);
      assertEqual("abc", returnNode.subNodes[0].subNodes[0].subNodes[0].value);
      assertEqual("def", returnNode.subNodes[0].subNodes[0].subNodes[1].value);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test too many collections
////////////////////////////////////////////////////////////////////////////////

    testTooManyCollections : function () {
      assertParseError(errors.ERROR_QUERY_TOO_MANY_COLLECTIONS.code, "for x0 in y0 for x1 in y1 for x2 in y2 for x3 in y3 for x4 in y4 for x5 in y5 for x6 in y6 for x7 in y7 for x8 in y8 for x9 in y9 for x10 in y10 for x11 in y11 for x12 in y12 for x13 in y13 for x14 in y14 for x15 in y15 for x16 in y16 for x17 in y17 for x18 in y18 for x19 in y19 for x20 in y20 for x21 in y21 for x22 in y22 for x23 in y23 for x24 in y24 for x25 in y25 for x26 in y26 for x27 in y27 for x28 in y28 for x29 in y29 for x30 in y30 for x31 in y31 for x32 in y32 for x33 in y33 for x34 in y34 for x35 in y35 for x36 in y36 for x37 in y37 for x38 in y38 for x39 in y39 for x40 in y40 for x41 in y41 for x42 in y42 for x43 in y43 for x44 in y44 for x45 in y45 for x46 in y46 for x47 in y47 for x48 in y48 for x49 in y49 for x50 in y50 for x51 in y51 for x52 in y52 for x53 in y53 for x54 in y54 for x55 in y55 for x56 in y56 for x57 in y57 for x58 in y58 for x59 in y59 for x60 in y60 for x61 in y61 for x62 in y62 for x63 in y63 for x64 in y64 for x65 in y65 for x66 in y66 for x67 in y67 for x68 in y68 for x69 in y69 for x70 in y70 for x71 in y71 for x72 in y72 for x73 in y73 for x74 in y74 for x75 in y75 for x76 in y76 for x77 in y77 for x78 in y78 for x79 in y79 for x80 in y80 for x81 in y81 for x82 in y82 for x83 in y83 for x84 in y84 for x85 in y85 for x86 in y86 for x87 in y87 for x88 in y88 for x89 in y89 for x90 in y90 for x91 in y91 for x92 in y92 for x93 in y93 for x94 in y94 for x95 in y95 for x96 in y96 for x97 in y97 for x98 in y98 for x99 in y99 for x100 in y100 for x101 in y101 for x102 in y102 for x103 in y103 for x104 in y104 for x105 in y105 for x106 in y106 for x107 in y107 for x108 in y108 for x109 in y109 for x110 in y110 for x111 in y111 for x112 in y112 for x113 in y113 for x114 in y114 for x115 in y115 for x116 in y116 for x117 in y117 for x118 in y118 for x119 in y119 for x120 in y120 for x121 in y121 for x122 in y122 for x123 in y123 for x124 in y124 for x125 in y125 for x126 in y126 for x127 in y127 for x128 in y128 for x129 in y129 for x130 in y130 for x131 in y131 for x132 in y132 for x133 in y133 for x134 in y134 for x135 in y135 for x136 in y136 for x137 in y137 for x138 in y138 for x139 in y139 for x140 in y140 for x141 in y141 for x142 in y142 for x143 in y143 for x144 in y144 for x145 in y145 for x146 in y146 for x147 in y147 for x148 in y148 for x149 in y149 for x150 in y150 for x151 in y151 for x152 in y152 for x153 in y153 for x154 in y154 for x155 in y155 for x156 in y156 for x157 in y157 for x158 in y158 for x159 in y159 for x160 in y160 for x161 in y161 for x162 in y162 for x163 in y163 for x164 in y164 for x165 in y165 for x166 in y166 for x167 in y167 for x168 in y168 for x169 in y169 for x170 in y170 for x171 in y171 for x172 in y172 for x173 in y173 for x174 in y174 for x175 in y175 for x176 in y176 for x177 in y177 for x178 in y178 for x179 in y179 for x180 in y180 for x181 in y181 for x182 in y182 for x183 in y183 for x184 in y184 for x185 in y185 for x186 in y186 for x187 in y187 for x188 in y188 for x189 in y189 for x190 in y190 for x191 in y191 for x192 in y192 for x193 in y193 for x194 in y194 for x195 in y195 for x196 in y196 for x197 in y197 for x198 in y198 for x199 in y199 for x200 in y200 for x201 in y201 for x202 in y202 for x203 in y203 for x204 in y204 for x205 in y205 for x206 in y206 for x207 in y207 for x208 in y208 for x209 in y209 for x210 in y210 for x211 in y211 for x212 in y212 for x213 in y213 for x214 in y214 for x215 in y215 for x216 in y216 for x217 in y217 for x218 in y218 for x219 in y219 for x220 in y220 for x221 in y221 for x222 in y222 for x223 in y223 for x224 in y224 for x225 in y225 for x226 in y226 for x227 in y227 for x228 in y228 for x229 in y229 for x230 in y230 for x231 in y231 for x232 in y232 for x233 in y233 for x234 in y234 for x235 in y235 for x236 in y236 for x237 in y237 for x238 in y238 for x239 in y239 for x240 in y240 for x241 in y241 for x242 in y242 for x243 in y243 for x244 in y244 for x245 in y245 for x246 in y246 for x247 in y247 for x248 in y248 for x249 in y249 for x250 in y250 for x251 in y251 for x252 in y252 for x253 in y253 for x254 in y254 for x255 in y255 for x256 in y256 for x257 in y257 return x1");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test line numbers in parse errors
////////////////////////////////////////////////////////////////////////////////

    testLineNumbers : function () {
      var queries = [
        [ "RETURN 1 +", [ 1, 10 ] ],
        [ "RETURN 1 + ", [ 1, 11 ] ],
        [ "RETURN 1 + ", [ 1, 11 ] ],
        [ "\nRETURN 1 + ", [ 2, 11 ] ],
        [ "\r\nRETURN 1 + ", [ 2, 11 ] ],
        [ "\n\n   RETURN 1 + ", [ 3, 14 ] ],
        [ "\n// foo\n   RETURN 1 + ", [ 3, 14 ] ],
        [ "\n/* foo \n\n bar */\n   RETURN 1 + ", [ 5, 14 ] ],
        [ "RETURN \"\n\n\" + ", [ 3, 5 ] ],
        [ "RETURN '\n\n' + ", [ 3, 5 ] ],
        [ "RETURN \"\n\n", [ 3, 1 ] ],
        [ "RETURN '\n\n", [ 3, 1 ] ],
        [ "RETURN `ahaa\n\n", [ 3, 1 ] ],
        [ "RETURN `ahaa\n\n", [ 3, 1 ] ],
        [ "RETURN `ahaa\n\n ", [ 3, 2 ] ]
      ];

      queries.forEach(function(query) {
        try {
          AQL_EXECUTE(query[0]);
          fail();
        }
        catch (err) {
          assertEqual(errors.ERROR_QUERY_PARSE.code, err.errorNum);
          assertMatch(/at position \d+:\d+/, err.errorMessage);
          var matches = err.errorMessage.match(/at position (\d+):(\d+)/);
          var line = parseInt(matches[1], 10);
          var column = parseInt(matches[2], 10);
          assertEqual(query[1][0], line);
          assertEqual(query[1][1], column);
        }
      });
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ahuacatlParseTestSuite);

return jsunity.done();

