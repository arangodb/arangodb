/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertUndefined, assertEqual, assertTrue, assertFalse, db._query */

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
var db = require("@arangodb").db;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function iResearchFeatureAqlTestSuite () {
  return {
    setUpAll : function () {
    },

    tearDownAll : function () {
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief IResearchFeature tests
////////////////////////////////////////////////////////////////////////////////

    testDefaultAnalyzers : function() {
      // invalid
      {
        let result = db._query(
          "RETURN TOKENS('a quick brown fox jumps', 'invalid')",
          null,
          { }
        ).toArray();
        assertEqual(1, result.length);
        assertEqual(null, result[0]);
      }

      // text_de
      {
        let result = db._query(
          "RETURN TOKENS('a quick brown fox jumps', 'text_de')",
          null,
          { }
        ).toArray();
        assertEqual(1, result.length);
        assertTrue(Array === result[0].constructor);
        assertEqual(5, result[0].length);
        assertEqual([ "a", "quick", "brown", "fox", "jumps" ], result[0]);
      }

      // text_en
      {
        let result = db._query(
          "RETURN TOKENS('a quick brown fox jumps', 'text_en')",
          null,
          { }
        ).toArray();
        assertEqual(1, result.length);
        assertTrue(Array === result[0].constructor);
        assertEqual(5, result[0].length);
        assertEqual([ "a", "quick", "brown", "fox", "jump" ], result[0]);
      }

      // text_es
      {
        let result = db._query(
          "RETURN TOKENS('a quick brown fox jumps', 'text_es')",
          null,
          { }
        ).toArray();
        assertEqual(1, result.length);
        assertTrue(Array === result[0].constructor);
        assertEqual(5, result[0].length);
        assertEqual([ "a", "quick", "brown", "fox", "jumps" ], result[0]);
      }

      // text_fi
      {
        let result = db._query(
          "RETURN TOKENS('a quick brown fox jumps', 'text_fi')",
          null,
          { }
        ).toArray();
        assertEqual(1, result.length);
        assertTrue(Array === result[0].constructor);
        assertEqual(5, result[0].length);
        assertEqual([ "a", "quick", "brow", "fox", "jumps" ], result[0]);
      }

      // text_fr
      {
        let result = db._query(
          "RETURN TOKENS('a quick brown fox jumps', 'text_fr')",
          null,
          { }
        ).toArray();
        assertEqual(1, result.length);
        assertTrue(Array === result[0].constructor);
        assertEqual(5, result[0].length);
        assertEqual([ "a", "quick", "brown", "fox", "jump" ], result[0]);
      }

      // text_it
      {
        let result = db._query(
          "RETURN TOKENS('a quick brown fox jumps', 'text_it')",
          null,
          { }
        ).toArray();
        assertEqual(1, result.length);
        assertTrue(Array === result[0].constructor);
        assertEqual(5, result[0].length);
        assertEqual([ "a", "quick", "brown", "fox", "jumps" ], result[0]);
      }

      // text_nl
      {
        let result = db._query(
          "RETURN TOKENS('a quick brown fox jumps', 'text_nl')",
          null,
          { }
        ).toArray();
        assertEqual(1, result.length);
        assertTrue(Array === result[0].constructor);
        assertEqual(5, result[0].length);
        assertEqual([ "a", "quick", "brown", "fox", "jump" ], result[0]);
      }

      // text_no
      {
        let result = db._query(
          "RETURN TOKENS('a quick brown fox jumps', 'text_no')",
          null,
          { }
        ).toArray();
        assertEqual(1, result.length);
        assertTrue(Array === result[0].constructor);
        assertEqual(5, result[0].length);
        assertEqual([ "a", "quick", "brown", "fox", "jump" ], result[0]);
      }

      // text_pt
      {
        let result = db._query(
          "RETURN TOKENS('a quick brown fox jumps', 'text_pt')",
          null,
          { }
        ).toArray();
        assertEqual(1, result.length);
        assertTrue(Array === result[0].constructor);
        assertEqual(5, result[0].length);
        assertEqual([ "a", "quick", "brown", "fox", "jumps" ], result[0]);
      }

      // text_ru (codepoints)
      {
        let result = db._query(
          "RETURN TOKENS('ArangoDB - \u044D\u0442\u043E \u043C\u043D\u043E\u0433\u043E\u043C\u043E\u0434\u0435\u043B\u044C\u043D\u0430\u044F \u0431\u0430\u0437\u0430 \u0434\u0430\u043D\u043D\u044B\u0445', 'text_ru')",
          null,
          { }
        ).toArray();
        assertEqual(1, result.length);
        assertTrue(Array === result[0].constructor);
        assertEqual(5, result[0].length);
        assertEqual([ "arangodb", "\u044D\u0442", "\u043C\u043D\u043E\u0433\u043E\u043C\u043E\u0434\u0435\u043B\u044C\u043D", "\u0431\u0430\u0437", "\u0434\u0430\u043D" ], result[0]);
        assertEqual([ "arangodb", "эт", "многомодельн", "баз", "дан" ], result[0]);
      }

      // text_ru (unicode)
      {
        let result = db._query(
          "RETURN TOKENS('ArangoDB - это многомодельная база данных', 'text_ru')",
          null,
          { }
        ).toArray();
        assertEqual(1, result.length);
        assertTrue(Array === result[0].constructor);
        assertEqual(5, result[0].length);
        assertEqual([ "arangodb", "\u044D\u0442", "\u043C\u043D\u043E\u0433\u043E\u043C\u043E\u0434\u0435\u043B\u044C\u043D", "\u0431\u0430\u0437", "\u0434\u0430\u043D" ], result[0]);
        assertEqual([ "arangodb", "эт", "многомодельн", "баз", "дан" ], result[0]);
      }

      // text_sv
      {
        let result = db._query(
          "RETURN TOKENS('a quick brown fox jumps', 'text_sv')",
          null,
          { }
        ).toArray();
        assertEqual(1, result.length);
        assertTrue(Array === result[0].constructor);
        assertEqual(5, result[0].length);
        assertEqual([ "a", "quick", "brown", "fox", "jump" ], result[0]);
      }

      // text_zh (codepoints)
      {
        let result = db._query(
           "RETURN TOKENS('ArangoDB \u662F\u4E00\u4E2A\u591A\u6A21\u578B\u6570\u636E\u5E93\u3002', 'text_zh')",
          null,
          { }
        ).toArray();
        assertEqual(1, result.length);
        assertTrue(Array === result[0].constructor);
        assertEqual(7, result[0].length);
        assertEqual([ "arangodb", "\u662F", "\u4E00\u4E2A", "\u591A", "\u6A21\u578B", "\u6570\u636E", "\u5E93" ], result[0]);
        assertEqual([ "arangodb", "是", "一个", "多", "模型", "数据", "库" ], result[0]);
      }

      // text_zh (unicode)
      {
        let result = db._query(
          "RETURN TOKENS('ArangoDB 是一个多模型数据库。', 'text_zh')",
          null,
          { }
        ).toArray();
        assertEqual(1, result.length);
        assertTrue(Array === result[0].constructor);
        assertEqual(7, result[0].length);
        assertEqual([ "arangodb", "\u662F", "\u4E00\u4E2A", "\u591A", "\u6A21\u578B", "\u6570\u636E", "\u5E93" ], result[0]);
        assertEqual([ "arangodb", "是", "一个", "多", "模型", "数据", "库" ], result[0]);
      }

    },

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(iResearchFeatureAqlTestSuite);

return jsunity.done();
