/*jslint indent: 2, nomen: true, maxlen: 120, sloppy: true, vars: true, white: true, plusplus: true, nonpropdel: true */
/*global require, db, ArangoCollection, ArangoDatabase, ArangoCursor, module,
         ShapedJson, RELOAD_AUTH, SYS_DEFINE_ACTION, SYS_EXECUTE_GLOBAL_CONTEXT_FUNCTION,
         AHUACATL_RUN, AHUACATL_PARSE, AHUACATL_EXPLAIN */

////////////////////////////////////////////////////////////////////////////////
/// @brief module "internal"
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
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
/// @author Dr. Frank Celler
/// @author Copyright 2010-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 Module "internal"
// -----------------------------------------------------------------------------

(function () {
  var internal = require("internal");
  var console = require("console");

////////////////////////////////////////////////////////////////////////////////
/// @brief hide global variables
////////////////////////////////////////////////////////////////////////////////

  internal.db = db;
  delete db;

  internal.ArangoCollection = ArangoCollection;
  delete ArangoCollection;

  internal.ArangoDatabase = ArangoDatabase;
  delete ArangoDatabase;

  internal.ArangoCursor = ArangoCursor;
  delete ArangoCursor;

  internal.ShapedJson = ShapedJson;
  delete ShapedJson;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief executes an AQL query
////////////////////////////////////////////////////////////////////////////////

  internal.AQL_QUERY = AHUACATL_RUN;
  delete AHUACATL_RUN;

////////////////////////////////////////////////////////////////////////////////
/// @brief parses an AQL query
////////////////////////////////////////////////////////////////////////////////

  internal.AQL_PARSE = AHUACATL_PARSE;
  delete AHUACATL_PARSE;

////////////////////////////////////////////////////////////////////////////////
/// @brief explains an AQL query
////////////////////////////////////////////////////////////////////////////////

  internal.AQL_EXPLAIN = AHUACATL_EXPLAIN;
  delete AHUACATL_EXPLAIN;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief returns mount point for system apps
////////////////////////////////////////////////////////////////////////////////

  function systemMountPoint (appName) {
    'use strict';

    if (appName === "aardvark") {
      return "/_admin/aardvark";
    }

    if (appName === "gharial") {
      return "/_api/gharial";
    }

    if (appName === "cerberus") {
      return "/system/cerberus";
    }

    return false;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief resets engine in development mode
////////////////////////////////////////////////////////////////////////////////

  internal.resetEngine = function () {
    'use strict';

    internal.flushModuleCache();
    require("org/arangodb/actions").reloadRouting();
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief rebuilds the authentication cache
////////////////////////////////////////////////////////////////////////////////

  internal.reloadAuth = RELOAD_AUTH;
  delete RELOAD_AUTH;

////////////////////////////////////////////////////////////////////////////////
/// @brief defines an action
////////////////////////////////////////////////////////////////////////////////

  if (typeof SYS_DEFINE_ACTION === "undefined") {
    internal.defineAction = function() {
      console.error("SYS_DEFINE_ACTION not available");
    };

    internal.actionLoaded = function() {
    };
  }
  else {
    internal.defineAction = SYS_DEFINE_ACTION;
    delete SYS_DEFINE_ACTION;

    // autoload specific actions
    internal.actionLoaded = function () {
      'use strict';

      console.debug("autoloading actions");

      var modules = internal.db._collection("_modules");

      if (modules !== null) {
        modules = modules.byExample({ autoload: true }).toArray();

        modules.forEach(function(module) {
          if (internal.threadNumber !== 0 && ! module.perThread) {
            // module is only meant to be executed in one thread
            return;
          }

          console.debug("autoloading module: %s", module.path);

          try {
            if (module.path !== undefined) {
              // require a module 
              require(module.path);
            }
            else if (module.func !== undefined) {
              // execute a user function
              /*jslint evil: true */
              var func = new Function(module.func);
              /*jslint evil: false */
              func();
            }
          }
          catch (err) {
            console.error("error while loading startup module '%s': %s", module.name || module.path, String(err));
          }
        });
      }

      console.debug("autoloading actions finished");
    };
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief initialize foxx applications
////////////////////////////////////////////////////////////////////////////////

  internal.initializeFoxx = function () {
    'use strict';

    var fm = require("org/arangodb/foxx/manager");

    try {
      fm.scanAppDirectory();
    }
    catch (err) {
      console.error("cannot initialize Foxx application: %s", String(err));
    }
    
    var aal = internal.db._collection("_aal");

    if (aal !== null) {
      var systemAppPath = module.systemAppPath();

      var fs = require("fs");
      var apps = fs.list(systemAppPath); 

      // make sure the aardvark app is always there
      if (apps.indexOf("aardvark") === -1) {
        apps.push("aardvark");
      }

      apps.forEach(function (appName) {
        var mount = systemMountPoint(appName);

        // for all unknown system apps: check that the directory actually exists
        if (! mount && ! fs.isDirectory(fs.join(systemAppPath, appName))) {
          return;
        }
          
        try {
          if (! mount) {
            mount = '/system/' + appName;
          }

          var found = aal.firstExample({ type: "mount", mount: mount });

          if (found === null) {
            fm.mount(appName, mount, {reload: false});
          }
        }
        catch (err) {
          console.error("unable to mount system application '%s': %s", appName, String(err));
        }
      });
    }
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief reloads the AQL user functions
////////////////////////////////////////////////////////////////////////////////

  if (typeof SYS_EXECUTE_GLOBAL_CONTEXT_FUNCTION === "undefined") {
    internal.reloadAqlFunctions = function () {
      require("org/arangodb/ahuacatl").reload();
    };
  }
  else {
    internal.reloadAqlFunctions = function () {
      internal.executeGlobalContextFunction("reloadAql");
      require("org/arangodb/ahuacatl").reload();
    };
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a string in all V8 contexts
////////////////////////////////////////////////////////////////////////////////

  if (typeof SYS_EXECUTE_GLOBAL_CONTEXT_FUNCTION === "undefined") {
    internal.executeGlobalContextFunction = function() {
      // nothing to do. we're probably in --console mode
    };
  }
  else {
    internal.executeGlobalContextFunction = SYS_EXECUTE_GLOBAL_CONTEXT_FUNCTION;
    delete SYS_EXECUTE_GLOBAL_CONTEXT_FUNCTION;
  }

}());

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}\\|/\\*jslint"
// End:
