/*jshint strict: false */
/*global arango, db, assertTrue */

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
// / @author Lucas Dohmen
// / @author Copyright 2011-2012, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const arangodb = require('@arangodb');
const db = arangodb.db;
const internal = require('internal'); // OK: processCsvFile
const request = require('@arangodb/request');
const fs = require('fs');

let instanceInfo = null;

// execute callback function cb upto maxTries times.
// in case of failure, execute callback function failCb.
exports.runWithRetry = (cb, failCb, maxTries = 3) => {
  if (!failCb) {
    // this indicates an error in the test code. check call site
    // and verify that it sets a proper failure callback
    throw "test setup error - failure callback not set!";
  }
  let tries = 0;
  while (true) {
    try {
      cb();
      // return upon first successful execution of the callback function
      return;
    } catch (err) {
      // if it fails, check how many failures we got. fail only if we failed
      // 3 times in a row
      if (tries++ === maxTries) {
        throw err;
      }

      // attempt failed.
      failCb();
    }
  }
};

exports.versionHas = function (attribute) {
  if (global.hasOwnProperty('ARANGODB_CLIENT_VERSION')) {
    return global.ARANGODB_CLIENT_VERSION(true)[attribute] === 'true';
  } else {
    return db._version(true)[attribute] === 'true';
  }
};

exports.isEnterprise = function () {
  if (global.hasOwnProperty('ARANGODB_CLIENT_VERSION')) {
    return global.ARANGODB_CLIENT_VERSION(true).hasOwnProperty('enterprise-version');
  } else {
    return db._version(true).hasOwnProperty('enterprise-version');
  }
};

exports.transactionFailure = function (trx, errorCode, errorMessage, crashOnSuccess, abortArangoshOnly) {
  try {
    db._executeTransaction(trx);
  } catch (ex) {
    if ((!abortArangoshOnly || global.arango) &&  // only on arangosh...
      (ex instanceof arangodb.ArangoError) && // only regular arango errors has the subsequent:
      (ex.errorNum === errorCode) && // check for right error code
      (!errorMessage || (ex.message === errorMessage))) { // optional errorMessage
      if (crashOnSuccess) {
        if (global.hasOwnProperty('instanceManager')) {
          global.instanceManager.debugTerminate();
        } else if (internal.hasOwnProperty('debugTerminate')) {
          internal.debugTerminate('crashing server');
        } else {
          throw new Error('instance manager not found!');
        }
      }
      return 0;
    }
    console.log(ex);
  }
  return 1;
};

exports.truncateFailure = function (collection) {
  try {
    collection.truncate();
    return 1;
  } catch (ex) {
    if (!(ex instanceof arangodb.ArangoError) ||
      ex.errorNum !== internal.errors.ERROR_DEBUG.code) {
      throw ex;
    }
  }
  if (global.hasOwnProperty('instanceManager')) {
    global.instanceManager.debugTerminate();
  } else if (internal.hasOwnProperty('debugTerminate')) {
    internal.debugTerminate('crashing server');
  } else {
    throw new Error('instance manager not found!');
  }
};

function getInstanceInfo() {
  if (global.hasOwnProperty('instanceManager')) {
    instanceInfo = global.instanceManager;
  }
  if (instanceInfo === null) {
    instanceInfo = JSON.parse(internal.env.INSTANCEINFO);
    if (instanceInfo.arangods.length > 2) {
      instanceInfo.arangods.forEach(arangod => {
        arangod.id = fs.readFileSync(fs.join(arangod.dataDir, 'UUID')).toString();
      });
    }
  }
  return instanceInfo;
}

exports.getServerById = function (id) {
  const instanceInfo = getInstanceInfo();
  return instanceInfo.arangods.filter((d) => (d.id === id))[0];
};

exports.getServersByType = function (type) {
  const isType = (d) => (d.instanceRole.toLowerCase() === type);
  const instanceInfo = getInstanceInfo();
  return instanceInfo.arangods.filter(isType);
};

exports.getEndpointById = function (id) {
  const toEndpoint = (d) => (d.endpoint);
  const instanceInfo = getInstanceInfo();
  return instanceInfo.arangods.filter((d) => (d.id === id))
    .map(toEndpoint)[0];
};

exports.getUrlById = function (id) {
  const toUrl = (d) => (d.url);
  const instanceInfo = getInstanceInfo();
  return instanceInfo.arangods.filter((d) => (d.id === id))
    .map(toUrl)[0];
};

exports.getEndpointsByType = function (type) {
  const isType = (d) => (d.instanceRole.toLowerCase() === type);
  const toEndpoint = (d) => (d.endpoint);
  const endpointToURL = (endpoint) => {
    if (endpoint.substr(0, 6) === 'ssl://') {
      return 'https://' + endpoint.substr(6);
    }
    let pos = endpoint.indexOf('://');
    if (pos === -1) {
      return 'http://' + endpoint;
    }
    return 'http' + endpoint.substr(pos);
  };

  const instanceInfo = getInstanceInfo();
  return instanceInfo.arangods.filter(isType)
    .map(toEndpoint)
    .map(endpointToURL);
};

exports.helper = {
  process: function (file, processor) {
    internal.processCsvFile(file, function (raw_row, index) {
      if (index !== 0) {
        processor(raw_row.toString().split(','));
      }
    });
  },

  waitUnload: function (collection, waitForCollector) {
    let internal = require('internal');
    internal.wal.flush(true, waitForCollector || false);
  },
};

exports.deriveTestSuite = function (deriveFrom, deriveTo, namespace, blacklist = []) {
  for (let testcase in deriveFrom) {
    let targetTestCase = testcase + namespace;
    if (testcase === "setUp" ||
      testcase === "tearDown" ||
      testcase === "setUpAll" ||
      testcase === "tearDownAll" ||
      testcase === "internal") {
      targetTestCase = testcase;
    }
    if ((blacklist.length > 0) && blacklist.find(oneTestcase => testcase === oneTestcase)) {
      continue;
    }

    if (deriveTo.hasOwnProperty(targetTestCase)) {
      throw ("Duplicate testname - deriveTo already has the property " + targetTestCase);
    }
    deriveTo[targetTestCase] = deriveFrom[testcase];
  }
};

exports.deriveTestSuiteWithnamespace = function (deriveFrom, deriveTo, namespace) {
  let rc = {};
  for (let testcase in deriveTo) {
    let targetTestCase = testcase + namespace;
    if (testcase === "setUp" ||
      testcase === "tearDown" ||
      testcase === "setUpAll" ||
      testcase === "tearDownAll") {
      targetTestCase = testcase;
    }
    if (rc.hasOwnProperty(targetTestCase)) {
      throw ("Duplicate testname - rc already has the property " + targetTestCase);
    }
    rc[targetTestCase] = deriveTo[testcase];
  }

  for (let testcase in deriveFrom) {
    let targetTestCase = testcase + namespace;
    if (testcase === "setUp" ||
      testcase === "tearDown" ||
      testcase === "setUpAll" ||
      testcase === "tearDownAll") {
      targetTestCase = testcase;
    }
    if (rc.hasOwnProperty(targetTestCase)) {
      throw ("Duplicate testname - rc already has the property " + targetTestCase);
    }
    rc[targetTestCase] = deriveFrom[testcase];
  }
  return rc;
};

exports.typeName = function (value) {
  if (value === null) {
    return 'null';
  }
  if (value === undefined) {
    return 'undefined';
  }
  if (Array.isArray(value)) {
    return 'array';
  }

  var type = typeof value;

  if (type === 'object') {
    return 'object';
  }
  if (type === 'string') {
    return 'string';
  }
  if (type === 'boolean') {
    return 'boolean';
  }
  if (type === 'number') {
    return 'number';
  }

  throw 'unknown variable type';
};

exports.isEqual = function (lhs, rhs) {
  var ltype = exports.typeName(lhs), rtype = exports.typeName(rhs), i;

  if (ltype !== rtype) {
    return false;
  }

  if (ltype === 'null' || ltype === 'undefined') {
    return true;
  }

  if (ltype === 'array') {
    if (lhs.length !== rhs.length) {
      return false;
    }
    for (i = 0; i < lhs.length; ++i) {
      if (!exports.isEqual(lhs[i], rhs[i])) {
        return false;
      }
    }
    return true;
  }

  if (ltype === 'object') {
    var lkeys = Object.keys(lhs), rkeys = Object.keys(rhs);
    if (lkeys.length !== rkeys.length) {
      return false;
    }
    for (i = 0; i < lkeys.length; ++i) {
      var key = lkeys[i];
      if (!exports.isEqual(lhs[key], rhs[key])) {
        return false;
      }
    }
    return true;
  }

  if (ltype === 'boolean') {
    return (lhs === rhs);
  }
  if (ltype === 'string') {
    return (lhs === rhs);
  }
  if (ltype === 'number') {
    if (isNaN(lhs)) {
      return isNaN(rhs);
    }
    if (!isFinite(lhs)) {
      return (lhs === rhs);
    }
    return (lhs.toFixed(10) === rhs.toFixed(10));
  }

  throw 'unknown variable type';
};

