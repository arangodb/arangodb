/*jshint strict: false */
/*global arango */

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

var internal = require('internal'); // OK: processCsvFile

var processCsvFile = internal.processCsvFile;

// wait for initial selfHeal in cluster
exports.waitForFoxxInitialized = function () {
  let internal = require("internal");
  if (!internal.isCluster()) {
    // the initial selfHeal is only required in cluster.
    // single server runs the selfHeal directly at startup.
    return;
  }

  let tries = 0;
  while (++tries < 4 * 30) {
    let isServer = require('@arangodb').isServer;
    if (isServer) {
      // arangod
      if (!global.KEYSPACE_EXISTS('FoxxFirstSelfHeal')) {
        return;
      }
    } else {
      // arangosh
      let result = arango.GET("/wenn-der-fuxxmann-zweimal-klingelt");
      if (result.code === 404) {
        // selfHeal was already executed - Foxx is ready!
        return;
      }
    }
    // otherwise we will likely see HTTP 500
    if (tries % 4 === 0) {
      require("console").warn("waiting for initial Foxx selfHeal to kick in");
    }
    internal.sleep(0.25);
  }
};

exports.Helper = {
  process: function (file, processor) {
    processCsvFile(file, function (raw_row, index) {
      if (index !== 0) {
        processor(raw_row.toString().split(','));
      }
    });
  },

  waitUnload: function (collection, waitForCollector) {
    var arangodb = require('@arangodb');
    var internal = require('internal');

    collection.unload();
    internal.wal.flush(true, waitForCollector || false);

    var iterations = 0;

    while (collection.status() !== arangodb.ArangoCollection.STATUS_UNLOADED) {
      collection.unload();
      internal.wait(0.25, true);

      ++iterations;

      if (iterations === 20) {
        require('console').log('waiting for collection ' + collection.name() + ' to unload');
      } else if (iterations === 400) {
        require('console').log('waited very long for unload of collection ' + collection.name());
      } else if (iterations === 1600) {
        throw 'waited too long for unload of collection ' + collection.name();
      }
    }
  },
};

exports.deriveTestSuite = function (deriveFrom, deriveTo, namespace) {
  for (let testcase in deriveFrom) {
    let targetTestCase = testcase + namespace;
    if (testcase === "setUp" ||
        testcase === "tearDown" ||
        testcase === "setUpAll" ||
        testcase === "tearDownAll") {
      targetTestCase = testcase;
    }
    if (deriveTo.hasOwnProperty(targetTestCase)) {
      throw("Duplicate testname - deriveTo already has the property " + targetTestCase);
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
	    throw("Duplicate testname - rc already has the property " + targetTestCase);
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
	    throw("Duplicate testname - rc already has the property " + targetTestCase);
	}
	rc[targetTestCase] = deriveFrom[testcase];
    }
    return rc;
};

exports.typeName = function  (value) {
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

exports.isEqual = function(lhs, rhs) {
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
  -1, -1, -1, -1, -1, 0,  -1, -1,  //  32 - 47
  54, 55, 56, 57, 58, 59, 60, 61,
  62, 63, -1, -1, -1, -1, -1, -1,  //  48 - 63
  -1, 2,  3,  4,  5,  6,  7,  8,
  9,  10, 11, 12, 13, 14, 15, 16,  //  64 - 79
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

let decode = function(value) {
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
