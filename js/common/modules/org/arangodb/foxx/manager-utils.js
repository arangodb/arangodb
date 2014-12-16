/*global require, exports, module */

////////////////////////////////////////////////////////////////////////////////
/// @brief ArangoDB Application Launcher Utilities
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2013 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Dr. Frank Celler
/// @author Michael Hackstein
/// @author Copyright 2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var fs = require("fs");
var arangodb = require("org/arangodb");
var db = arangodb.db;
var download = require("internal").download;
var checkedFishBowl = false;

var throwFileNotFound = arangodb.throwFileNotFound;
var throwDownloadError = arangodb.throwDownloadError;

////////////////////////////////////////////////////////////////////////////////
/// @brief builds a github repository URL
////////////////////////////////////////////////////////////////////////////////

function buildGithubUrl (repository, version) {
  'use strict';

  if (version === undefined) {
    version = "master";
  }

  return 'https://github.com/' + repository + '/archive/' + version + '.zip';
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts the name and version from a manifest file
////////////////////////////////////////////////////////////////////////////////

function extractNameAndVersionManifest (source, filename) {
  'use strict';

  var manifest = JSON.parse(fs.read(filename));

  source.name = manifest.name;
  source.version = manifest.version;
}


////////////////////////////////////////////////////////////////////////////////
/// @brief processes files in a directory
////////////////////////////////////////////////////////////////////////////////

function processDirectory (source) {
  'use strict';

  var location = source.location;

  if (! fs.exists(location) || ! fs.isDirectory(location)) {
    throwFileNotFound("'" + String(location) + "' is not a directory");
  }

  // .............................................................................
  // extract name and version from manifest
  // .............................................................................

  extractNameAndVersionManifest(source, fs.join(location, "manifest.json"));

  // .............................................................................
  // extract name and version from manifest
  // .............................................................................

  var tree = fs.listTree(location);
  var files = [];
  var i;
  var filename;

  for (i = 0;  i < tree.length;  ++i) {
    filename = fs.join(location, tree[i]);

    if (fs.isFile(filename)) {
      files.push(tree[i]);
    }
  }

  if (files.length === 0) {
    throwFileNotFound("Directory '" + String(location) + "' is empty");
  }

  var tempFile = fs.getTempFile("downloads", false);
  source.filename = tempFile;
  source.removeFile = true;

  fs.zipFile(tempFile, location, files);
  return tempFile;
}


////////////////////////////////////////////////////////////////////////////////
/// @brief extracts the name and version from a zip
////////////////////////////////////////////////////////////////////////////////

function repackZipFile (source) {
  'use strict';

  var i;

  var filename = source.filename;
  var path = fs.getTempFile("zip", false);

  fs.unzipFile(filename, path, false, true);

  // .............................................................................
  // locate the manifest file
  // .............................................................................

  var tree = fs.listTree(path).sort(function(a,b) {
    return a.length - b.length;
  });
  var found;
  var mf = "manifest.json";
  var re = /[\/\\\\]manifest\.json$/; // Windows!
  var tf;

  for (i = 0;  i < tree.length && found === undefined;  ++i) {
    tf = tree[i];

    if (re.test(tf) || tf === mf) {
      found = tf;
    }
  }

  if (found === undefined) {
    throwFileNotFound("Cannot find manifest file in zip file '" + filename + "'");
  }

  var mp;

  if (found === mf) {
    mp = ".";
  }
  else {
    mp = found.substr(0, found.length - mf.length - 1);
  }

  // .............................................................................
  // throw away source file if necessary
  // .............................................................................

  if (source.removeFile && source.filename !== '') {
    try {
      fs.remove(source.filename);
    }
    catch (err1) {
      arangodb.printf("Cannot remove temporary file '%s'\n", source.filename);
    }
  }

  delete source.filename;
  delete source.removeFile;

  // .............................................................................
  // repack the zip file
  // .............................................................................

  var newSource = { location: fs.join(path, mp) };

  processDirectory(newSource);

  source.name = newSource.name;
  source.version = newSource.version;
  source.filename = newSource.filename;
  source.removeFile = newSource.removeFile;

  // .............................................................................
  // cleanup temporary paths
  // .............................................................................

  try {
    fs.removeDirectoryRecursive(path);
  }
  catch (err2) {
    arangodb.printf("Cannot remove temporary directory '%s'\n", path);
  }
}
////////////////////////////////////////////////////////////////////////////////
/// @brief processes files from a github repository
////////////////////////////////////////////////////////////////////////////////

function processGithubRepository (source) {
  'use strict';

  var url = buildGithubUrl(source.location, source.version);
  var tempFile = fs.getTempFile("downloads", false);

  try {
    var result = download(url, "", {
      method: "get",
      followRedirects: true,
      timeout: 30
    }, tempFile);

    if (result.code >= 200 && result.code <= 299) {
      source.filename = tempFile;
      source.removeFile = true;
    }
    else {
      throwDownloadError("Could not download from repository '" + url + "'");
    }
  }
  catch (err) {
    throwDownloadError("Could not download from repository '" + url + "': " + String(err));
  }

  repackZipFile(source);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the fishbowl collection
///
/// this will create the collection if it does not exist. this is better than
/// needlessly creating the collection for each database in case it is not
/// used in context of the database.
////////////////////////////////////////////////////////////////////////////////

function getFishbowlStorage () {
  'use strict';

  var c = db._collection('_fishbowl');
  if (c ===  null) {
    c = db._create('_fishbowl', { isSystem : true });
  }

  if (c !== null && ! checkedFishBowl) {
    // ensure indexes
    c.ensureFulltextIndex("description");
    c.ensureFulltextIndex("name");
    checkedFishBowl = true;
  }

  return c;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns all available FOXX applications
////////////////////////////////////////////////////////////////////////////////

function availableJson() {
  'use strict';

  var fishbowl = getFishbowlStorage();
  var cursor = fishbowl.all();
  var result = [];

  while (cursor.hasNext()) {
    var doc = cursor.next();

    var maxVersion = "-";
    var versions = Object.keys(doc.versions);
    versions.sort(module.compareVersions);
    if (versions.length > 0) {
      versions.reverse();
      maxVersion = versions[0];
    }

    var res = {
      name: doc.name,
      description: doc.description || "",
      author: doc.author || "",
      latestVersion: maxVersion
    };

    result.push(res);
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the aal collection
////////////////////////////////////////////////////////////////////////////////

function getStorage () {
  'use strict';

  return db._collection('_aal');
}


////////////////////////////////////////////////////////////////////////////////
/// @brief returns all installed FOXX applications
////////////////////////////////////////////////////////////////////////////////

function listJson (showPrefix) {
  'use strict';

  var aal = getStorage();
  var cursor = aal.byExample({ type: "mount" });
  var result = [];

  while (cursor.hasNext()) {
    var doc = cursor.next();

    var version = doc.app.replace(/^.+:(\d+(\.\d+)*)$/g, "$1");

    var res = {
      mountId: doc._key,
      mount: doc.mount,
      appId: doc.app,
      name: doc.name,
      description: doc.description,
      author: doc.author,
      system: doc.isSystem ? "yes" : "no",
      active: doc.active ? "yes" : "no",
      version: version
    };

    if (showPrefix) {
      res.collectionPrefix = doc.options.collectionPrefix;
    }

    result.push(res);
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the fishbowl repository
////////////////////////////////////////////////////////////////////////////////

function getFishbowlUrl () {
  'use strict';

  return "arangodb/foxx-apps";
}

////////////////////////////////////////////////////////////////////////////////
/// @brief updates the fishbowl from a zip archive
////////////////////////////////////////////////////////////////////////////////

function updateFishbowlFromZip (filename) {
  'use strict';

  var i;
  var tempPath = fs.getTempPath();
  var toSave = [ ];

  try {
    fs.makeDirectoryRecursive(tempPath);
    var root = fs.join(tempPath, "foxx-apps-master/applications");

    // remove any previous files in the directory
    fs.listTree(root).forEach(function (file) {
      if (file.match(/\.json$/)) {
        try {
          fs.remove(fs.join(root, file));
        }
        catch (ignore) {
        }
      }
    });

    fs.unzipFile(filename, tempPath, false, true);

    if (! fs.exists(root)) {
      throw new Error("'applications' directory is missing in foxx-apps-master, giving up");
    }

    var m = fs.listTree(root);
    var reSub = /(.*)\.json$/;
    var f, match, app, desc;

    for (i = 0;  i < m.length;  ++i) {
      f = m[i];
      match = reSub.exec(f);

      if (match === null) {
        continue;
      }

      app = fs.join(root, f);

      try {
        desc = JSON.parse(fs.read(app));
      }
      catch (err1) {
        arangodb.printf("Cannot parse description for app '" + f + "': %s\n", String(err1));
        continue;
      }

      desc._key = match[1];

      if (! desc.hasOwnProperty("name")) {
        desc.name = match[1];
      }

      toSave.push(desc);
    }

    if (toSave.length > 0) {
      var fishbowl = getFishbowlStorage();

      db._executeTransaction({
        collections: {
          write: fishbowl.name()
        },
        action: function (params) {
          var c = require("internal").db._collection(params.collection);
          c.truncate();

          params.apps.forEach(function(app) {
            c.save(app);
          });
        },
        params: {
          apps: toSave,
          collection: fishbowl.name()
        }
      });

      arangodb.printf("Updated local repository information with %d application(s)\n",
                      toSave.length);
    }
  }
  catch (err) {
    if (tempPath !== undefined && tempPath !== "") {
      try {
        fs.removeDirectoryRecursive(tempPath);
      }
      catch (ignore) {
      }
    }

    throw err;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief downloads the fishbowl repository
////////////////////////////////////////////////////////////////////////////////

function updateFishbowl () {
  'use strict';

  var url = buildGithubUrl(getFishbowlUrl());
  var filename = fs.getTempFile("downloads", false);
  var path = fs.getTempFile("zip", false);

  try {
    var result = download(url, "", {
      method: "get",
      followRedirects: true,
      timeout: 30
    }, filename);

    if (result.code < 200 || result.code > 299) {
      throwDownloadError("Github download from '" + url + "' failed with error code " + result.code);
    }

    updateFishbowlFromZip(filename);

    filename = undefined;
  }
  catch (err) {
    if (filename !== undefined && fs.exists(filename)) {
      fs.remove(filename);
    }

    try {
      fs.removeDirectoryRecursive(path);
    }
    catch (ignore) {
    }

    throw err;
  }
}


////////////////////////////////////////////////////////////////////////////////
/// @brief Exports
////////////////////////////////////////////////////////////////////////////////

exports.updateFishbowl = updateFishbowl;
exports.listJson = listJson;
exports.getFishbowlStorage = getFishbowlStorage;
exports.availableJson = availableJson;
exports.buildGithubUrl = buildGithubUrl;
exports.repackZipFile = repackZipFile;
exports.processDirectory = processDirectory;
exports.processGithubRepository = processGithubRepository;

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

/// Local Variables:
/// mode: outline-minor
/// outline-regexp: "/// @brief\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}\\|/\\*jslint"
/// End:
