/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global fail, assertEqual, assertNotEqual, assertTrue */

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
/// @author Valery Mironov
// //////////////////////////////////////////////////////////////////////////////

let arangodb = require("@arangodb");
let analyzers = require("@arangodb/analyzers");
let db = arangodb.db;
let jsunity = require("jsunity");
const deriveTestSuite = require('@arangodb/test-helper').deriveTestSuite;
let IM = global.instanceManager;

function ArangoSearchWildcardAnalyzer(hasPos) {
  const dbName = "ArangoSearchWildcardAnalyzer" + hasPos;

  let cleanup = function() {
    IM.debugClearFailAt();
    db._useDatabase("_system");
    try { db._dropDatabase(dbName); } catch (err) {}
  };

  let query = function(pattern, needsPrefix, needsMatcher, ignoreCollection = false) {
    IM.debugClearFailAt();
    if (needsPrefix) {
      IM.debugSetFailAt("wildcard::Filter::needsPrefix");
    } else {
      IM.debugSetFailAt("wildcard::Filter::dissallowPrefix");
    }
    if (needsMatcher) {
      IM.debugSetFailAt("wildcard::Filter::needsMatcher");
    } else {
      IM.debugSetFailAt("wildcard::Filter::dissallowMatcher");
    }
    let c = db._query("FOR d in c FILTER d.s LIKE '" + pattern + "' RETURN d.s").toArray().sort();
    let i = db._query("FOR d in v1 SEARCH ANALYZER(d.s LIKE '" + pattern + "', 'identity') RETURN d.s").toArray().sort();
    let w1 = db._query("FOR d in v1 SEARCH ANALYZER(d.s LIKE '" + pattern + "', 'w') RETURN d.s").toArray().sort();
    let w2 = db._query("FOR d in v2 SEARCH d.s LIKE '" + pattern + "' RETURN d.s").toArray().sort();
    let ii = db._query("FOR d in c OPTIONS { indexHint: 'i', forceIndexHint: true } FILTER d.s LIKE '" + pattern + "' RETURN d.s").toArray().sort();
    if (!ignoreCollection) {
      assertEqual(c, i);
    }
    assertEqual(i, w1);
    assertEqual(i, w2);
    assertEqual(i, ii);
    return i;
  };

  return {
    setUpAll : function () { 
      cleanup();

      db._createDatabase(dbName);
      db._useDatabase(dbName);
      if (hasPos) {
        analyzers.save("w", "wildcard", { ngramSize: 4 }, ["frequency", "position"]);
      } else {
        analyzers.save("w", "wildcard", { ngramSize: 4 }, []);
      }
      let c = db._create("c");
      c.insert({s: ""});
      c.insert({s: "\\"});
      c.insert({s: "abcdef"});
      c.insert({s:  "bcde"});
      c.insert({s:   "c"});
      c.insert({s: "qwerty"});
      c.insert({s:  "wert"});
      c.insert({s:   "e"});
      c.insert({s:   "a"});
      c.insert({s:   "f"});
      c.insert({s: "abcdef qwerty"});
      c.insert({s: "qwerty abcdef"});
      db._createView("v1", "arangosearch",
          { links: { c: { fields: {s: {}}, analyzers: ["w", "identity"] } } });
      c.ensureIndex({name: "i", type: "inverted", fields: ["s"], analyzer: "w"});
      db._createView("v2", "search-alias", { indexes: [ { collection: "c", index: "i" } ] });
    },

    tearDownAll : function () { 
      cleanup();
    },

    testExactEscape1: function() {
      // "\\" -- one escape will be in aql query, because js
      // "\\\\" -- one escape in wildcard pattern, because aql
      try {
        query("\\", false, false);
        fail("invalid pattern should be disallowed by AQL");
      } catch (e) {
      }
    },

    testExactEscape2: function() {
      let res = query("\\\\", false, false);
      assertEqual(res, [""]);
    },
 
    testExactEscape4: function() {
      let res = query("\\\\\\\\", false, false);
      assertEqual(res, ["\\"]);
    },

    testExactEmpty: function() {
      let res = query("", false, false);
      assertEqual(res, [""]);
    },

    testExactShort: function() {
      let res = query("c", false, false);
      assertEqual(res, ["c"]);
    },

    testExactShortEscaped: function() {
      // We don't want to follow AQL behavior, because it's inconsistent:
      // it's ignore last escape but not ignore inner escape
      let res = query("\\\\c", false, false, true);
      assertEqual(res, ["c"]);
    },

    testExactMid: function() {
      let res = query("bcde", false, !hasPos);
      assertEqual(res, ["bcde"]);
    },

    testExactLong: function() {
      let res = query("abcdef", false, !hasPos);
      assertEqual(res, ["abcdef"]);
    },

    testAnyShort: function() {
      let res = query("_", true, true);
      assertEqual(res, ["\\", "a", "c", "e", "f"]);
    },

    testAnyMid1: function() {
      let res = query("bcd_", false, true);
      assertEqual(res, ["bcde"]);
    },

    testAnyMid2: function() {
      let res = query("_cde", false, true);
      assertEqual(res, ["bcde"]);
    },

    testAnyLong: function() {
      let res = query("a_cd_f", false, true);
      assertEqual(res, ["abcdef"]);
    },

    testPrefixShort: function() {
      let res = query("a%", true, false);
      assertEqual(res, ["a", "abcdef", "abcdef qwerty"]);
    },

    testPrefixMed: function() {
      let res = query("abcd%", false, !hasPos);
      assertEqual(res, ["abcdef", "abcdef qwerty"]);
    },

    testPrefixLong: function() {
      let res = query("abcdef%", false, !hasPos);
      assertEqual(res, ["abcdef", "abcdef qwerty"]);
    },

    testSuffixShort: function() {
      let res = query("%f", false, false);
      assertEqual(res, ["abcdef", "f", "qwerty abcdef"]);
    },

    testSuffixMed: function() {
      let res = query("%cdef", false, !hasPos);
      assertEqual(res, ["abcdef", "qwerty abcdef"]);
    },

    testSuffixLong: function() {
      let res = query("%abcdef", false, !hasPos);
      assertEqual(res, ["abcdef", "qwerty abcdef"]);
    },

    testAllShort: function() {
      let res = query("%c%", true, false);
      assertEqual(res, ["abcdef", "abcdef qwerty", "bcde", "c", "qwerty abcdef"]);
    },

    testAllMed: function() {
      let res = query("%bcde%", false, !hasPos);
      assertEqual(res, ["abcdef", "abcdef qwerty", "bcde", "qwerty abcdef"]);
    },

    testAllLong: function() {
      let res = query("%abcdef%", false, !hasPos);
      assertEqual(res, ["abcdef", "abcdef qwerty", "qwerty abcdef"]);
    },

    testAllBetweenLong: function() {
      let res = query("%abcdef% %", false, true);
      assertEqual(res, ["abcdef qwerty"]);
    },

    testAll: function() {
      let res = query("%", true, false);
      assertEqual(res.length, 12);
      res = query("%%", true, false);
      assertEqual(res.length, 12);
      res = query("%%%", true, true);
      assertEqual(res.length, 12);
    },
  };
}

function ArangoSearchWildcardAnalyzerNoPos() {
  let suite = {};
  deriveTestSuite(
    ArangoSearchWildcardAnalyzer(false),
    suite,
    "_no_pos"
  );
  return suite;
}

function ArangoSearchWildcardAnalyzerHasPos() {
  let suite = {};
  deriveTestSuite(
    ArangoSearchWildcardAnalyzer(true),
    suite,
    "_has_pos"
  );
  return suite;
}

jsunity.run(ArangoSearchWildcardAnalyzerNoPos);
jsunity.run(ArangoSearchWildcardAnalyzerHasPos);

return jsunity.done();
