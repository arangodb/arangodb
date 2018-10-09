/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertTrue, fail */

////////////////////////////////////////////////////////////////////////////////
/// @brief test the skiplist index, selectivity estimates
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
/// @author Copyright 2018, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////


var jsunity = require("jsunity");
var internal = require("internal");

function SkiplistIndexSuite() {
  'use strict';
  const cn = "UnitTestsCollectionSkip";
  let collection = null;

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
      collection.drop();
      collection = null;
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief Validate that selectivity estimate is not modified if the transaction
///        is aborted.
////////////////////////////////////////////////////////////////////////////////

    testSelectivityAfterAbortion : function () {
      let idx = collection.ensureSkiplist("value");
      let docs = [];
      for (let i = 0; i < 1000; ++i) {
        docs.push({value: i % 100});
      }
      collection.save(docs);
      idx = collection.ensureSkiplist("value");
      internal.waitForEstimatorSync(); // make sure estimates are consistent
      idx = collection.ensureSkiplist("value"); // fetch new estimates
      let oldEstimate = idx.selectivityEstimate;

      assertTrue(oldEstimate >= 0);
      assertTrue(oldEstimate <= 1);
      try {
        internal.db._executeTransaction({
          collections: {write: cn},
          action: function () {
            const cn = "UnitTestsCollectionSkip";
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
        // Validate that estimate is non-modified
        idx = collection.ensureSkiplist("value");
        internal.waitForEstimatorSync(); // make sure estimates are consistent
        assertEqual(idx.selectivityEstimate, oldEstimate);
      }
    },

  };

}

jsunity.run(SkiplistIndexSuite);

return jsunity.done();
