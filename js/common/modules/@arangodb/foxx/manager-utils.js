'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / @brief ArangoDB Application Launcher Utilities
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2013 triAGENS GmbH, Cologne, Germany
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
// / @author Jan Steemann
// / @author Dr. Frank Celler
// / @author Michael Hackstein
// / @author Copyright 2014, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

var fs = require('fs');
var arangodb = require('@arangodb');
var db = arangodb.db;
var internal = require('internal');

var throwFileNotFound = arangodb.throwFileNotFound;
var errors = arangodb.errors;
var ArangoError = arangodb.ArangoError;
var mountRegEx = /^(\/[a-zA-Z0-9_\-%]+)+$/;
var mountAppRegEx = /\/APP(\/|$)/i;
var mountNumberRegEx = /^\/[\d\-%]/;
var pathRegex = /^((\.{0,2}(\/|\\))|(~\/)|[a-zA-Z]:\\)/;

const DEFAULT_REPLICATION_FACTOR_SYSTEM = internal.DEFAULT_REPLICATION_FACTOR_SYSTEM;

function getReadableName (name) {
  return name.charAt(0).toUpperCase() + name.substr(1)
  .replace(/([-_]|\s)+/g, ' ')
  .replace(/([a-z])([A-Z])/g, (m) => `${m[0]} ${m[1]}`)
  .replace(/([A-Z])([A-Z][a-z])/g, (m) => `${m[0]} ${m[1]}`)
  .replace(/\s([a-z])/g, (m) => ` ${m[0].toUpperCase()}`);
}

function getStorage () {
  let c = db._collection('_apps');
  if (c === null) {
    try {
      c = db._create('_apps', {
        isSystem: true,
        replicationFactor: DEFAULT_REPLICATION_FACTOR_SYSTEM,
        distributeShardsLike: '_graphs',
        journalSize: 4 * 1024 * 1024
      });
      c.ensureIndex({
        type: 'hash',
        fields: ['mount'],
        unique: true
      });
    } catch (e) {
      c = db._collection('_apps');
      if (!c) {
        throw e;
      }
    }
  }
  return c;
}

