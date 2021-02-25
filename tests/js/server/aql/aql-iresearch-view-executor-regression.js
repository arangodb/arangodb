/*jshint globalstrict:false, strict:false, sub: true, maxlen: 500 */
/*global assertEqual, assertTrue, assertFalse */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for query language, graph functions
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Markus Pfeiffer
/// @author Copyright 2020, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const db = require("@arangodb").db;
const analyzers = require("@arangodb/analyzers");

const collName1 = "collection_1";
const collName2 = "collection_2";

const analyzerName = "test_analyzer";
const viewName = "testView";

const N = 10001;

const data1 = [
  {
    name: "A",
    seq: 0,
    same: "xyz",
    value: 100,
    duplicated: "abcd",
    prefix: "abcd",
  },
  { name: "B", seq: 1, same: "xyz", value: 101, duplicated: "vczc" },
  { name: "C", seq: 2, same: "xyz", value: 123, duplicated: "vczc" },
  { name: "D", seq: 3, same: "xyz", value: 12, prefix: "abcde" },
  { name: "E", seq: 4, same: "xyz", value: 100, duplicated: "abcd" },
  { name: "F", seq: 5, same: "xyz", value: 1234 },
  { name: "G", seq: 6, same: "xyz", value: 100 },
  { name: "H", seq: 7, same: "xyz", value: 123, duplicated: "vczc" },
  { name: "I", seq: 8, same: "xyz", value: 100, prefix: "bcd" },
  { name: "J", seq: 9, same: "xyz", value: 100 },
  { name: "K", seq: 10, same: "xyz", value: 12, duplicated: "abcd" },
  { name: "L", seq: 11, same: "xyz", value: 95 },
  { name: "M", seq: 12, same: "xyz", value: 90.564 },
  { name: "N", seq: 13, same: "xyz", value: 1, duplicated: "vczc" },
  { name: "O", seq: 14, same: "xyz", value: 0 },
  { name: "P", seq: 15, same: "xyz", value: 50, prefix: "abde" },
  { name: "Q", seq: 16, same: "xyz", value: -32.5, duplicated: "vczc" },
  { name: "R", seq: 17, same: "xyz" },
  { name: "S", seq: 18, same: "xyz", duplicated: "vczc" },
  { name: "T", seq: 19, same: "xyz" },
  { name: "U", seq: 20, same: "xyz", prefix: "abc", duplicated: "abcd" },
  { name: "V", seq: 21, same: "xyz" },
  { name: "W", seq: 22, same: "xyz" },
  {
    name: "X",
    seq: 23,
    same: "xyz",
    duplicated: "vczc",
    prefix: "bateradsfsfasdf",
  },
  { name: "Y", seq: 24, same: "xyz" },
  { name: "Z", seq: 25, same: "xyz", prefix: "abcdrer" },
  { name: "~", seq: 26, same: "xyz", duplicated: "abcd" },
  { name: "!", seq: 27, same: "xyz" },
  { name: "@", seq: 28, same: "xyz", prefix: "ahtrtrt" },
  { name: "#", seq: 29, same: "xyz" },
  { name: "$", seq: 30, same: "xyz", duplicated: "abcd", prefix: "abcy" },
  { name: "%", seq: 31, same: "xyz", prefix: "abcy" },
];

const tearDownAll = () => {
  try {
    db._view(viewName).drop();
    db._drop(collName2, true);
    db._drop(collName1, true);
  } catch (e) {
    // Don't care for error, we might run initially with no views existing
  }
  try {
    analyzers.remove(analyzerName, true);
  } catch (e) {}

  assertEqual(0, db._analyzers.count(), db._analyzers.toArray());
};

function IResearchViewEnumerationRegressionTest() {
  return {
    setUpAll: function () {
      tearDownAll();

      analyzers.save(
        analyzerName,
        "text",
        {
          locale: "en.utf-8",
          case: "lower",
          accent: false,
          stemming: false,
          stopwords: [],
        },
        ["frequency", "norm", "position"]
      );

      const c1 = db._create(collName1);
      const c2 = db._create(collName2);

      for (let i = 0; i < data1.length; i += 2) {
        c1.save(data1[i]);
      }
      for (let i = 1; i < data1.length; i += 2) {
        c2.save(data1[i]);
      }

      const view = db._createView(viewName, "arangosearch", {
        links: {
          [collName1]: {
            analyzers: [analyzerName, "identity"],
            includeAllFields: true,
            trackListPositions: true,
          },
          [collName2]: {
            analyzers: [analyzerName, "identity"],
            includeAllFields: true,
          },
        },
      });
    },
    tearDownAll,

    testRegression: function () {
      const query = `FOR x IN 1..@n FOR d in @@view SEARCH 1 == d.seq OPTIONS { waitForSync: true } RETURN d`;

      const res = db._query(query, { n: N, "@view": viewName });
      const arr = res.toArray();

      assertEqual(arr.length, N);
    },

    /**
     * Test the same query es testRegression but will trigger a skip
     * in the Search executor. This Skip on purpose uses
     * values around 1000 which is the default batch size.
     */
    testRegressionSkip: function () {
      const k = 1200;
      for (const k of [10, 999, 1000, 1001, 1200]) {
        const query = `
        FOR x IN 1..@n
          FOR d in @@view SEARCH 1 == d.seq OPTIONS { waitForSync: true }
          LIMIT ${k}, null
          RETURN d`;

        const res = db._query(query, { n: N, "@view": viewName });
        const arr = res.toArray();

        assertEqual(arr.length, N - k);
      }
    },
  };
}

jsunity.run(IResearchViewEnumerationRegressionTest);
return jsunity.done();
