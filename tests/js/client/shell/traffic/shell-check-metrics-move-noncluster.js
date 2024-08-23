/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global assertEqual, assertTrue, assertNotEqual, arango */

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
// / @author Max Neunhoeffer
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require('jsunity');
const getMetric = require('@arangodb/test-helper').getMetricSingle;
const console = require('console');
const db = require('internal').db;

function getStats() {
  let s = arango.GET("/_admin/statistics?sync=true");
  return {bytesReceived: s.client.bytesReceived.sum,
          bytesSent: s.client.bytesSent.sum};
}

function checkMetricsMoveSuite() {
  'use strict';
  let compressTransfer;

  return {
    setUpAll: function () {
      // fetch compress transfer value
      compressTransfer = arango.compressTransfer();
      // turn off request/response compression
      arango.compressTransfer(false);
    },
    
    tearDownAll: function () {
      // restore previous default
      arango.compressTransfer(compressTransfer);
    },
    
    testByProtocol: function () {
      // Note that this test is "-noncluster", since in the cluster
      // all sorts of requests constantly arrive and we can never be
      // sure that nothing happens. In a single server, this is OK, 
      // only our own requests should arrive at the server.
      let http1ReqCount = getMetric("arangodb_request_body_size_http1_count");
      let http1ReqSum = getMetric("arangodb_request_body_size_http1_sum");
      let http2ReqCount = getMetric("arangodb_request_body_size_http2_count");
      let http2ReqSum = getMetric("arangodb_request_body_size_http2_sum");
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

    testEgressByProtocolTraffic: function () {
      // Note that this test is "-noncluster", since in the cluster
      // all sorts of requests constantly arrive and we can never be
      // sure that nothing happens. In a single server, this is OK, 
      // only our own requests should arrive at the server.
      const cn = "UnitTestsCollection";
      let c = db._create(cn);
      try {
        let s = "abc";
        for (let i = 0; i < 10; ++i) { 
          s = s + s; 
        }
        let k = c.insert({Hallo:s})._key;

        let stats = getStats();
        let count = getMetric("arangodb_client_connection_statistics_bytes_sent_count");
        let metric = getMetric("arangodb_client_connection_statistics_bytes_sent_sum");
        let metricUser = getMetric("arangodb_client_user_connection_statistics_bytes_sent_sum");

        // Do a few requests:
        for (let i = 0; i < 1000; ++i) {
          let res = arango.GET_RAW(`/_api/document/${cn}/${k}`);
          assertEqual(200, res.code);
        }

        let stats2 = getStats();
        let count2 = getMetric("arangodb_client_connection_statistics_bytes_sent_count");
        let metric2 = getMetric("arangodb_client_connection_statistics_bytes_sent_sum");
        let metric2User = getMetric("arangodb_client_user_connection_statistics_bytes_sent_sum");

        // Why 3000000? We have read 1000 docs, and the response body
        // contains a string of 3 * 1024 bytes, so 3000000 is a solid lower limit.
        console.info("Received:", stats2.bytesReceived - stats.bytesReceived, "Sent:", stats2.bytesSent - stats.bytesSent, stats.bytesReceived, stats2.bytesReceived, stats.bytesSent, stats2.bytesSent);
        assertTrue(stats2.bytesSent - stats.bytesSent > 3000000, { stats, stats2, diff: stats2.bytesSent - stats.bytesSent });
        assertTrue(stats2.bytesSent - stats.bytesSent < 2 * 3000000, { stats, stats2, diff: stats2.bytesSent - stats.bytesSent });
        assertTrue(metric2 - metric > 3000000, { metric, metric2, diff: metric2 - metric });
        assertTrue(metric2User - metricUser > 3000000, { metricUser, metric2User, diff: metric2User - metricUser });
        assertTrue(count2 - count >= 1000, { count, count2 });
      } finally {
        db._drop(cn);
      }
    },
    
    testIngressByProtocolTraffic: function () {
      // Note that this test is "-noncluster", since in the cluster
      // all sorts of requests constantly arrive and we can never be
      // sure that nothing happens. In a single server, this is OK, 
      // only our own requests should arrive at the server.
      const cn = "UnitTestsCollection";
      let c = db._create(cn);
      try {
        let s = "abc";
        for (let i = 0; i < 10; ++i) { 
          s = s + s; 
        }

        let stats = getStats();
        let count = getMetric("arangodb_client_connection_statistics_bytes_received_count");
        let metric = getMetric("arangodb_client_connection_statistics_bytes_received_sum");
        let metricUser = getMetric("arangodb_client_user_connection_statistics_bytes_received_sum");

        // Do a few requests:
        for (let i = 0; i < 1000; ++i) {
          let res = arango.POST_RAW(`/_api/document/${cn}`, {Hallo:i, s:s});
          assertEqual(202, res.code);
        }

        let stats2 = getStats();
        let count2 = getMetric("arangodb_client_connection_statistics_bytes_received_count");
        let metric2 = getMetric("arangodb_client_connection_statistics_bytes_received_sum");
        let metric2User = getMetric("arangodb_client_user_connection_statistics_bytes_received_sum");

        // Why 3000000? We have written 1000 docs, and the request body
        // contains a string of 3 * 1024 bytes, so 3000000 is a solid lower limit.
        console.info("Received:", stats2.bytesReceived - stats.bytesReceived, "Sent:", stats2.bytesSent - stats.bytesSent, stats.bytesReceived, stats2.bytesReceived, stats.bytesSent, stats2.bytesSent);
        assertTrue(stats2.bytesReceived - stats.bytesReceived > 3000000, { stats, stats2, diff: stats2.bytesReceived - stats.bytesReceived });
        assertTrue(stats2.bytesReceived - stats.bytesReceived < 2 * 3000000, { stats, stats2, diff: stats2.bytesReceived - stats.bytesReceived });
        assertTrue(metric2 - metric > 3000000, { metric, metric2, diff: metric2 - metric });
        assertTrue(metric2User - metricUser > 3000000, { metricUser, metric2User, diff: metric2User - metricUser });
        assertTrue(count2 - count >= 1000, { count, count2 });
      } finally {
        db._drop(cn);
      }
    },
    
    testIngressHeadersByProtocolTraffic: function () {
      // Note that this test is "-noncluster", since in the cluster
      // all sorts of requests constantly arrive and we can never be
      // sure that nothing happens. In a single server, this is OK, 
      // only our own requests should arrive at the server.
      let stats = getStats();

      // Do a few requests:
      const headers = {
        "x-arango-foo-bar-baz-this-is-a-long-and-useless-header" : Array(1000).join("x"),
        "x-arango-foo-bar-baz-this-is-another-long-and-useless-header" : Array(1000).join("y"),
        "x-arango-foo-bar-baz-this-is-a-third-long-and-useless-header" : Array(1000).join("z")
      };

      for (let i = 0; i < 1000; ++i) {
        let res = arango.GET_RAW(`/_api/version`, headers);
        assertEqual(200, res.code);
      }

      let stats2 = getStats();

      // Why 3000000? We have sent 1000 requests, and the header size per request is 
      // > 3000 bytes, so 3000000 is a solid lower limit.
      console.info("Received:", stats2.bytesReceived - stats.bytesReceived, "Sent:", stats2.bytesSent - stats.bytesSent, stats.bytesReceived, stats2.bytesReceived, stats.bytesSent, stats2.bytesSent);
      assertTrue(stats2.bytesReceived - stats.bytesReceived > 3000000, { stats, stats2, diff: stats2.bytesReceived - stats.bytesReceived });
      assertTrue(stats2.bytesReceived - stats.bytesReceived < 2 * 3000000, { stats, stats2, diff: stats2.bytesReceived - stats.bytesReceived });
    },

    testConnectionCountByProtocol: function () {
      // I have not found a reliable way to trigger a new connection from
      // `arangosh`. `arango.reconnect` does not work since it is caching
      // connections and the request timeout is not honored for HTTP/2
      // by fuerte. The idle timeout runs into an assertion failure.
      // Therefore, we must be content here to check that the connection
      // count is non-zero for the currently underlying protocol:
      if (arango.protocol() === "http2") {
        let http2ConnCount = getMetric("arangodb_http2_connections_total");
        assertNotEqual(0, http2ConnCount);
      } else if (arango.protocol() === "http") {
        let httpConnCount = getMetric("arangodb_http1_connections_total");
        assertNotEqual(0, httpConnCount);
      }
    },
  };
}

jsunity.run(checkMetricsMoveSuite);
return jsunity.done();
