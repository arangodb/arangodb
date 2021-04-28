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
// / @author Lucas Dohmen
// / @author Copyright 2011-2012, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

let internal = require('internal'); // OK: processCsvFile
const request = require('@arangodb/request');
let { 
  getServerById,
  getServersByType,
  getEndpointById,
  getEndpointsByType,
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
exports.Helper = Helper;
exports.deriveTestSuite = deriveTestSuite;
exports.deriveTestSuiteWithnamespace = deriveTestSuiteWithnamespace;
exports.typeName = typeName;
exports.isEqual = isEqual;
exports.compareStringIds = compareStringIds;

/// @brief set failure point
exports.debugCanUseFailAt = function (endpoint) {
  let res = request.get({
    url: endpoint + '/_admin/debug/failat',
  });
  return res.status === 200;
};

/// @brief set failure point
exports.debugSetFailAt = function (endpoint, failAt) {
  let res = request.put({
    url: endpoint + '/_admin/debug/failat/' + failAt,
    body: ""
  });
  if (res.status !== 200) {
    throw "Error setting failure point";
  }
};

exports.debugResetRaceControl = function (endpoint) {
  let res = request.delete({
    url: endpoint + '/_admin/debug/raceControl',
    body: ""
  });
  if (res.status !== 200) {
    throw "Error resetting race control.";
  }
};

exports.debugRemoveFailAt = function (endpoint, failAt) {
  let res = request.delete({
    url: endpoint + '/_admin/debug/failat/' + failAt,
    body: ""
  });
  if (res.status !== 200) {
    throw "Error removing failure point";
  }
};

exports.debugClearFailAt = function (endpoint) {
  let res = request.delete({
    url: endpoint + '/_admin/debug/failat',
    body: ""
  });
  if (res.status !== 200) {
    throw "Error removing failure points";
  }
};

exports.getChecksum = function (endpoint, name) {
  let res = request.get({
    url: endpoint + '/_api/collection/' + name + '/checksum',
  });
  return JSON.parse(res.body).checksum;
};

exports.getMetric = function (endpoint, name) {

  let res = request.get({
    url: endpoint + '/_admin/metrics',
  });
  let re = new RegExp("^" + name);
  let matches = res.body.split('\n').filter((line) => !line.match(/^#/)).filter((line) => line.match(re));
  if (!matches.length) {
    throw "Metric " + name + " not found";
  }
  return Number(matches[0].replace(/^.* (\d+)$/, '$1'));
};
