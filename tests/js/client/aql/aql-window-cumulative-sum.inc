/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertNotEqual, fail */

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
/// @author Jan Steemann
// //////////////////////////////////////////////////////////////////////////////


(function() {
  const jsunity = require("jsunity");
  const db = require("@arangodb").db;
  const internal = require("internal");
  const errors = internal.errors;
  const isCluster = internal.isCluster();
  const isEnterprise = internal.isEnterprise();

  const collectionName = "UnitTestWindowCollection";
  // This test suite expect a collection with 10 documents and keys 0..9 and value v: 0..9 respectively
  // And timestamps like `"time": "2021-05-25 07:00:00",` increasing by 1 minute each

  return function(namePrefix) {
    return {
      [`testAggregate${namePrefix}`]: function() {
        const query = `
        FOR doc IN ${collectionName}
        SORT doc.key
        LET key = doc.key
        WINDOW {preceding: "unbounded", following: 0} AGGREGATE i = SUM(1)
        RETURN {key, i}`;
        const result = db._query(query).toArray();
        for (let i = 0; i < result.length; ++i) {
          assertEqual(result[i].key, `${i}`);
          assertEqual(result[i].i, i + 1);
        }
      },

      [`testLength${namePrefix}`]: function() {
        const query = `
        FOR doc IN ${collectionName}
        SORT doc.key
        LET key = doc.key
        WINDOW {preceding: "unbounded", following: 0} AGGREGATE i = LENGTH()
        RETURN {key, i}`;
        const result = db._query(query).toArray();
        for (let i = 0; i < result.length; ++i) {
          assertEqual(result[i].key, `${i}`);
          assertEqual(result[i].i, i + 1);
        }
      },

      [`testRangeWindow${namePrefix}`]: function() {
        const query = `
        FOR doc IN ${collectionName}
        LET key = doc.key
        WINDOW doc.v WITH {preceding: 100, following: 0} AGGREGATE i = LENGTH()
        RETURN {key, i}`;
        const result = db._query(query).toArray();
        // As Docs key and v are identical, this should yield
        // a result sorted by key.
        for (let i = 0; i < result.length; ++i) {
          assertEqual(result[i].key, `${i}`);
          assertEqual(result[i].i, i + 1);
        }
      },

      [`testRangeDuration${namePrefix}`]: function() {
        const query = `
        FOR doc IN ${collectionName}
        LET key = doc.key
        WINDOW DATE_TIMESTAMP(doc.time) WITH { preceding: "PT03M" }
        AGGREGATE i = LENGTH()
        RETURN {key, i}`;
        const result = db._query(query).toArray();
        // As Docs key and v are identical, this should yield
        // a result sorted by key.
        for (let i = 0; i < result.length; ++i) {
          assertEqual(result[i].key, `${i}`);
          if (i < 4) {
            assertEqual(result[i].i, i + 1);
          } else {
            // All other will have 3 in range + themself
            assertEqual(result[i].i, 4);
          }
        }
      },
    };
  };
}());
