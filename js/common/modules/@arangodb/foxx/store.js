'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / @brief Foxx service store
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2015 triagens GmbH, Cologne, Germany
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
// / @brief returns the fishbowl collection
// /
// / this will create the collection if it does not exist. this is better than
// / needlessly creating the collection for each database in case it is not
// / used in context of the database.
// //////////////////////////////////////////////////////////////////////////////

var getFishbowlStorage = function () {
  return db._collection('_fishbowl');
};

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
  var i;
  var tempPath = fs.getTempPath();
  var toSave = [];

  fs.makeDirectoryRecursive(tempPath);
  var root = fs.join(tempPath, 'foxx-apps-master/applications');

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

  var m = fs.listTree(root);
  var reSub = /(.*)\.json$/;
  var f, match, service, desc;

  for (i = 0; i < m.length; ++i) {
    f = m[i];
    match = reSub.exec(f);
    if (match === null) {
      continue;
    }

    service = fs.join(root, f);

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
    let fishbowl = getFishbowlStorage();
    if (!fishbowl) {
      // collection is not present. now create it
      try {
        fishbowl = db._create('_fishbowl', { isSystem: true, distributeShardsLike: '_graphs' });
      } catch (err) {
        try {
          fishbowl = db._create('_fishbowl', { isSystem: true, distributeShardsLike: '_users' });
        } catch (err) {
          require('console').warn('Unable to create _fishbowl collection for application results:' + String(err));
        }
      }
    }

    if (fishbowl) {
      let toRemove = {};
      fishbowl.toArray().forEach((doc) => {
        toRemove[doc._key] = { _key: doc._key };
      });
      toSave.forEach((doc) => {
        delete toRemove[doc._key];
      });
     
      // first insert/replace all apps that are new or remain
      fishbowl.insert(toSave, { overwriteMode: "replace" });

      // second remove all apps that are not present anymore
      // (very unlikely)
      toRemove = Object.values(toRemove);
      // execute remove and insert in 2 separate steps. this is not transactional
      // but cheaper
      if (toRemove.length > 0) {
        fishbowl.remove(toRemove);
      }

      require('console').debug('Updated local Foxx repository with ' + toSave.length + ' service(s)');
    }
  }
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief returns the search result for Foxx services
// //////////////////////////////////////////////////////////////////////////////

var searchJson = function (name) {
  var fishbowl = getFishbowlStorage();

  if (!fishbowl || fishbowl.count() === 0) {
    arangodb.print("Repository is empty, please use 'update'");

    return [];
  }

  if (name === undefined || (typeof name === 'string' && name.length === 0)) {
    return fishbowl.toArray();
  } else {
    name = name.replace(/[^a-zA-Z0-9]/g, ' ');

    // get results by looking in "description" and "name" attributes
    return db._query('FOR doc IN @@collection FILTER CONTAINS(doc.description, @name) || CONTAINS(doc.name, @name) RETURN doc', { name, '@collection': fishbowl.name() }).toArray();
  }
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

    if (!versionRange || semver.outside(serverVersion, versionRange, '<')) {
      // Explicitly backwards-incompatible with the server version: ignore
      continue;
    }
    if (!matchEngine || semver.satisfies(serverVersion, versionRange)) {
      return version;
    }
  }

  return fallback;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief returns all available Foxx services
// //////////////////////////////////////////////////////////////////////////////

function availableJson (matchEngine) {
  let result = [];

  let fishbowl = getFishbowlStorage();
  if (fishbowl && fishbowl.count() > 0) {
    let cursor = fishbowl.all();

    while (cursor.hasNext()) {
      let doc = cursor.next();
      let latestVersion = extractMaxVersion(matchEngine, doc.versions);

      if (latestVersion) {
        let serverVersion = plainServerVersion();
        let versionInfo = doc.versions[latestVersion];
        let legacy = Boolean(
          versionInfo.engines &&
          versionInfo.engines.arangodb &&
          !semver.satisfies(serverVersion, versionInfo.engines.arangodb)
        );
        let res = {
          name: doc.name,
          description: doc.description || '',
          author: doc.author || '',
          latestVersion,
          legacy,
          location: doc.versions[latestVersion].location,
          license: doc.license,
          categories: doc.keywords
        };

        result.push(res);
      }
    }
  }

  return result;
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
    var result = download(url, '', {
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
// / @brief prints all available Foxx services
// //////////////////////////////////////////////////////////////////////////////

var available = function (matchEngine) {
  var list = availableJson(matchEngine);

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
// / @brief gets json-info for an available Foxx service
// //////////////////////////////////////////////////////////////////////////////

var infoJson = function (name) {
  var fishbowl = getFishbowlStorage();

  if (!fishbowl || fishbowl.count() === 0) {
    arangodb.print("Repository is empty, please use 'update'");
    return;
  }

  var desc = db._query(
    'FOR u IN @@storage FILTER u.name == @name OR @name IN u.aliases RETURN DISTINCT u',
    { '@storage': fishbowl.name(), 'name': name }).toArray();

  if (desc.length === 0) {
    arangodb.print("No service '" + name + "' available, please try 'search'");
    return;
  } else if (desc.length > 1) {
    arangodb.print("Multiple services are named '" + name + "', please try 'search'");
    return;
  } else {
    return desc[0];
  }
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
exports.getFishbowlStorage = getFishbowlStorage;
exports.info = info;
exports.search = search;
exports.searchJson = searchJson;
exports.update = update;
