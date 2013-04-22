/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, nonpropdel: true, white: true, plusplus: true, evil: true */
/*global require, IS_EXECUTE_SCRIPT, IS_CHECK_SCRIPT, IS_UNIT_TESTS, IS_JS_LINT */

////////////////////////////////////////////////////////////////////////////////
/// @brief ArangoShell client API
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
/// @author Achim Brandt
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  global functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief start paging
////////////////////////////////////////////////////////////////////////////////

function start_pager () {
  var internal = require("internal");
  internal.startPager();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stop paging
////////////////////////////////////////////////////////////////////////////////

function stop_pager () {
  var internal = require("internal");
  internal.stopPager();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief print the overall help
////////////////////////////////////////////////////////////////////////////////

function help () {
  var internal = require("internal");
  var arangodb = require("org/arangodb");
  var client = require("org/arangodb/arangosh");

  internal.print(client.HELP);
  arangodb.ArangoStatement.prototype._help();
  arangodb.ArangoDatabase.prototype._help();
  arangodb.ArangoCollection.prototype._help();
  arangodb.ArangoQueryCursor.prototype._help();
  internal.print(client.helpExtended);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief clear screen (poor man's way)
////////////////////////////////////////////////////////////////////////////////

function clear () {
  var internal = require("internal");
  var i;
  var result = '';

  for (i = 0; i < 100; ++i) {
    result += '\n';
  }
  internal.print(result);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief global 'db'
////////////////////////////////////////////////////////////////////////////////

var db = require("org/arangodb").db;

////////////////////////////////////////////////////////////////////////////////
/// @brief global 'arango'
////////////////////////////////////////////////////////////////////////////////

var arango = require("org/arangodb").arango;

////////////////////////////////////////////////////////////////////////////////
/// @brief global 'Buffer'
////////////////////////////////////////////////////////////////////////////////

var Buffer = require("buffer").Buffer;

// -----------------------------------------------------------------------------
// --SECTION--                                                        initialise
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief prints help
////////////////////////////////////////////////////////////////////////////////

(function() {
  var internal = require("internal");
  var arangosh = require("org/arangodb/arangosh");
  var special;

  if (internal.db !== undefined) {
    try {
      internal.db._collections();
    }
    catch (err) {
    }
  }

  try {
    // these variables don't exist in the browser context
    special = IS_EXECUTE_SCRIPT || IS_CHECK_SCRIPT || IS_UNIT_TESTS || IS_JS_LINT;
  }
  catch (err2) {
    special = false;
  }

  if (internal.quiet !== true && ! special) {
    if (typeof internal.arango !== "undefined") {
      if (typeof internal.arango.isConnected !== "undefined" && internal.arango.isConnected()) {
        internal.print(arangosh.HELP);
      }
    }
  }
}());

////////////////////////////////////////////////////////////////////////////////
/// @brief read rc file
////////////////////////////////////////////////////////////////////////////////

(function () {
  var __special__;

  try {
    // these variables are not defined in the browser context
    __special__ = IS_EXECUTE_SCRIPT || IS_CHECK_SCRIPT || IS_UNIT_TESTS || IS_JS_LINT;
  }
  catch (err) {
    __special__ = true;
  }

  if (! __special__) {
    try {
      // this will not work from within a browser
      var __fs__ = require("fs");
      var __rcf__ = __fs__.join(__fs__.home(), ".arangosh.rc");

      if (__fs__.exists(__rcf__)) {
        var __content__ = __fs__.read(__rcf__);
        eval(__content__);
      }
    }
    catch (err2) {
      require("console").warn("arangosh.rc: %s", String(err2));
    }
  }

  try {
    delete IS_EXECUTE_SCRIPT;
    delete IS_CHECK_SCRIPT;
    delete IS_UNIT_TESTS;
    delete IS_JS_LINT;
  }
  catch (err3) {
  }
}());

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}\\|/\\*jslint"
// End:
