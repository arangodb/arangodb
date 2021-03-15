/*jshint globalstrict:false, strict:false */
/* global getOptions, assertEqual, arango */

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
    'server.export-read-write-metrics': "false",
  };
}

const jsunity = require('jsunity');

function testSuite() {
  return {
    testMetricsUnavailable : function() {
      let res = arango.GET_RAW(`/_admin/metrics`);

      let lines = res.body.split(/\n/);
      assertEqual(0, lines.filter((line) => line.match(/^arangodb_document_read_time/) ).length);
      assertEqual(0, lines.filter((line) => line.match(/^arangodb_document_insert_time/) ).length);
      assertEqual(0, lines.filter((line) => line.match(/^arangodb_document_replace_time/) ).length);
      assertEqual(0, lines.filter((line) => line.match(/^arangodb_document_remove_time/) ).length);
      assertEqual(0, lines.filter((line) => line.match(/^arangodb_collection_truncate_time/) ).length);
    },
    
  };
}

jsunity.run(testSuite);
return jsunity.done();
