/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global assertEqual, assertTrue, assertNotEqual, arango */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2021 ArangoDB GmbH, Cologne, Germany
// /
// / Licensed under the Apache License, Version 2.0 (the "License")
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
// / @author Max Neunhoeffer
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require('jsunity');

function getMetric(name) {
  let res = arango.GET_RAW( '/_admin/metrics/v2');
  if (res.code !== 200) {
    throw "error fetching metric";
  }
  let re = new RegExp("^" + name);
  let matches = res.body.split('\n').filter((line) => !line.match(/^#/)).filter((line) => line.match(re));
  if (!matches.length) {
    throw "Metric " + name + " not found";
  }
  return Number(matches[0].replace(/^.* (\d+)$/, '$1'));
}

function checkMetricsMoveSuite() {
  'use strict';

  return {
    
    testByProtocol: function () {
      // Note that this test is "-noncluster", since in the cluster
      // all sorts of requests constantly arrive and we can never be
      // sure that nothing happens. In a single server, this is OK, 
      // only our own requests should arrive at the server.
      let http1ReqCount = getMetric("arangodb_request_body_size_http1_count");
      let http1ReqSum = getMetric("arangodb_request_body_size_http1_sum");
      let http2ReqCount = getMetric("arangodb_request_body_size_http2_count");
      let http2ReqSum = getMetric("arangodb_request_body_size_http2_sum");
      let vstReqCount = getMetric("arangodb_request_body_size_vst_count");
      let vstReqSum = getMetric("arangodb_request_body_size_vst_sum");
      // Do a few requests:
      for (let i = 0; i < 10; ++i) {
        let res = arango.GET_RAW("/_api/version");
        assertEqual(200, res.code);
      }
      // Note that GET requests do not have bodies, so they do not move
      // the sum of the histogram. So let's do a PUT just for good measure:
      let logging = arango.GET("/_admin/log/level");
      let res = arango.PUT_RAW("/_admin/log/level", logging);
      assertEqual(200, res.code);
      // And get the values again:
      let http1ReqCount2 = getMetric("arangodb_request_body_size_http1_count");
      let http1ReqSum2 = getMetric("arangodb_request_body_size_http1_sum");
      let http2ReqCount2 = getMetric("arangodb_request_body_size_http2_count");
      let http2ReqSum2 = getMetric("arangodb_request_body_size_http2_sum");
      let vstReqCount2 = getMetric("arangodb_request_body_size_vst_count");
      let vstReqSum2 = getMetric("arangodb_request_body_size_vst_sum");
      if (arango.protocol() === "vst") {
        assertNotEqual(vstReqCount, vstReqCount2);
        assertNotEqual(vstReqSum, vstReqSum2);
      } else {
        assertEqual(vstReqCount, vstReqCount2);
        assertEqual(vstReqSum, vstReqSum2);
      }
      if (arango.protocol() === "http") {
        assertNotEqual(http1ReqCount, http1ReqCount2);
        assertNotEqual(http1ReqSum, http1ReqSum2);
      } else {
        assertEqual(http1ReqCount, http1ReqCount2);
        assertEqual(http1ReqSum, http1ReqSum2);
      }
      if (arango.protocol() === "http2") {
        assertNotEqual(http2ReqCount, http2ReqCount2);
        assertNotEqual(http2ReqSum, http2ReqSum2);
      } else {
        assertEqual(http2ReqCount, http2ReqCount2);
        assertEqual(http2ReqSum, http2ReqSum2);
      }
    },

    testConnectionCountByProtocol: function () {
      let http2ConnCount = getMetric("arangodb_http2_connections_total");
      let vstConnCount = getMetric("arangodb_vst_connections_total");
      // I have not found a reliable way to trigger a new connection from
      // `arangosh`. `arango.reconnect` does not work since it is caching
      // connections and the request timeout is not honoured for HTTP/2
      // and VST by fuerte. The idle timeout runs into an assertion failure.
      // Therefore, we must be content here to check that the connection
      // count is non-zero for the currently underlying protocol:
      if (arango.protocol() === "http2") {
        assertNotEqual(0, http2ConnCount);
      } else if (arango.protocol() === "vst") {
        assertNotEqual(0, vstConnCount);
      }
    },
  };
}

jsunity.run(checkMetricsMoveSuite);
return jsunity.done();
