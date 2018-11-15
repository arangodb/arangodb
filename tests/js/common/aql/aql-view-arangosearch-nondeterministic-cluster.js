/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertUndefined, assertEqual, assertTrue, assertFalse*/

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for iresearch usage
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
var db = require("@arangodb").db;
const deriveTestSuite = require('@arangodb/test-helper').deriveTestSuite;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function IResearchAqlTestSuite(args) {
  var c;
  var v;

  // provided arguments
  console.info("Test suite arguments: " + JSON.stringify(args));

  return {
    setUp : function () {
      db._drop("UnitTestsCollection");
      c = db._create("UnitTestsCollection", args);

      db._drop("AnotherUnitTestsCollection");
      var ac = db._create("AnotherUnitTestsCollection", args);

      db._dropView("UnitTestsView");
      v = db._createView("UnitTestsView", "arangosearch", {});
      var meta = {
        links: { 
          "UnitTestsCollection": { 
            includeAllFields: true,
            storeValues: "id",
            fields: {
              text: { analyzers: [ "text_en" ] }
            }
          }
        }
      };
      v.properties(meta);

      ac.save({ a: "foo", id : 0 });
      ac.save({ a: "ba", id : 1 });

      for (let i = 0; i < 5; i++) {
        c.save({ a: "foo", b: "bar", c: i });
        c.save({ a: "foo", b: "baz", c: i });
        c.save({ a: "bar", b: "foo", c: i });
        c.save({ a: "baz", b: "foo", c: i });
      }

      c.save({ name: "full", text: "the quick brown fox jumps over the lazy dog" });
      c.save({ name: "half", text: "quick fox over lazy" });
      c.save({ name: "other half", text: "the brown jumps the dog" });
      c.save({ name: "quarter", text: "quick over" });

      c.save({ name: "numeric", anotherNumericField: 0 });
      c.save({ name: "null", anotherNullField: null });
      c.save({ name: "bool", anotherBoolField: true });
      c.save({ _key: "foo", xyz: 1 });
    },

    tearDown : function () {
      var meta = { links : { "UnitTestsCollection": null } };
      v.properties(meta);
      v.drop();
      db._drop("UnitTestsCollection");
      db._drop("AnotherUnitTestsCollection");
    },

    testInTokensFilterSortTFIDF : function () {
      var result = db._query("FOR doc IN UnitTestsView SEARCH ANALYZER(doc.text IN TOKENS('the quick brown', 'text_en'), 'text_en') OPTIONS { waitForSync: true } SORT TFIDF(doc) LIMIT 4 RETURN doc").toArray();

      assertEqual(result.length, 4);
      assertEqual(result[0].name, 'other half');
      assertEqual(result[1].name, 'full');
      assertEqual(result[2].name, 'half');
      assertEqual(result[3].name, 'quarter');
    },

/*    testViewInInnerLoopSortByTFIDF_BM25_Attribute : function() {
      var expected = [];
      expected.push({ a: "baz", b: "foo", c: 1 });
      expected.push({ a: "bar", b: "foo", c: 1 });
      expected.push({ a: "foo", b: "bar", c: 0 });
      expected.push({ a: "foo", b: "baz", c: 0 });

      var result = db._query(
        "FOR adoc IN AnotherUnitTestsCollection" +
        "  FOR doc IN UnitTestsView SEARCH adoc.id == doc.c && STARTS_WITH(doc['a'], adoc.a) " +
        "OPTIONS { waitForSync: true } SORT TFIDF(doc) DESC, BM25(doc) DESC, doc.a DESC, doc.b " +
        "RETURN doc").toArray();

      assertEqual(result.length, expected.length);
      var i = 0;
      result.forEach(function(res) {
        var doc = expected[i++];
        assertEqual(doc.a, res.a);
        assertEqual(doc.b, res.b);
        assertEqual(doc.c, res.c);
      });
    }*/

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(function IResearchAqlTestSuite_s1_r1() {
  let derivedSuite = {};
  deriveTestSuite(IResearchAqlTestSuite({ numberOfShards: 1, replicationFactor: 1 }), derivedSuite, "_1_1");
  return derivedSuite;
});

jsunity.run(function IResearchAqlTestSuite_s4_r1() {
  let derivedSuite = {};
  deriveTestSuite(IResearchAqlTestSuite({ numberOfShards: 4, replicationFactor: 1 }), derivedSuite, "_4_1");
  return derivedSuite;
});

jsunity.run(function IResearchAqlTestSuite_s1_r2() {
  let derivedSuite = {};
  deriveTestSuite(IResearchAqlTestSuite({ numberOfShards: 1, replicationFactor: 2 }), derivedSuite, "_1_2");
  return derivedSuite;
});

jsunity.run(function IResearchAqlTestSuite_s4_r3() {
  let derivedSuite = {};
  deriveTestSuite(IResearchAqlTestSuite({ numberOfShards: 4, replicationFactor: 2 }), derivedSuite, "_4_2");
  return derivedSuite;
});

return jsunity.done();
