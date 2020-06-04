/*jshint globalstrict:false, strict:false, maxlen: 7000 */
/*global assertEqual, assertTrue, assertMatch, fail, AQL_EXECUTE, AQL_PARSE */

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

function ahuacatlParseTestSuite () {
  var errors = internal.errors;

  function getCollections (result) {
    var collections = result.collections;

    assertTrue(result.parsed);
   
    collections.sort();
    return collections;
  }

  function getParameters (result) {
    var parameters = result.parameters;

    assertTrue(result.parsed);
   
    parameters.sort();
    return parameters;
  }

  return {

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
      assertParseError(errors.ERROR_QUERY_PARSE.code, "return 00"); 
      assertParseError(errors.ERROR_QUERY_PARSE.code, "return 01"); 
      assertParseError(errors.ERROR_QUERY_PARSE.code, "return 1."); 
      assertParseError(errors.ERROR_QUERY_PARSE.code, "return -");
      assertParseError(errors.ERROR_QUERY_PARSE.code, "return +");
      assertParseError(errors.ERROR_QUERY_PARSE.code, "return ."); 
      assertParseError(errors.ERROR_QUERY_PARSE.code, "RETURN 1 /* "); 
      assertParseError(errors.ERROR_QUERY_PARSE.code, "RETURN 1 \" foo "); 
      assertParseError(errors.ERROR_QUERY_PARSE.code, "RETURN 1 ' foo "); 
      assertParseError(errors.ERROR_QUERY_PARSE.code, "RETURN 1 `foo "); 
      assertParseError(errors.ERROR_QUERY_PARSE.code, "RETURN 1 ´foo "); 
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
/// @brief test number parsing
////////////////////////////////////////////////////////////////////////////////

    testNumbers : function () {
      function getRoot(query) { return getParseResults(query).ast[0]; }

      var returnNode = getRoot("return 0").subNodes[0];
      assertEqual("return", returnNode.type);
      assertEqual("value", returnNode.subNodes[0].type);
      assertEqual(0, returnNode.subNodes[0].value);
      
      returnNode = getRoot("return 1").subNodes[0];
      assertEqual("return", returnNode.type);
      assertEqual("value", returnNode.subNodes[0].type);
      assertEqual(1, returnNode.subNodes[0].value);
      
      returnNode = getRoot("return 9993835").subNodes[0];
      assertEqual("return", returnNode.type);
      assertEqual("value", returnNode.subNodes[0].type);
      assertEqual(9993835, returnNode.subNodes[0].value);
      
      returnNode = getRoot("return .24982").subNodes[0];
      assertEqual("return", returnNode.type);
      assertEqual("value", returnNode.subNodes[0].type);
      assertEqual(0.24982, returnNode.subNodes[0].value);
      
      returnNode = getRoot("return 1.2").subNodes[0];
      assertEqual("return", returnNode.type);
      assertEqual("value", returnNode.subNodes[0].type);
      assertEqual(1.2, returnNode.subNodes[0].value);
      
      returnNode = getRoot("return 13442.029285").subNodes[0];
      assertEqual("return", returnNode.type);
      assertEqual("value", returnNode.subNodes[0].type);
      assertEqual(13442.029285, returnNode.subNodes[0].value);
      
      returnNode = getRoot("return 12345").subNodes[0];
      assertEqual("return", returnNode.type);
      assertEqual("value", returnNode.subNodes[0].type);
      assertEqual(12345, returnNode.subNodes[0].value);
      
      returnNode = getRoot("return -1").subNodes[0];
      assertEqual("return", returnNode.type);
      assertEqual("unary minus", returnNode.subNodes[0].type);
      assertEqual("value", returnNode.subNodes[0].subNodes[0].type);
      assertEqual(1, returnNode.subNodes[0].subNodes[0].value);
      
      returnNode = getRoot("return -193817872892").subNodes[0];
      assertEqual("return", returnNode.type);
      assertEqual("unary minus", returnNode.subNodes[0].type);
      assertEqual("value", returnNode.subNodes[0].subNodes[0].type);
      assertEqual(193817872892, returnNode.subNodes[0].subNodes[0].value);
      
      returnNode = getRoot("return -.95264").subNodes[0];
      assertEqual("return", returnNode.type);
      assertEqual("unary minus", returnNode.subNodes[0].type);
      assertEqual("value", returnNode.subNodes[0].subNodes[0].type);
      assertEqual(0.95264, returnNode.subNodes[0].subNodes[0].value);
      
      returnNode = getRoot("return -12345").subNodes[0];
      assertEqual("return", returnNode.type);
      assertEqual("unary minus", returnNode.subNodes[0].type);
      assertEqual("value", returnNode.subNodes[0].subNodes[0].type);
      assertEqual(12345, returnNode.subNodes[0].subNodes[0].value);
      
      returnNode = getRoot("return -92.4987521").subNodes[0];
      assertEqual("return", returnNode.type);
      assertEqual("unary minus", returnNode.subNodes[0].type);
      assertEqual("value", returnNode.subNodes[0].subNodes[0].type);
      assertEqual(92.4987521, returnNode.subNodes[0].subNodes[0].value);
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
      assertParseError(errors.ERROR_QUERY_TOO_MANY_COLLECTIONS.code, "RETURN MERGE((FOR x IN y0 RETURN x), (FOR x IN y1 RETURN x), (FOR x IN y2 RETURN x), (FOR x IN y3 RETURN x), (FOR x IN y4 RETURN x), (FOR x IN y5 RETURN x), (FOR x IN y6 RETURN x), (FOR x IN y7 RETURN x), (FOR x IN y8 RETURN x), (FOR x IN y9 RETURN x), (FOR x IN y10 RETURN x), (FOR x IN y11 RETURN x), (FOR x IN y12 RETURN x), (FOR x IN y13 RETURN x), (FOR x IN y14 RETURN x), (FOR x IN y15 RETURN x), (FOR x IN y16 RETURN x), (FOR x IN y17 RETURN x), (FOR x IN y18 RETURN x), (FOR x IN y19 RETURN x), (FOR x IN y20 RETURN x), (FOR x IN y21 RETURN x), (FOR x IN y22 RETURN x), (FOR x IN y23 RETURN x), (FOR x IN y24 RETURN x), (FOR x IN y25 RETURN x), (FOR x IN y26 RETURN x), (FOR x IN y27 RETURN x), (FOR x IN y28 RETURN x), (FOR x IN y29 RETURN x), (FOR x IN y30 RETURN x), (FOR x IN y31 RETURN x), (FOR x IN y32 RETURN x), (FOR x IN y33 RETURN x), (FOR x IN y34 RETURN x), (FOR x IN y35 RETURN x), (FOR x IN y36 RETURN x), (FOR x IN y37 RETURN x), (FOR x IN y38 RETURN x), (FOR x IN y39 RETURN x), (FOR x IN y40 RETURN x), (FOR x IN y41 RETURN x), (FOR x IN y42 RETURN x), (FOR x IN y43 RETURN x), (FOR x IN y44 RETURN x), (FOR x IN y45 RETURN x), (FOR x IN y46 RETURN x), (FOR x IN y47 RETURN x), (FOR x IN y48 RETURN x), (FOR x IN y49 RETURN x), (FOR x IN y50 RETURN x), (FOR x IN y51 RETURN x), (FOR x IN y52 RETURN x), (FOR x IN y53 RETURN x), (FOR x IN y54 RETURN x), (FOR x IN y55 RETURN x), (FOR x IN y56 RETURN x), (FOR x IN y57 RETURN x), (FOR x IN y58 RETURN x), (FOR x IN y59 RETURN x), (FOR x IN y60 RETURN x), (FOR x IN y61 RETURN x), (FOR x IN y62 RETURN x), (FOR x IN y63 RETURN x), (FOR x IN y64 RETURN x), (FOR x IN y65 RETURN x), (FOR x IN y66 RETURN x), (FOR x IN y67 RETURN x), (FOR x IN y68 RETURN x), (FOR x IN y69 RETURN x), (FOR x IN y70 RETURN x), (FOR x IN y71 RETURN x), (FOR x IN y72 RETURN x), (FOR x IN y73 RETURN x), (FOR x IN y74 RETURN x), (FOR x IN y75 RETURN x), (FOR x IN y76 RETURN x), (FOR x IN y77 RETURN x), (FOR x IN y78 RETURN x), (FOR x IN y79 RETURN x), (FOR x IN y80 RETURN x), (FOR x IN y81 RETURN x), (FOR x IN y82 RETURN x), (FOR x IN y83 RETURN x), (FOR x IN y84 RETURN x), (FOR x IN y85 RETURN x), (FOR x IN y86 RETURN x), (FOR x IN y87 RETURN x), (FOR x IN y88 RETURN x), (FOR x IN y89 RETURN x), (FOR x IN y90 RETURN x), (FOR x IN y91 RETURN x), (FOR x IN y92 RETURN x), (FOR x IN y93 RETURN x), (FOR x IN y94 RETURN x), (FOR x IN y95 RETURN x), (FOR x IN y96 RETURN x), (FOR x IN y97 RETURN x), (FOR x IN y98 RETURN x), (FOR x IN y99 RETURN x), (FOR x IN y100 RETURN x), (FOR x IN y101 RETURN x), (FOR x IN y102 RETURN x), (FOR x IN y103 RETURN x), (FOR x IN y104 RETURN x), (FOR x IN y105 RETURN x), (FOR x IN y106 RETURN x), (FOR x IN y107 RETURN x), (FOR x IN y108 RETURN x), (FOR x IN y109 RETURN x), (FOR x IN y110 RETURN x), (FOR x IN y111 RETURN x), (FOR x IN y112 RETURN x), (FOR x IN y113 RETURN x), (FOR x IN y114 RETURN x), (FOR x IN y115 RETURN x), (FOR x IN y116 RETURN x), (FOR x IN y117 RETURN x), (FOR x IN y118 RETURN x), (FOR x IN y119 RETURN x), (FOR x IN y120 RETURN x), (FOR x IN y121 RETURN x), (FOR x IN y122 RETURN x), (FOR x IN y123 RETURN x), (FOR x IN y124 RETURN x), (FOR x IN y125 RETURN x), (FOR x IN y126 RETURN x), (FOR x IN y127 RETURN x), (FOR x IN y128 RETURN x), (FOR x IN y129 RETURN x), (FOR x IN y130 RETURN x), (FOR x IN y131 RETURN x), (FOR x IN y132 RETURN x), (FOR x IN y133 RETURN x), (FOR x IN y134 RETURN x), (FOR x IN y135 RETURN x), (FOR x IN y136 RETURN x), (FOR x IN y137 RETURN x), (FOR x IN y138 RETURN x), (FOR x IN y139 RETURN x), (FOR x IN y140 RETURN x), (FOR x IN y141 RETURN x), (FOR x IN y142 RETURN x), (FOR x IN y143 RETURN x), (FOR x IN y144 RETURN x), (FOR x IN y145 RETURN x), (FOR x IN y146 RETURN x), (FOR x IN y147 RETURN x), (FOR x IN y148 RETURN x), (FOR x IN y149 RETURN x), (FOR x IN y150 RETURN x), (FOR x IN y151 RETURN x), (FOR x IN y152 RETURN x), (FOR x IN y153 RETURN x), (FOR x IN y154 RETURN x), (FOR x IN y155 RETURN x), (FOR x IN y156 RETURN x), (FOR x IN y157 RETURN x), (FOR x IN y158 RETURN x), (FOR x IN y159 RETURN x), (FOR x IN y160 RETURN x), (FOR x IN y161 RETURN x), (FOR x IN y162 RETURN x), (FOR x IN y163 RETURN x), (FOR x IN y164 RETURN x), (FOR x IN y165 RETURN x), (FOR x IN y166 RETURN x), (FOR x IN y167 RETURN x), (FOR x IN y168 RETURN x), (FOR x IN y169 RETURN x), (FOR x IN y170 RETURN x), (FOR x IN y171 RETURN x), (FOR x IN y172 RETURN x), (FOR x IN y173 RETURN x), (FOR x IN y174 RETURN x), (FOR x IN y175 RETURN x), (FOR x IN y176 RETURN x), (FOR x IN y177 RETURN x), (FOR x IN y178 RETURN x), (FOR x IN y179 RETURN x), (FOR x IN y180 RETURN x), (FOR x IN y181 RETURN x), (FOR x IN y182 RETURN x), (FOR x IN y183 RETURN x), (FOR x IN y184 RETURN x), (FOR x IN y185 RETURN x), (FOR x IN y186 RETURN x), (FOR x IN y187 RETURN x), (FOR x IN y188 RETURN x), (FOR x IN y189 RETURN x), (FOR x IN y190 RETURN x), (FOR x IN y191 RETURN x), (FOR x IN y192 RETURN x), (FOR x IN y193 RETURN x), (FOR x IN y194 RETURN x), (FOR x IN y195 RETURN x), (FOR x IN y196 RETURN x), (FOR x IN y197 RETURN x), (FOR x IN y198 RETURN x), (FOR x IN y199 RETURN x), (FOR x IN y200 RETURN x), (FOR x IN y201 RETURN x), (FOR x IN y202 RETURN x), (FOR x IN y203 RETURN x), (FOR x IN y204 RETURN x), (FOR x IN y205 RETURN x), (FOR x IN y206 RETURN x), (FOR x IN y207 RETURN x), (FOR x IN y208 RETURN x), (FOR x IN y209 RETURN x), (FOR x IN y210 RETURN x), (FOR x IN y211 RETURN x), (FOR x IN y212 RETURN x), (FOR x IN y213 RETURN x), (FOR x IN y214 RETURN x), (FOR x IN y215 RETURN x), (FOR x IN y216 RETURN x), (FOR x IN y217 RETURN x), (FOR x IN y218 RETURN x), (FOR x IN y219 RETURN x), (FOR x IN y220 RETURN x), (FOR x IN y221 RETURN x), (FOR x IN y222 RETURN x), (FOR x IN y223 RETURN x), (FOR x IN y224 RETURN x), (FOR x IN y225 RETURN x), (FOR x IN y226 RETURN x), (FOR x IN y227 RETURN x), (FOR x IN y228 RETURN x), (FOR x IN y229 RETURN x), (FOR x IN y230 RETURN x), (FOR x IN y231 RETURN x), (FOR x IN y232 RETURN x), (FOR x IN y233 RETURN x), (FOR x IN y234 RETURN x), (FOR x IN y235 RETURN x), (FOR x IN y236 RETURN x), (FOR x IN y237 RETURN x), (FOR x IN y238 RETURN x), (FOR x IN y239 RETURN x), (FOR x IN y240 RETURN x), (FOR x IN y241 RETURN x), (FOR x IN y242 RETURN x), (FOR x IN y243 RETURN x), (FOR x IN y244 RETURN x), (FOR x IN y245 RETURN x), (FOR x IN y246 RETURN x), (FOR x IN y247 RETURN x), (FOR x IN y248 RETURN x), (FOR x IN y249 RETURN x), (FOR x IN y250 RETURN x), (FOR x IN y251 RETURN x), (FOR x IN y252 RETURN x), (FOR x IN y253 RETURN x), (FOR x IN y254 RETURN x), (FOR x IN y255 RETURN x), (FOR x IN y256 RETURN x), (FOR x IN y257 RETURN x), (FOR x IN y258 RETURN x), (FOR x IN y259 RETURN x), (FOR x IN y260 RETURN x), (FOR x IN y261 RETURN x), (FOR x IN y262 RETURN x), (FOR x IN y263 RETURN x), (FOR x IN y264 RETURN x), (FOR x IN y265 RETURN x), (FOR x IN y266 RETURN x), (FOR x IN y267 RETURN x), (FOR x IN y268 RETURN x), (FOR x IN y269 RETURN x), (FOR x IN y270 RETURN x), (FOR x IN y271 RETURN x), (FOR x IN y272 RETURN x), (FOR x IN y273 RETURN x), (FOR x IN y274 RETURN x), (FOR x IN y275 RETURN x), (FOR x IN y276 RETURN x), (FOR x IN y277 RETURN x), (FOR x IN y278 RETURN x), (FOR x IN y279 RETURN x), (FOR x IN y280 RETURN x), (FOR x IN y281 RETURN x), (FOR x IN y282 RETURN x), (FOR x IN y283 RETURN x), (FOR x IN y284 RETURN x), (FOR x IN y285 RETURN x), (FOR x IN y286 RETURN x), (FOR x IN y287 RETURN x), (FOR x IN y288 RETURN x), (FOR x IN y289 RETURN x), (FOR x IN y290 RETURN x), (FOR x IN y291 RETURN x), (FOR x IN y292 RETURN x), (FOR x IN y293 RETURN x), (FOR x IN y294 RETURN x), (FOR x IN y295 RETURN x), (FOR x IN y296 RETURN x), (FOR x IN y297 RETURN x), (FOR x IN y298 RETURN x), (FOR x IN y299 RETURN x), (FOR x IN y300 RETURN x), (FOR x IN y301 RETURN x), (FOR x IN y302 RETURN x), (FOR x IN y303 RETURN x), (FOR x IN y304 RETURN x), (FOR x IN y305 RETURN x), (FOR x IN y306 RETURN x), (FOR x IN y307 RETURN x), (FOR x IN y308 RETURN x), (FOR x IN y309 RETURN x), (FOR x IN y310 RETURN x), (FOR x IN y311 RETURN x), (FOR x IN y312 RETURN x), (FOR x IN y313 RETURN x), (FOR x IN y314 RETURN x), (FOR x IN y315 RETURN x), (FOR x IN y316 RETURN x), (FOR x IN y317 RETURN x), (FOR x IN y318 RETURN x), (FOR x IN y319 RETURN x), (FOR x IN y320 RETURN x), (FOR x IN y321 RETURN x), (FOR x IN y322 RETURN x), (FOR x IN y323 RETURN x), (FOR x IN y324 RETURN x), (FOR x IN y325 RETURN x), (FOR x IN y326 RETURN x), (FOR x IN y327 RETURN x), (FOR x IN y328 RETURN x), (FOR x IN y329 RETURN x), (FOR x IN y330 RETURN x), (FOR x IN y331 RETURN x), (FOR x IN y332 RETURN x), (FOR x IN y333 RETURN x), (FOR x IN y334 RETURN x), (FOR x IN y335 RETURN x), (FOR x IN y336 RETURN x), (FOR x IN y337 RETURN x), (FOR x IN y338 RETURN x), (FOR x IN y339 RETURN x), (FOR x IN y340 RETURN x), (FOR x IN y341 RETURN x), (FOR x IN y342 RETURN x), (FOR x IN y343 RETURN x), (FOR x IN y344 RETURN x), (FOR x IN y345 RETURN x), (FOR x IN y346 RETURN x), (FOR x IN y347 RETURN x), (FOR x IN y348 RETURN x), (FOR x IN y349 RETURN x), (FOR x IN y350 RETURN x), (FOR x IN y351 RETURN x), (FOR x IN y352 RETURN x), (FOR x IN y353 RETURN x), (FOR x IN y354 RETURN x), (FOR x IN y355 RETURN x), (FOR x IN y356 RETURN x), (FOR x IN y357 RETURN x), (FOR x IN y358 RETURN x), (FOR x IN y359 RETURN x), (FOR x IN y360 RETURN x), (FOR x IN y361 RETURN x), (FOR x IN y362 RETURN x), (FOR x IN y363 RETURN x), (FOR x IN y364 RETURN x), (FOR x IN y365 RETURN x), (FOR x IN y366 RETURN x), (FOR x IN y367 RETURN x), (FOR x IN y368 RETURN x), (FOR x IN y369 RETURN x), (FOR x IN y370 RETURN x), (FOR x IN y371 RETURN x), (FOR x IN y372 RETURN x), (FOR x IN y373 RETURN x), (FOR x IN y374 RETURN x), (FOR x IN y375 RETURN x), (FOR x IN y376 RETURN x), (FOR x IN y377 RETURN x), (FOR x IN y378 RETURN x), (FOR x IN y379 RETURN x), (FOR x IN y380 RETURN x), (FOR x IN y381 RETURN x), (FOR x IN y382 RETURN x), (FOR x IN y383 RETURN x), (FOR x IN y384 RETURN x), (FOR x IN y385 RETURN x), (FOR x IN y386 RETURN x), (FOR x IN y387 RETURN x), (FOR x IN y388 RETURN x), (FOR x IN y389 RETURN x), (FOR x IN y390 RETURN x), (FOR x IN y391 RETURN x), (FOR x IN y392 RETURN x), (FOR x IN y393 RETURN x), (FOR x IN y394 RETURN x), (FOR x IN y395 RETURN x), (FOR x IN y396 RETURN x), (FOR x IN y397 RETURN x), (FOR x IN y398 RETURN x), (FOR x IN y399 RETURN x), (FOR x IN y400 RETURN x), (FOR x IN y401 RETURN x), (FOR x IN y402 RETURN x), (FOR x IN y403 RETURN x), (FOR x IN y404 RETURN x), (FOR x IN y405 RETURN x), (FOR x IN y406 RETURN x), (FOR x IN y407 RETURN x), (FOR x IN y408 RETURN x), (FOR x IN y409 RETURN x), (FOR x IN y410 RETURN x), (FOR x IN y411 RETURN x), (FOR x IN y412 RETURN x), (FOR x IN y413 RETURN x), (FOR x IN y414 RETURN x), (FOR x IN y415 RETURN x), (FOR x IN y416 RETURN x), (FOR x IN y417 RETURN x), (FOR x IN y418 RETURN x), (FOR x IN y419 RETURN x), (FOR x IN y420 RETURN x), (FOR x IN y421 RETURN x), (FOR x IN y422 RETURN x), (FOR x IN y423 RETURN x), (FOR x IN y424 RETURN x), (FOR x IN y425 RETURN x), (FOR x IN y426 RETURN x), (FOR x IN y427 RETURN x), (FOR x IN y428 RETURN x), (FOR x IN y429 RETURN x), (FOR x IN y430 RETURN x), (FOR x IN y431 RETURN x), (FOR x IN y432 RETURN x), (FOR x IN y433 RETURN x), (FOR x IN y434 RETURN x), (FOR x IN y435 RETURN x), (FOR x IN y436 RETURN x), (FOR x IN y437 RETURN x), (FOR x IN y438 RETURN x), (FOR x IN y439 RETURN x), (FOR x IN y440 RETURN x), (FOR x IN y441 RETURN x), (FOR x IN y442 RETURN x), (FOR x IN y443 RETURN x), (FOR x IN y444 RETURN x), (FOR x IN y445 RETURN x), (FOR x IN y446 RETURN x), (FOR x IN y447 RETURN x), (FOR x IN y448 RETURN x), (FOR x IN y449 RETURN x), (FOR x IN y450 RETURN x), (FOR x IN y451 RETURN x), (FOR x IN y452 RETURN x), (FOR x IN y453 RETURN x), (FOR x IN y454 RETURN x), (FOR x IN y455 RETURN x), (FOR x IN y456 RETURN x), (FOR x IN y457 RETURN x), (FOR x IN y458 RETURN x), (FOR x IN y459 RETURN x), (FOR x IN y460 RETURN x), (FOR x IN y461 RETURN x), (FOR x IN y462 RETURN x), (FOR x IN y463 RETURN x), (FOR x IN y464 RETURN x), (FOR x IN y465 RETURN x), (FOR x IN y466 RETURN x), (FOR x IN y467 RETURN x), (FOR x IN y468 RETURN x), (FOR x IN y469 RETURN x), (FOR x IN y470 RETURN x), (FOR x IN y471 RETURN x), (FOR x IN y472 RETURN x), (FOR x IN y473 RETURN x), (FOR x IN y474 RETURN x), (FOR x IN y475 RETURN x), (FOR x IN y476 RETURN x), (FOR x IN y477 RETURN x), (FOR x IN y478 RETURN x), (FOR x IN y479 RETURN x), (FOR x IN y480 RETURN x), (FOR x IN y481 RETURN x), (FOR x IN y482 RETURN x), (FOR x IN y483 RETURN x), (FOR x IN y484 RETURN x), (FOR x IN y485 RETURN x), (FOR x IN y486 RETURN x), (FOR x IN y487 RETURN x), (FOR x IN y488 RETURN x), (FOR x IN y489 RETURN x), (FOR x IN y490 RETURN x), (FOR x IN y491 RETURN x), (FOR x IN y492 RETURN x), (FOR x IN y493 RETURN x), (FOR x IN y494 RETURN x), (FOR x IN y495 RETURN x), (FOR x IN y496 RETURN x), (FOR x IN y497 RETURN x), (FOR x IN y498 RETURN x), (FOR x IN y499 RETURN x), (FOR x IN y500 RETURN x), (FOR x IN y501 RETURN x), (FOR x IN y502 RETURN x), (FOR x IN y503 RETURN x), (FOR x IN y504 RETURN x), (FOR x IN y505 RETURN x), (FOR x IN y506 RETURN x), (FOR x IN y507 RETURN x), (FOR x IN y508 RETURN x), (FOR x IN y509 RETURN x), (FOR x IN y510 RETURN x), (FOR x IN y511 RETURN x), (FOR x IN y512 RETURN x), (FOR x IN y513 RETURN x), (FOR x IN y514 RETURN x), (FOR x IN y515 RETURN x), (FOR x IN y516 RETURN x), (FOR x IN y517 RETURN x), (FOR x IN y518 RETURN x), (FOR x IN y519 RETURN x), (FOR x IN y520 RETURN x), (FOR x IN y521 RETURN x), (FOR x IN y522 RETURN x), (FOR x IN y523 RETURN x), (FOR x IN y524 RETURN x), (FOR x IN y525 RETURN x), (FOR x IN y526 RETURN x), (FOR x IN y527 RETURN x), (FOR x IN y528 RETURN x), (FOR x IN y529 RETURN x), (FOR x IN y530 RETURN x), (FOR x IN y531 RETURN x), (FOR x IN y532 RETURN x), (FOR x IN y533 RETURN x), (FOR x IN y534 RETURN x), (FOR x IN y535 RETURN x), (FOR x IN y536 RETURN x), (FOR x IN y537 RETURN x), (FOR x IN y538 RETURN x), (FOR x IN y539 RETURN x), (FOR x IN y540 RETURN x), (FOR x IN y541 RETURN x), (FOR x IN y542 RETURN x), (FOR x IN y543 RETURN x), (FOR x IN y544 RETURN x), (FOR x IN y545 RETURN x), (FOR x IN y546 RETURN x), (FOR x IN y547 RETURN x), (FOR x IN y548 RETURN x), (FOR x IN y549 RETURN x), (FOR x IN y550 RETURN x), (FOR x IN y551 RETURN x), (FOR x IN y552 RETURN x), (FOR x IN y553 RETURN x), (FOR x IN y554 RETURN x), (FOR x IN y555 RETURN x), (FOR x IN y556 RETURN x), (FOR x IN y557 RETURN x), (FOR x IN y558 RETURN x), (FOR x IN y559 RETURN x), (FOR x IN y560 RETURN x), (FOR x IN y561 RETURN x), (FOR x IN y562 RETURN x), (FOR x IN y563 RETURN x), (FOR x IN y564 RETURN x), (FOR x IN y565 RETURN x), (FOR x IN y566 RETURN x), (FOR x IN y567 RETURN x), (FOR x IN y568 RETURN x), (FOR x IN y569 RETURN x), (FOR x IN y570 RETURN x), (FOR x IN y571 RETURN x), (FOR x IN y572 RETURN x), (FOR x IN y573 RETURN x), (FOR x IN y574 RETURN x), (FOR x IN y575 RETURN x), (FOR x IN y576 RETURN x), (FOR x IN y577 RETURN x), (FOR x IN y578 RETURN x), (FOR x IN y579 RETURN x), (FOR x IN y580 RETURN x), (FOR x IN y581 RETURN x), (FOR x IN y582 RETURN x), (FOR x IN y583 RETURN x), (FOR x IN y584 RETURN x), (FOR x IN y585 RETURN x), (FOR x IN y586 RETURN x), (FOR x IN y587 RETURN x), (FOR x IN y588 RETURN x), (FOR x IN y589 RETURN x), (FOR x IN y590 RETURN x), (FOR x IN y591 RETURN x), (FOR x IN y592 RETURN x), (FOR x IN y593 RETURN x), (FOR x IN y594 RETURN x), (FOR x IN y595 RETURN x), (FOR x IN y596 RETURN x), (FOR x IN y597 RETURN x), (FOR x IN y598 RETURN x), (FOR x IN y599 RETURN x), (FOR x IN y600 RETURN x), (FOR x IN y601 RETURN x), (FOR x IN y602 RETURN x), (FOR x IN y603 RETURN x), (FOR x IN y604 RETURN x), (FOR x IN y605 RETURN x), (FOR x IN y606 RETURN x), (FOR x IN y607 RETURN x), (FOR x IN y608 RETURN x), (FOR x IN y609 RETURN x), (FOR x IN y610 RETURN x), (FOR x IN y611 RETURN x), (FOR x IN y612 RETURN x), (FOR x IN y613 RETURN x), (FOR x IN y614 RETURN x), (FOR x IN y615 RETURN x), (FOR x IN y616 RETURN x), (FOR x IN y617 RETURN x), (FOR x IN y618 RETURN x), (FOR x IN y619 RETURN x), (FOR x IN y620 RETURN x), (FOR x IN y621 RETURN x), (FOR x IN y622 RETURN x), (FOR x IN y623 RETURN x), (FOR x IN y624 RETURN x), (FOR x IN y625 RETURN x), (FOR x IN y626 RETURN x), (FOR x IN y627 RETURN x), (FOR x IN y628 RETURN x), (FOR x IN y629 RETURN x), (FOR x IN y630 RETURN x), (FOR x IN y631 RETURN x), (FOR x IN y632 RETURN x), (FOR x IN y633 RETURN x), (FOR x IN y634 RETURN x), (FOR x IN y635 RETURN x), (FOR x IN y636 RETURN x), (FOR x IN y637 RETURN x), (FOR x IN y638 RETURN x), (FOR x IN y639 RETURN x), (FOR x IN y640 RETURN x), (FOR x IN y641 RETURN x), (FOR x IN y642 RETURN x), (FOR x IN y643 RETURN x), (FOR x IN y644 RETURN x), (FOR x IN y645 RETURN x), (FOR x IN y646 RETURN x), (FOR x IN y647 RETURN x), (FOR x IN y648 RETURN x), (FOR x IN y649 RETURN x), (FOR x IN y650 RETURN x), (FOR x IN y651 RETURN x), (FOR x IN y652 RETURN x), (FOR x IN y653 RETURN x), (FOR x IN y654 RETURN x), (FOR x IN y655 RETURN x), (FOR x IN y656 RETURN x), (FOR x IN y657 RETURN x), (FOR x IN y658 RETURN x), (FOR x IN y659 RETURN x), (FOR x IN y660 RETURN x), (FOR x IN y661 RETURN x), (FOR x IN y662 RETURN x), (FOR x IN y663 RETURN x), (FOR x IN y664 RETURN x), (FOR x IN y665 RETURN x), (FOR x IN y666 RETURN x), (FOR x IN y667 RETURN x), (FOR x IN y668 RETURN x), (FOR x IN y669 RETURN x), (FOR x IN y670 RETURN x), (FOR x IN y671 RETURN x), (FOR x IN y672 RETURN x), (FOR x IN y673 RETURN x), (FOR x IN y674 RETURN x), (FOR x IN y675 RETURN x), (FOR x IN y676 RETURN x), (FOR x IN y677 RETURN x), (FOR x IN y678 RETURN x), (FOR x IN y679 RETURN x), (FOR x IN y680 RETURN x), (FOR x IN y681 RETURN x), (FOR x IN y682 RETURN x), (FOR x IN y683 RETURN x), (FOR x IN y684 RETURN x), (FOR x IN y685 RETURN x), (FOR x IN y686 RETURN x), (FOR x IN y687 RETURN x), (FOR x IN y688 RETURN x), (FOR x IN y689 RETURN x), (FOR x IN y690 RETURN x), (FOR x IN y691 RETURN x), (FOR x IN y692 RETURN x), (FOR x IN y693 RETURN x), (FOR x IN y694 RETURN x), (FOR x IN y695 RETURN x), (FOR x IN y696 RETURN x), (FOR x IN y697 RETURN x), (FOR x IN y698 RETURN x), (FOR x IN y699 RETURN x), (FOR x IN y700 RETURN x), (FOR x IN y701 RETURN x), (FOR x IN y702 RETURN x), (FOR x IN y703 RETURN x), (FOR x IN y704 RETURN x), (FOR x IN y705 RETURN x), (FOR x IN y706 RETURN x), (FOR x IN y707 RETURN x), (FOR x IN y708 RETURN x), (FOR x IN y709 RETURN x), (FOR x IN y710 RETURN x), (FOR x IN y711 RETURN x), (FOR x IN y712 RETURN x), (FOR x IN y713 RETURN x), (FOR x IN y714 RETURN x), (FOR x IN y715 RETURN x), (FOR x IN y716 RETURN x), (FOR x IN y717 RETURN x), (FOR x IN y718 RETURN x), (FOR x IN y719 RETURN x), (FOR x IN y720 RETURN x), (FOR x IN y721 RETURN x), (FOR x IN y722 RETURN x), (FOR x IN y723 RETURN x), (FOR x IN y724 RETURN x), (FOR x IN y725 RETURN x), (FOR x IN y726 RETURN x), (FOR x IN y727 RETURN x), (FOR x IN y728 RETURN x), (FOR x IN y729 RETURN x), (FOR x IN y730 RETURN x), (FOR x IN y731 RETURN x), (FOR x IN y732 RETURN x), (FOR x IN y733 RETURN x), (FOR x IN y734 RETURN x), (FOR x IN y735 RETURN x), (FOR x IN y736 RETURN x), (FOR x IN y737 RETURN x), (FOR x IN y738 RETURN x), (FOR x IN y739 RETURN x), (FOR x IN y740 RETURN x), (FOR x IN y741 RETURN x), (FOR x IN y742 RETURN x), (FOR x IN y743 RETURN x), (FOR x IN y744 RETURN x), (FOR x IN y745 RETURN x), (FOR x IN y746 RETURN x), (FOR x IN y747 RETURN x), (FOR x IN y748 RETURN x), (FOR x IN y749 RETURN x), (FOR x IN y750 RETURN x), (FOR x IN y751 RETURN x), (FOR x IN y752 RETURN x), (FOR x IN y753 RETURN x), (FOR x IN y754 RETURN x), (FOR x IN y755 RETURN x), (FOR x IN y756 RETURN x), (FOR x IN y757 RETURN x), (FOR x IN y758 RETURN x), (FOR x IN y759 RETURN x), (FOR x IN y760 RETURN x), (FOR x IN y761 RETURN x), (FOR x IN y762 RETURN x), (FOR x IN y763 RETURN x), (FOR x IN y764 RETURN x), (FOR x IN y765 RETURN x), (FOR x IN y766 RETURN x), (FOR x IN y767 RETURN x), (FOR x IN y768 RETURN x), (FOR x IN y769 RETURN x), (FOR x IN y770 RETURN x), (FOR x IN y771 RETURN x), (FOR x IN y772 RETURN x), (FOR x IN y773 RETURN x), (FOR x IN y774 RETURN x), (FOR x IN y775 RETURN x), (FOR x IN y776 RETURN x), (FOR x IN y777 RETURN x), (FOR x IN y778 RETURN x), (FOR x IN y779 RETURN x), (FOR x IN y780 RETURN x), (FOR x IN y781 RETURN x), (FOR x IN y782 RETURN x), (FOR x IN y783 RETURN x), (FOR x IN y784 RETURN x), (FOR x IN y785 RETURN x), (FOR x IN y786 RETURN x), (FOR x IN y787 RETURN x), (FOR x IN y788 RETURN x), (FOR x IN y789 RETURN x), (FOR x IN y790 RETURN x), (FOR x IN y791 RETURN x), (FOR x IN y792 RETURN x), (FOR x IN y793 RETURN x), (FOR x IN y794 RETURN x), (FOR x IN y795 RETURN x), (FOR x IN y796 RETURN x), (FOR x IN y797 RETURN x), (FOR x IN y798 RETURN x), (FOR x IN y799 RETURN x), (FOR x IN y800 RETURN x), (FOR x IN y801 RETURN x), (FOR x IN y802 RETURN x), (FOR x IN y803 RETURN x), (FOR x IN y804 RETURN x), (FOR x IN y805 RETURN x), (FOR x IN y806 RETURN x), (FOR x IN y807 RETURN x), (FOR x IN y808 RETURN x), (FOR x IN y809 RETURN x), (FOR x IN y810 RETURN x), (FOR x IN y811 RETURN x), (FOR x IN y812 RETURN x), (FOR x IN y813 RETURN x), (FOR x IN y814 RETURN x), (FOR x IN y815 RETURN x), (FOR x IN y816 RETURN x), (FOR x IN y817 RETURN x), (FOR x IN y818 RETURN x), (FOR x IN y819 RETURN x), (FOR x IN y820 RETURN x), (FOR x IN y821 RETURN x), (FOR x IN y822 RETURN x), (FOR x IN y823 RETURN x), (FOR x IN y824 RETURN x), (FOR x IN y825 RETURN x), (FOR x IN y826 RETURN x), (FOR x IN y827 RETURN x), (FOR x IN y828 RETURN x), (FOR x IN y829 RETURN x), (FOR x IN y830 RETURN x), (FOR x IN y831 RETURN x), (FOR x IN y832 RETURN x), (FOR x IN y833 RETURN x), (FOR x IN y834 RETURN x), (FOR x IN y835 RETURN x), (FOR x IN y836 RETURN x), (FOR x IN y837 RETURN x), (FOR x IN y838 RETURN x), (FOR x IN y839 RETURN x), (FOR x IN y840 RETURN x), (FOR x IN y841 RETURN x), (FOR x IN y842 RETURN x), (FOR x IN y843 RETURN x), (FOR x IN y844 RETURN x), (FOR x IN y845 RETURN x), (FOR x IN y846 RETURN x), (FOR x IN y847 RETURN x), (FOR x IN y848 RETURN x), (FOR x IN y849 RETURN x), (FOR x IN y850 RETURN x), (FOR x IN y851 RETURN x), (FOR x IN y852 RETURN x), (FOR x IN y853 RETURN x), (FOR x IN y854 RETURN x), (FOR x IN y855 RETURN x), (FOR x IN y856 RETURN x), (FOR x IN y857 RETURN x), (FOR x IN y858 RETURN x), (FOR x IN y859 RETURN x), (FOR x IN y860 RETURN x), (FOR x IN y861 RETURN x), (FOR x IN y862 RETURN x), (FOR x IN y863 RETURN x), (FOR x IN y864 RETURN x), (FOR x IN y865 RETURN x), (FOR x IN y866 RETURN x), (FOR x IN y867 RETURN x), (FOR x IN y868 RETURN x), (FOR x IN y869 RETURN x), (FOR x IN y870 RETURN x), (FOR x IN y871 RETURN x), (FOR x IN y872 RETURN x), (FOR x IN y873 RETURN x), (FOR x IN y874 RETURN x), (FOR x IN y875 RETURN x), (FOR x IN y876 RETURN x), (FOR x IN y877 RETURN x), (FOR x IN y878 RETURN x), (FOR x IN y879 RETURN x), (FOR x IN y880 RETURN x), (FOR x IN y881 RETURN x), (FOR x IN y882 RETURN x), (FOR x IN y883 RETURN x), (FOR x IN y884 RETURN x), (FOR x IN y885 RETURN x), (FOR x IN y886 RETURN x), (FOR x IN y887 RETURN x), (FOR x IN y888 RETURN x), (FOR x IN y889 RETURN x), (FOR x IN y890 RETURN x), (FOR x IN y891 RETURN x), (FOR x IN y892 RETURN x), (FOR x IN y893 RETURN x), (FOR x IN y894 RETURN x), (FOR x IN y895 RETURN x), (FOR x IN y896 RETURN x), (FOR x IN y897 RETURN x), (FOR x IN y898 RETURN x), (FOR x IN y899 RETURN x), (FOR x IN y900 RETURN x), (FOR x IN y901 RETURN x), (FOR x IN y902 RETURN x), (FOR x IN y903 RETURN x), (FOR x IN y904 RETURN x), (FOR x IN y905 RETURN x), (FOR x IN y906 RETURN x), (FOR x IN y907 RETURN x), (FOR x IN y908 RETURN x), (FOR x IN y909 RETURN x), (FOR x IN y910 RETURN x), (FOR x IN y911 RETURN x), (FOR x IN y912 RETURN x), (FOR x IN y913 RETURN x), (FOR x IN y914 RETURN x), (FOR x IN y915 RETURN x), (FOR x IN y916 RETURN x), (FOR x IN y917 RETURN x), (FOR x IN y918 RETURN x), (FOR x IN y919 RETURN x), (FOR x IN y920 RETURN x), (FOR x IN y921 RETURN x), (FOR x IN y922 RETURN x), (FOR x IN y923 RETURN x), (FOR x IN y924 RETURN x), (FOR x IN y925 RETURN x), (FOR x IN y926 RETURN x), (FOR x IN y927 RETURN x), (FOR x IN y928 RETURN x), (FOR x IN y929 RETURN x), (FOR x IN y930 RETURN x), (FOR x IN y931 RETURN x), (FOR x IN y932 RETURN x), (FOR x IN y933 RETURN x), (FOR x IN y934 RETURN x), (FOR x IN y935 RETURN x), (FOR x IN y936 RETURN x), (FOR x IN y937 RETURN x), (FOR x IN y938 RETURN x), (FOR x IN y939 RETURN x), (FOR x IN y940 RETURN x), (FOR x IN y941 RETURN x), (FOR x IN y942 RETURN x), (FOR x IN y943 RETURN x), (FOR x IN y944 RETURN x), (FOR x IN y945 RETURN x), (FOR x IN y946 RETURN x), (FOR x IN y947 RETURN x), (FOR x IN y948 RETURN x), (FOR x IN y949 RETURN x), (FOR x IN y950 RETURN x), (FOR x IN y951 RETURN x), (FOR x IN y952 RETURN x), (FOR x IN y953 RETURN x), (FOR x IN y954 RETURN x), (FOR x IN y955 RETURN x), (FOR x IN y956 RETURN x), (FOR x IN y957 RETURN x), (FOR x IN y958 RETURN x), (FOR x IN y959 RETURN x), (FOR x IN y960 RETURN x), (FOR x IN y961 RETURN x), (FOR x IN y962 RETURN x), (FOR x IN y963 RETURN x), (FOR x IN y964 RETURN x), (FOR x IN y965 RETURN x), (FOR x IN y966 RETURN x), (FOR x IN y967 RETURN x), (FOR x IN y968 RETURN x), (FOR x IN y969 RETURN x), (FOR x IN y970 RETURN x), (FOR x IN y971 RETURN x), (FOR x IN y972 RETURN x), (FOR x IN y973 RETURN x), (FOR x IN y974 RETURN x), (FOR x IN y975 RETURN x), (FOR x IN y976 RETURN x), (FOR x IN y977 RETURN x), (FOR x IN y978 RETURN x), (FOR x IN y979 RETURN x), (FOR x IN y980 RETURN x), (FOR x IN y981 RETURN x), (FOR x IN y982 RETURN x), (FOR x IN y983 RETURN x), (FOR x IN y984 RETURN x), (FOR x IN y985 RETURN x), (FOR x IN y986 RETURN x), (FOR x IN y987 RETURN x), (FOR x IN y988 RETURN x), (FOR x IN y989 RETURN x), (FOR x IN y990 RETURN x), (FOR x IN y991 RETURN x), (FOR x IN y992 RETURN x), (FOR x IN y993 RETURN x), (FOR x IN y994 RETURN x), (FOR x IN y995 RETURN x), (FOR x IN y996 RETURN x), (FOR x IN y997 RETURN x), (FOR x IN y998 RETURN x), (FOR x IN y999 RETURN x), (FOR x IN y1000 RETURN x), (FOR x IN y1001 RETURN x), (FOR x IN y1002 RETURN x), (FOR x IN y1003 RETURN x), (FOR x IN y1004 RETURN x), (FOR x IN y1005 RETURN x), (FOR x IN y1006 RETURN x), (FOR x IN y1007 RETURN x), (FOR x IN y1008 RETURN x), (FOR x IN y1009 RETURN x), (FOR x IN y1010 RETURN x), (FOR x IN y1011 RETURN x), (FOR x IN y1012 RETURN x), (FOR x IN y1013 RETURN x), (FOR x IN y1014 RETURN x), (FOR x IN y1015 RETURN x), (FOR x IN y1016 RETURN x), (FOR x IN y1017 RETURN x), (FOR x IN y1018 RETURN x), (FOR x IN y1019 RETURN x), (FOR x IN y1020 RETURN x), (FOR x IN y1021 RETURN x), (FOR x IN y1022 RETURN x), (FOR x IN y1023 RETURN x), (FOR x IN y1024 RETURN x), (FOR x IN y1025 RETURN x), (FOR x IN y1026 RETURN x), (FOR x IN y1027 RETURN x), (FOR x IN y1028 RETURN x), (FOR x IN y1029 RETURN x), (FOR x IN y1030 RETURN x), (FOR x IN y1031 RETURN x), (FOR x IN y1032 RETURN x), (FOR x IN y1033 RETURN x), (FOR x IN y1034 RETURN x), (FOR x IN y1035 RETURN x), (FOR x IN y1036 RETURN x), (FOR x IN y1037 RETURN x), (FOR x IN y1038 RETURN x), (FOR x IN y1039 RETURN x), (FOR x IN y1040 RETURN x), (FOR x IN y1041 RETURN x), (FOR x IN y1042 RETURN x), (FOR x IN y1043 RETURN x), (FOR x IN y1044 RETURN x), (FOR x IN y1045 RETURN x), (FOR x IN y1046 RETURN x), (FOR x IN y1047 RETURN x), (FOR x IN y1048 RETURN x), (FOR x IN y1049 RETURN x), (FOR x IN y1050 RETURN x), (FOR x IN y1051 RETURN x), (FOR x IN y1052 RETURN x), (FOR x IN y1053 RETURN x), (FOR x IN y1054 RETURN x), (FOR x IN y1055 RETURN x), (FOR x IN y1056 RETURN x), (FOR x IN y1057 RETURN x), (FOR x IN y1058 RETURN x), (FOR x IN y1059 RETURN x), (FOR x IN y1060 RETURN x), (FOR x IN y1061 RETURN x), (FOR x IN y1062 RETURN x), (FOR x IN y1063 RETURN x), (FOR x IN y1064 RETURN x), (FOR x IN y1065 RETURN x), (FOR x IN y1066 RETURN x), (FOR x IN y1067 RETURN x), (FOR x IN y1068 RETURN x), (FOR x IN y1069 RETURN x), (FOR x IN y1070 RETURN x), (FOR x IN y1071 RETURN x), (FOR x IN y1072 RETURN x), (FOR x IN y1073 RETURN x), (FOR x IN y1074 RETURN x), (FOR x IN y1075 RETURN x), (FOR x IN y1076 RETURN x), (FOR x IN y1077 RETURN x), (FOR x IN y1078 RETURN x), (FOR x IN y1079 RETURN x), (FOR x IN y1080 RETURN x), (FOR x IN y1081 RETURN x), (FOR x IN y1082 RETURN x), (FOR x IN y1083 RETURN x), (FOR x IN y1084 RETURN x), (FOR x IN y1085 RETURN x), (FOR x IN y1086 RETURN x), (FOR x IN y1087 RETURN x), (FOR x IN y1088 RETURN x), (FOR x IN y1089 RETURN x), (FOR x IN y1090 RETURN x), (FOR x IN y1091 RETURN x), (FOR x IN y1092 RETURN x), (FOR x IN y1093 RETURN x), (FOR x IN y1094 RETURN x), (FOR x IN y1095 RETURN x), (FOR x IN y1096 RETURN x), (FOR x IN y1097 RETURN x), (FOR x IN y1098 RETURN x), (FOR x IN y1099 RETURN x), (FOR x IN y1100 RETURN x), (FOR x IN y1101 RETURN x), (FOR x IN y1102 RETURN x), (FOR x IN y1103 RETURN x), (FOR x IN y1104 RETURN x), (FOR x IN y1105 RETURN x), (FOR x IN y1106 RETURN x), (FOR x IN y1107 RETURN x), (FOR x IN y1108 RETURN x), (FOR x IN y1109 RETURN x), (FOR x IN y1110 RETURN x), (FOR x IN y1111 RETURN x), (FOR x IN y1112 RETURN x), (FOR x IN y1113 RETURN x), (FOR x IN y1114 RETURN x), (FOR x IN y1115 RETURN x), (FOR x IN y1116 RETURN x), (FOR x IN y1117 RETURN x), (FOR x IN y1118 RETURN x), (FOR x IN y1119 RETURN x), (FOR x IN y1120 RETURN x), (FOR x IN y1121 RETURN x), (FOR x IN y1122 RETURN x), (FOR x IN y1123 RETURN x), (FOR x IN y1124 RETURN x), (FOR x IN y1125 RETURN x), (FOR x IN y1126 RETURN x), (FOR x IN y1127 RETURN x), (FOR x IN y1128 RETURN x), (FOR x IN y1129 RETURN x), (FOR x IN y1130 RETURN x), (FOR x IN y1131 RETURN x), (FOR x IN y1132 RETURN x), (FOR x IN y1133 RETURN x), (FOR x IN y1134 RETURN x), (FOR x IN y1135 RETURN x), (FOR x IN y1136 RETURN x), (FOR x IN y1137 RETURN x), (FOR x IN y1138 RETURN x), (FOR x IN y1139 RETURN x), (FOR x IN y1140 RETURN x), (FOR x IN y1141 RETURN x), (FOR x IN y1142 RETURN x), (FOR x IN y1143 RETURN x), (FOR x IN y1144 RETURN x), (FOR x IN y1145 RETURN x), (FOR x IN y1146 RETURN x), (FOR x IN y1147 RETURN x), (FOR x IN y1148 RETURN x), (FOR x IN y1149 RETURN x), (FOR x IN y1150 RETURN x), (FOR x IN y1151 RETURN x), (FOR x IN y1152 RETURN x), (FOR x IN y1153 RETURN x), (FOR x IN y1154 RETURN x), (FOR x IN y1155 RETURN x), (FOR x IN y1156 RETURN x), (FOR x IN y1157 RETURN x), (FOR x IN y1158 RETURN x), (FOR x IN y1159 RETURN x), (FOR x IN y1160 RETURN x), (FOR x IN y1161 RETURN x), (FOR x IN y1162 RETURN x), (FOR x IN y1163 RETURN x), (FOR x IN y1164 RETURN x), (FOR x IN y1165 RETURN x), (FOR x IN y1166 RETURN x), (FOR x IN y1167 RETURN x), (FOR x IN y1168 RETURN x), (FOR x IN y1169 RETURN x), (FOR x IN y1170 RETURN x), (FOR x IN y1171 RETURN x), (FOR x IN y1172 RETURN x), (FOR x IN y1173 RETURN x), (FOR x IN y1174 RETURN x), (FOR x IN y1175 RETURN x), (FOR x IN y1176 RETURN x), (FOR x IN y1177 RETURN x), (FOR x IN y1178 RETURN x), (FOR x IN y1179 RETURN x), (FOR x IN y1180 RETURN x), (FOR x IN y1181 RETURN x), (FOR x IN y1182 RETURN x), (FOR x IN y1183 RETURN x), (FOR x IN y1184 RETURN x), (FOR x IN y1185 RETURN x), (FOR x IN y1186 RETURN x), (FOR x IN y1187 RETURN x), (FOR x IN y1188 RETURN x), (FOR x IN y1189 RETURN x), (FOR x IN y1190 RETURN x), (FOR x IN y1191 RETURN x), (FOR x IN y1192 RETURN x), (FOR x IN y1193 RETURN x), (FOR x IN y1194 RETURN x), (FOR x IN y1195 RETURN x), (FOR x IN y1196 RETURN x), (FOR x IN y1197 RETURN x), (FOR x IN y1198 RETURN x), (FOR x IN y1199 RETURN x), (FOR x IN y1200 RETURN x), (FOR x IN y1201 RETURN x), (FOR x IN y1202 RETURN x), (FOR x IN y1203 RETURN x), (FOR x IN y1204 RETURN x), (FOR x IN y1205 RETURN x), (FOR x IN y1206 RETURN x), (FOR x IN y1207 RETURN x), (FOR x IN y1208 RETURN x), (FOR x IN y1209 RETURN x), (FOR x IN y1210 RETURN x), (FOR x IN y1211 RETURN x), (FOR x IN y1212 RETURN x), (FOR x IN y1213 RETURN x), (FOR x IN y1214 RETURN x), (FOR x IN y1215 RETURN x), (FOR x IN y1216 RETURN x), (FOR x IN y1217 RETURN x), (FOR x IN y1218 RETURN x), (FOR x IN y1219 RETURN x), (FOR x IN y1220 RETURN x), (FOR x IN y1221 RETURN x), (FOR x IN y1222 RETURN x), (FOR x IN y1223 RETURN x), (FOR x IN y1224 RETURN x), (FOR x IN y1225 RETURN x), (FOR x IN y1226 RETURN x), (FOR x IN y1227 RETURN x), (FOR x IN y1228 RETURN x), (FOR x IN y1229 RETURN x), (FOR x IN y1230 RETURN x), (FOR x IN y1231 RETURN x), (FOR x IN y1232 RETURN x), (FOR x IN y1233 RETURN x), (FOR x IN y1234 RETURN x), (FOR x IN y1235 RETURN x), (FOR x IN y1236 RETURN x), (FOR x IN y1237 RETURN x), (FOR x IN y1238 RETURN x), (FOR x IN y1239 RETURN x), (FOR x IN y1240 RETURN x), (FOR x IN y1241 RETURN x), (FOR x IN y1242 RETURN x), (FOR x IN y1243 RETURN x), (FOR x IN y1244 RETURN x), (FOR x IN y1245 RETURN x), (FOR x IN y1246 RETURN x), (FOR x IN y1247 RETURN x), (FOR x IN y1248 RETURN x), (FOR x IN y1249 RETURN x), (FOR x IN y1250 RETURN x), (FOR x IN y1251 RETURN x), (FOR x IN y1252 RETURN x), (FOR x IN y1253 RETURN x), (FOR x IN y1254 RETURN x), (FOR x IN y1255 RETURN x), (FOR x IN y1256 RETURN x), (FOR x IN y1257 RETURN x), (FOR x IN y1258 RETURN x), (FOR x IN y1259 RETURN x), (FOR x IN y1260 RETURN x), (FOR x IN y1261 RETURN x), (FOR x IN y1262 RETURN x), (FOR x IN y1263 RETURN x), (FOR x IN y1264 RETURN x), (FOR x IN y1265 RETURN x), (FOR x IN y1266 RETURN x), (FOR x IN y1267 RETURN x), (FOR x IN y1268 RETURN x), (FOR x IN y1269 RETURN x), (FOR x IN y1270 RETURN x), (FOR x IN y1271 RETURN x), (FOR x IN y1272 RETURN x), (FOR x IN y1273 RETURN x), (FOR x IN y1274 RETURN x), (FOR x IN y1275 RETURN x), (FOR x IN y1276 RETURN x), (FOR x IN y1277 RETURN x), (FOR x IN y1278 RETURN x), (FOR x IN y1279 RETURN x), (FOR x IN y1280 RETURN x), (FOR x IN y1281 RETURN x), (FOR x IN y1282 RETURN x), (FOR x IN y1283 RETURN x), (FOR x IN y1284 RETURN x), (FOR x IN y1285 RETURN x), (FOR x IN y1286 RETURN x), (FOR x IN y1287 RETURN x), (FOR x IN y1288 RETURN x), (FOR x IN y1289 RETURN x), (FOR x IN y1290 RETURN x), (FOR x IN y1291 RETURN x), (FOR x IN y1292 RETURN x), (FOR x IN y1293 RETURN x), (FOR x IN y1294 RETURN x), (FOR x IN y1295 RETURN x), (FOR x IN y1296 RETURN x), (FOR x IN y1297 RETURN x), (FOR x IN y1298 RETURN x), (FOR x IN y1299 RETURN x), (FOR x IN y1300 RETURN x), (FOR x IN y1301 RETURN x), (FOR x IN y1302 RETURN x), (FOR x IN y1303 RETURN x), (FOR x IN y1304 RETURN x), (FOR x IN y1305 RETURN x), (FOR x IN y1306 RETURN x), (FOR x IN y1307 RETURN x), (FOR x IN y1308 RETURN x), (FOR x IN y1309 RETURN x), (FOR x IN y1310 RETURN x), (FOR x IN y1311 RETURN x), (FOR x IN y1312 RETURN x), (FOR x IN y1313 RETURN x), (FOR x IN y1314 RETURN x), (FOR x IN y1315 RETURN x), (FOR x IN y1316 RETURN x), (FOR x IN y1317 RETURN x), (FOR x IN y1318 RETURN x), (FOR x IN y1319 RETURN x), (FOR x IN y1320 RETURN x), (FOR x IN y1321 RETURN x), (FOR x IN y1322 RETURN x), (FOR x IN y1323 RETURN x), (FOR x IN y1324 RETURN x), (FOR x IN y1325 RETURN x), (FOR x IN y1326 RETURN x), (FOR x IN y1327 RETURN x), (FOR x IN y1328 RETURN x), (FOR x IN y1329 RETURN x), (FOR x IN y1330 RETURN x), (FOR x IN y1331 RETURN x), (FOR x IN y1332 RETURN x), (FOR x IN y1333 RETURN x), (FOR x IN y1334 RETURN x), (FOR x IN y1335 RETURN x), (FOR x IN y1336 RETURN x), (FOR x IN y1337 RETURN x), (FOR x IN y1338 RETURN x), (FOR x IN y1339 RETURN x), (FOR x IN y1340 RETURN x), (FOR x IN y1341 RETURN x), (FOR x IN y1342 RETURN x), (FOR x IN y1343 RETURN x), (FOR x IN y1344 RETURN x), (FOR x IN y1345 RETURN x), (FOR x IN y1346 RETURN x), (FOR x IN y1347 RETURN x), (FOR x IN y1348 RETURN x), (FOR x IN y1349 RETURN x), (FOR x IN y1350 RETURN x), (FOR x IN y1351 RETURN x), (FOR x IN y1352 RETURN x), (FOR x IN y1353 RETURN x), (FOR x IN y1354 RETURN x), (FOR x IN y1355 RETURN x), (FOR x IN y1356 RETURN x), (FOR x IN y1357 RETURN x), (FOR x IN y1358 RETURN x), (FOR x IN y1359 RETURN x), (FOR x IN y1360 RETURN x), (FOR x IN y1361 RETURN x), (FOR x IN y1362 RETURN x), (FOR x IN y1363 RETURN x), (FOR x IN y1364 RETURN x), (FOR x IN y1365 RETURN x), (FOR x IN y1366 RETURN x), (FOR x IN y1367 RETURN x), (FOR x IN y1368 RETURN x), (FOR x IN y1369 RETURN x), (FOR x IN y1370 RETURN x), (FOR x IN y1371 RETURN x), (FOR x IN y1372 RETURN x), (FOR x IN y1373 RETURN x), (FOR x IN y1374 RETURN x), (FOR x IN y1375 RETURN x), (FOR x IN y1376 RETURN x), (FOR x IN y1377 RETURN x), (FOR x IN y1378 RETURN x), (FOR x IN y1379 RETURN x), (FOR x IN y1380 RETURN x), (FOR x IN y1381 RETURN x), (FOR x IN y1382 RETURN x), (FOR x IN y1383 RETURN x), (FOR x IN y1384 RETURN x), (FOR x IN y1385 RETURN x), (FOR x IN y1386 RETURN x), (FOR x IN y1387 RETURN x), (FOR x IN y1388 RETURN x), (FOR x IN y1389 RETURN x), (FOR x IN y1390 RETURN x), (FOR x IN y1391 RETURN x), (FOR x IN y1392 RETURN x), (FOR x IN y1393 RETURN x), (FOR x IN y1394 RETURN x), (FOR x IN y1395 RETURN x), (FOR x IN y1396 RETURN x), (FOR x IN y1397 RETURN x), (FOR x IN y1398 RETURN x), (FOR x IN y1399 RETURN x), (FOR x IN y1400 RETURN x), (FOR x IN y1401 RETURN x), (FOR x IN y1402 RETURN x), (FOR x IN y1403 RETURN x), (FOR x IN y1404 RETURN x), (FOR x IN y1405 RETURN x), (FOR x IN y1406 RETURN x), (FOR x IN y1407 RETURN x), (FOR x IN y1408 RETURN x), (FOR x IN y1409 RETURN x), (FOR x IN y1410 RETURN x), (FOR x IN y1411 RETURN x), (FOR x IN y1412 RETURN x), (FOR x IN y1413 RETURN x), (FOR x IN y1414 RETURN x), (FOR x IN y1415 RETURN x), (FOR x IN y1416 RETURN x), (FOR x IN y1417 RETURN x), (FOR x IN y1418 RETURN x), (FOR x IN y1419 RETURN x), (FOR x IN y1420 RETURN x), (FOR x IN y1421 RETURN x), (FOR x IN y1422 RETURN x), (FOR x IN y1423 RETURN x), (FOR x IN y1424 RETURN x), (FOR x IN y1425 RETURN x), (FOR x IN y1426 RETURN x), (FOR x IN y1427 RETURN x), (FOR x IN y1428 RETURN x), (FOR x IN y1429 RETURN x), (FOR x IN y1430 RETURN x), (FOR x IN y1431 RETURN x), (FOR x IN y1432 RETURN x), (FOR x IN y1433 RETURN x), (FOR x IN y1434 RETURN x), (FOR x IN y1435 RETURN x), (FOR x IN y1436 RETURN x), (FOR x IN y1437 RETURN x), (FOR x IN y1438 RETURN x), (FOR x IN y1439 RETURN x), (FOR x IN y1440 RETURN x), (FOR x IN y1441 RETURN x), (FOR x IN y1442 RETURN x), (FOR x IN y1443 RETURN x), (FOR x IN y1444 RETURN x), (FOR x IN y1445 RETURN x), (FOR x IN y1446 RETURN x), (FOR x IN y1447 RETURN x), (FOR x IN y1448 RETURN x), (FOR x IN y1449 RETURN x), (FOR x IN y1450 RETURN x), (FOR x IN y1451 RETURN x), (FOR x IN y1452 RETURN x), (FOR x IN y1453 RETURN x), (FOR x IN y1454 RETURN x), (FOR x IN y1455 RETURN x), (FOR x IN y1456 RETURN x), (FOR x IN y1457 RETURN x), (FOR x IN y1458 RETURN x), (FOR x IN y1459 RETURN x), (FOR x IN y1460 RETURN x), (FOR x IN y1461 RETURN x), (FOR x IN y1462 RETURN x), (FOR x IN y1463 RETURN x), (FOR x IN y1464 RETURN x), (FOR x IN y1465 RETURN x), (FOR x IN y1466 RETURN x), (FOR x IN y1467 RETURN x), (FOR x IN y1468 RETURN x), (FOR x IN y1469 RETURN x), (FOR x IN y1470 RETURN x), (FOR x IN y1471 RETURN x), (FOR x IN y1472 RETURN x), (FOR x IN y1473 RETURN x), (FOR x IN y1474 RETURN x), (FOR x IN y1475 RETURN x), (FOR x IN y1476 RETURN x), (FOR x IN y1477 RETURN x), (FOR x IN y1478 RETURN x), (FOR x IN y1479 RETURN x), (FOR x IN y1480 RETURN x), (FOR x IN y1481 RETURN x), (FOR x IN y1482 RETURN x), (FOR x IN y1483 RETURN x), (FOR x IN y1484 RETURN x), (FOR x IN y1485 RETURN x), (FOR x IN y1486 RETURN x), (FOR x IN y1487 RETURN x), (FOR x IN y1488 RETURN x), (FOR x IN y1489 RETURN x), (FOR x IN y1490 RETURN x), (FOR x IN y1491 RETURN x), (FOR x IN y1492 RETURN x), (FOR x IN y1493 RETURN x), (FOR x IN y1494 RETURN x), (FOR x IN y1495 RETURN x), (FOR x IN y1496 RETURN x), (FOR x IN y1497 RETURN x), (FOR x IN y1498 RETURN x), (FOR x IN y1499 RETURN x), (FOR x IN y1500 RETURN x), (FOR x IN y1501 RETURN x), (FOR x IN y1502 RETURN x), (FOR x IN y1503 RETURN x), (FOR x IN y1504 RETURN x), (FOR x IN y1505 RETURN x), (FOR x IN y1506 RETURN x), (FOR x IN y1507 RETURN x), (FOR x IN y1508 RETURN x), (FOR x IN y1509 RETURN x), (FOR x IN y1510 RETURN x), (FOR x IN y1511 RETURN x), (FOR x IN y1512 RETURN x), (FOR x IN y1513 RETURN x), (FOR x IN y1514 RETURN x), (FOR x IN y1515 RETURN x), (FOR x IN y1516 RETURN x), (FOR x IN y1517 RETURN x), (FOR x IN y1518 RETURN x), (FOR x IN y1519 RETURN x), (FOR x IN y1520 RETURN x), (FOR x IN y1521 RETURN x), (FOR x IN y1522 RETURN x), (FOR x IN y1523 RETURN x), (FOR x IN y1524 RETURN x), (FOR x IN y1525 RETURN x), (FOR x IN y1526 RETURN x), (FOR x IN y1527 RETURN x), (FOR x IN y1528 RETURN x), (FOR x IN y1529 RETURN x), (FOR x IN y1530 RETURN x), (FOR x IN y1531 RETURN x), (FOR x IN y1532 RETURN x), (FOR x IN y1533 RETURN x), (FOR x IN y1534 RETURN x), (FOR x IN y1535 RETURN x), (FOR x IN y1536 RETURN x), (FOR x IN y1537 RETURN x), (FOR x IN y1538 RETURN x), (FOR x IN y1539 RETURN x), (FOR x IN y1540 RETURN x), (FOR x IN y1541 RETURN x), (FOR x IN y1542 RETURN x), (FOR x IN y1543 RETURN x), (FOR x IN y1544 RETURN x), (FOR x IN y1545 RETURN x), (FOR x IN y1546 RETURN x), (FOR x IN y1547 RETURN x), (FOR x IN y1548 RETURN x), (FOR x IN y1549 RETURN x), (FOR x IN y1550 RETURN x), (FOR x IN y1551 RETURN x), (FOR x IN y1552 RETURN x), (FOR x IN y1553 RETURN x), (FOR x IN y1554 RETURN x), (FOR x IN y1555 RETURN x), (FOR x IN y1556 RETURN x), (FOR x IN y1557 RETURN x), (FOR x IN y1558 RETURN x), (FOR x IN y1559 RETURN x), (FOR x IN y1560 RETURN x), (FOR x IN y1561 RETURN x), (FOR x IN y1562 RETURN x), (FOR x IN y1563 RETURN x), (FOR x IN y1564 RETURN x), (FOR x IN y1565 RETURN x), (FOR x IN y1566 RETURN x), (FOR x IN y1567 RETURN x), (FOR x IN y1568 RETURN x), (FOR x IN y1569 RETURN x), (FOR x IN y1570 RETURN x), (FOR x IN y1571 RETURN x), (FOR x IN y1572 RETURN x), (FOR x IN y1573 RETURN x), (FOR x IN y1574 RETURN x), (FOR x IN y1575 RETURN x), (FOR x IN y1576 RETURN x), (FOR x IN y1577 RETURN x), (FOR x IN y1578 RETURN x), (FOR x IN y1579 RETURN x), (FOR x IN y1580 RETURN x), (FOR x IN y1581 RETURN x), (FOR x IN y1582 RETURN x), (FOR x IN y1583 RETURN x), (FOR x IN y1584 RETURN x), (FOR x IN y1585 RETURN x), (FOR x IN y1586 RETURN x), (FOR x IN y1587 RETURN x), (FOR x IN y1588 RETURN x), (FOR x IN y1589 RETURN x), (FOR x IN y1590 RETURN x), (FOR x IN y1591 RETURN x), (FOR x IN y1592 RETURN x), (FOR x IN y1593 RETURN x), (FOR x IN y1594 RETURN x), (FOR x IN y1595 RETURN x), (FOR x IN y1596 RETURN x), (FOR x IN y1597 RETURN x), (FOR x IN y1598 RETURN x), (FOR x IN y1599 RETURN x), (FOR x IN y1600 RETURN x), (FOR x IN y1601 RETURN x), (FOR x IN y1602 RETURN x), (FOR x IN y1603 RETURN x), (FOR x IN y1604 RETURN x), (FOR x IN y1605 RETURN x), (FOR x IN y1606 RETURN x), (FOR x IN y1607 RETURN x), (FOR x IN y1608 RETURN x), (FOR x IN y1609 RETURN x), (FOR x IN y1610 RETURN x), (FOR x IN y1611 RETURN x), (FOR x IN y1612 RETURN x), (FOR x IN y1613 RETURN x), (FOR x IN y1614 RETURN x), (FOR x IN y1615 RETURN x), (FOR x IN y1616 RETURN x), (FOR x IN y1617 RETURN x), (FOR x IN y1618 RETURN x), (FOR x IN y1619 RETURN x), (FOR x IN y1620 RETURN x), (FOR x IN y1621 RETURN x), (FOR x IN y1622 RETURN x), (FOR x IN y1623 RETURN x), (FOR x IN y1624 RETURN x), (FOR x IN y1625 RETURN x), (FOR x IN y1626 RETURN x), (FOR x IN y1627 RETURN x), (FOR x IN y1628 RETURN x), (FOR x IN y1629 RETURN x), (FOR x IN y1630 RETURN x), (FOR x IN y1631 RETURN x), (FOR x IN y1632 RETURN x), (FOR x IN y1633 RETURN x), (FOR x IN y1634 RETURN x), (FOR x IN y1635 RETURN x), (FOR x IN y1636 RETURN x), (FOR x IN y1637 RETURN x), (FOR x IN y1638 RETURN x), (FOR x IN y1639 RETURN x), (FOR x IN y1640 RETURN x), (FOR x IN y1641 RETURN x), (FOR x IN y1642 RETURN x), (FOR x IN y1643 RETURN x), (FOR x IN y1644 RETURN x), (FOR x IN y1645 RETURN x), (FOR x IN y1646 RETURN x), (FOR x IN y1647 RETURN x), (FOR x IN y1648 RETURN x), (FOR x IN y1649 RETURN x), (FOR x IN y1650 RETURN x), (FOR x IN y1651 RETURN x), (FOR x IN y1652 RETURN x), (FOR x IN y1653 RETURN x), (FOR x IN y1654 RETURN x), (FOR x IN y1655 RETURN x), (FOR x IN y1656 RETURN x), (FOR x IN y1657 RETURN x), (FOR x IN y1658 RETURN x), (FOR x IN y1659 RETURN x), (FOR x IN y1660 RETURN x), (FOR x IN y1661 RETURN x), (FOR x IN y1662 RETURN x), (FOR x IN y1663 RETURN x), (FOR x IN y1664 RETURN x), (FOR x IN y1665 RETURN x), (FOR x IN y1666 RETURN x), (FOR x IN y1667 RETURN x), (FOR x IN y1668 RETURN x), (FOR x IN y1669 RETURN x), (FOR x IN y1670 RETURN x), (FOR x IN y1671 RETURN x), (FOR x IN y1672 RETURN x), (FOR x IN y1673 RETURN x), (FOR x IN y1674 RETURN x), (FOR x IN y1675 RETURN x), (FOR x IN y1676 RETURN x), (FOR x IN y1677 RETURN x), (FOR x IN y1678 RETURN x), (FOR x IN y1679 RETURN x), (FOR x IN y1680 RETURN x), (FOR x IN y1681 RETURN x), (FOR x IN y1682 RETURN x), (FOR x IN y1683 RETURN x), (FOR x IN y1684 RETURN x), (FOR x IN y1685 RETURN x), (FOR x IN y1686 RETURN x), (FOR x IN y1687 RETURN x), (FOR x IN y1688 RETURN x), (FOR x IN y1689 RETURN x), (FOR x IN y1690 RETURN x), (FOR x IN y1691 RETURN x), (FOR x IN y1692 RETURN x), (FOR x IN y1693 RETURN x), (FOR x IN y1694 RETURN x), (FOR x IN y1695 RETURN x), (FOR x IN y1696 RETURN x), (FOR x IN y1697 RETURN x), (FOR x IN y1698 RETURN x), (FOR x IN y1699 RETURN x), (FOR x IN y1700 RETURN x), (FOR x IN y1701 RETURN x), (FOR x IN y1702 RETURN x), (FOR x IN y1703 RETURN x), (FOR x IN y1704 RETURN x), (FOR x IN y1705 RETURN x), (FOR x IN y1706 RETURN x), (FOR x IN y1707 RETURN x), (FOR x IN y1708 RETURN x), (FOR x IN y1709 RETURN x), (FOR x IN y1710 RETURN x), (FOR x IN y1711 RETURN x), (FOR x IN y1712 RETURN x), (FOR x IN y1713 RETURN x), (FOR x IN y1714 RETURN x), (FOR x IN y1715 RETURN x), (FOR x IN y1716 RETURN x), (FOR x IN y1717 RETURN x), (FOR x IN y1718 RETURN x), (FOR x IN y1719 RETURN x), (FOR x IN y1720 RETURN x), (FOR x IN y1721 RETURN x), (FOR x IN y1722 RETURN x), (FOR x IN y1723 RETURN x), (FOR x IN y1724 RETURN x), (FOR x IN y1725 RETURN x), (FOR x IN y1726 RETURN x), (FOR x IN y1727 RETURN x), (FOR x IN y1728 RETURN x), (FOR x IN y1729 RETURN x), (FOR x IN y1730 RETURN x), (FOR x IN y1731 RETURN x), (FOR x IN y1732 RETURN x), (FOR x IN y1733 RETURN x), (FOR x IN y1734 RETURN x), (FOR x IN y1735 RETURN x), (FOR x IN y1736 RETURN x), (FOR x IN y1737 RETURN x), (FOR x IN y1738 RETURN x), (FOR x IN y1739 RETURN x), (FOR x IN y1740 RETURN x), (FOR x IN y1741 RETURN x), (FOR x IN y1742 RETURN x), (FOR x IN y1743 RETURN x), (FOR x IN y1744 RETURN x), (FOR x IN y1745 RETURN x), (FOR x IN y1746 RETURN x), (FOR x IN y1747 RETURN x), (FOR x IN y1748 RETURN x), (FOR x IN y1749 RETURN x), (FOR x IN y1750 RETURN x), (FOR x IN y1751 RETURN x), (FOR x IN y1752 RETURN x), (FOR x IN y1753 RETURN x), (FOR x IN y1754 RETURN x), (FOR x IN y1755 RETURN x), (FOR x IN y1756 RETURN x), (FOR x IN y1757 RETURN x), (FOR x IN y1758 RETURN x), (FOR x IN y1759 RETURN x), (FOR x IN y1760 RETURN x), (FOR x IN y1761 RETURN x), (FOR x IN y1762 RETURN x), (FOR x IN y1763 RETURN x), (FOR x IN y1764 RETURN x), (FOR x IN y1765 RETURN x), (FOR x IN y1766 RETURN x), (FOR x IN y1767 RETURN x), (FOR x IN y1768 RETURN x), (FOR x IN y1769 RETURN x), (FOR x IN y1770 RETURN x), (FOR x IN y1771 RETURN x), (FOR x IN y1772 RETURN x), (FOR x IN y1773 RETURN x), (FOR x IN y1774 RETURN x), (FOR x IN y1775 RETURN x), (FOR x IN y1776 RETURN x), (FOR x IN y1777 RETURN x), (FOR x IN y1778 RETURN x), (FOR x IN y1779 RETURN x), (FOR x IN y1780 RETURN x), (FOR x IN y1781 RETURN x), (FOR x IN y1782 RETURN x), (FOR x IN y1783 RETURN x), (FOR x IN y1784 RETURN x), (FOR x IN y1785 RETURN x), (FOR x IN y1786 RETURN x), (FOR x IN y1787 RETURN x), (FOR x IN y1788 RETURN x), (FOR x IN y1789 RETURN x), (FOR x IN y1790 RETURN x), (FOR x IN y1791 RETURN x), (FOR x IN y1792 RETURN x), (FOR x IN y1793 RETURN x), (FOR x IN y1794 RETURN x), (FOR x IN y1795 RETURN x), (FOR x IN y1796 RETURN x), (FOR x IN y1797 RETURN x), (FOR x IN y1798 RETURN x), (FOR x IN y1799 RETURN x), (FOR x IN y1800 RETURN x), (FOR x IN y1801 RETURN x), (FOR x IN y1802 RETURN x), (FOR x IN y1803 RETURN x), (FOR x IN y1804 RETURN x), (FOR x IN y1805 RETURN x), (FOR x IN y1806 RETURN x), (FOR x IN y1807 RETURN x), (FOR x IN y1808 RETURN x), (FOR x IN y1809 RETURN x), (FOR x IN y1810 RETURN x), (FOR x IN y1811 RETURN x), (FOR x IN y1812 RETURN x), (FOR x IN y1813 RETURN x), (FOR x IN y1814 RETURN x), (FOR x IN y1815 RETURN x), (FOR x IN y1816 RETURN x), (FOR x IN y1817 RETURN x), (FOR x IN y1818 RETURN x), (FOR x IN y1819 RETURN x), (FOR x IN y1820 RETURN x), (FOR x IN y1821 RETURN x), (FOR x IN y1822 RETURN x), (FOR x IN y1823 RETURN x), (FOR x IN y1824 RETURN x), (FOR x IN y1825 RETURN x), (FOR x IN y1826 RETURN x), (FOR x IN y1827 RETURN x), (FOR x IN y1828 RETURN x), (FOR x IN y1829 RETURN x), (FOR x IN y1830 RETURN x), (FOR x IN y1831 RETURN x), (FOR x IN y1832 RETURN x), (FOR x IN y1833 RETURN x), (FOR x IN y1834 RETURN x), (FOR x IN y1835 RETURN x), (FOR x IN y1836 RETURN x), (FOR x IN y1837 RETURN x), (FOR x IN y1838 RETURN x), (FOR x IN y1839 RETURN x), (FOR x IN y1840 RETURN x), (FOR x IN y1841 RETURN x), (FOR x IN y1842 RETURN x), (FOR x IN y1843 RETURN x), (FOR x IN y1844 RETURN x), (FOR x IN y1845 RETURN x), (FOR x IN y1846 RETURN x), (FOR x IN y1847 RETURN x), (FOR x IN y1848 RETURN x), (FOR x IN y1849 RETURN x), (FOR x IN y1850 RETURN x), (FOR x IN y1851 RETURN x), (FOR x IN y1852 RETURN x), (FOR x IN y1853 RETURN x), (FOR x IN y1854 RETURN x), (FOR x IN y1855 RETURN x), (FOR x IN y1856 RETURN x), (FOR x IN y1857 RETURN x), (FOR x IN y1858 RETURN x), (FOR x IN y1859 RETURN x), (FOR x IN y1860 RETURN x), (FOR x IN y1861 RETURN x), (FOR x IN y1862 RETURN x), (FOR x IN y1863 RETURN x), (FOR x IN y1864 RETURN x), (FOR x IN y1865 RETURN x), (FOR x IN y1866 RETURN x), (FOR x IN y1867 RETURN x), (FOR x IN y1868 RETURN x), (FOR x IN y1869 RETURN x), (FOR x IN y1870 RETURN x), (FOR x IN y1871 RETURN x), (FOR x IN y1872 RETURN x), (FOR x IN y1873 RETURN x), (FOR x IN y1874 RETURN x), (FOR x IN y1875 RETURN x), (FOR x IN y1876 RETURN x), (FOR x IN y1877 RETURN x), (FOR x IN y1878 RETURN x), (FOR x IN y1879 RETURN x), (FOR x IN y1880 RETURN x), (FOR x IN y1881 RETURN x), (FOR x IN y1882 RETURN x), (FOR x IN y1883 RETURN x), (FOR x IN y1884 RETURN x), (FOR x IN y1885 RETURN x), (FOR x IN y1886 RETURN x), (FOR x IN y1887 RETURN x), (FOR x IN y1888 RETURN x), (FOR x IN y1889 RETURN x), (FOR x IN y1890 RETURN x), (FOR x IN y1891 RETURN x), (FOR x IN y1892 RETURN x), (FOR x IN y1893 RETURN x), (FOR x IN y1894 RETURN x), (FOR x IN y1895 RETURN x), (FOR x IN y1896 RETURN x), (FOR x IN y1897 RETURN x), (FOR x IN y1898 RETURN x), (FOR x IN y1899 RETURN x), (FOR x IN y1900 RETURN x), (FOR x IN y1901 RETURN x), (FOR x IN y1902 RETURN x), (FOR x IN y1903 RETURN x), (FOR x IN y1904 RETURN x), (FOR x IN y1905 RETURN x), (FOR x IN y1906 RETURN x), (FOR x IN y1907 RETURN x), (FOR x IN y1908 RETURN x), (FOR x IN y1909 RETURN x), (FOR x IN y1910 RETURN x), (FOR x IN y1911 RETURN x), (FOR x IN y1912 RETURN x), (FOR x IN y1913 RETURN x), (FOR x IN y1914 RETURN x), (FOR x IN y1915 RETURN x), (FOR x IN y1916 RETURN x), (FOR x IN y1917 RETURN x), (FOR x IN y1918 RETURN x), (FOR x IN y1919 RETURN x), (FOR x IN y1920 RETURN x), (FOR x IN y1921 RETURN x), (FOR x IN y1922 RETURN x), (FOR x IN y1923 RETURN x), (FOR x IN y1924 RETURN x), (FOR x IN y1925 RETURN x), (FOR x IN y1926 RETURN x), (FOR x IN y1927 RETURN x), (FOR x IN y1928 RETURN x), (FOR x IN y1929 RETURN x), (FOR x IN y1930 RETURN x), (FOR x IN y1931 RETURN x), (FOR x IN y1932 RETURN x), (FOR x IN y1933 RETURN x), (FOR x IN y1934 RETURN x), (FOR x IN y1935 RETURN x), (FOR x IN y1936 RETURN x), (FOR x IN y1937 RETURN x), (FOR x IN y1938 RETURN x), (FOR x IN y1939 RETURN x), (FOR x IN y1940 RETURN x), (FOR x IN y1941 RETURN x), (FOR x IN y1942 RETURN x), (FOR x IN y1943 RETURN x), (FOR x IN y1944 RETURN x), (FOR x IN y1945 RETURN x), (FOR x IN y1946 RETURN x), (FOR x IN y1947 RETURN x), (FOR x IN y1948 RETURN x), (FOR x IN y1949 RETURN x), (FOR x IN y1950 RETURN x), (FOR x IN y1951 RETURN x), (FOR x IN y1952 RETURN x), (FOR x IN y1953 RETURN x), (FOR x IN y1954 RETURN x), (FOR x IN y1955 RETURN x), (FOR x IN y1956 RETURN x), (FOR x IN y1957 RETURN x), (FOR x IN y1958 RETURN x), (FOR x IN y1959 RETURN x), (FOR x IN y1960 RETURN x), (FOR x IN y1961 RETURN x), (FOR x IN y1962 RETURN x), (FOR x IN y1963 RETURN x), (FOR x IN y1964 RETURN x), (FOR x IN y1965 RETURN x), (FOR x IN y1966 RETURN x), (FOR x IN y1967 RETURN x), (FOR x IN y1968 RETURN x), (FOR x IN y1969 RETURN x), (FOR x IN y1970 RETURN x), (FOR x IN y1971 RETURN x), (FOR x IN y1972 RETURN x), (FOR x IN y1973 RETURN x), (FOR x IN y1974 RETURN x), (FOR x IN y1975 RETURN x), (FOR x IN y1976 RETURN x), (FOR x IN y1977 RETURN x), (FOR x IN y1978 RETURN x), (FOR x IN y1979 RETURN x), (FOR x IN y1980 RETURN x), (FOR x IN y1981 RETURN x), (FOR x IN y1982 RETURN x), (FOR x IN y1983 RETURN x), (FOR x IN y1984 RETURN x), (FOR x IN y1985 RETURN x), (FOR x IN y1986 RETURN x), (FOR x IN y1987 RETURN x), (FOR x IN y1988 RETURN x), (FOR x IN y1989 RETURN x), (FOR x IN y1990 RETURN x), (FOR x IN y1991 RETURN x), (FOR x IN y1992 RETURN x), (FOR x IN y1993 RETURN x), (FOR x IN y1994 RETURN x), (FOR x IN y1995 RETURN x), (FOR x IN y1996 RETURN x), (FOR x IN y1997 RETURN x), (FOR x IN y1998 RETURN x), (FOR x IN y1999 RETURN x), (FOR x IN y2000 RETURN x), (FOR x IN y2001 RETURN x), (FOR x IN y2002 RETURN x), (FOR x IN y2003 RETURN x), (FOR x IN y2004 RETURN x), (FOR x IN y2005 RETURN x), (FOR x IN y2006 RETURN x), (FOR x IN y2007 RETURN x), (FOR x IN y2008 RETURN x), (FOR x IN y2009 RETURN x), (FOR x IN y2010 RETURN x), (FOR x IN y2011 RETURN x), (FOR x IN y2012 RETURN x), (FOR x IN y2013 RETURN x), (FOR x IN y2014 RETURN x), (FOR x IN y2015 RETURN x), (FOR x IN y2016 RETURN x), (FOR x IN y2017 RETURN x), (FOR x IN y2018 RETURN x), (FOR x IN y2019 RETURN x), (FOR x IN y2020 RETURN x), (FOR x IN y2021 RETURN x), (FOR x IN y2022 RETURN x), (FOR x IN y2023 RETURN x), (FOR x IN y2024 RETURN x), (FOR x IN y2025 RETURN x), (FOR x IN y2026 RETURN x), (FOR x IN y2027 RETURN x), (FOR x IN y2028 RETURN x), (FOR x IN y2029 RETURN x), (FOR x IN y2030 RETURN x), (FOR x IN y2031 RETURN x), (FOR x IN y2032 RETURN x), (FOR x IN y2033 RETURN x), (FOR x IN y2034 RETURN x), (FOR x IN y2035 RETURN x), (FOR x IN y2036 RETURN x), (FOR x IN y2037 RETURN x), (FOR x IN y2038 RETURN x), (FOR x IN y2039 RETURN x), (FOR x IN y2040 RETURN x), (FOR x IN y2041 RETURN x), (FOR x IN y2042 RETURN x), (FOR x IN y2043 RETURN x), (FOR x IN y2044 RETURN x), (FOR x IN y2045 RETURN x), (FOR x IN y2046 RETURN x), (FOR x IN y2047 RETURN x), (FOR x IN y2048 RETURN x))");
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
        } catch (err) {
          assertEqual(errors.ERROR_QUERY_PARSE.code, err.errorNum);
          assertMatch(/at position \d+:\d+/, err.errorMessage);
          var matches = err.errorMessage.match(/at position (\d+):(\d+)/);
          var line = parseInt(matches[1], 10);
          var column = parseInt(matches[2], 10);
          assertEqual(query[1][0], line);
          assertEqual(query[1][1], column);
        }
      });
    },
    
    testTrailingCommas : function() {
      let queries = [
        [ "RETURN [ ]", [ ] ],
        [ "RETURN [ 1, 2, 3 ]", [ 1, 2, 3 ] ],
        [ "RETURN [ 1, 2, 3, ]", [ 1, 2, 3 ] ],
        [ "RETURN [ 1, ]", [ 1 ] ],
        [ "RETURN [ 1, 2, ]", [ 1, 2 ] ],
        
        [ "RETURN { }", { } ],
        [ "RETURN { a: 1, b: 2, c: 3 }", { a: 1, b: 2, c :3 } ],
        [ "RETURN { a: 1, b: 2, c: 3, }", { a: 1, b: 2, c: 3 } ],
        [ "RETURN { a: 1, }", { a: 1 } ],
        [ "RETURN { a: 1, b: 2, }", { a: 1, b: 2 } ],
      ];

      queries.forEach(function(query) {
        let results = AQL_EXECUTE(query[0]).json;
        assertEqual(query[1], results[0]);
      });
    },
    
    testTrailingCommasInvalid : function() {
      let queries = [
        "RETURN [ , ]",
        "RETURN [ , 1 ]",
        "RETURN [ , 1, ]",
        "RETURN [ 1, , ]",
        "RETURN [ 1, , 2 ]",
        "RETURN [ 1, , 2, ]",
        "RETURN [ , 2, 3 ]",
        "RETURN [ 1, , 2 ]",

        "RETURN { , }",
        "RETURN { , a: 1 }",
        "RETURN { , a: 1, }",
        "RETURN { a: 1, , }",
        "RETURN { a: 1, , b: 2 }",
        "RETURN { a: 1, , b: 2, }",
        "RETURN { , a: 2, b: 3 }",
        "RETURN { a: 1, , b. 2 }",
      ];

      queries.forEach(function(query) {
        try {
          AQL_EXECUTE(query);
          fail();
        } catch (err) {
          assertEqual(errors.ERROR_QUERY_PARSE.code, err.errorNum);
        }
      });
    },

    testPrecedenceOfNotIn : function() {
      let result = AQL_PARSE("RETURN 3..4 NOT IN 1..2").ast;

      assertEqual("root", result[0].type);
      result = result[0].subNodes;

      assertEqual("return", result[0].type);
      result = result[0].subNodes;

      assertEqual("compare not in", result[0].type);
      let sub = result[0].subNodes;
      assertEqual("range", sub[0].type);
      assertEqual("value", sub[0].subNodes[0].type);
      assertEqual(3, sub[0].subNodes[0].value);
      assertEqual("value", sub[0].subNodes[1].type);
      assertEqual(4, sub[0].subNodes[1].value);
      
      assertEqual("range", sub[1].type);
      assertEqual("value", sub[1].subNodes[0].type);
      assertEqual(1, sub[1].subNodes[0].value);
      assertEqual("value", sub[1].subNodes[1].type);
      assertEqual(2, sub[1].subNodes[1].value);
     

      result = AQL_PARSE("RETURN 3..(4 NOT IN 1)..2").ast;

      assertEqual("root", result[0].type);
      result = result[0].subNodes;

      assertEqual("return", result[0].type);
      result = result[0].subNodes;
      
      assertEqual("range", result[0].type);
      sub = result[0].subNodes;

      assertEqual("range", sub[0].type);
      assertEqual("value", sub[0].subNodes[0].type);
      assertEqual(3, sub[0].subNodes[0].value);
      assertEqual("compare not in", sub[0].subNodes[1].type);
      assertEqual("value", sub[0].subNodes[1].subNodes[0].type);
      assertEqual(4, sub[0].subNodes[1].subNodes[0].value);
      assertEqual("value", sub[0].subNodes[1].subNodes[1].type);
      assertEqual(1, sub[0].subNodes[1].subNodes[1].value);
      assertEqual("value", sub[1].type);
      assertEqual(2, sub[1].value);
    },
    
    testPrecedenceOfNestedNotIn : function() {
      let result = AQL_PARSE("RETURN 3..4 NOT IN 1..2 NOT IN 7..8").ast;

      assertEqual("root", result[0].type);
      result = result[0].subNodes;

      assertEqual("return", result[0].type);
      result = result[0].subNodes;
      
      assertEqual("compare not in", result[0].type);
      let sub = result[0].subNodes;
      assertEqual("compare not in", sub[0].type);
      assertEqual("range", sub[0].subNodes[0].type);
      assertEqual("value", sub[0].subNodes[0].subNodes[0].type);
      assertEqual(3, sub[0].subNodes[0].subNodes[0].value);
      assertEqual("value", sub[0].subNodes[0].subNodes[1].type);
      assertEqual(4, sub[0].subNodes[0].subNodes[1].value);
      assertEqual("range", sub[0].subNodes[1].type);
      assertEqual("value", sub[0].subNodes[1].subNodes[0].type);
      assertEqual(1, sub[0].subNodes[1].subNodes[0].value);
      assertEqual("value", sub[0].subNodes[1].subNodes[1].type);
      assertEqual(2, sub[0].subNodes[1].subNodes[1].value);
      
      assertEqual("range", sub[1].type);
      assertEqual("value", sub[1].subNodes[0].type);
      assertEqual(7, sub[1].subNodes[0].value);
      assertEqual("value", sub[1].subNodes[1].type);
      assertEqual(8, sub[1].subNodes[1].value);
    },
    
    testNotLike : function() {
      let result = AQL_PARSE("RETURN 'a' NOT LIKE 'b'").ast;

      assertEqual("root", result[0].type);
      result = result[0].subNodes;
      
      assertEqual("return", result[0].type);
      result = result[0].subNodes;

      assertEqual("unary not", result[0].type);
      let sub = result[0].subNodes[0];

      assertEqual("function call", sub.type);
      assertEqual("LIKE", sub.name);
    },

    testNotMatches : function() {
      let result = AQL_PARSE("RETURN 'a' NOT =~ 'b'").ast;

      assertEqual("root", result[0].type);
      result = result[0].subNodes;
      
      assertEqual("return", result[0].type);
      result = result[0].subNodes;

      assertEqual("unary not", result[0].type);
      let sub = result[0].subNodes[0];

      assertEqual("function call", sub.type);
      assertEqual("REGEX_TEST", sub.name);
    },

    testNotNotMatches : function() {
      let result = AQL_PARSE("RETURN 'a' NOT !~ 'b'").ast;

      assertEqual("root", result[0].type);
      result = result[0].subNodes;
      
      assertEqual("return", result[0].type);
      result = result[0].subNodes;

      assertEqual("function call", result[0].type);
      assertEqual("REGEX_TEST", result[0].name);
    }
  };
}

jsunity.run(ahuacatlParseTestSuite);

return jsunity.done();
