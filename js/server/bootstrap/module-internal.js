/*jshint -W051: true */
/*global require, db, ArangoCollection, ArangoDatabase, ArangoCursor, ShapedJson,
  RELOAD_AUTH, SYS_DEFINE_ACTION, SYS_EXECUTE_GLOBAL_CONTEXT_FUNCTION,
  AHUACATL_RUN, WAL_FLUSH, WAL_PROPERTIES,
  REPLICATION_LOGGER_STATE, REPLICATION_LOGGER_CONFIGURE, REPLICATION_SERVER_ID,
  REPLICATION_APPLIER_CONFIGURE, REPLICATION_APPLIER_START, REPLICATION_APPLIER_SHUTDOWN,
  REPLICATION_APPLIER_FORGET, REPLICATION_APPLIER_STATE, REPLICATION_SYNCHRONISE,
  ENABLE_STATISTICS, DISPATCHER_THREADS, SYS_CREATE_NAMED_QUEUE, SYS_ADD_JOB,
  SYS_RAW_REQUEST_BODY, SYS_REQUEST_PARTS */

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
  /*jshint strict: false */
  var internal = require("internal");
  var fs = require("fs");
  var console = require("console");

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief db
////////////////////////////////////////////////////////////////////////////////

  internal.db = db;
  delete db;

////////////////////////////////////////////////////////////////////////////////
/// @brief ArangoCollection
////////////////////////////////////////////////////////////////////////////////

  internal.ArangoCollection = ArangoCollection;
  delete ArangoCollection;

////////////////////////////////////////////////////////////////////////////////
/// @brief ArangoDatabase
////////////////////////////////////////////////////////////////////////////////

  internal.ArangoDatabase = ArangoDatabase;
  delete ArangoDatabase;

////////////////////////////////////////////////////////////////////////////////
/// @brief ArangoCursor
////////////////////////////////////////////////////////////////////////////////

  internal.ArangoCursor = ArangoCursor;
  delete ArangoCursor;

////////////////////////////////////////////////////////////////////////////////
/// @brief ShapedJson
////////////////////////////////////////////////////////////////////////////////

  internal.ShapedJson = ShapedJson;
  delete ShapedJson;

////////////////////////////////////////////////////////////////////////////////
/// @brief enableStatistics
////////////////////////////////////////////////////////////////////////////////

  internal.enableStatistics = ENABLE_STATISTICS;
  delete ENABLE_STATISTICS;

////////////////////////////////////////////////////////////////////////////////
/// @brief dispatcherThreads
////////////////////////////////////////////////////////////////////////////////

  internal.dispatcherThreads = DISPATCHER_THREADS;
  delete DISPATCHER_THREADS;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief executes an AQL query
