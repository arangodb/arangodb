/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertTrue, assertEqual, arango, print */

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
/// @author Julia Puget
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const internal = require('internal');
const fs = require('fs');
const IM = global.instanceManager;
const ct = require('@arangodb/testutils/client-tools');

const wordListForRoute = [
  "/_db", "/_admin", "/_api", "/_system", "/_cursor", "/version", "/status",
  "/license", "/collection", "/database", "/current", "/log", "/"
];

const wordListForKeys = [
  "Accept",
  "",
  "Accept-Charset",
  "Accept-Encoding",
  "Accept-Language",
  "Accept-Ranges",
  "Allow",
  "Authorization",
  "Cache-control",
  "Connection",
  "Content-encoding",
  "Content-language",
  "Content-location",
  "Content-MD5",
  "Content-range",
  "Content-type",
  "Date",
  "ETag",
  "Expect",
  "Expires",
  "From",
  "Host",
  "If-Match",
  "If-modified-since",
  "If-none-match",
  "If-range",
  "If-unmodified-since",
  "Last-modified",
  "Location",
  "Max-forwards",
  "Pragma",
  "Proxy-authenticate",
  "Proxy-authorization",
  "Range",
  "Referer",
  "Retry-after",
  "Server",
  "TE",
  "Trailer",
  "Transfer-encoding",
  "Upgrade",
  "User-agent",
  "Vary",
  "Via",
  "Warning",
  "Www-authenticate",
  "random"
];

const messages = [
  "creating data",
  "cleaning up"
];

////////////////////////////////////////////////////////////////////////////////
/// @brief Http Request Fuzzer suite
////////////////////////////////////////////////////////////////////////////////
function httpRequestsFuzzerTestSuite() {
  return {
    setUpAll: function () {
      let moreargv = [];
      let logFile = fs.join(fs.getTempPath(), `rta_out_create.log`);
      let rc = ct.run.rtaMakedata(IM.options, IM, 0, messages[0], logFile, moreargv);
      print(rc)
      if (!rc.status) {
        let rx = new RegExp(/\\n/g);
        throw("http_fuzz: failed to create testdatas:\n" + fs.read(logFile).replace(rx, '\n'));
      }

      IM.rememberConnection();
    },
    tearDown: function () {
      let moreargv = [];
      let logFile = fs.join(fs.getTempPath(), `rta_out_clean.log`);
      let rc = ct.run.rtaMakedata(IM.options, IM, 2, messages[1], logFile, moreargv);
      if (!rc.status) {
        let rx = new RegExp(/\\n/g);
        print("http_fuzz: failed to clear testdatas:\n" + fs.read(logFile).replace(rx, '\n'));
      }

      IM.gatherNetstat();
      IM.printNetstat();
    },
    testRandReqs: function () {
      // main expectation here is that the server does not crash!
      try {
        IM.arangods.forEach(arangod => {
          print(`Connecting ${arangod.getProcessInfo([])}`);
          arangod.connect();
          arangod._disconnect();
          IM.gatherNetstat();
          IM.printNetstat();
          for (let i = 0; i < 15; ++i) {
            let response = arango.fuzzRequests(25000, i, wordListForRoute, wordListForKeys);
            assertTrue(response.hasOwnProperty("seed"));
            assertTrue(response.hasOwnProperty("totalRequests"));
            let numReqs = response["totalRequests"];
            let tempSum = 0;
            for (const [key, value] of Object.entries(response)) {
              if (key !== "totalRequests" && key !== "seed") {
                if (key === "returnCodes") {
                  for (const [, innerValue] of Object.entries(response[key])) {
                    tempSum += innerValue;
                  }
                } else {
                  tempSum += value;
                }
              }
            }
            assertEqual(numReqs, tempSum);
            if (IM.options.cluster) {
              IM.reconnectMe();
              IM.checkClusterAlive();
            }
          }
          IM.reconnectMe();
        });
      } finally {
        IM.reconnectMe();
      }
    },

    testReqWithSameSeed: function () {
      // main expectation here is that the server does not crash!
      try {
        IM.arangods.forEach(arangod => {
          print(`Connecting ${arangod.getProcessInfo([])}`);
          arangod.connect();
          arangod._disconnect();
          IM.gatherNetstat();
          IM.printNetstat();
          for (let i = 0; i < 10; ++i) {
            let response = arango.fuzzRequests(1, 10, wordListForRoute, wordListForKeys);
            assertTrue(response.hasOwnProperty("seed"));
            let seed = response["seed"];
            assertTrue(response.hasOwnProperty("totalRequests"));
            let numReqs = response["totalRequests"];
            let tempSum = 0;
            let keysAndValues1 = new Map();
            for (const [key, value] of Object.entries(response)) {
              if (key !== "totalRequests" && key !== "seed") {
                if (key === "returnCodes") {
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

            let newResponse = arango.fuzzRequests(1, 10, wordListForRoute, wordListForKeys, seed);
            assertEqual(response, newResponse);
            assertTrue(response.hasOwnProperty("seed"));
            assertTrue(response.hasOwnProperty("totalRequests"));
            let numReqs2 = response["totalRequests"];
            let tempSum2 = 0;
            for (const [key, value] of Object.entries(response)) {
              if (key !== "totalRequests" && key !== "seed") {
                if (key === "returnCodes") {
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
            if (IM.options.cluster) {
              IM.reconnectMe();
              IM.checkClusterAlive();
            }
          }
          IM.reconnectMe();
        });
      } finally {
        IM.reconnectMe();
      }
    },
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(httpRequestsFuzzerTestSuite);
return jsunity.done();
