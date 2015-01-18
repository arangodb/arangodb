/*jshint unused: false */
/*global require, exports*/

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
/// @author Michael Hackstein
/// @author Dr. Frank Celler
/// @author Copyright 2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

(function() {
  "use strict";

// -----------------------------------------------------------------------------
// --SECTION--                                                           imports
// -----------------------------------------------------------------------------

  var arangodb = require("org/arangodb");
  var arangosh = require("org/arangodb/arangosh");
  var ArangoError = arangodb.ArangoError;
  var checkParameter = arangodb.checkParameter;
  var arango = require("internal").arango;

  var utils = require("org/arangodb/foxx/manager-utils");
  var store = require("org/arangodb/foxx/store");
// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief extracts command-line options
////////////////////////////////////////////////////////////////////////////////

  var extractCommandLineOptions = function(args) {

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
  };


////////////////////////////////////////////////////////////////////////////////
/// @brief extract options from CLI options
////////////////////////////////////////////////////////////////////////////////
  var extractOptions = function (args) {
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
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief prints out usage message for the command-line tool
////////////////////////////////////////////////////////////////////////////////

  var cmdUsage = function () {
    var printf = arangodb.printf;
    var fm = "foxx-manager";

    printf("Example usage:\n");
    printf(" %s install <foxx> <mount-point> option1=value1\n", fm);
    printf(" %s uninstall <mount-point>\n\n", fm);

    printf("Further help:\n");
    printf(" %s help   for the list of foxx-manager commands\n", fm);
    printf(" %s --help for the list of options\n", fm);
  };

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------


////////////////////////////////////////////////////////////////////////////////
/// @brief outputs the help
////////////////////////////////////////////////////////////////////////////////

  var help = function () {

    /*jshint maxlen: 200 */
    var commands = {
      "setup"        : "executes the setup script",
      "install"      : "installs a foxx application identified by the given information to the given mountpoint",
      "replace"      : ["replaces an installed Foxx application",
                           "WARNING: this action will remove application data if the application implements teardown!" ],
      "upgrade"      : ["upgrades an installed Foxx application",
                           "Note: this action will not call setup or teardown" ],
      "teardown"     : [ "executes the teardown script",
                             "WARNING: this action will remove application data if the application implements teardown!" ],
      "uninstall"    : ["uninstalls a Foxx application and calls its teardown method",
                         "WARNING: this will remove all data and code of the application!" ],
      "list"         : "lists all installed Foxx applications",
      "installed"    : "alias for the 'list' command",
      "available"    : "lists all Foxx applications available in the local repository",
      "info"         : "displays information about a Foxx application",
      "search"       : "searches the local foxx-apps repository",
      "update"       : "updates the local foxx-apps repository with data from the central foxx-apps repository",
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
/// @brief sets up a Foxx application
///
/// Input:
/// * mount: the mount path starting with a "/"
///
/// Output:
/// -
////////////////////////////////////////////////////////////////////////////////
  var setup = function(mount) {
    checkParameter(
      "mount(<mount>)",
      [ [ "Mount path", "string" ] ],
      [ mount ] );
    var res;
    var req = {
      mount: mount,
    };

    res = arango.POST("/_admin/foxx/setup", JSON.stringify(req));
    arangosh.checkRequestResult(res);
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief tears down a Foxx application
///
/// Input:
/// * mount: the mount path starting with a "/"
///
/// Output:
/// -
////////////////////////////////////////////////////////////////////////////////

  var teardown = function(mount) {
    checkParameter(
      "mount(<mount>)",
      [ [ "Mount path", "string" ] ],
      [ mount ] );
    var res;
    var req = {
      mount: mount,
    };

    res = arango.POST("/_admin/foxx/teardown", JSON.stringify(req));
    arangosh.checkRequestResult(res);
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief Installs a new foxx application on the given mount point.
///
/// TODO: Long Documentation!
////////////////////////////////////////////////////////////////////////////////

  var install = function(appInfo, mount, options) {
    checkParameter(
      "mount(<appInfo>, <mount>, [<options>])",
      [ [ "Install information", "string" ],
        [ "Mount path", "string" ] ],
      [ appInfo, mount ] );

    var res;
    var req = {
      appInfo: appInfo,
      mount: mount,
      options: options
    };

    res = arango.POST("/_admin/foxx/install", JSON.stringify(req));
    arangosh.checkRequestResult(res);
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief Uninstalls the foxx application on the given mount point.
///
/// TODO: Long Documentation!
////////////////////////////////////////////////////////////////////////////////

  var uninstall = function(mount) {
    checkParameter(
      "mount(<mount>)",
      [ [ "Mount path", "string" ] ],
      [ mount ] );
    var res;
    var req = {
      mount: mount,
    };

    res = arango.POST("/_admin/foxx/uninstall", JSON.stringify(req));
    arangosh.checkRequestResult(res);
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief Replaces a foxx application on the given mount point by an other one.
///
/// TODO: Long Documentation!
////////////////////////////////////////////////////////////////////////////////

  var replace = function(appInfo, mount, options) {
    checkParameter(
      "mount(<appInfo>, <mount>, [<options>])",
      [ [ "Install information", "string" ],
        [ "Mount path", "string" ] ],
      [ appInfo, mount ] );
    var res;
    var req = {
      appInfo: appInfo,
      mount: mount,
      options: options
    };

    res = arango.POST("/_admin/foxx/replace", JSON.stringify(req));
    arangosh.checkRequestResult(res);
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief Upgrade a foxx application on the given mount point by a new one.
///
/// TODO: Long Documentation!
////////////////////////////////////////////////////////////////////////////////

  var upgrade = function(appInfo, mount, options) {
    checkParameter(
      "mount(<appInfo>, <mount>, [<options>])",
      [ [ "Install information", "string" ],
        [ "Mount path", "string" ] ],
      [ appInfo, mount ] );
    var res;
    var req = {
      appInfo: appInfo,
      mount: mount,
      options: options
    };

    res = arango.POST("/_admin/foxx/upgrade", JSON.stringify(req));
    arangosh.checkRequestResult(res);
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief command line dispatcher
////////////////////////////////////////////////////////////////////////////////

  var run = function (args) {
    if (args === undefined || args.length === 0) {
      arangodb.print("Expecting a command, please try:\n");
      cmdUsage();
      return 0;
    }

    var type = args[0];
    var printf = arangodb.printf;
    var res;
    var options;


    try {
      switch (type) {
        case "setup":
          setup(args[1]);
          break;
        case "teardown":
          teardown(args[1]);
          break;
        case "install":
          options = extractOptions(args);
          res = install(args[1], args[2], options);
          printf("Application %s installed successfully at mount point %s\n",
               res.appId,
               res.mount);
          printf("options used: %s\n", JSON.stringify(options));
          break;
        case "replace":
          options = extractOptions(args);
          res = replace(args[1], args[2], options);
          printf("Application %s replaced successfully at mount point %s\n",
             res.appId,
             res.mount);
          break;
        case "upgrade":
          options = extractOptions(args);
          res = upgrade(args[1], args[2], options);
          printf("Application %s upgraded successfully at mount point %s\n",
             res.appId,
             res.mount);
          break;
        case "uninstall":
          res = uninstall(args[1]);

          printf("Application %s uninstalled successfully from mount point %s\n",
             res.appId,
             res.mount);
          break;
        case "list":
        case "installed":
          if (1 < args.length && args[1] === "prefix") {
            utils.list(true);
          }
          else {
            utils.list();
          }
          break;
        case "available":
          store.available();
          break;
        case "info":
          store.info(args[1]);
          break;
        case "search":
          store.search(args[1]);
          break;
        case "update":
          store.available();
          break;
        case "help":
          help();
          break;
        default:
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



// -----------------------------------------------------------------------------
// --SECTION--                                                           exports
// -----------------------------------------------------------------------------

  exports.run = run;
  exports.help = help;
  exports.install = install;
  exports.setup = setup;
  exports.teardown = teardown;
  exports.uninstall = uninstall;
  exports.replace = replace;
  exports.upgrade = upgrade;

////////////////////////////////////////////////////////////////////////////////
/// @brief Exports from foxx utils module.
////////////////////////////////////////////////////////////////////////////////

  exports.mountedApp = utils.mountedApp;
  exports.list = utils.list;
  exports.listJson = utils.listJson;
  exports.listDevelopment = utils.listDevelopment;
  exports.listDevelopmentJson = utils.listDevelopmentJson;

////////////////////////////////////////////////////////////////////////////////
/// @brief Exports from foxx store module.
////////////////////////////////////////////////////////////////////////////////

  exports.available = store.available;
  exports.availableJson = store.availableJson;
  exports.search = store.search;
  exports.searchJson = store.searchJson;
  exports.update = store.update;
  exports.info = store.info;

}());
// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}\\|/\\*jslint"
// End:
