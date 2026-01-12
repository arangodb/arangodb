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
// / @brief searchs for an available Foxx services
// //////////////////////////////////////////////////////////////////////////////

var search = function (name) {
  var docs = searchJson(name);

  arangodb.printTable(
    docs.sort(compareServices),
    [ 'name', 'author', 'description' ],
    {
      prettyStrings: true,
      totalString: '%s service(s) found',
      emptyString: 'no services found',
      rename: {
        name: 'Name',
        author: 'Author',
        description: 'Description'
      }
    }
  );
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
// / @brief prints all available Foxx services
// //////////////////////////////////////////////////////////////////////////////

var available = function (matchEngine) {
  const list = availableJson(matchEngine);

  arangodb.printTable(
    list.sort(compareServices),
    [ 'name', 'author', 'description', 'latestVersion' ],
    {
      prettyStrings: true,
      totalString: '%s service(s) found',
      emptyString: "no services found, please use 'update'",
      rename: {
        'name': 'Name',
        'author': 'Author',
        'description': 'Description',
        'latestVersion': 'Latest Version'
      }
    }
  );
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief create a download URL for the given service information
// //////////////////////////////////////////////////////////////////////////////

var installationInfo = function (serviceInfo) {
  // TODO Validate
  let infoSplit = serviceInfo.split(':');
  let name = infoSplit[0];
  let version = infoSplit[1];
  let storeInfo = infoJson(name);

  if (!storeInfo) {
    return null;
  }

  let versions = storeInfo.versions;
  let versionInfo;

  if (!version) {
    let maxVersion = extractMaxVersion(true, versions);

    if (!maxVersion) {
      maxVersion = extractMaxVersion(false, versions);
    }

    if (!maxVersion) {
      throw new Error('No available version');
    }

    versionInfo = versions[maxVersion];
  } else {
    if (!versions.hasOwnProperty(version)) {
      throw new Error('Unknown version');
    }

    versionInfo = versions[version];
  }

  let url;
  if (!versionInfo.type) {
    url = versionInfo.location;
  } else if (versionInfo.type === 'github') {
    url = utils.buildGithubUrl(versionInfo.location, versionInfo.tag);
  } else {
    throw new Error(`Unknown location type ${versionInfo.type}`);
  }
  const manifest = {name};

  if (serviceInfo.author) {
    manifest.author = serviceInfo.author;
  }
  if (serviceInfo.description) {
    manifest.description = serviceInfo.description;
  }
  if (serviceInfo.license) {
    manifest.license = serviceInfo.license;
  }
  if (serviceInfo.keywords) {
    manifest.keywords = serviceInfo.keywords;
  }
  if (versionInfo.engines) {
    manifest.engines = versionInfo.engines;
  }
  if (versionInfo.provides) {
    manifest.provides = versionInfo.provides;
  }

  return {url, manifest};
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief prints info for an available Foxx service
// //////////////////////////////////////////////////////////////////////////////

var info = function (name) {
  var desc = infoJson(name);
  arangodb.printf('Name:        %s\n', desc.name);

  if (desc.hasOwnProperty('author')) {
    arangodb.printf('Author:      %s\n', desc.author);
  }

  var isSystem = desc.hasOwnProperty('isSystem') && desc.isSystem;
  arangodb.printf('System:      %s\n', JSON.stringify(isSystem));

  if (desc.hasOwnProperty('description')) {
    arangodb.printf('Description: %s\n\n', desc.description);
  }

  var header = false;
  var versions = Object.keys(desc.versions);
  versions.sort(semver.compare);

  versions.forEach(function (v) {
    var version = desc.versions[v];

    if (!header) {
      arangodb.print('Versions:');
      header = true;
    }

    if (version.type === 'github') {
      if (version.hasOwnProperty('tag')) {
        arangodb.printf('%s: fetch github "%s" "%s"\n', v, version.location, version.tag);
      } else if (v.hasOwnProperty('branch')) {
        arangodb.printf('%s: fetch github "%s" "%s"\n', v, version.location, version.branch);
      } else {
        arangodb.printf('%s: fetch "github" "%s"\n', v, version.location);
      }
    }
  });

  arangodb.printf('\n');
};

exports.available = available;
exports.availableJson = availableJson;
exports.installationInfo = installationInfo;
exports.info = info;
exports.search = search;
exports.searchJson = searchJson;
exports.update = update;
