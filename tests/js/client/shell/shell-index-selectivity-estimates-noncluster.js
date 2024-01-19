/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertTrue, fail, arango */

////////////////////////////////////////////////////////////////////////////////
/// @brief test the persistent index, selectivity estimates
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

const jsunity = require("jsunity");
const internal = require("internal");
const {waitForEstimatorSync } = require('@arangodb/test-helper');

function SelectivityIndexSuite() {
  'use strict';

  const cn = "UnitTestsCollection";

  let collection = null;

  return {

    setUp : function () {
      internal.db._drop(cn);
      collection = internal.db._create(cn);
    },

    tearDown : function () {
      collection = internal.db._drop(cn);
      collection = null;
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief persistent index selectivity
////////////////////////////////////////////////////////////////////////////////

    testSelectivityEstimateUnique : function () {
      let idx = collection.ensureIndex({ type: "persistent", fields: ["value"], unique: true });
      let docs = [];
      for (let i = 0; i < 1000; ++i) {
        docs.push({ _key: "test" + i, value: i });
      }
      collection.insert(docs);

      waitForEstimatorSync(); // make sure estimates are consistent
      idx = collection.ensureIndex({ type: "persistent", fields: ["value"], unique: true });
      assertEqual(1, idx.selectivityEstimate);

      for (let i = 0; i < 50; ++i) {
        collection.remove("test" + i);
      }

      waitForEstimatorSync(); // make sure estimates are consistent
      idx = collection.ensureIndex({ type: "persistent", fields: ["value"], unique: true });
      assertEqual(1, idx.selectivityEstimate);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief multi persistent index selectivity
////////////////////////////////////////////////////////////////////////////////

    testSelectivityEstimateNonUnique : function () {
      let idx = collection.ensureIndex({ type: "persistent", fields: ["value"] });
      let docs = [];
      for (let i = 0; i < 1000; ++i) {
        docs.push({ value: i });
      }
      collection.insert(docs);

      waitForEstimatorSync(); // make sure estimates are consistent
      idx = collection.ensureIndex({ type: "persistent", fields: ["value"] });
      assertEqual(1, idx.selectivityEstimate);

      docs = [];
      for (let i = 0; i < 1000; ++i) {
        docs.push({ value: i });
      }
      collection.insert(docs);

      waitForEstimatorSync(); // make sure estimates are consistent
      idx = collection.ensureIndex({ type: "persistent", fields: ["value"] });
      assertTrue(idx.selectivityEstimate >= 0.45 && idx.selectivityEstimate <= 0.55);

      docs = [];
      for (let i = 0; i < 1000; ++i) {
        docs.push({ value: i });
      }
      collection.insert(docs);

      waitForEstimatorSync(); // make sure estimates are consistent
      idx = collection.ensureIndex({ type: "persistent", fields: ["value"] });
      assertTrue(idx.selectivityEstimate >= 0.3 && idx.selectivityEstimate <= 0.36);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief multi persistent index selectivity
////////////////////////////////////////////////////////////////////////////////

    testSelectivityEstimateAllIdentical : function () {
      let docs = [];
      let idx = collection.ensureIndex({ type: "persistent", fields: ["value"] });
      for (let i = 0; i < 1000; ++i) {
        docs.push({ value: 1 });
      }
      collection.insert(docs);

      waitForEstimatorSync(); // make sure estimates are consistent
      idx = collection.ensureIndex({ type: "persistent", fields: ["value"] });
      assertTrue(idx.selectivityEstimate <= ((1 / 1000) + 0.0001));

      docs = [];
      for (let i = 0; i < 1000; ++i) {
        docs.push({ value: 1 });
      }
      collection.insert(docs);

      waitForEstimatorSync(); // make sure estimates are consistent
      idx = collection.ensureIndex({ type: "persistent", fields: ["value"] });
      assertTrue(idx.selectivityEstimate <= ((2 / 2000) + 0.0001));

      docs = [];
      for (let i = 0; i < 1000; ++i) {
        docs.push({ value: 1 });
      }
      collection.insert(docs);

      waitForEstimatorSync(); // make sure estimates are consistent
      idx = collection.ensureIndex({ type: "persistent", fields: ["value"] });
      assertTrue(idx.selectivityEstimate <= ((2 / 3000) + 0.0001));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief Validate that selectivity estimate is not modified if the transaction
///        is aborted.
////////////////////////////////////////////////////////////////////////////////

    testSelectivityAfterCancelation : function () {
      let idx = collection.ensureIndex({ type: "persistent", fields: ["value"] });
      let docs = [];
      for (let i = 0; i < 1000; ++i) {
        docs.push({value: i % 100});
      }
      collection.insert(docs);
      waitForEstimatorSync(); // make sure estimates are consistent
      idx = collection.ensureIndex({ type: "persistent", fields: ["value"] });

      assertEqual(idx.selectivityEstimate, 100 / 1000);
      try {
        internal.db._executeTransaction({
          collections: {write: cn},
          action: function () {
            const cn = "UnitTestsCollection";
            let docs = [];
            for (let i = 0; i < 1000; ++i) {
              docs.push({value: 1});
            }
            // This should significantly modify the estimate
            // if successful
            require('@arangodb').db[cn].insert(docs);
            throw "banana";
          }
        });
        fail();
      } catch (e) {
        assertEqual(e.errorMessage, "banana");
        // Insert failed.
        // Validate that estimate is non modified
        waitForEstimatorSync(); // make sure estimates are consistent
        idx = collection.ensureIndex({ type: "persistent", fields: ["value"] });
        assertEqual(idx.selectivityEstimate, 100 / 1000);
      }
    },

  };
}

jsunity.run(SelectivityIndexSuite);

return jsunity.done();
