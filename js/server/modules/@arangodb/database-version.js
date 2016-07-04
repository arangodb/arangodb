/* jshint strict: false */

// //////////////////////////////////////////////////////////////////////////////
// / @brief database version check
// /
// / @file
// /
// / Checks if the database needs to be upgraded.
// /
// / DISCLAIMER
// /
// / Copyright 2014 triAGENS GmbH, Cologne, Germany
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
// / @author Dr. Frank Celler
// / @author Copyright 2014, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

var cluster = require('@arangodb/cluster');
var fs = require('fs');
var db = require('@arangodb').db;
var console = require('console');

// //////////////////////////////////////////////////////////////////////////////
// / @brief logger
// //////////////////////////////////////////////////////////////////////////////

var logger = {
  info: function (msg) {
    console.log("In database '%s': %s", db._name(), msg);
  },

  error: function (msg) {
    console.error("In database '%s': %s", db._name(), msg);
  },

  log: function (msg) {
    this.info(msg);
  }
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief CURRENT_VERSION
// //////////////////////////////////////////////////////////////////////////////

exports.CURRENT_VERSION = (function () {
  var v = db._version().replace(/-[a-zA-Z0-9_\-]*$/g, '').split('.');

  var major = parseFloat(v[0], 10) || 0;
  var minor = parseFloat(v[1], 10) || 0;
  var patch = parseFloat(v[2], 10) || 0;

  return (((major * 100) + minor) * 100) + patch;
}());

// //////////////////////////////////////////////////////////////////////////////
// / @brief VERSION_MATCH
// //////////////////////////////////////////////////////////////////////////////

exports.VERSION_MATCH = 1;

// //////////////////////////////////////////////////////////////////////////////
// / @brief DOWNGRADE
// //////////////////////////////////////////////////////////////////////////////

exports.DOWNGRADE_NEEDED = 2;

// //////////////////////////////////////////////////////////////////////////////
// / @brief UPGRADE
// //////////////////////////////////////////////////////////////////////////////

exports.UPGRADE_NEEDED = 3;

// //////////////////////////////////////////////////////////////////////////////
// / @brief IS_CLUSTER
// //////////////////////////////////////////////////////////////////////////////

exports.IS_CLUSTER = -1;

// //////////////////////////////////////////////////////////////////////////////
// / @brief CANNOT_PARSE_VERSION_FILE
// //////////////////////////////////////////////////////////////////////////////

exports.CANNOT_PARSE_VERSION_FILE = -2;

// //////////////////////////////////////////////////////////////////////////////
// / @brief CANNOT_READ_VERSION_FILE
// //////////////////////////////////////////////////////////////////////////////

exports.CANNOT_READ_VERSION_FILE = -3;

// //////////////////////////////////////////////////////////////////////////////
// / @brief NO_VERSION_FILE
// //////////////////////////////////////////////////////////////////////////////

exports.NO_VERSION_FILE = -4;

// //////////////////////////////////////////////////////////////////////////////
// / @brief NO_SERVER_VERSION
// //////////////////////////////////////////////////////////////////////////////

exports.NO_SERVER_VERSION = -5;

// //////////////////////////////////////////////////////////////////////////////
// / @brief checks the version
// //////////////////////////////////////////////////////////////////////////////

exports.databaseVersion = function () {
  if (cluster.isCoordinator()) {
    console.debug('skip on corrdinator');

    return {
      result: exports.IS_CLUSTER
    };
  }

  // path to the VERSION file
  var versionFile = db._path() + '/VERSION';
  var lastVersion = null;

  // VERSION file exists, read its contents
  if (fs.exists(versionFile)) {
    var versionInfo = fs.read(versionFile);
    console.debug('found version file: ' + versionInfo);

    if (versionInfo !== '') {
      var versionValues = JSON.parse(versionInfo);

      if (versionValues && versionValues.version && !isNaN(versionValues.version)) {
        lastVersion = parseFloat(versionValues.version);
      } else {
        logger.error("Cannot parse VERSION file '" + versionFile + "': '" + versionInfo + "'");
        return {
          result: exports.CANNOT_PARSE_VERSION_FILE
        };
      }
    } else {
      logger.error("Cannot read VERSION file: '" + versionFile + "'");
      return {
        result: exports.CANNOT_READ_VERSION_FILE
      };
    }
  } else {
    console.debug('version file (' + versionFile + ') not found');

    return {
      result: exports.NO_VERSION_FILE
    };
  }

  // extract server version
  var currentVersion = exports.CURRENT_VERSION;

  // version match!
  if (Math.floor(lastVersion / 100) === Math.floor(currentVersion / 100)) {
    console.debug('version match: last version ' + lastVersion +
      ', current version ' + currentVersion);

    return {
      result: exports.VERSION_MATCH,
      serverVersion: currentVersion,
      databaseVersion: lastVersion
    };
  }

  // downgrade??
  if (lastVersion > currentVersion) {
    console.debug('downgrade: last version ' + lastVersion +
      ', current version ' + currentVersion);

    return {
      result: exports.DOWNGRADE_NEEDED,
      serverVersion: currentVersion,
      databaseVersion: lastVersion
    };
  }

  // upgrade
  if (lastVersion < currentVersion) {
    console.debug('upgrade: last version ' + lastVersion +
      ', current version ' + currentVersion);

    return {
      result: exports.UPGRADE_NEEDED,
      serverVersion: currentVersion,
      databaseVersion: lastVersion
    };
  }

  console.error('should not happen: last version ' + lastVersion +
    ', current version ' + currentVersion);

  return {
    result: exports.NO_VERSION_FILE
  };
};
