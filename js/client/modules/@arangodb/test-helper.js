/*jshint strict: false */
/*global arango, db */

// //////////////////////////////////////////////////////////////////////////////
// / @brief Helper for JavaScript Tests
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2010-2012 triagens GmbH, Cologne, Germany
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
// / Copyright holder is triAGENS GmbH, Cologne, Germany
// /
// / @author Wilfried Goesgens
// / @author Copyright 2011-2012, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

let internal = require('internal'); // OK: processCsvFile
let { 
  getServerById,
  getServersByType,
  getEndpointById,
  getEndpointsByType,
  waitForFoxxInitialized,
  Helper,
  deriveTestSuite,
  deriveTestSuiteWithnamespace,
  typeName,
  isEqual,
  compareStringIds,
    } = require('@arangodb/test-helper-common');

exports.getServerById = getServerById;
exports.getServersByType = getServersByType;
exports.getEndpointById = getEndpointById;
exports.getEndpointsByType = getEndpointsByType;
exports.waitForFoxxInitialized = waitForFoxxInitialized;
exports.Helper = Helper;
exports.deriveTestSuite = deriveTestSuite;
exports.deriveTestSuiteWithnamespace = deriveTestSuiteWithnamespace;
exports.typeName = typeName;
exports.isEqual = isEqual;
exports.compareStringIds = compareStringIds;

const primaryEndpoint = arango.getEndpoint();

let reconnectRetry = exports.reconnectRetry = require('@arangodb/replication-common').reconnectRetry;

/// @brief set failure point
exports.debugCanUseFailAt = function (endpoint) {
  try {
    reconnectRetry(endpoint, db._name(), "root", "");
    
    let res = arango.GET_RAW('/_admin/debug/failat');
    return res.code === 200;
  } finally {
    reconnectRetry(primaryEndpoint, "_system", "root", "");
  }
};

/// @brief set failure point
exports.debugSetFailAt = function (endpoint, failAt) {
  try {
    reconnectRetry(endpoint, db._name(), "root", "");
    let res = arango.PUT_RAW('/_admin/debug/failat/' + failAt, {});
    if (res.parsedBody !== true) {
      throw "Error setting failure point + " + res;
    }
    return true;
  } finally {
    reconnectRetry(primaryEndpoint, "_system", "root", "");
  }
};

exports.debugResetRaceControl = function (endpoint) {
  try {
    reconnectRetry(endpoint, db._name(), "root", "");
    let res = arango.DELETE_RAW('/_admin/debug/raceControl');
    if (res.code !== 200) {
      throw "Error resetting race control.";
    }
    return false;
  } finally {
    reconnectRetry(primaryEndpoint, "_system", "root", "");
  }
};

/// @brief remove failure point
exports.debugRemoveFailAt = function (endpoint, failAt) {
  try {
    reconnectRetry(endpoint, db._name(), "root", "");
    let res = arango.DELETE_RAW('/_admin/debug/failat/' + failAt);
    if (res.code !== 200) {
      throw "Error removing failure point";
    }
    return true;
  } finally {
    reconnectRetry(primaryEndpoint, "_system", "root", "");
  }
};

exports.debugClearFailAt = function (endpoint) {
  try {
    reconnectRetry(endpoint, db._name(), "root", "");
    let res = arango.DELETE_RAW('/_admin/debug/failat');
    if (res.code !== 200) {
      throw "Error removing failure points";
    }
    return true;
  } finally {
    reconnectRetry(primaryEndpoint, "_system", "root", "");
  }
};


exports.debugClearFailAt = function (endpoint) {
  try {
    reconnectRetry(endpoint, db._name(), "root", "");
    let res = arango.DELETE_RAW('/_admin/debug/failat');
    if (res.code !== 200) {
      throw "Error removing failure points";
    }
    return true;
  } finally {
    reconnectRetry(primaryEndpoint, "_system", "root", "");
  }
};

exports.getChecksum = function (endpoint, name) {
  try {
    reconnectRetry(endpoint, db._name(), "root", "");
    let res = arango.GET_RAW( '/_api/collection/' + name + '/checksum');
    if (res.code !== 200) {
      throw "Error getting collection checksum";
    }
    return res.parsedBody.checksum;
  } finally {
    reconnectRetry(primaryEndpoint, "_system", "root", "");
  }
};

exports.getMetric = function (endpoint, name) {
  try {
    reconnectRetry(endpoint, db._name(), "root", "");
    let res = arango.GET_RAW( '/_admin/metrics');
    if (res.code !== 200) {
      throw "error fetching metric";
    }
    let re = new RegExp("^" + name);
    let matches = res.body.split('\n').filter((line) => !line.match(/^#/)).filter((line) => line.match(re));
    if (!matches.length) {
      throw "Metric " + name + " not found";
    }
    return Number(matches[0].replace(/^.* (\d+)$/, '$1'));
  } finally {
    reconnectRetry(primaryEndpoint, "_system", "root", "");
  }
};