function getBundleStorage () {
  let c = db._collection('_appbundles');
  if (c === null) {
    try {
      c = db._create('_appbundles', {
        isSystem: true,
        replicationFactor: DEFAULT_REPLICATION_FACTOR_SYSTEM,
        distributeShardsLike: '_graphs',
        journalSize: 4 * 1024 * 1024
      });
    } catch (e) {
      c = db._collection('_appbundles');
      if (!c) {
        throw e;
      }
    }
  }
  return c;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief comparator for mount points
// //////////////////////////////////////////////////////////////////////////////

function compareMounts (l, r) {
  var left = l.mount.toLowerCase();
  var right = r.mount.toLowerCase();

  if (left < right) {
    return -1;
  }
  return 1;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief builds a github repository URL
// //////////////////////////////////////////////////////////////////////////////

function buildGithubUrl (repository, version) {
  if (version === undefined) {
    version = 'master';
  }

  var urlPrefix = require('process').env.FOXX_BASE_URL;
  if (urlPrefix === undefined) {
    urlPrefix = 'https://github.com/';
  }
  return urlPrefix + repository + '/archive/' + version + '.zip';
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief prints all running Foxx applications
// //////////////////////////////////////////////////////////////////////////////

function list (onlyDevelopment) {
  var services = getStorage().toArray().map((definition) => ({
    mount: definition.mount,
    development: definition.options.development
  }));

  arangodb.printTable(
    services.sort(compareMounts),
    [ 'mount', 'development' ],
    {
      prettyStrings: true,
      totalString: '%s service(s) found',
      emptyString: 'no services found',
      rename: {
        'mount': 'Mount',
        'development': 'Development'
      }
    }
  );
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief prints all running Foxx applications
// //////////////////////////////////////////////////////////////////////////////

function listDevelopment () {
  return list(true);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief validate the mount point of an app
// //////////////////////////////////////////////////////////////////////////////

function validateMount (mount, internal) {
  if (mount[0] !== '/') {
    throw new ArangoError({
      errorNum: errors.ERROR_INVALID_MOUNTPOINT.code,
      errorMessage: 'Mountpoint has to start with /.'
    });
  }
  if (!mountRegEx.test(mount)) {
    // Controller routes may be /. Foxxes are not allowed to
    if (!internal || mount.length !== 1) {
      throw new ArangoError({
        errorNum: errors.ERROR_INVALID_MOUNTPOINT.code,
        errorMessage: 'Mountpoint can only contain a-z, A-Z, 0-9 or _.'
      });
    }
  }
  if (!internal) {
    // routes starting with _ are disallowed...
    // ...except they start with _open. the _open prefix provides a non-authenticated
    // way to access routes
    if (mount[1] === '_' && !/^\/_open\//.test(mount)) {
      throw new ArangoError({
        errorNum: errors.ERROR_INVALID_MOUNTPOINT.code,
        errorMessage: '/_ apps are reserved for internal use.'
      });
    }
    if (mountNumberRegEx.test(mount)) {
      throw new ArangoError({
        errorNum: errors.ERROR_INVALID_MOUNTPOINT.code,
        errorMessage: 'Mointpoints are not allowed to start with a number, - or %.'
      });
    }
    if (mountAppRegEx.test(mount)) {
      throw new ArangoError({
        errorNum: errors.ERROR_INVALID_MOUNTPOINT.code,
        errorMessage: 'Mountpoint is not allowed to contain /app/.'
      });
    }
  }
  return true;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief get the app installed at this mount point
// //////////////////////////////////////////////////////////////////////////////

function getServiceDefinition (mount) {
  return getStorage().firstExample({mount});
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief Update the app installed at this mountpoint with the new app
// //////////////////////////////////////////////////////////////////////////////

function updateService (mount, update) {
  return getStorage().replaceByExample({mount}, update);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief Joins the last two diretories to one subdir, removes the unwanted original
// //////////////////////////////////////////////////////////////////////////////
function joinLastPath (tempPath) {
  var pathParts = tempPath.split(fs.pathSeparator).reverse();
  var individual = pathParts.shift();

  // we already have a directory which would be shared amongst tasks.
  // since we don't want that we remove it here.
  var voidDir = pathParts.slice().reverse().join(fs.pathSeparator);
  if (fs.isDirectory(voidDir)) {
    fs.removeDirectoryRecursive(voidDir);
  }

  var base = pathParts.shift();
  pathParts.unshift(base + '-' + individual);
  var rc = pathParts.reverse().join(fs.pathSeparator);

  return rc;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief creates a zip archive of a foxx app. Returns the absolute path
// //////////////////////////////////////////////////////////////////////////////
function zipDirectory (directory, zipFilename) {
  if (!fs.isDirectory(directory)) {
    throw new Error(directory + ' is not a directory.');
  }
  if (!zipFilename) {
    zipFilename = joinLastPath((fs.getTempFile('bundles', false)));
  }

  var tree = fs.listTree(directory);
  var files = [];
  var i;
  var filename;

  for (i = 0; i < tree.length; i++) {
    filename = fs.join(directory, tree[i]);

    if (fs.isFile(filename)) {
      files.push(tree[i]);
    }
  }
  if (files.length === 0) {
    throwFileNotFound("Directory '" + String(directory) + "' is empty");
  }
  // sort files to be sure they are always in same order within the zip file
  // independent of the OS and file system
  files.sort();
  fs.zipFile(zipFilename, directory, files);
  return zipFilename;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief Exports
// //////////////////////////////////////////////////////////////////////////////

exports.getServiceDefinition = getServiceDefinition;
exports.updateService = updateService;
exports.getReadableName = getReadableName;
exports.list = list;
exports.listDevelopment = listDevelopment;
exports.buildGithubUrl = buildGithubUrl;
exports.validateMount = validateMount;
exports.zipDirectory = zipDirectory;
exports.getStorage = getStorage;
exports.getBundleStorage = getBundleStorage;
exports.pathRegex = pathRegex;
exports.joinLastPath = joinLastPath;
