/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertTrue, assertFalse, assertEqual, assertNotEqual */

const { assert, log } = require("console");

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
/// @author Koushal Kawade
/// @author Copyright 2025, ArangoDB GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////
(function() {
  var db = require("@arangodb").db;
  return function (newDbName, dbArgs) {
    const ncoll = "coll";
    const nviewSortedOnBaseObj = "view1";
    const nviewSortedOnPostfix = "view2";
    let records = [
      {_key: 'c200_1_2_3_4',
        obj: {a: {a1: 200}, b: {b1: 1}, c: 2, d: {d1: 3}, e: {e1: 4}}},
      {
      _key: 'c_10_11_12_13_14',
      obj: {a: {a1: 10}, b: {b1: 11}, c: 12, d: {d1: 13}, e: {e1: 14}}
      },
      {
      _key: 'c20_21_22_23_24',
      obj: {a: {a1: 20}, b: {b1: 21}, c: 22, d: {d1: 23}, e: {e1: 24}}
      },
      {
      _key: 'c_30_31_32_33_34',
      obj: {a: {a1: 30}, b: {b1: 31}, c: 32, d: {d1: 33}, e: {e1: 34}}
      }
    ];

    return {
      setUpAll: function () {
        db._useDatabase("_system");
        try {
          db._dropDatabase(newDbName);
        } catch (err) {
        }
        db._createDatabase(newDbName, dbArgs);
        db._useDatabase(newDbName);
        let numberOfShards = 3;
        if (dbArgs.sharding === "single") {
          numberOfShards = 1;
        }
        let coll = db._create(ncoll, {numberOfShards});

        //  view1 sorts on doc.obj.
        //  We'll run queries that sort on doc.obj.d.d1
        let viewSortedOnBaseObj = db._createView(nviewSortedOnBaseObj, "arangosearch", {
          consolidationIntervalMsec: 5000,
          primarySort: [{"field": "obj", "direction": "asc"}],
          primarySortCompression: "none",
          links: {
            [ncoll]: {includeAllFields: true, fields: {strval: {analyzers: ['text_en']}}}
          }
        });

        //  view2 sorts on doc.obj.d.d1.
        //  We'll run queries that sort on doc.obj
        let viewSortedOnPostfix = db._createView(nviewSortedOnPostfix, "arangosearch", {
          consolidationIntervalMsec: 5000,
          primarySort: [{"field": "obj.d.d1", "direction": "asc"}],
          primarySortCompression: "none",
          links: {
            [ncoll]: {includeAllFields: true, fields: {strval: {analyzers: ['text_en']}}}
          }
        });

        for (const record of records) {
          coll.save(record);
        }

        db._query("FOR d IN " + nviewSortedOnBaseObj + " OPTIONS { waitForSync: true } RETURN d");
        db._query("FOR d IN " + nviewSortedOnPostfix + " OPTIONS { waitForSync: true } RETURN d");
      },
      tearDownAll : function () {
        db._useDatabase("_system");
        db._dropDatabase(newDbName);
      },

      //  primarySort field: obj
      //  SORT: obj.d, obj.d.d1
      testSortLimitWithBaseSortField() {
        let testcases = [
          //  doc.obj desc
          {
            expected: ['c200_1_2_3_4', 'c_30_31_32_33_34', 'c20_21_22_23_24', 'c_10_11_12_13_14'],
            query: "for doc in " + nviewSortedOnBaseObj + " SORT doc.obj desc limit 10 return doc",
            description: "doc.obj desc"
          },

          //  doc.obj asc
          {
            expected: ['c_10_11_12_13_14', 'c20_21_22_23_24', 'c_30_31_32_33_34', 'c200_1_2_3_4'],
            query: "for doc in " + nviewSortedOnBaseObj + " SORT doc.obj asc limit 10 return doc",
            description: "doc.obj asc"
          },

          //  doc.obj.d desc
          {
            expected: ['c_30_31_32_33_34', 'c20_21_22_23_24', 'c_10_11_12_13_14', 'c200_1_2_3_4'],
            query: "for doc in " + nviewSortedOnBaseObj + " SORT doc.obj.d desc limit 10 return doc",
            description: "doc.obj.d desc"
          },

          //  doc.obj.d asc
          {
            expected: ['c200_1_2_3_4', 'c_10_11_12_13_14', 'c20_21_22_23_24', 'c_30_31_32_33_34'],
            query: "for doc in " + nviewSortedOnBaseObj + " SORT doc.obj.d asc limit 10 return doc",
            description: "doc.obj.d asc"
          },

          //  doc.obj.d.d1 desc
          {
            expected: ['c_30_31_32_33_34', 'c20_21_22_23_24', 'c_10_11_12_13_14', 'c200_1_2_3_4'],
            query: "for doc in " + nviewSortedOnBaseObj + " SORT doc.obj.d.d1 desc limit 10 return doc",
            description: "doc.obj.d.d1 desc"
          },

          //  doc.obj.d.d1 asc
          {
            expected: ['c200_1_2_3_4', 'c_10_11_12_13_14', 'c20_21_22_23_24', 'c_30_31_32_33_34'],
            query: "for doc in " + nviewSortedOnBaseObj + " SORT doc.obj.d.d1 asc limit 10 return doc",
            description: "doc.obj.d.d1 asc"
          }
        ];

        for (let i = 0; i < testcases.length; i++) {

          let testcase = testcases[i];
          let results = db._query(testcase.query).toArray();
          let resultKeys = results.map(row => row['_key']);
          assertEqual(resultKeys, testcase.expected, "Failed test description: (" + testcase.description + ")");
        }

        //  `return doc._key`
        //  BTS-2283: returning a different value from the same
        //  query was generating results in a different sorted order.
        for (let i = 0; i < testcases.length; i++) {

          let testcase = testcases[i];
          let resultKeys = db._query(testcase.query + "._key");
          assertEqual(resultKeys['data']['result'], testcase.expected, "Failed test description: (" + testcase.description + ")");
        }
      },

      //  primarySort field: obj
      //  SORT: obj.d, obj.d.d1
      testSortLimitForSortFieldWithPostfix() {
        let testcases = [
          //  SORT obj.d.d1 desc
          {
            expected: ['c_30_31_32_33_34', 'c20_21_22_23_24', 'c_10_11_12_13_14', 'c200_1_2_3_4'],
            query: "for doc in " + nviewSortedOnPostfix + " SORT doc.obj.d.d1 desc limit 10 return doc",
            description: "SORT obj.d.d1 desc"
          },
          
          //  SORT obj.d.d1 asc
          {
            expected: ['c200_1_2_3_4', 'c_10_11_12_13_14', 'c20_21_22_23_24', 'c_30_31_32_33_34'],
            query: "for doc in " + nviewSortedOnPostfix + " SORT doc.obj.d.d1 asc limit 10 return doc",
            description: "SORT obj.d.d1 asc"
          },
          
          //  SORT doc.obj.d desc
          {
            expected: ['c_30_31_32_33_34', 'c20_21_22_23_24', 'c_10_11_12_13_14', 'c200_1_2_3_4'],
            query: "for doc in " + nviewSortedOnPostfix + " SORT doc.obj.d desc limit 10 return doc",
            description: "SORT doc.obj.d desc"
          },
          
          //  SORT doc.obj.d asc
          {
            expected: ['c200_1_2_3_4', 'c_10_11_12_13_14', 'c20_21_22_23_24', 'c_30_31_32_33_34'],
            query: "for doc in " + nviewSortedOnPostfix + " SORT doc.obj.d asc limit 10 return doc",
            description: "SORT doc.obj.d asc"
          },
          
          //  SORT doc.obj desc
          {
            expected: ['c200_1_2_3_4', 'c_30_31_32_33_34', 'c20_21_22_23_24', 'c_10_11_12_13_14'],
            query: "for doc in " + nviewSortedOnPostfix + " SORT doc.obj desc limit 10 return doc",
            description: "SORT doc.obj desc"
          },
          
          //  SORT doc.obj asc
          {
            expected: ['c_10_11_12_13_14', 'c20_21_22_23_24', 'c_30_31_32_33_34', 'c200_1_2_3_4'],
            query: "for doc in " + nviewSortedOnPostfix + " SORT doc.obj asc limit 10 return doc",
            description: "SORT doc.obj asc"
          }
        ];

        for (let i = 0; i < testcases.length; i++) {

          let testcase = testcases[i];
          let results = db._query(testcase.query);
          let resultKeys = results['data']['result'].map(row => row['_key']);
          assertEqual(resultKeys, testcase.expected, "Failed test description: (" + testcase.description + ")");
        }

        //  `return doc._key`
        //  BTS-2283: returning a different value from the same
        //  query was generating results in a different sorted order.
        for (let i = 0; i < testcases.length; i++) {

          let testcase = testcases[i];
          let resultKeys = db._query(testcase.query + "._key");
          assertEqual(resultKeys['data']['result'], testcase.expected, "Failed test description: (" + testcase.description + ")");
        }
      },
    };
  };
}());
