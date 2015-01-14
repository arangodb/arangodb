/*jslint continue:true */
/*global require, exports*/

////////////////////////////////////////////////////////////////////////////////
/// @brief Foxx application store
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2015 triagens GmbH, Cologne, Germany
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
/// @author Dr. Frank Celler, Michael Hackstein
/// @author Copyright 2015, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

(function() {
  "use strict";


////////////////////////////////////////////////////////////////////////////////
/// Global variables
////////////////////////////////////////////////////////////////////////////////

  var checkedFishBowl = false;

////////////////////////////////////////////////////////////////////////////////
/// Section imports
////////////////////////////////////////////////////////////////////////////////

  var arangodb = require("org/arangodb");
  var db = arangodb.db;
  var download = require("internal").download;
  var fs = require("fs");
  var throwDownloadError = arangodb.throwDownloadError;
  var utils = require("org/arangodb/foxx/manager-utils");

////////////////////////////////////////////////////////////////////////////////
/// Section private functions
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the fishbowl repository
////////////////////////////////////////////////////////////////////////////////

  function getFishbowlUrl () {
    return "arangodb/foxx-apps";
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the fishbowl collection
///
/// this will create the collection if it does not exist. this is better than
/// needlessly creating the collection for each database in case it is not
/// used in context of the database.
////////////////////////////////////////////////////////////////////////////////

  var getFishbowlStorage = function() {

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
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief comparator for applications
////////////////////////////////////////////////////////////////////////////////

  var compareApps =  function(l, r) {
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

////////////////////////////////////////////////////////////////////////////////
/// @brief comparator for versions
////////////////////////////////////////////////////////////////////////////////

  var compareVersions = function (a, b) {
    var i;

    if (a === b) {
      return 0;
    }

    // error handling
    if (typeof a !== "string") {
      return -1;
    }
    if (typeof b !== "string") {
      return 1;
    }

    var aComponents = a.split(".");
    var bComponents = b.split(".");
    var len = Math.min(aComponents.length, bComponents.length);

    // loop while the components are equal
    for (i = 0; i < len; i++) {

      // A bigger than B
      if (parseInt(aComponents[i], 10) > parseInt(bComponents[i], 10)) {
        return 1;
      }

      // B bigger than A
      if (parseInt(aComponents[i], 10) < parseInt(bComponents[i], 10)) {
        return -1;
      }
    }

    // If one's a prefix of the other, the longer one is bigger one.
    if (aComponents.length > bComponents.length) {
      return 1;
    }

    if (aComponents.length < bComponents.length) {
      return -1;
    }

    // Otherwise they are the same.
    return 0;
  };


////////////////////////////////////////////////////////////////////////////////
/// @brief updates the fishbowl from a zip archive
////////////////////////////////////////////////////////////////////////////////

  var updateFishbowlFromZip = function(filename) {
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

        if (match !== null) {
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
  };

////////////////////////////////////////////////////////////////////////////////
/// Section public functions
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
/// @brief returns the search result for FOXX applications
////////////////////////////////////////////////////////////////////////////////

  var searchJson = function (name) {

    var fishbowl = getFishbowlStorage();

    if (fishbowl.count() === 0) {
      arangodb.print("Repository is empty, please use 'update'");

      return [];
    }

    var docs;

    if (name === undefined || (typeof name === "string" && name.length === 0)) {
      docs = fishbowl.toArray();
    }
    else {
      name = name.replace(/[^a-zA-Z0-9]/g, ' ');

      // get results by looking in "description" attribute
      docs = fishbowl.fulltext("description", "prefix:" + name).toArray();

      // build a hash of keys
      var i;
      var keys = { };

      for (i = 0; i < docs.length; ++i) {
        keys[docs[i]._key] = 1;
      }

      // get results by looking in "name" attribute
      var docs2= fishbowl.fulltext("name", "prefix:" + name).toArray();

      // merge the two result sets, avoiding duplicates
      for (i = 0; i < docs2.length; ++i) {
        if (!keys.hasOwnProperty(docs2[i]._key)) {
          docs.push(docs2[i]);
        }

      }
    }

    return docs;
  };


////////////////////////////////////////////////////////////////////////////////
/// @brief searchs for an available FOXX applications
////////////////////////////////////////////////////////////////////////////////

  var search = function (name) {
    var docs = searchJson(name);

    arangodb.printTable(
      docs.sort(compareApps),
      [ "name", "author", "description" ],
      {
        prettyStrings: true,
        totalString: "%s application(s) found",
        emptyString: "no applications found",
        rename: {
          name : "Name",
          author : "Author",
          description : "Description"
        }
      }
    );
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief returns all available FOXX applications
////////////////////////////////////////////////////////////////////////////////

function availableJson() {
  var fishbowl = getFishbowlStorage();
  var cursor = fishbowl.all();
  var result = [];
  var doc, maxVersion, versions, res;

  while (cursor.hasNext()) {
    doc = cursor.next();

    maxVersion = "-";
    versions = Object.keys(doc.versions);
    versions.sort(compareVersions);
    if (versions.length > 0) {
      versions.reverse();
      maxVersion = versions[0];
    }

    res = {
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
/// @brief updates the repository
////////////////////////////////////////////////////////////////////////////////


  var update = function() {
    var url = utils.buildGithubUrl(getFishbowlUrl());
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
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief prints all available FOXX applications
////////////////////////////////////////////////////////////////////////////////

  var available = function () {
    var list = availableJson();

    arangodb.printTable(
      list.sort(compareApps),
      [ "name", "author", "description", "latestVersion" ],
      {
        prettyStrings: true,
        totalString: "%s application(s) found",
        emptyString: "no applications found, please use 'update'",
        rename: {
          "name" : "Name",
          "author" : "Author",
          "description" : "Description",
          "latestVersion" : "Latest Version"
        }
      }
    );
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief info for an available FOXX application
////////////////////////////////////////////////////////////////////////////////

  var info = function (name) {
    utils.validateAppName(name);

    var fishbowl = getFishbowlStorage();

    if (fishbowl.count() === 0) {
      arangodb.print("Repository is empty, please use 'update'");
      return;
    }

    var desc;

    try {
      desc = fishbowl.document(name);
    }
    catch (err) {
      arangodb.print("No application '" + name + "' available, please try 'search'");
      return;
    }

    arangodb.printf("Name:        %s\n", desc.name);

    if (desc.hasOwnProperty('author')) {
      arangodb.printf("Author:      %s\n", desc.author);
    }

    var isSystem = desc.hasOwnProperty('isSystem') && desc.isSystem;
    arangodb.printf("System:      %s\n", JSON.stringify(isSystem));

    if (desc.hasOwnProperty('description')) {
      arangodb.printf("Description: %s\n\n", desc.description);
    }

    var header = false;
    var versions = Object.keys(desc.versions);
    versions.sort(compareVersions);

    versions.forEach(function (v) {
      var version = desc.versions[v];

      if (! header) {
        arangodb.print("Versions:");
        header = true;
      }

      if (version.type === "github") {
        if (version.hasOwnProperty("tag")) {
          arangodb.printf('%s: fetch github "%s" "%s"\n', v, version.location, version.tag);
        }
        else if (v.hasOwnProperty("branch")) {
          arangodb.printf('%s: fetch github "%s" "%s"\n', v, version.location, version.branch);
        }
        else {
          arangodb.printf('%s: fetch "github" "%s"\n', v, version.location);
        }
      }
    });

    arangodb.printf("\n");
  };

////////////////////////////////////////////////////////////////////////////////
/// Section export public API
////////////////////////////////////////////////////////////////////////////////

  exports.available = available;
  exports.availableJson = availableJson;
  exports.getFishbowlStorage = getFishbowlStorage;
  exports.search = search;
  exports.searchJson = searchJson;
  exports.update = update;
  exports.info = info;

  // Temporary export to avoid breaking the client
  exports.compareVersions = compareVersions;

}());

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