////////////////////////////////////////////////////////////////////////////////

  internal.AQL_QUERY = AHUACATL_RUN;
  delete AHUACATL_RUN;

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
/// @brief write-ahead log object
////////////////////////////////////////////////////////////////////////////////

  internal.wal = {
    flush: function () {
      return WAL_FLUSH.apply(null, arguments);
    },

    properties: function () {
      return WAL_PROPERTIES.apply(null, arguments);
    }
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief defines an action
////////////////////////////////////////////////////////////////////////////////

  if (typeof SYS_DEFINE_ACTION !== "undefined") {
    internal.defineAction = SYS_DEFINE_ACTION;
    delete SYS_DEFINE_ACTION;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief autoload modules from database
////////////////////////////////////////////////////////////////////////////////

  // autoload specific modules
  internal.autoloadModules = function () {
    'use strict';

    console.debug("autoloading actions");

    var modules = internal.db._collection("_modules");

    if (modules !== null) {
      modules = modules.byExample({ autoload: true }).toArray();

      modules.forEach(function(module) {

        // this module is only meant to be executed in one thread
        if (internal.threadNumber !== 0 && ! module.perThread) {
          return;
        }

        console.debug("autoloading module: %s", module.path);

        try {

          // require a module
          if (module.path !== undefined) {
            require(module.path);
          }

          // execute a user function
          else if (module.func !== undefined) {
              /*jshint evil: true */
              var func = new Function(module.func);
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

////////////////////////////////////////////////////////////////////////////////
/// @brief reloads the AQL user functions
////////////////////////////////////////////////////////////////////////////////

  if (typeof SYS_EXECUTE_GLOBAL_CONTEXT_FUNCTION === "undefined") {
    internal.reloadAqlFunctions = function () {
      require("org/arangodb/ahuacatl").reload();
      require("org/arangodb/aql").reload();
    };
  }
  else {
    internal.reloadAqlFunctions = function () {
      internal.executeGlobalContextFunction("reloadAql");
      require("org/arangodb/ahuacatl").reload();
      require("org/arangodb/aql").reload();
    };
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief getStateReplicationLogger
////////////////////////////////////////////////////////////////////////////////

  if (typeof REPLICATION_LOGGER_STATE !== "undefined") {
    internal.getStateReplicationLogger = REPLICATION_LOGGER_STATE;
    delete REPLICATION_LOGGER_STATE;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief configureReplicationLogger
////////////////////////////////////////////////////////////////////////////////

  if (typeof REPLICATION_LOGGER_CONFIGURE !== "undefined") {
    internal.configureReplicationLogger = REPLICATION_LOGGER_CONFIGURE;
    delete REPLICATION_LOGGER_CONFIGURE;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief configureReplicationApplier
////////////////////////////////////////////////////////////////////////////////

  if (typeof REPLICATION_APPLIER_CONFIGURE !== "undefined") {
    internal.configureReplicationApplier = REPLICATION_APPLIER_CONFIGURE;
    delete REPLICATION_APPLIER_CONFIGURE;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief startReplicationApplier
////////////////////////////////////////////////////////////////////////////////

  if (typeof REPLICATION_APPLIER_START !== "undefined") {
    internal.startReplicationApplier = REPLICATION_APPLIER_START;
    delete REPLICATION_APPLIER_START;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief shutdownReplicationApplier
////////////////////////////////////////////////////////////////////////////////

  if (typeof REPLICATION_APPLIER_SHUTDOWN !== "undefined") {
    internal.shutdownReplicationApplier = REPLICATION_APPLIER_SHUTDOWN;
    delete REPLICATION_APPLIER_SHUTDOWN;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief getStateReplicationApplier
////////////////////////////////////////////////////////////////////////////////

  if (typeof REPLICATION_APPLIER_STATE !== "undefined") {
    internal.getStateReplicationApplier = REPLICATION_APPLIER_STATE;
    delete REPLICATION_APPLIER_STATE;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief forgetStateReplicationApplier
////////////////////////////////////////////////////////////////////////////////

  if (typeof REPLICATION_APPLIER_FORGET !== "undefined") {
    internal.forgetStateReplicationApplier = REPLICATION_APPLIER_FORGET;
    delete REPLICATION_APPLIER_FORGET;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief sychroniseReplication
////////////////////////////////////////////////////////////////////////////////

  if (typeof REPLICATION_SYNCHRONISE !== "undefined") {
    internal.synchroniseReplication = REPLICATION_SYNCHRONISE;
    delete REPLICATION_SYNCHRONISE;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief serverId
////////////////////////////////////////////////////////////////////////////////

  if (typeof REPLICATION_SERVER_ID !== "undefined") {
    internal.serverId = REPLICATION_SERVER_ID;
    delete REPLICATION_SERVER_ID;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief loadStartup
////////////////////////////////////////////////////////////////////////////////

  internal.loadStartup = function (path) {
    return internal.load(fs.join(internal.startupPath, path));
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief createNamedQueue
////////////////////////////////////////////////////////////////////////////////

  if (typeof SYS_CREATE_NAMED_QUEUE !== "undefined") {
    internal.createNamedQueue = SYS_CREATE_NAMED_QUEUE;
    delete SYS_CREATE_NAMED_QUEUE;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief addJob
////////////////////////////////////////////////////////////////////////////////

  if (typeof SYS_ADD_JOB !== "undefined") {
    internal.addJob = SYS_ADD_JOB;
    delete SYS_ADD_JOB;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief raw request body
////////////////////////////////////////////////////////////////////////////////

  if (typeof SYS_RAW_REQUEST_BODY !== "undefined") {
    internal.rawRequestBody = SYS_RAW_REQUEST_BODY;
    delete SYS_RAW_REQUEST_BODY;
  }

  if (typeof SYS_REQUEST_PARTS !== "undefined") {
    internal.requestParts = SYS_REQUEST_PARTS;
    delete SYS_REQUEST_PARTS;
  }

}());

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}\\|/\\*jslint"
// End:
