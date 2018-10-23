/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertTrue, fail */

////////////////////////////////////////////////////////////////////////////////
/// @brief test the hash index, selectivity estimates
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
var internal = require("internal");


////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: Creation
////////////////////////////////////////////////////////////////////////////////

function HashIndexSuite() {
  'use strict';
  var cn = "UnitTestsCollectionHash";
  var collection = null;

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      internal.db._drop(cn);
      collection = internal.db._create(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      // try...catch is necessary as some tests delete the collection itself!
      try {
        collection.unload();
        collection.drop();
      }
      catch (err) {
      }

      collection = null;
      internal.wait(0.0);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief hash index selectivity
////////////////////////////////////////////////////////////////////////////////

    testSelectivityEstimateUnique : function () {
      var i;

      var idx = collection.ensureUniqueConstraint("value");
      for (i = 0; i < 1000; ++i) {
        collection.save({ _key: "test" + i, value: i });
      }

      internal.waitForEstimatorSync(); // make sure estimates are consistent
      idx = collection.ensureUniqueConstraint("value");
      assertEqual(1, idx.selectivityEstimate);

      for (i = 0; i < 50; ++i) {
        collection.remove("test" + i);
      }

      internal.waitForEstimatorSync(); // make sure estimates are consistent
      idx = collection.ensureUniqueConstraint("value");
      assertEqual(1, idx.selectivityEstimate);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief multi hash index selectivity
////////////////////////////////////////////////////////////////////////////////

    testSelectivityEstimateNonUnique : function () {
      var i;

      var idx = collection.ensureHashIndex("value");
      for (i = 0; i < 1000; ++i) {
        collection.save({ value: i });
      }

      internal.waitForEstimatorSync(); // make sure estimates are consistent
      idx = collection.ensureHashIndex("value");
      assertEqual(1, idx.selectivityEstimate);

      for (i = 0; i < 1000; ++i) {
        collection.save({ value: i });
      }

      internal.waitForEstimatorSync(); // make sure estimates are consistent
      idx = collection.ensureHashIndex("value");
      assertTrue(idx.selectivityEstimate >= 0.45 && idx.selectivityEstimate <= 0.55);

      for (i = 0; i < 1000; ++i) {
        collection.save({ value: i });
      }

      internal.waitForEstimatorSync(); // make sure estimates are consistent
      idx = collection.ensureHashIndex("value");
      assertTrue(idx.selectivityEstimate >= 0.3 && idx.selectivityEstimate <= 0.36);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief multi hash index selectivity
////////////////////////////////////////////////////////////////////////////////

    testSelectivityEstimateAllIdentical : function () {
      var i;

      var idx = collection.ensureHashIndex("value");
      for (i = 0; i < 1000; ++i) {
        collection.save({ value: 1 });
      }

      internal.waitForEstimatorSync(); // make sure estimates are consistent
      idx = collection.ensureHashIndex("value");
      assertTrue(idx.selectivityEstimate <= ((1 / 1000) + 0.0001));

      for (i = 0; i < 1000; ++i) {
        collection.save({ value: 1 });
      }

      internal.waitForEstimatorSync(); // make sure estimates are consistent
      idx = collection.ensureHashIndex("value");
      assertTrue(idx.selectivityEstimate <= ((2 / 2000) + 0.0001));

      for (i = 0; i < 1000; ++i) {
        collection.save({ value: 1 });
      }

      internal.waitForEstimatorSync(); // make sure estimates are consistent
      idx = collection.ensureHashIndex("value");
      assertTrue(idx.selectivityEstimate <= ((2 / 3000) + 0.0001));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief Validate that selectivity estimate is not modified if the transaction
///        is aborted.
////////////////////////////////////////////////////////////////////////////////

    testSelectivityAfterAbortion : function () {
      let idx = collection.ensureHashIndex("value");
      let docs = [];
      for (let i = 0; i < 1000; ++i) {
        docs.push({value: i % 100});
      }
      collection.save(docs);
      internal.waitForEstimatorSync(); // make sure estimates are consistent
      idx = collection.ensureHashIndex("value");

      assertEqual(idx.selectivityEstimate, 100 / 1000);
      try {
        internal.db._executeTransaction({
          collections: {write: cn},
          action: function () {
            const cn = "UnitTestsCollectionHash";
            let docs = [];
            for (let i = 0; i < 1000; ++i) {
              docs.push({value: 1});
            }
            // This should significantly modify the estimate
            // if successful
            require('@arangodb').db[cn].save(docs);
            throw "banana";
          }
        });
        fail();
      } catch (e) {
        assertEqual(e.errorMessage, "banana");
        // Insert failed.
        // Validate that estimate is non modified
        internal.waitForEstimatorSync(); // make sure estimates are consistent
        idx = collection.ensureHashIndex("value");
        assertEqual(idx.selectivityEstimate, 100 / 1000);
      }
    },

  };
}


////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

jsunity.run(HashIndexSuite);

return jsunity.done();
