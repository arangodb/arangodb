/* jshint -W051, -W020 */
/* global global:true, require */
'use strict';

// /////////////////////////////////////////////////////////////////////////////
// @brief ArangoDB Inspector
// 
// @file
// 
// DISCLAIMER
// 
// Copyright 2018 ArangoDB GmbH, Cologne, Germany
// 
// Licensed under the Apache License, Version 2.0 (the "License")
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// 
//     http://www.apache.org/licenses/LICENSE-2.0
// 
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
// 
// Copyright holder is ArangoDB GmbH, Cologne, Germany
// 
// @author Dr. Frank Celler
// @author Copyright 2018, ArangoDB GmbH, Cologne, Germany
// /////////////////////////////////////////////////////////////////////////////

var arango;

// global 'arango'
global.arango = require('@arangodb').arango;

// global 'db'
global.db = require('@arangodb').db;

// cleanup
delete global.IS_EXECUTE_SCRIPT;
delete global.IS_EXECUTE_STRING;
delete global.IS_CHECK_SCRIPT;
delete global.IS_UNIT_TESTS;
delete global.IS_JS_LINT;

// print banner
(function() {
  const internal = require('internal');

  internal.print("    _                                  ___                           _");
  internal.print("   / \\   _ __ __ _ _ __   __ _  ___   |_ _|_ __  ___ _ __   ___  ___| |_ ___  _ __");
  internal.print("  / _ \\ | '__/ _` | '_ \\ / _` |/ _ \\   | || '_ \\/ __| '_ \\ / _ \\/ __| __/ _ \\| '__|");
  internal.print(" / ___ \\| | | (_| | | | | (_| | (_) |  | || | | \\__ \\ |_) |  __/ (__| || (_) | |");
  internal.print("/_/   \\_\\_|  \\__,_|_| |_|\\__, |\\___/  |___|_| |_|___/ .__/ \\___|\\___|\\__\\___/|_|");
  internal.print("                         |___/                      |_|                         ");
  internal.print("");
})();

// load rc file
try {
  // this will not work from within a browser
  const __fs__ = require('fs');
  const __rcf__ = __fs__.join(__fs__.home(), '.arangoinspect.rc');

  if (__fs__.exists(__rcf__)) {
    /* jshint evil: true */
    const __content__ = __fs__.read(__rcf__);
    eval(__content__);
  }
} catch (e) {
  require('console').warn('arangoinspect.rc: %s', String(e));
}

// check connection success
if (!arango.isConnected()) {
  const internal = require('internal');
  internal.print("FATAL cannot connect to server '" + arango.getEndpoint() + "'");
  require("process").exit(1);
}

if (arango.lastErrorMessage()) {
  const internal = require('internal');
  internal.print("FATAL cannot connect to server '" + arango.getEndpoint() + "': "
                 + arango.lastErrorMessage());
  require("process").exit(1);
}

require("@arangodb/inspector");
require("process").exit();



