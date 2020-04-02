/* jshint globalstrict:false, strict:false */
/* global assertEqual, assertTrue, assertFalse, assertNotNull, fail */

// //////////////////////////////////////////////////////////////////////////////
// / @brief test the document interface
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2010-2012 triagens GmbH, Cologne, Germany
// /
// / Licensed under the Apache License, Version 2.0 (the "License");
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     http://www.apache.org/licenses/LICENSE-2.0
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is triAGENS GmbH, Cologne, Germany
// /
// / @author Dr. Frank Celler
// / @author Copyright 2012, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

var jsunity = require('jsunity');
var arangodb = require('@arangodb');
var db = arangodb.db;
var internal = require('internal');
var wait = internal.wait;
var ArangoCollection = arangodb.ArangoCollection;

function EdgeIndexSuite () {
  var vn = 'UnitTestsCollectionVertex';
  var vertex = null;

  var en = 'UnitTestsCollectionEdge';
  var edge = null;

  var v1 = null;
  var v2 = null;

  return {

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief set up
    // //////////////////////////////////////////////////////////////////////////////

    setUp: function () {
      db._drop(en);
      edge = db._createEdgeCollection(en);

      db._drop(vn);
      vertex = db._create(vn);

      v1 = vertex.save({ a: 1 });
      v2 = vertex.save({ a: 2 });
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief tear down
    // //////////////////////////////////////////////////////////////////////////////

    tearDown: function () {
      edge.drop();
      vertex.drop();
      edge = null;
      vertex = null;
      wait(0.0);
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test index selectivity
    // //////////////////////////////////////////////////////////////////////////////

    testIndexSelectivityEmpty: function () {
      internal.waitForEstimatorSync(); // make sure estimates are consistent
      var edgeIndex = edge.getIndexes()[1];
      assertTrue(edgeIndex.hasOwnProperty('selectivityEstimate'));
      assertEqual(1, edgeIndex.selectivityEstimate);
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test index selectivity
    // //////////////////////////////////////////////////////////////////////////////

    testIndexSelectivityOneDoc: function () {
      edge.save(v1, v2, { });
      internal.waitForEstimatorSync(); // make sure estimates are consistent
      var edgeIndex = edge.getIndexes()[1];
      assertTrue(edgeIndex.hasOwnProperty('selectivityEstimate'));
      assertEqual(1, edgeIndex.selectivityEstimate);
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test index selectivity
    // //////////////////////////////////////////////////////////////////////////////

    testIndexSelectivityDuplicateDocs: function () {
      var i, c, edgeIndex, expectedSelectivity;

      for (i = 0; i < 1000; ++i) {
        edge.save(v1, v2, { });
      }
      internal.waitForEstimatorSync(); // make sure estimates are consistent
      edgeIndex = edge.getIndexes()[1];
      expectedSelectivity = 1 / 1000;
      // allow for some floating-point deviations
      assertTrue(Math.abs(expectedSelectivity - edgeIndex.selectivityEstimate) <= 0.001);

      var n = edge.count();
      assertEqual(1000, n);

      for (i = 0; i < n; ++i) {
        var doc = edge.any();
        assertNotNull(doc);
        edge.remove(doc._key);
      }

      internal.waitForEstimatorSync(); // make sure estimates are consistent
      edgeIndex = edge.getIndexes()[1];
      expectedSelectivity = 1.0;
      // allow for some floating-point deviations
      assertTrue(Math.abs(expectedSelectivity - edgeIndex.selectivityEstimate) <= 0.001);
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test index selectivity
    // //////////////////////////////////////////////////////////////////////////////

    testIndexSelectivityUniqueDocs: function () {
      for (var i = 0; i < 1000; ++i) {
        edge.save(vn + '/from' + i, vn + '/to' + i, { });
      }
      internal.waitForEstimatorSync(); // make sure estimates are consistent
      var edgeIndex = edge.getIndexes()[1];
      assertTrue(1, edgeIndex.selectivityEstimate);
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test index selectivity
    // //////////////////////////////////////////////////////////////////////////////

    testIndexSelectivityUniqueDocsFrom: function () {
      for (var i = 0; i < 1000; ++i) {
        edge.save(vn + '/from' + i, vn + '/1', { });
      }
      internal.waitForEstimatorSync(); // make sure estimates are consistent
      var edgeIndex = edge.getIndexes()[1];
      var expectedSelectivity = (1 + (1 / 1000)) * 0.5;
      assertTrue(Math.abs(expectedSelectivity - edgeIndex.selectivityEstimate) <= 0.001);
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test index selectivity
    // //////////////////////////////////////////////////////////////////////////////

    testIndexSelectivityRepeatingDocs: function () {
      for (var i = 0; i < 1000; ++i) {
        edge.save(vn + '/from' + (i % 20), vn + '/to' + i, { });
      }
      internal.waitForEstimatorSync(); // make sure estimates are consistent
      var edgeIndex = edge.getIndexes()[1];
      var expectedSelectivity = (1 + (20 / 1000)) * 0.5;
      assertTrue(Math.abs(expectedSelectivity - edgeIndex.selectivityEstimate) <= 0.001);
    },

    testIndexSelectivityAfterAbortion: function () {
      let docs = [];
      for (let i = 0; i < 1000; ++i) {
        docs.push({_from: `${vn}/from${i % 32}`, _to: `${vn}/to${i % 47}`});
      }
      edge.save(docs);
      internal.waitForEstimatorSync(); // make sure estimates are consistent
      let idx = edge.getIndexes()[1];
      let estimateBefore = idx.selectivityEstimate;
      try {
        internal.db._executeTransaction({
          collections: {write: en},
          action: function () {
            const vn = 'UnitTestsCollectionVertex';
            const en = 'UnitTestsCollectionEdge';
            let docs = [];
            for (let i = 0; i < 1000; ++i) {
              docs.push({_from: `${vn}/from${i % 32}`, _to: `${vn}/to${i % 47}`});
            }
            // This should significantly modify the estimate
            // if successful
            require('@arangodb').db[en].save(docs);
            throw "banana";
          }
        });
        fail();
      } catch (e) {
        assertEqual(e.errorMessage, "banana");
        // Insert failed.
        // Validate that estimate is non modified
        internal.waitForEstimatorSync(); // make sure estimates are consistent
        idx = edge.getIndexes()[1];
        assertEqual(idx.selectivityEstimate, estimateBefore);
      }

    },
  };
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief executes the test suite
// //////////////////////////////////////////////////////////////////////////////

jsunity.run(EdgeIndexSuite);

return jsunity.done();
