/*jshint globalstrict:false, strict:false */
/* global getOptions, assertNotEqual, assertTrue, arango */

////////////////////////////////////////////////////////////////////////////////
/// @brief test metrics options
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
/// Copyright holder is ArangoDB Inc, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2019, ArangoDB Inc, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

if (getOptions === true) {
  return {
    'server.export-read-write-metrics': "true",
  };
}

const jsunity = require('jsunity');

function testSuite() {
  let getMetrics = function() {
    let res = arango.GET_RAW(`/_admin/metrics`);
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
      let oldInserts = parseInt(lines.filter((line) => line.match(/^arangodb_document_insert_time_count/))[0].split(" ")[1]);

      let cn = "UnitTestsReplication";
      db._drop(cn);
      let c = db._create(cn);
      try {
        for (let i = 0; i < 1000; ++i) {
          c.insert({});
        }
        // fetch updated metrics
        lines = getMetrics();
        let newInserts = parseInt(lines.filter((line) => line.match(/^arangodb_document_insert_time_count/))[0].split(" ")[1]);
        assertTrue(newInserts >= oldInserts + 1000, { oldInserts, newInserts });
      } finally {
        db._drop(cn);
      }
    },
    
  };
}

jsunity.run(testSuite);
return jsunity.done();
