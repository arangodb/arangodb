/*jshint globalstrict:false, strict:false, maxlen : 4000 */
/* global arango, assertTrue, assertEqual */

////////////////////////////////////////////////////////////////////////////////
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

'use strict';
const jsunity = require('jsunity');
const db = require("@arangodb").db;
const { getMetricSingle } = require("@arangodb/test-helper");

const cn = "UnitTestsCollection";
  
function MetricsSuite () {
  'use strict';
      
  return {
    setUp: function () {
      let docs = [];
      for (let i = 0; i < 5000; ++i) {
        docs.push({ value1: "testmann" + i, value2: i, value3: "testsalat" + i, value4: i });
      }
      
      let c = db._create(cn);
      while (getMetricSingle("rocksdb_total_sst_files") <= 3) {
        c.insert(docs);
      }
    },

    tearDown: function () {
      db._drop(cn);
    },

    testCompareTotalNumberToNumberOfFilesOnLevels: function () {
      let res = arango.GET_RAW("/_admin/metrics");
      assertEqual(200, res.code);

      const extract = (text, name) => {
        let re = new RegExp("^" + name);
        let matches = text.split('\n').filter((line) => !line.match(/^#/)).filter((line) => line.match(re));
        if (!matches.length) {
          throw "Metric " + name + " not found";
        }
        return Number(matches[0].replace(/^.*?\s*([0-9.]+)$/, "$1"));
      };

      let total = 0;
      for (let i = 0; i < 7; ++i) {
        total += extract(res.body, "rocksdb_num_files_at_level" + i);
      }

      assertTrue(total > 0, total);

      assertEqual(total, extract(res.body, "rocksdb_total_sst_files"));
    },

  };
}

jsunity.run(MetricsSuite);
return jsunity.done();
