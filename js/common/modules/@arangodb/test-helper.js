/* jshint strict: false */

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

  rotate: function (collection) {
    var internal = require('internal');

    internal.wal.flush(true, true);
    internal.wait(1, false);

    var fig = collection.figures();
    var files = fig.datafiles.count + fig.journals.count;

    // wait for at most 15 seconds
    var end = internal.time() + 15;
    collection.rotate();

    while (internal.time() < end) {
      // wait until the figures change
      fig = collection.figures();
      if (fig.datafiles.count + fig.journals.count !== files) {
        break;
      }

      internal.wait(1);
    }
  }
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
