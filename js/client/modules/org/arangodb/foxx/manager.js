/*jshint unused: false */
/*global require, exports, module */

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

var fs = require("fs");

var arangodb = require("org/arangodb");
var arangosh = require("org/arangodb/arangosh");

var errors = arangodb.errors;
var ArangoError = arangodb.ArangoError;
var db = arangodb.db;
var throwDownloadError = arangodb.throwDownloadError;
var throwFileNotFound = arangodb.throwFileNotFound;
var throwBadParameter = arangodb.throwBadParameter;
var checkParameter = arangodb.checkParameter;

var arango = require("internal").arango;
var download = require("internal").download;
var utils = require("org/arangodb/foxx/manager-utils");
var store = require("org/arangodb/foxx/store");

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the aal collection
////////////////////////////////////////////////////////////////////////////////

function getStorage () {
  'use strict';

  return db._collection('_aal');
}

////////////////////////////////////////////////////////////////////////////////
/// @brief validate a mount and fail if it is invalid
////////////////////////////////////////////////////////////////////////////////

function validateMount (mnt) {
  'use strict';

  if (typeof mnt === 'string' && mnt.substr(0, 1) === '/') {
    return;
  }

  throw new ArangoError({
    errorNum: errors.ERROR_APPLICATION_INVALID_MOUNT.code,
    errorMessage: errors.ERROR_APPLICATION_INVALID_MOUNT.message + ': ' + String(mnt)
  });
}

////////////////////////////////////////////////////////////////////////////////
/// @brief processes files in a zip file
////////////////////////////////////////////////////////////////////////////////

function processZip (source) {
  'use strict';

  var location = source.location;

  if (! fs.exists(location) || ! fs.isFile(location)) {
    throwFileNotFound("Cannot find zip file '" + String(location) + "'");
  }

  source.filename = source.location;
  source.removeFile = false;

  utils.repackZipFile(source);
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
    utils.processDirectory(src);
  }
  else if (src.type === "github") {
    utils.processGithubRepository(src);
  }
  else {
    throwBadParameter("Unknown application type '" + src.type + "'. " +
                      "expected type: 'github', 'zip', or 'directory'.");
  }

  // upload file to the server
  var response = arango.SEND_FILE("/_api/upload", src.filename);

  if (src.removeFile && src.filename !== '') {
    try {
      fs.remove(src.filename);
    }
    catch (err2) {
      arangodb.printf("Cannot remove temporary file '%s'\n", src.filename);
    }
  }

  if (! response.filename) {
    throw new ArangoError({
      errorNum: errors.ERROR_APPLICATION_UPLOAD_FAILED.code,
      errorMessage: errors.ERROR_APPLICATION_UPLOAD_FAILED.message
                  + ": " + String(response.errorMessage)
    });
  }

  return response.filename;
}


////////////////////////////////////////////////////////////////////////////////
/// @brief prints out usage message for the command-line tool
////////////////////////////////////////////////////////////////////////////////

function cmdUsage () {
  'use strict';

  var printf = arangodb.printf;
  var fm = "foxx-manager";

  printf("Example usage:\n");
  printf(" %s install <foxx> <mount-point> option1=value1\n", fm);
  printf(" %s uninstall <mount-point>\n\n", fm);

  printf("Further help:\n");
  printf(" %s help   for the list of foxx-manager commands\n", fm);
  printf(" %s --help for the list of options\n", fm);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief fetchs github for install
////////////////////////////////////////////////////////////////////////////////

function fetchGithubForInstall (name) {
  'use strict';

  // .............................................................................
  // latest fishbowl version
  // .............................................................................

  var fishbowl = store.getFishbowlStorage();
  var available = fishbowl.firstExample({name: name});
  var source = null;
  var version = null;

  if (available !== null) {
    var keys = [];
    var key;

    for (key in available.versions) {
      if (available.versions.hasOwnProperty(key)) {
        keys.push(key);
      }
    }

    keys = keys.sort(module.compareVersions);
    version = keys[keys.length - 1];
    source = available.versions[version];
  }

  // .............................................................................
  // latest fetched version
  // .............................................................................

  var appId = null;
  var aal = getStorage();
  var cursor = aal.byExample({ type: "app", name: name });
  var doc;

  while (cursor.hasNext()) {
    doc = cursor.next();

    if (module.compareVersions(version, doc.version) <= 0) {
      version = doc.version;
      source = "fetched";
      appId = doc.app;
    }
  }

  // .............................................................................
  // fetched latest version
  // .............................................................................

  if (source === null) {
    throw new Error("Unknown foxx application '" + name + "', use search");
  }

  if (source !== "fetched") {
    appId = exports.fetch(source.type, source.location, source.tag).app;
  }

  return appId;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief fetchs directory for install
////////////////////////////////////////////////////////////////////////////////

function fetchDirectoryForInstall (name) {
  'use strict';

  return exports.fetch("directory", name).app;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts command-line options
////////////////////////////////////////////////////////////////////////////////

function extractCommandLineOptions (args) {
  'use strict';

  var options = {};
  var nargs = [];
  var i;

  var re1 = /^([\-_a-zA-Z0-9]*)=(.*)$/;
  var re2 = /^(0|.0|([0-9]*(\.[0-9]*)?))$/;
  var a, m, k, v;

  for (i = 0;  i < args.length;  ++i) {
    a = args[i];
    m = re1.exec(a);

    if (m !== null) {
      k = m[1];
      v = m[2];

      if (re2.test(v)) {
        options[k] = parseFloat(v);
      }
      else {
        options[k] = v;
      }
    }
    else {
      nargs.push(args[i]);
    }
  }

  return { 'options': options, 'args': nargs };
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief command line dispatcher
////////////////////////////////////////////////////////////////////////////////

exports.run = function (args) {
  'use strict';

  if (args === undefined || args.length === 0) {
    arangodb.print("Expecting a command, please try:\n");
    cmdUsage();
    return 0;
  }

  var type = args[0];
  var printf = arangodb.printf;
  var res;

  function extractOptions () {
    var co = extractCommandLineOptions(args);

    if (3 < co.args.length) {
      var options = JSON.parse(co.args[3]);

      if (options.hasOwnProperty("configuration")) {
        var k;

        for (k in co.options) {
          if (co.options.hasOwnProperty(k)) {
            options.configuration[k] = co.options[k];
          }
        }
      }
      else {
        options.configuration = co.options;
      }

      return options;
    }

    return { configuration: co.options };
  }

  try {
    if (type === 'fetch') {
      exports.fetch(args[1], args[2], args[3]);
    }
    else if (type === 'mount') {
      exports.mount(args[1], args[2], extractOptions());
    }
    else if (type === 'rescan') {
      exports.rescan();
    }
    else if (type === 'setup') {
      exports.setup(args[1]);
    }
    else if (type === 'teardown') {
      exports.teardown(args[1]);
    }
    else if (type === 'unmount') {
      exports.unmount(args[1]);
    }
    else if (type === 'install') {
      var options = extractOptions();
      res = exports.install(args[1], args[2], options);

      printf("Application %s installed successfully at mount point %s\n",
             res.appId,
             res.mount);
      printf("options used: %s\n", JSON.stringify(options));
    }
    else if (type === 'replace') {
      if (3 < args.length) {
        res = exports.replace(args[1], args[2], JSON.parse(args[3]));
      }
      else {
        res = exports.replace(args[1], args[2]);
      }

      printf("Application %s replaced successfully at mount point %s\n",
             res.appId,
             res.mount);
    }
    else if (type === 'uninstall') {
      res = exports.uninstall(args[1]);

      printf("Application %s unmounted successfully from mount point %s\n",
             res.appId,
             res.mount);
    }
    else if (type === 'purge' || type === 'remove') {
      res = exports.purge(args[1]);

      printf("Application %s and %d mounts purged successfully\n",
             res.name,
             res.purged.length);
    }
    else if (type === 'config') {
      exports.config();
    }
    else if (type === 'configJson') {
      exports.configJson();
    }
    else if (type === 'list' || type === 'installed') {
      if (1 < args.length && args[1] === "prefix") {
        exports.list(true);
      }
      else {
        exports.list();
      }
    }
    else if (type === 'fetched') {
      exports.fetched();
    }
    else if (type === 'available') {
      store.available();
    }
    else if (type === 'info') {
      store.info(args[1]);
    }
    else if (type === 'search') {
      store.search(args[1]);
    }
    else if (type === 'update') {
      store.update();
    }
    else if (type === 'help') {
      exports.help();
    }
    else {
      arangodb.printf("Unknown command '%s', please try:\n", type);
      cmdUsage();
    }

    return 0;
  }
  catch (err) {
    if (err instanceof ArangoError) {
      arangodb.printf("%s\n", err.errorMessage);
    }
    else {
      arangodb.printf("%s\n", err.message);
    }

    return 1;
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief fetches a FOXX application
////////////////////////////////////////////////////////////////////////////////

exports.fetch = function (type, location, version) {
  'use strict';

  checkParameter(
    "fetch(<type>, <location>, [<version>])",
    [ [ "Location type", "string" ],
      [ "Location", "string" ] ],
    [ type, location ] );

  var source = {
    type: type,
    location: location,
    version: version
  };

  var filename = processSource(source);

  if (source.name === undefined) {
    throwBadParameter("Name missing for '" + JSON.stringify(source) + "'");
  }

  if (source.version === undefined) {
    throwBadParameter("Version missing for '" + JSON.stringify(source) + "'");
  }

  var req = {
    name: source.name,
    version: source.version,
    filename: filename
  };

  var res = arango.POST("/_admin/foxx/fetch", JSON.stringify(req));

  return arangosh.checkRequestResult(res);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief rescans the FOXX application directory
////////////////////////////////////////////////////////////////////////////////

exports.rescan = function () {
  'use strict';

  var res = arango.POST("/_admin/foxx/rescan", "");

  return arangosh.checkRequestResult(res);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief mounts a FOXX application
////////////////////////////////////////////////////////////////////////////////

exports.mount = function (appId, mount, options) {
  'use strict';

  checkParameter(
    "mount(<appId>, <mount>, [<options>])",
    [ [ "Application identifier", "string" ],
      [ "Mount path", "string" ] ],
    [ appId, mount ] );

  var req = {
    appId: appId,
    mount: mount,
    options: options
  };

  utils.validateAppName(appId);
  validateMount(mount);

  var res = arango.POST("/_admin/foxx/mount", JSON.stringify(req));

  return arangosh.checkRequestResult(res);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief sets up a FOXX application
////////////////////////////////////////////////////////////////////////////////

exports.setup = function (mount) {
  'use strict';

  checkParameter(
    "setup(<mount>)",
    [ [ "Mount identifier", "string" ] ],
    [ mount ] );

  var req = {
    mount: mount
  };

  validateMount(mount);

  var res = arango.POST("/_admin/foxx/setup", JSON.stringify(req));
  arangosh.checkRequestResult(res);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief tears down a FOXX application
////////////////////////////////////////////////////////////////////////////////

exports.teardown = function (mount) {
  'use strict';

  checkParameter(
    "teardown(<mount>)",
    [ [ "Mount identifier", "string" ] ],
    [ mount ] );

  var req = {
    mount: mount
  };

  validateMount(mount);

  var res = arango.POST("/_admin/foxx/teardown", JSON.stringify(req));
  arangosh.checkRequestResult(res);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief unmounts a FOXX application
////////////////////////////////////////////////////////////////////////////////

exports.unmount = function (mount) {
  'use strict';

  checkParameter(
    "unmount(<mount>)",
    [ [ "Mount identifier", "string" ] ],
    [ mount ] );

  utils.validateAppName(mount);

  var req = {
    mount: mount
  };

  var res = arango.POST("/_admin/foxx/unmount", JSON.stringify(req));

  return arangosh.checkRequestResult(res);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief replaces a FOXX application
////////////////////////////////////////////////////////////////////////////////

exports.replace = function (name, mount, options) {
  'use strict';

  checkParameter(
    "replace(<name>, <mount>, [<options>])",
    [ [ "Name", "string" ],
      [ "Mount path", "string" ]],
    [ name, mount ] );

  validateMount(mount);

  var aal = getStorage();
  var existing = aal.firstExample({ type: "mount", name: name, mount: mount });

  if (existing === null) {
    throw new Error("Cannot find application '" + name + "' at mount '" + mount + "'");
  }

  var appId = existing.app;

  // .............................................................................
  // install at path
  // .............................................................................

  if (appId === null) {
    throw new Error("Cannot extract application id");
  }

  options = options || {};

  if (options.setup === undefined) {
    options.setup = true;
  }

  options.reload = true;

  exports.unmount(mount);
  var res = exports.mount(appId, mount, options);

  return res;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief installs a FOXX application
////////////////////////////////////////////////////////////////////////////////

exports.install = function (name, mount, options) {
  'use strict';

  checkParameter(
    "install(<name>, <mount>, [<options>])",
    [ [ "Name", "string" ],
      [ "Mount path", "string" ]],
    [ name, mount ] );

  validateMount(mount);

  var appId;

  if (name[0] === '.') {
    appId = fetchDirectoryForInstall(name);
  }
  else {
    appId = fetchGithubForInstall(name);
  }

  // .............................................................................
  // install at path
  // .............................................................................

  if (appId === null) {
    throw new Error("Cannot extract application id");
  }

  options = options || {};

  if (options.setup === undefined) {
    options.setup = true;
  }

  if (! options.setup) {
    options.reload = false;
  }

  var res = exports.mount(appId, mount, options);

  return res;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief uninstalls a FOXX application
////////////////////////////////////////////////////////////////////////////////

exports.uninstall = function (mount) {
  'use strict';

  checkParameter(
    "teardown(<mount>)",
    [ [ "Mount identifier", "string" ] ],
    [ mount ] );

  exports.teardown(mount);

  return exports.unmount(mount);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief purges a FOXX application
////////////////////////////////////////////////////////////////////////////////

exports.purge = function (key) {
  'use strict';

  checkParameter(
    "purge(<app-id>)",
    [ [ "app-id or name", "string" ] ],
    [ key ] );

  var req = {
    name: key
  };

  var res = arango.POST("/_admin/foxx/purge", JSON.stringify(req));

  return arangosh.checkRequestResult(res);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief returns configuration from the server
////////////////////////////////////////////////////////////////////////////////

exports.config = function () {
  'use strict';

  var res = arango.GET("/_admin/foxx/config"), name;

  arangosh.checkRequestResult(res);

  arangodb.printf("The following configuration values are effective on the server:\n");

  for (name in res.result) {
    if (res.result.hasOwnProperty(name)) {
      arangodb.printf("- %s: %s\n", name, JSON.stringify(res.result[name]));
    }
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief returns configuration from the server
////////////////////////////////////////////////////////////////////////////////

exports.configJson = function () {
  'use strict';

  var res = arango.GET("/_admin/foxx/config");

  arangosh.checkRequestResult(res);

  return res.result;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief returns all installed FOXX applications
////////////////////////////////////////////////////////////////////////////////

exports.listJson = function (showPrefix) {
  'use strict';

  return utils.listJson(showPrefix);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief prints all installed FOXX applications
////////////////////////////////////////////////////////////////////////////////

exports.list = function (showPrefix) {
  'use strict';

  var list = exports.listJson(showPrefix);
  var columns = [ "name", "author", "description", "appId", "version", "mount" ];

  if (showPrefix) {
    columns.push("collectionPrefix");
  }
  columns.push("active");
  columns.push("system");

  arangodb.printTable(list, columns, {
    prettyStrings: true,
    totalString: "%s application(s) found",
    emptyString: "no applications found",
    rename: {
      mount: "Mount",
      appId: "AppID",
      name: "Name",
      description: "Description",
      author: "Author",
      system: "System",
      active: "Active",
      version: "Version",
      collectionPrefix: "CollectionPrefix"
    }
  });
};

////////////////////////////////////////////////////////////////////////////////
/// @brief returns all fetched FOXX applications
////////////////////////////////////////////////////////////////////////////////

exports.fetchedJson = function () {
  'use strict';

  var aal = getStorage();
  var cursor = aal.byExample({ type: "app" });
  var result = [];
  var doc, res;

  while (cursor.hasNext()) {
    doc = cursor.next();

    if (!doc.isSystem) {
      res = {
        appId: doc.app,
        name: doc.name,
        description: doc.description || "",
        author: doc.author || "",
        version: doc.version,
        path: doc.path
      };
      result.push(res);
    }
  }

  return result;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief prints all fetched FOXX applications
////////////////////////////////////////////////////////////////////////////////

exports.fetched = function () {
  'use strict';

  var list = exports.fetchedJson();

  arangodb.printTable(
    list,
    ["name", "author", "description", "appId", "version", "path"],
    {
      prettyStrings: true,
      totalString: "%s application(s) found",
      emptyString: "no applications found",
      rename: {
        appId: "AppID",
        name: "Name",
        description: "Description",
        author: "Author",
        version: "Version",
        path: "Path"
      }
    }
  );
};
////////////////////////////////////////////////////////////////////////////////
/// @brief outputs the help
////////////////////////////////////////////////////////////////////////////////

exports.help = function () {
  'use strict';

  /*jshint maxlen: 200 */
  var commands = {
    "fetch"        : "fetches a Foxx application from the central foxx-apps repository into the local repository",
    "mount"        : "mounts a fetched Foxx application to a local URL",
    "setup"        : "executes the setup script (app must already be mounted)",
    "install"      : "fetches a Foxx application from the central foxx-apps repository, mounts it to a local URL and sets it up",
    "rescan"       : [ "rescans the Foxx application directory on the server side",
                       "note: this is only required if the server-side apps directory was modified by other processes" ],
    "replace"      : "replaces an existing Foxx application with the current local version found in the application directory",
    "teardown"     : [ "executes the teardown script (app must be still be mounted)",
                           "WARNING: this action will remove application data if the application implements teardown!" ],
    "uninstall"    : "unmounts a mounted Foxx application and calls its teardown method",
    "unmount"      : "unmounts a mounted Foxx application without calling its teardown method",
    "purge"        : [ "uninstalls a Foxx application with all its mounts and physically removes the application directory",
                       "WARNING: this will remove all data and code of the application!" ],
    "remove"       : "alias for the 'purge' command",
    "list"         : "lists all installed Foxx applications",
    "installed"    : "alias for the 'list' command",
    "fetched"      : "lists all fetched Foxx applications that were fetched into the local repository",
    "available"    : "lists all Foxx applications available in the local repository",
    "info"         : "displays information about a Foxx application",
    "search"       : "searches the local foxx-apps repository",
    "update"       : "updates the local foxx-apps repository with data from the central foxx-apps repository",
    "config"       : "returns configuration information from the server",
    "help"         : "shows this help"
  };

  arangodb.print("\nThe following commands are available:\n");
  var keys = Object.keys(commands).sort();

  var i;
  var pad, name, extra;
  for (i = 0; i < keys.length; ++i) {
    pad  = "                  ";
    name = keys[i] + pad;
    extra = commands[keys[i]];

    if (typeof extra !== 'string') {
      // list of strings
      extra = extra.join("\n  " + pad) + "\n";
    }
    arangodb.printf(" %s %s\n", name.substr(0, pad.length), extra);
  }

  arangodb.print();
  arangodb.print("Use foxx-manager --help to show a list of global options\n");
  arangodb.print("There is also an online manual available at:");
  arangodb.print("https://docs.arangodb.com/FoxxManager/README.html");

  // additional newline
  arangodb.print();
};

////////////////////////////////////////////////////////////////////////////////
/// @brief sets up a FOXX dev application
////////////////////////////////////////////////////////////////////////////////

exports.devSetup = function (name) {
  'use strict';

  var res;
  var req = {
    name: name
  };

  res = arango.POST("/_admin/foxx/dev-setup", JSON.stringify(req));
  arangosh.checkRequestResult(res);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief tears down a FOXX dev application
////////////////////////////////////////////////////////////////////////////////

exports.devTeardown = function (name) {
  'use strict';

  var res;
  var req = {
    name: name
  };

  res = arango.POST("/_admin/foxx/dev-teardown", JSON.stringify(req));
  arangosh.checkRequestResult(res);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief Exports from foxx store module.
////////////////////////////////////////////////////////////////////////////////

exports.available = store.available;
exports.availableJson = store.availableJson;
exports.search = store.search;
exports.searchJson = store.searchJson;
exports.update = store.update;
exports.info = store.info;

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}\\|/\\*jslint"
// End:
