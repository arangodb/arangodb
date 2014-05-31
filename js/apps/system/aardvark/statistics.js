/*jslint indent: 2, nomen: true, maxlen: 120, white: true, plusplus: true, unparam: true, vars: true, continue: true */
/*global require, applicationContext, ArangoServerState, ArangoClusterInfo, ArangoClusterComm*/

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

var internal = require("internal");
var cluster = require("org/arangodb/cluster");
var actions = require("org/arangodb/actions");

var FoxxController = require("org/arangodb/foxx").Controller;
var controller = new FoxxController(applicationContext);
var db = require("org/arangodb").db;

var STATISTICS_INTERVALL = require("org/arangodb/statistics").STATISTICS_INTERVALL;
var STATISTICS_HISTORY_INTERVALL = require("org/arangodb/statistics").STATISTICS_HISTORY_INTERVALL;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief percentChange
////////////////////////////////////////////////////////////////////////////////

function percentChange (current, prev, section, src) {
  'use strict';

  if (prev === null) {
    return 0;
  }

  var p = prev[section][src];

  if (p !== 0) {
    return (current[section][src] - p) / p;
  }

  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief percentChange2
////////////////////////////////////////////////////////////////////////////////

function percentChange2 (current, prev, section, src1, src2) {
  'use strict';

  if (prev === null) {
    return 0;
  }

  var p = prev[section][src1] - prev[section][src2];

  if (p !== 0) {
    return (current[section][src1] -current[section][src2] - p) / p;
  }

  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief computeStatisticsRaw
////////////////////////////////////////////////////////////////////////////////

var STAT_SERIES = {
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

var STAT_DISTRIBUTION = {
  totalTimeDistributionPercent: [ "client", "totalTimePercent" ],
  requestTimeDistributionPercent: [ "client", "requestTimePercent" ],
  queueTimeDistributionPercent: [ "client", "queueTimePercent" ],
  bytesSentDistributionPercent: [ "client", "bytesSentPercent" ],
  bytesReceivedDistributionPercent: [ "client", "bytesReceivedPercent" ]
};

function computeStatisticsRaw (result, start, clusterId) {
  'use strict';

  var filter = "";

  if (clusterId !== undefined) {
    filter = " FILTER s.clusterId == @clusterId ";
  }

  var values = db._query(
        "FOR s IN _statistics "
      + "  FILTER s.time > @start "
      +    filter
      + "  SORT s.time "
      + "  return s",
    { start: start - 2 * STATISTICS_INTERVALL, clusterId: clusterId });
  
  result.times = [];

  var key;

  for (key in STAT_SERIES) {
    if (STAT_SERIES.hasOwnProperty(key)) {
      result[key] = [];
    }
  }

  var lastRaw = null;
  var lastRaw2 = null;
  var path;

  // read the last entries
  while (values.hasNext()) {
    var stat = values.next();

    lastRaw2 = lastRaw;
    lastRaw = stat;

    if (stat.time <= start) {
      continue;
    }

    result.times.push(stat.time);

    for (key in STAT_SERIES) {
      if (STAT_SERIES.hasOwnProperty(key)) {
        path = STAT_SERIES[key];

        result[key].push(stat[path[0]][path[1]]);
      }
    }
  }

  // have at least one entry, use it
  if (lastRaw !== null) {
    for (key in STAT_DISTRIBUTION) {
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
    for (key in STAT_DISTRIBUTION) {
      if (STAT_DISTRIBUTION.hasOwnProperty(key)) {
        result[key] = { values: [0,0,0,0,0,0,0], cuts: [0,0,0,0,0,0] };
      }
    }

    var ps = internal.processStatistics();

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
  var ss = internal.serverStatistics();

  result.physicalMemory = ss.physicalMemory;

  // add next start time
  if (lastRaw === null) {
    result.nextStart = internal.time();
    result.waitFor = STATISTICS_INTERVALL;
  }
  else {
    result.nextStart = lastRaw.time;
    result.waitFor = (lastRaw.time + STATISTICS_INTERVALL) - internal.time();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief computeStatisticsRaw15M
////////////////////////////////////////////////////////////////////////////////

function computeStatisticsRaw15M (result, start, clusterId) {
  'use strict';

  var filter = "";

  if (clusterId !== undefined) {
    filter = " FILTER s.clusterId == @clusterId ";
  }

  var values = db._query(
        "FOR s IN _statistics15 "
      + "  FILTER s.time > @start "
      +    filter
      + "  SORT s.time "
      + "  return s",
    { start: start - 2 * STATISTICS_HISTORY_INTERVALL, clusterId: clusterId });
  
  var lastRaw = null;
  var lastRaw2 = null;

  // read the last entries
  while (values.hasNext()) {
    var stat = values.next();

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
    var ps = internal.processStatistics();

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

////////////////////////////////////////////////////////////////////////////////
/// @brief computeStatisticsShort
////////////////////////////////////////////////////////////////////////////////

function computeStatisticsShort (start, clusterId) {
  'use strict';

  var result = {};

  computeStatisticsRaw(result, start, clusterId);
  computeStatisticsRaw15M(result, start, clusterId);

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief computeStatisticsValues
////////////////////////////////////////////////////////////////////////////////

function computeStatisticsValues (result, values, attrs) {
  'use strict';

  var key;

  for (key in attrs) {
    if (attrs.hasOwnProperty(key)) {
      result[key] = [];
    }
  }

  while (values.hasNext()) {
    var stat = values.next();

    result.times.push(stat.time);

    for (key in attrs) {
      if (attrs.hasOwnProperty(key) && STAT_SERIES.hasOwnProperty(key)) {
        var path = STAT_SERIES[key];

        result[key].push(stat[path[0]][path[1]]);
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief computeStatisticsLong
////////////////////////////////////////////////////////////////////////////////

function computeStatisticsLong (attrs, clusterId) {
  'use strict';

  var short = { times: [] };

  var filter = "";

  if (clusterId !== undefined) {
    filter = " FILTER s.clusterId == @clusterId ";
  }

  var values = db._query(
      "FOR s IN _statistics "
    +    filter
    + "  SORT s.time "
      + "  return s",
    { clusterId: clusterId });

  computeStatisticsValues(short, values, attrs);

  var filter2 = "";
  var end = 0;

  if (short.times.length !== 0) {
    filter2 = " FILTER s.time < @end ";
    end = short.times[0];
  }

  values = db._query(
    "FOR s IN _statistics15 "
    +    filter
    +    filter2
    + "  SORT s.time "
    + "  return s",
    { end: end, clusterId: clusterId });
  
  var long = { times: [] };

  computeStatisticsValues(long, values, attrs);

  var key;

  for (key in attrs) {
    if (attrs.hasOwnProperty(key) && long.hasOwnProperty(key)) {
      long[key] = long[key].concat(short[key]);
    }
  }

  if (! attrs.times) {
    delete long.times;
  }

  return long;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                     public routes
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief short term history
////////////////////////////////////////////////////////////////////////////////

controller.get("short", function (req, res) {
  'use strict';

  var start = req.params("start");
  var dbServer = req.params("DBserver");

  if (start !== null && start !== undefined) {
    start = parseFloat(start, 10);
  }
  else {
    start = internal.time() - STATISTICS_INTERVALL * 10;
  }

  var clusterId;

  if (dbServer === undefined) {
    if (cluster.isCluster()) {
      clusterId = ArangoServerState.id();
    }
  }
  else {
    clusterId = dbServer;
  }

  var series = computeStatisticsShort(start, clusterId);

  res.json(series);
}).summary("Returns the statistics")
  .notes("This function is used to get the statistics history.");

////////////////////////////////////////////////////////////////////////////////
/// @brief long term history
////////////////////////////////////////////////////////////////////////////////

controller.get("long", function (req, res) {
  'use strict';

  var filter = req.params("filter");
  var dbServer = req.params("DBserver");

  if (filter === undefined) {
    actions.resultError(req, res, actions.HTTP_BAD, "required parameter 'filter' was not given");
  }

  var attrs = {};
  var s = filter.split(",");
  var i;

  for (i = 0;  i < s.length;  ++i) {
    attrs[s[i]] = true;
  }

  var clusterId;

  if (dbServer === undefined) {
    if (cluster.isCluster()) {
      clusterId = ArangoServerState.id();
    }
  }
  else {
    clusterId = dbServer;
  }

  var series = computeStatisticsLong(attrs, clusterId);

  res.json(series);
}).summary("Returns the aggregated history")
  .notes("This function is used to get the aggregated statistics history.");

////////////////////////////////////////////////////////////////////////////////
/// @brief cluster statistics history
////////////////////////////////////////////////////////////////////////////////

controller.get("cluster", function (req, res) {
  'use strict';

  if (! cluster.isCoordinator()) {
    actions.resultError(req, res, actions.HTTP_FORBIDDEN, 0, "only allowed on coordinator");
    return;
  }

  if (! req.parameters.hasOwnProperty("DBserver")) {
    actions.resultError(req, res, actions.HTTP_BAD, "required parameter DBserver was not given");
    return;
  }

  var DBserver = req.parameters.DBserver;
  var type = req.parameters.type;
  var coord = { coordTransactionID: ArangoClusterInfo.uniqid() };
  var options = { coordTransactionID: coord.coordTransactionID, timeout:10 };

  if (type !== "short" && type !== "long") {
    type = "short";
  }

  var url = "/_admin/aardvark/statistics/" + type;
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
