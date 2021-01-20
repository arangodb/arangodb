/* jshint -W051, -W020 */
/* global global:true, require, arango */
'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / @brief ArangoShell client API
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
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
// / @author Achim Brandt
// / @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

// //////////////////////////////////////////////////////////////////////////////
// @brief common globals
// //////////////////////////////////////////////////////////////////////////////

global.Buffer = require('buffer').Buffer;
global.process = require('process');
global.setInterval = global.setInterval || function () {};
global.clearInterval = global.clearInterval || function () {};
global.setTimeout = global.setTimeout || function () {};
global.clearTimeout = global.clearTimeout || function () {};

// //////////////////////////////////////////////////////////////////////////////
// / @brief start paging
// //////////////////////////////////////////////////////////////////////////////

global.start_pager = function start_pager () {
  require('internal').startPager();
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief stop paging
// //////////////////////////////////////////////////////////////////////////////

global.stop_pager = function stop_pager () {
  require('internal').stopPager();
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief print the overall help
// //////////////////////////////////////////////////////////////////////////////

global.help = function help () {
  let internal = require('internal');
  let arangodb = require('@arangodb');
  let arangosh = require('@arangodb/arangosh');

  internal.print(arangosh.HELP);
  arangodb.ArangoDatabase.prototype._help();
  arangodb.ArangoCollection.prototype._help();
  arangodb.ArangoStatement.prototype._help();
  arangodb.ArangoQueryCursor.prototype._help();
  internal.print(arangosh.helpExtended);
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief clear screen (poor man's way)
// //////////////////////////////////////////////////////////////////////////////

global.clear = function clear () {
  require('internal').print(Array(100).join('\n'));
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief global 'console'
// //////////////////////////////////////////////////////////////////////////////

global.console = require('console');

// //////////////////////////////////////////////////////////////////////////////
// / @brief global 'db'
// //////////////////////////////////////////////////////////////////////////////

global.db = require('@arangodb').db;

// //////////////////////////////////////////////////////////////////////////////
// / @brief template string generator for building an AQL query
// //////////////////////////////////////////////////////////////////////////////

global.aql = global.aqlQuery = require('@arangodb').aql;

// //////////////////////////////////////////////////////////////////////////////
// / @brief global 'arango'
// //////////////////////////////////////////////////////////////////////////////

global.arango = require('@arangodb').arango;

// //////////////////////////////////////////////////////////////////////////////
// / @brief global 'fm'
// //////////////////////////////////////////////////////////////////////////////

global.fm = require('@arangodb/foxx/manager');

// //////////////////////////////////////////////////////////////////////////////
// / @brief global 'ArangoStatement'
// //////////////////////////////////////////////////////////////////////////////

global.ArangoStatement = require('@arangodb/arango-statement').ArangoStatement;

// //////////////////////////////////////////////////////////////////////////////
// / @brief shell tutorial
// //////////////////////////////////////////////////////////////////////////////

global.tutorial = require('@arangodb/tutorial');

// //////////////////////////////////////////////////////////////////////////////
// / @brief prints help
// //////////////////////////////////////////////////////////////////////////////

(function () {
  let withTimeout = function(connection, timeout, cb) {
    if (!connection) {
      return;
    }
    let oldTimeout = connection.timeout();
    try {
      try {
        connection.timeout(timeout);
      } catch (err) {}
      return cb();
    } finally {
      try {
        connection.timeout(oldTimeout);
      } catch (err) {}
    }
  };

  let internal = require('internal');

  if (internal.db) {
    try {
      // cap request timeout to 10 seconds for initially fetching
      // the list of collections
      // this allows going on and using the arangosh even if the list
      // of collections cannot be fetched in sensible time
      withTimeout(internal.db._connection, 10, function() {
        internal.db._collections();
      });
    } catch (e) {}
   
    if (arango.isConnected()) {
      try {
        let remoteVersion = arango.getVersion();
        let [ourMajor, ourMinor] = internal.version.split('.');
        let [remoteMajor, remoteMinor] = remoteVersion.split('.');

        if (remoteMajor !== ourMajor ||
            (remoteMajor === ourMajor && remoteMinor > ourMinor)) {
          internal.print("Client/server version mismatch detected. arangosh version: " + internal.version + ", server version: " + remoteVersion);
        }
      } catch (e) {}
    }
  }

  if (internal.quiet !== true) {
    require('@arangodb').checkAvailableVersions();

    if (internal.arango && internal.arango.isConnected && internal.arango.isConnected()) {
      internal.print("Type 'tutorial' for a tutorial or 'help' to see common examples");
    }
  }
})();

// //////////////////////////////////////////////////////////////////////////////
// / @brief read rc file
// //////////////////////////////////////////////////////////////////////////////

if (!(
  global.IS_EXECUTE_SCRIPT ||
  global.IS_EXECUTE_STRING ||
  global.IS_CHECK_SCRIPT ||
  global.IS_UNIT_TESTS ||
  global.IS_JS_LINT
  )) {
  try {
    // this will not work from within a browser
    let __fs__ = require('fs');
    let __rcf__ = __fs__.join(__fs__.home(), '.arangosh.rc');

    if (__fs__.exists(__rcf__)) {
      /* jshint evil: true */
      let __content__ = __fs__.read(__rcf__);
      eval(__content__);
    }
  } catch (e) {
    require('console').debug('arangosh.rc: %s', String(e));
  }
}

delete global.IS_EXECUTE_SCRIPT;
delete global.IS_EXECUTE_STRING;
delete global.IS_CHECK_SCRIPT;
delete global.IS_UNIT_TESTS;
delete global.IS_JS_LINT;
