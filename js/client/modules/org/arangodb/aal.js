/*jslint indent: 2, nomen: true, maxlen: 120, vars: true, white: true, plusplus: true, nonpropdel: true, continue: true, regexp: true */
/*global require, exports */

////////////////////////////////////////////////////////////////////////////////
/// @brief ArangoDB Application Launcher
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
/// @author Copyright 2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var internal = require("internal");

var console = require("console");
var fs = require("fs");

var arangodb = require("org/arangodb");
var arangosh = require("org/arangodb/arangosh");

var ArangoError = arangodb.ArangoError;
var arango = internal.arango;
var db = arangodb.db;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoAPI
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the aal collection
////////////////////////////////////////////////////////////////////////////////

function getStorage () {
  'use strict';

  return db._collection('_aal');
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the aal collection
////////////////////////////////////////////////////////////////////////////////

function getFishbowlStorage () {
  'use strict';

  return db._collection('_fishbowl');
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the fishbow repository
////////////////////////////////////////////////////////////////////////////////

function getFishbowlUrl (version) {
  'use strict';

  return "triAGENS/ArangoDB-Apps";
}

////////////////////////////////////////////////////////////////////////////////
/// @brief builds a github repository URL
////////////////////////////////////////////////////////////////////////////////

function buildGithubUrl (repository, version) {
  'use strict';

  if (typeof version === "undefined") {
    version = "master";
  }

  return 'https://github.com/' + repository + '/archive/' + version + '.zip';
}

////////////////////////////////////////////////////////////////////////////////
/// @brief builds a github repository URL
////////////////////////////////////////////////////////////////////////////////

function buildGithubFishbowlUrl (name) {
  'use strict';

  return "https://raw.github.com/" + getFishbowlUrl() + "/master/Fishbowl/" + name + ".json";
}

////////////////////////////////////////////////////////////////////////////////
/// @brief thrown an error in case a download failed
////////////////////////////////////////////////////////////////////////////////

function throwDownloadError (msg) {
  'use strict';

  var err = new ArangoError();
  err.errorNum = arangodb.errors.ERROR_APPLICATION_DOWNLOAD_FAILED.code;
  err.errorMessage = arangodb.errors.ERROR_APPLICATION_DOWNLOAD_FAILED.message + ': ' + String(msg);

  throw err;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief validate an app name and fail if it is invalid
////////////////////////////////////////////////////////////////////////////////

function validateAppName (name) {
  'use strict';

  if (typeof name === 'string' && name.length > 0) {
    return;
  }

  var err = new ArangoError();
  err.errorNum = arangodb.errors.ERROR_APPLICATION_INVALID_NAME.code;
  err.errorMessage = arangodb.errors.ERROR_APPLICATION_INVALID_NAME.message;

  throw err;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief validate a mount and fail if it is invalid
////////////////////////////////////////////////////////////////////////////////

function validateMount (mnt) {
  'use strict';

  if (typeof mnt === 'string' && mnt.substr(0, 1) === '/') {
    return;
  }

  var err = new ArangoError();
  err.errorNum = arangodb.errors.ERROR_APPLICATION_INVALID_MOUNT.code;
  err.errorMessage = arangodb.errors.ERROR_APPLICATION_INVALID_MOUNT.message;

  throw err;
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

  var i;
  var location = source.location;


  if (! fs.exists(location) || ! fs.isDirectory(location)) {
    throw "could not find directory";
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

  for (i = 0;  i < tree.length;  ++i) {
    var filename = fs.join(location, tree[i]);

    if (fs.isFile(filename)) {
      files.push(tree[i]);
    }
  }

  if (files.length === 0) {
    throw "directory '" + location + "' is empty";
  }

  var tempFile = fs.getTempFile("downloads", false); 
  source.filename = tempFile;
  source.removeFile = true;
    
  fs.zipFile(tempFile, location, files);
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

  var tree = fs.listTree(path).sort(function(a,b) { return a.length - b.length; });
  var found;
  var mf = "manifest.json";
  var re = /\/manifest\.json$/;

  for (i = 0;  i < tree.length && found === undefined;  ++i) {
    var tf = tree[i];

    if (re.test(tf) || tf === mf) {
      found = tf;
    }
  }

  if (found === "undefined") {
    throw "cannot find manifest file in '" + filename + "'";
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
    catch (err2) {
      console.warn("cannot remove temporary file '%s'", source.filename);
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
  catch (err) {
    console.warn("cannot remove temporary directory '%s'", path);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief processes files in a zip file
////////////////////////////////////////////////////////////////////////////////

function processZip (source) {
  'use strict';

  var location = source.location;

  if (! fs.exists(location) || ! fs.isFile(location)) {
    throw "could not find zip file '" + location + "'";
  }

  source.filename = source.location;
  source.removeFile = false;

  repackZipFile(source);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief processes files from a github repository 
////////////////////////////////////////////////////////////////////////////////

function processGithubRepository (source) {
  'use strict';

  var url = buildGithubUrl(source.location, source.version);
  var tempFile = fs.getTempFile("downloads", false); 

  try {
    var result = internal.download(url, "get", tempFile);

    if (result.code >= 200 && result.code <= 299) {
      source.filename = tempFile;
      source.removeFile = true;
    }
    else {
      throwDownloadError("could not download from repository '" + url + "'");
    }
  }
  catch (err) {
    throwDownloadError("could not download from repository '" + url + "': " + String(err));
  }

  repackZipFile(source);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief processes a source declaration
////////////////////////////////////////////////////////////////////////////////

function processSource (src) {
  'use strict';

  if (src.type === "zip") {
    processZip(src);
  }
  else if (src.type === "directory") {
    processDirectory(src);
  }
  else if (src.type === "github") {
    processGithubRepository(src);
  }
  else {
    throw "unknown application type '" + src.type + "'";
  }

  // upload file to the server 
  var response = internal.arango.SEND_FILE("/_api/upload", src.filename);

  if (src.removeFile && src.filename !== '') {
    try {
      fs.remove(src.filename);
    }
    catch (err2) {
      console.warn("cannot remove temporary file '%s'", src.filename);
    }
  } 

  if (! response.filename) {
    throw "could not upload application to arangodb";
  }

  return response.filename;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief updates the fishbowl from a zip archive
////////////////////////////////////////////////////////////////////////////////

function updateFishbowlFromZip (filename) {
  'use strict';

  var i;
  var tempPath = fs.getTempPath();
  var fishbowl = getFishbowlStorage();

  try {
    fs.makeDirectoryRecursive(tempPath);
    fs.unzipFile(filename, tempPath, false, true);

    var root = fs.join(tempPath, "ArangoDB-Apps-master/Fishbowl");

    if (! fs.exists(root)) {
      throw new Error("fishbowl not found");
    }

    var m = fs.listTree(root);
    var reSub = /(.*)\.json$/;
    
    for (i = 0;  i < m.length;  ++i) {
      var f = m[i];
      var match = reSub.exec(f);

      if (match === null) {
        continue;
      }

      var app = fs.join(root, f);
      var desc;

      try {
        desc = JSON.parse(fs.read(app));
      }
      catch (err1) {
        console.error("cannot parse description for app '" + f + "': %s", String(err1));
        continue;
      }

      desc._key = match[1];

      if (! desc.hasOwnProperty("name")) {
        desc.name = match[1];
      }

      try {
        try {
          fishbowl.save(desc);
        }
        catch (err3) {
          fishbowl.replace(desc._key, desc);
        }
      }
      catch (err2) {
        console.error("cannot save description for app '" + f + "': %s", String(err2));
        continue;
      }
    }
  }
  catch (err) {
    if (tempPath !== undefined && tempPath !== "") {
      fs.removeDirectoryRecursive(tempPath);
    }

    throw err;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief downloads the fishbowl repository
////////////////////////////////////////////////////////////////////////////////

function updateFishbowl () {
  'use strict';

  var i;

  var url = buildGithubUrl(getFishbowlUrl());
  var filename = fs.getTempFile("downloads", false); 
  var path = fs.getTempFile("zip", false); 

  try {
    var result = internal.download(url, "get", filename);

    if (result.code < 200 || result.code > 299) {
      throw "github download failed";
    }

    updateFishbowlFromZip(filename);

    filename = undefined;
  }
  catch (err) {
    if (filename !== undefined) {
      fs.remove(filename);
    }

    fs.removeDirectoryRecursive(path);

    throw err;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoAPI
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief loads a FOXX application
////////////////////////////////////////////////////////////////////////////////

exports.load = function (type, location, version) {
  'use strict';

  var source;
  var filename;
  var req;
  var res;

  if (typeof location === "undefined") {
    throw "missing type";
  }
  else {
    source = { type: type, location: location, version: version };
  }

  filename = processSource(source);

  if (typeof source.name === "undefined") {
    throw "name is missing for '" + JSON.stringify(source) + "'";
  }

  if (typeof source.version === "undefined") {
    throw "version is missing for '" + JSON.stringify(source) + "'";
  }

  req = {
    name: source.name,
    version: source.version,
    filename: filename
  };

  res = arango.POST("/_admin/foxx/load", JSON.stringify(req));
  arangosh.checkRequestResult(res);

  return { path: res.path };
};

////////////////////////////////////////////////////////////////////////////////
/// @brief installs (aka mounts) a FOXX application
////////////////////////////////////////////////////////////////////////////////

exports.installApp = function (appId, mount, options) {
  'use strict';

  var res;
  var req = {
    appId: appId,
    mount: mount,
    options: options
  };
  
  validateAppName(appId);
  validateMount(mount);

  res = arango.POST("/_admin/foxx/install", JSON.stringify(req));
  arangosh.checkRequestResult(res);

  return { appId: res.appId, mountId: res.mountId };
};

////////////////////////////////////////////////////////////////////////////////
/// @brief installs (aka mounts) a FOXX dev application
////////////////////////////////////////////////////////////////////////////////

exports.installDevApp = function (name, mount, options) {
  'use strict';

  var res;
  var req = {
    name: name,
    mount: mount,
    options: options
  };
  
  validateAppName(name);
  validateMount(mount);

  res = arango.POST("/_admin/foxx/dev-install", JSON.stringify(req));
  arangosh.checkRequestResult(res);

  return { appId: res.appId, mountId: res.mountId };
};

////////////////////////////////////////////////////////////////////////////////
/// @brief uninstalls (aka unmounts) a FOXX application
////////////////////////////////////////////////////////////////////////////////

exports.uninstallApp = function (key) {
  'use strict';

  var res;
  var req = {
    key: key
  };

  validateAppName(key);

  res = arango.POST("/_admin/foxx/uninstall", JSON.stringify(req));
  arangosh.checkRequestResult(res);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief prints all installed FOXX applications
////////////////////////////////////////////////////////////////////////////////

exports.printInstalled = function (showPrefix) {
  'use strict';

  var list = exports.listInstalled(showPrefix);

  if (showPrefix) {
    arangodb.printTable(
      list,
      ["MountID", "AppID", "CollectionPrefix", "Active", "Devel"]);
  }
  else {
    arangodb.printTable(
      list,
      ["MountID", "AppID", "Mount", "Active", "Devel"]);
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief lists all installed FOXX applications
////////////////////////////////////////////////////////////////////////////////

exports.listInstalled = function (showPrefix) {
  'use strict';

  var aal = getStorage();
  var cursor = aal.byExample({ type: "mount" });
  var result = [];

  while (cursor.hasNext()) {
    var doc = cursor.next();
    var res;

    if (showPrefix) {
      res = {
        MountID: doc._key,
        AppID: doc.app,
        CollectionPrefix: doc.collectionPrefix,
        Active: doc.active ? "yes" : "no",
        Devel: doc.development ? "yes" : "no"
      };
    }
    else {
      res = {
        MountID: doc._key,
        AppID: doc.app,
        Mount: doc.mount,
        Active: doc.active ? "yes" : "no",
        Devel: doc.development ? "yes" : "no"
      };
    }

    result.push(res);
  }

  return result;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief prints all loaded FOXX applications
////////////////////////////////////////////////////////////////////////////////

exports.printAvailable = function () {
  'use strict';

  var list = exports.listAvailable();

  arangodb.printTable(
    list,
    ["AppID", "Name", "Version", "Path"]);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief lists all loaded FOXX applications
////////////////////////////////////////////////////////////////////////////////

exports.listAvailable = function () {
  'use strict';

  var aal = getStorage();
  var cursor = aal.byExample({ type: "app" });
  var result = [];

  while (cursor.hasNext()) {
    var doc = cursor.next();
    var res = {
      AppID: doc.app,
      Name: doc.name,
      Version: doc.version,
      Path: doc.path
    };

    result.push(res);
  }

  return result;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief lists all FOXX applications from the FishBowl
////////////////////////////////////////////////////////////////////////////////

exports.listFishbowl = function () {
  'use strict';

  var fishbowl = getFishbowlStorage();
  var cursor = fishbowl.all();
  var result = [];

  while (cursor.hasNext()) {
    var doc = cursor.next();
    var res = {
      name: doc.name,
      description: doc.description,
      author: doc.author
    };

    result.push(res);
  }

  return result;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief lists all FOXX applications from the FishBowl
////////////////////////////////////////////////////////////////////////////////

exports.printFishbowl = function () {
  'use strict';

  var compare = function (a,b) {
    if (a.name < b.name) {return -1;}
    if (b.name < a.name) {return 1;}
    return 0;
  };

  var list = exports.listFishbowl();

  if (list.length === 0) {
    arangodb.print("Fishbowl is empty, please use 'updateFishbowl'");
  }
  else {
    arangodb.printTable(
      list.sort(compare),
      [ "name", "author", "description" ]);
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief searchs for FOXX applications from the FishBowl
////////////////////////////////////////////////////////////////////////////////

exports.search = function (name) {
  'use strict';

  var fishbowl = getFishbowlStorage();

  if (fishbowl.count() === 0) {
    arangodb.print("Fishbowl is empty, please use 'updateFishbowl'");

    return [];
  }

  var docs = fishbowl.fulltext("description", "prefix:" + name).toArray();
  docs = docs.concat(fishbowl.fulltext("name", "prefix:" + name).toArray());

  arangodb.printTable(
    docs,
    [ "name", "author", "description" ]);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief details for a FOXX application
////////////////////////////////////////////////////////////////////////////////

exports.details = function (name) {
  'use strict';

  var i;

  validateAppName(name);

  var fishbowl = getFishbowlStorage();

  if (fishbowl.count() === 0) {
    arangodb.print("Fishbowl is empty, please use 'updateFishbowl'");
    return;
  }

  var desc;

  try {
    desc = fishbowl.document(name);
  }
  catch (err) {
    arangodb.print("No '" + name + "' in Fishbowl, please try 'search'");
    return;
  }

  internal.printf("Name: %s\n", desc.name);

  if (desc.hasOwnProperty('author')) {
    internal.printf("Author: %s\n", desc.author);
  }

  if (desc.hasOwnProperty('description')) {
    internal.printf("\nDescription:\n%s\n\n", desc.description);
  }

  for (i in desc.versions) {
    if (desc.versions.hasOwnProperty(i)) {
      var v = desc.versions[i];

      if (v.type === "github") {
        if (v.hasOwnProperty("tag")) {
          internal.printf('%s: aal.load("github", "%s", "%s");\n', i, v.location, v.tag);
        }
        else if (v.hasOwnProperty("branch")) {
          internal.printf('%s: aal.load("github", "%s", "%s");\n', i, v.location, v.branch);
        }
        else {
          internal.printf('%s: aal.load("github", "%s");\n', i, v.location);
        }
      }
    }
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief updates the Fishbowl
////////////////////////////////////////////////////////////////////////////////

exports.updateFishbowl = updateFishbowl;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}\\|/\\*jslint"
// End:
