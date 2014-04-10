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

var FoxxController = require("org/arangodb/foxx").Controller;
var controller = new FoxxController(applicationContext);
var db = require("org/arangodb").db;
var internal = require("internal");
var STATISTICS_INTERVALL = 10;

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
  var result = new Array(cuts.length - 1);
  var count = 0;
  var i;

  if (last === null) {
    count = now.count;
  }
  else {
    count = now.count - last.count;
  }

  if (count === 0) {
    for (i = 0;  i < cuts.length;  ++i) {
      result[i] = 0;
    }
  }

  else if (last === null) {
    for (i = 0;  i < cuts.length;  ++i) {
      result[i] = now.counts[i] / count;
    }
  }

  else  {
    for (i = 0;  i < cuts.length;  ++i) {
      result[i] = (now.counts[i] - last.counts[i]) / count;
    }
  }

  return { values: result, cuts: cuts };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief computeStatisticsRaw
////////////////////////////////////////////////////////////////////////////////

function computeStatisticsRaw (start) {
  var values;

  if (start === null || start === undefined) {
    values = db._query(
        "FOR s IN _statistics "
      + "  SORT s.time "
      + "  return s");
  }
  else {
    values = db._query(
        "FOR s IN _statistics "
      + "  FILTER s.time >= @start "
      + "  SORT s.time "
      + "  return s",
      { start: start });
  }

  var times = [];
  var series = [];

  var lastTime = null;
  var lastRaw = null;
  var lastLastRaw = null;

  while (values.hasNext()) {
    var raw = values.next();
    var t = raw.time;

    if (lastTime === null) {
      lastTime = t;
      lastLastRaw = null;
      lastRaw = raw;
    }
    else if (lastTime + STATISTICS_INTERVALL * 1.5 < t
          || raw.server.uptime < lastRaw.server.uptime) {
      while (lastTime + STATISTICS_INTERVALL * 1.1 < t) {
        lastTime += STATISTICS_INTERVALL;

        times.push(lastTime);

        series.push({
          totalTime: null,
          requestTime: null,
          queueTime: null,
          ioTime: null,
          bytesSent: null,
          bytesReceived: null,
          requestPerSecondOptions: null,
          requestPerSecondPut: null,
          requestPerSecondHead: null,
          requestPerSecondPost: null,
          requestPerSecondGet: null,
          virtualSize: null,
          majorPageFaults: null,
          userTime: null,
          systemTime: null
        });
      }

      lastTime = t;
      lastLastRaw = null;
      lastRaw = raw;
    }
    else {

      // x (aka time) axis
      times.push(t);

      // y axis
      var data = {};
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

      data.ioTime = data.totalTime - data.requestTime - data.queueTime;

      // data transfer figures
      data.bytesSent = avgPerTimeSlice(
        t - lastTime,
        raw.client.bytesSent,
        lastRaw.client.bytesSent);

      data.bytesReceived = avgPerTimeSlice(
        t - lastTime,
        raw.client.bytesReceived,
        lastRaw.client.bytesReceived);

      // requests
      data.optionsPerSecond = perTimeSlice(
        t - lastTime,
        raw.http.requestsOptions, 
        lastRaw.http.requestsOptions);

      data.putsPerSecond = perTimeSlice(
        t - lastTime,
        raw.http.requestsPut, 
        lastRaw.http.requestsPut);

      data.headsPerSecond = perTimeSlice(
        t - lastTime,
        raw.http.requestsHead, 
        lastRaw.http.requestsHead);

      data.postsPerSecond = perTimeSlice(
        t - lastTime,
        raw.http.requestsPost, 
        lastRaw.http.requestsPost);

      data.getsPerSecond = perTimeSlice(
        t - lastTime,
        raw.http.requestsGet, 
        lastRaw.http.requestsGet);

      // memory size
      data.virtualSize = raw.system.virtualSize;
      data.residentSize = raw.system.residentSize;
      data.residentSizePercent = raw.system.residentSizePercent;

      // page faults
      data.majorPageFaultsPerSecond = perTimeSlice(
        t - lastTime,
        raw.system.majorPageFaults,
        lastRaw.system.majorPageFaults);

      // cpu time
      data.userTime = raw.system.userTime;
      data.systemTime = raw.system.systemTime;

      // update last
      lastTime = t;
      lastLastRaw = lastRaw;
      lastRaw = raw;
    }
  }

  var distribution;

  if (lastRaw === null) {
    distribution = {
      bytesSent: null
    };
  }
  else if (lastLastRaw === null) {
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
    distribution = {
      bytesSentPercent: avgPercentDistributon(
        lastRaw.client.bytesSent,
        lastLastRaw.client.bytesSent,
        internal.bytesSentDistribution),

      bytesReceivedPercent: avgPercentDistributon(
        lastRaw.client.bytesReceived,
        lastLastRaw.client.bytesReceived,
        internal.bytesReceivedDistribution),

      totalTimePercent: avgPercentDistributon(
        lastRaw.client.totalTime,
        lastLastRaw.client.totalTime,
        internal.requestTimeDistribution),

      requestTimePercent: avgPercentDistributon(
        lastRaw.client.requestTime,
        lastLastRaw.client.requestTime,
        internal.requestTimeDistribution),

      queueTimePercent: avgPercentDistributon(
        lastRaw.client.queueTime,
        lastLastRaw.client.queueTime,
        internal.requestTimeDistribution)
    };
  }

  return {
    next: lastTime,
    times: times,
    series: series,
    distribution: distribution
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

function computeStatisticsSeries (start) {
  var result = {};
  var raw = computeStatisticsRaw(start);

  result.nextStart = raw.next;
  result.waitFor = (raw.next + STATISTICS_INTERVALL) - internal.time();
  result.times = raw.times;

  result.avgTotalTime = convertSeries(raw.series, "totalTime");
  result.totalTimeDistributionPercent = raw.distribution.totalTimePercent;

  result.avgRequestTime = convertSeries(raw.series, "requestTime");
  result.requestTimeDistributionPercent = raw.distribution.requestTimePercent;

  result.avgQueueTime = convertSeries(raw.series, "queueTime");
  result.queueTimeDistributionPercent = raw.distribution.queueTimePercent;

  result.avgIoTime = convertSeries(raw.series, "ioTime");

  result.bytesSentPerSecond = convertSeries(raw.series, "bytesSent");
  result.bytesSentDistributionPercent = raw.distribution.bytesSentPercent;

  result.bytesReceivedPerSecond = convertSeries(raw.series, "bytesReceived");
  result.bytesReceivedDistributionPercent = raw.distribution.bytesReceivedPercent;

  result.optionsPerSecond = convertSeries(raw.series, "optionsPerSecond");
  result.putsPerSecond = convertSeries(raw.series, "putsPerSecond");
  result.headsPerSecond = convertSeries(raw.series, "headsPerSecond");
  result.postsPerSecond = convertSeries(raw.series, "postsPerSecond");
  result.getsPerSecond = convertSeries(raw.series, "getsPerSecond");

//  result.asyncRequestsCurrent = computeAsyncRequestsCurrent(raw);
//  result.asyncRequests15M = computeAsyncRequests15M(raw);

//  result.clientConnectionsCurrent = computeClientConnectionsCurrent(raw);
//  result.clientConnections15M = computeClientConnections15M(raw);

//  result.numberThreadsCurrent = computeNumberThreadsCurrent(raw);
//  result.numberThreads15M = computeNumberThreads15M(raw);

  result.residentSize = convertSeries(raw.series, "residentSize");
  result.residentSizePercent = convertSeries(raw.series, "residentSizePercent");

  result.virtualSize = convertSeries(raw.series, "virtualSize");
//  result.virtualSize15M = computeVirtualSize15M(raw);

  result.majorPageFaultsPerSecond = convertSeries(raw.series, "majorPageFaultsPerSecond");
//  result.cpuTime = computeCpuTime(raw);

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

  if (start !== null && start !== undefined) {
    start = parseInt(start, 10);
  }

  var series = computeStatisticsSeries(start);

  res.json(series);
}).summary("Returns the complete or partial history")
  .notes("This function is just to get the complete or partial statistics history");

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @\\}"
// End:
