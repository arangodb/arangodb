/*global require, exports */

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

var throwFileNotFound = arangodb.throwFileNotFound;
var throwDownloadError = arangodb.throwDownloadError;
var errors = arangodb.errors;
var ArangoError = arangodb.ArangoError;
var mountRegEx = /^(\/[a-zA-Z0-9_\-%]+)+$/;
var mountAppRegEx = /\/APP(\/|$)/i;
var mountNumberRegEx = /^\/[\d\-%]/;
var pathRegex = /^((\.{0,2}(\/|\\))|(~\/)|[a-zA-Z]:\\)/;

var getStorage = function() {
  "use strict";
  var c = db._collection("_apps");
  if (c === null) {
    c = db._create("_apps", {isSystem: true});
    c.ensureUniqueConstraint("mount");
  }
  return c;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief comparator for mount points
////////////////////////////////////////////////////////////////////////////////

var compareMounts = function(l, r) {
  "use strict";
  var left = l.mount.toLowerCase();
  var right = r.mount.toLowerCase();

  if (left < right) {
    return -1;
  }
  return 1;
};

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

  fs.makeDirectory(path);
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
      var msg = "Could not download from repository '" + url + "'";
      if (result.hasOwnProperty('message')) {
        msg += " message: " + result.message;
      }
      throwDownloadError(msg);
    }
  }
  catch (err) {
    var msg = "Could not download from repository '" + url + "': " + String(err);
    if (err.hasOwnProperty('message')) {
      msg += " message: " + err.message;
    }
    throwDownloadError(msg);
  }

  repackZipFile(source);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns all running Foxx applications
////////////////////////////////////////////////////////////////////////////////

function listJson (showPrefix, onlyDevelopment) {
  'use strict';

  var mounts = getStorage();
  var cursor;
  if (onlyDevelopment) {
    cursor = mounts.byExample({isDevelopment: true});
  } else {
    cursor = mounts.all();
  }
  var result = [];
  var doc, res;
  while (cursor.hasNext()) {
    doc = cursor.next();

    res = {
      mountId: doc._key,
      mount: doc.mount,
      name: doc.name,
      description: doc.manifest.description,
      author: doc.manifest.author,
      system: doc.isSystem || false,
      development: doc.isDevelopment || false,
      version: doc.version,
      path: fs.join(fs.makeAbsolute(doc.root), doc.path)
    };

    if (showPrefix) {
      res.collectionPrefix = doc.options.collectionPrefix;
    }

    result.push(res);
  }

  return result;
}


////////////////////////////////////////////////////////////////////////////////
/// @brief prints all running Foxx applications
////////////////////////////////////////////////////////////////////////////////

function list(onlyDevelopment) {
  "use strict";
  var apps = listJson(undefined, onlyDevelopment);

  arangodb.printTable(
    apps.sort(compareMounts),
    [ "mount", "name", "author", "description", "version", "development" ],
    {
      prettyStrings: true,
      totalString: "%s application(s) found",
      emptyString: "no applications found",
      rename: {
        "mount": "Mount",
        "name" : "Name",
        "author" : "Author",
        "description" : "Description",
        "version" : "Version",
        "development" : "Development"
      }
    }
  );
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns all running Foxx applications in development mode
////////////////////////////////////////////////////////////////////////////////

function listDevelopmentJson (showPrefix) {
  'use strict';
  return listJson(showPrefix, true);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief prints all running Foxx applications
////////////////////////////////////////////////////////////////////////////////

function listDevelopment() {
  'use strict';
  return list(true);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief validate the mount point of an app
////////////////////////////////////////////////////////////////////////////////

function validateMount(mount, internal) {
  'use strict';
  if (mount[0] !== "/") {
    throw new ArangoError({
      errorNum: errors.ERROR_INVALID_MOUNTPOINT.code,
      errorMessage: "Mountpoint has to start with /."
    });
  }
  if (!mountRegEx.test(mount)) {
    // Controller routes may be /. Foxxes are not allowed to
    if (!internal || mount.length !== 1) {
      throw new ArangoError({
        errorNum: errors.ERROR_INVALID_MOUNTPOINT.code,
        errorMessage: "Mountpoint can only contain a-z, A-Z, 0-9 or _."
      });
    }
  }
  if (!internal) {
    if (mount[1] === "_") {
      throw new ArangoError({
        errorNum: errors.ERROR_INVALID_MOUNTPOINT.code,
        errorMessage: "/_ apps are reserved for internal use."
      });
    }
    if (mountNumberRegEx.test(mount)) {
      throw new ArangoError({
        errorNum: errors.ERROR_INVALID_MOUNTPOINT.code,
        errorMessage: "Mointpoints are not allowed to start with a number, - or %."
      });
    }
    if (mountAppRegEx.test(mount)) {
      throw new ArangoError({
        errorNum: errors.ERROR_INVALID_MOUNTPOINT.code,
        errorMessage: "Mountpoint is not allowed to contain /app/."
      });
    }
  }
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief validate an app name and fail if it is invalid
////////////////////////////////////////////////////////////////////////////////

function validateAppName (name) {
  'use strict';

  if (typeof name === 'string' && name.length > 0) {
    return;
  }

  throw new ArangoError({
    errorNum: errors.ERROR_APPLICATION_INVALID_NAME.code,
    errorMessage: errors.ERROR_APPLICATION_INVALID_NAME.message
  });
}


////////////////////////////////////////////////////////////////////////////////
/// @brief get the app mounted at this mount point
////////////////////////////////////////////////////////////////////////////////
function mountedApp (mount) {
  "use strict";
  return getStorage().firstExample({mount: mount});
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Update the app mounted at this mountpoint with the new app
////////////////////////////////////////////////////////////////////////////////
function updateApp (mount, update) {
  "use strict";
  return getStorage().updateByExample({mount: mount}, update);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief define regex for parameter types
////////////////////////////////////////////////////////////////////////////////
var typeToRegex = {
  "int": /[0-9]+/,
  "integer": /[0-9]+/,
  "boolean": /(true)|(false)/,
  "bool": /(true)|(false)/,
  "string": /[\w\W]*/
};

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a zip archive of a foxx app. Returns the absolute path
////////////////////////////////////////////////////////////////////////////////
var zipDirectory = function(directory) {
  "use strict";
  if (!fs.isDirectory(directory)) {
    throw directory + " is not a directory.";
  }
  var tempFile = fs.getTempFile("zip", false);

  var tree = fs.listTree(directory);
  var files = [];
  var i;
  var filename;

  for (i = 0;  i < tree.length;  ++i) {
    filename = fs.join(directory, tree[i]);

    if (fs.isFile(filename)) {
      files.push(tree[i]);
    }
  }
  if (files.length === 0) {
    throwFileNotFound("Directory '" + String(directory) + "' is empty");
  }
  fs.zipFile(tempFile, directory, files);
  return tempFile;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief Exports
////////////////////////////////////////////////////////////////////////////////

exports.mountedApp = mountedApp;
exports.updateApp = updateApp;
exports.list = list;
exports.listJson = listJson;
exports.listDevelopment = listDevelopment;
exports.listDevelopmentJson = listDevelopmentJson;
exports.buildGithubUrl = buildGithubUrl;
exports.repackZipFile = repackZipFile;
exports.processDirectory = processDirectory;
exports.processGithubRepository = processGithubRepository;
exports.validateAppName = validateAppName;
exports.validateMount = validateMount;
exports.typeToRegex = typeToRegex;
exports.zipDirectory = zipDirectory;
exports.getStorage = getStorage;
exports.pathRegex = pathRegex;

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

/// Local Variables:
/// mode: outline-minor
/// outline-regexp: "/// @brief\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}\\|/\\*jslint"
/// End:
