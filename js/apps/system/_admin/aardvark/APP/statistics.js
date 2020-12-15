/*global ArangoServerState, ArangoClusterComm*/
'use strict';

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014 triAGENS GmbH, Cologne, Germany
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Dr. Frank Celler
/// @author Alan Plum
////////////////////////////////////////////////////////////////////////////////

const internal = require('internal');
const download = internal.clusterDownload;
const cluster = require('@arangodb/cluster');

const db = require('@arangodb').db;
const _ = require('lodash');

const STATISTICS_INTERVAL = 10; // seconds
const STATISTICS_HISTORY_INTERVAL = 15 * 60; // seconds

const joi = require('joi');
const httperr = require('http-errors');
const createRouter = require('@arangodb/foxx/router');
const { MergeStatisticSamples } = require('@arangodb/statistics-helper');

const router = createRouter();
module.exports = router;

const startOffsetSchema = joi.number().default(
  () => internal.time() - STATISTICS_INTERVAL * 10,
  'Default offset'
);

const clusterIdSchema = joi.string().default(
  () => cluster.isCluster() ? ArangoServerState.id() : undefined,
  'Default DB server'
);


function percentChange (current, prev, section, src) {
  if (prev === null) {
    return 0;
  }

  const p = prev[section][src];

  if (p !== 0) {
    return (current[section][src] - p) / p;
  }

  return 0;
}


function percentChange2 (current, prev, section, src1, src2) {
  if (prev === null) {
    return 0;
  }

  const p = prev[section][src1] - prev[section][src2];

  if (p !== 0) {
    return (current[section][src1] - current[section][src2] - p) / p;
  }

  return 0;
}


const STAT_SERIES = {
  avgTotalTime: [ "client", "avgTotalTime" ],
  avgRequestTime: [ "client", "avgRequestTime" ],
  avgQueueTime: [ "client", "avgQueueTime" ],
  avgIoTime: [ "client", "avgIoTime" ],

  bytesSentPerSecond: [ "client", "bytesSentPerSecond" ],
  bytesReceivedPerSecond: [ "client", "bytesReceivedPerSecond" ],

  asyncPerSecond: [ "http", "requestsAsyncPerSecond" ],
  optionsPerSecond: [ "http", "requestsOptionsPerSecond" ],
  putsPerSecond: [ "http", "requestsPutPerSecond" ],
  headsPerSecond: [ "http", "requestsHeadPerSecond" ],
  postsPerSecond: [ "http", "requestsPostPerSecond" ],
  getsPerSecond: [ "http", "requestsGetPerSecond" ],
  deletesPerSecond: [ "http", "requestsDeletePerSecond" ],
  othersPerSecond: [ "http", "requestsOptionsPerSecond" ],
  patchesPerSecond: [ "http", "requestsPatchPerSecond" ],

  systemTimePerSecond: [ "system", "systemTimePerSecond" ],
  userTimePerSecond: [ "system", "userTimePerSecond" ],
  majorPageFaultsPerSecond: [ "system", "majorPageFaultsPerSecond" ],
  minorPageFaultsPerSecond: [ "system", "minorPageFaultsPerSecond" ]
};


const STAT_DISTRIBUTION = {
  totalTimeDistributionPercent: [ "client", "totalTimePercent" ],
  requestTimeDistributionPercent: [ "client", "requestTimePercent" ],
  queueTimeDistributionPercent: [ "client", "queueTimePercent" ],
  bytesSentDistributionPercent: [ "client", "bytesSentPercent" ],
  bytesReceivedDistributionPercent: [ "client", "bytesReceivedPercent" ]
};


function computeStatisticsRaw (result, start, clusterId) {

  let filter = "";

  if (clusterId !== undefined) {
    filter = " FILTER s.clusterId == @clusterId ";
  }

  const values = db._query(
        "FOR s IN _statistics "
      + "  FILTER s.time > @start "
      +    filter
      + "  SORT s.time "
      + "  return s",
    { start: start - 2 * STATISTICS_INTERVAL, clusterId: clusterId });

  result.enabled = internal.enabledStatistics(); 
  result.times = [];

  for (let key in STAT_SERIES) {
    if (STAT_SERIES.hasOwnProperty(key)) {
      result[key] = [];
    }
  }

  let lastRaw = null;
  let lastRaw2 = null;
  let path;

  // read the last entries
  while (values.hasNext()) {
    const stat = values.next();

    lastRaw2 = lastRaw;
    lastRaw = stat;

    if (stat.time <= start) {
      continue;
    }

    result.times.push(stat.time);

    for (let key in STAT_SERIES) {
      if (STAT_SERIES.hasOwnProperty(key)) {
        path = STAT_SERIES[key];
        result[key].push(stat[path[0]][path[1]]);
      }
    }
  }

  // have at least one entry, use it
  if (lastRaw !== null) {
    for (let key in STAT_DISTRIBUTION) {
      if (STAT_DISTRIBUTION.hasOwnProperty(key)) {
        path = STAT_DISTRIBUTION[key];

        result[key] = lastRaw[path[0]][path[1]];
      }
    }

    result.numberOfThreadsCurrent = lastRaw.system.numberOfThreads;
    result.numberOfThreadsPercentChange = percentChange(lastRaw, lastRaw2, 'system', 'numberOfThreads');

    result.virtualSizeCurrent = lastRaw.system.virtualSize;
    result.virtualSizePercentChange = percentChange(lastRaw, lastRaw2, 'system', 'virtualSize');

    result.residentSizeCurrent = lastRaw.system.residentSize;
    result.residentSizePercent = lastRaw.system.residentSizePercent;

    result.asyncPerSecondCurrent = lastRaw.http.requestsAsyncPerSecond;
    result.asyncPerSecondPercentChange = percentChange(lastRaw, lastRaw2, 'http', 'requestsAsyncPerSecond');

    result.syncPerSecondCurrent = lastRaw.http.requestsTotalPerSecond - lastRaw.http.requestsAsyncPerSecond;
    result.syncPerSecondPercentChange
        = percentChange2(lastRaw, lastRaw2, 'http', 'requestsTotalPerSecond', 'requestsAsyncPerSecond');

    result.clientConnectionsCurrent = lastRaw.client.httpConnections;
    result.clientConnectionsPercentChange = percentChange(lastRaw, lastRaw2, 'client', 'httpConnections');
  }

  // have no entry, add nulls
  else {
    for (let key in STAT_DISTRIBUTION) {
      if (STAT_DISTRIBUTION.hasOwnProperty(key)) {
        result[key] = { values: [0,0,0,0,0,0,0], cuts: [0,0,0,0,0,0] };
      }
    }

    const ps = internal.processStatistics();

    result.numberOfThreadsCurrent = ps.numberOfThreads;
    result.numberOfThreadsPercentChange = 0;

    result.virtualSizeCurrent = ps.virtualSize;
    result.virtualSizePercentChange = 0;

    result.residentSizeCurrent = ps.residentSize;
    result.residentSizePercent = ps.residentSizePercent;

    result.asyncPerSecondCurrent = 0;
    result.asyncPerSecondPercentChange = 0;

    result.syncPerSecondCurrent = 0;
    result.syncPerSecondPercentChange = 0;

    result.clientConnectionsCurrent = 0;
    result.clientConnectionsPercentChange = 0;
  }

  // add physical memory
  const ss = internal.serverStatistics();

  result.physicalMemory = ss.physicalMemory;

  // add next start time
  if (lastRaw === null) {
    result.nextStart = internal.time();
    result.waitFor = STATISTICS_INTERVAL;
  }
  else {
    result.nextStart = lastRaw.time;
    result.waitFor = (lastRaw.time + STATISTICS_INTERVAL) - internal.time();
  }
}


function computeStatisticsRaw15M (result, start, clusterId) {
  let filter = "";

  if (clusterId !== undefined) {
    filter = " FILTER s.clusterId == @clusterId ";
  }

  const values = db._query(
        "FOR s IN _statistics15 "
      + "  FILTER s.time > @start "
      +    filter
      + "  SORT s.time "
      + "  return s",
    { start: start - 2 * STATISTICS_HISTORY_INTERVAL, clusterId: clusterId });

  let lastRaw = null;
  let lastRaw2 = null;

  // read the last entries
  while (values.hasNext()) {
    const stat = values.next();

    lastRaw2 = lastRaw;
    lastRaw = stat;
  }

  // have at least one entry, use it
  if (lastRaw !== null) {
    result.numberOfThreads15M = lastRaw.system.numberOfThreads;
    result.numberOfThreads15MPercentChange = percentChange(lastRaw, lastRaw2, 'system', 'numberOfThreads');

    result.virtualSize15M = lastRaw.system.virtualSize;
    result.virtualSize15MPercentChange = percentChange(lastRaw, lastRaw2, 'system', 'virtualSize');

    result.asyncPerSecond15M = lastRaw.http.requestsAsyncPerSecond;
    result.asyncPerSecond15MPercentChange = percentChange(lastRaw, lastRaw2, 'http', 'requestsAsyncPerSecond');

    result.syncPerSecond15M = lastRaw.http.requestsTotalPerSecond - lastRaw.http.requestsAsyncPerSecond;
    result.syncPerSecond15MPercentChange
      = percentChange2(lastRaw, lastRaw2, 'http', 'requestsTotalPerSecond', 'requestsAsyncPerSecond');

    result.clientConnections15M = lastRaw.client.httpConnections;
    result.clientConnections15MPercentChange = percentChange(lastRaw, lastRaw2, 'client', 'httpConnections');
  }

  // have no entry, add nulls
  else {
    const ps = internal.processStatistics();

    result.numberOfThreads15M = ps.numberOfThreads;
    result.numberOfThreads15MPercentChange = 0;

    result.virtualSize15M = ps.virtualSize;
    result.virtualSize15MPercentChange = 0;

    result.asyncPerSecond15M = 0;
    result.asyncPerSecond15MPercentChange = 0;

    result.syncPerSecond15M = 0;
    result.syncPerSecond15MPercentChange = 0;

    result.clientConnections15M = 0;
    result.clientConnections15MPercentChange = 0;
  }
}


function computeStatisticsShort (start, clusterId) {
  const result = {};

  computeStatisticsRaw(result, start, clusterId);

  computeStatisticsRaw15M(result, start, clusterId);

  return result;
}


function computeStatisticsValues (result, values, attrs) {
  for (let key in attrs) {
    if (attrs.hasOwnProperty(key)) {
      result[key] = [];
    }
  }

  while (values.hasNext()) {
    const stat = values.next();

    result.times.push(stat.time);

    for (let key in attrs) {
      if (attrs.hasOwnProperty(key) && STAT_SERIES.hasOwnProperty(key)) {
        const path = STAT_SERIES[key];

        result[key].push(stat[path[0]][path[1]]);
      }
    }
  }
}


function computeStatisticsLong (attrs, clusterId) {
  const short = { times: [] };
  let filter = "";

  if (clusterId !== undefined) {
    filter = " FILTER s.clusterId == @clusterId ";
  }

  const values = db._query(
      "FOR s IN _statistics "
    +    filter
    + "  SORT s.time "
      + "  return s",
    { clusterId: clusterId });

  computeStatisticsValues(short, values, attrs);

  let filter2 = "";
  let end = 0;

  if (short.times.length !== 0) {
    filter2 = " FILTER s.time < @end ";
    end = short.times[0];
  }

  const values2 = db._query(
    "FOR s IN _statistics15 "
    +    filter
    +    filter2
    + "  SORT s.time "
    + "  return s",
    { end: end, clusterId: clusterId });

  const long = { times: [] };

  computeStatisticsValues(long, values2, attrs);

  for (let key in attrs) {
    if (attrs.hasOwnProperty(key) && long.hasOwnProperty(key)) {
      long[key] = long[key].concat(short[key]);
    }
  }

  if (!attrs.times) {
    delete long.times;
  }

  return long;
}


router.use((req, res, next) => {
  if (internal.authenticationEnabled()) {
    if (!req.authorized) {
      throw new httperr.Unauthorized();
    }
  }
  next();
});

router.get('/coordshort', function (req, res) {
  var merged = { };

  const coordinators = global.ArangoClusterInfo.getCoordinators();
  try {
    const stats15Query = `
      FOR s IN _statistics15
        FILTER s.time > @start
        FILTER s.clusterId IN @clusterIds
        SORT s.time
        COLLECT clusterId = s.clusterId INTO clientConnections = s.client.httpConnections
        LET clientConnectionsCurrent = LAST(clientConnections)
        COLLECT AGGREGATE clientConnections15M = SUM(clientConnectionsCurrent)
        RETURN {clientConnections15M: clientConnections15M || 0}`;

    const statsSampleQuery = `
      FOR s IN _statistics
        FILTER s.time > @start
        FILTER s.clusterId IN @clusterIds
        RETURN {
          time: s.time,
          clusterId: s.clusterId,
          physicalMemory: s.server.physicalMemory,
          residentSizeCurrent: s.system.residentSize,
          clientConnectionsCurrent: s.client.httpConnections,
          avgRequestTime: s.client.avgRequestTime,
          bytesSentPerSecond: s.client.bytesSentPerSecond,
          bytesReceivedPerSecond: s.client.bytesReceivedPerSecond,
          http: {
            optionsPerSecond: s.http.requestsOptionsPerSecond,
            putsPerSecond: s.http.requestsPutPerSecond,
            headsPerSecond: s.http.requestsHeadPerSecond,
            postsPerSecond: s.http.requestsPostPerSecond,
            getsPerSecond: s.http.requestsGetPerSecond,
            deletesPerSecond: s.http.requestsDeletePerSecond,
            othersPerSecond: s.http.requestsOptionsPerSecond,
            patchesPerSecond: s.http.requestsPatchPerSecond
          }
      }`;
    
    const params = { start: startOffsetSchema - 2 * STATISTICS_INTERVAL, clusterIds: coordinators };
    const stats15 = db._query(stats15Query, params).toArray();
    const statsSamples = db._query(statsSampleQuery, params).toArray();
    if (statsSamples.length !== 0) {
      // we have no samples -> either statistics are disabled, or the server has just been started
      merged = MergeStatisticSamples(statsSamples);
      merged.clientConnections15M = stats15.length === 0 ? 0 : stats15[0].clientConnections15M;
    }
  } catch (e) {
    // ignore exceptions, because throwing here will render the entire web UI cluster stats broken
  }

  res.json({'enabled': internal.enabledStatistics(), 'data': merged});
})
  .summary('Short term history for all coordinators')
  .description('This function is used to get the statistics history.');

router.get("/short", function (req, res) {
  const start = req.queryParams.start;
  const clusterId = req.queryParams.DBserver;
  const series = computeStatisticsShort(start, clusterId);
  res.json(series);
})
.queryParam("start", startOffsetSchema)
.queryParam("DBserver", clusterIdSchema)
.summary("Short term history")
.description("This function is used to get the statistics history.");


router.get("/long", function (req, res) {
  const filter = req.queryParams.filter;
  const clusterId = req.queryParams.DBserver;

  const attrs = {};
  const s = filter.split(",");

  for (let i = 0;  i < s.length; i++) {
    attrs[s[i]] = true;
  }

  const series = computeStatisticsLong(attrs, clusterId);
  res.json(series);
})
.queryParam("filter", joi.string().required())
.queryParam("DBserver", clusterIdSchema)
.summary("Long term history")
.description("This function is used to get the aggregated statistics history.");


router.get("/cluster", function (req, res) {
  if (!cluster.isCoordinator()) {
    throw new httperr.Forbidden("only allowed on coordinator");
  }

  const DBserver = req.queryParams.DBserver;
  let type = req.queryParams.type;
  const options = { timeout: 10 };

  if (type !== "short" && type !== "long") {
    type = "short";
  }

  let url = "/_admin/statistics/" + type;
  let sep = "?";

  if (req.queryParams.start) {
    url += sep + "start=" + encodeURIComponent(req.queryParams.start);
    sep = "&";
  }

  if (req.queryParams.filter) {
    url += sep + "filter=" + encodeURIComponent(req.queryParams.filter);
    sep = "&";
  }

  url += sep + "DBserver=" + encodeURIComponent(DBserver);

  const op = ArangoClusterComm.asyncRequest(
    "GET",
    "server:" + DBserver,
    "_system",
    url,
    "",
    {},
    options
  );

  const r = ArangoClusterComm.wait(op);

  if (r.status === "RECEIVED") {
    res.set("content-type", "application/json; charset=utf-8");
    res.body = r.body;
  } else if (r.status === "TIMEOUT") {
    throw new httperr.BadRequest("operation timed out");
  } else {
    let body;

    try {
      body = JSON.parse(r.body);
    } catch (e) {
      // noop
    }

    throw Object.assign(
      new httperr.BadRequest("error from DBserver '" + DBserver + "', possibly DBserver unknown or down"),
      {extra: {body}}
    );
  }
})
.queryParam("DBserver", joi.string().required())
.summary("Cluster statistics history")
.description("This function is used to get the complete or partial statistics history of a cluster member.");
