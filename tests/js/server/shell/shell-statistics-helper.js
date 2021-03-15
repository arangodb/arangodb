/*jshint globalstrict:false, strict:false, maxlen: 5000 */
/*global fail, assertTrue, assertFalse, assertEqual, assertNotEqual, assertTypeOf */

////////////////////////////////////////////////////////////////////////////////
/// @brief test statistics helper interface
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Manuel Pöter
/// @author Copyright 2020, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

'use strict';

let jsunity = require("jsunity");
let helper = require("@arangodb/statistics-helper");

function StatisticsHelpers () {
  return {
    testMergeStatisticsSamples : function () {
      const samples = [{
        time: 0.09,
        clusterId: "c1",
        physicalMemory: 1,
        residentSizeCurrent: 1,
        clientConnectionsCurrent: 1,
        avgRequestTime: 1,
        bytesSentPerSecond: 1,
        bytesReceivedPerSecond: 1,
        http: {
          optionsPerSecond: 1,
          putsPerSecond: 1,
          headsPerSecond: 1,
          postsPerSecond: 1,
          getsPerSecond: 1,
          deletesPerSecond: 1,
          othersPerSecond: 1,
          patchesPerSecond: 1
        }
      }, {
        time: 10.2,
        clusterId: "c1",
        physicalMemory: 1,
        residentSizeCurrent: 1,
        clientConnectionsCurrent: 10,
        avgRequestTime: 2,
        bytesSentPerSecond: 1,
        bytesReceivedPerSecond: 1,
        http: {
          optionsPerSecond: 1,
          putsPerSecond: 1,
          headsPerSecond: 1,
          postsPerSecond: 1,
          getsPerSecond: 1,
          deletesPerSecond: 1,
          othersPerSecond: 1,
          patchesPerSecond: 1
        }
      }, {
        time: 29.9,
        clusterId: "c1",
        physicalMemory: 1,
        residentSizeCurrent: 2,
        clientConnectionsCurrent: 7,
        avgRequestTime: 2,
        bytesSentPerSecond: 1,
        bytesReceivedPerSecond: 1,
        http: {
          optionsPerSecond: 1,
          putsPerSecond: 1,
          headsPerSecond: 1,
          postsPerSecond: 1,
          getsPerSecond: 1,
          deletesPerSecond: 1,
          othersPerSecond: 1,
          patchesPerSecond: 1
        }
      }, {
        time: 9.9,
        clusterId: "c2",
        physicalMemory: 1,
        residentSizeCurrent: 1,
        clientConnectionsCurrent: 1,
        avgRequestTime: 2,
        bytesSentPerSecond: 1,
        bytesReceivedPerSecond: 1,
        http: {
          optionsPerSecond: 1,
          putsPerSecond: 1,
          headsPerSecond: 1,
          postsPerSecond: 1,
          getsPerSecond: 1,
          deletesPerSecond: 1,
          othersPerSecond: 1,
          patchesPerSecond: 1
        }
      }, {
        time: 19.7,
        clusterId: "c2",
        physicalMemory: 1,
        residentSizeCurrent: 3,
        clientConnectionsCurrent: 0,
        avgRequestTime: 1,
        bytesSentPerSecond: 1,
        bytesReceivedPerSecond: 1,
        http: {
          optionsPerSecond: 1,
          putsPerSecond: 1,
          headsPerSecond: 1,
          postsPerSecond: 1,
          getsPerSecond: 1,
          deletesPerSecond: 1,
          othersPerSecond: 1,
          patchesPerSecond: 1
        }
      }, {
        time: 15.0,
        clusterId: "c3",
        physicalMemory: 2,
        residentSizeCurrent: 2,
        clientConnectionsCurrent: 2,
        avgRequestTime: 3,
        bytesSentPerSecond: 1,
        bytesReceivedPerSecond: 2,
        http: {
          optionsPerSecond: 2,
          putsPerSecond: 2,
          headsPerSecond: 2,
          postsPerSecond: 2,
          getsPerSecond: 2,
          deletesPerSecond: 2,
          othersPerSecond: 5,
          patchesPerSecond: 0
        }
      }];
      const merged = helper.MergeStatisticSamples(samples);

      const expected = {
        physicalMemory: 4,
        residentSizeCurrent: 7,
        clientConnectionsCurrent: 9,
        times: [0, 10, 20],
        avgRequestTime: [1.5, 2, 2],
        bytesSentPerSecond: [2, 3, 1],
        bytesReceivedPerSecond: [2, 4, 1],
        http: {
          optionsPerSecond: [2, 4, 1],
          putsPerSecond: [2, 4, 1],
          headsPerSecond: [2, 4, 1],
          postsPerSecond: [2, 4, 1],
          getsPerSecond: [2, 4, 1],
          deletesPerSecond: [2, 4, 1],
          othersPerSecond: [2, 7, 1],
          patchesPerSecond: [2, 2, 1],
        }
      };
      assertEqual(expected, merged);
    }
  };
}

jsunity.run(StatisticsHelpers);

return jsunity.done();
