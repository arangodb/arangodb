/*jshint globalstrict:false, strict:false */
/* global getOptions, assertNotEqual, assertTrue, arango */

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
/// @author Copyright 2019, ArangoDB Inc, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

if (getOptions === true) {
  return {
    'server.export-read-write-metrics': "true",
  };
}

const jsunity = require('jsunity');

function testSuite() {
  let getMetrics = function() {
    let res = arango.GET_RAW(`/_admin/metrics/v2`);
    let lines = res.body.split(/\n/);
    return lines;
  };

  let db = require('@arangodb').db;

  return {
    testMetricsUnavailable : function() {
      let lines = getMetrics();
      assertNotEqual(0, lines.filter((line) => line.match(/^arangodb_document_read_time/) ).length);
      assertNotEqual(0, lines.filter((line) => line.match(/^arangodb_document_insert_time/) ).length);
      assertNotEqual(0, lines.filter((line) => line.match(/^arangodb_document_replace_time/) ).length);
      assertNotEqual(0, lines.filter((line) => line.match(/^arangodb_document_remove_time/) ).length);
      assertNotEqual(0, lines.filter((line) => line.match(/^arangodb_collection_truncate_time/) ).length);

      // now check that metrics actually work
      let oldInserts = parseInt(lines.filter((line) => line.match(/^arangodb_document_insert_time_count/))[0].split("}")[1]);

      let cn = "UnitTestsReplication";
      db._drop(cn);
      let c = db._create(cn);
      try {
        for (let i = 0; i < 10; ++i) {
          let docs = [];
          for (let j = 0; j < 100; ++j) {
            docs.push({});
          }
          c.insert(docs);
        }
        // fetch updated metrics
        lines = getMetrics();
        let newInserts = parseInt(lines.filter((line) => line.match(/^arangodb_document_insert_time_count/))[0].split("}")[1]);
        assertTrue(newInserts >= oldInserts + 1000, { oldInserts, newInserts });
      } finally {
        db._drop(cn);
      }
    },
    
  };
}

jsunity.run(testSuite);
return jsunity.done();
