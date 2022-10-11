/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for index usage
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
const db = require("@arangodb").db;

function optimizerQueryStatsTestSuite () {
  let c;

  return {
    setUp : function () {
      db._drop("UnitTestsCollection");
      // intentionally single-shard only to test statistics here (much easier with one shard)
      c = db._create("UnitTestsCollection");
    },

    tearDown : function () {
      db._drop("UnitTestsCollection");
    },

    testFullScanEmpty : function () {
      let stats = db._query("FOR doc IN " + c.name() + " RETURN doc").getExtra().stats;

      assertEqual(0, stats.filtered);
      assertEqual(0, stats.scannedFull);
      assertEqual(0, stats.scannedIndex);
    },

    testFullScanNonEmpty : function () {
      for (let i = 0; i < 1000; ++i) {
        c.insert({ value: i });
      }
      let stats = db._query("FOR doc IN " + c.name() + " RETURN doc").getExtra().stats;

      assertEqual(0, stats.filtered);
      assertEqual(1000, stats.scannedFull);
      assertEqual(0, stats.scannedIndex);
    },

    testFullScanSkipped : function () {
      for (let i = 0; i < 1000; ++i) {
        c.insert({ value: i });
      }
      let stats = db._query("FOR doc IN " + c.name() + " LIMIT 500, 1000 RETURN doc").getExtra().stats;

      assertEqual(0, stats.filtered);
      assertEqual(1000, stats.scannedFull);
      assertEqual(0, stats.scannedIndex);
    },

    testFullScanLimited : function () {
      for (let i = 0; i < 1000; ++i) {
        c.insert({ value: i });
      }
      let stats = db._query("FOR doc IN " + c.name() + " LIMIT 0, 100 RETURN doc").getExtra().stats;

      assertEqual(0, stats.filtered);
      assertEqual(100, stats.scannedFull);
      assertEqual(0, stats.scannedIndex);
    },

    testFullScanFiltered : function () {
      for (let i = 0; i < 1000; ++i) {
        c.insert({ value: i % 10 });
      }
      let stats = db._query("FOR doc IN " + c.name() + " FILTER doc.value == 3 RETURN doc").getExtra().stats;

      assertEqual(900, stats.filtered);
      assertEqual(1000, stats.scannedFull);
      assertEqual(0, stats.scannedIndex);
    },

    testFullScanFilteredSkipped : function () {
      for (let i = 0; i < 1000; ++i) {
        c.insert({ value: i % 10 });
      }
      let stats = db._query("FOR doc IN " + c.name() + " FILTER doc.value == 3 LIMIT 500, 1000 RETURN doc").getExtra().stats;

      assertEqual(900, stats.filtered);
      assertEqual(1000, stats.scannedFull);
      assertEqual(0, stats.scannedIndex);
    },

    testFullScanFilteredLimited : function () {
      for (let i = 0; i < 1000; ++i) {
        c.insert({ value: i % 10 });
      }
      let stats = db._query("FOR doc IN " + c.name() + " FILTER doc.value == 3 LIMIT 0, 10 RETURN doc",{},{fullCount: true}).getExtra().stats;

      assertEqual(900, stats.filtered);
      assertEqual(1000, stats.scannedFull);
      assertEqual(0, stats.scannedIndex);
    },

    testIndexScan : function () {
      for (let i = 0; i < 1000; ++i) {
        c.insert({ value: i % 10 });
      }
      c.ensureIndex({ type: "hash", fields: ["value"] });
      let stats = db._query("FOR doc IN " + c.name() + " FILTER doc.value == 3 RETURN doc").getExtra().stats;

      assertEqual(0, stats.filtered);
      assertEqual(0, stats.scannedFull);
      assertEqual(100, stats.scannedIndex);
    },

    testIndexScanFiltered : function () {
      for (let i = 0; i < 1000; ++i) {
        c.insert({ value: i % 10 });
      }
      c.ensureIndex({ type: "hash", fields: ["value"] });
      let stats = db._query("FOR doc IN " + c.name() + " FILTER doc.value == 3 && doc._key != 'peng' RETURN doc").getExtra().stats;

      assertEqual(0, stats.filtered);
      assertEqual(0, stats.scannedFull);
      assertEqual(100, stats.scannedIndex);
    },
  };
}

jsunity.run(optimizerQueryStatsTestSuite);

return jsunity.done();
