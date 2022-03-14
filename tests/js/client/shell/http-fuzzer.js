/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertTrue, assertEqual, arango*/

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// @author Julia Puget
////////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const internal = require('internal');

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////
function httpRequestsFuzzerTestSuite() {
  return {
////////////////////////////////////////////////////////////////////////////////
/// @brief CoordinatorMetricsTestSuite tests
////////////////////////////////////////////////////////////////////////////////
    testRandReqs: function () {
      for (let i = 0; i < 15; ++i) {
        let response;
        response = arango.fuzzRequests(200000, i);
        assertTrue(response.hasOwnProperty("seed"));
        assertTrue(response.hasOwnProperty("total-requests"));
        let numReqs = response["total-requests"];
        let tempSum = 0;
        for (const [key, value] of Object.entries(response)) {
          if (key !== "total-requests" && key !== "seed") {
            if (key === "return-codes") {
              for (const [, innerValue] of Object.entries(response[key])) {
                tempSum += innerValue;
              }
            } else {
              tempSum += value;
            }
          }
        }
        assertEqual(numReqs, tempSum);
      }
    },

    testReqWithSameSeed: function () {
      let response = arango.fuzzRequests(1, 10);
      assertTrue(response.hasOwnProperty("seed"));
      let seed = response["seed"];
      assertTrue(response.hasOwnProperty("total-requests"));
      let numReqs = response["total-requests"];
      let tempSum = 0;
      let keysAndValues1 = new Map();
      for (const [key, value] of Object.entries(response)) {
        if (key !== "total-requests" && key !== "seed") {
          if (key === "return-codes") {
            for (const [innerKey, innerValue] of Object.entries(response[key])) {
              keysAndValues1.set(innerKey, innerValue);
              tempSum += innerValue;
            }
          } else {
            tempSum += value;
            keysAndValues1.set(key, value);
          }
        }
      }
      assertEqual(numReqs, tempSum);

      let newResponse = arango.fuzzRequests(1, 10, seed);
      assertEqual(response, newResponse);
      assertTrue(response.hasOwnProperty("seed"));
      assertTrue(response.hasOwnProperty("total-requests"));
      let numReqs2 = response["total-requests"];
      let tempSum2 = 0;
      for (const [key, value] of Object.entries(response)) {
        if (key !== "total-requests" && key !== "seed") {
          if (key === "return-codes") {
            for (const [innerKey, innerValue] of Object.entries(response[key])) {
              tempSum2 += innerValue;
              assertEqual(keysAndValues1.get(innerKey), innerValue);
            }
          } else {
            tempSum2 += value;
            assertEqual(keysAndValues1.get(key), value);
          }
        }
      }
      assertEqual(numReqs, numReqs2);
      assertEqual(tempSum, tempSum2);
      assertEqual(numReqs2, tempSum2);
    },

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

if (internal.debugCanUseFailAt()) {
  jsunity.run(httpRequestsFuzzerTestSuite);
}

return jsunity.done();