// copied from lib/Basics/HybridLogicalClock.cpp
let decodeTable = [
  -1, -1, -1, -1, -1, -1, -1, -1,
  -1, -1, -1, -1, -1, -1, -1, -1,  //   0 - 15
  -1, -1, -1, -1, -1, -1, -1, -1,
  -1, -1, -1, -1, -1, -1, -1, -1,  //  16 - 31
  -1, -1, -1, -1, -1, -1, -1, -1,
  -1, -1, -1, -1, -1, 0, -1, -1,  //  32 - 47
  54, 55, 56, 57, 58, 59, 60, 61,
  62, 63, -1, -1, -1, -1, -1, -1,  //  48 - 63
  -1, 2, 3, 4, 5, 6, 7, 8,
  9, 10, 11, 12, 13, 14, 15, 16,  //  64 - 79
  17, 18, 19, 20, 21, 22, 23, 24,
  25, 26, 27, -1, -1, -1, -1, 1,  //  80 - 95
  -1, 28, 29, 30, 31, 32, 33, 34,
  35, 36, 37, 38, 39, 40, 41, 42,  //  96 - 111
  43, 44, 45, 46, 47, 48, 49, 50,
  51, 52, 53, -1, -1, -1, -1, -1,  // 112 - 127
  -1, -1, -1, -1, -1, -1, -1, -1,
  -1, -1, -1, -1, -1, -1, -1, -1,  // 128 - 143
  -1, -1, -1, -1, -1, -1, -1, -1,
  -1, -1, -1, -1, -1, -1, -1, -1,  // 144 - 159
  -1, -1, -1, -1, -1, -1, -1, -1,
  -1, -1, -1, -1, -1, -1, -1, -1,  // 160 - 175
  -1, -1, -1, -1, -1, -1, -1, -1,
  -1, -1, -1, -1, -1, -1, -1, -1,  // 176 - 191
  -1, -1, -1, -1, -1, -1, -1, -1,
  -1, -1, -1, -1, -1, -1, -1, -1,  // 192 - 207
  -1, -1, -1, -1, -1, -1, -1, -1,
  -1, -1, -1, -1, -1, -1, -1, -1,  // 208 - 223
  -1, -1, -1, -1, -1, -1, -1, -1,
  -1, -1, -1, -1, -1, -1, -1, -1,  // 224 - 239
  -1, -1, -1, -1, -1, -1, -1, -1,
  -1, -1, -1, -1, -1, -1, -1, -1   // 240 - 255
];

let decode = function (value) {
  let result = 0;
  if (value !== '0') {
    for (var i = 0, n = value.length; i < n; ++i) {
      result = (result * 2 * 2 * 2 * 2 * 2 * 2) + decodeTable[value.charCodeAt(i)];
    }
  }
  return result;
};

exports.compareStringIds = function (l, r) {
  if (l.length === r.length) {
    // strip common prefix because the accuracy of JS numbers is limited
    let prefixLength = 0;
    for (let i = 0; i < l.length; ++i) {
      if (l[i] !== r[i]) {
        break;
      }
      ++prefixLength;
    }
    if (prefixLength > 0) {
      l = l.substr(prefixLength);
      r = r.substr(prefixLength);
    }
  }

  l = decode(l);
  r = decode(r);
  if (l !== r) {
    return l < r ? -1 : 1;
  }
  return 0;
};

exports.endpointToURL = (endpoint) => {
  let protocol = endpoint.split('://')[0];
  switch (protocol) {
    case 'ssl':
      return 'https://' + endpoint.substr(6);
    case 'tcp':
      return 'http://' + endpoint.substr(6);
    default:
      var pos = endpoint.indexOf('://');
      if (pos === -1) {
        return 'http://' + endpoint;
      }
      return 'http' + endpoint.substr(pos);
  }
};

exports.checkIndexMetrics = (checkFunction) => {
  var isMetricArrived = false;
  var timeToWait = 100;
  while (!isMetricArrived) {
    try {
      checkFunction();
      isMetricArrived = true;
    } catch (err) {
      isMetricArrived = false;
      if (timeToWait > 0) {
        require("internal").sleep(1);
        timeToWait = timeToWait - 1;
      } else {
        throw err;
      }
    }
  }
};
