/*jslint indent: 2, nomen: true, maxlen: 100, white: true, plusplus: true, unparam: true */
/*global require, applicationContext*/

////////////////////////////////////////////////////////////////////////////////
/// @brief A Foxx.Controller to handle the statistics
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 triagens GmbH, Cologne, Germany
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
/// @author Dr. Frank Celler
/// @author Copyright 2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

"use strict";

var internal = require("internal");
var cluster = require("org/arangodb/cluster");
var actions = require("org/arangodb/actions");
var console = require("console");

var FoxxController = require("org/arangodb/foxx").Controller;
var controller = new FoxxController(applicationContext);
var db = require("org/arangodb").db;

var STATISTICS_INTERVAL = 10;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief avgPerSlice
////////////////////////////////////////////////////////////////////////////////

function avgPerSlice (now, last) {
  var d = now.count - last.count;

  if (d === 0) {
    return 0;
  }

  return (now.sum - last.sum) / d;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief avgPerTimeSlice
////////////////////////////////////////////////////////////////////////////////

function avgPerTimeSlice (duration, now, last) {
  if (duration === 0) {
    return 0;
  }

  return (now.sum - last.sum) / duration;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief perTimeSlice
////////////////////////////////////////////////////////////////////////////////

function perTimeSlice (duration, now, last) {
  if (duration === 0) {
    return 0;
  }

  return (now - last) / duration;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief avgDistributon
////////////////////////////////////////////////////////////////////////////////

function avgPercentDistributon (now, last, cuts) {
  var n = cuts.length + 1;
  var result = new Array(n);
  var count = 0;
  var i;

  if (last === null) {
    count = now.count;
  }
  else {
    count = now.count - last.count;
  }

  if (count === 0) {
    for (i = 0;  i < n;  ++i) {
      result[i] = 0;
    }
  }

  else if (last === null) {
    for (i = 0;  i < n;  ++i) {
      result[i] = now.counts[i] / count;
    }
  }

  else  {
    for (i = 0;  i < n;  ++i) {
      result[i] = (now.counts[i] - last.counts[i]) / count;
    }
  }

  return { values: result, cuts: cuts };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief computeStatisticsRaw
////////////////////////////////////////////////////////////////////////////////

function computeStatisticsRaw (start, dbServer) {
  var values;
  var hasStart = false;
  var serverFilter = "";

  if (dbServer === undefined) {
    if (cluster.isCluster()) {
      if (cluster.isCoordinator()) {
        serverFilter = "FILTER s.clusterId == @server";
        dbServer = cluster.coordinatorId();
      }
    }
  }
  else {
    serverFilter = "FILTER s.clusterId == @server";
  }

  if (start === null || start === undefined) {
    values = db._query(
        "FOR s IN _statistics "
      +    serverFilter
      + "  SORT s.time "
      + "  return s",
      { server: dbServer });
  }
  else {
    values = db._query(
        "FOR s IN _statistics "
      + "  FILTER s.time >= @start "
      +    serverFilter
      + "  SORT s.time "
      + "  return s",
      { start: start - 2 * STATISTICS_INTERVAL, server: dbServer });

    hasStart = true;
  }

  var times = [];
  var series = [];

  var lastTime = null;
  var lastRaw = null;
  var last2Raw = null;
  var last3Raw = null;

  while (values.hasNext()) {
    var raw = values.next();
    var t = raw.time;

    if (lastTime !== null && (! hasStart || t >= start)) {
      if (lastTime + STATISTICS_INTERVAL * 1.5 < t || raw.server.uptime < lastRaw.server.uptime) {
        while (lastTime + STATISTICS_INTERVAL * 1.1 < t) {
          lastTime += STATISTICS_INTERVAL;

          times.push(lastTime);
          series.push({});
        }
      }
      else {
        var d = t - lastTime;
        var data = {};

        // x (aka time) axis
        times.push(t);

        // y axis
        series.push(data);

        // time figures
        data.totalTime = avgPerSlice(
          raw.client.totalTime,
          lastRaw.client.totalTime);

        data.requestTime = avgPerSlice(
          raw.client.requestTime,
          lastRaw.client.requestTime);

        data.queueTime = avgPerSlice(
          raw.client.queueTime,
          lastRaw.client.queueTime);

        // there is a chance that the request and queue do not fit perfectly
        // if io time get negative, simple ignore it

        data.ioTime = data.totalTime - data.requestTime - data.queueTime;

        if (data.ioTime < 0.0) {
          data.ioTime = 0;
        }

        // data transfer figures
        data.bytesSent = avgPerTimeSlice(
          d,
          raw.client.bytesSent,
          lastRaw.client.bytesSent);

        data.bytesReceived = avgPerTimeSlice(
          d,
          raw.client.bytesReceived,
          lastRaw.client.bytesReceived);

        // requests
        data.optionsPerSecond = perTimeSlice(
          d,
          raw.http.requestsOptions, 
          lastRaw.http.requestsOptions);

        data.putsPerSecond = perTimeSlice(
          d,
          raw.http.requestsPut, 
          lastRaw.http.requestsPut);

        data.headsPerSecond = perTimeSlice(
          d,
          raw.http.requestsHead, 
          lastRaw.http.requestsHead);

        data.postsPerSecond = perTimeSlice(
          d,
          raw.http.requestsPost, 
          lastRaw.http.requestsPost);

        data.getsPerSecond = perTimeSlice(
          d,
          raw.http.requestsGet, 
          lastRaw.http.requestsGet);

        data.deletesPerSecond = perTimeSlice(
          d,
          raw.http.requestsDelete, 
          lastRaw.http.requestsDelete);

        data.othersPerSecond = perTimeSlice(
          d,
          raw.http.requestsOther, 
          lastRaw.http.requestsOther);

        data.patchesPerSecond = perTimeSlice(
          d,
          raw.http.requestsPatch, 
          lastRaw.http.requestsPatch);

        data.asyncPerSecond = perTimeSlice(
          d,
          raw.http.requestsAsync,
          lastRaw.http.requestsAsync);

        // memory size
        data.virtualSize = raw.system.virtualSize;
        data.residentSize = raw.system.residentSize;
        data.residentSizePercent = raw.system.residentSizePercent;

        data.numberOfThreads = raw.system.numberOfThreads;
        data.clientConnections = raw.client.httpConnections;

        // page faults
        data.majorPageFaultsPerSecond = perTimeSlice(
          d,
          raw.system.majorPageFaults,
          lastRaw.system.majorPageFaults);

        data.minorPageFaultsPerSecond = perTimeSlice(
          d,
          raw.system.minorPageFaults,
          lastRaw.system.minorPageFaults);

        // cpu time
        data.userTimePerSecond = perTimeSlice(
          d,
          raw.system.userTime,
          lastRaw.http.userTime);

        data.systemTimePerSecond = perTimeSlice(
          d,
          raw.system.systemTime,
          lastRaw.system.systemTime);
      }
    }

    // update last
    lastTime = t;

    last3Raw = last2Raw;
    last2Raw = lastRaw;
    lastRaw = raw;
  }

  var distribution;
  var server;
  var current;

  if (lastRaw === null) {
    distribution = {};
    server = {};
    current = {};
  }
  else {
    server = {
      physicalMemory: lastRaw.server.physicalMemory,
      uptime: lastRaw.server.uptime
    };

    if (last2Raw === null) {
      var dt = lastRaw.time;
      var http = lastRaw.http;

      current = {
        asyncPerSecond: http.requestsAsync / dt,
        asyncPerSecondPercentChange: 0,
        syncPerSecond: (http.requestsTotal - http.requestsAsync) / dt,
        syncPerSecondPercentChange: 0,
        clientConnections: lastRaw.client.httpConnections,
        clientConnectionsPercentChange: 0,
        numberOfThreads: lastRaw.system.numberOfThreads,
        numberOfThreadsPercentChange: 0,
        virtualSize: lastRaw.system.virtualSize,
        virtualSizePercentChange: 0
      };

      distribution = {
        bytesSentPercent: avgPercentDistributon(
          lastRaw.client.bytesSent,
          null,
          internal.bytesSentDistribution),

        bytesReceivedPercent: avgPercentDistributon(
          lastRaw.client.bytesReceived,
          null,
          internal.bytesReceivedDistribution),

        totalTimePercent: avgPercentDistributon(
          lastRaw.client.totalTime,
          null,
          internal.requestTimeDistribution),

        requestTimePercent: avgPercentDistributon(
          lastRaw.client.requestTime,
          null,
          internal.requestTimeDistribution),

        queueTimePercent: avgPercentDistributon(
          lastRaw.client.queueTime,
          null,
          internal.requestTimeDistribution)
      };
    }
    else {
      var dt = lastRaw.time - last2Raw.time;
      var http = lastRaw.http;
      var http2 = last2Raw.http;

      var dt2 = 0;
      var http3 = null;

      if (last3Raw !== null) {
        dt2 = last2Raw.time - last3Raw.time;
        http3 = last3Raw.http;
      }

      // async requests
      var n1 = (http.requestsAsync - http2.requestsAsync) / dt;
      var m1 = 0;

      if (last3Raw !== null) {
        m1 = (http2.requestsAsync - http3.requestsAsync) / dt2;
      }

      if (n1 < 0) {
        n1 = 0;
      }

      if (m1 < 0) {
        m1 = 0;
      }

      var d1 = 0;

      if (m1 !== 0) {
        d1 = (n1 - m1) / m1;
      }

      // sync requests
      var n5 = ((http.requestsTotal - http.requestsAsync)
              - (http2.requestsTotal - http2.requestsAsync))
              / dt;
      var m5 = 0;

      if (last3Raw !== null) {
        m5 = ((http2.requestsTotal - http2.requestsAsync) 
            - (http3.requestsTotal - http3.requestsAsync))
            / dt2;
      }

      if (n5 < 0) {
        n5 = 0;
      }

      if (m5 < 0) {
        m5 = 0;
      }

      var d5 = 0;

      if (m5 !== 0) {
        d5 = (n5 - m5) / m5;
      }

      // http client connections
      var n2 = lastRaw.client.httpConnections;
      var m2 = last2Raw.client.httpConnections;
      var d2 = 0;

      if (m2 !== 0) {
        d2 = (n2 - m2) / n2;
      }

      // number of threads
      var n3 = lastRaw.system.numberOfThreads;
      var m3 = last2Raw.system.numberOfThreads;
      var d3 = 0;

      if (m3 !== 0) {
        d3 = (n3 - m3) / n3;
      }

      // virtual size
      var n4 = lastRaw.system.virtualSize;
      var m4 = last2Raw.system.virtualSize;
      var d4 = 0;

      if (m4 !== 0) {
        d4 = (n4 - m4) / n4;
      }

      // current summary
      current = {
        asyncPerSecond: n1,
        asyncPerSecondPercentChange: d1,
        syncPerSecond: n5,
        syncPerSecondPercentChange: d5,
        clientConnections: n2,
        clientConnectionsPercentChange: d2,
        numberOfThreads: n3,
        numberOfThreadsPercentChange: d3,
        virtualSize: n4,
        virtualSizePercentChange: d4
      };

      // distribution summary
      distribution = {
        bytesSentPercent: avgPercentDistributon(
          lastRaw.client.bytesSent,
          last2Raw.client.bytesSent,
          internal.bytesSentDistribution),

        bytesReceivedPercent: avgPercentDistributon(
          lastRaw.client.bytesReceived,
          last2Raw.client.bytesReceived,
          internal.bytesReceivedDistribution),

        totalTimePercent: avgPercentDistributon(
          lastRaw.client.totalTime,
          last2Raw.client.totalTime,
          internal.requestTimeDistribution),

        requestTimePercent: avgPercentDistributon(
          lastRaw.client.requestTime,
          last2Raw.client.requestTime,
          internal.requestTimeDistribution),

        queueTimePercent: avgPercentDistributon(
          lastRaw.client.queueTime,
          last2Raw.client.queueTime,
          internal.requestTimeDistribution)
      };
    }
  }

  return {
    next: lastTime,
    times: times,
    series: series,
    distribution: distribution,
    current: current,
    server: server
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief computeStatisticsRaw15M
////////////////////////////////////////////////////////////////////////////////

function computeStatisticsRaw15M (start, dbServer) {
  var serverFilter = "";

  if (dbServer === undefined) {
    if (cluster.isCluster()) {
      if (cluster.isCoordinator()) {
        serverFilter = "FILTER s.clusterId == @server";
        dbServer = cluster.coordinatorId();
      }
    }
  }
  else {
    serverFilter = "FILTER s.clusterId == @server";
  }

  var values = db._query(
        "FOR s IN _statistics "
      + "  FILTER s.time >= @m15 "
      +    serverFilter
      + "  SORT s.time "
      + "  return { time: s.time, "
      + "           clientConnections: s.client.httpConnections, "
      + "           numberOfThreads: s.system.numberOfThreads, "
      + "           virtualSize: s.system.virtualSize }",
    { m15: start - 15 * 60, server: dbServer });

  var lastRaw = null;
  var count = 0;
  var sumConnections = 0;
  var sumThreads = 0;
  var sumVirtualSize = 0;

  var firstAsync = null;
  var firstConnections = null;
  var firstThreads = null;
  var firstVirtualSize = null;

  while (values.hasNext()) {
    var raw = values.next();

    if (lastRaw === null) {
      lastRaw = raw;
    }
    else {
      var d = raw.time - lastRaw.time;

      if (d !== 0) {
        var n2 = raw.clientConnections;
        var n3 = raw.numberOfThreads;
        var n4 = raw.virtualSize;

        count++;
        sumConnections += n2;
        sumThreads += n3;
        sumVirtualSize += n4;

        if (firstConnections === null) {
          firstConnections = n2;
        }

        if (firstThreads === null) {
          firstThreads = n3;
        }

        if (firstVirtualSize === null) {
          firstVirtualSize = n4;
        }
      }
    }
  }

  if (count !== 0) {
    sumConnections /= count;
    sumThreads /= count;
    sumVirtualSize /= count;
  }

  if (firstConnections === null) {
    firstConnections = 0;
  }
  else if (firstConnections !== 0) {
    firstConnections = (sumConnections - firstConnections) / firstConnections;
  }

  if (firstThreads === null) {
    firstThreads = 0;
  }
  else if (firstThreads !== 0) {
    firstThreads = (sumThreads - firstThreads) / firstThreads;
  }

  if (firstVirtualSize === null) {
    firstVirtualSize = 0;
  }
  else if (firstVirtualSize !== 0) {
    firstVirtualSize = (sumVirtualSize - firstVirtualSize) / firstVirtualSize;
  }

  return {
    clientConnections: sumConnections,
    clientConnectionsPercentChange: firstConnections,
    numberOfThreads: sumThreads,
    numberOfThreadsPercentChange: firstThreads,
    virtualSize: sumVirtualSize,
    virtualSizePercentChange: firstVirtualSize
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief convertSeries
////////////////////////////////////////////////////////////////////////////////

function convertSeries (series, name) {
  var result = [];
  var i;

  for (i = 0;  i < series.length;  ++i) {
    result.push(series[i][name]);
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief computeStatisticsSeries
////////////////////////////////////////////////////////////////////////////////

function computeStatisticsSeries (start, attrs, dbServer) {
  var result = {};
  var raw = computeStatisticsRaw(start, dbServer);

  result.nextStart = raw.next;
  result.waitFor = (raw.next + STATISTICS_INTERVAL) - internal.time();
  result.times = raw.times;

  if (result.waitFor < 1) {
    result.waitFor = 1;
  }

  if (attrs === null || attrs.avgTotalTime) {
    result.avgTotalTime = convertSeries(raw.series, "totalTime");
  }

  if (attrs === null || attrs.totalTimeDistributionPercent) {
    result.totalTimeDistributionPercent = raw.distribution.totalTimePercent;
  }

  if (attrs === null || attrs.avgRequestTime) {
    result.avgRequestTime = convertSeries(raw.series, "requestTime");
  }

  if (attrs === null || attrs.requestTimeDistribution) {
    result.requestTimeDistributionPercent = raw.distribution.requestTimePercent;
  }

  if (attrs === null || attrs.avgQueueTime) {
    result.avgQueueTime = convertSeries(raw.series, "queueTime");
  }

  if (attrs === null || attrs.queueTimeDistributionPercent) {
    result.queueTimeDistributionPercent = raw.distribution.queueTimePercent;
  }

  if (attrs === null || attrs.avgIoTime) {
    result.avgIoTime = convertSeries(raw.series, "ioTime");
  }

  if (attrs === null || attrs.bytesSentPerSecond) {
    result.bytesSentPerSecond = convertSeries(raw.series, "bytesSent");
  }

  if (attrs === null || attrs.bytesSentDistributionPercent) {
    result.bytesSentDistributionPercent = raw.distribution.bytesSentPercent;
  }

  if (attrs === null || attrs.bytesReceivedPerSecond) {
    result.bytesReceivedPerSecond = convertSeries(raw.series, "bytesReceived");
  }

  if (attrs === null || attrs.bytesReceivedDistributionPercent) {
    result.bytesReceivedDistributionPercent = raw.distribution.bytesReceivedPercent;
  }

  if (attrs === null || attrs.optionsPerSecond) {
    result.optionsPerSecond = convertSeries(raw.series, "optionsPerSecond");
  }

  if (attrs === null || attrs.putsPerSecond) {
    result.putsPerSecond = convertSeries(raw.series, "putsPerSecond");
  }

  if (attrs === null || attrs.headsPerSecond) {
    result.headsPerSecond = convertSeries(raw.series, "headsPerSecond");
  }

  if (attrs === null || attrs.postsPerSecond) {
    result.postsPerSecond = convertSeries(raw.series, "postsPerSecond");
  }

  if (attrs === null || attrs.getsPerSecond) {
    result.getsPerSecond = convertSeries(raw.series, "getsPerSecond");
  }

  if (attrs === null || attrs.deletesPerSecond) {
    result.deletesPerSecond = convertSeries(raw.series, "deletesPerSecond");
  }

  if (attrs === null || attrs.othersPerSecond) {
    result.othersPerSecond = convertSeries(raw.series, "othersPerSecond");
  }

  if (attrs === null || attrs.patchesPerSecond) {
    result.patchesPerSecond = convertSeries(raw.series, "patchesPerSecond");
  }

  if (attrs === null || attrs.asyncPerSecond) {
    result.asyncPerSecond = convertSeries(raw.series, "asyncPerSecond");
  }

  if (attrs === null || attrs.asyncPerSecondCurrent) {
    result.asyncPerSecondCurrent = raw.current.asyncPerSecond;
  }

  if (attrs === null || attrs.asyncPerSecondCurrentPercentChange) {
    result.asyncPerSecondCurrentPercentChange = raw.current.asyncPerSecondPercentChange;
  }

  if (attrs === null || attrs.syncPerSecondCurrent) {
    result.syncPerSecondCurrent = raw.current.syncPerSecond;
  }

  if (attrs === null || attrs.syncPerSecondCurrentPercentChange) {
    result.syncPerSecondCurrentPercentChange = raw.current.syncPerSecondPercentChange;
  }

  if (attrs === null || attrs.clientConnections) {
      result.clientConnections = convertSeries(raw.series, "clientConnections");
  }

  if (attrs === null || attrs.clientConnectionsCurrent) {
    result.clientConnectionsCurrent = raw.current.clientConnections;
  }

  if (attrs === null || attrs.clientConnectionsCurrentPercentChange) {
    result.clientConnectionsCurrentPercentChange = raw.current.clientConnectionsPercentChange;
  }

  if (attrs === null || attrs.numberOfThreads) {
      result.numberOfThreads = convertSeries(raw.series, "numberOfThreads");
  }

  if (attrs === null || attrs.numberOfThreadsCurrent) {
    result.numberOfThreadsCurrent = raw.current.numberOfThreads;
  }

  if (attrs === null || attrs.numberOfThreadsCurrentPercentChange) {
    result.numberOfThreadsCurrentPercentChange = raw.current.numberOfThreadsPercentChange;
  }

  if (attrs === null || attrs.residentSize) {
    result.residentSize = convertSeries(raw.series, "residentSize");
  }

  if (attrs === null || attrs.residentSizePercent) {
    result.residentSizePercent = convertSeries(raw.series, "residentSizePercent");
  }

  if (attrs === null || attrs.virtualSize) {
    result.virtualSize = convertSeries(raw.series, "virtualSize");
  }

  if (attrs === null || attrs.virtualSizeCurrent) {
    result.virtualSizeCurrent = raw.current.virtualSize;
  }

  if (attrs === null || attrs.virtualSizePercentChange) {
    result.virtualSizePercentChange = raw.current.virtualSizePercentChange;
  }

  if (attrs === null || attrs.majorPageFaultsPerSecond) {
    result.majorPageFaultsPerSecond = convertSeries(raw.series, "majorPageFaultsPerSecond");
  }

  if (attrs === null || attrs.minorPageFaultsPerSecond) {
    result.minorPageFaultsPerSecond = convertSeries(raw.series, "minorPageFaultsPerSecond");
  }

  if (attrs === null || attrs.userTimePerSecond) {
    result.userTimePerSecond = convertSeries(raw.series, "userTimePerSecond");
  }

  if (attrs === null || attrs.systemTimePerSecond) {
    result.systemTimePerSecond = convertSeries(raw.series, "systemTimePerSecond");
  }

  if (attrs === null || attrs.uptime) {
    result.uptime = raw.server.uptime;
  }

  if (attrs === null || attrs.physicalMemory) {
    result.physicalMemory = raw.server.physicalMemory;
  }

  if (result.next !== null) {
    var raw15 = computeStatisticsRaw15M(raw.next, dbServer);

    if (attrs === null || attrs.clientConnections15M) {
      result.clientConnections15M = raw15.clientConnections;
    }

    if (attrs === null || attrs.clientConnectionsPercentChange15M) {
      result.clientConnectionsPercentChange15M = raw15.clientConnectionsPercentChange;
    }

    if (attrs === null || attrs.numberOfThreads15M) {
      result.numberOfThreads15M = raw15.numberOfThreads;
    }

    if (attrs === null || attrs.numberOfThreadsPercentChange15M) {
      result.numberOfThreadsPercentChange15M = raw15.numberOfThreadsPercentChange;
    }

    if (attrs === null || attrs.virtualSize15M) {
      result.virtualSize15M = raw15.virtualSize;
    }

    if (attrs === null || attrs.virtualSizePercentChange15M) {
      result.virtualSizePercentChange15M = raw15.virtualSizePercentChange;
    }
  }

  return result;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                     public routes
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief full statistics history
////////////////////////////////////////////////////////////////////////////////

controller.get("full", function (req, res) {
  var start = req.params("start");
  var dbServer = req.params("DBserver");
  var attrs = null;

  if (start !== null && start !== undefined) {
    start = parseInt(start, 10);
  }

  var series = computeStatisticsSeries(start, attrs, dbServer);

  res.json(series);
}).summary("Returns the statistics")
  .notes("This function is used to get the statistics history.");

////////////////////////////////////////////////////////////////////////////////
/// @brief full statistics history
////////////////////////////////////////////////////////////////////////////////

controller.get("history", function (req, res) {
  var start = req.params("start");
  var filter = req.params("filter");
  var dbServer = req.params("DBserver");
  var attrs = {};
  var s = filter.split(",");
  var i;

  if (start !== null && start !== undefined) {
    start = parseInt(start, 10);
  }

  for (i = 0;  i < s.length;  ++i) {
    attrs[s[i]] = true;
  }

  if (dbServer === null) {
    dbServer = undefined;
  }

  var series = computeStatisticsSeries(start, attrs, dbServer);

  res.json(series);
}).summary("Returns the aggregated history")
  .notes("This function is used to get the aggregated statistics history.");

////////////////////////////////////////////////////////////////////////////////
/// @brief cluster statistics history
////////////////////////////////////////////////////////////////////////////////

controller.get("cluster", function (req, res) {
  if (! cluster.isCoordinator()) {
    actions.resultError(req, res, actions.HTTP_FORBIDDEN, 0, "only allowed on coordinator");
    return;
  }

  if (! req.parameters.hasOwnProperty("DBserver")) {
    actions.resultError(req, res, actions.HTTP_BAD, "required parameter DBserver was not given");
    return;
  }

  var DBserver = req.parameters.DBserver;
  var coord = { coordTransactionID: ArangoClusterInfo.uniqid() };
  var options = { coordTransactionID: coord.coordTransactionID, timeout:10 };

  var url = "/_admin/aardvark/statistics/full";
  var sep = "?";

  if (req.parameters.hasOwnProperty("start")) {
    url += sep + "start=" + encodeURIComponent(req.params("start"));
    sep = "&";
  }
  
  if (req.parameters.hasOwnProperty("filter")) {
    url += sep + "filter=" + encodeURIComponent(req.params("filter"));
    sep = "&";
  }

  url += sep + "DBserver=" + encodeURIComponent(DBserver);

  var op = ArangoClusterComm.asyncRequest(
    "GET",
    "server:"+DBserver,
    "_system",
    url,
    "",
    {},
    options);

  var r = ArangoClusterComm.wait(op);

  res.contentType = "application/json; charset=utf-8";

  if (r.status === "RECEIVED") {
    res.responseCode = actions.HTTP_OK;
    res.body = r.body;
  }
  else if (r.status === "TIMEOUT") {
    res.responseCode = actions.HTTP_BAD;
    res.body = JSON.stringify( {"error":true, "errorMessage": "operation timed out"});
  }
  else {
    res.responseCode = actions.HTTP_BAD;

    var bodyobj;

    try {
      bodyobj = JSON.parse(r.body);
    }
    catch (err) {
    }

    res.body = JSON.stringify({
      "error":true,
      "errorMessage": "error from DBserver, possibly DBserver unknown",
      "body": bodyobj
    } );
  }
}).summary("Returns the complete or partial history of a cluster member")
  .notes("This function is used to get the complete or partial statistics history of a cluster member.");

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @\\}"
// End:
