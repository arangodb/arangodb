/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true, nonpropdel: true */
/*global require, db, ArangoCollection, ArangoDatabase, ArangoError, ShapedJson,
         RELOAD_AUTH, SYS_DEFINE_ACTION, SYS_EXECUTE_GLOBAL_CONTEXT_FUNCTION */

////////////////////////////////////////////////////////////////////////////////
/// @brief module "internal"
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2013 triagens GmbH, Cologne, Germany
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

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8ModuleInternal
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief internal module
////////////////////////////////////////////////////////////////////////////////

(function () {
  var internal = require("internal");
  var console = require("console");

  internal.db = db;
  delete db;

  internal.ArangoCollection = ArangoCollection;
  delete ArangoCollection;

  internal.ArangoDatabase = ArangoDatabase;
  delete ArangoDatabase;

  internal.ArangoError = ArangoError;
  delete ArangoError;

  internal.ShapedJson = ShapedJson;
  delete ShapedJson;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8ModuleInternal
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief rebuilds the authentication cache
////////////////////////////////////////////////////////////////////////////////

  internal.reloadAuth = RELOAD_AUTH;

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

    internal.actionLoaded = function() {
      var modules;
      var i;

      console.debug("actions loaded");

      modules = internal.db._collection("_modules");

      if (modules !== null) {
        modules = modules.byExample({ autoload: true }).toArray();

        for (i = 0;  i < modules.length;  ++i) {
          var module = modules[i];

          console.debug("autoloading module: %s", module.path);

          try {
            require(module.path);
          }
          catch (err) {
            console.error("while loading '%s': %s", module.path, String(err));
          }
        }
      }
    };
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a string in all V8 contexts
////////////////////////////////////////////////////////////////////////////////

  if (typeof SYS_EXECUTE_GLOBAL_CONTEXT_FUNCTION === "undefined") {
    internal.executeGlobalContextFunction = function() {
      console.error("SYS_EXECUTE_GLOBAL_CONTEXT_FUNCTION not available");
    };
  }
  else {
    internal.executeGlobalContextFunction = SYS_EXECUTE_GLOBAL_CONTEXT_FUNCTION;
    delete SYS_EXECUTE_GLOBAL_CONTEXT_FUNCTION;
  }

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

}());

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
