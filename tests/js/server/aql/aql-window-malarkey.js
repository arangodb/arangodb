/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertNotEqual, assertTrue, fail */

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const db = require("@arangodb").db;
const internal = require("internal");
const errors = internal.errors;

function WindowMalarkeyTestSuite() {

  const cname1 = "WindowTestCollection1";
  const cname2 = "WindowTestCollection2";
  let c1 = null, c2 = null;
  return {
    setUp: function () {
      c1 = db._create(cname1, { numberOfShards: 4 });
      c2 = db._create(cname2, { numberOfShards: 4 });

      // add sorted index on time
      c1.ensureIndex({ type: 'skiplist', fields: ['time'] });

      let docs = [];
      for (i = 0; i < 1000; i++) {
        docs.push({ value: i, time: Date.now() })
      }
      c1.insert(docs);
    },

    tearDown: function () {
      db._drop(cname1);
      db._drop(cname2);
    },

    testResultsInsertAfterRow: function () {
      const q = `
      FOR doc IN @@input 
        SORT doc.value ASC
        WINDOW {preceding: 1, following: 1} AGGREGATE rolling_sum = SUM(doc.value), av = AVG(doc.value), l = LENGTH(doc)
        INSERT {value: doc.value, sum: rolling_sum, avg: av, len: l} into @@output
      `;
      db._query(q, { '@input': cname1, '@output': cname2 });
      assertEqual(c1.count(), c2.count());
      let count = c1.count();
      c2.toArray().forEach(doc => {
        let sum = Math.max(0, doc.value - 1) + doc.value + (doc.value < count-1 ? doc.value + 1 : 0);
        assertEqual(doc.sum, sum, doc);
        assertEqual(doc.avg, doc.sum / doc.len, doc);
      });
    },

    testResultsInsertAfterRange: function () {
      const q = `
      FOR doc IN @@input 
        WINDOW doc.value WITH {preceding: 1, following: 1} AGGREGATE rolling_sum = SUM(doc.value), av = AVG(doc.value), l = LENGTH(doc)
        INSERT {value: doc.value, sum: rolling_sum, avg: av, len: l} into @@output
      `;
      db._query(q, { '@input': cname1, '@output': cname2 });
      assertEqual(c1.count(), c2.count());
      let count = c1.count();
      c2.toArray().forEach(doc => {
        let sum = Math.max(0, doc.value - 1) + doc.value + (doc.value < count-1 ? doc.value + 1 : 0);
        assertEqual(doc.sum, sum);
        assertEqual(doc.avg, doc.sum / doc.len, doc);
      });
    },

    testResultsUpdateAfterRow: function () {
      const q = `
      FOR doc IN @@cc 
        SORT doc.value ASC
        WINDOW doc.value WITH {preceding: 1, following: 1} AGGREGATE rolling_sum = SUM(doc.value), av = AVG(doc.value), l = LENGTH(doc)
        UPDATE doc WITH {value: doc.value, sum: rolling_sum, avg: av, len: l} into @@cc
      `;
      db._query(q, { '@cc': cname1 });
      let count = c1.count();
      c1.toArray().forEach(doc => {
        let sum = Math.max(0, doc.value - 1) + doc.value + (doc.value < count-1 ? doc.value + 1 : 0);
        assertEqual(doc.sum, sum);
        assertEqual(doc.avg, doc.sum / doc.len, doc);
      });
    },

    testResultsUpdateAfterRange: function () {
      const q = `
      FOR doc IN @@cc 
        WINDOW doc.value WITH {preceding: 1, following: 1} AGGREGATE rolling_sum = SUM(doc.value), av = AVG(doc.value), l = LENGTH(doc)
        UPDATE doc WITH {value: doc.value, sum: rolling_sum, avg: av, len: l} into @@cc
      `;
      db._query(q, { '@cc': cname1 });
      let count = c1.count();
      c1.toArray().forEach(doc => {
        let sum = Math.max(0, doc.value - 1) + doc.value + (doc.value < count-1 ? doc.value + 1 : 0);
        assertEqual(doc.sum, sum);
        assertEqual(doc.avg, doc.sum / doc.len, doc);
      });
    },

    testResultsInsertBefore: function () {
      const q = `
      FOR doc IN @@input 
        INSERT {value: doc.value} into @@output
        WINDOW {preceding: 1, following: 1} AGGREGATE av = AVG(doc.value)
        RETURN av 
      `;
      let didThrow = false;
      try {
        db._query(q, { '@input': cname1, '@output': cname2 });
        worked = true;
      } catch (err) {
        didThrow = true;
        assertEqual(internal.errors.ERROR_QUERY_WINDOW_AFTER_MODIFICATION.code, err.errorNum);
      }
      assertTrue(didThrow);
    },

    testResultsUpdateBefore: function () {
      const q = `
      FOR doc IN @@cc 
        UPDATE doc WITH {value2: doc.value + 1} into @@cc
        WINDOW {preceding: 1, following: 1} AGGREGATE av = AVG(doc.value)
        RETURN av 
      `;
      let didThrow = false;
      try {
        db._query(q, { '@cc': cname1 });
        worked = true;
      } catch (err) {
        didThrow = true;
        assertEqual(internal.errors.ERROR_QUERY_WINDOW_AFTER_MODIFICATION.code, err.errorNum);
      }
      assertTrue(didThrow);
    },

    testResultsReplaceBefore: function () {
      const q = `
      FOR doc IN @@cc 
        REPLACE doc WITH {value: doc.value + 1} into @@cc
        WINDOW {preceding: 1, following: 1} AGGREGATE av = AVG(doc.value)
        RETURN av 
      `;
      let didThrow = false;
      try {
        db._query(q, { '@cc': cname1 });
        worked = true;
      } catch (err) {
        didThrow = true;
        assertEqual(internal.errors.ERROR_QUERY_WINDOW_AFTER_MODIFICATION.code, err.errorNum);
      }
      assertTrue(didThrow);
    },

  };
}

jsunity.run(WindowMalarkeyTestSuite);

return jsunity.done();
