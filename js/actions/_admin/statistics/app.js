/*jshint strict: false */
/*global ArangoServerState*/

// //////////////////////////////////////////////////////////////////////////////
// / @brief static information required for the cluster interface
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2014-2015 ArangoDB GmbH, Cologne, Germany
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
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
// / @author Michael hackstein
// / @author Copyright 2014-2015, ArangoDB GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

var internal = require('internal');
var db = internal.db;
var cluster = require('@arangodb/cluster');
var actions = require('@arangodb/actions');
var STATISTICS_INTERVAL = require('@arangodb/statistics').STATISTICS_INTERVAL;
var STATISTICS_HISTORY_INTERVAL = require('@arangodb/statistics').STATISTICS_HISTORY_INTERVAL;

// //////////////////////////////////////////////////////////////////////////////
// / @brief percentChange
// //////////////////////////////////////////////////////////////////////////////

function percentChange (current, prev, section, src) {
  if (prev === null) {
    return 0;
  }

  var p = prev[section][src];

  if (p !== 0) {
    return (current[section][src] - p) / p;
  }

  return 0;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief percentChange2
// //////////////////////////////////////////////////////////////////////////////

function percentChange2 (current, prev, section, src1, src2) {
  if (prev === null) {
    return 0;
  }

  var p = prev[section][src1] - prev[section][src2];

  if (p !== 0) {
    return (current[section][src1] - current[section][src2] - p) / p;
  }

  return 0;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief computeStatisticsRaw
// //////////////////////////////////////////////////////////////////////////////

var STAT_SERIES = {
  avgTotalTime: [ 'client', 'avgTotalTime' ],
  avgRequestTime: [ 'client', 'avgRequestTime' ],
  avgQueueTime: [ 'client', 'avgQueueTime' ],
  avgIoTime: [ 'client', 'avgIoTime' ],

  bytesSentPerSecond: [ 'client', 'bytesSentPerSecond' ],
  bytesReceivedPerSecond: [ 'client', 'bytesReceivedPerSecond' ],

  asyncPerSecond: [ 'http', 'requestsAsyncPerSecond' ],
  optionsPerSecond: [ 'http', 'requestsOptionsPerSecond' ],
  putsPerSecond: [ 'http', 'requestsPutPerSecond' ],
  headsPerSecond: [ 'http', 'requestsHeadPerSecond' ],
  postsPerSecond: [ 'http', 'requestsPostPerSecond' ],
  getsPerSecond: [ 'http', 'requestsGetPerSecond' ],
  deletesPerSecond: [ 'http', 'requestsDeletePerSecond' ],
  othersPerSecond: [ 'http', 'requestsOptionsPerSecond' ],
  patchesPerSecond: [ 'http', 'requestsPatchPerSecond' ],

  systemTimePerSecond: [ 'system', 'systemTimePerSecond' ],
  userTimePerSecond: [ 'system', 'userTimePerSecond' ],
  majorPageFaultsPerSecond: [ 'system', 'majorPageFaultsPerSecond' ],
  minorPageFaultsPerSecond: [ 'system', 'minorPageFaultsPerSecond' ]
};

var STAT_DISTRIBUTION = {
  totalTimeDistributionPercent: [ 'client', 'totalTimePercent' ],
  requestTimeDistributionPercent: [ 'client', 'requestTimePercent' ],
  queueTimeDistributionPercent: [ 'client', 'queueTimePercent' ],
  bytesSentDistributionPercent: [ 'client', 'bytesSentPercent' ],
  bytesReceivedDistributionPercent: [ 'client', 'bytesReceivedPercent' ]
};

function computeStatisticsRaw (result, start, clusterId) {
  var filter = '';

  if (clusterId !== undefined) {
    filter = ' FILTER s.clusterId == @clusterId ';
  }

  var values = db._query(
    'FOR s IN _statistics '
    + '  FILTER s.time > @start '
    + filter
    + '  SORT s.time '
    + '  return s',
    { start: start - 2 * STATISTICS_INTERVAL, clusterId: clusterId });

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
        result[key] = { values: [0, 0, 0, 0, 0, 0, 0], cuts: [0, 0, 0, 0, 0, 0] };
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
    result.waitFor = STATISTICS_INTERVAL;
  }else {
    result.nextStart = lastRaw.time;
    result.waitFor = (lastRaw.time + STATISTICS_INTERVAL) - internal.time();
  }
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief computeStatisticsRaw15M
// //////////////////////////////////////////////////////////////////////////////

function computeStatisticsRaw15M (result, start, clusterId) {
  var filter = '';

  if (clusterId !== undefined) {
    filter = ' FILTER s.clusterId == @clusterId ';
  }

  var values = db._query(
    'FOR s IN _statistics15 '
    + '  FILTER s.time > @start '
    + filter
    + '  SORT s.time '
    + '  return s',
    { start: start - 2 * STATISTICS_HISTORY_INTERVAL, clusterId: clusterId });

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

// //////////////////////////////////////////////////////////////////////////////
// / @brief computeStatisticsShort
// //////////////////////////////////////////////////////////////////////////////

function computeStatisticsShort (start, clusterId) {
  var result = {};

  computeStatisticsRaw(result, start, clusterId);
  computeStatisticsRaw15M(result, start, clusterId);
  result.enabled = internal.enabledStatistics();

  return result;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief computeStatisticsValues
// //////////////////////////////////////////////////////////////////////////////

function computeStatisticsValues (result, values, attrs) {
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

// //////////////////////////////////////////////////////////////////////////////
// / @brief computeStatisticsLong
// //////////////////////////////////////////////////////////////////////////////

function computeStatisticsLong (attrs, clusterId) {
  var short = { times: [] };

  var filter = '';

  if (clusterId !== undefined) {
    filter = ' FILTER s.clusterId == @clusterId ';
  }

  var values = db._query(
    'FOR s IN _statistics '
    + filter
    + '  SORT s.time '
    + '  return s',
    { clusterId: clusterId });

  computeStatisticsValues(short, values, attrs);

  var filter2 = '';
  var end = 0;

  if (short.times.length !== 0) {
    filter2 = ' FILTER s.time < @end ';
    end = short.times[0];
  }

  values = db._query(
    'FOR s IN _statistics15 '
    + filter
    + filter2
    + '  SORT s.time '
    + '  return s',
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

actions.defineHttp({
  url: '_admin/statistics/short',
  prefix: false,

  callback: function (req, res) {
    var start = req.parameters.start;
    var dbServer = req.parameters.DBserver;

    if (start !== null && start !== undefined) {
      start = parseFloat(start, 10);
    }else {
      start = internal.time() - STATISTICS_INTERVAL * 10;
    }

    var clusterId;

    if (dbServer === undefined) {
      if (cluster.isCluster()) {
        clusterId = ArangoServerState.id();
      }
    }else {
      clusterId = dbServer;
    }

    var series = computeStatisticsShort(start, clusterId);

    actions.resultOk(req, res, actions.HTTP_OK, series);
  }
});

actions.defineHttp({
  url: '_admin/statistics/long',
  prefix: false,

  callback: function (req, res) {
    var filter = req.params('filter');
    var dbServer = req.params('DBserver');

    if (filter === undefined) {
      actions.resultError(req, res, actions.HTTP_BAD, "required parameter 'filter' was not given");
    }

    var attrs = {};
    var s = filter.split(',');
    var i;

    for (i = 0;  i < s.length;  ++i) {
      attrs[s[i]] = true;
    }

    var clusterId;

    if (dbServer === undefined) {
      if (cluster.isCluster()) {
        clusterId = ArangoServerState.id();
      }
    }else {
      clusterId = dbServer;
    }

    var series = computeStatisticsLong(attrs, clusterId);

    actions.resultOk(req, res, actions.HTTP_OK, series);
  }
});
