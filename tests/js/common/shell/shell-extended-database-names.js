/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertNotEqual, assertTrue, fail, NORMALIZE_STRING */

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2018 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const arangodb = require("@arangodb");
const internal = require("internal");
const console = require("console");
const db = arangodb.db;

function DatabaseNamesSuite() {
  return {
    setUp : function () {
      db._useDatabase("_system");
    },
    
    tearDown : function () {
      db._useDatabase("_system");
    },
    
    testInvalidPunctuationDatabaseNames : function () {
      const names = [
        "",
        " ",
        "_",
        ".",
        ":",
        "/",
        "//",
        "a/",
        "/a",
        "a/1",
        "foo/bar",
        "foo/bar/baz",
        ".a.a.a.a.",
        "..",
        "0",
        "1",
        "2",
        "3",
        "4",
        "5",
        "6",
        "7",
        "8",
        "9",
        "10",
        "00",
        "10000",
        "001000",
        "999999999",
        "9999/3933",
        "9aaaa",
      ];

      names.forEach((name) => {
        db._useDatabase("_system");
        try {
          db._createDatabase(name);
          fail();
        } catch (err) {
          assertEqual(internal.errors.ERROR_ARANGO_DATABASE_NAME_INVALID.code, err.errorNum, name);
        }
      });
    },
    
    testValidPunctuationDatabaseNames : function () {
      const names = [
        "a b c",
        "A B C",
        "a.a.a.a.",
        "a00000",
        "a quick brown fox",
        "(foo|bar|baz)",
        "-.-.-.,",
        "-,;!",
        "!!",
        "\"",
        "\\",
        "'",
        "$#%&",
        "()",
        "=",
        "[]{}<>|",
        "?",
        "´`",
        "+-*#",
      ];
      
      names.forEach((name) => {
        console.warn("creating database '" + name + "'");
        db._useDatabase("_system");
        let d = db._createDatabase(name);
        try {
          assertTrue(d);

          assertNotEqual(-1, db._databases().indexOf(name), name);

          db._useDatabase(name);
          assertEqual(db._name(), name);
        
          db._useDatabase("_system");
          db._dropDatabase(name);
          assertEqual(-1, db._databases().indexOf(name), name);
        } catch (err) {
          db._useDatabase("_system");
          try {
            db._dropDatabase(name);
          } catch (err2) {}
          // must rethrow to not swallow errors
          throw err;
        }
      });
    },

    testCheckUnicodeDatabaseNames : function () {
      // some of these test strings are taken from https://www.w3.org/2001/06/utf-8-test/UTF-8-demo.html
      // note: some of these only work because the `_createDatabase()` methods on client and server
      // NFC-normalize their inputs. But without NFC-normalization, some of these names actually would
      // not work!
      const names = [
        "mötör",
        "TRÖÖÖÖÖÖÖÖÖTKÄKÄR",
        "∮ E⋅da = Q,  n → ∞, ∑ f(i) = ∏ g(i)",
        "∀x∈ℝ ⌈x⌉ = −⌊−x⌋",
        "α ∧ ¬β = ¬(¬α ∨ β)",
        "two H₂ + O₂ ⇌ 2H₂O",
        "R = 4.7 kΩ, ⌀ 200 mm",
        "ði ıntəˈnæʃənəl fəˈnɛtık əsoʊsiˈeıʃn",
        "Y [ˈʏpsilɔn], Yen [jɛn], Yoga [ˈjoːgɑ]",
        "Οὐχὶ ταὐτὰ παρίσταταί μοι",
        "γιγνώσκειν, ὦ ἄνδρες ᾿Αθηναῖοι",
        "გთხოვთ ახლავე გაიაროთ რეგისტრაცია",
        "Unicode-ის მეათე საერთაშორისო",
        "Зарегистрируйтесь сейчас на",
        "Десятую Международную Конференцию по",
        "ሰማይ አይታረስ ንጉሥ አይከሰስ።",
        "ᚻᛖ ᚳᚹᚫᚦ ᚦᚫᛏ ᚻᛖ ᛒᚢᛞᛖ ᚩᚾ ᚦᚫᛗ",
        "ᛚᚪᚾᛞᛖ ᚾᚩᚱᚦᚹᛖᚪᚱᛞᚢᛗ ᚹᛁᚦ ᚦᚪ ᚹᛖᛥᚫ",
        "⡌⠁⠧⠑ ⠼⠁⠒  ⡍⠜⠇⠑⠹⠰⠎ ⡣⠕⠌",
        "£ ß ó ę Я λ",
        "💩🍺🌧⛈🌩⚡🔥💥🌨",
        "😀 *grinning* 😬 *grimacing* 😅 *sweat_smile* 😆 *laughing*",
        "😁 *grin* 😂 *joy* 😃 *smiley* 😄 *smile*",
        "😇 *innocent* 😉 *wink* 😊 *blush* 🙂 *slight_smile*",
        "🙃 *upside_down* 😋 *yum* 😌 *relieved* 😍 *heart_eyes*",
        "😘 *kissing_heart* 😗 *kissing* 😙 *kissing_smiling_eyes* 😚 *kissing_closed_eyes*",
        "😜 *stuck_out_tongue_winking_eye* 😝 *stuck_out_tongue_closed_eyes*",
        "😛 *stuck_out_tongue* 🤑 *money_mouth*",
        "🤓 *nerd* 😎 *sunglasses* 🤗 *hugging* 😏 *smirk*",
        "😶 *no_mouth* 😐 *neutral_face*",
        "😑 *expressionless* 😒 *unamused* 🙄 *rolling_eyes* 🤔 *thinking*",
        "😳 *flushed* 😞 *disappointed* 😟 *worried* 😠 *angry*",
        "😡 *rage* 😔 *pensive* 😕 *confused*", 
        "\u00c5", // Angstrom
        "\u1e69", // s with two combining marks
      ];

      names.forEach((name) => {
        console.warn("creating database '" + name + "'");
        db._useDatabase("_system");
        let d = db._createDatabase(name);
        try {
          assertTrue(d);

          assertNotEqual(-1, db._databases().indexOf(NORMALIZE_STRING(name)), NORMALIZE_STRING(name));

          db._useDatabase(name);
          assertEqual(NORMALIZE_STRING(db._name()), NORMALIZE_STRING(name));
          
          db._useDatabase("_system");
          db._dropDatabase(name);
          assertEqual(-1, db._databases().indexOf(name), NORMALIZE_STRING(name));
        } catch (err) {
          db._useDatabase("_system");
          try {
            db._dropDatabase(name);
          } catch (err2) {}
          // must rethrow to not swallow errors
          throw err;
        }
      });
    },

    testNonNormalizedUnicodeDatabaseNames : function () {
      const names = [
        "\u212b", // Angstrom, not normalized
        "\u0041\u030a", // Angstrom, NFD-normalized
        "\u0073\u0323\u0307", // s with two combining marks, NFD-normalized
        "\u006e\u0303\u00f1", // non-normalized sequence
      ];

      // even though the names are not properly NFC-normalized, this works.
      // this is because the arangosh (shell_client) will normalize all inputs
      // when creating a database from a client-provided value. The server-side
      // JS APIs will do the same. 
      // however, sending such values via the HTTP API will fail.
      names.forEach((name) => {
        db._useDatabase("_system");
        let d = db._createDatabase(name);
        try {
          assertTrue(d);
        } finally {
          db._dropDatabase(name);
        }
      });
    },

  };
}

jsunity.run(DatabaseNamesSuite);

return jsunity.done();
