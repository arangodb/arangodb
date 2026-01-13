'use strict';

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
// / @author Dr. Frank Celler, Michael Hackstein
// / @author Copyright 2015, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const dd = require('dedent');
const arangodb = require('@arangodb');
const plainServerVersion = arangodb.plainServerVersion;
const db = arangodb.db;
const errors = arangodb.errors;
const ArangoError = arangodb.ArangoError;
const download = require('internal').download;
const fs = require('fs');
const utils = require('@arangodb/foxx/manager-utils');
const semver = require('semver');

// //////////////////////////////////////////////////////////////////////////////
// / @brief returns the fishbowl repository
// //////////////////////////////////////////////////////////////////////////////

function getFishbowlUrl () {
  return 'arangodb/foxx-apps';
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief comparator for services
// //////////////////////////////////////////////////////////////////////////////

var compareServices = function (l, r) {
  var left = l.name.toLowerCase();
  var right = r.name.toLowerCase();

  if (left < right) {
    return -1;
  }

  if (right < left) {
    return 1;
  }

  return 0;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief updates the fishbowl from a zip archive
// //////////////////////////////////////////////////////////////////////////////

var updateFishbowlFromZip = function (filename) {
  let tempPath = fs.getTempPath();

  fs.makeDirectoryRecursive(tempPath);
  let root = fs.join(tempPath, 'foxx-apps-master/applications');

  // remove any previous files in the directory
  fs.listTree(root).forEach(function (file) {
    if (file.match(/\.json$/)) {
      try {
        fs.remove(fs.join(root, file));
      } catch (ignore) {}
    }
  });

  fs.unzipFile(filename, tempPath, false, true);

  if (!fs.exists(root)) {
    throw new Error("'applications' directory is missing in foxx-apps-master, giving up");
  }

  const m = fs.listTree(root);
  const reSub = /(.*)\.json$/;

  let toSave = [];
  for (let i = 0; i < m.length; ++i) {
    let f = m[i];
    let match = reSub.exec(f);
    if (match === null) {
      continue;
    }

    let service = fs.join(root, f);
    let desc;
    try {
      desc = JSON.parse(fs.read(service));
    } catch (err1) {
      arangodb.printf("Cannot parse description for service '" + f + "': %s\n", String(err1));
      continue;
    }

    desc._key = match[1];

    if (!desc.hasOwnProperty('name')) {
      desc.name = match[1];
    }

    toSave.push(desc);
  }

  if (toSave.length > 0) {
    global.FISHBOWL_SET(toSave);
    require('console').debug('Updated local Foxx repository with ' + toSave.length + ' service(s)');
  }
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief extracts the highest version number from the document
// //////////////////////////////////////////////////////////////////////////////

function extractMaxVersion (matchEngine, versionDoc) {
  let serverVersion = plainServerVersion();
  let versions = Object.keys(versionDoc);
  versions.sort(semver.compare).reverse();
  let fallback;

  for (let version of versions) {
    let info = versionDoc[version];
    if (!info.engines || Object.keys(info.engines).length === 0) {
      // No known compatibility requirements indicated: use as last resort
      if (!matchEngine) {
        return version;
      }
      if (!fallback) {
        fallback = version;
      }
      continue;
    }
    let versionRange = info.engines.arangodb;

    if (!versionRange || semver.outside(serverVersion, versionRange, '<', {includePrerelease: true})) {
      // Explicitly backwards-incompatible with the server version: ignore
      continue;
    }
    if (!matchEngine || semver.satisfies(serverVersion, versionRange, {includePrerelease: true})) {
      return version;
    }
  }

  return fallback;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief updates the repository
// //////////////////////////////////////////////////////////////////////////////

var update = function () {
  let url = utils.buildGithubUrl(getFishbowlUrl());
  let filename = fs.getTempFile('bundles', false);
  let internal = require('internal');
    
  if (internal.isFoxxStoreDisabled && internal.isFoxxStoreDisabled()) {
    throw new Error('Foxx store is disabled in configuration');
  }

  try {
    let result = download(url, '', {
      method: 'get',
      followRedirects: true,
      timeout: 15
    }, filename);

    if (result.code < 200 || result.code > 299) {
      throw new ArangoError({
        errorNum: errors.ERROR_SERVICE_SOURCE_ERROR.code,
        errorMessage: dd`
          ${errors.ERROR_SERVICE_SOURCE_ERROR.message}
          URL: ${url}
          Status Code: ${result.code}
        `
      });
    }

    updateFishbowlFromZip(filename);

    filename = undefined;
  } catch (e) {
    if (filename !== undefined && fs.exists(filename)) {
      fs.remove(filename);
    }

    throw Object.assign(
      new Error('Failed to update Foxx store: ' + String(e.message)),
      {cause: e}
    );
  }
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief prints info for an available Foxx service
// //////////////////////////////////////////////////////////////////////////////