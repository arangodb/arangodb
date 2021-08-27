/*jshint globalstrict:false, strict:false */
/* global getOptions, assertEqual, assertTrue, arango */

////////////////////////////////////////////////////////////////////////////////
/// @brief test for security-related server options
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
/// Copyright holder is ArangoDB Inc, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2019, ArangoDB Inc, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

if (getOptions === true) {
  return {
    'server.statistics': "false",
    'server.export-metrics-api': "true",
  };
}

const jsunity = require('jsunity');

function testSuite() {
  return {
    testGetAdminStatistics : function() {
      let res = arango.GET("/_admin/statistics");
      assertTrue(res.error);
      assertEqual(404, res.code);
    },
    
    testGetAdminStatisticsDescription : function() {
      let res = arango.GET("/_admin/statistics-description");
      assertTrue(res.error);
      assertEqual(404, res.code);
    },
    
    testGetMetrics : function() {
      let metrics = {};
      String(arango.GET("/_admin/metrics"))
        .split("\n")
        .filter((line) => line.match(/^[^#]/))
        .filter((line) => line.match(/^arangodb_/))
        .forEach((line) => {
          let name = line.replace(/[ \{].*$/g, '');
          let value = Number(line.replace(/^.+ (\d+)$/, '$1'));
          metrics[name] = value;
        });

      const expected = [
        "arangodb_process_statistics_minor_page_faults",
        "arangodb_process_statistics_major_page_faults",
        "arangodb_process_statistics_user_time",
        "arangodb_process_statistics_system_time",
        "arangodb_process_statistics_number_of_threads",
        "arangodb_process_statistics_resident_set_size",
        "arangodb_process_statistics_resident_set_size_percent",
        "arangodb_process_statistics_virtual_memory_size",
        "arangodb_server_statistics_physical_memory",
        "arangodb_server_statistics_server_uptime",
      ];

      expected.forEach((name) => {
        assertTrue(metrics.hasOwnProperty(name));
        assertEqual("number", typeof metrics[name]);
      });
    },
  };
}

jsunity.run(testSuite);
return jsunity.done();
